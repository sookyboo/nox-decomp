// src/upnp.c - self-contained UPnP IGD port mapping (no external deps)
//
// Env vars:
//   NOX_UPNP_ENABLE=1
//   NOX_UPNP_PORT=18590
//   NOX_UPNP_PROTO=udp|tcp|udp,tcp
//   NOX_UPNP_DESC="Nox game port"
//   NOX_UPNP_LEASE_SEC=10800 (3h)  (0 = permanent)
//   NOX_UPNP_TIMEOUT_MS=2000
//   NOX_UPNP_DEBUG=1

#define _GNU_SOURCE
//#include "upnp.h"
#include "proto.h"

#include <ctype.h>
#include <errno.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <netdb.h>

#ifndef INET6_ADDRSTRLEN
#define INET6_ADDRSTRLEN 46
#endif

static pthread_mutex_t g_mu = PTHREAD_MUTEX_INITIALIZER;

static int g_inited = 0;
static int g_mapped_udp = 0;
static int g_mapped_tcp = 0;
static int g_port = 18590;

static char g_service_type[128];
static char g_ctrl_host[256];
static int  g_ctrl_port = 80;
static char g_ctrl_path[512];

static char g_router_ip[64];     // internal router IP from LOCATION host
static char g_internal_ip[64];   // our LAN IP selected by OS
static int  g_lease_sec = 10800;

static time_t g_next_try = 0;    // backoff
static int g_debug = 0;

static void dlog(const char *fmt, ...) {
    if (!g_debug) return;
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
}

static int env_truthy(const char *s) {
    if (!s) return 0;
    while (*s && (unsigned char)*s <= ' ') s++;
    if (!*s) return 0;
    if (strcmp(s, "1") == 0) return 1;
    char buf[16];
    size_t n = 0;
    while (s[n] && n + 1 < sizeof(buf)) { buf[n] = (char)tolower((unsigned char)s[n]); n++; }
    buf[n] = 0;
    return (strcmp(buf, "true") == 0) || (strcmp(buf, "yes") == 0) || (strcmp(buf, "on") == 0);
}

static int env_int(const char *name, int defv) {
    const char *s = getenv(name);
    if (!s || !*s) return defv;
    return atoi(s);
}

static const char *env_str(const char *name, const char *defv) {
    const char *s = getenv(name);
    return (s && *s) ? s : defv;
}

static int set_nonblock(int fd, int nb) {
    int fl = fcntl(fd, F_GETFL, 0);
    if (fl < 0) return -1;
    if (nb) fl |= O_NONBLOCK; else fl &= ~O_NONBLOCK;
    return fcntl(fd, F_SETFL, fl);
}

static int connect_with_timeout(int fd, const struct sockaddr *sa, socklen_t slen, int ms) {
    int fl = fcntl(fd, F_GETFL, 0);
    if (fl < 0) return -1;
    if (set_nonblock(fd, 1) < 0) return -1;

    int rc = connect(fd, (__CONST_SOCKADDR_ARG)sa, slen);
    if (rc == 0) { (void)fcntl(fd, F_SETFL, fl); return 0; }
    if (rc < 0 && errno != EINPROGRESS) { (void)fcntl(fd, F_SETFL, fl); return -1; }

    fd_set wfds;
    FD_ZERO(&wfds);
    FD_SET(fd, &wfds);

    struct timeval tv;
    tv.tv_sec = ms / 1000;
    tv.tv_usec = (ms % 1000) * 1000;

    rc = select(fd + 1, NULL, &wfds, NULL, &tv);
    if (rc <= 0) {
        (void)fcntl(fd, F_SETFL, fl);
        errno = (rc == 0) ? ETIMEDOUT : errno;
        return -1;
    }

    int soerr = 0;
    socklen_t sl = sizeof(soerr);
    if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &soerr, &sl) < 0) {
        (void)fcntl(fd, F_SETFL, fl);
        return -1;
    }
    (void)fcntl(fd, F_SETFL, fl);
    if (soerr != 0) { errno = soerr; return -1; }
    return 0;
}

static int tcp_connect_hostport(const char *host, int port, int timeout_ms) {
    char portstr[16];
    snprintf(portstr, sizeof(portstr), "%d", port);

    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_family = AF_UNSPEC;

    struct addrinfo *res = NULL;
    if (getaddrinfo(host, portstr, &hints, &res) != 0 || !res) return -1;

    int fd = -1;
    for (struct addrinfo *it = res; it; it = it->ai_next) {
        fd = (int)socket(it->ai_family, it->ai_socktype, it->ai_protocol);
        if (fd < 0) continue;
        if (connect_with_timeout(fd, it->ai_addr, (socklen_t)it->ai_addrlen, timeout_ms) == 0) {
            break;
        }
        close(fd);
        fd = -1;
    }

    freeaddrinfo(res);
    return fd;
}

static int send_all(int fd, const void *buf, size_t len) {
    const unsigned char *p = (const unsigned char *)buf;
    while (len) {
        ssize_t n = send(fd, p, len, 0);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        p += (size_t)n;
        len -= (size_t)n;
    }
    return 0;
}

static int recv_some(int fd, char *buf, size_t cap, int timeout_ms) {
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(fd, &rfds);
    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    int rc = select(fd + 1, &rfds, NULL, NULL, &tv);
    if (rc <= 0) {
        errno = (rc == 0) ? ETIMEDOUT : errno;
        return -1;
    }
    ssize_t n = recv(fd, buf, cap, 0);
    if (n < 0) return -1;
    return (int)n;
}

/* Very small HTTP fetch: reads until close into out (NUL-terminated). */
static int http_get(const char *host, int port, const char *path, char *out, size_t outcap, int timeout_ms) {
    if (!out || outcap == 0) return -1;
    out[0] = 0;

    int fd = tcp_connect_hostport(host, port, timeout_ms);
    if (fd < 0) return -1;

    char req[1024];
    int n = snprintf(req, sizeof(req),
        "GET %s HTTP/1.1\r\n"
        "Host: %s:%d\r\n"
        "User-Agent: nox-upnp/1\r\n"
        "Connection: close\r\n"
        "\r\n",
        path, host, port);
    if (n <= 0 || (size_t)n >= sizeof(req)) { close(fd); return -1; }

    if (send_all(fd, req, (size_t)n) < 0) { close(fd); return -1; }

    size_t used = 0;
    for (;;) {
        char tmp[2048];
        int r = recv_some(fd, tmp, sizeof(tmp), timeout_ms);
        if (r < 0) break;
        if (r == 0) break;
        if (used + (size_t)r + 1 > outcap) { close(fd); errno = ENOSPC; return -1; }
        memcpy(out + used, tmp, (size_t)r);
        used += (size_t)r;
    }
    close(fd);
    out[used] = 0;
    return (int)used;
}

/* HTTP POST (SOAP): returns HTTP status code, stores body (best-effort). */
static int http_post_soap(const char *host, int port, const char *path,
                          const char *soap_action,
                          const char *xml,
                          char *resp, size_t respcap,
                          int timeout_ms)
{
    if (resp && respcap) resp[0] = 0;
    int fd = tcp_connect_hostport(host, port, timeout_ms);
    if (fd < 0) return -1;

    size_t body_len = strlen(xml);

    char hdr[2048];
    int hn = snprintf(hdr, sizeof(hdr),
        "POST %s HTTP/1.1\r\n"
        "Host: %s:%d\r\n"
        "User-Agent: nox-upnp/1\r\n"
        "Content-Type: text/xml; charset=\"utf-8\"\r\n"
        "SOAPACTION: \"%s\"\r\n"
        "Content-Length: %u\r\n"
        "Connection: close\r\n"
        "\r\n",
        path, host, port, soap_action, (unsigned)body_len);
    if (hn <= 0 || (size_t)hn >= sizeof(hdr)) { close(fd); return -1; }

    if (send_all(fd, hdr, (size_t)hn) < 0) { close(fd); return -1; }
    if (send_all(fd, xml, body_len) < 0) { close(fd); return -1; }

    /* Read all */
    size_t used = 0;
    for (;;) {
        char tmp[2048];
        int r = recv_some(fd, tmp, sizeof(tmp), timeout_ms);
        if (r < 0) break;
        if (r == 0) break;
        if (resp && respcap) {
            size_t can = respcap - 1 - used;
            size_t take = (size_t)r;
            if (take > can) take = can;
            memcpy(resp + used, tmp, take);
            used += take;
        }
    }
    close(fd);
    if (resp && respcap) resp[used] = 0;

    /* Parse status line from response buffer (if present) */
    int status = -1;
    if (resp && respcap && resp[0]) {
        const char *p = strstr(resp, "HTTP/");
        if (p) {
            const char *sp = strchr(p, ' ');
            if (sp) status = atoi(sp + 1);
        }
    }
    return status;
}

/* Parse URL: http://host[:port]/path */
static int parse_http_url(const char *url, char *host, size_t hostsz, int *port, char *path, size_t pathsz) {
    if (!url || !host || !port || !path) return -1;
    if (strncmp(url, "http://", 7) != 0) return -1;
    const char *p = url + 7;

    const char *slash = strchr(p, '/');
    const char *hostend = slash ? slash : (p + strlen(p));

    char hbuf[256];
    size_t hlen = (size_t)(hostend - p);
    if (hlen == 0 || hlen >= sizeof(hbuf)) return -1;
    memcpy(hbuf, p, hlen);
    hbuf[hlen] = 0;

    int prt = 80;
    char *colon = strrchr(hbuf, ':');
    if (colon && strchr(hbuf, ']') == NULL) {
        *colon = 0;
        prt = atoi(colon + 1);
        if (prt <= 0 || prt > 65535) prt = 80;
    }

    strncpy(host, hbuf, hostsz - 1);
    host[hostsz - 1] = 0;
    *port = prt;

    if (slash) {
        strncpy(path, slash, pathsz - 1);
        path[pathsz - 1] = 0;
    } else {
        strncpy(path, "/", pathsz - 1);
        path[pathsz - 1] = 0;
    }
    return 0;
}

/* Join base (scheme://host:port) with controlURL which may be absolute or relative. */
static int make_control_url(const char *location_url, const char *urlbase, const char *controlURL,
                            char *out, size_t outsz)
{
    if (!controlURL || !*controlURL || !out || outsz == 0) return -1;

    if (strncmp(controlURL, "http://", 7) == 0) {
        strncpy(out, controlURL, outsz - 1);
        out[outsz - 1] = 0;
        return 0;
    }

    const char *base = (urlbase && *urlbase) ? urlbase : location_url;
    if (!base) return -1;

    // base is a full URL; we only need scheme://host[:port]
    char host[256], path[512];
    int port;
    if (parse_http_url(base, host, sizeof(host), &port, path, sizeof(path)) != 0) return -1;

    char prefix[512];
    snprintf(prefix, sizeof(prefix), "http://%s:%d", host, port);

    if (controlURL[0] == '/') {
        snprintf(out, outsz, "%s%s", prefix, controlURL);
    } else {
        // relative path: append '/' + controlURL
        snprintf(out, outsz, "%s/%s", prefix, controlURL);
    }
    out[outsz - 1] = 0;
    return 0;
}

/* Extract tag contents: <Tag> ... </Tag> (first occurrence) */
static int xml_tag_value(const char *xml, const char *tag, char *out, size_t outsz) {
    if (!xml || !tag || !out || outsz == 0) return 0;
    out[0] = 0;

    char open[64], close[64];
    snprintf(open, sizeof(open), "<%s>", tag);
    snprintf(close, sizeof(close), "</%s>", tag);

    const char *p = strstr(xml, open);
    if (!p) return 0;
    p += strlen(open);
    const char *q = strstr(p, close);
    if (!q) return 0;

    size_t n = (size_t)(q - p);
    if (n >= outsz) n = outsz - 1;
    memcpy(out, p, n);
    out[n] = 0;

    // trim whitespace
    while (out[0] && (unsigned char)out[0] <= ' ') memmove(out, out + 1, strlen(out));
    size_t L = strlen(out);
    while (L && (unsigned char)out[L - 1] <= ' ') out[--L] = 0;

    return 1;
}

/* Find service block containing serviceType, then extract controlURL. */
static int xml_find_service_control_url(const char *xml, const char *want_type, char *out, size_t outsz) {
    if (!xml || !want_type || !out || outsz == 0) return 0;
    out[0] = 0;

    const char *p = xml;
    while ((p = strstr(p, "<service>")) != NULL) {
        const char *end = strstr(p, "</service>");
        if (!end) break;

        const char *stype = strstr(p, "<serviceType>");
        if (stype && stype < end) {
            char st[256];
            if (xml_tag_value(p, "serviceType", st, sizeof(st))) {
                if (strcmp(st, want_type) == 0) {
                    if (xml_tag_value(p, "controlURL", out, outsz)) return 1;
                }
            }
        }
        p = end + 9;
    }
    return 0;
}

/* SSDP discovery: returns LOCATION url in out. */
static int ssdp_discover_location(char *out, size_t outsz, int timeout_ms) {
    if (!out || outsz == 0) return -1;
    out[0] = 0;

    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) return -1;

    int yes = 1;
    (void)setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    struct sockaddr_in dst;
    memset(&dst, 0, sizeof(dst));
    dst.sin_family = AF_INET;
    dst.sin_port = htons(1900);
    inet_pton(AF_INET, "239.255.255.250", &dst.sin_addr);

    // try multiple STs
    const char *sts[] = {
        "urn:schemas-upnp-org:service:WANIPConnection:2",
        "urn:schemas-upnp-org:service:WANIPConnection:1",
        "urn:schemas-upnp-org:service:WANPPPConnection:1",
        "urn:schemas-upnp-org:device:InternetGatewayDevice:1",
        NULL
    };

    char req[1024];
    for (int i = 0; sts[i]; i++) {
        int n = snprintf(req, sizeof(req),
            "M-SEARCH * HTTP/1.1\r\n"
            "HOST: 239.255.255.250:1900\r\n"
            "MAN: \"ssdp:discover\"\r\n"
            "MX: 1\r\n"
            "ST: %s\r\n"
            "\r\n",
            sts[i]);
        if (n <= 0 || (size_t)n >= sizeof(req)) continue;
        (void)sendto(fd, req, (size_t)n, 0, (struct sockaddr *)&dst, sizeof(dst));
    }

    // collect responses until timeout; pick first with LOCATION
    time_t start = time(NULL);
    for (;;) {
        int remain = timeout_ms;
        time_t now = time(NULL);
        int elapsed = (int)((now - start) * 1000);
        remain = timeout_ms - elapsed;
        if (remain <= 0) break;

        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(fd, &rfds);
        struct timeval tv;
        tv.tv_sec = remain / 1000;
        tv.tv_usec = (remain % 1000) * 1000;

        int rc = select(fd + 1, &rfds, NULL, NULL, &tv);
        if (rc <= 0) break;

        char buf[2048];
        struct sockaddr_in from;
        socklen_t fl = sizeof(from);
        ssize_t r = recvfrom(fd, buf, sizeof(buf) - 1, 0, (struct sockaddr *)&from, &fl);
        if (r <= 0) continue;
        buf[r] = 0;

        // Find "LOCATION:" (case-insensitive-ish)
        const char *loc = NULL;
        const char *p = buf;
        while (*p) {
            // line start
            const char *line = p;
            const char *eol = strstr(p, "\r\n");
            if (!eol) eol = p + strlen(p);

            // compare prefix
            const char *k = "location:";
            size_t klen = strlen(k);
            if ((size_t)(eol - line) > klen) {
                int match = 1;
                for (size_t j = 0; j < klen; j++) {
                    if (tolower((unsigned char)line[j]) != k[j]) { match = 0; break; }
                }
                if (match) {
                    const char *v = line + klen;
                    while (*v && (unsigned char)*v <= ' ') v++;
                    size_t n = (size_t)(eol - v);
                    if (n >= outsz) n = outsz - 1;
                    memcpy(out, v, n);
                    out[n] = 0;
                    loc = out;
                    break;
                }
            }

            if (*eol == 0) break;
            p = eol + 2;
        }

        if (loc && out[0]) {
            close(fd);
            return 0;
        }
    }

    close(fd);
    return -1;
}

/* Determine our LAN IP by "connecting" a UDP socket to router internal IP. */
static int determine_internal_ip(const char *router_ip, char *out, size_t outsz) {
    if (!router_ip || !out || outsz == 0) return -1;
    out[0] = 0;

    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) return -1;

    struct sockaddr_in dst;
    memset(&dst, 0, sizeof(dst));
    dst.sin_family = AF_INET;
    dst.sin_port = htons(1900);
    if (inet_pton(AF_INET, router_ip, &dst.sin_addr) != 1) {
        close(fd);
        return -1;
    }

    // UDP connect doesn't send packets but selects route
    (void)connect(fd, (__CONST_SOCKADDR_ARG)(const struct sockaddr *)&dst, sizeof(dst));

    struct sockaddr_in me;
    socklen_t ml = sizeof(me);
    if (getsockname(fd, (__SOCKADDR_ARG)&me, &ml) != 0) {
        close(fd);
        return -1;
    }

    char ip[INET_ADDRSTRLEN];
    if (!inet_ntop(AF_INET, &me.sin_addr, ip, sizeof(ip))) {
        close(fd);
        return -1;
    }
    strncpy(out, ip, outsz - 1);
    out[outsz - 1] = 0;
    close(fd);
    return 0;
}

static int soap_error_contains(const char *resp, const char *needle) {
    if (!resp || !needle) return 0;
    // case-sensitive is fine; many routers use exact string in UPnPError
    return strstr(resp, needle) != NULL;
}

static int upnp_add_mapping_one(const char *proto, const char *desc, int lease_sec, int timeout_ms) {
    char action[256];
    snprintf(action, sizeof(action), "%s#AddPortMapping", g_service_type);

    char xml[2048];
    snprintf(xml, sizeof(xml),
        "<?xml version=\"1.0\"?>"
        "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" "
        "s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">"
          "<s:Body>"
            "<u:AddPortMapping xmlns:u=\"%s\">"
              "<NewRemoteHost></NewRemoteHost>"
              "<NewExternalPort>%d</NewExternalPort>"
              "<NewProtocol>%s</NewProtocol>"
              "<NewInternalPort>%d</NewInternalPort>"
              "<NewInternalClient>%s</NewInternalClient>"
              "<NewEnabled>1</NewEnabled>"
              "<NewPortMappingDescription>%s</NewPortMappingDescription>"
              "<NewLeaseDuration>%d</NewLeaseDuration>"
            "</u:AddPortMapping>"
          "</s:Body>"
        "</s:Envelope>",
        g_service_type, g_port, proto, g_port, g_internal_ip, desc, lease_sec
    );

    char resp[4096];
    int status = http_post_soap(g_ctrl_host, g_ctrl_port, g_ctrl_path, action, xml, resp, sizeof(resp), timeout_ms);
    if (status >= 200 && status < 300) {
        dlog("upnp: AddPortMapping %s OK\n", proto);
        return 0;
    }

    // Retry with permanent lease if router requires it
    if (soap_error_contains(resp, "OnlyPermanentLeasesSupported") && lease_sec != 0) {
        dlog("upnp: lease rejected, retrying permanent\n");
        return upnp_add_mapping_one(proto, desc, 0, timeout_ms);
    }

    dlog("upnp: AddPortMapping %s failed status=%d\n", proto, status);
    if (g_debug && resp[0]) {
        dlog("upnp: response: %.512s\n", resp);
    }
    return -1;
}

static int upnp_del_mapping_one(const char *proto, int timeout_ms) {
    if (!g_service_type[0]) return -1;

    char action[256];
    snprintf(action, sizeof(action), "%s#DeletePortMapping", g_service_type);

    char xml[1024];
    snprintf(xml, sizeof(xml),
        "<?xml version=\"1.0\"?>"
        "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" "
        "s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">"
          "<s:Body>"
            "<u:DeletePortMapping xmlns:u=\"%s\">"
              "<NewRemoteHost></NewRemoteHost>"
              "<NewExternalPort>%d</NewExternalPort>"
              "<NewProtocol>%s</NewProtocol>"
            "</u:DeletePortMapping>"
          "</s:Body>"
        "</s:Envelope>",
        g_service_type, g_port, proto
    );

    char resp[2048];
    int status = http_post_soap(g_ctrl_host, g_ctrl_port, g_ctrl_path, action, xml, resp, sizeof(resp), timeout_ms);
    if (status >= 200 && status < 300) {
        dlog("upnp: DeletePortMapping %s OK\n", proto);
        return 0;
    }
    dlog("upnp: DeletePortMapping %s status=%d\n", proto, status);
    return -1;
}

void nox_upnp_cleanup(void) {
    pthread_mutex_lock(&g_mu);

    int timeout_ms = env_int("NOX_UPNP_TIMEOUT_MS", 2000);

    if (g_mapped_udp) { (void)upnp_del_mapping_one("UDP", timeout_ms); g_mapped_udp = 0; }
    if (g_mapped_tcp) { (void)upnp_del_mapping_one("TCP", timeout_ms); g_mapped_tcp = 0; }

    pthread_mutex_unlock(&g_mu);
}

/* Discover and cache control endpoint */
static int upnp_discover_and_cache(int timeout_ms) {
    char location[512];
    if (ssdp_discover_location(location, sizeof(location), timeout_ms) != 0) {
        dlog("upnp: SSDP discovery found no LOCATION\n");
        return -1;
    }

    dlog("upnp: LOCATION=%s\n", location);

    // parse LOCATION to host/port/path
    char loc_host[256], loc_path[512];
    int loc_port;
    if (parse_http_url(location, loc_host, sizeof(loc_host), &loc_port, loc_path, sizeof(loc_path)) != 0) {
        dlog("upnp: cannot parse LOCATION url\n");
        return -1;
    }

    strncpy(g_router_ip, loc_host, sizeof(g_router_ip) - 1);
    g_router_ip[sizeof(g_router_ip) - 1] = 0;

    // determine internal IP
    if (determine_internal_ip(g_router_ip, g_internal_ip, sizeof(g_internal_ip)) != 0) {
        dlog("upnp: cannot determine internal client IP\n");
        return -1;
    }
    dlog("upnp: internal client IP=%s\n", g_internal_ip);

    // fetch device xml
    char xml[64 * 1024];
    int n = http_get(loc_host, loc_port, loc_path, xml, sizeof(xml), timeout_ms);
    if (n <= 0) {
        dlog("upnp: cannot fetch device XML\n");
        return -1;
    }

    // xml includes headers; find body start
    const char *body = strstr(xml, "\r\n\r\n");
    body = body ? body + 4 : xml;

    char urlbase[512];
    urlbase[0] = 0;
    (void)xml_tag_value(body, "URLBase", urlbase, sizeof(urlbase));

    const char *service_candidates[] = {
        "urn:schemas-upnp-org:service:WANIPConnection:2",
        "urn:schemas-upnp-org:service:WANIPConnection:1",
        "urn:schemas-upnp-org:service:WANPPPConnection:1",
        NULL
    };

    char ctrl[512];
    ctrl[0] = 0;

    const char *chosen = NULL;
    for (int i = 0; service_candidates[i]; i++) {
        if (xml_find_service_control_url(body, service_candidates[i], ctrl, sizeof(ctrl))) {
            chosen = service_candidates[i];
            break;
        }
    }
    if (!chosen || !ctrl[0]) {
        dlog("upnp: no WAN*Connection service found in XML\n");
        return -1;
    }

    // build control full URL
    char ctrl_url[512];
    if (make_control_url(location, urlbase, ctrl, ctrl_url, sizeof(ctrl_url)) != 0) {
        dlog("upnp: cannot build control url\n");
        return -1;
    }

    dlog("upnp: service=%s control=%s\n", chosen, ctrl_url);

    // parse ctrl url
    if (parse_http_url(ctrl_url, g_ctrl_host, sizeof(g_ctrl_host), &g_ctrl_port, g_ctrl_path, sizeof(g_ctrl_path)) != 0) {
        dlog("upnp: cannot parse control url\n");
        return -1;
    }

    strncpy(g_service_type, chosen, sizeof(g_service_type) - 1);
    g_service_type[sizeof(g_service_type) - 1] = 0;

    return 0;
}

static int proto_wants(const char *proto_csv, const char *needle_lower) {
    if (!proto_csv || !*proto_csv) return 0;
    // naive CSV contains check, case-insensitive, token-based
    const char *p = proto_csv;
    while (*p) {
        while (*p == ',' || (unsigned char)*p <= ' ') p++;
        if (!*p) break;

        char tok[32];
        size_t n = 0;
        while (*p && *p != ',' && n + 1 < sizeof(tok)) {
            tok[n++] = (char)tolower((unsigned char)*p++);
        }
        tok[n] = 0;
        while (*p && *p != ',') p++; // skip remainder if any
        if (*p == ',') p++;

        if (strcmp(tok, needle_lower) == 0) return 1;
    }
    return 0;
}

int nox_upnp_ensure_mapped_from_env(void) {
    pthread_mutex_lock(&g_mu);

    g_debug = env_truthy(getenv("NOX_UPNP_DEBUG")) ? 1 : 0;

    if (!env_truthy(getenv("NOX_UPNP_ENABLE"))) {
        pthread_mutex_unlock(&g_mu);
        return -1;
    }

    time_t now = time(NULL);
    if (g_next_try && now < g_next_try) {
        pthread_mutex_unlock(&g_mu);
        return -1;
    }

    g_port = env_int("NOX_UPNP_PORT", 18590);
    if (g_port <= 0 || g_port > 65535) g_port = 18590;

    g_lease_sec = env_int("NOX_UPNP_LEASE_SEC", 10800);
    if (g_lease_sec < 0) g_lease_sec = 10800;

    int timeout_ms = env_int("NOX_UPNP_TIMEOUT_MS", 2000);
    if (timeout_ms < 200) timeout_ms = 200;
    if (timeout_ms > 20000) timeout_ms = 20000;

    const char *proto_csv = env_str("NOX_UPNP_PROTO", "udp");
    const char *desc = env_str("NOX_UPNP_DESC", "Nox game port");

    // If already mapped for requested protos, return quickly
    int want_udp = proto_wants(proto_csv, "udp");
    int want_tcp = proto_wants(proto_csv, "tcp");
    if (!want_udp && !want_tcp) want_udp = 1; // default safety

    if ((want_udp && g_mapped_udp) || (!want_udp)) {
        // ok
    }
    if ((want_tcp && g_mapped_tcp) || (!want_tcp)) {
        // ok
    }
    if ((!want_udp || g_mapped_udp) && (!want_tcp || g_mapped_tcp)) {
        pthread_mutex_unlock(&g_mu);
        return 0;
    }

    if (!g_inited) {
        if (upnp_discover_and_cache(timeout_ms) != 0) {
            // backoff 60s on failure
            g_next_try = now + 60;
            pthread_mutex_unlock(&g_mu);
            return -1;
        }
        g_inited = 1;
        // register cleanup once we have a chance of deleting mappings
        atexit(nox_upnp_cleanup);
    }

    int ok = 1;
    if (want_udp && !g_mapped_udp) {
        if (upnp_add_mapping_one("UDP", desc, g_lease_sec, timeout_ms) == 0) g_mapped_udp = 1;
        else ok = 0;
    }
    if (want_tcp && !g_mapped_tcp) {
        if (upnp_add_mapping_one("TCP", desc, g_lease_sec, timeout_ms) == 0) g_mapped_tcp = 1;
        else ok = 0;
    }

    if (!ok) {
        // backoff 60s to avoid hammering
        g_next_try = now + 60;
        pthread_mutex_unlock(&g_mu);
        return -1;
    }

    pthread_mutex_unlock(&g_mu);
    return 0;
}
