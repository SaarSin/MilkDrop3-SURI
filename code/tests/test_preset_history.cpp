#include "mini_test.h"
#include "../vis_milk2/features/preset_history.h"

// Ring of 66 like PRESET_HIST_LEN. After a successful load at slot 4 the forward fence is 5.

TEST(TestPresetHistory_FailedLoadAtFrontierStepsBack) {
    // the caller advanced pos to the forward fence, then the file was not found
    CHECK(PosAfterFailedLoad(5, 5, 0, 66) == 4);
    // wraps around the ring
    CHECK(PosAfterFailedLoad(0, 0, 10, 66) == 65);
}

TEST(TestPresetHistory_FailedLoadWithEmptyHistoryStays) {
    // first load ever: nothing was advanced, both fences equal pos
    CHECK(PosAfterFailedLoad(0, 0, 0, 66) == 0);
}

TEST(TestPresetHistory_FailedLoadWhileRetracingStays) {
    // walking back/forward through history to a file that was deleted: keep the place
    CHECK(PosAfterFailedLoad(2, 5, 0, 66) == 2);
}

TEST(TestPresetHistory_CanStepForwardSkipsEmptySlot) {
    CHECK(CanStepForward(3, 5, 0, false));
    CHECK(!CanStepForward(3, 5, 0, true));    // the next slot holds no file name
    CHECK(!CanStepForward(5, 5, 0, false));   // next is the forward fence
    CHECK(!CanStepForward(0, 0, 0, false));   // empty history
}
