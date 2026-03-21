// # MIT License
// #
// # Copyright (c) 2026 sookyboo
// #
// # Permission is hereby granted, free of charge, to any person obtaining a copy
// # of this software and associated documentation files (the "Software"), to deal
// # in the Software without restriction, including without limitation the rights
// # to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// # copies of the Software, and to permit persons to whom the Software is
// # furnished to do so, subject to the following conditions:
// #
// # The above copyright notice and this permission notice shall be included in all
// # copies or substantial portions of the Software.
// #
// # THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// # IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// # FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// # AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// # LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// # OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// # SOFTWARE.

// Cloudflare Worker (JavaScript)
// Endpoints:
//   GET  /api/v0/games/list[?refresh=1]
//   POST /api/v0/games/register
//
// Design constraints:
// - one game per IP (replace on register)
// - refresh on demand, return cached until refresh completes
// - no background refresh (no cron) to keep free-tier friendly
// - rate limiting
// - combine internal regs + XWIS list + another HTTP lobby list (same JSON format)
//
// XWIS:
// - Speaks XWIS TCP protocol directly (LIST only), via cloudflare:sockets.
// - Mirrors your C xwis.c: handshake + LIST -1 37 + parse 326 lines and decode G1P3 payload.
//
// Optional env vars:
// - UPSTREAM_LIST_URL: another HTTP lobby list in the same JSON format
// - XWIS_HOST / XWIS_PORT: override xwis.net:4000
// - XWIS_DISABLE: "1" to skip XWIS
// - NOX_XWIS_NICK: optional fixed nick; otherwise random probeXXXX
// - RL_LIST_PER_MIN / RL_REGISTER_PER_MIN
// - TRUST_ADDR: "1" to trust client-supplied addr; otherwise overwrite with caller IP

import { connect } from "cloudflare:sockets";

/**
 * @typedef {{cur:number,max:number}} Players
 * @typedef {{
 *  name:string, addr:string, port:number, map:string, mode:string, vers?:string,
 *  players:Players, _ts?:number, _src?:string
 * }} GameRow
 */

function json(res, init = {}) {
    return new Response(JSON.stringify(res), {
        ...init,
        headers: {
            "content-type": "application/json; charset=utf-8",
            ...(init.headers || {}),
        },
    });
}
function badRequest(msg) { return json({ error: msg }, { status: 400 }); }
function methodNotAllowed() { return json({ error: "method not allowed" }, { status: 405 }); }

function getClientIp(req) {
    return (
        req.headers.get("CF-Connecting-IP") ||
        (req.headers.get("X-Forwarded-For") || "").split(",")[0].trim() ||
        ""
    );
}
function clamp(n, lo, hi) { return Math.max(lo, Math.min(hi, n)); }

function normalizeMode(m) {
    const s = (typeof m === "string" ? m : "").trim().toLowerCase();
    if (!s) return "";
    if (s === "capflag") return "ctf";
    if (s === "elim") return "elimination";
    return s;
}
function sanitizeString(s, maxLen) {
    if (typeof s !== "string") return "";
    const t = s.trim();
    return t.length > maxLen ? t.slice(0, maxLen) : t;
}
function asUInt(n, def, lo, hi) {
    const v = typeof n === "number" ? n : (typeof n === "string" ? Number(n) : NaN);
    if (!Number.isFinite(v)) return def;
    return clamp(Math.floor(v), lo, hi);
}

async function fetchList(url, cf) {
    const r = await fetch(url, { method: "GET", headers: { accept: "application/json" }, cf });
    if (!r.ok) return [];
    let j;
    try { j = await r.json(); } catch { return []; }
    if (!j || !Array.isArray(j.data)) return [];
    return j.data.map((x) => ({
        name: sanitizeString(x?.name, 64),
        addr: sanitizeString(x?.addr, 32),
        port: asUInt(x?.port, 18590, 1, 65535),
        map: sanitizeString(x?.map, 32),
        mode: normalizeMode(x?.mode),
        vers: sanitizeString(x?.vers, 16) || undefined,
        players: {
            cur: asUInt(x?.players?.cur, 0, 0, 255),
            max: asUInt(x?.players?.max, 31, 0, 255),
        },
        _src: sanitizeString(x?._src, 32) || undefined,
    }));
}

// ============================================================================
// XWIS TCP LIST (ported from your C xwis.c; LIST only)
// ============================================================================

function u32ToIPv4(v) {
    const a = (v >>> 24) & 255;
    const b = (v >>> 16) & 255;
    const c = (v >>> 8) & 255;
    const d = v & 255;
    return `${a}.${b}.${c}.${d}`;
}

function splitTokensBinarySafe(lineBytes, maxTokens) {
    let start = 0;
    const out = [];

    if (lineBytes.length > 0 && lineBytes[0] === 0x3a) start = 1; // ':'
    while (start < lineBytes.length && (lineBytes[start] === 0x20 || lineBytes[start] === 0x09)) start++;

    let i = start;
    while (i < lineBytes.length && out.length < maxTokens) {
        if (out.length === maxTokens - 1) {
            out.push(lineBytes.subarray(i));
            break;
        }
        const tokStart = i;
        while (i < lineBytes.length && lineBytes[i] !== 0x20 && lineBytes[i] !== 0x09) i++;
        out.push(lineBytes.subarray(tokStart, i));
        while (i < lineBytes.length && (lineBytes[i] === 0x20 || lineBytes[i] === 0x09)) i++;
    }
    return out;
}

function bytesEqAscii(b, s) {
    if (b.length !== s.length) return false;
    for (let i = 0; i < s.length; i++) if (b[i] !== s.charCodeAt(i)) return false;
    return true;
}

function parseU32Ascii(b) {
    if (!b.length) return null;
    let v = 0;
    for (let i = 0; i < b.length; i++) {
        const c = b[i];
        if (c < 0x30 || c > 0x39) return null;
        v = v * 10 + (c - 0x30);
        if (v > 0xffffffff) return null;
    }
    return v >>> 0;
}

function channelTokToName(chanTok) {
    let s = new TextDecoder().decode(chanTok);
    if (s.startsWith("#")) s = s.slice(1);
    return s.slice(0, 64);
}

const XWIS_FULLHDR_LEN = 12; // "128:" (4) + ":G1P3\x9a\x03\x01" (8)

function payloadHasG1P3(payload) {
    if (payload.length < XWIS_FULLHDR_LEN) return false;
    if (!(payload[0] === 0x31 && payload[1] === 0x32 && payload[2] === 0x38 && payload[3] === 0x3a)) return false;
    if (!(payload[4] === 0x3a && payload[5] === 0x47 && payload[6] === 0x31 && payload[7] === 0x50 && payload[8] === 0x33)) return false;
    return true;
}

// matches your C xwis_decrypt_inplace()
function xwisDecryptInPlace(data) {
    if (data.length < 10) return;

    let ind = 0;
    let cnt = 0;
    let loc = 0;

    for (let i = 0; i + 10 <= data.length; i++) {
        let acc = 0;
        for (let j = 0; j <= 7; j++) {
            if (cnt === 7) { cnt = 0; loc++; }
            if (loc === 8) {
                if (ind < data.length) data[ind] = 0;
                ind++;
                loc = 0;
            }
            const v6 = ind < data.length ? data[ind] : 0;
            const v5 = 1 << loc;
            const v4 = v6 & v5;
            const v3 = v4 >> loc;
            acc ^= ((v3 << j) & 0xff);
            loc++;
            cnt++;
        }
        data[i] = acc & 0xff;
    }
}

function copyTrimNulAscii(src, maxOut) {
    const out = [];
    for (let i = 0; i < src.length && out.length + 1 < maxOut; i++) {
        const c = src[i];
        if (c === 0) break;
        out.push(c);
    }
    while (out.length && out[out.length - 1] <= 0x20) out.pop();
    return new TextDecoder().decode(new Uint8Array(out));
}

function modeFromMaptypeBits(maptype) {
    switch (maptype) {
        case 0x0010: return "kotr";
        case 0x0020: return "ctf";
        case 0x0040: return "flagball";
        case 0x0080: return "chat";
        case 0x0100: return "arena";
        case 0x0400: return "elimination";
        case 0x0a00: return "coop";
        case 0x1000: return "quest";
        default: return "";
    }
}

function decodeG1P3IntoRow(payload, row) {
    if (!payloadHasG1P3(payload)) return row;

    const enc = payload.subarray(XWIS_FULLHDR_LEN);
    if (enc.length < 69) return row;

    const buf = new Uint8Array(enc); // copy
    xwisDecryptInPlace(buf);

    const cur = buf[3];
    const max = buf[4] ? buf[4] : 31;

    const map = copyTrimNulAscii(buf.subarray(11, 11 + 9), 32);
    const name = copyTrimNulAscii(buf.subarray(20, 20 + 15), 64);

    const flags = (buf[63] | (buf[64] << 8)) & 0xffff;
    const maptype = flags & 0x1ff0;
    const mode = modeFromMaptypeBits(maptype);

    return {
        ...row,
        players: { cur, max },
        map: map || row.map,
        name: name || row.name,
        mode: mode || row.mode,
    };
}

async function readLineBytes(reader, bufState) {
    for (;;) {
        const b = bufState.buf;
        const nl = b.indexOf(0x0a); // '\n'
        if (nl >= 0) {
            const line = b.subarray(0, nl + 1);
            bufState.buf = b.subarray(nl + 1);
            return line;
        }

        const { value, done } = await reader.read();
        if (done) {
            if (b.length) { bufState.buf = new Uint8Array(); return b; }
            return null;
        }

        if (value && value.length) {
            const merged = new Uint8Array(b.length + value.length);
            merged.set(b, 0);
            merged.set(value, b.length);
            bufState.buf = merged;
        }
    }
}

async function writeLine(writer, s) {
    const enc = new TextEncoder().encode(s + "\r\n");
    await writer.write(enc);
}

// FIXED: reuse caller bufState so any extra bytes stay available for later parsing.
async function readUntilNumeric(reader, bufState, want, maxLines) {
    const dec = new TextDecoder();
    for (let i = 0; i < maxLines; i++) {
        const line = await readLineBytes(reader, bufState);
        if (!line) return false;

        const s = dec.decode(line).replace(/[\r\n]+$/g, "");
        const t = s.startsWith(":") ? s.slice(1).trim() : s.trim();
        const tok0 = t.split(/[ \t]+/, 1)[0] || "";
        if (tok0 === want) return true;
    }
    return false;
}

function makeRandomNick() {
    return "probe" + Math.floor(Math.random() * 0xffff).toString(16).padStart(4, "0");
}

async function xwisListTcp(env) {
    if ((env.XWIS_DISABLE || "0") === "1") return [];

    const host = env.XWIS_HOST || "xwis.net";
    const port = asUInt(env.XWIS_PORT, 4000, 1, 65535);

    const nick = (env.NOX_XWIS_NICK && String(env.NOX_XWIS_NICK).trim())
        ? String(env.NOX_XWIS_NICK).trim()
        : makeRandomNick();

    const socket = connect({ hostname: host, port });
    const reader = socket.readable.getReader();
    const writer = socket.writable.getWriter();

    // IMPORTANT: single shared buffer for the entire session (handshake + list)
    const bufState = { buf: new Uint8Array() };

    try {
        await writeLine(writer, "CVERS 11015 9472");
        await writeLine(writer, "PASS supersecret");
        await writeLine(writer, `NICK ${nick}`);
        await writeLine(writer, `apgar ${nick} 0`);
        await writeLine(writer, `USER UserName HostName ${host} :RealName`);

        const ok = await readUntilNumeric(reader, bufState, "376", 250);
        if (!ok) return [];

        await writeLine(writer, "LIST -1 37");

        /** @type {GameRow[]} */
        const games = [];

        for (;;) {
            const line = await readLineBytes(reader, bufState);
            if (!line) break;

            let lineTrim = line;
            while (
                lineTrim.length &&
                (lineTrim[lineTrim.length - 1] === 0x0a || lineTrim[lineTrim.length - 1] === 0x0d)
                ) {
                lineTrim = lineTrim.subarray(0, lineTrim.length - 1);
            }
            if (!lineTrim.length) continue;

            const toks = splitTokensBinarySafe(lineTrim, 10);
            if (toks.length < 1) continue;

            if (bytesEqAscii(toks[0], "323")) break;
            if (!bytesEqAscii(toks[0], "326")) continue;
            if (toks.length < 10) continue;

            const ipU32 = parseU32Ascii(toks[8]);
            if (ipU32 === null) continue;

            /** @type {GameRow} */
            let row = {
                name: channelTokToName(toks[2]),
                addr: u32ToIPv4(ipU32),
                port: 18590,
                map: "",
                mode: "",
                players: { cur: 0, max: 31 },
                _src: "xwis",
            };

            let payload = toks[9];
            while (payload.length && (payload[0] === 0x20 || payload[0] === 0x09)) payload = payload.subarray(1);
            if (payload.length && payload[0] === 0x3a) payload = payload.subarray(1); // IRC trailing marker

            if (payloadHasG1P3(payload)) {
                row = decodeG1P3IntoRow(payload, row);
            }

            if (!row.players?.max) row.players.max = 31;
            if (!row.mode) row.mode = "chat";

            games.push(row);
        }

        try { await writeLine(writer, "QUIT"); } catch {}
        return games;
    } catch {
        return [];
    } finally {
        try { writer.releaseLock(); } catch {}
        try { reader.releaseLock(); } catch {}
        try { await socket.close(); } catch {}
    }
}

// ============================================================================
// Durable Object
// ============================================================================

export class LobbyDO {
    constructor(state, env) {
        this.state = state;
        this.env = env;
        this.cache = undefined;
        this.refreshPromise = undefined;
    }

    async fetch(request) {
        const url = new URL(request.url);
        const path = url.pathname;

        if (path === "/do/register") {
            if (request.method !== "POST") return methodNotAllowed();
            return this.handleRegister(request);
        }

        if (path === "/do/list") {
            if (request.method !== "GET") return methodNotAllowed();
            const refresh = url.searchParams.get("refresh") === "1";
            return this.handleList(request, refresh);
        }

        if (path === "/do/stop") {
            if (request.method !== "POST") return methodNotAllowed();
            return this.handleStop(request);
        }

        return new Response("not found", { status: 404 });
    }

    async rateLimit(ip, kind) {
        const perMin =
            kind === "list"
                ? asUInt(this.env.RL_LIST_PER_MIN, 120, 1, 5000)
                : asUInt(this.env.RL_REGISTER_PER_MIN, 20, 1, 5000);

        const key = `rl:${kind}:${ip}`;
        const now = Date.now();
        const windowMs = 60_000;

        const rs = (await this.state.storage.get(key)) || { windowStartMs: now, count: 0 };

        if (now - rs.windowStartMs >= windowMs) {
            rs.windowStartMs = now;
            rs.count = 0;
        }

        rs.count++;
        await this.state.storage.put(key, rs);

        if (rs.count > perMin) {
            return json({ error: "rate limited" }, { status: 429, headers: { "retry-after": "60" } });
        }
        return null;
    }

    async handleRegister(request) {
        const ip = getClientIp(request);
        if (!ip) return badRequest("missing client ip");

        const rl = await this.rateLimit(ip, "register");
        if (rl) return rl;

        let body;
        try { body = await request.json(); }
        catch { return badRequest("invalid json"); }

        const name = sanitizeString(body?.name, 64);
        const map = sanitizeString(body?.map, 32);
        const mode = normalizeMode(body?.mode);
        const vers = sanitizeString(body?.vers, 16);
        const port = asUInt(body?.port, 18590, 1, 65535);

        const cur = asUInt(body?.players?.cur, 0, 0, 255);
        const max = asUInt(body?.players?.max, 31, 0, 255);

        if (!name) return badRequest("missing name");
        if (!map) return badRequest("missing map");

        const trustAddr = (this.env.TRUST_ADDR || "0") === "1";
        const addr = trustAddr ? (sanitizeString(body?.addr, 32) || ip) : ip;

        const row = {
            name,
            addr,
            port,
            map,
            mode: mode || "chat",
            vers: vers || undefined,
            players: { cur: Math.min(cur, max || 255), max: max || 31 },
            _ts: Date.now(),
            _src: "internal",
        };

        // one game per IP => overwrite
        await this.state.storage.put(`game:${ip}`, row);

        // invalidate cache
        this.cache = undefined;

        return json({ ok: true });
    }

    async handleStop(request) {
        const ip = getClientIp(request);
        if (!ip) return badRequest("missing client ip");

        await this.state.storage.delete(`game:${ip}`);
        this.cache = undefined;

        return json({ ok: true });
    }

    async handleList(request, refresh) {
        const ip = getClientIp(request) || "unknown";

        const rl = await this.rateLimit(ip, "list");
        if (rl) return rl;

        const cached = this.cache;

        if (refresh) {
            if (!this.refreshPromise) {
                this.refreshPromise = this.refreshAll().finally(() => { this.refreshPromise = undefined; });
            }
            if (cached) return json({ data: cached.data });
            await this.refreshPromise;
            return json({ data: (this.cache && this.cache.data) || [] });
        }

        if (cached) return json({ data: cached.data });

        await this.refreshAll();
        return json({ data: (this.cache && this.cache.data) || [] });
    }

    async refreshAll() {
        // 1) internal registrations
        const internal = [];
        const list = await this.state.storage.list({ prefix: "game:" });
        for (const [, row] of list) {
            if (!row?.addr || !row?.name) continue;
            internal.push({ ...row, _src: "internal" });
        }

        // 2) upstream http lobby list
        let upstream = [];
        if (this.env.UPSTREAM_LIST_URL) {
            try {
                upstream = await fetchList(this.env.UPSTREAM_LIST_URL, { cacheTtl: 0 });
                upstream = upstream.map((x) => ({ ...x, _src: "upstream" }));
            } catch {
                upstream = [];
            }
        }

        // 3) xwis list via direct TCP protocol
        let xwis = [];
        try {
            xwis = await xwisListTcp(this.env);
            xwis = xwis.map((x) => ({ ...x, _src: "xwis" }));
        } catch {
            xwis = [];
        }

        // Merge: internal wins for same IP, then upstream, then xwis
        const byIp = new Map();
        for (const r of xwis) if (r.addr) byIp.set(r.addr, r);
        for (const r of upstream) if (r.addr) byIp.set(r.addr, r);
        for (const r of internal) if (r.addr) byIp.set(r.addr, r);

        const merged = [...byIp.values()]
            .filter((r) => r.addr && r.name)
            .map((r) => ({
                ...r,
                port: r.port || 18590,
                players: {
                    cur: clamp(r.players?.cur ?? 0, 0, 255),
                    max: clamp(r.players?.max ?? 31, 0, 255) || 31,
                },
                mode: (r.mode || "chat").slice(0, 15),
                map: (r.map || "").slice(0, 31),
                name: (r.name || "").slice(0, 63),
                addr: (r.addr || "").slice(0, 31),
            }));

        this.cache = { ts: Date.now(), data: merged };
    }
}

// ============================================================================
// Worker entry
// ============================================================================

function makeInMemoryLobby(env) {
    // Minimal in-memory substitute for LobbyDO so you can run without DO bindings.
    // NOTE: not durable, not shared across instances.
    const storage = new Map(); // key -> value

    const state = {
        storage: {
            async get(k) { return storage.get(k); },
            async put(k, v) { storage.set(k, v); },
            async delete(k) { storage.delete(k); },
            async list({ prefix }) {
                const out = new Map();
                for (const [k, v] of storage.entries()) {
                    if (!prefix || k.startsWith(prefix)) out.set(k, v);
                }
                return out;
            },
        },
    };

    const lobby = new LobbyDO(state, env);
    return lobby;
}

export default {
    async fetch(request, env) {
        const url = new URL(request.url);

        // If DO is bound, use it.
        if (env.LOBBY_DO && typeof env.LOBBY_DO.idFromName === "function") {
            const id = env.LOBBY_DO.idFromName("global");
            const stub = env.LOBBY_DO.get(id);

            if (url.pathname === "/api/v0/games/register") {
                if (request.method !== "POST") return methodNotAllowed();
                return stub.fetch(new Request(new URL("/do/register", url).toString(), request));
            }
            if (url.pathname === "/api/v0/games/list") {
                if (request.method !== "GET") return methodNotAllowed();
                return stub.fetch(new Request(new URL("/do/list" + url.search, url).toString(), request));
            }
            if (url.pathname === "/api/v0/games/stop") {
                if (request.method !== "POST") return methodNotAllowed();
                return stub.fetch(new Request(new URL("/do/stop", url).toString(), request));
            }

            return new Response("not found", { status: 404 });
        }

        // Otherwise, fallback (won’t crash)
        const lobby = makeInMemoryLobby(env);

        if (url.pathname === "/api/v0/games/register") {
            if (request.method !== "POST") return methodNotAllowed();
            return lobby.fetch(new Request(new URL("/do/register", url).toString(), request));
        }
        if (url.pathname === "/api/v0/games/list") {
            if (request.method !== "GET") return methodNotAllowed();
            return lobby.fetch(new Request(new URL("/do/list" + url.search, url).toString(), request));
        }
        if (url.pathname === "/api/v0/games/stop") {
            if (request.method !== "POST") return methodNotAllowed();
            return lobby.fetch(new Request(new URL("/do/stop", url).toString(), request));
        }

        return new Response("not found", { status: 404 });
    },
};