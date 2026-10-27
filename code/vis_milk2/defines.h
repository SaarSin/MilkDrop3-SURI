/*
  LICENSE
  -------
Copyright 2005-2012 Nullsoft, Inc.
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

#ifndef __NULLSOFT_DX8_PLUGIN_SHELL_DEFINES_H__
#define __NULLSOFT_DX8_PLUGIN_SHELL_DEFINES_H__ 1

#define MILKDROP_VERSION_STR  "3.0.S"   // product version shown in the window title and exe properties
#define MILKDROP_VERSION_STRW L"3.0.S"

// INT_VERSION / INT_SUBVERSION are written to milk2.ini.  When MilkDrop reads an ini
// file written by an older version, it ignores the old settings and uses the defaults.
#define INT_VERSION     200
#define INT_SUBVERSION  5      //straight=0, a=1, b=2, ...

// SUBDIR is the folder (under <exe folder>\plugins\) that holds data, docs, presets and textures.
#define SUBDIR  L"Milkdrop2\\"

// settings, custom messages and sprites; these files live next to MilkDrop3.exe.
#define INIFILE         L"milk2.ini"
#define MSG_INIFILE     L"milk2_msg.ini"
#define IMG_INIFILE     L"milk2_img.ini"

// defaults for the 4 built-in fonts
#define SIMPLE_FONT_DEFAULT_FACE L"Segoe UI"   // SURI: was Courier
#define SIMPLE_FONT_DEFAULT_SIZE 24
#define SIMPLE_FONT_DEFAULT_BOLD 0
#define SIMPLE_FONT_DEFAULT_ITAL 0
#define SIMPLE_FONT_DEFAULT_AA   1   // SURI: was 0
#define DECORATIVE_FONT_DEFAULT_FACE L"Segoe UI"   // SURI: was Arial
#define DECORATIVE_FONT_DEFAULT_SIZE 24
#define DECORATIVE_FONT_DEFAULT_BOLD 0
#define DECORATIVE_FONT_DEFAULT_ITAL 0
#define DECORATIVE_FONT_DEFAULT_AA   1
#define HELPSCREEN_FONT_DEFAULT_FACE L"Segoe UI"   // SURI: was Courier
#define HELPSCREEN_FONT_DEFAULT_SIZE 24
#define HELPSCREEN_FONT_DEFAULT_BOLD 0
#define HELPSCREEN_FONT_DEFAULT_ITAL 0
#define HELPSCREEN_FONT_DEFAULT_AA   1   // SURI: was 0
#define PLAYLIST_FONT_DEFAULT_FACE L"Segoe UI"   // SURI: was Arial
#define PLAYLIST_FONT_DEFAULT_SIZE 24
#define PLAYLIST_FONT_DEFAULT_BOLD 0
#define PLAYLIST_FONT_DEFAULT_ITAL 0
#define PLAYLIST_FONT_DEFAULT_AA   1   // SURI: was 0

// extra fonts, UP TO A MAX OF 5.
// access the font by calling GetFont(EXTRA_1) for extra font #1,
// GetExtraFont(EXTRA_2) for extra font #2, and so on.
#define NUM_EXTRA_FONTS 2  // <- don't exceed 5 here!
#define TOOLTIP_FONT EXTRA_1
#define EXTRA_FONT_1_DEFAULT_FACE L"Segoe UI"   // SURI: was Arial
#define EXTRA_FONT_1_DEFAULT_SIZE 14
#define EXTRA_FONT_1_DEFAULT_BOLD 0
#define EXTRA_FONT_1_DEFAULT_ITAL 0
#define EXTRA_FONT_1_DEFAULT_AA   1   // SURI: was 0
#define SONGTITLE_FONT EXTRA_2
#define EXTRA_FONT_2_DEFAULT_FACE L"Times New Roman"
#define EXTRA_FONT_2_DEFAULT_SIZE 18
#define EXTRA_FONT_2_DEFAULT_BOLD 0
#define EXTRA_FONT_2_DEFAULT_ITAL 1
#define EXTRA_FONT_2_DEFAULT_AA   1

// Finally, a few parameters that will control how things are done
// inside the plugin shell:
#define NUM_WAVEFORM_SAMPLES         480   // RANGE: 32-576.  This is the # of samples of waveform data that you want.
                                           //   Note that if it is less than 576, then MilkDrop will do its best
                                           //   to line up the waveforms from frame to frame for you, using
                                           //   the extra samples as 'squish' space.
                                           // Note: the more 'slush' samples you leave, the better the alignment
                                           //   will be.  512 samples gives you decent alignment; 400 samples
                                           //   leaves room for fantastic alignment.
                                           // Observe that if you specify a value here (say 400) and then only
                                           //   render a sub-portion of that in some cases (say, 200 samples),
                                           //   make sure you render the *middle* 200 samples (#100-300), because
                                           //   the alignment happens *mostly at the center*.
#define NUM_FREQUENCIES              512   // # of freq. samples you want *out* of the FFT, for 0-11kHz range.
                                           //   ** this must be a power of 2!
                                           //   ** the actual FFT will use twice this many frequencies **

#define TEXT_MARGIN                  10    // the # of pixels of margin to leave between text and the edge of the screen
#define PLAYLIST_INNER_MARGIN        4     // the extra margin between the playlist box and the text inside

#define PLAYLIST_COLOR_PLAYING_TRACK 0xFFCCFF00 // alpha|red|green|blue
#define PLAYLIST_COLOR_HILITE_TRACK  0xFFFF5050
#define PLAYLIST_COLOR_BOTH          0xFFFFCC22
#define PLAYLIST_COLOR_NORMAL        0xFFCCCCCC

#define MENU_COLOR    0xFFCCCCCC
#define MENU_HILITE_COLOR 0xFFFF4400
#define DIR_COLOR     0xFF88CCFF

// SURI: on-screen "cards" (phase 6 design B). Fills are translucent; text on them is opaque.
#define CARD_FILL_INFO   0xB8141022   // F4/F5/F6 info card
#define CARD_FILL_TOAST  0xD1141022   // notifications
#define CARD_FILL_PANEL  0xDB141022   // help, M menu, L browser
#define CARD_BORDER      0x24FFFFFF
#define CARD_TEXT        0xFFFFFFFF
#define CARD_TEXT_2      0xFFC9C4DA   // secondary text
#define CARD_ACCENT      0xFFB9A7F0   // active tab fill
#define CARD_ACCENT_TEXT 0xFF1A1430   // text on CARD_ACCENT
#define CARD_BAND        0x40B9A7F0   // selected row in a list
#define CARD_KEYCAP      0x1AFFFFFF
#define CARD_STAR        0xFFF3C969
#define CARD_DOT_NOTIFY  0xFF8EE6A8
#define CARD_DOT_ERROR   0xFFFF8A7A

#define MAX_PRESETS_PER_PAGE 128

#define PRESS_F1_DUR                 3.0f // in seconds
#define PRESS_F1_EXP                 10.0f // exponent for how quickly it accelerates to leave the screen. 1 = linear; >1 = stays & then dashes off @ end

#endif
