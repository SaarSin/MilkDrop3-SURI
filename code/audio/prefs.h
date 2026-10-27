// prefs.h

class CPrefs {
public:
    IMMDevice *m_pMMDevice;

    // opens the default render endpoint (eRender, eConsole)
    CPrefs(HRESULT &hr);
    ~CPrefs();
};
