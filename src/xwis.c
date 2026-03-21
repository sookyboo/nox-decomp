// xwis.c
//
// Minimal XWIS (xwis.net:4000) client for listing NOX games (game id 37)
// and converting them into accurate nox_server_row entries, using the
// 326 payload decode (G1P3) you pasted.
//
// Refrence implementation https://github.com/opennox/xwis/blob/master/gameinfo.go
//
// No extra deps beyond sockets + SDL2 (for random nick).
//
// Exports (no header; netextras.c should declare extern):
//   int nox_xwis_list_nox_games(nox_server_row *out, size_t out_cap, size_t *out_n);
//
// Host-side (added, minimal state; persistent connection; JOIN/PART/TOPIC):
//   int  nox_xwis_host_update_game(const char *name, const char *map, const char *mode,
//                                 unsigned cur, unsigned max, unsigned port);
//   void nox_xwis_host_stop(void);
//
// Env:
//   NOX_XWIS_HOST  (default "xwis.net")
//   NOX_XWIS_PORT  (default 4000)
//   NOX_XWIS_NICK  (optional; if empty -> random probeXXXX)
//
// Notes:
// - Binary-safe line reading: payload contains NUL / non-UTF8.
// - Parsing: we split first 9 tokens by spaces, and treat the remainder as payload.
// - Payload decode:
//     token payload begins with "128::G1P3..." (12-byte header: "128:" + ":G1P3\x9a\x03\x01")
//     skip 12 bytes, run decrypt() bit-unpack, then parse fixed GameInfo offsets.
//

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>

#include <SDL2/SDL.h>  // for SDL_GetTicks + SDL_GetPerformanceCounter

#include "netextras_types.h"

#ifdef _WIN32
  #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
  #endif
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #include <windows.h>

  #ifndef strcasecmp
  #define strcasecmp _stricmp
  #endif

  #ifndef SHUT_RDWR
  #define SHUT_RDWR SD_BOTH
  #endif
  #define close closesocket
  typedef int socklen_t;

  static int g_wsa_inited_xwis = 0;
  static void xwis_wsa_init_once(void) {
      if (!g_wsa_inited_xwis) {
          WSADATA w;
          if (WSAStartup(MAKEWORD(2, 2), &w) == 0) g_wsa_inited_xwis = 1;
      }
  }

  static int xwis_set_nonblock(int fd, int on)
  {
      u_long mode = on ? 1UL : 0UL;
      return ioctlsocket((SOCKET)fd, FIONBIO, &mode) == 0 ? 0 : -1;
  }

  static int xwis_sock_last_err(void) { return WSAGetLastError(); }

  static void xwis_errno_from_wsa(int wsa)
  {
      if (wsa == WSAEINTR)             errno = EINTR;
      else if (wsa == WSAEWOULDBLOCK)  errno = EWOULDBLOCK;
      else if (wsa == WSAEINPROGRESS)  errno = EINPROGRESS;
      else if (wsa == WSAETIMEDOUT)    errno = ETIMEDOUT;
      else if (wsa == WSAECONNRESET)   errno = ECONNRESET;
      else if (wsa == WSAEADDRINUSE)   errno = EADDRINUSE;
      else if (wsa == WSAENETUNREACH)  errno = ENETUNREACH;
      else if (wsa == WSAEHOSTUNREACH) errno = EHOSTUNREACH;
      else if (wsa == WSAECONNREFUSED) errno = ECONNREFUSED;
      else                             errno = EIO;
  }
#else
  #include <unistd.h>
  #include <sys/types.h>
  #include <sys/socket.h>
  #include <fcntl.h>
  #include <netdb.h>
  #include <sys/time.h>
#endif

#ifdef _WIN32
  #define XWIS_SOCKFD(fd) ((SOCKET)(fd))
#else
  #define XWIS_SOCKFD(fd) (fd)
#endif

// Increase buffer for long payload lines.
#define XWIS_LINE_CAP (32768u)

// -------------------- small helpers --------------------

static int xwis_env_int(const char *name, int defv)
{
    const char *s = getenv(name);
    if (!s || !*s) return defv;
    return atoi(s);
}

static const char *xwis_env_str(const char *name, const char *defv)
{
    const char *s = getenv(name);
    return (s && *s) ? s : defv;
}

static int xwis_send_all(int fd, const void *buf, size_t len)
{
    const unsigned char *p = (const unsigned char *)buf;
    while (len) {
        ssize_t n = send(XWIS_SOCKFD(fd), (const char*)p, (int)len, 0);
        if (n < 0) {
#ifdef _WIN32
            int wsa = xwis_sock_last_err();
            if (wsa == WSAEINTR) continue;
            xwis_errno_from_wsa(wsa);
#else
            if (errno == EINTR) continue;
#endif
            return -1;
        }
        p   += (size_t)n;
        len -= (size_t)n;
    }
    return 0;
}

static int xwis_send_linef(int fd, char *tmp, size_t tmp_cap, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(tmp, tmp_cap, fmt, ap);
    va_end(ap);
    if (n <= 0 || (size_t)n >= tmp_cap) return -1;

    // ensure CRLF
    if ((size_t)n + 2 >= tmp_cap) return -1;
    tmp[n++] = '\r';
    tmp[n++] = '\n';

    return xwis_send_all(fd, tmp, (size_t)n);
}

// Binary-safe CRLF/LF line reader.
// Returns: >0 length (including trailing LF/CRLF bytes), 0 EOF, -1 error.
// If the line exceeds cap before '\n', returns cap and leaves the stream positioned
// mid-line; caller may drain.
static int xwis_recv_line_len(int fd, unsigned char *buf, size_t cap)
{
    size_t n = 0;
    if (!buf || cap < 2) { errno = EINVAL; return -1; }

    while (n < cap) {
        unsigned char c = 0;
        ssize_t r = recv(XWIS_SOCKFD(fd), (char*)&c, 1, 0);
        if (r == 0) return 0;
        if (r < 0) {
#ifdef _WIN32
            int wsa = xwis_sock_last_err();
            if (wsa == WSAEINTR) continue;
            xwis_errno_from_wsa(wsa);
#else
            if (errno == EINTR) continue;
#endif
            return -1;
        }
        buf[n++] = c;
        if (c == '\n') break;
    }
    return (int)n;
}

static int xwis_drain_to_newline(int fd)
{
    // Drain until '\n' (best-effort), to recover from overlong line truncation.
    for (;;) {
        unsigned char c = 0;
        ssize_t r = recv(XWIS_SOCKFD(fd), (char*)&c, 1, 0);
        if (r <= 0) return -1;
        if (c == '\n') return 0;
    }
}

static void xwis_trim_crlf(const unsigned char **p, size_t *n)
{
    if (!p || !*p || !n) return;
    while (*n && ((*p)[*n - 1] == '\n' || (*p)[*n - 1] == '\r')) (*n)--;
}

// -------------------- connect with timeout --------------------

static void xwis_set_sock_timeouts(int fd, int ms)
{
#ifdef _WIN32
    xwis_wsa_init_once();
    DWORD tv = (DWORD)ms;
    setsockopt((SOCKET)fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, (int)sizeof(tv));
    setsockopt((SOCKET)fd, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, (int)sizeof(tv));
#else
    struct timeval tv;
    tv.tv_sec  = ms / 1000;
    tv.tv_usec = (ms % 1000) * 1000;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, (socklen_t)sizeof(tv));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, (socklen_t)sizeof(tv));
#endif
}

static int xwis_connect_with_timeout(int fd, const struct sockaddr *sa, socklen_t slen, int ms)
{
#ifdef _WIN32
    xwis_wsa_init_once();
    if (xwis_set_nonblock(fd, 1) < 0) return -1;

    int rc = connect((SOCKET)fd, sa, (int)slen);
    if (rc == 0) { (void)xwis_set_nonblock(fd, 0); return 0; }

    int wsa = xwis_sock_last_err();
    if (wsa != WSAEWOULDBLOCK && wsa != WSAEINPROGRESS && wsa != WSAEALREADY) {
        xwis_errno_from_wsa(wsa);
        (void)xwis_set_nonblock(fd, 0);
        return -1;
    }

    fd_set wfds;
    FD_ZERO(&wfds);
    FD_SET((SOCKET)fd, &wfds);

    struct timeval tv;
    tv.tv_sec  = ms / 1000;
    tv.tv_usec = (ms % 1000) * 1000;

    rc = select(fd + 1, NULL, &wfds, NULL, &tv);
    if (rc <= 0) {
        (void)xwis_set_nonblock(fd, 0);
        if (rc == 0) errno = ETIMEDOUT;
        return -1;
    }

    int soerr = 0;
    socklen_t soerrlen = sizeof(soerr);
    if (getsockopt((SOCKET)fd, SOL_SOCKET, SO_ERROR, (char*)&soerr, &soerrlen) < 0) {
        (void)xwis_set_nonblock(fd, 0);
        return -1;
    }

    (void)xwis_set_nonblock(fd, 0);

    if (soerr != 0) {
        xwis_errno_from_wsa(soerr);
        return -1;
    }
    return 0;
#else
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) return -1;
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) return -1;

    int rc = connect(fd, sa, slen);
    if (rc == 0) { (void)fcntl(fd, F_SETFL, flags); return 0; }
    if (rc < 0 && errno != EINPROGRESS) { (void)fcntl(fd, F_SETFL, flags); return -1; }

    fd_set wfds;
    FD_ZERO(&wfds);
    FD_SET(fd, &wfds);

    struct timeval tv;
    tv.tv_sec  = ms / 1000;
    tv.tv_usec = (ms % 1000) * 1000;

    rc = select(fd + 1, NULL, &wfds, NULL, &tv);
    if (rc <= 0) {
        (void)fcntl(fd, F_SETFL, flags);
        errno = (rc == 0) ? ETIMEDOUT : errno;
        return -1;
    }

    int soerr = 0;
    socklen_t soerrlen = sizeof(soerr);
    if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &soerr, &soerrlen) < 0) {
        (void)fcntl(fd, F_SETFL, flags);
        return -1;
    }

    (void)fcntl(fd, F_SETFL, flags);

    if (soerr != 0) { errno = soerr; return -1; }
    return 0;
#endif
}

static int xwis_connect_tcp(const char *host, const char *port_str, int timeout_ms)
{
#ifdef _WIN32
    xwis_wsa_init_once();
#endif
    struct addrinfo hints;
    struct addrinfo *res = NULL, *it = NULL;
    int fd = -1;

    memset(&hints, 0, sizeof(hints));
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_family   = AF_UNSPEC;

    int rc = getaddrinfo(host, port_str, &hints, &res);
    if (rc != 0 || !res) return -1;

    for (it = res; it; it = it->ai_next) {
        fd = (int)socket(it->ai_family, it->ai_socktype, it->ai_protocol);
        if (fd < 0) continue;

        xwis_set_sock_timeouts(fd, timeout_ms);

        if (xwis_connect_with_timeout(fd, it->ai_addr, (socklen_t)it->ai_addrlen, timeout_ms) == 0) {
            break;
        }
        close(fd);
        fd = -1;
    }

    freeaddrinfo(res);
    return fd;
}

// -------------------- random nick (SDL2) --------------------

static uint32_t xwis_xorshift32(uint32_t *s)
{
    uint32_t x = *s;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *s = x;
    return x;
}

static void xwis_make_random_nick(char *out, size_t cap)
{
    if (!out || cap == 0) return;
    uint64_t pc = SDL_GetPerformanceCounter();
    uint32_t seed = (uint32_t)SDL_GetTicks() ^ (uint32_t)(pc) ^ (uint32_t)(pc >> 32);
    if (seed == 0) seed = 0xA5C3D2E1u;

    uint32_t r = xwis_xorshift32(&seed);
    snprintf(out, cap, "probe%04x", (unsigned)(r & 0xFFFFu));
}

// -------------------- payload decode (ported from Go) --------------------

#define XWIS_PREHDR_LEN   4  // "128:"
#define XWIS_HDR_LEN      8  // ":G1P3\x9a\x03\x01"
#define XWIS_FULLHDR_LEN  (XWIS_PREHDR_LEN + XWIS_HDR_LEN)

static void xwis_decrypt_inplace(unsigned char *data, size_t len)
{
    if (!data || len < 10) return;

    size_t ind = 0;
    int cnt = 0;
    int loc = 0;

    for (size_t i = 0; i + 10 <= len; i++) {
        unsigned char acc = 0;
        for (int j = 0; j <= 7; j++) {
            if (cnt == 7) { cnt = 0; loc++; }
            if (loc == 8) {
                if (ind < len) data[ind] = 0;
                ind++;
                loc = 0;
            }
            unsigned char v6 = 0;
            if (ind < len) v6 = data[ind];
            unsigned char v5 = (unsigned char)(1u << (unsigned)loc);
            int v4 = ((int)v6) & (int)v5;
            int v3 = v4 >> loc;
            acc ^= (unsigned char)((v3 << j) & 0xFF);
            loc++;
            cnt++;
        }
        data[i] = acc;
    }
}

static void xwis_copy_trim_nul(char *dst, size_t dst_cap, const unsigned char *src, size_t src_len)
{
    if (!dst || dst_cap == 0) return;
    dst[0] = 0;
    if (!src || src_len == 0) return;

    size_t n = 0;
    for (; n < src_len && n + 1 < dst_cap; n++) {
        unsigned char c = src[n];
        if (c == 0) break;
        dst[n] = (char)c;
    }
    dst[n] = 0;

    while (n && (unsigned char)dst[n - 1] <= ' ') dst[--n] = 0;
}

static const char *xwis_mode_from_maptype_u16(uint16_t maptype_bits)
{
    switch (maptype_bits) {
        case 0x0010: return "kotr";
        case 0x0020: return "ctf";
        case 0x0040: return "flagball";
        case 0x0080: return "chat";
        case 0x0100: return "arena";
        case 0x0400: return "elimination";
        case 0x0A00: return "coop";
        case 0x1000: return "quest";
        default: return NULL;
    }
}

static int xwis_payload_has_g1p3(const unsigned char *payload, size_t payload_len)
{
    // Accept both "128::G1P3..." and ":128::G1P3..." (colon-stripped elsewhere too).
    if (!payload || payload_len < XWIS_FULLHDR_LEN) return 0;
    if (!(payload[0] == '1' && payload[1] == '2' && payload[2] == '8' && payload[3] == ':')) return 0;
    // require ":G1P3" at payload[4..8]
    if (!(payload[4] == ':' && payload[5] == 'G' && payload[6] == '1' && payload[7] == 'P' && payload[8] == '3')) return 0;
    return 1;
}

static int xwis_decode_gameinfo_into_row(const unsigned char *payload, size_t payload_len, nox_server_row *row)
{
    if (!row || !xwis_payload_has_g1p3(payload, payload_len)) return -1;

    if (payload_len < XWIS_FULLHDR_LEN + 69) return -1;

    const unsigned char *enc = payload + XWIS_FULLHDR_LEN;
    size_t enc_len = payload_len - XWIS_FULLHDR_LEN;
    if (enc_len < 69) return -1;

    unsigned char *buf = (unsigned char *)malloc(enc_len);
    if (!buf) return -1;
    memcpy(buf, enc, enc_len);
    xwis_decrypt_inplace(buf, enc_len);

    // Decode into temporaries so we don't overwrite good channel defaults with empty strings.
    nox_server_row tmp = *row;

    tmp.players_cur = buf[3];
    tmp.players_max = buf[4] ? buf[4] : 31;

    char map_tmp[32] = {0};
    char name_tmp[64] = {0};
    xwis_copy_trim_nul(map_tmp, sizeof(map_tmp), &buf[11], 9);
    xwis_copy_trim_nul(name_tmp, sizeof(name_tmp), &buf[20], 15);

    if (map_tmp[0]) {
        strncpy(tmp.map, map_tmp, sizeof(tmp.map) - 1);
        tmp.map[sizeof(tmp.map) - 1] = 0;
    }
    if (name_tmp[0]) {
        strncpy(tmp.name, name_tmp, sizeof(tmp.name) - 1);
        tmp.name[sizeof(tmp.name) - 1] = 0;
    }

    uint16_t flags = (uint16_t)((uint16_t)buf[63] | ((uint16_t)buf[64] << 8));
    uint16_t maptype = (uint16_t)(flags & 0x1FF0u);
    const char *mode = xwis_mode_from_maptype_u16(maptype);
    if (mode) {
        strncpy(tmp.mode, mode, sizeof(tmp.mode) - 1);
        tmp.mode[sizeof(tmp.mode) - 1] = 0;
    }

    free(buf);

    *row = tmp;
    return 0;
}

// -------------------- IRC-ish token parsing --------------------

typedef struct {
    const unsigned char *p;
    size_t n;
} xwis_tok;

static int xwis_is_space(unsigned char c) { return c == ' ' || c == '\t'; }

static const unsigned char *xwis_skip_spaces(const unsigned char *p, const unsigned char *end)
{
    while (p < end && xwis_is_space(*p)) p++;
    return p;
}

static int xwis_split_tokens(const unsigned char *line, size_t line_len, xwis_tok *toks, int max_tokens)
{
    if (!line || !toks || max_tokens <= 0) return 0;

    const unsigned char *p = line;
    const unsigned char *end = line + line_len;

    if (p < end && *p == ':') p++;
    p = xwis_skip_spaces(p, end);

    int nt = 0;
    while (p < end && nt < max_tokens) {
        if (nt == max_tokens - 1) {
            toks[nt].p = p;
            toks[nt].n = (size_t)(end - p);
            nt++;
            break;
        }

        const unsigned char *start = p;
        while (p < end && !xwis_is_space(*p)) p++;
        toks[nt].p = start;
        toks[nt].n = (size_t)(p - start);
        nt++;

        p = xwis_skip_spaces(p, end);
    }
    return nt;
}

static int xwis_tok_eq(const xwis_tok *t, const char *s)
{
    if (!t || !s) return 0;
    size_t sl = strlen(s);
    return t->n == sl && memcmp(t->p, s, sl) == 0;
}

static int xwis_parse_u32_tok(const xwis_tok *t, uint32_t *out)
{
    if (!t || !out || t->n == 0) return -1;
    uint64_t v = 0;
    for (size_t i = 0; i < t->n; i++) {
        unsigned char c = t->p[i];
        if (c < '0' || c > '9') return -1;
        v = v * 10u + (uint64_t)(c - '0');
        if (v > 0xFFFFFFFFu) return -1;
    }
    *out = (uint32_t)v;
    return 0;
}

static void xwis_u32_to_ipv4(uint32_t v, char *out, size_t cap)
{
    unsigned a = (unsigned)((v >> 24) & 255u);
    unsigned b = (unsigned)((v >> 16) & 255u);
    unsigned c = (unsigned)((v >>  8) & 255u);
    unsigned d = (unsigned)((v >>  0) & 255u);
    snprintf(out, cap, "%u.%u.%u.%u", a, b, c, d);
}

static void xwis_channel_to_name(const xwis_tok *chan, char *out, size_t cap)
{
    if (!out || cap == 0) return;
    out[0] = 0;
    if (!chan || chan->n == 0) return;

    const unsigned char *p = chan->p;
    size_t n = chan->n;
    if (n && *p == '#') { p++; n--; }

    size_t k = 0;
    while (k < n && k + 1 < cap) {
        unsigned char c = p[k];
        if (c == 0) break;
        out[k] = (char)c;
        k++;
    }
    out[k] = 0;
}

// -------------------- main API --------------------

static int xwis_read_until_numeric(int fd, const char *want_numeric, int max_lines)
{
    unsigned char *line = (unsigned char*)malloc(XWIS_LINE_CAP);
    if (!line) return -1;

    for (int i = 0; i < max_lines; i++) {
        int rl = xwis_recv_line_len(fd, line, XWIS_LINE_CAP);
        if (rl <= 0) { free(line); return -1; }

        // If the buffer filled and no newline, drain and keep going (can't parse reliably).
        if ((size_t)rl == XWIS_LINE_CAP && line[rl - 1] != '\n') {
            (void)xwis_drain_to_newline(fd);
            continue;
        }

        const unsigned char *p = line;
        size_t n = (size_t)rl;
        xwis_trim_crlf(&p, &n);

        xwis_tok toks[2];
        int nt = xwis_split_tokens(p, n, toks, 2);
        if (nt >= 1 && xwis_tok_eq(&toks[0], want_numeric)) { free(line); return 0; }
    }

    free(line);
    return -1;
}

int nox_xwis_list_nox_games(nox_server_row *out, size_t out_cap, size_t *out_n)
{
    if (out_n) *out_n = 0;
    if (!out || out_cap == 0 || !out_n) { errno = EINVAL; return -1; }

    const char *host = xwis_env_str("NOX_XWIS_HOST", "xwis.net");
    int port = xwis_env_int("NOX_XWIS_PORT", 4000);
    if (port <= 0 || port > 65535) port = 4000;

    const char *nick_env = getenv("NOX_XWIS_NICK");
    char nick[32];
    if (nick_env && *nick_env) {
        snprintf(nick, sizeof(nick), "%s", nick_env);
        nick[sizeof(nick) - 1] = 0;
    } else {
        xwis_make_random_nick(nick, sizeof(nick));
    }

    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%d", port);

    int timeout_ms = xwis_env_int("NOX_LOBBY_CONNECT_TIMEOUT", 2000);
    if (timeout_ms < 100) timeout_ms = 100;
    if (timeout_ms > 60000) timeout_ms = 60000;

    int fd = xwis_connect_tcp(host, port_str, timeout_ms);
    if (fd < 0) return -1;

    char tmp[512];

    if (xwis_send_linef(fd, tmp, sizeof(tmp), "CVERS 11015 9472") < 0) goto fail;
    if (xwis_send_linef(fd, tmp, sizeof(tmp), "PASS supersecret") < 0) goto fail;
    if (xwis_send_linef(fd, tmp, sizeof(tmp), "NICK %s", nick) < 0) goto fail;
    if (xwis_send_linef(fd, tmp, sizeof(tmp), "apgar %s 0", nick) < 0) goto fail;
    if (xwis_send_linef(fd, tmp, sizeof(tmp), "USER UserName HostName %s :RealName", host) < 0) goto fail;

    if (xwis_read_until_numeric(fd, "376", 250) < 0) goto fail;

    if (xwis_send_linef(fd, tmp, sizeof(tmp), "LIST -1 37") < 0) goto fail;

    unsigned char *line = (unsigned char*)malloc(XWIS_LINE_CAP);
    if (!line) goto fail;

    size_t count = 0;

    for (;;) {
        int rl = xwis_recv_line_len(fd, line, XWIS_LINE_CAP);
        if (rl <= 0) { free(line); goto done_or_fail; }

        if ((size_t)rl == XWIS_LINE_CAP && line[rl - 1] != '\n') {
            // Overlong line: drain remainder and skip (rare, but prevents desync).
            (void)xwis_drain_to_newline(fd);
            continue;
        }

        const unsigned char *p = line;
        size_t n = (size_t)rl;
        xwis_trim_crlf(&p, &n);

        xwis_tok toks[10];
        int nt = xwis_split_tokens(p, n, toks, 10);
        if (nt < 1) continue;

        if (xwis_tok_eq(&toks[0], "323")) break;
        if (!xwis_tok_eq(&toks[0], "326")) continue;
        if (nt < 10) continue;
        if (count >= out_cap) continue;

        uint32_t ip_u32 = 0;
        if (xwis_parse_u32_tok(&toks[8], &ip_u32) != 0) continue;

        nox_server_row row;
        memset(&row, 0, sizeof(row));
        row.port = 18590;
        row.players_max = 31;

        xwis_u32_to_ipv4(ip_u32, row.addr, sizeof(row.addr));
        xwis_channel_to_name(&toks[2], row.name, sizeof(row.name));

        const unsigned char *payload = toks[9].p;
        size_t payload_len = toks[9].n;

        while (payload_len && xwis_is_space(*payload)) { payload++; payload_len--; }
        if (payload_len && payload[0] == ':') { payload++; payload_len--; } // IRC trailing param marker

        if (xwis_payload_has_g1p3(payload, payload_len)) {
            (void)xwis_decode_gameinfo_into_row(payload, payload_len, &row);
        }

        if (row.players_max == 0) row.players_max = 31;

        out[count++] = row;
    }

    free(line);

done_or_fail:
    (void)xwis_send_linef(fd, tmp, sizeof(tmp), "QUIT");
    close(fd);
    *out_n = count;
    return 0;

fail:
    (void)xwis_send_linef(fd, tmp, sizeof(tmp), "QUIT");
    close(fd);
    return -1;
}

// ============================================================================
// Host-side (persistent connection): JOINGAME + TOPIC + PART
// ============================================================================

static int g_xwis_host_fd = -1;
static int g_xwis_host_logged_in = 0;
static int g_xwis_host_joined = 0;
static char g_xwis_host_nick[32];
static char g_xwis_host_chan[64];

static void xwis_host_reset_state(void)
{
    g_xwis_host_fd = -1;
    g_xwis_host_logged_in = 0;
    g_xwis_host_joined = 0;
    g_xwis_host_nick[0] = 0;
    g_xwis_host_chan[0] = 0;
}

static int xwis_host_handshake(int fd, const char *host, const char *nick)
{
    char tmp[512];

    if (xwis_send_linef(fd, tmp, sizeof(tmp), "CVERS 11015 9472") < 0) return -1;
    if (xwis_send_linef(fd, tmp, sizeof(tmp), "PASS supersecret") < 0) return -1;
    if (xwis_send_linef(fd, tmp, sizeof(tmp), "NICK %s", nick) < 0) return -1;
    if (xwis_send_linef(fd, tmp, sizeof(tmp), "apgar %s 0", nick) < 0) return -1;
    if (xwis_send_linef(fd, tmp, sizeof(tmp), "USER UserName HostName %s :RealName", host) < 0) return -1;

    if (xwis_read_until_numeric(fd, "376", 250) < 0) return -1;
    return 0;
}

static int xwis_host_ensure_connected(void)
{
    if (g_xwis_host_fd >= 0 && g_xwis_host_logged_in) return 0;

    const char *host = xwis_env_str("NOX_XWIS_HOST", "xwis.net");
    int port = xwis_env_int("NOX_XWIS_PORT", 4000);
    if (port <= 0 || port > 65535) port = 4000;

    const char *nick_env = getenv("NOX_XWIS_NICK");
    if (nick_env && *nick_env) {
        snprintf(g_xwis_host_nick, sizeof(g_xwis_host_nick), "%s", nick_env);
        g_xwis_host_nick[sizeof(g_xwis_host_nick) - 1] = 0;
    } else {
        xwis_make_random_nick(g_xwis_host_nick, sizeof(g_xwis_host_nick));
    }

    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%d", port);

    int timeout_ms = xwis_env_int("NOX_LOBBY_CONNECT_TIMEOUT", 2000);
    if (timeout_ms < 100) timeout_ms = 100;
    if (timeout_ms > 60000) timeout_ms = 60000;

    int fd = xwis_connect_tcp(host, port_str, timeout_ms);
    if (fd < 0) return -1;

    if (xwis_host_handshake(fd, host, g_xwis_host_nick) != 0) {
        char tmp[256];
        (void)xwis_send_linef(fd, tmp, sizeof(tmp), "QUIT");
        close(fd);
        return -1;
    }

    g_xwis_host_fd = fd;
    g_xwis_host_logged_in = 1;
    g_xwis_host_joined = 0;
    g_xwis_host_chan[0] = 0;
    return 0;
}

static int xwis_host_ensure_joined(unsigned max_players)
{
    if (g_xwis_host_fd < 0 || !g_xwis_host_logged_in) return -1;
    if (g_xwis_host_joined && g_xwis_host_chan[0]) return 0;

    // Channel name: #<nick>'s_game
    snprintf(g_xwis_host_chan, sizeof(g_xwis_host_chan), "#%s's_game", g_xwis_host_nick);
    g_xwis_host_chan[sizeof(g_xwis_host_chan) - 1] = 0;

    char tmp[512];
    if (max_players == 0) max_players = 31;
    if (max_players > 31) max_players = 31;

    // Match Go: JOINGAME <channel> 1 <maxPlayers> 37 3 1 1 13893824
    if (xwis_send_linef(g_xwis_host_fd, tmp, sizeof(tmp),
                        "JOINGAME %s 1 %u 37 3 1 1 13893824",
                        g_xwis_host_chan, (unsigned)max_players) < 0) {
        return -1;
    }

    // Wait for end-of-names (366) for the channel join to be "complete"
    if (xwis_read_until_numeric(g_xwis_host_fd, "366", 200) < 0) {
        return -1;
    }

    g_xwis_host_joined = 1;
    return 0;
}

// --- payload encode/encrypt (ported from Go) ---

static const unsigned char g_xwis_header8[8] = {':','G','1','P','3', 0x9a, 0x03, 0x01};

static void xwis_encrypt_to(unsigned char *out, size_t out_cap, const unsigned char *data, size_t data_len)
{
    // Port of Go encryptTo(out, data):
    // expects out points to destination buffer (len >= data_len, but algorithm writes bit-packed bytes)
    int ind = 0;
    size_t cnt = 0;
    int loc = 0;
    int loc2 = 0;
    int acc = 0;

    (void)cnt;

    // The Go code loops i := 0; i <= len(data)-10; i++ { for j := 0; j <= 7; j++ { ... } }
    // Note: it effectively reads from data[ind] while advancing loc/ind, and writes packed bytes to out[cnt].
    // We'll mirror logic exactly.
    ind = 0;
    cnt = 0;
    loc = 0;
    loc2 = 0;
    acc = 0;

    if (!out || !data || data_len < 10) return;

    for (size_t i = 0; i + 10 <= data_len; i++) {
        for (int j = 0; j <= 7; j++) {
            if (loc == 8) {
                ind++;
                loc = 0;
            }
            if (loc2 == 7) {
                acc ^= 1 << 7;
                if (cnt >= out_cap) return;   // prevent overflow
                out[cnt] = (unsigned char)acc;
                cnt++;
                acc = 0;
                loc2 = 0;
            }
            unsigned char v6 = 0;
            if (ind < (int)data_len) v6 = data[ind];
            int v5 = 1 << loc;
            int v4 = ((int)v6) & v5;
            int v3 = v4 >> loc;
            acc ^= (v3 << loc2) & 0xFF;
            loc++;
            loc2++;
        }
    }
}

static uint16_t xwis_mode_to_maptype_bits(const char *mode)
{
    if (!mode || !*mode) return 0x0080; // chat default
    // minimal, consistent with decode table / Go MapType
    if (!strcasecmp(mode, "kotr")) return 0x0010;
    if (!strcasecmp(mode, "ctf")) return 0x0020;
    if (!strcasecmp(mode, "flagball")) return 0x0040;
    if (!strcasecmp(mode, "chat")) return 0x0080;
    if (!strcasecmp(mode, "arena")) return 0x0100;
    if (!strcasecmp(mode, "elimination")) return 0x0400;
    if (!strcasecmp(mode, "coop")) return 0x0A00;
    if (!strcasecmp(mode, "quest")) return 0x1000;
    return 0x0080;
}

static void xwis_fill_gameinfo(unsigned char *out69,
                               const char *name,
                               const char *map,
                               const char *mode,
                               unsigned cur,
                               unsigned max)
{
    // Layout per Go MarshalBinary (base 69 bytes + optional Unknown; we use Unknown=9 but only encrypt base+unknown rules;
    // For XWIS topic payload we only need the marshaled bytes; Go uses Unknown length 9.
    // We'll write 69 bytes here, with Unknown handled separately by caller if desired.
    memset(out69, 0, 69);

    // byte 0: access/disallow. AccessOpen=0, disallow=0
    out69[0] = 0;

    // byte 1: unk1 = 0xff
    out69[1] = 0xff;

    // byte 2: resolution + limit flag (we keep 0)
    out69[2] = 0;

    // byte 3: players
    out69[3] = (unsigned char)(cur & 0xFFu);

    // byte 4: max players (Go clamps 32->31)
    if (max == 32) max = 31;
    if (max == 0) max = 31;
    out69[4] = (unsigned char)(max & 0xFFu);

    // byte 5-6: min ping (0xffff means -1)
    out69[5] = 0xff;
    out69[6] = 0xff;

    // byte 7-8: max ping (0xffff means -1)
    out69[7] = 0xff;
    out69[8] = 0xff;

    // byte 9-10: unk2 = 0x489e (LE)
    out69[9]  = 0x9e;
    out69[10] = 0x48;

    // byte 11-19: map (9 bytes)
    if (map && *map) {
        size_t mlen = strlen(map);
        if (mlen > 9) mlen = 9;
        memcpy(&out69[11], map, mlen);
    }

    // byte 20-34: name (15 bytes)
    if (name && *name) {
        size_t nlen = strlen(name);
        if (nlen > 15) nlen = 15;
        memcpy(&out69[20], name, nlen);
    }

    // byte 35-62: unk3Data (28 bytes)
    // from Go: mostly 0xff, with byte5 (index 5) = 0xef
    for (int i = 0; i < 28; i++) out69[35 + i] = 0xff;
    out69[35 + 5] = 0xef;

    // byte 63-64: flags | maptype (LE). defaultFlags=8199 (0x2007)
    {
        uint16_t flags = 0x2007u;
        uint16_t mt = xwis_mode_to_maptype_bits(mode);
        flags = (uint16_t)(flags | mt);
        out69[63] = (unsigned char)(flags & 0xFFu);
        out69[64] = (unsigned char)((flags >> 8) & 0xFFu);
    }

    // byte 65-66 frag limit
    out69[65] = 0;
    out69[66] = 0;

    // byte 67-68 time limit
    out69[67] = 0;
    out69[68] = 0;
}

static int xwis_send_topic_raw(int fd, const char *channel, const unsigned char *payload, size_t payload_len)
{
    if (fd < 0 || !channel || !*channel || !payload || payload_len == 0) return -1;

    // Send: "TOPIC <channel> " + payload + "\r\n"
    const char *prefix1 = "TOPIC ";
    const char *sp = " ";

    if (xwis_send_all(fd, prefix1, strlen(prefix1)) < 0) return -1;
    if (xwis_send_all(fd, channel, strlen(channel)) < 0) return -1;
    if (xwis_send_all(fd, sp, 1) < 0) return -1;
    if (xwis_send_all(fd, (const void*)payload, payload_len) < 0) return -1;
    if (xwis_send_all(fd, "\r\n", 2) < 0) return -1;
    return 0;
}

int nox_xwis_host_update_game(const char *name,
                             const char *map,
                             const char *mode,
                             unsigned cur,
                             unsigned max,
                             unsigned port)
{
    (void)port; // XWIS topic payload doesn’t carry port in the pasted Go code; keep signature for callers.

    if (xwis_host_ensure_connected() != 0) {
        nox_xwis_host_stop();
        return -1;
    }

    if (xwis_host_ensure_joined(max) != 0) {
        // Connection may be desynced; drop and let caller retry later.
        nox_xwis_host_stop();
        return -1;
    }

    // Build GameInfo bytes: 69 + Unknown(9)
    unsigned char gdata[69 + 9];
    memset(gdata, 0, sizeof(gdata));
    xwis_fill_gameinfo(gdata, name ? name : "", map ? map : "", mode ? mode : "", cur, max);
    // Unknown bytes (9) already zero.

    // Encrypt (bit-pack) into payload tail. Go allocates headerLength+len(gdata), then encryptTo writes into tail.
    unsigned char enc[sizeof(gdata)];
    memset(enc, 0, sizeof(enc));
    // encryptTo uses data_len >= 10; ours is 78 so OK
    xwis_encrypt_to(enc, sizeof(enc), gdata, sizeof(gdata));

    // Build full payload: "128:" + header8 + enc...
    // This results in "128::G1P3..." because header8 begins with ':'
    unsigned char payload[4 + 8 + sizeof(enc)];
    size_t payload_len = 0;
    payload[payload_len++] = '1';
    payload[payload_len++] = '2';
    payload[payload_len++] = '8';
    payload[payload_len++] = ':';
    memcpy(payload + payload_len, g_xwis_header8, sizeof(g_xwis_header8));
    payload_len += sizeof(g_xwis_header8);
    memcpy(payload + payload_len, enc, sizeof(enc));
    payload_len += sizeof(enc);

    if (xwis_send_topic_raw(g_xwis_host_fd, g_xwis_host_chan, payload, payload_len) != 0) {
        nox_xwis_host_stop();
        return -1;
    }

    return 0;
}

void nox_xwis_host_stop(void)
{
    if (g_xwis_host_fd < 0) {
        xwis_host_reset_state();
        return;
    }

    char tmp[256];

    if (g_xwis_host_joined && g_xwis_host_chan[0]) {
        (void)xwis_send_linef(g_xwis_host_fd, tmp, sizeof(tmp), "PART %s", g_xwis_host_chan);
    }
    (void)xwis_send_linef(g_xwis_host_fd, tmp, sizeof(tmp), "QUIT");
    close(g_xwis_host_fd);

    xwis_host_reset_state();
}