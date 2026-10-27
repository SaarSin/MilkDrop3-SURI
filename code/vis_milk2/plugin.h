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

#ifndef __NULLSOFT_DX9_EXAMPLE_PLUGIN_H__
#define __NULLSOFT_DX9_EXAMPLE_PLUGIN_H__ 1

#include "pluginshell.h"
#include <shellapi.h>   // SURI: HDROP (drag & drop)
#include "md_defines.h"
#include "menu.h"
#include "support.h"
#include "texmgr.h"
#include "state.h"
#include <vector>
#include "../ns-eel2/ns-eel.h"
#include "features/commands.h"
#include "features/shader_fx.h"
#include "features/ui_scale.h"
#include "features/now_playing.h"
#include "features/now_playing_winrt.h"
#include "features/sprite_preset.h"
#include "features/playlist.h"
#include "features/playlist_screen.h"
#include <string>

extern "C" int (*warand)(void);

typedef enum { TEX_DISK, TEX_VS, TEX_BLUR0, TEX_BLUR1, TEX_BLUR2, TEX_BLUR3, TEX_BLUR4, TEX_BLUR5, TEX_BLUR6, TEX_BLUR_LAST } tex_code;
typedef enum { UI_REGULAR, UI_MENU, UI_LOAD, UI_LOAD_DEL, UI_LOAD_RENAME, UI_SAVEAS, UI_SAVE_OVERWRITE, UI_EDIT_MENU_STRING, UI_CHANGEDIR, UI_IMPORT_WAVE, UI_EXPORT_WAVE, UI_IMPORT_SHAPE, UI_EXPORT_SHAPE, UI_UPGRADE_PIXEL_SHADER, UI_MASHUP, UI_PLAYLIST, UI_IMPORT_NEW } ui_mode;
typedef struct { float rad; float ang; float a; float c;  } td_vertinfo; // blending: mix = max(0,min(1,a*t + c));
typedef char* CHARPTR;

#define MY_FFT_SAMPLES 512     // for old [pre-vms] milkdrop sound analysis
typedef struct
{
	float   imm[3];			// bass, mids, treble (absolute)
	float	imm_rel[3];		// bass, mids, treble (relative to song; 1=avg, 0.9~below, 1.1~above)
	float	avg[3];			// bass, mids, treble (absolute)
	float	avg_rel[3];		// bass, mids, treble (relative to song; 1=avg, 0.9~below, 1.1~above)
	float	long_avg[3];	// bass, mids, treble (absolute)
    float   fWave[2][576];
    float   fSpecLeft[MY_FFT_SAMPLES];
} td_mysounddata;

typedef struct
{
	int 	bActive;
	int 	bFilterBadChars;	// if true, it will filter out any characters that don't belong in a filename, plus the & symbol (because it doesn't display properly with DrawText)
	int 	bDisplayAsCode;		// if true, semicolons will be followed by a newline, for display
	int		nMaxLen;			// can't be more than 511
	int		nCursorPos;
	int		nSelAnchorPos;		// -1 if no selection made
	int 	bOvertypeMode;
	wchar_t	szText[48000];
	wchar_t	szPrompt[512];
	wchar_t	szToolTip[512];
	char	szClipboard[48000];
	wchar_t	szClipboardW[48000];
} td_waitstr;

typedef struct
{
	int 	bBold;
	int 	bItal;
	wchar_t	szFace[128];
	int		nColorR;    // 0..255
	int		nColorG;    // 0..255
	int		nColorB;    // 0..255
}
td_custom_msg_font;

enum
{
	MD2_PS_NONE = 0,
	MD2_PS_2_0 = 2,
	MD2_PS_2_X = 3,
	MD2_PS_3_0 = 4,
	MD2_PS_4_0 = 5, // not supported by milkdrop
};

typedef struct
{
	int		nFont;
	float	fSize;	// 0..100
	float	x;
	float	y;
	float	randx;
	float   randy;
	float	growth;
	float	fTime;	// total time to display the message, in seconds
	float	fFade;	// % (0..1) of the time that is spent fading in

	// overrides
	int     bOverrideBold;
	int     bOverrideItal;
	int     bOverrideFace;
	int     bOverrideColorR;
	int     bOverrideColorG;
	int     bOverrideColorB;
	int		nColorR;    // 0..255
	int		nColorG;    // 0..255
	int		nColorB;    // 0..255
	int  	nRandR;
	int     nRandG;
	int  	nRandB;
	int     bBold;
	int     bItal;
	wchar_t szFace[128];

	wchar_t	szText[256];
}
td_custom_msg;

typedef struct
{
	int 	bRedrawSuperText;	// true if it needs redraw
	int 	bIsSongTitle;		// false for custom message, true for song title
	wchar_t	szTextW[256];
	wchar_t	nFontFace[128];
	int 	bBold;
	int 	bItal;
	float	fX;
	float   fY;
	float	fFontSize;			// [0..100] for custom messages, [0..4] for song titles
	float   fGrowth;			// applies to custom messages only
	int		nFontSizeUsed;		// height IN PIXELS
	float	fStartTime;
	float	fDuration;
	float	fFadeTime;			// applies to custom messages only; song title fade times are handled specially
	float	fHoldTime;			// SURI: song title only: seconds on screen after the fly-in (0 = classic burn-in)
	float	fOutFadeTime;		// SURI: song title only: seconds to fade out after fHoldTime
	int  	nColorR;
	int     nColorG;
	int  	nColorB;
}
td_supertext;

typedef struct
{
    wchar_t        texname[256];   // ~filename, but without path or extension!
    LPDIRECT3DBASETEXTURE9 texptr;
    int                w,h,d;
    bool               bEvictable;
    int                 nAge;   // only valid if bEvictable is true
    int                 nSizeInBytes;    // only valid if bEvictable is true
} TexInfo;

typedef struct
{
    std::wstring    texname;  // just for ref
    D3DXHANDLE texsize_param;
    int        w,h;
} TexSizeParamInfo;

typedef struct
{
    LPDIRECT3DBASETEXTURE9 texptr;
    bool               bBilinear;
    bool               bWrap;
} SamplerInfo;

typedef std::vector<CShaderParams*> CShaderParamsList;

class CShaderParams
{
public:
    // float4 handles:
    D3DXHANDLE rand_frame ;
    D3DXHANDLE rand_preset;
    D3DXHANDLE const_handles[24];
    D3DXHANDLE q_const_handles[(NUM_Q_VAR+3)/4];
    D3DXHANDLE rot_mat[24];

    typedef std::vector<TexSizeParamInfo> TexSizeParamInfoList;
    TexSizeParamInfoList texsize_params;

    // sampler stages for various PS texture bindings:
    //int texbind_vs;
    //int texbind_disk[32];
    //int texbind_voronoi;
    //...
    SamplerInfo   m_texture_bindings[16];  // an entry for each sampler slot.  These are ALIASES - DO NOT DELETE.
    tex_code      m_texcode[16];  // if ==TEX_VS, forget the pointer - texture bound @ that stage is the double-buffered VS.

    void Clear();
    void CacheParams(LPD3DXCONSTANTTABLE pCT, bool bHardErrors);
    void OnTextureEvict(LPDIRECT3DBASETEXTURE9 texptr);
    CShaderParams();
    ~CShaderParams();
};

class VShaderInfo
{
public:
    IDirect3DVertexShader9* ptr;
    LPD3DXCONSTANTTABLE     CT;
    CShaderParams           params;
    VShaderInfo()  { ptr=NULL; CT=NULL; params.Clear(); }
    ~VShaderInfo() { Clear(); }
    void Clear();
};

class PShaderInfo
{
public:
    IDirect3DPixelShader9*  ptr;
    LPD3DXCONSTANTTABLE     CT;
    CShaderParams           params;
    PShaderInfo()  { ptr=NULL; CT=NULL; params.Clear(); }
    ~PShaderInfo() { Clear(); }
    void Clear();
};

typedef struct
{
    VShaderInfo vs;
    PShaderInfo ps;
} ShaderPairInfo;

typedef struct
{
    PShaderInfo warp;
    PShaderInfo comp;
} PShaderSet;

typedef struct
{
    VShaderInfo warp;
    VShaderInfo comp;
} VShaderSet;

typedef struct
{
    std::wstring  szFilename;    // without path
    float    fRatingThis;
    float    fRatingCum;
} PresetInfo;
typedef std::vector<PresetInfo> PresetList;


class CPlugin : public CPluginShell
{
public:

    //====[ 1. members added to create this specific example plugin: ]================================================

        /// SETTINGS (milk2.ini)
        bool		m_bFirstRun;
        float		m_fBlendTimeAuto;		// blend time when preset auto-switches
        float		m_fBlendTimeUser;		// blend time when user loads a new preset
        float		m_fTimeBetweenPresets;		// <- this is in addition to m_fBlendTimeAuto
        float		m_fTimeBetweenPresetsRand;	// <- this is in addition to m_fTimeBetweenPresets
        bool        m_bSequentialPresetOrder;
        bool		m_bHardCutsDisabled;
        float		m_fHardCutLoudnessThresh;
        float		m_fHardCutHalflife;
        float		m_fHardCutThresh;
        int         m_nCanvasStretch;   // 0=Auto, 100=None, 125 = 1.25X, 133, 150, 167, 200, 300, 400 (4X).
        int			m_nTexSizeX;			// -1 = exact match to screen; -2 = nearest power of 2.
        int			m_nTexSizeY;
        float       m_fAspectX;
        float       m_fAspectY;
        float       m_fInvAspectX;
        float       m_fInvAspectY;
		int         m_nTexBitsPerCh;
        int			m_nGridX;
        int			m_nGridY;

        bool		m_bShowPressF1ForHelp;
        bool		m_bShowMenuToolTips;
        int			m_n16BitGamma;
        bool		m_bAutoGamma;
        bool		m_bEnableRating;
        bool		m_bSongTitleAnims;
        bool		m_bNowPlaying;		// SURI: read the song from Windows media controls (ini: bNowPlaying)
        SongInfoBox	m_songBox;		// SURI: filled by m_nowPlaying's thread, read by GetSongTitle
        CNowPlaying	m_nowPlaying;
        float		m_fSongTitleAnimDuration;
        float		m_fSongTitleHold;		// SURI: now-playing title stays on screen this long (ini: fSongTitleHold)
        float		m_fSongTitleFade;		// SURI: ...then fades out over this long (ini: fSongTitleFade)
        float		m_fTimeBetweenRandomSongTitles;
        float		m_fTimeBetweenRandomCustomMsgs;
        int			m_nSongTitlesSpawned;
        int			m_nCustMsgsSpawned;

        bool		m_bWarningsDisabled2;		// warnings/errors in upper-right corner (m_szUserMessage)
        bool        m_bPresetLockOnAtStartup;
		bool		m_bPreventScollLockHandling;
        int         m_nMaxPSVersion_ConfigPanel;  // -1 = auto, 0 = disable shaders, 2 = ps_2_0, 3 = ps_3_0
        int         m_nMaxPSVersion_DX9;          // 0 = no shader support, 2 = ps_2_0, 3 = ps_3_0
        int         m_nMaxPSVersion;              // this one will be the ~min of the other two.  0/2/3.
        int         m_nMaxImages;
        int         m_nMaxBytes;

        HFONT       m_gdi_title_font_doublesize;
        LPD3DXFONT  m_d3dx_title_font_doublesize;
        int         m_nTitleFontPx;		// SURI: pixel height m_d3dx_title_font_doublesize was made with

        // PIXEL SHADERS
        DWORD                   m_dwShaderFlags;       // Shader compilation/linking flags
        LPD3DXBUFFER            m_pShaderCompileErrors;
        VShaderSet              m_fallbackShaders_vs;  // *these are the only vertex shaders used for the whole app.*
        PShaderSet              m_fallbackShaders_ps;  // these are just used when the preset's pixel shaders fail to compile.
        PShaderSet              m_shaders;     // includes shader pointers and constant tables for warp & comp shaders, for cur. preset
        PShaderSet              m_OldShaders;  // includes shader pointers and constant tables for warp & comp shaders, for prev. preset
        PShaderSet              m_NewShaders;  // includes shader pointers and constant tables for warp & comp shaders, for coming preset
        ShaderPairInfo          m_BlurShaders[2];
        bool                    m_bWarpShaderLock;
        bool                    m_bCompShaderLock;
        //bool LoadShaderFromFile( char* szFile, char* szFn, char* szProfile,
        //                         LPD3DXCONSTANTTABLE* ppConstTable, void** ppShader );
        #define SHADER_WARP  0
        #define SHADER_COMP  1
        #define SHADER_BLUR  2
        #define SHADER_OTHER 3
        bool LoadShaderFromMemory( const char* szShaderText, char* szFn, char* szProfile,
                                   LPD3DXCONSTANTTABLE* ppConstTable, void** ppShader, int shaderType, bool bHardErrors );
        bool RecompileVShader(const char* szShadersText, VShaderInfo *si, int shaderType, bool bHardErrors);
        bool RecompilePShader(const char* szShadersText, PShaderInfo *si, int shaderType, bool bHardErrors, int PSVersion);
        bool EvictSomeTexture();
        typedef std::vector<TexInfo> TexInfoList;
        TexInfoList     m_textures;
        bool m_bNeedRescanTexturesDir;
        // vertex declarations:
        IDirect3DVertexDeclaration9* m_pSpriteVertDecl;
        IDirect3DVertexDeclaration9* m_pWfVertDecl;
        IDirect3DVertexDeclaration9* m_pMyVertDecl;

        D3DXVECTOR4 m_rand_frame;  // 4 random floats (0..1); randomized once per frame; fed to pixel shaders.

        // RUNTIME SETTINGS THAT WE'VE ADDED
        float       m_prev_time;
        bool		m_bTexSizeWasAutoPow2;
		bool        m_bTexSizeWasAutoExact;
        bool		m_bPresetLockedByUser;
        bool		m_bPresetLockedByCode;
        float		m_fAnimTime;
        float		m_fStartTime;
        float		m_fPresetStartTime;
        float		m_fNextPresetTime;
        float       m_fSnapPoint;
        CState		*m_pState;				// points to current CState
        CState		*m_pOldState;			// points to previous CState
        CState		*m_pNewState;			// points to the coming CState - we're not yet blending to it b/c we're still compiling the shaders for it!
        int         m_nLoadingPreset;
        wchar_t     m_szLoadingPreset[MAX_PATH];
        float       m_fLoadingPresetBlendTime;
        int         m_nPresetsLoadedTotal; //important for texture eviction age-tracking...
        CState		m_state_DO_NOT_USE[3];	// do not use; use pState and pOldState instead.
        ui_mode		m_UI_mode;				// can be UI_REGULAR, UI_LOAD, UI_SAVEHOW, or UI_SAVEAS

        #define MASH_SLOTS 5
        #define MASH_APPLY_DELAY_FRAMES 1
        int         m_nMashSlot;    //0..MASH_SLOTS-1
        int         m_nMashPreset[MASH_SLOTS];
        int         m_nLastMashChangeFrame[MASH_SLOTS];

        bool		m_bUserPagedUp;
        bool		m_bUserPagedDown;
        float		m_fMotionVectorsTempDx;
        float		m_fMotionVectorsTempDy;

        td_waitstr  m_waitstring;
        void		WaitString_NukeSelection();
        void		WaitString_Cut();
        void		WaitString_Copy();
        void		WaitString_Paste();
        void		WaitString_SeekLeftWord();
        void		WaitString_SeekRightWord();
        int			WaitString_GetCursorColumn();
        int			WaitString_GetLineLength();
        void		WaitString_SeekUpOneLine();
        void		WaitString_SeekDownOneLine();

        int			m_nPresets;			// the # of entries in the file listing.  Includes directories and then files, sorted alphabetically.
        int			m_nDirs;			// the # of presets that are actually directories.  Always between 0 and m_nPresets.
        int			m_nPresetListCurPos;// Index of the currently-HIGHLIGHTED preset (the user must press Enter on it to select it).
        int			m_nCurrentPreset;	// Index of the currently-RUNNING preset.
								        //   Note that this is NOT the same as the currently-highlighted preset! (that's m_nPresetListCurPos)
								        //   Be careful - this can be -1 if the user changed dir. & a new preset hasn't been loaded yet.
        wchar_t		m_szCurrentPresetFile[512];	// w/o path.  this is always valid (unless no presets were found)
        PresetList  m_presets;
	    void		UpdatePresetList(bool bBackground=false, bool bForce=false, bool bTryReselectCurrentPreset=true);
        wchar_t     m_szUpdatePresetMask[MAX_PATH];
        bool        m_bPresetListReady;
	    //void		UpdatePresetRatings();
        //int         m_nRatingReadProgress;  // equals 'm_nPresets' if all ratings are read in & ready to go; -1 if uninitialized; otherwise, it's still reading them in, and range is: [0 .. m_nPresets-1]
        bool        m_bInitialPresetSelected;

        // PRESET HISTORY
        #define PRESET_HIST_LEN (64+2)     // make this 2 more than the # you REALLY want to be able to go back.
        std::wstring     m_presetHistory[PRESET_HIST_LEN];   //circular
        int         m_presetHistoryPos;
        int         m_presetHistoryBackFence;
        int         m_presetHistoryFwdFence;
        // SURI: the session playlist (phase 10). IsAll() = the whole library, as before.
        Playlist    m_playlist;
        void        SetPlaylist(const std::vector<std::wstring>& names);   // empty = all; always resets the history
        void        ResetPresetHistory();
        const std::vector<int>& PlaylistIndices();   // m_presets indices the playlist allows (cached)
        void        OpenPlaylistScreen();
        // SURI: the on-screen playlist screen (P): two columns, search, save / load (.m3u)
        PlaylistScreen m_plScreen;
        std::vector<std::wstring> m_plLib;           // the library m_plScreen was opened with
        int         m_plPageRows;                    // list rows drawn last frame (PgUp / PgDn step)
        size_t      m_plTop[2];                      // first row shown in each column (PlCol order)
        void        ClosePlaylistScreen();
        void        OnPlaylistAction(PlAction a);
        void        SavePlaylistDialog();
        void        LoadPlaylistDialog();
        void        DrawPlaylistScreen();
        std::vector<std::wstring> LibraryFileNames();   // m_presets file names (no dirs), list order
        // SURI: drag & drop onto the window (phase 10): folders / .milk files; new presets go
        // through the "new presets" prompt (UI_IMPORT_NEW) before they are copied to the library
        void        OnDropFiles(HDROP h);            // calls DragFinish
        ImportPrompt m_importPrompt;
        bool        m_importSingle;                  // one .milk file was dropped: load it, playlist unchanged
        std::vector<ImportItem> m_importExisting;    // dropped presets already in the library
        int         m_importPageRows;                // prompt rows drawn last frame (PgUp / PgDn step)
        size_t      m_importTop;                     // first prompt row shown
        void        FinishImport(PromptResult r);    // copy the checked ones (Accept), then apply the drop
        void        CopyPresetTextures(const std::wstring& presetPath, int* copied, int* kept, int* failed);
        void        LoadLibraryPreset(int index);    // m_presets[index] with m_fBlendTimeUser
        void        DrawImportPrompt();
        bool        (*m_pfnIsFullscreen)();          // set by the window code; NULL = never full screen
        bool        (*m_pfnRunCommand)(HWND, Cmd);   // set by the window code: runs window commands too (Cmd::ViewFullscreen)
        std::vector<int> m_playlistIdx;              // cache for PlaylistIndices()
        int         m_playlistIdxPresets;
        int         m_playlistIdxDirs;
        unsigned    m_playlistIdxGen;
        void        PrevPreset(float fBlendTime);
        void        NextPreset(float fBlendTime);  // if not retracing our former steps, it will choose a random one.
        void        OnFinishedLoadingPreset();

        FFT            myfft;
        td_mysounddata mysound;

        // stuff for displaying text to user:
        bool		m_bShowFPS;
        bool		m_bShowRating;
        bool		m_bShowPresetInfo;
        bool		m_bShowDebugInfo;
        bool		m_bShowSongTitle;
        bool		m_bShowSongTime;
        bool		m_bShowSongLen;
        float		m_fShowRatingUntilThisTime;

        #define ERR_ALL    0
        #define ERR_INIT   1  //specifically, loading a preset
        #define ERR_PRESET 2  //specifically, loading a preset
        #define ERR_MISC   3
        #define ERR_NOTIFY 4  // a simple notification - not an error at all. ("shuffle is now ON." etc.)
                              // NOTE: each NOTIFY msg clears all the old NOTIFY messages!
        #define ERR_SCANNING_PRESETS 5
        #define ERR_PRESET_DIR 6      // SURI: "no preset files or directories found"; cleared when a retry finds presets
        ToastQueue  m_toasts;   // SURI: AddError shows its messages as toasts (category = toast tag)
        int         m_ui_band_left, m_ui_band_right;   // SURI: while > 0, MyMenuTextOut puts a band under the selected row
        void        AddError(wchar_t* szMsg, float fDuration, int category=ERR_ALL, bool bBold=true);
        void        ClearErrors(int category=ERR_ALL);  // 0=all categories
        void        DrawInfoCard(RECT* pCard, int* upper_right_corner_y, int xL, int xR);   // SURI
        void        MyRenderOverlay();   // SURI: draws the toasts, above all other text
        void        OnFontsReallocated(); // SURI: DPI change: rebuild the song-title fonts
        bool        CreateTitleFonts();   // SURI: m_gdi/m_d3dx_title_font_doublesize from SONGTITLE_FONT
        int         m_toast_top, m_toast_xL, m_toast_xR;   // SURI: toast layout, set by MyRenderUI each frame
        RECT        m_toast_avoid;                        // SURI: the info card (toasts go below it if they would overlap)


        void GetSongTitle(wchar_t *szSongTitle, int nSize);

        char		m_szDebugMessage[512];
        wchar_t		m_szSongTitle    [512];
        wchar_t		m_szSongTitlePrev[512];

        // stuff for menu system:
        CMilkMenu	*m_pCurMenu;	// should always be valid!
        CMilkMenu	 m_menuPreset;
        CMilkMenu	  m_menuWave;
        CMilkMenu	  m_menuAugment;
        CMilkMenu	  m_menuCustomWave;
        CMilkMenu	  m_menuCustomShape;
        CMilkMenu	  m_menuMotion;
        CMilkMenu	  m_menuPost;
        CMilkMenu    m_menuWavecode[MAX_CUSTOM_WAVES];
        CMilkMenu    m_menuShapecode[MAX_CUSTOM_SHAPES];
        bool         m_bShowShaderHelp;

        wchar_t		m_szMilkdrop2Path[MAX_PATH];		// ends in a backslash
        wchar_t		m_szMsgIniFile[MAX_PATH];
        wchar_t     m_szImgIniFile[MAX_PATH];
        wchar_t		m_szPresetDir[MAX_PATH];
        float		m_fRandStart[4];

        // DIRECTX 9:
        IDirect3DTexture9 *m_lpVS[2];
        #define NUM_BLUR_TEX 6
        #if (NUM_BLUR_TEX>0)
	    IDirect3DTexture9 *m_lpBlur[NUM_BLUR_TEX]; // each is successively 1/2 size of prev.
        int               m_nBlurTexW[NUM_BLUR_TEX];
        int               m_nBlurTexH[NUM_BLUR_TEX];
        #endif
        int m_nHighestBlurTexUsedThisFrame;
        IDirect3DTexture9 *m_lpDDSTitle;    // CAREFUL: MIGHT BE NULL (if not enough mem)!
        int               m_nTitleTexSizeX, m_nTitleTexSizeY;
        UINT              m_adapterId;
        MYVERTEX          *m_verts;
        MYVERTEX          *m_verts_temp;
        td_vertinfo       *m_vertinfo;
        int               *m_indices_strip;
        int               *m_indices_list;

        // for final composite grid:
        #define FCGSX 32 // final composite gridsize - # verts - should be EVEN.
        #define FCGSY 24 // final composite gridsize - # verts - should be EVEN.
                         // # of grid *cells* is two less,
                         // since we have redundant verts along the center line in X and Y (...for clean 'ang' interp)
        MYVERTEX    m_comp_verts[FCGSX*FCGSY];
        int         m_comp_indices[(FCGSX-2)*(FCGSY-2)*2*3];

        bool		m_bMMX;
        bool        m_bHasFocus;
        bool        m_bHadFocus;
        bool		m_bOrigScrollLockState;
        //bool      m_bMilkdropScrollLockState;  // saved when focus is lost; restored when focus is regained

        int         m_nNumericInputMode;	// NUMERIC_INPUT_MODE_CUST_MSG, NUMERIC_INPUT_MODE_SPRITE
        int         m_nNumericInputNum;
        int			m_nNumericInputDigits;
        td_custom_msg_font   m_CustomMessageFont[MAX_CUSTOM_MESSAGE_FONTS];
        td_custom_msg        m_CustomMessage[MAX_CUSTOM_MESSAGES];

        texmgr      m_texmgr;		// for user sprites
        float       m_spriteQuad[NUM_TEX][4][2];	// SURI: last drawn corners per slot (pre-projection space whose y grows downward, no burn-in), for hit tests
        int         m_nSelectedSprite;	// SURI: slot of the selected sprite, -1 = none
        bool        m_bSpriteDrag;		// SURI: left button dragging the selected sprite
        POINT       m_ptSpriteDrag;		// SURI: last mouse position of the drag (client coords)
        struct SpriteSnapshot {			// SURI: a live sprite kept across a device reset (CleanUpMyDX9Stuff -> AllocateMyDX9Stuff)
            SpriteDesc desc;			// where it is now (user offsets / scale / blend folded in)
            unsigned   ck;				// colour key it was loaded with (may be TEXMGR_NO_COLORKEY)
            int        slot;			// same slot again, so the draw order does not change
            bool       owned;			// bOwnedByPreset
            int        nStartFrame;
            float      fStartTime;
        };
        SpriteSnapshot m_spriteSnap[NUM_TEX];
        int         m_nSpriteSnap;		// SURI: entries in m_spriteSnap waiting to be relaunched
        int         m_nSpriteSnapSel;	// SURI: index in m_spriteSnap of the selected sprite, -1 = none

        td_supertext m_supertext;	// **contains info about current Song Title or Custom Message.**

        IDirect3DTexture9 *m_tracer_tex;

        int         m_nFramesSinceResize;

        char        m_szShaderIncludeText[32768];     // note: this still has char 13's and 10's in it - it's never edited on screen or loaded/saved with a preset.
        int         m_nShaderIncludeTextLen;          //  # of chars, not including the final NULL.
        char        m_szDefaultWarpVShaderText[32768]; // THIS HAS CHAR 13/10 CONVERTED TO LINEFEED_CONTROL_CHAR
        char        m_szDefaultWarpPShaderText[32768]; // THIS HAS CHAR 13/10 CONVERTED TO LINEFEED_CONTROL_CHAR
        char        m_szDefaultCompVShaderText[32768]; // THIS HAS CHAR 13/10 CONVERTED TO LINEFEED_CONTROL_CHAR
        char        m_szDefaultCompPShaderText[32768]; // THIS HAS CHAR 13/10 CONVERTED TO LINEFEED_CONTROL_CHAR
        char        m_szBlurVS[32768];
        char        m_szBlurPSX[32768];
        char        m_szBlurPSY[32768];
        void        GenWarpPShaderText(char *szShaderText, float decay, bool bWrap);
        void        GenCompPShaderText(char *szShaderText, float brightness, float ve_alpha, float ve_zoom, int ve_orient, float hue_shader, bool bBrighten, bool bDarken, bool bSolarize, bool bInvert);

   //====[ 2. methods added: ]=====================================================================================

        void RenderFrame(int bRedraw);

        void        DrawTooltip(wchar_t* str, int xR, int yB);
        void        RandomizeBlendPattern();
        void        GenPlasma(int x0, int x1, int y0, int y1, float dt);
        void        LoadPerFrameEvallibVars(CState* pState);
        void        LoadCustomWavePerFrameEvallibVars(CState* pState, int i);
        void        LoadCustomShapePerFrameEvallibVars(CState* pState, int i, int instance);
    	void		WriteRealtimeConfig();	// called on Finish()
	    void		dumpmsg(wchar_t *s);
	    void		Randomize();
	    void		LoadRandomPreset(float fBlendTime);
	    void		LoadPreset(const wchar_t *szPresetFilename, float fBlendTime);
        void        LoadPresetTick();
        void        FindValidPresetDir();
	    wchar_t*	GetMsgIniFile()    { return m_szMsgIniFile; };
	    wchar_t*    GetPresetDir()     { return m_szPresetDir; };
	    void		SavePresetAs(wchar_t *szNewFile);		// overwrites the file if it was already there.
	    void		DeletePresetFile(wchar_t *szDelFile);
	    void		RenamePresetFile(wchar_t *szOldFile, wchar_t *szNewFile);
	    void		SetCurrentPresetRating(float fNewRating);
	    void		SeekToPreset(wchar_t cStartChar);
	    bool		ReversePropagatePoint(float fx, float fy, float *fx2, float *fy2);
	    int 		HandleRegularKey(WPARAM wParam);
	    bool		DispatchCommand(Cmd c, HWND hWnd);	// SURI: true if handled; false -> caller continues the old key path
	    AppSnapshot	GetAppSnapshot() const;			// SURI: menu state (fullscreen/stretch are filled in by the window code)
	    FxState		m_fx;							// SURI: live shader effects, injected into the comp shader text on every preset load
	    void		ApplyFxToState(CState* pState);	// SURI: upgrades comp to ps_2_0 if needed, then injects m_fx into pState's comp text
	    void		RefreshFx();					// SURI: re-injects into the current preset and recompiles its comp shader (and a preset still loading)
	    void		RecompileCompShader(CState* pState, PShaderInfo* si);	// SURI: compiles pState's comp text into si, fallback shader on failure
	    void		CloseOverlays();				// SURI: F1 help, menus / browser and any text entry off (entering desktop mode)
	    bool		OnResizeGraphicsWindow();
	    bool		OnResizeTextWindow();
	    void		ClearGraphicsWindow();	// for windowed mode only
	    void		LaunchCustomMessage(int nMsgNum);
	    void		ReadCustomMessages();
	    void		LaunchSongTitleAnim(bool bNowPlaying = false);	// SURI: bNowPlaying = hold + fade instead of burn-in
	    float		SuperTextEndTime() const;

	    bool		RenderStringToTitleTexture();
	    void		ShowSongTitleAnim(/*IDirect3DTexture9* lpRenderTarget,*/ int w, int h, float fProgress, float fAlpha = 1.0f);
	    void		DrawWave(float *fL, float *fR);
        void        DrawCustomWaves();
        void        DrawCustomShapes();
	    void		DrawSprites();
        void        ComputeGridAlphaValues();
        //void        WarpedBlit();
                     // note: 'bFlipAlpha' just flips the alpha blending in fixed-fn pipeline - not the values for culling tiles.
	    void		 WarpedBlit_Shaders  (int nPass, bool bAlphaBlend, bool bFlipAlpha, bool bCullTiles, bool bFlipCulling);
        void		 WarpedBlit_NoShaders(int nPass, bool bAlphaBlend, bool bFlipAlpha, bool bCullTiles, bool bFlipCulling);
	    void		 ShowToUser_Shaders  (int nPass, bool bAlphaBlend, bool bFlipAlpha, bool bCullTiles, bool bFlipCulling);
	    void		 ShowToUser_NoShaders();
        void        BlurPasses();
        void        GetSafeBlurMinMax(CState* pState, float* blur_min, float* blur_max);
	    void		RunPerFrameEquations(int code);
	    void		DrawUserSprites();
	    void		MergeSortPresets(int left, int right);
	    void		BuildMenus();
        void        SetMenusForPresetVersion(int WarpPSVersion, int CompPSVersion);
	    bool		LaunchSprite(int nSpriteNum, int nSlot);
	    int 		FindSpriteSlot();	// SURI: free slot, or the oldest one (killed)
	    bool		LaunchStaticSprite(const SpriteDesc& d, bool ownedByPreset);	// SURI: false + AddError on failure
	    bool		LaunchStaticSpriteEx(const SpriteDesc& d, bool ownedByPreset, unsigned ck, int nSlot);	// SURI: explicit colour key; nSlot -1 = FindSpriteSlot
	    void		SnapshotSprites();		// SURI: before m_texmgr.Finish on a routine cleanup
	    void		RelaunchSnapshotSprites();	// SURI: after m_texmgr.Init
	    void		KillSprite(int iSlot);
	    void		LaunchPresetSprites(const CState* pState);	// SURI: drop the old preset's sprites, launch pState's
	    int 		CollectLiveSprites(SpriteDesc* out, int maxOut, int* slotsOut = NULL);	// SURI: live sprites as preset entries, oldest first; slotsOut[i] = slot of out[i]
	    int 		SpriteAtClientPoint(HWND hWnd, int x, int y);	// SURI: topmost live sprite slot under the point, -1 = none
	    bool		SpriteMouseMessage(HWND hWnd, unsigned uMsg, WPARAM wParam, LPARAM lParam);	// SURI: drag / wheel on sprites; true = handled
        void        DoCustomSoundAnalysis();
        void        DrawMotionVectors();

        bool        LoadShaders(PShaderSet* sh, CState* pState, bool bTick);
        void        UvToMathSpace(float u, float v, float* rad, float* ang);
        void        ApplyShaderParams(CShaderParams* p, LPD3DXCONSTANTTABLE pCT, CState* pState);
        void        RestoreShaderParams();
        bool        AddNoiseTex(const wchar_t* szTexName, int size, int zoom_factor);
        bool        AddNoiseVol(const wchar_t* szTexName, int size, int zoom_factor);


    //====[ 3. virtual functions: ]===========================================================================

        virtual void OverrideDefaults();
        virtual void MyPreInitialize();
        virtual void MyReadConfig();
        virtual void MyWriteConfig();
        virtual int  AllocateMyNonDx9Stuff();
        virtual void  CleanUpMyNonDx9Stuff();
        virtual int  AllocateMyDX9Stuff();
        virtual void  CleanUpMyDX9Stuff(int final_cleanup);
        virtual void MyRenderFn(int redraw);
        virtual void MyRenderUI(int *upper_left_corner_y, int *upper_right_corner_y, int *lower_left_corner_y, int *lower_right_corner_y, int xL, int xR);
        virtual LRESULT MyWindowProc(HWND hWnd, unsigned uMsg, WPARAM wParam, LPARAM lParam);
};

// SURI: a WM_KEYDOWN key (with Ctrl / Shift from GetKeyState) as a key of the on-screen lists;
// false = not one of theirs
bool TranslateUiKey(WPARAM vk, UiKey* out);

#endif
