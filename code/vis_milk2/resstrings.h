#pragma once

#include <windows.h>

// Loads a string from the exe's string table (plugin.rc).
// The one-argument form returns a shared static buffer: copy the result before the next call.
extern wchar_t* LoadResString(int id, wchar_t* buffer, int len);
extern wchar_t* LoadResString(int id);
