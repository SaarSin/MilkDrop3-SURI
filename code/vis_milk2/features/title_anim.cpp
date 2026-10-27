// title_anim.cpp - see title_anim.h

#include "title_anim.h"
#include <math.h>

static const float kLastDrawnProgress = 0.9999f;   // progress 1 means "burn in" to ShowSongTitleAnim

TitlePhase SongTitlePhase(float elapsed, float anim, float hold, float fade)
{
    TitlePhase p = { false, false, 0.0f, 1.0f };
    if (elapsed < 0)
        return p;
    if (anim < 0)
        anim = 0;   // bad ini value: treat the fly-in as already done

    if (elapsed < anim)
    {
        p.visible = true;
        p.progress = elapsed / anim;
        if (p.progress > kLastDrawnProgress)
            p.progress = kLastDrawnProgress;
        return p;
    }

    if (hold <= 0 && fade <= 0)
    {
        p.visible = true;
        p.burnIn = true;
        p.progress = 1.0f;
        return p;
    }

    float t = elapsed - anim;
    if (t < hold)
    {
        p.visible = true;
        p.progress = kLastDrawnProgress;
        return p;
    }

    t -= hold;
    if (fade <= 0 || t >= fade)
        return p;

    // cosine ease: starts and ends softly
    p.visible = true;
    p.progress = kLastDrawnProgress;
    p.alpha = 0.5f + 0.5f * cosf(3.14159265f * t / fade);
    return p;
}

float SongTitleEnd(float anim, float hold, float fade)
{
    if (hold <= 0 && fade <= 0)
        return anim;
    return anim + (hold > 0 ? hold : 0) + (fade > 0 ? fade : 0);
}

float FitTitleScale(int textWidth, int avail, float minScale)
{
    if (textWidth <= avail || textWidth <= 0)
        return 1.0f;
    float s = avail / (float)textWidth;
    return s < minScale ? minScale : s;
}
