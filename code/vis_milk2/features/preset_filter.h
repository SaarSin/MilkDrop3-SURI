// preset_filter.h
//
// SURI: type-to-filter for the preset list (L). Pure code (no D3D);
// unit-tested in code/tests/test_preset_filter.cpp.

#pragma once

#include <stddef.h>

// True if 'name' matches what the user typed. The text matches anywhere in the name, like
// "*text*", ignoring case; '*' inside it stands for any run of characters and '?' for one
// character. An empty filter matches everything.
bool PresetNameMatches(const wchar_t* name, const wchar_t* filter);

// Turns pasted clipboard text into a search: the first non-blank line, without control
// characters, surrounding spaces or a ".milk" extension, cut to outLen-1 characters.
// 'out' is always terminated; null or blank text gives "".
void PresetSearchFromPaste(const wchar_t* text, wchar_t* out, size_t outLen);

// Where the highlight goes after the search changes. visible[i] says whether entry i is shown;
// entries 0..nDirs-1 are folders, parentIdx is "[ .. ]" (-1 if none). The highlight stays on a
// shown entry other than "[ .. ]"; otherwise it moves to the first shown preset, else the first
// shown folder, else "[ .. ]". Returns -1 when nothing is shown.
int PresetSearchHighlight(int cur, const bool* visible, int n, int nDirs, int parentIdx);
