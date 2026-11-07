// test_calm.cpp
//
// SURI: tests for the calm-motion maths (features/calm.*): speed / sensitivity steps,
// anti-flicker smoothing, slew limits, step pacing and the animation clock.

#include "mini_test.h"
#include "..\vis_milk2\features\calm.h"
#include <math.h>
#include <string.h>

static bool Near(float a, float b, float eps = 1e-4f) { return fabsf(a - b) <= eps; }

TEST(TestCalm_Steps) {
    CHECK(Near(StepSpeed(1.0f, +1), 1.05f));
    float s = 1.0f;
    for (int i = 0; i < 30; i++) s = StepSpeed(s, -1);
    CHECK(s == 0.10f);
    CHECK(StepSpeed(1.5f, +1) == 1.5f);
    CHECK(Near(StepSensitivity(1.0f, -1), 0.9f));
    float z = 0.0f;
    z = StepSensitivity(z, -1);
    CHECK(z == 0.0f);
    float r = 1.0f;
    for (int i = 0; i < 3; i++) r = StepSpeed(r, -1);
    for (int i = 0; i < 3; i++) r = StepSpeed(r, +1);
    CHECK(r == 1.0f);
}

TEST(TestCalm_ClampSettings) {
    CalmSettings a = ClampSettings(50.0f, -3.0f, 7);
    CHECK(a.speed == 1.5f);
    CHECK(a.sensitivity == 0.0f);
    CHECK(a.antiFlicker == AntiFlicker::Strong);
    CalmSettings b = ClampSettings(NAN, NAN, -1);
    CHECK(b.speed == 1.0f);
    CHECK(b.sensitivity == 1.0f);
    CHECK(b.antiFlicker == AntiFlicker::Off);
    CHECK(IsDefault(b));
    CHECK(!IsDefault(a));
}

TEST(TestCalm_Labels) {
    CHECK(SpeedLabel(0.15f) == L"Speed 15%");
    CHECK(SensitivityLabel(0.8f) == L"Sensitivity 80%");
    CHECK(AntiFlickerLabel(AntiFlicker::Soft) == L"Anti-flicker: Soft");
    CHECK(NextAntiFlicker(AntiFlicker::Off) == AntiFlicker::Soft);
    CHECK(NextAntiFlicker(AntiFlicker::Soft) == AntiFlicker::Strong);
    CHECK(NextAntiFlicker(AntiFlicker::Strong) == AntiFlicker::Off);
}

TEST(TestCalm_PresetKeys) {
    std::vector<CalmKey> d = CalmPresetKeys(CalmSettings());
    CHECK(d.size() == 3);
    for (size_t i = 0; i < d.size(); i++) CHECK(d[i].value.empty());
    CalmSettings c; c.speed = 0.15f; c.sensitivity = 1.0f; c.antiFlicker = AntiFlicker::Soft;
    std::vector<CalmKey> k = CalmPresetKeys(c);
    CHECK(k.size() == 3);
    CHECK(wcscmp(k[0].name, L"fCalmSpeed") == 0);
    CHECK(k[0].value == L"0.15");
    CHECK(wcscmp(k[1].name, L"fCalmSensitivity") == 0);
    CHECK(k[1].value.empty());
    CHECK(wcscmp(k[2].name, L"nCalmAntiFlicker") == 0);
    CHECK(k[2].value == L"1");
}

TEST(TestCalm_BlendCalm) {
    CalmSettings o; o.speed = 0.4f; o.sensitivity = 0.5f; o.antiFlicker = AntiFlicker::Strong;
    CalmSettings n;
    CalmSettings m0 = BlendCalm(o, n, 0.0f);
    CHECK(Near(m0.speed, 0.4f) && Near(m0.sensitivity, 0.5f) && m0.antiFlicker == AntiFlicker::Strong);
    CalmSettings mh = BlendCalm(o, n, 0.5f);
    CHECK(Near(mh.speed, 0.7f));
    CHECK(Near(mh.sensitivity, 0.75f));
    CHECK(mh.antiFlicker == AntiFlicker::Strong);
    CalmSettings m1 = BlendCalm(o, n, 1.0f);
    CHECK(IsDefault(m1));
}

TEST(TestCalm_StepDue) {
    float acc = 0; int steps = 0;
    for (int i = 0; i < 144; i++) if (StepDue(&acc, 1.0f / 144.0f)) steps++;
    CHECK(steps >= 59 && steps <= 61);
    acc = 0; steps = 0;
    for (int i = 0; i < 30; i++) if (StepDue(&acc, 1.0f / 30.0f)) steps++;
    CHECK(steps == 30);
    acc = 0; steps = 0;
    for (int i = 0; i < 60; i++) if (StepDue(&acc, 1.0f / 60.5f)) steps++;
    CHECK(steps == 60);
    CHECK(StepFps(144.0f) == 60.0f);
    CHECK(StepFps(30.0f) == 30.0f);
    CHECK(StepFps(NAN) == 60.0f);
}

TEST(TestCalm_ScaleMotion) {
    CalmMotion m = { 1.21f, 0.2f, 0.7f, 0.01f, -0.02f, 1.0f, 1.0f };
    CalmMotion c = m;
    ScaleMotion(&c, 1.0f);
    CHECK(memcmp(&c, &m, sizeof(m)) == 0);
    ScaleMotion(&c, 0.5f);
    CHECK(Near(c.zoom, 1.1f));
    CHECK(Near(c.rot, 0.1f));
    CHECK(c.sx == 1.0f);
}

TEST(TestCalm_ScaleMotionNegativeAndZero) {
    CalmMotion m = { -1.21f, 0.2f, 0.0f, 0.0f, 0.0f, -4.0f, 0.0f };
    ScaleMotion(&m, 0.5f);
    CHECK(Near(m.zoom, -1.1f));
    CHECK(Near(m.sx, -2.0f));
    CHECK(m.sy == 0.0f);
    CHECK(isfinite(m.zoom) && isfinite(m.sx) && isfinite(m.sy));
    CalmMotion z = { 0.0f, 0, 0, 0, 0, 0.0f, 1.0f };
    ScaleMotion(&z, 0.5f);
    CHECK(z.zoom == 0.0f && z.sx == 0.0f && z.sy == 1.0f);
}

TEST(TestCalm_Audio) {
    CHECK(Near(ShapeAudio(1.7f, 1.0f), 1.7f));
    CHECK(ShapeAudio(3.7f, 1.0f) == 3.7f);
    CHECK(ShapeAudio(0.3f, 1.0f) == 0.3f);
    CHECK(ShapeAudio(1.7f, 0.0f) == 1.0f);
    CHECK(Near(ShapeAudio(1.7f, 0.5f), 1.35f));
    CHECK(ShapeAudio(0.1f, 2.0f) == 0.0f);
    CHECK(WaveFollow(1.0f, 1.0f / 60.0f) == 1.0f);
    CHECK(Near(WaveFollow(0.5f, 1.0f / 60.0f), 0.25f));
    CHECK(Near(WaveFollow(0.15f, 1.0f / 60.0f), 0.0225f));
    CHECK(WaveAmplitude(1.5f) == 1.0f);
}

TEST(TestCalm_AudioSmoother) {
    AudioSmoother off;
    CHECK(off.Step(1.0f, 1.0f / 60.0f, AntiFlicker::Off) == 1.0f);
    CHECK(off.Step(3.0f, 1.0f / 60.0f, AntiFlicker::Off) == 3.0f);
    AudioSmoother s;
    float y = s.Step(1.0f, 1.0f / 60.0f, AntiFlicker::Soft);
    for (int i = 0; i < 12; i++) y = s.Step(3.0f, 1.0f / 60.0f, AntiFlicker::Soft);
    CHECK(y > 2.9f);
    for (int i = 0; i < 12; i++) y = s.Step(1.0f, 1.0f / 60.0f, AntiFlicker::Soft);
    CHECK(y > 1.3f);
}

TEST(TestCalm_SlewFirstSamplePassesThrough) {
    CalmSlew sl; sl.Reset();
    CHECK(sl.Step(0, 0.9f, 1.5f, 1.0f / 60.0f) == 0.9f);
    CHECK(Near(sl.Step(0, 0.0f, 1.5f, 1.0f / 60.0f), 0.875f));
    CHECK(sl.Step(0, 0.0f, 0.0f, 1.0f / 60.0f) == 0.0f);
    CHECK(SlewRate(AntiFlicker::Off, SlewKind::Colour) == 0.0f);
    CHECK(SlewRate(AntiFlicker::Strong, SlewKind::Angle) == 3.0f);
}

TEST(TestCalm_Persistence) {
    CHECK(PersistenceAmount(AntiFlicker::Off, 60.0f) == 0.0f);
    CHECK(Near(PersistenceAmount(AntiFlicker::Strong, 60.0f), 0.65f));   // manual test: 0.8 smeared everything
    CHECK(Near(PersistenceAmount(AntiFlicker::Strong, 30.0f), 0.4225f));
    CHECK(Near(PersistenceAmount(AntiFlicker::Soft, 60.0f), 0.5f));
}

TEST(TestCalm_AnimClockNoJump) {
    AnimClock c; c.Reset();
    for (int i = 0; i < 60; i++) c.Advance(1.0f / 60.0f, 1.0f);
    for (int i = 0; i < 60; i++) c.Advance(1.0f / 60.0f, 0.5f);
    CHECK(Near((float)c.Now(), 1.5f, 1e-3f));
    double before = c.Now();
    c.Advance(5.0f, 1.0f);
    CHECK(Near((float)(c.Now() - before), 0.25f));
    before = c.Now();
    c.Advance(-1.0f, 1.0f);
    CHECK(c.Now() == before);
}

TEST(TestCalm_AnimClockLongRun) {
    // 10 h at 144 Hz, Speed 10%: a float clock would stall; the double one keeps counting
    AnimClock c; c.Reset();
    const int frames = 10 * 3600 * 144;
    for (int i = 0; i < frames; i++) c.Advance(1.0f / 144.0f, 0.1f);
    CHECK(fabs(c.Now() - 3600.0) <= 1.0);
    double before = c.Now();
    c.Advance(1.0f / 144.0f, 0.1f);
    CHECK(c.Now() > before);
}

TEST(TestCalm_FlickerMeterCountsStrobe) {
    // black/white every 10 frames at 60 fps = 6 swings per second = 3 flashes per second
    FlickerMeter m;
    float img[16 * 9];
    for (int f = 0; f < 120; f++) {
        const float l = ((f / 10) % 2) ? 1.0f : 0.0f;
        for (int i = 0; i < 16 * 9; i++) img[i] = l;
        m.AddFrame(img, 16, 9, 1 / 60.f);
    }
    CHECK(m.FlashesPerSec() >= 2.5f && m.FlashesPerSec() <= 3.5f);
    CHECK(m.ChangePct() > 5.0f);
    m.Reset();
    CHECK(m.FlashesPerSec() == 0.0f);
    CHECK(m.ChangePct() == 0.0f);
}
TEST(TestCalm_FlickerMeterStillImage) {
    FlickerMeter m;
    float img[16 * 9];
    for (int i = 0; i < 16 * 9; i++) img[i] = 0.4f;
    for (int f = 0; f < 120; f++) m.AddFrame(img, 16, 9, 1 / 60.f);
    CHECK(m.FlashesPerSec() == 0.0f);
    CHECK(m.ChangePct() < 0.01f);
}
TEST(TestCalm_FlickerMeterIgnoresBadInput) {
    FlickerMeter m;
    float img[4] = {0, 1, 0, 1};
    m.AddFrame(nullptr, 2, 2, 1 / 60.f);
    m.AddFrame(img, 100, 100, 1 / 60.f);   // larger than 64x64
    m.AddFrame(img, 0, 2, 1 / 60.f);
    CHECK(m.FlashesPerSec() == 0.0f);
    CHECK(m.ChangePct() == 0.0f);
}
TEST(TestCalm_MeshMotion) {
    float du[100], dv[100];
    for (int i = 0; i < 100; i++) { du[i] = 0.01f; dv[i] = 0; }
    CHECK(Near(MeshMotion(du, dv, 100), 0.01f));
    for (int i = 0; i < 100; i++) { du[i] = 0.006f; dv[i] = 0.008f; }
    CHECK(Near(MeshMotion(du, dv, 100), 0.01f));
    for (int i = 0; i < 100; i++) { du[i] = 0; dv[i] = 0; }
    CHECK(MeshMotion(du, dv, 100) == 0.0f);
    CHECK(MeshMotion(nullptr, dv, 100) == 0.0f);
    CHECK(MeshMotion(du, dv, 0) == 0.0f);
    du[0] = NAN; dv[1] = INFINITY; du[2] = 0.03f; dv[2] = 0.04f;
    CHECK(Near(MeshMotion(du, dv, 3), 0.05f));   // non-finite entries are skipped, not averaged in
}

TEST(TestCalm_MotionTrackerSmoothing) {
    MotionTracker t;
    for (int i = 0; i < 120; i++) t.Add(0.01f, 60.0f, 0.5f, 1.0f / 60.0f);   // 2 s
    CHECK(Near(t.MotionPct(), 60.0f, 60.0f * 0.03f));
    MotionTracker u;
    for (int i = 0; i < 120; i++) u.Add(0.0f, 60.0f, 0.5f, 1.0f / 60.0f);
    u.Add(0.1f, 60.0f, 0.5f, 1.0f / 60.0f);                                   // one spike of 600 %
    CHECK(u.MotionPct() < 600.0f * 0.1f);
    u.Add(NAN, 60.0f, 0.5f, 1.0f / 60.0f);                                    // ignored
    CHECK(isfinite(u.MotionPct()));
}
TEST(TestCalm_MotionTrackerCorrelation) {
    const float dt = 1.0f / 60.0f;
    MotionTracker a;
    for (int i = 0; i < 400; i++) {
        float bass = 1.0f + 0.5f * sinf(i * dt * 2.0f * 3.14159265f * 0.1f);
        a.Add(0.005f * bass, 60.0f, bass, dt);
    }
    CHECK(a.BassCorrelation() > 0.8f);
    MotionTracker b;
    for (int i = 0; i < 400; i++) {
        float bass = 1.0f + 0.5f * sinf(i * dt * 2.0f * 3.14159265f * 0.1f);
        b.Add(0.005f, 60.0f, bass, dt);
    }
    CHECK(fabsf(b.BassCorrelation()) < 0.2f);
    MotionTracker c;
    for (int i = 0; i < 40; i++) c.Add(0.005f * (i % 7), 60.0f, (float)(i % 5), dt);   // 0.67 s: < 10 samples
    CHECK(c.BassCorrelation() == 0.0f);
    c.Reset();
    CHECK(c.MotionPct() == 0.0f);
}

static CalmStats Stats(float flashes, float motion, float corr) {
    CalmStats s; s.flashesPerSec = flashes; s.motionPct = motion; s.bassCorrelation = corr; return s;
}
TEST(TestCalm_SuggestStrobe) {
    CalmSettings cur;
    CHECK(SuggestCalm(Stats(4, 0, 0), cur).antiFlicker == AntiFlicker::Strong);
    CHECK(SuggestCalm(Stats(2, 0, 0), cur).antiFlicker == AntiFlicker::Soft);
    cur.antiFlicker = AntiFlicker::Strong;
    CHECK(SuggestCalm(Stats(2, 0, 0), cur).antiFlicker == AntiFlicker::Strong);   // never weakened
    CHECK(SuggestCalm(Stats(0, 0, 0), cur).antiFlicker == AntiFlicker::Strong);
}
TEST(TestCalm_SuggestSlowsSelfMotion) {
    CalmSettings cur; cur.speed = 1.0f; cur.sensitivity = 0.8f;
    CalmSettings r = SuggestCalm(Stats(0, 160, 0), cur);
    CHECK(Near(r.speed, 0.25f));
    CHECK(r.speed <= 0.30f);
    CHECK(r.sensitivity == cur.sensitivity);
}
TEST(TestCalm_SuggestLowersSensitivityForAudioMotion) {
    CalmSettings cur; cur.speed = 1.0f; cur.sensitivity = 1.0f;
    CalmSettings r = SuggestCalm(Stats(0, 80, 0.8f), cur);
    CHECK(Near(r.sensitivity, 0.5f));
    CHECK(Near(r.speed, 0.5f));
}
TEST(TestCalm_SuggestNeverRaises) {
    CalmSettings cur; cur.speed = 0.15f; cur.sensitivity = 0.3f;
    CalmSettings q = SuggestCalm(Stats(0, 10, 0.9f), cur);
    CHECK(q.speed == cur.speed && q.sensitivity == cur.sensitivity && q.antiFlicker == cur.antiFlicker);
    CalmSettings r = SuggestCalm(Stats(0, 4000, 0.9f), cur);
    CHECK(r.speed <= cur.speed && r.speed >= kSpeedMin);
    CHECK(r.sensitivity <= cur.sensitivity && r.sensitivity >= kSensMin);
}
TEST(TestCalm_SuggestNeverApplied) {
    CalmSettings cur; cur.speed = 0.7f; cur.sensitivity = 1.3f; cur.antiFlicker = AntiFlicker::Soft;
    CalmSettings copy = cur;
    const float flashes[] = { 0, 2, 5, 100 };
    const float motions[] = { 0, 50, 5000, 1e9f };
    for (float f : flashes) for (float m : motions) for (float c : { 0.0f, 0.9f })
        SuggestCalm(Stats(f, m, c), cur);
    CHECK(memcmp(&cur, &copy, sizeof cur) == 0);
}
TEST(TestCalm_MotionTrackerCorrelationBeat) {
    const float dt = 1.0f / 60.0f;
    MotionTracker a;
    for (int i = 0; i < 360; i++) {      // 6 s, 2 Hz bass
        float bass = 1.0f + 0.5f * sinf(i * dt * 2.0f * 3.14159265f * 2.0f);
        a.Add(0.005f * bass, 60.0f, bass, dt);
    }
    CHECK(a.BassCorrelation() > 0.8f);
}
TEST(TestCalm_MeterLines) {   // SURI: two lines, so a narrow card no longer cuts the suggestion
    CHECK(MeterReadingLine(14.04f, 254.4f) == L"flash 14.0/s  motion 254%/s");
    CalmSettings s; s.speed = 0.15f; s.sensitivity = 0.2f; s.antiFlicker = AntiFlicker::Strong;
    CHECK(MeterSuggestionLine(s) == L"\x2192 Speed 15% \x00B7 Sensitivity 20% \x00B7 Anti-flicker: Strong");
}
TEST(TestCalm_SuggestSensitivityFloor) {   // SURI: Speed already brings motion to the target;
    // Sensitivity is lowered at most to 50% and never chased further (manual test: Soup 20% -> 0%)
    CalmSettings cur;   // 100% / 100%
    CalmSettings r = SuggestCalm(Stats(0, 217, 0.9f), cur);
    CHECK(Near(r.speed, 0.2f));
    CHECK(Near(r.sensitivity, kSensSuggestFloor));
    cur.sensitivity = 0.2f;
    CHECK(SuggestCalm(Stats(0, 230, 0.9f), cur).sensitivity == 0.2f);
    cur.sensitivity = 0.5f;
    CHECK(SuggestCalm(Stats(0, 230, 0.9f), cur).sensitivity == 0.5f);
}
