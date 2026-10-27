// image_load_win32.h
//
// SURI: image loading helpers that need GDI+ (D3DX9 cannot read .gif files). The first frame
// of a GIF is re-encoded as PNG in memory, so D3DXCreateTextureFromFileInMemoryEx can load it
// with the same parameters (and the same colorkey handling) as any other sprite image.

#pragma once

#include <windows.h>
#include <vector>

// Starts GDI+ once per process; later calls only add a reference. Returns false on failure.
bool GdiplusStartupOnce();
// Releases one reference; GDI+ shuts down when the last reference goes away.
void GdiplusShutdownOnce();

// Loads the first frame of a GIF (or any GDI+ readable image) and stores it as PNG bytes in
// *png. Transparency is kept (PNG alpha). Returns S_OK, or an error HRESULT (E_FAIL when GDI+
// is not running or the file cannot be read).
HRESULT LoadGifAsPngInMemory(const wchar_t* path, std::vector<unsigned char>* png);
