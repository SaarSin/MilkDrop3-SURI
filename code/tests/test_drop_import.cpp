// test_drop_import.cpp
//
// SURI: tests for features/drop_import.* (drag & drop import core).

#include "mini_test.h"
#include <string>
#include <utility>
#include <vector>
#include "..\vis_milk2\features\drop_import.h"

TEST(TestDrop_Classify) {
    CHECK(ClassifyDropPath(L"C:\\p", true) == DropKind::Folder);
    CHECK(ClassifyDropPath(L"C:\\p\\a.milk", false) == DropKind::Preset);
    CHECK(ClassifyDropPath(L"C:\\p\\A.MILK", false) == DropKind::Preset);
    CHECK(ClassifyDropPath(L"C:\\p\\a.PNG", false) == DropKind::Image);
    CHECK(ClassifyDropPath(L"C:\\p\\a.jpeg", false) == DropKind::Image);
    CHECK(ClassifyDropPath(L"C:\\p\\a.txt", false) == DropKind::Unsupported);
    CHECK(ClassifyDropPath(L"C:\\p\\milk", false) == DropKind::Unsupported);
}
TEST(TestDrop_ExistsOrNew) {
    std::vector<LibraryEntry> lib = { { L"Galaxy.milk", 100 }, { L"Pulse.milk", 50 } };
    auto r = PlanImport({ { L"D:\\x\\galaxy.milk", 100 }, { L"D:\\x\\Pulse.milk", 51 }, { L"D:\\x\\Neon.milk", 9 } }, lib);
    CHECK(r.size() == 3);
    CHECK(!r[0].isNew);                                   // same name (case-insensitive) + same size
    CHECK(r[1].isNew && r[1].targetName == L"Pulse (2).milk");
    CHECK(r[2].isNew && r[2].targetName == L"Neon.milk");
}
TEST(TestDrop_NumberingSkipsTaken) {
    std::vector<LibraryEntry> lib = { { L"A.milk", 1 }, { L"A (2).milk", 2 } };
    auto r = PlanImport({ { L"D:\\A.milk", 3 } }, lib);
    CHECK(r[0].targetName == L"A (3).milk");
}
TEST(TestDrop_BatchCollision) {
    std::vector<LibraryEntry> lib = { { L"X.milk", 1 } };
    auto r = PlanImport({ { L"D:\\a\\X.milk", 2 }, { L"D:\\b\\X.milk", 3 }, { L"D:\\a\\Y.milk", 1 }, { L"D:\\b\\Y.milk", 2 } }, lib);
    CHECK(r[0].targetName == L"X (2).milk");
    CHECK(r[1].targetName == L"X (3).milk");
    CHECK(r[2].targetName == L"Y.milk");
    CHECK(r[3].targetName == L"Y (2).milk");
}
TEST(TestDrop_TextureRefs) {
    std::string t = "comp_1=`sampler sampler_rand00;\ncomp_2=`sampler sampler_fw_clouds2;\n"
                    "comp_3=`ret = tex2D(sampler_main, uv) + tex2D(sampler_blur1, uv) + tex2D(sampler_noise_lq, uv);\n"
                    "warp_1=`sampler sampler_Worms; sampler sampler_worms; tex3D(sampler_noisevol_hq, x);\n";
    auto r = TextureRoots(t);
    CHECK(r.size() == 2);
    CHECK(r[0] == L"clouds2");
    CHECK(r[1] == L"Worms");
}
// a fake file system: path -> size (exact spelling)
static long long FakeSize(const std::vector<std::pair<std::wstring, long long>>& fs, const std::wstring& p) {
    for (size_t i = 0; i < fs.size(); i++) if (fs[i].first == p) return fs[i].second;
    return -1;
}
static const std::vector<std::wstring> kExts = { L"jpg", L"dds", L"png", L"tga", L"bmp", L"dib" };
static const std::wstring kTex = L"C:\\md\\textures\\", kPre = L"C:\\md\\presets\\";

TEST(TestDrop_TextureResolveOrder) {
    // jpg in the preset dir wins over png in textures\: extension order first, then the folder
    std::vector<std::pair<std::wstring, long long>> fs = { { kTex + L"clouds.png", 5 }, { kPre + L"clouds.jpg", 7 } };
    auto f = [&](const std::wstring& p) { return FakeSize(fs, p); };
    TextureHit h;
    CHECK(ResolveTexture(L"clouds", kTex, kPre, kExts, f, &h));
    CHECK(h.path == kPre + L"clouds.jpg" && h.ext == L"jpg" && h.size == 7 && !h.inTexturesDir);
    CHECK(!ResolveTexture(L"worms", kTex, kPre, kExts, f, &h));
}
TEST(TestDrop_TextureOtherExtInTextures) {
    std::vector<std::pair<std::wstring, long long>> fs = { { kTex + L"clouds.png", 5 } };
    auto f = [&](const std::wstring& p) { return FakeSize(fs, p); };
    CHECK(DecideTextureCopy(L"clouds", L"jpg", 5, kTex, kPre, kExts, f) == TextureCopy::Kept);
}
TEST(TestDrop_TextureSameNameInPresetDir) {
    std::vector<std::pair<std::wstring, long long>> fs = { { kPre + L"clouds.jpg", 9 } };
    auto f = [&](const std::wstring& p) { return FakeSize(fs, p); };
    CHECK(DecideTextureCopy(L"clouds", L"jpg", 9, kTex, kPre, kExts, f) == TextureCopy::Kept);
}
TEST(TestDrop_TextureSameFile) {
    std::vector<std::pair<std::wstring, long long>> fs = { { kTex + L"clouds.jpg", 9 } };
    auto f = [&](const std::wstring& p) { return FakeSize(fs, p); };
    CHECK(DecideTextureCopy(L"clouds", L"JPG", 9, kTex, kPre, kExts, f) == TextureCopy::SameFile);
    CHECK(DecideTextureCopy(L"clouds", L"jpg", 10, kTex, kPre, kExts, f) == TextureCopy::Kept);   // other size
}
TEST(TestDrop_TextureNothingFound) {
    std::vector<std::pair<std::wstring, long long>> fs = { { kTex + L"other.jpg", 9 } };
    auto f = [&](const std::wstring& p) { return FakeSize(fs, p); };
    CHECK(DecideTextureCopy(L"clouds", L"jpg", 9, kTex, kPre, kExts, f) == TextureCopy::Copy);
}
TEST(TestDrop_CanAccept) { CHECK(CanAcceptDrop(true)); CHECK(!CanAcceptDrop(false)); }
