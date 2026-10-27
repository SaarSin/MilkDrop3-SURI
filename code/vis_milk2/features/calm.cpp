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

// SURI: presets may give a negative (mirroring) or zero zoom/stretch; powf would return NaN or inf.
static float SignedPow(float v, float k)
{
    if (v == 0.0f || !(v == v))
        return v;
    const float r = powf(fabsf(v), k);
    return v < 0 ? -r : r;
}

void ScaleMotion(CalmMotion* m, float k)
{
    if (k == 1.0f)
        return;
    m->zoom = SignedPow(m->zoom, k);
    m->sx   = SignedPow(m->sx, k);
    m->sy   = SignedPow(m->sy, k);
    m->rot  *= k;
    m->warp *= k;
    m->dx   *= k;
    m->dy   *= k;
}

float ShapeAudio(float rel, float sens)
{
    if (sens == 1.0f)
        return rel;   // bit-identical at the default
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

void FlickerMeter::Reset()
{
    m_hasPrev = false;
    m_t = 0;
    for (int r = 0; r < kRegions; r++) {
        m_dir[r] = 0;
        m_nEvents[r] = 0;
        m_extreme[r] = 0;
    }
    m_flashes = 0;
    m_change = 0;
}

void FlickerMeter::AddFrame(const float* luma, int w, int h, float dtSec)
{
    if (!luma || w < 2 || h < 2 || w > 64 || h > 64 || w * h > kMaxPix)
        return;
    const int n = w * h;
    if (!(dtSec == dtSec)) dtSec = 1.0f / kRefFps;
    dtSec = Clamp(dtSec, 0.0001f, 0.25f);
    m_t += dtSec;

    // region means: whole + 4 quadrants
    float sum[kRegions] = {};
    int   cnt[kRegions] = {};
    float change = 0;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            const float l = luma[y * w + x];
            const int q = 1 + (y >= h / 2 ? 2 : 0) + (x >= w / 2 ? 1 : 0);
            sum[0] += l; cnt[0]++;
            sum[q] += l; cnt[q]++;
            if (m_hasPrev)
                change += fabsf(l - m_prev[y * w + x]);
            m_prev[y * w + x] = l;
        }

    // change per 60-fps frame, smoothed (tau 0.5 s)
    if (m_hasPrev) {
        const float perRefFrame = (change / n) / (dtSec * kRefFps) * 100.0f;
        const float a = 1.0f - expf(-dtSec / 0.5f);
        m_change += (perRefFrame - m_change) * a;
    }

    // flashes: a transition is a swing of >= 0.1 against the previous extreme
    const float kSwing = 0.1f;
    float worst = 0;
    for (int r = 0; r < kRegions; r++) {
        const float mean = sum[r] / (cnt[r] ? cnt[r] : 1);
        bool turned = false;
        if (!m_hasPrev) {
            m_extreme[r] = mean;
            m_dir[r] = 0;
        } else if (m_dir[r] == 0) {
            if (fabsf(mean - m_extreme[r]) >= kSwing) {  // first clear move sets the direction
                m_dir[r] = (mean > m_extreme[r]) ? 1 : -1;
                m_extreme[r] = mean;
            }
        } else if (m_dir[r] > 0) {
            if (mean > m_extreme[r])
                m_extreme[r] = mean;                     // still rising
            else if (m_extreme[r] - mean >= kSwing)
                turned = true;
        } else {
            if (mean < m_extreme[r])
                m_extreme[r] = mean;                     // still falling
            else if (mean - m_extreme[r] >= kSwing)
                turned = true;
        }
        if (turned) {
            m_dir[r] = -m_dir[r];
            m_extreme[r] = mean;
            if (m_nEvents[r] < kMaxEvents)
                m_events[r][m_nEvents[r]++] = m_t;
        }
        // drop events older than one second
        int keep = 0;
        for (int i = 0; i < m_nEvents[r]; i++)
            if (m_t - m_events[r][i] <= 1.0f)
                m_events[r][keep++] = m_events[r][i];
        m_nEvents[r] = keep;
        const float flashes = m_nEvents[r] * 0.5f;       // two transitions = one flash
        if (flashes > worst) worst = flashes;
    }
    m_flashes = worst;
    m_hasPrev = true;
}

float MeshMotion(const float* du, const float* dv, int n)
{
    if (!du || !dv || n <= 0)
        return 0;
    double sum = 0;
    int used = 0;
    for (int i = 0; i < n; i++) {
        const float a = du[i], b = dv[i];
        if (!isfinite(a) || !isfinite(b))
            continue;
        sum += sqrtf(a * a + b * b);
        used++;
    }
    return used ? (float)(sum / used) : 0.0f;
}

void MotionTracker::Reset()
{
    m_motion = 0; m_init = false; m_acc = 0; m_head = 0; m_count = 0;
    for (int i = 0; i < kSlots; i++) { m_m[i] = 0; m_b[i] = 0; }
}

void MotionTracker::Add(float meshMotion, float stepsPerSec, float bass, float dtSec)
{
    if (!isfinite(meshMotion) || !isfinite(stepsPerSec) || !isfinite(bass) || !isfinite(dtSec))
        return;
    const float dt = Clamp(dtSec, 0.0001f, 0.25f);
    const float inst = meshMotion * stepsPerSec * 100.0f;
    if (!m_init) { m_motion = inst; m_init = true; }
    else         m_motion += (inst - m_motion) * (1.0f - expf(-dt / 0.5f));
    m_acc += dt;
    if (m_acc >= 0.1f) {
        m_acc -= 0.1f;
        if (m_acc > 0.1f) m_acc = 0;                 // a long frame stores one sample, not a burst
        m_m[m_head] = m_motion;
        m_b[m_head] = bass;
        m_head = (m_head + 1) % kSlots;
        if (m_count < kSlots) m_count++;
    }
}

float MotionTracker::BassCorrelation() const
{
    if (m_count < 10)
        return 0.0f;
    double mm = 0, mb = 0;
    for (int i = 0; i < m_count; i++) { mm += m_m[i]; mb += m_b[i]; }
    mm /= m_count; mb /= m_count;
    double sxx = 0, syy = 0, sxy = 0;
    for (int i = 0; i < m_count; i++) {
        const double x = m_m[i] - mm, y = m_b[i] - mb;
        sxx += x * x; syy += y * y; sxy += x * y;
    }
    if (sxx < 1e-9 || syy < 1e-9)
        return 0.0f;
    const double r = sxy / sqrt(sxx * syy);
    return (float)(r > 1.0 ? 1.0 : (r < -1.0 ? -1.0 : r));
}

CalmSettings SuggestCalm(const CalmStats& s, const CalmSettings& current)
{
    CalmSettings r = current;
    if (s.flashesPerSec >= 3.0f)
        r.antiFlicker = AntiFlicker::Strong;
    else if (s.flashesPerSec >= 1.5f && (int)r.antiFlicker < (int)AntiFlicker::Soft)
        r.antiFlicker = AntiFlicker::Soft;

    if (s.motionPct > kTargetMotionPct) {
        const float ratio = kTargetMotionPct / s.motionPct;
        float sp = Snap(current.speed * ratio, kSpeedStep, kSpeedMin, current.speed);
        if (sp > current.speed) sp = current.speed;
        r.speed = sp;
        if (s.bassCorrelation > 0.5f) {
            float se = Snap(current.sensitivity * ratio, kSensStep, kSensMin, current.sensitivity);
            if (se > current.sensitivity) se = current.sensitivity;
            r.sensitivity = se;
        }
    }
    return r;
}
