// title_anim.h
//
// SURI: timeline and fitting of the song-title animation. Pure code (no D3D);
// unit-tested in code/tests/test_title_anim.cpp.

#pragma once

// Where a song title is on its timeline. 'anim' is the classic fly-in. With hold == 0 and
// fade == 0 it is classic MilkDrop: when the fly-in ends the title is burned into the image
// once (burnIn) and is gone. Otherwise it stays on top for 'hold' seconds, then fades out
// over 'fade' seconds (alpha 1 -> 0) and is never burned in.
struct TitlePhase {
    bool  visible;    // draw it this frame
    bool  burnIn;     // classic end: burn into the image, then switch off
    float progress;   // fly-in progress for ShowSongTitleAnim, < 1 while it is drawn on top
    float alpha;      // 1 = full, 0 = gone
};
TitlePhase SongTitlePhase(float elapsed, float anim, float hold, float fade);

// Seconds from launch until the title is gone.
float SongTitleEnd(float anim, float hold, float fade);

// Font scale that makes a line textWidth pixels wide fit into avail pixels: 1 if it fits,
// never below minScale (the caller then cuts the text).
float FitTitleScale(int textWidth, int avail, float minScale);
