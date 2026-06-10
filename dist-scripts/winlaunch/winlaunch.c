// nox_launcher.c
// Single-file Win32 launcher for nox-decomp.
// - Win32 UI controls driven by launch-nox-decomp.ini
// - Runs innoextract + ffmpeg if needed (logs to UI + log.txt)
// - Deletes gamefiles/app/nox.cfg ONLY after successful extraction (per spec)
// - Computes resolution via WinAPI and patches nox.cfg (VideoMode + Fullscreen)
// - Launches noxd.*.exe with stdout/stderr redirected to log.txt, waits, then restores display mode
//
// Build (MSVC):
//   cl /O2 /W4 /DUNICODE /D_UNICODE nox_launcher.c comdlg32.lib user32.lib gdi32.lib shell32.lib ole32.lib advapi32.lib
//
// Build (MinGW-w64):
//   x86_64-w64-mingw32-gcc -O2 -municode nox_launcher.c -lcomdlg32 -lshell32 -lole32 -ladvapi32 -lgdi32 -luser32

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commdlg.h>
#include <shellapi.h>
#include <shlobj.h>
#include <stdint.h>
#include <stdio.h>
#include <wchar.h>
#include <stdlib.h>
#include <stdbool.h>

#ifndef EM_EXLIMITTEXT
#define EM_EXLIMITTEXT (WM_USER + 53)
#endif

#ifndef ARRAYSIZE
#define ARRAYSIZE(x) (sizeof(x)/sizeof((x)[0]))
#endif

// -------------------------
// Utility: wide string
// -------------------------
static wchar_t* wcsdup_heap(const wchar_t* s) {
    if (!s) return NULL;
    size_t n = wcslen(s);
    wchar_t* out = (wchar_t*)malloc((n + 1) * sizeof(wchar_t));
    if (!out) return NULL;
    memcpy(out, s, (n + 1) * sizeof(wchar_t));
    return out;
}

static void wcs_trim_inplace(wchar_t* s) {
    if (!s) return;
    size_t len = wcslen(s);
    size_t start = 0;
    while (start < len && (s[start] == L' ' || s[start] == L'\t' || s[start] == L'\r' || s[start] == L'\n'))
        start++;
    size_t end = len;
    while (end > start && (s[end-1] == L' ' || s[end-1] == L'\t' || s[end-1] == L'\r' || s[end-1] == L'\n'))
        end--;
    if (start > 0) memmove(s, s + start, (end - start) * sizeof(wchar_t));
    s[end - start] = 0;
}

static bool wcs_ieq(const wchar_t* a, const wchar_t* b) {
    if (!a || !b) return false;
    while (*a && *b) {
        wchar_t ca = *a, cb = *b;
        if (ca >= L'A' && ca <= L'Z') ca = (wchar_t)(ca - L'A' + L'a');
        if (cb >= L'A' && cb <= L'Z') cb = (wchar_t)(cb - L'A' + L'a');
        if (ca != cb) return false;
        a++; b++;
    }
    return *a == 0 && *b == 0;
}

static bool wcs_istarts(const wchar_t* s, const wchar_t* prefix) {
    if (!s || !prefix) return false;
    while (*prefix) {
        wchar_t cs = *s, cp = *prefix;
        if (cs >= L'A' && cs <= L'Z') cs = (wchar_t)(cs - L'A' + L'a');
        if (cp >= L'A' && cp <= L'Z') cp = (wchar_t)(cp - L'A' + L'a');
        if (cs != cp) return false;
        s++; prefix++;
    }
    return true;
}

static bool path_is_abs(const wchar_t* p) {
    if (!p || !*p) return false;
    // "C:\", "\\server\share"
    if (wcslen(p) >= 3 && p[1] == L':' && (p[2] == L'\\' || p[2] == L'/')) return true;
    if (wcslen(p) >= 2 && p[0] == L'\\' && p[1] == L'\\') return true;
    return false;
}

static void path_join(wchar_t* out, size_t outcap, const wchar_t* a, const wchar_t* b) {
    // out = a + "\" + b (if needed)
    if (!out || outcap == 0) return;
    out[0] = 0;
    if (!a) a = L"";
    if (!b) b = L"";
    wcsncpy(out, a, outcap - 1);
    out[outcap - 1] = 0;
    size_t len = wcslen(out);
    if (len > 0 && out[len - 1] != L'\\' && out[len - 1] != L'/') {
        if (len + 1 < outcap) {
            out[len] = L'\\';
            out[len + 1] = 0;
        }
    }
    len = wcslen(out);
    if (len + wcslen(b) < outcap) {
        wcscat(out, b);
    } else {
        // truncate
        size_t remain = outcap - 1 - len;
        wcsncat(out, b, remain);
        out[outcap - 1] = 0;
    }
}

static bool file_exists(const wchar_t* p) {
    DWORD attr = GetFileAttributesW(p);
    return (attr != INVALID_FILE_ATTRIBUTES) && ((attr & FILE_ATTRIBUTE_DIRECTORY) == 0);
}

static bool dir_exists(const wchar_t* p) {
    DWORD attr = GetFileAttributesW(p);
    return (attr != INVALID_FILE_ATTRIBUTES) && ((attr & FILE_ATTRIBUTE_DIRECTORY) != 0);
}

static void ensure_dir(const wchar_t* p) {
    // CreateDirectory only creates one level; we create parents iteratively.
    if (!p || !*p) return;
    wchar_t buf[MAX_PATH * 4];
    wcsncpy(buf, p, ARRAYSIZE(buf) - 1);
    buf[ARRAYSIZE(buf) - 1] = 0;

    // skip drive or UNC prefix
    size_t i = 0;
    if (wcslen(buf) >= 3 && buf[1] == L':' && (buf[2] == L'\\' || buf[2] == L'/')) i = 3;
    else if (wcslen(buf) >= 2 && buf[0] == L'\\' && buf[1] == L'\\') {
        // UNC: \\server\share\...
        i = 2;
        int slashes = 0;
        while (buf[i] && slashes < 2) {
            if (buf[i] == L'\\' || buf[i] == L'/') slashes++;
            i++;
        }
    }

    for (; buf[i]; i++) {
        if (buf[i] == L'/' ) buf[i] = L'\\';
        if (buf[i] == L'\\') {
            wchar_t c = buf[i];
            buf[i] = 0;
            if (wcslen(buf) > 0 && !dir_exists(buf)) {
                CreateDirectoryW(buf, NULL);
            }
            buf[i] = c;
        }
    }
    if (!dir_exists(buf)) CreateDirectoryW(buf, NULL);
}

static void ensure_parent_dir(const wchar_t* filePath) {
    if (!filePath || !*filePath) return;
    wchar_t tmp[MAX_PATH * 4];
    wcsncpy(tmp, filePath, ARRAYSIZE(tmp) - 1);
    tmp[ARRAYSIZE(tmp) - 1] = 0;

    wchar_t* last = wcsrchr(tmp, L'\\');
    wchar_t* last2 = wcsrchr(tmp, L'/');
    wchar_t* cut = last;
    if (last2 && (!cut || last2 > cut)) cut = last2;

    if (!cut) return;          // no parent component
    *cut = 0;                  // keep parent only
    if (*tmp) ensure_dir(tmp); // create parent chain
}

static bool get_drive_root_from_path(const wchar_t* path, wchar_t* out, size_t cap) {
    if (!path || !*path || !out || cap < 4) return false;

    // Drive path: C:\...
    if (wcslen(path) >= 3 && path[1] == L':' && (path[2] == L'\\' || path[2] == L'/')) {
        out[0] = path[0];
        out[1] = L':';
        out[2] = L'\\';
        out[3] = 0;
        return true;
    }

    // UNC path: \\server\share\...
    if (wcslen(path) >= 2 && path[0] == L'\\' && path[1] == L'\\') {
        const wchar_t* p = path + 2;
        const wchar_t* s1 = wcschr(p, L'\\');
        if (!s1) return false;
        const wchar_t* s2 = wcschr(s1 + 1, L'\\');
        if (!s2) s2 = path + wcslen(path);

        size_t len = (size_t)(s2 - path);
        if (len + 2 > cap) return false;

        wcsncpy(out, path, len);
        out[len] = L'\\';
        out[len + 1] = 0;
        return true;
    }

    return false;
}

static bool has_required_free_space(const wchar_t* pathOnTargetVolume,
                                    ULONGLONG requiredBytes,
                                    ULONGLONG* outFreeBytes) {
    wchar_t root[MAX_PATH * 4];
    ULARGE_INTEGER freeAvail, totalBytes, totalFree;

    if (outFreeBytes) *outFreeBytes = 0;

    if (!get_drive_root_from_path(pathOnTargetVolume, root, ARRAYSIZE(root))) {
        return false;
    }

    if (!GetDiskFreeSpaceExW(root, &freeAvail, &totalBytes, &totalFree)) {
        return false;
    }

    if (outFreeBytes) *outFreeBytes = freeAvail.QuadPart;
    return freeAvail.QuadPart >= requiredBytes;
}


// -------------------------
// Logging: UI ring buffer + file
// -------------------------
#define WM_APP_LOG   (WM_APP + 1)
#define WM_APP_DONE  (WM_APP + 2)

typedef struct LogChunk {
    wchar_t* text; // heap
} LogChunk;

typedef struct LogState {
    HWND hEdit;
    size_t max_bytes;   // ring buffer cap (approx bytes of UTF-16 *2, but treat as chars*2)
    bool auto_follow;   // follow when at end
} LogState;

static HANDLE g_logFile = INVALID_HANDLE_VALUE; // launcher log file handle (append), used until game launch
static LogState g_logState = {0};

#define UI_FLUSH_MAX_CHARS   8192   // max chars appended per WM_APP_LOG_FLUSH
#define UI_KEEP_MIN_CHARS    4096   // never try to trim below this

static void log_file_open_append(const wchar_t* path) {
    if (g_logFile != INVALID_HANDLE_VALUE) CloseHandle(g_logFile);
    g_logFile = CreateFileW(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
}

static void log_file_write_w(const wchar_t* s) {
    if (g_logFile == INVALID_HANDLE_VALUE || !s) return;
    // write UTF-8? keep simple UTF-16LE? We'll write UTF-8 for readability in editors.
    // Convert to UTF-8 and append.
    int needed = WideCharToMultiByte(CP_UTF8, 0, s, -1, NULL, 0, NULL, NULL);
    if (needed <= 1) return;
    char* buf = (char*)malloc((size_t)needed);
    if (!buf) return;
    WideCharToMultiByte(CP_UTF8, 0, s, -1, buf, needed, NULL, NULL);
    DWORD written = 0;
    // exclude terminating NUL
    WriteFile(g_logFile, buf, (DWORD)(needed - 1), &written, NULL);
    free(buf);
}

static void log_ui_append(HWND hEdit, const wchar_t* s, size_t max_bytes) {
    if (!hEdit || !s) return;

    // Determine if user is at end -> follow.
    DWORD selStart = 0, selEnd = 0;
    SendMessageW(hEdit, EM_GETSEL, (WPARAM)&selStart, (LPARAM)&selEnd);
    int textLen = GetWindowTextLengthW(hEdit);
    bool atEnd = (int)selEnd >= textLen;

    // Append at end
    SendMessageW(hEdit, EM_SETSEL, (WPARAM)textLen, (LPARAM)textLen);
    SendMessageW(hEdit, EM_REPLACESEL, (WPARAM)FALSE, (LPARAM)s);

    // Ring buffer trimming (approx bytes; UTF-16 wchar_t is 2 bytes)
    int newLen = GetWindowTextLengthW(hEdit);
    size_t approxBytes = (size_t)newLen * sizeof(wchar_t);
    if (approxBytes > max_bytes && max_bytes > 0) {
        // target chars to keep
        size_t targetChars = max_bytes / sizeof(wchar_t);
        if (targetChars < UI_KEEP_MIN_CHARS) targetChars = UI_KEEP_MIN_CHARS;

        int keepFrom = newLen - (int)targetChars;
        if (keepFrom > 0) {
            // Convert char index -> line number, then delete whole lines up to that point.
            int line = (int)SendMessageW(hEdit, EM_LINEFROMCHAR, (WPARAM)keepFrom, 0);
            if (line > 0) {
                int cut = (int)SendMessageW(hEdit, EM_LINEINDEX, (WPARAM)line, 0);
                if (cut > 0) {
                    SendMessageW(hEdit, EM_SETSEL, 0, cut);
                    SendMessageW(hEdit, EM_REPLACESEL, (WPARAM)FALSE, (LPARAM)L"");
                }
            } else {
                // fallback: just delete some chars (no line alignment)
                SendMessageW(hEdit, EM_SETSEL, 0, keepFrom);
                SendMessageW(hEdit, EM_REPLACESEL, (WPARAM)FALSE, (LPARAM)L"");
            }
        }
    }

    if (atEnd) {
        int endLen = GetWindowTextLengthW(hEdit);
        SendMessageW(hEdit, EM_SETSEL, (WPARAM)endLen, (LPARAM)endLen);
        SendMessageW(hEdit, EM_SCROLLCARET, 0, 0);
    }
}

static void log_post(HWND hwnd, const wchar_t* text) {
    if (!hwnd || !text) return;
    LogChunk* c = (LogChunk*)malloc(sizeof(LogChunk));
    if (!c) return;
    c->text = wcsdup_heap(text);
    if (!c->text) { free(c); return; }
    PostMessageW(hwnd, WM_APP_LOG, 0, (LPARAM)c);
}

// -------------------------
// INI parsing with preservation
// -------------------------
typedef struct IniLine {
    wchar_t* raw;       // includes newline? we store without trailing \r\n
    wchar_t* section;   // current section at this line (lowercased), or NULL
    bool is_section;
    bool is_kv;
    wchar_t* key;       // for kv (trimmed)
    wchar_t* value;     // for kv (trimmed, unquoted as-is)
} IniLine;

typedef struct IniDoc {
    IniLine* lines;
    size_t count;
    size_t cap;
} IniDoc;

static void ini_doc_free(IniDoc* d) {
    if (!d) return;
    for (size_t i = 0; i < d->count; i++) {
        free(d->lines[i].raw);
        free(d->lines[i].section);
        free(d->lines[i].key);
        free(d->lines[i].value);
    }
    free(d->lines);
    d->lines = NULL; d->count = d->cap = 0;
}

static wchar_t* wcs_to_lower_heap(const wchar_t* s) {
    if (!s) return NULL;
    size_t n = wcslen(s);
    wchar_t* out = (wchar_t*)malloc((n + 1) * sizeof(wchar_t));
    if (!out) return NULL;
    for (size_t i = 0; i < n; i++) {
        wchar_t c = s[i];
        if (c >= L'A' && c <= L'Z') c = (wchar_t)(c - L'A' + L'a');
        out[i] = c;
    }
    out[n] = 0;
    return out;
}

static bool ini_doc_push(IniDoc* d, IniLine* l) {
    if (d->count == d->cap) {
        size_t ncap = d->cap ? d->cap * 2 : 256;
        IniLine* n = (IniLine*)realloc(d->lines, ncap * sizeof(IniLine));
        if (!n) return false;
        d->lines = n;
        d->cap = ncap;
    }
    d->lines[d->count++] = *l;
    return true;
}

static bool is_comment_line(const wchar_t* s) {
    if (!s) return false;
    while (*s == L' ' || *s == L'\t') s++;
    return (*s == L';' || *s == L'#');
}

static void strip_trailing_crlf(wchar_t* s) {
    if (!s) return;
    size_t n = wcslen(s);
    while (n > 0 && (s[n-1] == L'\r' || s[n-1] == L'\n')) {
        s[n-1] = 0;
        n--;
    }
}

static bool parse_kv(const wchar_t* line, wchar_t** outKey, wchar_t** outVal) {
    *outKey = NULL; *outVal = NULL;
    if (!line) return false;
    const wchar_t* p = line;
    while (*p == L' ' || *p == L'\t') p++;
    if (*p == 0) return false;
    if (*p == L'[') return false;
    if (*p == L';' || *p == L'#') return false;

    const wchar_t* eq = wcschr(p, L'=');
    const wchar_t* col = wcschr(p, L':');
    const wchar_t* sep = NULL;
    if (eq && col) sep = (eq < col) ? eq : col;
    else sep = eq ? eq : col;
    if (!sep) return false;

    size_t klen = (size_t)(sep - p);
    wchar_t* k = (wchar_t*)malloc((klen + 1) * sizeof(wchar_t));
    if (!k) return false;
    wcsncpy(k, p, klen);
    k[klen] = 0;
    wcs_trim_inplace(k);
    if (!*k) { free(k); return false; }

    const wchar_t* v = sep + 1;
    while (*v == L' ' || *v == L'\t') v++;
    wchar_t* val = wcsdup_heap(v);
    if (!val) { free(k); return false; }
    wcs_trim_inplace(val);

    // Unquote if fully quoted
    size_t vlen = wcslen(val);
    if (vlen >= 2 && ((val[0] == L'"' && val[vlen-1] == L'"') || (val[0] == L'\'' && val[vlen-1] == L'\''))) {
        val[vlen-1] = 0;
        memmove(val, val+1, (vlen-1) * sizeof(wchar_t));
        // now trimmed already; keep inner spaces as-is
    }

    *outKey = k;
    *outVal = val;
    return true;
}

static bool parse_section(const wchar_t* line, wchar_t** outSectionLower) {
    *outSectionLower = NULL;
    if (!line) return false;
    const wchar_t* p = line;
    while (*p == L' ' || *p == L'\t') p++;
    if (*p != L'[') return false;
    const wchar_t* end = wcschr(p, L']');
    if (!end) return false;
    size_t len = (size_t)(end - (p + 1));
    if (len == 0) return false;
    wchar_t* sec = (wchar_t*)malloc((len + 1) * sizeof(wchar_t));
    if (!sec) return false;
    wcsncpy(sec, p + 1, len);
    sec[len] = 0;
    wcs_trim_inplace(sec);
    wchar_t* lower = wcs_to_lower_heap(sec);
    free(sec);
    if (!lower) return false;
    *outSectionLower = lower;
    return true;
}

static bool ini_load(const wchar_t* path, IniDoc* outDoc) {
    memset(outDoc, 0, sizeof(*outDoc));
    FILE* f = _wfopen(path, L"rb");
    if (!f) return false;

    // read entire file as bytes; assume UTF-8 or ANSI. We'll do a simple UTF-8->UTF-16 conversion.
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz < 0) { fclose(f); return false; }
    char* buf = (char*)malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return false; }
    size_t rd = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[rd] = 0;

    // Convert to UTF-16
    int wneed = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, buf, (int)rd, NULL, 0);
    UINT cp = CP_UTF8;
    if (wneed <= 0) {
        // fallback ANSI
        cp = CP_ACP;
        wneed = MultiByteToWideChar(cp, 0, buf, (int)rd, NULL, 0);
        if (wneed <= 0) { free(buf); return false; }
    }
    wchar_t* wbuf = (wchar_t*)malloc(((size_t)wneed + 1) * sizeof(wchar_t));
    if (!wbuf) { free(buf); return false; }
    MultiByteToWideChar(cp, 0, buf, (int)rd, wbuf, wneed);
    wbuf[wneed] = 0;
    free(buf);

    // Split into lines
    wchar_t* curSec = NULL;
    wchar_t* p = wbuf;
    while (*p) {
        wchar_t* lineStart = p;
        wchar_t* nl = wcschr(p, L'\n');
        if (nl) {
            *nl = 0;
            p = nl + 1;
        } else {
            p = lineStart + wcslen(lineStart);
        }
        strip_trailing_crlf(lineStart);

        IniLine l = {0};
        l.raw = wcsdup_heap(lineStart);
        if (!l.raw) { free(wbuf); free(curSec); ini_doc_free(outDoc); return false; }

        wchar_t* secLower = NULL;
        if (parse_section(lineStart, &secLower)) {
            l.is_section = true;
            free(curSec);
            curSec = secLower;
            l.section = wcsdup_heap(curSec);
        } else {
            l.section = curSec ? wcsdup_heap(curSec) : NULL;
            wchar_t* k = NULL; wchar_t* v = NULL;
            if (parse_kv(lineStart, &k, &v)) {
                l.is_kv = true;
                l.key = k;
                l.value = v;
            }
        }

        if (!ini_doc_push(outDoc, &l)) {
            free(wbuf); free(curSec); ini_doc_free(outDoc); return false;
        }
    }

    free(wbuf);
    free(curSec);
    return true;
}

static bool ini_save_preserve(const wchar_t* path, IniDoc* doc) {
    // Write UTF-8
    FILE* f = _wfopen(path, L"wb");
    if (!f) return false;
    for (size_t i = 0; i < doc->count; i++) {
        const wchar_t* wline = doc->lines[i].raw ? doc->lines[i].raw : L"";
        // Convert
        int need = WideCharToMultiByte(CP_UTF8, 0, wline, -1, NULL, 0, NULL, NULL);
        if (need <= 0) continue;
        char* b = (char*)malloc((size_t)need);
        if (!b) { fclose(f); return false; }
        WideCharToMultiByte(CP_UTF8, 0, wline, -1, b, need, NULL, NULL);
        // exclude NUL, add \r\n
        fwrite(b, 1, (size_t)(need - 1), f);
        fwrite("\r\n", 1, 2, f);
        free(b);
    }
    fclose(f);
    return true;
}

// Find last occurrence of key within a given section (lowercase section name).
static ssize_t ini_find_last_key(const IniDoc* doc, const wchar_t* sectionLower, const wchar_t* key) {
    ssize_t last = -1;
    for (size_t i = 0; i < doc->count; i++) {
        const IniLine* l = &doc->lines[i];
        if (!l->is_kv || !l->section || !l->key) continue;
        if (sectionLower && !wcs_ieq(l->section, sectionLower)) continue;
        if (wcs_ieq(l->key, key)) last = (ssize_t)i;
    }
    return last;
}

// Find end index of a section (line after last line in section), or doc->count if section is last.
// Also returns start index of section header if found (or -1).
static void ini_find_section_bounds(const IniDoc* doc, const wchar_t* sectionLower, ssize_t* outHeaderIdx, size_t* outInsertIdx) {
    *outHeaderIdx = -1;
    *outInsertIdx = doc->count;
    bool in = false;
    for (size_t i = 0; i < doc->count; i++) {
        const IniLine* l = &doc->lines[i];
        if (l->is_section && l->raw) {
            wchar_t* sec = NULL;
            if (parse_section(l->raw, &sec)) {
                bool isTarget = wcs_ieq(sec, sectionLower);
                free(sec);
                if (isTarget) {
                    *outHeaderIdx = (ssize_t)i;
                    in = true;
                    continue;
                }
                if (in) {
                    *outInsertIdx = i;
                    return;
                }
                in = false;
            }
        }
    }
    if (in) *outInsertIdx = doc->count;
}

// Insert a line at index (shifts)
static bool ini_insert_line(IniDoc* doc, size_t idx, const wchar_t* text, const wchar_t* sectionLower) {
    if (doc->count == doc->cap) {
        size_t ncap = doc->cap ? doc->cap * 2 : 256;
        IniLine* n = (IniLine*)realloc(doc->lines, ncap * sizeof(IniLine));
        if (!n) return false;
        doc->lines = n;
        doc->cap = ncap;
    }
    if (idx > doc->count) idx = doc->count;
    memmove(&doc->lines[idx + 1], &doc->lines[idx], (doc->count - idx) * sizeof(IniLine));
    doc->count++;

    IniLine l = {0};
    l.raw = wcsdup_heap(text);
    l.section = sectionLower ? wcsdup_heap(sectionLower) : NULL;
    // parse kv for tracking
    wchar_t* k = NULL; wchar_t* v = NULL;
    if (parse_kv(text, &k, &v)) {
        l.is_kv = true;
        l.key = k;
        l.value = v;
    }
    doc->lines[idx] = l;
    return true;
}

static bool ini_set_value_preserve(IniDoc* doc, const wchar_t* sectionLower, const wchar_t* key, const wchar_t* value) {
    ssize_t idx = ini_find_last_key(doc, sectionLower, key);
    wchar_t newline[2048];
    _snwprintf(newline, ARRAYSIZE(newline), L"%s=%s", key, value ? value : L"");
    newline[ARRAYSIZE(newline)-1] = 0;

    if (idx >= 0) {
        // Replace raw line only; preserve section assignment
        free(doc->lines[idx].raw);
        doc->lines[idx].raw = wcsdup_heap(newline);
        // update parsed value too
        free(doc->lines[idx].value);
        doc->lines[idx].value = value ? wcsdup_heap(value) : wcsdup_heap(L"");
        return doc->lines[idx].raw != NULL && doc->lines[idx].value != NULL;
    }

    // Need to insert at end of section
    ssize_t headerIdx = -1; size_t insertIdx = doc->count;
    ini_find_section_bounds(doc, sectionLower, &headerIdx, &insertIdx);
    if (headerIdx < 0) {
        wchar_t secLine[256];
        _snwprintf(secLine, ARRAYSIZE(secLine), L"[%s]", sectionLower);
        secLine[ARRAYSIZE(secLine)-1] = 0;
        if (!ini_insert_line(doc, doc->count, secLine, sectionLower)) return false;
        insertIdx = doc->count;
    }
    // Insert just before insertIdx (end of section)
    return ini_insert_line(doc, insertIdx, newline, sectionLower);
}

static bool ini_set_env_value_preserve(IniDoc* doc, const wchar_t* key, const wchar_t* value) {
    return ini_set_value_preserve(doc, L"env", key, value);
}

// -------------------------
// Schema model: [ui.env]
// -------------------------
typedef enum VarType {
    VAR_BOOL,
    VAR_INT,
    VAR_FLOAT,
    VAR_STRING
} VarType;

typedef struct VarSpec {
    wchar_t* name;   // env var name
    VarType type;

    wchar_t* label;
    wchar_t* help;

    wchar_t* defval;

    // bool
    wchar_t* true_val;
    wchar_t* false_val;

    // int/float
    bool has_min, has_max;
    double minv, maxv;

    // string
    int maxlen; // 0 = no limit
    bool secret;

    int order;
    bool visible;

    // runtime
    wchar_t* curval; // current value string (from [env] or default)
    HWND hCtrl;
    HWND hLabel;
} VarSpec;

typedef struct VarList {
    VarSpec* v;
    size_t n, cap;
} VarList;

static void varspec_free(VarSpec* s) {
    if (!s) return;
    free(s->name);
    free(s->label);
    free(s->help);
    free(s->defval);
    free(s->true_val);
    free(s->false_val);
    free(s->curval);
    memset(s, 0, sizeof(*s));
}

static void varlist_free(VarList* L) {
    if (!L) return;
    for (size_t i = 0; i < L->n; i++) varspec_free(&L->v[i]);
    free(L->v);
    L->v = NULL; L->n = L->cap = 0;
}

static bool varlist_push(VarList* L, VarSpec* s) {
    if (L->n == L->cap) {
        size_t ncap = L->cap ? L->cap * 2 : 64;
        VarSpec* nv = (VarSpec*)realloc(L->v, ncap * sizeof(VarSpec));
        if (!nv) return false;
        L->v = nv;
        L->cap = ncap;
    }
    L->v[L->n++] = *s;
    return true;
}

static bool parse_attr_kv(wchar_t* token, wchar_t** outK, wchar_t** outV) {
    *outK = NULL; *outV = NULL;
    wcs_trim_inplace(token);
    if (!*token) return false;
    wchar_t* eq = wcschr(token, L'=');
    if (!eq) return false;
    *eq = 0;
    wchar_t* k = token;
    wchar_t* v = eq + 1;
    wcs_trim_inplace(k);
    wcs_trim_inplace(v);
    if (!*k) return false;

    // Unquote v if fully quoted
    size_t vlen = wcslen(v);
    if (vlen >= 2 && ((v[0] == L'"' && v[vlen-1] == L'"') || (v[0] == L'\'' && v[vlen-1] == L'\''))) {
        v[vlen-1] = 0;
        memmove(v, v+1, (vlen-1) * sizeof(wchar_t));
    }
    *outK = k;
    *outV = v;
    return true;
}

static bool parse_bool01(const wchar_t* s, bool* out) {
    if (!s) return false;
    if (wcs_ieq(s, L"1") || wcs_ieq(s, L"true") || wcs_ieq(s, L"yes") || wcs_ieq(s, L"on")) { *out = true; return true; }
    if (wcs_ieq(s, L"0") || wcs_ieq(s, L"false") || wcs_ieq(s, L"no") || wcs_ieq(s, L"off")) { *out = false; return true; }
    return false;
}

static bool parse_int32(const wchar_t* s, int* out) {
    if (!s) return false;
    wchar_t* end = NULL;
    long v = wcstol(s, &end, 10);
    if (end == s || (end && *end != 0)) return false;
    if (v < INT32_MIN || v > INT32_MAX) return false;
    *out = (int)v;
    return true;
}

static bool parse_double(const wchar_t* s, double* out) {
    if (!s) return false;
    wchar_t* end = NULL;
    double v = wcstod(s, &end);
    if (end == s || (end && *end != 0)) return false;
    *out = v;
    return true;
}

static int cmp_vars(const void* a, const void* b) {
    const VarSpec* A = (const VarSpec*)a;
    const VarSpec* B = (const VarSpec*)b;
    if (A->order != B->order) return (A->order < B->order) ? -1 : 1;
    const wchar_t* la = A->label ? A->label : A->name;
    const wchar_t* lb = B->label ? B->label : B->name;
    int c = _wcsicmp(la, lb);
    if (c != 0) return c;
    return _wcsicmp(A->name, B->name);
}

// Extract env value for key from doc (last occurrence in [env])
static wchar_t* ini_get_env_value(const IniDoc* doc, const wchar_t* key) {
    ssize_t idx = ini_find_last_key(doc, L"env", key);
    if (idx < 0) return NULL;
    if (!doc->lines[idx].value) return NULL;
    return wcsdup_heap(doc->lines[idx].value);
}

static void varspec_set_defaults(VarSpec* s) {
    s->label = NULL;
    s->help = NULL;
    s->defval = NULL;
    s->true_val = wcsdup_heap(L"1");
    s->false_val = wcsdup_heap(L"0");
    s->has_min = s->has_max = false;
    s->minv = 0; s->maxv = 0;
    s->maxlen = 0;
    s->secret = false;
    s->order = 1000;
    s->visible = true;
    s->curval = NULL;
    s->hCtrl = NULL;
    s->hLabel = NULL;
}

// MinGW's wcstok is 2-arg; MSVC has wcstok_s. Use our own splitter.
static wchar_t* wcs_tok_semicolon(wchar_t* s, wchar_t** ctx) {
    if (s) {
        *ctx = s;
    } else if (!ctx || !*ctx) {
        return NULL;
    }

    wchar_t* p = *ctx;

    // Skip leading delimiters ';'
    while (*p == L';') p++;

    if (*p == 0) {
        *ctx = p;
        return NULL;
    }

    wchar_t* start = p;
    while (*p && *p != L';') p++;

    if (*p == L';') {
        *p = 0;
        p++;
    }

    *ctx = p;
    return start;
}

static bool load_schema(const IniDoc* doc, VarList* outVars) {
    memset(outVars, 0, sizeof(*outVars));

    for (size_t i = 0; i < doc->count; i++) {
        const IniLine* l = &doc->lines[i];
        if (!l->is_kv || !l->section || !wcs_ieq(l->section, L"ui.env")) continue;
        if (!l->key || !l->value) continue;

        VarSpec s = {0};
        varspec_set_defaults(&s);
        s.name = wcsdup_heap(l->key);
        if (!s.name) { varspec_free(&s); varlist_free(outVars); return false; }

        // Parse attribute list from l->value (semicolon-separated)
        wchar_t* tmp = wcsdup_heap(l->value);
        if (!tmp) { varspec_free(&s); varlist_free(outVars); return false; }

        // Split by ';'
        wchar_t* ctx = NULL;
        wchar_t* token = wcs_tok_semicolon(tmp, &ctx);
        bool hasType = false;
        while (token) {
            wchar_t* k = NULL; wchar_t* v = NULL;
            if (parse_attr_kv(token, &k, &v)) {
                wchar_t* kl = wcs_to_lower_heap(k);
                if (kl) {
                    if (wcs_ieq(kl, L"type")) {
                        hasType = true;
                        if (wcs_ieq(v, L"bool")) s.type = VAR_BOOL;
                        else if (wcs_ieq(v, L"int")) s.type = VAR_INT;
                        else if (wcs_ieq(v, L"float")) s.type = VAR_FLOAT;
                        else s.type = VAR_STRING;
                    } else if (wcs_ieq(kl, L"label")) {
                        free(s.label); s.label = wcsdup_heap(v);
                    } else if (wcs_ieq(kl, L"help")) {
                        free(s.help); s.help = wcsdup_heap(v);
                    } else if (wcs_ieq(kl, L"default")) {
                        free(s.defval); s.defval = wcsdup_heap(v);
                    } else if (wcs_ieq(kl, L"true_value")) {
                        free(s.true_val); s.true_val = wcsdup_heap(v);
                    } else if (wcs_ieq(kl, L"false_value")) {
                        free(s.false_val); s.false_val = wcsdup_heap(v);
                    } else if (wcs_ieq(kl, L"min")) {
                        double dv;
                        if (parse_double(v, &dv)) { s.has_min = true; s.minv = dv; }
                    } else if (wcs_ieq(kl, L"max")) {
                        double dv;
                        if (parse_double(v, &dv)) { s.has_max = true; s.maxv = dv; }
                    } else if (wcs_ieq(kl, L"maxlen")) {
                        int iv;
                        if (parse_int32(v, &iv) && iv > 0) s.maxlen = iv;
                    } else if (wcs_ieq(kl, L"secret")) {
                        bool bv;
                        if (parse_bool01(v, &bv)) s.secret = bv;
                    } else if (wcs_ieq(kl, L"order")) {
                        int iv;
                        if (parse_int32(v, &iv)) s.order = iv;
                    } else if (wcs_ieq(kl, L"visible")) {
                        bool bv;
                        if (parse_bool01(v, &bv)) s.visible = bv;
                    }
                    free(kl);
                }
            }
            token = wcs_tok_semicolon(NULL, &ctx);
        }
        free(tmp);

        if (!hasType) { varspec_free(&s); continue; }
        if (!s.visible) { varspec_free(&s); continue; }

        // Set current value:
        wchar_t* ev = ini_get_env_value(doc, s.name);
        if (ev) s.curval = ev;
        else if (s.defval) s.curval = wcsdup_heap(s.defval);
        else {
            // neutral default
            if (s.type == VAR_BOOL) s.curval = wcsdup_heap(s.false_val ? s.false_val : L"0");
            else if (s.type == VAR_INT) s.curval = wcsdup_heap(L"0");
            else if (s.type == VAR_FLOAT) s.curval = wcsdup_heap(L"0.0");
            else s.curval = wcsdup_heap(L"");
        }

        if (!varlist_push(outVars, &s)) { varspec_free(&s); varlist_free(outVars); return false; }
    }

    // Sort
    qsort(outVars->v, outVars->n, sizeof(VarSpec), cmp_vars);
    return true;
}

// -------------------------
// Launcher config from [launcher]
// -------------------------
typedef struct LauncherCfg {
    bool hide_when_ready;
    uint32_t ring_buffer_bytes;
    bool convert_dialog;
    wchar_t convert_marker[MAX_PATH * 4];
    wchar_t dialog_dir[MAX_PATH * 4];
    wchar_t assets_dir[MAX_PATH * 4];
    wchar_t template_cfg[MAX_PATH * 4];
    wchar_t log_file[MAX_PATH * 4];
    int fullscreen;
    int bits;
    wchar_t system_resolution[32];
} LauncherCfg;

static void cfg_defaults(LauncherCfg* c) {
    c->hide_when_ready = false;
    c->ring_buffer_bytes = 1048576;
    c->convert_dialog = true;
    wcscpy_s(c->convert_marker, ARRAYSIZE(c->convert_marker), L"gamefiles\\app\\converted_dialog.txt");
    wcscpy_s(c->dialog_dir, ARRAYSIZE(c->dialog_dir), L"gamefiles\\app\\Dialog");
    wcscpy_s(c->assets_dir, ARRAYSIZE(c->assets_dir), L"gamefiles\\app");
    wcscpy_s(c->template_cfg, ARRAYSIZE(c->template_cfg), L"nox.cfg");
    wcscpy_s(c->log_file, ARRAYSIZE(c->log_file), L"log.txt");
    c->fullscreen = 1;
    c->bits = 16;
    wcscpy_s(c->system_resolution, ARRAYSIZE(c->system_resolution), L"native");
}

static wchar_t* ini_get_launcher_value(const IniDoc* doc, const wchar_t* key) {
    ssize_t idx = ini_find_last_key(doc, L"launcher", key);
    if (idx < 0) return NULL;
    if (!doc->lines[idx].value) return NULL;
    return wcsdup_heap(doc->lines[idx].value);
}

static void load_launcher_cfg(const IniDoc* doc, LauncherCfg* c) {
    cfg_defaults(c);
    wchar_t* v = NULL;
    bool b;
    int iv;

    v = ini_get_launcher_value(doc, L"hide_when_ready");
    if (v) { if (parse_bool01(v, &b)) c->hide_when_ready = b; free(v); }

    v = ini_get_launcher_value(doc, L"ring_buffer_bytes");
    if (v) { if (parse_int32(v, &iv) && iv > 0) c->ring_buffer_bytes = (uint32_t)iv; free(v); }

    v = ini_get_launcher_value(doc, L"convert_dialog");
    if (v) { if (parse_bool01(v, &b)) c->convert_dialog = b; free(v); }

    v = ini_get_launcher_value(doc, L"convert_marker");
    if (v) { wcsncpy(c->convert_marker, v, ARRAYSIZE(c->convert_marker)-1); c->convert_marker[ARRAYSIZE(c->convert_marker)-1]=0; free(v); }

    v = ini_get_launcher_value(doc, L"dialog_dir");
    if (v) { wcsncpy(c->dialog_dir, v, ARRAYSIZE(c->dialog_dir)-1); c->dialog_dir[ARRAYSIZE(c->dialog_dir)-1]=0; free(v); }

    v = ini_get_launcher_value(doc, L"assets_dir");
    if (v) { wcsncpy(c->assets_dir, v, ARRAYSIZE(c->assets_dir)-1); c->assets_dir[ARRAYSIZE(c->assets_dir)-1]=0; free(v); }

    v = ini_get_launcher_value(doc, L"template_cfg");
    if (v) { wcsncpy(c->template_cfg, v, ARRAYSIZE(c->template_cfg)-1); c->template_cfg[ARRAYSIZE(c->template_cfg)-1]=0; free(v); }

    v = ini_get_launcher_value(doc, L"log_file");
    if (v) { wcsncpy(c->log_file, v, ARRAYSIZE(c->log_file)-1); c->log_file[ARRAYSIZE(c->log_file)-1]=0; free(v); }

    v = ini_get_launcher_value(doc, L"fullscreen");
    if (v) { if (parse_int32(v, &iv)) c->fullscreen = (iv != 0) ? 1 : 0; free(v); }

    v = ini_get_launcher_value(doc, L"bits");
    if (v) { if (parse_int32(v, &iv) && iv > 0) c->bits = iv; free(v); }

    v = ini_get_launcher_value(doc, L"system_resolution");
    if (v) { wcsncpy(c->system_resolution, v, ARRAYSIZE(c->system_resolution)-1); c->system_resolution[ARRAYSIZE(c->system_resolution)-1]=0; free(v); }
}

// Resolve config path possibly relative to gamedir.
static void resolve_path(wchar_t* out, size_t cap, const wchar_t* gamedir, const wchar_t* maybeRel) {
    if (path_is_abs(maybeRel)) {
        wcsncpy(out, maybeRel, cap-1);
        out[cap-1]=0;
    } else {
        path_join(out, cap, gamedir, maybeRel);
    }
}

// -------------------------
// UI
// -------------------------
#define IDC_BTN_SAVE   1001
#define IDC_BTN_LAUNCH 1002
#define IDC_BTN_BROWSE 1003
#define IDC_STATUS     1004
#define IDC_LOG        1005
#define IDC_SCROLLPANE 1006
#define IDC_RESOLUTION 1007
#define IDC_SRC_GOG       2001
#define IDC_SRC_INSTALLED 2002
#define IDC_SRC_CANCEL    2003

typedef struct AppState {
    HINSTANCE hInst;
    HWND hwnd;

    HWND hStatus;
    HWND hBtnSave;
    HWND hBtnLaunch;
    HWND hBtnBrowse;

    HWND hScrollPane; // container with WS_VSCROLL
    HWND hLogEdit;
    HWND hResolutionLabel;
    HWND hResolutionCombo;

    wchar_t launcherDir[MAX_PATH * 4];
    wchar_t gamedir[MAX_PATH * 4];
    wchar_t iniPath[MAX_PATH * 4];

    wchar_t chosenInstallerAbs[MAX_PATH * 4];  // setup file picked for this run only
    wchar_t chosenInstallAppAbs[MAX_PATH * 4]; // installed game app dir picked for this run only

    IniDoc ini;
    VarList vars;
    LauncherCfg cfg;

    wchar_t assetsDirAbs[MAX_PATH * 4];
    wchar_t dialogDirAbs[MAX_PATH * 4];
    wchar_t markerAbs[MAX_PATH * 4];
    wchar_t introMarkerAbs[MAX_PATH * 4];
    wchar_t logAbs[MAX_PATH * 4];
    wchar_t templateCfgAbs[MAX_PATH * 4];
    wchar_t gamepadIniAbs[MAX_PATH * 4];

    wchar_t srcDirAbs[MAX_PATH * 4];
    wchar_t neededAbs[MAX_PATH * 4];

    wchar_t innoextractAbs[MAX_PATH * 4];
    wchar_t ffmpegAbs[MAX_PATH * 4];

    wchar_t runArch[16]; // "i386" / "armhf"
    wchar_t deviceArch[16];

    bool extraction_succeeded_this_run;
} AppState;

static AppState g_app = {0};

static bool ensure_required_free_space_for_install(HWND hwnd, const AppState* a) {
    const ULONGLONG required = 700ULL * 1024ULL * 1024ULL;
    ULONGLONG freeBytes = 0;
    bool installWillHappen = false;

    if (a->chosenInstallerAbs[0]) installWillHappen = true;
    if (a->chosenInstallAppAbs[0]) installWillHappen = true;

    if (!installWillHappen) {
        return true;
    }

    if (has_required_free_space(a->launcherDir, required, &freeBytes)) {
        return true;
    }

    wchar_t msg[512];
    _snwprintf(
        msg, ARRAYSIZE(msg),
        L"Not enough free disk space on the launcher drive.\n\n"
        L"Required: 700 MB\n"
        L"Available: %llu MB\n\n"
        L"Please free some space and try again.",
        (unsigned long long)(freeBytes / (1024ULL * 1024ULL))
    );
    msg[ARRAYSIZE(msg) - 1] = 0;

    MessageBoxW(hwnd, msg, L"Nox Launcher", MB_ICONERROR);
    return false;
}

static void ui_set_status(const wchar_t* s) {
    if (g_app.hStatus) SetWindowTextW(g_app.hStatus, s ? s : L"");
}

// -------------------------
// Batched UI logging
// -------------------------
#define WM_APP_LOG_FLUSH (WM_APP + 10)

static CRITICAL_SECTION g_uiLogCs;
static wchar_t* g_uiLogBuf = NULL;
static size_t   g_uiLogLen = 0;   // chars
static size_t   g_uiLogCap = 0;   // chars
static LONG     g_uiFlushPosted = 0; // 0/1

static void ui_logbuf_append_nolock(const wchar_t* s)
{
    if (!s) return;
    size_t add = wcslen(s);
    if (add == 0) return;

    size_t need = g_uiLogLen + add + 1;
    if (need > g_uiLogCap) {
        size_t ncap = g_uiLogCap ? g_uiLogCap : 4096;
        while (ncap < need) ncap *= 2;
        wchar_t* nb = (wchar_t*)realloc(g_uiLogBuf, ncap * sizeof(wchar_t));
        if (!nb) return; // drop UI update if OOM (file logging still works)
        g_uiLogBuf = nb;
        g_uiLogCap = ncap;
    }

    memcpy(g_uiLogBuf + g_uiLogLen, s, add * sizeof(wchar_t));
    g_uiLogLen += add;
    g_uiLogBuf[g_uiLogLen] = 0;
}

static void ui_log_enqueue(const wchar_t* s)
{
    if (!s) return;

    // Always write to file immediately (unchanged behavior)
    log_file_write_w(s);

    // UI may not exist yet (startup/shutdown)
    if (!g_app.hwnd) return;

    EnterCriticalSection(&g_uiLogCs);
    ui_logbuf_append_nolock(s);
    LeaveCriticalSection(&g_uiLogCs);

    // Post exactly one flush message until UI drains the buffer
    if (InterlockedExchange(&g_uiFlushPosted, 1) == 0) {
        PostMessageW(g_app.hwnd, WM_APP_LOG_FLUSH, 0, 0);
    }
}

static void ui_log_enqueue_line(const wchar_t* s)
{
    if (!s) return;
    wchar_t buf[4096];
    _snwprintf(buf, ARRAYSIZE(buf), L"%s\r\n", s);
    buf[ARRAYSIZE(buf) - 1] = 0;
    ui_log_enqueue(buf);
}

static void ui_log(const wchar_t* s)
{
    ui_log_enqueue(s);
}

static void ui_log_line(const wchar_t* s)
{
    ui_log_enqueue_line(s);
}

static void ui_log_kv(const wchar_t* key, const wchar_t* value)
{
    wchar_t buf[4096];
    _snwprintf(buf, ARRAYSIZE(buf), L"%s: %s",
               key ? key : L"(null)",
               value ? value : L"(null)");
    buf[ARRAYSIZE(buf) - 1] = 0;
    ui_log_line(buf);
}

static void ui_log_kv_u32(const wchar_t* key, unsigned long value)
{
    wchar_t buf[256];
    _snwprintf(buf, ARRAYSIZE(buf), L"%s: %lu",
               key ? key : L"(null)",
               value);
    buf[ARRAYSIZE(buf) - 1] = 0;
    ui_log_line(buf);
}

// -------------------------
// Process runner (captures output to UI + log file)
// -------------------------
typedef struct ProcRunResult {
    DWORD exit_code;
    bool started;
} ProcRunResult;

static bool create_pipe_inheritable(HANDLE* outRead, HANDLE* outWrite) {
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;
    if (!CreatePipe(outRead, outWrite, &sa, 0)) return false;
    // Ensure read end not inherited by child
    SetHandleInformation(*outRead, HANDLE_FLAG_INHERIT, 0);
    return true;
}

static DWORD WINAPI reader_thread(LPVOID param)
{
    HANDLE hRead = (HANDLE)param;
    char buf[65536];
    DWORD n = 0;

    while (ReadFile(hRead, buf, sizeof(buf), &n, NULL) && n > 0) {
        // Convert to wide (UTF-8 best effort, fallback ACP)
        wchar_t wbuf[131072];
        int wlen = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, buf, (int)n,
                                       wbuf, (int)ARRAYSIZE(wbuf) - 1);
        if (wlen <= 0) {
            wlen = MultiByteToWideChar(CP_ACP, 0, buf, (int)n,
                                       wbuf, (int)ARRAYSIZE(wbuf) - 1);
        }
        if (wlen > 0) {
            wbuf[wlen] = 0;
            ui_log_enqueue(wbuf); // batched UI + file
        }
    }
    return 0;
}

static ProcRunResult run_process_capture(const wchar_t* exe, const wchar_t* args, const wchar_t* workdir) {
    ProcRunResult rr = {0};
    rr.exit_code = (DWORD)-1;
    rr.started = false;

    // Build command line: "exe" args
    wchar_t cmd[8192];
    if (args && *args) _snwprintf(cmd, ARRAYSIZE(cmd), L"\"%s\" %s", exe, args);
    else _snwprintf(cmd, ARRAYSIZE(cmd), L"\"%s\"", exe);
    cmd[ARRAYSIZE(cmd)-1] = 0;

    HANDLE hOutR = NULL, hOutW = NULL;
    if (!create_pipe_inheritable(&hOutR, &hOutW)) return rr;

    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    ZeroMemory(&pi, sizeof(pi));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    si.hStdOutput = hOutW;
    si.hStdError = hOutW;

    rr.started = CreateProcessW(
        NULL,
        cmd,
        NULL, NULL,
        TRUE,                // inherit handles
        CREATE_NO_WINDOW,
        NULL,
        workdir && *workdir ? workdir : NULL,
        &si,
        &pi
    ) ? true : false;

    CloseHandle(hOutW);
    if (!rr.started) {
        CloseHandle(hOutR);
        return rr;
    }

    HANDLE hReader = CreateThread(NULL, 0, reader_thread, (LPVOID)hOutR, 0, NULL);

    WaitForSingleObject(pi.hProcess, INFINITE);
    GetExitCodeProcess(pi.hProcess, &rr.exit_code);

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    // Close read end, wait reader
    CloseHandle(hOutR);
    if (hReader) {
        WaitForSingleObject(hReader, 2000);
        CloseHandle(hReader);
    }

    return rr;
}

// -------------------------
// File dialog for installer
// -------------------------
static bool pick_installer(HWND hwnd, wchar_t* outPath, size_t cap) {
    OPENFILENAMEW ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    wchar_t buf[MAX_PATH * 4] = {0};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFile = buf;
    ofn.nMaxFile = ARRAYSIZE(buf);
    ofn.lpstrTitle = L"Select Nox GOG installer (setup_nox*.exe)";
    ofn.lpstrFilter = L"GOG installer (setup_nox*.exe)\0setup_nox*.exe\0EXE files (*.exe)\0*.exe\0All files (*.*)\0*.*\0\0";
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    if (!GetOpenFileNameW(&ofn)) return false;
    wcsncpy(outPath, buf, cap-1);
    outPath[cap-1]=0;
    return true;
}

static bool pick_folder(HWND hwnd, wchar_t* outPath, size_t cap, const wchar_t* title) {
    BROWSEINFOW bi;
    ZeroMemory(&bi, sizeof(bi));
    bi.hwndOwner = hwnd;
    bi.lpszTitle = title;
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE | BIF_USENEWUI;

    PIDLIST_ABSOLUTE pidl = SHBrowseForFolderW(&bi);
    if (!pidl) return false;

    bool ok = false;
    wchar_t buf[MAX_PATH * 4] = {0};
    if (SHGetPathFromIDListW(pidl, buf)) {
        wcsncpy(outPath, buf, cap - 1);
        outPath[cap - 1] = 0;
        ok = true;
    }

    CoTaskMemFree(pidl);
    return ok;
}

static bool path_is_dot_or_dotdot(const wchar_t* name) {
    return wcs_ieq(name, L".") || wcs_ieq(name, L"..");
}

static bool copy_file_simple(const wchar_t* src, const wchar_t* dst) {
    ensure_parent_dir(dst);
    return CopyFileW(src, dst, FALSE) ? true : false;
}

static bool copy_tree_recursive(const wchar_t* srcDir, const wchar_t* dstDir) {
    WIN32_FIND_DATAW fd;
    wchar_t pattern[MAX_PATH * 4];
    path_join(pattern, ARRAYSIZE(pattern), srcDir, L"*");

    HANDLE h = FindFirstFileW(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) return false;

    ensure_dir(dstDir);

    bool ok = true;
    do {
        if (path_is_dot_or_dotdot(fd.cFileName)) continue;

        wchar_t srcPath[MAX_PATH * 4];
        wchar_t dstPath[MAX_PATH * 4];
        path_join(srcPath, ARRAYSIZE(srcPath), srcDir, fd.cFileName);
        path_join(dstPath, ARRAYSIZE(dstPath), dstDir, fd.cFileName);

        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (!copy_tree_recursive(srcPath, dstPath)) {
                ok = false;
                break;
            }
        } else {
            if (!copy_file_simple(srcPath, dstPath)) {
                ok = false;
                break;
            }
        }
    } while (FindNextFileW(h, &fd));

    FindClose(h);
    return ok;
}

static bool resolve_install_app_dir_from_pick(const wchar_t* pickedDir, wchar_t* outAppDir, size_t cap) {
    wchar_t p[MAX_PATH * 4];

    // case 1: user picked the app dir directly
    path_join(p, ARRAYSIZE(p), pickedDir, L"gamedata.bin");
    if (file_exists(p)) {
        wcsncpy(outAppDir, pickedDir, cap - 1);
        outAppDir[cap - 1] = 0;
        return true;
    }

    // case 2: user picked game root containing gamefiles/app
    path_join(p, ARRAYSIZE(p), pickedDir, L"gamefiles\\app\\gamedata.bin");
    if (file_exists(p)) {
        path_join(outAppDir, cap, pickedDir, L"gamefiles\\app");
        return true;
    }

    return false;
}

static bool copy_installed_files_into_local_app(const wchar_t* srcAppDir, const wchar_t* dstAppDir) {
    wchar_t srcGameData[MAX_PATH * 4];
    path_join(srcGameData, ARRAYSIZE(srcGameData), srcAppDir, L"gamedata.bin");
    if (!file_exists(srcGameData)) return false;

    ensure_dir(dstAppDir);
    return copy_tree_recursive(srcAppDir, dstAppDir);
}

typedef struct SourceChoiceState {
    int result;
} SourceChoiceState;

static LRESULT CALLBACK SourceChoiceWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    SourceChoiceState* st = (SourceChoiceState*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);

    switch (msg) {
        case WM_CREATE: {
            CREATESTRUCTW* cs = (CREATESTRUCTW*)lParam;
            st = (SourceChoiceState*)cs->lpCreateParams;
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)st);

            HFONT hf = (HFONT)GetStockObject(DEFAULT_GUI_FONT);

            HWND hText = CreateWindowExW(
                0, L"STATIC",
                L"Game data was not found in gamefiles\\app.\n\nChoose how to provide it:\n- GOG setup: extracts files into this launcher folder and uses about 700 MB of disk space.\n- Installed files: copies files from an existing Nox installation including save files into this launcher folder and also uses about 700 MB of disk space.",
                WS_CHILD | WS_VISIBLE,
                16, 16, 430, 110,
                hwnd, NULL, g_app.hInst, NULL
            );
            SendMessageW(hText, WM_SETFONT, (WPARAM)hf, TRUE);

            HWND hInstalled = CreateWindowExW(
                0, L"BUTTON", L"Installed files",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
                16, 150, 140, 30,
                hwnd, (HMENU)(INT_PTR)IDC_SRC_INSTALLED, g_app.hInst, NULL
            );
            SendMessageW(hInstalled, WM_SETFONT, (WPARAM)hf, TRUE);

            HWND hGog = CreateWindowExW(
                0, L"BUTTON", L"GOG setup",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
                168, 150, 140, 30,
                hwnd, (HMENU)(INT_PTR)IDC_SRC_GOG, g_app.hInst, NULL
            );
            SendMessageW(hGog, WM_SETFONT, (WPARAM)hf, TRUE);

            HWND hCancel = CreateWindowExW(
                0, L"BUTTON", L"Cancel",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
                320, 150, 80, 30,
                hwnd, (HMENU)(INT_PTR)IDC_SRC_CANCEL, g_app.hInst, NULL
            );
            SendMessageW(hCancel, WM_SETFONT, (WPARAM)hf, TRUE);

            SetFocus(hInstalled);
            return 0;
        }

        case WM_COMMAND: {
            int id = LOWORD(wParam);
            if (!st) return 0;

            if (id == IDC_SRC_INSTALLED) {
                st->result = IDC_SRC_INSTALLED;
                DestroyWindow(hwnd);
                return 0;
            }
            if (id == IDC_SRC_GOG) {
                st->result = IDC_SRC_GOG;
                DestroyWindow(hwnd);
                return 0;
            }
            if (id == IDC_SRC_CANCEL) {
                st->result = IDCANCEL;
                DestroyWindow(hwnd);
                return 0;
            }
            return 0;
        }

        case WM_CLOSE:
            if (st) st->result = IDCANCEL;
            DestroyWindow(hwnd);
            return 0;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

static int choose_source_dialog(HWND parent) {
    const wchar_t* cls = L"NoxSourceChoiceWnd";
    static bool registered = false;

    if (!registered) {
        WNDCLASSW wc;
        ZeroMemory(&wc, sizeof(wc));
        wc.lpfnWndProc = SourceChoiceWndProc;
        wc.hInstance = g_app.hInst;
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wc.lpszClassName = cls;
        if (!RegisterClassW(&wc)) return IDCANCEL;
        registered = true;
    }

    SourceChoiceState st;
    st.result = IDCANCEL;

    HWND hwnd = CreateWindowExW(
        WS_EX_DLGMODALFRAME,
        cls,
        L"Nox Launcher",
        WS_CAPTION | WS_SYSMENU | WS_POPUP,
        CW_USEDEFAULT, CW_USEDEFAULT, 470, 230,
        parent, NULL, g_app.hInst, &st
    );
    if (!hwnd) return IDCANCEL;

    EnableWindow(parent, FALSE);
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    MSG msg;
    while (IsWindow(hwnd) && GetMessageW(&msg, NULL, 0, 0) > 0) {
        if (!IsDialogMessageW(hwnd, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    EnableWindow(parent, TRUE);
    SetActiveWindow(parent);
    return st.result;
}

static void resolve_paths(AppState* a);

static bool choose_game_source_and_target(HWND hwnd, AppState* a) {
    // Launcher always runs from its own local files.
    wcsncpy(a->gamedir, a->launcherDir, ARRAYSIZE(a->gamedir) - 1);
    a->gamedir[ARRAYSIZE(a->gamedir) - 1] = 0;

    a->chosenInstallerAbs[0] = 0;
    a->chosenInstallAppAbs[0] = 0;

    // If local game data already exists, nothing to ask.
    resolve_paths(a);
    if (file_exists(a->neededAbs)) {
        return true;
    }

    int r = choose_source_dialog(hwnd);
    if (r == IDCANCEL) return false;

    if (r == IDC_SRC_GOG) {
        wchar_t installer[MAX_PATH * 4] = {0};
        if (!pick_installer(hwnd, installer, ARRAYSIZE(installer))) {
            return false;
        }

        wcsncpy(a->chosenInstallerAbs, installer, ARRAYSIZE(a->chosenInstallerAbs) - 1);
        a->chosenInstallerAbs[ARRAYSIZE(a->chosenInstallerAbs) - 1] = 0;
        return true;
    }

    if (r == IDC_SRC_INSTALLED) {
        wchar_t pickedDir[MAX_PATH * 4] = {0};
        wchar_t appDir[MAX_PATH * 4] = {0};

        if (!pick_folder(hwnd, pickedDir, ARRAYSIZE(pickedDir),
                         L"Select the installed Nox folder or its gamefiles\\app folder")) {
            return false;
        }

        if (!resolve_install_app_dir_from_pick(pickedDir, appDir, ARRAYSIZE(appDir))) {
            MessageBoxW(
                hwnd,
                L"The selected location does not contain a valid gamedata.bin.\n"
                L"Please choose either the installed game root or its gamefiles\\app folder.",
                L"Nox Launcher",
                MB_ICONERROR
            );
            return false;
        }

        wcsncpy(a->chosenInstallAppAbs, appDir, ARRAYSIZE(a->chosenInstallAppAbs) - 1);
        a->chosenInstallAppAbs[ARRAYSIZE(a->chosenInstallAppAbs) - 1] = 0;
        return true;
    }

    return false;
}

// -------------------------
// Desktop resolution + Nox resolution rules
// -------------------------
static bool get_desktop_resolution(int* outW, int* outH) {
    if (!outW || !outH) return false;
    DEVMODEW dm;
    ZeroMemory(&dm, sizeof(dm));
    dm.dmSize = sizeof(dm);
    if (!EnumDisplaySettingsW(NULL, ENUM_CURRENT_SETTINGS, &dm)) return false;
    if (dm.dmPelsWidth <= 0 || dm.dmPelsHeight <= 0) return false;
    *outW = (int)dm.dmPelsWidth;
    *outH = (int)dm.dmPelsHeight;
    return true;
}

typedef struct ResolutionEntry {
    int w;
    int h;
} ResolutionEntry;

static int __cdecl resolution_entry_cmp_desc(const void* a, const void* b) {
    const ResolutionEntry* A = (const ResolutionEntry*)a;
    const ResolutionEntry* B = (const ResolutionEntry*)b;
    int areaA = A->w * A->h;
    int areaB = B->w * B->h;
    if (areaA != areaB) return (areaA > areaB) ? -1 : 1;
    if (A->w != B->w) return (A->w > B->w) ? -1 : 1;
    return 0;
}

static const wchar_t* resolution_aspect_label(int w, int h) {
    if (w <= 0 || h <= 0) return L"Wide";
    double aspect = (double)w / (double)h;
    if (aspect > 1.30 && aspect < 1.36) return L"4:3";
    return L"Wide";
}

static bool parse_resolution_setting(const wchar_t* s, int* outW, int* outH) {
    if (outW) *outW = 0;
    if (outH) *outH = 0;
    if (!s || !*s || wcs_ieq(s, L"native")) return false;

    int w = 0, h = 0;
    if (swscanf(s, L"%dx%d", &w, &h) == 2 && w > 0 && h > 0) {
        if (outW) *outW = w;
        if (outH) *outH = h;
        return true;
    }
    return false;
}

static DWORD pack_resolution_item_data(int w, int h) {
    if (w <= 0 || h <= 0 || w > 65535 || h > 65535) return 0;
    return ((DWORD)w << 16) | (DWORD)h;
}

static void unpack_resolution_item_data(DWORD data, int* outW, int* outH) {
    if (outW) *outW = (int)((data >> 16) & 0xffff);
    if (outH) *outH = (int)(data & 0xffff);
}

static void resolution_update_from_ctrl(LauncherCfg* cfg) {
    if (!cfg || !g_app.hResolutionCombo) return;
    int sel = (int)SendMessageW(g_app.hResolutionCombo, CB_GETCURSEL, 0, 0);
    if (sel == CB_ERR) {
        wcscpy_s(cfg->system_resolution, ARRAYSIZE(cfg->system_resolution), L"native");
        return;
    }

    DWORD data = (DWORD)SendMessageW(g_app.hResolutionCombo, CB_GETITEMDATA, (WPARAM)sel, 0);
    int w = 0, h = 0;
    unpack_resolution_item_data(data, &w, &h);
    if (w <= 0 || h <= 0) {
        wcscpy_s(cfg->system_resolution, ARRAYSIZE(cfg->system_resolution), L"native");
    } else {
        _snwprintf(cfg->system_resolution, ARRAYSIZE(cfg->system_resolution), L"%dx%d", w, h);
        cfg->system_resolution[ARRAYSIZE(cfg->system_resolution)-1] = 0;
    }
}

static void populate_resolution_combo(HWND hCombo, const wchar_t* selectedSetting) {
    if (!hCombo) return;

    SendMessageW(hCombo, CB_RESETCONTENT, 0, 0);

    int nativeIdx = (int)SendMessageW(hCombo, CB_ADDSTRING, 0, (LPARAM)L"Native");
    if (nativeIdx >= 0) SendMessageW(hCombo, CB_SETITEMDATA, (WPARAM)nativeIdx, (LPARAM)0);

    DEVMODEW cur;
    ZeroMemory(&cur, sizeof(cur));
    cur.dmSize = sizeof(cur);
    bool haveCur = EnumDisplaySettingsW(NULL, ENUM_CURRENT_SETTINGS, &cur) ? true : false;

    ResolutionEntry entries[256];
    int n = 0;

    if (haveCur) {
        for (DWORD i = 0; ; i++) {
            DEVMODEW dm;
            ZeroMemory(&dm, sizeof(dm));
            dm.dmSize = sizeof(dm);
            if (!EnumDisplaySettingsW(NULL, i, &dm)) break;

            int w = (int)dm.dmPelsWidth;
            int h = (int)dm.dmPelsHeight;
            if (w < 640 || h < 360) continue;
            if (w > (int)cur.dmPelsWidth || h > (int)cur.dmPelsHeight) continue;
            if (cur.dmBitsPerPel > 0 && dm.dmBitsPerPel > 0 && dm.dmBitsPerPel != cur.dmBitsPerPel) continue;

            bool exists = false;
            for (int j = 0; j < n; j++) {
                if (entries[j].w == w && entries[j].h == h) { exists = true; break; }
            }
            if (exists) continue;
            if (n >= (int)ARRAYSIZE(entries)) break;
            entries[n].w = w;
            entries[n].h = h;
            n++;
        }
    }

    qsort(entries, (size_t)n, sizeof(entries[0]), resolution_entry_cmp_desc);

    int selectedIdx = nativeIdx;
    int selectedW = 0, selectedH = 0;
    bool wantFixed = parse_resolution_setting(selectedSetting, &selectedW, &selectedH);

    for (int i = 0; i < n; i++) {
        wchar_t text[128];
        _snwprintf(text, ARRAYSIZE(text), L"%d x %d (%s)", entries[i].w, entries[i].h, resolution_aspect_label(entries[i].w, entries[i].h));
        text[ARRAYSIZE(text)-1] = 0;

        int idx = (int)SendMessageW(hCombo, CB_ADDSTRING, 0, (LPARAM)text);
        if (idx >= 0) {
            SendMessageW(hCombo, CB_SETITEMDATA, (WPARAM)idx, (LPARAM)pack_resolution_item_data(entries[i].w, entries[i].h));
            if (wantFixed && entries[i].w == selectedW && entries[i].h == selectedH) selectedIdx = idx;
        }
    }

    if (selectedIdx >= 0) SendMessageW(hCombo, CB_SETCURSEL, (WPARAM)selectedIdx, 0);
}

static bool find_display_mode_for_resolution(int w, int h, DEVMODEW* outDm) {
    if (!outDm || w <= 0 || h <= 0) return false;

    DEVMODEW cur;
    ZeroMemory(&cur, sizeof(cur));
    cur.dmSize = sizeof(cur);
    bool haveCur = EnumDisplaySettingsW(NULL, ENUM_CURRENT_SETTINGS, &cur) ? true : false;

    DEVMODEW fallback;
    ZeroMemory(&fallback, sizeof(fallback));
    bool haveFallback = false;

    for (DWORD i = 0; ; i++) {
        DEVMODEW dm;
        ZeroMemory(&dm, sizeof(dm));
        dm.dmSize = sizeof(dm);
        if (!EnumDisplaySettingsW(NULL, i, &dm)) break;
        if ((int)dm.dmPelsWidth != w || (int)dm.dmPelsHeight != h) continue;
        if (haveCur && cur.dmBitsPerPel > 0 && dm.dmBitsPerPel > 0 && dm.dmBitsPerPel != cur.dmBitsPerPel) continue;

        if (!haveFallback) {
            fallback = dm;
            haveFallback = true;
        }

        if (haveCur && dm.dmDisplayFrequency == cur.dmDisplayFrequency) {
            *outDm = dm;
            return true;
        }
    }

    if (haveFallback) {
        *outDm = fallback;
        return true;
    }
    return false;
}

static bool apply_system_resolution_for_launch(const wchar_t* setting,
                                               DEVMODEW* oldMode,
                                               bool* oldModeValid,
                                               bool* changedMode) {
    if (oldModeValid) *oldModeValid = false;
    if (changedMode) *changedMode = false;

    int w = 0, h = 0;
    if (!parse_resolution_setting(setting, &w, &h)) return true; // Native

    DEVMODEW current;
    ZeroMemory(&current, sizeof(current));
    current.dmSize = sizeof(current);
    if (!EnumDisplaySettingsW(NULL, ENUM_CURRENT_SETTINGS, &current)) {
        ui_log_line(L"ERROR: failed to read current display mode.");
        return false;
    }

    if (oldMode) *oldMode = current;
    if (oldModeValid) *oldModeValid = true;

    if ((int)current.dmPelsWidth == w && (int)current.dmPelsHeight == h) {
        ui_log_line(L"Selected system resolution already active.");
        return true;
    }

    DEVMODEW target;
    if (!find_display_mode_for_resolution(w, h, &target)) {
        wchar_t msg[256];
        _snwprintf(msg, ARRAYSIZE(msg), L"ERROR: selected system resolution is not available: %dx%d", w, h);
        msg[ARRAYSIZE(msg)-1] = 0;
        ui_log_line(msg);
        return false;
    }

    wchar_t msg[256];
    _snwprintf(msg, ARRAYSIZE(msg), L"Changing system resolution: %lux%lu -> %dx%d",
               (unsigned long)current.dmPelsWidth,
               (unsigned long)current.dmPelsHeight,
               w, h);
    msg[ARRAYSIZE(msg)-1] = 0;
    ui_log_line(msg);

    LONG r = ChangeDisplaySettingsW(&target, CDS_FULLSCREEN);
    if (r != DISP_CHANGE_SUCCESSFUL) {
        _snwprintf(msg, ARRAYSIZE(msg), L"ERROR: ChangeDisplaySettingsW failed (%ld).", (long)r);
        msg[ARRAYSIZE(msg)-1] = 0;
        ui_log_line(msg);
        return false;
    }

    if (changedMode) *changedMode = true;
    return true;
}

static void restore_system_resolution_after_launch(const DEVMODEW* oldMode,
                                                   bool oldModeValid,
                                                   bool changedMode) {
    if (!oldModeValid || !changedMode || !oldMode) return;

    wchar_t msg[256];
    _snwprintf(msg, ARRAYSIZE(msg), L"Restoring system resolution: %lux%lu",
               (unsigned long)oldMode->dmPelsWidth,
               (unsigned long)oldMode->dmPelsHeight);
    msg[ARRAYSIZE(msg)-1] = 0;
    ui_log_line(msg);

    LONG r = ChangeDisplaySettingsW((DEVMODEW*)oldMode, 0);
    if (r != DISP_CHANGE_SUCCESSFUL) {
        _snwprintf(msg, ARRAYSIZE(msg), L"WARNING: failed to restore system resolution (%ld).", (long)r);
        msg[ARRAYSIZE(msg)-1] = 0;
        ui_log_line(msg);
    }
}

static void compute_nox_resolution(int dispW, int dispH, int* outW, int* outH) {
    int w = 1024, h = 768;
    if (dispW > 0 && dispH > 0) {
        double aspect = (double)dispW / (double)dispH;

        // 4:3 ~ 1.3333
        if (aspect > 1.30 && aspect < 1.36) {
            if (dispW < 1024 && dispH < 768) { w = dispW; h = dispH; }
            else { w = 1024; h = 768; }
        }
        // 1:1 ~ 1.0
        else if (aspect > 0.98 && aspect < 1.02) {
            if (dispW < 768) { w = dispW; h = dispW; }
            else { w = 768; h = 768; }
        }
        // widescreen
        else {
            w = (dispW > 1024) ? 1024 : dispW;
            if (w < 1) w = 1024;
            double hh = (double)w / aspect;
            h = (int)(hh + 0.0); // floor
            if (h > 768) h = 768;
            if (h < 1) h = 768;
        }
    }
    // absolute limits
    if (w > 1024) w = 1024;
    if (h > 768) h = 768;
    if (w < 1) w = 1024;
    if (h < 1) h = 768;
    *outW = w; *outH = h;
}

// -------------------------
// nox.cfg copy + patch
// -------------------------
static bool copy_file_overwrite(const wchar_t* src, const wchar_t* dst) {
    ensure_parent_dir(dst);
    return CopyFileW(src, dst, FALSE) ? true : false;
}

static bool patch_nox_cfg(const wchar_t* cfgPath, int w, int h, int bits, int fullscreen) {
    FILE* f = _wfopen(cfgPath, L"rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz < 0) { fclose(f); return false; }
    char* buf = (char*)malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return false; }
    size_t rd = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[rd] = 0;

    // decode as UTF-8 else ANSI
    int wneed = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, buf, (int)rd, NULL, 0);
    UINT cp = CP_UTF8;
    if (wneed <= 0) { cp = CP_ACP; wneed = MultiByteToWideChar(cp, 0, buf, (int)rd, NULL, 0); }
    wchar_t* wbuf = (wchar_t*)malloc(((size_t)wneed + 1) * sizeof(wchar_t));
    if (!wbuf) { free(buf); return false; }
    MultiByteToWideChar(cp, 0, buf, (int)rd, wbuf, wneed);
    wbuf[wneed] = 0;
    free(buf);

    // line-based patch
    wchar_t videoLine[128];
    wchar_t fullLine[64];
    _snwprintf(videoLine, ARRAYSIZE(videoLine), L"VideoMode = %d %d %d", w, h, bits);
    _snwprintf(fullLine, ARRAYSIZE(fullLine), L"Fullscreen = %d", fullscreen ? 1 : 0);

    bool foundVideo = false, foundFull = false;

    // Build output into dynamic buffer
    size_t outCap = (size_t)wneed + 1024;
    wchar_t* out = (wchar_t*)malloc(outCap * sizeof(wchar_t));
    if (!out) { free(wbuf); return false; }
    out[0] = 0;

    wchar_t* p = wbuf;
    while (*p) {
        wchar_t* lineStart = p;
        wchar_t* nl = wcschr(p, L'\n');
        if (nl) { *nl = 0; p = nl + 1; }
        else { p = lineStart + wcslen(lineStart); }

        strip_trailing_crlf(lineStart);

        // copy line, possibly replaced
        wchar_t* tmpLine = wcsdup_heap(lineStart);
        if (!tmpLine) { free(out); free(wbuf); return false; }
        wcs_trim_inplace(tmpLine);

        const wchar_t* writeLine = lineStart;
        if (wcs_istarts(tmpLine, L"videomode")) {
            writeLine = videoLine;
            foundVideo = true;
        } else if (wcs_istarts(tmpLine, L"fullscreen")) {
            writeLine = fullLine;
            foundFull = true;
        }
        free(tmpLine);

        size_t needAdd = wcslen(writeLine) + 2 + wcslen(out) + 1;
        if (needAdd > outCap) {
            outCap = needAdd + 2048;
            wchar_t* nout = (wchar_t*)realloc(out, outCap * sizeof(wchar_t));
            if (!nout) { free(out); free(wbuf); return false; }
            out = nout;
        }
        wcscat(out, writeLine);
        wcscat(out, L"\r\n");
    }

    if (!foundVideo) {
        size_t needAdd = wcslen(out) + wcslen(videoLine) + 4;
        if (needAdd > outCap) {
            outCap = needAdd + 1024;
            wchar_t* nout = (wchar_t*)realloc(out, outCap * sizeof(wchar_t));
            if (!nout) { free(out); free(wbuf); return false; }
            out = nout;
        }
        wcscat(out, videoLine);
        wcscat(out, L"\r\n");
    }
    if (!foundFull) {
        size_t needAdd = wcslen(out) + wcslen(fullLine) + 4;
        if (needAdd > outCap) {
            outCap = needAdd + 1024;
            wchar_t* nout = (wchar_t*)realloc(out, outCap * sizeof(wchar_t));
            if (!nout) { free(out); free(wbuf); return false; }
            out = nout;
        }
        wcscat(out, fullLine);
        wcscat(out, L"\r\n");
    }

    free(wbuf);

    // Write back as ASCII (your batch used ASCII). We'll write UTF-8 without BOM to be safe.
    int need8 = WideCharToMultiByte(CP_UTF8, 0, out, -1, NULL, 0, NULL, NULL);
    if (need8 <= 1) { free(out); return false; }
    char* out8 = (char*)malloc((size_t)need8);
    if (!out8) { free(out); return false; }
    WideCharToMultiByte(CP_UTF8, 0, out, -1, out8, need8, NULL, NULL);

    FILE* wf = _wfopen(cfgPath, L"wb");
    if (!wf) { free(out8); free(out); return false; }
    fwrite(out8, 1, (size_t)(need8 - 1), wf);
    fclose(wf);

    free(out8);
    free(out);
    return true;
}

// -------------------------
// Environment block for CreateProcess (merge current env + overrides)
// -------------------------
typedef struct EnvKV {
    wchar_t* k;
    wchar_t* v;
} EnvKV;

static void envkv_free(EnvKV* e) {
    if (!e) return;
    free(e->k); free(e->v);
}

typedef struct EnvSaved {
    wchar_t* k;
    wchar_t* oldv;   // NULL if not present
    bool had_old;
} EnvSaved;

static void envsaved_free(EnvSaved* s) {
    if (!s) return;
    free(s->k);
    free(s->oldv);
    s->k = NULL;
    s->oldv = NULL;
    s->had_old = false;
}

static bool get_env_heap(const wchar_t* key, wchar_t** outVal) {
    *outVal = NULL;
    DWORD need = GetEnvironmentVariableW(key, NULL, 0);
    if (need == 0) {
        // not present (or other error); treat as not present
        return false;
    }
    wchar_t* buf = (wchar_t*)malloc((size_t)need * sizeof(wchar_t));
    if (!buf) return false;
    DWORD got = GetEnvironmentVariableW(key, buf, need);
    if (got == 0 || got >= need) { free(buf); return false; }
    *outVal = buf;
    return true;
}

static bool save_and_set_env(EnvSaved* s, const wchar_t* key, const wchar_t* val) {
    memset(s, 0, sizeof(*s));
    s->k = wcsdup_heap(key);
    if (!s->k) return false;

    wchar_t* oldv = NULL;
    bool had = get_env_heap(key, &oldv);
    s->had_old = had;
    s->oldv = oldv; // may be NULL

    if (!SetEnvironmentVariableW(key, val ? val : L"")) {
        envsaved_free(s);
        return false;
    }
    return true;
}

static void restore_env(const EnvSaved* s) {
    if (!s || !s->k) return;
    if (s->had_old) {
        SetEnvironmentVariableW(s->k, s->oldv ? s->oldv : L"");
    } else {
        SetEnvironmentVariableW(s->k, NULL); // delete var
    }
}

static wchar_t* get_current_env_block(void) {
    // returns heap block in Windows env-block format (double-null terminated)
    LPWCH env = GetEnvironmentStringsW();
    if (!env) return NULL;
    // compute size
    size_t n = 0;
    for (LPWCH p = env; *p; ) {
        size_t len = wcslen(p);
        n += len + 1;
        p += len + 1;
    }
    n += 1; // final null
    wchar_t* out = (wchar_t*)malloc(n * sizeof(wchar_t));
    if (!out) { FreeEnvironmentStringsW(env); return NULL; }
    memcpy(out, env, n * sizeof(wchar_t));
    FreeEnvironmentStringsW(env);
    return out;
}

typedef struct EnvRaw {
    wchar_t* s; // full raw entry, e.g. L"=C:=C:\\Windows"
} EnvRaw;

static void envraw_free(EnvRaw* e) {
    if (!e) return;
    free(e->s);
    e->s = NULL;
}

static bool split_env_block(wchar_t* block,
                            EnvKV** outArr, size_t* outN,
                            EnvRaw** outRaw, size_t* outRawN)
{
    *outArr = NULL; *outN = 0;
    *outRaw = NULL; *outRawN = 0;

    size_t count = 0;
    for (wchar_t* p = block; *p; ) { count++; p += wcslen(p) + 1; }

    EnvKV* arr = (EnvKV*)calloc(count ? count : 1, sizeof(EnvKV));
    EnvRaw* raw = (EnvRaw*)calloc(count ? count : 1, sizeof(EnvRaw));
    if (!arr || !raw) { free(arr); free(raw); return false; }

    size_t idx = 0;
    size_t ridx = 0;

    for (wchar_t* p = block; *p; ) {
        wchar_t* entry = p;
        size_t len = wcslen(entry);

        if (entry[0] == L'=') {
            // Preserve special entries like "=C:=C:\path"
            raw[ridx].s = wcsdup_heap(entry);
            if (!raw[ridx].s) {
                for (size_t j = 0; j < idx; j++) envkv_free(&arr[j]);
                for (size_t j = 0; j < ridx; j++) envraw_free(&raw[j]);
                free(arr); free(raw);
                return false;
            }
            ridx++;
        } else {
            wchar_t* eq = wcschr(entry, L'=');
            if (eq && eq != entry) {
                *eq = 0;
                arr[idx].k = wcsdup_heap(entry);
                arr[idx].v = wcsdup_heap(eq + 1);
                *eq = L'=';
                if (!arr[idx].k || !arr[idx].v) {
                    for (size_t j = 0; j <= idx; j++) envkv_free(&arr[j]);
                    for (size_t j = 0; j < ridx; j++) envraw_free(&raw[j]);
                    free(arr); free(raw);
                    return false;
                }
                idx++;
            }
        }

        p += len + 1;
    }

    *outArr = arr;
    *outN = idx;
    *outRaw = raw;
    *outRawN = ridx;
    return true;
}

static int __cdecl envkv_cmp_ci(const void* a, const void* b) {
    const EnvKV* A = (const EnvKV*)a;
    const EnvKV* B = (const EnvKV*)b;
    if (!A->k && !B->k) return 0;
    if (!A->k) return -1;
    if (!B->k) return 1;
    return _wcsicmp(A->k, B->k);
}

static int envkv_find(EnvKV* arr, size_t n, const wchar_t* key) {
    for (size_t i = 0; i < n; i++) {
        if (arr[i].k && _wcsicmp(arr[i].k, key) == 0) return (int)i;
    }
    return -1;
}

static wchar_t* build_env_block_with_overrides(const IniDoc* doc, const VarList* vars) {
    // Start with current env, then override with [env] for keys present in vars or any [env] keys.
    // Spec: preserve unknown keys; runtime: apply all [env] keys.
    wchar_t* baseBlock = get_current_env_block();
    if (!baseBlock) return NULL;

    // Parse base env into KV pairs, but ALSO preserve raw "=X:=..." entries (per-drive CWD).
    // We keep them verbatim and re-emit them unchanged at the start of the env block.
    wchar_t** raw = NULL; size_t rn = 0, rcap = 0;

    EnvKV* base = NULL; size_t bn = 0;

    // Count entries for a small reserve
    for (wchar_t* p = baseBlock; *p; ) { rn++; p += wcslen(p) + 1; }
    rcap = rn ? rn : 1;
    raw = (wchar_t**)calloc(rcap, sizeof(wchar_t*));
    rn = 0;

    // Split baseBlock in-place
    size_t kvCap = 0;
    {
        // worst-case count
        size_t count = 0;
        for (wchar_t* p = baseBlock; *p; ) { count++; p += wcslen(p) + 1; }
        kvCap = count ? count : 1;
        base = (EnvKV*)calloc(kvCap, sizeof(EnvKV));
        if (!base || !raw) { free(base); free(raw); free(baseBlock); return NULL; }

        for (wchar_t* p = baseBlock; *p; ) {
            wchar_t* entry = p;
            size_t len = wcslen(entry);

            if (entry[0] == L'=') {
                // Preserve special entries like "=C:=C:\path"
                if (rn < rcap) {
                    raw[rn] = wcsdup_heap(entry);
                    if (raw[rn]) rn++;
                }
            } else {
                wchar_t* eq = wcschr(entry, L'=');
                if (eq && eq != entry) {
                    *eq = 0;
                    base[bn].k = wcsdup_heap(entry);
                    base[bn].v = wcsdup_heap(eq + 1);
                    *eq = L'=';
                    if (base[bn].k && base[bn].v) {
                        bn++;
                    } else {
                        // OOM: cleanup and bail
                        for (size_t i = 0; i < bn; i++) envkv_free(&base[i]);
                        for (size_t i = 0; i < rn; i++) free(raw[i]);
                        free(base);
                        free(raw);
                        free(baseBlock);
                        return NULL;
                    }
                }
            }

            p += len + 1;
        }
    }

    // Apply all [env] keys in ini doc (not only vars)
    for (size_t i = 0; i < doc->count; i++) {
        const IniLine* l = &doc->lines[i];
        if (!l->is_kv || !l->section || !wcs_ieq(l->section, L"env")) continue;
        if (!l->key) continue;
        const wchar_t* val = l->value ? l->value : L"";
        int idx = envkv_find(base, bn, l->key);
        if (idx >= 0) {
            free(base[idx].v);
            base[idx].v = wcsdup_heap(val);
        } else {
            // append
            EnvKV* nb = (EnvKV*)realloc(base, (bn + 1) * sizeof(EnvKV));
            if (!nb) continue;
            base = nb;
            base[bn].k = wcsdup_heap(l->key);
            base[bn].v = wcsdup_heap(val);
            bn++;
        }
    }

    // Also apply current UI edits (vars->curval) to ensure latest values get applied even if not saved
    for (size_t i = 0; i < vars->n; i++) {
        const VarSpec* s = &vars->v[i];
        if (!s->name) continue;
        const wchar_t* val = s->curval ? s->curval : L"";
        int idx = envkv_find(base, bn, s->name);
        if (idx >= 0) {
            free(base[idx].v);
            base[idx].v = wcsdup_heap(val);
        } else {
            EnvKV* nb = (EnvKV*)realloc(base, (bn + 1) * sizeof(EnvKV));
            if (!nb) continue;
            base = nb;
            base[bn].k = wcsdup_heap(s->name);
            base[bn].v = wcsdup_heap(val);
            bn++;
        }
    }

    // Sort env entries (recommended for CreateProcess environment blocks)
    qsort(base, bn, sizeof(EnvKV), envkv_cmp_ci);

    // Compute block size (raw entries + kv entries + final NUL)
    size_t total = 1; // final null

    for (size_t i = 0; i < rn; i++) {
        if (!raw[i]) continue;
        total += wcslen(raw[i]) + 1;
    }

    for (size_t i = 0; i < bn; i++) {
        if (!base[i].k || !base[i].v) continue;
        total += wcslen(base[i].k) + 1 + wcslen(base[i].v) + 1;
    }

    wchar_t* out = (wchar_t*)malloc(total * sizeof(wchar_t));
    if (!out) {
        for (size_t i = 0; i < bn; i++) envkv_free(&base[i]);
        for (size_t i = 0; i < rn; i++) free(raw[i]);
        free(base);
        free(raw);
        free(baseBlock);
        return NULL;
    }

    wchar_t* w = out;

    // Emit preserved raw "=X:=..." entries first (unchanged)
    for (size_t i = 0; i < rn; i++) {
        if (!raw[i]) continue;
        size_t len = wcslen(raw[i]);
        memcpy(w, raw[i], len * sizeof(wchar_t)); w += len;
        *w++ = 0;
    }

    // Emit sorted KEY=VALUE entries
    for (size_t i = 0; i < bn; i++) {
        if (!base[i].k || !base[i].v) continue;
        size_t klen = wcslen(base[i].k);
        size_t vlen = wcslen(base[i].v);
        memcpy(w, base[i].k, klen * sizeof(wchar_t)); w += klen;
        *w++ = L'=';
        memcpy(w, base[i].v, vlen * sizeof(wchar_t)); w += vlen;
        *w++ = 0;
    }
    *w++ = 0;

    for (size_t i = 0; i < bn; i++) envkv_free(&base[i]);
    for (size_t i = 0; i < rn; i++) free(raw[i]);
    free(base);
    free(raw);
    free(baseBlock);
    return out;
}

// -------------------------
// Controls: apply clamp/revert policy into vars->curval
// -------------------------
static void var_update_from_ctrl(VarSpec* s) {
    if (!s || !s->hCtrl) return;

    if (s->type == VAR_BOOL) {
        LRESULT checked = SendMessageW(s->hCtrl, BM_GETCHECK, 0, 0);
        const wchar_t* v = (checked == BST_CHECKED) ? (s->true_val ? s->true_val : L"1")
                                                    : (s->false_val ? s->false_val : L"0");
        free(s->curval);
        s->curval = wcsdup_heap(v);
        return;
    }

    // For edit controls
    int len = GetWindowTextLengthW(s->hCtrl);
    wchar_t* buf = (wchar_t*)malloc(((size_t)len + 1) * sizeof(wchar_t));
    if (!buf) return;
    GetWindowTextW(s->hCtrl, buf, len + 1);
    wcs_trim_inplace(buf);

    bool ok = true;
    wchar_t outVal[256] = {0};

    if (s->type == VAR_INT) {
        int iv;
        if (!parse_int32(buf, &iv)) ok = false;
        if (ok) {
            double dv = (double)iv;
            if (s->has_min && dv < s->minv) dv = s->minv;
            if (s->has_max && dv > s->maxv) dv = s->maxv;
            // round to int for int type
            int clamped = (int)dv;
            _snwprintf(outVal, ARRAYSIZE(outVal), L"%d", clamped);
        }
    } else if (s->type == VAR_FLOAT) {
        double fv;
        if (!parse_double(buf, &fv)) ok = false;
        if (ok) {
            if (s->has_min && fv < s->minv) fv = s->minv;
            if (s->has_max && fv > s->maxv) fv = s->maxv;
            // default formatting
            _snwprintf(outVal, ARRAYSIZE(outVal), L"%.6g", fv);
        }
    } else { // string
        // apply maxlen
        if (s->maxlen > 0 && (int)wcslen(buf) > s->maxlen) buf[s->maxlen] = 0;
        wcsncpy(outVal, buf, ARRAYSIZE(outVal)-1);
        outVal[ARRAYSIZE(outVal)-1]=0;
    }

    if (!ok) {
        // revert to default (schema default if present else neutral)
        const wchar_t* def = s->defval ? s->defval : L"";
        if (s->type == VAR_INT && (!def || !*def)) def = L"0";
        if (s->type == VAR_FLOAT && (!def || !*def)) def = L"0.0";
        if (s->type == VAR_STRING && !def) def = L"";
        wcsncpy(outVal, def, ARRAYSIZE(outVal)-1);
        outVal[ARRAYSIZE(outVal)-1]=0;
    }

    // Update control to clamped/reverted value
    SetWindowTextW(s->hCtrl, outVal);
    free(s->curval);
    s->curval = wcsdup_heap(outVal);

    free(buf);
}

// -------------------------
// Layout: metrics
// -------------------------
typedef struct Metrics {
    int pad;
    int gap;
    int bottom_h;
    int btn_w;
    int btn_h;
    int min_w;
    int min_h;
    int label_w;
    int row_h_bool;
    int row_h_edit;
    int row_gap;
} Metrics;

static int dpi_scale(HWND hwnd, int px96) {
    UINT dpi = 96;
    HMODULE u32 = GetModuleHandleW(L"user32.dll");
    if (u32) {
        typedef UINT (WINAPI *GetDpiForWindowFn)(HWND);
        GetDpiForWindowFn fn = (GetDpiForWindowFn)GetProcAddress(u32, "GetDpiForWindow");
        if (fn) dpi = fn(hwnd);
    }
    return MulDiv(px96, (int)dpi, 96);
}

static Metrics metrics_get(HWND hwnd) {
    Metrics m;
    m.pad = dpi_scale(hwnd, 12);
    m.gap = dpi_scale(hwnd, 10);
    m.bottom_h = dpi_scale(hwnd, 44);
    m.btn_w = dpi_scale(hwnd, 110);
    m.btn_h = dpi_scale(hwnd, 28);
    m.min_w = dpi_scale(hwnd, 760);
    m.min_h = dpi_scale(hwnd, 520);
    m.label_w = dpi_scale(hwnd, 180);
    m.row_h_bool = dpi_scale(hwnd, 24);
    m.row_h_edit = dpi_scale(hwnd, 28);
    m.row_gap = dpi_scale(hwnd, 8);
    return m;
}

// -------------------------
// Arch detection + tool paths
// -------------------------
static void detect_arch(AppState* a) {
    // Detect native OS arch (not the current process arch)
    SYSTEM_INFO si;
    ZeroMemory(&si, sizeof(si));
    GetNativeSystemInfo(&si);

    // DEVICE_ARCH: x86, x86_64, aarch64
    wcscpy_s(a->deviceArch, ARRAYSIZE(a->deviceArch), L"x86");

    switch (si.wProcessorArchitecture) {
        case PROCESSOR_ARCHITECTURE_AMD64:
            wcscpy_s(a->deviceArch, ARRAYSIZE(a->deviceArch), L"x86_64");
            break;
        case PROCESSOR_ARCHITECTURE_ARM64:
            wcscpy_s(a->deviceArch, ARRAYSIZE(a->deviceArch), L"aarch64");
            break;
        case PROCESSOR_ARCHITECTURE_INTEL:
        default:
            wcscpy_s(a->deviceArch, ARRAYSIZE(a->deviceArch), L"x86");
            break;
    }

    // RUN_ARCH mapping (your existing policy)
    // - x86_64 host runs i386 build
    // - arm64 host runs armhf build
    wcscpy_s(a->runArch, ARRAYSIZE(a->runArch), a->deviceArch);

    if (_wcsicmp(a->runArch, L"x86_64") == 0) wcscpy_s(a->runArch, ARRAYSIZE(a->runArch), L"i386");
    if (_wcsicmp(a->runArch, L"amd64")  == 0) wcscpy_s(a->runArch, ARRAYSIZE(a->runArch), L"i386");
    if (_wcsicmp(a->runArch, L"aarch64")== 0) wcscpy_s(a->runArch, ARRAYSIZE(a->runArch), L"armhf");
}

// Find first matching installer in gamefiles\setup_nox*.exe
static bool find_installer(const wchar_t* srcDirAbs, wchar_t* outPath, size_t cap) {
    wchar_t pat[MAX_PATH * 4];
    path_join(pat, ARRAYSIZE(pat), srcDirAbs, L"setup_nox*.exe");
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(pat, &fd);
    if (h == INVALID_HANDLE_VALUE) return false;
    bool ok = false;
    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            path_join(outPath, cap, srcDirAbs, fd.cFileName);
            ok = true;
            break;
        }
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    return ok;
}


static void log_last_error(const wchar_t* prefix)
{
    DWORD e = GetLastError();
    wchar_t msg[512];
    wchar_t sys[512];

    FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                   NULL, e, 0, sys, ARRAYSIZE(sys), NULL);

    _snwprintf(msg, ARRAYSIZE(msg), L"%s (GetLastError=%lu) %s", prefix, (unsigned long)e, sys);
    msg[ARRAYSIZE(msg)-1]=0;
    ui_log_line(msg);
}

// -------------------------
// Game launch (redirect stdout/stderr to log.txt and exit launcher)
// -------------------------
static void path_dirname(wchar_t* out, size_t cap, const wchar_t* fullpath)
{
    if (!out || cap == 0) return;
    out[0] = 0;
    if (!fullpath) return;

    wcsncpy(out, fullpath, cap - 1);
    out[cap - 1] = 0;

    wchar_t* b1 = wcsrchr(out, L'\\');
    wchar_t* b2 = wcsrchr(out, L'/');
    wchar_t* cut = b1;
    if (b2 && (!cut || b2 > cut)) cut = b2;

    if (cut) *cut = 0;
    else out[0] = 0;
}

static bool launch_game_and_exit(const wchar_t* exePath, const wchar_t* workdir, wchar_t* envBlock, const wchar_t* logPathAbs) {
    (void)logPathAbs;

    wchar_t cmd[4096];
    _snwprintf(cmd, ARRAYSIZE(cmd), L"\"%s\"", exePath);
    cmd[ARRAYSIZE(cmd)-1]=0;

    HANDLE hOutR = NULL, hOutW = NULL;
    if (!create_pipe_inheritable(&hOutR, &hOutW)) {
        log_last_error(L"create_pipe_inheritable failed");
        return false;
    }

    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    ZeroMemory(&pi, sizeof(pi));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput  = GetStdHandle(STD_INPUT_HANDLE);
    si.hStdOutput = hOutW;
    si.hStdError  = hOutW;

    DWORD flags = 0;
    LPVOID envp = NULL;

    if (envBlock) {
        flags |= CREATE_UNICODE_ENVIRONMENT;
        envp = envBlock;
    }

    BOOL ok = CreateProcessW(
        exePath,
        cmd,
        NULL, NULL,
        TRUE,   // child must inherit pipe write end
        flags,
        envp,
        workdir,
        &si,
        &pi
    );

    CloseHandle(hOutW);
    hOutW = NULL;

    if (!ok) {
        CloseHandle(hOutR);
        log_last_error(L"CreateProcessW failed");
        return false;
    }

    ui_log_line(L"CreateProcessW succeeded.");

    HANDLE hReader = CreateThread(NULL, 0, reader_thread, (LPVOID)hOutR, 0, NULL);
    if (!hReader) {
        log_last_error(L"Failed to create game log reader thread");
    }

    DWORD wait = WaitForSingleObject(pi.hProcess, INFINITE);

    if (wait == WAIT_OBJECT_0) {
        DWORD code = 0;
        GetExitCodeProcess(pi.hProcess, &code);

        wchar_t msg[256];
        _snwprintf(msg, ARRAYSIZE(msg),
                   L"Game exited (exit code %lu).",
                   (unsigned long)code);
        msg[ARRAYSIZE(msg)-1]=0;
        ui_log_line(msg);
    } else {
        log_last_error(L"WaitForSingleObject on child process failed");
    }

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    CloseHandle(hOutR);
    if (hReader) {
        WaitForSingleObject(hReader, 2000);
        CloseHandle(hReader);
    }

    return wait == WAIT_OBJECT_0;
}

// -------------------------
// Worker pipeline
// -------------------------
typedef struct WorkerArgs {
    HWND hwnd;
} WorkerArgs;

static bool delete_file_quiet(const wchar_t* p) {
    if (!file_exists(p)) return true;
    return DeleteFileW(p) ? true : false;
}

static bool copy_template_if_missing(const wchar_t* templateCfgAbs, const wchar_t* dstCfgAbs) {
    if (file_exists(dstCfgAbs)) return true;
    return CopyFileW(templateCfgAbs, dstCfgAbs, FALSE) ? true : false;
}

static bool write_text_file_utf8(const wchar_t* path, const char* text) {
    ensure_parent_dir(path);
    FILE* f = _wfopen(path, L"wb");
    if (!f) return false;
    if (text && *text) fwrite(text, 1, strlen(text), f);
    fclose(f);
    return true;
}

static DWORD WINAPI worker_thread(LPVOID param) {
    WorkerArgs* wa = (WorkerArgs*)param;
    HWND hwnd = wa ? wa->hwnd : NULL;
    free(wa);

    AppState* a = &g_app;
    ui_log_line(L"Worker thread started.");

    a->extraction_succeeded_this_run = false;

    // 1) Load current control values (clamp/revert) into vars->curval
    for (size_t i = 0; i < a->vars.n; i++) {
        var_update_from_ctrl(&a->vars.v[i]);
    }
    resolution_update_from_ctrl(&a->cfg);

    // 2) Update ini doc [env] with current values (preserve unknown)
    for (size_t i = 0; i < a->vars.n; i++) {
        VarSpec* s = &a->vars.v[i];
        if (!s->name) continue;
        if (!ini_set_env_value_preserve(&a->ini, s->name, s->curval ? s->curval : L"")) {
            ui_log_line(L"Failed to update INI env values.");
        }
    }
    ini_set_value_preserve(&a->ini, L"launcher", L"system_resolution", a->cfg.system_resolution);

    // Save INI (preserve)
    if (!ini_save_preserve(a->iniPath, &a->ini)) {
        ui_log_line(L"WARNING: Failed to save launch-nox-decomp.ini (continuing).");
    }

    // 3) Ensure dirs
    ensure_dir(a->assetsDirAbs);
    ensure_dir(a->dialogDirAbs);

    // 4) If gamedata missing -> extract
    if (!file_exists(a->neededAbs)) {
        ui_set_status(L"Preparing game data...");
        ui_log_line(L"Game data missing (gamedata.bin).");
        ui_log_line(L"Please supply either a GOG setup or Installed files.");

        if (a->chosenInstallAppAbs[0]) {
            ui_log_line(L"Copying files from installed game directory...");

            if (!copy_installed_files_into_local_app(a->chosenInstallAppAbs, a->assetsDirAbs)) {
                ui_log_line(L"ERROR: failed to copy files from installed game directory.");
                PostMessageW(hwnd, WM_APP_DONE, 0, 0);
                return 0;
            }

            if (!file_exists(a->neededAbs)) {
                ui_log_line(L"ERROR: copied files but gamedata.bin is still missing.");
                PostMessageW(hwnd, WM_APP_DONE, 0, 0);
                return 0;
            }

            ui_log_line(L"Installed files copied successfully.");
        } else {
            wchar_t installer[MAX_PATH * 4] = {0};

            if (a->chosenInstallerAbs[0]) {
                wcsncpy(installer, a->chosenInstallerAbs, ARRAYSIZE(installer) - 1);
                installer[ARRAYSIZE(installer) - 1] = 0;
                ui_log_line(L"Using GOG setup selected by the user.");
            } else if (find_installer(a->srcDirAbs, installer, ARRAYSIZE(installer))) {
                ui_log_line(L"Found GOG setup in gamefiles.");
            } else {
                ui_log_line(L"No GOG setup available.");
                ui_log_line(L"Please supply either a GOG setup or Installed files.");
                PostMessageW(hwnd, WM_APP_DONE, 0, 0);
                return 0;
            }

            ui_log_line(L"Running innoextract...");

            if (!file_exists(a->innoextractAbs)) {
                ui_log_line(L"ERROR: innoextract missing.");
                PostMessageW(hwnd, WM_APP_DONE, 0, 0);
                return 0;
            }

            wchar_t args[8192];
            _snwprintf(args, ARRAYSIZE(args), L"\"%s\" -d \"%s\"", installer, a->srcDirAbs);
            args[ARRAYSIZE(args)-1]=0;

            ui_set_status(L"Extracting...");
            ProcRunResult r = run_process_capture(a->innoextractAbs, args, a->srcDirAbs);
            if (!r.started || r.exit_code != 0 || !file_exists(a->neededAbs)) {
                ui_log_line(L"ERROR: innoextract failed or gamedata.bin still missing.");
                PostMessageW(hwnd, WM_APP_DONE, 0, 0);
                return 0;
            }

            a->extraction_succeeded_this_run = true;
            ui_log_line(L"innoextract succeeded.");

            // Delete launcher-local gamefiles/app/nox.cfg only after successful extraction
            {
                wchar_t appCfg[MAX_PATH * 4];
                path_join(appCfg, ARRAYSIZE(appCfg), a->assetsDirAbs, L"nox.cfg");
                if (file_exists(appCfg)) {
                    if (DeleteFileW(appCfg)) ui_log_line(L"Deleted gamefiles/app/nox.cfg after extraction.");
                    else ui_log_line(L"WARNING: failed to delete gamefiles/app/nox.cfg.");
                }
            }
        }
    } else {
        ui_log_line(L"Game data present.");
    }

    // 5) Convert dialog audio if enabled and marker missing
    if (a->cfg.convert_dialog) {
        if (!file_exists(a->markerAbs)) {
            // Only if dialog dir exists
            if (dir_exists(a->dialogDirAbs) && file_exists(a->ffmpegAbs)) {
                ui_set_status(L"Converting dialog audio...");
                ui_log_line(L"Converting dialog WAV files...");

                wchar_t pat[MAX_PATH * 4];
                path_join(pat, ARRAYSIZE(pat), a->dialogDirAbs, L"*.wav");
                WIN32_FIND_DATAW fd;
                HANDLE h = FindFirstFileW(pat, &fd);
                if (h != INVALID_HANDLE_VALUE) {
                    do {
                        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;

                        wchar_t inWav[MAX_PATH * 4];
                        path_join(inWav, ARRAYSIZE(inWav), a->dialogDirAbs, fd.cFileName);

                        wchar_t tmpWav[MAX_PATH * 4];
                        _snwprintf(tmpWav, ARRAYSIZE(tmpWav), L"%s.tmp", inWav);
                        tmpWav[ARRAYSIZE(tmpWav)-1]=0;

                        wchar_t msg[1024];
                        _snwprintf(msg, ARRAYSIZE(msg), L"Converting %s", fd.cFileName);
                        msg[ARRAYSIZE(msg)-1]=0;
                        ui_log_line(msg);

                        wchar_t fargs[8192];
                        _snwprintf(fargs, ARRAYSIZE(fargs),
                                   L"-y -hide_banner -nostdin -v error -nostats "
                                   L"-i \"%s\" -ac 1 -ar 22050 -c:a pcm_s16le -f wav \"%s\"",
                                   inWav, tmpWav);
                        fargs[ARRAYSIZE(fargs)-1]=0;

                        ProcRunResult fr = run_process_capture(a->ffmpegAbs, fargs, a->dialogDirAbs);
                        if (!fr.started || fr.exit_code != 0 || !file_exists(tmpWav)) {
                            ui_log_line(L"ERROR: ffmpeg conversion failed.");
                            FindClose(h);
                            PostMessageW(hwnd, WM_APP_DONE, 0, 0);
                            return 0;
                        }

                        // Replace original safely (atomic-ish)
                        if (!MoveFileExW(tmpWav, inWav, MOVEFILE_REPLACE_EXISTING)) {
                            ui_log_line(L"ERROR: failed to replace WAV with converted output.");
                            // Best effort cleanup of tmp
                            DeleteFileW(tmpWav);
                            FindClose(h);
                            PostMessageW(hwnd, WM_APP_DONE, 0, 0);
                            return 0;
                        }
                    } while (FindNextFileW(h, &fd));
                    FindClose(h);
                } else {
                    ui_log_line(L"No dialog WAV files found.");
                }

                // Write marker
                ensure_parent_dir(a->markerAbs);
                FILE* mf = _wfopen(a->markerAbs, L"wb");
                if (mf) {
                    const char* s = "dialog conversion completed\n";
                    fwrite(s, 1, strlen(s), mf);
                    fclose(mf);
                }
                ui_log_line(L"Dialog conversion complete. Marker written.");
            } else {
                ui_log_line(L"Dialog conversion skipped (missing Dialog dir or ffmpeg).");
            }
        } else {
            ui_log_line(L"Dialog conversion skipped (marker exists).");
        }
    } else {
        ui_log_line(L"Dialog conversion disabled.");
    }

    // 6) Ensure/copy nox.cfg template into assets
    wchar_t dstCfg[MAX_PATH * 4];
    path_join(dstCfg, ARRAYSIZE(dstCfg), a->assetsDirAbs, L"nox.cfg");

    if (!file_exists(a->templateCfgAbs)) {
        ui_log_line(L"ERROR: template nox.cfg missing next to launcher.");
        PostMessageW(hwnd, WM_APP_DONE, 0, 0);
        return 0;
    }

    if (!copy_template_if_missing(a->templateCfgAbs, dstCfg)) {
        ui_log_line(L"ERROR: failed to copy template nox.cfg into assets.");
        PostMessageW(hwnd, WM_APP_DONE, 0, 0);
        return 0;
    }

    // 7) Optionally change Windows system resolution before existing auto-detection
    DEVMODEW oldDisplayMode;
    ZeroMemory(&oldDisplayMode, sizeof(oldDisplayMode));
    bool oldDisplayModeValid = false;
    bool changedDisplayMode = false;

    if (!apply_system_resolution_for_launch(a->cfg.system_resolution, &oldDisplayMode, &oldDisplayModeValid, &changedDisplayMode)) {
        PostMessageW(hwnd, WM_APP_DONE, 0, 0);
        return 0;
    }

    // 8) Detect desktop resolution and compute Nox resolution
    int dispW = 0, dispH = 0;
    if (!get_desktop_resolution(&dispW, &dispH)) {
        dispW = 0; dispH = 0;
    }
    int noxW = 1024, noxH = 768;
    compute_nox_resolution(dispW, dispH, &noxW, &noxH);

    wchar_t rmsg[256];
    _snwprintf(rmsg, ARRAYSIZE(rmsg), L"Selected VideoMode: %dx%dx%d (desktop=%dx%d)", noxW, noxH, a->cfg.bits, dispW, dispH);
    rmsg[ARRAYSIZE(rmsg)-1]=0;
    ui_log_line(rmsg);

    // 8) Patch cfg
    ui_set_status(L"Patching config...");
    if (!patch_nox_cfg(dstCfg, noxW, noxH, a->cfg.bits, a->cfg.fullscreen)) {
        ui_log_line(L"ERROR: failed to patch nox.cfg.");
        restore_system_resolution_after_launch(&oldDisplayMode, oldDisplayModeValid, changedDisplayMode);
        PostMessageW(hwnd, WM_APP_DONE, 0, 0);
        return 0;
    }

    // 9) Decide game exe (exe lives in GAMEDIR, but CWD must be assetsDirAbs)
    wchar_t exe1[MAX_PATH * 4];
    wchar_t exe2[MAX_PATH * 4];
    wchar_t tmpName[64];

    _snwprintf(tmpName, ARRAYSIZE(tmpName), L"noxd.%s.exe", a->runArch);
    tmpName[ARRAYSIZE(tmpName)-1] = 0;

    path_join(exe1, ARRAYSIZE(exe1), a->gamedir, tmpName);
    path_join(exe2, ARRAYSIZE(exe2), a->gamedir, L"noxd.exe");

    const wchar_t* gameExe = NULL;
    if (file_exists(exe1)) gameExe = exe1;
    else if (file_exists(exe2)) gameExe = exe2;

    if (!gameExe) {
        ui_log_line(L"ERROR: no game binary found (noxd.<arch>.exe or noxd.exe).");
        restore_system_resolution_after_launch(&oldDisplayMode, oldDisplayModeValid, changedDisplayMode);
        PostMessageW(hwnd, WM_APP_DONE, 0, 0);
        return 0;
    }

    // Ensure working dir exists and is gamefiles/app
    if (!dir_exists(a->assetsDirAbs)) {
        ui_log_line(L"ERROR: assets directory missing (gamefiles/app).");
        restore_system_resolution_after_launch(&oldDisplayMode, oldDisplayModeValid, changedDisplayMode);
        PostMessageW(hwnd, WM_APP_DONE, 0, 0);
        return 0;
    }

    // 11) Launch game with stdout/stderr -> log.txt, with CWD=gamefiles/app
    ui_set_status(L"Launching game...");
    ui_log_line(L"Launching game...");
    ui_log_kv(L"Run arch", a->runArch);
    ui_log_kv(L"Device arch", a->deviceArch);
    ui_log_kv(L"Game exe", gameExe);
    ui_log_kv(L"Assets dir", a->assetsDirAbs);
    ui_log_kv(L"Launcher dir", a->launcherDir);
    ui_log_kv(L"Template cfg", a->templateCfgAbs);
    ui_log_kv(L"Log file", a->logAbs);
    ui_log_kv(L"Chosen installer", a->chosenInstallerAbs[0] ? a->chosenInstallerAbs : L"(none)");
    ui_log_kv(L"Chosen installed app", a->chosenInstallAppAbs[0] ? a->chosenInstallAppAbs : L"(none)");

    // Apply env overrides in THIS process temporarily (BAT parity), then launch with lpEnvironment=NULL.
    size_t saveCap = a->ini.count + a->vars.n + 16;
    EnvSaved* saved = (EnvSaved*)calloc(saveCap ? saveCap : 1, sizeof(EnvSaved));
    size_t sn = 0;

    if (!saved) {
        ui_log_line(L"ERROR: OOM preparing env overrides.");
        restore_system_resolution_after_launch(&oldDisplayMode, oldDisplayModeValid, changedDisplayMode);
        PostMessageW(hwnd, WM_APP_DONE, 0, 0);
        return 0;
    }

    // Apply all [env] keys from INI
    for (size_t i = 0; i < a->ini.count; i++) {
        const IniLine* l = &a->ini.lines[i];
        if (!l->is_kv || !l->section || !wcs_ieq(l->section, L"env")) continue;
        if (!l->key) continue;
        const wchar_t* val = l->value ? l->value : L"";
        if (sn < saveCap) {
            if (save_and_set_env(&saved[sn], l->key, val)) sn++;
        }
    }

    // Apply current UI edits last so they win
    for (size_t i = 0; i < a->vars.n; i++) {
        const VarSpec* s = &a->vars.v[i];
        if (!s->name) continue;
        const wchar_t* val = s->curval ? s->curval : L"";
        if (sn < saveCap) {
            if (save_and_set_env(&saved[sn], s->name, val)) sn++;
        }
    }
    // Force gamepad config to the file next to the launcher exe
    if (sn < saveCap) {
        if (save_and_set_env(&saved[sn], L"NOX_GAMEPAD_INI", a->gamepadIniAbs)) {
            sn++;
            ui_log_kv(L"NOX_GAMEPAD_INI", a->gamepadIniAbs);
        } else {
            ui_log_line(L"WARNING: failed to set NOX_GAMEPAD_INI.");
        }
    }

    // Force Intro after WWLogo only until we've marked it as already handled once.
    {
        bool introMarkerExists = file_exists(a->introMarkerAbs);
        const wchar_t* introForceVal = introMarkerExists ? L"0" : L"1";

        if (sn < saveCap) {
            if (save_and_set_env(&saved[sn], L"NOX_FORCE_INTRO_AT_START", introForceVal)) {
                sn++;
                ui_log_kv(L"NOX_FORCE_INTRO_AT_START", introForceVal);

                if (introMarkerExists) {
                    ui_log_line(L"Intro marker exists; disabling NOX_FORCE_INTRO_AT_START.");
                } else {
                    ui_log_line(L"Intro marker missing; enabling NOX_FORCE_INTRO_AT_START.");

                    // Same marker-file approach as dialog conversion:
                    // once we decide to force the intro on this run, write the marker now
                    // so future launches do not force it again.
                    if (write_text_file_utf8(a->introMarkerAbs, "intro handled\n")) {
                        ui_log_line(L"Wrote intro marker file.");
                    } else {
                        ui_log_line(L"WARNING: failed to write intro marker file.");
                    }
                }
            } else {
                ui_log_line(L"WARNING: failed to set NOX_FORCE_INTRO_AT_START.");
            }
        }
    }

    if (g_logFile != INVALID_HANDLE_VALUE) {
        FlushFileBuffers(g_logFile);
        CloseHandle(g_logFile);
        g_logFile = INVALID_HANDLE_VALUE;
    }

    bool launched = launch_game_and_exit(gameExe, a->assetsDirAbs, NULL, a->logAbs);

    restore_system_resolution_after_launch(&oldDisplayMode, oldDisplayModeValid, changedDisplayMode);

    // Always restore env overrides
    for (size_t i = 0; i < sn; i++) restore_env(&saved[i]);
    for (size_t i = 0; i < sn; i++) envsaved_free(&saved[i]);
    free(saved);

    if (!launched) {
        log_file_open_append(a->logAbs);
        ui_log_line(L"ERROR: failed to launch game.");
        PostMessageW(hwnd, WM_APP_DONE, 0, 0);
        return 0;
    }

    ui_log_line(L"Game process finished; launcher ready.");
    ui_set_status(L"Game exited");

    if (g_logFile != INVALID_HANDLE_VALUE) {
        FlushFileBuffers(g_logFile);
    }
    EnableWindow(g_app.hBtnLaunch, TRUE);
    EnableWindow(g_app.hBtnSave, TRUE);
    PostMessageW(hwnd, WM_APP_DONE, 0, 0);
//    PostMessageW(hwnd, WM_CLOSE, 0, 0);
    return 0;
}

// -------------------------
// UI creation and layout
// -------------------------
static void create_controls(HWND hwnd) {
    g_app.hLogEdit = CreateWindowExW(
        WS_EX_CLIENTEDGE, L"RICHEDIT50W", L"",
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY | WS_VSCROLL,
        0, 0, 100, 100,
        hwnd, (HMENU)(INT_PTR)IDC_LOG, g_app.hInst, NULL
    );
    SendMessageW(g_app.hLogEdit, EM_EXLIMITTEXT, 0, (LPARAM)0x7fffffff);

    g_app.hStatus = CreateWindowExW(
        0, L"STATIC", L"Ready",
        WS_CHILD | WS_VISIBLE,
        0, 0, 100, 20,
        hwnd, (HMENU)(INT_PTR)IDC_STATUS, g_app.hInst, NULL
    );

    g_app.hBtnLaunch = CreateWindowExW(
        0, L"BUTTON", L"Launch",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        0, 0, 100, 28,
        hwnd, (HMENU)(INT_PTR)IDC_BTN_LAUNCH, g_app.hInst, NULL
    );

    g_app.hBtnSave = CreateWindowExW(
        0, L"BUTTON", L"Save",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        0, 0, 100, 28,
        hwnd, (HMENU)(INT_PTR)IDC_BTN_SAVE, g_app.hInst, NULL
    );

//    g_app.hBtnBrowse = CreateWindowExW(
//        0, L"BUTTON", L"Browse Installer...",
//        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
//        0, 0, 140, 28,
//        hwnd, (HMENU)(INT_PTR)IDC_BTN_BROWSE, g_app.hInst, NULL
//    );

    // Scroll pane for env controls
    g_app.hScrollPane = CreateWindowExW(
        WS_EX_CLIENTEDGE, L"STATIC", L"",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL,
        0, 0, 200, 200,
        hwnd, (HMENU)(INT_PTR)IDC_SCROLLPANE, g_app.hInst, NULL
    );

    g_app.hResolutionLabel = CreateWindowExW(
        0, L"STATIC", L"Resolution",
        WS_CHILD | WS_VISIBLE,
        0, 0, 10, 10,
        g_app.hScrollPane, NULL, g_app.hInst, NULL
    );

    g_app.hResolutionCombo = CreateWindowExW(
        WS_EX_CLIENTEDGE, L"COMBOBOX", L"",
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_TABSTOP,
        0, 0, 10, 200,
        g_app.hScrollPane, (HMENU)(INT_PTR)IDC_RESOLUTION, g_app.hInst, NULL
    );

    // Apply font
    HFONT hf = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    SendMessageW(g_app.hLogEdit, WM_SETFONT, (WPARAM)hf, TRUE);
    SendMessageW(g_app.hStatus, WM_SETFONT, (WPARAM)hf, TRUE);
    SendMessageW(g_app.hBtnLaunch, WM_SETFONT, (WPARAM)hf, TRUE);
    SendMessageW(g_app.hBtnSave, WM_SETFONT, (WPARAM)hf, TRUE);
    SendMessageW(g_app.hScrollPane, WM_SETFONT, (WPARAM)hf, TRUE);
    SendMessageW(g_app.hResolutionLabel, WM_SETFONT, (WPARAM)hf, TRUE);
    SendMessageW(g_app.hResolutionCombo, WM_SETFONT, (WPARAM)hf, TRUE);
    populate_resolution_combo(g_app.hResolutionCombo, g_app.cfg.system_resolution);

    g_logState.hEdit = g_app.hLogEdit;
    g_logState.max_bytes = g_app.cfg.ring_buffer_bytes;
}

static void layout_controls(HWND hwnd) {
    RECT rc;
    GetClientRect(hwnd, &rc);
    Metrics m = metrics_get(hwnd);

    int W = rc.right - rc.left;
    int H = rc.bottom - rc.top;

    int contentX = m.pad;
    int contentY = m.pad;
    int contentW = W - 2 * m.pad;
    int contentH = H - 2 * m.pad - m.bottom_h;

    // Split: left env pane ~42% with min/max
    int leftW = (int)(contentW * 0.42);
    int minLeft = dpi_scale(hwnd, 280);
    int maxLeft = dpi_scale(hwnd, 520);
    if (leftW < minLeft) leftW = minLeft;
    if (leftW > maxLeft) leftW = maxLeft;
    if (leftW > contentW - dpi_scale(hwnd, 200)) leftW = contentW - dpi_scale(hwnd, 200);

    int rightX = contentX + leftW + m.gap;
    int rightW = contentW - leftW - m.gap;

    // Place scroll pane and log edit
    MoveWindow(g_app.hScrollPane, contentX, contentY, leftW, contentH, TRUE);
    MoveWindow(g_app.hLogEdit, rightX, contentY, rightW, contentH, TRUE);

    // Bottom bar
    int barY = contentY + contentH + m.gap;
    int barH = m.bottom_h - m.gap;
    int btnY = barY + (barH - m.btn_h) / 2;

    int btnX = contentX + contentW;
    btnX -= m.btn_w;
    MoveWindow(g_app.hBtnLaunch, btnX, btnY, m.btn_w, m.btn_h, TRUE);
    btnX -= (m.btn_w + dpi_scale(hwnd, 8));
    MoveWindow(g_app.hBtnSave, btnX, btnY, m.btn_w, m.btn_h, TRUE);

//    int browseW = dpi_scale(hwnd, 160);
//    btnX -= (browseW + dpi_scale(hwnd, 8));
//    MoveWindow(g_app.hBtnBrowse, btnX, btnY, browseW, m.btn_h, TRUE);

    // Status on left
    int statusW = btnX - contentX - dpi_scale(hwnd, 10);
    if (statusW < 50) statusW = 50;
    MoveWindow(g_app.hStatus, contentX, btnY, statusW, m.btn_h, TRUE);

    // Layout env controls inside scroll pane (simple: manual children)
    // We'll position them relative to scroll pane client area; no scrolling logic v1 beyond clipping.
    RECT prc;
    GetClientRect(g_app.hScrollPane, &prc);
    int px = dpi_scale(hwnd, 10);
    int py = dpi_scale(hwnd, 10);
    int paneW = prc.right - prc.left;
    int labelW = m.label_w;
    int ctrlX = px + labelW + dpi_scale(hwnd, 8);
    int ctrlW = paneW - ctrlX - px;
    if (ctrlW < dpi_scale(hwnd, 80)) ctrlW = dpi_scale(hwnd, 80);

    int y = py;

    int resRowH = m.row_h_edit;
    if (g_app.hResolutionLabel) MoveWindow(g_app.hResolutionLabel, px, y + (resRowH - dpi_scale(hwnd, 18))/2, labelW, dpi_scale(hwnd, 18), TRUE);
    if (g_app.hResolutionCombo) MoveWindow(g_app.hResolutionCombo, ctrlX, y, ctrlW, dpi_scale(hwnd, 200), TRUE);
    y += resRowH + m.row_gap;

    for (size_t i = 0; i < g_app.vars.n; i++) {
        VarSpec* s = &g_app.vars.v[i];
        int rowH = (s->type == VAR_BOOL) ? m.row_h_bool : m.row_h_edit;

        if (s->hLabel) MoveWindow(s->hLabel, px, y + (rowH - dpi_scale(hwnd, 18))/2, labelW, dpi_scale(hwnd, 18), TRUE);

        if (s->type == VAR_BOOL) {
            if (s->hCtrl) MoveWindow(s->hCtrl, ctrlX, y, min(ctrlW, dpi_scale(hwnd, 200)), rowH, TRUE);
        } else {
            if (s->hCtrl) MoveWindow(s->hCtrl, ctrlX, y, ctrlW, rowH, TRUE);
        }
        y += rowH + m.row_gap;
    }
}

static void build_env_controls(HWND hwnd) {
    HFONT hf = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    HWND parent = g_app.hScrollPane;

    for (size_t i = 0; i < g_app.vars.n; i++) {
        VarSpec* s = &g_app.vars.v[i];
        const wchar_t* label = s->label ? s->label : s->name;

        s->hLabel = CreateWindowExW(
            0, L"STATIC", label,
            WS_CHILD | WS_VISIBLE,
            0, 0, 10, 10,
            parent, NULL, g_app.hInst, NULL
        );
        SendMessageW(s->hLabel, WM_SETFONT, (WPARAM)hf, TRUE);

        if (s->type == VAR_BOOL) {
            s->hCtrl = CreateWindowExW(
                0, L"BUTTON", L"",
                WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                0, 0, 10, 10,
                parent, NULL, g_app.hInst, NULL
            );
            SendMessageW(s->hCtrl, WM_SETFONT, (WPARAM)hf, TRUE);

            // set checked based on curval matching true_value
            if (s->curval && s->true_val && wcs_ieq(s->curval, s->true_val))
                SendMessageW(s->hCtrl, BM_SETCHECK, BST_CHECKED, 0);
            else
                SendMessageW(s->hCtrl, BM_SETCHECK, BST_UNCHECKED, 0);
        } else {
            DWORD ex = WS_EX_CLIENTEDGE;
            DWORD style = WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL;
            if (s->secret) style |= ES_PASSWORD;

            s->hCtrl = CreateWindowExW(
                ex, L"EDIT", s->curval ? s->curval : L"",
                style,
                0, 0, 10, 10,
                parent, NULL, g_app.hInst, NULL
            );
            SendMessageW(s->hCtrl, WM_SETFONT, (WPARAM)hf, TRUE);
            if (s->maxlen > 0) SendMessageW(s->hCtrl, EM_SETLIMITTEXT, (WPARAM)s->maxlen, 0);
        }
    }
}

// -------------------------
// WinProc
// -------------------------
static void start_pipeline(HWND hwnd) {
    if (!choose_game_source_and_target(hwnd, &g_app)) {
        ui_log_line(L"Launch cancelled.");
        ui_set_status(L"Ready");
        return;
    }

    resolve_paths(&g_app);

    if (!ensure_required_free_space_for_install(hwnd, &g_app)) {
        ui_log_line(L"Not enough free disk space on the launcher drive.");
        ui_set_status(L"Ready");
        return;
    }

    WorkerArgs* wa = (WorkerArgs*)malloc(sizeof(WorkerArgs));
    if (!wa) {
        ui_log_line(L"ERROR: failed to allocate worker args.");
        ui_set_status(L"Ready");
        return;
    }

    wa->hwnd = hwnd;

    HANDLE th = CreateThread(NULL, 0, worker_thread, wa, 0, NULL);
    if (!th) {
        free(wa);
        ui_log_line(L"ERROR: failed to create worker thread.");
        ui_set_status(L"Ready");
        return;
    }
    CloseHandle(th);
    ui_log_line(L"Worker thread created successfully.");

    EnableWindow(g_app.hBtnLaunch, FALSE);
    EnableWindow(g_app.hBtnSave, FALSE);
    ui_set_status(L"Working...");
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE:
            create_controls(hwnd);
            build_env_controls(hwnd);
            layout_controls(hwnd);
            return 0;

        case WM_SIZE:
            layout_controls(hwnd);
            return 0;

        case WM_GETMINMAXINFO: {
            MINMAXINFO* mmi = (MINMAXINFO*)lParam;
            Metrics m = metrics_get(hwnd);
            mmi->ptMinTrackSize.x = m.min_w;
            mmi->ptMinTrackSize.y = m.min_h;
            return 0;
        }

        case WM_COMMAND: {
            int id = LOWORD(wParam);
            if (id == IDC_BTN_SAVE) {
                // clamp/revert and save ini
                for (size_t i = 0; i < g_app.vars.n; i++) var_update_from_ctrl(&g_app.vars.v[i]);
                resolution_update_from_ctrl(&g_app.cfg);
                for (size_t i = 0; i < g_app.vars.n; i++) {
                    VarSpec* s = &g_app.vars.v[i];
                    if (s->name) ini_set_env_value_preserve(&g_app.ini, s->name, s->curval ? s->curval : L"");
                }
                ini_set_value_preserve(&g_app.ini, L"launcher", L"system_resolution", g_app.cfg.system_resolution);
                if (ini_save_preserve(g_app.iniPath, &g_app.ini)) ui_log_line(L"Saved launch-nox-decomp.ini.");
                else ui_log_line(L"WARNING: Failed to save launch-nox-decomp.ini.");
                return 0;
            } else if (id == IDC_BTN_LAUNCH) {
                ui_log_line(L"Launch button clicked.");
                start_pipeline(hwnd);
                return 0;
            }
            return 0;
        }

        case WM_APP_LOG_FLUSH: {
            if (!g_logState.hEdit) {
                InterlockedExchange(&g_uiFlushPosted, 0);
                return 0;
            }

            InterlockedExchange(&g_uiFlushPosted, 0);

            wchar_t* local = NULL;
            size_t take = 0;

            EnterCriticalSection(&g_uiLogCs);
            if (g_uiLogLen > 0 && g_uiLogBuf) {
                take = g_uiLogLen;
                if (take > UI_FLUSH_MAX_CHARS) take = UI_FLUSH_MAX_CHARS;

                local = (wchar_t*)malloc((take + 1) * sizeof(wchar_t));
                if (local) {
                    memcpy(local, g_uiLogBuf, take * sizeof(wchar_t));
                    local[take] = 0;

                    // remove [0..take) from g_uiLogBuf by memmove
                    size_t remain = g_uiLogLen - take;
                    memmove(g_uiLogBuf, g_uiLogBuf + take, (remain + 1) * sizeof(wchar_t));
                    g_uiLogLen = remain;
                }
            }
            LeaveCriticalSection(&g_uiLogCs);

            if (local) {
                log_ui_append(g_logState.hEdit, local, g_logState.max_bytes);
                free(local);
            }

            // If still more buffered, request another flush
            EnterCriticalSection(&g_uiLogCs);
            bool hasMore = (g_uiLogLen > 0);
            LeaveCriticalSection(&g_uiLogCs);

            if (hasMore && InterlockedExchange(&g_uiFlushPosted, 1) == 0) {
                PostMessageW(hwnd, WM_APP_LOG_FLUSH, 0, 0);
            }
            return 0;
        }

        case WM_APP_DONE:
            EnableWindow(g_app.hBtnLaunch, TRUE);
            EnableWindow(g_app.hBtnSave, TRUE);
            ui_set_status(L"Ready");
            return 0;

        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

// -------------------------
// Initialization
// -------------------------
static void get_exe_dir(wchar_t* out, size_t cap) {
    wchar_t buf[MAX_PATH * 4];
    GetModuleFileNameW(NULL, buf, ARRAYSIZE(buf));
    buf[ARRAYSIZE(buf)-1]=0;
    // strip filename
    wchar_t* last = wcsrchr(buf, L'\\');
    if (last) *(last + 1) = 0;
    wcsncpy(out, buf, cap-1);
    out[cap-1]=0;
}

static void resolve_paths(AppState* a) {
    // launcher-local files
    resolve_path(a->iniPath, ARRAYSIZE(a->iniPath), a->launcherDir, L"launch-nox-decomp.ini");
    resolve_path(a->logAbs, ARRAYSIZE(a->logAbs), a->launcherDir, a->cfg.log_file);
    resolve_path(a->templateCfgAbs, ARRAYSIZE(a->templateCfgAbs), a->launcherDir, a->cfg.template_cfg);
    path_join(a->gamepadIniAbs, ARRAYSIZE(a->gamepadIniAbs), a->launcherDir, L"nox.gptk2.ini");

    // runtime game files are always launcher-local
    resolve_path(a->assetsDirAbs, ARRAYSIZE(a->assetsDirAbs), a->launcherDir, a->cfg.assets_dir);
    resolve_path(a->dialogDirAbs, ARRAYSIZE(a->dialogDirAbs), a->launcherDir, a->cfg.dialog_dir);
    resolve_path(a->markerAbs, ARRAYSIZE(a->markerAbs), a->launcherDir, a->cfg.convert_marker);
    path_join(a->introMarkerAbs, ARRAYSIZE(a->introMarkerAbs), a->assetsDirAbs, L"played_intro.txt");

    // source/extraction area
    path_join(a->srcDirAbs, ARRAYSIZE(a->srcDirAbs), a->launcherDir, L"gamefiles");
    path_join(a->neededAbs, ARRAYSIZE(a->neededAbs), a->assetsDirAbs, L"gamedata.bin");

    // tools stay next to launcher
    {
        wchar_t utilDir[MAX_PATH * 4];
        path_join(utilDir, ARRAYSIZE(utilDir), a->launcherDir, L"utils");

        wchar_t innoName[128];
        _snwprintf(innoName, ARRAYSIZE(innoName), L"innoextract.%s.exe", a->deviceArch);
        innoName[ARRAYSIZE(innoName)-1] = 0;
        path_join(a->innoextractAbs, ARRAYSIZE(a->innoextractAbs), utilDir, innoName);

        wchar_t ffName[128];
        _snwprintf(ffName, ARRAYSIZE(ffName), L"ffmpeg.%s.exe", a->deviceArch);
        ffName[ARRAYSIZE(ffName)-1] = 0;
        path_join(a->ffmpegAbs, ARRAYSIZE(a->ffmpegAbs), utilDir, ffName);
    }

    // actual exe root is launcher-local too
    wcsncpy(a->gamedir, a->launcherDir, ARRAYSIZE(a->gamedir) - 1);
    a->gamedir[ARRAYSIZE(a->gamedir) - 1] = 0;
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE hPrev, PWSTR lpCmdLine, int nShowCmd) {
    (void)hPrev; (void)lpCmdLine;

    g_app.hInst = hInst;
    get_exe_dir(g_app.launcherDir, ARRAYSIZE(g_app.launcherDir));

    g_app.gamedir[0] = 0;
    g_app.chosenInstallerAbs[0] = 0;
    g_app.chosenInstallAppAbs[0] = 0;

    // Load ini (must exist) next to launcher
    resolve_path(g_app.iniPath, ARRAYSIZE(g_app.iniPath), g_app.launcherDir, L"launch-nox-decomp.ini");
    if (!ini_load(g_app.iniPath, &g_app.ini)) {
        MessageBoxW(NULL, L"Missing or unreadable launch-nox-decomp.ini next to the launcher.", L"Nox Launcher", MB_ICONERROR);
        return 1;
    }

    InitializeCriticalSection(&g_uiLogCs);
    g_uiFlushPosted = 0;
    g_uiLogBuf = NULL;
    g_uiLogLen = 0;
    g_uiLogCap = 0;

    // Load launcher cfg + schema
    load_launcher_cfg(&g_app.ini, &g_app.cfg);
    detect_arch(&g_app);

    // default to launcher-local game root until a source is chosen
    wcsncpy(g_app.gamedir, g_app.launcherDir, ARRAYSIZE(g_app.gamedir) - 1);
    g_app.gamedir[ARRAYSIZE(g_app.gamedir) - 1] = 0;

    resolve_paths(&g_app);

    if (!load_schema(&g_app.ini, &g_app.vars)) {
        MessageBoxW(NULL, L"Failed to parse [ui.env] schema in launch-nox-decomp.ini.", L"Nox Launcher", MB_ICONERROR);
        ini_doc_free(&g_app.ini);
        return 1;
    }

    // Open log file (launcher writes until just before game launch)
    log_file_open_append(g_app.logAbs);



    // Optional: if hide_when_ready and gamefiles exist, we can auto-launch (no UI) and exit on success.
    // But extraction/conversion may still be needed; only auto-launch when ready.
    if (g_app.cfg.hide_when_ready && file_exists(g_app.neededAbs)) {
        // We still need to patch cfg each run (resolution) and launch.
        // We'll create a minimal hidden window to run pipeline so logs can still go to file.
        // For simplicity, we just show UI in this build; hiding UI can be added later if desired.
        // (You asked to exit launcher on game launch; so hiding is mostly redundant.)
    }

    // Register window class
    const wchar_t* cls = L"NoxLauncherWnd";
    WNDCLASSW wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = cls;
    RegisterClassW(&wc);

    LoadLibraryW(L"Msftedit.dll");

    HWND hwnd = CreateWindowExW(
        0, cls, L"Nox-Decomp Launcher",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 960, 720,
        NULL, NULL, hInst, NULL
    );
    g_app.hwnd = hwnd;

    ShowWindow(hwnd, nShowCmd);
    UpdateWindow(hwnd);

    // Initial status
    ui_set_status(L"Ready");
    ui_log_line(L"Launcher started.");

    // Message loop
    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    // Cleanup
    if (g_logFile != INVALID_HANDLE_VALUE) CloseHandle(g_logFile);
    varlist_free(&g_app.vars);
    ini_doc_free(&g_app.ini);

    EnterCriticalSection(&g_uiLogCs);
    free(g_uiLogBuf);
    g_uiLogBuf = NULL;
    g_uiLogLen = g_uiLogCap = 0;
    LeaveCriticalSection(&g_uiLogCs);
    DeleteCriticalSection(&g_uiLogCs);

    return 0;
}