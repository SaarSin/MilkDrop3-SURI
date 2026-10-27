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

float PersistenceAmount(AntiFlicker a, float stepFps);   // Off 0; Soft 0.5, Strong 0.8 at 60 steps/s, ^(60/stepFps)

class AnimClock {
public:
    void  Advance(float dtSec, float speed);         // t += clamp(dt, 0, 0.25) * speed
    float Now() const { return m_t; }
    void  Reset() { m_t = 0; }
private:
    float m_t = 0;
};
