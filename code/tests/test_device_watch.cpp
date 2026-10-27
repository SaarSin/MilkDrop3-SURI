// test_device_watch.cpp

#include "mini_test.h"
#include <audioclient.h>
#include "../audio/device-watch.h"

static AudioSnapshot SpeakersSnapshot() {
    AudioSnapshot s;
    s.deviceId = L"{0.0.0.00000000}.{speakers}";
    s.state = DEVICE_STATE_ACTIVE;
    s.formatTag = WAVE_FORMAT_IEEE_FLOAT;
    s.sampleRate = 48000;
    s.followDefault = true;
    return s;
}

TEST(TestAudioRecovery_DefaultChanged) {
    AudioSnapshot cur = SpeakersSnapshot();
    AudioSnapshot now = cur;
    now.deviceId = L"{0.0.0.00000000}.{headphones}";
    CHECK(DecideAudioAction(cur, now, S_FALSE) == AudioAction::SwitchToDefault);
}

TEST(TestAudioRecovery_DeviceInvalidated) {
    AudioSnapshot cur = SpeakersSnapshot();
    CHECK(DecideAudioAction(cur, cur, AUDCLNT_E_DEVICE_INVALIDATED) == AudioAction::SwitchToDefault);
}

TEST(TestAudioRecovery_FormatChanged) {
    AudioSnapshot cur = SpeakersSnapshot();
    AudioSnapshot now = cur;
    now.sampleRate = 44100;
    CHECK(DecideAudioAction(cur, now, S_FALSE) == AudioAction::Restart);
}

TEST(TestAudioRecovery_Unplugged) {
    AudioSnapshot cur = SpeakersSnapshot();
    AudioSnapshot now = cur;
    now.state = DEVICE_STATE_UNPLUGGED;
    CHECK(DecideAudioAction(cur, now, S_FALSE) == AudioAction::SwitchToDefault);
}

TEST(TestAudioRecovery_Steady) {
    AudioSnapshot cur = SpeakersSnapshot();
    CHECK(DecideAudioAction(cur, cur, S_FALSE) == AudioAction::None);
    CHECK(DecideAudioAction(cur, cur, S_OK) == AudioAction::None);
}

TEST(TestAudioRecovery_PinnedDevice) {
    AudioSnapshot cur = SpeakersSnapshot();
    cur.followDefault = false;
    AudioSnapshot now = cur;
    now.deviceId = L"{0.0.0.00000000}.{headphones}";
    CHECK(DecideAudioAction(cur, now, S_FALSE) == AudioAction::None);
}

// any other capture failure on a still-present device: start capturing again
TEST(TestAudioRecovery_OtherFailure) {
    AudioSnapshot cur = SpeakersSnapshot();
    CHECK(DecideAudioAction(cur, cur, E_FAIL) == AudioAction::Restart);
}
