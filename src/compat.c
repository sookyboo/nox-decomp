#define _GNU_SOURCE
#include <ctype.h>
#include <fcntl.h>
#include <fenv.h>
#include <glob.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <unistd.h>
#include <wctype.h>
#include "proto.h"
#include <limits.h>
#include <libgen.h>   // dirname
#include <unistd.h>   // readlink
#include <fnmatch.h>
#undef fopen

#include <stdlib.h>
#include <string.h>

#include <dirent.h>
#include <errno.h>

#include <stdarg.h>
#include <sys/stat.h>

#include <SDL2/SDL_stdinc.h>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#endif


#ifndef COMPAT_STAT_LOG
#define COMPAT_STAT_LOG 1
#endif

#if COMPAT_STAT_LOG
#define STATLOG(...) fprintf(stderr, __VA_ARGS__)
#else
#define STATLOG(...) do {} while (0)
#endif

enum
{
    HANDLE_FILE,
    HANDLE_PROCESS,
    HANDLE_THREAD,
    HANDLE_MUTEX,
};

struct _REGKEY
{
    char *path;
};

extern const char *progname;
DWORD last_error;
DWORD last_socket_error;
void *handles[1024];

// ---- NET logging helpers ----
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

static void net_dump_sockname(int fd, const char *tag)
{
#if defined(__arm__) || defined(__arm) || defined(__ARM_EABI__) || defined(__aarch64__)
    (void)fd; (void)tag;
    return;
#else
    struct sockaddr_in s;
    socklen_t slen = sizeof(s);
    char ipbuf[INET_ADDRSTRLEN] = "?";
    unsigned port = 0;

    if (getsockname(fd, (struct sockaddr *)&s, &slen) == 0 && s.sin_family == AF_INET) {
        inet_ntop(AF_INET, &s.sin_addr, ipbuf, sizeof(ipbuf));
        port = (unsigned)ntohs(s.sin_port);
    }
    NETLOG("compat_net: %s fd=%d local=%s:%u\n", tag, fd, ipbuf, port);
#endif
}

static void net_dump_flags(int fd, const char *tag)
{
    int flags = fcntl(fd, F_GETFL, 0);
    NETLOG("compat_net: %s fd=%d fcntl_flags=0x%x %s\n",
           tag, fd, flags,
           (flags >= 0 && (flags & O_NONBLOCK)) ? "(NONBLOCK)" : "(blocking)");
}


/* forward decls you already have elsewhere */
static char *dos_to_unix(const char *path);
static int casepath(const char *path, char *r);
static void fill_find_data(const char *path, LPWIN32_FIND_DATAA lpFindFileData);
static int compat_do_stat(const char *path, struct stat *st);
static int  wsa_from_errno(int e);
static void wsa_set_last_from_errno(void);

#ifndef FNM_CASEFOLD
// GNU fnmatch has FNM_CASEFOLD; if missing, we’ll fall back to manual lowercasing below.
#define FNM_CASEFOLD 0
#endif

#ifndef ERROR_INVALID_PARAMETER
#define ERROR_INVALID_PARAMETER 87
#endif
#ifndef ERROR_PATH_NOT_FOUND
#define ERROR_PATH_NOT_FOUND 3
#endif
#ifndef ERROR_FILE_NOT_FOUND
#define ERROR_FILE_NOT_FOUND 2
#endif

#ifdef open
#undef open
#endif
static char *dos_to_unix(const char *path);
int compat_open(const char *filename, int oflag, ...);


int compat_open(const char *filename, int oflag, ...)
{
    mode_t mode = 0;

    if (!filename) {
        errno = EINVAL;
        return -1;
    }

    /* Only read a mode arg when O_CREAT is present (like real open). */
    if (oflag & O_CREAT) {
        va_list ap;
        va_start(ap, oflag);
        mode = (mode_t)va_arg(ap, int);
        va_end(ap);
    }

    char *converted = dos_to_unix(filename);
    if (!converted) {
        errno = ENOMEM;
        return -1;
    }

    int fd;
    if (oflag & O_CREAT) {
        fd = open(converted, oflag, mode);
    } else {
        fd = open(converted, oflag);
    }

    /* Optional logging if you want */
    // fprintf(stderr, "compat_open: '%s' -> '%s' flags=0x%x => %d (errno=%d)\n",
    //         filename, converted, oflag, fd, errno);

    free(converted);
    return fd;
}



static inline HANDLE new_handle(unsigned int type, void *data)
{
    unsigned int i;
    for (i = 0; i < 1024; i++)
    {
        if (!handles[i])
        {
            handles[i] = data;
            return (HANDLE)((type << 16) | i);
        }
    }
    return (HANDLE)-1;
}

static inline void *lookup_handle(unsigned int type, HANDLE h)
{
    if (type != ((DWORD)h >> 16))
        return NULL;
    if ((WORD)h >= 1024)
        return NULL;
    return handles[(WORD)h];
}

// Debug functions
VOID WINAPI DebugBreak(){    fprintf(stderr, "DebugBreak() called (ignored)\n");}

VOID WINAPI OutputDebugStringA(LPCSTR lpOutputString)
{
    fprintf(stderr, "%s", lpOutputString);
}

// Memory functions
BOOL WINAPI HeapDestroy(HANDLE hHeap)
{
    DebugBreak();
}

// CRT functions
unsigned int _control87(unsigned int new_, unsigned int mask)
{
    if (new_ == 0x300 && mask == 0x300)
        fesetround(FE_TOWARDZERO);
    else
        DebugBreak();
}

unsigned int _controlfp(unsigned int new_, unsigned int mask)
{
    if (new_ == 0x300 && mask == 0x300)
        fesetround(FE_TOWARDZERO);
    else
        DebugBreak();
}

uintptr_t _beginthread(void( __cdecl *start_address )( void * ), unsigned int stack_size, void *arglist)
{
    pthread_t handle;

#ifdef __EMSCRIPTEN__
    fprintf(stderr, "%s: unsupported\n");
    while (1) {}
#endif

    if (pthread_create(&handle, NULL, start_address, arglist))
        return (uintptr_t)-1;

    return new_handle(HANDLE_THREAD, handle);
}

char *_strrev(char *str)
{
    char *begin, *end;

    if (!str[0])
        return str;

    begin = str;
    end = str + strlen(str) - 1;

    while (begin < end)
    {
        char tmp = *end;
        *end = *begin;
        *begin = tmp;
    }

    return str;
}

char *_itoa(int val, char *s, int radix)
{
    return SDL_itoa(val, s, radix);
}

char *_utoa(unsigned int val, char *s, int radix)
{
    return SDL_uitoa(val, s, radix);
}

wchar_t *_itow(int val, wchar_t *s, int radix)
{
    char tmp[32];
    unsigned int i;

    _itoa(val, tmp, radix);
    for (i = 0; tmp[i]; i++)
        s[i] = tmp[i];
    s[i] = 0;

    return s;
}

void _makepath(char *path, const char *drive, const char *dir, const char *fname, const char *ext)
{
    sprintf(path, "%s%s%s%s%s%s%s",
            drive,
            drive && drive[0] && (!dir || dir[0] != '\\') ? "\\" : "",
            dir,
            dir && dir[0] && dir[strlen(dir) - 1] != '\\' ? "\\" : "",
            fname,
            ext && ext[0] && ext[0] != '.' ? "." : "",
            ext);
    dprintf("%s: (\"%s\", \"%s\", \"%s\", \"%s\") = \"%s\"", __FUNCTION__, drive, dir, fname, ext, path);
}

void _splitpath(const char *path, char *drive, char *dir, char *fname, char *ext)
{
    const char *tmp;

    if (isalpha(path[0]) && path[1] == ':')
    {
        if (drive)
        {
            drive[0] = path[0];
            drive[1] = path[1];
            drive[2] = 0;
        }
        path += 2;
    }
    else if (drive)
    {
        drive[0] = 0;
    }

    tmp = strrchr(path, '\\');
    if (tmp)
    {
        if (dir)
        {
            memcpy(dir, path, tmp + 1 - path);
            dir[tmp + 1 - path] = 0;
        }
        path = tmp + 1;
    }
    else if (dir)
    {
        dir[0] = 0;
    }

    tmp = strrchr(path, '.');
    if (tmp)
    {
        if (fname)
        {
            memcpy(fname, path, tmp - path);
            fname[tmp - path] = 0;
        }
        path = tmp;
    }
    else if (fname)
    {
        fname[0] = 0;
    }

    if (ext)
    {
        strcpy(ext, path);
    }

    //dprintf("%s: \"%s\" = (\"%s\", \"%s\", \"%s\", \"%s\")", __FUNCTION__, path, drive, dir, fname, ext);
}

// Misc functions
BOOL WINAPI CloseHandle(HANDLE hObject)
{
    switch ((DWORD)hObject >> 16)
    {
    case 0:
        close((WORD)hObject);
        break;
    case HANDLE_THREAD:
        handles[(WORD)hObject] = NULL;
        break;
    case HANDLE_MUTEX:
        {
            pthread_mutex_t *m = lookup_handle(HANDLE_MUTEX, hObject);
            pthread_mutex_destroy(m);
            free(m);
        }
        handles[(WORD)hObject] = NULL;
        break;
    default:
        DebugBreak();
        return FALSE;
    }

    return TRUE;
}

DWORD WINAPI GetModuleFileNameA(HMODULE hModule, LPSTR lpFileName, DWORD nSize)
{
    unsigned int i, size = strlen(progname);
    DWORD ret;

    if (hModule != NULL)
        DebugBreak();

    if (size < nSize)
    {
        strcpy(lpFileName, progname);
        ret = size;
    }
    else if (nSize)
    {
        memcpy(lpFileName, progname, nSize);
        lpFileName[nSize-1] = 0;
        ret = nSize;
    }

    for (i = 0; lpFileName[i]; i++)
        if (lpFileName[i] == '/')
            lpFileName[i] = '\\';

    return ret;
}

DWORD WINAPI GetLastError()
{
    return last_error;
}

BOOL WINAPI GetVersionExA(LPOSVERSIONINFOA lpVersionInformation)
{
    lpVersionInformation->dwOSVersionInfoSize = sizeof(OSVERSIONINFOA);
    lpVersionInformation->dwMajorVersion = 5;
    lpVersionInformation->dwMinorVersion = 1;
    return TRUE;
}

LONG InterlockedExchange(volatile LONG *Target, LONG Value)
{
    return __sync_lock_test_and_set(Target, Value);
}

LONG InterlockedDecrement(volatile LONG *Addend)
{
    return __sync_fetch_and_sub(Addend, 1);
}

LONG InterlockedIncrement(volatile LONG *Addend)
{
    return __sync_fetch_and_add(Addend, 1);
}

int WINAPI MessageBoxA(HWND hWnd, LPCSTR lpText, LPCSTR lpCaption, UINT uType)
{
    DebugBreak();
}

int WINAPI MulDiv(int nNumber, int nNumerator, int nDenominator)
{
    DebugBreak();
}

HINSTANCE WINAPI ShellExecuteA(HWND hwnd, LPCSTR lpOperation, LPCSTR lpFile, LPCSTR lpParameters, LPCSTR lpDirectory, INT nShowCmd)
{
    DebugBreak();
}

int WINAPI WideCharToMultiByte(UINT CodePage, DWORD dwFlags, LPCWSTR lpWideCharStr, int cchWideChar, LPSTR lpMultiByteStr, int cbMultiByte, LPCCH lpDefaultChar, LPBOOL lpUsedDefaultChar)
{
    DebugBreak();
}

// Socket functions
int WINAPI WSAStartup(WORD wVersionRequested, struct WSAData *lpWSAData)
{
    return 0;
}

int WINAPI WSACleanup()
{
    return 0;
}

SOCKET WINAPI socket(int domain, int type, int protocol)
#undef socket
{
#ifdef __EMSCRIPTEN__
    static int fd = 1024;
    return fd++;
#else
    // sookyboo
    //return socket(domain, type, protocol);
    int fd = socket(domain, type, protocol);
    NETLOG("compat_net: socket(domain=%d, type=%d, proto=%d) = %d\n",
           domain, type, protocol, fd);

    if (fd >= 0) {
        net_dump_flags(fd, "after socket()");
    }

    if (fd >= 0 && type == SOCK_DGRAM) {
        int yes = 1;

        if (setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &yes, sizeof(yes)) < 0) {
            fprintf(stderr, "compat_net: setsockopt(fd=%d, SO_BROADCAST) FAILED errno=%d (%s)\n",
                    fd, errno, strerror(errno));
        } else {
            fprintf(stderr, "compat_net: setsockopt(fd=%d, SO_BROADCAST) ok\n", fd);
        }

        if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0) {
            fprintf(stderr, "compat_net: setsockopt(fd=%d, SO_REUSEADDR) FAILED errno=%d (%s)\n",
                    fd, errno, strerror(errno));
        } else {
            fprintf(stderr, "compat_net: setsockopt(fd=%d, SO_REUSEADDR) ok\n", fd);
        }

#ifdef SO_REUSEPORT
        if (setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &yes, sizeof(yes)) < 0) {
            fprintf(stderr, "compat_net: setsockopt(fd=%d, SO_REUSEPORT) FAILED errno=%d (%s)\n",
                    fd, errno, strerror(errno));
        } else {
            fprintf(stderr, "compat_net: setsockopt(fd=%d, SO_REUSEPORT) ok\n", fd);
        }
#endif
    }

    return fd;
#endif
}

int WINAPI closesocket(SOCKET s)
{
    NETLOG("compat_net: closesocket(fd=%d)\n", (int)s);
    net_dump_sockname((int)s, "closesocket local");
    net_dump_flags((int)s, "closesocket flags");
    int r = close(s);
    if (r == 0) {
        last_socket_error = 0;
        return 0;
    }
    wsa_set_last_from_errno();
    return -1;
}

char *WINAPI inet_ntoa(struct in_addr compat_addr)
#undef in_addr
#undef inet_ntoa
{
    //struct in_addr addr;

    //addr.s_addr = compat_addr.S_un.S_addr;
    //return inet_ntoa(addr);
    // sookyboo

    struct in_addr addr;
    addr.s_addr = compat_addr.S_un.S_addr;

    /* avoid recursion by NOT calling our own inet_ntoa in the log */
    char buf[INET_ADDRSTRLEN] = {0};
    if (!inet_ntop(AF_INET, &addr, buf, sizeof(buf))) {
        snprintf(buf, sizeof(buf), "???");
    }
    fprintf(stderr, "compat_net: inet_ntoa(%s)\n", buf);

    return inet_ntoa(addr); /* this is libc inet_ntoa because of the #undef above */

}

int WINAPI setsockopt(SOCKET s, int level, int opt, const void *value, unsigned int len)
#undef setsockopt
{
#ifdef __EMSCRIPTEN__
    return 0;
#else
    int r = setsockopt(s, level, opt, value, len);
    if (r < 0) {
        fprintf(stderr,
                "compat_net: setsockopt(fd=%d, level=%d, opt=%d) FAILED errno=%d (%s)\n",
                (int)s, level, opt, errno, strerror(errno));

        /* Windows-only / unknown options – just pretend they succeeded. */
        if (level == 65535) {
            fprintf(stderr,
                    "compat_net: treating unknown level=65535/opt=%d as success\n",
                    opt);
            last_socket_error = 0;
            return 0;
        }
    } else {
        last_socket_error = 0;
        fprintf(stderr,
                "compat_net: setsockopt(fd=%d, level=%d, opt=%d) ok\n",
                (int)s, level, opt);
    }
    return r;
#endif
}


//int WINAPI ioctlsocket(SOCKET s, long cmd, unsigned long *argp)
//{
//    int ret;
//
//    switch (cmd)
//    {
//    case 0x4004667f: // FIONREAD
//#ifdef __EMSCRIPTEN__
//        *argp = EM_ASM_INT({
//            return network.available($0);
//        }, s);
//        ret = 0;
//#else
//        ret = ioctl(s, FIONREAD, argp);
//#endif
//        break;
//    default:
//        DebugBreak();
//        ret = -1;
//        break;
//    }
//
//    return ret;
//}
// sookyboo

static int wsa_from_errno(int e)
{
    switch (e) {
#if defined(EWOULDBLOCK)
        case EWOULDBLOCK:
#endif
#if defined(EAGAIN)
# if !defined(EWOULDBLOCK) || (EAGAIN != EWOULDBLOCK)
        case EAGAIN:
# endif
#endif
            return 10035; /* WSAEWOULDBLOCK */
        case EINPROGRESS:
            return 10036; /* WSAEINPROGRESS */
        case EALREADY:
            return 10037; /* WSAEALREADY */
        case EINTR:
            return 10004; /* WSAEINTR */
        case ECONNRESET:
            return 10054; /* WSAECONNRESET */
        case ECONNREFUSED:
            return 10061; /* WSAECONNREFUSED */
        case ETIMEDOUT:
            return 10060; /* WSAETIMEDOUT */
        case ENETUNREACH:
            return 10051; /* WSAENETUNREACH */
        case EHOSTUNREACH:
            return 10065; /* WSAEHOSTUNREACH */
        case EADDRINUSE:
            return 10048; /* WSAEADDRINUSE */
        default:
            return 10000 + e; /* fallback */
    }
}


static void wsa_set_last_from_errno(void)
{
    last_socket_error = (DWORD)wsa_from_errno(errno);
}

int WINAPI ioctlsocket(SOCKET s, long cmd, unsigned long *argp)
{
    int ret;

    switch (cmd)
    {
    case 0x4004667f: // FIONREAD
#ifdef __EMSCRIPTEN__
        ret = 0;
        *argp = EM_ASM_INT({ return network.available($0); }, s);
#else
        ret = ioctl(s, FIONREAD, argp);
        if (ret < 0) {
            wsa_set_last_from_errno();
            return -1;
        }
        last_socket_error = 0;
#endif
        break;

    case 0x8004667e: // FIONBIO (non-blocking)
    {
        if (!argp) {
            errno = EINVAL;
            last_socket_error = 10022u; /* WSAEINVAL */
            return -1;
        }

#ifdef __EMSCRIPTEN__
        /* If your JS networking is always nonblocking-ish, just accept. */
        ret = 0;
#else
        int nonblock = (*argp != 0);

        NETLOG("compat_net: ioctlsocket(FIONBIO) fd=%d request=%s\n",
               (int)s, nonblock ? "NONBLOCK" : "BLOCK");
        net_dump_sockname((int)s, "FIONBIO socket local");
        net_dump_flags((int)s, "FIONBIO before");


        /* Option A: fcntl (recommended, portable) */
        int flags = fcntl((int)s, F_GETFL, 0);
        if (flags < 0) {
            last_socket_error = 10000u + errno;
            return -1;
        }
        if (nonblock) flags |= O_NONBLOCK;
        else          flags &= ~O_NONBLOCK;

        ret = fcntl((int)s, F_SETFL, flags);
        if (ret < 0) {
            last_socket_error = 10000u + errno;
            return -1;
        }
        ret = 0;
        net_dump_flags((int)s, "FIONBIO after");


        /* Option B (alternative): ioctl(FIONBIO) also works on Linux:
           int nb = nonblock;
           ret = ioctl((int)s, FIONBIO, &nb);
        */
#endif

        // optional chatty log:
        // fprintf(stderr, "compat_net: ioctlsocket(fd=%d, FIONBIO=%lu) -> %s\n",
        //         (int)s, *argp, (*argp ? "NONBLOCK" : "BLOCK"));

        last_socket_error = 0;
        break;
    }

    default:
        fprintf(stderr, "compat_net: ioctlsocket(fd=%d, cmd=0x%lx) unsupported\n",
                (int)s, cmd);
        /* IMPORTANT: don't DebugBreak() here; callers may legitimately probe ioctls */
        // DebugBreak();
        last_socket_error = 10022u; /* WSAEINVAL */
        ret = -1;
        break;
    }

    return ret;
}

#ifdef __EMSCRIPTEN__
void build_server_info(void *arg)
{
    static char oldbuf[256];
    static int oldlen;
    char buf[256];
    int dummy[3] = { 0, 0, 0 };
    int length, ready;

    ready = EM_ASM_INT({
        return network.isready();
    });

    if (!ready) {
        oldlen = 0;
        return;
    }

    length = sub_554040(dummy, sizeof(buf), buf);
//    dhexdump(buf, length);
    if (oldlen != length || memcmp(buf, oldbuf, length) != 0)
    {
        memcpy(oldbuf, buf, length);
        oldlen = length;

        EM_ASM_({
            network.registerServer($1 > 0 ? Module.HEAPU8.slice($0, $0 + $1) : null);
        }, oldbuf, oldlen);
    }
}
#endif

// ---- LAN broadcast for native host ----
#ifndef __EMSCRIPTEN__
//static pthread_t g_lan_bcast_thread;
//static int g_lan_bcast_started = 0;
//
//static void *lan_broadcast_thread(void *arg)
//{
//    int sockfd = (int)(intptr_t)arg;
//
//    struct sockaddr_in bcast;
//    memset(&bcast, 0, sizeof(bcast));
//    bcast.sin_family = AF_INET;
//    bcast.sin_port   = htons(18590);
//    bcast.sin_addr.s_addr = htonl(INADDR_BROADCAST);
//
//    static char oldbuf[256];
//    static int oldlen = 0;
//    char buf[256];
//    int dummy[3] = {0, 0, 0};
//
//    fprintf(stderr, "compat_net: lan_broadcast_thread started on fd=%d\n", sockfd);
//
//    for (;;)
//    {
//        int len = sub_554040(dummy, sizeof(buf), buf);
//        dhexdump(buf, len);
//        if (len <= 0) {
//            // Nothing to advertise, just wait and retry
//            SDL_Delay(1000);
//            continue;
//        }
//
//        // Optional: only spam when the packet actually changes (like Emscripten)
//        if (len == oldlen && memcmp(buf, oldbuf, len) == 0) {
//            SDL_Delay(1000);
//            continue;
//        }
//
//        memcpy(oldbuf, buf, len);
//        oldlen = len;
//
//        fprintf(stderr,
//                "compat_net: LAN broadcast from fd=%d -> 255.255.255.255:18590, len=%d\n",
//                sockfd, len);
//
//        int r = sendto(sockfd, buf, len, 0,
//                       (struct sockaddr *)&bcast, sizeof(bcast));
//        if (r < 0) {
//            fprintf(stderr, "compat_net: LAN broadcast sendto(fd=%d) FAILED errno=%d (%s)\n",
//                    sockfd, errno, strerror(errno));
//        }
//
//        SDL_Delay(1000);  // 1 second between adverts (tune as needed)
//    }
//
//    return NULL;
//}

static void compat_hexdump(const char *tag, const void *buf, size_t len)
{
    const unsigned char *p = (const unsigned char *)buf;
    size_t i;

    fprintf(stderr, "%s len=%zu", tag ? tag : "hexdump", len);

    for (i = 0; i < len; ++i) {
        if ((i % 16) == 0) {
            fprintf(stderr, "\n  %04zx:", i);
        }
        fprintf(stderr, " %02x", p[i]);
    }
    fprintf(stderr, "\n");
}

static void nox_log_server_info_packet(const unsigned char *pkt,
                                       size_t len,
                                       const struct sockaddr_in *from)
{
    if (!pkt || len < 73) {
        return;
    }

    /* Packet type: buf[2] == 13 for server info */
    if (pkt[2] != 13) {
        return;
    }

    char ipbuf[INET_ADDRSTRLEN] = "0.0.0.0";
    uint16_t port = 0;
    if (from) {
        inet_ntop(AF_INET, &from->sin_addr, ipbuf, sizeof(ipbuf));
        port = ntohs(from->sin_port);
    }

    uint8_t curPlayers = pkt[3];
    uint8_t maxPlayers = pkt[4];

    const char *mapName = (const char *)&pkt[10];
    const char *serverName = (const char *)&pkt[72];

    /* Be defensive: ensure strings are NUL-terminated within packet bounds. */
    char mapBuf[32];
    char nameBuf[64];

    /* mapName is supposed to fit in 10..18, but we’ll just clamp. */
    {
        size_t maxMapLen = sizeof(mapBuf) - 1;
        size_t i;
        for (i = 0; 10 + i < len && i < maxMapLen; ++i) {
            char c = pkt[10 + i];
            mapBuf[i] = c;
            if (!c) break;
        }
        mapBuf[maxMapLen] = 0;
        if (i == maxMapLen)
            mapBuf[maxMapLen] = 0;
    }

    {
        size_t maxNameLen = sizeof(nameBuf) - 1;
        size_t i;
        size_t off = 72;
        for (i = 0; off + i < len && i < maxNameLen; ++i) {
            char c = pkt[off + i];
            nameBuf[i] = c;
            if (!c) break;
        }
        nameBuf[maxNameLen] = 0;
        if (i == maxNameLen)
            nameBuf[maxNameLen] = 0;
    }

    fprintf(stderr,
            "NET server info from %s:%u\n"
            "  Name   : %s\n"
            "  Players: %u/%u\n"
            "  Map    : %s\n",
            ipbuf, port,
            nameBuf,
            (unsigned)curPlayers,
            (unsigned)maxPlayers,
            mapBuf);
}

struct fake_srv_pkt {
    int used;
    int sockfd;
    size_t len;
    unsigned char data[256];
    struct sockaddr_in from;
};

static struct fake_srv_pkt g_fake_packets[4];  /* support up to 4 internet servers */

//static void queue_internet_server_reply(const unsigned char *ping,
//                                        size_t ping_len)
//{
//    /* Find a free slot. */
//    struct fake_srv_pkt *slot = NULL;
//    for (int i = 0; i < (int)(sizeof(g_fake_packets)/sizeof(g_fake_packets[0])); ++i) {
//        if (!g_fake_packets[i].used) {
//            slot = &g_fake_packets[i];
//            break;
//        }
//    }
//    if (!slot) return; /* queue full, just skip */
//
//    /* Build a minimal valid server-info packet. */
//    unsigned char *out = slot->data;
//    memset(out, 0, sizeof(slot->data));
//
//    /* Header */
//    out[0] = 0;
//    out[1] = 0;
//    out[2] = 13;          /* type: server info */
//    out[3] = 0;           /* cur players */
//    out[4] = 31;          /* max players */
//
//    /* Mode bits – use some sane defaults, e.g. arena mode similar to LAN packet you saw.
//       For now we just copy nibbles from a LAN packet later if you like, but here we'll
//       pick something simple. */
//    out[5] = 0x0F;        /* low nibble flags */
//    out[6] = 0x0F;        /* high nibble flags */
//
//    /* game/session id (just some non-zero value) */
//    *(uint32_t *)&out[7] = 0xFFFFFFFFu;
//
//    /* Map name at offset 10 – keep it short (< 9 chars) */
//    strcpy((char *)&out[10], "!takest");  /* from your JSON */
//
//    /* The rest of the header can be mostly defaulted to 0 or FFFF.
//       We *do* want to echo the ping cookie (bytes 8..11 in the original ping). */
//    if (ping_len >= 12) {
//        *(uint32_t *)&out[44] = *(const uint32_t *)&ping[8];
//    }
//
//    /* Optional: mark as 1024x768 by mimicking your LAN packet fields.
//       For now we'll just leave most flags as zero; the client only really cares
//       about name/map/players. */
//
//    /* questStage at 68–69 = 0 (non-quest) */
//
//    /* Server name at offset 72 */
//    const char *srvName = "USA Server #1";
//    strcpy((char *)&out[72], srvName);
//
//    size_t total_len = 72 + strlen(srvName) + 1;
//    if (total_len > sizeof(slot->data))
//        total_len = sizeof(slot->data);
//
//    slot->len = total_len;
//
//    memset(&slot->from, 0, sizeof(slot->from));
//    slot->from.sin_family = AF_INET;
//    slot->from.sin_port   = htons(18590);
//    inet_pton(AF_INET, "173.208.129.98", &slot->from.sin_addr);
//
//    slot->used = 1;
//
//    fprintf(stderr,
//            "compat_net: queued internet server '%s' as fake reply (%zu bytes)\n",
//            srvName, total_len);
//}

/* Cache the fetched list so we don't HTTP on every ping */
static int g_srv_cache_valid = 0;
static nox_server_row g_srv_cache[32];
static size_t g_srv_cache_n = 0;
static size_t g_srv_rr = 0; /* round-robin index */

static long g_srv_cache_last_ok = 0;   /* epoch seconds of last successful refresh */
static long g_srv_cache_next_try = 0;  /* epoch seconds when we may retry after failure */

static long g_last_lobby_reg = 0;      // epoch seconds
static int  g_lobby_reg_inflight = 0;  // simple guard
static pthread_mutex_t g_lobby_reg_mu = PTHREAD_MUTEX_INITIALIZER;

static int nox_lobby_register_period_sec(void) {
    int v = nox_env_int("NOX_LOBBY_REGISTER_PERIOD", 20);
    if (v < 5) v = 5;
    return v;
}

static int nox_should_register_lobby(void) {
    // default OFF unless enabled
    return nox_env_truthy(getenv("NOX_LOBBY_REGISTER_ENABLE"));
}

static long nox_now_sec(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (long)tv.tv_sec;
}

/* Fetch+parse once (best-effort). */
static void nox_refresh_server_cache_once(void)
{
    /* TTL seconds; default 30; 0 = never refresh after first success */
    int ttl = nox_env_int("NOX_SERVER_CACHE_TTL", 30);
    if (ttl < 30) ttl = 30;

    long now = nox_now_sec();

    /* If we already have a valid cache: honor TTL and failure backoff */
    if (g_srv_cache_valid) {
        if (ttl == 0) {
            /* never refresh after first attempt/success */
            return;
        }
        if (g_srv_cache_last_ok != 0 && (now - g_srv_cache_last_ok) < (long)ttl) {
            return; /* cache still fresh */
        }
        if (g_srv_cache_next_try != 0 && now < g_srv_cache_next_try) {
            return; /* still backing off after a failure */
        }
        /* else: TTL expired or never had success -> attempt refresh */
    }

    char json[64 * 1024];
    int n = nox_fetch_games_list_json(json, sizeof(json));
    if (n <= 0) {
        fprintf(stderr, "compat_net: fetch games/list failed\n");

        /* prevent hammering, but DO NOT wipe any existing cache */
        g_srv_cache_valid = 1;

        /* backoff retry (default 10s) */
        g_srv_cache_next_try = now + 10;
        return;
    }

    /* Success: parse and commit */
    g_srv_cache_n = nox_parse_games_list_json(
        json,
        g_srv_cache,
        sizeof(g_srv_cache) / sizeof(g_srv_cache[0])
    );

    g_srv_cache_valid = 1;
    g_srv_cache_last_ok = now;
    g_srv_cache_next_try = 0;

    fprintf(stderr, "compat_net: games/list parsed %zu servers\n", g_srv_cache_n);
}

/* Find a free fake packet slot. */
static struct fake_srv_pkt *alloc_fake_slot(void)
{
    for (int i = 0; i < (int)(sizeof(g_fake_packets)/sizeof(g_fake_packets[0])); ++i) {
        if (!g_fake_packets[i].used) {
            /* wipe stale metadata from previous use */
            g_fake_packets[i].sockfd = -1;
            g_fake_packets[i].len = 0;
            memset(&g_fake_packets[i].from, 0, sizeof(g_fake_packets[i].from));
            return &g_fake_packets[i];
        }
    }
    return NULL;
}

static void queue_internet_server_reply(int sockfd, const unsigned char *ping,
                                        size_t ping_len)
{
#ifndef __EMSCRIPTEN__
    if (!nox_should_inject_internet_servers()) {
        return; /* env disables injection */
    }

    /* Fetch/parse list once (or keep existing cache if fetch fails) */
    nox_refresh_server_cache_once();

    /* If still nothing cached, do nothing: LAN-only */
    if (g_srv_cache_n == 0) {
        return;
    }

    /* Queue as many servers as we can fit right now (round-robin across pings). */
    size_t queued = 0;
    size_t slots_avail = sizeof(g_fake_packets)/sizeof(g_fake_packets[0]);

    for (size_t tries = 0; tries < g_srv_cache_n && queued < slots_avail; ++tries) {
        struct fake_srv_pkt *slot = alloc_fake_slot();
        if (!slot) break;

        const nox_server_row *row = &g_srv_cache[(g_srv_rr + tries) % g_srv_cache_n];

        /* skip known-bad servers via env deny lists */
        if (row->addr[0] && nox_is_bad_server_ip(row->addr)) {
            continue;
        }
        if (row->name[0] && nox_is_bad_server_name(row->name)) {
            continue;
        }

        unsigned char *out = slot->data;
        memset(out, 0, sizeof(slot->data));

        out[0] = 0;
        out[1] = 0;
        out[2] = 13;                    /* server info */
        out[3] = row->players_cur;
        out[4] = row->players_max ? row->players_max : 31;
        out[5] = 0x0F;
        out[6] = 0x0F;

        {
            uint32_t sid = 0xFFFFFFFFu;
            memcpy(&out[7], &sid, sizeof(sid));
        }

        /* map at offset 10 */
        if (row->map[0]) {
            size_t mlen = strnlen(row->map, 31);
            memcpy(&out[10], row->map, mlen);
            out[10 + mlen] = 0;
        }

        /* flags are little-endian at 28..29 */
        uint16_t base = nox_lobby_get_last_serverinfo_flags();

        /* If we haven't seen a real serverinfo yet, use a known-good baseline.
           From your real packet: 0x2087, where the non-mode bits are 0x2007. */
        if (base == 0) base = 0x2007;

        uint16_t modebit = nox_mode_to_flagbit(row->mode);

        /* preserve everything except mode bits, then apply mode */
        uint16_t flags = (uint16_t)((base & (uint16_t)~NOX_MODE_MASK) | modebit);

        /* write LE */
        out[28] = (unsigned char)(flags & 0xFF);
        out[29] = (unsigned char)((flags >> 8) & 0xFF);
        NETLOG("compat_net: fake flags base=0x%04x mode='%s' modebit=0x%04x final=0x%04x\n",
               base, row->mode, modebit, flags);

        /* echo ping cookie (bytes 8..11 from ping) at offset 44 */
        if (ping && ping_len >= 12) {
            memcpy(&out[44], &ping[8], 4);
        }

        /* server name at offset 72 */
        if (row->name[0]) {
            strncpy((char *)&out[72], row->name, sizeof(slot->data) - 72 - 1);
            out[sizeof(slot->data) - 1] = 0;
        }

        /* compute packet length */
        {
            size_t name_len = strnlen((char *)&out[72], sizeof(slot->data) - 72);
            size_t total_len = 72 + name_len + 1;
            if (total_len > sizeof(slot->data)) total_len = sizeof(slot->data);
            slot->len = total_len;
        }

        if (g_packetlog()) {
            compat_hexdump("NET FAKE QUEUE", slot->data, slot->len);
        }

        /* sockaddr from JSON */
        memset(&slot->from, 0, sizeof(slot->from));
        slot->from.sin_family = AF_INET;
        slot->from.sin_port   = htons(row->port ? row->port : 18590);
        if (inet_pton(AF_INET, row->addr, &slot->from.sin_addr) != 1) {
            /* bad IP in JSON; skip this one */
            slot->used = 0;
            continue;
        }

        slot->sockfd = sockfd;

        slot->used = 1;
        queued++;

        NETLOG("compat_net: queued internet server '%s' (%s:%u) map=%s %u/%u\n",
                row->name, row->addr, (unsigned)row->port, row->map,
                (unsigned)row->players_cur, (unsigned)row->players_max);
    }

    /* advance round-robin pointer */
    if (g_srv_cache_n) {
        g_srv_rr = (g_srv_rr + queued) % g_srv_cache_n;
    }

#else
    (void)ping; (void)ping_len;
#endif
}

#endif
// ---- end LAN broadcast code ----


// compat.c (somewhere above sendto)
typedef struct {
    char name[64];
    char map[32];
    unsigned cur;
    unsigned max;
    unsigned port;
} reg_job_t;

// declare your lobby.c function
int nox_lobby_register_game(const char *name, const char *map, unsigned cur, unsigned max, unsigned port);

static void *lobby_register_thread(void *arg)
{
    reg_job_t *j = (reg_job_t *)arg;

    // Best-effort UPnP mapping (runs in this background thread, not in sendto()).
    // Controlled by NOX_UPNP_ENABLE and related NOX_UPNP_* env vars.
    (void)nox_upnp_ensure_mapped_from_env();

    // best-effort
    int rc = nox_lobby_register_game(j->name, j->map, j->cur, j->max, j->port);
    if (rc != 0) {
        fprintf(stderr, "compat_net: lobby register FAILED name='%s' map='%s'\n", j->name, j->map);
    } else {
        NETLOG("compat_net: lobby register OK name='%s' map='%s'\n", j->name, j->map);
    }

    free(j);

    pthread_mutex_lock(&g_lobby_reg_mu);
    g_lobby_reg_inflight = 0;
    pthread_mutex_unlock(&g_lobby_reg_mu);

    return NULL;
}


/* Cache latest flags seen in sub_554040 serverinfo packet (offset 28). */

static void log_serverinfo_packet(const unsigned char *pkt, size_t len, const char *where)
{
    if (!g_packetlog()) {
        return;
    }

    if (!pkt) return;



    PACKETLOG("compat_net: %s serverinfo len=%zu type=0x%02x\n",
            where ? where : "?", len, (len >= 3) ? pkt[2] : 0xff);

    // We care about flags at offset 28..29 (little endian)
    if (len >= 30) {
        unsigned b28 = pkt[28];
        unsigned b29 = pkt[29];
        uint16_t flags = (uint16_t)b28 | ((uint16_t)b29 << 8);
        PACKETLOG("compat_net: %s flags bytes: pkt[28]=0x%02x pkt[29]=0x%02x -> flags=0x%04x\n",
                where ? where : "?", b28, b29, (unsigned)flags);
    } else {
        PACKETLOG("compat_net: %s too short for flags (need >=30)\n", where ? where : "?");
    }

    // Optional: log some other useful fields to sanity-check we’re parsing right packet
    if (len >= 74) {
        PACKETLOG("compat_net: %s players cur=%u max=%u map@10='%.20s' name@72='%.20s'\n",
                where ? where : "?", (unsigned)pkt[3], (unsigned)pkt[4],
                (const char *)&pkt[10], (const char *)&pkt[72]);
    }

    // Full hexdump (guarded by packet logging toggle)
    if (g_packetlog()) {
        compat_hexdump(where ? where : "serverinfo", pkt, len);
    }
}


static void maybe_register_lobby_from_serverinfo(int sockfd, const unsigned char *pkt, size_t len)
{
    if (!nox_should_register_lobby()) return;
    if (!pkt || len < 73) return;
    if (pkt[2] != 0x0D) return; // server info
        log_serverinfo_packet(pkt, len, "maybe_register_lobby_from_serverinfo");
        /* cache serverinfo flags (uint16 LE at offset 28) for later mode debugging */
        if (len >= 30) {
            uint16_t flags =
                (uint16_t)((unsigned)pkt[28] | ((unsigned)pkt[29] << 8));
            nox_lobby_set_last_serverinfo_flags(flags);
            fprintf(stderr, "compat_net: serverinfo flags=0x%04x\n", (unsigned)flags);

        }


    long now = nox_now_sec();
    const int period = nox_lobby_register_period_sec();

    pthread_mutex_lock(&g_lobby_reg_mu);

    if (g_lobby_reg_inflight) {
        pthread_mutex_unlock(&g_lobby_reg_mu);
        return;
    }
    if (g_last_lobby_reg != 0 && (now - g_last_lobby_reg) < period) {
        pthread_mutex_unlock(&g_lobby_reg_mu);
        return;
    }

    g_last_lobby_reg = now;
    g_lobby_reg_inflight = 1;

    pthread_mutex_unlock(&g_lobby_reg_mu);

    // Extract fields from the *real* packet we’re sending
    reg_job_t *j = (reg_job_t *)calloc(1, sizeof(*j));
    if (!j) return;

    j->cur = pkt[3];
    j->max = pkt[4];

    // map at offset 10 (your earlier code already does defensive reads)
    strncpy(j->map, (const char *)&pkt[10], sizeof(j->map) - 1);
    j->map[sizeof(j->map) - 1] = 0;

    // name at offset 72
    strncpy(j->name, (const char *)&pkt[72], sizeof(j->name) - 1);
    j->name[sizeof(j->name) - 1] = 0;

    // port: prefer bound local port of the socket if possible
    j->port = 18590;
#if !defined(__arm__) && !defined(__arm) && !defined(__ARM_EABI__)
    {
        struct sockaddr_in s;
        socklen_t slen = sizeof(s);
        if (getsockname(sockfd, (struct sockaddr *)&s, &slen) == 0 && s.sin_family == AF_INET) {
            unsigned p = (unsigned)ntohs(s.sin_port);
            if (p) j->port = p;
        }
    }
#endif

    // spawn background thread
    pthread_t th;
    if (pthread_create(&th, NULL, lobby_register_thread, j) == 0) {
        pthread_detach(th);
    } else {
        free(j);
        pthread_mutex_lock(&g_lobby_reg_mu);
        g_lobby_reg_inflight = 0;
        pthread_mutex_unlock(&g_lobby_reg_mu);
    }
}


static int g_lobby_started = 0;

static void lobby_kickoff_if_needed(int sockfd)
{
    if (!nox_should_register_lobby()) return;

    if (__sync_lock_test_and_set(&g_lobby_started, 1) != 0)
        return; // already started

    // Build a server-info packet and register once immediately.
    unsigned char buf[256];
    int dummy[3] = {0,0,0};
    int len = sub_554040(dummy, sizeof(buf), (char*)buf);
    if (len > 0) {
        // sub_554040 may produce either ping or serverinfo depending on context;
        // but your log shows serverinfo is type 0x0D when it’s correct.
        maybe_register_lobby_from_serverinfo(sockfd, buf, (size_t)len);
    }
}

// ---- Lobby register polling thread (native) ----
#ifndef __EMSCRIPTEN__

static int g_lobby_poll_thread_started = 0;

static void *lobby_poll_register_thread(void *arg)
{
    (void)arg;

    for (;;)
    {
        int period = nox_lobby_register_period_sec();
        if (period < 5) period = 5;

        if (!nox_should_register_lobby()) {
            // If disabled, don't hammer; just idle briefly and re-check.
            SDL_Delay(1000);
            continue;
        }

        unsigned char buf[256];
        int dummy[3] = {0, 0, 0};
        int len = sub_554040(dummy, sizeof(buf), (char *)buf);

        // Always log what sub_554040 returns
        PACKETLOG("compat_net: sub_554040() -> len=%d\n", len);
        if (len > 0) {
            unsigned type = (len >= 3) ? buf[2] : 0xff;
            PACKETLOG("compat_net: sub_554040 packet type=0x%02x\n", type);
            if (len >= 30 && buf[2] == 0x0D) {
                log_serverinfo_packet(buf, (size_t)len, "lobby_poll_register_thread");
            }

            // Hexdump if packet logging enabled
            if (g_packetlog()) {
                compat_hexdump("NET sub_554040", buf, (size_t)len);
            }

            // If it's a server-info packet (0x0D), register it.
            if (len >= 73 && buf[2] == 0x0D) {
                /* cache serverinfo flags (uint16 LE at offset 28) */
                if (len >= 30) {
                    uint16_t flags =
                        (uint16_t)((unsigned)buf[28] | ((unsigned)buf[29] << 8));
                    nox_lobby_set_last_serverinfo_flags(flags);
                    PACKETLOG("compat_net: serverinfo flags=0x%04x\n",
                            (unsigned)flags);
                }

                // Extract fields (defensive)
                char name[64] = {0};
                char map[32]  = {0};

                strncpy(map,  (const char *)&buf[10], sizeof(map)  - 1);
                strncpy(name, (const char *)&buf[72], sizeof(name) - 1);

                unsigned cur = buf[3];
                unsigned max = buf[4];

                // Port: default 18590; allow override if you ever add NOX_SERVER_PORT
                unsigned port = 18590;
                {
                    const char *p = getenv("NOX_SERVER_PORT");
                    if (p && *p) {
                        int pv = atoi(p);
                        if (pv > 0 && pv < 65536) port = (unsigned)pv;
                    }
                }

                // Best-effort UPnP mapping in the background thread (optional)
                (void)nox_upnp_ensure_mapped_from_env();

                int rc = nox_lobby_register_game(name, map, cur, max, port);
                if (rc != 0) {
                    fprintf(stderr,
                            "compat_net: lobby register FAILED name='%s' map='%s' %u/%u port=%u\n",
                            name, map, cur, max, port);
                } else {
                    NETLOG("compat_net: lobby register OK name='%s' map='%s' %u/%u port=%u\n",
                            name, map, cur, max, port);
                }
            }
        }

        SDL_Delay((Uint32)period * 1000);
    }

    return NULL;
}

static void lobby_poll_thread_start_once(void)
{
    // Start once, ever. If called again, do nothing.
    if (__sync_lock_test_and_set(&g_lobby_poll_thread_started, 1) != 0)
        return;

    pthread_t th;
    if (pthread_create(&th, NULL, lobby_poll_register_thread, NULL) == 0) {
        pthread_detach(th);
        fprintf(stderr, "compat_net: lobby polling thread started\n");
    } else {
        // If thread creation failed, allow retry on next bind()
        __sync_lock_test_and_set(&g_lobby_poll_thread_started, 0);
        fprintf(stderr, "compat_net: lobby polling thread FAILED to start\n");
    }
}

#endif // !__EMSCRIPTEN__

int WINAPI bind(int sockfd, const struct sockaddr *addr, unsigned int addrlen)
#undef bind
{
    int ret;
#ifdef __EMSCRIPTEN__
    static long updater = -1;

    EM_ASM_({
        network.bind($0);
    }, sockfd);

    if (updater == -1)
        updater = emscripten_set_interval(build_server_info, 1000, NULL);

    ret = 0;
#else
    lobby_poll_thread_start_once();
    const struct sockaddr_in *in = (const struct sockaddr_in *)addr;
    struct sockaddr_in tmp;
    char ipbuf[INET_ADDRSTRLEN] = "0.0.0.0";
    uint16_t port = 0;

    if (addrlen >= sizeof(struct sockaddr_in) && in->sin_family == AF_INET) {
        port = ntohs(in->sin_port);
        inet_ntop(AF_INET, &in->sin_addr, ipbuf, sizeof(ipbuf));

        NETLOG(
                "compat_net: bind(fd=%d, ip=%s, port=%u)\n",
                sockfd, ipbuf, port);

        /* Check if this is a UDP broadcast socket the game is binding to port 0.
           That’s almost certainly the LAN discovery socket – on Linux this needs
           to listen on the *same* port (18590) that the host is broadcasting to. */
        int sock_type = 0, broadcast = 0;
        socklen_t optlen = sizeof(int);

        if (getsockopt(sockfd, SOL_SOCKET, SO_TYPE, &sock_type, &optlen) == 0 &&
            sock_type == SOCK_DGRAM) {
            optlen = sizeof(int);
            NETLOG("compat_net: bind requested fd=%d\n", sockfd);
            net_dump_sockname(sockfd, "pre-bind getsockname");
            net_dump_flags(sockfd, "pre-bind flags");

            if (getsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &broadcast, &optlen) == 0 &&
                broadcast && port == 0) {

                tmp = *in;
                tmp.sin_port = htons(18590);

                NETLOG("compat_net: FORCING bind fd=%d from port 0 -> 18590 (broadcast discovery heuristic)\n",
                       sockfd);

                addr = (const struct sockaddr *)&tmp;
            }
        }
    }

    ret = bind(sockfd, (__CONST_SOCKADDR_ARG)addr, addrlen);
    if (ret < 0) {
        if (errno == EADDRINUSE) {
            last_socket_error = 10048u; /* WSAEADDRINUSE */
        }
        NETLOG("compat_net: bind(fd=%d) FAILED errno=%d (%s)\n",
                sockfd, errno, strerror(errno));
        return -1;
    }
    if (ret == 0) {
        /* We can decide kickoff purely from the bind address we passed in.
           This avoids getsockname() prototype issues on some ARM/glibc + Win32 shim builds. */
        if (addrlen >= sizeof(struct sockaddr_in)) {
            const struct sockaddr_in *bin = (const struct sockaddr_in *)addr;
            if (bin->sin_family == AF_INET && ntohs(bin->sin_port) == 18590) {
                lobby_kickoff_if_needed(sockfd);
            }
        }
    }
    last_socket_error = 0;
    net_dump_sockname(sockfd, "post-bind getsockname");
    net_dump_flags(sockfd, "post-bind flags");

    /* Log the final bound address/port after bind succeeds.
       On some 32-bit ARM/glibc combos, the getsockname prototype
       (__SOCKADDR_ARG) clashes with our Win32 shims, so only do
       this on non-ARM builds. */
#if !defined(__arm__) && !defined(__arm) && !defined(__ARM_EABI__)
    if (addrlen >= sizeof(struct sockaddr_in)) {
        struct sockaddr_in s;
        socklen_t slen = sizeof(s);
        if (getsockname(sockfd, (struct sockaddr *)&s, &slen) == 0) {
            NETLOG("compat_net: bind(fd=%d) now at %s:%u\n",
                    sockfd,
                    inet_ntoa(s.sin_addr),
                    ntohs(s.sin_port));
        }
    }
#endif
#endif
    return ret;
}

static void nox_log_rx_pkt(const char *tag,
                           const void *buf, int len,
                           const struct sockaddr_in *from)
{
    if (!buf || len < 0) return;

    unsigned type = (len >= 3) ? ((const unsigned char *)buf)[2] : 0xFF;

    if (from) {
        char ipbuf[INET_ADDRSTRLEN] = {0};
        inet_ntop(AF_INET, &from->sin_addr, ipbuf, sizeof(ipbuf));
        PACKETLOG("compat_net: RX %s %d bytes from %s:%u type=0x%02x\n",
                tag ? tag : "?", len, ipbuf, ntohs(from->sin_port), type);
    } else {
        PACKETLOG("compat_net: RX %s %d bytes (no addr) type=0x%02x\n",
                tag ? tag : "?", len, type);
    }

    /* Useful for serverinfo: flags at 28..29 */
    if (len >= 30) {
        const unsigned char *p = (const unsigned char *)buf;
        uint16_t f = (uint16_t)p[28] | ((uint16_t)p[29] << 8);
        PACKETLOG("compat_net: RX %s flags=0x%04x (b28=0x%02x b29=0x%02x)\n",
                tag ? tag : "?", (unsigned)f, (unsigned)p[28], (unsigned)p[29]);
    }

    /* Hexdump only when packet logging is enabled */
    if (g_packetlog()) {
        compat_hexdump(tag ? tag : "NET RX", buf, (size_t)len);
    }
}

int WINAPI recvfrom(int sockfd, void *buffer, unsigned int length, int flags,
                    struct sockaddr *addr, unsigned int *addrlen)
#undef recvfrom
{
#ifdef __EMSCRIPTEN__
    int ret;
    struct sockaddr_in *addr_in = addr;
    ret = EM_ASM_INT(({
        const [ remote, port, data ] = network.recvfrom($4);
        if (remote === null)
            return -1;
        const length = Math.min(data.length, $1);
        Module.HEAPU8.set(new Uint8Array(data, 0, length), $0);
        if ($2) {
            Module.HEAPU32[$2 >> 2] = remote;
        }
        if ($3) {
            Module.HEAPU8[$3] = port >> 8;
            Module.HEAPU8[$3 + 1] = port >> 0;
        }
        return length;
    }), buffer, length, addr_in ? &addr_in->sin_addr : NULL, addr_in ? &addr_in->sin_port : NULL, sockfd);
    if (addr_in)
        addr_in->sin_family = AF_INET;
    return ret;
#else
    /* First, see if we have a fake server packet queued. */
    for (int i = 0; i < (int)(sizeof(g_fake_packets)/sizeof(g_fake_packets[0])); ++i) {
        struct fake_srv_pkt *slot = &g_fake_packets[i];
        if (!slot->used) continue;
        if (slot->sockfd != sockfd) continue;
        if (slot->len > length) continue;

        memcpy(buffer, slot->data, slot->len);
        nox_log_rx_pkt("FAKE", buffer, (int)slot->len, &slot->from);

        /* Fill addr if caller provided a buffer; don't require it. */
        if (addr && addrlen) {
            unsigned int want = *addrlen;

            if (want >= sizeof(struct sockaddr_in)) {
                memcpy(addr, &slot->from, sizeof(struct sockaddr_in));
                *addrlen = sizeof(struct sockaddr_in);
            } else {
                /* Caller buffer too small (or 0): don't write it, just report size. */
                *addrlen = sizeof(struct sockaddr_in);
            }
        }

        int ret = (int)slot->len;
        slot->used = 0;

        NETLOG("compat_net: returning fake internet server packet (%d bytes) on fd=%d\n",
                ret, sockfd);

        return ret;
    }

    /* No fake packets waiting: fall back to real recvfrom. */
    int r = recvfrom(sockfd, buffer, length, flags,
                     (__SOCKADDR_ARG)addr, addrlen);
    if (r >= 0) {
        last_socket_error = 0;
    } else {
        wsa_set_last_from_errno();
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            NETLOG("compat_net: recvfrom(fd=%d) -> EAGAIN/EWOULDBLOCK (wsa=%u)\n",
                   sockfd, last_socket_error);
        } else {
            NETLOG("compat_net: recvfrom(fd=%d) FAIL errno=%d(%s) wsa=%u\n",
                   sockfd, errno, strerror(errno), last_socket_error);
        }
        net_dump_sockname(sockfd, "recvfrom error local");
        net_dump_flags(sockfd, "recvfrom error flags");
        return -1;
    }

    if (r >= 0) {
        if (addr && addrlen && *addrlen >= sizeof(struct sockaddr_in)) {
            struct sockaddr_in *in = (struct sockaddr_in *)addr;
            nox_log_rx_pkt("REAL", buffer, r, in);
            uint16_t sport = ntohs(in->sin_port);

            /* Heuristic: only dump Nox UDP (18590) and only “non-server-info”
               packets, since server-info is already decoded/logged. */
//            if (sport == 18590 && r >= 3) {
//                const unsigned char *pkt = (const unsigned char *)buffer;
//
//                /* 0x0D is server info (already parsed); dump everything else. */
//                if (pkt[2] != 0x0D) {
//                    compat_hexdump("NET recvfrom 18590", buffer, (size_t)r);
//                }
//            }
//
//            /* Only bother parsing packets on the Nox LAN discovery port. */
//            if (ntohs(in->sin_port) == 18590 && r >= 12) {
//                const unsigned char *pkt = (const unsigned char *)buffer;
//
//                /* 0x0C = discovery ping, 0x0D = server info; we only decode replies. */
//                if (pkt[2] == 0x0D && (size_t)r >= 73) {
//                    nox_log_server_info_packet(pkt, (size_t)r, in);
//                }
//            }

            char ipbuf[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &in->sin_addr, ipbuf, sizeof(ipbuf));

//            fprintf(stderr,
//                    "compat_net: recvfrom(fd=%d) <= %d bytes from %s:%u\n",
//                    sockfd, r, ipbuf, ntohs(in->sin_port));
        } else {
//            fprintf(stderr,
//                    "compat_net: recvfrom(fd=%d) <= %d bytes (no addr)\n",
//                    sockfd, r);
        }
//    } else if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
//        fprintf(stderr,
//                "compat_net: recvfrom(fd=%d) FAILED errno=%d (%s)\n",
//                sockfd, errno, strerror(errno));
//    }
    }

    return r;
#endif
}

int WINAPI sendto(int sockfd, void *buffer, unsigned int length, int flags,
                  const struct sockaddr *addr, unsigned int addrlen)
#undef sendto
{
#ifdef __EMSCRIPTEN__
    struct sockaddr_in *addr_in = (struct sockaddr_in *)addr;
    unsigned int dest = addr_in->sin_addr.s_addr;
    unsigned char *p = buffer;

    // broadcast packet
    if (dest == 0xffffffff)
    {
        if (p[2] == 12)
        {
            EM_ASM_({
                network.isready() && network.listServers($0, $1)
            }, *(DWORD *)(p + 8), sockfd);
        }
//        fprintf(stderr, "compat_net: sendto(EMSCRIPTEN, fd=%d, broadcast len=%u)\n",
//                sockfd, length);
        return length;
    }

//    fprintf(stderr, "compat_net: sendto(EMSCRIPTEN, fd=%d, len=%u)\n",
//            sockfd, length);

    return EM_ASM_INT({
        network.sendto($3, $2, $4, Module.HEAPU8.slice($0, $0 + $1));
        return $1;
    }, buffer, length, dest, sockfd, ntohs(addr_in->sin_port));
#else
    const struct sockaddr_in *in = (const struct sockaddr_in *)addr;
    uint32_t dest_ip = in->sin_addr.s_addr;
    uint16_t dest_port = ntohs(in->sin_port);

    char ipbuf[INET_ADDRSTRLEN] = {0};
    inet_ntop(AF_INET, &in->sin_addr, ipbuf, sizeof(ipbuf));
//    /* Dump discovery / game packets to port 18590 so we can clone them later */
//    if (dest_port == 18590) {
//        fprintf(stderr, "NET sendto 18590 len=%u\n", length);
//        if (length <= 64 || length == 520) { // small control pkts + full chunk-size pkts if you ever send them
//            compat_hexdump("NET sendto 18590", buffer, length);
//        }
//    }

    /* Discovery ping: broadcast to 18590 with type 0x0C at byte 2. */
    if (dest_port == 18590 && length >= 3) {
        const unsigned char *p = (const unsigned char *)buffer;
        if (p[2] == 0x0C) {
            /* Queue our hardcoded internet server reply. */
            queue_internet_server_reply(sockfd, p, length);
        }

//        if (p[2] == 0x0D) {
//            // optional: record / hexdump
//            if (g_packetlog()) {
//                compat_hexdump("NET sendto serverinfo", buffer, length);
//            }
//            // do rate-limited lobby register
//            maybe_register_lobby_from_serverinfo(sockfd, p, (size_t)length);
//        }
    }

//    fprintf(stderr, "compat_net: sendto(fd=%d -> %s:%u, len=%u)\n",
//            sockfd, ipbuf, dest_port, length);

    int r = sendto(sockfd, buffer, length, flags,
                   (__CONST_SOCKADDR_ARG)addr, addrlen);
    if (r < 0) {
        wsa_set_last_from_errno();
        return -1;
    }
    last_socket_error = 0;
    return r;
#endif
}

int WINAPI WSAGetLastError()
{
    static int chatty = -1;
    if (chatty < 0) {
        /* default OFF; set NOX_WSA_LOG=1 to enable */
        const char *e = getenv("NOX_WSA_LOG");
        chatty = (e && *e && strcmp(e, "0") != 0) ? 1 : 0;
    }

    if (chatty && last_socket_error != 0) {
        NETLOG("compat_net: WSAGetLastError() => %u\n", last_socket_error);
    }

    return last_socket_error;
}

// Time functions
// compatGetDateFormatA(Locale=2048, dwFlags=1, lpDate=0x1708c6a4, lpFormat=0x00000000, lpDateStr="nox.str:Warrior", cchDate=256) at compat.c:1001
int WINAPI GetDateFormatA(LCID Locale, DWORD dwFlags, const SYSTEMTIME *lpDate, LPCSTR lpFormat, LPSTR lpDateStr, int cchDate)
{
    if (Locale != 0x800 || dwFlags != 1 || lpFormat)
        DebugBreak();

    /* default locale, short date (MM/dd/yy) */
    int month = (int)lpDate->wMonth;
    int day   = (int)lpDate->wDay;
    int year  = (int)(lpDate->wYear % 100);

    /* If month is 0-based (tm_mon style), normalize to 1..12 */
    if (month >= 0 && month <= 11)
        month += 1;

    return snprintf(lpDateStr, cchDate, "%02d/%02d/%02d", month, day, year);
}

int WINAPI GetTimeFormatA(LCID Locale, DWORD dwFlags, const SYSTEMTIME *lpTime, LPCSTR lpFormat, LPSTR lpTimeStr, int cchTime)
{
    if (Locale != 0x800 || dwFlags != 14 || lpFormat)
        DebugBreak();

    // default locale, 24 hour, no time marker, no seconds
    return snprintf(lpTimeStr, cchTime, "%02d:%02d", lpTime->wHour, lpTime->wMinute);
}

BOOL WINAPI SystemTimeToFileTime(const SYSTEMTIME *lpSystemTime, LPFILETIME lpFileTime)
{
    QWORD t;
    struct tm tm;

    tm.tm_sec = lpSystemTime->wSecond;
    tm.tm_min = lpSystemTime->wMinute;
    tm.tm_hour = lpSystemTime->wHour;
    tm.tm_mday = lpSystemTime->wDay;
    tm.tm_mon = lpSystemTime->wMonth;
    tm.tm_year = lpSystemTime->wYear - 1900;
    tm.tm_isdst = -1;
    tm.tm_zone = NULL;
    tm.tm_gmtoff = 0;

    t = mktime(&tm);
    t = (t * 1000 + lpSystemTime->wMilliseconds) * 10000;
    lpFileTime->dwLowDateTime = t & 0xffffffff;
    lpFileTime->dwHighDateTime = t >> 32;
}

LONG WINAPI CompareFileTime(const FILETIME *lpFileTime1, const FILETIME *lpFileTime2)
{
    if (lpFileTime1->dwHighDateTime != lpFileTime2->dwHighDateTime)
        return (LONG)lpFileTime1->dwHighDateTime - (LONG)lpFileTime2->dwHighDateTime;
    if (lpFileTime1->dwLowDateTime != lpFileTime2->dwLowDateTime)
        return (LONG)lpFileTime1->dwLowDateTime - (LONG)lpFileTime2->dwLowDateTime;
}

VOID WINAPI GetLocalTime(LPSYSTEMTIME lpSystemTime)
{
    time_t t;
    struct tm *tm;

    time(&t);
    tm = localtime(&t);

    lpSystemTime->wYear = tm->tm_year + 1900;
    lpSystemTime->wMonth = tm->tm_mon;
    lpSystemTime->wDayOfWeek = tm->tm_wday;
    lpSystemTime->wDay = tm->tm_mday;
    lpSystemTime->wHour = tm->tm_hour;
    lpSystemTime->wMinute = tm->tm_min;
    lpSystemTime->wSecond = tm->tm_sec;
    lpSystemTime->wMilliseconds = 0;
}

BOOL WINAPI QueryPerformanceCounter(LARGE_INTEGER *lpPerformanceCount)
{
    struct timeval tv;

    gettimeofday(&tv, NULL);
    lpPerformanceCount->QuadPart = (QWORD)tv.tv_sec * 1000000 + tv.tv_usec;
    return TRUE;
}

BOOL WINAPI QueryPerformanceFrequency(LARGE_INTEGER *lpFrequency)
{
    lpFrequency->QuadPart = 1000000;
    return TRUE;
}

VOID WINAPI Sleep(DWORD dwMilliseconds)
{
#ifndef __EMSCRIPTEN__
    SDL_Delay(dwMilliseconds);
#endif
}

DWORD WINAPI timeGetTime()
{
    return SDL_GetTicks();
}

DWORD WINAPI GetTickCount()
{
    return timeGetTime();
}

// File functions
static char *dos_to_unix(const char *path)
{
    int i, len = strlen(path);
    char *str = malloc(len + 1);

    if (path[0] == 'C' && path[1] == ':')
        path += 2;

    for (i = 0; path[i]; i++)
    {
        if (path[i] == '\\')
            str[i] = '/';
        else
            str[i] = path[i];
    }
    str[i] = 0;

    return str;
}

// ---- FindFirstFileA/FindNextFileA/FindClose (Windows-ish, case-insensitive) ----
// NOX hacks are opt-in (default OFF).

#ifndef NOX_FIND_HACKS
#define NOX_FIND_HACKS 1
#endif

#ifndef ERROR_INVALID_PARAMETER
#define ERROR_INVALID_PARAMETER 87
#endif
#ifndef ERROR_PATH_NOT_FOUND
#define ERROR_PATH_NOT_FOUND 3
#endif
#ifndef ERROR_FILE_NOT_FOUND
#define ERROR_FILE_NOT_FOUND 2
#endif
#ifndef ERROR_NO_MORE_FILES
#define ERROR_NO_MORE_FILES 18
#endif

struct _FIND_FILE {
    size_t idx;
    size_t count;
    char **paths;   // full paths for stat/fill
    char **names;   // just the entry names (cFileName)
};

static int has_wildcards(const char *s) {
    return s && (strchr(s, '*') || strchr(s, '?'));
}

static int has_dot(const char *s) {
    return s && strchr(s, '.');
}

/* Simple Windows-ish wildcard match: '*' and '?', CASE-INSENSITIVE.
   (Not a perfect emulation of all Win32 wildcard edge-cases.) */
static int win_match_ci(const char *pat, const char *str) {
    const char *p = pat, *s = str;
    const char *star = NULL, *ss = NULL;

    while (*s) {
        char pc = (char)tolower((unsigned char)*p);
        char sc = (char)tolower((unsigned char)*s);

        if (pc == '*') {
            star = p++;
            ss = s;
            continue;
        }
        if (pc == '?' || pc == sc) {
            p++; s++;
            continue;
        }
        if (star) {
            p = star + 1;
            s = ++ss;
            continue;
        }
        return 0;
    }
    while (*p == '*') p++;
    return *p == '\0';
}

static int is_star_dot_star(const char *pat) {
    return pat && strcmp(pat, "*.*") == 0;
}

static void free_findfile(struct _FIND_FILE *ff) {
    if (!ff) return;
    for (size_t i = 0; i < ff->count; i++) {
        free(ff->paths[i]);
        free(ff->names[i]);
    }
    free(ff->paths);
    free(ff->names);
    free(ff);
}

/* Use compat_do_stat to avoid recursion. */
static void fill_find_data_fullpath(const char *fullpath, const char *name,
                                   LPWIN32_FIND_DATAA out) {
    struct stat st;
    memset(out, 0, sizeof(*out));

    if (compat_do_stat(fullpath, &st) == 0) {
        out->dwFileAttributes =
            S_ISDIR(st.st_mode) ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL;

        unsigned long long size = (unsigned long long)st.st_size;
        out->nFileSizeHigh = (DWORD)(size >> 32);
        out->nFileSizeLow  = (DWORD)(size & 0xffffffffu);
    } else {
        out->dwFileAttributes = FILE_ATTRIBUTE_NORMAL;
    }

    strncpy(out->cFileName, name ? name : "", sizeof(out->cFileName) - 1);
    out->cFileName[sizeof(out->cFileName) - 1] = 0;
}

static void split_dir_pat(const char *in, char *dir, size_t dirsz,
                          char *pat, size_t patsz) {
    const char *slash = strrchr(in, '/');
    if (!slash) {
        strncpy(dir, ".", dirsz - 1); dir[dirsz - 1] = 0;
        strncpy(pat, in, patsz - 1);  pat[patsz - 1] = 0;
        return;
    }

    size_t dlen = (size_t)(slash - in);
    if (dlen == 0) {
        strncpy(dir, "/", dirsz - 1); dir[dirsz - 1] = 0;
    } else {
        if (dlen >= dirsz) dlen = dirsz - 1;
        memcpy(dir, in, dlen);
        dir[dlen] = 0;
    }

    strncpy(pat, slash + 1, patsz - 1);
    pat[patsz - 1] = 0;
}

static int match_windowsish(const char *pat, const char *name) {
    if (!pat || !name) return 0;

#if NOX_FIND_HACKS
    /* Nox hack: treat "*.*" as "*" (often true in practice on Windows). */
    if (is_star_dot_star(pat)) pat = "*";
#endif

#if NOX_FIND_HACKS
    /* Nox hack: if caller supplies "Con01a" (no dot, no wildcards),
       also accept "Con01a.*". This is NOT Win32 FindFirstFile behavior. */
    if (!has_wildcards(pat) && !has_dot(pat)) {
        if (win_match_ci(pat, name)) return 1;

        char buf[PATH_MAX];
        snprintf(buf, sizeof(buf), "%s.*", pat);
        return win_match_ci(buf, name);
    }
#endif

    return win_match_ci(pat, name);
}

HANDLE WINAPI FindFirstFileA(LPCSTR lpFileName, LPWIN32_FIND_DATAA lpFindFileData)
{
    if (!lpFileName || !lpFindFileData) {
        last_error = ERROR_INVALID_PARAMETER;
        return (HANDLE)-1;
    }

    char *converted = dos_to_unix(lpFileName);
    if (!converted) {
        last_error = ERROR_INVALID_PARAMETER;
        return (HANDLE)-1;
    }

    char dirbuf[PATH_MAX], patbuf[PATH_MAX];
    split_dir_pat(converted, dirbuf, sizeof(dirbuf), patbuf, sizeof(patbuf));

    /* Case-fold directory itself (so Save\\SAVE0002\\Con02a works on ext4) */
    const char *opendir_path = dirbuf;
    char *dircase = NULL;

    if (dirbuf[0]) {
        dircase = alloca(strlen(dirbuf) + 2);
        if (casepath(dirbuf, dircase)) {
            opendir_path = dircase;
        }
    }

    DIR *d = opendir(opendir_path);
    if (!d) {
        free(converted);
        last_error = ERROR_PATH_NOT_FOUND;
        return (HANDLE)-1;
    }

    struct _FIND_FILE *ff = calloc(1, sizeof(*ff));
    if (!ff) {
        closedir(d);
        free(converted);
        last_error = ERROR_INVALID_PARAMETER;
        return (HANDLE)-1;
    }

    size_t cap = 64;
    ff->paths = calloc(cap, sizeof(char*));
    ff->names = calloc(cap, sizeof(char*));
    if (!ff->paths || !ff->names) {
        closedir(d);
        free(converted);
        free_findfile(ff);
        last_error = ERROR_INVALID_PARAMETER;
        return (HANDLE)-1;
    }

    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        const char *name = e->d_name;

        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) continue;
        if (!match_windowsish(patbuf, name)) continue;

        if (ff->count == cap) {
            cap *= 2;
            char **np = realloc(ff->paths, cap * sizeof(char*));
            char **nn = realloc(ff->names, cap * sizeof(char*));
            if (!np || !nn) {
                closedir(d);
                free(converted);
                ff->paths = np ? np : ff->paths;
                ff->names = nn ? nn : ff->names;
                free_findfile(ff);
                last_error = ERROR_INVALID_PARAMETER;
                return (HANDLE)-1;
            }
            ff->paths = np;
            ff->names = nn;
        }

        size_t need = strlen(opendir_path) + 1 + strlen(name) + 1;
        char *full = malloc(need);
        if (!full) continue;

        if (strcmp(opendir_path, "/") == 0)
            snprintf(full, need, "/%s", name);
        else
            snprintf(full, need, "%s/%s", opendir_path, name);

        ff->paths[ff->count] = full;
        ff->names[ff->count] = strdup(name);
        if (!ff->names[ff->count]) {
            free(full);
            ff->paths[ff->count] = NULL;
            continue;
        }

        ff->count++;
    }

    closedir(d);

    if (ff->count == 0) {
        free(converted);
        free_findfile(ff);
        last_error = ERROR_FILE_NOT_FOUND;
        return (HANDLE)-1;
    }

    ff->idx = 0;
    fill_find_data_fullpath(ff->paths[ff->idx], ff->names[ff->idx], lpFindFileData);
    ff->idx++;

    free(converted);
    last_error = 0;
    return (HANDLE)ff;
}

BOOL WINAPI FindNextFileA(HANDLE hFindFile, LPWIN32_FIND_DATAA lpFindFileData)
{
    struct _FIND_FILE *ff = (struct _FIND_FILE *)hFindFile;
    if (!ff || !lpFindFileData) {
        last_error = ERROR_INVALID_PARAMETER;
        return FALSE;
    }

    if (ff->idx >= ff->count) {
        last_error = ERROR_NO_MORE_FILES;
        return FALSE;
    }

    fill_find_data_fullpath(ff->paths[ff->idx], ff->names[ff->idx], lpFindFileData);
    ff->idx++;
    last_error = 0;
    return TRUE;
}

BOOL WINAPI FindClose(HANDLE hFindFile)
{
    free_findfile((struct _FIND_FILE *)hFindFile);
    last_error = 0;
    return TRUE;
}

HANDLE WINAPI CreateFileA(LPCSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile)
{
    char *converted = dos_to_unix(lpFileName);
    int fd, flags;

    switch (dwDesiredAccess)
    {
    case GENERIC_READ:
        flags = O_RDONLY;
        break;
    case GENERIC_WRITE:
        flags = O_WRONLY;
        break;
    case GENERIC_READ|GENERIC_WRITE:
        flags = O_RDWR;
        break;
    default:
        DebugBreak();
    }

    switch (dwCreationDisposition)
    {
    case CREATE_NEW:
        flags |= O_CREAT | O_EXCL;
        break;
    case CREATE_ALWAYS:
        flags |= O_CREAT | O_TRUNC;
        break;
    default:
    case OPEN_EXISTING:
        break;
    case OPEN_ALWAYS:
        flags |= O_CREAT;
        break;
    case TRUNCATE_EXISTING:
        flags |= O_TRUNC;
        break;
    }

    fd = open(converted, flags, 0666);
    dprintf("%s: CreateFileA(%s) = %d\n", __FUNCTION__, converted, fd);
    free(converted);
    return (HANDLE)fd;
}

BOOL WINAPI ReadFile(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead, LPDWORD lpNumberOfBytesRead, LPOVERLAPPED lpOverlapped)
{
    int fd = (int)hFile;
    int ret;

    if (lpOverlapped)
        DebugBreak();

    ret = read(fd, lpBuffer, nNumberOfBytesToRead);
    if (ret >= 0)
    {
        *lpNumberOfBytesRead = ret;
        last_error = 0;
        return TRUE;
    }
    else
    {
        *lpNumberOfBytesRead = 0;
        last_error = 2; // FIXME
        return FALSE;
    }
}

DWORD WINAPI SetFilePointer(HANDLE hFile, LONG lDistanceToMove, PLONG lpDistanceToMoveHigh, DWORD dwMoveMethod)
{
    int fd = (int)hFile;
    int whence;
    off_t offset;

    if (lpDistanceToMoveHigh && *lpDistanceToMoveHigh)
        DebugBreak();

    switch (dwMoveMethod)
    {
    case FILE_BEGIN:
        whence = SEEK_SET;
        break;
    case FILE_CURRENT:
        whence = SEEK_CUR;
        break;
    case FILE_END:
        whence = SEEK_END;
        break;
    default:
        DebugBreak();
    }

    if (lpDistanceToMoveHigh)
        *lpDistanceToMoveHigh = 0;

    offset = lseek(fd, lDistanceToMove, whence);
    if (offset != (off_t)-1)
        last_error = 0;
    else
        last_error = 2; // FIXME
    return offset;
}

BOOL WINAPI CopyFileA(LPCSTR lpExistingFileName, LPCSTR lpNewFileName, BOOL bFailIfExists)
{
    char buf[1024];
    int rfd = _open(lpExistingFileName, O_RDONLY);
    int wfd;

    if (rfd < 0)
        return FALSE;

    wfd = _open(lpNewFileName, bFailIfExists ? O_WRONLY | O_CREAT | O_EXCL : O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (wfd < 0)
    {
error:
        close(rfd);
        return FALSE;
    }

    while (1)
    {
        int ret = read(rfd, buf, sizeof(buf));
        if (ret <= 0)
            break;
        if (write(wfd, buf, ret) != ret)
            goto error;
    }

    close(rfd);
    close(wfd);
    return TRUE;
}

BOOL WINAPI DeleteFileA(LPCSTR lpFileName)
{
    return _unlink(lpFileName) == 0;
}

BOOL WINAPI MoveFileA(LPCSTR lpExistingFileName, LPCSTR lpNewFileName)
{
    char *converted = dos_to_unix(lpExistingFileName);
    free(converted);
    dprintf("%s\n", __FUNCTION__);
    DebugBreak();
}

BOOL WINAPI CreateDirectoryA(LPCSTR lpPathName, LPSECURITY_ATTRIBUTES lpSecurityAttributes)
{
    return _mkdir(lpPathName) == 0;
}

BOOL WINAPI RemoveDirectoryA(LPCSTR lpPathName)
{
    int result;
    char *converted = dos_to_unix(lpPathName);
    result = rmdir(converted);
    free(converted);
    return result == 0;
}

DWORD WINAPI GetCurrentDirectoryA(DWORD nBufferLength, LPSTR lpBuffer)
{
    dprintf("%s: GetCurrentDirectoryA(%s)\n", __FUNCTION__, lpBuffer);

    if (_getcwd(lpBuffer, nBufferLength))
        return strlen(lpBuffer);
    else
        return 0;
}

BOOL WINAPI SetCurrentDirectoryA(LPCSTR lpPathName)
{
    if (!lpPathName) {
        last_error = 87;//ERROR_INVALID_PARAMETER;
        return FALSE;
    }

    char *converted_ptr = dos_to_unix(lpPathName);
    char *converted = converted_ptr;

    // Build candidate with casepath if possible
    char *r = alloca(strlen(converted) + 2);
    const char *use = converted;

    if (casepath(converted, r))
        use = r;

    dprintf("%s: chdir(%s) [from %s]\n", __FUNCTION__, use, converted);

    int result = chdir(use);
    if (result != 0) {
        dprintf("%s: chdir FAILED errno=%d (%s)\n", __FUNCTION__, errno, strerror(errno));
        last_error =  3;//ERROR_PATH_NOT_FOUND;
    } else {
        last_error = 0;
    }

    free(converted_ptr);
    return result == 0;
}

int _chmod(const char *filename, int mode)
{
    int result;
    char *converted = dos_to_unix(filename);
    result = chmod(converted, mode);
    free(converted);
    return result;
}

int _access(const char *filename, int mode)
{
    if (!filename) {
        errno = EINVAL;
        return -1;
    }

    char *converted_ptr = dos_to_unix(filename);
    const char *converted = converted_ptr ? converted_ptr : filename;

    char *cwd = get_current_dir_name();

    // Strip accidental "cwd/" prefix (same idea as compat_fopen/_stat)
    if (cwd && converted_ptr && strncmp(converted, cwd, strlen(cwd)) == 0) {
        size_t clen = strlen(cwd);
        if (converted[clen] == '/')
            converted += clen + 1;
    }

    char *r = alloca(strlen(converted) + 2);
    const char *use = converted;

    if (casepath(converted, r))
        use = r;

    int result = access(use, mode);

    if (cwd) free(cwd);
    if (converted_ptr) free(converted_ptr);
    return result;
}


#include <fcntl.h>     // AT_FDCWD
#include <sys/stat.h>

static int compat_do_stat(const char *path, struct stat *st)
{
    // fstatat avoids calling libc stat/__xstat/__stat symbols, so no recursion.
    return fstatat(AT_FDCWD, path, st, 0);
}

static void get_exedir_unix(char *out, size_t outsz)
{
    ssize_t n = readlink("/proc/self/exe", out, outsz - 1);
    if (n > 0) {
        out[n] = 0;
        char *slash = strrchr(out, '/');
        if (slash) *slash = 0;
        return;
    }

    if (progname && progname[0]) {
        strncpy(out, progname, outsz - 1);
        out[outsz - 1] = 0;
        char *slash = strrchr(out, '/');
        if (slash) {
            *slash = 0;
            return;
        }
    }

    out[0] = 0;
}

int _stat(const char *path, struct _stat *buffer)
{
    if (!path || !buffer) {
        errno = EINVAL;
        return -1;
    }

    int result = -1;
    struct stat st;

    char *converted_ptr = dos_to_unix(path);
    char *converted = converted_ptr;

    char *cwd = get_current_dir_name();
    STATLOG("_stat: path='%s' converted='%s' cwd='%s'\n",
            path, converted_ptr ? converted_ptr : "(null)", cwd ? cwd : "(null)");

    // Strip accidental "cwd/" prefix like compat_fopen does
    if (cwd && converted && strncmp(converted, cwd, strlen(cwd)) == 0) {
        size_t clen = strlen(cwd);
        if (converted[clen] == '/')
            converted = converted + clen + 1;
        STATLOG("_stat: stripped cwd prefix -> '%s'\n", converted);
    }

    // Prepare casepath candidate
    char *r = NULL;
    const char *try1 = converted ? converted : path;

    if (converted) {
        r = alloca(strlen(converted) + 2);
        if (casepath(converted, r)) {
            try1 = r;
        }
    }

    // Helper macro to attempt stat with logging
    #define TRY_STAT(label, p) do { \
        errno = 0; \
        STATLOG("_stat: try %-12s '%s'\n", label, (p) ? (p) : "(null)"); \
        result = compat_do_stat((p), &st); \
        if (result == 0) { \
            STATLOG("_stat: OK  %-12s '%s'\n", label, (p)); \
        } else { \
            STATLOG("_stat: FAIL%-12s '%s' errno=%d (%s)\n", \
                    label, (p) ? (p) : "(null)", errno, strerror(errno)); \
        } \
    } while (0)

    // 1) normalized+casepath (best guess)
    TRY_STAT("casepath", try1);

    // 2) plain converted
    if (result != 0 && converted && converted != try1) {
        TRY_STAT("converted", converted);
    }

    // 3) raw original (maybe already unix)
    if (result != 0) {
        TRY_STAT("raw", path);
    }

    // 4) exe-dir fallback for relative paths (fix wrong cwd at launch)
    if (result != 0 && converted && converted[0] != '/') {
        char exedir[PATH_MAX];
        get_exedir_unix(exedir, sizeof(exedir));
        if (exedir[0]) {
            char joined[PATH_MAX];
            snprintf(joined, sizeof(joined), "%s/%s", exedir, converted);

            STATLOG("_stat: exedir='%s' joined='%s'\n", exedir, joined);

            char *r2 = alloca(strlen(joined) + 2);
            if (casepath(joined, r2)) {
                TRY_STAT("exe+case", r2);
            }
            if (result != 0) {
                TRY_STAT("exe+raw", joined);
            }
        } else {
            STATLOG("_stat: exedir unavailable\n");
        }
    }

    #undef TRY_STAT

    if (cwd) free(cwd);
    if (converted_ptr) free(converted_ptr);

    if (result != 0) {
        // preserve errno from last attempt
        return result;
    }

    // Fill Win-like _stat from libc stat
    buffer->st_dev = st.st_dev;
    buffer->st_ino = st.st_ino;
    buffer->st_mode = st.st_mode;
    buffer->st_nlink = st.st_nlink;
    buffer->st_uid = st.st_uid;
    buffer->st_gid = st.st_gid;
    buffer->st_rdev = st.st_rdev;
    buffer->st_size = st.st_size;
#ifdef __APPLE__
    buffer->st_mtime = st.st_mtimespec.tv_sec;
    buffer->st_atime = st.st_atimespec.tv_sec;
    buffer->st_ctime = st.st_ctimespec.tv_sec;
#else
    buffer->st_mtime = st.st_mtim.tv_sec;
    buffer->st_atime = st.st_atim.tv_sec;
    buffer->st_ctime = st.st_ctim.tv_sec;
#endif
    return 0;
}

int _mkdir(const char *path)
{
    int result;
    char *converted = dos_to_unix(path);
    result = mkdir(converted, 0777);
    free(converted);
    return result;
}

int _unlink(const char *filename)
{
    int result;
    char *converted = dos_to_unix(filename);
    result = unlink(converted);
    free(converted);
    return result;
}

char *_getcwd(char *buffer, int maxlen)
{
    int i;

    if (maxlen < 2)
        return NULL;

    strcpy(buffer, "C:");
    if (!getcwd(buffer + 2, maxlen - 2))
        return NULL;

    for (i = 0; buffer[i]; i++)
        if (buffer[i] == '/')
            buffer[i] = '\\';

    printf("%s: _getcwd(%s, %d)\n", __FUNCTION__, buffer, maxlen);
    return buffer;
}

// r must have strlen(path) + 2 bytes
static int casepath(char const *path, char *r)
{
    size_t l = strlen(path);
    char *p = alloca(l + 1);
    strcpy(p, path);
    size_t rl = 0;

    DIR *d;
    if (p[0] == '/')
    {
        d = opendir("/");
        p = p + 1;
    }
    else
    {
        d = opendir(".");
        r[0] = '.';
        r[1] = 0;
        rl = 1;
    }

    int last = 0;
    char *c = strsep(&p, "/");
    while (c)
    {
        /* Skip empty and "." path components to avoid ./././... growth */
        if (c[0] == 0 || (c[0] == '.' && c[1] == 0)) {
            c = strsep(&p, "/");
            continue;
        }

        /* Preserve ".." literally (don’t try to case-fold it) */
        if (c[0] == '.' && c[1] == '.' && c[2] == 0) {
            if (!d) {
                return 0;
            }
            if (last) {
                closedir(d);
                return 0;
            }

            r[rl] = '/';
            rl += 1;
            r[rl] = 0;

            strcpy(r + rl, "..");
            rl += 2;

            /* After a ".." we can’t reliably keep walking directories via opendir
               without normalizing the path; treat the rest as literal. */
            last = 1;

            c = strsep(&p, "/");
            continue;
        }

        if (!d)
        {
            return 0;
        }

        if (last)
        {
            closedir(d);
            return 0;
        }

        r[rl] = '/';
        rl += 1;
        r[rl] = 0;

        struct dirent *e = readdir(d);
        while (e)
        {
            if (strcasecmp(c, e->d_name) == 0)
            {
                strcpy(r + rl, e->d_name);
                rl += strlen(e->d_name);

                closedir(d);
                d = opendir(r);

                break;
            }

            e = readdir(d);
        }

        if (!e)
        {
            strcpy(r + rl, c);
            rl += strlen(c);
            last = 1;
        }

        c = strsep(&p, "/");
    }

    if (d) closedir(d);
    return 1;
}


void casechdir(char const *path)
{
    char *r = alloca(strlen(path) + 2);
    if (casepath(path, r))
    {
        chdir(r);
    }
    else
    {
        errno = ENOENT;
    }
}

FILE *compat_fopen(const char *path, const char *mode)
{
    const char *orig_mode = mode ? mode : "(null)";

    /* Treat NULL mode as "r" just so checks below are safe. */
    if (!mode) {
        mode = "r";
    }

    /* Workaround: some calls pass garbage like "g.bin" for .map files.
       On Windows/x86 this slipped through ABI quirks; on ARM we just want
       to read the map as a binary file.

       BUT: only do this for *read-style* opens. If the mode includes
       'w', 'a', or '+', we must NOT override it, because those calls
       are trying to create/write save .map files.
    */
    if (path && strstr(path, ".map")) {
        int wants_write =
            (strchr(mode, 'w') != NULL) ||
            (strchr(mode, 'a') != NULL) ||
            (strchr(mode, '+') != NULL);

        if (!wants_write && strcmp(mode, "rb") != 0) {
            fprintf(stderr,
                    "compat_fopen: fixing map mode %s -> rb for %s\n",
                    orig_mode, path);
            mode = "rb";
        }
    }

    FILE *result;
    char *converted = dos_to_unix(path);
    char *converted_ptr = converted;
    fprintf(stderr, "compat_fopen: fcaseopen(%s)\n", converted);

    char *r = alloca(strlen(converted) + 2);
    char *cdir = get_current_dir_name();

    /* Some paths are absolute for some reason */
    if (strstr(converted, cdir) == converted_ptr) {
        converted = converted + strlen(cdir) + 1;
    }

    free(cdir);
    int found = casepath(converted, r);
    if (found == 0)
        r = converted;

    /* Call the real C library fopen (macro undefined above) */
    result = fopen(r, mode);

    if (!result) {
        fprintf(stderr,
                "compat_fopen: primary fopen(%s, %s) FAILED, errno=%d (%s)\n",
                r, mode ? mode : "(null)", errno, strerror(errno));
        if (path) {
            fprintf(stderr,
                    "compat_fopen: fallback trying raw path %s\n",
                    path);
            result = fopen(path, mode);
            fprintf(stderr,
                    "compat_fopen: fallback fopen(%s, %s) = %p\n",
                    path, mode ? mode : "(null)", (void *)result);
        }
    } else {
        fprintf(stderr,
                "compat_fopen: fopen(%s, %s) = %p\n",
                r, orig_mode, (void *)result);
    }

    free(converted_ptr);
    return result;
}

char *fgets(char *str, int size, FILE *stream)
#undef fgets
{
    char *result;
    //printf("stream: %x, size: %d, str: %x\n", stream, size, str);

    result = fgets(str, size, stream);
    //printf("%s: fgets(%s) = %s\n", __FUNCTION__, str, result);
 
    if (result)
    {
        // XXX hack for text-mode line conversion
        size = strlen(result);
        if (size >= 2 && result[size - 1] == '\n' && result[size - 2] == '\r')
        {
            result[size - 2] = '\n';
            result[size - 1] = '\0';
        }
    }
    //printf("%s: fgets(%s) = %s\n", __FUNCTION__, str, result);

    return result;
}

// Registry functions
LSTATUS WINAPI RegCreateKeyExA(HKEY hKey, LPCSTR lpSubKey, DWORD Reserved, LPSTR lpClass, DWORD dwOptions, REGSAM samDesired, const LPSECURITY_ATTRIBUTES lpSecurityAttributes, PHKEY phkResult, LPDWORD lpdwDisposition)
{
    DebugBreak();
}

LSTATUS WINAPI RegOpenKeyExA(HKEY hKey, LPCSTR lpSubKey, DWORD ulOptions, REGSAM samDesired, PHKEY phkResult)
{
    HKEY hkResult;
    const char *root;

    if (hKey == HKEY_LOCAL_MACHINE)
        root = "HKEY_LOCAL_MACHINE";
    else
        root = hKey->path;

    hkResult = calloc(sizeof(*hkResult), 1);
    hkResult->path = malloc(strlen(root) + strlen(lpSubKey) + 2);
    sprintf(hkResult->path, "%s\\%s", root, lpSubKey);
    *phkResult = hkResult;
    return 0;
}

LSTATUS WINAPI RegQueryValueExA(HKEY hKey, LPCSTR lpValueName, LPDWORD lpReserved, LPDWORD lpType, LPBYTE lpData, LPDWORD lpcbData)
{
    dprintf("%s: key=\"%s\", value=\"%s\"", __FUNCTION__, hKey->path, lpValueName);

    if (strcmp(hKey->path, "HKEY_LOCAL_MACHINE\\SOFTWARE\\Westwood\\Nox") == 0 && strcmp(lpValueName, "Serial") == 0)
    {
        int i;
        for (i = 0; i < *lpcbData - 1; i++)
            lpData[i] = (rand() % 10) + '0';
        lpData[i] = 0;
        *lpType = 1; // REG_SZ
        return 0;
    }

    return 3;
}

LSTATUS WINAPI RegSetValueExA(HKEY hKey, LPCSTR lpValueName, DWORD Reserved, DWORD dwType, const BYTE *lpData, DWORD cbData)
{
    DebugBreak();
}

LSTATUS WINAPI RegCloseKey(HKEY hKey)
{
    free(hKey->path);
    free(hKey);
}

// Synchronization functions
VOID WINAPI InitializeCriticalSection(LPCRITICAL_SECTION lpCriticalSection)
{
    lpCriticalSection->opaque = SDL_CreateMutex();
}

VOID WINAPI DeleteCriticalSection(LPCRITICAL_SECTION lpCriticalSection)
{
    SDL_DestroyMutex(lpCriticalSection->opaque);
}

VOID WINAPI EnterCriticalSection(LPCRITICAL_SECTION lpCriticalSection)
{
    SDL_LockMutex(lpCriticalSection->opaque);
}

VOID WINAPI LeaveCriticalSection(LPCRITICAL_SECTION lpCriticalSection)
{
    SDL_UnlockMutex(lpCriticalSection->opaque);
}

HANDLE WINAPI CreateMutexA(LPSECURITY_ATTRIBUTES lpSecurityAttributes, BOOL bInitialOwner, LPCSTR lpName)
{
    pthread_mutex_t *m = malloc(sizeof(pthread_mutex_t));
    pthread_mutexattr_t attr;

    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(m, &attr);
    pthread_mutexattr_destroy(&attr);

    if (bInitialOwner)
        pthread_mutex_lock(m);

    return new_handle(HANDLE_MUTEX, m);
}

BOOL WINAPI ReleaseMutex(HANDLE hMutex)
{
    pthread_mutex_t *m = lookup_handle(HANDLE_MUTEX, hMutex);
    if (m == NULL)
        return FALSE;
    pthread_mutex_unlock(m);
    return TRUE;
}

BOOL WINAPI SetEvent(HANDLE hEvent)
{
    DebugBreak();
}

DWORD WINAPI WaitForSingleObject(HANDLE hHandle, DWORD dwMilliseconds)
{
    DWORD result = (DWORD)-1;
    struct timespec tv;

    clock_gettime(CLOCK_REALTIME, &tv);
    tv.tv_sec += dwMilliseconds / 1000;
    tv.tv_nsec += (dwMilliseconds % 1000) * 1000000;
    while (tv.tv_nsec >= 1000000000)
    {
        tv.tv_sec++;
        tv.tv_nsec -= 1000000000;
    }

    switch ((DWORD)hHandle >> 16)
    {
    case HANDLE_THREAD:
        {
            pthread_t thr = (pthread_t)(uintptr_t)lookup_handle(HANDLE_THREAD, hHandle);
            result = 0;
            if (dwMilliseconds == INFINITE)
                pthread_join(thr, NULL);
#if defined(__APPLE__) || defined(__EMSCRIPTEN__)
            else
                pthread_join(thr, NULL);
#else
            else if (pthread_timedjoin_np(thr, NULL, &tv) == ETIMEDOUT)
                result = 0x102;
#endif
        }
        break;
    case HANDLE_MUTEX:
        {
            pthread_mutex_t *m = lookup_handle(HANDLE_MUTEX, hHandle);
            result = 0;
            if (dwMilliseconds == INFINITE)
                pthread_mutex_lock(m);
#if defined(__APPLE__) || defined(__EMSCRIPTEN__)
            else
                pthread_mutex_lock(m);
#else
            else if (pthread_mutex_timedlock(m, &tv) == ETIMEDOUT)
                result = 0x102;
#endif
        }
        break;
    default:
        DebugBreak();
        break;
    }

    return result;
}


// Public helper: normalize Win paths + apply casepath.
// Returns 1 on success, 0 on failure (ENOENT-like).
int external_compat_casepath(const char *path, char *out, size_t outsz)
{
    if (!path || !out || outsz < 2) return 0;

    char *converted_ptr = dos_to_unix(path);
    const char *converted = converted_ptr ? converted_ptr : path;

    // casepath needs strlen(path)+2 bytes; build into a temp then copy to out.
    char *tmp = alloca(strlen(converted) + 2);

    const char *use = converted;
    if (casepath(converted, tmp)) {
        use = tmp;
    }

    // copy out safely
    strncpy(out, use, outsz - 1);
    out[outsz - 1] = 0;

    if (converted_ptr) free(converted_ptr);
    return 1;
}