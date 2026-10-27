// device-watch.h
//
// SURI: audio device recovery. The capture thread keeps a snapshot of the device it
// captures from; when something changes (device event or a capture error) it takes a
// new snapshot and DecideAudioAction() says whether capture must be restarted.

#pragma once

#include <windows.h>
#include <mmdeviceapi.h>
#include <string>

enum class AudioAction { None, Restart, SwitchToDefault };

struct AudioSnapshot {
    std::wstring deviceId;  // cur: the captured device; now: the current default render device
    DWORD state;            // DEVICE_STATE_* of the captured device
    WORD formatTag;         // mix format of the captured device
    DWORD sampleRate;
    bool followDefault;     // follow the Windows default device (false = pinned device)
};

// Pure decision, no Win32 calls. lastHr is the result of the capture run that just
// ended (S_OK / S_FALSE when it ended because of a device notification).
AudioAction DecideAudioAction(const AudioSnapshot& cur, const AudioSnapshot& now, HRESULT lastHr);

// Signals hChangedEvent when the default render device, a device state or a device
// format changes. Callbacks run on an MMDevAPI thread, so they only set the event.
class CDeviceWatch : public IMMNotificationClient {
public:
    HANDLE hChangedEvent;

    CDeviceWatch();
    ~CDeviceWatch();

    // IUnknown
    ULONG STDMETHODCALLTYPE AddRef() override;
    ULONG STDMETHODCALLTYPE Release() override;
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppv) override;

    // IMMNotificationClient
    HRESULT STDMETHODCALLTYPE OnDefaultDeviceChanged(EDataFlow flow, ERole role, LPCWSTR pwstrDefaultDeviceId) override;
    HRESULT STDMETHODCALLTYPE OnDeviceStateChanged(LPCWSTR pwstrDeviceId, DWORD dwNewState) override;
    HRESULT STDMETHODCALLTYPE OnDeviceAdded(LPCWSTR pwstrDeviceId) override;
    HRESULT STDMETHODCALLTYPE OnDeviceRemoved(LPCWSTR pwstrDeviceId) override;
    HRESULT STDMETHODCALLTYPE OnPropertyValueChanged(LPCWSTR pwstrDeviceId, const PROPERTYKEY key) override;

private:
    LONG m_refs;
};
