// netextras.c
//
// Standalone “network extras” module extracted from compat.c.
//
// This file provides:
//  - Internet server injection (fetch list JSON, parse, queue fake server-info packets)
//  - Fake recvfrom delivery (drain queued fake packets before calling real recvfrom)
//  - Lobby registration helpers (kickoff + polling thread) for hosted games
//  - NET/PACKET logging toggles
//
// Intended patch points in game code (per your traces):
//   - sub_554C80(): before real sendto(), call nox_netextras_on_discovery_ping_send(sockfd, buf, len, dest_port)
//   - sub_554D70(): before real recvfrom(), call nox_netextras_try_fake_recvfrom(...)
//   - sub_554380(): after successful bind() for host socket, call nox_netextras_on_host_bind_success(sockfd, bound_port)
//
// Notes:
//  - Uses SDL threads/mutex for portability (Windows + Linux).
//  - Does NOT wrap sendto/recvfrom globally; you call these helpers from patched sub_*.
//
// Build assumptions:
//  - You have SDL2 available.
//  - The external “nox_*” helpers/types referenced below exist somewhere (likely already in proto.h / your codebase).
//  - If any symbols differ in your tree, adjust the extern decls accordingly.

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>

#include <SDL2/SDL_stdinc.h>
#include <SDL2/SDL_thread.h>
#include <SDL2/SDL_mutex.h>
#include <SDL2/SDL_atomic.h>
#include <SDL2/SDL_timer.h>

#ifdef _WIN32
  #include <winsock2.h>
  #include <ws2tcpip.h>
  typedef int socklen_t;
#else
  #include <unistd.h>
  #include <sys/time.h>
  #include <sys/types.h>
  #include <sys/socket.h>
  #include <arpa/inet.h>
  #include <netinet/in.h>
#endif

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

#include "netextras_types.h"


/* ---- IPv4-only inet_pton shim (avoids missing inet_pton link issues) ---- */
static int nox_inet_pton_ipv4(const char *src, struct in_addr *dst)
{
    if (!src || !dst) return 0;

    /* inet_addr returns INADDR_NONE for invalid strings AND for 255.255.255.255 */
    unsigned long a = inet_addr(src);
    if (a == INADDR_NONE && strcmp(src, "255.255.255.255") != 0) {
        return 0;
    }

    dst->s_addr = a;
    return 1;
}

static int looks_like_ipv4(const char *s)
{
    if (!s || !*s) return 0;

    // reject obvious non-ip
    for (const char *p = s; *p; ++p) {
        if (!((*p >= '0' && *p <= '9') || *p == '.')) return 0;
    }

    // inet_addr accepts some weird forms; but good enough as a gate here
    unsigned long a = inet_addr(s);
    if (a == INADDR_NONE && strcmp(s, "255.255.255.255") != 0) return 0;

    return 1;
}


// Pull games list JSON via HTTP (or whatever your implementation does).
extern int nox_fetch_games_list_json(char *out, size_t outsz);

// Parse JSON into rows; returns number of rows filled.
extern size_t nox_parse_games_list_json(const char *json, nox_server_row *out, size_t out_cap);

// Env helpers
extern int  nox_env_int(const char *name, int defval);
extern int  nox_env_truthy(const char *s);

// Filtering (deny lists)
extern int  nox_is_bad_server_ip(const char *ip);
extern int  nox_is_bad_server_name(const char *name);

// Mode bits and flags baseline cache
extern void     nox_lobby_set_last_serverinfo_flags(uint16_t flags);
extern uint16_t nox_lobby_get_last_serverinfo_flags(void);

// Convert mode string to a bitmask that lives under NOX_MODE_MASK.
extern uint16_t nox_mode_to_flagbit(const char *mode);

// Your mode mask constant must be visible to this compilation unit.
// If it’s not globally visible, move it to a header.
#ifndef NOX_MODE_MASK
// Fallback: define to 0 so compilation succeeds, but you should override with real mask.
#define NOX_MODE_MASK 0
#endif

// UPnP optional
extern int nox_upnp_ensure_mapped_from_env(void);

// Lobby register API
extern int nox_lobby_register_game(const char *name, const char *map,
                                  unsigned cur, unsigned max, unsigned port);

// Packet builder used for serverinfo (your existing function).
// Signature based on your compat.c usage.
extern int sub_554040(int *dummy3, int outcap, char *outbuf);

// -----------------------------------------------------------------------------
// Logging toggles (env-controlled)
// -----------------------------------------------------------------------------
static int g_netlog(void) {
    static int v = -1;
    if (v < 0) {
        const char *e = getenv("NOX_NET_LOG");
        v = (e && *e && strcmp(e, "0") != 0) ? 1 : 0;
    }
    return v;
}
#define NETLOG(...) do { if (g_netlog()) fprintf(stderr, __VA_ARGS__); } while (0)

static int g_packetlog(void) {
    static int v = -1;
    if (v < 0) {
        const char *e = getenv("NOX_PACKET_LOG");
        v = (e && *e && strcmp(e, "0") != 0) ? 1 : 0;
    }
    return v;
}
#define PACKETLOG(...) do { if (g_packetlog()) fprintf(stderr, __VA_ARGS__); } while (0)

// -----------------------------------------------------------------------------
// Small utilities
// -----------------------------------------------------------------------------
static void compat_hexdump(const char *tag, const void *buf, size_t len)
{
    if (!g_packetlog()) return;

    const unsigned char *p = (const unsigned char *)buf;
    fprintf(stderr, "%s len=%zu", tag ? tag : "hexdump", len);
    for (size_t i = 0; i < len; ++i) {
        if ((i % 16) == 0) fprintf(stderr, "\n  %04zx:", i);
        fprintf(stderr, " %02x", p[i]);
    }
    fprintf(stderr, "\n");
}

static long nox_now_sec(void)
{
#ifdef _WIN32
    // SDL ticks are ms; convert to sec. Good enough for rate limiting.
    return (long)(SDL_GetTicks() / 1000u);
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (long)tv.tv_sec;
#endif
}

// -----------------------------------------------------------------------------
// Feature toggles
// -----------------------------------------------------------------------------
static int nox_should_inject_internet_servers(void) {
    // default ON unless disabled
    const char *e = getenv("NOX_INJECT_INTERNET_SERVERS");
    if (!e) return 1;
    return (strcmp(e, "0") != 0);
}

static int nox_should_register_lobby(void) {
    // default OFF unless enabled
    return nox_env_truthy(getenv("NOX_LOBBY_REGISTER_ENABLE"));
}

static int nox_lobby_register_period_sec(void) {
    int v = nox_env_int("NOX_LOBBY_REGISTER_PERIOD", 20);
    if (v < 5) v = 5;
    return v;
}

static unsigned nox_server_port_override_or(unsigned defport)
{
    const char *p = getenv("NOX_SERVER_PORT");
    if (p && *p) {
        int pv = atoi(p);
        if (pv > 0 && pv < 65536) return (unsigned)pv;
    }
    return defport;
}

// -----------------------------------------------------------------------------
// Fake packet queue (for injected internet servers)
// -----------------------------------------------------------------------------
struct fake_srv_pkt {
    int used;
    int sockfd;
    size_t len;
    unsigned char data[256];
    struct sockaddr_in from;
};

static struct fake_srv_pkt g_fake_packets[4];  // up to 4 servers per tick
static SDL_mutex *g_fake_mu = NULL;

static void netextras_init_once(void)
{
    static SDL_atomic_t inited = {0};
    if (SDL_AtomicCAS(&inited, 0, 1)) {
        g_fake_mu = SDL_CreateMutex();
        if (!g_fake_mu) {
            fprintf(stderr, "netextras: SDL_CreateMutex failed\n");
        }
    }
}

static struct fake_srv_pkt *alloc_fake_slot_locked(void)
{
    for (int i = 0; i < (int)(sizeof(g_fake_packets)/sizeof(g_fake_packets[0])); ++i) {
        if (!g_fake_packets[i].used) {
            g_fake_packets[i].used = 1;   // reserve immediately
            g_fake_packets[i].sockfd = -1;
            g_fake_packets[i].len = 0;
            memset(&g_fake_packets[i].from, 0, sizeof(g_fake_packets[i].from));
            return &g_fake_packets[i];
        }
    }
    return NULL;
}

// -----------------------------------------------------------------------------
// Server list cache (fetched from lobby)
// -----------------------------------------------------------------------------
static int g_srv_cache_valid = 0;
static nox_server_row g_srv_cache[32];
static size_t g_srv_cache_n = 0;
static size_t g_srv_rr = 0;

static long g_srv_cache_last_ok = 0;
static long g_srv_cache_next_try = 0;

static void nox_refresh_server_cache_once(void)
{
    int ttl = nox_env_int("NOX_SERVER_CACHE_TTL", 30);
    if (ttl < 30) ttl = 30;

    long now = nox_now_sec();

    if (g_srv_cache_valid) {
        if (ttl == 0) return;
        if (g_srv_cache_last_ok != 0 && (now - g_srv_cache_last_ok) < (long)ttl) return;
        if (g_srv_cache_next_try != 0 && now < g_srv_cache_next_try) return;
    }

    char json[64 * 1024];
    int n = nox_fetch_games_list_json(json, sizeof(json));
    if (n <= 0) {
        fprintf(stderr, "netextras: fetch games/list failed\n");
        // Keep any existing cache; just back off retries.
        g_srv_cache_valid = 1;
        g_srv_cache_next_try = now + 10;
        return;
    }

    g_srv_cache_n = nox_parse_games_list_json(
        json, g_srv_cache, sizeof(g_srv_cache) / sizeof(g_srv_cache[0])
    );

    g_srv_cache_valid = 1;
    g_srv_cache_last_ok = now;
    g_srv_cache_next_try = 0;

    NETLOG("netextras: games/list parsed %zu servers\n", g_srv_cache_n);
}

// -----------------------------------------------------------------------------
// Packet crafting: fake server-info reply (type 0x0D)
// -----------------------------------------------------------------------------
static void queue_internet_server_reply(int sockfd, const unsigned char *ping, size_t ping_len)
{
    if (!nox_should_inject_internet_servers()) return;

    nox_refresh_server_cache_once();
    if (g_srv_cache_n == 0) return;

    netextras_init_once();
    if (!g_fake_mu) return;

    SDL_LockMutex(g_fake_mu);

    size_t queued = 0;
    size_t slots_avail = sizeof(g_fake_packets)/sizeof(g_fake_packets[0]);

    for (size_t tries = 0; tries < g_srv_cache_n && queued < slots_avail; ++tries) {
        struct fake_srv_pkt *slot = alloc_fake_slot_locked();
        if (!slot) break;

        const nox_server_row *row = &g_srv_cache[(g_srv_rr + tries) % g_srv_cache_n];

        if (row->addr[0] && nox_is_bad_server_ip(row->addr)) {
            slot->used = 0;
            NETLOG("netextras: skip row (bad ip) addr='%s' name='%s'\n", row->addr, row->name);
            continue;
        }
        if (row->name[0] && nox_is_bad_server_name(row->name)) {
            slot->used = 0;
            NETLOG("netextras: skip row (bad name) addr='%s' name='%s'\n", row->addr, row->name);
            continue;
        }



        unsigned char *out = slot->data;
        memset(out, 0, sizeof(slot->data));

        // Header / type
        out[0] = 0;
        out[1] = 0;
        out[2] = 0x0D; // server info
        out[3] = row->players_cur;
        out[4] = row->players_max ? row->players_max : 31;
        out[5] = 0x0F;
        out[6] = 0x0F;

        // Session id placeholder
        {
            uint32_t sid = 0xFFFFFFFFu;
            memcpy(&out[7], &sid, sizeof(sid));
        }

        // Map name @ offset 10
        if (row->map[0]) {
            size_t mlen = strnlen(row->map, 31);
            memcpy(&out[10], row->map, mlen);
            out[10 + mlen] = 0;
        }

        // Flags @ 28..29 (LE), preserve baseline and apply mode bits.
        uint16_t base = nox_lobby_get_last_serverinfo_flags();
        if (base == 0) base = 0x2007; // known-good baseline from your real packet
        uint16_t modebit = nox_mode_to_flagbit(row->mode);
        uint16_t flags = (uint16_t)((base & (uint16_t)~NOX_MODE_MASK) | modebit);

        out[28] = (unsigned char)(flags & 0xFF);
        out[29] = (unsigned char)((flags >> 8) & 0xFF);

        // Echo ping cookie (bytes 8..11) into offset 44 (your previous behavior)
        if (ping && ping_len >= 12) {
            memcpy(&out[44], &ping[8], 4);
        }

        // Server name @ offset 72
        if (row->name[0]) {
            strncpy((char *)&out[72], row->name, sizeof(slot->data) - 72 - 1);
            out[sizeof(slot->data) - 1] = 0;
        }

        // Compute length: 72 + name + NUL
        {
            size_t name_len = strnlen((char *)&out[72], sizeof(slot->data) - 72);
            size_t min_len = 136;                 // <-- IMPORTANT: stable “original-like” size
            size_t total_len = 72 + name_len + 1; // keep name + NUL included
            if (total_len < min_len) total_len = min_len;
            if (total_len > sizeof(slot->data)) total_len = sizeof(slot->data);
            slot->len = total_len;
        }

        memset(&slot->from, 0, sizeof(slot->from));
        slot->from.sin_family = AF_INET;
        slot->from.sin_port   = htons(row->port ? row->port : 18590);
        if (!nox_inet_pton_ipv4(row->addr, &slot->from.sin_addr)) {
            NETLOG("netextras: skip row (unparseable ipv4) addr='%s' name='%s'\n", row->addr, row->name);
            slot->used = 0;
            continue;
        }

        slot->sockfd = sockfd;
        slot->used = 1;
        queued++;

        NETLOG("netextras: queued internet server '%s' (%s:%u) map=%s %u/%u flags=0x%04x\n",
               row->name, row->addr, (unsigned)(row->port ? row->port : 18590),
               row->map, (unsigned)row->players_cur, (unsigned)(row->players_max ? row->players_max : 31),
               (unsigned)flags);

        compat_hexdump("NET FAKE QUEUE", slot->data, slot->len);
    }

    if (g_srv_cache_n) {
        g_srv_rr = (g_srv_rr + queued) % g_srv_cache_n;
    }

    SDL_UnlockMutex(g_fake_mu);
}

// -----------------------------------------------------------------------------
// Public API: called from patched sub_554C80() (discovery ping send)
// -----------------------------------------------------------------------------
void nox_netextras_on_discovery_ping_send(int sockfd,
                                         const void *buf,
                                         size_t len,
                                         unsigned dest_port)
{
    if (!buf || len < 3) return;
    if (dest_port != 18590) return;

    const unsigned char *p = (const unsigned char *)buf;
    if (p[2] != 0x0C) return; // discovery ping

    NETLOG("netextras: discovery ping seen on fd=%d len=%zu -> queue internet replies\n",
           sockfd, len);

    queue_internet_server_reply(sockfd, p, len);
}

// -----------------------------------------------------------------------------
// Public API: try to satisfy recvfrom() from fake queue.
// Return values:
//   >= 0 : number of bytes written (a fake packet was returned)
//   -1   : error (rare; will set errno)
//   -2   : no fake packet available (caller should call real recvfrom)
// -----------------------------------------------------------------------------
int nox_netextras_try_fake_recvfrom(int sockfd,
                                   void *buffer,
                                   size_t length,
                                   struct sockaddr *addr,
                                   socklen_t *addrlen)
{
    if (!buffer) { errno = EINVAL; return -1; }

    netextras_init_once();
    if (!g_fake_mu) return -2;

    SDL_LockMutex(g_fake_mu);

    for (int i = 0; i < (int)(sizeof(g_fake_packets)/sizeof(g_fake_packets[0])); ++i) {
        struct fake_srv_pkt *slot = &g_fake_packets[i];
        if (!slot->used) continue;
        if (slot->sockfd != sockfd) continue;
        if (slot->len > length) continue;

        memset(buffer, 0, length);
        memcpy(buffer, slot->data, slot->len);

        if (addr && addrlen) {
            unsigned want = (unsigned)(*addrlen);
            if (want >= sizeof(struct sockaddr_in)) {
                memcpy(addr, &slot->from, sizeof(struct sockaddr_in));
                *addrlen = (socklen_t)sizeof(struct sockaddr_in);
            } else {
                *addrlen = (socklen_t)sizeof(struct sockaddr_in);
            }
        }

        int ret = (int)slot->len;
        slot->used = 0;

        SDL_UnlockMutex(g_fake_mu);

        NETLOG("netextras: returning fake serverinfo (%d bytes) on fd=%d\n", ret, sockfd);
        compat_hexdump("NET FAKE RX", buffer, (size_t)ret);
        return ret;
    }

    SDL_UnlockMutex(g_fake_mu);
    return -2;
}

// -----------------------------------------------------------------------------
// Lobby registration (host side)
// -----------------------------------------------------------------------------
typedef struct {
    char name[64];
    char map[32];
    unsigned cur;
    unsigned max;
    unsigned port;
} reg_job_t;

static SDL_mutex *g_lobby_mu = NULL;
static SDL_atomic_t g_lobby_init_state = {0};
static int g_lobby_reg_inflight = 0;
static long g_last_lobby_reg = 0;
static unsigned g_lobby_bound_port = 18590;
static unsigned char g_lobby_serverinfo[256];
static size_t g_lobby_serverinfo_len = 0;
static int g_lobby_host_active = 0;
static Uint32 g_lobby_last_snapshot_tick = 0;

static int lobby_init_once(void)
{
    int state = SDL_AtomicGet(&g_lobby_init_state);
    if (state == 2) return g_lobby_mu ? 0 : -1;

    if (SDL_AtomicCAS(&g_lobby_init_state, 0, 1)) {
        g_lobby_mu = SDL_CreateMutex();
        SDL_AtomicSet(&g_lobby_init_state, 2);
    } else {
        while (SDL_AtomicGet(&g_lobby_init_state) == 1) {
            SDL_Delay(0);
        }
    }

    return g_lobby_mu ? 0 : -1;
}

static int lobby_register_thread_fn(void *arg)
{
    reg_job_t *j = (reg_job_t *)arg;
    if (!j) return 0;

    // Optional best-effort UPnP mapping. Do not enter the UPnP module at all
    // when it is disabled; registration itself does not depend on UPnP.
    if (nox_env_truthy(getenv("NOX_UPNP_ENABLE"))) {
        (void)nox_upnp_ensure_mapped_from_env();
    }

    int rc = nox_lobby_register_game(j->name, j->map, j->cur, j->max, j->port);
    if (rc != 0) {
        fprintf(stderr, "netextras: lobby register FAILED name='%s' map='%s'\n", j->name, j->map);
    } else {
        NETLOG("netextras: lobby register OK name='%s' map='%s'\n", j->name, j->map);
    }

    free(j);

    if (lobby_init_once() == 0) {
        SDL_LockMutex(g_lobby_mu);
        g_lobby_reg_inflight = 0;
        SDL_UnlockMutex(g_lobby_mu);
    }

    return 0;
}

static void maybe_register_lobby_from_serverinfo(int sockfd, const unsigned char *pkt, size_t len)
{
    (void)sockfd;

    if (!nox_should_register_lobby()) return;
    if (!pkt || len < 73) return;
    if (pkt[2] != 0x0D) return;
    if (lobby_init_once() != 0) return;

    // Cache the last packet built by the game thread. The periodic lobby worker
    // reuses this snapshot instead of calling into mutable game state itself.
    unsigned bound_port;
    SDL_LockMutex(g_lobby_mu);
    g_lobby_serverinfo_len = len < sizeof(g_lobby_serverinfo) ? len : sizeof(g_lobby_serverinfo);
    memcpy(g_lobby_serverinfo, pkt, g_lobby_serverinfo_len);
    bound_port = g_lobby_bound_port;
    SDL_UnlockMutex(g_lobby_mu);

    // Cache serverinfo flags for later baseline use.
    if (len >= 30) {
        uint16_t flags = (uint16_t)((unsigned)pkt[28] | ((unsigned)pkt[29] << 8));
        nox_lobby_set_last_serverinfo_flags(flags);
        NETLOG("netextras: serverinfo flags=0x%04x\n", (unsigned)flags);
    }

    reg_job_t *j = (reg_job_t *)calloc(1, sizeof(*j));
    if (!j) return;

    j->cur = (unsigned)(uint8_t)pkt[3];
    j->max = (unsigned)(uint8_t)pkt[4];

    strncpy(j->map,  (const char *)&pkt[10], sizeof(j->map)  - 1);
    strncpy(j->name, (const char *)&pkt[72], sizeof(j->name) - 1);

    // Default to the port that actually bound. Keep NOX_SERVER_PORT as an
    // explicit external-port override for existing setups.
    j->port = nox_server_port_override_or(bound_port ? bound_port : 18590);

    long now = nox_now_sec();
    const int period = nox_lobby_register_period_sec();

    SDL_LockMutex(g_lobby_mu);

    if (g_lobby_reg_inflight ||
        (g_last_lobby_reg != 0 && (now - g_last_lobby_reg) < period)) {
        SDL_UnlockMutex(g_lobby_mu);
        free(j);
        return;
    }

    g_last_lobby_reg = now;
    g_lobby_reg_inflight = 1;

    SDL_UnlockMutex(g_lobby_mu);

    SDL_Thread *th = SDL_CreateThread(lobby_register_thread_fn, "lobby_register", j);
    if (th) {
        SDL_DetachThread(th);
    } else {
        free(j);
        SDL_LockMutex(g_lobby_mu);
        g_lobby_reg_inflight = 0;
        SDL_UnlockMutex(g_lobby_mu);
    }
}

void nox_netextras_on_host_serverinfo(const void *packet, size_t len)
{
    maybe_register_lobby_from_serverinfo(-1, (const unsigned char *)packet, len);
}

/*
 * Lobby server-info must be built on the normal game/network thread because
 * sub_554040() walks live game state. Periodic refresh is driven by
 * nox_netextras_host_tick() below; the detached worker only performs HTTP.
 */

// Build one server-info immediately on the caller/game thread and attempt a
// single registration. Periodic refreshes only reuse the cached result.
static void lobby_build_serverinfo_on_game_thread(int sockfd)
{
    unsigned char buf[256];
    int dummy[3] = {0,0,0};
    int len = sub_554040(dummy, (int)sizeof(buf), (char *)buf);
    if (len > 0 && len >= 73 && buf[2] == 0x0D) {
        maybe_register_lobby_from_serverinfo(sockfd, buf, (size_t)len);
    }
}

void nox_netextras_host_tick(void)
{
    if (!nox_should_register_lobby()) return;
    if (lobby_init_once() != 0) return;

    Uint32 now = SDL_GetTicks();
    int should_refresh = 0;

    SDL_LockMutex(g_lobby_mu);
    if (g_lobby_host_active &&
        (g_lobby_last_snapshot_tick == 0 ||
         (Uint32)(now - g_lobby_last_snapshot_tick) >= 1000u)) {
        g_lobby_last_snapshot_tick = now ? now : 1u;
        should_refresh = 1;
    }
    SDL_UnlockMutex(g_lobby_mu);

    if (!should_refresh) return;

    /* Called from the game network pump, so reading live game state is safe.
       Registration HTTP itself remains asynchronous. */
    lobby_build_serverinfo_on_game_thread(-1);
}

// -----------------------------------------------------------------------------
// Public API: called from patched sub_554380() after successful host bind()
// -----------------------------------------------------------------------------
void nox_netextras_on_host_bind_success(int sockfd, unsigned bound_port)
{
    (void)sockfd;

    if (!nox_should_register_lobby()) return;

    if (lobby_init_once() != 0) return;

    SDL_LockMutex(g_lobby_mu);
    g_lobby_bound_port = bound_port ? bound_port : 18590;
    g_lobby_serverinfo_len = 0;
    g_lobby_host_active = 1;
    g_lobby_last_snapshot_tick = 0;
    g_last_lobby_reg = 0;
    SDL_UnlockMutex(g_lobby_mu);

    NETLOG("netextras: host bind success fd=%d port=%u\n", sockfd, bound_port);

    /* Try immediately, then let the normal network pump retry once the hosted
       game has finished initializing. */
    lobby_build_serverinfo_on_game_thread(sockfd);
}

int nox_netextras_fake_pending(int sockfd)
{
    netextras_init_once();
    if (!g_fake_mu) return 0;

    SDL_LockMutex(g_fake_mu);
    for (int i = 0; i < (int)(sizeof(g_fake_packets)/sizeof(g_fake_packets[0])); ++i) {
        if (g_fake_packets[i].used && g_fake_packets[i].sockfd == sockfd) {
            SDL_UnlockMutex(g_fake_mu);
            return 1;
        }
    }
    SDL_UnlockMutex(g_fake_mu);
    return 0;
}

