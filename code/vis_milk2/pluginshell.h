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

#ifndef __NULLSOFT_DX9_PLUGIN_SHELL_H__
#define __NULLSOFT_DX9_PLUGIN_SHELL_H__ 1

#include "shell_defines.h"
#include "dxcontext.h"
#include "fft.h"
#include "defines.h"
#include "textmgr.h"
#include <vector>

#define TIME_HIST_SLOTS 128     // # of slots used if fps > 60.  half this many if fps==30.

typedef struct
{
    wchar_t szFace[256];
    int nSize;  // size requested @ font creation time
    int bBold;
    int bItalic;
    int bAntiAliased;
} td_fontinfo;

typedef struct
{
    float imm[2][3];                        // bass, mids, treble, no damping, for each channel (long-term average is 1)
    float avg[2][3];                        // bass, mids, treble, some damping, for each channel (long-term average is 1)
    float med_avg[2][3];                    // bass, mids, treble, more damping, for each channel (long-term average is 1)
    float long_avg[2][3];                   // bass, mids, treble, heavy damping, for each channel (long-term average is 1)
    float infinite_avg[2][3];               // bass, mids, treble: long-term average output levels. (1)
    float fWaveform[2][576];                // Not all 576 are valid! - only NUM_WAVEFORM_SAMPLES samples are valid for each channel (note: NUM_WAVEFORM_SAMPLES is declared in shell_defines.h)
    float fSpectrum[2][NUM_FREQUENCIES];    // NUM_FREQUENCIES samples for each channel (note: NUM_FREQUENCIES is declared in shell_defines.h)
} td_soundinfo;                             // ...range is 0 Hz to 22050 Hz, evenly spaced.

class CPluginShell
{
public:
    // GET METHODS
    // ------------------------------------------------------------
    int       GetFrame();          // returns current frame # (starts at zero)
    float     GetTime();           // returns current animation time (in seconds) (starts at zero) (updated once per frame)
    float     GetFps();            // returns current estimate of framerate (frames per second)
    HINSTANCE GetInstance();       // returns handle to the plugin DLL module; used for things like loading resources (dialogs, bitmaps, icons...) that are built into the plugin.
    wchar_t*  GetPluginsDirPath(); // the folder that holds MilkDrop3.exe (with a trailing backslash)
    wchar_t*  GetConfigIniFile();  // milk2.ini next to the exe (INIFILE in defines.h)
	char*     GetConfigIniFileA();
protected:

    // GET METHODS THAT ONLY WORK ONCE DIRECTX IS READY
    // ------------------------------------------------------------
    //  The following 'Get' methods are only available after DirectX has been initialized.
    //  If you call these from OverrideDefaults, MyPreInitialize, or MyReadConfig,
    //    they will return NULL (zero).
    // ------------------------------------------------------------
    HWND         GetPluginWindow();      // returns handle to the plugin window.  NOT persistent; can change!
    int          GetWidth();             // returns width of plugin window interior, in pixels.  Note: in windowed mode, this is a fudged, larger, aligned value, and on final display, it gets cropped.
    int          GetHeight();            // returns height of plugin window interior, in pixels. Note: in windowed mode, this is a fudged, larger, aligned value, and on final display, it gets cropped.
    int          GetBitDepth();          // returns 8, 16, 24 (rare), or 32
    LPDIRECT3DDEVICE9  GetDevice();      // returns a pointer to the DirectX 8 Device.  NOT persistent; can change!
    D3DCAPS9*    GetCaps();              // returns a pointer to the D3DCAPS9 structer for the device.  NOT persistent; can change.
    D3DFORMAT    GetBackBufFormat();     // returns the pixelformat of the back buffer (probably D3DFMT_R8G8B8, D3DFMT_A8R8G8B8, D3DFMT_X8R8G8B8, D3DFMT_R5G6B5, D3DFMT_X1R5G5B5, D3DFMT_A1R5G5B5, D3DFMT_A4R4G4B4, D3DFMT_R3G3B2, D3DFMT_A8R3G3B2, D3DFMT_X4R4G4B4, or D3DFMT_UNKNOWN)
    D3DFORMAT    GetBackBufZFormat();    // returns the pixelformat of the back buffer's Z buffer (probably D3DFMT_D16_LOCKABLE, D3DFMT_D32, D3DFMT_D15S1, D3DFMT_D24S8, D3DFMT_D16, D3DFMT_D24X8, D3DFMT_D24X4S4, or D3DFMT_UNKNOWN)
    char*        GetDriverFilename();    // returns a text string with the filename of the current display adapter driver, such as "nv4_disp.dll"
    char*        GetDriverDescription(); // returns a text string describing the current display adapter, such as "NVIDIA GeForce4 Ti 4200"

    // FONTS & TEXT
    // ------------------------------------------------------------
public:
    LPD3DXFONT   GetFont(eFontIndex idx);       // returns a D3DX font handle for drawing text; see shell_defines.h for the definition of the 'eFontIndex' enum.
    int          GetFontHeight(eFontIndex idx); // returns the height of the font, in pixels; see shell_defines.h for the definition of the 'eFontIndex' enum.
    CTextManager m_text;
protected:

    // MISC
    // ------------------------------------------------------------
    td_soundinfo m_sound;                   // a structure always containing the most recent sound analysis information; defined in pluginshell.h.
    void         SuggestHowToFreeSomeMem(); // gives the user a 'smart' messagebox that suggests how they can free up some video memory.

    // SETTINGS (milk2.ini)
    // ------------------------------------------------------------
    // *** only read/write these values during CPlugin::OverrideDefaults! ***
    int          m_start_fullscreen;        // 0 or 1
    int          m_fake_fullscreen_mode;    // 0 or 1
    int          m_max_fps_fs;              // 1-120, or 0 for 'unlimited'
    int          m_max_fps_w;               // 1-120, or 0 for 'unlimited'
    int          m_show_press_f1_msg;       // 0 or 1
    int          m_allow_page_tearing_w;    // 0 or 1
    int          m_allow_page_tearing_fs;   // 0 or 1
    int          m_dualhead_horz;           // 0 = both, 1 = left, 2 = right
    int          m_dualhead_vert;           // 0 = both, 1 = top, 2 = bottom
    int          m_save_cpu;                // 0 or 1
    int          m_skin;                    // 0 or 1
    int          m_fix_slow_text;           // 0 or 1
    td_fontinfo  m_fontinfo[NUM_BASIC_FONTS + NUM_EXTRA_FONTS];
    int          m_fontbase[NUM_BASIC_FONTS + NUM_EXTRA_FONTS];   // SURI: configured size (0 = not captured yet); m_fontinfo[].nSize becomes the measured, DPI-scaled height
    D3DDISPLAYMODE m_disp_mode_fs;          // a D3DDISPLAYMODE struct that specifies the width, height, refresh rate, and color format to use when the plugin goes fullscreen.

    // PURE VIRTUAL FUNCTIONS (...must be implemented by derived classes)
    // ------------------------------------------------------------
    virtual void OverrideDefaults()      = 0;
    virtual void MyPreInitialize()       = 0;
    virtual void MyReadConfig()          = 0;
    virtual void MyWriteConfig()         = 0;
    virtual int  AllocateMyNonDx9Stuff() = 0;
    virtual void  CleanUpMyNonDx9Stuff() = 0;
    virtual int  AllocateMyDX9Stuff()    = 0;
    virtual void  CleanUpMyDX9Stuff(int final_cleanup) = 0;
    virtual void MyRenderFn(int redraw)  = 0;
    virtual void MyRenderUI(int *upper_left_corner_y, int *upper_right_corner_y, int *lower_left_corner_y, int *lower_right_corner_y, int xL, int xR) = 0;
    virtual LRESULT MyWindowProc(HWND hWnd, unsigned uMsg, WPARAM wParam, LPARAM lParam) = 0;
    virtual void MyRenderOverlay() {}   // SURI: called after the queued m_text text is drawn (toasts go on top of it)
    virtual void OnFontsReallocated() {} // SURI: after ReallocateFontsForDpi (the plugin rebuilds its own fonts)

    int m_show_help;
    int m_help_page;            // SURI: 0-based page of the F1 help

//=====================================================================================================================
private:

    // GENERAL PRIVATE STUFF
    int          m_frame;           // current frame #, starting at zero
    float        m_time;            // current animation time in seconds; starts at zero.
    float        m_fps;             // current estimate of frames per second
    HINSTANCE    m_hInstance;       // handle to application instance
    DXContext*   m_lpDX;            // pointer to DXContext object
    wchar_t      m_szPluginsDirPath[MAX_PATH];  // folder of MilkDrop3.exe
    wchar_t      m_szConfigIniFile[MAX_PATH];   // full path of milk2.ini
	char         m_szConfigIniFileA[MAX_PATH];   // same, as ANSI

    // FONTS
	IDirect3DTexture9* m_lpDDSText;
    LPD3DXFONT   m_d3dx_font[NUM_BASIC_FONTS + NUM_EXTRA_FONTS];
    HFONT        m_font[NUM_BASIC_FONTS + NUM_EXTRA_FONTS];

    // PRIVATE SETTINGS (milk2.ini)
    D3DMULTISAMPLE_TYPE m_multisample_fullscreen;
    D3DMULTISAMPLE_TYPE m_multisample_windowed;
    GUID m_adapter_guid_fullscreen;
    GUID m_adapter_guid_windowed;
    char m_adapter_devicename_fullscreen[256];  // also needed when one adapter (e.g. a laptop) drives two displays:
    char m_adapter_devicename_windowed[256];    //  DeviceName will be \\.\Display1 and \\.\Display2 or something.

    // PRIVATE RUNTIME SETTINGS
    int m_lost_focus;     // ~mostly for fullscreen mode
    int m_hidden;         // ~mostly for windowed mode
    int m_resizing;       // ~mostly for windowed mode
    int m_exiting;
    int m_upper_left_corner_y;
    int m_lower_left_corner_y;
    int m_upper_right_corner_y;
    int m_lower_right_corner_y;
    int m_left_edge;
    int m_right_edge;
    int m_force_accept_WM_WINDOWPOSCHANGING;

    // PRIVATE - VJ MODE STUFF
    bool                m_bClearVJWindow;

    // PRIVATE - MORE TIMEKEEPING
   protected:
    double m_last_raw_time;
    LARGE_INTEGER m_high_perf_timer_freq;  // 0 if high-precision timer not available
   private:
    float  m_time_hist[TIME_HIST_SLOTS];		// cumulative
    int    m_time_hist_pos;
    LARGE_INTEGER m_prev_end_of_frame;

    // PRIVATE AUDIO PROCESSING DATA
    FFT   m_fftobj;
    float m_oldwave[2][576];        // for wave alignment
    int   m_prev_align_offset[2];   // for wave alignment
    int   m_align_weights_ready;

public:
    CPluginShell();
    ~CPluginShell();

    // called by Milkdrop2PcmVisualizer.cpp:
    int  PluginPreInitialize(HWND hParentWnd, HINSTANCE hInstance);
    int  PluginInitialize(LPDIRECT3DDEVICE9 device, D3DPRESENT_PARAMETERS* d3dpp, HWND hwnd, int iWidth, int iHeight);
    int  PluginRender(unsigned char *pWaveL, unsigned char *pWaveR);
    void PluginQuit();

    void ToggleHelp();
    void HelpPageStep(int dir);   // SURI: +1 / -1, only while the help is shown; wraps around
    void ReallocateFontsForDpi(); // SURI: WM_DPICHANGED: rebuild the fonts at the window's current DPI

	void READ_FONT(int n);
	void WRITE_FONT(int n);

    // window messaging:
    static LRESULT CALLBACK WindowProc(HWND hWnd, unsigned uMsg, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK VJModeWndProc(HWND hWnd, unsigned uMsg, WPARAM wParam, LPARAM lParam);

private:
    void DrawAndDisplay(int redraw);
    void ReadConfig();
    void WriteConfig();
    void DoTime();
    void AnalyzeNewSound(unsigned char *pWaveL, unsigned char *pWaveR);
    void AlignWaves();
    int  InitDirectX(LPDIRECT3DDEVICE9 device, D3DPRESENT_PARAMETERS* d3dpp, HWND hwnd);
    void CleanUpDirectX();
    int  InitGDIStuff();
    void CleanUpGDIStuff();
    int  AllocateDX9Stuff();
    void CleanUpDX9Stuff(int final_cleanup);
    int  InitNondx9Stuff();
    void CleanUpNondx9Stuff();
    int  InitVJStuff(RECT* pClientRect=NULL);
    void CleanUpVJStuff();
    int  AllocateFonts(IDirect3DDevice9 *pDevice);
    void CleanUpFonts();
    void AllocateTextSurface();
    void OnUserResizeWindow();
    void OnUserResizeTextWindow();
    void PrepareFor2DDrawing_B(IDirect3DDevice9 *pDevice, int w, int h);
    void RenderBuiltInTextMsgs();
public:
    int  GetCanvasMarginX();     // returns the # of pixels that exist on the canvas, on each side, that the user will never see.  Mainly here for windowed mode, where sometimes, up to 15 pixels get cropped at edges of the screen.
    int  GetCanvasMarginY();     // returns the # of pixels that exist on the canvas, on each side, that the user will never see.  Mainly here for windowed mode, where sometimes, up to 15 pixels get cropped at edges of the screen.
    void DrawDarkTranslucentBox(RECT* pr);

    // SURI: on-screen "cards" (phase 6 design B). All sizes are given in pixels at 96 DPI and
    // go through UiPx. Panels are drawn right away (so they sit under the queued m_text text);
    // they are skipped in VJ mode, like DrawDarkTranslucentBox.
    // UIF_CODE: Consolas at SIMPLE_FONT's size (code editor); *_COMPACT: the help card when a page is too tall
    enum UiFont { UIF_TITLE, UIF_META, UIF_TOAST, UIF_BODY, UIF_SMALL, UIF_KEY, UIF_TAB_ON, UIF_STARS, UIF_CODE,
                  UIF_KEY_COMPACT, UIF_BODY_COMPACT, UIF_COUNT };
    LPD3DXFONT GetUiFont(int f) { return (f >= 0 && f < UIF_COUNT) ? m_ui_font[f] : NULL; }
    int  UiPx(int px96);
    // radius > half the height gives a pill; fill or border with alpha 0 is skipped
    void DrawRoundedPanel(const RECT* r, int radius, DWORD fill, DWORD border);
    // draws now, or queues it in m_text in VJ mode (the fonts then live on the VJ device);
    // returns the text height
    int  UiText(LPD3DXFONT f, const wchar_t* s, RECT* r, DWORD flags, DWORD color);
    // DT_CALCRECT: r keeps its left/top, gets the text's right/bottom; returns the height
    int  UiMeasure(LPD3DXFONT f, const wchar_t* s, RECT* r, DWORD flags);
private:
    LPD3DXFONT   m_ui_font[UIF_COUNT];
    UINT         m_ui_dpi;
    void         RenderHelpCard();

protected:
    void StuffParams(DXCONTEXT_PARAMS *pParams);
    void EnforceMaxFPS();

    // SEPARATE TEXT WINDOW (FOR VJ MODE)
	  int 		m_vj_mode;
      int       m_hidden_textwnd;
      int       m_resizing_textwnd;
      protected:
	   HWND		m_hTextWnd;
      private:
	  int		m_nTextWndWidth;
	  int		m_nTextWndHeight;
	  bool		m_bTextWindowClassRegistered;
      LPDIRECT3D9 m_vjd3d9;
      LPDIRECT3DDEVICE9 m_vjd3d9_device;

    // WINDOWPROC FUNCTIONS
public:
    LRESULT PluginShellWindowProc(HWND hWnd, unsigned uMsg, WPARAM wParam, LPARAM lParam);   // in windowproc.cpp
    LRESULT PluginShellVJModeWndProc(HWND hWnd, unsigned uMsg, WPARAM wParam, LPARAM lParam);

protected:
	LPDIRECT3DDEVICE9 GetTextDevice() { return (m_vjd3d9_device) ? m_vjd3d9_device : m_lpDX->m_lpDevice; }

    // CHANGES:
    friend class CShaderParams;
};

#endif