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

TEST(TestCalm_Audio) {
    CHECK(Near(ShapeAudio(1.7f, 1.0f), 1.7f));
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
    CHECK(Near(PersistenceAmount(AntiFlicker::Strong, 60.0f), 0.8f));
    CHECK(Near(PersistenceAmount(AntiFlicker::Strong, 30.0f), 0.64f));
}

TEST(TestCalm_AnimClockNoJump) {
    AnimClock c; c.Reset();
    for (int i = 0; i < 60; i++) c.Advance(1.0f / 60.0f, 1.0f);
    for (int i = 0; i < 60; i++) c.Advance(1.0f / 60.0f, 0.5f);
    CHECK(Near(c.Now(), 1.5f, 1e-3f));
    float before = c.Now();
    c.Advance(5.0f, 1.0f);
    CHECK(Near(c.Now() - before, 0.25f));
    before = c.Now();
    c.Advance(-1.0f, 1.0f);
    CHECK(c.Now() == before);
}
