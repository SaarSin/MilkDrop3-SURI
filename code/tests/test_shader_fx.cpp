// test_shader_fx.cpp
//
// SURI: tests for the shader effect injection core (features/shader_fx.*): toggling,
// marker blocks, idempotent apply, strip before save, level stepping, comment-aware braces.

#include "mini_test.h"
#include "..\vis_milk2\features\shader_fx.h"

static const char* kBody = "sampler sampler_x;\nshader_body\n{\n    ret = tex2D(sampler_main, uv).xyz;\n}\n";

TEST(TestShaderFx_ToggleTwiceRestores) {
    std::string t = kBody; FxState s;
    ToggleFx(s, FxId::Invert); CHECK(ApplyFx(t, s)); CHECK(t != kBody);
    ToggleFx(s, FxId::Invert); CHECK(ApplyFx(t, s)); CHECK(t == kBody);
}
TEST(TestShaderFx_NoShaderBody_ReturnsFalse) {
    std::string t = "float4 x;"; FxState s; ToggleFx(s, FxId::Mono);
    CHECK(!ApplyFx(t, s)); CHECK(t == "float4 x;");
}
TEST(TestShaderFx_TwoEffectsIndependent) {
    std::string t = kBody; FxState s;
    ToggleFx(s, FxId::MirrorX); ToggleFx(s, FxId::Invert); ApplyFx(t, s);
    CHECK(t.find("SURI-FX:mirror-x begin") < t.find("ret = tex2D"));   // geometry before user code
    CHECK(t.find("SURI-FX:invert begin")   > t.find("ret = tex2D"));   // colour after it
    ToggleFx(s, FxId::MirrorX); ApplyFx(t, s);
    CHECK(t.find("mirror-x") == std::string::npos); CHECK(t.find("SURI-FX:invert begin") != std::string::npos);
}
TEST(TestShaderFx_ApplyIsIdempotent) {
    std::string t = kBody; FxState s; ToggleFx(s, FxId::Kaleido); s.brightness = 1.5f;
    ApplyFx(t, s); std::string once = t; ApplyFx(t, s); CHECK(t == once);
}
TEST(TestShaderFx_StripAllBeforeSave) {
    std::string t = kBody; FxState s; ToggleFx(s, FxId::Ripple); ToggleFx(s, FxId::Posterize); s.contrast = 0.4f;
    ApplyFx(t, s); CHECK(StripAllFx(t)); CHECK(t == kBody); CHECK(!StripAllFx(t));
}
TEST(TestShaderFx_BrightnessClamp) {
    CHECK(StepLevel(1.0f, +1) == 1.1f); CHECK(StepLevel(0.2f, -1) == 0.2f);
    CHECK(StepLevel(3.0f, +1) == 3.0f); CHECK(StepLevel(0.95f, -1) == 0.9f);
}
TEST(TestShaderFx_BraceInComment) {
    std::string t = "shader_body\n{\n    // } not the end\n    ret = 1;\n}\n";
    FxState s; ToggleFx(s, FxId::Invert); CHECK(ApplyFx(t, s));
    CHECK(t.find("SURI-FX:invert begin") > t.find("ret = 1;"));
}
TEST(TestShaderFx_BrokenMarkerLeftAlone) {
    std::string t = std::string(kBody) + "// SURI-FX:invert begin\n";
    CHECK(!StripAllFx(t)); CHECK(t.find("SURI-FX:invert begin") != std::string::npos);
}
TEST(TestShaderFx_FunctionsBeforeBody) {
    std::string t = "float3 f(float3 c) { return c; }\nshader_body\n{\n    ret = f(1);\n}\n";
    FxState s; ToggleFx(s, FxId::SwapAxes); CHECK(ApplyFx(t, s));
    CHECK(t.find("SURI-FX:swap-axes begin") > t.find("shader_body"));
}
TEST(TestShaderFx_StrayBeginBeforeBody) {
    std::string t = "// SURI-FX:invert begin\nfloat3 keep;\nshader_body\n{\n    ret = tex2D(sampler_main, uv).xyz;\n}\n";
    FxState s; ToggleFx(s, FxId::Invert);
    CHECK(ApplyFx(t, s)); CHECK(ApplyFx(t, s));
    CHECK(t.find("// SURI-FX:invert begin\nfloat3 keep;") == 0);
    CHECK(t.find("shader_body") != std::string::npos);
    CHECK(t.find("ret = tex2D") != std::string::npos);
    CHECK(t.find("ret = 1.0 - ret;") != std::string::npos);
}

// SURI: cleanup round (item 8)
TEST(TestShaderFx_CrlfRoundTrip) {
    const std::string orig = "sampler sampler_x;\r\nshader_body\r\n{\r\n    ret = tex2D(sampler_main, uv).xyz;\r\n}\r\n";
    std::string t = orig; FxState s;
    ToggleFx(s, FxId::MirrorX); ToggleFx(s, FxId::Invert); s.brightness = 1.5f;
    CHECK(ApplyFx(t, s));
    // every inserted line uses the text's own CRLF line ending
    for (size_t i = 0; i < t.size(); i++) if (t[i] == '\n') CHECK(i > 0 && t[i - 1] == '\r');
    CHECK(StripAllFx(t)); CHECK(t == orig);
    t = orig; ApplyFx(t, s); s = FxState(); CHECK(ApplyFx(t, s)); CHECK(t == orig);
}
TEST(TestShaderFx_OneLineBodyRoundTripExact) {
    const std::string orig = "shader_body{ret=1;}";
    std::string t = orig; FxState s;
    ToggleFx(s, FxId::SwapAxes); ToggleFx(s, FxId::Mono);
    CHECK(ApplyFx(t, s));
    CHECK(t.find("SURI-FX:swap-axes begin") < t.find("ret=1;"));
    CHECK(t.find("SURI-FX:mono begin") > t.find("ret=1;"));
    CHECK(t.find("SURI-FX:mono end") < t.rfind('}'));
    std::string stripped = t; CHECK(StripAllFx(stripped)); CHECK(stripped == orig);
    s = FxState(); CHECK(ApplyFx(t, s)); CHECK(t == orig);
}
TEST(TestShaderFx_LevelsTextAndOrder) {
    std::string t = kBody; FxState s; ToggleFx(s, FxId::HueSpin); s.brightness = 1.5f; s.contrast = 0.4f;
    CHECK(ApplyFx(t, s));
    CHECK(t.find("ret *= 1.50;") != std::string::npos);
    CHECK(t.find("ret = (ret - 0.5) * 0.40 + 0.5;") != std::string::npos);
    size_t hue = t.find("SURI-FX:hue-spin begin"), br = t.find("SURI-FX:brightness begin"), co = t.find("SURI-FX:contrast begin");
    CHECK(t.find("ret = tex2D") < hue); CHECK(hue < br); CHECK(br < co);
}
TEST(TestShaderFx_BlockCommentBraceInBody) {
    std::string t = "shader_body\n{\n    /* } */\n    ret = 1;\n}\n";
    FxState s; ToggleFx(s, FxId::Invert); CHECK(ApplyFx(t, s));
    CHECK(t.find("SURI-FX:invert begin") > t.find("ret = 1;"));
}
TEST(TestShaderFx_IdempotentFromOtherState) {
    FxState a; ToggleFx(a, FxId::Kaleido); ToggleFx(a, FxId::Posterize); a.contrast = 2.0f;
    FxState b; ToggleFx(b, FxId::Ripple); ToggleFx(b, FxId::Invert); b.brightness = 0.5f;
    std::string direct = kBody; ApplyFx(direct, b);
    std::string viaA = kBody; ApplyFx(viaA, a); ApplyFx(viaA, b);
    CHECK(viaA == direct);
    std::string oneLine = "shader_body{ret=1;}", d2 = oneLine;
    ApplyFx(d2, b); ApplyFx(oneLine, a); ApplyFx(oneLine, b); CHECK(oneLine == d2);
}
TEST(TestShaderFx_MarkerNeedsWordEnd) {
    // "beginX" / "endX" are not markers
    std::string t = std::string(kBody) + "// SURI-FX:invert beginX\nkeep1;\n// SURI-FX:invert end\n";
    CHECK(!StripAllFx(t)); CHECK(t.find("keep1;") != std::string::npos);
    std::string u = std::string(kBody) + "// SURI-FX:invert begin\nkeep2;\n// SURI-FX:invert endX\n";
    CHECK(!StripAllFx(u)); CHECK(u.find("keep2;") != std::string::npos);
    std::string v = std::string(kBody) + "// SURI-FX:invert begin\t\ngone;\n// SURI-FX:invert end";
    CHECK(StripAllFx(v)); CHECK(v == kBody);
}

// SURI: final fix item 9 - each effect sits in its own { } scope, so its locals cannot
// clash with the preset's own variables
TEST(TestShaderFx_EachBlockScoped) {
    std::string t = kBody; FxState s; ToggleFx(s, FxId::Kaleido); ToggleFx(s, FxId::HueSpin); s.brightness = 1.5f;
    CHECK(ApplyFx(t, s));
    CHECK(t.find("SURI-FX:kaleido begin\n    { float2 kd = uv - 0.5;") != std::string::npos);
    CHECK(t.find("uv = 0.5 + kr * float2(cos(ka), sin(ka)); }\n    // SURI-FX:kaleido end") != std::string::npos);
    CHECK(t.find("    { float3 hk = 0.57735;") != std::string::npos);
    CHECK(t.find("    { ret *= 1.50; }\n") != std::string::npos);
}
TEST(TestShaderFx_StripsOldUnscopedBlocks) {
    // blocks written before the { } scoping (presets saved mid-session, older builds)
    std::string t = "sampler sampler_x;\nshader_body\n{\n"
                    "    // SURI-FX:mirror-x begin\n    uv.x = 0.5 - abs(uv.x - 0.5);\n    // SURI-FX:mirror-x end\n"
                    "    ret = tex2D(sampler_main, uv).xyz;\n"
                    "    // SURI-FX:invert begin\n    ret = 1.0 - ret;\n    // SURI-FX:invert end\n}\n";
    CHECK(StripAllFx(t)); CHECK(t == kBody);
    std::string u = "sampler sampler_x;\nshader_body\n{\n"
                    "    // SURI-FX:invert begin\n    ret = 1.0 - ret;\n    // SURI-FX:invert end\n"
                    "    ret = tex2D(sampler_main, uv).xyz;\n}\n";
    FxState s; ToggleFx(s, FxId::Mono); CHECK(ApplyFx(u, s));
    CHECK(u.find("invert") == std::string::npos); CHECK(u.find("{ ret = dot(ret") != std::string::npos);
}
