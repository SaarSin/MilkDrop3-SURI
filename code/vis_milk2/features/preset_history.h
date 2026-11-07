// preset_history.h
//
// SURI: rules for the circular preset history (CPlugin::m_presetHistory). Pure code;
// unit-tested in code/tests/test_preset_history.cpp.
//
// pos is the slot of the preset on screen; fwd and back are the fences (fwd == back: empty).
// Callers advance pos to fwd before LoadPreset, which writes the file name there on success.

#pragma once

// Where pos should be after LoadPreset could not find the file. If pos had just been moved
// to the forward fence (nothing was written there), it steps back, so the history keeps no
// empty slot. Otherwise (empty history, or walking back/forward through it) it stays.
int PosAfterFailedLoad(int pos, int fwd, int back, int len);

// Whether "next preset" may walk forward through the history: there is a next slot before
// the forward fence and it holds a file name.
bool CanStepForward(int next, int fwd, int back, bool nextSlotEmpty);
