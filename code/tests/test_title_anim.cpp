#include "mini_test.h"
#include "../vis_milk2/features/title_anim.h"
#include <math.h>

static bool Near(float a, float b) { return fabsf(a - b) < 1e-4f; }

TEST(TestTitleAnim_ClassicBurnsInAfterFlyIn) {
    TitlePhase p = SongTitlePhase(0.85f, 1.7f, 0, 0);
    CHECK(p.visible && !p.burnIn && Near(p.progress, 0.5f) && Near(p.alpha, 1.0f));
    p = SongTitlePhase(1.7f, 1.7f, 0, 0);
    CHECK(p.visible && p.burnIn && Near(p.progress, 1.0f));
    CHECK(Near(SongTitleEnd(1.7f, 0, 0), 1.7f));
}

TEST(TestTitleAnim_HoldThenFade) {
    // 1.7 s fly-in, 7 s on screen, 3 s fade
    TitlePhase p = SongTitlePhase(1.69f, 1.7f, 7, 3);
    CHECK(p.visible && !p.burnIn && p.progress < 1.0f && Near(p.alpha, 1.0f));
    p = SongTitlePhase(5.0f, 1.7f, 7, 3);                     // holding
    CHECK(p.visible && !p.burnIn && p.progress < 1.0f && Near(p.alpha, 1.0f));
    p = SongTitlePhase(8.69f, 1.7f, 7, 3);                    // end of hold
    CHECK(p.visible && Near(p.alpha, 1.0f));
    p = SongTitlePhase(1.7f + 7 + 1.5f, 1.7f, 7, 3);          // half way through the fade
    CHECK(p.visible && Near(p.alpha, 0.5f));
    p = SongTitlePhase(1.7f + 7 + 2.9f, 1.7f, 7, 3);
    CHECK(p.visible && p.alpha > 0 && p.alpha < 0.05f);
    p = SongTitlePhase(1.7f + 7 + 3.0f, 1.7f, 7, 3);          // gone, never burned in
    CHECK(!p.visible && !p.burnIn);
    CHECK(Near(SongTitleEnd(1.7f, 7, 3), 11.7f));
}

TEST(TestTitleAnim_NotStartedOrBadDuration) {
    CHECK(!SongTitlePhase(-1.0f, 1.7f, 7, 3).visible);
    TitlePhase noFly = SongTitlePhase(1.0f, 0.0f, 7, 3);   // no fly-in: straight to the hold
    CHECK(noFly.visible && !noFly.burnIn && Near(noFly.alpha, 1.0f));
    CHECK(SongTitlePhase(0.0f, 0.0f, 0, 0).burnIn);         // classic title still burns in
}

TEST(TestTitleAnim_FitScale) {
    CHECK(Near(FitTitleScale(500, 1024, 0.35f), 1.0f));
    CHECK(Near(FitTitleScale(2048, 1024, 0.35f), 0.5f));
    CHECK(Near(FitTitleScale(10000, 1024, 0.35f), 0.35f));
    CHECK(Near(FitTitleScale(0, 1024, 0.35f), 1.0f));
}
