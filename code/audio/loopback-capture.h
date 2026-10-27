// loopback-capture.h

// call CreateThread on this function
// feed it the address of a LoopbackCaptureThreadFunctionArguments
// it will capture via loopback from the default render device
// and feed the visualizer audio buffer
// until the stop event is set
// SURI: it follows device changes (unplug, new default device, format change) by
// itself; hStartedEvent is set whenever capture runs, hGaveUpEvent when it could not
// start after several attempts (the user has been told; it keeps waiting for a device)

struct LoopbackCaptureThreadFunctionArguments {
    HANDLE hStartedEvent;
    HANDLE hGaveUpEvent;
    HANDLE hStopEvent;  // must be manual-reset
    UINT32 nFrames;
    HRESULT hr;
};

DWORD WINAPI LoopbackCaptureThreadFunction(LPVOID pContext);
