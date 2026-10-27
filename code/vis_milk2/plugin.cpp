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
/*
Order of Function Calls
-----------------------
    The plugin shell calls the virtual functions in plugin.h.
    In what order are they called?
    A breakdown follows.  A function name in { } means that it is only
    called under certain conditions.

    Order of function calls...

    When MilkDrop starts
    --------------------
        INITIALIZATION
            OverrideDefaults
            MyPreInitialize
            MyReadConfig
            << DirectX gets initialized at this point >>
            AllocateMyNonDx9Stuff
            AllocateMyDX9Stuff
        RUNNING
            +--> { CleanUpMyDX9Stuff + AllocateMyDX9Stuff }  // called together when user resizes window or toggles fullscreen<->windowed.
            |    MyRenderFn
            |    MyRenderUI
            |    { MyWindowProc }                            // called, between frames, on mouse/keyboard/system events.  100% threadsafe.
            +----<< repeat >>
        CLEANUP
            CleanUpMyDX9Stuff
            CleanUpMyNonDx9Stuff
            << DirectX gets uninitialized at this point >>
*/

#include "plugin.h"
#include "utility.h"
#include "support.h"
#include "resource.h"
#include "defines.h"
#include "shell_defines.h"
#include "resstrings.h"
#include "features/context_menu.h"
#include "features/messages.h"
#include "features/image_load_win32.h"
#include "features/sprite_layout.h"
#include "features/title_anim.h"
#include <assert.h>
#include <algorithm>
#include <commdlg.h>
#include <locale.h>
#include <process.h>  // for beginthread, etc.
#include <shellapi.h>
#include <strsafe.h>
#include "AutoCharFn.h"
#include "../audio/log.h"

#define FRAND ((rand() % 7381)/7380.0f)

static bool m_bAlwaysOnTop = false;

void NSEEL_HOSTSTUB_EnterMutex(){}
void NSEEL_HOSTSTUB_LeaveMutex(){}

// note: these must match layouts in support.h!!
D3DVERTEXELEMENT9 g_MyVertDecl[] =
{
    { 0, 0,  D3DDECLTYPE_FLOAT3,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
    { 0, 12, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR,    0 },
    { 0, 16, D3DDECLTYPE_FLOAT4,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
    { 0, 32, D3DDECLTYPE_FLOAT2,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 1 },
    D3DDECL_END()
};
D3DVERTEXELEMENT9 g_WfVertDecl[] =
{
    { 0, 0,  D3DDECLTYPE_FLOAT3,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
    { 0, 12, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR,    0 },
    D3DDECL_END()
};
D3DVERTEXELEMENT9 g_SpriteVertDecl[] =
{
    // matches D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1
    { 0, 0,  D3DDECLTYPE_FLOAT3,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
    { 0, 12, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR,    0 },
    { 0, 16, D3DDECLTYPE_FLOAT2,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
    D3DDECL_END()
};

extern CPlugin g_plugin;		// declared in main.cpp (note: was 'pg')

// from support.cpp:
extern bool g_bDebugOutput;
extern bool g_bDumpFileCleared;

// for __UpdatePresetList:
volatile HANDLE g_hThread;  // only r/w from our MAIN thread
volatile bool g_bThreadAlive; // set true by MAIN thread, and set false upon exit from 2nd thread.
volatile int  g_bThreadShouldQuit;  // set by MAIN thread to flag 2nd thread that it wants it to exit.
static CRITICAL_SECTION g_cs;

#define IsAlphabetChar(x) ((x >= 'a' && x <= 'z') || (x >= 'A' && x <= 'Z'))
#define IsAlphanumericChar(x) ((x >= 'a' && x <= 'z') || (x >= 'A' && x <= 'Z') || (x >= '0' && x <= '9') || x == '.')
#define IsNumericChar(x) (x >= '0' && x <= '9')

const unsigned char LC2UC[256] = {
	0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,
	17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,255,
	33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,
	49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,64,
	97,98,99,100,101,102,103,104,105,106,107,108,109,110,111,112,
	113,114,115,116,117,118,119,120,121,122,91,92,93,94,95,96,
	97,98,99,100,101,102,103,104,105,106,107,108,109,110,111,112,
	113,114,115,116,117,118,119,120,121,122,123,124,125,126,127,128,
	129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,144,
	145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,160,
	161,162,163,164,165,166,167,168,169,170,171,172,173,174,175,176,
	177,178,179,180,181,182,183,184,185,186,187,188,189,190,191,192,
	193,194,195,196,197,198,199,200,201,202,203,204,205,206,207,208,
	209,210,211,212,213,214,215,216,217,218,219,220,221,222,223,224,
	225,226,227,228,229,230,231,232,233,234,235,236,237,238,239,240,
	241,242,243,244,245,246,247,248,249,250,251,252,253,254,255,
};

/*
 * Copies the given string TO the clipboard.
 */
void copyStringToClipboardA(const char * source)
{
    int ok = OpenClipboard(NULL);
    if (!ok)
        return;

    HGLOBAL clipbuffer;
    EmptyClipboard();
    clipbuffer = GlobalAlloc(GMEM_DDESHARE, (lstrlenA(source)+1)*sizeof(char));
    char* buffer = (char*)GlobalLock(clipbuffer);
    lstrcpyA(buffer, source);
    GlobalUnlock(clipbuffer);
    SetClipboardData(CF_TEXT, clipbuffer);
    CloseClipboard();
}

void copyStringToClipboardW(const wchar_t * source)
{
    int ok = OpenClipboard(NULL);
    if (!ok)
        return;

    HGLOBAL clipbuffer;
    EmptyClipboard();
    clipbuffer = GlobalAlloc(GMEM_DDESHARE, (lstrlenW(source)+1)*sizeof(wchar_t));
    wchar_t* buffer = (wchar_t*)GlobalLock(clipbuffer);
    lstrcpyW(buffer, source);
    GlobalUnlock(clipbuffer);
    SetClipboardData(CF_UNICODETEXT, clipbuffer);
    CloseClipboard();
}

/*
 * Suppose there is a string on the clipboard.
 * This function copies it FROM there.
 */
char * getStringFromClipboardA()
{
    int ok = OpenClipboard(NULL);
    if (!ok)
        return NULL;

    HANDLE hData = GetClipboardData(CF_TEXT);
    char* buffer = (char*)GlobalLock(hData);
    GlobalUnlock(hData);
    CloseClipboard();
    return buffer;
}

wchar_t * getStringFromClipboardW()
{
    int ok = OpenClipboard(NULL);
    if (!ok)
        return NULL;

    HANDLE hData = GetClipboardData(CF_UNICODETEXT);
    wchar_t* buffer = (wchar_t*)GlobalLock(hData);
    GlobalUnlock(hData);
    CloseClipboard();
    return buffer;
}

void ConvertCRsToLFCA(const char* src, char* dst)
{
    while (*src)
    {
        char ch = *src;
        if (*src==13 && *(src+1)==10)
        {
            *dst++ = LINEFEED_CONTROL_CHAR;
            src += 2;
        }
        else
        {
            *dst++ = *src++;
        }
    }
    *dst = 0;
}

void ConvertCRsToLFCW(const wchar_t* src, wchar_t* dst)
{
    while (*src)
    {
        wchar_t ch = *src;
        if (*src==13 && *(src+1)==10)
        {
            *dst++ = LINEFEED_CONTROL_CHAR;
            src += 2;
        }
        else
        {
            *dst++ = *src++;
        }
    }
    *dst = 0;
}

void ConvertLFCToCRsA(const char* src, char* dst)
{
    while (*src)
    {
        char ch = *src;
        if (*src==LINEFEED_CONTROL_CHAR)
        {
            *dst++ = 13;
            *dst++ = 10;
            src++;
        }
        else
        {
            *dst++ = *src++;
        }
    }
    *dst = 0;
}

void ConvertLFCToCRsW(const wchar_t* src, wchar_t* dst)
{
    while (*src)
    {
        wchar_t ch = *src;
        if (*src==LINEFEED_CONTROL_CHAR)
        {
            *dst++ = 13;
            *dst++ = 10;
            src++;
        }
        else
        {
            *dst++ = *src++;
        }
    }
    *dst = 0;
}

int mystrcmpiW(const wchar_t *s1, const wchar_t *s2)
{
	// returns  1 if s1 comes before s2
	// returns  0 if equal
	// returns -1 if s1 comes after s2
	// treats all characters/symbols by their ASCII values,
	//    except that it DOES ignore case.

	int i=0;

	while (LC2UC[s1[i]] == LC2UC[s2[i]] && s1[i] != 0)
		i++;

	//FIX THIS!

	if (s1[i]==0 && s2[i]==0)
		return 0;
	else if (s1[i]==0)
		return -1;
	else if (s2[i]==0)
		return 1;
	else
		return (LC2UC[s1[i]] < LC2UC[s2[i]]) ? -1 : 1;
}

bool ReadFileToString(const wchar_t* szBaseFilename, char* szDestText, int nMaxBytes, bool bConvertLFsToSpecialChar)
{
    wchar_t szFile[MAX_PATH];
    swprintf(szFile, L"%s%s", g_plugin.m_szMilkdrop2Path, szBaseFilename);

    // read in all chars.  Replace char combos:  { 13;  13+10;  10 } with LINEFEED_CONTROL_CHAR, if bConvertLFsToSpecialChar is true.
    FILE* f = _wfopen(szFile, L"rb");
    if (!f)
    {
        wchar_t buf[1024], title[64];
		swprintf(buf, LoadResString(IDS_UNABLE_TO_READ_DATA_FILE_X), szFile);
		g_plugin.dumpmsg(buf);
		MessageBoxW(NULL, buf, LoadResString(IDS_MILKDROP_ERROR,title,64), MB_OK|MB_SETFOREGROUND|MB_TOPMOST );
		return false;
    }
    int len = 0;
    int x;
    char prev_ch = 0;
    while ( (x = fgetc(f)) >= 0 && len < nMaxBytes-4 )
    {
        char orig_ch = (char)x;
        char ch = orig_ch;
        bool bSkipChar = false;
        if (bConvertLFsToSpecialChar)
        {
            if (ch==10)
            {
                if (prev_ch==13)
                    bSkipChar = true;
                else
                    ch = LINEFEED_CONTROL_CHAR;
            }
            else if (ch==13)
                ch = LINEFEED_CONTROL_CHAR;
        }

        if (!bSkipChar)
            szDestText[len++] = ch;
        prev_ch = orig_ch;
    }
    szDestText[len] = 0;
    szDestText[len++] = ' ';   // make sure there is some whitespace after
    fclose(f);
    return true;
}

// these callback functions are called by menu.cpp whenever the user finishes editing an eval_ expression.
void OnUserEditedPerFrame(LPARAM param1, LPARAM param2)
{
	g_plugin.m_pState->RecompileExpressions(RECOMPILE_PRESET_CODE, 0);
}

void OnUserEditedPerPixel(LPARAM param1, LPARAM param2)
{
	g_plugin.m_pState->RecompileExpressions(RECOMPILE_PRESET_CODE, 0);
}

void OnUserEditedPresetInit(LPARAM param1, LPARAM param2)
{
	g_plugin.m_pState->RecompileExpressions(RECOMPILE_PRESET_CODE, 1);
}

void OnUserEditedWavecode(LPARAM param1, LPARAM param2)
{
	g_plugin.m_pState->RecompileExpressions(RECOMPILE_WAVE_CODE, 0);
}

void OnUserEditedWavecodeInit(LPARAM param1, LPARAM param2)
{
	g_plugin.m_pState->RecompileExpressions(RECOMPILE_WAVE_CODE, 1);
}

void OnUserEditedShapecode(LPARAM param1, LPARAM param2)
{
	g_plugin.m_pState->RecompileExpressions(RECOMPILE_SHAPE_CODE, 0);
}

void OnUserEditedShapecodeInit(LPARAM param1, LPARAM param2)
{
	g_plugin.m_pState->RecompileExpressions(RECOMPILE_SHAPE_CODE, 1);
}

void OnUserEditedWarpShaders(LPARAM param1, LPARAM param2)
{
    g_plugin.m_bNeedRescanTexturesDir = true;
    g_plugin.ClearErrors(ERR_PRESET);
    if (g_plugin.m_nMaxPSVersion == 0)
        return;
    if (!g_plugin.RecompilePShader(g_plugin.m_pState->m_szWarpShadersText, &g_plugin.m_shaders.warp, SHADER_WARP, false, g_plugin.m_pState->m_nWarpPSVersion))
    {
        // switch to fallback
        g_plugin.m_fallbackShaders_ps.warp.ptr->AddRef();
        g_plugin.m_fallbackShaders_ps.warp.CT->AddRef();
        g_plugin.m_shaders.warp = g_plugin.m_fallbackShaders_ps.warp;
    }
}

void OnUserEditedCompShaders(LPARAM param1, LPARAM param2)
{
    // SURI: a manual edit does not re-inject effects. The user sees the SURI-FX blocks in the text and may delete them.
    g_plugin.m_bNeedRescanTexturesDir = true;
    g_plugin.ClearErrors(ERR_PRESET);
    if (g_plugin.m_nMaxPSVersion == 0)
        return;
    if (!g_plugin.RecompilePShader(g_plugin.m_pState->m_szCompShadersText, &g_plugin.m_shaders.comp, SHADER_COMP, false, g_plugin.m_pState->m_nCompPSVersion))
    {
        // switch to fallback
        g_plugin.m_fallbackShaders_ps.comp.ptr->AddRef();
        g_plugin.m_fallbackShaders_ps.comp.CT->AddRef();
        g_plugin.m_shaders.comp = g_plugin.m_fallbackShaders_ps.comp;
    }
}

//----------------------------------------------------------------------

void CPlugin::OverrideDefaults()
{
    // Here, you have the option of overriding the "default defaults"
    //   for the shell settings, replacing them
    //   with custom defaults for your plugin.
    // To override any of the defaults, just uncomment the line
    //   and change the value.
    // DO NOT modify these values from any function but this one!

    // This example plugin only changes the default width/height
    //   for fullscreen mode; the "default defaults" are just
    //   640 x 480.
    // If your plugin is very dependent on smooth animation and you
    //   wanted it plugin to have the 'save cpu' option OFF by default,
    //   for example, you could set 'm_save_cpu' to 0 here.

       m_max_fps_fs            = 60;       // 1-120, or 0 for 'unlimited' 
       m_max_fps_w             = 60;       // 1-120, or 0 for 'unlimited'
       m_show_press_f1_msg     = 0;       // 0 or 1
       m_allow_page_tearing_w  = 0;       // 0 or 1
       m_save_cpu              = 0;       // 0 or 1


    // Don't override default FS mode here; shell is now smart and sets it to match
    // the current desktop display mode, by default.

    //m_disp_mode_fs.Width    = 1024;             // normally 640
    //m_disp_mode_fs.Height   = 768;              // normally 480
    // use either D3DFMT_X8R8G8B8 or D3DFMT_R5G6B5.
    // The former will match to any 32-bit color format available,
    // and the latter will match to any 16-bit color available,
    // if that exact format can't be found.
	// m_disp_mode_fs.RefreshRate = 60;
}

//----------------------------------------------------------------------

void CPlugin::MyPreInitialize()
{
    // Initialize EVERY data member you've added to CPlugin here;
    //   these will be the default values.
    // If you want to initialize any of your variables with random values
    //   (using rand()), be sure to seed the random number generator first!
    // (If you want to change the default values for settings that are part of
    //   the plugin shell (framework), do so from OverrideDefaults() above.)

    // SETTINGS (milk2.ini)
	m_bFirstRun		            = true;
    m_bInitialPresetSelected    = false;
	m_fBlendTimeUser			= 1.7f;
	m_fBlendTimeAuto			= 2.7f;
	m_fTimeBetweenPresets		= 16.0f;
	m_fTimeBetweenPresetsRand	= 10.0f;
	m_bSequentialPresetOrder    = false;
	m_bHardCutsDisabled			= true;
	m_fHardCutLoudnessThresh	= 2.5f;
	m_fHardCutHalflife			= 60.0f;
        m_nCanvasStretch    = 100;
	m_nTexSizeX			= -1;	// -1 means "auto"
	m_nTexSizeY			= -1;	// -1 means "auto"
	m_nTexBitsPerCh     =  8;
	m_nGridX			= 64;//32;
	m_nGridY			= 64;//24;

	m_bShowPressF1ForHelp = true;
	m_bShowMenuToolTips = true;	// NOTE: THIS IS CURRENTLY HARDWIRED TO TRUE - NO OPTION TO CHANGE
	m_n16BitGamma	= 2;
	m_bAutoGamma    = true;
	m_bEnableRating			= true;
	m_bSongTitleAnims		= true;
	m_bNowPlaying			= true;
	m_fSongTitleAnimDuration = 1.7f;
	m_fSongTitleHold         = 7.0f;
	m_fSongTitleFade         = 3.0f;
	m_fTimeBetweenRandomSongTitles = -1.0f;
	m_fTimeBetweenRandomCustomMsgs = -1.0f;
	m_nSongTitlesSpawned = 0;
	m_nCustMsgsSpawned = 0;
    m_nFramesSinceResize = 0;

    m_bWarningsDisabled2    = false;
    m_bPresetLockOnAtStartup = false;
	m_bPreventScollLockHandling = false;
    m_nMaxPSVersion_ConfigPanel = 3;  // -1 = auto, 0 = disable shaders, 2 = ps_2_0, 3 = ps_3_0
    m_nMaxPSVersion_DX9 = 3;          // 0 = no shader support, 2 = ps_2_0, 3 = ps_3_0
    m_nMaxPSVersion = 3;              // this one will be the ~min of the other two.  0/2/3.
    m_nMaxImages = 3000;
    m_nMaxBytes  = 2000000000;

    #ifdef _DEBUG
        m_dwShaderFlags = D3DXSHADER_DEBUG|(1<<16);
    #else
        m_dwShaderFlags = (1<<16);//D3DXSHADER_SKIPOPTIMIZATION|D3DXSHADER_NO_PRESHADER;
    #endif
    m_pShaderCompileErrors = NULL;
    ZeroMemory(&m_shaders,    sizeof(PShaderSet));
    ZeroMemory(&m_OldShaders, sizeof(PShaderSet));
    ZeroMemory(&m_NewShaders, sizeof(PShaderSet));
    ZeroMemory(&m_fallbackShaders_vs, sizeof(VShaderSet));
    ZeroMemory(&m_fallbackShaders_ps, sizeof(PShaderSet));
    ZeroMemory(m_BlurShaders, sizeof(m_BlurShaders));
    m_bWarpShaderLock = false;
    m_bCompShaderLock = false;
    m_bNeedRescanTexturesDir = true;

    // vertex declarations:
    m_pSpriteVertDecl = NULL;
    m_pWfVertDecl = NULL;
    m_pMyVertDecl = NULL;

    m_gdi_title_font_doublesize  = NULL;
    m_d3dx_title_font_doublesize = NULL;

    // RUNTIME SETTINGS THAT WE'VE ADDED
    m_prev_time = GetTime() - 0.0333f; // note: this will be updated each frame, at bottom of MyRenderFn.
	m_bTexSizeWasAutoPow2	= false;
	m_bTexSizeWasAutoExact	= false;
	m_bPresetLockedByCode = false;
	m_fStartTime	= 0.0f;
	m_fPresetStartTime = 0.0f;
	m_fNextPresetTime  = -1.0f;	// negative value means no time set (...it will be auto-set on first call to UpdateTime)
    m_nLoadingPreset   = 0;
    m_nPresetsLoadedTotal = 0;
    m_fSnapPoint = 0.5f;
	m_pState    = &m_state_DO_NOT_USE[0];
	m_pOldState = &m_state_DO_NOT_USE[1];
	m_pNewState = &m_state_DO_NOT_USE[2];
	m_UI_mode			= UI_REGULAR;
    m_bShowShaderHelp = false;

    m_nMashSlot = 0;    //0..MASH_SLOTS-1
    for (int mash=0; mash<MASH_SLOTS; mash++)
        m_nLastMashChangeFrame[mash] = 0;

    m_bUserPagedUp      = false;
    m_bUserPagedDown    = false;
	m_fMotionVectorsTempDx = 0.0f;
	m_fMotionVectorsTempDy = 0.0f;

    m_waitstring.bActive		= false;
	m_waitstring.bOvertypeMode  = false;
	m_waitstring.szClipboard[0] = 0;

	m_nPresets		= 0;
	m_nDirs			= 0;
    m_nPresetListCurPos = 0;
	m_nCurrentPreset = -1;
	m_szCurrentPresetFile[0] = 0;
    m_szLoadingPreset[0] = 0;
	//m_szPresetDir[0] = 0; // will be set @ end of this function
    m_bPresetListReady = false;
    m_szUpdatePresetMask[0] = 0;

    myfft.Init(576, MY_FFT_SAMPLES, -1);
	memset(&mysound, 0, sizeof(mysound));

    for (int i=0; i<PRESET_HIST_LEN; i++)
    m_presetHistory[i] = L"";
    m_presetHistoryPos = 0;
    m_presetHistoryBackFence = 0;
    m_presetHistoryFwdFence = 0;
    m_playlistIdxPresets = -1;   // SURI: PlaylistIndices() cache is empty
    m_playlistIdxDirs = -1;
    m_playlistIdxGen = 0;
    m_plPageRows = 10;           // SURI: playlist screen (until its first frame is drawn)
    m_plTop[0] = m_plTop[1] = 0;
    m_importSingle = false;      // SURI: drag & drop "new presets" prompt
    m_importPageRows = 10;
    m_importTop = 0;
    m_pfnIsFullscreen = NULL;    // SURI: the window code sets these after PluginPreInitialize
    m_pfnRunCommand = NULL;

	m_bShowFPS			= false;
	m_bShowRating		= false;
	m_bShowPresetInfo	= false;
	m_bShowDebugInfo	= false;
	m_bShowSongTitle	= false;
	m_bShowSongTime		= false;
	m_bShowSongLen		= false;
	m_fShowRatingUntilThisTime = -1.0f;
	m_ui_band_left = m_ui_band_right = 0;   // SURI
	m_toast_top = m_toast_xL = m_toast_xR = 0;
	SetRect(&m_toast_avoid, 0, 0, 0, 0);
	ClearErrors();
	m_szDebugMessage[0]	= 0;
    m_szSongTitle[0]    = 0;
    m_szSongTitlePrev[0] = 0;

	m_lpVS[0]				= NULL;
	m_lpVS[1]				= NULL;
    #if (NUM_BLUR_TEX>0)
        for (i=0; i<NUM_BLUR_TEX; i++)
            m_lpBlur[i] = NULL;
    #endif
    m_lpDDSTitle			= NULL;
    m_nTitleTexSizeX        = 0;
    m_nTitleTexSizeY        = 0;
	m_verts					= NULL;
	m_verts_temp            = NULL;
	m_vertinfo				= NULL;
	m_indices_list			= NULL;
	m_indices_strip			= NULL;

	m_bMMX			        = false;
    m_bHasFocus             = true;
    m_bHadFocus             = false;
    m_bOrigScrollLockState  = GetKeyState(VK_SCROLL) & 1;
    // m_bMilkdropScrollLockState is derived at end of MyReadConfig()

	m_nNumericInputMode   = NUMERIC_INPUT_MODE_CUST_MSG;
	m_nNumericInputNum    = 0;
	m_nNumericInputDigits = 0;
	m_nSelectedSprite     = -1;
	m_bSpriteDrag         = false;
	m_ptSpriteDrag.x      = 0;
	m_ptSpriteDrag.y      = 0;
	ZeroMemory(m_spriteQuad, sizeof(m_spriteQuad));
	m_nSpriteSnap         = 0;
	m_nSpriteSnapSel      = -1;


	m_supertext.bRedrawSuperText = false;
	m_supertext.fStartTime = -1.0f;
	m_supertext.fHoldTime = 0;
	m_supertext.fOutFadeTime = 0;
	m_nTitleFontPx = 0;

	// --------------------other init--------------------

    g_bDebugOutput		= false;
	g_bDumpFileCleared	= false;

    wchar_t buffer[MAX_PATH];
    GetModuleFileNameW(NULL, buffer, MAX_PATH);
    std::wstring::size_type pos = std::wstring(buffer).find_last_of(L"\\/");
    std::wstring dir = std::wstring(buffer).substr(0, pos);

    swprintf(m_szMilkdrop2Path, L"%s\\plugins\\%s", dir.c_str(), SUBDIR);
    swprintf(m_szPresetDir, L"%spresets\\", m_szMilkdrop2Path);

    // note that the config dir can be under Program Files or Application Data!!
    wchar_t szConfigDir[MAX_PATH] = {0};
    lstrcpyW(szConfigDir, GetConfigIniFile());
    wchar_t* p = wcsrchr(szConfigDir, L'\\');
    if (p) *(p+1) = 0;
   	swprintf(m_szMsgIniFile, L"%s%s", szConfigDir, MSG_INIFILE );
	swprintf(m_szImgIniFile, L"%s%s", szConfigDir, IMG_INIFILE );
}

//----------------------------------------------------------------------

void CPlugin::MyReadConfig()
{
    // Read the user's settings from the .INI file.

    // use this function         declared in   to read a value of this type:
    // -----------------         -----------   ----------------------------
    // GetPrivateProfileInt      Win32 API     int
    // GetPrivateProfileBool     utility.h     bool
    // GetPrivateProfileBOOL     utility.h     BOOL
    // GetPrivateProfileFloat    utility.h     float
    // GetPrivateProfileString   Win32 API     string


	int n=0;
    wchar_t *pIni = GetConfigIniFile();

	m_bFirstRun		= !GetPrivateProfileBoolW(L"settings",L"bConfigured" ,false,pIni);
	m_bEnableRating = GetPrivateProfileBoolW(L"settings",L"bEnableRating",m_bEnableRating,pIni);
	m_bHardCutsDisabled = GetPrivateProfileBoolW(L"settings",L"bHardCutsDisabled",m_bHardCutsDisabled,pIni);
	g_bDebugOutput	= GetPrivateProfileBoolW(L"settings",L"bDebugOutput",g_bDebugOutput,pIni);
	m_bShowPressF1ForHelp = GetPrivateProfileBoolW(L"settings",L"bShowPressF1ForHelp",m_bShowPressF1ForHelp,pIni);
	m_bSongTitleAnims   = GetPrivateProfileBoolW(L"settings",L"bSongTitleAnims",m_bSongTitleAnims,pIni);
	m_bNowPlaying       = GetPrivateProfileBoolW(L"settings",L"bNowPlaying",m_bNowPlaying,pIni);

	m_bShowFPS			= GetPrivateProfileBoolW(L"settings",L"bShowFPS",       m_bShowFPS			,pIni);
	m_bShowRating		= GetPrivateProfileBoolW(L"settings",L"bShowRating",    m_bShowRating		,pIni);
	m_bShowPresetInfo	= GetPrivateProfileBoolW(L"settings",L"bShowPresetInfo",m_bShowPresetInfo	,pIni);
	m_bShowSongTitle	= GetPrivateProfileBoolW(L"settings",L"bShowSongTitle", m_bShowSongTitle	,pIni);
	m_bShowSongTime		= GetPrivateProfileBoolW(L"settings",L"bShowSongTime",  m_bShowSongTime	,pIni);
	m_bShowSongLen		= GetPrivateProfileBoolW(L"settings",L"bShowSongLen",   m_bShowSongLen		,pIni);

	int nTemp = GetPrivateProfileBoolW(L"settings",L"bFixPinkBug",-1,pIni);
	if (nTemp == 0)
		m_n16BitGamma = 0;
	else if (nTemp == 1)
		m_n16BitGamma = 2;
	m_n16BitGamma		= GetPrivateProfileIntW(L"settings",L"n16BitGamma",m_n16BitGamma,pIni);
	m_bAutoGamma        = GetPrivateProfileBoolW(L"settings",L"bAutoGamma",m_bAutoGamma,pIni);
	m_bWarningsDisabled2    = GetPrivateProfileBoolW(L"settings",L"bWarningsDisabled2",m_bWarningsDisabled2,pIni);
    m_bPresetLockOnAtStartup = GetPrivateProfileBoolW(L"settings",L"bPresetLockOnAtStartup",m_bPresetLockOnAtStartup,pIni);
	m_bPreventScollLockHandling = GetPrivateProfileBoolW(L"settings",L"m_bPreventScollLockHandling",m_bPreventScollLockHandling,pIni);

    m_nCanvasStretch = 100;
	m_nTexSizeX		= -1;
	m_nTexSizeY		= -1;
	m_bTexSizeWasAutoPow2   = (m_nTexSizeX == -2);
	m_bTexSizeWasAutoExact = (m_nTexSizeX == -1);
	m_nTexBitsPerCh = GetPrivateProfileIntW(L"settings", L"nTexBitsPerCh", m_nTexBitsPerCh, pIni);
	m_nGridX		= 64;
	m_nGridY        = 64;
    m_nMaxPSVersion_ConfigPanel = GetPrivateProfileIntW(L"settings",L"MaxPSVersion",m_nMaxPSVersion_ConfigPanel,pIni);
    m_nMaxImages    = 3000;
    m_nMaxBytes     = 2000000000;

	m_fBlendTimeUser			= GetPrivateProfileFloatW(L"settings",L"fBlendTimeUser"         ,m_fBlendTimeUser         ,pIni);
	m_fBlendTimeAuto			= GetPrivateProfileFloatW(L"settings",L"fBlendTimeAuto"         ,m_fBlendTimeAuto         ,pIni);
	m_fTimeBetweenPresets		= GetPrivateProfileFloatW(L"settings",L"fTimeBetweenPresets"    ,m_fTimeBetweenPresets    ,pIni);
	m_fTimeBetweenPresetsRand	= GetPrivateProfileFloatW(L"settings",L"fTimeBetweenPresetsRand",m_fTimeBetweenPresetsRand,pIni);
	m_fHardCutLoudnessThresh	= GetPrivateProfileFloatW(L"settings",L"fHardCutLoudnessThresh" ,m_fHardCutLoudnessThresh ,pIni);
	m_fHardCutHalflife			= GetPrivateProfileFloatW(L"settings",L"fHardCutHalflife"       ,m_fHardCutHalflife       ,pIni);
	m_fSongTitleAnimDuration	= GetPrivateProfileFloatW(L"settings",L"fSongTitleAnimDuration" ,m_fSongTitleAnimDuration ,pIni);
	m_fSongTitleHold			= GetPrivateProfileFloatW(L"settings",L"fSongTitleHold"         ,m_fSongTitleHold         ,pIni);
	m_fSongTitleFade			= GetPrivateProfileFloatW(L"settings",L"fSongTitleFade"         ,m_fSongTitleFade         ,pIni);
	m_fTimeBetweenRandomSongTitles = GetPrivateProfileFloatW(L"settings",L"fTimeBetweenRandomSongTitles" ,m_fTimeBetweenRandomSongTitles,pIni);
	m_fTimeBetweenRandomCustomMsgs = GetPrivateProfileFloatW(L"settings",L"fTimeBetweenRandomCustomMsgs" ,m_fTimeBetweenRandomCustomMsgs,pIni);
    m_adapterId = GetPrivateProfileIntW(L"settings", L"nVideoAdapterIndex", 0, pIni);

    // --------

	GetPrivateProfileStringW(L"settings",L"szPresetDir",m_szPresetDir,m_szPresetDir,sizeof(m_szPresetDir),pIni);

	ReadCustomMessages();

	// bounds-checking:
	if (m_nGridX > MAX_GRID_X)
		m_nGridX = MAX_GRID_X;
	if (m_nGridY > MAX_GRID_Y)
		m_nGridY = MAX_GRID_Y;
	if (m_fTimeBetweenPresetsRand < 0)
		m_fTimeBetweenPresetsRand = 0;
	if (m_fTimeBetweenPresets < 0.1f)
		m_fTimeBetweenPresets = 0.1f;

    // DERIVED SETTINGS
    m_bPresetLockedByUser      = m_bPresetLockOnAtStartup;
}

//----------------------------------------------------------------------

void CPlugin::MyWriteConfig()
{
    // Write the user's settings to the .INI file.
    // Called when MilkDrop exits.

    // use this function         declared in   to write a value of this type:
    // -----------------         -----------   ----------------------------
    // WritePrivateProfileInt    Win32 API     int
    // WritePrivateProfileInt    utility.h     bool
    // WritePrivateProfileInt    utility.h     BOOL
    // WritePrivateProfileFloat  utility.h     float
    // WritePrivateProfileString Win32 API     string


    wchar_t *pIni = GetConfigIniFile();

	// constants:
	WritePrivateProfileStringW(L"settings",L"bConfigured",L"1",pIni);

	//note: m_szPresetDir is not written here; it is written manually, whenever it changes.

	wchar_t szSectionName[] = L"settings";

	WritePrivateProfileIntW(m_bSongTitleAnims,		L"bSongTitleAnims",		pIni, L"settings");
	WritePrivateProfileIntW(m_bNowPlaying,			L"bNowPlaying",			pIni, L"settings");
	WritePrivateProfileIntW(m_bHardCutsDisabled,	    L"bHardCutsDisabled",	pIni, L"settings");
	WritePrivateProfileIntW(m_bEnableRating,		    L"bEnableRating",		pIni, L"settings");
	WritePrivateProfileIntW(g_bDebugOutput,		    L"bDebugOutput",			pIni, L"settings");


	WritePrivateProfileIntW(m_bShowPressF1ForHelp,   L"bShowPressF1ForHelp",	pIni, L"settings");
	WritePrivateProfileIntW(m_n16BitGamma, 		    L"n16BitGamma",			pIni, L"settings");
	WritePrivateProfileIntW(m_bAutoGamma,  		    L"bAutoGamma",			pIni, L"settings");

	WritePrivateProfileIntW(m_bWarningsDisabled2,	L"bWarningsDisabled2",	pIni, L"settings");
    WritePrivateProfileIntW(m_bPresetLockOnAtStartup,L"bPresetLockOnAtStartup",pIni,L"settings");
	WritePrivateProfileIntW(m_bPreventScollLockHandling,L"m_bPreventScollLockHandling",pIni,L"settings");
    // note: this is also written @ exit of the visualizer

    WritePrivateProfileIntW(100,        L"nCanvasStretch",   	pIni, L"settings");
    WritePrivateProfileIntW(-1,			    L"nTexSize",				pIni, L"settings");
	WritePrivateProfileIntW(m_nTexBitsPerCh,         L"nTexBitsPerCh",        pIni, L"settings");
	WritePrivateProfileIntW(64, 				L"nMeshSize",			pIni, L"settings");
	WritePrivateProfileIntW(3, L"MaxPSVersion",  	pIni, L"settings");
    WritePrivateProfileIntW(64, L"MaxImages",  	pIni, L"settings");
    WritePrivateProfileIntW(2000000000 , L"MaxBytes",  	pIni, L"settings");

	WritePrivateProfileFloatW(m_fBlendTimeAuto,          L"fBlendTimeAuto",           pIni, L"settings");
	WritePrivateProfileFloatW(m_fBlendTimeUser,          L"fBlendTimeUser",           pIni, L"settings");
	WritePrivateProfileFloatW(m_fTimeBetweenPresets,     L"fTimeBetweenPresets",      pIni, L"settings");
	WritePrivateProfileFloatW(m_fTimeBetweenPresetsRand, L"fTimeBetweenPresetsRand",  pIni, L"settings");
	WritePrivateProfileFloatW(m_fHardCutLoudnessThresh,  L"fHardCutLoudnessThresh",   pIni, L"settings");
	WritePrivateProfileFloatW(m_fHardCutHalflife,        L"fHardCutHalflife",         pIni, L"settings");
	WritePrivateProfileFloatW(m_fSongTitleAnimDuration,  L"fSongTitleAnimDuration",   pIni, L"settings");
	WritePrivateProfileFloatW(m_fSongTitleHold,          L"fSongTitleHold",           pIni, L"settings");
	WritePrivateProfileFloatW(m_fSongTitleFade,          L"fSongTitleFade",           pIni, L"settings");
	WritePrivateProfileFloatW(m_fTimeBetweenRandomSongTitles,L"fTimeBetweenRandomSongTitles",pIni, L"settings");
	WritePrivateProfileFloatW(m_fTimeBetweenRandomCustomMsgs,L"fTimeBetweenRandomCustomMsgs",pIni, L"settings");

    WritePrivateProfileIntW(m_adapterId, L"nVideoAdapterIndex", pIni, L"settings");

}

//----------------------------------------------------------------------

void ConvertLLCto1310(char* d, const char *s)
{
    // src and dest can NOT be the same pointer.
    assert(s != d);

    while (*s)
    {
        if (*s == LINEFEED_CONTROL_CHAR)
        {
            *d++ = 13;
            *d++ = 10;
        }
        else
        {
            *d++ = *s;
        }
        s++;
    };
    *d = 0;
}

void StripComments(char* str)
{
    if (!str || !str[0] || !str[1])
        return;

    char c0 = str[0];
    char c1 = str[1];
    char* dest = str;
    char* p = &str[1];
    bool bIgnoreTilEndOfLine = false;
    bool bIgnoreTilCloseComment = false; //this one takes precedence
    int nCharsToSkip = 0;
    while (1)
    {
        // handle '//' comments
        if (!bIgnoreTilCloseComment && c0=='/' && c1=='/')
            bIgnoreTilEndOfLine = true;
        if (bIgnoreTilEndOfLine && (c0==10 || c0==13))
        {
            bIgnoreTilEndOfLine = false;
            nCharsToSkip = 0;
        }

        // handle /* */ comments
        if (!bIgnoreTilEndOfLine && c0=='/' && c1=='*')
            bIgnoreTilCloseComment = true;
        if (bIgnoreTilCloseComment && c0=='*' && c1=='/')
        {
            bIgnoreTilCloseComment = false;
            nCharsToSkip = 2;
        }

        if (!bIgnoreTilEndOfLine && !bIgnoreTilCloseComment)
        {
            if (nCharsToSkip > 0)
                nCharsToSkip--;
            else
                *dest++ = c0;
        }

        if (c1==0)
            break;

        p++;
        c0 = c1;
        c1 = *p;
    }

    *dest++ = 0;
}

int CPlugin::AllocateMyNonDx9Stuff()
{
    // This gets called only once, when MilkDrop starts.
    // If anything fails here, return FALSE to safely exit the plugin,
    //   but only after displaying a messagebox giving the user some information
    //   about what went wrong.


    g_hThread = INVALID_HANDLE_VALUE;
    g_bThreadAlive = false;
    g_bThreadShouldQuit = false;
	InitializeCriticalSection(&g_cs);

    // read in 'm_szShaderIncludeText'
    bool bSuccess = true;
    bSuccess = ReadFileToString(L"data\\include.fx", m_szShaderIncludeText, sizeof(m_szShaderIncludeText)-4, false);
	if (!bSuccess) return false;
	StripComments(m_szShaderIncludeText);
	m_nShaderIncludeTextLen = lstrlen(m_szShaderIncludeText);
    bSuccess |= ReadFileToString(L"data\\warp_vs.fx", m_szDefaultWarpVShaderText, sizeof(m_szDefaultWarpVShaderText), true);
    if (!bSuccess) return false;
    bSuccess |= ReadFileToString(L"data\\warp_ps.fx", m_szDefaultWarpPShaderText, sizeof(m_szDefaultWarpPShaderText), true);
	if (!bSuccess) return false;
    bSuccess |= ReadFileToString(L"data\\comp_vs.fx", m_szDefaultCompVShaderText, sizeof(m_szDefaultCompVShaderText), true);
    if (!bSuccess) return false;
    bSuccess |= ReadFileToString(L"data\\comp_ps.fx", m_szDefaultCompPShaderText, sizeof(m_szDefaultCompPShaderText), true);
    if (!bSuccess) return false;
    bSuccess |= ReadFileToString(L"data\\blur_vs.fx", m_szBlurVS, sizeof(m_szBlurVS), true);
    if (!bSuccess) return false;
    bSuccess |= ReadFileToString(L"data\\blur1_ps.fx", m_szBlurPSX, sizeof(m_szBlurPSX), true);
    if (!bSuccess) return false;
    bSuccess |= ReadFileToString(L"data\\blur2_ps.fx", m_szBlurPSY, sizeof(m_szBlurPSY), true);
    if (!bSuccess) return false;

	BuildMenus();

	m_bMMX = CheckForMMX();

	m_pState->Default();
	m_pOldState->Default();
    m_pNewState->Default();

	// SURI: GDI+ loads .gif sprite images (failure only disables GIF sprites)
	GdiplusStartupOnce();

	// SURI: background reader of the current song (Windows media controls); returns at once
	if (m_bNowPlaying)
		m_nowPlaying.Start(&m_songBox);

	//LoadRandomPreset(0.0f);   -avoid this here; causes some DX9 stuff to happen.

    return true;
}

//----------------------------------------------------------------------

void CancelThread(int max_wait_time_ms)
{
    g_bThreadShouldQuit = true;
    int waited = 0;
    while (g_bThreadAlive && waited < max_wait_time_ms)
    {
        Sleep(30);
        waited += 30;
    }

    if (g_bThreadAlive)
    {
        TerminateThread(g_hThread,0);
        g_bThreadAlive = false;
    }

    if (g_hThread != INVALID_HANDLE_VALUE)
        CloseHandle(g_hThread);
    g_hThread = INVALID_HANDLE_VALUE;
}

void CPlugin::CleanUpMyNonDx9Stuff()
{
    // This gets called only once, when your plugin exits.
    // Be sure to clean up any objects here that were
    //   created/initialized in AllocateMyNonDx9Stuff.


    // NOTE: DO NOT DELETE m_gdi_titlefont_doublesize HERE!!!

    // SURI: stop the preset-scan thread before deleting the critical section it uses, and
    // give it time to see the quit flag. Killing it at once (TerminateThread) could leave
    // a lock held (g_cs or the CRT heap lock) and hang the process on exit.
    CancelThread(3000);

    // SURI: the now-playing thread waits at most kNowPlayingJoinMs
    m_nowPlaying.Stop();

    DeleteCriticalSection(&g_cs);

	m_menuPreset  .Finish();
	m_menuWave    .Finish();
	m_menuAugment .Finish();
    m_menuCustomWave.Finish();
    m_menuCustomShape.Finish();
	m_menuMotion  .Finish();
	m_menuPost    .Finish();
    for (int i=0; i<MAX_CUSTOM_WAVES; i++)
	    m_menuWavecode[i].Finish();
    for (i=0; i<MAX_CUSTOM_SHAPES; i++)
	    m_menuShapecode[i].Finish();

	GdiplusShutdownOnce();
}

//----------------------------------------------------------------------

float SquishToCenter(float x, float fExp)
{
    if (x > 0.5f)
        return powf(x*2-1, fExp)*0.5f + 0.5f;

    return (1-powf(1-x*2, fExp))*0.5f;
}

int GetNearestPow2Size(int w, int h)
{
	float fExp = logf( max(w,h)*0.75f + 0.25f*min(w,h) ) / logf(2.0f);
    float bias = 0.55f;
    if (fExp + bias >= 11.0f)   // ..don't jump to 2048x2048 quite as readily
        bias = 0.5f;
	int   nExp = (int)(fExp + bias);
	int log2size = (int)powf(2.0f, (float)nExp);
    return log2size;
}

int CPlugin::AllocateMyDX9Stuff()
{
    // (...aka OnUserResizeWindow)
    // (...aka OnToggleFullscreen)

    // Allocate and initialize all your DX9 and D3DX stuff here: textures,
    //   surfaces, vertex/index buffers, D3DX fonts, and so on.
    // If anything fails here, return FALSE to safely exit the plugin,
    //   but only after displaying a messagebox giving the user some information
    //   about what went wrong.  If the error is NON-CRITICAL, you don't *have*
    //   to return; just make sure that the rest of the code will be still safely
    //   run (albeit with degraded features).
    // If you run out of video memory, you might want to show a short messagebox
    //   saying what failed to allocate and that the reason is a lack of video
    //   memory, and then call SuggestHowToFreeSomeMem(), which will show them
    //   a *second* messagebox that (intelligently) suggests how they can free up
    //   some video memory.
    // Don't forget to account for each object you create/allocate here by cleaning
    //   it up in CleanUpMyDX9Stuff!
    // IMPORTANT:
    //   Note that the code here isn't just run at program startup!
    //   When the user toggles between fullscreen and windowed modes
    //   or resizes the window, the base class calls this function before
    //   destroying & recreating the plugin window and DirectX object, and then
    //   calls AllocateMyDX9Stuff afterwards, to get your plugin running again.

    wchar_t buf[32768], title[64];

    m_nFramesSinceResize = 0;

    int nNewCanvasStretch = (m_nCanvasStretch == 0) ? 100 : m_nCanvasStretch;

    DWORD PSVersion = GetCaps()->PixelShaderVersion & 0xFFFF;  // 0x0300, etc.
    if (PSVersion >= 0x0300)
        m_nMaxPSVersion_DX9 = MD2_PS_3_0;
		else if (PSVersion > 0x0200)
        m_nMaxPSVersion_DX9 = MD2_PS_2_X;
    else if (PSVersion >= 0x0200)
        m_nMaxPSVersion_DX9 = MD2_PS_2_0;
    else
        m_nMaxPSVersion_DX9 = MD2_PS_NONE;

    if (m_nMaxPSVersion_ConfigPanel == -1)
        m_nMaxPSVersion = m_nMaxPSVersion_DX9;
    else
    {
        // to still limit their choice by what HW reports:
        //m_nMaxPSVersion = min(m_nMaxPSVersion_DX9, m_nMaxPSVersion_ConfigPanel);

        // to allow them to override:
        m_nMaxPSVersion = m_nMaxPSVersion_ConfigPanel;
    }

    /*
       Auto mode: do a check against a few known, *SLOW* DX9/ps_2_0 cards to see
        if we should run them without pixel shaders instead.
       Here is valve's list of the cards they run DX8 on (mostly because they're too slow under DX9 + ps_2_0):
            NVIDIA GeForce FX 5200  31.12%
            ATI Radeon 9200         21.29%
            NVIDIA GeForce FX 5500  11.27%
            NVIDIA GeForce4          7.74%
            NVIDIA GeForce FX 5700   7.12%
            NVIDIA GeForce FX 5600   5.16%
            SiS 661FX_760_741        3.34%
            NVIDIA GeForce FX 5900   3.24%
            NVIDIA GeForce3          2.09%
            ATI Radeon 9000          1.98%
            other                    5.66%
            [ from http://www.steampowered.com/status/survey.html ]
            see also:
                http://en.wikipedia.org/wiki/Radeon
                http://en.wikipedia.org/wiki/Geforce_fx
    */

    const char* szGPU = GetDriverDescription();
    /* known examples of this string:
        "ATI MOBILITY RADEON X600"
        "RADEON X800 Series   "     <- note the spaces at the end
        "Sapphire RADEON X700"
        "NVIDIA GeForce Go 6200  "  <- note the spaces at the end
        "NVIDIA GeForce 6800 GT"
        "Intel(R) 82865G Graphics Controller"
        "Mobile Intel(R) 915GM/GMS,910GML Express Chipset Family"

		might want to consider adding these to the list: [from http://www.intel.com/support/graphics/sb/cs-014257.htm ]
			(...they should support PS2.0, but not sure if they're fast...)
        "Mobile Intel(R) 945GM Express Chipset Family"
        "Mobile Intel(R) 915GM/GMS,910GML Express Chipset"
        "Intel(R) 945G Express Chipset"
        "Intel(R) 82915G/82910GL Express Chipset Family"

		or these, if they seem to be reporting that they do support ps_2_0, which would be very bogus info:
        "Intel(R) 82865G Graphics Controller"
        "Intel(R) 82852/82855 Graphics Controller Family"
        "Intel(R) 82845G Graphics Controller"
        "Intel(R) 82830M Graphics Controller"
        "Intel(R) 82815 Graphics Controller"
        "Intel(R) 82810 Graphics Controller"
    */

    // GREY LIST - slow ps_2_0 cards
    // In Canvas Stretch==Auto mode, for these cards, if they (claim to) run ps_2_0,
    //   we run at half-res (cuz they're slow).
    // THE GENERAL GUIDELINE HERE:
    //   It should be at least as fast as a GeForce FX 5700 or my GeForce 6200 (TC)
    //   if it's to run without stretch.
    if (m_nCanvasStretch==0)// && m_nMaxPSVersion_DX9 > 0)
    {
        // put cards on this list if you see them successfully run ps_2_0 (using override)
        // and they run well at a low resolution (512x512 or less).
        if (
            strstr(szGPU, "GeForce 4"       ) ||    // probably not even ps_2_0
            strstr(szGPU, "GeForce FX 52"   ) ||    // chip's computer (FX 5200) - does do ps_2_0, but slow
            strstr(szGPU, "GeForce FX 53"   ) ||
            strstr(szGPU, "GeForce FX 54"   ) ||
            strstr(szGPU, "GeForce FX 55"   ) ||   //GeForce FX 5600 is 13 GB/s - 2.5x as fast as my 6200!
            strstr(szGPU, "GeForce FX 56"   ) ||
             //...GeForce FX 5700 and up, we let those run at full-res on ps_2_0...
            strstr(szGPU, "GeForce FX 56"   ) ||
            strstr(szGPU, "GeForce FX 56"   ) ||
            strstr(szGPU, "SiS 300/305/630/540/730") ||    // mom's computer - just slow.
            strstr(szGPU, "Radeon 8"   ) ||    // no shader model 2.
            strstr(szGPU, "Radeon 90"  ) ||    // from Valve.  no shader model 2.
            strstr(szGPU, "Radeon 91"  ) ||    // no shader model 2.
            strstr(szGPU, "Radeon 92"  ) ||    // from Valve.  no shader model 2.
            strstr(szGPU, "Radeon 93"  ) ||    // no shader model 2.
            strstr(szGPU, "Radeon 94"  ) ||    // no shader model 2.
            // guessing that 9500+ are ok - they're all ps_2_0 and the 9600 is like an FX 5900.
            strstr(szGPU, "Radeon 9550") ||  // *maybe* - kiv - super budget R200 chip.  def. ps_2_0 but possibly very slow.
            strstr(szGPU, "Radeon X300") ||  // *maybe* - kiv - super budget R200 chip   def. ps_2_0 but possibly very slow.
            0)
        {
            nNewCanvasStretch = 200;
        }
    }

    /*                           pix pipes
                               core    Fill(G)  membw(GB/s)
        Radeon 9600 Pro	        400	 4	1.6	     9.6
        Radeon 9600 XT	        500	 4	2.0	     9.6
        GeForce FX 5600 Ultra	400	 4	1.6	    12.8
        GeForce FX 5700 Ultra	475	 4	1.9	    14.4
        GeForce FX 5900 XT	    400	 4	1.6	    22.4
        GeForce FX 5900	        450	 4	1.8	    27.2
        GeForce FX 5950 Ultra 	475  4  2.9     30
        GeForce 6200 TC-32 	    350  4  1.1      5.6 (TurboDonkey)
        GeForce 6600 GT 	    500  8  2.0     16
        GeForce 6800 Ultra 	    400 16  6.4     35
        ATI Radeon X800 XT PE 	520 16  8.3     36
        ATI Radeon X850 XT PE   540 16  8.6     38

        Entry-level GPU 	5200, 5300, 5500
        Mid-Range GPU 	    5600, 5700
        High-end GPU 	    5800, 5900, 5950

        Entry-level GPU 	6200, 6500
        Mid-Range GPU 	    6600
        High-end GPU 	    6800

        Entry-level GPU
        Mid-Range GPU
        High-end GPU

        R200: only ps_1_4.  Radeon 8500-9250.
        R300: ps_2_0.       Radeon 9500-9800, X300-X600, X1050.  9600 fast enough (~FX5900).  9500/9700 ~ GeForce 4 Ti.
        R420: ps_2_0        Radeon X700-8750 - ALL FAST ENOUGH.  X700 is same speed as a GeForce 6600.

        6600		    ~	X700
        GeForce 4		<	X300 / X600 / 9600
        GeForce 4 Ti	>	Radeon 8500
        FX 5900		    =	Radeon 9600
        FX 5900 Ultra	<< [half]	Radeon 9800 Pro
        GeForce FX		<	Radeon 9700/9800
    */

    // BLACK LIST
    // In Pixel Shaders==Auto mode, for these cards, we avoid ps_2_0 completely.
    // There shouldn't be much on this list... feel free to put anything you KNOW doesn't do ps_2_0 (why not),
    // and to put anything that is slow to begin with, and HAS BUGGY DRIVERS (INTEL).
    if (m_nMaxPSVersion_ConfigPanel==-1)
    {
        if (strstr(szGPU, "GeForce2"     ) ||    // from Valve
            strstr(szGPU, "GeForce3"     ) ||    // from Valve
            strstr(szGPU, "GeForce4"     ) ||    // from Valve
            strstr(szGPU, "Radeon 7"     ) ||    // from Valve
            strstr(szGPU, "Radeon 8"     ) ||
            strstr(szGPU, "SiS 661FX_760_741") ||    // from Valve
                //FOR NOW, FOR THESE, ASSUME INTEL EITHER DOESN'T DO PS_2_0,
                //OR DRIVERS SUCK AND IT WOULDN'T WORK ANYWAY!
                (strstr(szGPU,"Intel") && strstr(szGPU,"945G")) ||
                (strstr(szGPU,"Intel") && strstr(szGPU,"915G")) ||  // ben allison's laptop - snow, freezing when you try ps_2_0
                (strstr(szGPU,"Intel") && strstr(szGPU,"910G")) ||
            (strstr(szGPU,"Intel") && strstr(szGPU,"8291")) ||     // gonna guess that this supports ps_2_0 but is SLOW
            (strstr(szGPU,"Intel") && strstr(szGPU,"8281")) ||     // definitely DOESN'T support pixel shaders
            (strstr(szGPU,"Intel") && strstr(szGPU,"8283")) ||     // definitely DOESN'T support pixel shaders
            (strstr(szGPU,"Intel") && strstr(szGPU,"8284")) ||     // definitely DOESN'T support pixel shaders
            (strstr(szGPU,"Intel") && strstr(szGPU,"8285")) ||     // definitely DOESN'T support pixel shaders
            (strstr(szGPU,"Intel") && strstr(szGPU,"8286")) ||     // definitely DOESN'T support pixel shaders.  Ben Allison's desktop (865) - no image w/ps_2_0.  Plus Nes's desktop - no ps_2_0.
            0)
        {
            m_nMaxPSVersion = MD2_PS_NONE;
        }
    }


    // Note: this code used to be in OnResizeGraphicsWindow().

    // SHADERS
    //-------------------------------------
    if (m_nMaxPSVersion > MD2_PS_NONE)
    {
        // Create vertex declarations (since we're not using FVF anymore)
        if (D3D_OK != GetDevice()->CreateVertexDeclaration( g_MyVertDecl, &m_pMyVertDecl ))
        {
			LoadResString(IDS_COULD_NOT_CREATE_MY_VERTEX_DECLARATION,buf,sizeof(buf));
		    dumpmsg(buf);
		    MessageBoxW(GetPluginWindow(), buf, LoadResString(IDS_MILKDROP_ERROR,title,sizeof(title)), MB_OK|MB_SETFOREGROUND|MB_TOPMOST );
		    return false;
        }
        if (D3D_OK != GetDevice()->CreateVertexDeclaration( g_WfVertDecl, &m_pWfVertDecl ))
        {
			LoadResString(IDS_COULD_NOT_CREATE_WF_VERTEX_DECLARATION,buf,sizeof(buf));
		    dumpmsg(buf);
		    MessageBoxW(GetPluginWindow(), buf, LoadResString(IDS_MILKDROP_ERROR,title,sizeof(title)), MB_OK|MB_SETFOREGROUND|MB_TOPMOST );
		    return false;
        }
        if (D3D_OK != GetDevice()->CreateVertexDeclaration( g_SpriteVertDecl, &m_pSpriteVertDecl ))
        {
			LoadResString(IDS_COULD_NOT_CREATE_SPRITE_VERTEX_DECLARATION,buf,sizeof(buf));
		    dumpmsg(buf);
		    MessageBoxW(GetPluginWindow(), buf, LoadResString(IDS_MILKDROP_ERROR,title,sizeof(title)), MB_OK|MB_SETFOREGROUND|MB_TOPMOST );
		    return false;
				}

				// Load the FALLBACK shaders...
				if (!RecompilePShader(m_szDefaultWarpPShaderText, &m_fallbackShaders_ps.warp, SHADER_WARP, true, 2))
				{
					wchar_t szSM[64];
					switch(m_nMaxPSVersion_DX9)
					{
					case MD2_PS_2_0:
					case MD2_PS_2_X:
						LoadResString(IDS_SHADER_MODEL_2,szSM,64); break;
					case MD2_PS_3_0: LoadResString(IDS_SHADER_MODEL_3,szSM,64); break;
					case MD2_PS_4_0: LoadResString(IDS_SHADER_MODEL_4,szSM,64); break;
					default:
						swprintf(szSM, LoadResString(IDS_UKNOWN_CASE_X), m_nMaxPSVersion_DX9);
						break;
					}
					if (m_nMaxPSVersion_ConfigPanel >= MD2_PS_NONE && m_nMaxPSVersion_DX9 < m_nMaxPSVersion_ConfigPanel)
						swprintf(buf, LoadResString(IDS_FAILED_TO_COMPILE_PIXEL_SHADERS_USING_X),szSM,PSVersion);
					else
						swprintf(buf, LoadResString(IDS_FAILED_TO_COMPILE_PIXEL_SHADERS_HARDWARE_MIS_REPORT),szSM,PSVersion);
					dumpmsg(buf);
					MessageBoxW(GetPluginWindow(), buf, LoadResString(IDS_MILKDROP_ERROR,title,64), MB_OK|MB_SETFOREGROUND|MB_TOPMOST );
					return false;
				}
				if (!RecompileVShader(m_szDefaultWarpVShaderText, &m_fallbackShaders_vs.warp, SHADER_WARP, true))
				{
			LoadResString(IDS_COULD_NOT_COMPILE_FALLBACK_WV_SHADER,buf,sizeof(buf));
		    dumpmsg(buf);
		    MessageBoxW(GetPluginWindow(), buf, LoadResString(IDS_MILKDROP_ERROR,title,64), MB_OK|MB_SETFOREGROUND|MB_TOPMOST );
		    return false;
        }
        if (!RecompileVShader(m_szDefaultCompVShaderText, &m_fallbackShaders_vs.comp, SHADER_COMP, true))
        {
			LoadResString(IDS_COULD_NOT_COMPILE_FALLBACK_CV_SHADER,buf,sizeof(buf));
		    dumpmsg(buf);
		    MessageBoxW(GetPluginWindow(), buf, LoadResString(IDS_MILKDROP_ERROR,title,64), MB_OK|MB_SETFOREGROUND|MB_TOPMOST );
		    return false;
        }
        if (!RecompilePShader(m_szDefaultCompPShaderText, &m_fallbackShaders_ps.comp, SHADER_COMP, true, 2))
        {
			LoadResString(IDS_COULD_NOT_COMPILE_FALLBACK_CP_SHADER,buf,sizeof(buf));
		    dumpmsg(buf);
		    MessageBoxW(GetPluginWindow(), buf, LoadResString(IDS_MILKDROP_ERROR,title,64), MB_OK|MB_SETFOREGROUND|MB_TOPMOST );
		    return false;
        }

        // Load the BLUR shaders...
        if (!RecompileVShader(m_szBlurVS, &m_BlurShaders[0].vs, SHADER_BLUR, true))
        {
			LoadResString(IDS_COULD_NOT_COMPILE_BLUR1_VERTEX_SHADER,buf,sizeof(buf));
		    dumpmsg(buf);
		    MessageBoxW(GetPluginWindow(), buf, LoadResString(IDS_MILKDROP_ERROR,title,64), MB_OK|MB_SETFOREGROUND|MB_TOPMOST );
		    return false;
        }
        if (!RecompilePShader(m_szBlurPSX, &m_BlurShaders[0].ps, SHADER_BLUR, true, 2))
        {
		    LoadResString(IDS_COULD_NOT_COMPILE_BLUR1_PIXEL_SHADER,buf,sizeof(buf));
		    dumpmsg(buf);
		    MessageBoxW(GetPluginWindow(), buf, LoadResString(IDS_MILKDROP_ERROR,title,64), MB_OK|MB_SETFOREGROUND|MB_TOPMOST );
		    return false;
        }
        if (!RecompileVShader(m_szBlurVS, &m_BlurShaders[1].vs, SHADER_BLUR, true))
        {
			LoadResString(IDS_COULD_NOT_COMPILE_BLUR2_VERTEX_SHADER,buf,sizeof(buf));
		    dumpmsg(buf);
		    MessageBoxW(GetPluginWindow(), buf, LoadResString(IDS_MILKDROP_ERROR,title,64), MB_OK|MB_SETFOREGROUND|MB_TOPMOST );
		    return false;
        }
        if (!RecompilePShader(m_szBlurPSY, &m_BlurShaders[1].ps, SHADER_BLUR, true, 2))
        {
			LoadResString(IDS_COULD_NOT_COMPILE_BLUR2_PIXEL_SHADER,buf,sizeof(buf));
		    dumpmsg(buf);
		    MessageBoxW(GetPluginWindow(), buf, LoadResString(IDS_MILKDROP_ERROR,title,64), MB_OK|MB_SETFOREGROUND|MB_TOPMOST );
		    return false;
        }
    }

	// create m_lpVS[2]
    {
        int log2texsize = GetNearestPow2Size(GetWidth(), GetHeight());

        // auto-guess texsize
	    if (m_bTexSizeWasAutoExact)
		{
            // note: in windowed mode, the window could be weird sizes,
            //        so the plugin shell now gives us a slightly enlarged size,
            //        which pads it out to the nearest 32x32 block size,
            //        and then on display, it intelligently crops the image.
            //       This is pretty likely to work on non-shitty GPUs.
            //        but some shitty ones will still only do powers of 2!
            //       So if we are running out of video memory here or experience
            //        other problems, though, we can make our VS's smaller;
            //        which will work, although it will lead to stretching.
			m_nTexSizeX = GetWidth();
			m_nTexSizeY = GetHeight();
		}
        else if (m_bTexSizeWasAutoPow2)
	    {
		    m_nTexSizeX = log2texsize;
			m_nTexSizeY = log2texsize;
	    }

	    // clip texsize by max. from caps
	    if ((DWORD)m_nTexSizeX > GetCaps()->MaxTextureWidth && GetCaps()->MaxTextureWidth>0)
		    m_nTexSizeX = GetCaps()->MaxTextureWidth;
	    if ((DWORD)m_nTexSizeY > GetCaps()->MaxTextureHeight && GetCaps()->MaxTextureHeight>0)
		    m_nTexSizeY = GetCaps()->MaxTextureHeight;

        // apply canvas stretch
        m_nTexSizeX = (m_nTexSizeX * 100)/nNewCanvasStretch;
        m_nTexSizeY = (m_nTexSizeY * 100)/nNewCanvasStretch;

        // re-compute closest power-of-2 size, now that we've factored in the stretching...
        log2texsize = GetNearestPow2Size(m_nTexSizeX, m_nTexSizeY);
        if (m_bTexSizeWasAutoPow2)
	    {
		    m_nTexSizeX = log2texsize;
			m_nTexSizeY = log2texsize;
	    }

        // snap to 16x16 blocks
        m_nTexSizeX = ((m_nTexSizeX+15)/16)*16;
        m_nTexSizeY = ((m_nTexSizeY+15)/16)*16;

		// determine format for VS1/VS2
		D3DFORMAT fmt;
		switch(m_nTexBitsPerCh) {
		  case 5:  fmt = D3DFMT_R5G6B5   ; break;
		  case 8:  fmt = D3DFMT_X8R8G8B8 ; break;
		  case 10: fmt = D3DFMT_A2R10G10B10; break;  // D3DFMT_A2W10V10U10 or D3DFMT_A2R10G10B10 or D3DFMT_A2B10G10R10
		  case 16: fmt = D3DFMT_A16B16G16R16F; break;
		  case 32: fmt = D3DFMT_A32B32G32R32F; break; //FIXME
		  default: fmt = D3DFMT_X8R8G8B8 ; break;
		}

	    // reallocate
	    bool bSuccess = false;
        DWORD vs_flags = D3DUSAGE_RENDERTARGET;// | D3DUSAGE_AUTOGENMIPMAP;//FIXME! (make automipgen optional)
		bool bRevertedBitDepth = false;
	    do
	    {
		    SafeRelease(m_lpVS[0]);
		    SafeRelease(m_lpVS[1]);

		    // create VS1
            bSuccess = (GetDevice()->CreateTexture(m_nTexSizeX, m_nTexSizeY, 1, vs_flags, fmt, D3DPOOL_DEFAULT, &m_lpVS[0], NULL) == D3D_OK);
			if (!bSuccess)
			{
				bSuccess = (GetDevice()->CreateTexture(m_nTexSizeX, m_nTexSizeY, 1, vs_flags, GetBackBufFormat(), D3DPOOL_DEFAULT, &m_lpVS[0], NULL) == D3D_OK);
				if (bSuccess)
					fmt = GetBackBufFormat();
			}

			// create VS2
			if (bSuccess)
                bSuccess = (GetDevice()->CreateTexture(m_nTexSizeX, m_nTexSizeY, 1, vs_flags, fmt, D3DPOOL_DEFAULT, &m_lpVS[1], NULL) == D3D_OK);

			if (!bSuccess)
			{
				if (m_bTexSizeWasAutoExact)
				{
                    if (m_nTexSizeX > 256 || m_nTexSizeY > 256)
                    {
                        m_nTexSizeX /= 2;
                        m_nTexSizeY /= 2;
                        m_nTexSizeX = ((m_nTexSizeX+15)/16)*16;
                        m_nTexSizeY = ((m_nTexSizeY+15)/16)*16;
                    }
                    else
                    {
					    m_nTexSizeX = log2texsize;
					    m_nTexSizeY = log2texsize;
                        m_bTexSizeWasAutoExact = false;
					    m_bTexSizeWasAutoPow2 = true;
                    }
                }
				else if (m_bTexSizeWasAutoPow2)
				{
                    if (m_nTexSizeX > 256)
                    {
				        m_nTexSizeX /= 2;
				        m_nTexSizeY /= 2;
                    }
                    else
                        break;
				}
			}
	    }
	    while (!bSuccess);// && m_nTexSizeX >= 256 && (m_bTexSizeWasAutoExact || m_bTexSizeWasAutoPow2));

	    if (!bSuccess)
	    {
            wchar_t buf[2048];
			UINT err_id = IDS_COULD_NOT_CREATE_INTERNAL_CANVAS_TEXTURE_NOT_ENOUGH_VID_MEM;

            if (!(m_bTexSizeWasAutoExact || m_bTexSizeWasAutoPow2))
				err_id = IDS_COULD_NOT_CREATE_INTERNAL_CANVAS_TEXTURE_NOT_ENOUGH_VID_MEM_RECOMMENDATION;

			LoadResString(err_id,buf,sizeof(buf));
		    dumpmsg(buf);
		    MessageBoxW(GetPluginWindow(), buf, LoadResString(IDS_MILKDROP_ERROR,title,64), MB_OK|MB_SETFOREGROUND|MB_TOPMOST );
		    return false;
	    }
        else
        {
            swprintf(buf, LoadResString(IDS_SUCCESSFULLY_CREATED_VS0_VS1), m_nTexSizeX, m_nTexSizeY, GetWidth(), GetHeight());
            dumpmsg(buf);
        }


        // create blur textures w/same format.  A complete mip chain costs 33% more video mem then 1 full-sized VS.
        #if (NUM_BLUR_TEX>0)
            int w = m_nTexSizeX;
            int h = m_nTexSizeY;
            DWORD blurtex_flags = D3DUSAGE_RENDERTARGET;// | D3DUSAGE_AUTOGENMIPMAP;//FIXME! (make automipgen optional)
            for (int i=0; i<NUM_BLUR_TEX; i++)
            {
                // main VS = 1024
                // blur0 = 512
                // blur1 = 256  <-  user sees this as "blur1"
                // blur2 = 128
                // blur3 = 128  <-  user sees this as "blur2"
                // blur4 =  64
                // blur5 =  64  <-  user sees this as "blur3"
                if (!(i&1) || (i<2))
                {
                    w = max(16, w/2);
                    h = max(16, h/2);
                }
                int w2 = ((w+3)/16)*16;
                int h2 = ((h+3)/4)*4;
                bSuccess = (GetDevice()->CreateTexture(w2, h2, 1, blurtex_flags, fmt, D3DPOOL_DEFAULT, &m_lpBlur[i], NULL) == D3D_OK);
                m_nBlurTexW[i] = w2;
                m_nBlurTexH[i] = h2;
                if (!bSuccess)
                {
                    m_nBlurTexW[i] = 1;
                    m_nBlurTexH[i] = 1;
			        MessageBoxW(GetPluginWindow(), LoadResString(IDS_ERROR_CREATING_BLUR_TEXTURES,buf,sizeof(buf)),
							   LoadResString(IDS_MILKDROP_WARNING,title,sizeof(title)), MB_OK|MB_SETFOREGROUND|MB_TOPMOST);
                    break;
                }

                // add it to m_textures[].
                TexInfo x;
				swprintf(x.texname, L"blur%d%s", i/2+1, (i%2) ? L"" : L"doNOTuseME");
                x.texptr  = m_lpBlur[i];
                x.w = w2;
                x.h = h2;
                x.d = 1;
                x.bEvictable    = false;
                x.nAge          = m_nPresetsLoadedTotal;
                x.nSizeInBytes  = 0;
                m_textures.push_back(x);
            }
        #endif
    }


    m_fAspectX = (m_nTexSizeY > m_nTexSizeX) ? m_nTexSizeX/(float)m_nTexSizeY : 1.0f;
    m_fAspectY = (m_nTexSizeX > m_nTexSizeY) ? m_nTexSizeY/(float)m_nTexSizeX : 1.0f;
    m_fInvAspectX = 1.0f/m_fAspectX;
    m_fInvAspectY = 1.0f/m_fAspectY;


    // BUILD VERTEX LIST for final composite blit
	//   note the +0.5-texel offset!
	//   (otherwise, a 1-pixel-wide line of the image would wrap at the top and left edges).
	ZeroMemory(m_comp_verts, sizeof(MYVERTEX)*FCGSX*FCGSY);
    float fHalfTexelW =  0.5f / (float)GetWidth();   // 2.5: 2 pixels bad @ bottom right
    float fHalfTexelH =  0.5f / (float)GetHeight();
    float fDivX = 1.0f / (float)(FCGSX-2);
    float fDivY = 1.0f / (float)(FCGSY-2);
    for (int j=0; j<FCGSY; j++)
    {
        int j2 = j - j/(FCGSY/2);
        float v = j2*fDivY;
        v = SquishToCenter(v, 3.0f);
        float sy = -((v-fHalfTexelH)*2-1);//fOnePlusInvHeight*v*2-1;
        for (int i=0; i<FCGSX; i++)
        {
            int i2 = i - i/(FCGSX/2);
            float u = i2*fDivX;
            u = SquishToCenter(u, 3.0f);
            float sx = (u-fHalfTexelW)*2-1;//fOnePlusInvWidth*u*2-1;
            MYVERTEX* p = &m_comp_verts[i + j*FCGSX];
            p->x = sx;
            p->y = sy;
            p->z = 0;
            float rad, ang;
            UvToMathSpace( u, v, &rad, &ang );
                // fix-ups:
               if (i==FCGSX/2-1) {
                   if (j < FCGSY/2-1)
                       ang = 3.1415926535898f*1.5f;
                   else if (j == FCGSY/2-1)
                       ang = 3.1415926535898f*1.25f;
                   else if (j == FCGSY/2)
                       ang = 3.1415926535898f*0.75f;
                   else
                       ang = 3.1415926535898f*0.5f;
               }
               else if (i==FCGSX/2) {
                   if (j < FCGSY/2-1)
                       ang = 3.1415926535898f*1.5f;
                   else if (j == FCGSY/2-1)
                       ang = 3.1415926535898f*1.75f;
                   else if (j == FCGSY/2)
                       ang = 3.1415926535898f*0.25f;
                   else
                       ang = 3.1415926535898f*0.5f;
               }
               else if (j==FCGSY/2-1) {
                   if (i < FCGSX/2-1)
                       ang = 3.1415926535898f*1.0f;
                   else if (i == FCGSX/2-1)
                       ang = 3.1415926535898f*1.25f;
                   else if (i == FCGSX/2)
                       ang = 3.1415926535898f*1.75f;
                   else
                       ang = 3.1415926535898f*2.0f;
               }
               else if (j==FCGSY/2) {
                   if (i < FCGSX/2-1)
                       ang = 3.1415926535898f*1.0f;
                   else if (i == FCGSX/2-1)
                       ang = 3.1415926535898f*0.75f;
                   else if (i == FCGSX/2)
                       ang = 3.1415926535898f*0.25f;
                   else
                       ang = 3.1415926535898f*0.0f;
               }
            p->tu = u;
            p->tv = v;
            p->rad = rad;
            p->ang = ang;
            p->Diffuse = 0xFFFFFFFF;
        }
    }

    // build index list for final composite blit -
    // order should be friendly for interpolation of 'ang' value!
    int* cur_index = &m_comp_indices[0];
    for (int y=0; y<FCGSY-1; y++)
    {
        if (y==FCGSY/2-1)
            continue;
        for (int x=0; x<FCGSX-1; x++)
        {
            if (x==FCGSX/2-1)
                continue;
            bool left_half = (x < FCGSX/2);
            bool top_half  = (y < FCGSY/2);
            bool center_4 = ((x==FCGSX/2 || x==FCGSX/2-1) && (y==FCGSY/2 || y==FCGSY/2-1));

            if ( ((int)left_half + (int)top_half + (int)center_4) % 2 )
            {
                *(cur_index+0) = (y  )*FCGSX + (x  );
                *(cur_index+1) = (y  )*FCGSX + (x+1);
                *(cur_index+2) = (y+1)*FCGSX + (x+1);
                *(cur_index+3) = (y+1)*FCGSX + (x+1);
                *(cur_index+4) = (y+1)*FCGSX + (x  );
                *(cur_index+5) = (y  )*FCGSX + (x  );
            }
            else
            {
                *(cur_index+0) = (y+1)*FCGSX + (x  );
                *(cur_index+1) = (y  )*FCGSX + (x  );
                *(cur_index+2) = (y  )*FCGSX + (x+1);
                *(cur_index+3) = (y  )*FCGSX + (x+1);
                *(cur_index+4) = (y+1)*FCGSX + (x+1);
                *(cur_index+5) = (y+1)*FCGSX + (x  );
            }
            cur_index += 6;
        }
    }

    // -----------------


    // -----------------

	// reallocate the texture for font titles + custom msgs (m_lpDDSTitle)
	{
		m_nTitleTexSizeX = max(m_nTexSizeX, m_nTexSizeY);
		m_nTitleTexSizeY = m_nTitleTexSizeX/4;


        // [DEPRECATED as of transition to dx9:]
		// We could just create one title surface, but this is a problem because many
		// systems can only call DrawText on DDSCAPS_OFFSCREENPLAIN surfaces, and can NOT
		// draw text on a DDSCAPS_TEXTURE surface (it comes out garbled).
		// So, we create one of each; we draw the text to the DDSCAPS_OFFSCREENPLAIN surface
		// (m_lpDDSTitle[1]), then we blit that (once) to the DDSCAPS_TEXTURE surface
		// (m_lpDDSTitle[0]), which can then be drawn onto the screen on polys.

        HRESULT hr;

		do
		{
			hr = D3DXCreateTexture(GetDevice(), m_nTitleTexSizeX, m_nTitleTexSizeY, 1, D3DUSAGE_RENDERTARGET, GetBackBufFormat(), D3DPOOL_DEFAULT, &m_lpDDSTitle);
			if (hr != D3D_OK)
			{
				if (m_nTitleTexSizeY < m_nTitleTexSizeX)
				{
					m_nTitleTexSizeY *= 2;
				}
				else
				{
					m_nTitleTexSizeX /= 2;
					m_nTitleTexSizeY /= 2;
				}
			}
		}
		while (hr != D3D_OK && m_nTitleTexSizeX > 16);

		if (hr != D3D_OK)
		{
            m_lpDDSTitle = NULL;
		}
		else
		{
			m_supertext.bRedrawSuperText = true;
		}
	}

    // -----------------

    // create 'm_gdi_title_font_doublesize' and 'm_d3dx_title_font_doublesize'
    if (!CreateTitleFonts())   // SURI: moved into CreateTitleFonts (also used after a DPI change)
        return false;

    // -----------------

    m_texmgr.Init(GetDevice());
    RelaunchSnapshotSprites();  // SURI: sprites that were live before a resize / device reset

	m_verts      = new MYVERTEX[(m_nGridX+1)*(m_nGridY+1)];
	m_verts_temp = new MYVERTEX[(m_nGridX+2) * 4];
	m_vertinfo   = new td_vertinfo[(m_nGridX+1)*(m_nGridY+1)];
	m_indices_strip = new int[(m_nGridX+2)*(m_nGridY*2)];
	m_indices_list  = new int[m_nGridX*m_nGridY*6];
	if (!m_verts || !m_vertinfo)
	{
		swprintf(buf, L"couldn't allocate mesh - out of memory");
		dumpmsg(buf);
		MessageBoxW(GetPluginWindow(), buf, LoadResString(IDS_MILKDROP_ERROR,title,64), MB_OK|MB_SETFOREGROUND|MB_TOPMOST );
		return false;
	}

	int nVert = 0;
	float texel_offset_x = 0.5f / (float)m_nTexSizeX;
	float texel_offset_y = 0.5f / (float)m_nTexSizeY;
	for (y=0; y<=m_nGridY; y++)
	{
		for (int x=0; x<=m_nGridX; x++)
		{
			// precompute x,y,z
			m_verts[nVert].x = x/(float)m_nGridX*2.0f - 1.0f;
			m_verts[nVert].y = y/(float)m_nGridY*2.0f - 1.0f;
			m_verts[nVert].z = 0.0f;

			// precompute rad, ang, being conscious of aspect ratio
			m_vertinfo[nVert].rad = sqrtf(m_verts[nVert].x*m_verts[nVert].x*m_fAspectX*m_fAspectX + m_verts[nVert].y*m_verts[nVert].y*m_fAspectY*m_fAspectY);
			if (y==m_nGridY/2 && x==m_nGridX/2)
				m_vertinfo[nVert].ang = 0.0f;
			else
				m_vertinfo[nVert].ang = atan2f(m_verts[nVert].y*m_fAspectY, m_verts[nVert].x*m_fAspectX);
            m_vertinfo[nVert].a = 1;
            m_vertinfo[nVert].c = 0;

            m_verts[nVert].rad = m_vertinfo[nVert].rad;
            m_verts[nVert].ang = m_vertinfo[nVert].ang;
            m_verts[nVert].tu_orig =  m_verts[nVert].x*0.5f + 0.5f + texel_offset_x;
            m_verts[nVert].tv_orig = -m_verts[nVert].y*0.5f + 0.5f + texel_offset_y;

			nVert++;
		}
	}

    // generate triangle strips for the 4 quadrants.
    // each quadrant has m_nGridY/2 strips.
    // each strip has m_nGridX+2 *points* in it, or m_nGridX/2 polygons.
	int xref, yref;
	int nVert_strip = 0;
	for (int quadrant=0; quadrant<4; quadrant++)
	{
		for (int slice=0; slice < m_nGridY/2; slice++)
		{
			for (int i=0; i < m_nGridX + 2; i++)
			{
				// quadrants:	2 3
				//				0 1
				xref = i/2;
				yref = (i%2) + slice;

				if (quadrant & 1)
					xref = m_nGridX - xref;
				if (quadrant & 2)
					yref = m_nGridY - yref;

                int v = xref + (yref)*(m_nGridX+1);

				m_indices_strip[nVert_strip++] = v;
			}
		}
	}

    // also generate triangle lists for drawing the main warp mesh.
    int nVert_list = 0;
	for (int quadrant=0; quadrant<4; quadrant++)
	{
		for (int slice=0; slice < m_nGridY/2; slice++)
		{
			for (int i=0; i < m_nGridX/2; i++)
			{
				// quadrants:	2 3
				//				0 1
				xref = i;
				yref = slice;

				if (quadrant & 1)
					xref = m_nGridX-1 - xref;
				if (quadrant & 2)
					yref = m_nGridY-1 - yref;

                int v = xref + (yref)*(m_nGridX+1);

                m_indices_list[nVert_list++] = v;
                m_indices_list[nVert_list++] = v           +1;
                m_indices_list[nVert_list++] = v+m_nGridX+1  ;
                m_indices_list[nVert_list++] = v           +1;
                m_indices_list[nVert_list++] = v+m_nGridX+1  ;
                m_indices_list[nVert_list++] = v+m_nGridX+1+1;
			}
		}
	}

    // GENERATED TEXTURES FOR SHADERS
    //-------------------------------------
    if (m_nMaxPSVersion > 0)
    {
        // Generate noise textures
        if (!AddNoiseTex(L"noise_lq",      256, 1)) return false;
        if (!AddNoiseTex(L"noise_lq_lite",  32, 1)) return false;
        if (!AddNoiseTex(L"noise_mq",      256, 4)) return false;
        if (!AddNoiseTex(L"noise_hq",      256, 8)) return false;

        if (!AddNoiseVol(L"noisevol_lq", 32, 1)) return false;
        if (!AddNoiseVol(L"noisevol_hq", 32, 4)) return false;
    }

    if (!m_bInitialPresetSelected)
    {
		UpdatePresetList(true); //...just does its initial burst!
        LoadRandomPreset(0.0f);
        m_bInitialPresetSelected = true;
    }
    else
        LoadShaders(&m_shaders, m_pState, false);  // Also force-load the shaders - otherwise they'd only get compiled on a preset switch.

	return true;
}

float fCubicInterpolate(float y0, float y1, float y2, float y3, float t)
{
   float a0,a1,a2,a3,t2;

   t2 = t*t;
   a0 = y3 - y2 - y0 + y1;
   a1 = y0 - y1 - a0;
   a2 = y2 - y0;
   a3 = y1;

   return(a0*t*t2+a1*t2+a2*t+a3);
}

DWORD dwCubicInterpolate(DWORD y0, DWORD y1, DWORD y2, DWORD y3, float t)
{
    // performs cubic interpolation on a D3DCOLOR value.
    DWORD ret = 0;
    DWORD shift = 0;
    for (int i=0; i<4; i++)
    {
        float f = fCubicInterpolate(
            ((y0 >> shift) & 0xFF)/255.0f,
            ((y1 >> shift) & 0xFF)/255.0f,
            ((y2 >> shift) & 0xFF)/255.0f,
            ((y3 >> shift) & 0xFF)/255.0f,
            t
        );
        if (f<0)
            f = 0;
        if (f>1)
            f = 1;
        ret |= ((DWORD)(f*255)) << shift;
        shift += 8;
    }
    return ret;
}

bool CPlugin::AddNoiseTex(const wchar_t* szTexName, int size, int zoom_factor)
{
    // size = width & height of the texture;
    // zoom_factor = how zoomed-in the texture features should be.
    //           1 = random noise
    //           2 = smoothed (interp)
    //           4/8/16... = cubic interp.

    wchar_t buf[2048], title[64];

    // Synthesize noise texture(s)
    LPDIRECT3DTEXTURE9 pNoiseTex = NULL;
    // try twice - once with mips, once without.
    for (int i=0; i<2; i++)
    {
        if (D3D_OK != GetDevice()->CreateTexture(size, size, i, D3DUSAGE_DYNAMIC | (i ? 0 : D3DUSAGE_AUTOGENMIPMAP), D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &pNoiseTex, NULL))
        {
            if (i==1)
            {
				LoadResString(IDS_COULD_NOT_CREATE_NOISE_TEXTURE,buf,sizeof(buf));
		        dumpmsg(buf);
		        MessageBoxW(GetPluginWindow(), buf, LoadResString(IDS_MILKDROP_ERROR,title,64), MB_OK|MB_SETFOREGROUND|MB_TOPMOST );
		        return false;
            }
        }
        else
            break;
    }

    D3DLOCKED_RECT r;
    if (D3D_OK != pNoiseTex->LockRect(0, &r, NULL, D3DLOCK_DISCARD))
    {
		LoadResString(IDS_COULD_NOT_LOCK_NOISE_TEXTURE,buf,sizeof(buf));
		dumpmsg(buf);
		MessageBoxW(GetPluginWindow(), buf, LoadResString(IDS_MILKDROP_ERROR,title,64), MB_OK|MB_SETFOREGROUND|MB_TOPMOST );
		return false;
    }

    if (r.Pitch < size*4)
    {
		LoadResString(IDS_NOISE_TEXTURE_BYTE_LAYOUT_NOT_RECOGNISED,buf,sizeof(buf));
		dumpmsg(buf);
		MessageBoxW(GetPluginWindow(), buf, LoadResString(IDS_MILKDROP_ERROR,title,64), MB_OK|MB_SETFOREGROUND|MB_TOPMOST );
		return false;
    }

    // write to the bits...
    DWORD* dst = (DWORD*)r.pBits;
    int dwords_per_line = r.Pitch / sizeof(DWORD);
    int RANGE = (zoom_factor > 1) ? 216 : 256;
    for (int y=0; y<size; y++) {
        LARGE_INTEGER q;
        QueryPerformanceCounter(&q);
        srand(q.LowPart ^ q.HighPart ^ rand());
        for (int x=0; x<size; x++) {
            dst[x] = (((DWORD)(rand() % RANGE)+RANGE/2) << 24) |
                     (((DWORD)(rand() % RANGE)+RANGE/2) << 16) |
                     (((DWORD)(rand() % RANGE)+RANGE/2) <<  8) |
                     (((DWORD)(rand() % RANGE)+RANGE/2)      );
        }
        // swap some pixels randomly, to improve 'randomness'
        for (x=0; x<size; x++)
        {
            int x1 = (rand() ^ q.LowPart ) % size;
            int x2 = (rand() ^ q.HighPart) % size;
            DWORD temp = dst[x2];
            dst[x2] = dst[x1];
            dst[x1] = temp;
        }
        dst += dwords_per_line;
    }

    // smoothing
    if (zoom_factor > 1)
    {
        // first go ACROSS, blending cubically on X, but only on the main lines.
        DWORD* dst = (DWORD*)r.pBits;
        for (int y=0; y<size; y+=zoom_factor)
            for (int x=0; x<size; x++)
                if (x % zoom_factor)
                {
                    int base_x = (x/zoom_factor)*zoom_factor + size;
                    int base_y = y*dwords_per_line;
                    DWORD y0 = dst[ base_y + ((base_x - zoom_factor  ) % size) ];
                    DWORD y1 = dst[ base_y + ((base_x                ) % size) ];
                    DWORD y2 = dst[ base_y + ((base_x + zoom_factor  ) % size) ];
                    DWORD y3 = dst[ base_y + ((base_x + zoom_factor*2) % size) ];

                    float t = (x % zoom_factor)/(float)zoom_factor;

                    DWORD result = dwCubicInterpolate(y0, y1, y2, y3, t);

                    dst[ y*dwords_per_line + x ] = result;
                }

        // next go down, doing cubic interp along Y, on every line.
        for (int x=0; x<size; x++)
            for (int y=0; y<size; y++)
                if (y % zoom_factor)
                {
                    int base_y = (y/zoom_factor)*zoom_factor + size;
                    DWORD y0 = dst[ ((base_y - zoom_factor  ) % size)*dwords_per_line + x ];
                    DWORD y1 = dst[ ((base_y                ) % size)*dwords_per_line + x ];
                    DWORD y2 = dst[ ((base_y + zoom_factor  ) % size)*dwords_per_line + x ];
                    DWORD y3 = dst[ ((base_y + zoom_factor*2) % size)*dwords_per_line + x ];

                    float t = (y % zoom_factor)/(float)zoom_factor;

                    DWORD result = dwCubicInterpolate(y0, y1, y2, y3, t);

                    dst[ y*dwords_per_line + x ] = result;
                }

    }

    // unlock texture
    pNoiseTex->UnlockRect(0);

    // add it to m_textures[].
    TexInfo x;
    lstrcpyW(x.texname, szTexName);
    x.texptr  = pNoiseTex;
    x.w = size;
    x.h = size;
    x.d = 1;
    x.bEvictable    = false;
    x.nAge          = m_nPresetsLoadedTotal;
    x.nSizeInBytes  = 0;
    m_textures.push_back(x);

    return true;
}

bool CPlugin::AddNoiseVol(const wchar_t* szTexName, int size, int zoom_factor)
{
    // size = width & height & depth of the texture;
    // zoom_factor = how zoomed-in the texture features should be.
    //           1 = random noise
    //           2 = smoothed (interp)
    //           4/8/16... = cubic interp.

    wchar_t buf[2048], title[64];

    // Synthesize noise texture(s)
    LPDIRECT3DVOLUMETEXTURE9 pNoiseTex = NULL;
    // try twice - once with mips, once without.
    // NO, TRY JUST ONCE - DX9 doesn't do auto mipgen w/volume textures.  (Debug runtime complains.)
    for (int i=1; i<2; i++)
    {
        if (D3D_OK != GetDevice()->CreateVolumeTexture(size, size, size, i, D3DUSAGE_DYNAMIC | (i ? 0 : D3DUSAGE_AUTOGENMIPMAP), D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &pNoiseTex, NULL))
        {
            if (i==1)
            {
		        LoadResString(IDS_COULD_NOT_CREATE_3D_NOISE_TEXTURE,buf,sizeof(buf));
		        dumpmsg(buf);
		        MessageBoxW(GetPluginWindow(), buf, LoadResString(IDS_MILKDROP_ERROR,title,64), MB_OK|MB_SETFOREGROUND|MB_TOPMOST );
		        return false;
            }
        }
        else
            break;
    }
    D3DLOCKED_BOX r;
    if (D3D_OK != pNoiseTex->LockBox(0, &r, NULL, D3DLOCK_DISCARD))
    {
		LoadResString(IDS_COULD_NOT_LOCK_3D_NOISE_TEXTURE,buf,sizeof(buf));
		dumpmsg(buf);
		MessageBoxW(GetPluginWindow(), buf, LoadResString(IDS_MILKDROP_ERROR,title,64), MB_OK|MB_SETFOREGROUND|MB_TOPMOST );
		return false;
    }
    if (r.RowPitch < size*4 || r.SlicePitch < size*size*4)
    {
		LoadResString(IDS_3D_NOISE_TEXTURE_BYTE_LAYOUT_NOT_RECOGNISED,buf,sizeof(buf));
		dumpmsg(buf);
		MessageBoxW(GetPluginWindow(), buf, LoadResString(IDS_MILKDROP_ERROR,title,64), MB_OK|MB_SETFOREGROUND|MB_TOPMOST );
		return false;
    }
    // write to the bits...
    int dwords_per_slice = r.SlicePitch / sizeof(DWORD);
    int dwords_per_line = r.RowPitch / sizeof(DWORD);
    int RANGE = (zoom_factor > 1) ? 216 : 256;
    for (int z=0; z<size; z++) {
        DWORD* dst = (DWORD*)r.pBits + z*dwords_per_slice;
        for (int y=0; y<size; y++) {
            LARGE_INTEGER q;
            QueryPerformanceCounter(&q);
            srand(q.LowPart ^ q.HighPart ^ rand());
            for (int x=0; x<size; x++) {
                dst[x] = (((DWORD)(rand() % RANGE)+RANGE/2) << 24) |
                         (((DWORD)(rand() % RANGE)+RANGE/2) << 16) |
                         (((DWORD)(rand() % RANGE)+RANGE/2) <<  8) |
                         (((DWORD)(rand() % RANGE)+RANGE/2)      );
            }
            // swap some pixels randomly, to improve 'randomness'
            for (x=0; x<size; x++)
            {
                int x1 = (rand() ^ q.LowPart ) % size;
                int x2 = (rand() ^ q.HighPart) % size;
                DWORD temp = dst[x2];
                dst[x2] = dst[x1];
                dst[x1] = temp;
            }
            dst += dwords_per_line;
        }
    }

    // smoothing
    if (zoom_factor > 1)
    {
        // first go ACROSS, blending cubically on X, but only on the main lines.
        DWORD* dst = (DWORD*)r.pBits;
        for (int z=0; z<size; z+=zoom_factor)
            for (int y=0; y<size; y+=zoom_factor)
                for (int x=0; x<size; x++)
                    if (x % zoom_factor)
                    {
                        int base_x = (x/zoom_factor)*zoom_factor + size;
                        int base_y = z*dwords_per_slice + y*dwords_per_line;
                        DWORD y0 = dst[ base_y + ((base_x - zoom_factor  ) % size) ];
                        DWORD y1 = dst[ base_y + ((base_x                ) % size) ];
                        DWORD y2 = dst[ base_y + ((base_x + zoom_factor  ) % size) ];
                        DWORD y3 = dst[ base_y + ((base_x + zoom_factor*2) % size) ];

                        float t = (x % zoom_factor)/(float)zoom_factor;

                        DWORD result = dwCubicInterpolate(y0, y1, y2, y3, t);

                        dst[ z*dwords_per_slice + y*dwords_per_line + x ] = result;
                    }

        // next go down, doing cubic interp along Y, on the main slices.
        for (z=0; z<size; z+=zoom_factor)
            for (int x=0; x<size; x++)
                for (int y=0; y<size; y++)
                    if (y % zoom_factor)
                    {
                        int base_y = (y/zoom_factor)*zoom_factor + size;
                        int base_z = z*dwords_per_slice;
                        DWORD y0 = dst[ ((base_y - zoom_factor  ) % size)*dwords_per_line + base_z + x ];
                        DWORD y1 = dst[ ((base_y                ) % size)*dwords_per_line + base_z + x ];
                        DWORD y2 = dst[ ((base_y + zoom_factor  ) % size)*dwords_per_line + base_z + x ];
                        DWORD y3 = dst[ ((base_y + zoom_factor*2) % size)*dwords_per_line + base_z + x ];

                        float t = (y % zoom_factor)/(float)zoom_factor;

                        DWORD result = dwCubicInterpolate(y0, y1, y2, y3, t);

                        dst[ y*dwords_per_line + base_z + x ] = result;
                    }

        // next go through, doing cubic interp along Z, everywhere.
        for (int x=0; x<size; x++)
            for (int y=0; y<size; y++)
                for (int z=0; z<size; z++)
                    if (z % zoom_factor)
                    {
                        int base_y = y*dwords_per_line;
                        int base_z = (z/zoom_factor)*zoom_factor + size;
                        DWORD y0 = dst[ ((base_z - zoom_factor  ) % size)*dwords_per_slice + base_y + x ];
                        DWORD y1 = dst[ ((base_z                ) % size)*dwords_per_slice + base_y + x ];
                        DWORD y2 = dst[ ((base_z + zoom_factor  ) % size)*dwords_per_slice + base_y + x ];
                        DWORD y3 = dst[ ((base_z + zoom_factor*2) % size)*dwords_per_slice + base_y + x ];

                        float t = (z % zoom_factor)/(float)zoom_factor;

                        DWORD result = dwCubicInterpolate(y0, y1, y2, y3, t);

                        dst[ z*dwords_per_slice + base_y + x ] = result;
                    }

    }

    // unlock texture
    pNoiseTex->UnlockBox(0);

    // add it to m_textures[].
    TexInfo x;
    lstrcpyW(x.texname, szTexName);
    x.texptr  = pNoiseTex;
    x.w = size;
    x.h = size;
    x.d = size;
    x.bEvictable    = false;
    x.nAge          = m_nPresetsLoadedTotal;
    x.nSizeInBytes  = 0;
    m_textures.push_back(x);

    return true;
}

void VShaderInfo::Clear()
{
    SafeRelease(ptr);
    SafeRelease(CT);
    params.Clear();
}
void PShaderInfo::Clear()
{
    SafeRelease(ptr);
    SafeRelease(CT);
    params.Clear();
}

// global_CShaderParams_master_list: a master list of all CShaderParams classes in existence.
//   ** when we evict a texture, we need to NULL out any texptrs these guys have! **
CShaderParamsList global_CShaderParams_master_list;
CShaderParams::CShaderParams() {
    global_CShaderParams_master_list.push_back(this);
}

CShaderParams::~CShaderParams() {
    auto first = global_CShaderParams_master_list.begin();

    int N = global_CShaderParams_master_list.size();
    for (int i=0; i<N; i++)
        if (global_CShaderParams_master_list[i] == this)
            global_CShaderParams_master_list.erase(first + i);
    texsize_params.clear();
}

void CShaderParams::OnTextureEvict(LPDIRECT3DBASETEXTURE9 texptr)
{
    for (int i=0; i<sizeof(m_texture_bindings)/sizeof(m_texture_bindings[0]); i++)
        if (m_texture_bindings[i].texptr == texptr)
            m_texture_bindings[i].texptr = NULL;
}

void CShaderParams::Clear()
{
    // float4 handles:
    rand_frame  = NULL;
    rand_preset = NULL;

    ZeroMemory(rot_mat, sizeof(rot_mat));
    ZeroMemory(const_handles, sizeof(const_handles));
    ZeroMemory(q_const_handles, sizeof(q_const_handles));
    texsize_params.clear();

    // sampler stages for various PS texture bindings:
    for (int i=0; i<sizeof(m_texture_bindings)/sizeof(m_texture_bindings[0]); i++)
    {
        m_texture_bindings[i].texptr = NULL;
        m_texcode[i] = TEX_DISK;
    }
}

bool CPlugin::EvictSomeTexture()
{
    // note: this won't evict a texture whose age is zero,
    //       or whose reported size is zero!

    #if _DEBUG
    {
        int nEvictableFiles = 0;
        int nEvictableBytes = 0;
        int N = m_textures.size();
        for (int i=0; i<N; i++)
            if (m_textures[i].bEvictable && m_textures[i].texptr)
            {
                nEvictableFiles++;
                nEvictableBytes += m_textures[i].nSizeInBytes;
            }
        char buf[1024];
        sprintf(buf, "evicting at %d textures, %.1f MB\n", nEvictableFiles, nEvictableBytes*0.000001f);
        OutputDebugString(buf);
    }
    #endif

    int N = m_textures.size();

    // find age gap
    int newest = 99999999;
    int oldest = 0;
    bool bAtLeastOneFound = false;
    for (int i=0; i<N; i++)
        if (m_textures[i].bEvictable && m_textures[i].nSizeInBytes>0 && m_textures[i].nAge < m_nPresetsLoadedTotal-1) // note: -1 here keeps images around for the blend-from preset, too...
        {
            newest = min(newest, m_textures[i].nAge);
            oldest = max(oldest, m_textures[i].nAge);
            bAtLeastOneFound = true;
        }
    if (!bAtLeastOneFound)
        return false;

    // find the "biggest" texture, but dilate things so that the newest textures
    // are HALF as big as the oldest textures, and thus, less likely to get booted.
    int biggest_bytes = 0;
    int biggest_index = -1;
    for (i=0; i<N; i++)
        if (m_textures[i].bEvictable && m_textures[i].nSizeInBytes>0 && m_textures[i].nAge < m_nPresetsLoadedTotal-1) // note: -1 here keeps images around for the blend-from preset, too...
        {
            float size_mult = 1.0f + (m_textures[i].nAge - newest)/(float)(oldest-newest);
            int bytes = (int)(m_textures[i].nSizeInBytes * size_mult);
            if (bytes > biggest_bytes)
            {
                biggest_bytes = bytes;
                biggest_index = i;
            }
        }
    if (biggest_index == -1)
        return false;


    // evict that sucker
    assert(m_textures[biggest_index].texptr);

    // notify all CShaderParams classes that we're releasing a bindable texture!!
    N = global_CShaderParams_master_list.size();
    for (i=0; i<N; i++)
        global_CShaderParams_master_list[i]->OnTextureEvict( m_textures[biggest_index].texptr );

    // 2. erase the texture itself
    SafeRelease(m_textures[biggest_index].texptr);
    m_textures.erase(m_textures.begin() + biggest_index);

    return true;
}

std::wstring texture_exts[] = { L"jpg", L"dds", L"png", L"tga", L"bmp", L"dib", };
const wchar_t szExtsWithSlashes[] = L"jpg|png|dds|etc.";
typedef std::vector<std::wstring> StringVec;
bool PickRandomTexture(const wchar_t* prefix, wchar_t* szRetTextureFilename)  //should be MAX_PATH chars
{
    static StringVec texfiles;
    static DWORD     texfiles_timestamp = 0;   // update this a max of every ~2 seconds or so

    // if it's been more than a few seconds since the last textures dir scan, redo it.
    // (..just enough to make sure we don't do it more than once per preset load)
    //DWORD t = timeGetTime(); // in milliseconds
    //if (abs(t - texfiles_timestamp) > 2000)
    if (g_plugin.m_bNeedRescanTexturesDir)
    {
        g_plugin.m_bNeedRescanTexturesDir = false;//texfiles_timestamp = t;
        texfiles.clear();

        wchar_t szMask[MAX_PATH];
        swprintf(szMask, L"%stextures\\*.*", g_plugin.m_szMilkdrop2Path);

		WIN32_FIND_DATAW ffd = {0};

        HANDLE hFindFile = INVALID_HANDLE_VALUE;
	    if( (hFindFile = FindFirstFileW(szMask, &ffd )) == INVALID_HANDLE_VALUE )		// note: returns filename -without- path
            return false;

        // first, count valid texture files
        do
        {
            if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                continue;

            wchar_t* ext = wcsrchr(ffd.cFileName, L'.');
            if (!ext)
                continue;

            for (int i=0; i<sizeof(texture_exts)/sizeof(texture_exts[0]); i++)
                if (!wcsicmp(texture_exts[i].c_str(), ext+1))
                {
                    // valid texture found - add it to the list.  ("heart.jpg", for example)
                    texfiles.push_back( ffd.cFileName );
                    continue;
                }
        }
	    while (FindNextFileW(hFindFile, &ffd));
	    FindClose(hFindFile);
    }

    if (texfiles.size() == 0)
        return false;

    // then randomly pick one
    if (prefix==NULL || prefix[0]==0)
    {
        // pick randomly from entire list
        int i = rand() % texfiles.size();
        lstrcpyW(szRetTextureFilename, texfiles[i].c_str());
    }
    else
    {
        // only pick from files w/the right prefix
        StringVec temp_list;
        int N = texfiles.size();
        int len = lstrlenW(prefix);
        for (int i=0; i<N; i++)
            if (!_wcsnicmp(prefix, texfiles[i].c_str(), len))
                temp_list.push_back(texfiles[i]);
        N = temp_list.size();
        if (N==0)
            return false;
        // pick randomly from the subset
        i = rand() % temp_list.size();
        lstrcpyW(szRetTextureFilename, temp_list[i].c_str());
    }
    return true;
}

void CShaderParams::CacheParams(LPD3DXCONSTANTTABLE pCT, bool bHardErrors)
{
    Clear();

    if (!pCT)
        return;

    D3DXCONSTANTTABLE_DESC d;
    pCT->GetDesc(&d);

    D3DXCONSTANT_DESC cd;

    #define MAX_RAND_TEX 16
    std::wstring RandTexName[MAX_RAND_TEX];

    // pass 1: find all the samplers (and texture bindings).
    for (UINT i=0; i<d.Constants; i++)
    {
        D3DXHANDLE h = pCT->GetConstant(NULL, i);
        unsigned int count = 1;
        pCT->GetConstantDesc(h, &cd, &count);

        // cd.Name          = VS_Sampler
        // cd.RegisterSet   = D3DXRS_SAMPLER
        // cd.RegisterIndex = 3
        if (cd.RegisterSet == D3DXRS_SAMPLER && cd.RegisterIndex >= 0 && cd.RegisterIndex < sizeof(m_texture_bindings)/sizeof(m_texture_bindings[0]))
        {
            assert(m_texture_bindings[cd.RegisterIndex].texptr == NULL);

            // remove "sampler_" prefix to create root file name.  could still have "FW_" prefix or something like that.
            wchar_t szRootName[MAX_PATH];
            if (!strncmp(cd.Name, "sampler_", 8))
                lstrcpyW(szRootName, AutoWide(&cd.Name[8]));
            else
                lstrcpyW(szRootName, AutoWide(cd.Name));

            // also peel off "XY_" prefix, if it's there, to specify filtering & wrap mode.
            bool bBilinear = true;
            bool bWrap     = true;
            bool bWrapFilterSpecified = false;
            if (lstrlenW(szRootName) > 3 && szRootName[2]==L'_')
            {
                wchar_t temp[3];
                temp[0] = szRootName[0];
                temp[1] = szRootName[1];
                temp[2] = 0;
                // convert to uppercase
                if (temp[0] >= L'a' && temp[0] <= L'z')
                    temp[0] -= L'a' - L'A';
                if (temp[1] >= L'a' && temp[1] <= L'z')
                    temp[1] -= L'a' - L'A';

                if      (!wcscmp(temp, L"FW")) { bWrapFilterSpecified = true; bBilinear = true;  bWrap = true; }
                else if (!wcscmp(temp, L"FC")) { bWrapFilterSpecified = true; bBilinear = true;  bWrap = false; }
                else if (!wcscmp(temp, L"PW")) { bWrapFilterSpecified = true; bBilinear = false; bWrap = true; }
                else if (!wcscmp(temp, L"PC")) { bWrapFilterSpecified = true; bBilinear = false; bWrap = false; }
                // also allow reverses:
                else if (!wcscmp(temp, L"WF")) { bWrapFilterSpecified = true; bBilinear = true;  bWrap = true; }
                else if (!wcscmp(temp, L"CF")) { bWrapFilterSpecified = true; bBilinear = true;  bWrap = false; }
                else if (!wcscmp(temp, L"WP")) { bWrapFilterSpecified = true; bBilinear = false; bWrap = true; }
                else if (!wcscmp(temp, L"CP")) { bWrapFilterSpecified = true; bBilinear = false; bWrap = false; }

                // peel off the prefix
                int i = 0;
                while (szRootName[i+3])
                {
                    szRootName[i] = szRootName[i+3];
                    i++;
                }
                szRootName[i] = 0;
            }
            m_texture_bindings[ cd.RegisterIndex ].bWrap     = bWrap;
            m_texture_bindings[ cd.RegisterIndex ].bBilinear = bBilinear;

            // if <szFileName> is "main", map it to the VS...
            if (!wcscmp(L"main", szRootName))
            {
                m_texture_bindings[ cd.RegisterIndex ].texptr    = NULL;
                m_texcode[ cd.RegisterIndex ] = TEX_VS;
            }
            #if (NUM_BLUR_TEX >= 2)
            else if (!wcscmp(L"blur1", szRootName))
            {
                m_texture_bindings[ cd.RegisterIndex ].texptr = g_plugin.m_lpBlur[1];
                m_texcode         [ cd.RegisterIndex ]        = TEX_BLUR1;
                if (!bWrapFilterSpecified) { // when sampling blur textures, default is CLAMP
                    m_texture_bindings[ cd.RegisterIndex ].bWrap = false;
                    m_texture_bindings[ cd.RegisterIndex ].bBilinear = true;
                }
            }
            #endif
            #if (NUM_BLUR_TEX >= 4)
                else if (!wcscmp(L"blur2", szRootName))
                {
                    m_texture_bindings[ cd.RegisterIndex ].texptr = g_plugin.m_lpBlur[3];
                    m_texcode         [ cd.RegisterIndex ]        = TEX_BLUR2;
                    if (!bWrapFilterSpecified) { // when sampling blur textures, default is CLAMP
                        m_texture_bindings[ cd.RegisterIndex ].bWrap = false;
                        m_texture_bindings[ cd.RegisterIndex ].bBilinear = true;
                    }
                }
            #endif
            #if (NUM_BLUR_TEX >= 6)
                else if (!wcscmp(L"blur3", szRootName))
                {
                    m_texture_bindings[ cd.RegisterIndex ].texptr    = g_plugin.m_lpBlur[5];
                    m_texcode         [ cd.RegisterIndex ]        = TEX_BLUR3;
                    if (!bWrapFilterSpecified) { // when sampling blur textures, default is CLAMP
                        m_texture_bindings[ cd.RegisterIndex ].bWrap = false;
                        m_texture_bindings[ cd.RegisterIndex ].bBilinear = true;
                    }
                }
            #endif
            #if (NUM_BLUR_TEX >= 8)
                else if (!wcscmp("blur4", szRootName))
                {
                    m_texture_bindings[ cd.RegisterIndex ].texptr    = g_plugin.m_lpBlur[7];
                    m_texcode         [ cd.RegisterIndex ]        = TEX_BLUR4;
                    if (!bWrapFilterSpecified) { // when sampling blur textures, default is CLAMP
                        m_texture_bindings[ cd.RegisterIndex ].bWrap = false;
                        m_texture_bindings[ cd.RegisterIndex ].bBilinear = true;
                    }
                }
            #endif
            #if (NUM_BLUR_TEX >= 10)
                else if (!wcscmp("blur5", szRootName))
                {
                    m_texture_bindings[ cd.RegisterIndex ].texptr    = g_plugin.m_lpBlur[9];
                    m_texcode         [ cd.RegisterIndex ]        = TEX_BLUR5;
                    if (!bWrapFilterSpecified) { // when sampling blur textures, default is CLAMP
                        m_texture_bindings[ cd.RegisterIndex ].bWrap = false;
                        m_texture_bindings[ cd.RegisterIndex ].bBilinear = true;
                    }
                }
            #endif
            #if (NUM_BLUR_TEX >= 12)
                else if (!wcscmp("blur6", szRootName))
                {
                    m_texture_bindings[ cd.RegisterIndex ].texptr    = g_plugin.m_lpBlur[11];
                    m_texcode         [ cd.RegisterIndex ]        = TEX_BLUR6;
                    if (!bWrapFilterSpecified) { // when sampling blur textures, default is CLAMP
                        m_texture_bindings[ cd.RegisterIndex ].bWrap = false;
                        m_texture_bindings[ cd.RegisterIndex ].bBilinear = true;
                    }
                }
            #endif
            else
            {
                m_texcode[ cd.RegisterIndex ] = TEX_DISK;

                // check for request for random texture.
                if (!wcsncmp(L"rand", szRootName, 4) &&
                    IsNumericChar(szRootName[4]) &&
                    IsNumericChar(szRootName[5]) &&
                    (szRootName[6]==0 || szRootName[6]=='_') )
                {
                    int rand_slot = -1;

                    // peel off filename prefix ("rand13_smalltiled", for example)
                    wchar_t prefix[MAX_PATH];
                    if (szRootName[6]==L'_')
                        lstrcpyW(prefix, &szRootName[7]);
                    else
                        prefix[0] = 0;
                    szRootName[6] = 0;

                    swscanf(&szRootName[4], L"%d", &rand_slot);
                    if (rand_slot >= 0 && rand_slot <= 15)      // otherwise, not a special filename - ignore it
                    {
                        if (!PickRandomTexture(prefix, szRootName))
                        {
                            if (prefix[0])
                                swprintf(szRootName, L"[rand%02d] %s*", rand_slot, prefix);
                            else
                                swprintf(szRootName, L"[rand%02d] *", rand_slot);
                        }
                        else
                        {
                            //chop off extension
                            wchar_t *p = wcsrchr(szRootName, L'.');
                            if (p)
                                *p = 0;
                        }

                        RandTexName[rand_slot] = szRootName; // we'll need to remember this for texsize_ params!
                    }
                }

                // see if <szRootName>.tga or .jpg has already been loaded.
                //   (if so, grab a pointer to it)
                //   (if NOT, create & load it).
                int N = g_plugin.m_textures.size();
                for (int n=0; n<N; n++) {
                    if (!wcscmp(g_plugin.m_textures[n].texname, szRootName))
                    {
                        // found a match - texture was already loaded
                        m_texture_bindings[ cd.RegisterIndex ].texptr = g_plugin.m_textures[n].texptr;
                        // also bump its age down to zero! (for cache mgmt)
                        g_plugin.m_textures[n].nAge = g_plugin.m_nPresetsLoadedTotal;
                        break;
                    }
                }
                // if still not found, load it up / make a new texture
                if (!m_texture_bindings[ cd.RegisterIndex ].texptr)
                {
                    TexInfo x;
                    wcsncpy(x.texname, szRootName, 254);
                    x.texptr  = NULL;

                    // check if we need to evict anything from the cache,
                    // due to our own cache constraints...
                    while (1)
                    {
                        int nTexturesCached = 0;
                        int nBytesCached = 0;
                        int N = g_plugin.m_textures.size();
                        for (int i=0; i<N; i++)
                            if (g_plugin.m_textures[i].bEvictable && g_plugin.m_textures[i].texptr)
                            {
                                nBytesCached += g_plugin.m_textures[i].nSizeInBytes;
                                nTexturesCached++;
                            }
                        if ( nTexturesCached < g_plugin.m_nMaxImages &&
                             nBytesCached < g_plugin.m_nMaxBytes )
                            break;
                        // otherwise, evict now - and loop until we are within the constraints
                        if (!g_plugin.EvictSomeTexture())
                            break; // or if there was nothing to evict, just give up
                    }

                    //load the texture
                    wchar_t szFilename[MAX_PATH];
                    for (int z=0; z<sizeof(texture_exts)/sizeof(texture_exts[0]); z++)
                    {
                        swprintf(szFilename, L"%stextures\\%s.%s", g_plugin.m_szMilkdrop2Path, szRootName, texture_exts[z].c_str());
                        if (GetFileAttributesW(szFilename) == 0xFFFFFFFF)
                        {
                            // try again, but in presets dir
                            swprintf(szFilename, L"%s%s.%s", g_plugin.m_szPresetDir, szRootName, texture_exts[z].c_str());
                            if (GetFileAttributesW(szFilename) == 0xFFFFFFFF)
                              continue;
                        }
                        D3DXIMAGE_INFO desc;

                        // keep trying to load it - if it fails due to memory, evict something and try again.
                        while (1)
                        {
                            HRESULT hr = D3DXCreateTextureFromFileExW(g_plugin.GetDevice(),
                                                                   szFilename,
                                                                   D3DX_DEFAULT_NONPOW2, // w
                                                                   D3DX_DEFAULT_NONPOW2, // h
                                                                   D3DX_DEFAULT,    // # mip levels to gen - all
                                                                   0,  // usage flags
                                                                   D3DFMT_UNKNOWN,
                                                                   D3DPOOL_DEFAULT,
                                                                   D3DX_DEFAULT,     //filter
                                                                   D3DX_DEFAULT,     //mipfilter
                                                                   0,                // color key
                                                                   &desc,
                                                                   NULL,             //palette
                                                                   (IDirect3DTexture9**)&x.texptr
                                                                     );
                            if (hr==D3DERR_OUTOFVIDEOMEMORY || hr==E_OUTOFMEMORY)
                            {
                                // out of memory - try evicting something old and/or big
                                if (g_plugin.EvictSomeTexture())
                                    continue;
                            }

                            if (hr==D3D_OK)
                            {
                                x.w = desc.Width;
                                x.h = desc.Height;
                                x.d = desc.Depth;
                                x.bEvictable    = true;
                                x.nAge          = g_plugin.m_nPresetsLoadedTotal;
                                int nPixels = desc.Width*desc.Height*max(1,desc.Depth);
                                int BitsPerPixel = GetDX9TexFormatBitsPerPixel(desc.Format);
                                x.nSizeInBytes  =  nPixels*BitsPerPixel/8 + 16384;  //plus some overhead
                            }
                            break;
                        }
                    }

                    if (!x.texptr)
                    {
		                wchar_t buf[2048], title[64];
                        swprintf(buf, LoadResString(IDS_COULD_NOT_LOAD_TEXTURE_X), szRootName, szExtsWithSlashes);
		                g_plugin.dumpmsg(buf);
                        if (bHardErrors)
		                    MessageBoxW(g_plugin.GetPluginWindow(), buf, LoadResString(IDS_MILKDROP_ERROR,title,64), MB_OK|MB_SETFOREGROUND|MB_TOPMOST );
                        else {
                            g_plugin.AddError(buf, 6.0f, ERR_PRESET, true);
                        }
		                return;
                    }

                    g_plugin.m_textures.push_back(x);
                    m_texture_bindings[ cd.RegisterIndex ].texptr    = x.texptr;
                }
            }
        }
    }

    // pass 2: bind all the float4's.  "texsize_XYZ" params will be filled out via knowledge of loaded texture sizes.
    for (i=0; i<d.Constants; i++)
    {
        D3DXHANDLE h = pCT->GetConstant(NULL, i);
        unsigned int count = 1;
        pCT->GetConstantDesc(h, &cd, &count);

        if (cd.RegisterSet == D3DXRS_FLOAT4)
        {
            if (cd.Class == D3DXPC_MATRIX_COLUMNS)
            {
                if      (!strcmp(cd.Name, "rot_s1" )) rot_mat[0]  = h;
                else if (!strcmp(cd.Name, "rot_s2" )) rot_mat[1]  = h;
                else if (!strcmp(cd.Name, "rot_s3" )) rot_mat[2]  = h;
                else if (!strcmp(cd.Name, "rot_s4" )) rot_mat[3]  = h;
                else if (!strcmp(cd.Name, "rot_d1" )) rot_mat[4]  = h;
                else if (!strcmp(cd.Name, "rot_d2" )) rot_mat[5]  = h;
                else if (!strcmp(cd.Name, "rot_d3" )) rot_mat[6]  = h;
                else if (!strcmp(cd.Name, "rot_d4" )) rot_mat[7]  = h;
                else if (!strcmp(cd.Name, "rot_f1" )) rot_mat[8]  = h;
                else if (!strcmp(cd.Name, "rot_f2" )) rot_mat[9]  = h;
                else if (!strcmp(cd.Name, "rot_f3" )) rot_mat[10] = h;
                else if (!strcmp(cd.Name, "rot_f4" )) rot_mat[11] = h;
                else if (!strcmp(cd.Name, "rot_vf1")) rot_mat[12] = h;
                else if (!strcmp(cd.Name, "rot_vf2")) rot_mat[13] = h;
                else if (!strcmp(cd.Name, "rot_vf3")) rot_mat[14] = h;
                else if (!strcmp(cd.Name, "rot_vf4")) rot_mat[15] = h;
                else if (!strcmp(cd.Name, "rot_uf1")) rot_mat[16] = h;
                else if (!strcmp(cd.Name, "rot_uf2")) rot_mat[17] = h;
                else if (!strcmp(cd.Name, "rot_uf3")) rot_mat[18] = h;
                else if (!strcmp(cd.Name, "rot_uf4")) rot_mat[19] = h;
                else if (!strcmp(cd.Name, "rot_rand1")) rot_mat[20] = h;
                else if (!strcmp(cd.Name, "rot_rand2")) rot_mat[21] = h;
                else if (!strcmp(cd.Name, "rot_rand3")) rot_mat[22] = h;
                else if (!strcmp(cd.Name, "rot_rand4")) rot_mat[23] = h;
            }
            else if (cd.Class == D3DXPC_VECTOR)
            {
                if      (!strcmp(cd.Name, "rand_frame"))  rand_frame  = h;
                else if (!strcmp(cd.Name, "rand_preset")) rand_preset = h;
                else if (!strncmp(cd.Name, "texsize_", 8))
                {
                    // remove "texsize_" prefix to find root file name.
                    wchar_t szRootName[MAX_PATH];
                    if (!strncmp(cd.Name, "texsize_", 8))
                        lstrcpyW(szRootName, AutoWide(&cd.Name[8]));
                    else
                        lstrcpyW(szRootName, AutoWide(cd.Name));

                    // check for request for random texture.
                    // it should be a previously-seen random index - just fetch/reuse the name.
                    if (!wcsncmp(L"rand", szRootName, 4) &&
                        IsNumericChar(szRootName[4]) &&
                        IsNumericChar(szRootName[5]) &&
                        (szRootName[6]==0 || szRootName[6]==L'_') )
                    {
                        int rand_slot = -1;

                        // ditch filename prefix ("rand13_smalltiled", for example)
                        // and just go by the slot
                        if (szRootName[6]==L'_')
                            szRootName[6] = 0;

                        swscanf(&szRootName[4], L"%d", &rand_slot);
                        if (rand_slot >= 0 && rand_slot <= 15)      // otherwise, not a special filename - ignore it
                            if (RandTexName[rand_slot].size() > 0)
                                lstrcpyW(szRootName, RandTexName[rand_slot].c_str());
                    }

                    // see if <szRootName>.tga or .jpg has already been loaded.
                    bool bTexFound = false;
                    int N = g_plugin.m_textures.size();
                    for (int n=0; n<N; n++) {
                        if (!wcscmp(g_plugin.m_textures[n].texname, szRootName))
                        {
                            // found a match - texture was loaded
                            TexSizeParamInfo y;
                            y.texname       = szRootName; //for debugging
                            y.texsize_param = h;
                            y.w             = g_plugin.m_textures[n].w;
                            y.h             = g_plugin.m_textures[n].h;
                            texsize_params.push_back(y);

                            bTexFound = true;
                            break;
                        }
                    }

                    if (!bTexFound)
                    {
                        wchar_t buf[1024];
			            swprintf(buf, LoadResString(IDS_UNABLE_TO_RESOLVE_TEXSIZE_FOR_A_TEXTURE_NOT_IN_USE), cd.Name);
                        g_plugin.AddError(buf, 6.0f, ERR_PRESET, true);
                    }
                }
                else if (cd.Name[0] == '_' && cd.Name[1] == 'c')
                {
                    int z;
                    if (sscanf(&cd.Name[2], "%d", &z)==1)
                        if (z >= 0 && z < sizeof(const_handles)/sizeof(const_handles[0]))
                            const_handles[z] = h;
                }
                else if (cd.Name[0] == '_' && cd.Name[1] == 'q')
                {
                    int z = cd.Name[2] - 'a';
                    if (z >= 0 && z < sizeof(q_const_handles)/sizeof(q_const_handles[0]))
                        q_const_handles[z] = h;
                }
            }
        }
    }
}

//----------------------------------------------------------------------

bool CPlugin::RecompileVShader(const char* szShadersText, VShaderInfo *si, int shaderType, bool bHardErrors)
{
    SafeRelease(si->ptr);
    ZeroMemory(si, sizeof(VShaderInfo));

    // LOAD SHADER
    if (!LoadShaderFromMemory( szShadersText, "VS", "vs_1_1", &si->CT, (void**)&si->ptr, shaderType, bHardErrors))
        return false;

    // Track down texture & float4 param bindings for this shader.
    // Also loads any textures that need loaded.
    si->params.CacheParams(si->CT, bHardErrors);

    return true;
}

bool CPlugin::RecompilePShader(const char* szShadersText, PShaderInfo *si, int shaderType, bool bHardErrors, int PSVersion)
{
    assert(m_nMaxPSVersion > 0);

    SafeRelease(si->ptr);
    ZeroMemory(si, sizeof(PShaderInfo));

    // LOAD SHADER
    // note: ps_1_4 required for dependent texture lookups.
    //       ps_2_0 required for tex2Dbias.
		char ver[16];
		lstrcpy(ver, "ps_0_0");
		switch(PSVersion) {
		case MD2_PS_NONE:
			// Even though the PRESET doesn't use shaders, if MilkDrop is running where it CAN do shaders,
			//   we run all the old presets through (shader) emulation.
			// This way, during a MilkDrop session, we are always calling either WarpedBlit() or WarpedBlit_NoPixelShaders(),
			//   and blending always works.
			lstrcpy(ver, "ps_2_0");
			break;
		case MD2_PS_2_0: lstrcpy(ver, "ps_2_0"); break;
		case MD2_PS_2_X: lstrcpy(ver, "ps_2_a"); break; // we'll try ps_2_a first, LoadShaderFromMemory will try ps_2_b if compilation fails
		case MD2_PS_3_0: lstrcpy(ver, "ps_3_0"); break;
		case MD2_PS_4_0: lstrcpy(ver, "ps_4_0"); break;
		default: assert(0); break;
		}

    if (!LoadShaderFromMemory( szShadersText, "PS", ver, &si->CT, (void**)&si->ptr, shaderType, bHardErrors))
        return false;

    // Track down texture & float4 param bindings for this shader.
    // Also loads any textures that need loaded.
    si->params.CacheParams(si->CT, bHardErrors);

    return true;
}

bool CPlugin::LoadShaders(PShaderSet* sh, CState* pState, bool bTick)
{
    if (m_nMaxPSVersion <= 0)
        return true;

    // load one of the pixel shaders
    if (!sh->warp.ptr && pState->m_nWarpPSVersion > 0)
    {
        bool bOK = RecompilePShader(pState->m_szWarpShadersText, &sh->warp, SHADER_WARP, false, pState->m_nWarpPSVersion);
        if (!bOK)
        {
            // switch to fallback shader
            m_fallbackShaders_ps.warp.ptr->AddRef();
            m_fallbackShaders_ps.warp.CT->AddRef();
            memcpy(&sh->warp, &m_fallbackShaders_ps.warp, sizeof(PShaderInfo));
            // cancel any slow-preset-load
            //m_nLoadingPreset = 1000;
        }

        if (bTick)
            return true;
    }

    if (!sh->comp.ptr && pState->m_nCompPSVersion > 0)
    {
        bool bOK = RecompilePShader(pState->m_szCompShadersText, &sh->comp, SHADER_COMP, false, pState->m_nCompPSVersion);
        if (!bOK)
        {
            // switch to fallback shader
            m_fallbackShaders_ps.comp.ptr->AddRef();
            m_fallbackShaders_ps.comp.CT->AddRef();
            memcpy(&sh->comp, &m_fallbackShaders_ps.comp, sizeof(PShaderInfo));
            // cancel any slow-preset-load
            //m_nLoadingPreset = 1000;
        }
    }

    return true;
}

//----------------------------------------------------------------------


bool CPlugin::LoadShaderFromMemory( const char* szOrigShaderText, char* szFn, char* szProfile,
                                    LPD3DXCONSTANTTABLE* ppConstTable, void** ppShader, int shaderType, bool bHardErrors )
{
    const char szWarpDefines[] = "#define rad _rad_ang.x\n"
                                 "#define ang _rad_ang.y\n"
                                 "#define uv _uv.xy\n"
                                 "#define uv_orig _uv.zw\n";
    const char szCompDefines[] = "#define rad _rad_ang.x\n"
                                 "#define ang _rad_ang.y\n"
                                 "#define uv _uv.xy\n"
                                 "#define uv_orig _uv.xy\n" //[sic]
                                 "#define hue_shader _vDiffuse.xyz\n";
    const char szWarpParams[] = "float4 _vDiffuse : COLOR, float4 _uv : TEXCOORD0, float2 _rad_ang : TEXCOORD1, out float4 _return_value : COLOR0";
    const char szCompParams[] = "float4 _vDiffuse : COLOR, float2 _uv : TEXCOORD0, float2 _rad_ang : TEXCOORD1, out float4 _return_value : COLOR0";
    const char szFirstLine[]  = "    float3 ret = 0;";
    const char szLastLine[]   = "    _return_value = float4(ret.xyz, _vDiffuse.w);";

    char szWhichShader[64];
    switch(shaderType)
    {
    case SHADER_WARP:  lstrcpy(szWhichShader, "warp"); break;
    case SHADER_COMP:  lstrcpy(szWhichShader, "composite"); break;
    case SHADER_BLUR:  lstrcpy(szWhichShader, "blur"); break;
    case SHADER_OTHER: lstrcpy(szWhichShader, "(other)"); break;
    default:           lstrcpy(szWhichShader, "(unknown)"); break;
    }

    LPD3DXBUFFER pShaderByteCode;
    wchar_t title[64];

    *ppShader = NULL;
    *ppConstTable = NULL;

    char szShaderText[128000];
    char temp[128000];
    int writePos = 0;

    // paste the universal #include
    lstrcpy(&szShaderText[writePos], m_szShaderIncludeText);  // first, paste in the contents of 'inputs.fx' before the actual shader text.  Has 13's and 10's.
    writePos += m_nShaderIncludeTextLen;

    // paste in any custom #defines for this shader type
    if (shaderType == SHADER_WARP && szProfile[0]=='p')
    {
        lstrcpy(&szShaderText[writePos], szWarpDefines);
        writePos += lstrlen(szWarpDefines);
    }
    else if (shaderType == SHADER_COMP && szProfile[0]=='p')
    {
        lstrcpy(&szShaderText[writePos], szCompDefines);
        writePos += lstrlen(szCompDefines);
    }

    // paste in the shader itself - converting LCC's to 13+10's.
    // avoid lstrcpy b/c it might not handle the linefeed stuff...?
    int shaderStartPos = writePos;
    {
        const char *s = szOrigShaderText;
        char *d = &szShaderText[writePos];
        while (*s)
        {
            if (*s == LINEFEED_CONTROL_CHAR)
            {
                *d++ = 13; writePos++;
                *d++ = 10; writePos++;
            }
            else
            {
                *d++ = *s; writePos++;
            }
            s++;
        }
        *d = 0; writePos++;
    }

    // strip out all comments - but cheat a little - start at the shader test.
    // (the include file was already stripped of comments)
    StripComments(&szShaderText[shaderStartPos]);


    //note: only do this stuff if type is WARP or COMP shader... not for blur, etc!
    //FIXME - hints on the inputs / output / samplers etc.
    //   can go in the menu header, NOT the preset!  =)
    //then update presets
    //  -> be sure to update the presets on disk AND THE DEFAULT SHADERS (for loading MD1 presets)
    //FIXME - then update auth. guide w/new examples,
    //   and a list of the invisible inputs (and one output) to each shader!
    //   warp: float2 uv, float2 uv_orig, rad, ang
    //   comp: float2 uv, rad, ang, float3 hue_shader
    // test all this string code in Debug mode - make sure nothing bad is happening

    /*
    1. paste warp or comp #defines
    2. search for "void" + whitespace + szFn + [whitespace] + '('
    3. insert params
    4. search for [whitespace] + ')'.
    5. search for final '}' (strrchr)
    6. back up one char, insert the Last Line, and add '}' and that's it.
    */
    if ((shaderType == SHADER_WARP || shaderType == SHADER_COMP) && szProfile[0]=='p')
    {
        char* p = &szShaderText[shaderStartPos];

        // seek to 'shader_body' and replace it with spaces
        while (*p && strncmp(p, "shader_body", 11))
            p++;
        if (p)
        {
            for (int i=0; i<11; i++)
                *p++ = ' ';
        }

        if (p)
        {
            // insert "void PS(...params...)\n"
            lstrcpy(temp, p);
            const char *params = (shaderType==SHADER_WARP) ? szWarpParams : szCompParams;
            sprintf(p, "void %s( %s )\n", szFn, params);
            p += lstrlen(p);
            lstrcpy(p, temp);

            // find the starting curly brace
            p = strchr(p, '{');
            if (p)
            {
                // skip over it
                p++;
                // then insert "float3 ret = 0;"
                lstrcpy(temp, p);
                sprintf(p, "%s\n", szFirstLine);
                p += lstrlen(p);
                lstrcpy(p, temp);

                // find the ending curly brace
                p = strrchr(p, '}');
                // add the last line - "    _return_value = float4(ret.xyz, _vDiffuse.w);"
                if (p)
                    sprintf(p, " %s\n}\n", szLastLine);
            }
        }

        if (!p)
        {
			wchar_t temp[512];
            swprintf(temp, LoadResString(IDS_ERROR_PARSING_X_X_SHADER), szProfile, szWhichShader);
		    dumpmsg(temp);
            AddError(temp, 8.0f, ERR_PRESET, true);
		    return false;
        }
    }

    // now really try to compile it.

	bool failed=false;
    int len = lstrlen(szShaderText);

    HRESULT hresult = D3DXCompileShader(
        szShaderText,
        len,
        NULL,//CONST D3DXMACRO* pDefines,
        NULL,//LPD3DXINCLUDE pInclude,
        szFn,
        szProfile,
        m_dwShaderFlags,
        &pShaderByteCode,
        &m_pShaderCompileErrors,
        ppConstTable);

    if (D3D_OK != hresult)
		{
			failed=true;
		}
		// before we totally fail, let's try using ps_2_b instead of ps_2_a
		if (failed && !strcmp(szProfile, "ps_2_a"))
		{
			SafeRelease(m_pShaderCompileErrors);
			if (D3D_OK == D3DXCompileShader(szShaderText, len, NULL, NULL, szFn,
				"ps_2_b", m_dwShaderFlags, &pShaderByteCode, &m_pShaderCompileErrors, ppConstTable))
			{
				failed=false;
			}
		}

		if (failed)
		{
			wchar_t temp[1024];
			swprintf(temp, LoadResString(IDS_ERROR_COMPILING_X_X_SHADER), szProfile, szWhichShader);
			if (m_pShaderCompileErrors && m_pShaderCompileErrors->GetBufferSize() < sizeof(temp) - 256)
			{
				lstrcatW(temp, L"\n\n");
				lstrcatW(temp, AutoWide((char*)m_pShaderCompileErrors->GetBufferPointer()));
			}
			SafeRelease(m_pShaderCompileErrors);
			dumpmsg(temp);
			if (bHardErrors)
				MessageBoxW(GetPluginWindow(), temp, LoadResString(IDS_MILKDROP_ERROR,title,64), MB_OK|MB_SETFOREGROUND|MB_TOPMOST );
			else {
				AddError(temp, 8.0f, ERR_PRESET, true);
			}
			return false;
		}

    HRESULT hr = 1;
    if (szProfile[0] == 'v')
    {
        hr = GetDevice()->CreateVertexShader((const unsigned long *)(pShaderByteCode->GetBufferPointer()), (IDirect3DVertexShader9**)ppShader);
    }
    else if (szProfile[0] == 'p')
    {
        hr = GetDevice()->CreatePixelShader((const unsigned long *)(pShaderByteCode->GetBufferPointer()), (IDirect3DPixelShader9**)ppShader);
    }

    if (hr != D3D_OK)
    {
		wchar_t temp[512];
        LoadResString(IDS_ERROR_CREATING_SHADER,temp,sizeof(temp));
		dumpmsg(temp);
        if (bHardErrors)
		    MessageBoxW(GetPluginWindow(), temp, LoadResString(IDS_MILKDROP_ERROR,title,64), MB_OK|MB_SETFOREGROUND|MB_TOPMOST );
        else {
            AddError(temp, 6.0f, ERR_PRESET, true);
        }
		return false;
    }

    pShaderByteCode->Release();

    return true;
}

//----------------------------------------------------------------------

void CPlugin::CleanUpMyDX9Stuff(int final_cleanup)
{
    // Clean up all your DX9 and D3DX textures, fonts, buffers, etc. here.
    // EVERYTHING CREATED IN ALLOCATEMYDX9STUFF() SHOULD BE CLEANED UP HERE.
    // The input parameter, 'final_cleanup', will be 0 if this is
    //   a routine cleanup (part of a window resize or switch between
    //   fullscr/windowed modes), or 1 if this is the final cleanup
    //   and the plugin is exiting.  Note that even if it is a routine
    //   cleanup, *you still have to release ALL your DirectX stuff,
    //   because the DirectX device is being destroyed and recreated!*
    // Also set all the pointers back to NULL;
    //   this is important because if we go to reallocate the DX9
    //   stuff later, and something fails, then CleanUp will get called,
    //   but it will then be trying to clean up invalid pointers.)
    // The SafeRelease() and SafeDelete() macros make your code prettier;
    //   they are defined here in utility.h as follows:
    //       #define SafeRelease(x) if (x) {x->Release(); x=NULL;}
    //       #define SafeDelete(x)  if (x) {delete x; x=NULL;}
    // IMPORTANT:
    //   This function ISN'T only called when the plugin exits!
    //   It is also called whenever the user toggles between fullscreen and
    //   windowed modes, or resizes the window.  Basically, on these events,
    //   the base class calls CleanUpMyDX9Stuff before Reset()ing the DirectX
    //   device, and then calls AllocateMyDX9Stuff afterwards.


    // One funky thing here: if we're switching between fullscreen and windowed,
    //  or doing any other thing that causes all this stuff to get reloaded in a second,
    //  then if we were blending 2 presets, jump fully to the new preset.
    // Otherwise the old preset wouldn't get all reloaded, and it app would crash
    //  when trying to use its stuff.
    if (m_nLoadingPreset != 0) {
        // finish up the pre-load & start the official blend
        m_nLoadingPreset = 8;
        LoadPresetTick();
    }
    // just force this:
    m_pState->m_bBlending = false;


    for (size_t i=0; i<m_textures.size(); i++)
        if (m_textures[i].texptr)
        {
            // notify all CShaderParams classes that we're releasing a bindable texture!!
            size_t N = global_CShaderParams_master_list.size();
            for (size_t j=0; j<N; j++)
                global_CShaderParams_master_list[j]->OnTextureEvict( m_textures[i].texptr );

            SafeRelease(m_textures[i].texptr);
        }
    m_textures.clear();

    // DON'T RELEASE blur textures - they were already released because they're in m_textures[].
    #if (NUM_BLUR_TEX>0)
        for (i=0; i<NUM_BLUR_TEX; i++)
            m_lpBlur[i] = NULL;//SafeRelease(m_lpBlur[i]);
    #endif

    // NOTE: not necessary; shell does this for us.

    SafeRelease(m_pSpriteVertDecl);
    SafeRelease(m_pWfVertDecl);
    SafeRelease(m_pMyVertDecl);

    m_shaders.comp.Clear();
    m_shaders.warp.Clear();
    m_OldShaders.comp.Clear();
    m_OldShaders.warp.Clear();
    m_NewShaders.comp.Clear();
    m_NewShaders.warp.Clear();
    m_fallbackShaders_vs.comp.Clear();
    m_fallbackShaders_ps.comp.Clear();
    m_fallbackShaders_vs.warp.Clear();
    m_fallbackShaders_ps.warp.Clear();
    m_BlurShaders[0].vs.Clear();
    m_BlurShaders[0].ps.Clear();
    m_BlurShaders[1].vs.Clear();
    m_BlurShaders[1].ps.Clear();
    SafeRelease( m_pShaderCompileErrors );

    // 2. release stuff
    SafeRelease(m_lpVS[0]);
    SafeRelease(m_lpVS[1]);
    SafeRelease(m_lpDDSTitle);
    SafeRelease(m_d3dx_title_font_doublesize);

    // NOTE: THIS CODE IS IN THE RIGHT PLACE.
    if (m_gdi_title_font_doublesize)
    {
        DeleteObject(m_gdi_title_font_doublesize);
        m_gdi_title_font_doublesize = NULL;
    }

    // SURI: a routine cleanup (resize, fullscreen / desktop toggle, lost device) keeps the live
    // sprites and relaunches them in AllocateMyDX9Stuff; the final cleanup drops them.
    if (final_cleanup)
        m_nSpriteSnap = 0;
    else
        SnapshotSprites();
    m_texmgr.Finish();

	if (m_verts != NULL)
	{
		delete m_verts;
		m_verts = NULL;
	}

	if (m_verts_temp != NULL)
	{
		delete m_verts_temp;
		m_verts_temp = NULL;
	}

	if (m_vertinfo != NULL)
	{
		delete m_vertinfo;
		m_vertinfo = NULL;
	}

	if (m_indices_list != NULL)
	{
		delete m_indices_list;
		m_indices_list = NULL;
	}

    if (m_indices_strip != NULL)
	{
		delete m_indices_strip;
		m_indices_strip = NULL;
	}

    ClearErrors();

    // This setting is closely tied to the modern skin "random" button.
    // The "random" state should be preserved from session to session.
    // It's pretty safe to do, because the Scroll Lock key is hard to
    //   accidentally click... :)
    WritePrivateProfileIntW(m_bPresetLockedByUser,L"bPresetLockOnAtStartup", GetConfigIniFile(),L"settings");
}

//----------------------------------------------------------------------
//----------------------------------------------------------------------
//----------------------------------------------------------------------

void CPlugin::MyRenderFn(int redraw)
{
	EnterCriticalSection(&g_cs);

    // Render a frame of animation here.
    // This function is called each frame just AFTER BeginScene().
    // For timing information, call 'GetTime()' and 'GetFps()'.
    // The usual formula is like this (but doesn't have to be):
    //   1. take care of timing/other paperwork/etc. for new frame
    //   2. clear the background
    //   3. get ready for 3D drawing
    //   4. draw your 3D stuff
    //   5. call PrepareFor2DDrawing()
    //   6. draw your 2D stuff (overtop of your 3D scene)
    // If the 'redraw' flag is 1, you should try to redraw
    //   the last frame; GetTime, GetFps, and GetFrame should
    //   all return the same values as they did on the last
    //   call to MyRenderFn().  Otherwise, the redraw flag will
    //   be zero, and you can draw a new frame.  The flag is
    //   used to force a repaint when nothing else changed.

    //   1. take care of timing/other paperwork/etc. for new frame
    if (!redraw)
    {
        float dt = GetTime() - m_prev_time;
        m_prev_time = GetTime(); // note: m_prev_time is not for general use!
        m_bPresetLockedByCode = (m_UI_mode != UI_REGULAR);
        if (m_bPresetLockedByUser || m_bPresetLockedByCode)
        {
            // to freeze time (at current preset time value) when menus are up or Scroll Lock is on:
		    //m_fPresetStartTime += dt;
		    //m_fNextPresetTime += dt;
            // OR, to freeze time @ [preset] zero, so that when you exit menus,
            //   you don't run the risk of it changing the preset on you right away:
            m_fPresetStartTime = GetTime();
        	m_fNextPresetTime = -1.0f;		// flags UpdateTime() to recompute this.
        }

    }

    m_bHadFocus = m_bHasFocus;

    HWND plugin = GetPluginWindow();
    HWND focus = GetFocus();
    HWND cur = plugin;

    m_bHasFocus = false;
    do
    {
        m_bHasFocus = (focus == cur);
        if (m_bHasFocus)
            break;
        cur = GetParent(cur);
    }
    while (cur != NULL);

    if (m_hTextWnd && focus==m_hTextWnd)
        m_bHasFocus = 1;

    if (GetFocus()==NULL)
        m_bHasFocus = 0;

    if (!redraw)
    {
        GetSongTitle(m_szSongTitle, _countof(m_szSongTitle));
        if (wcscmp(m_szSongTitle, m_szSongTitlePrev))
        {
            // SURI: prev follows every change (also to empty), so the same song is
            // announced again after the player is closed and reopened; empty is never announced
            bool bAnnounce = SongChanged(m_szSongTitlePrev, m_szSongTitle);
            lstrcpynW(m_szSongTitlePrev, m_szSongTitle, _countof(m_szSongTitlePrev));
            if (bAnnounce && m_bSongTitleAnims)
                LaunchSongTitleAnim(true);
        }
    }

    // 2. Clear the background:
    //DWORD clear_color = (m_fog_enabled) ? FOG_COLOR : 0xFF000000;
    //GetDevice()->Clear(0, 0, D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER, clear_color, 1.0f, 0);

    // 5. switch to 2D drawing mode.  2D coord system:
    //         +--------+ Y=-1
    //         |        |
    //         | screen |             Z=0: front of scene
    //         |        |             Z=1: back of scene
    //         +--------+ Y=1
    //       X=-1      X=1
    PrepareFor2DDrawing(GetDevice());

    if (!redraw)
        DoCustomSoundAnalysis();    // emulates old pre-vms milkdrop sound analysis

    RenderFrame(redraw);  // see milkdropfs.cpp

    if (!redraw)
    {
        m_nFramesSinceResize++;
        if (m_nLoadingPreset > 0)
        {
            LoadPresetTick();
        }
    }

    LeaveCriticalSection(&g_cs);
}

//----------------------------------------------------------------------
//----------------------------------------------------------------------
//----------------------------------------------------------------------
//----------------------------------------------------------------------

void CPlugin::DrawTooltip(wchar_t* str, int xR, int yB)
{
    // draws a string in the lower-right corner of the screen.
    // note: ID3DXFont handles DT_RIGHT and DT_BOTTOM *very poorly*.
    //       it is best to calculate the size of the text first,
    //       then place it in the right spot.
    // note: use DT_WORDBREAK instead of DT_WORD_ELLIPSES, otherwise certain fonts'
    //       calcrect (for the dark box) will be wrong.

    RECT r, r2;
    SetRect(&r, 0, 0, xR-UiPx(TEXT_MARGIN)*2, 2048);
	m_text.DrawTextW(GetFont(TOOLTIP_FONT), str, -1, &r, DT_CALCRECT, 0xFFFFFFFF, false);
    r2.bottom = yB - UiPx(TEXT_MARGIN) - UiPx(3);   // SURI: room for the rounded box; margin scaled as in DrawAndDisplay
    r2.right  = xR - UiPx(TEXT_MARGIN) - UiPx(8);
    r2.left   = r2.right - (r.right-r.left);
    r2.top    = r2.bottom - (r.bottom-r.top);
	RECT r3 = r2; r3.left -= UiPx(8); r3.top -= UiPx(3); r3.right += UiPx(8); r3.bottom += UiPx(3);
	DrawRoundedPanel(&r3, UiPx(6), CARD_FILL_TOAST, CARD_BORDER);   // SURI: was DrawDarkTranslucentBox
    m_text.DrawTextW(GetFont(TOOLTIP_FONT), str, -1, &r2, 0, 0xFFFFFFFF, false);
}

#define MTO_UPPER_RIGHT 0
#define MTO_UPPER_LEFT  1
#define MTO_LOWER_RIGHT 2
#define MTO_LOWER_LEFT  3

#define SelectFont(n) { \
    pFont = GetFont(n); \
    h = GetFontHeight(n); \
}

#define MyTextOut_BGCOLOR(str, corner, bDarkBox, boxColor) { \
    SetRect(&r, 0, 0, xR-xL, 2048); \
	m_text.DrawTextW(pFont, str, -1, &r, DT_NOPREFIX | ((corner == MTO_UPPER_RIGHT)?0:DT_SINGLELINE) | DT_WORD_ELLIPSIS | DT_CALCRECT | ((corner == MTO_UPPER_RIGHT) ? DT_RIGHT : 0), 0xFFFFFFFF, false, boxColor); \
    int w = r.right - r.left; \
    if      (corner == MTO_UPPER_LEFT ) SetRect(&r, xL, *upper_left_corner_y, xL+w, *upper_left_corner_y + h); \
    else if (corner == MTO_UPPER_RIGHT) SetRect(&r, xR-w, *upper_right_corner_y, xR, *upper_right_corner_y + h); \
    else if (corner == MTO_LOWER_LEFT ) SetRect(&r, xL, *lower_left_corner_y - h, xL+w, *lower_left_corner_y); \
    else if (corner == MTO_LOWER_RIGHT) SetRect(&r, xR-w, *lower_right_corner_y - h, xR, *lower_right_corner_y); \
	m_text.DrawTextW(pFont, str, -1, &r, DT_NOPREFIX | ((corner == MTO_UPPER_RIGHT)?0:DT_SINGLELINE) | DT_WORD_ELLIPSIS | ((corner == MTO_UPPER_RIGHT) ? DT_RIGHT: 0), 0xFFFFFFFF, bDarkBox, boxColor); \
    if      (corner == MTO_UPPER_LEFT ) *upper_left_corner_y  += h; \
    else if (corner == MTO_UPPER_RIGHT) *upper_right_corner_y += h; \
    else if (corner == MTO_LOWER_LEFT ) *lower_left_corner_y  -= h; \
    else if (corner == MTO_LOWER_RIGHT) *lower_right_corner_y -= h; \
}

#define MyTextOut(str, corner, bDarkBox) MyTextOut_BGCOLOR(str, corner, bDarkBox, 0xFF000000)

#define MyTextOut_Shadow(str, corner) { \
    /* calc rect size */        \
    SetRect(&r, 0, 0, xR-xL, 2048); \
	m_text.DrawTextW(pFont, (wchar_t*)str, -1, &r, DT_NOPREFIX | DT_SINGLELINE | DT_WORD_ELLIPSIS | DT_CALCRECT, 0xFFFFFFFF, false, 0xFF000000); \
    int w = r.right - r.left; \
    /* first the shadow */         \
    if      (corner == MTO_UPPER_LEFT ) SetRect(&r, xL, *upper_left_corner_y, xL+w, *upper_left_corner_y + h); \
    else if (corner == MTO_UPPER_RIGHT) SetRect(&r, xR-w, *upper_right_corner_y, xR, *upper_right_corner_y + h); \
    else if (corner == MTO_LOWER_LEFT ) SetRect(&r, xL, *lower_left_corner_y - h, xL+w, *lower_left_corner_y); \
    else if (corner == MTO_LOWER_RIGHT) SetRect(&r, xR-w, *lower_right_corner_y - h, xR, *lower_right_corner_y); \
    r.top += 1; r.left += 1;      \
    m_text.DrawTextW(pFont, (wchar_t*)str, -1, &r, DT_NOPREFIX | DT_SINGLELINE | DT_WORD_ELLIPSIS, 0xFF000000, false, 0xFF000000); \
    /* now draw real text */            \
    r.top -= 1; r.left -= 1;       \
	m_text.DrawTextW(pFont, (wchar_t*)str, -1, &r, DT_NOPREFIX | DT_SINGLELINE | DT_WORD_ELLIPSIS, 0xFFFFFFFF, false, 0xFF000000); \
    if      (corner == MTO_UPPER_LEFT ) *upper_left_corner_y  += h; \
    else if (corner == MTO_UPPER_RIGHT) *upper_right_corner_y += h; \
    else if (corner == MTO_LOWER_LEFT ) *lower_left_corner_y  -= h; \
    else if (corner == MTO_LOWER_RIGHT) *lower_right_corner_y -= h; \
}

// SURI: messages are toasts now. A notification replaces the previous one and stays 2.5 s with a
// green dot; "scanning presets" keeps its duration (green dot); every other category is an error:
// red dot, at least 6 s. The category is the toast tag, so ClearErrors still works.
void CPlugin::AddError(wchar_t* szMsg, float fDuration, int category, bool bBold)
{
    assert(category != ERR_ALL);
    if (category == ERR_NOTIFY)
    {
        m_toasts.Clear(ERR_NOTIFY);
        fDuration = 2.5f;
    }
    const bool isError = (category != ERR_NOTIFY && category != ERR_SCANNING_PRESETS);
    if (isError && fDuration < 6.0f)
        fDuration = 6.0f;   // errors stay at least 6 s; longer durations asked for by the caller are kept
    m_toasts.Push(szMsg ? szMsg : L"", GetTime(), fDuration, isError, category);
}

void CPlugin::ClearErrors(int category)  // 0=all categories
{
    m_toasts.Clear(category);   // SURI: ERR_ALL == 0 clears everything
}

// SURI: scales the alpha of an ARGB colour (toast fade-out)
static DWORD FadeArgb(DWORD c, float a)
{
    if (a >= 1.0f) return c;
    if (a <= 0.0f) return c & 0x00FFFFFF;
    return (c & 0x00FFFFFF) | ((DWORD)((c >> 24) * a) << 24);
}

void CPlugin::MyRenderUI(
                         int *upper_left_corner_y,  // increment me!
                         int *upper_right_corner_y, // increment me!
                         int *lower_left_corner_y,  // decrement me!
                         int *lower_right_corner_y, // decrement me!
                         int xL,
                         int xR
                        )
{
    // draw text messages directly to the back buffer.
    // when you draw text into one of the four corners,
    //   draw the text at the current 'y' value for that corner
    //   (one of the first 4 params to this function),
    //   and then adjust that y value so that the next time
    //   text is drawn in that corner, it gets drawn above/below
    //   the previous text (instead of overtop of it).
    // when drawing into the upper or lower LEFT corners,
    //   left-align your text to 'xL'.
    // when drawing into the upper or lower RIGHT corners,
    //   right-align your text to 'xR'.

    // note: try to keep the bounding rectangles on the text small;
    //   the smaller the area that has to be locked (to draw the text),
    //   the faster it will be.  (on some cards, drawing text is
    //   ferociously slow, so even if it works okay on yours, it might
    //   not work on another video card.)
    // note: if you want some text to be on the screen often, and the text
    //   won't be changing every frame, please consider the poor folks
    //   whose video cards hate that; in that case you should probably
    //   draw the text just once, to a texture, and then display the
    //   texture each frame.  This is how the help screen is done; see
    //   pluginshell.cpp for example code.

    RECT r = {0};
	wchar_t buf[512] = {0};
    LPD3DXFONT pFont = GetFont(DECORATIVE_FONT);
    int h = GetFontHeight(DECORATIVE_FONT);

    if (!pFont)
        return;

    if (!GetFont(DECORATIVE_FONT))
        return;

    const int toastTop = *upper_right_corner_y - UiPx(TEXT_MARGIN) + UiPx(28);   // SURI: 28 px below the window top
    RECT infoCard = { 0, 0, 0, 0 };                                        // SURI: set by DrawInfoCard

    // 1. render text in upper-right corner - EXCEPT USER MESSAGE - it goes last b/c it draws a box under itself
    //                                        and it should be visible over everything else (usually an error msg)
    {
        // SURI: a)-c) preset name, rating, frame rate and lock state as one card (design B)
        DrawInfoCard(&infoCard, upper_right_corner_y, xL, xR);

        // d) debug information
		if (m_bShowDebugInfo)
		{
            SelectFont(SIMPLE_FONT);
			swprintf(buf, L" %s: %6.4f ", LoadResString(IDS_PF_MONITOR), (float)(*m_pState->var_pf_monitor));
            MyTextOut_Shadow(buf, MTO_UPPER_RIGHT);
		}

        // NOTE: custom timed msg comes at the end!!
    }

    // 2. render text in lower-right corner
    {
        // waitstring tooltip:
		if (m_waitstring.bActive && m_bShowMenuToolTips && m_waitstring.szToolTip[0])
		{
            DrawTooltip(m_waitstring.szToolTip, xR, *lower_right_corner_y);
		}
    }

    // 3. render text in lower-left corner
    {
        // render song title in lower-left corner:
        if (m_bShowSongTitle)
        {
			wchar_t buf4[512] = {0};
            SelectFont(DECORATIVE_FONT);
            GetSongTitle(buf4, _countof(buf4));
            MyTextOut_Shadow(buf4, MTO_LOWER_LEFT);
        }

    }

    // 4. render text in upper-left corner
    {
		wchar_t buf[64000] = {0};  // must fit the longest strings (code strings are 32768 chars)
								   // AND leave extra space for &->&&, and [,[,& insertion
		char bufA[64000] = {0};

        SelectFont(SIMPLE_FONT);

		// stuff for loading presets, menus, etc:

		if (m_waitstring.bActive)
		{
			// 1. draw the prompt string
            MyTextOut(m_waitstring.szPrompt, MTO_UPPER_LEFT, true);

            // extra instructions:
            bool bIsWarp = m_waitstring.bDisplayAsCode && (m_pCurMenu == &m_menuPreset) && !wcscmp(m_menuPreset.GetCurItem()->m_szName, L"[ edit warp shader ]");
            bool bIsComp = m_waitstring.bDisplayAsCode && (m_pCurMenu == &m_menuPreset) && !wcscmp(m_menuPreset.GetCurItem()->m_szName, L"[ edit composite shader ]");
            if (bIsWarp || bIsComp)
            {
                if (m_bShowShaderHelp) {
                    MyTextOut(LoadResString(IDS_PRESS_F9_TO_HIDE_SHADER_QREF), MTO_UPPER_LEFT, true);
                }
                else {
                    MyTextOut(LoadResString(IDS_PRESS_F9_TO_SHOW_SHADER_QREF), MTO_UPPER_LEFT, true);
                }
                *upper_left_corner_y  += h*2/3;

                if (m_bShowShaderHelp)
                {
                    // draw dark box - based on longest line & # lines...
                    SetRect(&r, 0, 0, 2048, 2048);
                    m_text.DrawTextW(pFont, LoadResString(IDS_STRING615), -1, &r, DT_NOPREFIX | DT_SINGLELINE | DT_WORD_ELLIPSIS | DT_CALCRECT, 0xFFFFFFFF, false, 0xFF000000);
                    RECT darkbox;
                    SetRect(&darkbox, xL, *upper_left_corner_y-2, xL+r.right-r.left, *upper_left_corner_y + (r.bottom-r.top)*13 + 2);
                    DrawDarkTranslucentBox(&darkbox);

                    MyTextOut(LoadResString(IDS_STRING616), MTO_UPPER_LEFT, false);
                    MyTextOut(LoadResString(IDS_STRING617), MTO_UPPER_LEFT, false);
                    MyTextOut(LoadResString(IDS_STRING618), MTO_UPPER_LEFT, false);
                    MyTextOut(LoadResString(IDS_STRING619), MTO_UPPER_LEFT, false);
                    MyTextOut(LoadResString(IDS_STRING620), MTO_UPPER_LEFT, false);
                    MyTextOut(LoadResString(IDS_STRING621), MTO_UPPER_LEFT, false);
                    if (bIsWarp)
                    {
                        MyTextOut(LoadResString(IDS_STRING622), MTO_UPPER_LEFT, false);
                        MyTextOut(LoadResString(IDS_STRING623), MTO_UPPER_LEFT, false);
                        MyTextOut(LoadResString(IDS_STRING624), MTO_UPPER_LEFT, false);
                        MyTextOut(LoadResString(IDS_STRING625), MTO_UPPER_LEFT, false);
                        MyTextOut(LoadResString(IDS_STRING626), MTO_UPPER_LEFT, false);
                        MyTextOut(LoadResString(IDS_STRING627), MTO_UPPER_LEFT, false);
                        MyTextOut(LoadResString(IDS_STRING628), MTO_UPPER_LEFT, false);
                    }
                    else if (bIsComp)
                    {
                        MyTextOut(LoadResString(IDS_STRING629), MTO_UPPER_LEFT, false);
                        MyTextOut(LoadResString(IDS_STRING630), MTO_UPPER_LEFT, false);
                        MyTextOut(LoadResString(IDS_STRING631), MTO_UPPER_LEFT, false);
                        MyTextOut(LoadResString(IDS_STRING632), MTO_UPPER_LEFT, false);
                        MyTextOut(LoadResString(IDS_STRING633), MTO_UPPER_LEFT, false);
                        MyTextOut(LoadResString(IDS_STRING634), MTO_UPPER_LEFT, false);
                        MyTextOut(LoadResString(IDS_STRING635), MTO_UPPER_LEFT, false);
                    }
                    *upper_left_corner_y  += h*2/3;
                }
            }
            else if (m_UI_mode == UI_SAVEAS && (m_bWarpShaderLock || m_bCompShaderLock))
            {
                wchar_t buf[256] = {0};
				int shader_msg_id = IDS_COMPOSITE_SHADER_LOCKED;
                if (m_bWarpShaderLock && m_bCompShaderLock)
					shader_msg_id = IDS_WARP_AND_COMPOSITE_SHADERS_LOCKED;
                else if (m_bWarpShaderLock && !m_bCompShaderLock)
					shader_msg_id = IDS_WARP_SHADER_LOCKED;
                else
                    shader_msg_id = IDS_COMPOSITE_SHADER_LOCKED;

				LoadResString(shader_msg_id, buf, 256);
                MyTextOut_BGCOLOR(buf, MTO_UPPER_LEFT, true, 0xFF000000);
                *upper_left_corner_y  += h*2/3;
            }
            else
                *upper_left_corner_y  += h*2/3;


            // 2. reformat the waitstring text for display
            int bBrackets = m_waitstring.nSelAnchorPos != -1 && m_waitstring.nSelAnchorPos != m_waitstring.nCursorPos;
            int bCursorBlink = ( !bBrackets &&
                                 ((int)(GetTime()*270.0f) % 100 > 50)
                                 //((GetFrame() % 3) >= 2)
                               );

			lstrcpyW(buf, m_waitstring.szText);
			lstrcpyA(bufA, (char*)m_waitstring.szText);

            int temp_cursor_pos = m_waitstring.nCursorPos;
            int temp_anchor_pos = m_waitstring.nSelAnchorPos;

			if (bBrackets)
			{
				if (m_waitstring.bDisplayAsCode)
				{
					// insert [] around the selection
					int start = (temp_cursor_pos < temp_anchor_pos) ? temp_cursor_pos : temp_anchor_pos;
					int end   = (temp_cursor_pos > temp_anchor_pos) ? temp_cursor_pos - 1 : temp_anchor_pos - 1;
					int len   = lstrlenA(bufA);
					int i;

					for (i=len; i>end; i--)
						bufA[i+1] = bufA[i];
					bufA[end+1] = ']';
					len++;

					for (i=len; i>=start; i--)
						bufA[i+1] = bufA[i];
					bufA[start] = '[';
					len++;
				}
				else
				{
					// insert [] around the selection
					int start = (temp_cursor_pos < temp_anchor_pos) ? temp_cursor_pos : temp_anchor_pos;
					int end   = (temp_cursor_pos > temp_anchor_pos) ? temp_cursor_pos - 1 : temp_anchor_pos - 1;
					int len   = lstrlenW(buf);
					int i;

					for (i=len; i>end; i--)
						buf[i+1] = buf[i];
					buf[end+1] = L']';
					len++;

					for (i=len; i>=start; i--)
						buf[i+1] = buf[i];
					buf[start] = L'[';
					len++;
				}
			}
			else
			{
				// underline the current cursor position by rapidly toggling the character with an underscore
				if (m_waitstring.bDisplayAsCode)
				{
					if (bCursorBlink)
					{
						if (bufA[temp_cursor_pos] == 0)
						{
							bufA[temp_cursor_pos] = '_';
							bufA[temp_cursor_pos+1] = 0;
						}
						else if (bufA[temp_cursor_pos] == LINEFEED_CONTROL_CHAR)
						{
							for (int i=strlen(bufA); i>=temp_cursor_pos; i--)
					    		bufA[i+1] = bufA[i];
							bufA[temp_cursor_pos] = '_';
						}
						else if (bufA[temp_cursor_pos] == '_')
							bufA[temp_cursor_pos] = ' ';
						else // it's a space or symbol or alphanumeric.
							bufA[temp_cursor_pos] = '_';
					}
					else
					{
						if (bufA[temp_cursor_pos] == 0)
						{
							bufA[temp_cursor_pos] = ' ';
							bufA[temp_cursor_pos+1] = 0;
						}
						else if (bufA[temp_cursor_pos] == LINEFEED_CONTROL_CHAR)
						{
							for (int i=strlen(bufA); i>=temp_cursor_pos; i--)
					    		bufA[i+1] = bufA[i];
							bufA[temp_cursor_pos] = ' ';
						}
						//else if (buf[temp_cursor_pos] == '_')
							// do nothing
						//else // it's a space or symbol or alphanumeric.
							// do nothing
					}
				}
				else
				{
					if (bCursorBlink)
					{
						if (buf[temp_cursor_pos] == 0)
						{
							buf[temp_cursor_pos] = L'_';
							buf[temp_cursor_pos+1] = 0;
						}
						else if (buf[temp_cursor_pos] == LINEFEED_CONTROL_CHAR)
						{
							for (int i=wcslen(buf); i>=temp_cursor_pos; i--)
					    		buf[i+1] = buf[i];
							buf[temp_cursor_pos] = L'_';
						}
						else if (buf[temp_cursor_pos] == L'_')
							buf[temp_cursor_pos] = L' ';
						else // it's a space or symbol or alphanumeric.
							buf[temp_cursor_pos] = L'_';
					}
					else
					{
						if (buf[temp_cursor_pos] == 0)
						{
							buf[temp_cursor_pos] = L' ';
							buf[temp_cursor_pos+1] = 0;
						}
						else if (buf[temp_cursor_pos] == LINEFEED_CONTROL_CHAR)
						{
							for (int i=wcslen(buf); i>=temp_cursor_pos; i--)
					    		buf[i+1] = buf[i];
							buf[temp_cursor_pos] = L' ';
						}
						//else if (buf[temp_cursor_pos] == '_')
							// do nothing
						//else // it's a space or symbol or alphanumeric.
							// do nothing
					}
				}
			}

            RECT rect = {0};
            SetRect(&rect, xL, *upper_left_corner_y, xR, *lower_left_corner_y);
            rect.top += PLAYLIST_INNER_MARGIN;
            rect.left += PLAYLIST_INNER_MARGIN;
            rect.right -= PLAYLIST_INNER_MARGIN;
            rect.bottom -= PLAYLIST_INNER_MARGIN;

			// then draw the edit string
			if (m_waitstring.bDisplayAsCode)
			{
				char buf2[8192] = {0};
				int top_of_page_pos = 0;
				// SURI: monospace for code (the cursor is drawn by swapping '_' and ' ' into the text)
				LPD3DXFONT fCode = GetUiFont(UIF_CODE) ? GetUiFont(UIF_CODE) : GetFont(SIMPLE_FONT);

				// compute top_of_page_pos so that the line the cursor is on will show.
                // also compute dims of the black rectangle while we're at it.
				{
					int start = 0;
					int pos   = 0;
					int ypixels = 0;
					int page  = 1;
                    int exit_on_next_page = 0;

                    RECT box = rect;
                    box.right = box.left;
                    box.bottom = box.top;

					while (bufA[pos] != 0)  // for each line of text... (note that it might wrap)
					{
						start = pos;
						while (bufA[pos] != LINEFEED_CONTROL_CHAR && bufA[pos] != 0)
							pos++;

						char ch = bufA[pos];
						bufA[pos] = 0;
						sprintf(buf2, "   %sX", &bufA[start]); // put a final 'X' instead of ' ' b/c CALCRECT returns w==0 if string is entirely whitespace!
						RECT r2 = rect;
                        r2.bottom = 4096;
						m_text.DrawTextA(fCode, buf2, -1, &r2, DT_CALCRECT /*| DT_WORDBREAK*/, 0xFFFFFFFF, false);
                        int h = r2.bottom-r2.top;
						ypixels += h;
						bufA[pos] = ch;

                        if (start > m_waitstring.nCursorPos) // make sure 'box' gets updated for each line on this page
                            exit_on_next_page = 1;

						if (ypixels > rect.bottom-rect.top) // this line belongs on the next page
						{
                            if (exit_on_next_page)
                            {
                                bufA[start] = 0; // so text stops where the box stops, when we draw the text
                                break;
                            }

							ypixels = h;
							top_of_page_pos = start;
							page++;

                            box = rect;
                            box.right = box.left;
                            box.bottom = box.top;
						}
                        box.bottom += h;
                        box.right = max(box.right, box.left + r2.right-r2.left);

                        if (bufA[pos]==0)
                            break;
                        pos++;
					}

                    // use r2 to draw a dark box:
                    box.top -= PLAYLIST_INNER_MARGIN;
                    box.left -= PLAYLIST_INNER_MARGIN;
                    box.right += PLAYLIST_INNER_MARGIN;
                    box.bottom += PLAYLIST_INNER_MARGIN;
                    DrawDarkTranslucentBox(&box);
                    *upper_left_corner_y += box.bottom - box.top + PLAYLIST_INNER_MARGIN*3;
					swprintf(m_waitstring.szToolTip, LoadResString(IDS_PAGE_X), page);
				}

				// display multiline (replace all character 13's with a CR)
				{
					int start = top_of_page_pos;
					int pos   = top_of_page_pos;

					while (bufA[pos] != 0)
					{
						while (bufA[pos] != LINEFEED_CONTROL_CHAR && bufA[pos] != 0)
							pos++;

						char ch = bufA[pos];
						bufA[pos] = 0;
						sprintf(buf2, "   %s ", &bufA[start]);
                        DWORD color = MENU_COLOR;
						if (m_waitstring.nCursorPos >= start && m_waitstring.nCursorPos <= pos)
							color = MENU_HILITE_COLOR;
						rect.top += m_text.DrawTextA(fCode, buf2, -1, &rect, 0/*DT_WORDBREAK*/, color, false);
						bufA[pos] = ch;

                        if (rect.top > rect.bottom)
                            break;

						if (bufA[pos] != 0) pos++;
						start = pos;
					}
				}
                // note: *upper_left_corner_y is updated above, when the dark box is drawn.
			}
			else
			{
				wchar_t buf2[8192] = {0};

                // display on one line
                RECT box = rect;
                box.bottom = 4096;
				swprintf(buf2, L"    %sX", buf);  // put a final 'X' instead of ' ' b/c CALCRECT returns w==0 if string is entirely whitespace!
				m_text.DrawTextW(GetFont(SIMPLE_FONT), buf2, -1, &box, DT_CALCRECT, MENU_COLOR, false );

                // use r2 to draw a dark box:
                box.top -= PLAYLIST_INNER_MARGIN;
                box.left -= PLAYLIST_INNER_MARGIN;
                box.right += PLAYLIST_INNER_MARGIN;
                box.bottom += PLAYLIST_INNER_MARGIN;
                DrawDarkTranslucentBox(&box);
                *upper_left_corner_y += box.bottom - box.top + PLAYLIST_INNER_MARGIN*3;

				swprintf(buf2, L"    %s ", buf);
                m_text.DrawTextW(GetFont(SIMPLE_FONT), buf2, -1, &rect, 0, MENU_COLOR, false );
			}
		}
		else if (m_UI_mode == UI_MENU)
		{
			assert(m_pCurMenu);
            // SURI: the menu as a card. The selected row gets a band (MyMenuTextOut), and the
            // navigation hint / item tooltip is the card's last line instead of a corner tooltip.
            LPD3DXFONT fSmall = GetUiFont(UIF_SMALL);
            const int pad = UiPx(20), footGap = UiPx(10);
            const int x0 = xL + UiPx(18), y0 = *upper_left_corner_y + UiPx(18);
            int maxInnerW = (xR - UiPx(18)) - x0 - 2*pad;
            if (maxInnerW < UiPx(100)) maxInnerW = UiPx(100);

            wchar_t tip[1024] = {0};   // a copy: LoadResString's buffer is reused while the menu draws
            if (m_bShowMenuToolTips && m_pCurMenu->GetCurToolTip())
                lstrcpynW(tip, m_pCurMenu->GetCurToolTip(), 1024);
            int footW = 0, footH = 0;
            if (tip[0] && fSmall)
            {
                RECT fr = { 0, 0, maxInnerW, 0 };
                UiMeasure(fSmall, tip, &fr, DT_SINGLELINE);
                footW = min((int)(fr.right - fr.left), maxInnerW);
                SetRect(&fr, 0, 0, footW, 0);
                footH = UiMeasure(fSmall, tip, &fr, DT_WORDBREAK);
                footW = max(footW, (int)(fr.right - fr.left));
            }

            RECT mr;
            SetRect(&mr, x0 + pad, y0 + pad, x0 + pad + maxInnerW, *lower_left_corner_y - UiPx(18) - pad - (footH ? footH + footGap : 0));
            RECT calc = {0};
			m_pCurMenu->DrawMenu(mr, 1, &calc);
            const int innerW = max((int)(calc.right - calc.left), footW);
            const int innerH = (calc.bottom - calc.top) + (footH ? footGap + footH : 0);
            RECT card = { x0, y0, x0 + innerW + 2*pad, y0 + innerH + 2*pad };
            DrawRoundedPanel(&card, UiPx(14), CARD_FILL_PANEL, CARD_BORDER);

            m_ui_band_left  = x0 + UiPx(8);
            m_ui_band_right = card.right - UiPx(8);
			m_pCurMenu->DrawMenu(mr);
            m_ui_band_left = m_ui_band_right = 0;

            if (footH)
            {
                RECT fr = { x0 + pad, card.bottom - pad - footH, x0 + pad + footW + 2, card.bottom - pad };
                UiText(fSmall, tip, &fr, DT_WORDBREAK, CARD_TEXT_2);
            }
            *upper_left_corner_y = card.bottom + UiPx(10);
		}
		else if (m_UI_mode == UI_UPGRADE_PIXEL_SHADER)
		{
            RECT rect = {0};
            SetRect(&rect, xL, *upper_left_corner_y, xR, *lower_left_corner_y);

            if (m_pState->m_nWarpPSVersion >= m_nMaxPSVersion &&
                m_pState->m_nCompPSVersion >= m_nMaxPSVersion)
            {
                assert(m_pState->m_nMaxPSVersion == m_nMaxPSVersion);
                wchar_t buf[1024] = {0};
                swprintf(buf, LoadResString(IDS_PRESET_USES_HIGHEST_PIXEL_SHADER_VERSION), m_nMaxPSVersion);
		        rect.top += m_text.DrawTextW(GetFont(SIMPLE_FONT), buf, -1, &rect, DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX, MENU_COLOR, true);
		        rect.top += m_text.DrawTextW(GetFont(SIMPLE_FONT), LoadResString(IDS_PRESS_ESC_TO_RETURN), -1, &rect, DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX, MENU_COLOR, true);
            }
            else
            {
                if (m_pState->m_nMinPSVersion != m_pState->m_nMaxPSVersion)
                {
                    switch(m_pState->m_nMinPSVersion)
										{
										case MD2_PS_NONE:
											rect.top += m_text.DrawTextW(GetFont(SIMPLE_FONT), LoadResString(IDS_PRESET_HAS_MIXED_VERSIONS_OF_SHADERS), -1, &rect, DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX, MENU_COLOR, true);
											rect.top += m_text.DrawTextW(GetFont(SIMPLE_FONT), LoadResString(IDS_UPGRADE_SHADERS_TO_USE_PS2), -1, &rect, DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX, MENU_COLOR, true);
											break;
										case MD2_PS_2_0:
											rect.top += m_text.DrawTextW(GetFont(SIMPLE_FONT), LoadResString(IDS_PRESET_HAS_MIXED_VERSIONS_OF_SHADERS), -1, &rect, DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX, MENU_COLOR, true);
											rect.top += m_text.DrawTextW(GetFont(SIMPLE_FONT), LoadResString(IDS_UPGRADE_SHADERS_TO_USE_PS2X), -1, &rect, DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX, MENU_COLOR, true);
											break;
										case MD2_PS_2_X:
											rect.top += m_text.DrawTextW(GetFont(SIMPLE_FONT), LoadResString(IDS_PRESET_HAS_MIXED_VERSIONS_OF_SHADERS), -1, &rect, DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX, MENU_COLOR, true);
											rect.top += m_text.DrawTextW(GetFont(SIMPLE_FONT), LoadResString(IDS_UPGRADE_SHADERS_TO_USE_PS3), -1, &rect, DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX, MENU_COLOR, true);
											break;
										case MD2_PS_3_0:
											assert(false);
											break;
										default:
											assert(0);
											break;
										}
                }
                else
                {
                    switch(m_pState->m_nMinPSVersion)
										{
										case MD2_PS_NONE:
											rect.top += m_text.DrawTextW(GetFont(SIMPLE_FONT), LoadResString(IDS_PRESET_DOES_NOT_USE_PIXEL_SHADERS), -1, &rect, DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX, MENU_COLOR, true);
											rect.top += m_text.DrawTextW(GetFont(SIMPLE_FONT), LoadResString(IDS_UPGRADE_TO_USE_PS2), -1, &rect, DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX, MENU_COLOR, true);
											rect.top += m_text.DrawTextW(GetFont(SIMPLE_FONT), LoadResString(IDS_WARNING_OLD_GPU_MIGHT_NOT_WORK_WITH_PRESET), -1, &rect, DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX, MENU_COLOR, true);
											break;
										case MD2_PS_2_0:
											rect.top += m_text.DrawTextW(GetFont(SIMPLE_FONT), LoadResString(IDS_PRESET_CURRENTLY_USES_PS2), -1, &rect, DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX, MENU_COLOR, true);
											rect.top += m_text.DrawTextW(GetFont(SIMPLE_FONT), LoadResString(IDS_UPGRADE_TO_USE_PS2X), -1, &rect, DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX, MENU_COLOR, true);
											rect.top += m_text.DrawTextW(GetFont(SIMPLE_FONT), LoadResString(IDS_WARNING_OLD_GPU_MIGHT_NOT_WORK_WITH_PRESET), -1, &rect, DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX, MENU_COLOR, true);
											break;
										case MD2_PS_2_X:
											rect.top += m_text.DrawTextW(GetFont(SIMPLE_FONT), LoadResString(IDS_PRESET_CURRENTLY_USES_PS2X), -1, &rect, DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX, MENU_COLOR, true);
											rect.top += m_text.DrawTextW(GetFont(SIMPLE_FONT), LoadResString(IDS_UPGRADE_TO_USE_PS3), -1, &rect, DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX, MENU_COLOR, true);
											rect.top += m_text.DrawTextW(GetFont(SIMPLE_FONT), LoadResString(IDS_WARNING_OLD_GPU_MIGHT_NOT_WORK_WITH_PRESET), -1, &rect, DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX, MENU_COLOR, true);
											break;
										case MD2_PS_3_0:
											rect.top += m_text.DrawTextW(GetFont(SIMPLE_FONT), LoadResString(IDS_PRESET_CURRENTLY_USES_PS3), -1, &rect, DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX, MENU_COLOR, true);
											rect.top += m_text.DrawTextW(GetFont(SIMPLE_FONT), LoadResString(IDS_UPGRADE_TO_USE_PS4), -1, &rect, DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX, MENU_COLOR, true);
											rect.top += m_text.DrawTextW(GetFont(SIMPLE_FONT), LoadResString(IDS_WARNING_OLD_GPU_MIGHT_NOT_WORK_WITH_PRESET), -1, &rect, DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX, MENU_COLOR, true);
											break;
										default:
											assert(0);
											break;
										}
                }
            }
            *upper_left_corner_y = rect.top;
		}
		else if (m_UI_mode == UI_LOAD_DEL)
		{
            RECT rect;
            SetRect(&rect, xL, *upper_left_corner_y, xR, *lower_left_corner_y);
			rect.top += m_text.DrawTextW(GetFont(SIMPLE_FONT),  LoadResString(IDS_ARE_YOU_SURE_YOU_WANT_TO_DELETE_PRESET), -1, &rect, DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX, MENU_COLOR, true);
			swprintf(buf, LoadResString(IDS_PRESET_TO_DELETE), m_presets[m_nPresetListCurPos].szFilename.c_str());
			rect.top += m_text.DrawTextW(GetFont(SIMPLE_FONT),  buf, -1, &rect, DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX, MENU_COLOR, true);
            *upper_left_corner_y = rect.top;
		}
		else if (m_UI_mode == UI_SAVE_OVERWRITE)
		{
            RECT rect;
            SetRect(&rect, xL, *upper_left_corner_y, xR, *lower_left_corner_y);
			rect.top += m_text.DrawTextW(GetFont(SIMPLE_FONT),  LoadResString(IDS_FILE_ALREADY_EXISTS_OVERWRITE_IT), -1, &rect, DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX, MENU_COLOR, true);
			swprintf(buf, LoadResString(IDS_FILE_IN_QUESTION_X_MILK), m_waitstring.szText);
			rect.top += m_text.DrawTextW(GetFont(SIMPLE_FONT),  buf, -1, &rect, DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX, MENU_COLOR, true);
            if (m_bWarpShaderLock)
    			rect.top += m_text.DrawTextW(GetFont(SIMPLE_FONT), LoadResString(IDS_WARNING_DO_NOT_FORGET_WARP_SHADER_WAS_LOCKED), -1, &rect, DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX, 0xFFFFFFFF, true, 0xFFCC0000);
            if (m_bCompShaderLock)
    			rect.top += m_text.DrawTextW(GetFont(SIMPLE_FONT), LoadResString(IDS_WARNING_DO_NOT_FORGET_COMPOSITE_SHADER_WAS_LOCKED), -1, &rect, DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX, 0xFFFFFFFF, true, 0xFFCC0000);
            *upper_left_corner_y = rect.top;
		}
		else if (m_UI_mode == UI_MASHUP)
        {
			if (m_nPresets-m_nDirs == 0)
			{
				// note: this error message is repeated in milkdrop.cpp in LoadRandomPreset()
				wchar_t buf[1024];
                swprintf(buf, LoadResString(IDS_ERROR_NO_PRESET_FILE_FOUND_IN_X_MILK), m_szPresetDir);
                AddError(buf, 6.0f, ERR_MISC, true);
				m_UI_mode = UI_REGULAR;
			}
			else
			{
                UpdatePresetList(); // make sure list is completely ready

                // quick checks
                for (int mash=0; mash<MASH_SLOTS; mash++)
                {
                    // check validity
                    if (m_nMashPreset[mash] < m_nDirs)
                        m_nMashPreset[mash] = m_nDirs;
                    if (m_nMashPreset[mash] >= m_nPresets)
                        m_nMashPreset[mash] = m_nPresets-1;

                    // apply changes, if it's time
                    if (m_nLastMashChangeFrame[mash]+MASH_APPLY_DELAY_FRAMES+1 == GetFrame())
                    {
                        // import just a fragment of a preset!!
                        DWORD ApplyFlags = 0;
                        switch(mash)
                        {
							case 0: ApplyFlags = STATE_GENERAL; break;
							case 1: ApplyFlags = STATE_MOTION; break;
							case 2: ApplyFlags = STATE_WAVE; break;
							case 3: ApplyFlags = STATE_WARP; break;
							case 4: ApplyFlags = STATE_COMP; break;
                        }

                        wchar_t szFile[MAX_PATH];
                        swprintf(szFile, L"%s%s", m_szPresetDir, m_presets[m_nMashPreset[mash]].szFilename.c_str());

                        m_pState->Import(szFile, GetTime(), m_pState, ApplyFlags);
                        if (ApplyFlags & STATE_COMP)
                            ApplyFxToState(m_pState);   // SURI: the imported comp shader gets the live effects too

                        if (ApplyFlags & STATE_WARP)
                            SafeRelease( m_shaders.warp.ptr );
                        if (ApplyFlags & STATE_COMP)
                            SafeRelease( m_shaders.comp.ptr );
                        LoadShaders(&m_shaders, m_pState, false);

                        SetMenusForPresetVersion( m_pState->m_nWarpPSVersion, m_pState->m_nCompPSVersion );
                    }
                }

                MyTextOut(LoadResString(IDS_PRESET_MASH_UP_TEXT1), MTO_UPPER_LEFT, true);
                MyTextOut(LoadResString(IDS_PRESET_MASH_UP_TEXT2), MTO_UPPER_LEFT, true);
                MyTextOut(LoadResString(IDS_PRESET_MASH_UP_TEXT3), MTO_UPPER_LEFT, true);
                MyTextOut(LoadResString(IDS_PRESET_MASH_UP_TEXT4), MTO_UPPER_LEFT, true);
                *upper_left_corner_y += PLAYLIST_INNER_MARGIN;

                RECT rect;
                SetRect(&rect, xL, *upper_left_corner_y, xR, *lower_left_corner_y);
                rect.top += PLAYLIST_INNER_MARGIN;
                rect.left += PLAYLIST_INNER_MARGIN;
                rect.right -= PLAYLIST_INNER_MARGIN;
                rect.bottom -= PLAYLIST_INNER_MARGIN;

			    int lines_available = (rect.bottom - rect.top - PLAYLIST_INNER_MARGIN*2) / GetFontHeight(SIMPLE_FONT);
                lines_available -= MASH_SLOTS;

			    if (lines_available < 10)
			    {
				    // force it
				    rect.bottom = rect.top + GetFontHeight(SIMPLE_FONT)*10 + 1;
				    lines_available = 10;
			    }
                if (lines_available > 16)
                    lines_available = 16;

				if (m_bUserPagedDown)
				{
					m_nMashPreset[m_nMashSlot] += lines_available;
					if (m_nMashPreset[m_nMashSlot] >= m_nPresets)
						m_nMashPreset[m_nMashSlot] = m_nPresets - 1;
					m_bUserPagedDown = false;
				}
				if (m_bUserPagedUp)
				{
					m_nMashPreset[m_nMashSlot] -= lines_available;
					if (m_nMashPreset[m_nMashSlot] < m_nDirs)
						m_nMashPreset[m_nMashSlot] = m_nDirs;
					m_bUserPagedUp = false;
				}

				int i;
				int first_line = m_nMashPreset[m_nMashSlot] - (m_nMashPreset[m_nMashSlot] % lines_available);
				int last_line  = first_line + lines_available;
				wchar_t str[512], str2[512];

				if (last_line > m_nPresets)
					last_line = m_nPresets;

				// tooltip:
				if (m_bShowMenuToolTips)
				{
					wchar_t buf[256];
					swprintf(buf, LoadResString(IDS_PAGE_X_OF_X), m_nMashPreset[m_nMashSlot]/lines_available+1, (m_nPresets+lines_available-1)/lines_available);
                    DrawTooltip(buf, xR, *lower_right_corner_y);
				}

                RECT orig_rect = rect;

                RECT box;
                box.top = rect.top;
                box.left = rect.left;
                box.right = rect.left;
                box.bottom = rect.top;

				int mashNames[MASH_SLOTS] = { IDS_MASHUP_GENERAL_POSTPROC,
											  IDS_MASHUP_MOTION_EQUATIONS,
                                              IDS_MASHUP_WAVEFORMS_SHAPES,
                                              IDS_MASHUP_WARP_SHADER,
											  IDS_MASHUP_COMP_SHADER,
				};


                for (int pass=0; pass<2; pass++)
                {
                    box = orig_rect;
                    int w = 0;
                    int h = 0;

                    int start_y = orig_rect.top;
                    for (mash=0; mash<MASH_SLOTS; mash++)
                    {
                        int idx = m_nMashPreset[mash];

                        wchar_t buf[1024];
                        swprintf(buf, L"%s%s", LoadResString(mashNames[mash]), m_presets[idx].szFilename);
                        RECT r2 = orig_rect;
                        r2.top += h;
                        h += m_text.DrawTextW(GetFont(SIMPLE_FONT), buf, -1, &r2, DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX | (pass==0 ? DT_CALCRECT : 0), (mash==m_nMashSlot) ? PLAYLIST_COLOR_HILITE_TRACK : PLAYLIST_COLOR_NORMAL, false);
                        w = max(w, r2.right - r2.left);
                    }
                    if (pass==0) {
                        box.right = box.left + w;
                        box.bottom = box.top + h;
                        DrawDarkTranslucentBox(&box);
                    }
                    else
                        orig_rect.top += h;
                }

                orig_rect.top += GetFontHeight(SIMPLE_FONT) + PLAYLIST_INNER_MARGIN;

                box = orig_rect;
                box.right = box.left;
                box.bottom = box.top;

                // draw a directory listing box right after...
                for (pass=0; pass<2; pass++)
                {

                    rect = orig_rect;
				    for (i=first_line; i<last_line; i++)
				    {
					    // remove the extension before displaying the filename.  also pad w/spaces.
					    //lstrcpy(str, m_pPresetAddr[i]);
					    bool bIsDir = (m_presets[i].szFilename.c_str()[0] == '*');
					    bool bIsRunning = false;
					    bool bIsSelected = (i == m_nMashPreset[m_nMashSlot]);

					    if (bIsDir)
					    {
						    // directory
						    if (wcscmp(m_presets[i].szFilename.c_str()+1, L"..")==0)
							    swprintf(str2, L" [ %s ] (%s) ", m_presets[i].szFilename.c_str()+1, LoadResString(IDS_PARENT_DIRECTORY));
						    else
							    swprintf(str2, L" [ %s ] ", m_presets[i].szFilename.c_str()+1);
					    }
					    else
					    {
						    // preset file
						    lstrcpyW(str, m_presets[i].szFilename.c_str());
						    RemoveExtension(str);
						    swprintf(str2, L" %s ", str);

						    if (wcscmp(m_presets[m_nMashPreset[m_nMashSlot]].szFilename.c_str(), str)==0)
							    bIsRunning = true;
					    }

					    if (bIsRunning && m_bPresetLockedByUser)
						    lstrcatW(str2, LoadResString(IDS_LOCKED));

                        DWORD color = bIsDir ? DIR_COLOR : PLAYLIST_COLOR_NORMAL;
                        if (bIsRunning)
                            color = bIsSelected ? PLAYLIST_COLOR_BOTH : PLAYLIST_COLOR_PLAYING_TRACK;
                        else if (bIsSelected)
                            color = PLAYLIST_COLOR_HILITE_TRACK;

                        RECT r2 = rect;
                        rect.top += m_text.DrawTextW(GetFont(SIMPLE_FONT),  str2, -1, &r2, DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX | (pass==0 ? DT_CALCRECT : 0), color, false);

                        if (pass==0)  // calculating dark box
                        {
                            box.right = max(box.right, box.left + r2.right-r2.left);
                            box.bottom += r2.bottom-r2.top;
                        }
				    }


                    if (pass==0)  // calculating dark box
                    {
                        box.top -= PLAYLIST_INNER_MARGIN;
                        box.left -= PLAYLIST_INNER_MARGIN;
                        box.right += PLAYLIST_INNER_MARGIN;
                        box.bottom += PLAYLIST_INNER_MARGIN;
                        DrawDarkTranslucentBox(&box);
                        *upper_left_corner_y = box.bottom + PLAYLIST_INNER_MARGIN;
                    }
                    else
                        orig_rect.top += box.bottom-box.top;
                }

                orig_rect.top += PLAYLIST_INNER_MARGIN;

            }
        }
		else if (m_UI_mode == UI_LOAD)
		{
			if (m_nPresets == 0)
			{
				// note: this error message is repeated in milkdrop.cpp in LoadRandomPreset()
				wchar_t buf[1024];
                swprintf(buf, LoadResString(IDS_ERROR_NO_PRESET_FILE_FOUND_IN_X_MILK), m_szPresetDir);
                AddError(buf, 6.0f, ERR_MISC, true);
				m_UI_mode = UI_REGULAR;
			}
			else
			{
                // SURI: the preset list as a card: folder name and key hints on top, the list with a
                // band under the selected row, and "page N of M" as the card's last line.
                LPD3DXFONT fTitle = GetUiFont(UIF_TITLE), fSmall = GetUiFont(UIF_SMALL);
                const int pad = UiPx(20), secGap = UiPx(10);
                const int x0 = xL + UiPx(18), y0 = *upper_left_corner_y + UiPx(18);
                int maxInnerW = (xR - UiPx(18)) - x0 - 2*pad;
                if (maxInnerW < UiPx(100)) maxInnerW = UiPx(100);

                wchar_t folder[MAX_PATH];
                lstrcpynW(folder, m_szPresetDir, MAX_PATH);
                size_t fl = wcslen(folder);
                while (fl > 0 && (folder[fl-1] == L'\\' || folder[fl-1] == L'/'))
                    folder[--fl] = 0;
                const wchar_t* folderName = wcsrchr(folder, L'\\');
                folderName = folderName ? folderName + 1 : folder;
                wchar_t hint[256];   // SURI: own buffers (LoadResString's one-argument form shares a static buffer)
                LoadResString(IDS_UI_BROWSER_HINT, hint, 256);
                wchar_t pageFmt[64];
                LoadResString(IDS_UI_PAGE_X_OF_Y, pageFmt, 64);

                RECT mr = { 0, 0, maxInnerW, 0 };
                const int titleH = UiMeasure(fTitle, folderName, &mr, DT_SINGLELINE);
                const int titleW = mr.right - mr.left;
                SetRect(&mr, 0, 0, maxInnerW, 0);
                const int hintH = UiMeasure(fSmall, hint, &mr, DT_SINGLELINE);
                const int hintW = mr.right - mr.left;
                SetRect(&mr, 0, 0, maxInnerW, 0);
                const int footH = UiMeasure(fSmall, L"Ag", &mr, DT_SINGLELINE);
                const int headH = titleH + UiPx(2) + hintH + secGap;
                const int rowH = GetFontHeight(SIMPLE_FONT);

                RECT rect;
                SetRect(&rect, x0 + pad, y0 + pad + headH, x0 + pad + maxInnerW, *lower_left_corner_y - UiPx(18) - pad - secGap - footH);

				int lines_available = (rect.bottom - rect.top) / rowH;

				if (lines_available < 1)
				{
					// force it
					rect.bottom = rect.top + rowH + 1;
					lines_available = 1;
				}
                if (lines_available > MAX_PRESETS_PER_PAGE)
                    lines_available = MAX_PRESETS_PER_PAGE;

				if (m_bUserPagedDown)
				{
					m_nPresetListCurPos += lines_available;
					if (m_nPresetListCurPos >= m_nPresets)
						m_nPresetListCurPos = m_nPresets - 1;

					// remember this preset's name so the next time they hit 'L' it jumps straight to it
					//lstrcpy(m_szLastPresetSelected, m_presets[m_nPresetListCurPos].szFilename.c_str());

					m_bUserPagedDown = false;
				}

				if (m_bUserPagedUp)
				{
					m_nPresetListCurPos -= lines_available;
					if (m_nPresetListCurPos < 0)
						m_nPresetListCurPos = 0;

					// remember this preset's name so the next time they hit 'L' it jumps straight to it
					//lstrcpy(m_szLastPresetSelected, m_presets[m_nPresetListCurPos].szFilename.c_str());

					m_bUserPagedUp = false;
				}

				int i;
				int first_line = m_nPresetListCurPos - (m_nPresetListCurPos % lines_available);
				int last_line  = first_line + lines_available;
				wchar_t str[512], str2[512];

				if (last_line > m_nPresets)
					last_line = m_nPresets;

                wchar_t pageStr[64];
                swprintf(pageStr, 64, pageFmt, m_nPresetListCurPos/lines_available+1, (m_nPresets+lines_available-1)/lines_available);
                SetRect(&mr, 0, 0, maxInnerW, 0);
                UiMeasure(fSmall, pageStr, &mr, DT_SINGLELINE);
                int innerW = max(max(titleW, hintW), (int)(mr.right - mr.left));

                for (int pass=0; pass<2; pass++)    // 0: measure the rows, 1: draw
                {
                    RECT card = { x0, y0, 0, 0 };
                    if (pass == 1)
                    {
                        innerW = min(innerW, maxInnerW);
                        card.right  = x0 + innerW + 2*pad;
                        card.bottom = y0 + pad + headH + (last_line - first_line)*rowH + secGap + footH + pad;
                        DrawRoundedPanel(&card, UiPx(14), CARD_FILL_PANEL, CARD_BORDER);

                        RECT tr = { x0 + pad, y0 + pad, x0 + pad + innerW, y0 + pad + titleH };
                        UiText(fTitle, folderName, &tr, DT_SINGLELINE, CARD_TEXT);
                        SetRect(&tr, x0 + pad, y0 + pad + titleH + UiPx(2), x0 + pad + innerW, y0 + pad + titleH + UiPx(2) + hintH);
                        UiText(fSmall, hint, &tr, DT_SINGLELINE, CARD_TEXT_2);
                        SetRect(&tr, x0 + pad, card.bottom - pad - footH, x0 + pad + innerW, card.bottom - pad);
                        UiText(fSmall, pageStr, &tr, DT_SINGLELINE, CARD_TEXT_2);
                        *upper_left_corner_y = card.bottom + UiPx(10);
                    }

				    for (i=first_line; i<last_line; i++)
				    {
					    // remove the extension before displaying the filename.  also pad w/spaces.
					    bool bIsDir = (m_presets[i].szFilename.c_str()[0] == '*');
					    bool bIsRunning = (i == m_nCurrentPreset);
					    bool bIsSelected = (i == m_nPresetListCurPos);

					    if (bIsDir)
					    {
						    // directory
						    if (wcscmp(m_presets[i].szFilename.c_str()+1, L"..")==0)
							    swprintf(str2, L" [ %s ] (%s) ", m_presets[i].szFilename.c_str()+1, LoadResString(IDS_PARENT_DIRECTORY));
						    else
							    swprintf(str2, L" [ %s ] ", m_presets[i].szFilename.c_str()+1);
					    }
					    else
					    {
						    // preset file
						    lstrcpyW(str, m_presets[i].szFilename.c_str());
						    RemoveExtension(str);
						    swprintf(str2, L" %s ", str);
					    }

					    if (bIsRunning && m_bPresetLockedByUser)
							lstrcatW(str2, LoadResString(IDS_LOCKED));

                        RECT r2;
                        SetRect(&r2, x0 + pad, rect.top + (i - first_line)*rowH, x0 + pad + innerW, rect.top + (i - first_line + 1)*rowH);
                        if (pass == 0)
                        {
                            r2.right = x0 + pad + maxInnerW;
                            m_text.DrawTextW(GetFont(SIMPLE_FONT), str2, -1, &r2, DT_SINGLELINE | DT_NOPREFIX | DT_CALCRECT, 0xFFFFFFFF, false);
                            innerW = max(innerW, (int)(r2.right - r2.left));
                        }
                        else
                        {
                            if (bIsSelected)
                            {
                                RECT band = { x0 + UiPx(8), r2.top, x0 + innerW + 2*pad - UiPx(8), r2.bottom };
                                DrawRoundedPanel(&band, UiPx(6), CARD_BAND, 0);
                            }
                            DWORD color = bIsRunning ? CARD_STAR : (bIsDir ? CARD_ACCENT : CARD_TEXT);
                            m_text.DrawTextW(GetFont(SIMPLE_FONT), str2, -1, &r2, DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX, color, false);
                        }
				    }
                }
			}
		}
    }

    // 5. render *remaining* text to upper-right corner
    {
		// e) SURI: messages (AddError) are toasts, drawn by MyRenderOverlay after all the queued
		//    text so that menus and the code editor can't cover them; just remember the layout.
		m_toast_top = toastTop;
		m_toast_xL = xL;
		m_toast_xR = xR;
		m_toast_avoid = infoCard;
    }
}

// SURI: toasts: pills at the top centre, newest first, at most 3, fading out over their last
// half second (design B). Called by CPluginShell::DrawAndDisplay after m_text.DrawNow().
void CPlugin::MyRenderOverlay()
{
		// SURI: the playlist screen sits above all queued text; the toasts go on top of it
		if (m_UI_mode == UI_PLAYLIST)
			DrawPlaylistScreen();
		else if (m_UI_mode == UI_IMPORT_NEW)
			DrawImportPrompt();

		LPD3DXFONT fToast = GetUiFont(UIF_TOAST);
		const int xL = m_toast_xL, xR = m_toast_xR;
		const RECT infoCard = m_toast_avoid;
		if (!m_bWarningsDisabled2 && fToast && xR > xL)
		{
            const float t = GetTime();
            std::vector<Toast> v = m_toasts.Visible(t);   // SURI: copies (the scan thread may push meanwhile)
            const int padX = UiPx(26), padY = UiPx(10), dot = UiPx(10), dotGap = UiPx(12), stackGap = UiPx(10);
            const int mid = (xL + xR) / 2;
            int maxTextW = (xR - xL) * 9 / 10 - 2*padX - dot - dotGap;
            if (maxTextW < UiPx(80)) maxTextW = UiPx(80);
            RECT lr = { 0, 0, 4096, 0 };
            const int lineH = UiMeasure(fToast, L"Ag", &lr, DT_SINGLELINE);
            int y = m_toast_top;
            for (size_t k = 0; k < v.size(); k++)
            {
                const Toast* p = &v[k];
                float left = p->startTime + p->duration - t;
                float a = (left < 0.5f) ? left / 0.5f : 1.0f;
                RECT tr = { 0, 0, maxTextW, 0 };
                int th = UiMeasure(fToast, p->text.c_str(), &tr, DT_WORDBREAK);
                int tw = tr.right - tr.left;
                if (th < lineH) th = lineH;
                const int pillW = tw + 2*padX + dot + dotGap, pillH = th + 2*padY;
                RECT pill = { mid - pillW/2, y, mid - pillW/2 + pillW, y + pillH };
                RECT hit;
                if (infoCard.right > infoCard.left && IntersectRect(&hit, &pill, &infoCard))
                {
                    // narrow window: go below the info card instead of over it
                    y = infoCard.bottom + stackGap;
                    OffsetRect(&pill, 0, y - pill.top);
                }
                const int radius = (th <= lineH) ? pillH : (lineH + 2*padY) / 2;
                DrawRoundedPanel(&pill, radius, FadeArgb(CARD_FILL_TOAST, a), FadeArgb(CARD_BORDER, a));
                RECT d = { pill.left + padX, y + padY + (lineH - dot)/2, pill.left + padX + dot, y + padY + (lineH - dot)/2 + dot };
                DrawRoundedPanel(&d, dot, FadeArgb(p->isError ? CARD_DOT_ERROR : CARD_DOT_NOTIFY, a), 0);
                RECT txt = { d.right + dotGap, y + padY, d.right + dotGap + tw + 2, y + padY + th };
                UiText(fToast, p->text.c_str(), &txt, DT_WORDBREAK, FadeArgb(CARD_TEXT, a));
                y += pillH + stackGap;
            }
		}
}

// SURI: preset name (22 px) over a line of stars, frame rate and "locked" (17 px, secondary
// colour) in a card in the upper-right corner; replaces the separate shadowed text lines.
void CPlugin::DrawInfoCard(RECT* pCard, int* upper_right_corner_y, int xL, int xR)
{
    SetRect(pCard, 0, 0, 0, 0);
    const bool showName   = m_bShowPresetInfo;
    const bool showRating = m_bShowRating || GetTime() < m_fShowRatingUntilThisTime;
    const bool showFps    = m_bShowFPS;
    const bool showLock   = (showName || showRating || showFps) && (m_bPresetLockedByUser || m_bPresetLockedByCode);   // SURI: with any of F4/F5/F6
    LPD3DXFONT fTitle = GetUiFont(UIF_TITLE), fMeta = GetUiFont(UIF_META), fStars = GetUiFont(UIF_STARS);
    if (!(showName || showRating || showFps) || !fTitle || !fMeta || !fStars)
        return;

    const int padX = UiPx(20), padY = UiPx(14), lineGap = UiPx(4), itemGap = UiPx(16);
    int maxInnerW = (xR - xL) * 7 / 10 - 2*padX;
    if (maxInnerW < UiPx(100)) maxInnerW = UiPx(100);

    struct Item { LPD3DXFONT f; std::wstring s; DWORD color; int gapBefore; int w, h; };
    std::vector<Item> items;
    auto add = [&](LPD3DXFONT f, const std::wstring& str, DWORD color, int gapBefore) {
        Item it = { f, str, color, items.empty() ? 0 : gapBefore, 0, 0 };
        items.push_back(it);
    };
    if (showRating)
    {
        int n = (int)(m_pState->m_fRating + 0.5f);
        if (n < 0) n = 0;
        if (n > 5) n = 5;
        if (n > 0) add(fStars, std::wstring(n, (wchar_t)0x2605), CARD_STAR, itemGap);
        if (n < 5) add(fStars, std::wstring(5 - n, (wchar_t)0x2605), (CARD_STAR & 0x00FFFFFF) | 0x50000000, n > 0 ? 0 : itemGap);
        if (!m_bEnableRating) add(fMeta, LoadResString(IDS_UI_RATING_OFF), CARD_TEXT_2, itemGap);
    }
    if (showFps)
    {
        wchar_t fmt[32], buf[32];
        LoadResString(IDS_UI_X_FPS, fmt, 32);
        swprintf(buf, 32, fmt, (int)(GetFps() + 0.5f));
        add(fMeta, buf, CARD_TEXT_2, itemGap);
    }
    if (showLock)
        add(fMeta, LoadResString(IDS_UI_LOCKED), CARD_TEXT_2, itemGap);

    std::wstring name;
    int nameW = 0, nameH = 0;
    if (showName)
    {
        name = (m_nLoadingPreset != 0) ? m_pNewState->m_szDesc : m_pState->m_szDesc;
        RECT r = { 0, 0, 4096, 0 };
        nameH = UiMeasure(fTitle, name.c_str(), &r, DT_SINGLELINE);
        nameW = r.right - r.left;
    }
    int metaW = 0, metaH = 0;
    for (Item& it : items)
    {
        RECT r = { 0, 0, 4096, 0 };
        it.h = UiMeasure(it.f, it.s.c_str(), &r, DT_SINGLELINE);
        it.w = r.right - r.left;
        metaW += it.gapBefore + it.w;
        if (it.h > metaH) metaH = it.h;
    }

    const int innerW = min(max(nameW, metaW), maxInnerW);
    const int innerH = nameH + (nameH && metaH ? lineGap : 0) + metaH;
    const int right = xR + UiPx(TEXT_MARGIN) - UiPx(28);   // SURI: 28 px in from the window edge (xR is the scaled margin in)
    const int top = *upper_right_corner_y - UiPx(TEXT_MARGIN) + UiPx(28);
    SetRect(pCard, right - innerW - 2*padX, top, right, top + innerH + 2*padY);
    DrawRoundedPanel(pCard, UiPx(14), CARD_FILL_INFO, CARD_BORDER);

    int x = pCard->left + padX, y = top + padY;
    if (nameH)
    {
        RECT r = { x, y, x + innerW, y + nameH };
        UiText(fTitle, name.c_str(), &r, DT_SINGLELINE, CARD_TEXT);
        y += nameH + lineGap;
    }
    for (const Item& it : items)
    {
        x += it.gapBefore;
        if (x >= pCard->left + padX + innerW)
            break;
        RECT r = { x, y + (metaH - it.h)/2, min(x + it.w + 2, (int)(pCard->left + padX + innerW)), y + (metaH - it.h)/2 + it.h };
        UiText(it.f, it.s.c_str(), &r, DT_SINGLELINE, it.color);
        x += it.w;
    }
    *upper_right_corner_y = pCard->bottom + UiPx(10);
}

// Always On Top (F7)
void ToggleAlwaysOnTop(HWND hwnd) {

	RECT rect;
	GetWindowRect(hwnd, &rect);
	int x = rect.left;
	int y = rect.top;
	int width = rect.right - rect.left;
	int height = rect.bottom - rect.top;

	if (m_bAlwaysOnTop) {
		SetWindowPos(hwnd, HWND_TOPMOST, x, y, width, height, SWP_DRAWFRAME | SWP_FRAMECHANGED);
	}
	else {
		SetWindowPos(hwnd, HWND_NOTOPMOST, x, y, width, height, SWP_DRAWFRAME | SWP_FRAMECHANGED);
	}
}

//----------------------------------------------------------------------

LRESULT CPlugin::MyWindowProc(HWND hWnd, unsigned uMsg, WPARAM wParam, LPARAM lParam)
{
    // You can handle Windows messages here while the plugin is running,
    //   such as mouse events (WM_MOUSEMOVE/WM_LBUTTONDOWN), keypresses
    //   (WK_KEYDOWN/WM_CHAR), and so on.
    // This function runs on the window thread,
    //   so you don't have to worry about using semaphores or critical
    //   sections to read/write your class member variables.
    // If you don't handle a message, let it continue on the usual path
    //   by returning DefWindowProc(hWnd,uMsg,wParam,lParam).
    // If you do handle a message, prevent it from being handled again
    //   by returning 0.

    // IMPORTANT: For the WM_KEYDOWN, WM_KEYUP, and WM_CHAR messages,
    //   you must return 0 if you process the message (key),
    //   and 1 if you do not.  DO NOT call DefWindowProc()
    //   for these particular messages!

    USHORT mask = 1 << (sizeof(SHORT)*8 - 1);
    bool bShiftHeldDown = (GetKeyState(VK_SHIFT) & mask) != 0;
    bool bCtrlHeldDown  = (GetKeyState(VK_CONTROL) & mask) != 0;

    int nRepeat = 1;  //updated as appropriate
    int rep;

    switch (uMsg)
    {
    case WM_COMMAND:

    case WM_CHAR:   // plain & simple alphanumeric keys
        nRepeat = LOWORD(lParam);
		if (uMsg == WM_CHAR && m_UI_mode == UI_PLAYLIST)
		{
			// SURI: the playlist screen takes every character; printable ones (also Space and P)
			// go to its search. Ctrl+letters arrive here as control codes and are dropped
			// (WM_KEYDOWN handled them); AltGr (Ctrl+Alt) still types.
			bool bAltHeldDown = (GetKeyState(VK_MENU) & mask) != 0;
			if (wParam >= 0x20 && (!bCtrlHeldDown || bAltHeldDown))
				OnPlaylistAction(m_plScreen.Char((wchar_t)wParam));
			return 0; // we processed (or absorbed) the key
		}
		if (uMsg == WM_CHAR && m_UI_mode == UI_IMPORT_NEW)
			return 0; // SURI: the "new presets" prompt only takes keys (WM_KEYDOWN); no preset keys under it
		if (m_waitstring.bActive)	// if user is in the middle of editing a string
		{
			if ((wParam >= ' ' && wParam <= 'z') || wParam=='{' || wParam=='}')
			{
				int len;
				if(m_waitstring.bDisplayAsCode)
					len = lstrlenA((char*)m_waitstring.szText);
				else
					len = lstrlenW(m_waitstring.szText);

				if (m_waitstring.bFilterBadChars &&
					(wParam == '\"' ||
					wParam == '\\' ||
					wParam == '/' ||
					wParam == ':' ||
					wParam == '*' ||
					wParam == '?' ||
					wParam == '|' ||
					wParam == '<' ||
					wParam == '>' ||
					wParam == '&'))	// NOTE: '&' is legal in filenames, but we try to avoid it since during GDI display it acts as a control code (it will not show up, but instead, underline the character following it).
				{
					// illegal char
                    AddError(LoadResString(IDS_ILLEGAL_CHARACTER), 2.5f, ERR_MISC, true);
				}
				else if (len+nRepeat >= m_waitstring.nMaxLen)
				{
					// m_waitstring.szText has reached its limit
                    AddError(LoadResString(IDS_STRING_TOO_LONG), 2.5f, ERR_MISC, true);
				}
				else
				{

					if(m_waitstring.bDisplayAsCode)
					{
						char buf[16];
						sprintf(buf, "%c", wParam);

						if (m_waitstring.nSelAnchorPos != -1)
							WaitString_NukeSelection();

						if (m_waitstring.bOvertypeMode)
						{
							// overtype mode
							for (rep=0; rep<nRepeat; rep++)
							{
								if (m_waitstring.nCursorPos == len)
								{
									lstrcatA((char*)m_waitstring.szText, buf);
									len++;
								}
								else
								{
									char* ptr = (char*)m_waitstring.szText;
									*(ptr + m_waitstring.nCursorPos) = buf[0];
								}
								m_waitstring.nCursorPos++;
							}
						}
						else
						{
							// insert mode:
							char* ptr = (char*)m_waitstring.szText;
							for (rep=0; rep<nRepeat; rep++)
							{
								for (int i=len; i>=m_waitstring.nCursorPos; i--)
									*(ptr + i+1) = *(ptr + i);
								*(ptr + m_waitstring.nCursorPos) = buf[0];
								m_waitstring.nCursorPos++;
								len++;
							}
						}
					}
					else
					{
						wchar_t buf[16];
						swprintf(buf, L"%c", wParam);

						if (m_waitstring.nSelAnchorPos != -1)
							WaitString_NukeSelection();

						if (m_waitstring.bOvertypeMode)
						{
							// overtype mode
							for (rep=0; rep<nRepeat; rep++)
							{
								if (m_waitstring.nCursorPos == len)
								{
									lstrcatW(m_waitstring.szText, buf);
									len++;
								}
								else
									m_waitstring.szText[m_waitstring.nCursorPos] = buf[0];
								m_waitstring.nCursorPos++;
							}
						}
						else
						{
							// insert mode:
							for (rep=0; rep<nRepeat; rep++)
							{
								for (int i=len; i>=m_waitstring.nCursorPos; i--)
									m_waitstring.szText[i+1] = m_waitstring.szText[i];
								m_waitstring.szText[m_waitstring.nCursorPos] = buf[0];
								m_waitstring.nCursorPos++;
								len++;
							}
						}
					}
				}
			}
            return 0; // we processed (or absorbed) the key
		}
		else if (m_UI_mode == UI_LOAD_DEL)	// waiting to confirm file delete
		{
			if (wParam == 'y' || wParam == 'Y')	// 'y' or 'Y'
			{
				// first add pathname to filename
				wchar_t szDelFile[512];
				swprintf(szDelFile, L"%s%s", GetPresetDir(), m_presets[m_nPresetListCurPos].szFilename.c_str());

				DeletePresetFile(szDelFile);
			}

			m_UI_mode = UI_LOAD;

            return 0; // we processed (or absorbed) the key
		}
		else if (m_UI_mode == UI_UPGRADE_PIXEL_SHADER)
		{
			if (wParam == 'y' || wParam == 'Y')	// 'y' or 'Y'
			{
                if (m_pState->m_nMinPSVersion == m_pState->m_nMaxPSVersion)
                {
                    switch(m_pState->m_nMinPSVersion)
                    {
                    case MD2_PS_NONE:
                        m_pState->m_nWarpPSVersion = MD2_PS_2_0;
                        m_pState->m_nCompPSVersion = MD2_PS_2_0;
                        m_pState->GenDefaultWarpShader();
                        m_pState->GenDefaultCompShader();
                        break;
                    case MD2_PS_2_0:
                        m_pState->m_nWarpPSVersion = MD2_PS_2_X;
                        m_pState->m_nCompPSVersion = MD2_PS_2_X;
                        break;
											case MD2_PS_2_X:
                        m_pState->m_nWarpPSVersion = MD2_PS_3_0;
                        m_pState->m_nCompPSVersion = MD2_PS_3_0;
                        break;
                    default:
                        assert(0);
                        break;
                    }
                }
                else
                {
                    switch(m_pState->m_nMinPSVersion)
                    {
                    case MD2_PS_NONE:
                        if (m_pState->m_nWarpPSVersion < MD2_PS_2_0)
                        {
                            m_pState->m_nWarpPSVersion = MD2_PS_2_0;
                            m_pState->GenDefaultWarpShader();
                        }
                        if (m_pState->m_nCompPSVersion < MD2_PS_2_0)
                        {
                            m_pState->m_nCompPSVersion = MD2_PS_2_0;
                            m_pState->GenDefaultCompShader();
                        }
                        break;
                    case MD2_PS_2_0:
                        m_pState->m_nWarpPSVersion = max(m_pState->m_nWarpPSVersion, MD2_PS_2_X);
                        m_pState->m_nCompPSVersion = max(m_pState->m_nCompPSVersion, MD2_PS_2_X);
												break;
										case MD2_PS_2_X:
											m_pState->m_nWarpPSVersion = max(m_pState->m_nWarpPSVersion, MD2_PS_3_0);
											m_pState->m_nCompPSVersion = max(m_pState->m_nCompPSVersion, MD2_PS_3_0);
											break;
										default:
                        assert(0);
                        break;
                    }
                }
                m_pState->m_nMinPSVersion = min(m_pState->m_nWarpPSVersion, m_pState->m_nCompPSVersion);
                m_pState->m_nMaxPSVersion = max(m_pState->m_nWarpPSVersion, m_pState->m_nCompPSVersion);

                LoadShaders(&m_shaders, m_pState, false);
                SetMenusForPresetVersion( m_pState->m_nWarpPSVersion, m_pState->m_nCompPSVersion );
			}
            if (wParam != 13)
			    m_UI_mode = UI_MENU;
            return 0; // we processed (or absorbed) the key
		}
		else if (m_UI_mode == UI_SAVE_OVERWRITE)	// waiting to confirm overwrite file on save
		{
			if (wParam == 'y' || wParam == 'Y')	// 'y' or 'Y'
			{
				// first add pathname + extension to filename
				wchar_t szNewFile[512];
				swprintf(szNewFile, L"%s%s.milk", GetPresetDir(), m_waitstring.szText);

				SavePresetAs(szNewFile);

				// exit waitstring mode
				m_UI_mode = UI_REGULAR;
				m_waitstring.bActive = false;
			}
			else if ((wParam >= ' ' && wParam <= 'z') || wParam == 27)		// 27 is the ESCAPE key
			{
				// go back to SAVE AS mode
				m_UI_mode = UI_SAVEAS;
				m_waitstring.bActive = true;
			}

            return 0; // we processed (or absorbed) the key
		}
		else	// normal handling of a simple key (all non-virtual-key hotkeys end up here)
		{
			if (HandleRegularKey(wParam)==0)
                return 0;
		}
        return 1; // end case WM_CHAR

    case WM_KEYDOWN:    // virtual-key codes
        // Note that some keys will never reach this point, since they are
        //   intercepted by the plugin shell (see PluginShellWindowProc(),
        //   at the end of pluginshell.cpp for which ones).
        // For a complete list of virtual-key codes, look up the keyphrase
        //   "virtual-key codes [win32]" in the msdn help.
        nRepeat = LOWORD(lParam);

		if (m_UI_mode == UI_PLAYLIST)
		{
			// SURI: the playlist screen takes every key while it is open, before the command
			// table: no preset keys (Space, Backspace, ...) and no F-keys. Esc closes it.
			UiKey k;
			if (TranslateUiKey(wParam, &k))
				OnPlaylistAction(m_plScreen.Key(k, m_plPageRows));
			return 0; // we processed (or absorbed) the key
		}

		if (m_UI_mode == UI_IMPORT_NEW)
		{
			// SURI: the "new presets" prompt of a drop takes every key while it is open, like the
			// playlist screen. Enter adds the checked ones, Esc skips all of them.
			UiKey k;
			if (TranslateUiKey(wParam, &k))
			{
				PromptResult r = m_importPrompt.Key(k, m_importPageRows);
				if (r != PromptResult::None)
					FinishImport(r);
			}
			return 0; // we processed (or absorbed) the key
		}

		{
			// SURI: F2-F9 go through the command table (they work even while editing text)
			Cmd cmd = LookupKey(KeyScope::VKeyGlobal, (unsigned)wParam);
			if (cmd != Cmd::None && DispatchCommand(cmd, hWnd))
				return 0; // we processed (or absorbed) the key
		}

		switch(wParam)
		{
        case VK_SCROLL:
            m_bPresetLockedByUser = GetKeyState(VK_SCROLL) & 1;
            //SetScrollLock(m_bPresetLockedByUser);
            //int set = m_bPresetLockedByUser ?

			return 0; // we processed (or absorbed) the key
		}

		// next handle the waitstring case (for string-editing),
		//	then the menu navigation case,
		//  then handle normal case (handle the message normally or pass it on)

		if (m_waitstring.bActive)
		{
			// handle arrow keys, home, end, etc.

			USHORT mask = 1 << (sizeof(SHORT)*8 - 1);	// we want the highest-order bit
			bool bShiftHeldDown = (GetKeyState(VK_SHIFT) & mask) != 0;
			bool bCtrlHeldDown = (GetKeyState(VK_CONTROL) & mask) != 0;

			if (wParam == VK_LEFT || wParam == VK_RIGHT ||
				wParam == VK_HOME || wParam == VK_END ||
				wParam == VK_UP || wParam == VK_DOWN)
			{
				if (bShiftHeldDown)
				{
					if (m_waitstring.nSelAnchorPos == -1)
						m_waitstring.nSelAnchorPos = m_waitstring.nCursorPos;
				}
				else
				{
					m_waitstring.nSelAnchorPos = -1;
				}
			}

			if (bCtrlHeldDown)		// copy/cut/paste
			{
				switch(wParam)
				{
				case 'c':
				case 'C':
				case VK_INSERT:
					WaitString_Copy();
					return 0; // we processed (or absorbed) the key
				case 'x':
				case 'X':
					WaitString_Cut();
					return 0; // we processed (or absorbed) the key
				case 'v':
				case 'V':
					WaitString_Paste();
					return 0; // we processed (or absorbed) the key
				case VK_LEFT:	WaitString_SeekLeftWord();	return 0; // we processed (or absorbed) the key
				case VK_RIGHT:	WaitString_SeekRightWord();	return 0; // we processed (or absorbed) the key
				case VK_HOME:	m_waitstring.nCursorPos = 0;	return 0; // we processed (or absorbed) the key
				case VK_END:
					if (m_waitstring.bDisplayAsCode)
					{
						m_waitstring.nCursorPos = lstrlenA((char*)m_waitstring.szText);
					}
					else
					{
						m_waitstring.nCursorPos = lstrlenW(m_waitstring.szText);
					}
					return 0; // we processed (or absorbed) the key
				case VK_RETURN:
					if (m_waitstring.bDisplayAsCode)
					{
						// CTRL+ENTER accepts the string -> finished editing
						//assert(m_pCurMenu);
						m_pCurMenu->OnWaitStringAccept(m_waitstring.szText);
							// OnWaitStringAccept calls the callback function.  See the
							// calls to CMenu::AddItem from milkdrop.cpp to find the
							// callback functions for different "waitstrings".
						m_waitstring.bActive = false;
						m_UI_mode = UI_MENU;
					}
					return 0; // we processed (or absorbed) the key
				}
			}
			else	// waitstring mode key pressed, and ctrl NOT held down
			{
				switch(wParam)
				{
				case VK_INSERT:
					m_waitstring.bOvertypeMode = !m_waitstring.bOvertypeMode;
					return 0; // we processed (or absorbed) the key

				case VK_LEFT:
                    for (rep=0; rep<nRepeat; rep++)
					    if (m_waitstring.nCursorPos > 0)
						    m_waitstring.nCursorPos--;
					return 0; // we processed (or absorbed) the key

				case VK_RIGHT:
                    for (rep=0; rep<nRepeat; rep++)
					{
						if (m_waitstring.bDisplayAsCode)
						{
							if (m_waitstring.nCursorPos < (int)lstrlenA((char*)m_waitstring.szText))
								m_waitstring.nCursorPos++;
						}
						else
						{
							if (m_waitstring.nCursorPos < (int)lstrlenW(m_waitstring.szText))
								m_waitstring.nCursorPos++;
						}
					}
					return 0; // we processed (or absorbed) the key

				case VK_HOME:
					m_waitstring.nCursorPos -= WaitString_GetCursorColumn();
					return 0; // we processed (or absorbed) the key

				case VK_END:
					m_waitstring.nCursorPos += WaitString_GetLineLength() - WaitString_GetCursorColumn();
					return 0; // we processed (or absorbed) the key

				case VK_UP:
                    for (rep=0; rep<nRepeat; rep++)
	    				WaitString_SeekUpOneLine();
					return 0; // we processed (or absorbed) the key

				case VK_DOWN:
                    for (rep=0; rep<nRepeat; rep++)
    					WaitString_SeekDownOneLine();
  					return 0; // we processed (or absorbed) the key

				case VK_BACK:
					if (m_waitstring.nSelAnchorPos != -1)
					{
						WaitString_NukeSelection();
					}
					else if (m_waitstring.nCursorPos > 0)
					{
						int len;
						if (m_waitstring.bDisplayAsCode)
						{
							len = lstrlenA((char*)m_waitstring.szText);
						}
						else
						{
							len = lstrlenW(m_waitstring.szText);
						}
                        int src_pos = m_waitstring.nCursorPos;
                        int dst_pos = m_waitstring.nCursorPos - nRepeat;
                        int gap = nRepeat;
                        int copy_chars = len - m_waitstring.nCursorPos + 1;  // includes NULL @ end
                        if (dst_pos < 0)
                        {
                            gap += dst_pos;
                            dst_pos = 0;
                        }

						if (m_waitstring.bDisplayAsCode)
						{
							char* ptr = (char*)m_waitstring.szText;
							for (int i=0; i<copy_chars; i++)
								*(ptr + dst_pos+i) = *(ptr + src_pos+i);
						}
						else
						{
							for (int i=0; i<copy_chars; i++)
								m_waitstring.szText[dst_pos+i] = m_waitstring.szText[src_pos+i];
						}
						m_waitstring.nCursorPos -= gap;
					}
					return 0; // we processed (or absorbed) the key

				case VK_DELETE:
					if (m_waitstring.nSelAnchorPos != -1)
					{
						WaitString_NukeSelection();
					}
					else
					{
						if (m_waitstring.bDisplayAsCode)
						{
							int len = lstrlenA((char*)m_waitstring.szText);
							char* ptr = (char*)m_waitstring.szText;
							for (int i=m_waitstring.nCursorPos; i<=len - nRepeat; i++)
								*(ptr + i) = *(ptr + i+nRepeat);
						}
						else
						{
							int len = lstrlenW(m_waitstring.szText);
							for (int i=m_waitstring.nCursorPos; i<=len - nRepeat; i++)
								m_waitstring.szText[i] = m_waitstring.szText[i+nRepeat];
						}
					}
					return 0; // we processed (or absorbed) the key

				case VK_RETURN:
					if (m_UI_mode == UI_LOAD_RENAME)	// rename (move) the file
					{
						// first add pathnames to filenames
						wchar_t szOldFile[512];
						wchar_t szNewFile[512];
						lstrcpyW(szOldFile, GetPresetDir());
						lstrcpyW(szNewFile, GetPresetDir());
						lstrcatW(szOldFile, m_presets[m_nPresetListCurPos].szFilename.c_str());
						lstrcatW(szNewFile, m_waitstring.szText);
						lstrcatW(szNewFile, L".milk");

						RenamePresetFile(szOldFile, szNewFile);
					}
                    else if (m_UI_mode == UI_IMPORT_WAVE ||
                             m_UI_mode == UI_EXPORT_WAVE ||
                             m_UI_mode == UI_IMPORT_SHAPE ||
                             m_UI_mode == UI_EXPORT_SHAPE)
                    {
                        int bWave   = (m_UI_mode == UI_IMPORT_WAVE || m_UI_mode == UI_EXPORT_WAVE);
                        int bImport = (m_UI_mode == UI_IMPORT_WAVE || m_UI_mode == UI_IMPORT_SHAPE);

                        int i = m_pCurMenu->GetCurItem()->m_lParam;
                        int ret;
                        switch(m_UI_mode)
                        {
                        case UI_IMPORT_WAVE : ret = m_pState->m_wave[i].Import(NULL, m_waitstring.szText, 0); break;
                        case UI_EXPORT_WAVE : ret = m_pState->m_wave[i].Export(NULL, m_waitstring.szText, 0); break;
                        case UI_IMPORT_SHAPE: ret = m_pState->m_shape[i].Import(NULL, m_waitstring.szText, 0); break;
                        case UI_EXPORT_SHAPE: ret = m_pState->m_shape[i].Export(NULL, m_waitstring.szText, 0); break;
                        }

                        if (bImport)
                            m_pState->RecompileExpressions(1);

                        if (!ret)
                        {
                            wchar_t buf[1024];
                            if (m_UI_mode==UI_IMPORT_WAVE || m_UI_mode==UI_IMPORT_SHAPE)
								LoadResString(IDS_ERROR_IMPORTING_BAD_FILENAME, buf, 1024);
                            else
								LoadResString(IDS_ERROR_IMPORTING_BAD_FILENAME_OR_NOT_OVERWRITEABLE, buf, 1024);
                            AddError(LoadResString(IDS_STRING_TOO_LONG), 2.5f, ERR_MISC, true);
                        }

						m_waitstring.bActive = false;
                        m_UI_mode = UI_MENU;
                    }
					else if (m_UI_mode == UI_SAVEAS)
					{
						// first add pathname + extension to filename
						wchar_t szNewFile[512];
						swprintf(szNewFile, L"%s%s.milk", GetPresetDir(), m_waitstring.szText);

						if (GetFileAttributesW(szNewFile) != -1)		// check if file already exists
						{
							// file already exists -> overwrite it?
							m_waitstring.bActive = false;
							m_UI_mode = UI_SAVE_OVERWRITE;
						}
						else
						{
							SavePresetAs(szNewFile);

							// exit waitstring mode
							m_UI_mode = UI_REGULAR;
							m_waitstring.bActive = false;
						}
					}
					else if (m_UI_mode == UI_EDIT_MENU_STRING)
					{
						if (m_waitstring.bDisplayAsCode)
						{
							if (m_waitstring.nSelAnchorPos != -1)
								WaitString_NukeSelection();

							int len = lstrlenA((char*)m_waitstring.szText);
							char* ptr = (char*)m_waitstring.szText;
							if (len + 1 < m_waitstring.nMaxLen)
							{
								// insert a linefeed.  Use CTRL+return to accept changes in this case.
								for (int pos=len+1; pos > m_waitstring.nCursorPos; pos--)
									*(ptr + pos) = *(ptr + pos - 1);
								*(ptr + m_waitstring.nCursorPos++) = LINEFEED_CONTROL_CHAR;

							}
							else
							{
								// m_waitstring.szText has reached its limit
                                AddError(LoadResString(IDS_STRING_TOO_LONG), 2.5f, ERR_MISC, true);
							}
						}
						else
						{
							// finished editing
							//assert(m_pCurMenu);
							m_pCurMenu->OnWaitStringAccept(m_waitstring.szText);
								// OnWaitStringAccept calls the callback function.  See the
								// calls to CMenu::AddItem from milkdrop.cpp to find the
								// callback functions for different "waitstrings".
							m_waitstring.bActive = false;
							m_UI_mode = UI_MENU;
						}
					}
					else if (m_UI_mode == UI_CHANGEDIR)
					{

						// change dir
						wchar_t szOldDir[512];
						wchar_t szNewDir[512];
						lstrcpyW(szOldDir, g_plugin.m_szPresetDir);
						lstrcpyW(szNewDir, m_waitstring.szText);

						int len = lstrlenW(szNewDir);
						if (len > 0 && szNewDir[len-1] != L'\\')
							lstrcatW(szNewDir, L"\\");

						lstrcpyW(g_plugin.m_szPresetDir, szNewDir);

                        bool bSuccess = true;
                        if (GetFileAttributesW(g_plugin.m_szPresetDir) == -1)
                            bSuccess = false;
                        if (bSuccess) {
    						UpdatePresetList(false,true,false);
                            bSuccess = (m_nPresets > 0);
                        }

                        if (!bSuccess)
						{
							// new dir. was invalid -> allow them to try again
							lstrcpyW(g_plugin.m_szPresetDir, szOldDir);

							// give them a warning
                            AddError(LoadResString(IDS_INVALID_PATH), 3.5f, ERR_MISC, true);
						}
						else
						{
							// success
							lstrcpyW(g_plugin.m_szPresetDir, szNewDir);
							m_playlist.SetAll();	// SURI: the playlist named files of the old library

							// save new path to registry
							WritePrivateProfileStringW(L"settings",L"szPresetDir",g_plugin.m_szPresetDir,GetConfigIniFile());

							// set current preset index to -1 because current preset is no longer in the list
							m_nCurrentPreset = -1;

							// go to file load menu
							m_waitstring.bActive = false;
							m_UI_mode = UI_LOAD;

                            ClearErrors(ERR_MISC);
                        }
					}
					return 0; // we processed (or absorbed) the key

				case VK_ESCAPE:
					if (m_UI_mode == UI_LOAD_RENAME)
					{
						m_waitstring.bActive = false;
						m_UI_mode = UI_LOAD;
					}
					else if (
                        m_UI_mode == UI_SAVEAS ||
                        m_UI_mode == UI_SAVE_OVERWRITE ||
                        m_UI_mode == UI_EXPORT_SHAPE ||
                        m_UI_mode == UI_IMPORT_SHAPE ||
                        m_UI_mode == UI_EXPORT_WAVE ||
                        m_UI_mode == UI_IMPORT_WAVE)
					{
						m_waitstring.bActive = false;
						m_UI_mode = UI_REGULAR;
					}
                    else if (m_UI_mode == UI_EDIT_MENU_STRING)
                    {
						m_waitstring.bActive = false;
                        if (m_waitstring.bDisplayAsCode)    // if were editing code...
    						m_UI_mode = UI_MENU;    // return to menu
                        else
    						m_UI_mode = UI_REGULAR; // otherwise don't (we might have been editing a filename, for example)
                    }
                    else /*if (m_UI_mode == UI_EDIT_MENU_STRING || m_UI_mode == UI_CHANGEDIR || 1)*/
					{
						m_waitstring.bActive = false;
						m_UI_mode = UI_REGULAR;
					}
					return 0; // we processed (or absorbed) the key
				}
			}

			// don't let keys go anywhere else
			return 0; // we processed (or absorbed) the key
		}

		// case 2: menu is up & gets the keyboard input
		if (m_UI_mode == UI_MENU)
		{
			if (m_pCurMenu->HandleKeydown(hWnd, uMsg, wParam, lParam) == 0)
				return 0; // we processed (or absorbed) the key
		}

		// case 3: handle non-character keys (virtual keys) and return 0.
        //         if we don't handle them, return 1, and the shell will
        //         (passing some to the shell's key bindings and the rest to DefWindowProc)
		//		note: regular hotkeys should be handled in HandleRegularKey.
		{
			// SURI: Space, Backspace and F1 go through the command table
			Cmd cmd = LookupKey(KeyScope::VKeyNormal, (unsigned)wParam);
			if (cmd != Cmd::None && DispatchCommand(cmd, hWnd))
				return 0; // we processed (or absorbed) the key
		}

		switch(wParam)
		{
		case VK_LEFT:
		case VK_RIGHT:
			if (m_UI_mode == UI_LOAD)
			{
				// it's annoying when the music skips if you hit the left arrow from the Load menu, so instead, we exit the menu
				if (wParam == VK_LEFT) m_UI_mode = UI_REGULAR;
				return 0; // we processed (or absorbed) the key
			}
            else if (m_UI_mode == UI_UPGRADE_PIXEL_SHADER)
            {
				m_UI_mode = UI_MENU;
				return 0; // we processed (or absorbed) the key
            }
            else if (m_UI_mode == UI_MASHUP)
            {
                if (wParam==VK_LEFT)
                    m_nMashSlot = max(0, m_nMashSlot-1);
                else
                    m_nMashSlot = min(MASH_SLOTS-1, m_nMashSlot+1);
				return 0; // we processed (or absorbed) the key
            }
            break;

		case VK_ESCAPE:
			if (m_UI_mode == UI_LOAD || m_UI_mode == UI_MENU || m_UI_mode == UI_MASHUP)
			{
				m_UI_mode = UI_REGULAR;
				return 0; // we processed (or absorbed) the key
			}
			else if (m_UI_mode == UI_LOAD_DEL)
			{
				m_UI_mode = UI_LOAD;
				return 0; // we processed (or absorbed) the key
			}
            else if (m_UI_mode == UI_UPGRADE_PIXEL_SHADER)
            {
				m_UI_mode = UI_MENU;
				return 0; // we processed (or absorbed) the key
            }
			else if (m_UI_mode == UI_SAVE_OVERWRITE)
			{
				m_UI_mode = UI_SAVEAS;
				// return to waitstring mode, leaving all the parameters as they were before:
				m_waitstring.bActive = true;
				return 0; // we processed (or absorbed) the key
			}
			break;

		case VK_UP:
            if (m_UI_mode == UI_MASHUP)
            {
                for (rep=0; rep<nRepeat; rep++)
                    m_nMashPreset[m_nMashSlot] = max(m_nMashPreset[m_nMashSlot]-1, m_nDirs);
                m_nLastMashChangeFrame[m_nMashSlot] = GetFrame();  // causes delayed apply
				return 0; // we processed (or absorbed) the key
            }
            else if (m_UI_mode == UI_LOAD)
			{
                for (rep=0; rep<nRepeat; rep++)
				    if (m_nPresetListCurPos > 0)
					    m_nPresetListCurPos--;
				return 0; // we processed (or absorbed) the key

				// remember this preset's name so the next time they hit 'L' it jumps straight to it
				//lstrcpy(m_szLastPresetSelected, m_presets[m_nPresetListCurPos].szFilename.c_str());
			}
			break;

		case VK_DOWN:
            if (m_UI_mode == UI_MASHUP)
            {
                for (rep=0; rep<nRepeat; rep++)
                    m_nMashPreset[m_nMashSlot] = min(m_nMashPreset[m_nMashSlot]+1, m_nPresets-1);
                m_nLastMashChangeFrame[m_nMashSlot] = GetFrame();  // causes delayed apply
				return 0; // we processed (or absorbed) the key
            }
			else if (m_UI_mode == UI_LOAD)
			{
                for (rep=0; rep<nRepeat; rep++)
    				if (m_nPresetListCurPos < m_nPresets - 1)
	    				m_nPresetListCurPos++;
				return 0; // we processed (or absorbed) the key

				// remember this preset's name so the next time they hit 'L' it jumps straight to it
				//lstrcpy(m_szLastPresetSelected, m_presets[m_nPresetListCurPos].szFilename.c_str());
			}
			break;

		case VK_SPACE:	// (random preset is Cmd::PresetRandom; this is only the Load menu case)
			if (m_UI_mode == UI_LOAD)
                goto HitEnterFromLoadMenu;
			break;

		case VK_PRIOR:
			if (m_show_help) { HelpPageStep(-1); return 0; }   // SURI: help pages
			if (m_UI_mode == UI_LOAD || m_UI_mode == UI_MASHUP)
            {
				m_bUserPagedUp = true;
                if (m_UI_mode == UI_MASHUP)
                    m_nLastMashChangeFrame[m_nMashSlot] = GetFrame();  // causes delayed apply
				return 0; // we processed (or absorbed) the key
            }
			break;
		case VK_NEXT:
			if (m_show_help) { HelpPageStep(+1); return 0; }   // SURI: help pages
			if (m_UI_mode == UI_LOAD || m_UI_mode == UI_MASHUP)
            {
				m_bUserPagedDown = true;
                if (m_UI_mode == UI_MASHUP)
                    m_nLastMashChangeFrame[m_nMashSlot] = GetFrame();  // causes delayed apply
				return 0; // we processed (or absorbed) the key
            }
			break;
		case VK_HOME:
			if (m_UI_mode == UI_LOAD)
            {
				m_nPresetListCurPos = 0;
				return 0; // we processed (or absorbed) the key
            }
            else if (m_UI_mode == UI_MASHUP)
            {
                m_nMashPreset[m_nMashSlot] = m_nDirs;
                m_nLastMashChangeFrame[m_nMashSlot] = GetFrame();  // causes delayed apply
				return 0; // we processed (or absorbed) the key
            }
            break;
		case VK_END:
			if (m_UI_mode == UI_LOAD)
            {
				m_nPresetListCurPos = m_nPresets - 1;
				return 0; // we processed (or absorbed) the key
            }
            else if (m_UI_mode == UI_MASHUP)
            {
                m_nMashPreset[m_nMashSlot] = m_nPresets-1;
                m_nLastMashChangeFrame[m_nMashSlot] = GetFrame();  // causes delayed apply
				return 0; // we processed (or absorbed) the key
            }
			break;

		case VK_DELETE:
			if (m_UI_mode == UI_LOAD)
			{
				if (m_presets[m_nPresetListCurPos].szFilename.c_str()[0] != '*')	// can't delete directories
					m_UI_mode = UI_LOAD_DEL;
				return 0; // we processed (or absorbed) the key
			}
			else //if (m_nNumericInputDigits == 0)
			{
				if (m_nNumericInputMode == NUMERIC_INPUT_MODE_CUST_MSG)
				{
				    m_nNumericInputDigits = 0;
				    m_nNumericInputNum = 0;

					// stop display of text message.
					m_supertext.fStartTime = -1.0f;
    				return 0; // we processed (or absorbed) the key
				}
				else if (m_nNumericInputMode == NUMERIC_INPUT_MODE_SPRITE)
				{
					// kill newest sprite (regular DELETE key)
					// oldest sprite (SHIFT + DELETE),
					// or all sprites (CTRL + SHIFT + DELETE).

                    m_nNumericInputDigits = 0;
				    m_nNumericInputNum = 0;

					USHORT mask = 1 << (sizeof(SHORT)*8 - 1);	// we want the highest-order bit
					bool bShiftHeldDown = (GetKeyState(VK_SHIFT) & mask) != 0;
					bool bCtrlHeldDown = (GetKeyState(VK_CONTROL) & mask) != 0;

					if (bShiftHeldDown && bCtrlHeldDown)
					{
						for (int x=0; x<NUM_TEX; x++)
							KillSprite(x);
					}
					else
					{
						int newest = -1;
						int frame;
						for (int x=0; x<NUM_TEX; x++)
						{
							if (m_texmgr.m_tex[x].pSurface)
							{
								if ((newest == -1) ||
									(!bShiftHeldDown && m_texmgr.m_tex[x].nStartFrame > frame) ||
									(bShiftHeldDown && m_texmgr.m_tex[x].nStartFrame < frame))
								{
									newest = x;
									frame = m_texmgr.m_tex[x].nStartFrame;
								}
							}
						}

						if (newest != -1)
							KillSprite(newest);
					}
					return 0; // we processed (or absorbed) the key
    			}
			}
			break;

		case VK_INSERT:		// RENAME
			if (m_UI_mode == UI_LOAD)
			{
				if (m_presets[m_nPresetListCurPos].szFilename.c_str()[0] != '*')	// can't rename directories
				{
					// go into RENAME mode
					m_UI_mode = UI_LOAD_RENAME;
					m_waitstring.bActive = true;
					m_waitstring.bFilterBadChars = true;
					m_waitstring.bDisplayAsCode = false;
					m_waitstring.nSelAnchorPos = -1;
					m_waitstring.nMaxLen = min(sizeof(m_waitstring.szText)-1, MAX_PATH - lstrlenW(GetPresetDir()) - 6);	// 6 for the extension + null char.  We set this because win32 LoadFile, MoveFile, etc. barf if the path+filename+ext are > MAX_PATH chars.

					// initial string is the filename, minus the extension
					lstrcpyW(m_waitstring.szText, m_presets[m_nPresetListCurPos].szFilename.c_str());
					RemoveExtension(m_waitstring.szText);

					// set the prompt & 'tooltip'
					swprintf(m_waitstring.szPrompt, LoadResString(IDS_ENTER_THE_NEW_NAME_FOR_X), m_waitstring.szText);
					m_waitstring.szToolTip[0] = 0;

					// set the starting edit position
					m_waitstring.nCursorPos = lstrlenW(m_waitstring.szText);
				}
				return 0; // we processed (or absorbed) the key
			}
			break;

		case VK_RETURN:

            if (m_UI_mode == UI_MASHUP)
            {
                m_nLastMashChangeFrame[m_nMashSlot] = GetFrame() + MASH_APPLY_DELAY_FRAMES;  // causes instant apply
				return 0; // we processed (or absorbed) the key
            }
			else if (m_UI_mode == UI_LOAD)
			{
                HitEnterFromLoadMenu:

				if (m_presets[m_nPresetListCurPos].szFilename.c_str()[0] == '*')
				{
					// CHANGE DIRECTORY
					wchar_t *p = GetPresetDir();

					if (wcscmp(m_presets[m_nPresetListCurPos].szFilename.c_str(), L"*..") == 0)
					{
						// back up one dir
						wchar_t *p2 = wcsrchr(p, L'\\');
						if (p2)
						{
							*p2 = 0;
							p2 = wcsrchr(p, L'\\');
							if (p2)	*(p2+1) = 0;
						}
					}
					else
					{
						// open subdir
						lstrcatW(p, &m_presets[m_nPresetListCurPos].szFilename.c_str()[1]);
						lstrcatW(p, L"\\");
					}

					WritePrivateProfileStringW(L"settings",L"szPresetDir",GetPresetDir(),GetConfigIniFile());
					m_playlist.SetAll();	// SURI: the playlist named files of the old library

					UpdatePresetList(false, true, false);

					// set current preset index to -1 because current preset is no longer in the list
					m_nCurrentPreset = -1;
				}
				else
				{
					// LOAD NEW PRESET
					m_nCurrentPreset = m_nPresetListCurPos;

					// first take the filename and prepend the path.  (already has extension)
					wchar_t s[MAX_PATH];
					lstrcpyW(s, GetPresetDir());	// note: m_szPresetDir always ends with '\'
					lstrcatW(s, m_presets[m_nCurrentPreset].szFilename.c_str());

					// now load (and blend to) the new preset
                    m_presetHistoryPos = (m_presetHistoryPos+1) % PRESET_HIST_LEN;
                    LoadPreset(s, (wParam==VK_SPACE) ? m_fBlendTimeUser : 0);
				}
				return 0; // we processed (or absorbed) the key
			}
			break;

        case 'T':
            if (bCtrlHeldDown)
            {
    			// stop display of custom message or song title.
			    m_supertext.fStartTime = -1.0f;
                return 0;
            }
            break;
        case 'K':
            if (bCtrlHeldDown)      // kill all sprites
            {
		        for (int x=0; x<NUM_TEX; x++)
			        if (m_texmgr.m_tex[x].pSurface)
                        KillSprite(x);
                return 0;
            }
            break;
		}
		if (wParam == keyMappings[2])	// 'Y'
		{
            if (bCtrlHeldDown)      // stop display of custom message or song title.
            {
			    m_supertext.fStartTime = -1.0f;
                return 0;
            }
		}
        return 1; // end case WM_KEYDOWN

	case WM_KEYUP:
    	return 1;
		break;

	// SURI: drag / wheel on sprites; anything else keeps the old path
	case WM_LBUTTONDOWN:
	case WM_MOUSEMOVE:
	case WM_LBUTTONUP:
	case WM_CAPTURECHANGED:
	case WM_MOUSEWHEEL:
		if (SpriteMouseMessage(hWnd, uMsg, wParam, lParam))
			return 0;
		return DefWindowProcW(hWnd, uMsg, wParam, lParam);

    default:
        return DefWindowProcW(hWnd, uMsg, wParam, lParam);
        break;
    }

    return 0;
};

//----------------------------------------------------------------------

// SURI: runs one command from the command table (keyboard or right-click menu).
// Returns false when the command does not apply in the current UI mode, so the
// caller can continue with the old key handling (e.g. Space in the Load menu).
bool CPlugin::DispatchCommand(Cmd c, HWND hWnd)
{
	switch (c)
	{
	case Cmd::PresetNext:
		if (m_UI_mode == UI_MASHUP)
			return false;	// h/H pick mash-up presets there
		NextPreset(0);
		m_fHardCutThresh *= 2.0f;  // make it a little less likely that a random hard cut follows soon.
		return true;

	case Cmd::PresetPrev:
		PrevPreset(0);
		m_fHardCutThresh *= 2.0f;
		return true;

	case Cmd::PresetRandom:
		if (m_UI_mode == UI_LOAD || m_bPresetLockedByCode)
			return false;	// Space loads the selected preset in the Load menu
		LoadRandomPreset(m_fBlendTimeUser);
		return true;

	case Cmd::PresetLockToggle:
		m_bPresetLockedByUser = !m_bPresetLockedByUser;
		return true;

	case Cmd::PresetOrderToggle:
		m_bSequentialPresetOrder = !m_bSequentialPresetOrder;
        {
            wchar_t buf[1024], tmp[64];
            swprintf(buf, LoadResString(IDS_PRESET_ORDER_IS_NOW_X),
					LoadResString((m_bSequentialPresetOrder) ? IDS_SEQUENTIAL : IDS_RANDOM, tmp, 64));
            AddError(buf, 3.0f, ERR_NOTIFY, false);
        }

        // erase all history, too:
        m_presetHistory[0] = m_szCurrentPresetFile;
        m_presetHistoryPos = 0;
        m_presetHistoryFwdFence = 1;
        m_presetHistoryBackFence = 0;
		return true;

	case Cmd::PresetLoadMenu:
		if (m_UI_mode == UI_LOAD)
		{
			m_UI_mode = UI_REGULAR;
			return true;
		}
		if (m_UI_mode == UI_REGULAR || m_UI_mode == UI_MENU)
		{
            UpdatePresetList(); // make sure list is completely ready
			m_UI_mode = UI_LOAD;
			m_show_help = 0;   // SURI: so PgUp / PgDn page the list, not the help
			m_bUserPagedUp = false;
			m_bUserPagedDown = false;
			return true;
		}
		return false;

	case Cmd::PresetSave:
		if (m_UI_mode != UI_REGULAR)
			return false;
		m_UI_mode = UI_SAVEAS;

		// enter WaitString mode
		m_waitstring.bActive = true;
		m_waitstring.bFilterBadChars = true;
		m_waitstring.bDisplayAsCode = false;
		m_waitstring.nSelAnchorPos = -1;
		m_waitstring.nMaxLen = min(sizeof(m_waitstring.szText)-1, MAX_PATH - lstrlenW(GetPresetDir()) - 6);	// 6 for the extension + null char.    We set this because win32 LoadFile, MoveFile, etc. barf if the path+filename+ext are > MAX_PATH chars.
		lstrcpyW(m_waitstring.szText, m_pState->m_szDesc);			// initial string is the filename, minus the extension
		LoadResString(IDS_SAVE_AS,m_waitstring.szPrompt,512);
		m_waitstring.szToolTip[0] = 0;
		m_waitstring.nCursorPos = lstrlenW(m_waitstring.szText);	// set the starting edit position
		return true;

	case Cmd::PresetDelete:
		{
			AppSnapshot s = GetAppSnapshot();
			if (!s.canDeleteCurrent || s.textEditActive)
				return false;
		}
		// reuse the Load menu's on-screen "delete? y/n" prompt, pointed at the running preset
		UpdatePresetList();
		m_nPresetListCurPos = m_nCurrentPreset;
		m_UI_mode = UI_LOAD_DEL;
		return true;

	case Cmd::PresetEditMenu:
		if (m_UI_mode == UI_MENU)
			m_UI_mode = UI_REGULAR;
		else if (m_UI_mode == UI_REGULAR || m_UI_mode == UI_LOAD)
			m_UI_mode = UI_MENU;
		return true;

	case Cmd::PresetRateUp:
		SetCurrentPresetRating(m_pState->m_fRating + 1.0f);
		return true;

	case Cmd::PlaylistScreen:	// SURI
		if (m_UI_mode == UI_PLAYLIST)
		{
			ClosePlaylistScreen();
			return true;
		}
		if (m_UI_mode == UI_REGULAR || m_UI_mode == UI_MENU || m_UI_mode == UI_LOAD)
		{
			OpenPlaylistScreen();
			return true;
		}
		return false;

	case Cmd::PresetRateDown:
		SetCurrentPresetRating(m_pState->m_fRating - 1.0f);
		return true;

	case Cmd::ShaderLockCycle:
        if (!m_bCompShaderLock && !m_bWarpShaderLock) {
            m_bCompShaderLock = true; m_bWarpShaderLock = false;
            AddError(LoadResString(IDS_COMPSHADER_LOCKED), 3.0f, ERR_NOTIFY, false);
        } else if (m_bCompShaderLock && !m_bWarpShaderLock) {
            m_bCompShaderLock = false; m_bWarpShaderLock = true;
            AddError(LoadResString(IDS_WARPSHADER_LOCKED), 3.0f, ERR_NOTIFY, false);
        } else if (!m_bCompShaderLock && m_bWarpShaderLock) {
            m_bCompShaderLock = true; m_bWarpShaderLock = true;
            AddError(LoadResString(IDS_ALLSHADERS_LOCKED), 3.0f, ERR_NOTIFY, false);
        } else {
            m_bCompShaderLock = false; m_bWarpShaderLock = false;
            AddError(LoadResString(IDS_ALLSHADERS_UNLOCKED), 3.0f, ERR_NOTIFY, false);
        }
		return true;

	case Cmd::ViewSongTitle:
		if (!m_szSongTitle[0])
			return true;   // nothing playing: an empty title would only block other messages
		LaunchSongTitleAnim(true);
		return true;

	case Cmd::ViewNowPlaying:
		m_bNowPlaying = !m_bNowPlaying;
		if (m_bNowPlaying) {
			m_nowPlaying.Start(&m_songBox);
		} else {
			m_nowPlaying.Stop();
			m_songBox.Set(SongInfo());
		}
		AddError(LoadResString(m_bNowPlaying ? IDS_NOW_PLAYING_ON : IDS_NOW_PLAYING_OFF), 3.0f, ERR_NOTIFY, false);
		WritePrivateProfileIntW(m_bNowPlaying, L"bNowPlaying", GetConfigIniFile(), L"settings");
		return true;

	case Cmd::FpsLimitCycle:
		m_max_fps_w = m_max_fps_fs = NextFpsLimit(m_max_fps_w);
		{
			wchar_t fmt[64], msg[128];   // SURI: a toast (was the song-title animation)
			LoadResString(IDS_UI_MAX_FRAME_RATE_X, fmt, 64);
			swprintf(msg, 128, fmt, FpsLimitMessage(m_max_fps_w).c_str());
			AddError(msg, 2.5f, ERR_NOTIFY, false);
		}
		return true;

	case Cmd::AutoChangeCycle:
		{
			AutoChangeStep step = NextAutoChange(m_fTimeBetweenPresets);
			m_fTimeBetweenPresets = step.seconds;
			m_fTimeBetweenPresetsRand = step.randSeconds;
			m_bPresetLockedByUser = step.lockPreset;
			wchar_t fmt[64], msg[128];   // SURI: a toast (was the song-title animation)
			LoadResString(IDS_UI_AUTO_CHANGE_X, fmt, 64);
			swprintf(msg, 128, fmt, step.message);
			AddError(msg, 2.5f, ERR_NOTIFY, false);
		}
		return true;

	case Cmd::ViewPresetInfo:	m_bShowPresetInfo = !m_bShowPresetInfo;	return true;
	case Cmd::ViewFps:			m_bShowFPS = !m_bShowFPS;				return true;
	case Cmd::ViewRating:		m_bShowRating = !m_bShowRating;			return true;
	case Cmd::ViewShaderHelp:	m_bShowShaderHelp = !m_bShowShaderHelp;	return true;

	case Cmd::ViewAlwaysOnTop:
		m_bAlwaysOnTop = !m_bAlwaysOnTop;
		ToggleAlwaysOnTop(hWnd);
		wsprintfW(m_szSongTitle, m_bAlwaysOnTop ? L"Always On Top ON" : L"Always On Top OFF");
		LaunchSongTitleAnim();
		return true;

	case Cmd::FxMirrorX: case Cmd::FxMirrorY: case Cmd::FxKaleido: case Cmd::FxRipple:
	case Cmd::FxZoomIn: case Cmd::FxSwapAxes: case Cmd::FxTunnel: case Cmd::FxInvert:
	case Cmd::FxMono: case Cmd::FxPosterize: case Cmd::FxHueSpin:
	case Cmd::FxBrightnessUp: case Cmd::FxBrightnessDown:
	case Cmd::FxContrastUp: case Cmd::FxContrastDown: case Cmd::FxClearAll:
		{
			// SURI: shader effects (right-click menu only). Needs pixel shaders.
			if (m_nMaxPSVersion <= 0)
				return false;
			wchar_t msg[256], label[128], onOff[32];   // SURI: own buffers (LoadResString shares one)
			LoadResString((int)CmdLabelId(c), label, 128);
			FxId id;
			if (CmdToFx(c, &id))
			{
				ToggleFx(m_fx, id);
				bool on = (m_fx.mask & (1u << (unsigned)id)) != 0;
				LoadResString(on ? IDS_FX_ON : IDS_FX_OFF, onOff, 32);
				swprintf(msg, 256, L"%s: %s", label, onOff);
			}
			else if (c == Cmd::FxClearAll)
			{
				m_fx = FxState();
				LoadResString(IDS_FX_CLEARED, msg, 256);
			}
			else
			{
				bool brightness = (c == Cmd::FxBrightnessUp || c == Cmd::FxBrightnessDown);
				int dir = (c == Cmd::FxBrightnessUp || c == Cmd::FxContrastUp) ? +1 : -1;
				float& level = brightness ? m_fx.brightness : m_fx.contrast;
				level = StepLevel(level, dir);
				swprintf(msg, 256, L"%s: %.2f", label, level);
			}
			RefreshFx();
			AddError(msg, 2.0f, ERR_NOTIFY, false);
		}
		return true;

	case Cmd::HelpOverlay:
		m_show_press_f1_msg = 0;
		ToggleHelp();
		return true;

	// SURI: sprites and custom messages (phase 9)
	case Cmd::SpriteLoadImage:
		{
			wchar_t file[MAX_PATH] = L"";
			wchar_t dir[MAX_PATH];
			StringCchPrintfW(dir, _countof(dir), L"%stextures", m_szMilkdrop2Path);
			OPENFILENAMEW ofn;
			ZeroMemory(&ofn, sizeof(ofn));
			ofn.lStructSize = sizeof(ofn);
			ofn.hwndOwner = hWnd;
			ofn.lpstrFilter = L"Images (*.png;*.jpg;*.jpeg;*.bmp;*.gif;*.tga;*.dds)\0*.png;*.jpg;*.jpeg;*.bmp;*.gif;*.tga;*.dds\0\0";
			ofn.lpstrFile = file;
			ofn.nMaxFile = _countof(file);
			ofn.lpstrInitialDir = dir;
			ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR;
			if (!GetOpenFileNameW(&ofn))
				return true;	// cancelled
			SpriteDesc d;
			ZeroMemory(&d, sizeof(d));
			lstrcpynW(d.img, MakeSpritePathRelative(m_szMilkdrop2Path, file).c_str(), _countof(d.img));
			d.x = 0.5f;
			d.y = 0.5f;
			d.scale = 0.5f;
			d.rot = 0.0f;
			d.blend = kSpriteBlendImageAlpha;
			d.colorkey = 0;
			LaunchStaticSprite(d, false);
		}
		return true;

	case Cmd::SpriteLaunchPrompt:	// 'k', then two digits: milk2_img.ini [imgNN]
		if (GetAppSnapshot().textEditActive)
			return false;
		m_nNumericInputMode   = NUMERIC_INPUT_MODE_SPRITE;
		m_nNumericInputNum    = 0;
		m_nNumericInputDigits = 0;
		AddError(LoadResString(IDS_TYPE_SPRITE_NUMBER), 3.0f, ERR_NOTIFY, false);
		return true;

	case Cmd::MessagePrompt:		// 'y', then two digits: milk2_msg.ini [messageNN]
		if (GetAppSnapshot().textEditActive)
			return false;
		ReadCustomMessages();	// pick up edits made since the start
		m_nNumericInputMode   = NUMERIC_INPUT_MODE_CUST_MSG;
		m_nNumericInputNum    = 0;
		m_nNumericInputDigits = 0;
		AddError(LoadResString(IDS_TYPE_MESSAGE_NUMBER), 3.0f, ERR_NOTIFY, false);
		return true;

	case Cmd::SpriteRemoveSelected:
		if (m_nSelectedSprite >= 0 && m_nSelectedSprite < NUM_TEX)
			KillSprite(m_nSelectedSprite);
		m_nSelectedSprite = -1;
		return true;

	case Cmd::SpriteRemoveAll:
		for (int x=0; x<NUM_TEX; x++)
			if (m_texmgr.m_tex[x].pSurface)
				KillSprite(x);
		return true;

	case Cmd::SpriteBlendImage: case Cmd::SpriteBlendAlpha: case Cmd::SpriteBlendDecal:
	case Cmd::SpriteBlendAdditive: case Cmd::SpriteBlendColorMask: case Cmd::SpriteBlendColorKey:
		if (m_nSelectedSprite < 0 || m_nSelectedSprite >= NUM_TEX || !m_texmgr.m_tex[m_nSelectedSprite].pSurface)
			return false;
		m_texmgr.m_tex[m_nSelectedSprite].userBlend = SpriteBlendOf(c);
		return true;

	case Cmd::SpriteEditList:
	case Cmd::MessageEditList:
		{
			bool sprites = (c == Cmd::SpriteEditList);
			const wchar_t* path = sprites ? m_szImgIniFile : m_szMsgIniFile;
			WriteUtf16IniIfMissing(path, sprites ? SpritesIniTemplate() : MessagesIniTemplate());
			wchar_t arg[MAX_PATH + 3];	// two quotes and the terminating zero
			StringCchPrintfW(arg, _countof(arg), L"\"%s\"", path);
			ShellExecuteW(NULL, L"open", L"notepad.exe", arg, NULL, SW_SHOWNORMAL);
		}
		return true;

	case Cmd::MessageRandom:
		{
			ReadCustomMessages();	// pick up edits made since the start
			bool any = false;
			for (int i=0; i<MAX_CUSTOM_MESSAGES && !any; i++)
				any = (m_CustomMessage[i].szText[0] != 0);
			if (any)
				LaunchCustomMessage(-1);
			else
				AddError(LoadResString(IDS_NO_MESSAGES), 3.0f, ERR_NOTIFY, false);
		}
		return true;

	default:
		// window-level commands (full screen, stretch, quit, help pages, about)
		// are run by Milkdrop2PcmVisualizer.cpp
		return false;
	}
}

AppSnapshot CPlugin::GetAppSnapshot() const
{
	AppSnapshot s;
	s.presetLockedByUser = m_bPresetLockedByUser;
	s.presetLockedByCode = m_bPresetLockedByCode;
	s.sequentialOrder    = m_bSequentialPresetOrder;
	s.showFps            = m_bShowFPS;
	s.showPresetInfo     = m_bShowPresetInfo;
	s.showRating         = m_bShowRating;
	s.showShaderHelp     = m_bShowShaderHelp;
	s.showHelp           = m_show_help != 0;
	s.alwaysOnTop        = m_bAlwaysOnTop;
	s.loadMenuOpen       = m_UI_mode == UI_LOAD;
	s.editMenuOpen       = m_UI_mode == UI_MENU;
	s.playlistOpen       = m_UI_mode == UI_PLAYLIST;
	s.textEditActive     = m_waitstring.bActive || m_UI_mode == UI_LOAD_DEL ||
	                       m_UI_mode == UI_SAVE_OVERWRITE || m_UI_mode == UI_UPGRADE_PIXEL_SHADER ||
	                       m_UI_mode == UI_IMPORT_NEW;   // SURI: the drop's "new presets" prompt
	s.canDeleteCurrent   = m_nCurrentPreset >= m_nDirs && m_nCurrentPreset < m_nPresets;
	s.fpsLimit           = m_max_fps_w;
	s.autoChangeSeconds  = m_fTimeBetweenPresets;
	s.shadersAvailable   = m_nMaxPSVersion > 0;
	s.fxMask             = m_fx.mask;
	s.fxBrightness       = m_fx.brightness;
	s.fxContrast         = m_fx.contrast;
	s.nowPlayingAvailable = m_songBox.State() != NowPlayingState::Unavailable;
	s.nowPlayingOn       = m_bNowPlaying;
	s.hasSongTitle       = m_szSongTitle[0] != 0;
	// SURI: sprites (phase 9)
	for (int i=0; i<NUM_TEX; i++)
		if (m_texmgr.m_tex[i].pSurface)
			s.spriteCount++;
	int sel = m_nSelectedSprite;
	if (sel >= 0 && sel < NUM_TEX && m_texmgr.m_tex[sel].pSurface)
	{
		const td_tex& t = m_texmgr.m_tex[sel];
		s.selectedSprite = sel;
		if (t.userBlend >= 0)
			s.selectedSpriteBlend = min(5, t.userBlend);
		else if (t.var_blendmode)
			s.selectedSpriteBlend = min(5, max(0, (int)(*t.var_blendmode)));
	}
	return s;
}

int CPlugin::HandleRegularKey(WPARAM wParam)
{
	// here we handle all the normal keys for milkdrop-
	// these are the hotkeys that are used when you're not
	// in the middle of editing a string, navigating a menu, etc.

	// do not make references to virtual keys here; only
	// straight WM_CHAR messages should be sent in.

    // return 0 if you process/absorb the key; otherwise return 1.

	if (m_UI_mode == UI_LOAD && ((wParam >= 'A' && wParam <= 'Z') || (wParam >= 'a' && wParam <= 'z')))
	{
		SeekToPreset((char)wParam);
		return 0; // we processed (or absorbed) the key
	}
    else if (m_UI_mode == UI_MASHUP && wParam >= '1' && wParam <= ('0' + MASH_SLOTS))
    {
        m_nMashSlot = wParam - '1';
    }
	else if (DispatchCommand(LookupKey(KeyScope::Char, (unsigned)wParam), GetPluginWindow()))
	{
		return 0; // SURI: handled by the command table (features/commands.cpp)
	}
	else switch(wParam)
	{
	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
		{
			int digit = wParam - '0';
			m_nNumericInputNum		= (m_nNumericInputNum*10) + digit;
			m_nNumericInputDigits++;

			if (m_nNumericInputDigits >= 2)
			{
				if (m_nNumericInputMode == NUMERIC_INPUT_MODE_CUST_MSG)
					LaunchCustomMessage(m_nNumericInputNum);
				else if (m_nNumericInputMode == NUMERIC_INPUT_MODE_SPRITE)
					LaunchSprite(m_nNumericInputNum, -1);
				else if (m_nNumericInputMode == NUMERIC_INPUT_MODE_SPRITE_KILL)
				{
					for (int x=0; x<NUM_TEX; x++)
						if (m_texmgr.m_tex[x].nUserData == m_nNumericInputNum)
							KillSprite(x);
				}

				m_nNumericInputDigits = 0;
				m_nNumericInputNum = 0;
			}
		}
		return 0; // we processed (or absorbed) the key

    // row 1 keys
	case 'q':
		m_pState->m_fVideoEchoZoom /= 1.05f;
		return 0; // we processed (or absorbed) the key
	case 'Q':
		m_pState->m_fVideoEchoZoom *= 1.05f;
		return 0; // we processed (or absorbed) the key
	case 'w':
		m_pState->m_nWaveMode++;
		if (m_pState->m_nWaveMode >= NUM_WAVES) m_pState->m_nWaveMode = 0;
		return 0; // we processed (or absorbed) the key
	case 'W':
		m_pState->m_nWaveMode--;
		if (m_pState->m_nWaveMode < 0) m_pState->m_nWaveMode = NUM_WAVES - 1;
		return 0; // we processed (or absorbed) the key
	case 'e':
		m_pState->m_fWaveAlpha -= 0.1f;
		if (m_pState->m_fWaveAlpha.eval(-1) < 0.0f) m_pState->m_fWaveAlpha = 0.0f;
		return 0; // we processed (or absorbed) the key
	case 'E':
		m_pState->m_fWaveAlpha += 0.1f;
		return 0; // we processed (or absorbed) the key

	case 'I':	m_pState->m_fZoom -= 0.01f;		return 0; // we processed (or absorbed) the key
	case 'i':	m_pState->m_fZoom += 0.01f;		return 0; // we processed (or absorbed) the key

	case 'n':
	case 'N':
		m_bShowDebugInfo = !m_bShowDebugInfo;
		return 0; // we processed (or absorbed) the key

	case 'u':
	case 'U':
        AddError(LoadResString(IDS_SHUFFLE_IS_NOW_ON), 3.0f, ERR_NOTIFY, false);
		return 0; // we processed (or absorbed) the key

	case 'o':	m_pState->m_fWarpAmount /= 1.1f;	return 0; // we processed (or absorbed) the key
	case 'O':	m_pState->m_fWarpAmount *= 1.1f;	return 0; // we processed (or absorbed) the key

    case '!':
        // randomize warp shader
        {
            bool bWarpLock = m_bWarpShaderLock;
            wchar_t szOldPreset[MAX_PATH];
		    lstrcpyW(szOldPreset, m_szCurrentPresetFile);
            m_bWarpShaderLock = false;
            LoadRandomPreset(0.0f);
            m_bWarpShaderLock = true;
            LoadPreset(szOldPreset, 0.0f);
            m_bWarpShaderLock = bWarpLock;
        }
        break;
    case '@':
        // randomize comp shader
        {
            bool bCompLock = m_bCompShaderLock;
            wchar_t szOldPreset[MAX_PATH];
		    lstrcpyW(szOldPreset, m_szCurrentPresetFile);
            m_bCompShaderLock = false;
            LoadRandomPreset(0.0f);
            m_bCompShaderLock = true;
            LoadPreset(szOldPreset, 0.0f);
            m_bCompShaderLock = bCompLock;
        }
        break;

    case 'a':
    case 'A':
        // load a random preset, a random warp shader, and a random comp shader.
        // not quite as extreme as a mash-up.
        {
            bool bCompLock = m_bCompShaderLock;
            bool bWarpLock = m_bWarpShaderLock;
            m_bCompShaderLock = false; m_bWarpShaderLock = false;
            LoadRandomPreset(0.0f);
            m_bCompShaderLock = true; m_bWarpShaderLock = false;
            LoadRandomPreset(0.0f);
            m_bCompShaderLock = false; m_bWarpShaderLock = true;
            LoadRandomPreset(0.0f);
            m_bCompShaderLock = bCompLock;
            m_bWarpShaderLock = bWarpLock;
        }
        break;

	// row 2 keys
	case 'h':
	case 'H':
		// mash-up only: elsewhere h/H is Cmd::PresetNext (instant hard cut)
        if (m_UI_mode == UI_MASHUP)
        {
            if (wParam=='h')
            {
                m_nMashPreset[m_nMashSlot] = m_nDirs + (rand() % (m_nPresets-m_nDirs));
                m_nLastMashChangeFrame[m_nMashSlot] = GetFrame() + MASH_APPLY_DELAY_FRAMES;  // causes instant apply
            }
            else
            {
                for (int mash=0; mash<MASH_SLOTS; mash++)
                {
                    m_nMashPreset[mash] = m_nDirs + (rand() % (m_nPresets-m_nDirs));
                    m_nLastMashChangeFrame[mash] = GetFrame() + MASH_APPLY_DELAY_FRAMES;  // causes instant apply
                }
            }
        }
		return 0; // we processed (or absorbed) the key
	case 'f':
	case 'F':
		m_pState->m_nVideoEchoOrientation = (m_pState->m_nVideoEchoOrientation + 1) % 4;
		return 0; // we processed (or absorbed) the key
	case 'g':
		m_pState->m_fGammaAdj -= 0.1f;
		if (m_pState->m_fGammaAdj.eval(-1) < 0.0f) m_pState->m_fGammaAdj = 0.0f;
		return 0; // we processed (or absorbed) the key
	case 'G':
		m_pState->m_fGammaAdj += 0.1f;
		return 0; // we processed (or absorbed) the key
    case 'j':
		m_pState->m_fWaveScale *= 0.9f;
		return 0; // we processed (or absorbed) the key
	case 'J':
		m_pState->m_fWaveScale /= 0.9f;
		return 0; // we processed (or absorbed) the key
	case 'K':	// SURI: 'k' is Cmd::SpriteLaunchPrompt; Shift+K (remove a sprite) stays here
		{
			USHORT mask = 1 << (sizeof(SHORT)*8 - 1);	// we want the highest-order bit
			bool bShiftHeldDown = (GetKeyState(VK_SHIFT) & mask) != 0;

			if (bShiftHeldDown)
				m_nNumericInputMode   = NUMERIC_INPUT_MODE_SPRITE_KILL;
			else
				m_nNumericInputMode   = NUMERIC_INPUT_MODE_SPRITE;
			m_nNumericInputNum    = 0;
			m_nNumericInputDigits = 0;
		}
		return 0; // we processed (or absorbed) the key

	// row 3/misc. keys

	case '[':
		m_pState->m_fXPush -= 0.005f;
		return 0; // we processed (or absorbed) the key
	case ']':
		m_pState->m_fXPush += 0.005f;
		return 0; // we processed (or absorbed) the key
	case '{':
		m_pState->m_fYPush -= 0.005f;
		return 0; // we processed (or absorbed) the key
	case '}':
		m_pState->m_fYPush += 0.005f;
		return 0; // we processed (or absorbed) the key
	case '<':
		m_pState->m_fRot += 0.02f;
		return 0; // we processed (or absorbed) the key
	case '>':
		m_pState->m_fRot -= 0.02f;
		return 0; // we processed (or absorbed) the key

    case '*':
		m_nNumericInputDigits = 0;
		m_nNumericInputNum = 0;
        return 0;

	}

	// SURI: 'y' / 'Y' (custom message) and 'k' (sprite) are Cmd::MessagePrompt / Cmd::SpriteLaunchPrompt

    return 1;
}

wchar_t* FormImageCacheSizeString(wchar_t* itemStr, UINT sizeID)
{
	static wchar_t cacheBuf[128] = {0};
	StringCchPrintfW(cacheBuf, 128, L"%s %s", itemStr, LoadResString(sizeID));
	return cacheBuf;
}

//----------------------------------------------------------------------

void CPlugin::Randomize()
{
	srand((int)(GetTime()*100));
	m_fRandStart[0]		= (rand() % 64841L)*0.01f;
	m_fRandStart[1]		= (rand() % 53751L)*0.01f;
	m_fRandStart[2]		= (rand() % 42661L)*0.01f;
	m_fRandStart[3]		= (rand() % 31571L)*0.01f;

}

//----------------------------------------------------------------------

void CPlugin::SetMenusForPresetVersion(int WarpPSVersion, int CompPSVersion)
{
    int MaxPSVersion = max(WarpPSVersion, CompPSVersion);

    m_menuPreset.EnableItem(LoadResString(IDS_MENU_EDIT_WARP_SHADER), WarpPSVersion > 0);
	m_menuPreset.EnableItem(LoadResString(IDS_MENU_EDIT_COMPOSITE_SHADER), CompPSVersion > 0);
	m_menuPost.EnableItem(LoadResString(IDS_MENU_SUSTAIN_LEVEL), WarpPSVersion==0);
	m_menuPost.EnableItem(LoadResString(IDS_MENU_TEXTURE_WRAP), WarpPSVersion==0);
	m_menuPost.EnableItem(LoadResString(IDS_MENU_GAMMA_ADJUSTMENT), CompPSVersion==0);
	m_menuPost.EnableItem(LoadResString(IDS_MENU_HUE_SHADER), CompPSVersion==0);
	m_menuPost.EnableItem(LoadResString(IDS_MENU_VIDEO_ECHO_ALPHA), CompPSVersion==0);
	m_menuPost.EnableItem(LoadResString(IDS_MENU_VIDEO_ECHO_ZOOM), CompPSVersion==0);
	m_menuPost.EnableItem(LoadResString(IDS_MENU_VIDEO_ECHO_ORIENTATION), CompPSVersion==0);
	m_menuPost.EnableItem(LoadResString(IDS_MENU_FILTER_INVERT), CompPSVersion==0);
	m_menuPost.EnableItem(LoadResString(IDS_MENU_FILTER_BRIGHTEN), CompPSVersion==0);
	m_menuPost.EnableItem(LoadResString(IDS_MENU_FILTER_DARKEN), CompPSVersion==0);
	m_menuPost.EnableItem(LoadResString(IDS_MENU_FILTER_SOLARIZE), CompPSVersion==0);
	m_menuPost.EnableItem(LoadResString(IDS_MENU_BLUR1_EDGE_DARKEN_AMOUNT), MaxPSVersion > 0);
	m_menuPost.EnableItem(LoadResString(IDS_MENU_BLUR1_MIN_COLOR_VALUE), MaxPSVersion > 0);
	m_menuPost.EnableItem(LoadResString(IDS_MENU_BLUR1_MAX_COLOR_VALUE), MaxPSVersion > 0);
	m_menuPost.EnableItem(LoadResString(IDS_MENU_BLUR2_MIN_COLOR_VALUE), MaxPSVersion > 0);
	m_menuPost.EnableItem(LoadResString(IDS_MENU_BLUR2_MAX_COLOR_VALUE), MaxPSVersion > 0);
	m_menuPost.EnableItem(LoadResString(IDS_MENU_BLUR3_MIN_COLOR_VALUE), MaxPSVersion > 0);
	m_menuPost.EnableItem(LoadResString(IDS_MENU_BLUR3_MAX_COLOR_VALUE), MaxPSVersion > 0);
}

void CPlugin::BuildMenus()
{
    wchar_t buf[1024];

	m_pCurMenu = &m_menuPreset;//&m_menuMain;

	m_menuPreset     .Init(LoadResString(IDS_EDIT_CURRENT_PRESET));
	m_menuMotion     .Init(LoadResString(IDS_MOTION));
    m_menuCustomShape.Init(LoadResString(IDS_DRAWING_CUSTOM_SHAPES));
    m_menuCustomWave .Init(LoadResString(IDS_DRAWING_CUSTOM_WAVES));
	m_menuWave       .Init(LoadResString(IDS_DRAWING_SIMPLE_WAVEFORM));
	m_menuAugment    .Init(LoadResString(IDS_DRAWING_BORDERS_MOTION_VECTORS));
	m_menuPost       .Init(LoadResString(IDS_POST_PROCESSING_MISC));
    for (int i=0; i<MAX_CUSTOM_WAVES; i++)
    {
        swprintf(buf, LoadResString(IDS_CUSTOM_WAVE_X), i+1);
	    m_menuWavecode[i].Init(buf);
    }
    for (i=0; i<MAX_CUSTOM_SHAPES; i++)
    {
        swprintf(buf, LoadResString(IDS_CUSTOM_SHAPE_X), i+1);
	    m_menuShapecode[i].Init(buf);
    }

    //-------------------------------------------

    // MAIN MENU / menu hierarchy

    m_menuPreset.AddChildMenu(&m_menuMotion);
    m_menuPreset.AddChildMenu(&m_menuCustomShape);
    m_menuPreset.AddChildMenu(&m_menuCustomWave);
    m_menuPreset.AddChildMenu(&m_menuWave);
    m_menuPreset.AddChildMenu(&m_menuAugment);
    m_menuPreset.AddChildMenu(&m_menuPost);

    for (i=0; i<MAX_CUSTOM_SHAPES; i++)
	    m_menuCustomShape.AddChildMenu(&m_menuShapecode[i]);
    for (i=0; i<MAX_CUSTOM_WAVES; i++)
	    m_menuCustomWave.AddChildMenu(&m_menuWavecode[i]);

	// NOTE: all of the eval menuitems use a CALLBACK function to register the user's changes (see last param)
	m_menuPreset.AddItem(LoadResString(IDS_MENU_EDIT_PRESET_INIT_CODE),
						 &m_pState->m_szPerFrameInit, MENUITEMTYPE_STRING,
						 LoadResString(IDS_MENU_EDIT_PRESET_INIT_CODE_TT, buf, 1024),
						 256, 0, &OnUserEditedPresetInit, sizeof(m_pState->m_szPerFrameInit), 0);

	m_menuPreset.AddItem(LoadResString(IDS_MENU_EDIT_PER_FRAME_EQUATIONS),
						 &m_pState->m_szPerFrameExpr, MENUITEMTYPE_STRING,
						 LoadResString(IDS_MENU_EDIT_PER_FRAME_EQUATIONS_TT, buf, 1024),
                         256, 0, &OnUserEditedPerFrame, sizeof(m_pState->m_szPerFrameExpr), 0);

	m_menuPreset.AddItem(LoadResString(IDS_MENU_EDIT_PER_VERTEX_EQUATIONS),
						 &m_pState->m_szPerPixelExpr, MENUITEMTYPE_STRING,
						 LoadResString(IDS_MENU_EDIT_PER_VERTEX_EQUATIONS_TT, buf, 1024),
						 256, 0, &OnUserEditedPerPixel, sizeof(m_pState->m_szPerPixelExpr), 0);

	m_menuPreset.AddItem(LoadResString(IDS_MENU_EDIT_WARP_SHADER),
						 &m_pState->m_szWarpShadersText, MENUITEMTYPE_STRING,
						 LoadResString(IDS_MENU_EDIT_WARP_SHADER_TT, buf, 1024),
						 256, 0, &OnUserEditedWarpShaders, sizeof(m_pState->m_szWarpShadersText), 0);

	m_menuPreset.AddItem(LoadResString(IDS_MENU_EDIT_COMPOSITE_SHADER),
						 &m_pState->m_szCompShadersText, MENUITEMTYPE_STRING,
						 LoadResString(IDS_MENU_EDIT_COMPOSITE_SHADER_TT, buf, 1024),
						 256, 0, &OnUserEditedCompShaders, sizeof(m_pState->m_szCompShadersText), 0);

	m_menuPreset.AddItem(LoadResString(IDS_MENU_EDIT_UPGRADE_PRESET_PS_VERSION),
						 (void*)UI_UPGRADE_PIXEL_SHADER, MENUITEMTYPE_UIMODE,
						 LoadResString(IDS_MENU_EDIT_UPGRADE_PRESET_PS_VERSION_TT, buf, 1024),
						 0, 0, NULL, UI_UPGRADE_PIXEL_SHADER, 0);

	m_menuPreset.AddItem(LoadResString(IDS_MENU_EDIT_DO_A_PRESET_MASH_UP),
						 (void*)UI_MASHUP, MENUITEMTYPE_UIMODE,
						 LoadResString(IDS_MENU_EDIT_DO_A_PRESET_MASH_UP_TT, buf, 1024),
						 0, 0, NULL, UI_MASHUP, 0);

    //-------------------------------------------

	// menu items
	#define MEN_T(id) LoadResString(id)
	#define MEN_TT(id) LoadResString(id, buf, 1024)

	m_menuWave.AddItem(MEN_T(IDS_MENU_WAVE_TYPE),			&m_pState->m_nWaveMode,     MENUITEMTYPE_INT, MEN_TT(IDS_MENU_WAVE_TYPE_TT), 0, NUM_WAVES-1);
	m_menuWave.AddItem(MEN_T(IDS_MENU_SIZE),				&m_pState->m_fWaveScale,    MENUITEMTYPE_LOGBLENDABLE, MEN_TT(IDS_MENU_SIZE_TT));
	m_menuWave.AddItem(MEN_T(IDS_MENU_SMOOTH),				&m_pState->m_fWaveSmoothing,MENUITEMTYPE_BLENDABLE, MEN_TT(IDS_MENU_SMOOTH_TT), 0.0f, 0.9f);
	m_menuWave.AddItem(MEN_T(IDS_MENU_MYSTERY_PARAMETER),	&m_pState->m_fWaveParam,    MENUITEMTYPE_BLENDABLE, MEN_TT(IDS_MENU_MYSTERY_PARAMETER_TT), -1.0f, 1.0f);
	m_menuWave.AddItem(MEN_T(IDS_MENU_POSITION_X),			&m_pState->m_fWaveX,        MENUITEMTYPE_BLENDABLE, MEN_TT(IDS_MENU_POSITION_X_TT), 0, 1);
	m_menuWave.AddItem(MEN_T(IDS_MENU_POSITION_Y),			&m_pState->m_fWaveY,        MENUITEMTYPE_BLENDABLE, MEN_TT(IDS_MENU_POSITION_Y_TT), 0, 1);
	m_menuWave.AddItem(MEN_T(IDS_MENU_COLOR_RED),			&m_pState->m_fWaveR,        MENUITEMTYPE_BLENDABLE, MEN_TT(IDS_MENU_COLOR_RED_TT), 0, 1);
	m_menuWave.AddItem(MEN_T(IDS_MENU_COLOR_GREEN),			&m_pState->m_fWaveG,        MENUITEMTYPE_BLENDABLE, MEN_TT(IDS_MENU_COLOR_GREEN_TT), 0, 1);
	m_menuWave.AddItem(MEN_T(IDS_MENU_COLOR_BLUE),			&m_pState->m_fWaveB,        MENUITEMTYPE_BLENDABLE, MEN_TT(IDS_MENU_COLOR_BLUE_TT), 0, 1);
	m_menuWave.AddItem(MEN_T(IDS_MENU_OPACITY),				&m_pState->m_fWaveAlpha,    MENUITEMTYPE_LOGBLENDABLE, MEN_TT(IDS_MENU_OPACITY_TT), 0.001f, 100.0f);
	m_menuWave.AddItem(MEN_T(IDS_MENU_USE_DOTS),			&m_pState->m_bWaveDots,     MENUITEMTYPE_BOOL, MEN_TT(IDS_MENU_USE_DOTS_TT));
	m_menuWave.AddItem(MEN_T(IDS_MENU_DRAW_THICK),			&m_pState->m_bWaveThick,    MENUITEMTYPE_BOOL, MEN_TT(IDS_MENU_DRAW_THICK_TT));
	m_menuWave.AddItem(MEN_T(IDS_MENU_MODULATE_OPACITY_BY_VOLUME),	&m_pState->m_bModWaveAlphaByVolume,	MENUITEMTYPE_BOOL,      MEN_TT(IDS_MENU_MODULATE_OPACITY_BY_VOLUME_TT));
	m_menuWave.AddItem(MEN_T(IDS_MENU_MODULATION_TRANSPARENT_VOLUME), &m_pState->m_fModWaveAlphaStart,	MENUITEMTYPE_BLENDABLE, MEN_TT(IDS_MENU_MODULATION_TRANSPARENT_VOLUME_TT), 0.0f, 2.0f);
	m_menuWave.AddItem(MEN_T(IDS_MENU_MODULATION_OPAQUE_VOLUME),	&m_pState->m_fModWaveAlphaEnd,      MENUITEMTYPE_BLENDABLE, MEN_TT(IDS_MENU_MODULATION_OPAQUE_VOLUME_TT), 0.0f, 2.0f);
	m_menuWave.AddItem(MEN_T(IDS_MENU_ADDITIVE_DRAWING),	&m_pState->m_bAdditiveWaves, MENUITEMTYPE_BOOL, MEN_TT(IDS_MENU_ADDITIVE_DRAWING_TT));
	m_menuWave.AddItem(MEN_T(IDS_MENU_COLOR_BRIGHTENING),	&m_pState->m_bMaximizeWaveColor, MENUITEMTYPE_BOOL, MEN_TT(IDS_MENU_COLOR_BRIGHTENING_TT));

	m_menuAugment.AddItem(MEN_T(IDS_MENU_OUTER_BORDER_THICKNESS),	&m_pState->m_fOuterBorderSize,	MENUITEMTYPE_BLENDABLE, MEN_TT(IDS_MENU_OUTER_BORDER_THICKNESS_TT), 0, 0.5f);
	m_menuAugment.AddItem(MEN_T(IDS_MENU_COLOR_RED_OUTER),			&m_pState->m_fOuterBorderR,		MENUITEMTYPE_BLENDABLE, MEN_TT(IDS_MENU_COLOR_RED_OUTER_TT), 0, 1);
	m_menuAugment.AddItem(MEN_T(IDS_MENU_COLOR_GREEN_OUTER),		&m_pState->m_fOuterBorderG,		MENUITEMTYPE_BLENDABLE, MEN_TT(IDS_MENU_COLOR_GREEN_OUTER_TT), 0, 1);
	m_menuAugment.AddItem(MEN_T(IDS_MENU_COLOR_BLUE_OUTER),			&m_pState->m_fOuterBorderB,		MENUITEMTYPE_BLENDABLE, MEN_TT(IDS_MENU_COLOR_BLUE_OUTER_TT), 0, 1);
	m_menuAugment.AddItem(MEN_T(IDS_MENU_OPACITY_OUTER),			&m_pState->m_fOuterBorderA,		MENUITEMTYPE_BLENDABLE, MEN_TT(IDS_MENU_OPACITY_OUTER_TT), 0, 1);
	m_menuAugment.AddItem(MEN_T(IDS_MENU_INNER_BORDER_THICKNESS),	&m_pState->m_fInnerBorderSize,	MENUITEMTYPE_BLENDABLE, MEN_TT(IDS_MENU_INNER_BORDER_THICKNESS_TT), 0, 0.5f);
	m_menuAugment.AddItem(MEN_T(IDS_MENU_COLOR_RED_OUTER),			&m_pState->m_fInnerBorderR,		MENUITEMTYPE_BLENDABLE, MEN_TT(IDS_MENU_COLOR_RED_INNER_TT), 0, 1);
	m_menuAugment.AddItem(MEN_T(IDS_MENU_COLOR_GREEN_OUTER),		&m_pState->m_fInnerBorderG,		MENUITEMTYPE_BLENDABLE, MEN_TT(IDS_MENU_COLOR_GREEN_INNER_TT), 0, 1);
	m_menuAugment.AddItem(MEN_T(IDS_MENU_COLOR_BLUE_OUTER),			&m_pState->m_fInnerBorderB,		MENUITEMTYPE_BLENDABLE, MEN_TT(IDS_MENU_COLOR_BLUE_INNER_TT), 0, 1);
	m_menuAugment.AddItem(MEN_T(IDS_MENU_OPACITY_OUTER),			&m_pState->m_fInnerBorderA,		MENUITEMTYPE_BLENDABLE, MEN_TT(IDS_MENU_OPACITY_INNER_TT), 0, 1);
	m_menuAugment.AddItem(MEN_T(IDS_MENU_MOTION_VECTOR_OPACITY),	&m_pState->m_fMvA,				MENUITEMTYPE_BLENDABLE, MEN_TT(IDS_MENU_MOTION_VECTOR_OPACITY_TT), 0, 1);
	m_menuAugment.AddItem(MEN_T(IDS_MENU_NUM_MOT_VECTORS_X),		&m_pState->m_fMvX,				MENUITEMTYPE_BLENDABLE, MEN_TT(IDS_MENU_NUM_MOT_VECTORS_X_TT), 0, 64);
	m_menuAugment.AddItem(MEN_T(IDS_MENU_NUM_MOT_VECTORS_Y),		&m_pState->m_fMvY,				MENUITEMTYPE_BLENDABLE, MEN_TT(IDS_MENU_NUM_MOT_VECTORS_Y_TT), 0, 48);
	m_menuAugment.AddItem(MEN_T(IDS_MENU_OFFSET_X),					&m_pState->m_fMvDX,				MENUITEMTYPE_BLENDABLE, MEN_TT(IDS_MENU_OFFSET_X_TT), -1, 1);
	m_menuAugment.AddItem(MEN_T(IDS_MENU_OFFSET_Y),					&m_pState->m_fMvDY,				MENUITEMTYPE_BLENDABLE, MEN_TT(IDS_MENU_OFFSET_Y_TT), -1, 1);
	m_menuAugment.AddItem(MEN_T(IDS_MENU_TRAIL_LENGTH),				&m_pState->m_fMvL,				MENUITEMTYPE_BLENDABLE, MEN_TT(IDS_MENU_TRAIL_LENGTH_TT), 0, 5);
	m_menuAugment.AddItem(MEN_T(IDS_MENU_COLOR_RED_OUTER),			&m_pState->m_fMvR,				MENUITEMTYPE_BLENDABLE, MEN_TT(IDS_MENU_COLOR_RED_MOTION_VECTOR_TT), 0, 1);
	m_menuAugment.AddItem(MEN_T(IDS_MENU_COLOR_GREEN_OUTER),		&m_pState->m_fMvG,				MENUITEMTYPE_BLENDABLE, MEN_TT(IDS_MENU_COLOR_GREEN_MOTION_VECTOR_TT), 0, 1);
	m_menuAugment.AddItem(MEN_T(IDS_MENU_COLOR_BLUE_OUTER),			&m_pState->m_fMvB,				MENUITEMTYPE_BLENDABLE, MEN_TT(IDS_MENU_COLOR_BLUE_MOTION_VECTOR_TT), 0, 1);

	m_menuMotion.AddItem(MEN_T(IDS_MENU_ZOOM_AMOUNT),			&m_pState->m_fZoom,				MENUITEMTYPE_LOGBLENDABLE, MEN_TT(IDS_MENU_ZOOM_AMOUNT_TT));
	m_menuMotion.AddItem(MEN_T(IDS_MENU_ZOOM_EXPONENT),			&m_pState->m_fZoomExponent,		MENUITEMTYPE_LOGBLENDABLE, MEN_TT(IDS_MENU_ZOOM_EXPONENT_TT));
	m_menuMotion.AddItem(MEN_T(IDS_MENU_WARP_AMOUNT),			&m_pState->m_fWarpAmount,		MENUITEMTYPE_LOGBLENDABLE, MEN_TT(IDS_MENU_WARP_AMOUNT_TT));
	m_menuMotion.AddItem(MEN_T(IDS_MENU_WARP_SCALE),			&m_pState->m_fWarpScale,		MENUITEMTYPE_LOGBLENDABLE, MEN_TT(IDS_MENU_WARP_SCALE_TT));
	m_menuMotion.AddItem(MEN_T(IDS_MENU_WARP_SPEED),			&m_pState->m_fWarpAnimSpeed,	MENUITEMTYPE_LOGFLOAT,     MEN_TT(IDS_MENU_WARP_SPEED_TT));
	m_menuMotion.AddItem(MEN_T(IDS_MENU_ROTATION_AMOUNT),		&m_pState->m_fRot,				MENUITEMTYPE_BLENDABLE,    MEN_TT(IDS_MENU_ROTATION_AMOUNT_TT), -1.00f, 1.00f);
	m_menuMotion.AddItem(MEN_T(IDS_MENU_ROTATION_CENTER_OF_X),	&m_pState->m_fRotCX,			MENUITEMTYPE_BLENDABLE,    MEN_TT(IDS_MENU_ROTATION_CENTER_OF_X_TT), -1.0f, 2.0f);
	m_menuMotion.AddItem(MEN_T(IDS_MENU_ROTATION_CENTER_OF_Y),	&m_pState->m_fRotCY,			MENUITEMTYPE_BLENDABLE,    MEN_TT(IDS_MENU_ROTATION_CENTER_OF_Y_TT), -1.0f, 2.0f);
	m_menuMotion.AddItem(MEN_T(IDS_MENU_TRANSLATION_X),			&m_pState->m_fXPush,			MENUITEMTYPE_BLENDABLE,    MEN_TT(IDS_MENU_TRANSLATION_X_TT), -1.0f, 1.0f);
	m_menuMotion.AddItem(MEN_T(IDS_MENU_TRANSLATION_Y),			&m_pState->m_fYPush,			MENUITEMTYPE_BLENDABLE,    MEN_TT(IDS_MENU_TRANSLATION_Y_TT), -1.0f, 1.0f);
	m_menuMotion.AddItem(MEN_T(IDS_MENU_SCALING_X),				&m_pState->m_fStretchX,			MENUITEMTYPE_LOGBLENDABLE, MEN_TT(IDS_MENU_SCALING_X_TT));
	m_menuMotion.AddItem(MEN_T(IDS_MENU_SCALING_Y),				&m_pState->m_fStretchY,			MENUITEMTYPE_LOGBLENDABLE, MEN_TT(IDS_MENU_SCALING_Y_TT));

	m_menuPost.AddItem(MEN_T(IDS_MENU_SUSTAIN_LEVEL),			&m_pState->m_fDecay,			MENUITEMTYPE_BLENDABLE, MEN_TT(IDS_MENU_SUSTAIN_LEVEL_TT), 0.50f, 1.0f);
	m_menuPost.AddItem(MEN_T(IDS_MENU_DARKEN_CENTER),			&m_pState->m_bDarkenCenter,		MENUITEMTYPE_BOOL,      MEN_TT(IDS_MENU_DARKEN_CENTER_TT));
	m_menuPost.AddItem(MEN_T(IDS_MENU_GAMMA_ADJUSTMENT),		&m_pState->m_fGammaAdj,			MENUITEMTYPE_BLENDABLE, MEN_TT(IDS_MENU_GAMMA_ADJUSTMENT_TT), 1.0f, 8.0f);
	m_menuPost.AddItem(MEN_T(IDS_MENU_HUE_SHADER),				&m_pState->m_fShader,			MENUITEMTYPE_BLENDABLE, MEN_TT(IDS_MENU_HUE_SHADER_TT), 0.0f, 1.0f);
	m_menuPost.AddItem(MEN_T(IDS_MENU_VIDEO_ECHO_ALPHA),		&m_pState->m_fVideoEchoAlpha,	MENUITEMTYPE_BLENDABLE, MEN_TT(IDS_MENU_VIDEO_ECHO_ALPHA_TT), 0.0f, 1.0f);
	m_menuPost.AddItem(MEN_T(IDS_MENU_VIDEO_ECHO_ZOOM),			&m_pState->m_fVideoEchoZoom,	MENUITEMTYPE_LOGBLENDABLE, MEN_TT(IDS_MENU_VIDEO_ECHO_ZOOM_TT));
	m_menuPost.AddItem(MEN_T(IDS_MENU_VIDEO_ECHO_ORIENTATION),	&m_pState->m_nVideoEchoOrientation, MENUITEMTYPE_INT,	MEN_TT(IDS_MENU_VIDEO_ECHO_ORIENTATION_TT), 0.0f, 3.0f);
	 m_menuPost.AddItem(MEN_T(IDS_MENU_TEXTURE_WRAP),			&m_pState->m_bTexWrap,              MENUITEMTYPE_BOOL,	MEN_TT(IDS_MENU_TEXTURE_WRAP_TT));
	m_menuPost.AddItem(MEN_T(IDS_MENU_FILTER_INVERT),			&m_pState->m_bInvert,			MENUITEMTYPE_BOOL, MEN_TT(IDS_MENU_FILTER_INVERT_TT));
	m_menuPost.AddItem(MEN_T(IDS_MENU_FILTER_BRIGHTEN),			&m_pState->m_bBrighten,			MENUITEMTYPE_BOOL, MEN_TT(IDS_MENU_FILTER_BRIGHTEN_TT));
	m_menuPost.AddItem(MEN_T(IDS_MENU_FILTER_DARKEN),			&m_pState->m_bDarken,			MENUITEMTYPE_BOOL, MEN_TT(IDS_MENU_FILTER_DARKEN_TT));
	m_menuPost.AddItem(MEN_T(IDS_MENU_FILTER_SOLARIZE),			&m_pState->m_bSolarize,			MENUITEMTYPE_BOOL, MEN_TT(IDS_MENU_FILTER_SOLARIZE_TT));
	m_menuPost.AddItem(MEN_T(IDS_MENU_BLUR1_EDGE_DARKEN_AMOUNT),&m_pState->m_fBlur1EdgeDarken,	MENUITEMTYPE_FLOAT,	MEN_TT(IDS_MENU_BLUR1_EDGE_DARKEN_AMOUNT_TT), 0.0f, 1.0f);
	m_menuPost.AddItem(MEN_T(IDS_MENU_BLUR1_MIN_COLOR_VALUE),	&m_pState->m_fBlur1Min,			MENUITEMTYPE_FLOAT,	MEN_TT(IDS_MENU_BLUR1_MIN_COLOR_VALUE_TT), 0.0f, 1.0f);
	m_menuPost.AddItem(MEN_T(IDS_MENU_BLUR1_MAX_COLOR_VALUE),	&m_pState->m_fBlur1Max,			MENUITEMTYPE_FLOAT, MEN_TT(IDS_MENU_BLUR1_MAX_COLOR_VALUE_TT), 0.0f, 1.0f);
	m_menuPost.AddItem(MEN_T(IDS_MENU_BLUR2_MIN_COLOR_VALUE),	&m_pState->m_fBlur2Min,			MENUITEMTYPE_FLOAT, MEN_TT(IDS_MENU_BLUR2_MIN_COLOR_VALUE_TT), 0.0f, 1.0f);
	m_menuPost.AddItem(MEN_T(IDS_MENU_BLUR2_MAX_COLOR_VALUE),	&m_pState->m_fBlur2Max,			MENUITEMTYPE_FLOAT, MEN_TT(IDS_MENU_BLUR2_MAX_COLOR_VALUE_TT), 0.0f, 1.0f);
	m_menuPost.AddItem(MEN_T(IDS_MENU_BLUR3_MIN_COLOR_VALUE),	&m_pState->m_fBlur3Min,			MENUITEMTYPE_FLOAT, MEN_TT(IDS_MENU_BLUR3_MIN_COLOR_VALUE_TT), 0.0f, 1.0f);
	m_menuPost.AddItem(MEN_T(IDS_MENU_BLUR3_MAX_COLOR_VALUE),	&m_pState->m_fBlur3Max,			MENUITEMTYPE_FLOAT, MEN_TT(IDS_MENU_BLUR3_MAX_COLOR_VALUE_TT), 0.0f, 1.0f);

    for (i=0; i<MAX_CUSTOM_WAVES; i++)
    {
        // blending: do both; fade opacities in/out (w/exagerrated weighting)
        m_menuWavecode[i].AddItem(MEN_T(IDS_MENU_ENABLED),			&m_pState->m_wave[i].enabled,	MENUITEMTYPE_BOOL,	MEN_TT(IDS_MENU_ENABLED_TT)); // bool
        m_menuWavecode[i].AddItem(MEN_T(IDS_MENU_NUMBER_OF_SAMPLES),&m_pState->m_wave[i].samples,	MENUITEMTYPE_INT,	MEN_TT(IDS_MENU_NUMBER_OF_SAMPLES_TT), 2, 512);        // 0-512
        m_menuWavecode[i].AddItem(MEN_T(IDS_MENU_L_R_SEPARATION),	&m_pState->m_wave[i].sep,		MENUITEMTYPE_INT,	MEN_TT(IDS_MENU_L_R_SEPARATION_TT), 0, 256);        // 0-512
        m_menuWavecode[i].AddItem(MEN_T(IDS_MENU_SCALING),			&m_pState->m_wave[i].scaling,	MENUITEMTYPE_LOGFLOAT, MEN_TT(IDS_MENU_SCALING_TT));
        m_menuWavecode[i].AddItem(MEN_T(IDS_MENU_SMOOTH),			&m_pState->m_wave[i].smoothing,	MENUITEMTYPE_FLOAT, MEN_TT(IDS_MENU_SMOOTHING_TT), 0, 1);
	    m_menuWavecode[i].AddItem(MEN_T(IDS_MENU_COLOR_RED),		&m_pState->m_wave[i].r,			MENUITEMTYPE_FLOAT, MEN_TT(IDS_MENU_COLOR_RED_TT), 0, 1);
	    m_menuWavecode[i].AddItem(MEN_T(IDS_MENU_COLOR_GREEN),		&m_pState->m_wave[i].g,			MENUITEMTYPE_FLOAT, MEN_TT(IDS_MENU_COLOR_GREEN_TT), 0, 1);
		m_menuWavecode[i].AddItem(MEN_T(IDS_MENU_COLOR_BLUE),		&m_pState->m_wave[i].b,			MENUITEMTYPE_FLOAT, MEN_TT(IDS_MENU_COLOR_BLUE_TT), 0, 1);
	    m_menuWavecode[i].AddItem(MEN_T(IDS_MENU_OPACITY),			&m_pState->m_wave[i].a,			MENUITEMTYPE_FLOAT, MEN_TT(IDS_MENU_OPACITY_WAVE_TT), 0, 1);
        m_menuWavecode[i].AddItem(MEN_T(IDS_MENU_USE_SPECTRUM),		&m_pState->m_wave[i].bSpectrum,	MENUITEMTYPE_BOOL,	MEN_TT(IDS_MENU_USE_SPECTRUM_TT));        // 0-5 [0=wave left, 1=wave center, 2=wave right; 3=spectrum left, 4=spec center, 5=spec right]
        m_menuWavecode[i].AddItem(MEN_T(IDS_MENU_USE_DOTS),			&m_pState->m_wave[i].bUseDots,	MENUITEMTYPE_BOOL,	MEN_TT(IDS_MENU_USE_DOTS_WAVE_TT)); // bool
        m_menuWavecode[i].AddItem(MEN_T(IDS_MENU_DRAW_THICK),		&m_pState->m_wave[i].bDrawThick,MENUITEMTYPE_BOOL,	MEN_TT(IDS_MENU_DRAW_THICK_WAVE_TT)); // bool
        m_menuWavecode[i].AddItem(MEN_T(IDS_MENU_ADDITIVE_DRAWING),	&m_pState->m_wave[i].bAdditive,	MENUITEMTYPE_BOOL,	MEN_TT(IDS_MENU_ADDITIVE_DRAWING_WAVE_TT)); // bool
        m_menuWavecode[i].AddItem(MEN_T(IDS_MENU_EXPORT_TO_FILE),	(void*)UI_EXPORT_WAVE,			MENUITEMTYPE_UIMODE,MEN_TT(IDS_MENU_EXPORT_TO_FILE_TT), 0, 0, NULL, UI_EXPORT_WAVE, i);
        m_menuWavecode[i].AddItem(MEN_T(IDS_MENU_IMPORT_FROM_FILE),	(void*)UI_IMPORT_WAVE,			MENUITEMTYPE_UIMODE,MEN_TT(IDS_MENU_IMPORT_FROM_FILE_TT), 0, 0, NULL, UI_IMPORT_WAVE, i);
        m_menuWavecode[i].AddItem(MEN_T(IDS_MENU_EDIT_INIT_CODE),	&m_pState->m_wave[i].m_szInit,	MENUITEMTYPE_STRING,MEN_TT(IDS_MENU_EDIT_INIT_CODE_TT), 256, 0, &OnUserEditedWavecodeInit, sizeof(m_pState->m_wave[i].m_szInit), 0);
        m_menuWavecode[i].AddItem(MEN_T(IDS_MENU_EDIT_PER_FRAME_CODE),	&m_pState->m_wave[i].m_szPerFrame,	MENUITEMTYPE_STRING, MEN_TT(IDS_MENU_EDIT_PER_FRAME_CODE_TT), 256, 0, &OnUserEditedWavecode, sizeof(m_pState->m_wave[i].m_szPerFrame), 0);
        m_menuWavecode[i].AddItem(MEN_T(IDS_MENU_EDIT_PER_POINT_CODE),	&m_pState->m_wave[i].m_szPerPoint,  MENUITEMTYPE_STRING, MEN_TT(IDS_MENU_EDIT_PER_POINT_CODE_TT), 256, 0, &OnUserEditedWavecode, sizeof(m_pState->m_wave[i].m_szPerPoint), 0);
    }

    for (i=0; i<MAX_CUSTOM_SHAPES; i++)
    {
        // blending: do both; fade opacities in/out (w/exagerrated weighting)
        m_menuShapecode[i].AddItem(MEN_T(IDS_MENU_ENABLED),				&m_pState->m_shape[i].enabled,	MENUITEMTYPE_BOOL,	MEN_TT(IDS_MENU_ENABLED_SHAPE_TT)); // bool
        m_menuShapecode[i].AddItem(MEN_T(IDS_MENU_NUMBER_OF_INSTANCES),	&m_pState->m_shape[i].instances,MENUITEMTYPE_INT,	MEN_TT(IDS_MENU_NUMBER_OF_INSTANCES_TT), 1, 1024);
        m_menuShapecode[i].AddItem(MEN_T(IDS_MENU_NUMBER_OF_SIDES),		&m_pState->m_shape[i].sides,	MENUITEMTYPE_INT,	MEN_TT(IDS_MENU_NUMBER_OF_SIDES_TT), 3, 100);
        m_menuShapecode[i].AddItem(MEN_T(IDS_MENU_DRAW_THICK),			&m_pState->m_shape[i].thickOutline,	MENUITEMTYPE_BOOL,	MEN_TT(IDS_MENU_DRAW_THICK_SHAPE_TT)); // bool
        m_menuShapecode[i].AddItem(MEN_T(IDS_MENU_ADDITIVE_DRAWING),	&m_pState->m_shape[i].additive,	MENUITEMTYPE_BOOL,	MEN_TT(IDS_MENU_ADDITIVE_DRAWING_SHAPE_TT)); // bool
	    m_menuShapecode[i].AddItem(MEN_T(IDS_MENU_X_POSITION),			&m_pState->m_shape[i].x,		MENUITEMTYPE_FLOAT, MEN_TT(IDS_MENU_X_POSITION_TT), 0, 1);
	    m_menuShapecode[i].AddItem(MEN_T(IDS_MENU_Y_POSITION),			&m_pState->m_shape[i].y,		MENUITEMTYPE_FLOAT, MEN_TT(IDS_MENU_Y_POSITION_TT), 0, 1);
	    m_menuShapecode[i].AddItem(MEN_T(IDS_MENU_RADIUS),				&m_pState->m_shape[i].rad,		MENUITEMTYPE_LOGFLOAT, MEN_TT(IDS_MENU_RADIUS_TT));
	    m_menuShapecode[i].AddItem(MEN_T(IDS_MENU_ANGLE),				&m_pState->m_shape[i].ang,		MENUITEMTYPE_FLOAT,	MEN_TT(IDS_MENU_ANGLE_TT), 0, 3.1415927f*2.0f);
        m_menuShapecode[i].AddItem(MEN_T(IDS_MENU_TEXTURED),			&m_pState->m_shape[i].textured,	MENUITEMTYPE_BOOL,	MEN_TT(IDS_MENU_TEXTURED_TT)); // bool
        m_menuShapecode[i].AddItem(MEN_T(IDS_MENU_TEXTURE_ZOOM),		&m_pState->m_shape[i].tex_zoom,	MENUITEMTYPE_LOGFLOAT, MEN_TT(IDS_MENU_TEXTURE_ZOOM_TT)); // bool
        m_menuShapecode[i].AddItem(MEN_T(IDS_MENU_TEXTURE_ANGLE),		&m_pState->m_shape[i].tex_ang,	MENUITEMTYPE_FLOAT,	MEN_TT(IDS_MENU_TEXTURE_ANGLE_TT), 0, 3.1415927f*2.0f); // bool
	    m_menuShapecode[i].AddItem(MEN_T(IDS_MENU_INNER_COLOR_RED),		&m_pState->m_shape[i].r,		MENUITEMTYPE_FLOAT, MEN_TT(IDS_MENU_INNER_COLOR_RED_TT), 0, 1);
	    m_menuShapecode[i].AddItem(MEN_T(IDS_MENU_INNER_COLOR_GREEN),	&m_pState->m_shape[i].g,		MENUITEMTYPE_FLOAT, MEN_TT(IDS_MENU_INNER_COLOR_GREEN_TT), 0, 1);
	    m_menuShapecode[i].AddItem(MEN_T(IDS_MENU_INNER_COLOR_BLUE),	&m_pState->m_shape[i].b,		MENUITEMTYPE_FLOAT, MEN_TT(IDS_MENU_INNER_COLOR_BLUE_TT), 0, 1);
	    m_menuShapecode[i].AddItem(MEN_T(IDS_MENU_INNER_OPACITY),		&m_pState->m_shape[i].a,		MENUITEMTYPE_FLOAT, MEN_TT(IDS_MENU_INNER_OPACITY_TT), 0, 1);
	    m_menuShapecode[i].AddItem(MEN_T(IDS_MENU_OUTER_COLOR_RED),		&m_pState->m_shape[i].r2,		MENUITEMTYPE_FLOAT, MEN_TT(IDS_MENU_OUTER_COLOR_RED_TT), 0, 1);
	    m_menuShapecode[i].AddItem(MEN_T(IDS_MENU_OUTER_COLOR_GREEN),	&m_pState->m_shape[i].g2,		MENUITEMTYPE_FLOAT, MEN_TT(IDS_MENU_OUTER_COLOR_GREEN_TT), 0, 1);
	    m_menuShapecode[i].AddItem(MEN_T(IDS_MENU_OUTER_COLOR_BLUE),	&m_pState->m_shape[i].b2,		MENUITEMTYPE_FLOAT, MEN_TT(IDS_MENU_OUTER_COLOR_BLUE_TT), 0, 1);
	    m_menuShapecode[i].AddItem(MEN_T(IDS_MENU_OUTER_OPACITY),		&m_pState->m_shape[i].a2,		MENUITEMTYPE_FLOAT, MEN_TT(IDS_MENU_OUTER_OPACITY_TT), 0, 1);
	    m_menuShapecode[i].AddItem(MEN_T(IDS_MENU_BORDER_COLOR_RED),	&m_pState->m_shape[i].border_r,	MENUITEMTYPE_FLOAT, MEN_TT(IDS_MENU_BORDER_COLOR_RED_TT), 0, 1);
	    m_menuShapecode[i].AddItem(MEN_T(IDS_MENU_BORDER_COLOR_GREEN),	&m_pState->m_shape[i].border_g,	MENUITEMTYPE_FLOAT, MEN_TT(IDS_MENU_BORDER_COLOR_GREEN_TT), 0, 1);
	    m_menuShapecode[i].AddItem(MEN_T(IDS_MENU_BORDER_COLOR_BLUE),	&m_pState->m_shape[i].border_b,	MENUITEMTYPE_FLOAT, MEN_TT(IDS_MENU_BORDER_COLOR_BLUE_TT), 0, 1);
	    m_menuShapecode[i].AddItem(MEN_T(IDS_MENU_BORDER_OPACITY),		&m_pState->m_shape[i].border_a,	MENUITEMTYPE_FLOAT, MEN_TT(IDS_MENU_BORDER_OPACITY_TT), 0, 1);
        m_menuShapecode[i].AddItem(MEN_T(IDS_MENU_EXPORT_TO_FILE),		NULL,							MENUITEMTYPE_UIMODE, MEN_TT(IDS_MENU_EXPORT_TO_FILE_SHAPE_TT), 0, 0, NULL, UI_EXPORT_SHAPE, i);
        m_menuShapecode[i].AddItem(MEN_T(IDS_MENU_IMPORT_FROM_FILE),	NULL,							MENUITEMTYPE_UIMODE, MEN_TT(IDS_MENU_IMPORT_FROM_FILE_SHAPE_TT), 0, 0, NULL, UI_IMPORT_SHAPE, i);
        m_menuShapecode[i].AddItem(MEN_T(IDS_MENU_EDIT_INIT_CODE),		&m_pState->m_shape[i].m_szInit, MENUITEMTYPE_STRING, MEN_TT(IDS_MENU_EDIT_INIT_CODE_SHAPE_TT), 256, 0, &OnUserEditedShapecodeInit, sizeof(m_pState->m_shape[i].m_szInit), 0);
        m_menuShapecode[i].AddItem(MEN_T(IDS_MENU_EDIT_PER_FRAME_INSTANCE_CODE),	&m_pState->m_shape[i].m_szPerFrame, MENUITEMTYPE_STRING, MEN_TT(IDS_MENU_EDIT_PER_FRAME_INSTANCE_CODE_TT), 256, 0, &OnUserEditedShapecode, sizeof(m_pState->m_shape[i].m_szPerFrame), 0);
    }
}

void CPlugin::WriteRealtimeConfig()
{
	WritePrivateProfileIntW(m_bShowFPS, L"bShowFPS",GetConfigIniFile(), L"settings");
	WritePrivateProfileIntW(m_bShowRating, L"bShowRating",GetConfigIniFile(), L"settings");
	WritePrivateProfileIntW(m_bShowPresetInfo, L"bShowPresetInfo",GetConfigIniFile(), L"settings");
	WritePrivateProfileIntW(m_bShowSongTitle, L"bShowSongTitle",GetConfigIniFile(), L"settings");
	WritePrivateProfileIntW(m_bShowSongTime, L"bShowSongTime",GetConfigIniFile(), L"settings");
	WritePrivateProfileIntW(m_bShowSongLen, L"bShowSongLen",GetConfigIniFile(), L"settings");
}

void CPlugin::dumpmsg(wchar_t *s)
{
    #if _DEBUG
        OutputDebugStringW(s);
        if (s[0])
        {
            int len = lstrlenW(s);
            if (s[len-1] != L'\n')
                OutputDebugStringW(L"\n");
        }
    #endif
}

void CPlugin::PrevPreset(float fBlendTime)
{
    if (m_bSequentialPresetOrder)
    {
        if (!m_playlist.IsAll())
        {
            // SURI: step back through the session playlist only
            const std::vector<int>& allowed = PlaylistIndices();
            if (allowed.empty())
                return;
            m_nCurrentPreset = PickSequential(allowed, m_nCurrentPreset, -1);
        }
        else
        {
		m_nCurrentPreset--;
		if (m_nCurrentPreset < m_nDirs)
            m_nCurrentPreset = m_nPresets-1;
        if (m_nCurrentPreset >= m_nPresets) // just in case
			m_nCurrentPreset = m_nDirs;
        }

        wchar_t szFile[MAX_PATH];
        lstrcpyW(szFile, m_szPresetDir);	// note: m_szPresetDir always ends with '\'
        lstrcatW(szFile, m_presets[m_nCurrentPreset].szFilename.c_str());

    	LoadPreset(szFile, fBlendTime);
    }
    else
    {
        int prev = (m_presetHistoryPos-1 + PRESET_HIST_LEN) % PRESET_HIST_LEN;
        if (m_presetHistoryPos != m_presetHistoryBackFence)
        {
            m_presetHistoryPos = prev;
            LoadPreset( m_presetHistory[m_presetHistoryPos].c_str(), fBlendTime); 
        }
    }
}

// SURI: the session playlist (phase 10)
void CPlugin::SetPlaylist(const std::vector<std::wstring>& names)
{
    if (names.empty())
        m_playlist.SetAll();
    else
        m_playlist.SetNames(names);
    ResetPresetHistory();
}

void CPlugin::ResetPresetHistory()
{
    m_presetHistoryBackFence = m_presetHistoryFwdFence = m_presetHistoryPos;
}

const std::vector<int>& CPlugin::PlaylistIndices()
{
    EnterCriticalSection(&g_cs);
    if (m_playlistIdxPresets != m_nPresets || m_playlistIdxDirs != m_nDirs ||
        m_playlistIdxGen != m_playlist.Generation())
    {
        std::vector<std::wstring> names;
        int n = min(m_nPresets, (int)m_presets.size());
        names.reserve(n > 0 ? n : 0);
        for (int i = 0; i < n; i++)
            names.push_back(m_presets[i].szFilename);
        m_playlistIdx = AllowedIndices(names, min(m_nDirs, n), m_playlist);
        m_playlistIdxPresets = m_nPresets;
        m_playlistIdxDirs = m_nDirs;
        m_playlistIdxGen = m_playlist.Generation();
    }
    LeaveCriticalSection(&g_cs);
    return m_playlistIdx;
}

// SURI: the on-screen playlist screen (P). Keys come from MyWindowProc while m_UI_mode is
// UI_PLAYLIST; it is drawn by MyRenderOverlay.
std::vector<std::wstring> CPlugin::LibraryFileNames()
{
    std::vector<std::wstring> names;
    EnterCriticalSection(&g_cs);
    int n = min(m_nPresets, (int)m_presets.size());
    int d = max(0, min(m_nDirs, n));
    names.reserve(n - d);
    for (int i = d; i < n; i++)
        names.push_back(m_presets[i].szFilename);
    LeaveCriticalSection(&g_cs);
    return names;
}

void CPlugin::OpenPlaylistScreen()
{
    UpdatePresetList();   // make sure the list is completely ready (as the L browser does)
    m_plLib = LibraryFileNames();
    m_plScreen.Open(m_plLib, m_playlist);
    m_plTop[0] = m_plTop[1] = 0;
    m_show_help = 0;
    m_nNumericInputNum = 0;      // a k / y waiting for its digits is dropped
    m_nNumericInputDigits = 0;
    m_UI_mode = UI_PLAYLIST;
}

void CPlugin::ClosePlaylistScreen()
{
    m_UI_mode = UI_REGULAR;
    if (!m_plLib.empty() && m_plScreen.Count(PlCol::Playing) == 0)
        AddError(LoadResString(IDS_PL_EMPTY_PLAYING_ALL), 3.0f, ERR_NOTIFY, false);
}

void CPlugin::OnPlaylistAction(PlAction a)
{
    switch (a)
    {
    case PlAction::Changed:
        // an empty list means "all" (also when everything was moved to Not playing)
        SetPlaylist(m_plScreen.AllPlaying() ? std::vector<std::wstring>() : m_plScreen.PlayingNames());
        break;
    case PlAction::Close: ClosePlaylistScreen(); break;
    case PlAction::Save:  SavePlaylistDialog();  break;
    case PlAction::Load:  LoadPlaylistDialog();  break;
    default: break;
    }
}

bool TranslateUiKey(WPARAM vk, UiKey* out)
{
    // AltGr comes as Ctrl+Alt: with Alt down it is not a Ctrl shortcut (AltGr+S etc. type text)
    const bool ctrl  = (GetKeyState(VK_CONTROL) & 0x8000) != 0 && (GetKeyState(VK_MENU) & 0x8000) == 0;
    const bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
    switch (vk)
    {
    case VK_UP:     *out = shift ? UiKey::ShiftUp : UiKey::Up;     return true;
    case VK_DOWN:   *out = shift ? UiKey::ShiftDown : UiKey::Down; return true;
    case VK_PRIOR:  *out = UiKey::PageUp;    return true;
    case VK_NEXT:   *out = UiKey::PageDown;  return true;
    case VK_HOME:   *out = UiKey::Home;      return true;
    case VK_END:    *out = UiKey::End;       return true;
    case VK_TAB:    *out = UiKey::Tab;       return true;
    case VK_SPACE:  *out = ctrl ? UiKey::CtrlSpace : UiKey::Space; return true;
    case VK_LEFT:   *out = UiKey::Left;      return true;
    case VK_RIGHT:  *out = UiKey::Right;     return true;
    case VK_RETURN: *out = UiKey::Enter;     return true;
    case VK_BACK:   *out = UiKey::Backspace; return true;
    case VK_ESCAPE: *out = UiKey::Esc;       return true;
    case 'A': if (ctrl) { *out = UiKey::CtrlA; return true; } return false;
    case 'S': if (ctrl) { *out = UiKey::CtrlS; return true; } return false;
    case 'O': if (ctrl) { *out = UiKey::CtrlO; return true; } return false;
    default:        return false;
    }
}

// A file dialog would open under a full-screen (topmost) window: leave full screen through the
// window's own command first, and go back afterwards.
static bool LeaveFullscreenForDialog(CPlugin* p, HWND hwnd)
{
    if (!p->m_pfnIsFullscreen || !p->m_pfnRunCommand || !p->m_pfnIsFullscreen())
        return false;
    p->m_pfnRunCommand(hwnd, Cmd::ViewFullscreen);
    return true;
}

static void ReturnToFullscreen(CPlugin* p, HWND hwnd, bool was)
{
    if (was && !p->m_pfnIsFullscreen())
        p->m_pfnRunCommand(hwnd, Cmd::ViewFullscreen);
}

// Fills ofn for a playlist file dialog: filter "Playlists (*.m3u)", start in playlists\ (created).
static void PreparePlaylistDialog(OPENFILENAMEW* ofn, HWND hwnd, const wchar_t* milkdrop2Path,
                                  wchar_t* dir, size_t dirLen, wchar_t* filter, size_t filterLen,
                                  wchar_t* file, size_t fileLen)
{
    StringCchPrintfW(dir, dirLen, L"%splaylists", milkdrop2Path);
    CreateDirectoryW(dir, NULL);   // fails harmlessly when it is already there

    // "Playlists (*.m3u)\0*.m3u\0\0"
    wchar_t label[128];
    LoadResString(IDS_PL_FILE_FILTER, label, 128);
    ZeroMemory(filter, filterLen * sizeof(wchar_t));
    size_t n = wcslen(label);
    if (n + 8 < filterLen)
    {
        memcpy(filter, label, n * sizeof(wchar_t));
        memcpy(filter + n + 1, L"*.m3u", 5 * sizeof(wchar_t));
    }

    ZeroMemory(ofn, sizeof(*ofn));
    ofn->lStructSize = sizeof(*ofn);
    ofn->hwndOwner = hwnd;
    ofn->lpstrFilter = filter;
    ofn->lpstrFile = file;
    ofn->nMaxFile = (DWORD)fileLen;
    ofn->lpstrInitialDir = dir;
    ofn->lpstrDefExt = L"m3u";
}

void CPlugin::SavePlaylistDialog()
{
    HWND hwnd = GetPluginWindow();
    wchar_t dir[MAX_PATH], filter[160], file[MAX_PATH] = L"";
    OPENFILENAMEW ofn;
    PreparePlaylistDialog(&ofn, hwnd, m_szMilkdrop2Path, dir, MAX_PATH, filter, 160, file, MAX_PATH);
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR;   // the user confirms an overwrite there

    bool wasFullscreen = LeaveFullscreenForDialog(this, hwnd);
    BOOL ok = GetSaveFileNameW(&ofn);
    ReturnToFullscreen(this, hwnd, wasFullscreen);
    if (!ok)
        return;   // cancelled

    std::string bytes = SerializeM3u(m_plScreen.PlayingNames());
    bool saved = false;
    HANDLE h = CreateFileW(file, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h != INVALID_HANDLE_VALUE)
    {
        DWORD written = 0;
        saved = WriteFile(h, bytes.data(), (DWORD)bytes.size(), &written, NULL) && written == (DWORD)bytes.size();
        CloseHandle(h);
    }
    if (!saved)
        AddError(LoadResString(IDS_PL_SAVE_FAILED), 4.0f, ERR_MISC, true);
}

void CPlugin::LoadPlaylistDialog()
{
    HWND hwnd = GetPluginWindow();
    wchar_t dir[MAX_PATH], filter[160], file[MAX_PATH] = L"";
    OPENFILENAMEW ofn;
    PreparePlaylistDialog(&ofn, hwnd, m_szMilkdrop2Path, dir, MAX_PATH, filter, 160, file, MAX_PATH);
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR;

    bool wasFullscreen = LeaveFullscreenForDialog(this, hwnd);
    BOOL ok = GetOpenFileNameW(&ofn);
    ReturnToFullscreen(this, hwnd, wasFullscreen);
    if (!ok)
        return;   // cancelled

    // read the whole file (a playlist of the full library is well under 1 MB)
    std::string bytes;
    bool read = false;
    HANDLE h = CreateFileW(file, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h != INVALID_HANDLE_VALUE)
    {
        LARGE_INTEGER size;
        if (GetFileSizeEx(h, &size) && size.QuadPart < 64 * 1024 * 1024)
        {
            bytes.resize((size_t)size.QuadPart);
            DWORD got = 0;
            read = bytes.empty() || (ReadFile(h, &bytes[0], (DWORD)bytes.size(), &got, NULL) && got == (DWORD)bytes.size());
        }
        CloseHandle(h);
    }
    if (!read)
    {
        AddError(LoadResString(IDS_PL_LOAD_FAILED), 4.0f, ERR_MISC, true);
        return;
    }

    std::vector<std::wstring> library = LibraryFileNames();
    std::vector<std::wstring> names;
    LoadCount c = MatchToLibrary(ParseM3u(bytes), library, &names);
    std::wstring msg = LoadedMessage(c);
    AddError(&msg[0], 3.0f, ERR_NOTIFY, false);
    if (c.loaded == 0)
        return;   // nothing of it is in the library: the playlist stays as it was

    SetPlaylist(names);
    m_plLib = library;
    m_plScreen.Open(m_plLib, m_playlist);
    m_plTop[0] = m_plTop[1] = 0;

    // the running preset is not in the new playlist: go to its first preset
    const wchar_t* cur = wcsrchr(m_szCurrentPresetFile, L'\\');
    cur = cur ? cur + 1 : m_szCurrentPresetFile;
    if (!m_playlist.Contains(cur))
    {
        const std::vector<int>& allowed = PlaylistIndices();
        if (!allowed.empty())
            LoadLibraryPreset(allowed[0]);
    }
}

// m_presets[index] with the user's blend time. After SetPlaylist the history is empty: step the
// history position only when it is not (as LoadRandomPreset does), so this load is recorded.
void CPlugin::LoadLibraryPreset(int index)
{
    // the preset list can be rebuilt by the background scan: copy the name under g_cs
    EnterCriticalSection(&g_cs);
    const bool ok = index >= m_nDirs && index < m_nPresets && index < (int)m_presets.size();
    std::wstring name = ok ? m_presets[index].szFilename : std::wstring();
    LeaveCriticalSection(&g_cs);
    if (!ok)
        return;
    m_nCurrentPreset = index;
    std::wstring path = std::wstring(m_szPresetDir) + name;   // m_szPresetDir ends with '\'
    if (m_presetHistoryFwdFence != m_presetHistoryBackFence)
        m_presetHistoryPos = (m_presetHistoryPos+1) % PRESET_HIST_LEN;
    LoadPreset(path.c_str(), m_fBlendTimeUser);
}

// ---------------------------------------------------------------------------------------------
// SURI: drag & drop onto the window (phase 10)

// The .milk files at the top level of dir (ends with '\'), sorted by name (case-insensitive).
static void FindMilkFiles(const std::wstring& dir, std::vector<LibraryEntry>* out)
{
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW((dir + L"*.milk").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE)
        return;
    do
    {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            continue;
        if (ClassifyDropPath(fd.cFileName, false) != DropKind::Preset)
            continue;   // the wildcard also matches longer extensions through short names
        LibraryEntry e;
        e.name = fd.cFileName;
        e.size = ((unsigned long long)fd.nFileSizeHigh << 32) | fd.nFileSizeLow;
        out->push_back(e);
    }
    while (FindNextFileW(h, &fd));
    FindClose(h);
    std::sort(out->begin(), out->end(),
              [](const LibraryEntry& a, const LibraryEntry& b) { return _wcsicmp(a.name.c_str(), b.name.c_str()) < 0; });
}

static bool FileSize(const std::wstring& path, unsigned long long* size)
{
    WIN32_FILE_ATTRIBUTE_DATA fa;
    if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &fa) || (fa.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
        return false;
    *size = ((unsigned long long)fa.nFileSizeHigh << 32) | fa.nFileSizeLow;
    return true;
}

static std::wstring WithSlash(const std::wstring& dir)
{
    if (!dir.empty() && dir[dir.size() - 1] != L'\\' && dir[dir.size() - 1] != L'/')
        return dir + L"\\";
    return dir;
}

void CPlugin::OnDropFiles(HDROP h)
{
    std::vector<std::wstring> paths;
    const UINT n = DragQueryFileW(h, 0xFFFFFFFF, NULL, 0);
    for (UINT i = 0; i < n; i++)
    {
        UINT len = DragQueryFileW(h, i, NULL, 0);
        if (len == 0)
            continue;
        std::wstring p(len + 1, L'\0');
        UINT got = DragQueryFileW(h, i, &p[0], len + 1);
        p.resize(got);
        if (!p.empty())
            paths.push_back(p);
    }
    DragFinish(h);

    // 1. only over the plain visualizer: no screen, menu, prompt or text edit open
    if (!CanAcceptDrop(m_UI_mode == UI_REGULAR && !m_waitstring.bActive))
    {
        AddError(LoadResString(IDS_DROP_CLOSE_SCREEN), 3.0f, ERR_NOTIFY, false);
        return;
    }

    // 2. folders give their top-level .milk files; images are only counted
    std::vector<DroppedFile> files;
    int images = 0;
    bool singlePreset = false;
    for (size_t i = 0; i < paths.size(); i++)
    {
        const std::wstring& p = paths[i];
        DWORD attr = GetFileAttributesW(p.c_str());
        if (attr == INVALID_FILE_ATTRIBUTES)
            continue;
        switch (ClassifyDropPath(p, (attr & FILE_ATTRIBUTE_DIRECTORY) != 0))
        {
        case DropKind::Folder:
            {
                std::wstring dir = WithSlash(p);
                std::vector<LibraryEntry> found;
                FindMilkFiles(dir, &found);
                for (size_t j = 0; j < found.size(); j++)
                {
                    DroppedFile f;
                    f.sourcePath = dir + found[j].name;
                    f.size = found[j].size;
                    files.push_back(f);
                }
            }
            break;
        case DropKind::Preset:
            {
                DroppedFile f;
                f.sourcePath = p;
                if (FileSize(p, &f.size))
                {
                    files.push_back(f);
                    singlePreset = paths.size() == 1;
                }
            }
            break;
        case DropKind::Image:
            images++;
            break;
        default:
            break;
        }
    }
    if (files.empty())
    {
        AddError(LoadResString(images > 0 ? IDS_DROP_IMAGES : IDS_DROP_NO_PRESETS), 3.0f, ERR_NOTIFY, false);
        return;
    }

    // 3. one .milk file: load it and leave the playlist alone
    m_importSingle = singlePreset;

    // 4. what is new against the library (name + size)
    std::vector<LibraryEntry> library;
    FindMilkFiles(m_szPresetDir, &library);
    std::vector<ImportItem> plan = PlanImport(files, library);
    std::vector<ImportItem> newItems;
    m_importExisting.clear();
    for (size_t i = 0; i < plan.size(); i++)
        (plan[i].isNew ? newItems : m_importExisting).push_back(plan[i]);

    // 5. new ones: ask which to add first; otherwise apply right away (nothing to copy)
    if (!newItems.empty())
    {
        m_importPrompt.Open(newItems);
        m_importTop = 0;
        m_show_help = 0;
        m_nNumericInputNum = 0;      // a k / y waiting for its digits is dropped
        m_nNumericInputDigits = 0;
        m_UI_mode = UI_IMPORT_NEW;
    }
    else
        FinishImport(PromptResult::Skip);
}

// For DecideTextureCopy: like the loader's GetFileAttributesW test, anything at the path counts
// (a folder too); returns its size, or -1 when there is nothing.
static long long LoaderFileSize(const std::wstring& path)
{
    WIN32_FILE_ATTRIBUTE_DATA fa;
    if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &fa))
        return -1;
    if (fa.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        return 0;
    return (long long)(((unsigned long long)fa.nFileSizeHigh << 32) | fa.nFileSizeLow);
}

// The textures a preset being imported refers to ("sampler_<root>"), looked up next to it and in
// its textures\ folder, copied into MilkDrop's textures\ folder. Never overwrites and never
// shadows: when the loader would already find something for the root (any extension, textures\
// or the preset folder) nothing is copied; unless it is the same file, it is counted as kept.
void CPlugin::CopyPresetTextures(const std::wstring& presetPath, int* copied, int* kept, int* failed)
{
    std::string text;
    HANDLE h = CreateFileW(presetPath.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE)
        return;
    LARGE_INTEGER size;
    if (GetFileSizeEx(h, &size) && size.QuadPart > 0 && size.QuadPart < 16 * 1024 * 1024)
    {
        text.resize((size_t)size.QuadPart);
        DWORD got = 0;
        if (!ReadFile(h, &text[0], (DWORD)text.size(), &got, NULL))
            got = 0;
        text.resize(got);
    }
    CloseHandle(h);

    std::vector<std::wstring> roots = TextureRoots(text);
    if (roots.empty())
        return;
    size_t slash = presetPath.find_last_of(L"\\/");
    const std::wstring dir = (slash == std::wstring::npos) ? std::wstring() : presetPath.substr(0, slash + 1);
    const std::wstring texDir = std::wstring(m_szMilkdrop2Path) + L"textures\\";
    const int nExt = sizeof(texture_exts) / sizeof(texture_exts[0]);
    const std::vector<std::wstring> exts(texture_exts, texture_exts + nExt);

    for (size_t r = 0; r < roots.size(); r++)
    {
        std::wstring src, ext;
        unsigned long long srcSize = 0;
        for (int pass = 0; pass < 2 && src.empty(); pass++)
        {
            const std::wstring base = (pass == 0) ? dir : dir + L"textures\\";
            for (int z = 0; z < nExt; z++)
            {
                std::wstring cand = base + roots[r] + L"." + texture_exts[z];
                if (FileSize(cand, &srcSize))
                {
                    src = cand;
                    ext = texture_exts[z];
                    break;
                }
            }
        }
        if (src.empty())
            continue;   // not on the dropped side: probably the user has it already

        // what the loader would find for this root now (any extension, textures\ then the preset
        // folder): anything there wins, so copying would change presets that use it
        switch (DecideTextureCopy(roots[r], ext, (long long)srcSize, texDir, m_szPresetDir, exts, LoaderFileSize))
        {
        case TextureCopy::SameFile: continue;
        case TextureCopy::Kept:     (*kept)++; continue;
        default:                    break;
        }
        const std::wstring dst = texDir + roots[r] + L"." + ext;
        CreateDirectoryW(texDir.c_str(), NULL);   // normally there already
        if (CopyFileW(src.c_str(), dst.c_str(), TRUE))
            (*copied)++;
        else
            (*failed)++;
    }
}

void CPlugin::FinishImport(PromptResult r)
{
    m_UI_mode = UI_REGULAR;

    // copy the checked new presets (and their textures) into the library; never overwrite
    std::vector<ImportItem> copied;
    int texCopied = 0, kept = 0, failed = 0;
    if (r == PromptResult::Accept)
    {
        std::vector<ImportItem> checked = m_importPrompt.Checked();
        for (size_t i = 0; i < checked.size(); i++)
        {
            const std::wstring dst = std::wstring(m_szPresetDir) + checked[i].targetName;
            if (!CopyFileW(checked[i].sourcePath.c_str(), dst.c_str(), TRUE))
            {
                failed++;
                continue;
            }
            copied.push_back(checked[i]);
            CopyPresetTextures(checked[i].sourcePath, &texCopied, &kept, &failed);
        }
    }
    if (!copied.empty())
        UpdatePresetList(false, true, true);

    wchar_t buf[256];
    if (kept > 0)
    {
        swprintf(buf, 256, LoadResString(IDS_DROP_TEXTURES_KEPT), kept);
        AddError(buf, 4.0f, ERR_NOTIFY, false);
    }
    if (failed > 0)
    {
        swprintf(buf, 256, LoadResString(IDS_DROP_COPY_FAILED), failed);
        AddError(buf, 4.0f, ERR_MISC, true);
    }

    // apply: the presets that were there plus the ones just copied
    std::vector<std::wstring> names;
    for (size_t i = 0; i < m_importExisting.size(); i++)
        names.push_back(m_importExisting[i].targetName);
    for (size_t i = 0; i < copied.size(); i++)
        names.push_back(copied[i].targetName);
    m_importExisting.clear();
    if (names.empty())
        return;   // everything was new and skipped: nothing changes

    if (m_importSingle)
    {
        // the one dropped file: load it, the playlist stays as it is
        int found = -1;
        EnterCriticalSection(&g_cs);
        for (int i = max(0, m_nDirs); i < m_nPresets && i < (int)m_presets.size(); i++)
            if (_wcsicmp(m_presets[i].szFilename.c_str(), names[0].c_str()) == 0)
            {
                found = i;
                break;
            }
        LeaveCriticalSection(&g_cs);
        if (found >= 0)
            LoadLibraryPreset(found);
        else
            LoadPreset((std::wstring(m_szPresetDir) + names[0]).c_str(), m_fBlendTimeUser);
        return;
    }

    // a group: it becomes the session playlist; its first preset (by name) starts now
    SetPlaylist(names);
    const std::vector<int>& allowed = PlaylistIndices();
    if (!allowed.empty())
        LoadLibraryPreset(allowed[0]);
    swprintf(buf, 256, LoadResString(IDS_DROP_PLAYLIST_N), (int)m_playlist.Names().size());
    AddError(buf, 3.0f, ERR_NOTIFY, false);
}

// Design B of phase 10: "N new presets", a line under it, the list with a check box per row (the
// new name after an arrow when it is numbered), and the key line. Scrolls when it is longer than
// the card.
void CPlugin::DrawImportPrompt()
{
    LPD3DXFONT fTitle = GetUiFont(UIF_TITLE), fMeta = GetUiFont(UIF_META), fSmall = GetUiFont(UIF_SMALL);
    if (!fTitle || !fMeta || !fSmall)
        return;
    const size_t n = m_importPrompt.Size();

    const int mx = GetCanvasMarginX(), my = GetCanvasMarginY();
    const int visW = GetWidth() - 2*mx, visH = GetHeight() - 2*my;
    RECT mr = { 0, 0, 4096, 0 };
    const int titleH = UiMeasure(fTitle, L"Ag", &mr, DT_SINGLELINE);
    SetRect(&mr, 0, 0, 4096, 0);
    const int metaH = UiMeasure(fMeta, L"Ag", &mr, DT_SINGLELINE);
    SetRect(&mr, 0, 0, 4096, 0);
    const int smallH = UiMeasure(fSmall, L"Ag", &mr, DT_SINGLELINE);

    const int pad = UiPx(20), rowH = metaH + UiPx(6), listPadY = UiPx(4);
    const int gapSub = UiPx(4), gapList = UiPx(12), gapFoot = UiPx(12);
    const int fixedH = 2*pad + titleH + gapSub + metaH + gapList + 2*listPadY + gapFoot + smallH;
    int maxRows = (visH * 8 / 10 - fixedH) / rowH;
    if (maxRows < 1)
        maxRows = 1;
    const int rows = (int)min((size_t)maxRows, max(n, (size_t)1));
    m_importPageRows = rows;

    const int cardW = min(UiPx(640), visW * 9 / 10), cardH = fixedH + rows * rowH;
    if (cardW <= 0 || visH <= 0)
        return;
    RECT card;
    card.left = mx + (visW - cardW) / 2;
    card.top  = my + (visH - cardH) / 2;
    card.right  = card.left + cardW;
    card.bottom = card.top + cardH;
    DrawRoundedPanel(&card, UiPx(14), CARD_FILL_PANEL, CARD_BORDER);

    const int inL = card.left + pad, inR = card.right - pad, inT = card.top + pad, inB = card.bottom - pad;
    wchar_t fmt[128], buf[256];

    LoadResString(n == 1 ? IDS_DROP_NEW_TITLE_1 : IDS_DROP_NEW_TITLE, fmt, 128);
    swprintf(buf, 256, fmt, (int)n);
    RECT tr = { inL, inT, inR, inT + titleH };
    UiText(fTitle, buf, &tr, DT_SINGLELINE, CARD_TEXT);

    LoadResString(IDS_DROP_NEW_SUBTITLE, buf, 256);
    const int subTop = inT + titleH + gapSub;
    RECT sr = { inL, subTop, inR, subTop + metaH };
    UiText(fMeta, buf, &sr, DT_SINGLELINE, CARD_TEXT_2);

    // the list: keep the cursor row in view, draw only the rows that fit
    const int listTop = subTop + metaH + gapList;
    RECT lb = { inL, listTop, inR, listTop + 2*listPadY + rows * rowH };
    DrawRoundedPanel(&lb, UiPx(8), 0x0DFFFFFF, 0);
    const size_t page = (size_t)rows, cur = m_importPrompt.Cursor();
    size_t& top = m_importTop;
    if (cur < top)
        top = cur;
    if (cur >= top + page)
        top = cur - page + 1;
    if (top + page > n)
        top = (n > page) ? n - page : 0;
    const bool scroll = n > page;
    SetRect(&mr, 0, 0, 4096, 0);
    const int boxW = UiMeasure(fMeta, L"\u2611", &mr, DT_SINGLELINE);
    for (size_t r = 0; r < page && top + r < n; r++)
    {
        const size_t i = top + r;
        const int y = listTop + listPadY + (int)r * rowH;
        RECT rr = { inL + UiPx(4), y, inR - UiPx(scroll ? 10 : 4), y + rowH };
        const bool isCur = (i == cur);
        if (isCur)
            DrawRoundedPanel(&rr, UiPx(6), CARD_BAND, 0);
        const bool on = m_importPrompt.IsChecked(i);
        const int ty = y + (rowH - metaH) / 2;
        RECT bx = { rr.left + UiPx(8), ty, rr.left + UiPx(8) + boxW, ty + metaH };
        UiText(fMeta, on ? L"\u2611" : L"\u2610", &bx, DT_SINGLELINE | DT_CENTER | DT_VCENTER, on ? CARD_DOT_NOTIFY : CARD_TEXT_2);

        const ImportItem& it = m_importPrompt.Item(i);
        RECT tx = { bx.right + UiPx(8), ty, rr.right - UiPx(8), ty + metaH };
        UiText(fMeta, it.name.c_str(), &tx, DT_SINGLELINE, isCur ? CARD_TEXT : CARD_TEXT_2);
        if (it.targetName != it.name)
        {
            RECT nw = { 0, 0, 4096, 0 };
            UiMeasure(fMeta, it.name.c_str(), &nw, DT_SINGLELINE);
            std::wstring note = L"\u2192 " + it.targetName;
            RECT nr = { tx.left + (nw.right - nw.left) + UiPx(8), ty, tx.right, ty + metaH };
            if (nr.left < nr.right)
                UiText(fMeta, note.c_str(), &nr, DT_SINGLELINE, CARD_STAR);
        }
    }
    if (scroll)
    {
        const int trackTop = listTop + listPadY, trackH = rows * rowH;
        int thumbH = (int)((long long)trackH * page / n);
        if (thumbH < UiPx(16))
            thumbH = UiPx(16);
        const int thumbY = trackTop + (int)((long long)(trackH - thumbH) * top / (n - page));
        RECT th = { inR - UiPx(7), thumbY, inR - UiPx(3), thumbY + thumbH };
        DrawRoundedPanel(&th, UiPx(2), 0x40FFFFFF, 0);
    }

    swprintf(buf, 256, L"Space toggle \u00B7 Ctrl+A all \u00B7 Enter add %d \u00B7 Esc skip", (int)m_importPrompt.CheckedCount());
    RECT fr = { inL, inB - smallH, inR, inB };
    UiText(fSmall, buf, &fr, DT_SINGLELINE, CARD_TEXT_2);
}

static std::wstring GroupThousands(size_t n)
{
    std::wstring s = std::to_wstring((unsigned long long)n);
    for (int i = (int)s.size() - 3; i > 0; i -= 3)
        s.insert((size_t)i, L",");
    return s;
}

// Design B of phase 10 (on the phase 6 card style): title, two columns (Not playing | Playing),
// each with its name, count and search line over the list; the move keys between them; the key
// line at the bottom. Only the rows that fit are drawn.
void CPlugin::DrawPlaylistScreen()
{
    LPD3DXFONT fTitle = GetUiFont(UIF_TITLE), fHead = GetUiFont(UIF_TAB_ON), fMeta = GetUiFont(UIF_META);
    LPD3DXFONT fSmall = GetUiFont(UIF_SMALL), fArrow = GetUiFont(UIF_STARS);
    if (!fTitle || !fHead || !fMeta || !fSmall || !fArrow)
        return;

    const int mx = GetCanvasMarginX(), my = GetCanvasMarginY();
    const int visW = GetWidth() - 2*mx, visH = GetHeight() - 2*my;
    const int cardW = min(UiPx(980), visW * 9 / 10), cardH = visH * 8 / 10;
    if (cardW <= 0 || cardH <= 0)
        return;
    RECT card;
    card.left = mx + (visW - cardW) / 2;
    card.top  = my + (visH - cardH) / 2;
    card.right  = card.left + cardW;
    card.bottom = card.top + cardH;
    DrawRoundedPanel(&card, UiPx(14), CARD_FILL_PANEL, CARD_BORDER);

    const int pad = UiPx(20);
    const int inL = card.left + pad, inR = card.right - pad, inT = card.top + pad, inB = card.bottom - pad;
    RECT mr = { 0, 0, 4096, 0 };
    const int titleH = UiMeasure(fTitle, L"Ag", &mr, DT_SINGLELINE);
    SetRect(&mr, 0, 0, 4096, 0);
    const int headH = UiMeasure(fHead, L"Ag", &mr, DT_SINGLELINE);
    SetRect(&mr, 0, 0, 4096, 0);
    const int metaH = UiMeasure(fMeta, L"Ag", &mr, DT_SINGLELINE);
    SetRect(&mr, 0, 0, 4096, 0);
    const int smallH = UiMeasure(fSmall, L"Ag", &mr, DT_SINGLELINE);
    SetRect(&mr, 0, 0, 4096, 0);
    const int keyH = UiMeasure(fArrow, L"\u2192", &mr, DT_SINGLELINE);

    wchar_t title[64], colName[2][64], hint[64];
    LoadResString(IDS_PL_TITLE, title, 64);
    LoadResString(IDS_PL_NOT_PLAYING, colName[0], 64);
    LoadResString(IDS_PL_PLAYING, colName[1], 64);
    LoadResString(IDS_PL_TYPE_TO_SEARCH, hint, 64);

    RECT tr = { inL, inT, inR, inT + titleH };
    UiText(fTitle, title, &tr, DT_SINGLELINE, CARD_TEXT);

    static const wchar_t kFooter[] = L"Tab side \u00B7 Ctrl+A all \u00B7 Ctrl+S save \u00B7 Ctrl+O load \u00B7 Esc close";
    RECT fr = { inL, inB - smallH, inR, inB };
    UiText(fSmall, kFooter, &fr, DT_SINGLELINE, CARD_TEXT_2);

    const int gutter = UiPx(64);
    const int colW = (inR - inL - gutter) / 2;
    const int colsTop = inT + titleH + UiPx(14), colsBottom = inB - smallH - UiPx(14);
    const int searchTop = colsTop + headH + UiPx(6), searchH = metaH + UiPx(10);
    const int listTop = searchTop + searchH + UiPx(8), listPadY = UiPx(4);
    const int rowH = metaH + UiPx(6);
    int rows = (colsBottom - listTop - 2*listPadY) / rowH;
    if (rows < 1)
        rows = 1;
    m_plPageRows = rows;

    const wchar_t* running = wcsrchr(m_szCurrentPresetFile, L'\\');
    running = running ? running + 1 : m_szCurrentPresetFile;

    for (int c = 0; c < 2; c++)
    {
        const PlCol col = (PlCol)c;
        const bool active = m_plScreen.Active() == col;
        const int x0 = (c == 0) ? inL : inR - colW, x1 = x0 + colW;

        // name and count
        RECT hr = { x0 + UiPx(2), colsTop, x1 - UiPx(2), colsTop + headH };
        UiText(fHead, colName[c], &hr, DT_SINGLELINE, active ? CARD_TEXT : CARD_TEXT_2);
        std::wstring count = GroupThousands(m_plScreen.Count(col));
        RECT cr = { x0, colsTop + (headH - metaH) / 2, x1 - UiPx(2), colsTop + (headH - metaH) / 2 + metaH };
        UiText(fMeta, count.c_str(), &cr, DT_SINGLELINE | DT_RIGHT, CARD_TEXT_2);

        // search line
        RECT sb = { x0, searchTop, x1, searchTop + searchH };
        DrawRoundedPanel(&sb, UiPx(6), 0x14FFFFFF, active ? ((CARD_ACCENT & 0x00FFFFFF) | 0x80000000) : CARD_BORDER);
        const std::wstring& filter = m_plScreen.Filter(col);
        const wchar_t* sText = filter.empty() ? hint : filter.c_str();
        RECT st = { sb.left + UiPx(10), sb.top + (searchH - metaH) / 2, sb.right - UiPx(10), sb.top + (searchH - metaH) / 2 + metaH };
        RECT sw = { 0, 0, 4096, 0 };
        UiMeasure(fMeta, sText, &sw, DT_SINGLELINE);
        // a search longer than the line shows its end
        DWORD sAlign = (sw.right - sw.left > st.right - st.left) ? DT_RIGHT : 0;
        UiText(fMeta, sText, &st, DT_SINGLELINE | sAlign, CARD_TEXT_2);

        // the list: keep the cursor row in view, draw only the rows that fit
        RECT lb = { x0, listTop, x1, colsBottom };
        DrawRoundedPanel(&lb, UiPx(8), 0x0DFFFFFF, 0);
        const std::vector<size_t> vis = m_plScreen.Visible(col);
        const size_t n = vis.size(), page = (size_t)rows, cur = m_plScreen.Cursor(col);
        size_t& top = m_plTop[c];
        if (cur < top)
            top = cur;
        if (cur >= top + page)
            top = cur - page + 1;
        if (top + page > n)
            top = (n > page) ? n - page : 0;
        const bool scroll = n > page;
        for (size_t r = 0; r < page && top + r < n; r++)
        {
            const size_t li = vis[top + r];
            const int y = listTop + listPadY + (int)r * rowH;
            RECT rr = { x0 + UiPx(4), y, x1 - UiPx(scroll ? 10 : 4), y + rowH };
            const bool sel = m_plScreen.IsSelected(li);
            const bool isCur = active && (top + r == cur);
            if (sel)
                DrawRoundedPanel(&rr, UiPx(6), CARD_BAND, 0);
            if (isCur)
                DrawRoundedPanel(&rr, UiPx(6), 0, CARD_ACCENT);
            const std::wstring& name = m_plLib[li];
            DWORD color = (name == running) ? CARD_STAR : ((sel || isCur) ? CARD_TEXT : CARD_TEXT_2);
            RECT tx = { rr.left + UiPx(8), y + (rowH - metaH) / 2, rr.right - UiPx(8), y + (rowH - metaH) / 2 + metaH };
            UiText(fMeta, name.c_str(), &tx, DT_SINGLELINE, color);
        }
        if (scroll)
        {
            const int trackTop = listTop + listPadY, trackH = colsBottom - listTop - 2*listPadY;
            int thumbH = (int)((long long)trackH * page / n);
            if (thumbH < UiPx(16))
                thumbH = UiPx(16);
            const int thumbY = trackTop + (int)((long long)(trackH - thumbH) * top / (n - page));
            RECT th = { x1 - UiPx(7), thumbY, x1 - UiPx(3), thumbY + thumbH };
            DrawRoundedPanel(&th, UiPx(2), 0x40FFFFFF, 0);
        }
    }

    // the move keys between the columns
    const int gx = inL + colW, gw = (inR - colW) - gx, midY = (listTop + colsBottom) / 2;
    const int capW = UiPx(36), capH = keyH + UiPx(8);
    const wchar_t* arrows[2] = { L"\u2192", L"\u2190" };
    for (int k = 0; k < 2; k++)
    {
        const int y = (k == 0) ? midY - capH - UiPx(4) : midY + UiPx(4);
        RECT cap = { gx + (gw - capW) / 2, y, gx + (gw - capW) / 2 + capW, y + capH };
        DrawRoundedPanel(&cap, UiPx(6), CARD_KEYCAP, 0);
        UiText(fArrow, arrows[k], &cap, DT_SINGLELINE | DT_CENTER | DT_VCENTER, CARD_TEXT);
    }
}

void CPlugin::NextPreset(float fBlendTime)  // if not retracing our former steps, it will choose a random one.
{
    LoadRandomPreset(fBlendTime);
}

void CPlugin::LoadRandomPreset(float fBlendTime)
{
	// make sure file list is ok
	if (m_nPresets - m_nDirs == 0)
	{
			// note: this error message is repeated in milkdropfs.cpp in DrawText()
			wchar_t buf[1024];
            swprintf(buf, LoadResString(IDS_ERROR_NO_PRESET_FILE_FOUND_IN_X_MILK), m_szPresetDir);
            AddError(buf, 6.0f, ERR_MISC, true);

            // also bring up the dir. navigation menu...
			if (m_UI_mode == UI_REGULAR || m_UI_mode == UI_MENU)
		    {
			    m_UI_mode = UI_LOAD;
			    m_bUserPagedUp = false;
			    m_bUserPagedDown = false;
            }
			return;
		}

    bool bHistoryEmpty = (m_presetHistoryFwdFence==m_presetHistoryBackFence);

    // if we have history to march back forward through, do that first
    if (!m_bSequentialPresetOrder)
    {
        int next = (m_presetHistoryPos+1) % PRESET_HIST_LEN;
        if (next != m_presetHistoryFwdFence && !bHistoryEmpty)
        {
            m_presetHistoryPos = next;
            LoadPreset( m_presetHistory[m_presetHistoryPos].c_str(), fBlendTime); 
            return;
        }
    }

	// --TEMPORARY--
	// this comes in handy if you want to mass-modify a batch of presets;
	// just automatically tweak values in Import, then they immediately get exported to a .MILK in a new dir.
	// --[END]TEMPORARY--

	if (!m_playlist.IsAll())
	{
		// SURI: pick only from the session playlist
		const std::vector<int>& allowed = PlaylistIndices();
		if (allowed.empty())
			return;
		if (m_bSequentialPresetOrder)
			m_nCurrentPreset = PickSequential(allowed, m_nCurrentPreset, +1);
		else
		{
			std::vector<float> ratingThis;
			EnterCriticalSection(&g_cs);
			ratingThis.resize(m_presets.size(), 1.0f);
			if (m_bEnableRating)
				for (size_t i = 0; i < m_presets.size(); i++)
					ratingThis[i] = m_presets[i].fRatingThis;
			LeaveCriticalSection(&g_cs);
			m_nCurrentPreset = PickWeighted(allowed, ratingThis, (rand() % 14345) / 14345.0f);
		}
	}
	else if (m_bSequentialPresetOrder)
	{
		m_nCurrentPreset++;
		if (m_nCurrentPreset < m_nDirs || m_nCurrentPreset >= m_nPresets)
			m_nCurrentPreset = m_nDirs;
	}
	else
	{
		// pick a random file
		if (!m_bEnableRating || (m_presets[m_nPresets - 1].fRatingCum < 0.1f))// || (m_nRatingReadProgress < m_nPresets))
		{
			m_nCurrentPreset = m_nDirs + (rand() % (m_nPresets - m_nDirs));
		}
		else
		{
			float cdf_pos = (rand() % 14345)/14345.0f*m_presets[m_nPresets - 1].fRatingCum;


			if (cdf_pos < m_presets[m_nDirs].fRatingCum)
			{
				m_nCurrentPreset = m_nDirs;
			}
			else
			{
				int lo = m_nDirs;
				int hi = m_nPresets;
				while (lo + 1 < hi)
				{
					int mid = (lo+hi)/2;
					if (m_presets[mid].fRatingCum > cdf_pos)
						hi = mid;
					else
						lo = mid;
				}
				m_nCurrentPreset = hi;
			}
		}
	}

	// m_pPresetAddr[m_nCurrentPreset] points to the preset file to load (w/o the path);
	// first prepend the path, then load section [preset00] within that file
	wchar_t szFile[MAX_PATH] = {0};
	lstrcpyW(szFile, m_szPresetDir);	// note: m_szPresetDir always ends with '\'
	lstrcatW(szFile, m_presets[m_nCurrentPreset].szFilename.c_str());

    if (!bHistoryEmpty)
        m_presetHistoryPos = (m_presetHistoryPos+1) % PRESET_HIST_LEN;

	LoadPreset(szFile, fBlendTime);
}

void CPlugin::RandomizeBlendPattern()
{
    if (!m_vertinfo)
        return;

    // note: we now avoid constant uniform blend b/c it's half-speed for shader blending.
    //       (both old & new shaders would have to run on every pixel...)
    int mixtype = 1 + (rand()%3);//rand()%4;

    if (mixtype==0)
    {
        // constant, uniform blend
        int nVert = 0;
	    for (int y=0; y<=m_nGridY; y++)
	    {
		    for (int x=0; x<=m_nGridX; x++)
		    {
                m_vertinfo[nVert].a = 1;
                m_vertinfo[nVert].c = 0;
			    nVert++;
            }
        }
    }
    else if (mixtype==1)
    {
        // directional wipe
        float ang = FRAND*6.28f;
        float vx = cosf(ang);
        float vy = sinf(ang);
        float band = 0.1f + 0.2f*FRAND; // 0.2 is good
        float inv_band = 1.0f/band;

        int nVert = 0;
	    for (int y=0; y<=m_nGridY; y++)
	    {
            float fy = (y/(float)m_nGridY)*m_fAspectY;
		    for (int x=0; x<=m_nGridX; x++)
		    {
                float fx = (x/(float)m_nGridX)*m_fAspectX;

                // at t==0, mix rangse from -10..0
                // at t==1, mix ranges from   1..11

                float t = (fx-0.5f)*vx + (fy-0.5f)*vy + 0.5f;
                t = (t-0.5f)/sqrtf(2.0f) + 0.5f;

                m_vertinfo[nVert].a = inv_band * (1 + band);
                m_vertinfo[nVert].c = -inv_band + inv_band*t;//(x/(float)m_nGridX - 0.5f)/band;
			    nVert++;
		    }
	    }
    }
    else if (mixtype==2)
    {
        // plasma transition
        float band = 0.12f + 0.13f*FRAND;//0.02f + 0.18f*FRAND;
        float inv_band = 1.0f/band;

        // first generate plasma array of height values
        m_vertinfo[                               0].c = FRAND;
        m_vertinfo[                        m_nGridX].c = FRAND;
        m_vertinfo[m_nGridY*(m_nGridX+1)           ].c = FRAND;
        m_vertinfo[m_nGridY*(m_nGridX+1) + m_nGridX].c = FRAND;
        GenPlasma(0, m_nGridX, 0, m_nGridY, 0.25f);

        // then find min,max so we can normalize to [0..1] range and then to the proper 'constant offset' range.
        float minc = m_vertinfo[0].c;
        float maxc = m_vertinfo[0].c;
        int x,y,nVert;

        nVert = 0;
	    for (y=0; y<=m_nGridY; y++)
	    {
		    for (x=0; x<=m_nGridX; x++)
            {
                if (minc > m_vertinfo[nVert].c)
                    minc = m_vertinfo[nVert].c;
                if (maxc < m_vertinfo[nVert].c)
                    maxc = m_vertinfo[nVert].c;
			    nVert++;
		    }
	    }

        float mult = 1.0f/(maxc-minc);
        nVert = 0;
	    for (y=0; y<=m_nGridY; y++)
	    {
		    for (x=0; x<=m_nGridX; x++)
            {
                float t = (m_vertinfo[nVert].c - minc)*mult;
                m_vertinfo[nVert].a = inv_band * (1 + band);
                m_vertinfo[nVert].c = -inv_band + inv_band*t;
                nVert++;
            }
        }
    }
    else if (mixtype==3)
    {
        // radial blend
        float band = 0.02f + 0.14f*FRAND + 0.34f*FRAND;
        float inv_band = 1.0f/band;
        float dir = (float)((rand()%2)*2 - 1);      // 1=outside-in, -1=inside-out

        int nVert = 0;
	    for (int y=0; y<=m_nGridY; y++)
	    {
            float dy = (y/(float)m_nGridY - 0.5f)*m_fAspectY;
		    for (int x=0; x<=m_nGridX; x++)
		    {
                float dx = (x/(float)m_nGridX - 0.5f)*m_fAspectX;
                float t = sqrtf(dx*dx + dy*dy)*1.41421f;
                if (dir==-1)
                    t = 1-t;

                m_vertinfo[nVert].a = inv_band * (1 + band);
                m_vertinfo[nVert].c = -inv_band + inv_band*t;
			    nVert++;
            }
        }
    }
}

void CPlugin::GenPlasma(int x0, int x1, int y0, int y1, float dt)
{
    int midx = (x0+x1)/2;
    int midy = (y0+y1)/2;
    float t00 = m_vertinfo[y0*(m_nGridX+1) + x0].c;
    float t01 = m_vertinfo[y0*(m_nGridX+1) + x1].c;
    float t10 = m_vertinfo[y1*(m_nGridX+1) + x0].c;
    float t11 = m_vertinfo[y1*(m_nGridX+1) + x1].c;

    if (y1-y0 >= 2)
    {
        if (x0==0)
            m_vertinfo[midy*(m_nGridX+1) + x0].c = 0.5f*(t00 + t10) + (FRAND*2-1)*dt*m_fAspectY;
        m_vertinfo[midy*(m_nGridX+1) + x1].c = 0.5f*(t01 + t11) + (FRAND*2-1)*dt*m_fAspectY;
    }
    if (x1-x0 >= 2)
    {
        if (y0==0)
            m_vertinfo[y0*(m_nGridX+1) + midx].c = 0.5f*(t00 + t01) + (FRAND*2-1)*dt*m_fAspectX;
        m_vertinfo[y1*(m_nGridX+1) + midx].c = 0.5f*(t10 + t11) + (FRAND*2-1)*dt*m_fAspectX;
    }

    if (y1-y0 >= 2 && x1-x0 >= 2)
    {
        // do midpoint & recurse:
        t00 = m_vertinfo[midy*(m_nGridX+1) + x0].c;
        t01 = m_vertinfo[midy*(m_nGridX+1) + x1].c;
        t10 = m_vertinfo[y0*(m_nGridX+1) + midx].c;
        t11 = m_vertinfo[y1*(m_nGridX+1) + midx].c;
        m_vertinfo[midy*(m_nGridX+1) + midx].c = 0.25f*(t10 + t11 + t00 + t01) + (FRAND*2-1)*dt;

        GenPlasma(x0, midx, y0, midy, dt*0.5f);
        GenPlasma(midx, x1, y0, midy, dt*0.5f);
        GenPlasma(x0, midx, midy, y1, dt*0.5f);
        GenPlasma(midx, x1, midy, y1, dt*0.5f);
    }
}

void CPlugin::LoadPreset(const wchar_t *szPresetFilename, float fBlendTime)
{
    // clear old error msg...
    if (m_nFramesSinceResize > 4)
    	ClearErrors(ERR_PRESET);

    // make sure preset still exists.  (might not if they are using the "back"/fwd buttons
    //  in RANDOM preset order and a file was renamed or deleted!)
    if (GetFileAttributesW(szPresetFilename) == 0xFFFFFFFF)
    {
        const wchar_t *p = wcsrchr(szPresetFilename, L'\\');
        p = (p) ? p+1 : szPresetFilename;
        wchar_t buf[1024];
		swprintf(buf, LoadResString(IDS_ERROR_PRESET_NOT_FOUND_X), p);
        AddError(buf, 6.0f, ERR_PRESET, true);
        return;
    }

    if ( !m_bSequentialPresetOrder )
    {
        // save preset in the history.  keep in mind - maybe we are searching back through it already!
        if ( m_presetHistoryFwdFence == m_presetHistoryPos )
        {
            // we're at the forward frontier; add to history
            m_presetHistory[m_presetHistoryPos] = szPresetFilename;
            m_presetHistoryFwdFence = (m_presetHistoryFwdFence+1) % PRESET_HIST_LEN;

            // don't let the two fences touch
            if (m_presetHistoryBackFence == m_presetHistoryFwdFence)
                m_presetHistoryBackFence = (m_presetHistoryBackFence+1) % PRESET_HIST_LEN;
        }
        else
        {
            // we're retracing our steps, either forward or backward...

        }
    }

    // if no preset was valid before, make sure there is no blend, because there is nothing valid to blend from.
    if (!wcscmp(m_pState->m_szDesc, INVALID_PRESET_DESC))
        fBlendTime = 0;

    if (fBlendTime == 0)
    {
        // do it all NOW!
	    if (szPresetFilename != m_szCurrentPresetFile) //[sic]
		    lstrcpyW(m_szCurrentPresetFile, szPresetFilename);

	    CState *temp = m_pState;
	    m_pState = m_pOldState;
	    m_pOldState = temp;

        DWORD ApplyFlags = STATE_ALL;
        ApplyFlags ^= (m_bWarpShaderLock ? STATE_WARP : 0);
        ApplyFlags ^= (m_bCompShaderLock ? STATE_COMP : 0);

        m_pState->Import(m_szCurrentPresetFile, GetTime(), m_pOldState, ApplyFlags);
        ApplyFxToState(m_pState);  // SURI
        LaunchPresetSprites(m_pState);  // SURI


	    m_fPresetStartTime = GetTime();
	    m_fNextPresetTime = -1.0f;		// flags UpdateTime() to recompute this

        // release stuff from m_OldShaders, then move m_shaders to m_OldShaders, then load the new shaders.
        SafeRelease( m_OldShaders.comp.ptr );
        SafeRelease( m_OldShaders.warp.ptr );
        SafeRelease( m_OldShaders.comp.CT );
        SafeRelease( m_OldShaders.warp.CT );
        m_OldShaders = m_shaders;
        ZeroMemory(&m_shaders, sizeof(PShaderSet));

        LoadShaders(&m_shaders, m_pState, false);

        OnFinishedLoadingPreset();
    }
    else
    {
        // set ourselves up to load the preset (and esp. compile shaders) a little bit at a time
        SafeRelease( m_NewShaders.comp.ptr );
        SafeRelease( m_NewShaders.warp.ptr );
        ZeroMemory(&m_NewShaders, sizeof(PShaderSet));

        DWORD ApplyFlags = STATE_ALL;
        ApplyFlags ^= (m_bWarpShaderLock ? STATE_WARP : 0);
        ApplyFlags ^= (m_bCompShaderLock ? STATE_COMP : 0);

        m_pNewState->Import(szPresetFilename, GetTime(), m_pOldState, ApplyFlags);
        ApplyFxToState(m_pNewState);  // SURI
        LaunchPresetSprites(m_pNewState);  // SURI

        m_nLoadingPreset = 1;   // this will cause LoadPresetTick() to get called over the next few frames...

        m_fLoadingPresetBlendTime = fBlendTime;
        lstrcpyW(m_szLoadingPreset, szPresetFilename);
    }
}

void CPlugin::OnFinishedLoadingPreset()
{
    // note: only used this if you loaded the preset *intact* (or mostly intact)

    SetMenusForPresetVersion( m_pState->m_nWarpPSVersion, m_pState->m_nCompPSVersion );
    m_nPresetsLoadedTotal++; //only increment this on COMPLETION of the load.

    for (int mash=0; mash<MASH_SLOTS; mash++)
        m_nMashPreset[mash] = m_nCurrentPreset;
}

void CPlugin::LoadPresetTick()
{
    if (m_nLoadingPreset == 2 || m_nLoadingPreset == 5)
    {
        // just loads one shader (warp or comp) then returns.
        LoadShaders(&m_NewShaders, m_pNewState, true);
    }
    else if (m_nLoadingPreset == 8)
    {
        // finished loading the shaders - apply the preset!
        lstrcpyW(m_szCurrentPresetFile, m_szLoadingPreset);
        m_szLoadingPreset[0] = 0;

	    CState *temp = m_pState;
	    m_pState = m_pOldState;
	    m_pOldState = temp;

	    temp = m_pState;
	    m_pState = m_pNewState;
	    m_pNewState = temp;

        RandomizeBlendPattern();

		    m_pState->StartBlendFrom(m_pOldState, GetTime(), m_fLoadingPresetBlendTime);

	    m_fPresetStartTime = GetTime();
	    m_fNextPresetTime = -1.0f;		// flags UpdateTime() to recompute this

        // release stuff from m_OldShaders, then move m_shaders to m_OldShaders, then load the new shaders.
        SafeRelease( m_OldShaders.comp.ptr );
        SafeRelease( m_OldShaders.warp.ptr );
        m_OldShaders = m_shaders;
        m_shaders = m_NewShaders;
        ZeroMemory(&m_NewShaders, sizeof(PShaderSet));

        // end slow-preset-load mode
        m_nLoadingPreset = 0;

        OnFinishedLoadingPreset();
    }

    if (m_nLoadingPreset > 0)
        m_nLoadingPreset++;
}

void CPlugin::SeekToPreset(wchar_t cStartChar)
{
	if (cStartChar >= L'a' && cStartChar <= L'z')
		cStartChar -= L'a' - L'A';

	for (int i = m_nDirs; i < m_nPresets; i++)
	{
		wchar_t ch = m_presets[i].szFilename.c_str()[0];
		if (ch >= L'a' && ch <= L'z')
			ch -= L'a' - L'A';
		if (ch == cStartChar)
		{
			m_nPresetListCurPos = i;
			return;
		}
	}
}

void CPlugin::FindValidPresetDir()
{
    swprintf(m_szPresetDir, L"%spresets\\", m_szMilkdrop2Path );
    if (GetFileAttributesW(m_szPresetDir) != -1)
        return;
    lstrcpyW(m_szPresetDir, m_szMilkdrop2Path);
    if (GetFileAttributesW(m_szPresetDir) != -1)
        return;
    lstrcpyW(m_szPresetDir, GetPluginsDirPath());
    if (GetFileAttributesW(m_szPresetDir) != -1)
        return;
    lstrcpyW(m_szPresetDir, L"c:\\program files\\");  //getting desperate here
    if (GetFileAttributesW(m_szPresetDir) != -1)
        return;
    lstrcpyW(m_szPresetDir, L"c:\\");
}

char* NextLine(char* p)
{
    // p points to the beginning of a line
    // we'll return a pointer to the first char of the next line
    // if we hit a NULL char before that, we'll return NULL.
    if (!p)
        return NULL;

    char* s = p;
    while (*s != '\r' && *s != '\n' && *s != 0)
        s++;

    while (*s == '\r' || *s == '\n')
        s++;

    if (*s==0)
        return NULL;

    return s;
}

static unsigned int WINAPI __UpdatePresetList(void* lpVoid)
{
    // NOTE - this is run in a separate thread!!!

    DWORD flags = (DWORD)lpVoid;
    bool bForce = (flags & 1) ? true : false;
    bool bTryReselectCurrentPreset = (flags & 2) ? true : false;

    WIN32_FIND_DATAW fd;
    ZeroMemory(&fd, sizeof(fd));
    HANDLE h = INVALID_HANDLE_VALUE;

    int nTry = 0;
    bool bRetrying = false;

    EnterCriticalSection(&g_cs);
retry:

	// make sure the path exists; if not, fall back to a folder that does
	if (GetFileAttributesW(g_plugin.m_szPresetDir) == -1)
    {
        //FIXME...
        g_plugin.FindValidPresetDir();
    }

    wchar_t szMask[MAX_PATH];
	swprintf(szMask, L"%s*.*", g_plugin.m_szPresetDir);  // cuz dirnames could have extensions, etc.
    if (bForce || !g_plugin.m_szUpdatePresetMask[0] || wcscmp(szMask, g_plugin.m_szUpdatePresetMask))
    {
        // if old dir was "" or the dir changed, reset our search
        if (h != INVALID_HANDLE_VALUE)
            FindClose(h);
        h = INVALID_HANDLE_VALUE;
        g_plugin.m_bPresetListReady = false;
        lstrcpyW(g_plugin.m_szUpdatePresetMask, szMask);
        ZeroMemory(&fd, sizeof(fd));

        g_plugin.m_nPresets = 0;
	    g_plugin.m_nDirs    = 0;
        g_plugin.m_presets.clear();

	    // find first .MILK file
	    //if( (hFile = _findfirst(szMask, &c_file )) != -1L )		// note: returns filename -without- path
	    if( (h = FindFirstFileW(g_plugin.m_szUpdatePresetMask, &fd )) == INVALID_HANDLE_VALUE )		// note: returns filename -without- path
        {
            // --> revert back to plugins dir
            wchar_t buf[1024];
		    swprintf(buf, LoadResString(IDS_ERROR_NO_PRESET_FILES_OR_DIRS_FOUND_IN_X), g_plugin.m_szPresetDir);
            g_plugin.AddError(buf, 4.0f, ERR_PRESET_DIR, true);   // SURI: own category, see below

            if (bRetrying)
            {
                LeaveCriticalSection(&g_cs);
                g_bThreadAlive = false;
                _endthreadex(0);
                return 0;
            }

            g_plugin.FindValidPresetDir();

            bRetrying = true;
            goto retry;
        }

        g_plugin.AddError(LoadResString(IDS_SCANNING_PRESETS), 8.0f, ERR_SCANNING_PRESETS, false);
    }

    if (g_plugin.m_bPresetListReady)
    {
        LeaveCriticalSection(&g_cs);
        g_bThreadAlive = false;
        _endthreadex(0);
        return 0;
    }

    int  nMaxPSVersion = g_plugin.m_nMaxPSVersion;
    wchar_t szPresetDir[MAX_PATH];
    lstrcpyW(szPresetDir, g_plugin.m_szPresetDir);

	LeaveCriticalSection(&g_cs);

    PresetList temp_presets;
    int temp_nDirs = 0;
    int temp_nPresets = 0;

    // scan for the desired # of presets, this call...
    while (!g_bThreadShouldQuit && h != INVALID_HANDLE_VALUE)
    {
		bool bSkip = false;
        bool bIsDir = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        float fRating = 0;

		wchar_t szFilename[512];
		lstrcpyW(szFilename, fd.cFileName);

		if (bIsDir)
		{
			// skip "." directory
			if (wcscmp(fd.cFileName, L".")==0)// || lstrlen(ffd.cFileName) < 1)
				bSkip = true;
			else
				swprintf(szFilename, L"*%s", fd.cFileName);
		}
		else
		{
			// skip normal files not ending in ".milk"
			int len = lstrlenW(fd.cFileName);
			if (len < 5 || wcsicmp(fd.cFileName + len - 5, L".milk") != 0)
				bSkip = true;

            // if it is .milk, make sure we know how to run its pixel shaders -
            // otherwise we don't want to show it in the preset list!
            if (!bSkip)
            {
                // If the first line of the file is not "MILKDROP_PRESET_VERSION XXX",
                //   then it's a MilkDrop 1 era preset, so it is definitely runnable. (no shaders)
                // Otherwise, check for the value "PSVERSION".  It will be 0, 2, or 3.
                //   If missing, assume it is 2.
                wchar_t szFullPath[MAX_PATH];
                swprintf(szFullPath, L"%s%s", szPresetDir, fd.cFileName);
                FILE* f = _wfopen(szFullPath, L"r");
                if (!f)
                    bSkip = true;
                else {
                    #define PRESET_HEADER_SCAN_BYTES 160
                    char szLine[PRESET_HEADER_SCAN_BYTES];
                    char *p = szLine;

                    int bytes_to_read = sizeof(szLine)-1;
                    int count = fread(szLine, bytes_to_read, 1, f);
                    if (count < 1) {
                        fseek(f, SEEK_SET, 0);
                        count = fread(szLine, 1, bytes_to_read, f);
                        szLine[ count ] = 0;
                    }
                    else
                        szLine[bytes_to_read-1] = 0;

                    bool bScanForPreset00AndRating = false;
                    bool bRatingKnown = false;

                    // try to read the PSVERSION and the fRating= value.
                    // most presets (unless hand-edited) will have these right at the top.
                    // if not, [at least for fRating] use GetPrivateProfileFloat to search whole file.
                    // read line 1
                    //p = NextLine(p);//fgets(p, sizeof(p)-1, f);
                    if (!strncmp(p, "MILKDROP_PRESET_VERSION", 23))
                    {
                        p = NextLine(p);//fgets(p, sizeof(p)-1, f);
                        int ps_version = 2;
                        if (p && !strncmp(p, "PSVERSION", 9))
                        {
                            sscanf(&p[10], "%d", &ps_version);
                            if (ps_version > nMaxPSVersion)
                                bSkip = true;
                            else
                            {
                                p = NextLine(p);//fgets(p, sizeof(p)-1, f);
                                bScanForPreset00AndRating = true;
                            }
                        }
                    }
                    else
                    {
                        // otherwise it's a MilkDrop 1 preset - we can run it.
                        bScanForPreset00AndRating = true;
                    }

                    // scan up to 10 more lines in the file, looking for [preset00] and fRating=...
                    // (this is WAY faster than GetPrivateProfileFloat, when it works!)
                    int reps = (bScanForPreset00AndRating) ? 10 : 0;
                    for (int z=0; z<reps; z++)
                    {
                        if (p && !strncmp(p, "[preset00]", 10))
                        {
                            p = NextLine(p);
                            if (p && !strncmp(p, "fRating=", 8))
                            {
                                _sscanf_l(&p[8], "%f", g_use_C_locale, &fRating);
                                bRatingKnown = true;
                                break;
                            }
                        }
                        p = NextLine(p);
                    }

                    fclose(f);

                    if (!bRatingKnown)
		                fRating = GetPrivateProfileFloatW(L"preset00", L"fRating", 3.0f, szFullPath);
                    fRating = max(0.0f, min(5.0f, fRating));
                }
            }
		}

		if (!bSkip)
		{
            float fPrevPresetRatingCum = 0;
            if (temp_nPresets > 0)
                fPrevPresetRatingCum += temp_presets[temp_nPresets-1].fRatingCum;

            PresetInfo x;
            x.szFilename  = szFilename;
            x.fRatingThis = fRating;
            x.fRatingCum  = fPrevPresetRatingCum + fRating;
            temp_presets.push_back(x);

			temp_nPresets++;
			if (bIsDir)
				temp_nDirs++;
        }

    	if (!FindNextFileW(h, &fd))
        {
        	FindClose(h);
            h = INVALID_HANDLE_VALUE;

            break;
        }

        // every so often, add some presets...
        #define PRESET_UPDATE_INTERVAL 64
        if (temp_nPresets == 30 || ((temp_nPresets % PRESET_UPDATE_INTERVAL)==0))
        {
	        EnterCriticalSection(&g_cs);

            for (int i=g_plugin.m_nPresets; i<temp_nPresets; i++)
                g_plugin.m_presets.push_back(temp_presets[i]);
            g_plugin.m_nPresets = temp_nPresets;
            g_plugin.m_nDirs    = temp_nDirs;

            LeaveCriticalSection(&g_cs);
        }
    }

    if (g_bThreadShouldQuit)
    {
        // just abort... we are exiting the program or restarting the scan.
        g_bThreadAlive = false;
        _endthreadex(0);
        return 0;
    }

	EnterCriticalSection(&g_cs);

    for (int i=g_plugin.m_nPresets; i<temp_nPresets; i++)
        g_plugin.m_presets.push_back(temp_presets[i]);
    g_plugin.m_nPresets = temp_nPresets;
    g_plugin.m_nDirs    = temp_nDirs;
    g_plugin.m_bPresetListReady = true;
    g_plugin.m_playlistIdxPresets = -1;   // SURI: re-resolve the playlist (names may have changed, e.g. a rename)

    if (g_plugin.m_bPresetListReady && g_plugin.m_nPresets == 0)
    {
        // no presets OR directories found - weird - but it happens.
        // --> revert back to plugins dir
        wchar_t buf[1024];
		swprintf(buf, LoadResString(IDS_ERROR_NO_PRESET_FILES_OR_DIRS_FOUND_IN_X), g_plugin.m_szPresetDir);
        g_plugin.AddError(buf, 4.0f, ERR_PRESET_DIR, true);   // SURI: own category, see below

        if (bRetrying)
        {
        	LeaveCriticalSection(&g_cs);
            g_bThreadAlive = false;
            _endthreadex(0);
            return 0;
        }

        g_plugin.FindValidPresetDir();

        bRetrying = true;
        goto retry;
    }

    if (g_plugin.m_bPresetListReady)
    {
    	g_plugin.MergeSortPresets(0, g_plugin.m_nPresets-1);

        // update cumulative ratings, since order changed...
        g_plugin.m_presets[0].fRatingCum = g_plugin.m_presets[0].fRatingThis;
        for (int i=0; i<g_plugin.m_nPresets; i++)
            g_plugin.m_presets[i].fRatingCum = i == 0 ? 0 : g_plugin.m_presets[i-1].fRatingCum + g_plugin.m_presets[i].fRatingThis;

        // clear the "scanning presets" msg
        g_plugin.ClearErrors(ERR_SCANNING_PRESETS);
        // SURI: presets were found (maybe after FindValidPresetDir), so an earlier
        // "no preset files or directories found" no longer applies
        g_plugin.ClearErrors(ERR_PRESET_DIR);

	    // finally, try to re-select the most recently-used preset in the list
	    g_plugin.m_nPresetListCurPos = 0;
        if (bTryReselectCurrentPreset)
        {
	        if (g_plugin.m_szCurrentPresetFile[0])
	        {
		        // try to automatically seek to the last preset loaded
                wchar_t *p = wcsrchr(g_plugin.m_szCurrentPresetFile, L'\\');
                p = (p) ? (p+1) : g_plugin.m_szCurrentPresetFile;
		        for (int i=g_plugin.m_nDirs; i<g_plugin.m_nPresets; i++)
				{
                    if (wcscmp(p, g_plugin.m_presets[i].szFilename.c_str())==0) {
				        g_plugin.m_nPresetListCurPos = i;
                        break;
                    }
				}
	        }
        }
    }

    LeaveCriticalSection(&g_cs);

    g_bThreadAlive = false;
    _endthreadex(0);
    return 0;
}

void CPlugin::UpdatePresetList(bool bBackground, bool bForce, bool bTryReselectCurrentPreset)
{
    // note: if dir changed, make sure bForce is true!

    if (bForce)
    {
        if (g_bThreadAlive)
            CancelThread(3000);  // flags it to exit; the param is the # of ms to wait before forcefully killing it
    }
    else
    {
        if (bBackground && (g_bThreadAlive || m_bPresetListReady))
            return;
        if (!bBackground && m_bPresetListReady)
            return;
    }

    assert(!g_bThreadAlive);

    // spawn new thread:
    DWORD flags = (bForce ? 1 : 0) | (bTryReselectCurrentPreset ? 2 : 0);
    g_bThreadShouldQuit = false;
    g_bThreadAlive = true;
    g_hThread = (HANDLE)_beginthreadex(NULL,0,__UpdatePresetList,(void*)flags,0,0);

    if (!bBackground)
    {
        // crank up priority, wait for it to finish, and then return
	    SetThreadPriority(g_hThread,THREAD_PRIORITY_HIGHEST); //THREAD_PRIORITY_IDLE,    THREAD_PRIORITY_LOWEST,    THREAD_PRIORITY_NORMAL,    THREAD_PRIORITY_HIGHEST,

        // wait for it to finish
        while (g_bThreadAlive)
            Sleep(30);

        assert(g_hThread != INVALID_HANDLE_VALUE);
        CloseHandle(g_hThread);
        g_hThread = INVALID_HANDLE_VALUE;
    }
    else
    {
        // it will just run in the background til it finishes.
        // however, we want to wait until at least ~32 presets are found (or failure) before returning,
        // so we know we have *something* in the preset list to start with.

	    SetThreadPriority(g_hThread,THREAD_PRIORITY_HIGHEST); //THREAD_PRIORITY_IDLE,    THREAD_PRIORITY_LOWEST,    THREAD_PRIORITY_NORMAL,    THREAD_PRIORITY_HIGHEST,

        // wait until either the thread exits, or # of presets is >32, before returning.
        // also make sure you enter the CS whenever you check on it!
        // (thread will update preset list every so often, with the newest presets scanned in...)
        while (g_bThreadAlive)
        {
            Sleep(30);

	        EnterCriticalSection(&g_cs);
            int nPresets = g_plugin.m_nPresets;
	        LeaveCriticalSection(&g_cs);

            if (nPresets >= 30)
                break;
        }

        if (g_bThreadAlive)
        {
            // the load still takes a while even at THREAD_PRIORITY_ABOVE_NORMAL,
            // because it is waiting on the HDD so much...
            // but the OS is smart, and the CPU stays nice and zippy in other threads =)
        	SetThreadPriority(g_hThread,THREAD_PRIORITY_ABOVE_NORMAL); //THREAD_PRIORITY_IDLE,    THREAD_PRIORITY_LOWEST,    THREAD_PRIORITY_NORMAL,    THREAD_PRIORITY_HIGHEST,
        }
    }

    return;
}

void CPlugin::MergeSortPresets(int left, int right)
{
	// note: left..right range is inclusive
	int nItems = right-left+1;

	if (nItems > 2)
	{
		// recurse to sort 2 halves (but don't actually recurse on a half if it only has 1 element)
		int mid = (left+right)/2;
		/*if (mid   != left) */ MergeSortPresets(left, mid);
		/*if (mid+1 != right)*/ MergeSortPresets(mid+1, right);

		// then merge results
		int a = left;
		int b = mid + 1;
		while (a <= mid && b <= right)
		{
			bool bSwap;

			// merge the sorted arrays; give preference to strings that start with a '*' character
			int nSpecial = 0;
			if (m_presets[a].szFilename.c_str()[0] == '*') nSpecial++;
			if (m_presets[b].szFilename.c_str()[0] == '*') nSpecial++;

			if (nSpecial == 1)
			{
				bSwap = (m_presets[b].szFilename.c_str()[0] == '*');
			}
			else
			{
				bSwap = (mystrcmpiW(m_presets[a].szFilename.c_str(), m_presets[b].szFilename.c_str()) > 0);
			}

			if (bSwap)
			{
				PresetInfo temp = m_presets[b];
				for (int k=b; k>a; k--)
					m_presets[k] = m_presets[k-1];
				m_presets[a] = temp;
				mid++;
				b++;
			}
			a++;
		}
	}
	else if (nItems == 2)
	{
		// sort 2 items; give preference to 'special' strings that start with a '*' character
		int nSpecial = 0;
		if (m_presets[left].szFilename.c_str()[0] == '*') nSpecial++;
		if (m_presets[right].szFilename.c_str()[0] == '*') nSpecial++;

		if (nSpecial == 1)
		{
			if (m_presets[right].szFilename.c_str()[0] == '*')
			{
                PresetInfo temp = m_presets[left];
				m_presets[left] = m_presets[right];
				m_presets[right] = temp;
			}
		}
		else if (mystrcmpiW(m_presets[left].szFilename.c_str(), m_presets[right].szFilename.c_str()) > 0)
		{
            PresetInfo temp = m_presets[left];
			m_presets[left] = m_presets[right];
			m_presets[right] = temp;
		}
	}
}

void CPlugin::WaitString_NukeSelection()
{
	if (m_waitstring.bActive &&
		m_waitstring.nSelAnchorPos != -1)
	{
		// nuke selection.  note: start & end are INCLUSIVE.
		int start = (m_waitstring.nCursorPos < m_waitstring.nSelAnchorPos) ? m_waitstring.nCursorPos : m_waitstring.nSelAnchorPos;
		int end   = (m_waitstring.nCursorPos > m_waitstring.nSelAnchorPos) ? m_waitstring.nCursorPos - 1 : m_waitstring.nSelAnchorPos - 1;
		int len   = (m_waitstring.bDisplayAsCode ? lstrlenA((char*)m_waitstring.szText) : lstrlenW(m_waitstring.szText));
		int how_far_to_shift   = end - start + 1;
		int num_chars_to_shift = len - end;		// includes NULL char

		if (m_waitstring.bDisplayAsCode)
		{
			char* ptr = (char*)m_waitstring.szText;
			for (int i=0; i<num_chars_to_shift; i++)
				*(ptr + start + i) = *(ptr + start + i + how_far_to_shift);
		}
		else
		{
			for (int i=0; i<num_chars_to_shift; i++)
				m_waitstring.szText[start + i] = m_waitstring.szText[start + i + how_far_to_shift];
		}

		// clear selection
		m_waitstring.nCursorPos = start;
		m_waitstring.nSelAnchorPos = -1;
	}
}

void CPlugin::WaitString_Cut()
{
	if (m_waitstring.bActive &&
		m_waitstring.nSelAnchorPos != -1)
	{
		WaitString_Copy();
		WaitString_NukeSelection();
	}
}

void CPlugin::WaitString_Copy()
{
	if (m_waitstring.bActive &&
		m_waitstring.nSelAnchorPos != -1)
	{
		// note: start & end are INCLUSIVE.
		int start = (m_waitstring.nCursorPos < m_waitstring.nSelAnchorPos) ? m_waitstring.nCursorPos : m_waitstring.nSelAnchorPos;
		int end   = (m_waitstring.nCursorPos > m_waitstring.nSelAnchorPos) ? m_waitstring.nCursorPos - 1 : m_waitstring.nSelAnchorPos - 1;
		int chars_to_copy = end - start + 1;

		if (m_waitstring.bDisplayAsCode)
		{
			char* ptr = (char*)m_waitstring.szText;
			for (int i=0; i<chars_to_copy; i++)
				m_waitstring.szClipboard[i] = *(ptr + start + i);
			m_waitstring.szClipboard[chars_to_copy] = 0;

			char tmp[64000];
			ConvertLFCToCRsA(m_waitstring.szClipboard, tmp);
			copyStringToClipboardA(tmp);
		}
		else
		{
			for (int i=0; i<chars_to_copy; i++)
				m_waitstring.szClipboardW[i] = m_waitstring.szText[start + i];
			m_waitstring.szClipboardW[chars_to_copy] = 0;

			wchar_t tmp[64000];
			ConvertLFCToCRsW(m_waitstring.szClipboardW, tmp);
			copyStringToClipboardW(tmp);
		}
	}
}

void CPlugin::WaitString_Paste()
{
	// NOTE: if there is a selection, it is wiped out, and replaced with the clipboard contents.

	if (m_waitstring.bActive)
	{
		WaitString_NukeSelection();

		if (m_waitstring.bDisplayAsCode)
		{
	        char tmp[64000];
		    lstrcpyA(tmp, getStringFromClipboardA());
			ConvertCRsToLFCA(tmp, m_waitstring.szClipboard);
		}
		else
		{
	        wchar_t tmp[64000];
		    lstrcpyW(tmp, getStringFromClipboardW());
			ConvertCRsToLFCW(tmp, m_waitstring.szClipboardW);
		}

		int len;
		int chars_to_insert;

		if (m_waitstring.bDisplayAsCode)
		{
			len = lstrlenA((char*)m_waitstring.szText);
			chars_to_insert = lstrlenA(m_waitstring.szClipboard);
		}
		else
		{
			len = lstrlenW(m_waitstring.szText);
			chars_to_insert = lstrlenW(m_waitstring.szClipboardW);
		}

		if (len + chars_to_insert + 1 >= m_waitstring.nMaxLen)
		{
			chars_to_insert = m_waitstring.nMaxLen - len - 1;

			// inform user
            AddError(LoadResString(IDS_STRING_TOO_LONG), 2.5f, ERR_MISC, true);
		}
		else
		{
		}

		int i;
		if (m_waitstring.bDisplayAsCode)
		{
			char* ptr = (char*)m_waitstring.szText;
			for (i=len; i >= m_waitstring.nCursorPos; i--)
				*(ptr + i + chars_to_insert) = *(ptr + i);
			for (i=0; i < chars_to_insert; i++)
				*(ptr + i + m_waitstring.nCursorPos) = m_waitstring.szClipboard[i];
		}
		else
		{
			for (i=len; i >= m_waitstring.nCursorPos; i--)
				m_waitstring.szText[i + chars_to_insert] = m_waitstring.szText[i];
			for (i=0; i < chars_to_insert; i++)
				m_waitstring.szText[i + m_waitstring.nCursorPos] = m_waitstring.szClipboardW[i];
		}
		m_waitstring.nCursorPos += chars_to_insert;
	}
}

void CPlugin::WaitString_SeekLeftWord()
{
	// move to beginning of prior word
	if (m_waitstring.bDisplayAsCode)
	{
		char* ptr = (char*)m_waitstring.szText;
		while (m_waitstring.nCursorPos > 0 &&
			   !IsAlphanumericChar(*(ptr + m_waitstring.nCursorPos-1)))
			m_waitstring.nCursorPos--;

		while (m_waitstring.nCursorPos > 0 &&
			   IsAlphanumericChar(*(ptr + m_waitstring.nCursorPos-1)))
			m_waitstring.nCursorPos--;
	}
	else
	{
		while (m_waitstring.nCursorPos > 0 &&
			   !IsAlphanumericChar(m_waitstring.szText[m_waitstring.nCursorPos-1]))
			m_waitstring.nCursorPos--;

		while (m_waitstring.nCursorPos > 0 &&
			   IsAlphanumericChar(m_waitstring.szText[m_waitstring.nCursorPos-1]))
			m_waitstring.nCursorPos--;
	}
}

void CPlugin::WaitString_SeekRightWord()
{
	// move to beginning of next word

	//testing  lotsa   stuff

	if (m_waitstring.bDisplayAsCode)
	{
		int len = lstrlenA((char*)m_waitstring.szText);

		char* ptr = (char*)m_waitstring.szText;
		while (m_waitstring.nCursorPos < len &&
			   IsAlphanumericChar(*(ptr + m_waitstring.nCursorPos)))
			m_waitstring.nCursorPos++;

		while (m_waitstring.nCursorPos < len &&
			   !IsAlphanumericChar(*(ptr + m_waitstring.nCursorPos)))
			m_waitstring.nCursorPos++;
	}
	else
	{
		int len = lstrlenW(m_waitstring.szText);

		while (m_waitstring.nCursorPos < len &&
			   IsAlphanumericChar(m_waitstring.szText[m_waitstring.nCursorPos]))
			m_waitstring.nCursorPos++;

		while (m_waitstring.nCursorPos < len &&
			   !IsAlphanumericChar(m_waitstring.szText[m_waitstring.nCursorPos]))
			m_waitstring.nCursorPos++;
	}
}

int CPlugin::WaitString_GetCursorColumn()
{
	if (m_waitstring.bDisplayAsCode)
	{
		int column = 0;
		char* ptr = (char*)m_waitstring.szText;
		while (m_waitstring.nCursorPos - column - 1 >= 0 &&
			   *(ptr + m_waitstring.nCursorPos - column - 1) != LINEFEED_CONTROL_CHAR)
			column++;

		return column;
	}
	else
	{
		return m_waitstring.nCursorPos;
	}
}

int	CPlugin::WaitString_GetLineLength()
{
	int line_start = m_waitstring.nCursorPos - WaitString_GetCursorColumn();
	int line_length = 0;

	if (m_waitstring.bDisplayAsCode)
	{
		char* ptr = (char*)m_waitstring.szText;
		while (*(ptr + line_start + line_length) != 0 &&
			   *(ptr + line_start + line_length) != LINEFEED_CONTROL_CHAR)
			line_length++;
	}
	else
	{
		while (m_waitstring.szText[line_start + line_length] != 0 &&
			   m_waitstring.szText[line_start + line_length] != LINEFEED_CONTROL_CHAR)
			line_length++;
	}

	return line_length;
}

void CPlugin::WaitString_SeekUpOneLine()
{
	int column = g_plugin.WaitString_GetCursorColumn();

	if (column != m_waitstring.nCursorPos)
	{
		// seek to very end of previous line (cursor will be at the semicolon)
		m_waitstring.nCursorPos -= column + 1;

		int new_column = g_plugin.WaitString_GetCursorColumn();

		if (new_column > column)
			m_waitstring.nCursorPos -= (new_column - column);
	}
}

void CPlugin::WaitString_SeekDownOneLine()
{
	int column = g_plugin.WaitString_GetCursorColumn();
	int newpos = m_waitstring.nCursorPos;

	char* ptr = (char*)m_waitstring.szText;
	while (*(ptr + newpos) != 0 && *(ptr + newpos) != LINEFEED_CONTROL_CHAR)
		newpos++;

	if (*(ptr + newpos) != 0)
	{
		m_waitstring.nCursorPos = newpos + 1;

		while (	column > 0 &&
				*(ptr + m_waitstring.nCursorPos) != LINEFEED_CONTROL_CHAR &&
				*(ptr + m_waitstring.nCursorPos) != 0)
		{
			m_waitstring.nCursorPos++;
			column--;
		}
	}
}

// SURI: injects the live effects (m_fx) into pState's comp shader text.
void CPlugin::ApplyFxToState(CState* pState)
{
	if (m_nMaxPSVersion == 0)
		return;	// no pixel shader support: effects cannot run

	if (m_fx.Any() && pState->m_nCompPSVersion < MD2_PS_2_0)
	{
		if (pState->m_nFxOrigCompPSVersion == -1)
			pState->m_nFxOrigCompPSVersion = pState->m_nCompPSVersion;
		pState->m_nCompPSVersion = MD2_PS_2_0;
		pState->GenDefaultCompShader();
	}

	// ApplyFx strips old blocks first, so with no effects on it only strips
	std::string text(pState->m_szCompShadersText);
	bool bApplied = ApplyFx(text, m_fx);
	if (bApplied && text.size() > MAX_BIGSTRING_LEN - 1)
	{
		ERR(L"fx: result too long (%d), effects skipped", (int)text.size());
		bApplied = false;
	}
	if (!bApplied)
	{
		// SURI: no room (or no shader_body): still remove the old blocks, never keep stale ones
		text = pState->m_szCompShadersText;
		StripAllFx(text);
	}
	strcpy(pState->m_szCompShadersText, text.c_str());   // never longer than before when stripped
	if (bApplied && m_fx.Any())
		LOG(L"fx: applied mask=0x%x len=%d", m_fx.mask, (int)text.size());

	if (!m_fx.Any() && pState->m_nFxOrigCompPSVersion != -1)
	{
		pState->m_nCompPSVersion = pState->m_nFxOrigCompPSVersion;
		pState->m_nFxOrigCompPSVersion = -1;
	}
	pState->m_nMaxPSVersion = max(pState->m_nWarpPSVersion, pState->m_nCompPSVersion);
	pState->m_nMinPSVersion = min(pState->m_nWarpPSVersion, pState->m_nCompPSVersion);
}

// SURI: the song-title fonts, from SONGTITLE_FONT's current (DPI-scaled) size. Shows a message
// box and returns false on failure (was inline in AllocateMyDX9Stuff).
bool CPlugin::CreateTitleFonts()
{
    wchar_t title[64];
    int songtitle_font_size = m_fontinfo[SONGTITLE_FONT].nSize * m_nTitleTexSizeX/256;
    if (songtitle_font_size<6) songtitle_font_size=6;
    m_nTitleFontPx = songtitle_font_size;
    if (!(m_gdi_title_font_doublesize = CreateFontW(songtitle_font_size, 0, 0, 0, m_fontinfo[SONGTITLE_FONT].bBold ? 900 : 400,
		    m_fontinfo[SONGTITLE_FONT].bItalic, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, m_fontinfo[SONGTITLE_FONT].bAntiAliased ? ANTIALIASED_QUALITY : DEFAULT_QUALITY, DEFAULT_PITCH, m_fontinfo[SONGTITLE_FONT].szFace)))
    {
        MessageBoxW(NULL, LoadResString(IDS_ERROR_CREATING_DOUBLE_SIZED_GDI_TITLE_FONT),
						 LoadResString(IDS_MILKDROP_ERROR,title,sizeof(title)),
						 MB_OK|MB_SETFOREGROUND|MB_TOPMOST);
        return false;
    }

	if (D3DXCreateFontW(	GetDevice(),
						songtitle_font_size,
						0,
						m_fontinfo[SONGTITLE_FONT].bBold ? 900 : 400,
						1,
						m_fontinfo[SONGTITLE_FONT].bItalic,
						DEFAULT_CHARSET,
						OUT_DEFAULT_PRECIS,
						ANTIALIASED_QUALITY,//DEFAULT_QUALITY,
						DEFAULT_PITCH,
						m_fontinfo[SONGTITLE_FONT].szFace,
						&m_d3dx_title_font_doublesize
					) != D3D_OK)
    {
        m_d3dx_title_font_doublesize = NULL;
        MessageBoxW(GetPluginWindow(), LoadResString(IDS_ERROR_CREATING_DOUBLE_SIZED_D3DX_TITLE_FONT),
				   LoadResString(IDS_MILKDROP_ERROR,title,sizeof(title)), MB_OK|MB_SETFOREGROUND|MB_TOPMOST);
        return false;
    }
    return true;
}

// SURI: WM_DPICHANGED rebuilt the shell fonts, so SONGTITLE_FONT has a new size: rebuild the
// song-title fonts from it, and resize a song title that is on screen now.
void CPlugin::OnFontsReallocated()
{
    if (!GetDevice())
        return;
    SafeRelease(m_d3dx_title_font_doublesize);
    if (m_gdi_title_font_doublesize)
    {
        DeleteObject(m_gdi_title_font_doublesize);
        m_gdi_title_font_doublesize = NULL;
    }
    if (!CreateTitleFonts())
        return;
    if (m_supertext.bIsSongTitle)
        m_supertext.fFontSize = (float)m_fontinfo[SONGTITLE_FONT].nSize;   // as LaunchSongTitleAnim sets it
    m_supertext.bRedrawSuperText = true;   // re-render the title texture with the new font
}

// SURI: re-injects the effects into the current preset and recompiles its comp shader; a
// preset that is still loading (blend pending) gets them too, so the change survives its swap-in.
// Not OnUserEditedCompShaders: that also clears preset errors and rescans the textures.
void CPlugin::RefreshFx()
{
	ApplyFxToState(m_pState);
	RecompileCompShader(m_pState, &m_shaders.comp);
	if (m_nLoadingPreset != 0)
	{
		ApplyFxToState(m_pNewState);
		if (m_pNewState->m_nCompPSVersion > 0)   // as LoadShaders, which skips comp below that
			RecompileCompShader(m_pNewState, &m_NewShaders.comp);
	}
}

// SURI: compiles pState's comp shader text into si (whatever si held is released first);
// the fallback comp shader on failure, as OnUserEditedCompShaders / LoadShaders do.
void CPlugin::RecompileCompShader(CState* pState, PShaderInfo* si)
{
	if (m_nMaxPSVersion <= 0)
		return;
	SafeRelease(si->CT);   // RecompilePShader releases ptr only
	if (!RecompilePShader(pState->m_szCompShadersText, si, SHADER_COMP, false, pState->m_nCompPSVersion))
	{
		m_fallbackShaders_ps.comp.ptr->AddRef();
		m_fallbackShaders_ps.comp.CT->AddRef();
		memcpy(si, &m_fallbackShaders_ps.comp, sizeof(PShaderInfo));
	}
}

// SURI: what Esc does for each overlay, all at once: help off, back to the regular UI
// (menus, preset browser, mash-up) and any text entry cancelled.
void CPlugin::CloseOverlays()
{
	m_show_help = 0;
	m_help_page = 0;
	m_waitstring.bActive = false;
	m_UI_mode = UI_REGULAR;
}

void CPlugin::SavePresetAs(wchar_t *szNewFile)
{
	// overwrites the file if it was already there,
	// so you should check if the file exists first & prompt user to overwrite,
	//   before calling this function

	// SURI: save without the injected effect blocks, and with the comp version it had before an effect upgraded it
	char  szCompBackup[MAX_BIGSTRING_LEN];
	int   nCompBackup = m_pState->m_nCompPSVersion, nMinBackup = m_pState->m_nMinPSVersion, nMaxBackup = m_pState->m_nMaxPSVersion;
	strcpy(szCompBackup, m_pState->m_szCompShadersText);
	{
		std::string text(szCompBackup);
		if (StripAllFx(text))
			strcpy(m_pState->m_szCompShadersText, text.c_str());
	}
	if (m_pState->m_nFxOrigCompPSVersion != -1)
	{
		m_pState->m_nCompPSVersion = m_pState->m_nFxOrigCompPSVersion;
		m_pState->m_nMaxPSVersion = max(m_pState->m_nWarpPSVersion, m_pState->m_nCompPSVersion);
		m_pState->m_nMinPSVersion = min(m_pState->m_nWarpPSVersion, m_pState->m_nCompPSVersion);
	}
	int spriteSlots[kMaxPresetSprites];	// SURI: sprites saved with the preset
	m_pState->m_nSprites = CollectLiveSprites(m_pState->m_sprites, kMaxPresetSprites, spriteSlots);
	bool bSaved = m_pState->Export(szNewFile);
	strcpy(m_pState->m_szCompShadersText, szCompBackup);
	m_pState->m_nCompPSVersion = nCompBackup;
	m_pState->m_nMinPSVersion = nMinBackup;
	m_pState->m_nMaxPSVersion = nMaxBackup;

	if (bSaved)
	{
		// SURI: once saved, these sprites belong to the preset: they leave with it and come back with it
		for (int k=0; k<m_pState->m_nSprites; k++)
			m_texmgr.m_tex[spriteSlots[k]].bOwnedByPreset = true;
	}

	if (!bSaved)
	{
		// error
        AddError(LoadResString(IDS_ERROR_UNABLE_TO_SAVE_THE_FILE), 6.0f, ERR_PRESET, true);
	}
	else
	{
		// pop up confirmation
        AddError(LoadResString(IDS_SAVE_SUCCESSFUL), 3.0f, ERR_NOTIFY, false);

		// update m_pState->m_szDesc with the new name
		lstrcpyW(m_pState->m_szDesc, m_waitstring.szText);

		// refresh file listing
		UpdatePresetList(false,true);
	}
}

void CPlugin::DeletePresetFile(wchar_t *szDelFile)
{
	// NOTE: this function additionally assumes that m_nPresetListCurPos indicates
	//		 the slot that the to-be-deleted preset occupies!

	// delete file
	if (!DeleteFileW(szDelFile))
	{
		// error
        AddError(LoadResString(IDS_ERROR_UNABLE_TO_DELETE_THE_FILE), 6.0f, ERR_MISC, true);
	}
	else
	{
		// pop up confirmation
		wchar_t buf[1024];
        swprintf(buf, LoadResString(IDS_PRESET_X_DELETED), m_presets[m_nPresetListCurPos].szFilename.c_str());
        AddError(buf, 3.0f, ERR_NOTIFY, false);

		// refresh file listing & re-select the next file after the one deleted
        int newPos = m_nPresetListCurPos;
		UpdatePresetList(false,true);
        m_nPresetListCurPos = max(0, min(m_nPresets-1, newPos));
	}
}

void CPlugin::RenamePresetFile(wchar_t *szOldFile, wchar_t *szNewFile)
{
	// NOTE: this function additionally assumes that m_nPresetListCurPos indicates
	//		 the slot that the to-be-renamed preset occupies!

	if (GetFileAttributesW(szNewFile) != -1)		// check if file already exists
	{
		// error
        AddError(LoadResString(IDS_ERROR_A_FILE_ALREADY_EXISTS_WITH_THAT_FILENAME), 6.0f, ERR_PRESET, true);

		// (user remains in UI_LOAD_RENAME mode to try another filename)
	}
	else
	{
		// rename
		if (!MoveFileW(szOldFile, szNewFile))
		{
			// error
            AddError(LoadResString(IDS_ERROR_UNABLE_TO_RENAME_FILE), 6.0f, ERR_MISC, true);
		}
		else
		{
			// pop up confirmation
            AddError(LoadResString(IDS_RENAME_SUCCESSFUL), 3.0f, ERR_NOTIFY, false);

			// if this preset was the active one, update m_pState->m_szDesc with the new name
			wchar_t buf[512];
			swprintf(buf, L"%s.milk", m_pState->m_szDesc);
			if (wcscmp(m_presets[m_nPresetListCurPos].szFilename.c_str(), buf) == 0)
			{
				lstrcpyW(m_pState->m_szDesc, m_waitstring.szText);
			}

			// refresh file listing & do a trick to make it re-select the renamed file
            wchar_t buf2[512];
			lstrcpyW(buf2, m_waitstring.szText);
			lstrcatW(buf2, L".milk");
            m_presets[m_nPresetListCurPos].szFilename = buf2;
            m_playlistIdxPresets = -1;   // SURI: the playlist cache may hold the old name's index
			UpdatePresetList(false,true,false);

            // jump to (highlight) the new file:
            m_nPresetListCurPos = 0;
            wchar_t* p = wcsrchr(szNewFile, L'\\');
            if (p)
            {
                p++;
		        for (int i=m_nDirs; i<m_nPresets; i++)
				{
                    if (wcscmp(p, m_presets[i].szFilename.c_str())==0) {
				        m_nPresetListCurPos = i;
                        break;
                    }
				}
            }
		}

		// exit waitstring mode (return to load menu)
		m_UI_mode = UI_LOAD;
		m_waitstring.bActive = false;
	}
}


void CPlugin::SetCurrentPresetRating(float fNewRating)
{
	if (!m_bEnableRating)
		return;

	if (fNewRating < 0) fNewRating = 0;
	if (fNewRating > 5) fNewRating = 5;
	float change = (fNewRating - m_pState->m_fRating);

	WritePrivateProfileFloatW(fNewRating, L"fRating", m_szCurrentPresetFile, L"preset00");

	// update the copy of the preset in memory
	m_pState->m_fRating = fNewRating;

	// update the cumulative internal listing:
	m_presets[m_nCurrentPreset].fRatingThis += change;
	if (m_nCurrentPreset != -1)// && m_nRatingReadProgress >= m_nCurrentPreset)		// (can be -1 if dir. changed but no new preset was loaded yet)
		for (int i=m_nCurrentPreset; i<m_nPresets; i++)
			m_presets[i].fRatingCum += change;

	/* keep in view:
		-test switching dirs w/o loading a preset, and trying to change the rating
			->m_nCurrentPreset is out of range!
		-soln: when adjusting rating:
			1. file to modify is m_szCurrentPresetFile
			2. only update CDF if m_nCurrentPreset is not -1
		-> set m_nCurrentPreset to -1 whenever dir. changes
		-> set m_szCurrentPresetFile whenever you load a preset
	*/

	// show a message
	if (!m_bShowRating)
	{
		// see also: DrawText() in milkdropfs.cpp
		m_fShowRatingUntilThisTime = GetTime() + 2.0f;
	}
}

void CPlugin::ReadCustomMessages()
{
	int n;

	// First, clear all old data
	for (n=0; n<MAX_CUSTOM_MESSAGE_FONTS; n++)
	{
		wcscpy(m_CustomMessageFont[n].szFace, L"arial");
		m_CustomMessageFont[n].bBold = false;
		m_CustomMessageFont[n].bItal = false;
		m_CustomMessageFont[n].nColorR = 255;
		m_CustomMessageFont[n].nColorG = 255;
		m_CustomMessageFont[n].nColorB = 255;
	}

	for (n=0; n<MAX_CUSTOM_MESSAGES; n++)
	{
		m_CustomMessage[n].szText[0] = 0;
		m_CustomMessage[n].nFont = 0;
		m_CustomMessage[n].fSize = 50.0f;  // [0..100]  note that size is not absolute, but relative to the size of the window
		m_CustomMessage[n].x = 0.5f;
		m_CustomMessage[n].y = 0.5f;
		m_CustomMessage[n].randx = 0;
		m_CustomMessage[n].randy = 0;
		m_CustomMessage[n].growth = 1.0f;
		m_CustomMessage[n].fTime = 1.5f;
		m_CustomMessage[n].fFade = 0.2f;

		m_CustomMessage[n].bOverrideBold = false;
		m_CustomMessage[n].bOverrideItal = false;
		m_CustomMessage[n].bOverrideFace = false;
		m_CustomMessage[n].bOverrideColorR = false;
		m_CustomMessage[n].bOverrideColorG = false;
		m_CustomMessage[n].bOverrideColorB = false;
		m_CustomMessage[n].bBold = false;
		m_CustomMessage[n].bItal = false;
		wcscpy(m_CustomMessage[n].szFace, L"arial");
		m_CustomMessage[n].nColorR = 255;
		m_CustomMessage[n].nColorG = 255;
		m_CustomMessage[n].nColorB = 255;
		m_CustomMessage[n].nRandR = 0;
		m_CustomMessage[n].nRandG = 0;
		m_CustomMessage[n].nRandB = 0;
	}

	// Then read in the new file
	for (n=0; n<MAX_CUSTOM_MESSAGE_FONTS; n++)
	{
		wchar_t szSectionName[32];
		swprintf(szSectionName, L"font%02d", n);

		// get face, bold, italic, x, y for this custom message FONT
		GetPrivateProfileStringW(szSectionName,L"face",L"arial",m_CustomMessageFont[n].szFace,_countof(m_CustomMessageFont[n].szFace), m_szMsgIniFile);
		m_CustomMessageFont[n].bBold	= GetPrivateProfileBoolW(szSectionName,L"bold",m_CustomMessageFont[n].bBold,  m_szMsgIniFile);
		m_CustomMessageFont[n].bItal	= GetPrivateProfileBoolW(szSectionName,L"ital",m_CustomMessageFont[n].bItal,  m_szMsgIniFile);
		m_CustomMessageFont[n].nColorR  = GetPrivateProfileIntW (szSectionName,L"r"     ,m_CustomMessageFont[n].nColorR, m_szMsgIniFile);
		m_CustomMessageFont[n].nColorG  = GetPrivateProfileIntW (szSectionName,L"g"     ,m_CustomMessageFont[n].nColorG, m_szMsgIniFile);
		m_CustomMessageFont[n].nColorB  = GetPrivateProfileIntW (szSectionName,L"b"     ,m_CustomMessageFont[n].nColorB, m_szMsgIniFile);
	}

	for (n=0; n<MAX_CUSTOM_MESSAGES; n++)
	{
		wchar_t szSectionName[64];
		swprintf(szSectionName, L"message%02d", n);

		// get fontID, size, text, etc. for this custom message:
		GetPrivateProfileStringW(szSectionName,L"text",L"",m_CustomMessage[n].szText,_countof(m_CustomMessage[n].szText), m_szMsgIniFile);
        if (m_CustomMessage[n].szText[0])
        {
		    m_CustomMessage[n].nFont	= GetPrivateProfileIntW  (szSectionName,L"font"  ,m_CustomMessage[n].nFont,   m_szMsgIniFile);
		    m_CustomMessage[n].fSize	= GetPrivateProfileFloatW(szSectionName,L"size"  ,m_CustomMessage[n].fSize,   m_szMsgIniFile);
		    m_CustomMessage[n].x		= GetPrivateProfileFloatW(szSectionName,L"x"     ,m_CustomMessage[n].x,       m_szMsgIniFile);
		    m_CustomMessage[n].y		= GetPrivateProfileFloatW(szSectionName,L"y"     ,m_CustomMessage[n].y,       m_szMsgIniFile);
		    m_CustomMessage[n].randx    = GetPrivateProfileFloatW(szSectionName,L"randx" ,m_CustomMessage[n].randx,   m_szMsgIniFile);
		    m_CustomMessage[n].randy    = GetPrivateProfileFloatW(szSectionName,L"randy" ,m_CustomMessage[n].randy,   m_szMsgIniFile);

		    m_CustomMessage[n].growth   = GetPrivateProfileFloatW(szSectionName,L"growth",m_CustomMessage[n].growth,  m_szMsgIniFile);
		    m_CustomMessage[n].fTime    = GetPrivateProfileFloatW(szSectionName,L"time"  ,m_CustomMessage[n].fTime,   m_szMsgIniFile);
		    m_CustomMessage[n].fFade    = GetPrivateProfileFloatW(szSectionName,L"fade"  ,m_CustomMessage[n].fFade,   m_szMsgIniFile);
		    m_CustomMessage[n].nColorR  = GetPrivateProfileIntW  (szSectionName,L"r"     ,m_CustomMessage[n].nColorR, m_szMsgIniFile);
		    m_CustomMessage[n].nColorG  = GetPrivateProfileIntW  (szSectionName,L"g"     ,m_CustomMessage[n].nColorG, m_szMsgIniFile);
		    m_CustomMessage[n].nColorB  = GetPrivateProfileIntW  (szSectionName,L"b"     ,m_CustomMessage[n].nColorB, m_szMsgIniFile);
		    m_CustomMessage[n].nRandR   = GetPrivateProfileIntW  (szSectionName,L"randr" ,m_CustomMessage[n].nRandR,  m_szMsgIniFile);
		    m_CustomMessage[n].nRandG   = GetPrivateProfileIntW  (szSectionName,L"randg" ,m_CustomMessage[n].nRandG,  m_szMsgIniFile);
		    m_CustomMessage[n].nRandB   = GetPrivateProfileIntW  (szSectionName,L"randb" ,m_CustomMessage[n].nRandB,  m_szMsgIniFile);

		    // overrides: r,g,b,face,bold,ital
		    GetPrivateProfileStringW(szSectionName,L"face",L"",m_CustomMessage[n].szFace,_countof(m_CustomMessage[n].szFace), m_szMsgIniFile);
		    m_CustomMessage[n].bBold	= GetPrivateProfileIntW (szSectionName, L"bold", -1, m_szMsgIniFile);
		    m_CustomMessage[n].bItal  	= GetPrivateProfileIntW (szSectionName, L"ital", -1, m_szMsgIniFile);
		    m_CustomMessage[n].nColorR  = GetPrivateProfileIntW (szSectionName, L"r"   , -1, m_szMsgIniFile);
		    m_CustomMessage[n].nColorG  = GetPrivateProfileIntW (szSectionName, L"g"   , -1, m_szMsgIniFile);
		    m_CustomMessage[n].nColorB  = GetPrivateProfileIntW (szSectionName, L"b"   , -1, m_szMsgIniFile);

		    m_CustomMessage[n].bOverrideFace   = (m_CustomMessage[n].szFace[0] != 0);
		    m_CustomMessage[n].bOverrideBold   = (m_CustomMessage[n].bBold != -1);
		    m_CustomMessage[n].bOverrideItal   = (m_CustomMessage[n].bItal != -1);
		    m_CustomMessage[n].bOverrideColorR = (m_CustomMessage[n].nColorR != -1);
		    m_CustomMessage[n].bOverrideColorG = (m_CustomMessage[n].nColorG != -1);
		    m_CustomMessage[n].bOverrideColorB = (m_CustomMessage[n].nColorB != -1);
        }
	}
}

void CPlugin::LaunchCustomMessage(int nMsgNum)
{
	if (nMsgNum > 99)
		nMsgNum = 99;

	if (nMsgNum < 0)
	{
		// choose randomly among the defined messages (SURI: no divide by zero when none are defined)
		bool hasText[MAX_CUSTOM_MESSAGES];
		for (int i=0; i<MAX_CUSTOM_MESSAGES; i++)
			hasText[i] = (m_CustomMessage[i].szText[0] != 0);
		nMsgNum = PickRandomMessage(hasText, MAX_CUSTOM_MESSAGES, rand());
		if (nMsgNum < 0)
			return;
	}

	if (nMsgNum < 0 ||
		nMsgNum >= MAX_CUSTOM_MESSAGES ||
		m_CustomMessage[nMsgNum].szText[0]==0)
	{
		return;
	}

	int fontID = m_CustomMessage[nMsgNum].nFont;

	m_supertext.bRedrawSuperText = true;
	m_supertext.bIsSongTitle = false;
	// SURI: bounded copy into the 256-unit supertext buffer, never splitting a surrogate pair
	lstrcpynW(m_supertext.szTextW, TruncateUtf16(m_CustomMessage[nMsgNum].szText, _countof(m_supertext.szTextW) - 1).c_str(), _countof(m_supertext.szTextW));

	// regular properties:
	m_supertext.fFontSize   = m_CustomMessage[nMsgNum].fSize;
	m_supertext.fX          = m_CustomMessage[nMsgNum].x + m_CustomMessage[nMsgNum].randx * ((rand()%1037)/1037.0f*2.0f - 1.0f);
	m_supertext.fY          = m_CustomMessage[nMsgNum].y + m_CustomMessage[nMsgNum].randy * ((rand()%1037)/1037.0f*2.0f - 1.0f);
	m_supertext.fGrowth     = m_CustomMessage[nMsgNum].growth;
	m_supertext.fDuration   = m_CustomMessage[nMsgNum].fTime;
	m_supertext.fFadeTime   = m_CustomMessage[nMsgNum].fFade;
	m_supertext.fHoldTime   = 0;
	m_supertext.fOutFadeTime = 0;

	// overrideables:
	if (m_CustomMessage[nMsgNum].bOverrideFace)
		lstrcpyW(m_supertext.nFontFace, m_CustomMessage[nMsgNum].szFace);
	else
		lstrcpyW(m_supertext.nFontFace, m_CustomMessageFont[fontID].szFace);
	m_supertext.bItal   = (m_CustomMessage[nMsgNum].bOverrideItal) ? (m_CustomMessage[nMsgNum].bItal != 0) : (m_CustomMessageFont[fontID].bItal != 0);
	m_supertext.bBold   = (m_CustomMessage[nMsgNum].bOverrideBold) ? (m_CustomMessage[nMsgNum].bBold != 0) : (m_CustomMessageFont[fontID].bBold != 0);
	m_supertext.nColorR = (m_CustomMessage[nMsgNum].bOverrideColorR) ? m_CustomMessage[nMsgNum].nColorR : m_CustomMessageFont[fontID].nColorR;
	m_supertext.nColorG = (m_CustomMessage[nMsgNum].bOverrideColorG) ? m_CustomMessage[nMsgNum].nColorG : m_CustomMessageFont[fontID].nColorG;
	m_supertext.nColorB = (m_CustomMessage[nMsgNum].bOverrideColorB) ? m_CustomMessage[nMsgNum].nColorB : m_CustomMessageFont[fontID].nColorB;

	// randomize color
	m_supertext.nColorR += (int)(m_CustomMessage[nMsgNum].nRandR * ((rand()%1037)/1037.0f*2.0f - 1.0f));
	m_supertext.nColorG += (int)(m_CustomMessage[nMsgNum].nRandG * ((rand()%1037)/1037.0f*2.0f - 1.0f));
	m_supertext.nColorB += (int)(m_CustomMessage[nMsgNum].nRandB * ((rand()%1037)/1037.0f*2.0f - 1.0f));
	if (m_supertext.nColorR < 0) m_supertext.nColorR = 0;
	if (m_supertext.nColorG < 0) m_supertext.nColorG = 0;
	if (m_supertext.nColorB < 0) m_supertext.nColorB = 0;
	if (m_supertext.nColorR > 255) m_supertext.nColorR = 255;
	if (m_supertext.nColorG > 255) m_supertext.nColorG = 255;
	if (m_supertext.nColorB > 255) m_supertext.nColorB = 255;

	// fix &'s for display:

	m_supertext.fStartTime = GetTime();
}

// SURI: when the current supertext (song title or custom message) is gone
float CPlugin::SuperTextEndTime() const
{
	if (m_supertext.bIsSongTitle)
		return m_supertext.fStartTime + SongTitleEnd(m_supertext.fDuration, m_supertext.fHoldTime, m_supertext.fOutFadeTime);
	return m_supertext.fStartTime + m_supertext.fDuration;
}

void CPlugin::LaunchSongTitleAnim(bool bNowPlaying)
{
	// SURI: a now-playing title stays readable, then fades into the visuals; other texts
	// (random titles, "Always On Top") keep the classic fly-in + burn-in
	m_supertext.fHoldTime    = bNowPlaying ? max(0.0f, m_fSongTitleHold) : 0.0f;
	m_supertext.fOutFadeTime = bNowPlaying ? max(0.0f, m_fSongTitleFade) : 0.0f;
	m_supertext.bRedrawSuperText = true;
	m_supertext.bIsSongTitle = true;
	// SURI: m_szSongTitle (512) is longer than szTextW (256): bounded copy, never splitting a surrogate pair
	lstrcpynW(m_supertext.szTextW, TruncateUtf16(m_szSongTitle, _countof(m_supertext.szTextW) - 1).c_str(), _countof(m_supertext.szTextW));
	lstrcpyW(m_supertext.nFontFace, m_fontinfo[SONGTITLE_FONT].szFace);
	m_supertext.fFontSize   = (float)m_fontinfo[SONGTITLE_FONT].nSize;
	m_supertext.bBold       = m_fontinfo[SONGTITLE_FONT].bBold;
	m_supertext.bItal       = m_fontinfo[SONGTITLE_FONT].bItalic;
	m_supertext.fX          = 0.5f;
	m_supertext.fY          = 0.5f;
	m_supertext.fGrowth     = 1.0f;
	m_supertext.fDuration   = m_fSongTitleAnimDuration;
	m_supertext.nColorR     = 255;
	m_supertext.nColorG     = 255;
	m_supertext.nColorB     = 255;

	m_supertext.fStartTime = GetTime();
}

bool CPlugin::LaunchSprite(int nSpriteNum, int nSlot)
{
	char initcode[8192], code[8192], sectionA[64];
	char szTemp[8192];
	wchar_t img[512], section[64];

	initcode[0] = 0;
	code[0] = 0;
	img[0] = 0;
	swprintf(section, L"img%02d", nSpriteNum);
	sprintf(sectionA, "img%02d", nSpriteNum);

	// 1. read in image filename
	GetPrivateProfileStringW(section, L"img", L"", img, _countof(img), m_szImgIniFile);	// SURI: a character count, not bytes
	if (img[0] == 0)
	{
        wchar_t buf[1024];
		swprintf(buf, LoadResString(IDS_SPRITE_X_ERROR_COULD_NOT_FIND_IMG_OR_NOT_DEFINED), nSpriteNum);
        AddError(buf, 7.0f, ERR_MISC, false);
		return false;
	}

	if (img[1] != L':')// || img[2] != '\\')
	{
		// it's not in the form "x:\blah\billy.jpg" so prepend plugin dir path.
		wchar_t temp[512];
		wcscpy(temp, img);
		swprintf(img, L"%s%s", m_szMilkdrop2Path, temp);
	}

	// 2. get color key
	//unsigned int ck_lo = (unsigned int)GetPrivateProfileInt(section, "colorkey_lo", 0x00000000, m_szImgIniFile);
	//unsigned int ck_hi = (unsigned int)GetPrivateProfileInt(section, "colorkey_hi", 0x00202020, m_szImgIniFile);
    // FIRST try 'colorkey_lo' (for backwards compatibility) and then try 'colorkey'
    unsigned int ck = (unsigned int)GetPrivateProfileIntW(section, L"colorkey_lo", 0x00000000, m_szImgIniFile/*GetConfigIniFile()*/);
    ck = (unsigned int)GetPrivateProfileIntW(section, L"colorkey", ck, m_szImgIniFile/*GetConfigIniFile()*/);

	// 3. read in init code & per-frame code
	for (int n=0; n<2; n++)
	{
		char *pStr = (n==0) ? initcode : code;
		char szLineName[32];
		int len;

		int line = 1;
		int char_pos = 0;
		bool bDone = false;

		while (!bDone)
		{
			if (n==0)
				sprintf(szLineName, "init_%d", line);
			else
				sprintf(szLineName, "code_%d", line);

			GetPrivateProfileString(sectionA, szLineName, "~!@#$", szTemp, 8192, AutoCharFn(m_szImgIniFile));	// fixme
			len = lstrlen(szTemp);

			if ((strcmp(szTemp, "~!@#$")==0) ||		// if the key was missing,
				(len >= 8191-char_pos-1))			// or if we're out of space
			{
				bDone = true;
			}
			else
			{
				sprintf(&pStr[char_pos], "%s%c", szTemp, LINEFEED_CONTROL_CHAR);
			}

			char_pos += len + 1;
			line++;
		}
		pStr[char_pos++] = 0;	// null-terminate
	}

	if (nSlot == -1)
		nSlot = FindSpriteSlot();

	int ret = m_texmgr.LoadTex(img, nSlot, initcode, code, GetTime(), GetFrame(), ck);
	m_texmgr.m_tex[nSlot].nUserData = nSpriteNum;
	if ((ret & TEXMGR_ERROR_MASK) == 0)
		m_nSelectedSprite = nSlot;

    wchar_t buf[1024];
	switch(ret & TEXMGR_ERROR_MASK)
	{
	case TEXMGR_ERR_SUCCESS:
		switch(ret & TEXMGR_WARNING_MASK)
		{
		case TEXMGR_WARN_ERROR_IN_INIT_CODE:
            swprintf(buf, LoadResString(IDS_SPRITE_X_WARNING_ERROR_IN_INIT_CODE), nSpriteNum);
            AddError(buf, 6.0f, ERR_MISC, true);
            break;
		case TEXMGR_WARN_ERROR_IN_REG_CODE:
            swprintf(buf, LoadResString(IDS_SPRITE_X_WARNING_ERROR_IN_PER_FRAME_CODE), nSpriteNum);
            AddError(buf, 6.0f, ERR_MISC, true);
            break;
		default:
			// success; no errors OR warnings.
			break;
		}
		break;
	case TEXMGR_ERR_BAD_INDEX:
        swprintf(buf, LoadResString(IDS_SPRITE_X_ERROR_BAD_SLOT_INDEX), nSpriteNum);
        AddError(buf, 6.0f, ERR_MISC, true);
        break;
    case TEXMGR_ERR_FORMAT:
    case TEXMGR_ERR_BADFILE:
        swprintf(buf, LoadResString(IDS_SPRITE_X_ERROR_IMAGE_FILE_MISSING_OR_CORRUPT), nSpriteNum);
        AddError(buf, 6.0f, ERR_MISC, true);
        break;
    case TEXMGR_ERR_OUTOFMEM:
        swprintf(buf, LoadResString(IDS_SPRITE_X_ERROR_OUT_OF_MEM), nSpriteNum);
        AddError(buf, 6.0f, ERR_MISC, true);
        break;
	}

	return (ret & TEXMGR_ERROR_MASK) ? false : true;
}

int CPlugin::FindSpriteSlot()
{
	// find first empty slot; if none, chuck the oldest sprite & take its slot.
	int nSlot = -1;
	int oldest_index = 0;
	int oldest_frame = m_texmgr.m_tex[0].nStartFrame;
	for (int x=0; x<NUM_TEX; x++)
	{
		if (!m_texmgr.m_tex[x].pSurface)
		{
			nSlot = x;
			break;
		}
		else if (m_texmgr.m_tex[x].nStartFrame < oldest_frame)
		{
			oldest_index = x;
			oldest_frame = m_texmgr.m_tex[x].nStartFrame;
		}
	}

	if (nSlot == -1)
	{
		nSlot = oldest_index;
		KillSprite(nSlot);
	}
	return nSlot;
}

// SURI: a sprite placed by a SpriteDesc (preset sprite list / sprite editor): init code from
// SpriteInitCode, no per-frame code. The colour key follows SpriteLoadColorKey (blend 5 with
// colorkey 0 = no key, so black stays opaque).
bool CPlugin::LaunchStaticSprite(const SpriteDesc& d, bool ownedByPreset)
{
	static_assert(kSpriteNoColorKey == TEXMGR_NO_COLORKEY, "no-colour-key values must match");
	return LaunchStaticSpriteEx(d, ownedByPreset, SpriteLoadColorKey(d), -1);
}

bool CPlugin::LaunchStaticSpriteEx(const SpriteDesc& d, bool ownedByPreset, unsigned ck, int nSlot)
{
	std::wstring path = ResolveSpritePath(m_szMilkdrop2Path, d.img);
	wchar_t img[512];
	lstrcpynW(img, path.c_str(), _countof(img));

	std::string init = SpriteInitCode(d);
	char initcode[8192];
	char code[8192];
	lstrcpynA(initcode, init.c_str(), _countof(initcode));
	code[0] = 0;

	if (nSlot < 0 || nSlot >= NUM_TEX)
		nSlot = FindSpriteSlot();
	else if (m_texmgr.m_tex[nSlot].pSurface)
		KillSprite(nSlot);
	int ret = m_texmgr.LoadTex(img, nSlot, initcode, code, GetTime(), GetFrame(), ck);
	m_texmgr.m_tex[nSlot].nUserData = -1;
	if (ret & TEXMGR_ERROR_MASK)
	{
		// file name only, without the folder
		const wchar_t* name = d.img;
		for (const wchar_t* p = d.img; *p; p++)
			if (*p == L'\\' || *p == L'/')
				name = p + 1;
		wchar_t buf[1024];
		StringCchPrintfW(buf, _countof(buf), LoadResString(IDS_SPRITE_FILE_ERROR), name);
		AddError(buf, 6.0f, ERR_MISC, true);
		return false;
	}
	m_texmgr.m_tex[nSlot].bOwnedByPreset = ownedByPreset;
	m_nSelectedSprite = nSlot;
	return true;
}

// SURI: the preset's own sprites leave with it; the new preset's sprites start now (also when
// its blend starts later in LoadPresetTick). A missing image shows one error and is skipped.
void CPlugin::LaunchPresetSprites(const CState* pState)
{
	// a preset load does not change which sprite is selected (unless that sprite leaves)
	int sel = m_nSelectedSprite;
	int selStart = (sel >= 0 && sel < NUM_TEX) ? m_texmgr.m_tex[sel].nStartFrame : 0;
	for (int iSlot=0; iSlot<NUM_TEX; iSlot++)
		if (m_texmgr.m_tex[iSlot].pSurface && m_texmgr.m_tex[iSlot].bOwnedByPreset)
			KillSprite(iSlot);
	int n = min(pState->m_nSprites, kMaxPresetSprites);
	for (int k=0; k<n; k++)
		LaunchStaticSprite(pState->m_sprites[k], true);
	bool selAlive = (sel >= 0 && sel < NUM_TEX && m_texmgr.m_tex[sel].pSurface &&
	                 !m_texmgr.m_tex[sel].bOwnedByPreset && m_texmgr.m_tex[sel].nStartFrame == selStart);
	m_nSelectedSprite = selAlive ? sel : -1;
}

// SURI: the live sprites as preset entries, oldest first, where they are on the screen now:
// x,y in 0..1 like var_x/var_y (userDx/userDy are added to var_x*2-1 when drawing), the size
// with the mouse-wheel scale, the blend mode as drawn and the colorkey the image was loaded with.
int CPlugin::CollectLiveSprites(SpriteDesc* out, int maxOut, int* slotsOut)
{
	if (!out || maxOut <= 0)
		return 0;
	int order[NUM_TEX];
	int nLive = 0;
	for (int i=0; i<NUM_TEX; i++)
	{
		const td_tex& t = m_texmgr.m_tex[i];
		if (!t.pSurface || !t.var_x || !t.var_y || !t.var_sx || !t.var_rot || !t.var_blendmode)
			continue;
		int k = nLive++;	// insertion sort by start frame; equal frames keep slot order
		while (k > 0 && m_texmgr.m_tex[order[k-1]].nStartFrame > t.nStartFrame)
		{
			order[k] = order[k-1];
			k--;
		}
		order[k] = i;
	}
	int n = 0;
	for (int j=0; j<nLive && n<maxOut; j++)
	{
		const td_tex& t = m_texmgr.m_tex[order[j]];
		std::wstring img = MakeSpritePathRelative(m_szMilkdrop2Path, t.szFileName);
		if (img.empty() || img.size() >= MAX_PATH)
			continue;
		SpriteDesc& d = out[n];
		ZeroMemory(&d, sizeof(d));
		lstrcpynW(d.img, img.c_str(), MAX_PATH);
		// same clamp as the renderer (DrawUserSprites) before the user offsets are added
		float cx = min(1000.0f, max(-1000.0f, (float)*t.var_x * 2.0f - 1.0f));
		float cy = min(1000.0f, max(-1000.0f, (float)*t.var_y * 2.0f - 1.0f));
		d.x = (cx + t.userDx + 1.0f) * 0.5f;
		d.y = (cy + t.userDy + 1.0f) * 0.5f;
		d.scale = (float)*t.var_sx * t.userScale;
		d.rot = (float)*t.var_rot;
		d.blend = (t.userBlend >= 0) ? min(5, t.userBlend) : min(5, max(0, (int)(*t.var_blendmode)));
		// a sprite loaded without a colour key is saved as colorkey 0: with blend 5 that reloads
		// without a key again (SpriteLoadColorKey); with blend 4 it reloads keying out black
		d.colorkey = (t.colorkey == TEXMGR_NO_COLORKEY) ? 0 : (t.colorkey & 0xFFFFFFu);
		if (slotsOut)
			slotsOut[n] = order[j];
		n++;
	}
	return n;
}

// SURI: CleanUpMyDX9Stuff (routine cleanup) frees every sprite texture with m_texmgr.Finish;
// keep the live sprites here so AllocateMyDX9Stuff can put them back. Sprites with EEL motion
// code (milk2_img.ini) come back as static snapshots of where they were.
void CPlugin::SnapshotSprites()
{
	if (m_nSpriteSnap > 0)
		return;		// a cleanup without a realloc in between: the texmgr is already empty
	SpriteDesc descs[NUM_TEX];
	int slots[NUM_TEX];
	int n = CollectLiveSprites(descs, NUM_TEX, slots);
	m_nSpriteSnapSel = -1;
	for (int i=0; i<n; i++)
	{
		const td_tex& t = m_texmgr.m_tex[slots[i]];
		SpriteSnapshot& s = m_spriteSnap[i];
		s.desc        = descs[i];
		s.ck          = t.colorkey;
		s.slot        = slots[i];
		s.owned       = t.bOwnedByPreset;
		s.nStartFrame = t.nStartFrame;
		s.fStartTime  = t.fStartTime;
		if (slots[i] == m_nSelectedSprite)
			m_nSpriteSnapSel = i;
	}
	m_nSpriteSnap = n;
	m_nSelectedSprite = -1;
	m_bSpriteDrag = false;
}

void CPlugin::RelaunchSnapshotSprites()
{
	int n = m_nSpriteSnap;
	m_nSpriteSnap = 0;
	int sel = -1;
	for (int i=0; i<n; i++)
	{
		const SpriteSnapshot& s = m_spriteSnap[i];
		if (!LaunchStaticSpriteEx(s.desc, s.owned, s.ck, s.slot))
			continue;
		// same age as before, so "oldest first" (saving, slot reuse) does not change
		m_texmgr.m_tex[s.slot].nStartFrame = s.nStartFrame;
		m_texmgr.m_tex[s.slot].fStartTime  = s.fStartTime;
		if (i == m_nSpriteSnapSel)
			sel = s.slot;
	}
	m_nSelectedSprite = sel;
	m_nSpriteSnapSel = -1;
}

void CPlugin::KillSprite(int iSlot)
{
	m_texmgr.KillTex(iSlot);
	if (m_nSelectedSprite == iSlot)
		m_nSelectedSprite = -1;
}

// SURI: topmost live sprite under a point of the client area, -1 = none. m_spriteQuad keeps the
// corners of the last frame each slot was drawn in, so dead slots are skipped.
int CPlugin::SpriteAtClientPoint(HWND hWnd, int x, int y)
{
	RECT rc;
	if (!GetClientRect(hWnd, &rc) || rc.right <= 0 || rc.bottom <= 0)
		return -1;
	float cx, cy;
	WindowToClip(x, y, rc.right, rc.bottom, &cx, &cy);
	// sprites are drawn with a y-down projection (Ortho2D 2 x -2, see support.cpp), so the y of
	// m_spriteQuad grows towards the bottom of the window, unlike WindowToClip's clip space
	cy = -cy;
	SpriteHit hits[NUM_TEX];
	for (int i=0; i<NUM_TEX; i++)
	{
		hits[i].live = (m_texmgr.m_tex[i].pSurface != NULL);
		hits[i].startFrame = m_texmgr.m_tex[i].nStartFrame;
		memcpy(hits[i].q, m_spriteQuad[i], sizeof(hits[i].q));
	}
	return TopmostSpriteAt(hits, NUM_TEX, cx, cy);
}

// SURI: left-drag moves a sprite, the wheel over a sprite resizes it. Returns false (message not
// used) when the mouse is not on a sprite, so the window keeps its old behaviour.
bool CPlugin::SpriteMouseMessage(HWND hWnd, unsigned uMsg, WPARAM wParam, LPARAM lParam)
{
	// SURI: no picking up or scaling sprites under the playlist screen
	if ((m_UI_mode == UI_PLAYLIST || m_UI_mode == UI_IMPORT_NEW) && !m_bSpriteDrag &&
	    (uMsg == WM_LBUTTONDOWN || uMsg == WM_MOUSEWHEEL))
		return false;

	switch (uMsg)
	{
	case WM_LBUTTONDOWN:
		{
			int x = (int)(short)LOWORD(lParam), y = (int)(short)HIWORD(lParam);
			int slot = SpriteAtClientPoint(hWnd, x, y);
			if (slot < 0)
				return false;
			m_nSelectedSprite = slot;
			m_bSpriteDrag = true;
			m_ptSpriteDrag.x = x;
			m_ptSpriteDrag.y = y;
			SetCapture(hWnd);
			return true;
		}

	case WM_MOUSEMOVE:
		{
			if (!m_bSpriteDrag)
				return false;
			int sel = m_nSelectedSprite;
			if (sel < 0 || sel >= NUM_TEX || !m_texmgr.m_tex[sel].pSurface)
			{
				// the sprite went away during the drag
				m_bSpriteDrag = false;
				ReleaseCapture();
				return true;
			}
			int x = (int)(short)LOWORD(lParam), y = (int)(short)HIWORD(lParam);
			RECT rc;
			GetClientRect(hWnd, &rc);
			if (rc.right > 0 && rc.bottom > 0 && GetHeight() > 0)
			{
				float c0x, c0y, c1x, c1y, dx, dy;
				WindowToClip(m_ptSpriteDrag.x, m_ptSpriteDrag.y, rc.right, rc.bottom, &c0x, &c0y);
				WindowToClip(x, y, rc.right, rc.bottom, &c1x, &c1y);
				// -dy: sprite y grows downwards (see SpriteAtClientPoint)
				ClipDeltaToSpriteDelta(c1x - c0x, -(c1y - c0y), GetWidth() / (float)GetHeight(), &dx, &dy);
				m_texmgr.m_tex[sel].userDx += dx;
				m_texmgr.m_tex[sel].userDy += dy;
			}
			m_ptSpriteDrag.x = x;
			m_ptSpriteDrag.y = y;
			return true;
		}

	case WM_LBUTTONUP:
		if (!m_bSpriteDrag)
			return false;
		m_bSpriteDrag = false;
		ReleaseCapture();
		return true;

	case WM_CAPTURECHANGED:
		m_bSpriteDrag = false;
		return false;

	case WM_MOUSEWHEEL:
		{
			POINT p;
			p.x = (int)(short)LOWORD(lParam);	// screen coordinates
			p.y = (int)(short)HIWORD(lParam);
			ScreenToClient(hWnd, &p);
			int slot = SpriteAtClientPoint(hWnd, p.x, p.y);
			if (slot < 0)
				return false;
			m_nSelectedSprite = slot;
			m_texmgr.m_tex[slot].userScale = WheelScale(m_texmgr.m_tex[slot].userScale, GET_WHEEL_DELTA_WPARAM(wParam));
			return true;
		}
	}
	return false;
}

void CPlugin::DoCustomSoundAnalysis()
{
    memcpy(mysound.fWave[0], m_sound.fWaveform[0], sizeof(float)*576);
    memcpy(mysound.fWave[1], m_sound.fWaveform[1], sizeof(float)*576);

    // do our own [UN-NORMALIZED] fft
	float fWaveLeft[576];
	for (int i=0; i<576; i++)
        fWaveLeft[i] = m_sound.fWaveform[0][i];

	memset(mysound.fSpecLeft, 0, sizeof(float)*MY_FFT_SAMPLES);

	myfft.time_to_frequency_domain(fWaveLeft, mysound.fSpecLeft);

	// sum spectrum up into 3 bands
	for (i=0; i<3; i++)
	{
		// note: only look at bottom half of spectrum!  (hence divide by 6 instead of 3)
		int start = MY_FFT_SAMPLES*i/6;
		int end   = MY_FFT_SAMPLES*(i+1)/6;
		int j;

		mysound.imm[i] = 0;

		for (j=start; j<end; j++)
			mysound.imm[i] += mysound.fSpecLeft[j];
	}

	// do temporal blending to create attenuated and super-attenuated versions
	for (i=0; i<3; i++)
	{
        float rate;

		if (mysound.imm[i] > mysound.avg[i])
			rate = 0.2f;
		else
			rate = 0.5f;
        rate = AdjustRateToFPS(rate, 30.0f, GetFps());
        mysound.avg[i] = mysound.avg[i]*rate + mysound.imm[i]*(1-rate);

		if (GetFrame() < 50)
			rate = 0.9f;
		else
			rate = 0.992f;
        rate = AdjustRateToFPS(rate, 30.0f, GetFps());
        mysound.long_avg[i] = mysound.long_avg[i]*rate + mysound.imm[i]*(1-rate);


		// also get bass/mid/treble levels *relative to the past*
		if (fabsf(mysound.long_avg[i]) < 0.001f)
			mysound.imm_rel[i] = 1.0f;
		else
			mysound.imm_rel[i]  = mysound.imm[i] / mysound.long_avg[i];

		if (fabsf(mysound.long_avg[i]) < 0.001f)
			mysound.avg_rel[i]  = 1.0f;
		else
			mysound.avg_rel[i]  = mysound.avg[i] / mysound.long_avg[i];
	}
}

void CPlugin::GenWarpPShaderText(char *szShaderText, float decay, bool bWrap)
{
    // find the pixel shader body and replace it with custom code.

    lstrcpy(szShaderText, m_szDefaultWarpPShaderText);
    char LF = LINEFEED_CONTROL_CHAR;
    char *p = strrchr( szShaderText, '{' );
    if (!p)
        return;
    p++;
    p += sprintf(p, "%c", 1);

    p += sprintf(p, "    // sample previous frame%c", LF);
    p += sprintf(p, "    ret = tex2D( sampler%s_main, uv ).xyz;%c", bWrap ? L"" : L"_fc", LF);
    p += sprintf(p, "    %c", LF);
    p += sprintf(p, "    // darken (decay) over time%c", LF);
    p += sprintf(p, "    ret *= %.2f; //or try: ret -= 0.004;%c", decay, LF);
    p += sprintf(p, "}%c", LF);
}

void CPlugin::GenCompPShaderText(char *szShaderText, float brightness, float ve_alpha, float ve_zoom, int ve_orient, float hue_shader, bool bBrighten, bool bDarken, bool bSolarize, bool bInvert)
{
    // find the pixel shader body and replace it with custom code.

    lstrcpy(szShaderText, m_szDefaultCompPShaderText);
    char LF = LINEFEED_CONTROL_CHAR;
    char *p = strrchr( szShaderText, '{' );
    if (!p)
        return;
    p++;
    p += sprintf(p, "%c", 1);

    if (ve_alpha > 0.001f)
    {
        int orient_x = (ve_orient %  2) ? -1 : 1;
        int orient_y = (ve_orient >= 2) ? -1 : 1;
        p += sprintf(p, "    float2 uv_echo = (uv - 0.5)*%.3f*float2(%d,%d) + 0.5;%c", 1.0f/ve_zoom, orient_x, orient_y, LF);
        p += sprintf(p, "    ret = lerp( tex2D(sampler_main, uv).xyz, %c", LF);
        p += sprintf(p, "                tex2D(sampler_main, uv_echo).xyz, %c", LF);
        p += sprintf(p, "                %.2f %c", ve_alpha, LF);
        p += sprintf(p, "              ); //video echo%c", LF);
        p += sprintf(p, "    ret *= %.2f; //gamma%c", brightness, LF);
    }
    else
    {
        p += sprintf(p, "    ret = tex2D(sampler_main, uv).xyz;%c", LF);
        p += sprintf(p, "    ret *= %.2f; //gamma%c", brightness, LF);
    }
    if (hue_shader >= 1.0f)
        p += sprintf(p, "    ret *= hue_shader; //old hue shader effect%c", LF);
    else if (hue_shader > 0.001f)
        p += sprintf(p, "    ret *= %.2f + %.2f*hue_shader; //old hue shader effect%c", 1-hue_shader, hue_shader, LF);

    if (bBrighten)
        p += sprintf(p, "    ret = sqrt(ret); //brighten%c", LF);
    if (bDarken)
        p += sprintf(p, "    ret *= ret; //darken%c", LF);
    if (bSolarize)
        p += sprintf(p, "    ret = ret*(1-ret)*4; //solarize%c", LF);
    if (bInvert)
        p += sprintf(p, "    ret = 1 - ret; //invert%c", LF);
    p += sprintf(p, "}%c", LF);
}


void CPlugin::GetSongTitle(wchar_t *szSongTitle, int nSize)
{
    // SURI: the current song from Windows media controls (now_playing_winrt.cpp), or empty
    if (nSize <= 0)
        return;
    szSongTitle[0] = 0;
    if (!m_bNowPlaying)
        return;
    std::wstring t = TruncateUtf16(m_songBox.Text(), (size_t)(nSize - 1));
    memcpy(szSongTitle, t.c_str(), (t.size() + 1) * sizeof(wchar_t));
}
