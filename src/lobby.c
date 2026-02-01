// To reduce the libraries needed for the ARM port decided to not add additional deps

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

/* env: comma-separated deny lists (defaults used when env missing) */
#define NOX_ENV_BAD_IPS   "NOX_BAD_SERVER_IPS"
#define NOX_ENV_BAD_NAMES "NOX_BAD_SERVER_NAMES"

/* defaults if env not set (edit these to whatever you want) */
#define NOX_DEFAULT_BAD_IPS   ""
#define NOX_DEFAULT_BAD_NAMES ""

/* ----- csv helpers ----- */
static const char *csv_next_token(const char *s, char *tok, size_t tok_sz)
{
    /* Returns pointer to char after next comma, or NULL if end. Writes token trimmed. */
    if (!s || !*s || !tok || tok_sz == 0) return NULL;

    /* skip leading ws/commas */
    while (*s == ',' || (unsigned char)*s <= ' ') s++;
    if (!*s) return NULL;

    /* copy until comma/end */
    size_t n = 0;
    const char *start = s;
    while (*s && *s != ',') {
        if (n + 1 < tok_sz) tok[n++] = *s;
        s++;
    }
    tok[n] = 0;

    /* trim right */
    while (n && (unsigned char)tok[n - 1] <= ' ') tok[--n] = 0;

    /* advance past comma if present */
    if (*s == ',') s++;

    /* if token is empty after trimming, continue */
    if (tok[0] == 0) return csv_next_token(s, tok, tok_sz);

    return s;
}

static int str_eq_case(const char *a, const char *b)
{
    if (!a || !b) return 0;
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return 0;
        a++; b++;
    }
    return *a == 0 && *b == 0;
}

static int csv_contains_case(const char *csv, const char *value)
{
    char tok[128];
    const char *p = csv;
    if (!csv || !value || !*value) return 0;

    while ((p = csv_next_token(p, tok, sizeof(tok))) != NULL) {
        if (str_eq_case(tok, value)) return 1;
    }
    return 0;
}

static int csv_contains_exact(const char *csv, const char *value)
{
    char tok[64];
    const char *p = csv;
    if (!csv || !value || !*value) return 0;

    while ((p = csv_next_token(p, tok, sizeof(tok))) != NULL) {
        if (strcmp(tok, value) == 0) return 1;
    }
    return 0;
}

/* ----- env helpers ----- */
static int nox_env_truthy(const char *s)
{
    if (!s) return 0;
    while (*s && (unsigned char)*s <= ' ') s++;
    if (!*s) return 0;

    /* accept: 1/true/yes/on */
    if (s[0] == '1' && s[1] == 0) return 1;

    char buf[8];
    size_t n = 0;
    while (s[n] && n + 1 < sizeof(buf)) {
        buf[n] = (char)tolower((unsigned char)s[n]);
        n++;
    }
    buf[n] = 0;

    return (strcmp(buf, "true") == 0) ||
           (strcmp(buf, "yes")  == 0) ||
           (strcmp(buf, "on")   == 0);
}

static uint16_t nox_env_u16(const char *name, uint16_t defv)
{
    const char *s = getenv(name);
    if (!s || !*s) return defv;

    unsigned v = 0;
    while (*s && (unsigned char)*s <= ' ') s++;
    if (!*s) return defv;

    while (*s && isdigit((unsigned char)*s)) {
        v = v * 10u + (unsigned)(*s - '0');
        if (v > 65535u) return defv;
        s++;
    }
    return (uint16_t)v;
}

int nox_env_int(const char *name, int defv)
{
    const char *s = getenv(name);
    if (!s || !*s) return defv;
    /* atoi is fine here; tolerate garbage -> 0 */
    return atoi(s);
}

static const char *nox_env_str(const char *name, const char *defv)
{
    const char *s = getenv(name);
    return (s && *s) ? s : defv;
}

/* centralized config */
int nox_should_inject_internet_servers(void)
{
    /* default: inject (return 1). If NOX_NO_INTERNET_SERVERS is truthy -> disable. */
    const char *v = getenv("NOX_NO_INTERNET_SERVERS");
    return nox_env_truthy(v) ? 0 : 1;
}

static const char *nox_lobby_host(void)
{
    return nox_env_str("NOX_LOBBY_HOST", "nox.nwca.xyz");
}

static uint16_t nox_lobby_port(void)
{
    return nox_env_u16("NOX_LOBBY_PORT", 8088);
}

static const char *nox_lobby_path(void)
{
    return nox_env_str("NOX_LOBBY_PATH", "/api/v0/games/list");
}

/* ----- tiny helpers ----- */

static int str_case_contains(const char *hay, const char *needle)
{
    /* very small case-insensitive substring search */
    size_t nlen = strlen(needle);
    if (!hay || !needle || !nlen) return 0;

    for (const char *p = hay; *p; ++p) {
        size_t i = 0;
        while (i < nlen) {
            char a = p[i];
            char b = needle[i];
            if (!a) return 0;
            if (tolower((unsigned char)a) != tolower((unsigned char)b)) break;
            i++;
        }
        if (i == nlen) return 1;
    }
    return 0;
}

static const char *skip_ws(const char *p) {
    while (p && *p && (unsigned char)*p <= ' ') p++;
    return p;
}

static int connect_tcp(const char *host, const char *port_str)
{
    struct addrinfo hints;
    struct addrinfo *res = NULL, *it = NULL;
    int fd = -1;

    memset(&hints, 0, sizeof(hints));
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_family   = AF_UNSPEC;

    int rc = getaddrinfo(host, port_str, &hints, &res);
    if (rc != 0 || !res) {
        return -1;
    }

    for (it = res; it; it = it->ai_next) {
        fd = (int)socket(it->ai_family, it->ai_socktype, it->ai_protocol);
        if (fd < 0) continue;

        if (connect(fd, it->ai_addr, (socklen_t)it->ai_addrlen) == 0) {
            break; /* success */
        }
        close(fd);
        fd = -1;
    }

    freeaddrinfo(res);
    return fd;
}

static int send_all(int fd, const void *buf, size_t len)
{
    const unsigned char *p = (const unsigned char *)buf;
    while (len) {
        ssize_t n = send(fd, p, len, 0);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        p   += (size_t)n;
        len -= (size_t)n;
    }
    return 0;
}

/* Read a CRLF-terminated line from socket into linebuf (NUL-terminated).
   Returns 1 on success, 0 on EOF, -1 on error. */
static int recv_line(int fd, char *linebuf, size_t cap)
{
    size_t n = 0;
    while (n + 1 < cap) {
        unsigned char c;
        ssize_t r = recv(fd, &c, 1, 0);
        if (r == 0) {
            linebuf[n] = 0;
            return 0;
        }
        if (r < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        linebuf[n++] = (char)c;
        linebuf[n] = 0;
        if (n >= 2 && linebuf[n-2] == '\r' && linebuf[n-1] == '\n') {
            return 1;
        }
    }
    linebuf[cap ? cap - 1 : 0] = 0;
    return 1;
}

/* ----- core HTTP GET ----- */

/* Fetches HTTP response body into out (NUL-terminated if space).
   Returns >=0 body size on success, -1 on error. */
int nox_http_get_body(const char *host,
                      uint16_t port,
                      const char *path,
                      char *out,
                      size_t out_cap)
{
    if (out && out_cap) out[0] = 0;
    if (!host || !path || !out || out_cap == 0) return -1;

    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%u", (unsigned)port);

    int fd = connect_tcp(host, port_str);
    if (fd < 0) return -1;

    char req[512];
    int req_len = snprintf(req, sizeof(req),
        "GET %s HTTP/1.1\r\n"
        "Host: %s:%u\r\n"
        "User-Agent: nox-decomp/1\r\n"
        "Accept: application/json\r\n"
        "Connection: close\r\n"
        "\r\n",
        path, host, (unsigned)port);

    if (req_len <= 0 || (size_t)req_len >= sizeof(req)) {
        close(fd);
        return -1;
    }

    if (send_all(fd, req, (size_t)req_len) < 0) {
        close(fd);
        return -1;
    }

    /* Read status line */
    char line[1024];
    int rl = recv_line(fd, line, sizeof(line));
    if (rl <= 0) { close(fd); return -1; }

    /* Minimal status parse: "HTTP/1.1 200 ..." */
    int status = 0;
    {
        const char *p = strchr(line, ' ');
        if (p) status = atoi(p + 1);
    }
    if (status < 200 || status >= 300) {
        /* you can log line if you want */
        close(fd);
        return -1;
    }

    /* Read headers */
    int is_chunked = 0;
    long content_length = -1;

    for (;;) {
        rl = recv_line(fd, line, sizeof(line));
        if (rl < 0) { close(fd); return -1; }
        if (rl == 0) break; /* EOF */
        if (strcmp(line, "\r\n") == 0) break; /* end headers */

        if (str_case_contains(line, "transfer-encoding:") && str_case_contains(line, "chunked")) {
            is_chunked = 1;
        } else if (str_case_contains(line, "content-length:")) {
            const char *p = strchr(line, ':');
            if (p) {
                p = skip_ws(p + 1);
                content_length = strtol(p, NULL, 10);
                if (content_length < 0) content_length = -1;
            }
        }
    }

    /* Read body */
    size_t out_len = 0;

    if (is_chunked) {
        /* Chunked decoding */
        for (;;) {
            rl = recv_line(fd, line, sizeof(line));
            if (rl <= 0) { close(fd); return -1; }

            /* line: "<hex>\r\n" */
            unsigned chunk_sz = 0;
            {
                /* strip CRLF */
                char *cr = strstr(line, "\r\n");
                if (cr) *cr = 0;
                chunk_sz = (unsigned)strtoul(line, NULL, 16);
            }

            if (chunk_sz == 0) {
                /* read trailing headers until blank line */
                for (;;) {
                    rl = recv_line(fd, line, sizeof(line));
                    if (rl <= 0) break;
                    if (strcmp(line, "\r\n") == 0) break;
                }
                break;
            }

            while (chunk_sz) {
                char buf[2048];
                size_t want = chunk_sz < sizeof(buf) ? (size_t)chunk_sz : sizeof(buf);
                ssize_t r = recv(fd, buf, want, 0);
                if (r == 0) { close(fd); return -1; }
                if (r < 0) {
                    if (errno == EINTR) continue;
                    close(fd);
                    return -1;
                }

                size_t got = (size_t)r;
                if (out_len + got < out_cap) {
                    memcpy(out + out_len, buf, got);
                    out_len += got;
                } else {
                    /* output buffer full */
                    close(fd);
                    return -1;
                }
                chunk_sz -= (unsigned)got;
            }

            /* consume chunk trailing CRLF */
            rl = recv_line(fd, line, sizeof(line));
            if (rl <= 0) { close(fd); return -1; }
        }
    } else if (content_length >= 0) {
        /* Content-Length read */
        long remain = content_length;
        while (remain > 0) {
            char buf[2048];
            size_t want = (remain < (long)sizeof(buf)) ? (size_t)remain : sizeof(buf);
            ssize_t r = recv(fd, buf, want, 0);
            if (r == 0) break;
            if (r < 0) {
                if (errno == EINTR) continue;
                close(fd);
                return -1;
            }
            if (out_len + (size_t)r < out_cap) {
                memcpy(out + out_len, buf, (size_t)r);
                out_len += (size_t)r;
            } else {
                close(fd);
                return -1;
            }
            remain -= (long)r;
        }
    } else {
        /* Fallback: read until close */
        for (;;) {
            char buf[2048];
            ssize_t r = recv(fd, buf, sizeof(buf), 0);
            if (r == 0) break;
            if (r < 0) {
                if (errno == EINTR) continue;
                close(fd);
                return -1;
            }
            if (out_len + (size_t)r < out_cap) {
                memcpy(out + out_len, buf, (size_t)r);
                out_len += (size_t)r;
            } else {
                close(fd);
                return -1;
            }
        }
    }

    close(fd);

    /* NUL-terminate */
    if (out_len < out_cap) out[out_len] = 0;
    else out[out_cap - 1] = 0;

    return (int)out_len;
}

/* Convenience wrapper for your endpoint */
int nox_fetch_games_list_json(char *out, size_t out_cap)
{
//    return nox_http_get_body("nox.nwca.xyz", 8088, "/api/v0/games/list", out, out_cap);
    return nox_http_get_body(nox_lobby_host(), nox_lobby_port(), nox_lobby_path(), out, out_cap);
}

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

/* Minimal model for your endpoint */
typedef struct {
    char name[64];
    char addr[32];     /* IPv4 string fits */
    uint16_t port;
    char map[32];
    uint8_t players_cur;
    uint8_t players_max;
} nox_server_row;

/* ---- tiny helpers ---- */

static const char *skip_ws_json(const char *p) {
    while (p && *p && (unsigned char)*p <= ' ') p++;
    return p;
}

static const char *find_substr(const char *p, const char *needle) {
    if (!p || !needle || !*needle) return NULL;
    return strstr(p, needle);
}

/* Parses a JSON string value starting at the first quote.
   Copies into out (NUL-terminated). Returns pointer after closing quote or NULL. */
static const char *parse_json_string(const char *p, char *out, size_t out_sz) {
    size_t n = 0;

    if (!p) return NULL;
    p = skip_ws_json(p);
    if (*p != '"') return NULL;
    p++; /* skip opening quote */

    while (*p) {
        char c = *p++;
        if (c == '"') {
            /* end */
            if (out_sz) out[n < out_sz ? n : (out_sz - 1)] = 0;
            return p;
        }
        if (c == '\\') {
            /* minimal escape handling; enough for \" and \\ */
            char e = *p++;
            if (!e) return NULL;
            if (e == '"' || e == '\\' || e == '/') c = e;
            else if (e == 'b') c = '\b';
            else if (e == 'f') c = '\f';
            else if (e == 'n') c = '\n';
            else if (e == 'r') c = '\r';
            else if (e == 't') c = '\t';
            else {
                /* unsupported (like \uXXXX) -> keep raw escape char */
                c = e;
            }
        }
        if (out_sz && n + 1 < out_sz) out[n] = c;
        n++;
    }
    return NULL;
}

/* Parses an integer (non-negative). Returns pointer after number or NULL. */
static const char *parse_json_uint(const char *p, unsigned *out) {
    unsigned v = 0;
    int any = 0;

    if (!p) return NULL;
    p = skip_ws_json(p);
    while (*p && isdigit((unsigned char)*p)) {
        any = 1;
        v = v * 10u + (unsigned)(*p - '0');
        p++;
    }
    if (!any) return NULL;
    if (out) *out = v;
    return p;
}

/* Finds `"key"` within [obj_begin, obj_end) and returns pointer to value (after ':') */
static const char *find_key_value_range(const char *obj_begin, const char *obj_end, const char *key) {
    /* search for: "key" */
    char pat[64];
    size_t klen = strlen(key);
    if (klen + 3 >= sizeof(pat)) return NULL;

    pat[0] = '"';
    memcpy(pat + 1, key, klen);
    pat[1 + klen] = '"';
    pat[2 + klen] = 0;

    const char *p = obj_begin;
    while (p && p < obj_end) {
        const char *hit = strstr(p, pat);
        if (!hit || hit >= obj_end) return NULL;

        p = hit + (2 + (int)klen);
        p = skip_ws_json(p);
        if (p >= obj_end) return NULL;
        if (*p != ':') continue;
        p++;
        return skip_ws_json(p);
    }
    return NULL;
}

/* ---- main parser ----
   Returns number of servers parsed into out[] (up to out_cap). */
size_t nox_parse_games_list_json(const char *json, nox_server_row *out, size_t out_cap)
{
    size_t count = 0;
    if (!json || !out || out_cap == 0) return 0;

    /* locate "data":[ */
    const char *p = find_substr(json, "\"data\"");
    if (!p) return 0;
    p = find_substr(p, "[");
    if (!p) return 0;
    p++; /* after '[' */

    for (;;) {
        p = skip_ws_json(p);
        if (!*p) break;
        if (*p == ']') break;

        /* find next object '{' */
        while (*p && *p != '{' && *p != ']') p++;
        if (*p != '{') break;

        const char *obj_begin = p;
        int depth = 0;
        int in_str = 0;

        /* find matching '}' for this object (brace matching with string skipping) */
        for (; *p; p++) {
            char c = *p;
            if (in_str) {
                if (c == '\\' && p[1]) { p++; continue; }
                if (c == '"') in_str = 0;
                continue;
            } else {
                if (c == '"') { in_str = 1; continue; }
                if (c == '{') depth++;
                else if (c == '}') {
                    depth--;
                    if (depth == 0) { p++; break; } /* p now after '}' */
                }
            }
        }
        if (depth != 0) break;

        const char *obj_end = p;

        if (count < out_cap) {
            nox_server_row row;
            memset(&row, 0, sizeof(row));
            row.port = 18590;
            row.players_max = 31;

            /* name */
            {
                const char *v = find_key_value_range(obj_begin, obj_end, "name");
                if (v) parse_json_string(v, row.name, sizeof(row.name));
            }

            /* addr */
            {
                const char *v = find_key_value_range(obj_begin, obj_end, "addr");
                if (v) parse_json_string(v, row.addr, sizeof(row.addr));
            }

            /* port */
            {
                const char *v = find_key_value_range(obj_begin, obj_end, "port");
                unsigned n;
                if (v && (v = parse_json_uint(v, &n))) row.port = (uint16_t)n;
            }

            /* map */
            {
                const char *v = find_key_value_range(obj_begin, obj_end, "map");
                if (v) parse_json_string(v, row.map, sizeof(row.map));
            }

            /* players.cur / players.max (object) */
            {
                const char *players_v = find_key_value_range(obj_begin, obj_end, "players");
                if (players_v && *players_v == '{') {
                    /* derive a subrange for the players object */
                    const char *pb = players_v;
                    const char *pe = pb;
                    int d = 0, s = 0;
                    for (; *pe; pe++) {
                        char c = *pe;
                        if (s) {
                            if (c == '\\' && pe[1]) { pe++; continue; }
                            if (c == '"') s = 0;
                            continue;
                        } else {
                            if (c == '"') { s = 1; continue; }
                            if (c == '{') d++;
                            else if (c == '}') {
                                d--;
                                if (d == 0) { pe++; break; }
                            }
                        }
                    }
                    if (d == 0) {
                        const char *vcur = find_key_value_range(pb, pe, "cur");
                        const char *vmax = find_key_value_range(pb, pe, "max");
                        unsigned n;
                        if (vcur && (vcur = parse_json_uint(vcur, &n))) row.players_cur = (uint8_t)n;
                        if (vmax && (vmax = parse_json_uint(vmax, &n))) row.players_max = (uint8_t)n;
                    }
                }
            }

            out[count++] = row;
        }

        /* move past optional comma */
        p = skip_ws_json(p);
        if (*p == ',') p++;
    }

    return count;
}

/* ----- bad server filters ----- */

static const char *nox_getenv_or_default(const char *key, const char *defv)
{
    const char *v = getenv(key);
    if (!v || !*v) return defv;
    return v;
}

static const char *nox_bad_ip_csv(void)
{
    return nox_getenv_or_default(NOX_ENV_BAD_IPS, NOX_DEFAULT_BAD_IPS);
}

static const char *nox_bad_name_csv(void)
{
    return nox_getenv_or_default(NOX_ENV_BAD_NAMES, NOX_DEFAULT_BAD_NAMES);
}

int nox_is_bad_server_ip(const char *ip)
{
    return csv_contains_exact(nox_bad_ip_csv(), ip);
}

int nox_is_bad_server_name(const char *name)
{
    return csv_contains_case(nox_bad_name_csv(), name);
}
