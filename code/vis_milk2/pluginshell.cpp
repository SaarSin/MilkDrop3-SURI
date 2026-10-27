/*
  LICENSE
  -------
Copyright 2005-2013 Nullsoft, Inc.
All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

  * Redistributions of source code must retain the above copyright notice,
    this list of conditions and the following disclaimer.

  * Redistributions in binary form must reproduce the above copyright notice,
    this list of conditions and the following disclaimer in the documentation
    and/or other materials provided with the distribution.

  * Neither the name of Nullsoft nor the names of its contributors may be used to
    endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "pluginshell.h"
#include "utility.h"
#include "defines.h"
#include "shell_defines.h"
#include "resource.h"
#include "resstrings.h"
#include <multimon.h>
#include "AutoCharFn.h"
#include "features/ui_scale.h"
#include "features/help_text.h"
#include <mmsystem.h>
#pragma comment(lib,"winmm.lib")    // for timeGetTime

// STATE VALUES & VERTEX FORMATS FOR HELP SCREEN TEXTURE:
#define TEXT_SURFACE_NOT_READY  0
#define TEXT_SURFACE_REQUESTED  1
#define TEXT_SURFACE_READY      2
#define TEXT_SURFACE_ERROR      3
typedef struct _HELPVERTEX
{
	float x, y;      // screen position
	float z;         // Z-buffer depth
	DWORD Diffuse;   // diffuse color. also acts as filler; aligns struct to 16 bytes (good for random access/indexed prims)
	float tu, tv;    // texture coordinates for texture #0
} HELPVERTEX, *LPHELPVERTEX;
#define HELP_VERTEX_FORMAT (D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1)
typedef struct _SIMPLEVERTEX
{
	float x, y;      // screen position
	float z;         // Z-buffer depth
	DWORD Diffuse;   // diffuse color. also acts as filler; aligns struct to 16 bytes (good for random access/indexed prims)
} SIMPLEVERTEX, *LPSIMPLEVERTEX;
#define SIMPLE_VERTEX_FORMAT (D3DFVF_XYZ | D3DFVF_DIFFUSE)

// SURI: help pages (see features/help_text.cpp)
static const std::vector<std::wstring>& HelpPages()
{
	static std::vector<std::wstring> pages = SplitHelpPages(HelpText());
	return pages;
}


CPluginShell::CPluginShell()
{
	// this should remain empty!
}

CPluginShell::~CPluginShell()
{
	// this should remain empty!
}

int       CPluginShell::GetFrame()
{
	return m_frame;
};
float     CPluginShell::GetTime()
{
	return m_time;
};
float     CPluginShell::GetFps()
{
	return m_fps;
};
HWND      CPluginShell::GetPluginWindow()
{
	if (m_lpDX) return m_lpDX->GetHwnd();       else return NULL;
};
int       CPluginShell::GetWidth()
{
	if (m_lpDX) return m_lpDX->m_client_width;  else return 0;
};
int       CPluginShell::GetHeight()
{
	if (m_lpDX) return m_lpDX->m_client_height; else return 0;
};
int       CPluginShell::GetCanvasMarginX()
{
	if (m_lpDX) return (m_lpDX->m_client_width  - m_lpDX->m_REAL_client_width)/2; else return 0;
};
int       CPluginShell::GetCanvasMarginY()
{
	if (m_lpDX) return (m_lpDX->m_client_height - m_lpDX->m_REAL_client_height)/2; else return 0;
};
HINSTANCE CPluginShell::GetInstance()
{
	return m_hInstance;
};
wchar_t* CPluginShell::GetPluginsDirPath()
{
	return m_szPluginsDirPath;
};
wchar_t* CPluginShell::GetConfigIniFile()
{
	return m_szConfigIniFile;
};
char* CPluginShell::GetConfigIniFileA()
{
	return m_szConfigIniFileA;
}
int       CPluginShell::GetFontHeight(eFontIndex idx)
{
	if (idx >= 0 && idx < NUM_BASIC_FONTS + NUM_EXTRA_FONTS) return m_fontinfo[idx].nSize; else return 0;
};
int       CPluginShell::GetBitDepth()
{
	return m_lpDX->GetBitDepth();
};
LPDIRECT3DDEVICE9 CPluginShell::GetDevice()
{
	if (m_lpDX) return m_lpDX->m_lpDevice; else return NULL;
};
D3DCAPS9* CPluginShell::GetCaps()
{
	if (m_lpDX) return &(m_lpDX->m_caps);  else return NULL;
};
D3DFORMAT CPluginShell::GetBackBufFormat()
{
	if (m_lpDX) return m_lpDX->m_current_mode.display_mode.Format; else return D3DFMT_UNKNOWN;
};
D3DFORMAT CPluginShell::GetBackBufZFormat()
{
	if (m_lpDX) return m_lpDX->GetZFormat(); else return D3DFMT_UNKNOWN;
};
LPD3DXFONT CPluginShell::GetFont(eFontIndex idx)
{
	if (idx >= 0 && idx < NUM_BASIC_FONTS + NUM_EXTRA_FONTS) return m_d3dx_font[idx]; else return NULL;
};
char* CPluginShell::GetDriverFilename()
{
	if (m_lpDX) return m_lpDX->GetDriver(); else return NULL;
};
char* CPluginShell::GetDriverDescription()
{
	if (m_lpDX) return m_lpDX->GetDesc(); else return NULL;
};

int CPluginShell::InitNondx9Stuff()
{
	timeBeginPeriod(1);
	m_fftobj.Init(576, NUM_FREQUENCIES);
	if (!InitGDIStuff()) return false;
	return AllocateMyNonDx9Stuff();
}

void CPluginShell::CleanUpNondx9Stuff()
{
	timeEndPeriod(1);
	CleanUpMyNonDx9Stuff();
	CleanUpGDIStuff();
	m_fftobj.CleanUp();
}

int CPluginShell::InitGDIStuff()
{
	wchar_t title[64];
	// note: messagebox parent window should be NULL here, because lpDX is still NULL!
	for (int i=0; i<NUM_BASIC_FONTS + NUM_EXTRA_FONTS; i++)
	{
		if (!(m_font[i] = CreateFontW(m_fontinfo[i].nSize, 0, 0, 0, m_fontinfo[i].bBold ? 900 : 400, m_fontinfo[i].bItalic, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, m_fontinfo[i].bAntiAliased ? ANTIALIASED_QUALITY : DEFAULT_QUALITY, DEFAULT_PITCH, m_fontinfo[i].szFace)))
		{
			MessageBoxW(NULL, LoadResString(IDS_ERROR_CREATING_GDI_FONTS),
					    LoadResString(IDS_MILKDROP_ERROR, title, 64),
					    MB_OK|MB_SETFOREGROUND|MB_TOPMOST);
			return false;
		}
	}

	return true;
}

void CPluginShell::CleanUpGDIStuff()
{
	for (int i=0; i<NUM_BASIC_FONTS + NUM_EXTRA_FONTS; i++)
	{
		if (m_font[i])
		{
			DeleteObject(m_font[i]);
			m_font[i] = NULL;
		}
	}
}

int CPluginShell::InitVJStuff(RECT* pClientRect)
{
	wchar_t title[64];
	// Init VJ mode (second window for text):
	if (m_vj_mode)
	{
		DWORD dwStyle = WS_OVERLAPPEDWINDOW | WS_VISIBLE | WS_SYSMENU;
		POINT upper_left_corner;
		upper_left_corner.x = 0;
		upper_left_corner.y = 0;

		// Create direct 3d & get some infos
		if (!(m_vjd3d9 = Direct3DCreate9(D3D_SDK_VERSION)))
		{
			MessageBoxW(NULL, LoadResString(IDS_ERROR_CREATING_DIRECT3D_DEVICE_FOR_VJ_MODE),
					    LoadResString(IDS_MILKDROP_ERROR, title, 64), MB_OK|MB_SETFOREGROUND|MB_TOPMOST);
			return false;
		}

		// Get ordinal adapter # for the currently-selected Windowed Mode display adapter
		int ordinal_adapter = D3DADAPTER_DEFAULT;
		int nAdapters = m_vjd3d9->GetAdapterCount();
		for (int i=0; i<nAdapters; i++)
		{
			D3DADAPTER_IDENTIFIER9 temp;
			if ((m_vjd3d9->GetAdapterIdentifier(i, /*D3DENUM_NO_WHQL_LEVEL*/ 0, &temp) == D3D_OK) &&
			    (memcmp(&temp.DeviceIdentifier, &m_adapter_guid_windowed, sizeof(GUID))==0))
			{
				ordinal_adapter = i;
				break;
			}
		}

		// Get current display mode for windowed-mode adapter:
		D3DDISPLAYMODE dm;
		if (D3D_OK != m_vjd3d9->GetAdapterDisplayMode(ordinal_adapter, &dm))
		{
			MessageBoxW(NULL, LoadResString(IDS_VJ_MODE_INIT_ERROR),
					    LoadResString(IDS_MILKDROP_ERROR, title, 64),
					    MB_OK|MB_SETFOREGROUND|MB_TOPMOST);
			return false;
		}

		// And get the upper-left corner of the monitor for it:
		HMONITOR hMon = m_vjd3d9->GetAdapterMonitor(ordinal_adapter);
		if (hMon)
		{
			MONITORINFO mi;
			mi.cbSize = sizeof(mi);
		}

		// CREATE THE WINDOW

		RECT rect;
		if (pClientRect)
		{
			rect = *pClientRect;
			AdjustWindowRect(&rect, dwStyle, 0); // convert client->wnd
		}
		else
		{
			SetRect(&rect, 0, 0, 384, 384);
			AdjustWindowRect(&rect, dwStyle, 0); // convert client->wnd

			rect.right  -= rect.left;
			rect.left   = 0;
			rect.bottom -= rect.top;
			rect.top    = 0;

			rect.top    += upper_left_corner.y+32;
			rect.left   += upper_left_corner.x+32;
			rect.right  += upper_left_corner.x+32;
			rect.bottom += upper_left_corner.y+32;
		}

		WNDCLASS wc = {0};
		wc.lpfnWndProc = VJModeWndProc;				// our window procedure
		wc.hInstance = GetInstance();	// hInstance of DLL
		wc.hIcon = LoadIcon(GetInstance(), MAKEINTRESOURCE(IDI_PLUGIN_ICON));
		wc.lpszClassName = TEXT_WINDOW_CLASSNAME;			// our window class name
		wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS; // CS_DBLCLKS lets the window receive WM_LBUTTONDBLCLK, for toggling fullscreen mode...
		wc.cbWndExtra = sizeof(DWORD);
		wc.hCursor = LoadCursor(NULL, IDC_ARROW);
		wc.hbrBackground = (HBRUSH) GetStockObject(BLACK_BRUSH);

		if (!RegisterClass(&wc))
		{
			MessageBoxW(NULL, LoadResString(IDS_ERROR_REGISTERING_WINDOW_CLASS_FOR_TEXT_WINDOW),
					    LoadResString(IDS_MILKDROP_ERROR, title, 64),
					    MB_OK|MB_SETFOREGROUND|MB_TOPMOST);
			return false;
		}
		m_bTextWindowClassRegistered = true;


		// Create the text window
		m_hTextWnd = CreateWindowEx(
		               0,
		               TEXT_WINDOW_CLASSNAME,				// our window class name
		               TEXT_WINDOW_CLASSNAME,				// use description for a window title
		               dwStyle,
		               rect.left, rect.top,								// screen position (read from config)
		               rect.right - rect.left, rect.bottom - rect.top,  // width & height of window (need to adjust client area later)
		               NULL,								// no parent window
		               NULL,								// no menu
		               GetInstance(),						// hInstance of DLL
		               NULL
		             ); // no window creation data

		if (!m_hTextWnd)
		{
			MessageBoxW(NULL, LoadResString(IDS_ERROR_CREATING_VJ_WINDOW),
					   LoadResString(IDS_MILKDROP_ERROR, title, 64),
					   MB_OK|MB_SETFOREGROUND|MB_TOPMOST);
			return false;
		}

		SetWindowLongPtr(m_hTextWnd, GWLP_USERDATA, (LONG_PTR)this);

		GetClientRect(m_hTextWnd, &rect);
		m_nTextWndWidth  = rect.right-rect.left;
		m_nTextWndHeight = rect.bottom-rect.top;


		// Create the device
		D3DPRESENT_PARAMETERS pres_param;
		ZeroMemory(&pres_param,sizeof(pres_param));
		pres_param.BackBufferCount = 0;
		pres_param.BackBufferFormat = dm.Format;
		pres_param.BackBufferWidth  = rect.right - rect.left;
		pres_param.BackBufferHeight = rect.bottom - rect.top;
		pres_param.hDeviceWindow = m_hTextWnd;
		pres_param.AutoDepthStencilFormat = D3DFMT_D16;
		pres_param.EnableAutoDepthStencil = FALSE;
		pres_param.SwapEffect = D3DSWAPEFFECT_DISCARD;
		pres_param.MultiSampleType = D3DMULTISAMPLE_NONE;
		pres_param.Flags = 0;
		pres_param.FullScreen_RefreshRateInHz = 0;
		pres_param.PresentationInterval       = D3DPRESENT_INTERVAL_IMMEDIATE;//D3DPRESENT_INTERVAL_ONE;//D3DPRESENT_INTERVAL_IMMEDIATE;//m_current_mode.allow_page_tearing ? D3DPRESENT_INTERVAL_IMMEDIATE : D3DPRESENT_INTERVAL_ONE;//D3DPRESENT_INTERVAL_IMMEDIATE;//D3DPRESENT_INTERVAL_ONE;
		pres_param.Windowed = TRUE;

		HRESULT hr;
		if (D3D_OK != (hr = m_vjd3d9->CreateDevice(ordinal_adapter,//D3DADAPTER_DEFAULT,
		                    D3DDEVTYPE_HAL,
		                    m_hTextWnd,
		                    D3DCREATE_SOFTWARE_VERTEXPROCESSING,
		                    &pres_param,
		                    &m_vjd3d9_device)))
		{
			m_vjd3d9_device = NULL;
			MessageBoxW(m_lpDX->GetHwnd(), LoadResString(IDS_ERROR_CREATING_D3D_DEVICE_FOR_VJ_MODE),
					   LoadResString(IDS_MILKDROP_ERROR, title, 64),
					   MB_OK|MB_SETFOREGROUND|MB_TOPMOST);
			return false;
		}

		if (!AllocateFonts(m_vjd3d9_device))
			return false;

		if (m_fix_slow_text)    // note that when not doing vj mode, m_lpDDSText is allocated in AllocateDX9Stuff
			AllocateTextSurface();

		m_text.Finish();
		m_text.Init(m_vjd3d9_device, m_lpDDSText, 0);

		m_bClearVJWindow = true;
	}

	return true;
}

void CPluginShell::CleanUpVJStuff()
{
	// ALWAYS set the textures to NULL before releasing textures,
	// otherwise they might still have a hanging reference!
	if (m_lpDX && m_lpDX->m_lpDevice)
	{
		for (int i=0; i<16; i++)
			m_lpDX->m_lpDevice->SetTexture(i, NULL);
	}

	if (m_vjd3d9_device)
	{
		for (int i=0; i<16; i++)
			m_vjd3d9_device->SetTexture(i, NULL);
	}

	if (!m_vj_mode)
		return;

	// clean up VJ mode
	{
		CleanUpFonts();
		SafeRelease(m_lpDDSText);

		SafeRelease(m_vjd3d9_device);
		SafeRelease(m_vjd3d9);

		if (m_hTextWnd)
		{
			DestroyWindow(m_hTextWnd);
			m_hTextWnd = NULL;
		}

		if (m_bTextWindowClassRegistered)
		{
			UnregisterClass(TEXT_WINDOW_CLASSNAME,GetInstance()); // unregister window class
			m_bTextWindowClassRegistered = false;
		}
	}
}

int CPluginShell::AllocateFonts(IDirect3DDevice9* pDevice)
{
	// SURI: scale from the configured size, not from the measured height that is stored back
	// into nSize below (which would compound at every DPI change / device reset).
	UINT dpi = WindowDpi(m_lpDX ? m_lpDX->GetHwnd() : NULL);
	for (int k=0; k<NUM_BASIC_FONTS + NUM_EXTRA_FONTS; k++)
		if (m_fontbase[k] <= 0)
			m_fontbase[k] = m_fontinfo[k].nSize;

	// Create D3DX system font:
	for (int i=0; i<NUM_BASIC_FONTS + NUM_EXTRA_FONTS; i++)
	{
		int px = ScaleFontPixels(m_fontbase[i], dpi);
		if (D3DXCreateFontW(pDevice,  //m_font[i],
		                   px,
		                   px*4/10,
		                   m_fontinfo[i].bBold ? 900 : 400,
		                   1,  // mip levels
		                   m_fontinfo[i].bItalic,
		                   DEFAULT_CHARSET,
		                   OUT_DEFAULT_PRECIS,
		                   m_fontinfo[i].bAntiAliased ? ANTIALIASED_QUALITY : DEFAULT_QUALITY,
		                   DEFAULT_PITCH,
		                   m_fontinfo[i].szFace,
		                   &m_d3dx_font[i]
		                  ) != D3D_OK)
		{
			wchar_t title[64];
			MessageBoxW(m_lpDX ? m_lpDX->GetHwnd() : NULL, LoadResString(IDS_ERROR_CREATING_D3DX_FONTS),
					    LoadResString(IDS_MILKDROP_ERROR, title, 64),
					    MB_OK|MB_SETFOREGROUND|MB_TOPMOST);
			return false;
		}
	}

	// get actual font heights
	for (i=0; i<NUM_BASIC_FONTS + NUM_EXTRA_FONTS; i++)
	{
		RECT r;
		SetRect(&r, 0, 0, 1024, 1024);
		int h = m_d3dx_font[i]->DrawText(NULL, "M", -1, &r, DT_CALCRECT, 0xFFFFFFFF);
		if (h>0) m_fontinfo[i].nSize = h;
	}

	// SURI: fixed card fonts (design B); sizes are CSS-style em heights at 96 DPI.
	m_ui_dpi = dpi < 96 ? 96 : dpi;
	static const struct { const wchar_t* face; int px; int weight; } kUi[UIF_COUNT] = {
		{ L"Segoe UI", 22, 400 },         // UIF_TITLE
		{ L"Segoe UI", 17, 400 },         // UIF_META
		{ L"Segoe UI", 24, 400 },         // UIF_TOAST
		{ L"Segoe UI", 21, 400 },         // UIF_BODY
		{ L"Segoe UI", 16, 400 },         // UIF_SMALL
		{ L"Consolas", 18, 400 },         // UIF_KEY
		{ L"Segoe UI", 17, 600 },         // UIF_TAB_ON
		{ L"Segoe UI Symbol", 22, 400 },  // UIF_STARS: a star is shorter than a digit; 22 px stars match 17 px text
		{ L"Consolas", 0, 400 },          // UIF_CODE: cell height of SIMPLE_FONT, see below
		{ L"Consolas", 15, 400 },         // UIF_KEY_COMPACT
		{ L"Segoe UI", 17, 400 },         // UIF_BODY_COMPACT
	};
	for (int u=0; u<UIF_COUNT; u++)
	{
		// UIF_CODE uses SIMPLE_FONT's configured (DPI-scaled) cell height, so the code editor keeps
		// the menu text size but gets a monospace face (its cursor is drawn by swapping characters)
		int height = (u == UIF_CODE) ? ScaleFontPixels(m_fontbase[SIMPLE_FONT], dpi) : -UiPx(kUi[u].px);
		if (D3DXCreateFontW(pDevice, height, 0, kUi[u].weight, 1, FALSE, DEFAULT_CHARSET,
		                    OUT_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH, kUi[u].face, &m_ui_font[u]) != D3D_OK)
			m_ui_font[u] = NULL;   // callers check; that card text is then skipped
	}

	return true;
}

void CPluginShell::CleanUpFonts()
{
	for (int i=0; i<NUM_BASIC_FONTS + NUM_EXTRA_FONTS; i++)
		SafeRelease(m_d3dx_font[i]);
	for (int u=0; u<UIF_COUNT; u++)   // SURI
		SafeRelease(m_ui_font[u]);
}

// SURI: WM_DPICHANGED
void CPluginShell::ReallocateFontsForDpi()
{
	if (m_vj_mode || !m_lpDX || !m_lpDX->m_lpDevice)
		return;
	CleanUpFonts();
	AllocateFonts(m_lpDX->m_lpDevice);
	OnFontsReallocated();   // SURI: the song-title font follows the new SONGTITLE_FONT size
}

void CPluginShell::AllocateTextSurface()
{
	IDirect3DDevice9 *pDevice = m_vjd3d9_device ? m_vjd3d9_device : GetDevice();
	int w = m_vjd3d9_device ? m_nTextWndWidth  : GetWidth() ;
	int h = m_vjd3d9_device ? m_nTextWndHeight : GetHeight();

	if (D3D_OK != D3DXCreateTexture(pDevice, w, h, 1, D3DUSAGE_RENDERTARGET, GetBackBufFormat(), D3DPOOL_DEFAULT, &m_lpDDSText))
		m_lpDDSText = NULL; // OK if there's not enough mem for it!
	else
	{
		// if m_lpDDSText doesn't cover enough of screen, cancel it.
		D3DSURFACE_DESC desc;
		if (D3D_OK == m_lpDDSText->GetLevelDesc(0, &desc))
		{
			if ((desc.Width  < 256 && w >= 256)  ||
			    (desc.Height < 256 && h >= 256)  ||
			    (desc.Width /(float)w < 0.74f) ||
			    (desc.Height/(float)h < 0.74f)
			   )
			{
				m_lpDDSText->Release();
				m_lpDDSText = NULL;
			}
		}
	}
}

int CPluginShell::AllocateDX9Stuff()
{
	if (!m_vj_mode)
	{
		AllocateFonts(m_lpDX->m_lpDevice);
		if (m_fix_slow_text)    // note that when not doing vj mode, m_lpDDSText is allocated in AllocateDX9Stuff
			AllocateTextSurface();
	}

	int ret = AllocateMyDX9Stuff();

	if (!m_vj_mode)
	{
		m_text.Finish();
		m_text.Init(GetDevice(), m_lpDDSText, 1);
	}

	return ret;
}

void CPluginShell::CleanUpDX9Stuff(int final_cleanup)
{
	// ALWAYS unbind the textures before releasing textures,
	// otherwise they might still have a hanging reference!
	if (m_lpDX && m_lpDX->m_lpDevice)
	{
		for (int i=0; i<16; i++)
			m_lpDX->m_lpDevice->SetTexture(i, NULL);
	}

	if (!m_vj_mode)
	{
		for (int i=0; i<NUM_BASIC_FONTS + NUM_EXTRA_FONTS; i++)
			SafeRelease(m_d3dx_font[i]);
		for (int u=0; u<UIF_COUNT; u++)   // SURI
			SafeRelease(m_ui_font[u]);
		SafeRelease(m_lpDDSText);
	}

	CleanUpMyDX9Stuff(final_cleanup);
}

void CPluginShell::OnUserResizeTextWindow()
{
	// Update window properties
	RECT w, c;
	GetWindowRect(m_hTextWnd, &w);
	GetClientRect(m_hTextWnd, &c);

	WINDOWPLACEMENT wp;
	ZeroMemory(&wp, sizeof(wp));
	wp.length = sizeof(wp);
	GetWindowPlacement(m_hTextWnd, &wp);

	// convert client rect from client coords to screen coords:
	// (window rect is already in screen coords...)
	POINT p;
	p.x = c.left;
	p.y = c.top;
	if (ClientToScreen(m_hTextWnd, &p))
	{
		c.left += p.x;
		c.right += p.x;
		c.top += p.y;
		c.bottom += p.y;
	}

	if (wp.showCmd != SW_SHOWMINIMIZED)
	{
		if (m_nTextWndWidth  != c.right-c.left ||
		    m_nTextWndHeight != c.bottom-c.top)
		{
			CleanUpVJStuff();
			if (!InitVJStuff(&c))
			{
				SuggestHowToFreeSomeMem();
				m_lpDX->m_ready = false;   // flag to exit
				return;
			}
		}

		// save the new window position:
		//if (wp.showCmd==SW_SHOWNORMAL)
		//    SaveTextWindowPos();
	}
}

void CPluginShell::OnUserResizeWindow()
{
	// Update window properties
	RECT w, c;
	GetWindowRect(m_lpDX->GetHwnd(), &w);
	GetClientRect(m_lpDX->GetHwnd(), &c);

	WINDOWPLACEMENT wp;
	ZeroMemory(&wp, sizeof(wp));
	wp.length = sizeof(wp);
	GetWindowPlacement(m_lpDX->GetHwnd(), &wp);

	// convert client rect from client coords to screen coords:
	// (window rect is already in screen coords...)
	POINT p;
	p.x = c.left;
	p.y = c.top;
	if (ClientToScreen(m_lpDX->GetHwnd(), &p))
	{
		c.left += p.x;
		c.right += p.x;
		c.top += p.y;
		c.bottom += p.y;
	}

	if (wp.showCmd != SW_SHOWMINIMIZED)
	{
		int new_REAL_client_w = c.right-c.left;
		int new_REAL_client_h = c.bottom-c.top;

		// kiv: could we just resize when the *snapped* w/h changes?  slightly more ideal...
		if (m_lpDX->m_REAL_client_width  != new_REAL_client_w ||
		    m_lpDX->m_REAL_client_height != new_REAL_client_h)
		{
			CleanUpDX9Stuff(0);
			if (!m_lpDX->OnUserResizeWindow(&w, &c))
			{
				// note: a basic warning messagebox will have already been given.
				// now suggest specific advice on how to regain more video memory:
				SuggestHowToFreeSomeMem();
				return;
			}
			if (!AllocateDX9Stuff())
			{
				m_lpDX->m_ready = false;   // flag to exit
				return;
			}
		}

		// save the new window position:
		if (wp.showCmd==SW_SHOWNORMAL)
			m_lpDX->SaveWindow();
	}
}

void CPluginShell::StuffParams(DXCONTEXT_PARAMS *pParams)
{
	pParams->display_mode = m_disp_mode_fs;
	pParams->nbackbuf = 1;
	pParams->m_dualhead_horz = m_dualhead_horz;
	pParams->m_dualhead_vert = m_dualhead_vert;
	pParams->m_skin = m_skin;
    pParams->allow_page_tearing = m_allow_page_tearing_w;
    pParams->adapter_guid = m_adapter_guid_windowed;
    pParams->multisamp = m_multisample_windowed;
    strcpy(pParams->adapter_devicename, m_adapter_devicename_windowed);
	pParams->parent_window = NULL;
}

int CPluginShell::InitDirectX(LPDIRECT3DDEVICE9 device, D3DPRESENT_PARAMETERS* d3dpp, HWND hwnd)
{
    if (device) {
        m_lpDX = new DXContext(device, d3dpp, hwnd, m_szConfigIniFile);
    }

	if (!m_lpDX)
	{
		wchar_t title[64];
		MessageBoxW(NULL, LoadResString(IDS_UNABLE_TO_INIT_DXCONTEXT),
				    LoadResString(IDS_MILKDROP_ERROR, title, 64),
				    MB_OK|MB_SETFOREGROUND|MB_TOPMOST);
		return FALSE;
	}

	if (m_lpDX->m_lastErr != S_OK)
	{
		// warning messagebox will have already been given
		delete m_lpDX;
		return FALSE;
	}

	// initialize graphics
	DXCONTEXT_PARAMS params;
	StuffParams(&params);

	if (!m_lpDX->StartOrRestartDevice(&params))
	{
		// note: a basic warning messagebox will have already been given.

		if (m_lpDX->m_lastErr == DXC_ERR_CREATEDEV_PROBABLY_OUTOFVIDEOMEMORY)
		{
			// suggest specific advice on how to regain more video memory:
			SuggestHowToFreeSomeMem();
		}

		delete m_lpDX;
		m_lpDX = NULL;
		return FALSE;
	}

	return TRUE;
}

void CPluginShell::CleanUpDirectX()
{
	SafeDelete(m_lpDX);
}

int CPluginShell::PluginPreInitialize(HWND hParentWnd, HINSTANCE hInstance)
{
	// PROTECTED SETTINGS (also see 'private' settings, below)
	m_start_fullscreen      = 0;
	m_fake_fullscreen_mode  = 0;
	m_max_fps_fs            = 60;
	m_max_fps_w             = 60;
	m_show_press_f1_msg     = 1;
	m_allow_page_tearing_w  = 1;
	m_allow_page_tearing_fs = 0;
	m_dualhead_horz         = 2;
	m_dualhead_vert         = 1;
	m_save_cpu              = 1;
	m_skin                  = 1;
	m_fix_slow_text         = 0;

	// initialize font settings:
	wcscpy(m_fontinfo[SIMPLE_FONT    ].szFace,        SIMPLE_FONT_DEFAULT_FACE);
	m_fontinfo[SIMPLE_FONT    ].nSize        = SIMPLE_FONT_DEFAULT_SIZE ;
	m_fontinfo[SIMPLE_FONT    ].bBold        = SIMPLE_FONT_DEFAULT_BOLD ;
	m_fontinfo[SIMPLE_FONT    ].bItalic      = SIMPLE_FONT_DEFAULT_ITAL ;
	m_fontinfo[SIMPLE_FONT    ].bAntiAliased = SIMPLE_FONT_DEFAULT_AA   ;
	wcscpy(m_fontinfo[DECORATIVE_FONT].szFace,        DECORATIVE_FONT_DEFAULT_FACE);
	m_fontinfo[DECORATIVE_FONT].nSize        = DECORATIVE_FONT_DEFAULT_SIZE;
	m_fontinfo[DECORATIVE_FONT].bBold        = DECORATIVE_FONT_DEFAULT_BOLD;
	m_fontinfo[DECORATIVE_FONT].bItalic      = DECORATIVE_FONT_DEFAULT_ITAL;
	m_fontinfo[DECORATIVE_FONT].bAntiAliased = DECORATIVE_FONT_DEFAULT_AA  ;
	wcscpy(m_fontinfo[HELPSCREEN_FONT].szFace,        HELPSCREEN_FONT_DEFAULT_FACE);
	m_fontinfo[HELPSCREEN_FONT].nSize        = HELPSCREEN_FONT_DEFAULT_SIZE;
	m_fontinfo[HELPSCREEN_FONT].bBold        = HELPSCREEN_FONT_DEFAULT_BOLD;
	m_fontinfo[HELPSCREEN_FONT].bItalic      = HELPSCREEN_FONT_DEFAULT_ITAL;
	m_fontinfo[HELPSCREEN_FONT].bAntiAliased = HELPSCREEN_FONT_DEFAULT_AA  ;
	wcscpy(m_fontinfo[PLAYLIST_FONT  ].szFace,        PLAYLIST_FONT_DEFAULT_FACE);
	m_fontinfo[PLAYLIST_FONT  ].nSize        = PLAYLIST_FONT_DEFAULT_SIZE;
	m_fontinfo[PLAYLIST_FONT  ].bBold        = PLAYLIST_FONT_DEFAULT_BOLD;
	m_fontinfo[PLAYLIST_FONT  ].bItalic      = PLAYLIST_FONT_DEFAULT_ITAL;
	m_fontinfo[PLAYLIST_FONT  ].bAntiAliased = PLAYLIST_FONT_DEFAULT_AA  ;

#if (NUM_EXTRA_FONTS >= 1)
	wcscpy(m_fontinfo[NUM_BASIC_FONTS + 0].szFace,        EXTRA_FONT_1_DEFAULT_FACE);
	m_fontinfo[NUM_BASIC_FONTS + 0].nSize        = EXTRA_FONT_1_DEFAULT_SIZE;
	m_fontinfo[NUM_BASIC_FONTS + 0].bBold        = EXTRA_FONT_1_DEFAULT_BOLD;
	m_fontinfo[NUM_BASIC_FONTS + 0].bItalic      = EXTRA_FONT_1_DEFAULT_ITAL;
	m_fontinfo[NUM_BASIC_FONTS + 0].bAntiAliased = EXTRA_FONT_1_DEFAULT_AA;
#endif
#if (NUM_EXTRA_FONTS >= 2)
	wcscpy(m_fontinfo[NUM_BASIC_FONTS + 1].szFace,        EXTRA_FONT_2_DEFAULT_FACE);
	m_fontinfo[NUM_BASIC_FONTS + 1].nSize        = EXTRA_FONT_2_DEFAULT_SIZE;
	m_fontinfo[NUM_BASIC_FONTS + 1].bBold        = EXTRA_FONT_2_DEFAULT_BOLD;
	m_fontinfo[NUM_BASIC_FONTS + 1].bItalic      = EXTRA_FONT_2_DEFAULT_ITAL;
	m_fontinfo[NUM_BASIC_FONTS + 1].bAntiAliased = EXTRA_FONT_2_DEFAULT_AA;
#endif
#if (NUM_EXTRA_FONTS >= 3)
	strcpy(m_fontinfo[NUM_BASIC_FONTS + 2].szFace,        EXTRA_FONT_3_DEFAULT_FACE);
	m_fontinfo[NUM_BASIC_FONTS + 2].nSize        = EXTRA_FONT_3_DEFAULT_SIZE;
	m_fontinfo[NUM_BASIC_FONTS + 2].bBold        = EXTRA_FONT_3_DEFAULT_BOLD;
	m_fontinfo[NUM_BASIC_FONTS + 2].bItalic      = EXTRA_FONT_3_DEFAULT_ITAL;
	m_fontinfo[NUM_BASIC_FONTS + 2].bAntiAliased = EXTRA_FONT_3_DEFAULT_AA;
#endif
#if (NUM_EXTRA_FONTS >= 4)
	strcpy(m_fontinfo[NUM_BASIC_FONTS + 3].szFace,        EXTRA_FONT_4_DEFAULT_FACE);
	m_fontinfo[NUM_BASIC_FONTS + 3].nSize        = EXTRA_FONT_4_DEFAULT_SIZE;
	m_fontinfo[NUM_BASIC_FONTS + 3].bBold        = EXTRA_FONT_4_DEFAULT_BOLD;
	m_fontinfo[NUM_BASIC_FONTS + 3].bItalic      = EXTRA_FONT_4_DEFAULT_ITAL;
	m_fontinfo[NUM_BASIC_FONTS + 3].bAntiAliased = EXTRA_FONT_4_DEFAULT_AA;
#endif
#if (NUM_EXTRA_FONTS >= 5)
	strcpy(m_fontinfo[NUM_BASIC_FONTS + 4].szFace,        EXTRA_FONT_5_DEFAULT_FACE);
	m_fontinfo[NUM_BASIC_FONTS + 4].nSize        = EXTRA_FONT_5_DEFAULT_SIZE;
	m_fontinfo[NUM_BASIC_FONTS + 4].bBold        = EXTRA_FONT_5_DEFAULT_BOLD;
	m_fontinfo[NUM_BASIC_FONTS + 4].bItalic      = EXTRA_FONT_5_DEFAULT_ITAL;
	m_fontinfo[NUM_BASIC_FONTS + 4].bAntiAliased = EXTRA_FONT_5_DEFAULT_AA;
#endif

	m_disp_mode_fs.Width = DEFAULT_FULLSCREEN_WIDTH;
	m_disp_mode_fs.Height = DEFAULT_FULLSCREEN_HEIGHT;
	m_disp_mode_fs.Format = D3DFMT_UNKNOWN;
	m_disp_mode_fs.RefreshRate = 60;
	// better yet - in case there is no config INI file saved yet, use the current display mode (if detectable) as the default fullscreen res:
	DEVMODE dm;
	dm.dmSize = sizeof(dm);
	dm.dmDriverExtra = 0;
	if (EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &dm))
	{
		m_disp_mode_fs.Width  = dm.dmPelsWidth;
		m_disp_mode_fs.Height = dm.dmPelsHeight;
		m_disp_mode_fs.RefreshRate = dm.dmDisplayFrequency;
		m_disp_mode_fs.Format = (dm.dmBitsPerPel==16) ? D3DFMT_R5G6B5 : D3DFMT_X8R8G8B8;
	}

	// PROTECTED STRUCTURES/POINTERS
	for (int i=0; i<NUM_BASIC_FONTS + NUM_EXTRA_FONTS; i++)
		m_d3dx_font[i] = NULL;
	for (int i=0; i<UIF_COUNT; i++)   // SURI
		m_ui_font[i] = NULL;
	m_ui_dpi = 96;
	m_lpDDSText = NULL;
	ZeroMemory(&m_sound, sizeof(td_soundinfo));
	for (int ch=0; ch<2; ch++)
		for (int i=0; i<3; i++)
		{
			m_sound.infinite_avg[ch][i] = m_sound.avg[ch][i] = m_sound.med_avg[ch][i] = m_sound.long_avg[ch][i] = 1.0f;
		}

	// GENERAL PRIVATE STUFF
	//m_screenmode: set at end (derived setting)
	m_frame = 0;
	m_time = 0;
	m_fps = 60;
	m_hInstance = hInstance;
	m_lpDX = NULL;
	m_szPluginsDirPath[0] = 0;  // will be set further down
	m_szConfigIniFile[0] = 0;  // will be set further down
	// m_szPluginsDirPath:

	// get path to INI file & read in prefs/settings right away, so DumpMsg works!
	GetModuleFileNameW(m_hInstance, m_szPluginsDirPath, MAX_PATH);
	wchar_t *p = m_szPluginsDirPath + wcslen(m_szPluginsDirPath);
	while (p >= m_szPluginsDirPath && *p != L'\\') p--;
	if (++p >= m_szPluginsDirPath) *p = 0;
    swprintf(m_szConfigIniFile, L"%s%s", m_szPluginsDirPath, INIFILE);
	lstrcpyn(m_szConfigIniFileA,AutoCharFn(m_szConfigIniFile),MAX_PATH);

	// PRIVATE SETTINGS
	m_multisample_fullscreen = D3DMULTISAMPLE_NONE;
	m_multisample_windowed = D3DMULTISAMPLE_NONE;
	ZeroMemory(&m_adapter_guid_fullscreen, sizeof(GUID));
	ZeroMemory(&m_adapter_guid_windowed , sizeof(GUID));
	m_adapter_devicename_windowed[0]   = 0;
	m_adapter_devicename_fullscreen[0] = 0;


	// PRIVATE RUNTIME SETTINGS
	m_lost_focus = 0;
	m_hidden     = 0;
	m_resizing   = 0;
	m_show_help  = 0;
	m_help_page  = 0;
	for (int fb=0; fb<NUM_BASIC_FONTS + NUM_EXTRA_FONTS; fb++) m_fontbase[fb] = 0;
	m_exiting    = 0;
	m_upper_left_corner_y = 0;
	m_lower_left_corner_y = 0;
	m_upper_right_corner_y = 0;
	m_lower_right_corner_y = 0;
	m_left_edge = 0;
	m_right_edge = 0;
	m_force_accept_WM_WINDOWPOSCHANGING = 0;

	// PRIVATE - GDI STUFF
	for (i=0; i<NUM_BASIC_FONTS + NUM_EXTRA_FONTS; i++)
		m_font[i] = NULL;

	// PRIVATE - MORE TIMEKEEPING
	m_last_raw_time = 0;
	memset(m_time_hist, 0, sizeof(m_time_hist));
	m_time_hist_pos = 0;
	if (!QueryPerformanceFrequency(&m_high_perf_timer_freq))
		m_high_perf_timer_freq.QuadPart = 0;
	m_prev_end_of_frame.QuadPart = 0;

	// PRIVATE AUDIO PROCESSING DATA
	//(m_fftobj needs no init)
	memset(m_oldwave[0], 0, sizeof(float)*576);
	memset(m_oldwave[1], 0, sizeof(float)*576);
	m_prev_align_offset[0] = 0;
	m_prev_align_offset[1] = 0;
	m_align_weights_ready = 0;

	// SEPARATE TEXT WINDOW (FOR VJ MODE)
	m_vj_mode       = 0;
	m_hidden_textwnd = 0;
	m_resizing_textwnd = 0;
	m_hTextWnd		= NULL;
	m_nTextWndWidth = 0;
	m_nTextWndHeight = 0;
	m_bTextWindowClassRegistered = false;
	m_vjd3d9        = NULL;
	m_vjd3d9_device = NULL;

	//-----

	OverrideDefaults();
	ReadConfig();
	MyPreInitialize();
	MyReadConfig();

	//-----

	return TRUE;
}

int CPluginShell::PluginInitialize(LPDIRECT3DDEVICE9 device, D3DPRESENT_PARAMETERS* d3dpp, HWND hwnd, int iWidth, int iHeight)
{
    // note: initialize GDI before DirectX.  Also separate them because
    // when we change windowed<->fullscreen, or lose the device and restore it,
    // we don't want to mess with any (persistent) GDI stuff.

    if (!InitDirectX(device, d3dpp, hwnd)) return FALSE;  // gives its own error messages
    m_lpDX->m_client_width = iWidth;
    m_lpDX->m_client_height = iHeight;
    m_lpDX->m_REAL_client_height = iHeight;
    m_lpDX->m_REAL_client_width = iWidth;

    if (!InitNondx9Stuff()) return FALSE;  // gives its own error messages
    if (!AllocateDX9Stuff()) return FALSE;  // gives its own error messages
	if (!InitVJStuff()) return FALSE;

    return TRUE;
}

void CPluginShell::PluginQuit()
{
	CleanUpVJStuff();
	CleanUpDX9Stuff(1);
	CleanUpNondx9Stuff();
	CleanUpDirectX();
}

wchar_t* BuildSettingName(wchar_t* name, int number){
static wchar_t temp[64];
	swprintf(temp, L"%s%d", name, number);
	return temp;
}

void CPluginShell::READ_FONT(int n){
	GetPrivateProfileStringW(L"settings",BuildSettingName(L"szFontFace",n),m_fontinfo[n].szFace,m_fontinfo[n].szFace,sizeof(m_fontinfo[n].szFace), m_szConfigIniFile);
	m_fontinfo[n].nSize   = GetPrivateProfileIntW(L"settings",BuildSettingName(L"nFontSize",n),m_fontinfo[n].nSize  ,m_szConfigIniFile);
	m_fontinfo[n].bBold   = GetPrivateProfileIntW(L"settings",BuildSettingName(L"bFontBold",n),m_fontinfo[n].bBold  ,m_szConfigIniFile);
	m_fontinfo[n].bItalic = GetPrivateProfileIntW(L"settings",BuildSettingName(L"bFontItalic",n),m_fontinfo[n].bItalic,m_szConfigIniFile);
	m_fontinfo[n].bAntiAliased = GetPrivateProfileIntW(L"settings",BuildSettingName(L"bFontAA",n),m_fontinfo[n].bItalic,m_szConfigIniFile);
}

void CPluginShell::ReadConfig()
{
	int old_ver    = GetPrivateProfileIntW(L"settings",L"version"   ,-1,m_szConfigIniFile);
	int old_subver = GetPrivateProfileIntW(L"settings",L"subversion",-1,m_szConfigIniFile);

	// nuke old settings from prev. version:
	if (old_ver < INT_VERSION)
		return;
	else if (old_subver < INT_SUBVERSION)
		return;

	m_multisample_fullscreen      = (D3DMULTISAMPLE_TYPE)GetPrivateProfileIntW(L"settings",L"multisample_fullscreen",m_multisample_fullscreen,m_szConfigIniFile);
	m_multisample_windowed        = (D3DMULTISAMPLE_TYPE)GetPrivateProfileIntW(L"settings",L"multisample_windowed"  ,m_multisample_windowed  ,m_szConfigIniFile);

	//GUID m_adapter_guid_fullscreen
	//GUID m_adapter_guid_windowed
	char str[256];
	GetPrivateProfileString("settings","adapter_guid_fullscreen","",str,sizeof(str)-1,m_szConfigIniFileA);
	TextToGuid(str, &m_adapter_guid_fullscreen);
	GetPrivateProfileString("settings","adapter_guid_windowed","",str,sizeof(str)-1,m_szConfigIniFileA);
	TextToGuid(str, &m_adapter_guid_windowed);
	GetPrivateProfileString("settings","adapter_devicename_fullscreen","",m_adapter_devicename_fullscreen,sizeof(m_adapter_devicename_fullscreen)-1,m_szConfigIniFileA);
	GetPrivateProfileString("settings","adapter_devicename_windowed",  "",m_adapter_devicename_windowed  ,sizeof(m_adapter_devicename_windowed)-1,m_szConfigIniFileA);

	// FONTS
	READ_FONT(0);
	READ_FONT(1);
	READ_FONT(2);
	READ_FONT(3);
#if (NUM_EXTRA_FONTS >= 1)
	READ_FONT(4);
#endif
#if (NUM_EXTRA_FONTS >= 2)
	READ_FONT(5);
#endif
#if (NUM_EXTRA_FONTS >= 3)
	READ_FONT(6);
#endif
#if (NUM_EXTRA_FONTS >= 4)
	READ_FONT(7);
#endif
#if (NUM_EXTRA_FONTS >= 5)
	READ_FONT(8);
#endif

	m_start_fullscreen     = GetPrivateProfileIntW(L"settings",L"start_fullscreen",m_start_fullscreen,m_szConfigIniFile);
	m_fake_fullscreen_mode = GetPrivateProfileIntW(L"settings",L"fake_fullscreen_mode",m_fake_fullscreen_mode,m_szConfigIniFile);
	m_max_fps_fs           = GetPrivateProfileIntW(L"settings",L"max_fps_fs",m_max_fps_fs,m_szConfigIniFile);
	m_max_fps_w            = GetPrivateProfileIntW(L"settings",L"max_fps_w" ,m_max_fps_w ,m_szConfigIniFile);
	m_show_press_f1_msg    = GetPrivateProfileIntW(L"settings",L"show_press_f1_msg",m_show_press_f1_msg,m_szConfigIniFile);
	m_allow_page_tearing_w = GetPrivateProfileIntW(L"settings",L"allow_page_tearing_w",m_allow_page_tearing_w,m_szConfigIniFile);
	m_allow_page_tearing_fs= GetPrivateProfileIntW(L"settings",L"allow_page_tearing_fs",m_allow_page_tearing_fs,m_szConfigIniFile);
	m_dualhead_horz        = GetPrivateProfileIntW(L"settings",L"dualhead_horz",m_dualhead_horz,m_szConfigIniFile);
	m_dualhead_vert        = GetPrivateProfileIntW(L"settings",L"dualhead_vert",m_dualhead_vert,m_szConfigIniFile);
	m_save_cpu             = GetPrivateProfileIntW(L"settings",L"save_cpu",m_save_cpu,m_szConfigIniFile);
	m_skin                 = GetPrivateProfileIntW(L"settings",L"skin",m_skin,m_szConfigIniFile);
	m_fix_slow_text        = GetPrivateProfileIntW(L"settings",L"fix_slow_text",m_fix_slow_text,m_szConfigIniFile);
	m_vj_mode              = GetPrivateProfileBoolW(L"settings",L"vj_mode",m_vj_mode,m_szConfigIniFile);

	//D3DDISPLAYMODE m_fs_disp_mode
	m_disp_mode_fs.Width           = GetPrivateProfileIntW(L"settings",L"disp_mode_fs_w", m_disp_mode_fs.Width           ,m_szConfigIniFile);
	m_disp_mode_fs.Height           = GetPrivateProfileIntW(L"settings",L"disp_mode_fs_h",m_disp_mode_fs.Height          ,m_szConfigIniFile);
	m_disp_mode_fs.RefreshRate = GetPrivateProfileIntW(L"settings",L"disp_mode_fs_r",m_disp_mode_fs.RefreshRate,m_szConfigIniFile);
	m_disp_mode_fs.Format      = (D3DFORMAT)GetPrivateProfileIntW(L"settings",L"disp_mode_fs_f",m_disp_mode_fs.Format     ,m_szConfigIniFile);

	// note: we don't call MyReadConfig() yet, because we
	// want to completely finish CPluginShell's preinit (and ReadConfig)
	// before calling CPlugin's preinit and ReadConfig.
}

void CPluginShell::WRITE_FONT(int n){
	WritePrivateProfileStringW(L"settings",BuildSettingName(L"szFontFace",n),m_fontinfo[n].szFace,m_szConfigIniFile);
	WritePrivateProfileIntW(m_fontinfo[n].bBold,  BuildSettingName(L"bFontBold",n),   m_szConfigIniFile, L"settings");
	WritePrivateProfileIntW(m_fontinfo[n].bItalic,BuildSettingName(L"bFontItalic",n), m_szConfigIniFile, L"settings");
	WritePrivateProfileIntW(m_fontbase[n] > 0 ? m_fontbase[n] : m_fontinfo[n].nSize,  BuildSettingName(L"nFontSize",n),   m_szConfigIniFile, L"settings"); // SURI: configured size, not the scaled measured one
	WritePrivateProfileIntW(m_fontinfo[n].bAntiAliased, BuildSettingName(L"bFontAA",n),m_szConfigIniFile, L"settings");
}

void CPluginShell::WriteConfig()
{
	WritePrivateProfileIntW((int)m_multisample_fullscreen,L"multisample_fullscreen",m_szConfigIniFile,L"settings");
	WritePrivateProfileIntW((int)m_multisample_windowed  ,L"multisample_windowed"  ,m_szConfigIniFile,L"settings");

	//GUID m_adapter_guid_fullscreen
	//GUID m_adapter_guid_windowed
	char str[256];
	GuidToText(&m_adapter_guid_fullscreen, str, sizeof(str));
	WritePrivateProfileString("settings","adapter_guid_fullscreen",str,m_szConfigIniFileA);
	GuidToText(&m_adapter_guid_windowed,   str, sizeof(str));
	WritePrivateProfileString("settings","adapter_guid_windowed"  ,str,m_szConfigIniFileA);
	WritePrivateProfileString("settings","adapter_devicename_fullscreen",m_adapter_devicename_fullscreen,m_szConfigIniFileA);
	WritePrivateProfileString("settings","adapter_devicename_windowed"  ,m_adapter_devicename_windowed  ,m_szConfigIniFileA);

	// FONTS
	WRITE_FONT(0);
	WRITE_FONT(1);
	WRITE_FONT(2);
	WRITE_FONT(3);
#if (NUM_EXTRA_FONTS >= 1)
	WRITE_FONT(4);
#endif
#if (NUM_EXTRA_FONTS >= 2)
	WRITE_FONT(5);
#endif
#if (NUM_EXTRA_FONTS >= 3)
	WRITE_FONT(6);
#endif
#if (NUM_EXTRA_FONTS >= 4)
	WRITE_FONT(7);
#endif
#if (NUM_EXTRA_FONTS >= 5)
	WRITE_FONT(8);
#endif

	WritePrivateProfileIntW(m_start_fullscreen,L"start_fullscreen",m_szConfigIniFile,L"settings");
	WritePrivateProfileIntW(m_fake_fullscreen_mode,L"fake_fullscreen_mode",m_szConfigIniFile,L"settings");
	WritePrivateProfileIntW(m_max_fps_fs,L"max_fps_fs",m_szConfigIniFile,L"settings");
	WritePrivateProfileIntW(m_max_fps_w ,L"max_fps_w" ,m_szConfigIniFile,L"settings");
	WritePrivateProfileIntW(m_show_press_f1_msg,L"show_press_f1_msg",m_szConfigIniFile,L"settings");
	WritePrivateProfileIntW(m_allow_page_tearing_w,L"allow_page_tearing_w",m_szConfigIniFile,L"settings");
	WritePrivateProfileIntW(m_allow_page_tearing_fs,L"allow_page_tearing_fs",m_szConfigIniFile,L"settings");
	WritePrivateProfileIntW(m_dualhead_horz,L"dualhead_horz",m_szConfigIniFile,L"settings");
	WritePrivateProfileIntW(m_dualhead_vert,L"dualhead_vert",m_szConfigIniFile,L"settings");
	WritePrivateProfileIntW(m_save_cpu,L"save_cpu",m_szConfigIniFile,L"settings");
	WritePrivateProfileIntW(m_skin,L"skin",m_szConfigIniFile,L"settings");
	WritePrivateProfileIntW(m_fix_slow_text,L"fix_slow_text",m_szConfigIniFile,L"settings");
	WritePrivateProfileIntW(m_vj_mode,L"vj_mode",m_szConfigIniFile,L"settings");

	//D3DDISPLAYMODE m_fs_disp_mode
	WritePrivateProfileIntW(m_disp_mode_fs.Width          ,L"disp_mode_fs_w",m_szConfigIniFile,L"settings");
	WritePrivateProfileIntW(m_disp_mode_fs.Height          ,L"disp_mode_fs_h",m_szConfigIniFile,L"settings");
	WritePrivateProfileIntW(m_disp_mode_fs.RefreshRate,L"disp_mode_fs_r",m_szConfigIniFile,L"settings");
	WritePrivateProfileIntW(m_disp_mode_fs.Format     ,L"disp_mode_fs_f",m_szConfigIniFile,L"settings");

	WritePrivateProfileIntW(INT_VERSION            ,L"version"    ,m_szConfigIniFile,L"settings");
	WritePrivateProfileIntW(INT_SUBVERSION         ,L"subversion" ,m_szConfigIniFile,L"settings");

	// finally, save the plugin's unique settings:
	MyWriteConfig();
}

//----------------------------------------------------------------------
//----------------------------------------------------------------------
//----------------------------------------------------------------------

int CPluginShell::PluginRender(unsigned char *pWaveL, unsigned char *pWaveR)//, unsigned char *pSpecL, unsigned char *pSpecR)
{
	// return FALSE here to quit

	if (!m_lpDX || !m_lpDX->m_ready)
	{
		// note: 'm_ready' will go false when a device reset fatally fails
		//       (for example, when user resizes window, or toggles fullscreen.)
		m_exiting = 1;
		return false;   // EXIT THE PLUGIN
	}

	if (m_hTextWnd)
		m_lost_focus = ((GetFocus() != GetPluginWindow()) && (GetFocus() != m_hTextWnd));
	else
		m_lost_focus = (GetFocus() != GetPluginWindow());

	if (m_hidden || m_resizing)
	{
		Sleep(30);
		return true;
	}

	// test for lost device
	// (this happens when device is fullscreen & user alt-tabs away,
	//  or when monitor power-saving kicks in)
	HRESULT hr = m_lpDX->m_lpDevice->TestCooperativeLevel();
	if (hr == D3DERR_DEVICENOTRESET)
	{
		// device WAS lost, and is now ready to be reset (and come back online):
		CleanUpDX9Stuff(0);
		if (m_lpDX->m_lpDevice->Reset(m_lpDX->m_d3dpp) != D3D_OK)
		{
			// note: a basic warning messagebox will have already been given.
			// now suggest specific advice on how to regain more video memory:
			if (m_lpDX->m_lastErr == DXC_ERR_CREATEDEV_PROBABLY_OUTOFVIDEOMEMORY)
				SuggestHowToFreeSomeMem();
			return false;  // EXIT THE PLUGIN
		}
		if (!AllocateDX9Stuff())
			return false;  // EXIT THE PLUGIN
	}
	else if (hr != D3D_OK)
	{
		// device is lost, and not yet ready to come back; sleep.
		Sleep(30);
		return true;
	}

	if (m_vjd3d9_device)
	{
		HRESULT hr = m_vjd3d9_device->TestCooperativeLevel();
		if (hr == D3DERR_DEVICENOTRESET)
		{
			RECT c;
			GetClientRect(m_hTextWnd, &c);

			POINT p;
			p.x = c.left;
			p.y = c.top;
			if (ClientToScreen(m_hTextWnd, &p))
			{
				c.left += p.x;
				c.right += p.x;
				c.top += p.y;
				c.bottom += p.y;
			}

			CleanUpVJStuff();
			if (!InitVJStuff(&c))
				return false;  // EXIT THE PLUGIN
		}
	}

	DoTime();
	AnalyzeNewSound(pWaveL, pWaveR);
	AlignWaves();

	DrawAndDisplay(0);

	EnforceMaxFPS();

	m_frame++;

	return true;
}

void CPluginShell::DrawAndDisplay(int redraw)
{
	int cx = m_vjd3d9_device ? m_nTextWndWidth  : m_lpDX->m_client_width;
	int cy = m_vjd3d9_device ? m_nTextWndHeight : m_lpDX->m_client_height;
	if (m_lpDDSText)
	{
		D3DSURFACE_DESC desc;
		if (D3D_OK == m_lpDDSText->GetLevelDesc(0, &desc))
		{
			cx = min(cx, (int)desc.Width);
			cy = min(cy, (int)desc.Height);
		}
	}
	const int margin = UiPx(TEXT_MARGIN);   // SURI: DPI-scaled, like the card sizes that build on it
	m_upper_left_corner_y  = margin + GetCanvasMarginY();
	m_upper_right_corner_y = margin + GetCanvasMarginY();
	m_lower_left_corner_y  = cy - margin - GetCanvasMarginY();
	m_lower_right_corner_y = cy - margin - GetCanvasMarginY();
	m_left_edge            = margin + GetCanvasMarginX();
	m_right_edge           = cx - margin - GetCanvasMarginX();

	if (D3D_OK==m_lpDX->m_lpDevice->BeginScene())
	{
		MyRenderFn(redraw);

		PrepareFor2DDrawing_B(GetDevice(), GetWidth(), GetHeight());

		if (!m_vjd3d9_device)   // in VJ mode, this renders to different context, so do it after BeginScene() on 2nd device.
			RenderBuiltInTextMsgs();    // to m_lpDDSText?
		MyRenderUI(&m_upper_left_corner_y, &m_upper_right_corner_y, &m_lower_left_corner_y, &m_lower_right_corner_y, m_left_edge, m_right_edge);

		if (!m_vjd3d9_device)
			m_text.DrawNow();

		// SURI: things that must sit above all queued text (toasts). m_text.DrawNow changes the
		// projection, so set up 2D drawing again first. (In VJ mode the overlay's text is queued
		// and drawn by the VJ device's DrawNow below.)
		PrepareFor2DDrawing_B(GetDevice(), GetWidth(), GetHeight());
		MyRenderOverlay();

		m_lpDX->m_lpDevice->EndScene();
	}

	// VJ Mode:
	if (m_vj_mode && m_vjd3d9_device && !m_hidden_textwnd && D3D_OK==m_vjd3d9_device->BeginScene())
	{
		if (!m_lpDDSText || m_bClearVJWindow)
			m_vjd3d9_device->Clear(0, 0, D3DCLEAR_TARGET, 0xFF000000, 1.0f, 0);
		m_bClearVJWindow = false;
		// note: when using debug DX runtime, textwnd will flash red/green after frame 4, if no text is drawn on a frame!

		RenderBuiltInTextMsgs();

		PrepareFor2DDrawing_B(m_vjd3d9_device, m_nTextWndWidth, m_nTextWndHeight);

		m_text.DrawNow();

		m_vjd3d9_device->EndScene();
	}

	if (m_lpDX->m_client_width != m_lpDX->m_REAL_client_width || m_lpDX->m_client_height != m_lpDX->m_REAL_client_height)
	{
		int real_w = m_lpDX->m_REAL_client_width;   // real client size, in pixels
		int real_h = m_lpDX->m_REAL_client_height;
		int fat_w = m_lpDX->m_client_width;         // oversized VS canvas size, in pixels
		int fat_h = m_lpDX->m_client_height;
		int extra_w = fat_w - real_w;
		int extra_h = fat_h - real_h;
		RECT src, dst;
		SetRect(&src, extra_w/2, extra_h/2, extra_w/2 + real_w, extra_h/2 + real_h);
		SetRect(&dst, 0, 0, real_w, real_h);
		m_lpDX->m_lpDevice->Present(&src, &dst,NULL,NULL);
	}
	else
		m_lpDX->m_lpDevice->Present(NULL,NULL,NULL,NULL);

	if (m_vjd3d9_device && !m_hidden_textwnd)
		m_vjd3d9_device->Present(NULL,NULL,NULL,NULL);
}

void CPluginShell::EnforceMaxFPS()
{
	int max_fps = m_max_fps_w;

	if (max_fps <= 0)
		return;

	float fps_lo = (float)max_fps;
	float fps_hi = (float)max_fps;

	if (m_save_cpu)
	{
		// Find the optimal lo/hi bounds for the fps
		// that will result in a maximum difference,
		// in the time for a single frame, of 0.003 seconds -
		// the assumed granularity for Sleep(1) -

		// Using this range of acceptable fps
		// will allow us to do (sloppy) fps limiting
		// using only Sleep(1), and never the
		// second half of it: Sleep(0) in a tight loop,
		// which sucks up the CPU (whereas Sleep(1)
		// leaves it idle).

		// The original equation:
		//   1/(max_fps*t1) = 1/(max*fps/t1) - 0.003
		// where:
		//   t1 > 0
		//   max_fps*t1 is the upper range for fps
		//   max_fps/t1 is the lower range for fps

		float a = 1;
		float b = -0.003f * max_fps;
		float c = -1.0f;
		float det = b*b - 4*a*c;
		if (det>0)
		{
			float t1 = (-b + sqrtf(det)) / (2*a);

			if (t1 > 1.0f)
			{
				fps_lo = max_fps / t1;
				fps_hi = max_fps * t1;
				// verify: now [1.0f/fps_lo - 1.0f/fps_hi] should equal 0.003 seconds.
				// note: allowing tolerance to go beyond these values for
				// fps_lo and fps_hi would gain nothing.
			}
		}
	}

	if (m_high_perf_timer_freq.QuadPart > 0)
	{
		LARGE_INTEGER t = { 0 };
		QueryPerformanceCounter(&t);

		if (m_prev_end_of_frame.QuadPart != 0)
		{
			int ticks_to_wait_lo = (int)((float)m_high_perf_timer_freq.QuadPart / (float)fps_hi);
			int ticks_to_wait_hi = (int)((float)m_high_perf_timer_freq.QuadPart / (float)fps_lo);
			int done = 0;
			int loops = 0;
			do
			{
				QueryPerformanceCounter(&t);

				__int64 t2 = t.QuadPart - m_prev_end_of_frame.QuadPart;
				if (t2 > 2147483000)
					done = 1;
				if (t.QuadPart < m_prev_end_of_frame.QuadPart)    // time wrap
					done = 1;

				// this is sloppy - if your freq. is high, this can overflow (to a (-) int) in just a few minutes
				// but it's ok, we have protection for that above.
				int ticks_passed = (int)(t.QuadPart - m_prev_end_of_frame.QuadPart);
				if (ticks_passed >= ticks_to_wait_lo)
					done = 1;

				if (!done)
				{
					// if > 0.01s left, do Sleep(1), which will actually sleep some
					//   steady amount of up to 3 ms (depending on the OS),
					//   and do so in a nice way (cpu meter drops; laptop battery spared).
					// otherwise, do a few Sleep(0)'s, which just give up the timeslice,
					//   but don't really save cpu or battery, but do pass a tiny
					//   amount of time.

					if (ticks_to_wait_hi - ticks_passed > (int)m_high_perf_timer_freq.QuadPart/100)
						Sleep(5);
					else if (ticks_to_wait_hi - ticks_passed > (int)m_high_perf_timer_freq.QuadPart/1000)
						Sleep(1);
					else
						for (int i=0; i<10; i++)
							Sleep(0);  // causes thread to give up its timeslice
				}
			}
			while (!done);
		}

		m_prev_end_of_frame = t;
	}
	else
	{
		Sleep(1000/max_fps);
	}
}

void CPluginShell::DoTime()
{
	if (m_frame==0)
	{
		m_fps = 60;
		m_time = 0;
		m_time_hist_pos = 0;
	}

	double new_raw_time;
	float elapsed;

	if (m_high_perf_timer_freq.QuadPart != 0)
	{
		// get high-precision time
		// precision: usually from 1..6 us (MICROseconds), depending on the cpu speed.
		// (higher cpu speeds tend to have better precision here)
		LARGE_INTEGER t;
		if (!QueryPerformanceCounter(&t))
		{
			m_high_perf_timer_freq.QuadPart = 0;   // something went wrong (exception thrown) -> revert to crappy timer
		}
		else
		{
			new_raw_time = (double)t.QuadPart;
			elapsed = (float)((new_raw_time - m_last_raw_time)/(double)m_high_perf_timer_freq.QuadPart);
		}
	}

	if (m_high_perf_timer_freq.QuadPart == 0)
	{
		// get low-precision time
		// precision: usually 1 ms (MILLIsecond) for win98, and 10 ms for win2k.
		new_raw_time = (double)(timeGetTime()*0.001);
		elapsed = (float)(new_raw_time - m_last_raw_time);
	}

	m_last_raw_time = new_raw_time;
	int slots_to_look_back = (m_high_perf_timer_freq.QuadPart==0) ? TIME_HIST_SLOTS : TIME_HIST_SLOTS/2;

	m_time += 1.0f/m_fps;

	// timekeeping goals:
	//    1. keep 'm_time' increasing SMOOTHLY: (smooth animation depends on it)
	//          m_time += 1.0f/m_fps;     // where m_fps is a bit damped
	//    2. keep m_time_hist[] 100% accurate (except for filtering out pauses),
	//       so that when we look take the difference between two entries,
	//       we get the real amount of time that passed between those 2 frames.
	//          m_time_hist[i] = m_last_raw_time + elapsed_corrected;

	if (m_frame > TIME_HIST_SLOTS)
	{
		if (m_fps < 60.0f)
			slots_to_look_back = (int)(slots_to_look_back*(0.1f + 0.9f*(m_fps/60.0f)));

		if (elapsed > 5.0f/m_fps || elapsed > 1.0f || elapsed < 0)
			elapsed = 1.0f / 30.0f;

		float old_hist_time = m_time_hist[(m_time_hist_pos - slots_to_look_back + TIME_HIST_SLOTS) % TIME_HIST_SLOTS];
		float new_hist_time = m_time_hist[(m_time_hist_pos - 1 + TIME_HIST_SLOTS) % TIME_HIST_SLOTS]
		                      + elapsed;

		m_time_hist[m_time_hist_pos] = new_hist_time;
		m_time_hist_pos = (m_time_hist_pos+1) % TIME_HIST_SLOTS;

		float new_fps = slots_to_look_back / (float)(new_hist_time - old_hist_time);
		float damping = (m_high_perf_timer_freq.QuadPart==0) ? 0.93f : 0.87f;

		// damp heavily, so that crappy timer precision doesn't make animation jerky
		if (fabsf(m_fps - new_fps) > 3.0f)
			m_fps = new_fps;
		else
			m_fps = damping*m_fps + (1-damping)*new_fps;
	}
	else
	{
		float damping = (m_high_perf_timer_freq.QuadPart==0) ? 0.8f : 0.6f;

		if (m_frame < 2)
			elapsed = 1.0f / 30.0f;
		else if (elapsed > 1.0f || elapsed < 0)
			elapsed = 1.0f / m_fps;

		float old_hist_time = m_time_hist[0];
		float new_hist_time = m_time_hist[(m_time_hist_pos - 1 + TIME_HIST_SLOTS) % TIME_HIST_SLOTS]
		                      + elapsed;

		m_time_hist[m_time_hist_pos] = new_hist_time;
		m_time_hist_pos = (m_time_hist_pos+1) % TIME_HIST_SLOTS;

		if (m_frame > 0)
		{
			float new_fps = (m_frame) / (new_hist_time - old_hist_time);
			m_fps = damping*m_fps + (1-damping)*new_fps;
		}
	}
}

void CPluginShell::AnalyzeNewSound(unsigned char *pWaveL, unsigned char *pWaveR)
{
	// we get 576 samples in from the audio capture.
	// the output of the fft has 'num_frequencies' samples,
	//   and represents the frequency range 0 hz - 22,050 hz.
	// usually, plugins only use half of this output (the range 0 hz - 11,025 hz),
	//   since >10 khz doesn't usually contribute much.

	int i;

	float temp_wave[2][576];

	int old_i = 0;
	for (i=0; i<576; i++)
	{
		m_sound.fWaveform[0][i] = (float)((pWaveL[i] ^ 128) - 128);
		m_sound.fWaveform[1][i] = (float)((pWaveR[i] ^ 128) - 128);

		// simulating single frequencies from 200 to 11,025 Hz:
		//float freq = 1.0f + 11050*(GetFrame() % 100)*0.01f;
		//m_sound.fWaveform[0][i] = 10*sinf(i*freq*6.28f/44100.0f);

		// damp the input into the FFT a bit, to reduce high-frequency noise:
		temp_wave[0][i] = 0.5f*(m_sound.fWaveform[0][i] + m_sound.fWaveform[0][old_i]);
		temp_wave[1][i] = 0.5f*(m_sound.fWaveform[1][i] + m_sound.fWaveform[1][old_i]);
		old_i = i;
	}

	m_fftobj.time_to_frequency_domain(temp_wave[0], m_sound.fSpectrum[0]);
	m_fftobj.time_to_frequency_domain(temp_wave[1], m_sound.fSpectrum[1]);

	// sum (left channel) spectrum up into 3 bands
	// [note: the new ranges do it so that the 3 bands are equally spaced, pitch-wise]
	float min_freq = 200.0f;
	float max_freq = 11025.0f;
	float net_octaves = (logf(max_freq/min_freq) / logf(2.0f));     // 5.7846348455575205777914165223593
	float octaves_per_band = net_octaves / 3.0f;                    // 1.9282116151858401925971388407864
	float mult = powf(2.0f, octaves_per_band); // each band's highest freq. divided by its lowest freq.; 3.805831305510122517035102576162
	// [to verify: min_freq * mult * mult * mult should equal max_freq.]
	for (int ch=0; ch<2; ch++)
	{
		for (i=0; i<3; i++)
		{
			// old guesswork code for this:
			//   float exp = 2.1f;
			//   int start = (int)(NUM_FREQUENCIES*0.5f*powf(i/3.0f, exp));
			//   int end   = (int)(NUM_FREQUENCIES*0.5f*powf((i+1)/3.0f, exp));
			// results:
			//          old range:      new range (ideal):
			//   bass:  0-1097          200-761
			//   mids:  1097-4705       761-2897
			//   treb:  4705-11025      2897-11025
			int start = (int)(NUM_FREQUENCIES * min_freq*powf(mult, (float)i)/11025.0f);
			int end   = (int)(NUM_FREQUENCIES * min_freq*powf(mult, (float)(i+1))/11025.0f);
			if (start < 0) start = 0;
			if (end > NUM_FREQUENCIES) end = NUM_FREQUENCIES;

			m_sound.imm[ch][i] = 0;
			for (int j=start; j<end; j++)
				m_sound.imm[ch][i] += m_sound.fSpectrum[ch][j];
			m_sound.imm[ch][i] /= (float)(end-start);
		}
	}

	// multiply by long-term, empirically-determined inverse averages:
	// (for a trial of 244 songs, 10 seconds each, somewhere in the 2nd or 3rd minute,
	//  the average levels were: 0.326781557	0.38087377	0.199888934
	for (ch=0; ch<2; ch++)
	{
		m_sound.imm[ch][0] /= 0.326781557f;//0.270f;
		m_sound.imm[ch][1] /= 0.380873770f;//0.343f;
		m_sound.imm[ch][2] /= 0.199888934f;//0.295f;
	}

	// do temporal blending to create attenuated and super-attenuated versions
	for (ch=0; ch<2; ch++)
	{
		for (i=0; i<3; i++)
		{
			// m_sound.avg[i]
			{
				float avg_mix;
				if (m_sound.imm[ch][i] > m_sound.avg[ch][i])
					avg_mix = AdjustRateToFPS(0.2f, 14.0f, m_fps);
				else
					avg_mix = AdjustRateToFPS(0.5f, 14.0f, m_fps);
				m_sound.avg[ch][i] = m_sound.avg[ch][i]*avg_mix + m_sound.imm[ch][i]*(1-avg_mix);
			}

			// m_sound.med_avg[i]
			// m_sound.long_avg[i]
			{
				float med_mix  = 0.91f;//0.800f + 0.11f*powf(t, 0.4f);    // primarily used for velocity_damping
				float long_mix = 0.96f;//0.800f + 0.16f*powf(t, 0.2f);    // primarily used for smoke plumes
				med_mix  = AdjustRateToFPS(med_mix, 14.0f, m_fps);
				long_mix = AdjustRateToFPS(long_mix, 14.0f, m_fps);
				m_sound.med_avg[ch][i]  =  m_sound.med_avg[ch][i]*(med_mix) + m_sound.imm[ch][i]*(1-med_mix);
				m_sound.long_avg[ch][i] = m_sound.long_avg[ch][i]*(long_mix) + m_sound.imm[ch][i]*(1-long_mix);
			}
		}
	}
}

void CPluginShell::PrepareFor2DDrawing_B(IDirect3DDevice9 *pDevice, int w, int h)
{
	// New 2D drawing area will have x,y coords in the range <-1,-1> .. <1,1>
	//         +--------+ Y=-1
	//         |        |
	//         | screen |             Z=0: front of scene
	//         |        |             Z=1: back of scene
	//         +--------+ Y=1
	//       X=-1      X=1
	// NOTE: After calling this, be sure to then call (at least):
	//  1. SetVertexShader()
	//  2. SetTexture(), if you need it
	// before rendering primitives!
	// Also, be sure your sprites have a z coordinate of 0.

	pDevice->SetRenderState(D3DRS_ZENABLE, TRUE);
	pDevice->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
	pDevice->SetRenderState(D3DRS_ZFUNC,     D3DCMP_LESSEQUAL);
	pDevice->SetRenderState(D3DRS_SHADEMODE, D3DSHADE_GOURAUD);
	pDevice->SetRenderState(D3DRS_FILLMODE,  D3DFILL_SOLID);
	pDevice->SetRenderState(D3DRS_FOGENABLE, FALSE);
	pDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
	pDevice->SetRenderState(D3DRS_CLIPPING, TRUE);
	pDevice->SetRenderState(D3DRS_LIGHTING, FALSE);
	pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
	pDevice->SetRenderState(D3DRS_LOCALVIEWER, FALSE);
	pDevice->SetRenderState(D3DRS_COLORVERTEX, TRUE);

	pDevice->SetTexture(0, NULL);
	pDevice->SetTexture(1, NULL);
	pDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);//D3DTEXF_LINEAR);
	pDevice->SetSamplerState(1, D3DSAMP_MAGFILTER, D3DTEXF_POINT);//D3DTEXF_LINEAR);
	pDevice->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
	pDevice->SetTextureStageState(1, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
	pDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
	pDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
	pDevice->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_CURRENT);
	pDevice->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);

	pDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
	pDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
	pDevice->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);

	pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);

	// set up for 2D drawing:
	{
		D3DXMATRIX Ortho2D;
		D3DXMATRIX Identity;

		D3DXMatrixOrthoLH(&Ortho2D, (float)w, (float)h, 0.0f, 1.0f);
		D3DXMatrixIdentity(&Identity);

		pDevice->SetTransform(D3DTS_PROJECTION, &Ortho2D);
		pDevice->SetTransform(D3DTS_WORLD, &Identity);
		pDevice->SetTransform(D3DTS_VIEW, &Identity);
	}
}

void CPluginShell::DrawDarkTranslucentBox(RECT* pr)
{
	// 'pr' is the rectangle that some text will occupy;
	// a black box will be drawn around it, plus a bit of extra margin space.

	if (m_vjd3d9_device)
		return;

	m_lpDX->m_lpDevice->SetVertexShader(NULL);
	m_lpDX->m_lpDevice->SetPixelShader(NULL);
	m_lpDX->m_lpDevice->SetFVF(SIMPLE_VERTEX_FORMAT);
	m_lpDX->m_lpDevice->SetTexture(0, NULL);

	m_lpDX->m_lpDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
	m_lpDX->m_lpDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
	m_lpDX->m_lpDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);

	m_lpDX->m_lpDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
	m_lpDX->m_lpDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
	m_lpDX->m_lpDevice->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
	m_lpDX->m_lpDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
	m_lpDX->m_lpDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);

	// set up a quad
	SIMPLEVERTEX verts[4];
	for (int i=0; i<4; i++)
	{
		verts[i].x = (i%2==0) ? (float)(-m_lpDX->m_client_width /2  + pr->left)  :
		             (float)(-m_lpDX->m_client_width /2  + pr->right);
		verts[i].y = (i/2==0) ? (float)-(-m_lpDX->m_client_height/2 + pr->bottom)  :
		             (float)-(-m_lpDX->m_client_height/2 + pr->top);
		verts[i].z = 0;
		verts[i].Diffuse = 0xD0000000;
	}

	m_lpDX->m_lpDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, verts, sizeof(SIMPLEVERTEX));

	// undo unusual state changes:
	m_lpDX->m_lpDevice->SetRenderState(D3DRS_ZENABLE, TRUE);
	m_lpDX->m_lpDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
}

void CPluginShell::RenderBuiltInTextMsgs()
{
	int _show_press_f1_NOW = (m_show_press_f1_msg && m_time < PRESS_F1_DUR);

	{
		RECT r;

		if (m_show_help)
			RenderHelpCard();   // SURI: design B card, centred

		// render 'Press F1 for Help' message in lower-right corner:
		if (_show_press_f1_NOW)
		{
			int dx = (int)(160.0f * powf(m_time/(float)(PRESS_F1_DUR), (float)(PRESS_F1_EXP)));
			SetRect(&r, m_left_edge, m_lower_right_corner_y - GetFontHeight(DECORATIVE_FONT), m_right_edge + dx, m_lower_right_corner_y);
			m_lower_right_corner_y -= m_d3dx_font[DECORATIVE_FONT]->DrawTextW(NULL, LoadResString(IDS_PRESS_F1_MSG), -1, &r, DT_RIGHT, 0xFFFFFFFF);
		}
	}
}

// SURI: ---- on-screen cards (phase 6 design B) ----------------------------------------------

int CPluginShell::UiPx(int px96)
{
	return ScaleFontPixels(px96, m_ui_dpi);
}

void CPluginShell::DrawRoundedPanel(const RECT* r, int radius, DWORD fill, DWORD border)
{
	// Same approach as DrawDarkTranslucentBox: untextured DrawPrimitiveUP with alpha blending.
	// The outline is the rectangle with 4 quarter circles of 8 segments each. The inside is one
	// triangle fan from the centre (the shape is convex, so this covers the centre rectangle and
	// the 4 corner fans in one call); the border is a 1 px line strip along the same outline.
	if (m_vjd3d9_device || !m_lpDX || !m_lpDX->m_lpDevice || !r)
		return;
	const int w = r->right - r->left, h = r->bottom - r->top;
	if (w <= 0 || h <= 0)
		return;
	float rad = (float)radius;
	if (rad > w * 0.5f) rad = w * 0.5f;
	if (rad > h * 0.5f) rad = h * 0.5f;
	if (rad < 0) rad = 0;

	LPDIRECT3DDEVICE9 dev = m_lpDX->m_lpDevice;
	dev->SetVertexShader(NULL);
	dev->SetPixelShader(NULL);
	dev->SetFVF(SIMPLE_VERTEX_FORMAT);
	dev->SetTexture(0, NULL);
	dev->SetRenderState(D3DRS_ZENABLE, FALSE);
	dev->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
	dev->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
	dev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
	dev->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
	dev->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
	dev->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
	dev->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
	dev->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);

	const float cx = m_lpDX->m_client_width  * 0.5f;
	const float cy = m_lpDX->m_client_height * 0.5f;
	const int SEG = 8, NPTS = 4 * (SEG + 1);
	// Outline in pixel coordinates (D3D9 pixel centres are on integers). inset 0 runs along the
	// pixel edges (for the fill); inset 0.5 runs through the outermost pixels (for the line).
	auto outline = [&](SIMPLEVERTEX* out, float inset, DWORD color) {
		const float L = r->left - 0.5f + inset, R = r->right - 0.5f - inset;
		const float T = r->top - 0.5f + inset,  B = r->bottom - 0.5f - inset;
		float rr = rad - inset; if (rr < 0) rr = 0;
		const float ccx[4] = { L + rr, R - rr, R - rr, L + rr };
		const float ccy[4] = { T + rr, T + rr, B - rr, B - rr };
		int n = 0;
		for (int c = 0; c < 4; c++)
			for (int k = 0; k <= SEG; k++) {
				float a = 3.14159265f * (1.0f + 0.5f * c + 0.5f * k / SEG);   // 180..540 deg: clockwise on screen
				out[n].x = (ccx[c] + rr * cosf(a)) - cx;
				out[n].y = -((ccy[c] + rr * sinf(a)) - cy);
				out[n].z = 0;
				out[n].Diffuse = color;
				n++;
			}
	};

	if (fill >> 24)
	{
		SIMPLEVERTEX v[NPTS + 2];
		v[0].x = (r->left + r->right) * 0.5f - 0.5f - cx;
		v[0].y = -((r->top + r->bottom) * 0.5f - 0.5f - cy);
		v[0].z = 0;
		v[0].Diffuse = fill;
		outline(&v[1], 0.0f, fill);
		v[NPTS + 1] = v[1];
		dev->DrawPrimitiveUP(D3DPT_TRIANGLEFAN, NPTS, v, sizeof(SIMPLEVERTEX));
	}
	if (border >> 24)
	{
		SIMPLEVERTEX v[NPTS + 1];
		outline(v, 0.5f, border);
		v[NPTS] = v[0];
		dev->DrawPrimitiveUP(D3DPT_LINESTRIP, NPTS, v, sizeof(SIMPLEVERTEX));
	}

	// undo unusual state changes (as DrawDarkTranslucentBox does):
	dev->SetRenderState(D3DRS_ZENABLE, TRUE);
	dev->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
}

int CPluginShell::UiText(LPD3DXFONT f, const wchar_t* s, RECT* r, DWORD flags, DWORD color)
{
	if (!f || !s || !r || !s[0])
		return 0;
	if (m_vjd3d9_device)
		return m_text.DrawTextW(f, (wchar_t*)s, r, flags, color, false);
	return f->DrawTextW(NULL, s, -1, r, flags, color);
}

int CPluginShell::UiMeasure(LPD3DXFONT f, const wchar_t* s, RECT* r, DWORD flags)
{
	if (!f || !s || !r)
		return 0;
	if (!s[0]) { r->right = r->left; r->bottom = r->top; return 0; }
	return f->DrawTextW(NULL, s, -1, r, flags | DT_CALCRECT, 0xFFFFFFFF);
}

// The F1 help as a centred card: a row of topic tabs, then the page's keys as keycaps with
// their descriptions in a grid (two columns when there is room), then a footer line.
// SURI: a page taller than the window is drawn with smaller fonts, and cut with a "..." line
// if it still does not fit.
// Called from RenderBuiltInTextMsgs, which runs on the device that owns the fonts, so the text
// is drawn right away (DrawRoundedPanel skips the panels in VJ mode).
void CPluginShell::RenderHelpCard()
{
	LPD3DXFONT fTab = m_ui_font[UIF_META], fTabOn = m_ui_font[UIF_TAB_ON];
	if (!fTab || !fTabOn || !m_ui_font[UIF_KEY] || !m_ui_font[UIF_BODY])
		return;

	const std::vector<std::wstring>& pages = HelpPages();
	if (m_help_page < 0 || m_help_page >= (int)pages.size()) m_help_page = 0;

	const int W = m_vjd3d9_device ? m_nTextWndWidth  : m_lpDX->m_client_width;
	const int H = m_vjd3d9_device ? m_nTextWndHeight : m_lpDX->m_client_height;
	int cardW = UiPx(980);
	if (cardW > W * 9 / 10) cardW = W * 9 / 10;
	const int padX = UiPx(38), padY = UiPx(30), margin = UiPx(12);
	const int innerW = cardW - 2 * padX;
	if (innerW < UiPx(120))
		return;
	const int maxCardH = H - 2 * margin;   // SURI: the card must fit in the window

	struct Op { bool isPanel; LPD3DXFONT f; std::wstring s; RECT r; DWORD flags, color, fill, border; int radius; };
	std::vector<Op> ops;   // positions relative to the card's inner top-left corner
	auto text = [&](LPD3DXFONT f, const std::wstring& s, int x, int y, int w, int h, DWORD flags, DWORD color) {
		Op o = { false, f, s, { x, y, x + w, y + h }, flags, color, 0, 0, 0 };
		ops.push_back(o);
	};
	auto panel = [&](int x, int y, int w, int h, int radius, DWORD fill, DWORD border) {
		Op o = { true, NULL, L"", { x, y, x + w, y + h }, 0, 0, fill, border, radius };
		ops.push_back(o);
	};
	auto measure = [&](LPD3DXFONT f, const std::wstring& s, int maxW, DWORD flags, int* outW) -> int {
		RECT r = { 0, 0, maxW, 0 };
		int h = UiMeasure(f, s.c_str(), &r, flags);
		if (outW) *outW = r.right - r.left;
		return h;
	};

	// SURI: footer text from the string table (LoadResString returns one static buffer: copy each)
	const std::wstring footLeft  = LoadResString(IDS_HELP_FOOTER_TOPICS);
	const std::wstring footRight = LoadResString(IDS_HELP_FOOTER_CLOSE);
	const std::vector<HelpRow> rows = ParseHelpPage(pages[m_help_page], NULL);

	// SURI: lays the card out into ops and returns its inner height. compact: smaller key / body
	// fonts and gaps. limitCardH > 0: rows that would make the card taller than that are left
	// out, and a "..." line shows where the page was cut.
	auto layout = [&](bool compact, int limitCardH) -> int {
		ops.clear();
		LPD3DXFONT fKey  = m_ui_font[compact ? UIF_KEY_COMPACT  : UIF_KEY];
		LPD3DXFONT fBody = m_ui_font[compact ? UIF_BODY_COMPACT : UIF_BODY];
		const int gap = UiPx(compact ? 12 : 20);

		// 1. tabs: one per page, named by the page's title line
		int x = 0, y = 0, rowH = 0;
		const int tabPadX = UiPx(14), tabPadY = UiPx(compact ? 4 : 6), tabGap = UiPx(8);
		for (int i = 0; i < (int)pages.size(); i++)
		{
			std::wstring title;
			ParseHelpPage(pages[i], &title);
			const bool on = (i == m_help_page);
			LPD3DXFONT f = on ? fTabOn : fTab;
			int tw = 0, th = measure(f, title, 4096, DT_SINGLELINE, &tw);
			int w = tw + 2 * tabPadX, h = th + 2 * tabPadY;
			if (x > 0 && x + w > innerW) { x = 0; y += rowH + tabGap; rowH = 0; }
			panel(x, y, w, h, h, on ? CARD_ACCENT : 0, on ? 0 : CARD_BORDER);
			text(f, title, x + tabPadX, y + tabPadY, tw + 2, th, DT_SINGLELINE, on ? CARD_ACCENT_TEXT : CARD_TEXT_2);
			x += w + tabGap;
			if (h > rowH) rowH = h;
		}
		y += rowH + gap;

		// 2. key grid (paragraph lines span the full width)
		const int colGap = UiPx(36), rowGap = UiPx(compact ? 5 : 12), capPadX = UiPx(8), capPadY = UiPx(compact ? 1 : 2), keyGap = UiPx(14);
		const int cols = ((innerW - colGap) / 2 >= UiPx(380)) ? 2 : 1;
		const int colW = (cols == 2) ? (innerW - colGap) / 2 : innerW;
		int capW = UiPx(110);   // all keycaps of a page share one width, so the descriptions line up
		for (const HelpRow& r : rows)
			if (!r.key.empty()) {
				int kw = 0;
				measure(fKey, r.key, 4096, DT_SINGLELINE, &kw);
				if (kw + 2 * capPadX > capW) capW = kw + 2 * capPadX;
			}
		if (capW > colW * 45 / 100) capW = colW * 45 / 100;
		const int descX = capW + keyGap, descW = colW - descX;
		const int keyLineH  = measure(fKey,  L"Ag", 4096, DT_SINGLELINE, NULL);
		const int bodyLineH = measure(fBody, L"Ag", 4096, DT_SINGLELINE, NULL);
		const int footH     = measure(fTab,  L"Ag", 4096, DT_SINGLELINE, NULL);
		const int capH = keyLineH + 2 * capPadY;
		const int capTop  = (bodyLineH > capH) ? (bodyLineH - capH) / 2 : 0;   // centre the keycap on the first text line
		const int descTop = (capH > bodyLineH) ? (capH - bodyLineH) / 2 : 0;
		// lowest y a row may reach: leaves room for the "..." line, the gap and the footer
		const int limitY = (limitCardH > 0) ? limitCardH - 2 * padY - (bodyLineH + rowGap) - gap - footH : 0x7FFFFFFF;

		bool clipped = false;
		int col = 0, cellRowH = 0;
		auto endRow = [&]() { if (col > 0) { y += cellRowH + rowGap; col = 0; cellRowH = 0; } };
		for (const HelpRow& r : rows)
		{
			if (r.key.empty())
			{
				endRow();
				int h = measure(fBody, r.text, innerW, DT_WORDBREAK, NULL);
				if (y + h > limitY) { clipped = true; break; }
				text(fBody, r.text, 0, y, innerW, h, DT_WORDBREAK, CARD_TEXT_2);
				y += h + rowGap;
				continue;
			}
			const int cx0 = col * (colW + colGap);
			int dh = measure(fBody, r.text, descW, DT_WORDBREAK, NULL);
			int hh = descTop + dh;
			if (capTop + capH > hh) hh = capTop + capH;
			if (y + hh > limitY) { clipped = true; break; }
			int kw = 0;
			measure(fKey, r.key, 4096, DT_SINGLELINE, &kw);
			int kx = (capW - kw) / 2;
			if (kx < capPadX) kx = capPadX;
			panel(cx0, y + capTop, capW, capH, UiPx(6), CARD_KEYCAP, 0);
			text(fKey, r.key, cx0 + kx, y + capTop + capPadY, capW - kx, keyLineH, DT_SINGLELINE, CARD_TEXT);
			text(fBody, r.text, cx0 + descX, y + descTop, descW, dh, DT_WORDBREAK, CARD_TEXT);
			if (hh > cellRowH) cellRowH = hh;
			if (++col >= cols) endRow();
		}
		endRow();
		if (clipped)
		{
			text(fBody, L"\x2026", 0, y, innerW, bodyLineH, DT_SINGLELINE, CARD_TEXT_2);
			y += bodyLineH + rowGap;
		}
		y += gap - rowGap;

		// 3. footer, in the smaller 17 px font and the secondary colour
		{
			int lw = 0, rw = 0;
			int h = measure(fTab, footLeft, 4096, DT_SINGLELINE, &lw);
			measure(fTab, footRight, 4096, DT_SINGLELINE, &rw);
			text(fTab, footLeft, 0, y, lw + 2, h, DT_SINGLELINE, CARD_TEXT_2);
			text(fTab, footRight, innerW - rw - 2, y, rw + 2, h, DT_SINGLELINE, CARD_TEXT_2);
			y += h;
		}
		return y;
	};

	// SURI: too tall for the window: smaller fonts first, then cut the page
	int innerH = layout(false, 0);
	if (innerH + 2 * padY > maxCardH && m_ui_font[UIF_KEY_COMPACT] && m_ui_font[UIF_BODY_COMPACT])
	{
		innerH = layout(true, 0);
		if (innerH + 2 * padY > maxCardH)
			innerH = layout(true, maxCardH);
	}

	// 4. place the card and draw everything
	const int cardH = innerH + 2 * padY;
	const int left = (W - cardW) / 2;
	int top = (H - cardH) / 2;
	if (top < margin) top = margin;
	RECT card = { left, top, left + cardW, top + cardH };
	DrawRoundedPanel(&card, UiPx(18), CARD_FILL_PANEL, CARD_BORDER);
	for (Op& o : ops)
	{
		RECT r = o.r;
		OffsetRect(&r, left + padX, top + padY);
		if (o.isPanel)
			DrawRoundedPanel(&r, o.radius, o.fill, o.border);
		else
			o.f->DrawTextW(NULL, o.s.c_str(), -1, &r, o.flags, o.color);
	}
}

void CPluginShell::SuggestHowToFreeSomeMem()
{
	// This function is called when the plugin runs out of video memory;
	//   it lets you show a messagebox to the user so you can (intelligently)
	//   suggest how to free up some video memory, based on what settings
	//   they've chosen.

	wchar_t str[1024];

	if (m_lpDX->m_current_mode.multisamp != D3DMULTISAMPLE_NONE)
	{
		LoadResString(IDS_TO_FREE_UP_MEMORY_DISABLE_MULTISAMPLING, str, ARRAYSIZE(str));
	}
    else
    {
        LoadResString(IDS_TO_FREE_UP_VIDEO_MEMORY, str, ARRAYSIZE(str));
    }

	MessageBoxW(m_lpDX->GetHwnd(), str, LoadResString(IDS_MILKDROP_SUGGESTION), MB_OK|MB_SETFOREGROUND|MB_TOPMOST);
}

LRESULT CALLBACK CPluginShell::WindowProc(HWND hWnd, unsigned uMsg, WPARAM wParam, LPARAM lParam)
{
	//if (uMsg==WM_GETDLGCODE)
	//    return DLGC_WANTALLKEYS|DLGC_WANTCHARS|DLGC_WANTMESSAGE;    // this tells the embedwnd that we want keypresses to flow through to our client wnd.

	if (uMsg == WM_CREATE)
	{
		CREATESTRUCT *create = (CREATESTRUCT *)lParam;
		SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)create->lpCreateParams);
	}

	CPluginShell* p = (CPluginShell*)GetWindowLongPtr(hWnd,GWLP_USERDATA);
	if (p)
		return p->PluginShellWindowProc(hWnd, uMsg, wParam, lParam);
	else
		return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

LRESULT CPluginShell::PluginShellWindowProc(HWND hWnd, unsigned uMsg, WPARAM wParam, LPARAM lParam)
{
	USHORT mask = 1 << (sizeof(SHORT)*8 - 1);
	bool bCtrlHeldDown  = (GetKeyState(VK_CONTROL) & mask) != 0;
	//bool bAltHeldDown: most keys come in under WM_SYSKEYDOWN when ALT is depressed.

	switch (uMsg)
	{
	case WM_ERASEBKGND:
		// Repaint window when song is paused and image needs to be repainted:
		if (m_lpDX && m_lpDX->m_lpDevice && GetFrame() > 0)
		{
			m_lpDX->m_lpDevice->Present(NULL,NULL,NULL,NULL);
			return 0;
		}
		break;

	case WM_WINDOWPOSCHANGING:
		if (m_lpDX && m_lpDX->m_ready && m_lpDX->m_current_mode.m_skin)
			m_lpDX->SaveWindow();
		break;

	case WM_DESTROY:
		// note: don't post quit message here if the window is being destroyed
		// and re-created on a switch between windowed & FAKE fullscreen modes.
		if (!m_lpDX->TempIgnoreDestroyMessages())
		{
			// this is a final exit, and not just destroy-then-recreate-the-window.
			PostQuitMessage(0);
		}
		return FALSE;
		break;

	// benski> a little hack to get the window size correct. it seems to work
	case WM_USER+555:
		if (m_lpDX && m_lpDX->m_ready && !m_resizing)
		{
			OnUserResizeWindow();
			m_lpDX->SaveWindow();
		}
		break;

	case WM_SIZE:
		// clear or set activity flag to reflect focus
		if (m_lpDX && m_lpDX->m_ready && !m_resizing)
		{
			m_hidden = (SIZE_MAXHIDE==wParam || SIZE_MINIMIZED==wParam) ? TRUE : FALSE;

			if (SIZE_MAXIMIZED==wParam || SIZE_RESTORED==wParam) // the window has been maximized or restored
				OnUserResizeWindow();
		}
		break;

	case WM_ENTERSIZEMOVE:
		m_resizing = 1;
		break;

	case WM_EXITSIZEMOVE:
		if (m_lpDX && m_lpDX->m_ready)
			OnUserResizeWindow();
		m_resizing = 0;
		break;

	case WM_GETMINMAXINFO:
	    {
		    // don't let the window get too small
		    MINMAXINFO* p = (MINMAXINFO*)lParam;
		    if (p->ptMinTrackSize.x < 64)
			    p->ptMinTrackSize.x = 64;
		    p->ptMinTrackSize.y = p->ptMinTrackSize.x*3/4;
	    }
	    return 0;

	case WM_SETFOCUS:
		// note: this msg never comes in when embedwnd is used, but that's ok, because that's only
		// in Windowed mode, and m_lost_focus only makes us sleep when fullscreen.
		m_lost_focus = 0;
		break;

	case WM_KILLFOCUS:
		// note: this msg never comes in when embedwnd is used, but that's ok, because that's only
		// in Windowed mode, and m_lost_focus only makes us sleep when fullscreen.
		m_lost_focus = 1;
		break;

	case WM_COMMAND: {
		    // then allow the plugin to override any command:
		    if (MyWindowProc(hWnd, uMsg, wParam, lParam) == 0)
			    return 0;
		}
		break;

	case WM_CHAR:
		// allow the plugin to override any keys:
		if (MyWindowProc(hWnd, uMsg, wParam, lParam) == 0)
			return 0;

		return 0;

	case WM_KEYUP:

		// allow the plugin to override any keys:
		if (MyWindowProc(hWnd, uMsg, wParam, lParam) == 0)
			return 0;

		return 0;
		break;

	case WM_KEYDOWN:
		// allow the plugin to override any keys:
		if (MyWindowProc(hWnd, uMsg, wParam, lParam) == 0)
			return 0;

		switch (wParam)
		{
		    // (F1 is Cmd::HelpOverlay, handled in CPlugin::MyWindowProc)
		    case VK_ESCAPE:
			    if (m_show_help)
				    ToggleHelp();
			    return 0;

		    case VK_LEFT:
		    case VK_RIGHT:
		    {
                /* rewind 5 seconds, ff 5 seconds */
		    }
		    return 0;
		}
		return 0;
	}

	return MyWindowProc(hWnd, uMsg, wParam, lParam);
}

void CPluginShell::ToggleHelp()
{
    m_show_help = 1 - m_show_help;
    m_help_page = 0;   // SURI: F1 always opens page 1
}

void CPluginShell::HelpPageStep(int dir)
{
    const int n = (int)HelpPages().size();
    m_help_page = ((m_help_page + dir) % n + n) % n;
}

void CPluginShell::AlignWaves()
{
	// align waves, using recursive (mipmap-style) least-error matching
	// note: NUM_WAVEFORM_SAMPLES must be between 32 and 576.

	int align_offset[2] = { 0, 0 };

#if (NUM_WAVEFORM_SAMPLES < 576) // [don't let this code bloat our DLL size if it's not going to be used]

	int nSamples = NUM_WAVEFORM_SAMPLES;

#define MAX_OCTAVES 10

	int octaves = (int)floorf(logf((float)(576-nSamples))/logf(2.0f));
	if (octaves < 4)
		return;
	if (octaves > MAX_OCTAVES)
		octaves = MAX_OCTAVES;

	for (int ch=0; ch<2; ch++)
	{
		// only worry about matching the lower 'nSamples' samples
		float temp_new[MAX_OCTAVES][576];
		float temp_old[MAX_OCTAVES][576];
		static float temp_weight[MAX_OCTAVES][576];
		static int   first_nonzero_weight[MAX_OCTAVES];
		static int   last_nonzero_weight[MAX_OCTAVES];
		int spls[MAX_OCTAVES];
		int space[MAX_OCTAVES];

		memcpy(temp_new[0], m_sound.fWaveform[ch], sizeof(float)*576);
		memcpy(temp_old[0], &m_oldwave[ch][m_prev_align_offset[ch]], sizeof(float)*nSamples);
		spls[0] = 576;
		space[0] = 576 - nSamples;

		// potential optimization: could reuse (instead of recompute) mip levels for m_oldwave[2][]?
		for (int octave=1; octave<octaves; octave++)
		{
			spls[octave] = spls[octave-1]/2;
			space[octave] = space[octave-1]/2;
			for (int n=0; n<spls[octave]; n++)
			{
				temp_new[octave][n] = 0.5f*(temp_new[octave-1][n*2] + temp_new[octave-1][n*2+1]);
				temp_old[octave][n] = 0.5f*(temp_old[octave-1][n*2] + temp_old[octave-1][n*2+1]);
			}
		}

		if (!m_align_weights_ready)
		{
			m_align_weights_ready = 1;
			for (octave=0; octave<octaves; octave++)
			{
				int compare_samples = spls[octave] - space[octave];
				for (int n=0; n<compare_samples; n++)
				{
					// start with pyramid-shaped pdf, from 0..1..0
					if (n < compare_samples/2)
						temp_weight[octave][n] = n*2/(float)compare_samples;
					else
						temp_weight[octave][n] = (compare_samples-1 - n)*2/(float)compare_samples;

					// TWEAK how much the center matters, vs. the edges:
					temp_weight[octave][n] = (temp_weight[octave][n] - 0.8f)*5.0f + 0.8f;

					// clip:
					if (temp_weight[octave][n]>1) temp_weight[octave][n] = 1;
					if (temp_weight[octave][n]<0) temp_weight[octave][n] = 0;
				}

				n = 0;
				while (temp_weight[octave][n] == 0 && n < compare_samples)
					n++;
				first_nonzero_weight[octave] = n;

				n = compare_samples-1;
				while (temp_weight[octave][n] == 0 && n >= 0)
					n--;
				last_nonzero_weight[octave] = n;
			}
		}

		int n1 = 0;
		int n2 = space[octaves-1];
		for (octave = octaves-1; octave>=0; octave--)
		{
			// for example:
			//  space[octave] == 4
			//  spls[octave] == 36
			//  (so we test 32 samples, w/4 offsets)
			int compare_samples = spls[octave]-space[octave];

			int lowest_err_offset = -1;
			float lowest_err_amount = 0;
			for (int n=n1; n<n2; n++)
			{
				float err_sum = 0;
				for (int i=first_nonzero_weight[octave]; i<=last_nonzero_weight[octave]; i++)
				{
					float x = (temp_new[octave][i+n] - temp_old[octave][i]) * temp_weight[octave][i];
					if (x>0)
						err_sum += x;
					else
						err_sum -= x;
				}

				if (lowest_err_offset == -1 || err_sum < lowest_err_amount)
				{
					lowest_err_offset = n;
					lowest_err_amount = err_sum;
				}
			}

			// now use 'lowest_err_offset' to guide bounds of search in next octave:
			//  space[octave] == 8
			//  spls[octave] == 72
			//     -say 'lowest_err_offset' was 2
			//     -that corresponds to samples 4 & 5 of the next octave
			//     -also, expand about this by 2 samples?  YES.
			//  (so we'd test 64 samples, w/8->4 offsets)
			if (octave > 0)
			{
				n1 = lowest_err_offset*2  -1;
				n2 = lowest_err_offset*2+2+1;
				if (n1 < 0) n1=0;
				if (n2 > space[octave-1]) n2 = space[octave-1];
			}
			else
				align_offset[ch] = lowest_err_offset;
		}
	}
#endif
	memcpy(m_oldwave[0], m_sound.fWaveform[0], sizeof(float)*576);
	memcpy(m_oldwave[1], m_sound.fWaveform[1], sizeof(float)*576);
	m_prev_align_offset[0] = align_offset[0];
	m_prev_align_offset[1] = align_offset[1];

	// finally, apply the results: modify m_sound.fWaveform[2][0..576]
	// by scooting the aligned samples so that they start at m_sound.fWaveform[2][0].
	for (ch=0; ch<2; ch++)
		if (align_offset[ch]>0)
		{
			for (int i=0; i<nSamples; i++)
				m_sound.fWaveform[ch][i] = m_sound.fWaveform[ch][i+align_offset[ch]];
			// zero the rest out, so it's visually evident that these samples are now bogus:
			memset(&m_sound.fWaveform[ch][nSamples], 0, (576-nSamples)*sizeof(float));
		}
}

LRESULT CALLBACK CPluginShell::VJModeWndProc(HWND hWnd, unsigned uMsg, WPARAM wParam, LPARAM lParam)
{
  CPluginShell* p = (CPluginShell*)GetWindowLongPtr(hWnd,GWLP_USERDATA);
	if (p)
		return p->PluginShellVJModeWndProc(hWnd, uMsg, wParam, lParam);
	else
		return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

LRESULT CPluginShell::PluginShellVJModeWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_KEYDOWN:
	case WM_KEYUP:
	case WM_CHAR:
	case WM_SYSKEYDOWN:
	case WM_SYSKEYUP:
	case WM_SYSCHAR:
		// pass keystrokes on to plugin!
		return PluginShellWindowProc(GetPluginWindow(),message,wParam,lParam);

	case WM_ERASEBKGND:
		// Repaint window when song is paused and image needs to be repainted:
		if (m_vjd3d9_device && GetFrame() > 0)    // WM_USER/104 return codes: 1=playing, 3=paused, other=stopped
		{
			m_vjd3d9_device->Present(NULL,NULL,NULL,NULL);
			return 0;
		}
		break;

	case WM_CLOSE:
		// if they close the VJ window (by some means other than ESC key),
		// this will make the graphics window close, too.
		m_exiting = 1;
		if (GetPluginWindow())
			PostMessage(GetPluginWindow(), WM_CLOSE, 0, 0);
		break;

	case WM_GETMINMAXINFO:
	{
		// don't let the window get too small
		MINMAXINFO* p = (MINMAXINFO*)lParam;
		if (p->ptMinTrackSize.x < 64)
			p->ptMinTrackSize.x = 64;
		p->ptMinTrackSize.y = p->ptMinTrackSize.x*3/4;
	}
	return 0;

	case WM_SIZE:
		// clear or set activity flag to reflect focus
		if (m_vjd3d9_device && !m_resizing_textwnd)
		{
			m_hidden_textwnd = (SIZE_MAXHIDE==wParam || SIZE_MINIMIZED==wParam) ? TRUE : FALSE;

			if (SIZE_MAXIMIZED==wParam || SIZE_RESTORED==wParam) // the window has been maximized or restored
				OnUserResizeTextWindow();
		}
		break;

	case WM_ENTERSIZEMOVE:
		m_resizing_textwnd = 1;
		break;

	case WM_EXITSIZEMOVE:
		if (m_vjd3d9_device)
			OnUserResizeTextWindow();
		m_resizing_textwnd = 0;
		break;
	}

	return DefWindowProc(hwnd, message, wParam, lParam);
}
