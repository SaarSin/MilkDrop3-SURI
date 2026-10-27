// sprite_layout.h
//
// SURI: pure sprite geometry for the sprite editor: quad corners (same math as the sprite
// renderer in milkdropfs.cpp), hit testing, coordinate conversion and wheel scaling.
// Unit-tested in code/tests/test_sprite_layout.cpp.

#pragma once

struct SpriteXform {
    float x, y;       // already in clip space (var_x*2-1, var_y*2-1)
    float sx, sy;     // half extents before aspect correction
    float rot;        // radians
    bool flipx, flipy;
};

// Corners in the renderer's vertex order (triangle strip: 0,1,2,3), clip space, without the
// burn-in step. imgAspectHW = image height / width, screenAspectWH = screen width / height.
void SpriteCorners(const SpriteXform& t, float imgAspectHW, float screenAspectWH, float out[4][2]);

// Quad polygon order is 0,1,3,2 (strip order). Works for either winding.
bool PointInSpriteQuad(const float q[4][2], float px, float py);

struct SpriteHit {
    bool live;
    int startFrame;
    float q[4][2];
};

// Index of the live item containing the point that is drawn last, or -1. The renderer draws
// the slots in index order (DrawUserSprites), so the highest index is on top; startFrame does
// not matter here.
int TopmostSpriteAt(const SpriteHit* items, int n, float px, float py);

// cx = 2*mx/w - 1, cy = 1 - 2*my/h.
void WindowToClip(int mx, int my, int clientW, int clientH, float* cx, float* cy);

// Converts a clip space drag delta into a delta in the units of SpriteXform.x/y (clip units,
// before the renderer's "second aspect ratio" step), i.e. undoes that step. Note this is not
// a var_x/var_y delta: var_x = (x+1)/2, so a var_x delta is half of this.
void ClipDeltaToSpriteDelta(float dcx, float dcy, float screenAspectWH, float* dx, float* dy);

// scale * 1.1^(wheelDelta/120), clamped to [0.05, 10].
float WheelScale(float scale, int wheelDelta);

enum class SpriteImageKind { D3dx, Gif, Unsupported };
// .png .jpg .jpeg .bmp .tga .dds -> D3dx; .gif -> Gif; case-insensitive.
SpriteImageKind ImageKindFromPath(const wchar_t* path);
