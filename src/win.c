#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

#include "proto.h"

#ifdef USE_SDL
#include <SDL2/SDL.h>
#ifdef _WIN32
#include <SDL2/SDL_syswm.h>
#endif
#endif

#ifdef USE_SDL
void nox_control_server_init(void);
void nox_control_server_pump(void);
void nox_gamepad_update(void);
#endif

#ifdef USE_SDL
#include <stdio.h>     // for fprintf
#include <stdarg.h>    // for va_list
SDL_Window *g_window;

#ifdef USE_SDL
static void nox_dbgf(const char *tag, const char *fmt, ...)
{
    va_list ap;
    fprintf(stderr, "%s", tag ? tag : "[dbg] ");
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, "\n");
    fflush(stderr);
}
#define NOX_DBG(...) nox_dbgf("[win] ", __VA_ARGS__)
#else
#define NOX_DBG(...) do {} while (0)
#endif

/* Shared window handle used by draw.c (GL, grab, swap, etc) */
SDL_Window *dword_973FE0 = NULL;

static void nox_sdl_set_window(SDL_Window *w, const char *why)
{
    if (g_window != w || dword_973FE0 != w) {
        NOX_DBG("nox_sdl_set_window(%p) reason=%s (was g_window=%p dword_973FE0=%p)",
                (void*)w, why ? why : "(null)", (void*)g_window, (void*)dword_973FE0);
    }
    g_window = w;
    dword_973FE0 = w;
}

static void nox_sdl_assert_window(const char *where)
{
    if (!g_window && dword_973FE0) {
        NOX_DBG("SDL window missing at %s: g_window=NULL dword_973FE0=%p (HEAL g_window)",
                where ? where : "(null)", (void*)dword_973FE0);
        nox_sdl_set_window(dword_973FE0, "heal g_window NULL");
        return;
    }

    if (!dword_973FE0 && g_window) {
        NOX_DBG("SDL window missing at %s: g_window=%p dword_973FE0=NULL (HEAL dword_973FE0)",
                where ? where : "(null)", (void*)g_window);
        nox_sdl_set_window(g_window, "heal dword_973FE0 NULL");
        return;
    }

    if (!dword_973FE0 || !g_window) {
        NOX_DBG("SDL window missing at %s: g_window=%p dword_973FE0=%p",
                where ? where : "(null)", (void*)g_window, (void*)dword_973FE0);
        return;
    }

    if (g_window != dword_973FE0) {
        NOX_DBG("SDL window mismatch at %s: g_window=%p dword_973FE0=%p (HEAL)",
                where ? where : "(null)", (void*)g_window, (void*)dword_973FE0);
        nox_sdl_set_window(g_window, "heal mismatch");
    }
}


SDL_Window *nox_sdl_get_window(void)
{
    return dword_973FE0 ? dword_973FE0 : g_window;
}

static void nox_sdl_apply_mouse_capture(int enable)
{
    SDL_Window *w = nox_sdl_get_window();
    if (!w) return;

    if (enable) {
        SDL_SetRelativeMouseMode(SDL_TRUE);

        SDL_SetWindowGrab(w, SDL_TRUE);
#if SDL_VERSION_ATLEAST(2,0,4)
        SDL_CaptureMouse(SDL_TRUE);
#endif
        SDL_ShowCursor(SDL_DISABLE);
    } else {
        SDL_SetRelativeMouseMode(SDL_FALSE);
#if SDL_VERSION_ATLEAST(2,0,4)
        SDL_CaptureMouse(SDL_FALSE);
#endif
        SDL_SetWindowGrab(w, SDL_FALSE);
        SDL_ShowCursor(SDL_ENABLE);
    }

    NOX_DBG("mouse: show=%d rel=%d grab=%d",
            SDL_ShowCursor(SDL_QUERY),
            SDL_GetRelativeMouseMode(),
            (SDL_GetWindowFlags(w) & SDL_WINDOW_INPUT_GRABBED) != 0);
}


#else
WNDCLASSEXA g_wnd_class;
HWND g_hwnd;
#endif
DWORD dword_974854;
int g_fullscreen;

const char *g_argv[21];
unsigned int g_argc;



//----- (00401C70) --------------------------------------------------------
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
	char *v4; // edi
	int v5; // eax
	int v7; // esi
	int v10; // eax
	HWND v11; // esi
	HWND v13; // eax
	int v14; // eax

    NOX_DBG("WinMain enter hInstance=%p hPrevInstance=%p lpCmdLine='%s' nShowCmd=%d",
            (void*)hInstance, (void*)hPrevInstance,
            lpCmdLine ? lpCmdLine : "(null)", nShowCmd);

	init_data();
    NOX_DBG("after init_data");

	g_argv[0] = "nox.exe";
	g_argc = 1;

	sub_43BEF0(0, 0, 0);
    NOX_DBG("after sub_43BEF0");

	for (v4 = strtok(lpCmdLine, " \t"); v4; v4 = strtok(0, " \t"))
	{
		if (!strcmp("-window", v4))
		{
			*(_DWORD *)&byte_587000[80848] = 0;
			*(_DWORD *)&byte_5D4594[805860] = 0;
            NOX_DBG("arg -window");
		}
		else if (!strcmp("-swindow", v4))
		{
			*(_DWORD *)&byte_587000[80848] = 0;
			*(_DWORD *)&byte_5D4594[805860] = 1;
            NOX_DBG("arg -swindow");
		}
		else if (!strcmp("-minimize", v4))
		{
			*(_DWORD *)&byte_5D4594[805864] = 1;
            NOX_DBG("arg -minimize");
		}
		else
		{
			g_argv[g_argc++] = v4;
            NOX_DBG("arg passthrough '%s' (argc=%u)", v4, g_argc);
		}
	}
	g_argv[g_argc] = NULL;

	v7 = 0;
	v10 = 0;
	while (v7 < g_argc)
	{
		if (!_strcmpi(g_argv[v7++], "-serveronly"))
		{
			v10 = 1;
			break;
		}
	}
    NOX_DBG("parsed args argc=%u serveronly=%d", g_argc, v10);

#ifdef USE_SDL
	// ---------------------------------------------------------------------
	// SDL path: FORCE CLASSIC 640x480, NO WIDESCREEN
	// ---------------------------------------------------------------------
    NOX_DBG("SDL_Init starting");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER) != 0) {
        NOX_DBG("SDL_Init failed: %s", SDL_GetError());
        return 0;
    }
    NOX_DBG("SDL_Init OK");

    int w = *(int *)&byte_5D4594[3805496];
    int h = *(int *)&byte_5D4594[3807120];
    NOX_DBG("engine globals before clamp w=%d h=%d", w, h);

    // Wine/early init can leave these as 0. Clamp to sane classic default.
    if (w <= 0) w = 1024;
    if (h <= 0) h = 768;

    // Keep engine globals consistent with what we actually create.
    *(int *)&byte_5D4594[3805496] = w;
    *(int *)&byte_5D4594[3807120] = h;

    NOX_DBG("requested window %dx%d (after clamp)", w, h);

	// Hard clamp to 640x480 to avoid widescreen / wide backbuffer modes.
	// Also overwrite the original global resolution variables so the rest
	// of the engine thinks it's always 640x480.
	//int width  = 640;
	//int height = 480;

//	*(int *)&byte_5D4594[3805496] = width;   // original width global
//	*(int *)&byte_5D4594[3807120] = height;  // original height global

    // --- MUST be set BEFORE SDL_CreateWindow when using SDL_WINDOW_OPENGL ---
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 0);

    NOX_DBG("SDL_CreateWindow starting");
	SDL_Window *win = SDL_CreateWindow(
        "Nox Game Window",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        *(int *)&byte_5D4594[3805496],
        *(int *)&byte_5D4594[3807120],
        SDL_WINDOW_OPENGL | SDL_WINDOW_FULLSCREEN_DESKTOP     // NOTE: no SDL_WINDOW_RESIZABLE here
    );


    if (!win)
    {
        NOX_DBG("SDL_CreateWindow failed: %s", SDL_GetError());
        return 0;
    }


    /* Make draw.c see the same SDL_Window* */
    nox_sdl_set_window(win, "WinMain SDL_CreateWindow");


    // (optional but often helps) make sure it's foreground
    SDL_RaiseWindow(win);

    nox_sdl_apply_mouse_capture(1);

    NOX_DBG("SDL_CreateWindow OK g_window=%p", (void*)g_window);



    {
        Uint32 wf = SDL_GetWindowFlags(g_window);
        int ww = 0, wh = 0;
        SDL_GetWindowSize(g_window, &ww, &wh);
        NOX_DBG("window flags=0x%08x size=%dx%d", (unsigned)wf, ww, wh);
    }


	#ifdef USE_SDL
        NOX_DBG("calling nox_control_server_init()");
        nox_control_server_init();
        NOX_DBG("nox_control_server_init() returned");
    #endif

#ifdef __EMSCRIPTEN__
    if (EM_ASM_INT(return isMobile()))
    {
        SDL_EventState(SDL_MOUSEMOTION, SDL_IGNORE);
        SDL_EventState(SDL_MOUSEBUTTONDOWN, SDL_IGNORE);
        SDL_EventState(SDL_MOUSEBUTTONUP, SDL_IGNORE);
        SDL_EventState(SDL_MOUSEWHEEL, SDL_IGNORE);
        NOX_DBG("emscripten: mobile -> ignoring mouse events");
    }
    else
    {
        SDL_EventState(SDL_FINGERDOWN, SDL_IGNORE);
        SDL_EventState(SDL_FINGERUP, SDL_IGNORE);
        SDL_EventState(SDL_FINGERMOTION, SDL_IGNORE);
        NOX_DBG("emscripten: desktop -> ignoring touch events");
    }
#endif

    NOX_DBG("calling sub_401070(argc=%u,...)", g_argc);
	sub_401070(g_argc, g_argv);
    NOX_DBG("sub_401070 returned (unexpected; usually exits elsewhere)");

#else
	if (v10 || !(v11 = FindWindowA("Nox Game Window", 0)))
	{
		*(_DWORD *)&byte_5D4594[823784] = hInstance;
		*(_DWORD *)&byte_5D4594[823788] = nShowCmd;

		g_wnd_class.cbSize = 48;
		g_wnd_class.style = 4099;
		g_wnd_class.lpfnWndProc = sub_444FF0;
		g_wnd_class.cbClsExtra = 0;
		g_wnd_class.cbWndExtra = 0;
		g_wnd_class.hInstance = hInstance;
		g_wnd_class.hIcon = LoadIconA(hInstance, (LPCSTR)0x65);
		g_wnd_class.hCursor = LoadCursorA(0, (LPCSTR)0x7F00);
		g_wnd_class.hbrBackground = (HBRUSH)GetStockObject(4);
		g_wnd_class.lpszMenuName = 0;
		g_wnd_class.lpszClassName = "Nox Game Window";
		g_wnd_class.hIconSm = 0;

		RegisterClassExA(&g_wnd_class);
		v13 = CreateWindowExA(0x40000u, "Nox Game Window", "NOX", 0x80000000, 0, 0, *(int *)&byte_5D4594[3805496], *(int *)&byte_5D4594[3807120], 0, 0, hInstance, 0);
		if (!v13)
			return 0;
		g_hwnd = v13;

		sub_401070(g_argc, g_argv);
		if (byte_5D4594[2650637] & 0x10)
			sub_413E30(/* "Memory dump after GameLoop() :" */);
		*(_DWORD *)&byte_5D4594[823800] = 1;
		sub_4453A0();
		DestroyWindow(g_hwnd);
		sub_416B00();
		UnregisterClassA(g_wnd_class.lpszClassName, hInstance);
		ClipCursor(0);
	}
	else
	{
		SetForegroundWindow(v11);
		ShowWindow(v11, SW_RESTORE);
	}
#endif
    NOX_DBG("WinMain return 1");
	return 1;
}

//----- (00401FD0) --------------------------------------------------------
HWND sub_401FD0()
{
#ifdef USE_SDL
#ifdef _WIN32
    SDL_SysWMinfo info;
    SDL_VERSION(&info.version);

    // If window isn't ready yet, don't spam; return a safe fallback HWND.
    if (!g_window) {
        HWND fb = GetActiveWindow();
        if (!fb) fb = GetForegroundWindow();
        if (!fb) fb = GetDesktopWindow();
        NOX_DBG("sub_401FD0: g_window NULL -> fallback HWND=%p", (void*)fb);
        return fb;
    }

    if (SDL_GetWindowWMInfo(g_window, &info)) {
        HWND hwnd = info.info.win.window;
        NOX_DBG("sub_401FD0: SDL_GetWindowWMInfo OK hwnd=%p", (void*)hwnd);
        if (hwnd) return hwnd;
        NOX_DBG("sub_401FD0: WMInfo OK but hwnd NULL");
    } else {
        NOX_DBG("sub_401FD0: SDL_GetWindowWMInfo failed: %s", SDL_GetError());
    }

    // SDL window exists but WMInfo didn't give an HWND (can happen under Wine init races)
    {
        HWND fb = GetActiveWindow();
        if (!fb) fb = GetForegroundWindow();
        if (!fb) fb = GetDesktopWindow();
        nox_sdl_apply_mouse_capture(1);
        NOX_DBG("sub_401FD0: returning fallback HWND=%p", (void*)fb);
        return fb;
    }
#else
    return (HWND)g_window;
#endif
#else
    return g_hwnd;
#endif
}

//----- (00401FE0) --------------------------------------------------------
void __cdecl sub_401FE0(LPCSTR lpString)
{
#ifdef USE_SDL
	if (lpString)
		SDL_SetWindowTitle(g_window, lpString);
#else
	if (lpString)
		SetWindowTextA(g_hwnd, lpString);
#endif
}

//----- (0047D8A0) --------------------------------------------------------
void sub_47D8A0()
{
#ifdef USE_SDL
	SDL_RestoreWindow(g_window);
#else
	ShowWindow(sub_401FD0(), SW_RESTORE);
#endif
}

#ifdef USE_SDL
void process_keyboard_event(const SDL_KeyboardEvent *event);
void process_mouse_event(const SDL_MouseButtonEvent *event);
void process_motion_event(const SDL_MouseMotionEvent *event);
void process_wheel_event(const SDL_MouseWheelEvent *event);
void process_touch_event(const SDL_TouchFingerEvent *event);
void process_textediting_event(const SDL_TextEditingEvent *event);
void process_textinput_event(const SDL_TextInputEvent *event);

void process_event(const SDL_Event *event)
{
    if (!event) return;

    // Optional capture of user input into replayable telnet commands.
    nox_ctrl_capture_event(event);

	switch (event->type)
	{
//    case SDL_QUIT:
//        // Make movie loops exit (sub_555510 reads this)
//        sub_555500(1);
//        break;
    case SDL_WINDOWEVENT:
        if (event->window.event == SDL_WINDOWEVENT_FOCUS_GAINED ||
            event->window.event == SDL_WINDOWEVENT_ENTER ||
            event->window.event == SDL_WINDOWEVENT_SHOWN ||
            event->window.event == SDL_WINDOWEVENT_RESTORED) {
            nox_sdl_apply_mouse_capture(1);
        } else if (event->window.event == SDL_WINDOWEVENT_FOCUS_LOST ||
                   event->window.event == SDL_WINDOWEVENT_LEAVE ||
                   event->window.event == SDL_WINDOWEVENT_MINIMIZED) {
            nox_sdl_apply_mouse_capture(0);
        }
        break;
    case SDL_QUIT:
        NOX_DBG("SDL_QUIT received -> exit(0)");
        // Immediate exit (brutal but reliable)
        //        nox_gamepad_shutdown();
        exit(0);
        break;
    case SDL_TEXTEDITING:
        process_textediting_event(&event->edit);
        break;
    case SDL_TEXTINPUT:
        process_textinput_event(&event->text);
        break;
    case SDL_KEYDOWN:
    case SDL_KEYUP:
//        // Let existing keyboard queue logic run too, but also allow ESC to stop movie
//        if (event->key.keysym.sym == SDLK_ESCAPE)
//            sub_555500(1);
        process_keyboard_event(&event->key);
        break;
    case SDL_MOUSEBUTTONDOWN:
    case SDL_MOUSEBUTTONUP:
        process_mouse_event(&event->button);
        break;
    case SDL_MOUSEMOTION:
        process_motion_event(&event->motion);
        break;
    case SDL_MOUSEWHEEL:
        process_wheel_event(&event->wheel);
        break;
#ifdef __EMSCRIPTEN__
    case SDL_FINGERMOTION:
    case SDL_FINGERDOWN:
    case SDL_FINGERUP:
        process_touch_event(&event->tfinger);
        break;
#endif
	default:
		break;
	}
}
#endif

//----- (004453A0) --------------------------------------------------------
int sub_4453A0()
{
#ifdef USE_SDL
	SDL_Event event;
	int n = 0;
	nox_sdl_assert_window("sub_4453A0 (event pump entry)");
	while (SDL_PollEvent(&event)) {
        n++;
		process_event(&event);
    }
    // NOX_DBG("sub_4453A0: polled %d events", n);
	//mm_timer_pump_mainthread();
#ifdef USE_SDL
    nox_gamepad_update();
    nox_control_server_pump();
    nox_sdl_assert_window("sub_4453A0 (event pump exit)");
#endif
	return 0;
#else
	struct tagMSG Msg; // [esp+4h] [ebp-1Ch]

	while (PeekMessageA(&Msg, 0, 0, 0, 0))
	{
		if (!GetMessageA(&Msg, 0, 0, 0))
			break;
		TranslateMessage(&Msg);
		DispatchMessageA(&Msg);
	}
	if (!*(_DWORD *)&byte_5D4594[823800])
		return 0;
	PostMessageA(*(HWND *)&byte_5D4594[823796], WM_CLOSE, 0, 0);
	return 1;
#endif
}

//----- (004147E0) --------------------------------------------------------
BOOL __cdecl sub_4147E0(HWND hWnd)
{
#ifdef USE_SDL
#else
	// Trigger expiration time check.
	return PostMessageA(hWnd, 3024u, (WPARAM)&byte_5D4594[338464], *(LPARAM *)&byte_587000[32580]);
#endif
}

//----- (004516C0) --------------------------------------------------------
void sub_4516C0(wchar_t *a1, ...)
{
	wchar_t *v1; // eax
	HWND v2; // eax
	wchar_t *v3; // [esp-8h] [ebp-8h]
	va_list va; // [esp+8h] [ebp+8h]

	va_start(va, a1);
	if (!*(_DWORD *)&byte_5D4594[839880])
		sub_451630();
	sub_451610();
	v1 = sub_40F1D0("FatalErrorHeader", 0, (int)"C:\\NoxPost\\src\\Client\\Io\\Console.c", 314);
	nox_swprintf((wchar_t *)&byte_5D4594[833752], v1);
	nox_vswprintf((wchar_t *)&byte_5D4594[833778], a1, va);
	fprintf(*(FILE **)&byte_5D4594[839880], "%S", &byte_5D4594[833752]);
	fflush(*(FILE **)&byte_5D4594[839880]);
	v3 = sub_40F1D0("FatalError", 0, (int)"C:\\NoxPost\\src\\Client\\Io\\Console.c", 324);
	v2 = sub_401FD0();
    NOX_DBG("FatalError: sub_401FD0() hwnd=%p", (void*)v2);
	if (v2) {
        nullsub_4(v2, &byte_5D4594[833752], v3, 0);
    } else {
        NOX_DBG("FatalError: skipping nullsub_4 (no hwnd)");
    }
	fprintf(*(FILE **)&byte_5D4594[839880], "exiting..\n");
	fclose(*(FILE **)&byte_5D4594[839880]);
	if (*(_DWORD *)&byte_5D4594[823776])
		sub_430EF0();
	sub_4453A0();
	sub_4453A0();
	exit(0);
}
// 4514E0: using guessed type void __cdecl nullsub_4(_DWORD, _DWORD, _DWORD, _DWORD);

//----- (00416A10) --------------------------------------------------------
int sub_416A10()
{
	HWND v0; // esi
	int result; // eax
	HWND v2; // eax

#ifdef USE_SDL
	return 1;
#else
	*(_DWORD *)&byte_5D4594[371712] = CreateMutexA(0, 0, "843ED62B-354B-11d3-8F70-0060089C05B1");
	if (GetLastError() == 183)
	{
		if (*(_DWORD *)&byte_5D4594[371712])
		{
			CloseHandle(*(HANDLE *)&byte_5D4594[371712]);
			*(_DWORD *)&byte_5D4594[371712] = 0;
		}
		v0 = FindWindowA("Nox Game Window", 0);
		if (v0 || (v0 = FindWindowA("NOX Setup", 0)) != 0)
		{
			if (IsIconic(v0))
				ShowWindow(v0, 9);
			SetForegroundWindow(v0);
		}
		result = 0;
	}
	else
	{
		*(_DWORD *)&byte_5D4594[371708] = OpenMutexA(0x1F0001u, 0, "01AF9993-3492-11d3-8F6F-0060089C05B1");
		if (*(_DWORD *)&byte_5D4594[371708])
		{
			v2 = FindWindowA("NOX Autorun", 0);
			if (v2)
				SetForegroundWindow(v2);
			if (*(_DWORD *)&byte_5D4594[371708])
			{
				CloseHandle(*(HANDLE *)&byte_5D4594[371708]);
				*(_DWORD *)&byte_5D4594[371708] = 0;
			}
			if (*(_DWORD *)&byte_5D4594[371712])
			{
				CloseHandle(*(HANDLE *)&byte_5D4594[371712]);
				*(_DWORD *)&byte_5D4594[371712] = 0;
			}
			result = 0;
		}
		else
		{
			result = 1;
		}
	}
	return result;
#endif
}

//----- (00416B00) --------------------------------------------------------
HANDLE sub_416B00()
{
	HANDLE result; // eax

	result = *(HANDLE *)&byte_5D4594[371712];
	if (*(_DWORD *)&byte_5D4594[371712])
	{
		result = (HANDLE)CloseHandle(*(HANDLE *)&byte_5D4594[371712]);
		*(_DWORD *)&byte_5D4594[371712] = 0;
	}
	return result;
}
