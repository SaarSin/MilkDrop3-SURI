// sprite_layout.cpp
//
// SURI: see sprite_layout.h. C++14, no dependencies.

#include "sprite_layout.h"
#include <math.h>
#include <wchar.h>

void SpriteCorners(const SpriteXform& t, float imgAspectHW, float screenAspectWH, float out[4][2])
{
    const int fx = t.flipx ? 1 : 0;
    const int fy = t.flipy ? 1 : 0;
    float vx[4] = { 0, 0, 0, 0 };
    float vy[4] = { 0, 0, 0, 0 };

    vx[0 + fx] = -t.sx;
    vx[1 - fx] =  t.sx;
    vx[2 + fx] = -t.sx;
    vx[3 - fx] =  t.sx;
    vy[0 + fy * 2] = -t.sy;
    vy[1 + fy * 2] = -t.sy;
    vy[2 - fy * 2] =  t.sy;
    vy[3 - fy * 2] =  t.sy;

    // first aspect ratio: non-1:1 images
    for (int k = 0; k < 4; k++) {
        if (imgAspectHW < 1.0f) vy[k] *= imgAspectHW;   // wide image
        else                    vx[k] /= imgAspectHW;   // tall image
    }

    const float c = cosf(t.rot);
    const float s = sinf(t.rot);
    for (int k = 0; k < 4; k++) {
        float x2 = vx[k] * c - vy[k] * s;
        float y2 = vx[k] * s + vy[k] * c;
        vx[k] = x2 + t.x;
        vy[k] = y2 + t.y;
    }

    // second aspect ratio: normalize to the screen width
    for (int k = 0; k < 4; k++) {
        if (screenAspectWH > 1.0f) vy[k] *= screenAspectWH;
        else                       vx[k] /= screenAspectWH;
    }

    for (int k = 0; k < 4; k++) {
        out[k][0] = vx[k];
        out[k][1] = vy[k];
    }
}

bool PointInSpriteQuad(const float q[4][2], float px, float py)
{
    static const int order[4] = { 0, 1, 3, 2 };
    bool pos = false, neg = false;
    // Degenerate (zero area) quads never hit.
    float area2 = 0.0f;
    for (int i = 0; i < 4; i++) {
        const float* a = q[order[i]];
        const float* b = q[order[(i + 1) % 4]];
        area2 += a[0] * b[1] - b[0] * a[1];
    }
    if (fabsf(area2) < 2e-8f)
        return false;
    for (int i = 0; i < 4; i++) {
        const float* a = q[order[i]];
        const float* b = q[order[(i + 1) % 4]];
        float cross = (b[0] - a[0]) * (py - a[1]) - (b[1] - a[1]) * (px - a[0]);
        if (cross > 0.0f) pos = true;
        else if (cross < 0.0f) neg = true;
    }
    return !(pos && neg);
}

int TopmostSpriteAt(const SpriteHit* items, int n, float px, float py)
{
    int best = -1;
    for (int i = 0; i < n; i++) {
        if (!items[i].live || !PointInSpriteQuad(items[i].q, px, py))
            continue;
        best = i;   // later slots are drawn over earlier ones
    }
    return best;
}

void WindowToClip(int mx, int my, int clientW, int clientH, float* cx, float* cy)
{
    if (clientW < 1) clientW = 1;
    if (clientH < 1) clientH = 1;
    if (cx) *cx = 2.0f * (float)mx / (float)clientW - 1.0f;
    if (cy) *cy = 1.0f - 2.0f * (float)my / (float)clientH;
}

void ClipDeltaToSpriteDelta(float dcx, float dcy, float screenAspectWH, float* dx, float* dy)
{
    if (screenAspectWH <= 0.0f) screenAspectWH = 1.0f;
    float x = dcx, y = dcy;
    if (screenAspectWH > 1.0f) y /= screenAspectWH;
    else                       x *= screenAspectWH;
    if (dx) *dx = x;
    if (dy) *dy = y;
}

float WheelScale(float scale, int wheelDelta)
{
    float r = scale * powf(1.1f, (float)wheelDelta / 120.0f);
    if (r < 0.05f) r = 0.05f;
    if (r > 10.0f) r = 10.0f;
    return r;
}

SpriteImageKind ImageKindFromPath(const wchar_t* path)
{
    if (!path) return SpriteImageKind::Unsupported;
    const wchar_t* dot = nullptr;
    for (const wchar_t* p = path; *p; ++p) {
        if (*p == L'.') dot = p;
        else if (*p == L'\\' || *p == L'/') dot = nullptr;
    }
    if (!dot) return SpriteImageKind::Unsupported;
    wchar_t ext[8];
    int n = 0;
    for (const wchar_t* p = dot + 1; *p; ++p) {
        if (n >= 7) return SpriteImageKind::Unsupported;
        wchar_t ch = *p;
        if (ch >= L'A' && ch <= L'Z') ch = (wchar_t)(ch - L'A' + L'a');
        ext[n++] = ch;
    }
    ext[n] = 0;
    if (wcscmp(ext, L"gif") == 0) return SpriteImageKind::Gif;
    static const wchar_t* const d3dx[] = { L"png", L"jpg", L"jpeg", L"bmp", L"tga", L"dds" };
    for (int i = 0; i < 6; i++)
        if (wcscmp(ext, d3dx[i]) == 0) return SpriteImageKind::D3dx;
    return SpriteImageKind::Unsupported;
}
