/** @file src/video/video_win32.c WIN32 video driver. */

#include <stdio.h>
#define _WIN32_WINNT 0x0500
#include <windows.h>
#include <commctrl.h>
#include <malloc.h>
#include "types.h"
#include "../os/common.h"
#include "../os/error.h"

#include "video.h"
#include "video_fps.h"

#include "../gfx.h"
#include "../opendune.h"
#include "../input/input.h"
#include "../input/mouse.h"

#include "scalebit.h"
#include "hqx.h"

#ifdef _DEBUG
#define VIDEO_STATS
#endif

VideoScaleFilter g_scale_filter;

/** The the magnification of the screen. 2 means 640x400, 3 means 960x600, etc. */
int g_screen_magnification;

/** The palette used for HQX */
static uint32 rgb_palette[256];

static const char *s_className = "OpenDUNE";
static bool s_init = false;
static bool s_lock = false;
static HWND s_hwnd = NULL;
static HBITMAP s_dib = NULL;
static void *s_screen = NULL;
static void *s_screen2 = NULL;
static uint16 s_x;
static uint16 s_y;
static uint16 s_window_width = 0;
static uint16 s_window_height = 0;

#ifdef VIDEO_STATS
static const char *s_classNamePal = "OpenDUNEPal";
static HWND s_hwnd_pal = NULL;
static HBITMAP s_pal_dib = NULL;
static void *s_pal_screen = NULL;
#endif /* VIDEO_STATS */

static LONG s_adjustLeft;
static LONG s_adjustTop;
static LONG s_adjustRight;
static LONG s_adjustBottom;

static uint16 s_screenOffset = 0;

static bool s_mouseTracking = false;

static uint16 s_mousePosX = 0;
static uint16 s_mousePosY = 0;
static bool s_mouseButtonLeft  = false;
static bool s_mouseButtonRight = false;

static uint16 s_mouseMinX = 0;
static uint16 s_mouseMaxX = 0;
static uint16 s_mouseMinY = 0;
static uint16 s_mouseMaxY = 0;

static bool s_showFPS = false;

typedef struct VkMapping {
	WPARAM  vk;
	uint16 scan;
} VkMapping;

#define M(vk, scan) { vk, scan }
static VkMapping s_keyMap[] = {
	M(VK_ESCAPE,     0x0001),
	M('1',           0x0002),
	M('2',           0x0003),
	M('3',           0x0004),
	M('4',           0x0005),
	M('5',           0x0006),
	M('6',           0x0007),
	M('7',           0x0008),
	M('8',           0x0009),
	M('9',           0x000A),
	M('0',           0x000B),
	M(VK_OEM_MINUS,  0x000C),
	M(VK_OEM_PLUS,   0x000D),
	M(VK_BACK,       0x000E),
	M(VK_TAB,        0x000F),
	M('A',           0x001E),
	M('B',           0x0030),
	M('C',           0x002E),
	M('D',           0x0020),
	M('E',           0x0012),
	M('F',           0x0021),
	M('G',           0x0022),
	M('H',           0x0023),
	M('I',           0x0017),
	M('J',           0x0024),
	M('K',           0x0025),
	M('L',           0x0026),
	M('M',           0x0032),
	M('N',           0x0031),
	M('O',           0x0018),
	M('P',           0x0019),
	M('Q',           0x0010),
	M('R',           0x0013),
	M('S',           0x001F),
	M('T',           0x0014),
	M('U',           0x0016),
	M('V',           0x002F),
	M('W',           0x0011),
	M('X',           0x002D),
	M('Y',           0x0015),
	M('Z',           0x002C),
	M(VK_OEM_1,      0x001B),
	M(VK_RETURN,     0x001C),
	M(VK_CONTROL,    0x001D),
	M(VK_OEM_3,      0x0029),
	M(VK_OEM_5,      0x002B),
	M(VK_OEM_COMMA,  0x0033),
	M(VK_OEM_PERIOD, 0x0034),
	M(VK_OEM_2,      0x0035),
	M(VK_SHIFT,      0x0036),
	M(VK_MENU,       0x0038),
	M(VK_SPACE,      0x0039),
	M(VK_CAPITAL,	 0x003A),
	M(VK_F1,         0x003B),
	M(VK_F2,         0x003C),
	M(VK_F3,         0x003D),
	M(VK_F4,         0x003E),
	M(VK_F5,         0x003F),
	M(VK_F6,         0x0040),
	M(VK_F7,         0x0041),
	M(VK_F8,         0x0042),
	M(VK_F9,         0x0043),
	M(VK_F10,        0x0044),
	M(VK_F11,        0x0057),
	M(VK_F12,        0x0058),
	M(VK_NUMPAD0,	 0x0052),
	M(VK_NUMPAD1,    0x004F),
	M(VK_NUMPAD2,    0x0050),
	M(VK_NUMPAD3,    0x0051),
	M(VK_NUMPAD4,    0x004B),
	M(VK_NUMPAD5,    0x001C),
	M(VK_NUMPAD6,    0x004D),
	M(VK_NUMPAD7,    0x0047),
	M(VK_NUMPAD8,    0x0048),
	M(VK_NUMPAD9,    0x0049),
	M(VK_MULTIPLY,   0x0037),
	M(VK_ADD,        0x004e),
	M(VK_SUBTRACT,   0x004a),
	M(VK_DECIMAL,    0x0053),
	M(VK_DIVIDE,     0x0035),
	M(VK_UP,         0x0048),
	M(VK_DOWN,       0x0050),
	M(VK_RIGHT,      0x004D),
	M(VK_LEFT,       0x004B),
	M(VK_INSERT,     0x0052),
	M(VK_HOME,       0x0047),
	M(VK_END,        0x004F),
	M(VK_PRIOR,      0x0049),
	M(VK_NEXT,       0x0051),
	M(VK_OEM_102,	 0x0056),
	M(VK_OEM_4,      0x000c),
	M(VK_OEM_6,      0x001a),
	M(VK_OEM_7,      0x0029),
	M(VK_OEM_8,      0x0035)
};
#undef M

static uint16 MapKey(WPARAM vk)
{
	uint16 i;

	for (i = 0; i < lengthof(s_keyMap); i++) {
		if (s_keyMap[i].vk == vk) return s_keyMap[i].scan;
	}

	return 0x0000;
}

/**
 * Callback wrapper for mouse actions.
 */
static void Video_Mouse_Callback(void)
{
	Mouse_EventHandler(s_mousePosX * SCREEN_WIDTH / s_window_width, s_mousePosY * SCREEN_HEIGHT / s_window_height,
		               s_mouseButtonLeft, s_mouseButtonRight);
}

static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	bool keyup = true;

#ifdef VIDEO_STATS
	if(hwnd == s_hwnd_pal) {
		switch (uMsg) {
		case WM_PAINT: {
			PAINTSTRUCT ps;
			HDC dc;
			HDC dc2;

			dc = BeginPaint(hwnd, &ps);
			dc2 = CreateCompatibleDC(dc);
			/*old_bmp = */(HBITMAP)SelectObject(dc2, s_pal_dib);
			StretchBlt(dc, 0, 0, 256, 256,
					   dc2, 0, 0, 16, 16, SRCCOPY);
			//SelectObject(dc2, old_bmp);
			DeleteDC(dc2);

			EndPaint(hwnd, &ps);
			break;
		}
		default:
			return DefWindowProc(hwnd, uMsg, wParam, lParam);
		}
	}
#endif /* VIDEO_STATS */

	switch (uMsg) {
		case WM_CREATE: {
			CREATESTRUCT *cs = (CREATESTRUCT*)lParam;
			s_x = cs->x;
			s_y = cs->y;
			return 0;
		}

		case WM_MOVE:
			s_x = (uint16)(LOWORD(lParam));
			s_y = (uint16)(HIWORD(lParam));
			return 0;

		case WM_CLOSE:
			DestroyWindow(s_hwnd);
			PrepareEnd();
			exit(0);

		case WM_PAINT: {
			PAINTSTRUCT ps;
			HDC dc;
			HDC dc2;
			HBITMAP old_bmp;

			if (!GetUpdateRect(hwnd, NULL, FALSE)) return 0;

			if (s_showFPS) {
				Video_ShowFPS(s_screen);
			}
			if (g_scale_filter == FILTER_SCALE2X) {
				scale(g_screen_magnification, s_screen2, g_screen_magnification * SCREEN_WIDTH, s_screen, SCREEN_WIDTH, 1, SCREEN_WIDTH, SCREEN_HEIGHT);
			} else if(g_scale_filter == FILTER_HQX) {
				switch(g_screen_magnification) {
				case 2:
					hq2x_8to32(s_screen, s_screen2, SCREEN_WIDTH, SCREEN_HEIGHT, rgb_palette);
					break;
				case 3:
					hq3x_8to32(s_screen, s_screen2, SCREEN_WIDTH, SCREEN_HEIGHT, rgb_palette);
					break;
				case 4:
					hq4x_8to32(s_screen, s_screen2, SCREEN_WIDTH, SCREEN_HEIGHT, rgb_palette);
					break;
				}
			}
			dc = BeginPaint(hwnd, &ps);
			dc2 = CreateCompatibleDC(dc);
			old_bmp = (HBITMAP)SelectObject(dc2, s_dib);
			switch (g_scale_filter) {
			case FILTER_HQX:
			case FILTER_SCALE2X:
				BitBlt(dc, 0, 0, SCREEN_WIDTH * g_screen_magnification, SCREEN_HEIGHT * g_screen_magnification, dc2, 0, 0, SRCCOPY);
				break;
			case FILTER_NEAREST_NEIGHBOR:
			default:
				/*StretchBlt(dc, 0, 0, SCREEN_WIDTH * g_screen_magnification, SCREEN_HEIGHT * g_screen_magnification, dc2, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, SRCCOPY);*/
				StretchBlt(dc, 0, 0, s_window_width, s_window_height,
					       dc2, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, SRCCOPY);
			}
			SelectObject(dc2, old_bmp);
			DeleteDC(dc2);
			EndPaint(hwnd, &ps);
			return 0;
		}

		case WM_SIZING: {
			LONG width, height;
			RECT * rect = (RECT *)lParam;

			width = rect->right - rect->left - s_adjustRight + s_adjustLeft;
			height = rect->bottom - rect->top - s_adjustBottom + s_adjustTop;
			if(width * SCREEN_HEIGHT != height * SCREEN_WIDTH) {
				/* adjust window to keep aspect ratio ! */
				switch(wParam) {
				case WMSZ_TOPRIGHT:
				case WMSZ_TOPLEFT:
				case WMSZ_LEFT:
					rect->top = rect->bottom + s_adjustTop - s_adjustBottom - (width * SCREEN_HEIGHT) / SCREEN_WIDTH;
					break;
				case WMSZ_BOTTOMLEFT:
				case WMSZ_BOTTOMRIGHT:
				case WMSZ_RIGHT:
					rect->bottom = rect->top - s_adjustTop + s_adjustBottom + (width * SCREEN_HEIGHT) / SCREEN_WIDTH;
					break;
				case WMSZ_TOP:
					rect->left = rect->right - s_adjustRight + s_adjustLeft - (height * SCREEN_WIDTH) / SCREEN_HEIGHT;
					break;
				case WMSZ_BOTTOM:
					rect->right = rect->left + s_adjustRight - s_adjustLeft + (height * SCREEN_WIDTH) / SCREEN_HEIGHT;
					break;
				}
			}
			return 0;
		}

		case WM_SIZE: {
			s_window_width = (uint16)(LOWORD(lParam));
			s_window_height = (uint16)(HIWORD(lParam));
			return 0;
		}

		case WM_MOUSELEAVE:
			ShowCursor(TRUE);
			s_mouseTracking = false;
			return 0;

		case WM_MOUSEMOVE: {
			TRACKMOUSEEVENT tme;
			uint16 x, y;
			uint16 rx, ry;

			if (!s_mouseTracking) {
				tme.cbSize = sizeof(TRACKMOUSEEVENT);
				tme.dwFlags = TME_LEAVE;
				tme.hwndTrack = hwnd;
				if (_TrackMouseEvent(&tme)) {
					ShowCursor(FALSE);
					s_mouseTracking = true;
				}
			}

			x = (uint16)(LOWORD(lParam));
			y = (uint16)(HIWORD(lParam));

			rx = x;
			ry = y;

			if (s_mouseMinX != 0 && rx < s_mouseMinX) rx = s_mouseMinX;
			if (s_mouseMaxX != 0 && rx > s_mouseMaxX) rx = s_mouseMaxX;
			if (s_mouseMinY != 0 && ry < s_mouseMinY) ry = s_mouseMinY;
			if (s_mouseMaxY != 0 && ry > s_mouseMaxY) ry = s_mouseMaxY;

			/* If we moved, send the signal back to the window to correct for it */
			if (x != rx || y != ry) {
				SetCursorPos(s_x + rx, s_y + ry);
				return 0;
			}

			s_mousePosX = rx;
			s_mousePosY = ry;
			Video_Mouse_Callback();
			return 0;
		}

		case WM_LBUTTONDOWN:
			s_mouseButtonLeft = true;
			Video_Mouse_Callback();
			return 0;

		case WM_LBUTTONUP:
			s_mouseButtonLeft = false;
			Video_Mouse_Callback();
			return 0;

		case WM_RBUTTONDOWN:
			s_mouseButtonRight = true;
			Video_Mouse_Callback();
			return 0;

		case WM_RBUTTONUP:
			s_mouseButtonRight = false;
			Video_Mouse_Callback();
			return 0;

		case WM_KEYDOWN:
			keyup = false;
			/* Fall Through */
		case WM_KEYUP: {
			uint16 scan;

			if(wParam == VK_F8) {
				if(keyup) s_showFPS = !s_showFPS;
				return 0;
			}

			scan = MapKey(wParam);
			// Real scancode is also ((lParam >> 16) & 0xff)

			if (scan == 0) {
				Warning("Unhandled key %X (='%c')  (scan = %x)\n", wParam, wParam >= 32 ? wParam : '.', (lParam >> 16) & 0xff);
				return 0;
			}
			if ((scan >> 8) != 0) Input_EventHandler(scan >> 8);
			Input_EventHandler((scan & 0xFF) | (keyup ? 0x80 : 0x0));
			return 0;
		}

		default: break;
	}

	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

bool Video_Init(void)
{
	WNDCLASS wc;
	HINSTANCE hInstance;
	BITMAPINFO *bi;
	HDC dc;

	if (s_init) return true;
	if (g_screen_magnification <= 0 || g_screen_magnification > 4) {
		Error("Incorrect screen magnification factor : %d\n", g_screen_magnification);
		return false;
	}
	if (g_screen_magnification == 1) g_scale_filter = FILTER_NEAREST_NEIGHBOR;

	if (g_scale_filter == FILTER_HQX) {
		hqxInit();
	}

	hInstance = GetModuleHandle(NULL);

	wc.style = 0;
	wc.lpfnWndProc = WindowProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = hInstance;
	wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(100));
	wc.hCursor = NULL;
	wc.hbrBackground = 0;
	wc.lpszMenuName = NULL;
	wc.lpszClassName = s_className;
	if (!RegisterClass(&wc)) {
		Error("RegisterClass failed\n");
		return false;
	}
#ifdef VIDEO_STATS
	wc.style = 0;
	wc.lpfnWndProc = WindowProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = hInstance;
	wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(100));
	wc.hCursor = NULL;
	wc.hbrBackground = 0;
	wc.lpszMenuName = NULL;
	wc.lpszClassName = s_classNamePal;
	if (!RegisterClass(&wc)) {
		Error("RegisterClass failed\n");
		return false;
	}
#endif /* VIDEO_STATS */

	bi = (BITMAPINFO*)_alloca(sizeof(BITMAPINFOHEADER) + sizeof(RGBQUAD) * 256);
	memset(bi, 0, sizeof(BITMAPINFOHEADER) + sizeof(RGBQUAD) * 256);
	bi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	if (g_scale_filter == FILTER_NEAREST_NEIGHBOR) {
		bi->bmiHeader.biWidth = SCREEN_WIDTH;
		bi->bmiHeader.biHeight = -SCREEN_HEIGHT;
	} else {
		bi->bmiHeader.biWidth = SCREEN_WIDTH * g_screen_magnification;
		bi->bmiHeader.biHeight = -SCREEN_HEIGHT * g_screen_magnification;
	}
	bi->bmiHeader.biPlanes = 1;
	if (g_scale_filter == FILTER_HQX) {
		bi->bmiHeader.biBitCount = 32;
	} else {
		bi->bmiHeader.biBitCount = 8;
	}
	bi->bmiHeader.biCompression = BI_RGB;

	dc = GetDC(NULL);
	s_dib = CreateDIBSection(dc, bi, DIB_RGB_COLORS, (g_scale_filter == FILTER_NEAREST_NEIGHBOR) ? &s_screen : &s_screen2, NULL, 0);
	if (s_dib == NULL) {
		Error("CreateDIBSection failed\n");
		return false;
	}
#ifdef VIDEO_STATS
	bi->bmiHeader.biWidth = 16;
	bi->bmiHeader.biHeight = -16;
	bi->bmiHeader.biPlanes = 1;
	bi->bmiHeader.biBitCount = 8;
	bi->bmiHeader.biCompression = BI_RGB;
	s_pal_dib = CreateDIBSection(dc, bi, DIB_RGB_COLORS, &s_pal_screen, NULL, 0);
	if (s_pal_dib == NULL) {
		Error("CreateDIBSection failed\n");
	} else {
		int i;
		uint8 * data = (uint8 *)s_pal_screen;
		for(i = 0; i < 256; i++) {
			data[i] = i;
		}
	}
#endif /* VIDEO_STATS */
	ReleaseDC(NULL, dc);

	if (g_scale_filter != FILTER_NEAREST_NEIGHBOR) {
#ifdef _MSC_VER
		/* we need aligned memory for rescale filter */
		s_screen = _aligned_malloc(SCREEN_WIDTH * SCREEN_HEIGHT, 16);
#else  /* _MSC_VER */
		s_screen = malloc(SCREEN_WIDTH * SCREEN_HEIGHT);
#endif /* _MSC_VER */
	}
	s_init = true;
	return true;
}

void Video_Uninit(void)
{
	if (!s_init) return;

	DeleteObject(s_dib);
	if (g_scale_filter != FILTER_NEAREST_NEIGHBOR) {
#ifdef _MSC_VER
		_aligned_free(s_screen);
#else  /* _MSC_VER */
		free(s_screen);
#endif /* _MSC_VER */
	}
	UnregisterClass(s_className, GetModuleHandle(NULL));
	ShowCursor(TRUE);

	if (g_scale_filter == FILTER_HQX) {
		hqxUnInit();
	}
	s_init = false;
}

#ifdef VIDEO_STATS
static void Video_Stats(const uint8 * screen)
{
	unsigned int i, j;
	static uint16 freq[256];
	unsigned int unused_colors;
	static unsigned int last_unused_colors = 0;
	uint16 rgb12pal[256];
	uint8 similar_color[256];
	unsigned int n_similar_colors;
	static unsigned int last_n_similar_colors = 0;

	memset(freq, 0, sizeof(freq));
	for(i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; i++) {
		freq[screen[i]]++;
	}

	unused_colors = 0;
	for(i = 0; i < 256; i++) {
		if(freq[i] == 0) unused_colors++;
	}
	if(unused_colors != last_unused_colors) {
		char tmp[20];
		Debug("Unused colors : %u (used = %u)\n", unused_colors, 256 - unused_colors);
		Debug("        0123456789ABCDEF\n");
		last_unused_colors = unused_colors;
		for(i = 0; i < 256 ; i++) {
			tmp[i&15] = (freq[i] == 0)?' ':((freq[i] < 10)?'.':((freq[i] < 250)?'o':'O'));
			if((i&15) == 15) {
				tmp[16] = '\0';
				Debug("%3d %02X |%s| %3d\n", i & 0xf0, i & 0xf0, tmp, i);
			}
		}
	}
	n_similar_colors = 0;
	for(i = 0; i < 256; i++) {
		if(freq[i] == 0) rgb12pal[i] = 0xffff;
		else {
			rgb12pal[i] = ((rgb_palette[i] >> 4) & 0x00f) | ((rgb_palette[i] >> 8) & 0x0f0) | ((rgb_palette[i] >> 12) & 0xf00);
			similar_color[i] = i;
			for(j = 0; j < i; j++) {
				if(freq[j] != 0 && rgb_palette[i] == rgb_palette[j]) {
					similar_color[i] = j;
					n_similar_colors++;
					break;
				}
			}
		}
	}
	if(n_similar_colors > 0 && n_similar_colors != last_n_similar_colors) {
		Debug("Similar colors = %u\n", n_similar_colors);
		last_n_similar_colors = n_similar_colors;
	}
}
#endif	/* VIDEO_STATS */

void Video_Tick(void)
{
	MSG msg;
	const uint8 * screen0;

	if (!s_init) return;

	if (s_lock) return;
	s_lock = true;

	if (s_hwnd == NULL) {
		DWORD style;
		RECT r;

		style = WS_CAPTION | WS_MINIMIZEBOX | WS_SYSMENU;
		/* allow window to be resized with NEAREST NEIGHBOR filter */
		if(g_scale_filter == FILTER_NEAREST_NEIGHBOR) style |= WS_THICKFRAME;

		r.left   = 0;
		r.top    = 0;
		r.right  = SCREEN_WIDTH * g_screen_magnification;
		r.bottom = SCREEN_HEIGHT * g_screen_magnification;
		AdjustWindowRect(&r, style, false);
		s_adjustLeft = r.left;
		s_adjustTop = r.top;
		s_adjustRight = r.right - SCREEN_WIDTH * g_screen_magnification;
		s_adjustBottom = r.bottom - SCREEN_HEIGHT * g_screen_magnification;

		s_hwnd = CreateWindow(s_className, window_caption, style, CW_USEDEFAULT, CW_USEDEFAULT, r.right - r.left, r.bottom - r.top, NULL, NULL, GetModuleHandle(NULL), NULL);
		if (s_hwnd == NULL) {
			Error("CreateWindow failed\n");
			PrepareEnd();
			return;
		}

		Video_Mouse_SetPosition(g_mouseX, g_mouseY);
		ShowWindow(s_hwnd, SW_SHOWNORMAL);

#ifdef VIDEO_STATS
		GetWindowRect(s_hwnd, &r);
		style = WS_CAPTION;
		r.left = r.right + 10;
		r.top = r.top + 40;
		r.right = r.left + 256;
		r.bottom = r.top + 256;
		AdjustWindowRect(&r, style, false);
		s_hwnd_pal = CreateWindow(s_classNamePal, "palette", style, r.left, r.top, r.right - r.left, r.bottom - r.top, NULL, NULL, GetModuleHandle(NULL), NULL);
		if (s_hwnd_pal == NULL) {
			Error("CreateWindow failed\n");
		}
		ShowWindow(s_hwnd_pal, SW_SHOWNORMAL);
#endif /* VIDEO_STATS */
	}

	while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	screen0 = GFX_Screen_Get_ByIndex(SCREEN_0);
	screen0 += (s_screenOffset << 2);
	/* Do a quick compare to see if the screen changed at all */
	if (memcmp(screen0, s_screen, SCREEN_WIDTH * SCREEN_HEIGHT) != 0) {
		memcpy(s_screen, screen0, SCREEN_WIDTH * SCREEN_HEIGHT);
#ifdef VIDEO_STATS
		Video_Stats(screen0);
#endif	/* VIDEO_STATS */
		InvalidateRect(s_hwnd, NULL, TRUE);
	}

	s_lock = false;
}

void Video_SetPalette(void *palette, int from, int length)
{
	uint8 *p = palette;
	int i;

	if (g_scale_filter == FILTER_HQX) {
		uint32 value;

		for (i = from; i < from + length; i++) {
			value = (((*p++) & 0x3F) * 0x41000) & 0xff0000;
			value |= (((*p++) & 0x3F) * 0x410) & 0x00ff00;
			rgb_palette[i] = value | ((((*p++) & 0x3F) * 0x41)>> 4);
		}
	} else {
		RGBQUAD rgb[256];
		HDC dc;
		HDC dc2;
		HBITMAP old_bmp;

		/* convert from 6bit to 8bit per component */
		for (i = 0; i < length; i++) {
			rgb[i].rgbRed      = (((*p++) & 0x3F) * 0x41) >> 4;
			rgb[i].rgbGreen    = (((*p++) & 0x3F) * 0x41) >> 4;
			rgb[i].rgbBlue     = (((*p++) & 0x3F) * 0x41) >> 4;
			rgb[i].rgbReserved = 0;
		}

		dc = GetDC(s_hwnd);
		dc2 = CreateCompatibleDC(dc);
		old_bmp = SelectObject(dc2, s_dib);
		SetDIBColorTable(dc2, from, length, rgb);
		SelectObject(dc2, old_bmp);
		DeleteDC(dc2);
		ReleaseDC(s_hwnd, dc);
#ifdef VIDEO_STATS
		dc = GetDC(s_hwnd_pal);
		dc2 = CreateCompatibleDC(dc);
		old_bmp = SelectObject(dc2, s_pal_dib);
		SetDIBColorTable(dc2, from, length, rgb);
		SelectObject(dc2, old_bmp);
		DeleteDC(dc2);
		ReleaseDC(s_hwnd_pal, dc);
		InvalidateRect(s_hwnd_pal, NULL, TRUE);
#endif	/* VIDEO_STATS */
	}
	InvalidateRect(s_hwnd, NULL, TRUE);
}

void Video_Mouse_SetPosition(uint16 x, uint16 y)
{
	SetCursorPos(s_x + x * g_screen_magnification, s_y + y * g_screen_magnification);
}

void Video_Mouse_SetRegion(uint16 minX, uint16 maxX, uint16 minY, uint16 maxY)
{
	s_mouseMinX = minX * g_screen_magnification;
	s_mouseMaxX = maxX * g_screen_magnification;
	s_mouseMinY = minY * g_screen_magnification;
	s_mouseMaxY = maxY * g_screen_magnification;
}

/*
 * change the screen offset, equivalent to changing the
 * Start Address Register on a VGA card.
 * VGA Hardware has 4 "maps" of 64kB.
 * @param offset The address granularity is 4bytes
 */
void Video_SetOffset(uint16 offset)
{
	s_screenOffset = offset;
}
