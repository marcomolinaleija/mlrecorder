std::vector<DWORD> ProcessEnumerator::GetActiveAudioSessionPIDs() {
    std::vector<DWORD> activePids;
    CoInitialize(nullptr);

    IMMDeviceEnumerator* deviceEnumerator = nullptr;
    IMMDevice* device = nullptr;
    IAudioSessionManager2* sessionManager = nullptr;
    IAudioSessionEnumerator* sessionEnumerator = nullptr;

    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr,
        CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
        reinterpret_cast<void**>(&deviceEnumerator));

    if (SUCCEEDED(hr)) {
        hr = deviceEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);
        if (SUCCEEDED(hr)) {
            hr = device->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL,
                nullptr, reinterpret_cast<void**>(&sessionManager));
            if (SUCCEEDED(hr)) {
                hr = sessionManager->GetSessionEnumerator(&sessionEnumerator);
                if (SUCCEEDED(hr)) {
                    int sessionCount = 0;
                    sessionEnumerator->GetCount(&sessionCount);
                    for (int i = 0; i < sessionCount; i++) {
                        IAudioSessionControl* sessionControl = nullptr;
                        IAudioSessionControl2* sessionControl2 = nullptr;
                        if (SUCCEEDED(sessionEnumerator->GetSession(i, &sessionControl))) {
                            if (SUCCEEDED(sessionControl->QueryInterface(__uuidof(IAudioSessionControl2),
                                reinterpret_cast<void**>(&sessionControl2)))) {
                                AudioSessionState state;
                                if (SUCCEEDED(sessionControl2->GetState(&state)) && state == AudioSessionStateActive) {
                                    DWORD pid = 0;
                                    if (SUCCEEDED(sessionControl2->GetProcessId(&pid)) && pid != 0) {
                                        bool exists = false;
                                        for(DWORD p : activePids) if(p==pid) { exists=true; break; }
                                        if(!exists) activePids.push_back(pid);
                                    }
                                }
                                sessionControl2->Release();
                            }
                            sessionControl->Release();
                        }
                    }
                    sessionEnumerator->Release();
                }
                sessionManager->Release();
            }
            device->Release();
        }
        deviceEnumerator->Release();
    }
    CoUninitialize();
    return activePids;
}
