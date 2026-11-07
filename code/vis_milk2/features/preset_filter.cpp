// preset_filter.cpp
//
// SURI: see preset_filter.h.

#include "preset_filter.h"
#include <wctype.h>

// Glob match of the whole of 's' against 'p' ('*' and '?'), ignoring case. Iterative, with
// one backtrack point for the last '*', so it stays linear-ish on long names.
static bool GlobMatch(const wchar_t* s, const wchar_t* p)
{
    const wchar_t* star = nullptr;   // last '*' seen in p
    const wchar_t* resume = nullptr; // where in s that '*' is currently matched up to
    while (*s)
    {
        if (*p == L'*')
        {
            star = p++;
            resume = s;
        }
        else if (*p && (*p == L'?' || towlower(*p) == towlower(*s)))
        {
            p++;
            s++;
        }
        else if (star)
        {
            p = star + 1;
            s = ++resume;
        }
        else
            return false;
    }
    while (*p == L'*')
        p++;
    return *p == 0;
}

bool PresetNameMatches(const wchar_t* name, const wchar_t* filter)
{
    if (!filter || !*filter)
        return true;
    if (!name)
        return false;

    // "*" + filter + "*"; the filter is short (the Load menu caps it), longer ones are cut.
    wchar_t pat[260];
    int n = 0;
    pat[n++] = L'*';
    for (const wchar_t* f = filter; *f && n < 258; f++)
        pat[n++] = *f;
    pat[n++] = L'*';
    pat[n] = 0;
    return GlobMatch(name, pat);
}
