// test_sprite_layout.cpp
//
// SURI: tests for features/sprite_layout.* (sprite quad geometry, hit test, wheel scaling).

#include "mini_test.h"
#include <math.h>
#include <windows.h>
#include "..\vis_milk2\features\sprite_layout.h"

static bool Near(float a, float b) { return fabsf(a - b) < 1e-4f; }

TEST(TestSpriteLayout_SquareAtCenter) {
    float q[4][2];
    SpriteCorners(SpriteXform{ 0, 0, 0.5f, 0.5f, 0, false, false }, 1.0f, 1.0f, q);
    CHECK(Near(q[0][0], -0.5f) && Near(q[0][1], -0.5f));
    CHECK(Near(q[3][0],  0.5f) && Near(q[3][1],  0.5f));
    CHECK(PointInSpriteQuad(q, 0, 0));
    CHECK(!PointInSpriteQuad(q, 0.9f, 0.9f));
}
TEST(TestSpriteLayout_WideImageAndWideScreen) {
    float q[4][2];
    SpriteCorners(SpriteXform{ 0, 0, 0.5f, 0.5f, 0, false, false }, 0.5f, 2.0f, q); // image 2:1, screen 2:1
    CHECK(Near(q[3][0], 0.5f));
    CHECK(Near(q[3][1], 0.5f));   // 0.5 * 0.5 (image) * 2 (screen)
}
TEST(TestSpriteLayout_Rotated45) {
    float q[4][2];
    SpriteCorners(SpriteXform{ 0, 0, 0.5f, 0.5f, 0.785398f, false, false }, 1.0f, 1.0f, q);
    CHECK(PointInSpriteQuad(q, 0, 0.6f));    // a corner now points up
    CHECK(!PointInSpriteQuad(q, 0.45f, 0.45f));
}
TEST(TestSpriteLayout_FlippedStillHits) {
    float q[4][2];
    SpriteCorners(SpriteXform{ 0, 0, 0.5f, 0.5f, 0, true, false }, 1.0f, 1.0f, q);
    CHECK(Near(q[0][0], 0.5f) && Near(q[0][1], -0.5f));
    CHECK(PointInSpriteQuad(q, 0.1f, 0.1f));
    CHECK(!PointInSpriteQuad(q, 0.9f, 0.1f));
}
TEST(TestSpriteLayout_TopmostIsLastDrawn) {
    SpriteHit h[3] = {};
    SpriteCorners(SpriteXform{ 0, 0, 0.5f, 0.5f, 0, false, false }, 1, 1, h[0].q); h[0].live = true; h[0].startFrame = 10;
    SpriteCorners(SpriteXform{ 0, 0, 0.5f, 0.5f, 0, false, false }, 1, 1, h[1].q); h[1].live = true; h[1].startFrame = 20;
    SpriteCorners(SpriteXform{ 0, 0, 0.5f, 0.5f, 0, false, false }, 1, 1, h[2].q); h[2].live = false; h[2].startFrame = 30;
    CHECK(TopmostSpriteAt(h, 3, 0, 0) == 1);
    CHECK(TopmostSpriteAt(h, 3, 0.9f, 0.9f) == -1);
    // a lower slot with a newer start frame is still drawn under the higher slot
    h[0].startFrame = 50;
    CHECK(TopmostSpriteAt(h, 3, 0, 0) == 1);
    // only the lower slot under the point
    SpriteCorners(SpriteXform{ 0.6f, 0, 0.2f, 0.2f, 0, false, false }, 1, 1, h[0].q);
    CHECK(TopmostSpriteAt(h, 3, 0.7f, 0) == 0);
}
TEST(TestSpriteLayout_WindowToClip) {
    float x, y;
    WindowToClip(0, 0, 800, 600, &x, &y);     CHECK(Near(x, -1) && Near(y, 1));
    WindowToClip(400, 300, 800, 600, &x, &y); CHECK(Near(x, 0) && Near(y, 0));
}
TEST(TestSpriteLayout_DragDeltaUndoesAspect) {
    float dx, dy;
    ClipDeltaToSpriteDelta(0.1f, 0.1f, 16.0f / 9.0f, &dx, &dy);
    CHECK(Near(dx, 0.1f) && Near(dy, 0.1f * 9.0f / 16.0f));
    ClipDeltaToSpriteDelta(0.1f, 0.1f, 0.5f, &dx, &dy);
    CHECK(Near(dx, 0.05f) && Near(dy, 0.1f));
}
TEST(TestSpriteLayout_WheelScale) {
    CHECK(Near(WheelScale(1.0f, 120), 1.1f));
    CHECK(Near(WheelScale(1.1f, -120), 1.0f));
    CHECK(Near(WheelScale(9.9f, 1200), 10.0f));
    CHECK(Near(WheelScale(0.06f, -1200), 0.05f));
}
TEST(TestSpriteLayout_ImageKind) {
    CHECK(ImageKindFromPath(L"C:\\a\\b.PNG") == SpriteImageKind::D3dx);
    CHECK(ImageKindFromPath(L"dir.v2\\file") == SpriteImageKind::Unsupported);
    CHECK(ImageKindFromPath(L"x.") == SpriteImageKind::Unsupported);
    CHECK(ImageKindFromPath(L"x.jpeg") == SpriteImageKind::D3dx);
    CHECK(ImageKindFromPath(L"x.Gif") == SpriteImageKind::Gif);
    CHECK(ImageKindFromPath(L"x.webp") == SpriteImageKind::Unsupported);
    CHECK(ImageKindFromPath(L"noext") == SpriteImageKind::Unsupported);
}
TEST(TestSpriteLayout_DegenerateQuadNeverHits) {
    float q[4][2];
    SpriteCorners(SpriteXform{ 0, 0, 0, 0, 0, false, false }, 1.0f, 1.0f, q);
    CHECK(!PointInSpriteQuad(q, 0, 0));
    CHECK(!PointInSpriteQuad(q, 0.5f, 0.5f));
    SpriteCorners(SpriteXform{ 0, 0, 0, 0.5f, 0, false, false }, 1.0f, 1.0f, q);
    CHECK(!PointInSpriteQuad(q, 0, 0.2f));
}
TEST(TestSpriteLayout_RotationSign) {
    float q[4][2];
    SpriteCorners(SpriteXform{ 0, 0, 0.5f, 0.1f, 1.5707963f, false, false }, 1.0f, 1.0f, q);
    CHECK(Near(q[3][0], -0.1f) && Near(q[3][1], 0.5f));
}
