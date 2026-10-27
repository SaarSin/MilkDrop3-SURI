// image_load_win32.cpp
//
// SURI: GDI+ based GIF -> PNG (in memory) conversion for sprites. See image_load_win32.h.

#include "image_load_win32.h"

#include <objidl.h>
#include <algorithm>

// gdiplus.h uses unqualified min/max. windows.h provides them as macros unless NOMINMAX is
// defined somewhere earlier; make sure both spellings resolve either way.
#ifndef min
using std::min;
#endif
#ifndef max
using std::max;
#endif
#include <gdiplus.h>

static ULONG_PTR g_gdiplusToken = 0;
static int g_gdiplusRefs = 0;

bool GdiplusStartupOnce()
{
    if (g_gdiplusRefs > 0) {
        g_gdiplusRefs++;
        return true;
    }
    Gdiplus::GdiplusStartupInput input;
    if (Gdiplus::GdiplusStartup(&g_gdiplusToken, &input, NULL) != Gdiplus::Ok) {
        g_gdiplusToken = 0;
        return false;
    }
    g_gdiplusRefs = 1;
    return true;
}

void GdiplusShutdownOnce()
{
    if (g_gdiplusRefs <= 0)
        return;
    if (--g_gdiplusRefs == 0) {
        Gdiplus::GdiplusShutdown(g_gdiplusToken);
        g_gdiplusToken = 0;
    }
}

static bool FindEncoderClsid(const wchar_t* mimeType, CLSID* clsid)
{
    UINT num = 0, size = 0;
    if (Gdiplus::GetImageEncodersSize(&num, &size) != Gdiplus::Ok || size == 0)
        return false;
    std::vector<unsigned char> buf(size);
    Gdiplus::ImageCodecInfo* codecs = reinterpret_cast<Gdiplus::ImageCodecInfo*>(&buf[0]);
    if (Gdiplus::GetImageEncoders(num, size, codecs) != Gdiplus::Ok)
        return false;
    for (UINT i = 0; i < num; i++) {
        if (codecs[i].MimeType && wcscmp(codecs[i].MimeType, mimeType) == 0) {
            *clsid = codecs[i].Clsid;
            return true;
        }
    }
    return false;
}

HRESULT LoadGifAsPngInMemory(const wchar_t* path, std::vector<unsigned char>* png)
{
    if (!path || !*path || !png)
        return E_INVALIDARG;
    png->clear();
    if (g_gdiplusRefs <= 0)
        return E_FAIL;

    CLSID pngClsid;
    if (!FindEncoderClsid(L"image/png", &pngClsid))
        return E_FAIL;

    HRESULT result = E_FAIL;
    {
        // Bitmap must be destroyed before the stream is released.
        Gdiplus::Bitmap bmp(path, FALSE);
        if (bmp.GetLastStatus() != Gdiplus::Ok)
            return E_FAIL;
        // Image::Save writes the active frame, which is the first frame after loading.

        IStream* stream = NULL;
        if (FAILED(CreateStreamOnHGlobal(NULL, TRUE, &stream)) || !stream)
            return E_OUTOFMEMORY;

        if (bmp.Save(stream, &pngClsid, NULL) == Gdiplus::Ok) {
            HGLOBAL hg = NULL;
            STATSTG st;
            ZeroMemory(&st, sizeof(st));
            if (SUCCEEDED(GetHGlobalFromStream(stream, &hg)) &&
                SUCCEEDED(stream->Stat(&st, STATFLAG_NONAME)) &&
                st.cbSize.HighPart == 0 && st.cbSize.LowPart > 0) {
                const void* data = GlobalLock(hg);
                if (data) {
                    const unsigned char* bytes = static_cast<const unsigned char*>(data);
                    png->assign(bytes, bytes + st.cbSize.LowPart);
                    GlobalUnlock(hg);
                    result = S_OK;
                }
            }
        }
        stream->Release();
    }
    return result;
}
