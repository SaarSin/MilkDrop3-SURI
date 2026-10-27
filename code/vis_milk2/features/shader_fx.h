// shader_fx.h
//
// SURI: real-time effects injected into a preset's composite pixel shader text.
// Each effect is a marked block ("// SURI-FX:<name> begin" .. "end") placed inside
// shader_body, so it can be removed again exactly (toggle off, or strip before saving).
// Pure code: no D3D, no window; unit-tested in code/tests/test_shader_fx.cpp.

#pragma once

#include <string>

enum class FxId : unsigned char {
    MirrorX, MirrorY, Kaleido, Ripple, ZoomIn, SwapAxes, Tunnel,   // geometry (uv), in this order
    Invert, Mono, Posterize, HueSpin,                              // colour (ret), in this order
    Count
};

struct FxState {
    unsigned mask = 0;          // bit (1u << (unsigned)FxId)
    float brightness = 1.0f;    // 0.2 .. 3.0, 1.0 = off
    float contrast   = 1.0f;    // 0.2 .. 3.0, 1.0 = off
    bool Any() const;           // mask != 0 || brightness != 1 || contrast != 1
};

void  ToggleFx(FxState& s, FxId id);
float StepLevel(float current, int direction);   // +-0.1, rounded to 0.1, clamped 0.2..3.0

// Removes every "// SURI-FX:<name> begin" .. "// SURI-FX:<name> end" block (both marker lines included).
// A marker word must be followed by whitespace or the end of the text ("beginX" is not a marker).
// A begin line that also says "+newline" marks a line break ApplyFx added; it is removed too, so
// ApplyFx followed by StripAllFx gives back the exact original text.
// A begin with no matching end is left untouched. Returns true if anything was removed.
bool  StripAllFx(std::string& shaderText);

// StripAllFx, then inserts the blocks for s (inserted lines use the text's line ending: CRLF if
// it has any). Geometry blocks go right after the '{', colour and level blocks right before the '}'. Returns false (text unchanged) if there is no
// "shader_body" followed by a '{' with a matching '}' (braces inside // and /* */ comments ignored).
bool  ApplyFx(std::string& shaderText, const FxState& s);

const char* FxName(FxId id);                     // marker name: "mirror-x", "kaleido", ...
