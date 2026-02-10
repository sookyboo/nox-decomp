/* Minimal model for your endpoint */
typedef struct nox_server_row {
    char     name[64];
    char     addr[32];     /* IPv4 string */
    uint16_t port;
    char     map[32];
    uint8_t  players_cur;
    uint8_t  players_max;
    char mode[16];
} nox_server_row;

// Game mode flags (from opennox/noxflags)
#define NOX_GF_MODE_KOTR        0x0010
#define NOX_GF_MODE_CTF         0x0020
#define NOX_GF_MODE_FLAGBALL    0x0040
#define NOX_GF_MODE_CHAT        0x0080
#define NOX_GF_MODE_ARENA       0x0100
#define NOX_GF_MODE_COOPTEAM    0x0200
#define NOX_GF_MODE_ELIMINATION 0x0400
#define NOX_GF_MODE_COOP        0x0800
#define NOX_GF_MODE_QUEST       0x1000

#define NOX_MODE_MASK (NOX_GF_MODE_KOTR|NOX_GF_MODE_CTF|NOX_GF_MODE_FLAGBALL|NOX_GF_MODE_CHAT|NOX_GF_MODE_ARENA|NOX_GF_MODE_COOPTEAM|NOX_GF_MODE_ELIMINATION|NOX_GF_MODE_COOP|NOX_GF_MODE_QUEST)


/* HTTP: fetch response body (NUL-terminated if it fits). Returns body size or -1. */
//int nox_http_get_body(const char *host,
//                      uint16_t port,
//                      const char *path,
//                      char *out,
//                      size_t out_cap);

/* Convenience wrapper for your endpoint: /api/v0/games/list */
//int nox_fetch_games_list_json(char *out, size_t out_cap);

/* JSON: parse endpoint into rows. Returns number of rows written (<= out_cap). */
//size_t nox_parse_games_list_json(const char *json, nox_server_row *out, size_t out_cap);

//int nox_should_inject_internet_servers(void);

//int nox_is_bad_server_ip(const char *ip);
//int nox_is_bad_server_name(const char *name);


//int nox_lobby_register_game(const char *name,
//                           const char *map,
//                           unsigned cur,
//                           unsigned max,
//                           unsigned port);

/* Best-effort: reads NOX_UPNP_* env vars and tries to map.
  Safe to call repeatedly; internally rate-limited and cached.
  Returns 0 on success (or already mapped), -1 on failure/disabled. */
//int nox_upnp_ensure_mapped_from_env(void);

/* Best-effort cleanup (also registered via atexit on first success). */
void nox_upnp_cleanup(void);

//void nox_lobby_set_last_serverinfo_flags(uint16_t flags);
//uint16_t nox_lobby_get_last_serverinfo_flags(void);
//uint16_t nox_mode_to_flagbit(const char *mode);