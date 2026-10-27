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

#include "utility.h"
#include <math.h>
#include <locale.h>
#include <windows.h>
#ifdef _DEBUG
    #define D3D_DEBUG_INFO  // declare this before including d3d9.h
#endif
#include <d3d9.h>
#include "resource.h"
#include "resstrings.h"

extern HINSTANCE api_orig_hinstance;

float AdjustRateToFPS(float per_frame_decay_rate_at_fps1, float fps1, float actual_fps)
{
    // returns the equivalent per-frame decay rate at actual_fps

    // basically, do all your testing at fps1 and get a good decay rate;
    // then, in the real application, adjust that rate by the actual fps each time you use it.

    float per_second_decay_rate_at_fps1 = powf(per_frame_decay_rate_at_fps1, fps1);
    float per_frame_decay_rate_at_fps2 = powf(per_second_decay_rate_at_fps1, 1.0f/actual_fps);

    return per_frame_decay_rate_at_fps2;
}

float GetPrivateProfileFloatW(wchar_t *szSectionName, wchar_t *szKeyName, float fDefault, wchar_t *szIniFile)
{
    wchar_t string[64];
    wchar_t szDefault[64];
    float ret = fDefault;

    _swprintf_l(szDefault, L"%f", g_use_C_locale, fDefault);

    if (GetPrivateProfileStringW(szSectionName, szKeyName, szDefault, string, 64, szIniFile) > 0)
    {
        _swscanf_l(string, L"%f", g_use_C_locale, &ret);
    }
    return ret;
}

bool WritePrivateProfileFloatW(float f, wchar_t *szKeyName, wchar_t *szIniFile, wchar_t *szSectionName)
{
    wchar_t szValue[32];
    _swprintf_l(szValue, L"%f", g_use_C_locale, f);
    return (WritePrivateProfileStringW(szSectionName, szKeyName, szValue, szIniFile) != 0);
}

bool WritePrivateProfileIntW(int d, wchar_t *szKeyName, wchar_t *szIniFile, wchar_t *szSectionName)
{
    wchar_t szValue[32];
    swprintf(szValue, L"%d", d);
    return (WritePrivateProfileStringW(szSectionName, szKeyName, szValue, szIniFile) != 0);
}

void RemoveExtension(wchar_t *str)
{
    wchar_t *p = wcsrchr(str, L'.');
    if (p) *p = 0;
}

void TextToGuid(char *str, GUID *pGUID)
{
    if (!str) return;
    if (!pGUID) return;

    DWORD d[11];

    sscanf(str, "%X %X %X %X %X %X %X %X %X %X %X",
        &d[0], &d[1], &d[2], &d[3], &d[4], &d[5], &d[6], &d[7], &d[8], &d[9], &d[10]);

    pGUID->Data1 = (DWORD)d[0];
    pGUID->Data2 = (WORD)d[1];
    pGUID->Data3 = (WORD)d[2];
    pGUID->Data4[0] = (BYTE)d[3];
    pGUID->Data4[1] = (BYTE)d[4];
    pGUID->Data4[2] = (BYTE)d[5];
    pGUID->Data4[3] = (BYTE)d[6];
    pGUID->Data4[4] = (BYTE)d[7];
    pGUID->Data4[5] = (BYTE)d[8];
    pGUID->Data4[6] = (BYTE)d[9];
    pGUID->Data4[7] = (BYTE)d[10];
}

void GuidToText(GUID *pGUID, char *str, int nStrLen)
{
    // note: nStrLen should be set to sizeof(str).
    if (!str) return;
    if (!nStrLen) return;
    str[0] = 0;
    if (!pGUID) return;

    DWORD d[11];
    d[0]  = (DWORD)pGUID->Data1;
    d[1]  = (DWORD)pGUID->Data2;
    d[2]  = (DWORD)pGUID->Data3;
    d[3]  = (DWORD)pGUID->Data4[0];
    d[4]  = (DWORD)pGUID->Data4[1];
    d[5]  = (DWORD)pGUID->Data4[2];
    d[6]  = (DWORD)pGUID->Data4[3];
    d[7]  = (DWORD)pGUID->Data4[4];
    d[8]  = (DWORD)pGUID->Data4[5];
    d[9]  = (DWORD)pGUID->Data4[6];
    d[10] = (DWORD)pGUID->Data4[7];

    sprintf(str, "%08X %04X %04X %02X %02X %02X %02X %02X %02X %02X %02X",
        d[0], d[1], d[2], d[3], d[4], d[5], d[6], d[7], d[8], d[9], d[10]);
}

bool CheckForMMX()
{
    DWORD bMMX = 0;
    DWORD *pbMMX = &bMMX;
    __try {
        __asm {
            mov eax, 1
            cpuid
            mov edi, pbMMX
            mov dword ptr [edi], edx
        }
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        bMMX = 0;
    }

    if (bMMX & 0x00800000)  // check bit 23
		return true;

	return false;
}

bool CheckForSSE()
{
#ifdef _WIN64
	return true; // All x64 processors support SSE
#else
    /*
    The SSE instruction set was introduced with the Pentium III and features:
        * Additional MMX instructions such as min/max
        * Prefetch and write-through instructions for optimizing data movement
            from and to the L2/L3 caches and main memory
        * 8 New 128 bit XMM registers (xmm0..xmm7) and corresponding 32 bit floating point
            (single precision) instructions
    */

	DWORD bSSE = 0;
	DWORD *pbSSE = &bSSE;
    __try {
	    __asm
	    {
		    mov eax, 1
		    cpuid
            mov edi, pbSSE
            mov dword ptr [edi], edx
	    }
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        bSSE = 0;
    }

	if (bSSE & 0x02000000)  // check bit 25
		return true;

	return false;
#endif
}

