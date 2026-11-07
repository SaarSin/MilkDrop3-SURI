// preset_history.cpp - see preset_history.h

#include "preset_history.h"

int PosAfterFailedLoad(int pos, int fwd, int back, int len)
{
    if (len <= 0 || fwd == back || pos != fwd)
        return pos;
    return (pos - 1 + len) % len;
}

bool CanStepForward(int next, int fwd, int back, bool nextSlotEmpty)
{
    return fwd != back && next != fwd && !nextSlotEmpty;
}
