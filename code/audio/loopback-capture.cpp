// loopback-capture.cpp

#include "common.h"

#define CAPTURE_MAX_ATTEMPTS 5       // failed starts in a row before the user is told
#define CAPTURE_RETRY_DELAY_MS 500

HRESULT LoopbackCapture(
    IMMDevice *pMMDevice,
    HANDLE hStartedEvent,
    HANDLE hStopEvent,
    HANDLE hChangedEvent,
    PUINT32 pnFrames,
    bool *pbStarted
);

static const wchar_t *AudioActionName(AudioAction action) {
    switch (action) {
        case AudioAction::Restart: return L"Restart";
        case AudioAction::SwitchToDefault: return L"SwitchToDefault";
        default: return L"None";
    }
}

// SURI: fills the state and mix format of pMMDevice; deviceId is left to the caller
static void SnapshotDevice(IMMDevice *pMMDevice, AudioSnapshot *pSnap) {
    pSnap->state = DEVICE_STATE_NOTPRESENT;
    pSnap->formatTag = 0;
    pSnap->sampleRate = 0;
    if (FAILED(pMMDevice->GetState(&pSnap->state)) || DEVICE_STATE_ACTIVE != pSnap->state) {
        return;
    }

    IAudioClient *pAudioClient = NULL;
    if (FAILED(pMMDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, (void**)&pAudioClient))) {
        return;
    }
    ReleaseOnExit releaseAudioClient(pAudioClient);

    WAVEFORMATEX *pwfx = NULL;
    if (SUCCEEDED(pAudioClient->GetMixFormat(&pwfx))) {
        pSnap->formatTag = pwfx->wFormatTag;
        pSnap->sampleRate = pwfx->nSamplesPerSec;
        CoTaskMemFree(pwfx);
    }
}

static std::wstring GetDeviceId(IMMDevice *pMMDevice) {
    std::wstring id;
    LPWSTR pwszId = NULL;
    if (SUCCEEDED(pMMDevice->GetId(&pwszId))) {
        id = pwszId;
        CoTaskMemFree(pwszId);
    }
    return id;
}

static std::wstring GetDefaultDeviceId(IMMDeviceEnumerator *pEnumerator) {
    std::wstring id;
    IMMDevice *pDefault = NULL;
    if (SUCCEEDED(pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pDefault))) {
        id = GetDeviceId(pDefault);
        pDefault->Release();
    }
    return id;
}

static DWORD WINAPI AudioFailureMessageThread(LPVOID pContext) {
    HRESULT hr = (HRESULT)(LONG_PTR)pContext;
    wchar_t msg[512];
    swprintf_s(msg, L"MilkDrop3 could not capture sound from your playback device.\n\n"
        L"The visuals keep running, but they will not react to the music. "
        L"Sound capture starts again by itself when a playback device becomes available "
        L"(for example when you plug in your headphones or pick another output device in Windows).\n\n"
        L"Error code: 0x%08x (details in MilkDrop3.log)", hr);
    MessageBoxW(NULL, msg, L"MilkDrop3 - no audio", MB_OK | MB_ICONWARNING | MB_SETFOREGROUND | MB_TOPMOST);
    return 0;
}

// SURI: tell the user once, without blocking the capture thread (it must keep
// answering the stop event while the message box is open)
static void ReportAudioFailure(HRESULT hr) {
    HANDLE hThread = CreateThread(NULL, 0, AudioFailureMessageThread, (LPVOID)(LONG_PTR)hr, 0, NULL);
    if (NULL != hThread) {
        CloseHandle(hThread);
    }
}

// SURI: capture from the default render device and keep doing so across device
// changes. Each capture run ends on stop, on a device notification (S_FALSE) or on
// an error; DecideAudioAction() then picks what to capture from next.
DWORD WINAPI LoopbackCaptureThreadFunction(LPVOID pContext) {
    LoopbackCaptureThreadFunctionArguments *pArgs =
        (LoopbackCaptureThreadFunctionArguments*)pContext;

    pArgs->hr = CoInitialize(NULL);
    if (FAILED(pArgs->hr)) {
        ERR(L"CoInitialize failed: hr = 0x%08x", pArgs->hr);
        ReportAudioFailure(pArgs->hr);
        SetEvent(pArgs->hGaveUpEvent);
        return 0;
    }
    CoUninitializeOnExit cuoe;

    IMMDeviceEnumerator *pEnumerator = NULL;
    pArgs->hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL,
        __uuidof(IMMDeviceEnumerator),
        (void**)&pEnumerator
    );
    if (FAILED(pArgs->hr)) {
        ERR(L"CoCreateInstance(IMMDeviceEnumerator) failed: hr = 0x%08x", pArgs->hr);
        ReportAudioFailure(pArgs->hr);
        SetEvent(pArgs->hGaveUpEvent);
        return 0;
    }
    ReleaseOnExit releaseEnumerator(pEnumerator);

    CDeviceWatch *pWatch = new CDeviceWatch();
    ReleaseOnExit releaseWatch(pWatch);
    HRESULT hrWatch = pEnumerator->RegisterEndpointNotificationCallback(pWatch);
    if (FAILED(hrWatch)) {
        // capture still works; only device changes are noticed later (via capture errors)
        ERR(L"RegisterEndpointNotificationCallback failed: hr = 0x%08x", hrWatch);
    }

    IMMDevice *pDevice = NULL;
    ReleaseIfSetOnExit<IMMDevice> releaseDevice(&pDevice);
    AudioSnapshot cur = {};
    cur.followDefault = true;
    AudioAction action = AudioAction::SwitchToDefault; // the first run opens the default device
    bool bFirstDevice = true;
    bool bReported = false;
    int nFailures = 0;
    HRESULT hr = S_OK;

    for (;;) {
        if (AudioAction::SwitchToDefault == action || NULL == pDevice) {
            if (NULL != pDevice) {
                pDevice->Release();
                pDevice = NULL;
            }
            CPrefs prefs(hr);
            if (SUCCEEDED(hr)) {
                pDevice = prefs.m_pMMDevice;
                pDevice->AddRef();
                cur.deviceId = GetDeviceId(pDevice);
                LOG(L"%s: %s", bFirstDevice ? L"Opened default device" : L"Switched to default device", cur.deviceId.c_str());
                bFirstDevice = false;
            }
        }

        bool bStarted = false;
        if (NULL != pDevice) {
            SnapshotDevice(pDevice, &cur);
            hr = LoopbackCapture(
                pDevice,
                pArgs->hStartedEvent,
                pArgs->hStopEvent,
                pWatch->hChangedEvent,
                &pArgs->nFrames,
                &bStarted
            );
        }
        ResetAudioBuf();

        // hStopEvent is manual-reset, so it is still set after LoopbackCapture saw it
        if (WAIT_OBJECT_0 == WaitForSingleObject(pArgs->hStopEvent, 0)) {
            break;
        }

        if (bStarted) {
            nFailures = 0;
        }

        if (NULL == pDevice) {
            action = AudioAction::SwitchToDefault;
        } else {
            AudioSnapshot now;
            SnapshotDevice(pDevice, &now);
            now.deviceId = GetDefaultDeviceId(pEnumerator);
            now.followDefault = cur.followDefault;
            action = DecideAudioAction(cur, now, hr);
        }
        LOG(L"Audio capture ended: hr = 0x%08x, started = %d, action = %s", hr, bStarted, AudioActionName(action));

        if (SUCCEEDED(hr)) {
            continue; // a device notification that needs no new device, or a quick re-open
        }

        if (!bStarted && ++nFailures >= CAPTURE_MAX_ATTEMPTS) {
            ERR(L"Audio capture failed %d times in a row; running without audio until a device changes", nFailures);
            if (!bReported) {
                ReportAudioFailure(hr);
                bReported = true;
            }
            SetEvent(pArgs->hGaveUpEvent);

            HANDLE waitArray[2] = { pArgs->hStopEvent, pWatch->hChangedEvent };
            if (WAIT_OBJECT_0 == WaitForMultipleObjects(ARRAYSIZE(waitArray), waitArray, FALSE, INFINITE)) {
                break;
            }
            nFailures = 0;
            action = AudioAction::SwitchToDefault;
            continue;
        }

        if (WAIT_OBJECT_0 == WaitForSingleObject(pArgs->hStopEvent, CAPTURE_RETRY_DELAY_MS)) {
            break;
        }
    }

    if (SUCCEEDED(hrWatch)) {
        pEnumerator->UnregisterEndpointNotificationCallback(pWatch);
    }
    pArgs->hr = S_OK;
    return 0;
}

HRESULT LoopbackCapture(
    IMMDevice *pMMDevice,
    HANDLE hStartedEvent,
    HANDLE hStopEvent,
    HANDLE hChangedEvent,
    PUINT32 pnFrames,
    bool *pbStarted
) {
    HRESULT hr;

    // activate an IAudioClient
    IAudioClient *pAudioClient;
    hr = pMMDevice->Activate(
        __uuidof(IAudioClient),
        CLSCTX_ALL, NULL,
        (void**)&pAudioClient
    );
    if (FAILED(hr)) {
        ERR(L"IMMDevice::Activate(IAudioClient) failed: hr = 0x%08x", hr);
        return hr;
    }
    ReleaseOnExit releaseAudioClient(pAudioClient);
    
    // get the default device periodicity
    REFERENCE_TIME hnsDefaultDevicePeriod;
    hr = pAudioClient->GetDevicePeriod(&hnsDefaultDevicePeriod, NULL);
    if (FAILED(hr)) {
        ERR(L"IAudioClient::GetDevicePeriod failed: hr = 0x%08x", hr);
        return hr;
    }

    // get the default device format
    WAVEFORMATEX *pwfx;
    hr = pAudioClient->GetMixFormat(&pwfx);
    if (FAILED(hr)) {
        ERR(L"IAudioClient::GetMixFormat failed: hr = 0x%08x", hr);
        return hr;
    }
    CoTaskMemFreeOnExit freeMixFormat(pwfx);

    // create a periodic waitable timer
    HANDLE hWakeUp = CreateWaitableTimer(NULL, FALSE, NULL);
    if (NULL == hWakeUp) {
        DWORD dwErr = GetLastError();
        ERR(L"CreateWaitableTimer failed: last error = %u", dwErr);
        return HRESULT_FROM_WIN32(dwErr);
    }
    CloseHandleOnExit closeWakeUp(hWakeUp);

    *pnFrames = 0;
    
    // call IAudioClient::Initialize
    // note that AUDCLNT_STREAMFLAGS_LOOPBACK and AUDCLNT_STREAMFLAGS_EVENTCALLBACK
    // do not work together...
    // the "data ready" event never gets set
    // so we're going to do a timer-driven loop
    LOG(L"Loopback mix format: tag=0x%04x channels=%u rate=%u bits=%u blockAlign=%u",
        pwfx->wFormatTag, pwfx->nChannels, pwfx->nSamplesPerSec, pwfx->wBitsPerSample, pwfx->nBlockAlign);
    hr = pAudioClient->Initialize(
        AUDCLNT_SHAREMODE_SHARED,
        AUDCLNT_STREAMFLAGS_LOOPBACK,
        0, 0, pwfx, 0
    );
    // SURI: some drivers (e.g. multichannel headsets with vendor APOs) reject their own
    // mix format in loopback mode (AUDCLNT_E_UNSUPPORTED_FORMAT). Fall back to plain
    // stereo float and let the Windows audio engine convert (Windows 7+ shared mode).
    WAVEFORMATEX wfxStereo = { 0 };
    IAudioClient *pRetryClient = NULL;
    ReleaseIfSetOnExit<IAudioClient> releaseRetryClient(&pRetryClient);
    if (AUDCLNT_E_UNSUPPORTED_FORMAT == hr) {
        LOG(L"%s", L"Mix format rejected for loopback, retrying as stereo float with auto-conversion");
        hr = pMMDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, (void**)&pRetryClient);
        if (FAILED(hr)) {
            pRetryClient = NULL;
            ERR(L"IMMDevice::Activate(IAudioClient) retry failed: hr = 0x%08x", hr);
            return hr;
        }
        pAudioClient = pRetryClient; // the first client is still released by releaseAudioClient
        wfxStereo.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
        wfxStereo.nChannels = 2;
        wfxStereo.nSamplesPerSec = pwfx->nSamplesPerSec;
        wfxStereo.wBitsPerSample = 32;
        wfxStereo.nBlockAlign = wfxStereo.nChannels * wfxStereo.wBitsPerSample / 8;
        wfxStereo.nAvgBytesPerSec = wfxStereo.nBlockAlign * wfxStereo.nSamplesPerSec;
        hr = pAudioClient->Initialize(
            AUDCLNT_SHAREMODE_SHARED,
            AUDCLNT_STREAMFLAGS_LOOPBACK | AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM | AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY,
            0, 0, &wfxStereo, 0
        );
        if (SUCCEEDED(hr)) {
            pwfx = &wfxStereo; // freeMixFormat still owns and frees the original buffer
        }
    }
    if (FAILED(hr)) {
        ERR(L"IAudioClient::Initialize failed: hr = 0x%08x", hr);
        return hr;
    }

    // activate an IAudioCaptureClient
    IAudioCaptureClient *pAudioCaptureClient;
    hr = pAudioClient->GetService(
        __uuidof(IAudioCaptureClient),
        (void**)&pAudioCaptureClient
    );
    if (FAILED(hr)) {
        ERR(L"IAudioClient::GetService(IAudioCaptureClient) failed: hr = 0x%08x", hr);
        return hr;
    }
    ReleaseOnExit releaseAudioCaptureClient(pAudioCaptureClient);

    // set the waitable timer
    LARGE_INTEGER liFirstFire;
    liFirstFire.QuadPart = -hnsDefaultDevicePeriod / 2; // negative means relative time
    LONG lTimeBetweenFires = (LONG)hnsDefaultDevicePeriod / 2 / (10 * 1000); // convert to milliseconds
    BOOL bOK = SetWaitableTimer(
        hWakeUp,
        &liFirstFire,
        lTimeBetweenFires,
        NULL, NULL, FALSE
    );
    if (!bOK) {
        DWORD dwErr = GetLastError();
        ERR(L"SetWaitableTimer failed: last error = %u", dwErr);
        return HRESULT_FROM_WIN32(dwErr);
    }
    CancelWaitableTimerOnExit cancelWakeUp(hWakeUp);
    
    // call IAudioClient::Start
    hr = pAudioClient->Start();
    if (FAILED(hr)) {
        ERR(L"IAudioClient::Start failed: hr = 0x%08x", hr);
        return hr;
    }
    AudioClientStopOnExit stopAudioClient(pAudioClient);

    *pbStarted = true;
    SetEvent(hStartedEvent);

    // loopback capture loop
    HANDLE waitArray[3] = { hStopEvent, hWakeUp, hChangedEvent };
    DWORD dwWaitResult;

    bool bDone = false;
    bool bFirstPacket = true;

    bool bErrorInAudioData = false;

    for (UINT32 nPasses = 0; !bDone; nPasses++) {
        // drain data while it is available
        UINT32 nNextPacketSize;
        for (
            hr = pAudioCaptureClient->GetNextPacketSize(&nNextPacketSize);
            SUCCEEDED(hr) && nNextPacketSize > 0;
            hr = pAudioCaptureClient->GetNextPacketSize(&nNextPacketSize)
        ) {
            // get the captured data
            BYTE *pData;
            UINT32 nNumFramesToRead;
            DWORD dwFlags;

            hr = pAudioCaptureClient->GetBuffer(
                &pData,
                &nNumFramesToRead,
                &dwFlags,
                NULL,
                NULL
                );

            bErrorInAudioData = false;

            if (FAILED(hr)) {
                bErrorInAudioData = true;
                ERR(L"IAudioCaptureClient::GetBuffer failed on pass %u after %u frames: hr = 0x%08x", nPasses, *pnFrames, hr);
                return hr;
            }

            if (bFirstPacket && AUDCLNT_BUFFERFLAGS_DATA_DISCONTINUITY == dwFlags) {
                LOG(L"%s", L"Probably spurious glitch reported on first packet");
            } else if (0 != dwFlags) {
                bErrorInAudioData = true;
                LOG(L"IAudioCaptureClient::GetBuffer set flags to 0x%08x on pass %u after %u frames", dwFlags, nPasses, *pnFrames);
            }

            if (0 == nNumFramesToRead) {
                bErrorInAudioData = true;
                ERR(L"IAudioCaptureClient::GetBuffer said to read 0 frames on pass %u after %u frames", nPasses, *pnFrames);
            }

            if (bErrorInAudioData) {
                // Glitch in audio detected so we reset audio buffer
                ResetAudioBuf();
            }
            else
            {
                // Saving audio data for visualizer
                SetAudioBuf(pData, nNumFramesToRead, pwfx);
                *pnFrames += nNumFramesToRead;
            }

            hr = pAudioCaptureClient->ReleaseBuffer(nNumFramesToRead);
            if (FAILED(hr)) {
                ERR(L"IAudioCaptureClient::ReleaseBuffer failed on pass %u after %u frames: hr = 0x%08x", nPasses, *pnFrames, hr);
                return hr;
            }

            bFirstPacket = false;
        }

        if (FAILED(hr)) {
            ERR(L"IAudioCaptureClient::GetNextPacketSize failed on pass %u after %u frames: hr = 0x%08x", nPasses, *pnFrames, hr);
            return hr;
        }

        dwWaitResult = WaitForMultipleObjects(
            ARRAYSIZE(waitArray), waitArray,
            FALSE, INFINITE
        );

        if (WAIT_OBJECT_0 == dwWaitResult) {
            LOG(L"Received stop event after %u passes and %u frames", nPasses, *pnFrames);
            bDone = true;
            continue; // exits loop
        }

        // SURI: a device changed; the caller decides whether this stream is still valid
        if (WAIT_OBJECT_0 + 2 == dwWaitResult) {
            LOG(L"Audio device change notified after %u passes and %u frames", nPasses, *pnFrames);
            return S_FALSE;
        }

        if (WAIT_OBJECT_0 + 1 != dwWaitResult) {
            ERR(L"Unexpected WaitForMultipleObjects return value %u on pass %u after %u frames", dwWaitResult, nPasses, *pnFrames);
            return E_UNEXPECTED;
        }
    } // capture loop

    return hr;
}
