#include "resstrings.h"

extern HINSTANCE api_orig_hinstance;

static wchar_t buffer[4096];

wchar_t* LoadResString(int id, wchar_t* buffer, int len) {
    LoadStringW(api_orig_hinstance, id, buffer, len);
    return buffer;
}

wchar_t* LoadResString(int id) {
    LoadStringW(api_orig_hinstance, id, buffer, 4096);
    return buffer;
}
