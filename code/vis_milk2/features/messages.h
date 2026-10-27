// messages.h
//
// SURI: pure part of the custom messages / sprites feature: random message selection,
// sample milk2_msg.ini / milk2_img.ini templates and a UTF-16 ini writer.
// Unit-tested in code/tests/test_messages.cpp.

#pragma once

#include <string>

// Picks the (randomValue % n)-th (zero based) index whose hasText entry is true, where n is the
// number of true entries. Returns -1 when count <= 0 or no entry is true.
int PickRandomMessage(const bool* hasText, int count, int randomValue);

// Sample milk2_msg.ini: [font00], one English and one Hebrew message, with comments.
std::wstring MessagesIniTemplate();

// Sample milk2_img.ini: two sprites using textures that ship in content/textures.
std::wstring SpritesIniTemplate();

// Writes text as UTF-16LE with a BOM. Returns false when the file already exists or on failure.
bool WriteUtf16IniIfMissing(const wchar_t* path, const std::wstring& text);
