// drop_import.cpp
//
// SURI: see drop_import.h.

#include "drop_import.h"
#include <windows.h>
#include <stdio.h>
#include <wchar.h>

static bool EqualNoCase(const std::wstring& a, const std::wstring& b) {
    return CompareStringOrdinal(a.c_str(), (int)a.size(), b.c_str(), (int)b.size(), TRUE) == CSTR_EQUAL;
}

static std::wstring LowerAscii(std::wstring s) {
    for (size_t i = 0; i < s.size(); i++)
        if (s[i] >= L'A' && s[i] <= L'Z') s[i] = (wchar_t)(s[i] - L'A' + L'a');
    return s;
}

static std::wstring FileNameOf(const std::wstring& path) {
    size_t p = path.find_last_of(L"\\/");
    return p == std::wstring::npos ? path : path.substr(p + 1);
}

DropKind ClassifyDropPath(const std::wstring& path, bool isDirectory) {
    if (isDirectory) return DropKind::Folder;
    std::wstring name = FileNameOf(path);
    size_t dot = name.find_last_of(L'.');
    if (dot == std::wstring::npos) return DropKind::Unsupported;
    std::wstring ext = LowerAscii(name.substr(dot + 1));
    if (ext == L"milk") return DropKind::Preset;
    static const wchar_t* const kImg[] = { L"png", L"jpg", L"jpeg", L"gif", L"bmp", L"tga", L"dds" };
    for (size_t i = 0; i < sizeof(kImg) / sizeof(kImg[0]); i++)
        if (ext == kImg[i]) return DropKind::Image;
    return DropKind::Unsupported;
}

static bool NameTaken(const std::wstring& n, const std::vector<LibraryEntry>& lib,
                      const std::vector<std::wstring>& planned) {
    for (size_t i = 0; i < lib.size(); i++) if (EqualNoCase(lib[i].name, n)) return true;
    for (size_t i = 0; i < planned.size(); i++) if (EqualNoCase(planned[i], n)) return true;
    return false;
}

std::vector<ImportItem> PlanImport(const std::vector<DroppedFile>& files,
                                   const std::vector<LibraryEntry>& library) {
    std::vector<ImportItem> out;
    std::vector<std::wstring> planned;   // target names of new items so far
    for (size_t i = 0; i < files.size(); i++) {
        ImportItem it;
        it.sourcePath = files[i].sourcePath;
        it.name = FileNameOf(files[i].sourcePath);
        it.targetName = it.name;
        it.isNew = true;
        bool exists = false;
        for (size_t k = 0; k < library.size(); k++)
            if (EqualNoCase(library[k].name, it.name) && library[k].size == files[i].size) { exists = true; break; }
        if (exists) {
            it.isNew = false;
        } else {
            if (NameTaken(it.targetName, library, planned)) {
                size_t dot = it.name.find_last_of(L'.');
                std::wstring stem = dot == std::wstring::npos ? it.name : it.name.substr(0, dot);
                std::wstring ext = dot == std::wstring::npos ? std::wstring() : it.name.substr(dot);
                for (int n = 2;; n++) {
                    wchar_t num[16];
                    swprintf(num, sizeof(num) / sizeof(num[0]), L"%d", n);
                    std::wstring cand = stem + L" (" + num + L")" + ext;
                    if (!NameTaken(cand, library, planned)) { it.targetName = cand; break; }
                }
            }
            planned.push_back(it.targetName);
        }
        out.push_back(it);
    }
    return out;
}

static bool IsIdent(char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_';
}

std::vector<std::wstring> TextureRoots(const std::string& t) {
    std::vector<std::wstring> out;
    const std::string key = "sampler_";
    size_t pos = 0;
    while ((pos = t.find(key, pos)) != std::string::npos) {
        size_t b = pos + key.size();
        pos = b;
        size_t e = b;
        while (e < t.size() && IsIdent(t[e])) e++;
        std::string id = t.substr(b, e - b);
        if (id.size() > 3 && id[2] == '_') id = id.substr(3);   // fw_, fc_, pw_, pc_ ...
        if (id.empty()) continue;
        std::string low;
        for (size_t i = 0; i < id.size(); i++)
            low += (char)((id[i] >= 'A' && id[i] <= 'Z') ? id[i] - 'A' + 'a' : id[i]);
        if (low == "main" || low == "blur1" || low == "blur2" || low == "blur3") continue;
        if (low.compare(0, 5, "noise") == 0 || low.compare(0, 4, "rand") == 0) continue;
        std::wstring w(id.begin(), id.end());
        bool dup = false;
        for (size_t i = 0; i < out.size(); i++) if (EqualNoCase(out[i], w)) { dup = true; break; }
        if (!dup) out.push_back(w);
    }
    return out;
}

bool ResolveTexture(const std::wstring& root, const std::wstring& texturesDir, const std::wstring& presetDir,
                    const std::vector<std::wstring>& exts,
                    const std::function<long long(const std::wstring&)>& fileSize, TextureHit* hit) {
    for (size_t z = 0; z < exts.size(); z++) {
        for (int pass = 0; pass < 2; pass++) {
            const std::wstring path = (pass == 0 ? texturesDir : presetDir) + root + L"." + exts[z];
            long long size = fileSize(path);
            if (size < 0) continue;
            hit->path = path;
            hit->ext = exts[z];
            hit->size = size;
            hit->inTexturesDir = (pass == 0);
            return true;
        }
    }
    return false;
}

TextureCopy DecideTextureCopy(const std::wstring& root, const std::wstring& srcExt, long long srcSize,
                              const std::wstring& texturesDir, const std::wstring& presetDir,
                              const std::vector<std::wstring>& exts,
                              const std::function<long long(const std::wstring&)>& fileSize) {
    TextureHit hit;
    if (!ResolveTexture(root, texturesDir, presetDir, exts, fileSize, &hit))
        return TextureCopy::Copy;
    if (hit.inTexturesDir && EqualNoCase(hit.ext, srcExt) && hit.size == srcSize)
        return TextureCopy::SameFile;
    return TextureCopy::Kept;
}

bool CanAcceptDrop(bool regularUiMode) { return regularUiMode; }
