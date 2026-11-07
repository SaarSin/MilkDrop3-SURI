// calm.h
//
// SURI: calm motion. Speed, audio sensitivity and anti-flicker settings and the maths behind
// them: step sizes, labels, preset keys, audio shaping and smoothing, slew limits,
// step pacing and the animation clock.
// Pure code: no D3D, no window; unit-tested in code/tests/test_calm.cpp.

#pragma once

#include <string>
#include <vector>

enum class AntiFlicker : int { Off = 0, Soft = 1, Strong = 2 };

struct CalmSettings {
    float speed = 1.0f;
    float sensitivity = 1.0f;
    AntiFlicker antiFlicker = AntiFlicker::Off;
};

const float kSpeedMin = 0.10f, kSpeedMax = 1.5f, kSpeedStep = 0.05f;
const float kSensMin  = 0.0f,  kSensMax  = 2.0f, kSensStep  = 0.10f;
const float kRefFps   = 60.0f;

float StepSpeed(float cur, int dir);                 // dir = +1 / -1; clamped, rounded to the step
float StepSensitivity(float cur, int dir);
CalmSettings ClampSettings(float speed, float sens, int antiFlicker);   // NaN -> default, else clamp
AntiFlicker NextAntiFlicker(AntiFlicker a);          // Off -> Soft -> Strong -> Off
bool IsDefault(const CalmSettings& c);

std::wstring SpeedLabel(float speed);                // L"Speed 15%"
std::wstring SensitivityLabel(float sens);           // L"Sensitivity 80%"
std::wstring AntiFlickerLabel(AntiFlicker a);        // L"Anti-flicker: Soft"

struct CalmKey { const wchar_t* name; std::wstring value; };   // empty value = delete the key
std::vector<CalmKey> CalmPresetKeys(const CalmSettings& c);     // fCalmSpeed, fCalmSensitivity, nCalmAntiFlicker
CalmSettings BlendCalm(const CalmSettings& oldC, const CalmSettings& newC, float mix);   // stronger AF until mix >= 1

bool  StepDue(float* accum, float dtSec);            // frame pacing of the 60 Hz preset step
float StepFps(float displayFps);                     // min(displayFps, 60), at least 1; NaN -> 60

struct CalmMotion { float zoom, rot, warp, dx, dy, sx, sy; };
void  ScaleMotion(CalmMotion* m, float k);           // k == 1: untouched; else zoom/sx/sy ^ k, rot/warp/dx/dy * k

float ShapeAudio(float rel, float sens);             // max(0, 1 + (rel - 1) * sens)
float WaveFollow(float speed, float dtSec);          // speed >= 1: 1; else 1 - (1 - speed^2)^(60*dt)
float WaveAmplitude(float sens);                     // min(1, sens)

class AudioSmoother {
public:
    float Step(float x, float dtSec, AntiFlicker a); // Off: passes through. Soft: 0.04 s / 0.25 s. Strong: 0.10 s / 0.60 s
    void  Reset() { m_init = false; m_y = 0; }
private:
    float m_y = 0;
    bool  m_init = false;
};

const int kSlewChannels = 64;                        // 0..13 main colours, 16 + shape*12 .. shape outputs
struct CalmSlew {                                    // POD: CState is memcpy'd
    float value[kSlewChannels];
    bool  valid[kSlewChannels];
    float Step(int channel, float target, float maxPerSec, float dtSec);   // first sample after Reset passes; maxPerSec <= 0: no limit
    void  Reset();
};
enum class SlewKind { Colour, Angle, Size };
float SlewRate(AntiFlicker a, SlewKind k);           // Off 0 (no limit). Soft: 4 / 8 / 3 per s. Strong: 1.5 / 3 / 1

float PersistenceAmount(AntiFlicker a, float stepFps);   // Off 0; Soft 0.5, Strong 0.65 at 60 steps/s, ^(60/stepFps)

class AnimClock {
public:
    void  Advance(float dtSec, float speed);         // t += clamp(dt, 0, 0.25) * speed
    double Now() const { return m_t; }              // double: stays exact over hours (subtract, then convert to float)
    void  Reset() { m_t = 0; }
private:
    double m_t = 0;
};

// Calm meter (phase 15): measures the final image and the preset's own motion. Only measures;
// nothing here changes a setting.
struct CalmStats {
    float flashesPerSec = 0;     // pairs of opposite luminance swings >= 0.1 in the last second, worst of whole frame / 4 quadrants
    float motionPct = 0;         // warp-mesh motion: % of screen width per second (from the mesh, not the image)
    float changePct = 0;         // mean |pixel change| per 60-fps frame, % (shown for information)
    float bassCorrelation = 0;   // filled by the renderer hook (Task 9); stays 0 here
};

// Fed from a small luminance grid (values 0..1, up to 64x64) once per step. No D3D, no heap.
class FlickerMeter {
public:
    void  AddFrame(const float* luma, int w, int h, float dtSec);   // out-of-range w*h is ignored; dt clamped to [0.0001, 0.25]
    void  Reset();
    float FlashesPerSec() const { return m_flashes; }
    float ChangePct() const { return m_change; }
private:
    static const int kRegions = 5;            // whole frame + 4 quadrants
    static const int kMaxPix = 64 * 64;
    static const int kMaxEvents = 128;
    float m_prev[kMaxPix];
    bool  m_hasPrev = false;
    float m_extreme[kRegions] = {};
    int   m_dir[kRegions] = {};               // +1 rising, -1 falling, 0 unknown
    float m_events[kRegions][kMaxEvents];     // times of luminance transitions
    int   m_nEvents[kRegions] = {};
    float m_t = 0;
    float m_flashes = 0;
    float m_change = 0;
};

// Mean sqrt(du^2 + dv^2) over the warp-mesh vertices; du/dv = (u - u0, v - v0) in texture units.
// Non-finite entries are skipped; null pointers or n <= 0 give 0.
// motionPct = MeshMotion * steps/s * 100, smoothed over 0.5 s by the caller.
float MeshMotion(const float* du, const float* dv, int n);

// Smooths motionPct (one-pole, tau 0.5 s) and keeps the Pearson correlation of motion with bass
// over the last 4 s (40 samples, one per 0.1 s). Fixed arrays, no heap. Measures only.
class MotionTracker {
public:
    void  Add(float meshMotion, float stepsPerSec, float bass, float dtSec);   // non-finite inputs ignored
    void  Reset();
    float MotionPct() const { return m_motion; }
    float BassCorrelation() const;                   // 0 with < 10 samples or (near-)zero variance
private:
    static const int kSlots = 40;
    float m_motion = 0;
    float m_bass = 0;                                // smoothed like motion (tau 0.5 s)
    bool  m_init = false;
    float m_acc = 0;
    float m_m[kSlots] = {};
    float m_b[kSlots] = {};
    int   m_head = 0, m_count = 0;
};

// What the meter would suggest. Pure: only lowers speed / sensitivity, never weakens
// Anti-flicker, never applied here (the user presses Ctrl+7).
const float kTargetMotionPct = 40.0f;
CalmSettings SuggestCalm(const CalmStats& s, const CalmSettings& current);

// The calm meter's two lines in the info card: what it measures, and what Ctrl+7 would apply.
std::wstring MeterReadingLine(float flashesPerSec, float motionPct);   // L"flash 14.0/s  motion 254%/s"
std::wstring MeterSuggestionLine(const CalmSettings& shown);          // L"-> Speed 15% . Sensitivity 20% . Anti-flicker: Strong"
