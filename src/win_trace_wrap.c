// Unused enable in CMakeLists.txt if needed
#if defined(_WIN32)

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

/* ---------------- env toggles ---------------- */

static int truthy(const char *s){
    if(!s || !*s) return 0;
    if (s[0]=='0' && s[1]==0) return 0;
    return 1;
}
static int trace_fopen(void){
    static int v=-1; if(v<0) v = truthy(getenv("NOX_FOPEN_TRACE")); return v;
}
static int trace_reg(void){
    static int v=-1; if(v<0) v = truthy(getenv("NOX_WIN_REG_TRACE")); return v;
}

#define FOPENLOG(...) do{ if(trace_fopen()){ fprintf(stderr,__VA_ARGS__); fflush(stderr);} }while(0)
#define REGLOG(...)   do{ if(trace_reg()){   fprintf(stderr,__VA_ARGS__); fflush(stderr);} }while(0)

__attribute__((constructor))
static void wrap_init(void) {
  fprintf(stderr, "WRAP: init (NOX_FOPEN_TRACE=%s NOX_WIN_REG_TRACE=%s NOX_FOPEN_TRACE_MATCH=%s)\n",
          getenv("NOX_FOPEN_TRACE"), getenv("NOX_WIN_REG_TRACE"), getenv("NOX_FOPEN_TRACE_MATCH"));
  fflush(stderr);
}

/* ---------------- fopen stack trace ----------------
   We try to use dbghelp.dll at runtime (no link dependency).
   If that fails, we print raw return addresses. */

typedef USHORT (WINAPI *PFN_CaptureStackBackTrace)(ULONG, ULONG, PVOID*, PULONG);

typedef BOOL   (WINAPI *PFN_SymInitialize)(HANDLE, PCSTR, BOOL);
typedef DWORD  (WINAPI *PFN_SymSetOptions)(DWORD);
typedef BOOL   (WINAPI *PFN_SymFromAddr)(HANDLE, DWORD64, DWORD64*, void*);
typedef BOOL   (WINAPI *PFN_SymCleanup)(HANDLE);

#ifndef SYMOPT_DEFERRED_LOADS
#define SYMOPT_DEFERRED_LOADS 0x00000004
#endif
#ifndef SYMOPT_UNDNAME
#define SYMOPT_UNDNAME        0x00000002
#endif
#ifndef SYMOPT_LOAD_LINES
#define SYMOPT_LOAD_LINES     0x00000010
#endif

/* Minimal SYMBOL_INFO layout */
typedef struct _SYMBOL_INFO_MIN {
    ULONG   SizeOfStruct;
    ULONG   TypeIndex;
    ULONG64 Reserved[2];
    ULONG   Index;
    ULONG   Size;
    ULONG64 ModBase;
    ULONG   Flags;
    ULONG64 Value;
    ULONG64 Address;
    ULONG   Register;
    ULONG   Scope;
    ULONG   Tag;
    ULONG   NameLen;
    ULONG   MaxNameLen;
    CHAR    Name[1];
} SYMBOL_INFO_MIN;

/* ---------------- optional stacktrace match filter ---------------- */

static const char *fopen_trace_match(void)
{
    /* New: if set, only stacktrace when path matches this substring. */
    const char *s = getenv("NOX_FOPEN_TRACE_MATCH");
    if (!s || !*s) return NULL;
    return s;
}

static int contains_nocase(const char *hay, const char *needle)
{
    if (!hay || !needle) return 0;
    if (!*needle) return 1;

    size_t nlen = strlen(needle);
    for (const char *p = hay; *p; ++p) {
        if (!_strnicmp(p, needle, nlen))
            return 1;
    }
    return 0;
}

static void dump_stacktrace_if_enabled(const char *path)
{
    if (!trace_fopen())
        return;

    /* Old behaviour by default: stacktrace on every traced fopen. */
    const char *match = fopen_trace_match();
    if (match) {
        /* New behaviour when NOX_FOPEN_TRACE_MATCH is set: gate stacktrace on match. */
        if (!contains_nocase(path ? path : "", match))
            return;
    }

    HMODULE k32 = GetModuleHandleA("kernel32.dll");
    if (!k32) return;

    PFN_CaptureStackBackTrace pCapture =
        (PFN_CaptureStackBackTrace)GetProcAddress(k32, "RtlCaptureStackBackTrace");
    if (!pCapture) {
        /* Older name in some environments */
        pCapture = (PFN_CaptureStackBackTrace)GetProcAddress(k32, "CaptureStackBackTrace");
    }
    if (!pCapture) {
        FOPENLOG("FOPEN:  (no stack capture available)\n");
        return;
    }

    void *frames[32];
    USHORT n = pCapture(0, (ULONG)(sizeof(frames)/sizeof(frames[0])), frames, NULL);
    if (n == 0) {
        FOPENLOG("FOPEN:  (empty stack)\n");
        return;
    }

    /* Try symbolization via dbghelp.dll if present */
    HMODULE dbg = LoadLibraryA("dbghelp.dll");
    if (dbg) {
        PFN_SymInitialize pSymInitialize = (PFN_SymInitialize)GetProcAddress(dbg, "SymInitialize");
        PFN_SymSetOptions pSymSetOptions = (PFN_SymSetOptions)GetProcAddress(dbg, "SymSetOptions");
        PFN_SymFromAddr   pSymFromAddr   = (PFN_SymFromAddr)GetProcAddress(dbg, "SymFromAddr");
        PFN_SymCleanup    pSymCleanup    = (PFN_SymCleanup)GetProcAddress(dbg, "SymCleanup");

        HANDLE proc = GetCurrentProcess();

        if (pSymInitialize && pSymFromAddr && pSymCleanup) {
            if (pSymSetOptions) {
                pSymSetOptions(SYMOPT_DEFERRED_LOADS | SYMOPT_UNDNAME | SYMOPT_LOAD_LINES);
            }
            if (pSymInitialize(proc, NULL, TRUE)) {
                char buf[sizeof(SYMBOL_INFO_MIN) + 256];
                SYMBOL_INFO_MIN *sym = (SYMBOL_INFO_MIN*)buf;
                sym->SizeOfStruct = sizeof(SYMBOL_INFO_MIN);
                sym->MaxNameLen = 255;

                FOPENLOG("FOPEN:  stacktrace (%u frames):\n", (unsigned)n);

                for (USHORT i = 0; i < n; i++) {
                    DWORD64 addr = (DWORD64)(uintptr_t)frames[i];
                    DWORD64 disp = 0;

                    if (pSymFromAddr(proc, addr, &disp, sym)) {
                        FOPENLOG("  #%02u %p  %s + 0x%llx\n",
                                 (unsigned)i,
                                 frames[i],
                                 sym->Name,
                                 (unsigned long long)disp);
                    } else {
                        FOPENLOG("  #%02u %p\n", (unsigned)i, frames[i]);
                    }
                }

                pSymCleanup(proc);
                FreeLibrary(dbg);
                return;
            }
        }
        FreeLibrary(dbg);
    }

    /* Fallback: raw addresses only */
    FOPENLOG("FOPEN:  stacktrace (%u frames, raw):\n", (unsigned)n);
    for (USHORT i = 0; i < n; i++) {
        FOPENLOG("  #%02u %p\n", (unsigned)i, frames[i]);
    }
}

/* ---------------- fopen wrap (already working) ---------------- */

extern FILE * __real_fopen(const char *path, const char *mode);

FILE * __wrap_fopen(const char *path, const char *mode)
{
    /* Always print calls when NOX_FOPEN_TRACE is enabled (old behaviour). */
    FOPENLOG("FOPEN: fopen('%s','%s')\n",
             path ? path : "(null)", mode ? mode : "(null)");

    /* Stacktrace is old behaviour unless NOX_FOPEN_TRACE_MATCH is set. */
    dump_stacktrace_if_enabled(path);

    FILE *f = __real_fopen(path, mode);
    FOPENLOG("FOPEN:  -> %p\n", (void*)f);
    return f;
}


// Not working
///* ---------------- registry wraps via __imp__ (IAT) ----------------
//   We wrap the *import pointer* symbols (__imp__*) because your GAME*.obj
//   references __imp__RegOpenKeyExA@20 etc.
//
//   On i686 COFF, symbols get a leading underscore, so ld produces:
//     ___wrap___imp__RegOpenKeyExA@20
//     ___real___imp__RegOpenKeyExA@20
//   (same for Query/Close)
//*/
//
//typedef LSTATUS (WINAPI *PFN_RegOpenKeyExA)(HKEY, LPCSTR, DWORD, REGSAM, PHKEY);
//typedef LSTATUS (WINAPI *PFN_RegQueryValueExA)(HKEY, LPCSTR, LPDWORD, LPDWORD, LPBYTE, LPDWORD);
//typedef LSTATUS (WINAPI *PFN_RegCloseKey)(HKEY);
//
///* These are DATA symbols (function pointers), not functions. */
//extern PFN_RegOpenKeyExA    pReal_RegOpenKeyExA
//    __asm__("___real___imp__RegOpenKeyExA@20");
//extern PFN_RegQueryValueExA pReal_RegQueryValueExA
//    __asm__("___real___imp__RegQueryValueExA@24");
//extern PFN_RegCloseKey      pReal_RegCloseKey
//    __asm__("___real___imp__RegCloseKey@4");
//
//static LSTATUS WINAPI wrap_RegOpenKeyExA(HKEY root, LPCSTR sub, DWORD opt, REGSAM sam, PHKEY out)
//{
//    REGLOG("WINREG: RegOpenKeyExA(root=%p,'%s',sam=0x%lx)\n",
//           (void*)root, sub ? sub : "(null)", (unsigned long)sam);
//    LSTATUS r = pReal_RegOpenKeyExA(root, sub, opt, sam, out);
//    REGLOG("WINREG:  -> %ld out=%p\n", (long)r, (out && r==0) ? (void*)(*out) : NULL);
//    return r;
//}
//
//static LSTATUS WINAPI wrap_RegQueryValueExA(HKEY h, LPCSTR name, LPDWORD res, LPDWORD type,
//                                           LPBYTE data, LPDWORD cb)
//{
//    REGLOG("WINREG: RegQueryValueExA(h=%p,'%s',cb_in=%lu)\n",
//           (void*)h, name ? name : "(null)", cb ? (unsigned long)(*cb) : 0UL);
//    LSTATUS r = pReal_RegQueryValueExA(h, name, res, type, data, cb);
//    REGLOG("WINREG:  -> %ld type=%lu cb_out=%lu\n", (long)r,
//           type ? (unsigned long)(*type) : 0UL,
//           cb ? (unsigned long)(*cb) : 0UL);
//    return r;
//}
//
//static LSTATUS WINAPI wrap_RegCloseKey(HKEY h)
//{
//    REGLOG("WINREG: RegCloseKey(h=%p)\n", (void*)h);
//    LSTATUS r = pReal_RegCloseKey(h);
//    REGLOG("WINREG:  -> %ld\n", (long)r);
//    return r;
//}
//
///* Export the wrapped import-pointer variables */
//__attribute__((used))
//PFN_RegOpenKeyExA pWrap_RegOpenKeyExA
//    __asm__("___wrap___imp__RegOpenKeyExA@20") = wrap_RegOpenKeyExA;
//
//__attribute__((used))
//PFN_RegQueryValueExA pWrap_RegQueryValueExA
//    __asm__("___wrap___imp__RegQueryValueExA@24") = wrap_RegQueryValueExA;
//
//__attribute__((used))
//PFN_RegCloseKey pWrap_RegCloseKey
//    __asm__("___wrap___imp__RegCloseKey@4") = wrap_RegCloseKey;

#endif /* _WIN32 */
