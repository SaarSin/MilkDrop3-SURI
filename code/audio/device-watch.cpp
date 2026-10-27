// device-watch.cpp

#include "device-watch.h"
#include <audioclient.h>

AudioAction DecideAudioAction(const AudioSnapshot& cur, const AudioSnapshot& now, HRESULT lastHr) {
    // the device we capture from is gone (unplugged, disabled, removed)
    if (now.state != DEVICE_STATE_ACTIVE) {
        return AudioAction::SwitchToDefault;
    }

    // the user picked another default output device
    if (cur.followDefault && now.deviceId != cur.deviceId) {
        return AudioAction::SwitchToDefault;
    }

    // the audio engine dropped our stream (device reset, exclusive-mode app, format change)
    if (AUDCLNT_E_DEVICE_INVALIDATED == lastHr) {
        return cur.followDefault ? AudioAction::SwitchToDefault : AudioAction::Restart;
    }

    // same device, new mix format: the stream must be created again
    if (now.formatTag != cur.formatTag || now.sampleRate != cur.sampleRate) {
        return AudioAction::Restart;
    }

    if (FAILED(lastHr)) {
        return AudioAction::Restart;
    }

    return AudioAction::None;
}

CDeviceWatch::CDeviceWatch()
: m_refs(1)
{
    hChangedEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
}

CDeviceWatch::~CDeviceWatch() {
    if (NULL != hChangedEvent) {
        CloseHandle(hChangedEvent);
    }
}

ULONG STDMETHODCALLTYPE CDeviceWatch::AddRef() {
    return InterlockedIncrement(&m_refs);
}

ULONG STDMETHODCALLTYPE CDeviceWatch::Release() {
    ULONG refs = InterlockedDecrement(&m_refs);
    if (0 == refs) {
        delete this;
    }
    return refs;
}

HRESULT STDMETHODCALLTYPE CDeviceWatch::QueryInterface(REFIID riid, void **ppv) {
    if (__uuidof(IUnknown) == riid || __uuidof(IMMNotificationClient) == riid) {
        *ppv = static_cast<IMMNotificationClient*>(this);
        AddRef();
        return S_OK;
    }
    *ppv = NULL;
    return E_NOINTERFACE;
}

HRESULT STDMETHODCALLTYPE CDeviceWatch::OnDefaultDeviceChanged(EDataFlow flow, ERole role, LPCWSTR) {
    if (eRender == flow && eConsole == role) {
        SetEvent(hChangedEvent);
    }
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CDeviceWatch::OnDeviceStateChanged(LPCWSTR, DWORD) {
    SetEvent(hChangedEvent);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CDeviceWatch::OnDeviceAdded(LPCWSTR) {
    SetEvent(hChangedEvent);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CDeviceWatch::OnDeviceRemoved(LPCWSTR) {
    SetEvent(hChangedEvent);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CDeviceWatch::OnPropertyValueChanged(LPCWSTR, const PROPERTYKEY key) {
    // property changes are frequent (volume, names, ...); only the mix format matters
    if (key.fmtid == PKEY_AudioEngine_DeviceFormat.fmtid && key.pid == PKEY_AudioEngine_DeviceFormat.pid) {
        SetEvent(hChangedEvent);
    }
    return S_OK;
}
