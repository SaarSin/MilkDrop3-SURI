// calm.cpp
//
// SURI: calm motion - see calm.h.

#include "calm.h"
#include <math.h>
#include <stdio.h>

static float Clamp(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

static float Snap(float v, float step, float lo, float hi)
{
    return Clamp(roundf(v / step) * step, lo, hi);
}

float StepSpeed(float cur, int dir)
{
    if (!(cur == cur)) cur = 1.0f;
    return Snap(Snap(cur, kSpeedStep, kSpeedMin, kSpeedMax) + dir * kSpeedStep, kSpeedStep, kSpeedMin, kSpeedMax);
}

float StepSensitivity(float cur, int dir)
{
    if (!(cur == cur)) cur = 1.0f;
    return Snap(Snap(cur, kSensStep, kSensMin, kSensMax) + dir * kSensStep, kSensStep, kSensMin, kSensMax);
}

CalmSettings ClampSettings(float speed, float sens, int antiFlicker)
{
    CalmSettings c;
    if (speed == speed) c.speed = Clamp(speed, kSpeedMin, kSpeedMax);
    if (sens == sens)   c.sensitivity = Clamp(sens, kSensMin, kSensMax);
    c.antiFlicker = (antiFlicker < 0 || antiFlicker > 2) ? (antiFlicker > 2 ? AntiFlicker::Strong : AntiFlicker::Off)
                                                         : (AntiFlicker)antiFlicker;
    return c;
}

AntiFlicker NextAntiFlicker(AntiFlicker a)
{
    switch (a) {
    case AntiFlicker::Off:  return AntiFlicker::Soft;
    case AntiFlicker::Soft: return AntiFlicker::Strong;
    default:                return AntiFlicker::Off;
    }
}

static bool IsOne(float v) { return fabsf(v - 1.0f) < 0.0005f; }

bool IsDefault(const CalmSettings& c)
{
    return IsOne(c.speed) && IsOne(c.sensitivity) && c.antiFlicker == AntiFlicker::Off;
}

static std::wstring Percent(const wchar_t* name, float v)
{
    return std::wstring(name) + L" " + std::to_wstring((int)roundf(v * 100.0f)) + L"%";
}

std::wstring SpeedLabel(float speed)      { return Percent(L"Speed", speed); }
std::wstring SensitivityLabel(float sens) { return Percent(L"Sensitivity", sens); }

std::wstring AntiFlickerLabel(AntiFlicker a)
{
    switch (a) {
    case AntiFlicker::Soft:   return L"Anti-flicker: Soft";
    case AntiFlicker::Strong: return L"Anti-flicker: Strong";
    default:                  return L"Anti-flicker: Off";
    }
}

static std::wstring FloatKey(float v)
{
    if (IsOne(v))
        return std::wstring();
    wchar_t buf[32];
    swprintf_s(buf, L"%.2f", v);
    return buf;
}

std::vector<CalmKey> CalmPresetKeys(const CalmSettings& c)
{
    std::vector<CalmKey> k;
    k.push_back({ L"fCalmSpeed", FloatKey(c.speed) });
    k.push_back({ L"fCalmSensitivity", FloatKey(c.sensitivity) });
    k.push_back({ L"nCalmAntiFlicker",
                  c.antiFlicker == AntiFlicker::Off ? std::wstring() : std::to_wstring((int)c.antiFlicker) });
    return k;
}

CalmSettings BlendCalm(const CalmSettings& oldC, const CalmSettings& newC, float mix)
{
    if (!(mix == mix)) mix = 1.0f;
    mix = Clamp(mix, 0.0f, 1.0f);
    CalmSettings r;
    r.speed = oldC.speed + (newC.speed - oldC.speed) * mix;
    r.sensitivity = oldC.sensitivity + (newC.sensitivity - oldC.sensitivity) * mix;
    if (mix >= 1.0f)
        r.antiFlicker = newC.antiFlicker;
    else
        r.antiFlicker = ((int)oldC.antiFlicker > (int)newC.antiFlicker) ? oldC.antiFlicker : newC.antiFlicker;
    return r;
}

bool StepDue(float* accum, float dtSec)
{
    dtSec = Clamp(dtSec, 0.0f, 0.25f);
    const float frames = dtSec * kRefFps;
    if (frames >= 0.9f) {   // display at or below ~66.7 Hz: every frame is a step, jitter never holds one back
        *accum = 0;
        return true;
    }
    *accum += frames;
    if (*accum < 1.0f)
        return false;
    *accum -= 1.0f;
    if (*accum > 1.0f)
        *accum = 1.0f;   // a slow display does not catch up with a burst of steps
    return true;
}

float StepFps(float displayFps)
{
    if (!(displayFps == displayFps)) displayFps = kRefFps;
    const float r = displayFps < kRefFps ? displayFps : kRefFps;
    return r < 1.0f ? 1.0f : r;
}

void ScaleMotion(CalmMotion* m, float k)
{
    if (k == 1.0f)
        return;
    m->zoom = powf(m->zoom, k);
    m->sx   = powf(m->sx, k);
    m->sy   = powf(m->sy, k);
    m->rot  *= k;
    m->warp *= k;
    m->dx   *= k;
    m->dy   *= k;
}

float ShapeAudio(float rel, float sens)
{
    float v = 1.0f + (rel - 1.0f) * sens;
    return v < 0 ? 0 : v;
}

float WaveFollow(float speed, float dtSec)
{
    if (!(speed == speed) || speed >= 1.0f)
        return 1.0f;
    if (speed < 0) speed = 0;
    return 1.0f - powf(1.0f - speed * speed, kRefFps * Clamp(dtSec, 0.0f, 0.25f));
}

float WaveAmplitude(float sens)
{
    return sens < 1.0f ? sens : 1.0f;
}

float AudioSmoother::Step(float x, float dtSec, AntiFlicker a)
{
    if (a == AntiFlicker::Off || !m_init) {
        m_y = x;
        m_init = true;
        return x;
    }
    const bool strong = (a == AntiFlicker::Strong);
    const float tau = (x > m_y) ? (strong ? 0.10f : 0.04f) : (strong ? 0.60f : 0.25f);
    const float alpha = 1.0f - expf(-Clamp(dtSec, 0.0f, 0.25f) / tau);
    m_y += (x - m_y) * alpha;
    return m_y;
}

float CalmSlew::Step(int channel, float target, float maxPerSec, float dtSec)
{
    if (channel < 0 || channel >= kSlewChannels)
        return target;
    if (!valid[channel] || maxPerSec <= 0 || !(value[channel] == value[channel])) {
        valid[channel] = true;
        value[channel] = target;
        return target;
    }
    const float maxStep = maxPerSec * Clamp(dtSec, 0.0f, 0.25f);
    value[channel] += Clamp(target - value[channel], -maxStep, maxStep);
    return value[channel];
}

void CalmSlew::Reset()
{
    for (int i = 0; i < kSlewChannels; i++) {
        valid[i] = false;
        value[i] = 0;
    }
}

float SlewRate(AntiFlicker a, SlewKind kind)
{
    if (a == AntiFlicker::Off)
        return 0;   // no limit
    const bool strong = (a == AntiFlicker::Strong);
    switch (kind) {
    case SlewKind::Angle: return strong ? 3.0f : 8.0f;   // radians per second
    case SlewKind::Size:  return strong ? 1.0f : 3.0f;   // rad, tex_zoom, x, y
    default:              return strong ? 1.5f : 4.0f;   // colour / alpha 0..1
    }
}

float PersistenceAmount(AntiFlicker a, float stepFps)
{
    if (a == AntiFlicker::Off)
        return 0;
    const float p60 = (a == AntiFlicker::Strong) ? 0.8f : 0.5f;
    if (!(stepFps == stepFps)) stepFps = kRefFps;
    return powf(p60, kRefFps / Clamp(stepFps, 10.0f, 1000.0f));
}

void AnimClock::Advance(float dtSec, float speed)
{
    m_t += Clamp(dtSec, 0.0f, 0.25f) * speed;
}
