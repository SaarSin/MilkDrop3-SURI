// drop_import.h
//
// SURI: pure core of drag & drop import (classify dropped paths, plan copies into the
// preset library without overwriting, find textures a preset refers to).

#pragma once

#include <functional>
#include <string>
#include <vector>

enum class DropKind { Folder, Preset, Image, Unsupported };

// Extension is case-insensitive; images: png jpg jpeg gif bmp tga dds; presets: milk.
DropKind ClassifyDropPath(const std::wstring& path, bool isDirectory);

struct LibraryEntry { std::wstring name; unsigned long long size; };   // library root, name without path
struct DroppedFile  { std::wstring sourcePath; unsigned long long size; };

struct ImportItem {
    std::wstring sourcePath;
    std::wstring name;        // file name of sourcePath
    std::wstring targetName;  // == name when existing or no clash; numbered otherwise
    bool isNew;
};

// One item per file, in input order. Existing = a library name equal (case-insensitive) and
// same size. New names are unique against the library AND against earlier new items in the batch.
std::vector<ImportItem> PlanImport(const std::vector<DroppedFile>& files,
                                   const std::vector<LibraryEntry>& library);

// "sampler_<x>" roots in a preset's text: 2-letter prefix + '_' (fw_, fc_, pw_, pc_ ...) stripped,
// main / blur1..3 / noise* / noisevol* / rand* dropped, unique case-insensitive, first-seen order.
std::vector<std::wstring> TextureRoots(const std::string& presetText);

// The preset loader's texture search, exactly: for each extension in order, first
// <texturesDir><root>.<ext>, then <presetDir><root>.<ext>; the first hit wins. Both dirs end with
// '\'. fileSize(path) returns the file's size, or -1 when there is nothing at path. Returns false
// when nothing resolves.
struct TextureHit {
    std::wstring path;
    std::wstring ext;
    long long size;
    bool inTexturesDir;
};
bool ResolveTexture(const std::wstring& root, const std::wstring& texturesDir, const std::wstring& presetDir,
                    const std::vector<std::wstring>& exts,
                    const std::function<long long(const std::wstring&)>& fileSize, TextureHit* hit);

// What to do with a texture a dropped preset brings (<root>.<srcExt>, srcSize bytes): copy it only
// when the loader would find nothing for root; when it would find textures\<root>.<srcExt> with
// the same size, it is the same file; anything else that resolves is kept (and would shadow it).
enum class TextureCopy { Copy, SameFile, Kept };
TextureCopy DecideTextureCopy(const std::wstring& root, const std::wstring& srcExt, long long srcSize,
                              const std::wstring& texturesDir, const std::wstring& presetDir,
                              const std::vector<std::wstring>& exts,
                              const std::function<long long(const std::wstring&)>& fileSize);

// false while any screen/prompt/waitstring is open.
bool CanAcceptDrop(bool regularUiMode);
