#include "App.h"

// ========== Captura ==========

AudioFormat GetCurrentFormat() {
    switch (g_app.formatIndex) {
    case 1: return AudioFormat::MP3;
    case 2: return AudioFormat::OPUS;
    case 3: return AudioFormat::FLAC;
    default: return AudioFormat::WAV;
    }
}

const wchar_t* GetCurrentExtension() {
    switch (g_app.formatIndex) {
    case 1: return L".mp3";
    case 2: return L".opus";
    case 3: return L".flac";
    default: return L".wav";
    }
}

UINT32 GetCurrentBitrate() {
    switch (g_app.formatIndex) {
    case 1: { const UINT32 b[] = {128000,192000,256000,320000}; return (g_app.mp3BitrateIndex >= 0 && g_app.mp3BitrateIndex < 4) ? b[g_app.mp3BitrateIndex] : 192000; }
    case 2: { const UINT32 b[] = {64000,96000,128000,192000,256000}; return (g_app.opusBitrateIndex >= 0 && g_app.opusBitrateIndex < 5) ? b[g_app.opusBitrateIndex] : 128000; }
    case 3: return (g_app.flacCompressionIndex >= 0 && g_app.flacCompressionIndex <= 8) ? g_app.flacCompressionIndex : 5;
    default: return 0;
    }
}

void StartCapture() {
    std::vector<int> checkedIndices;

    if (!g_app.supportsProcessCapture) {
        if (g_app.captureManager->IsCapturing(0)) {
            MessageBox(g_app.hWnd, L"El audio del sistema ya se est\u00e1 capturando.", L"Ya capturando", MB_OK | MB_ICONINFORMATION);
            return;
        }
        checkedIndices.push_back(-1);
    } else {
        int itemCount = ListView_GetItemCount(g_app.hProcessList);
        for (int i = 0; i < itemCount; i++) {
            if (ListView_GetCheckState(g_app.hProcessList, i)) checkedIndices.push_back(i);
        }
        if (checkedIndices.empty()) {
            int focused = ListView_GetNextItem(g_app.hProcessList, -1, LVNI_FOCUSED);
            if (focused >= 0) checkedIndices.push_back(focused);
            else {
                bool micOnly = (SendMessage(g_app.hMicrophoneCheckbox, BM_GETCHECK, 0, 0) == BST_CHECKED);
                if (!micOnly) {
                    MessageBox(g_app.hWnd, L"Seleccione uno o m\u00e1s procesos para capturar.", L"Sin selecci\u00f3n", MB_OK | MB_ICONWARNING);
                    return;
                }
            }
        }
    }

    wchar_t outputPath[MAX_PATH];
    GetWindowText(g_app.hOutputPath, outputPath, MAX_PATH);
    std::wstring normalizedPath = NormalizeOutputPath(outputPath);
    if (normalizedPath != outputPath) SetWindowText(g_app.hOutputPath, normalizedPath.c_str());

    AudioFormat format = GetCurrentFormat();
    const wchar_t* ext = GetCurrentExtension();
    UINT32 bitrate = GetCurrentBitrate();

    bool enablePassthrough = (SendMessage(g_app.hPassthroughCheckbox, BM_GETCHECK, 0, 0) == BST_CHECKED);
    std::wstring passthroughDeviceId;
    if (enablePassthrough && g_app.audioDeviceEnum) {
        int di = (int)SendMessage(g_app.hPassthroughDeviceCombo, CB_GETCURSEL, 0, 0);
        if (di >= 0) {
            const auto& devs = g_app.audioDeviceEnum->GetDevices();
            if (di < (int)devs.size()) passthroughDeviceId = devs[di].deviceId;
        }
    }

    bool monitorOnly = (SendMessage(g_app.hMonitorOnlyCheckbox, BM_GETCHECK, 0, 0) == BST_CHECKED);
    bool captureMic = (SendMessage(g_app.hMicrophoneCheckbox, BM_GETCHECK, 0, 0) == BST_CHECKED);

    std::vector<size_t> micIndices;
    if (captureMic && !monitorOnly && g_app.audioDeviceEnum) {
        micIndices = GetCheckedMicrophoneDeviceIndices();
        if (micIndices.empty()) {
            int fi = ListView_GetNextItem(g_app.hMicrophoneDeviceList, -1, LVNI_FOCUSED);
            if (fi >= 0) { LVITEM item = {}; item.mask = LVIF_PARAM; item.iItem = fi;
                if (ListView_GetItem(g_app.hMicrophoneDeviceList, &item)) micIndices.push_back((size_t)item.lParam);
            }
        }
        if (micIndices.empty()) {
            MessageBox(g_app.hWnd, L"Seleccione al menos un dispositivo de entrada.", L"Sin dispositivo", MB_OK | MB_ICONWARNING);
            return;
        }
    }

    size_t totalSources = checkedIndices.size() + (captureMic && !monitorOnly ? micIndices.size() : 0);
    int modeIndex = (int)SendMessage(g_app.hRecordingModeCombo, CB_GETCURSEL, 0, 0);
    bool createSeparate = (totalSources == 1) || (modeIndex == 0) || (modeIndex == 2);
    bool createCombined = (totalSources > 1) && !monitorOnly && ((modeIndex == 1) || (modeIndex == 2));

    if ((createSeparate || createCombined) && normalizedPath.empty()) {
        MessageBox(g_app.hWnd, L"Elija una carpeta de salida v\u00e1lida.", L"Carpeta inv\u00e1lida", MB_OK | MB_ICONWARNING);
        return;
    }
    if ((createSeparate || createCombined) && !EnsureDirectoryExists(normalizedPath)) {
        MessageBox(g_app.hWnd, L"No se pudo crear la carpeta de salida.", L"Error", MB_OK | MB_ICONWARNING);
        return;
    }

    int started = 0, alreadyCapturing = 0;

    for (int ci : checkedIndices) {
        std::wstring procName; DWORD pid;
        if (!g_app.supportsProcessCapture) { procName = L"[Audio del Sistema]"; pid = 0; }
        else {
            wchar_t buf[256], pidBuf[32];
            ListView_GetItemText(g_app.hProcessList, ci, 0, buf, 256);
            ListView_GetItemText(g_app.hProcessList, ci, 1, pidBuf, 32);
            procName = buf; pid = (DWORD)_wtoi(pidBuf);
        }
        if (g_app.captureManager->IsCapturing(pid)) { alreadyCapturing++; continue; }

        SYSTEMTIME st; GetLocalTime(&st);
        wchar_t ts[64]; swprintf_s(ts, L"%04d_%02d_%02d-%02d_%02d_%02d", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);

        std::wstring clean = procName;
        size_t exePos = clean.find(L".exe");
        if (exePos != std::wstring::npos) clean = clean.substr(0, exePos);

        std::wstring fullPath;
        bool captureMonOnly = monitorOnly;
        if (createSeparate && !monitorOnly) {
            std::wstring base = normalizedPath;
            if (base.back() != L'\\') base += L'\\';
            fullPath = base + clean + L"-" + ts + ext;
        } else {
            fullPath = L"";
            if (!createSeparate && createCombined) captureMonOnly = true;
        }

        if (g_app.captureManager->StartCapture(pid, procName, fullPath, format, bitrate, g_app.skipSilence, passthroughDeviceId, captureMonOnly)) {
            auto sessions = g_app.captureManager->GetActiveSessions();
            for (auto* s : sessions) {
                if (s->processId == pid && s->capture) { s->capture->SetVolume(g_app.processVolume / 100.0f); break; }
            }
            started++;
        }
    }

    if (createCombined && started > 0) {
        std::wstring cp = normalizedPath;
        if (cp.back() != L'\\') cp += L'\\';
        SYSTEMTIME st; GetLocalTime(&st);
        wchar_t ts[64]; swprintf_s(ts, L"%04d_%02d_%02d-%02d_%02d_%02d", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
        cp += L"Combinado-" + std::wstring(ts) + ext;
        if (!g_app.captureManager->EnableMixedRecording(cp, format, bitrate))
            MessageBox(g_app.hWnd, L"Error al habilitar la grabaci\u00f3n combinada.", L"Advertencia", MB_OK | MB_ICONWARNING);
    }

    if (captureMic && !monitorOnly && g_app.audioDeviceEnum) {
        const auto& inputDevs = g_app.audioDeviceEnum->GetInputDevices();
        const DWORD kMicBase = 0xFFFF0000;
        for (size_t mi : micIndices) {
            if (mi >= inputDevs.size()) continue;
            DWORD micPid = kMicBase + (DWORD)mi;
            if (g_app.captureManager->IsCapturing(micPid)) { alreadyCapturing++; continue; }
            bool micCreateFile = createSeparate, micMonOnly = false;
            if (createCombined && !createSeparate) micMonOnly = true;
            std::wstring micPath;
            if (micCreateFile) {
                std::wstring base = normalizedPath;
                if (base.back() != L'\\') base += L'\\';
                SYSTEMTIME st; GetLocalTime(&st);
                wchar_t ts[64]; swprintf_s(ts, L"%04d_%02d_%02d-%02d_%02d_%02d", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
                micPath = base + SanitizeFileName(inputDevs[mi].friendlyName) + L"-" + std::wstring(ts) + ext;
            }
            if (g_app.captureManager->StartCaptureFromDevice(micPid, inputDevs[mi].friendlyName, inputDevs[mi].deviceId, true, micPath, format, bitrate, g_app.skipSilence, micMonOnly)) {
                auto sessions = g_app.captureManager->GetActiveSessions();
                for (auto* s : sessions) { if (s->processId == micPid && s->capture) { s->capture->SetVolume(g_app.microphoneVolume / 100.0f); break; } }
                started++;
            }
        }
    }

    if (started > 0) {
        UpdateRecordingList();
        std::wstring status = L"Se iniciaron " + std::to_wstring(started) + L" captura(s)";
        if (alreadyCapturing > 0) status += L" (" + std::to_wstring(alreadyCapturing) + L" ya activas)";
        SetWindowText(g_app.hStatusText, status.c_str());
    } else if (alreadyCapturing > 0)
        MessageBox(g_app.hWnd, L"Todas las fuentes seleccionadas ya se est\u00e1n capturando.", L"Ya capturando", MB_OK | MB_ICONINFORMATION);
    else
        MessageBox(g_app.hWnd, L"No se pudo iniciar ninguna captura.", L"Error", MB_OK | MB_ICONERROR);
}

void StopCapture() {
    int sel = ListView_GetNextItem(g_app.hRecordingList, -1, LVNI_SELECTED);
    if (sel < 0) { MessageBox(g_app.hWnd, L"Seleccione una grabaci\u00f3n para detener.", L"Sin selecci\u00f3n", MB_OK | MB_ICONWARNING); return; }
    wchar_t pidStr[32];
    ListView_GetItemText(g_app.hRecordingList, sel, 1, pidStr, 32);
    DWORD pid = (DWORD)wcstoul(pidStr, nullptr, 10);
    if (g_app.captureManager->StopCapture(pid)) {
        UpdateRecordingList();
        SetWindowText(g_app.hStatusText, L"Captura detenida.");
        auto sessions = g_app.captureManager->GetActiveSessions();
        if (sessions.empty()) g_app.captureManager->DisableMixedRecording();
        SetFocus(g_app.supportsProcessCapture ? g_app.hProcessList : g_app.hStartBtn);
    }
}

void UpdateRecordingList() {
    DWORD selPID = 0;
    int sel = ListView_GetNextItem(g_app.hRecordingList, -1, LVNI_SELECTED);
    if (sel >= 0) { wchar_t p[32]; ListView_GetItemText(g_app.hRecordingList, sel, 1, p, 32); selPID = (DWORD)wcstoul(p, nullptr, 10); }

    ListView_DeleteAllItems(g_app.hRecordingList);
    auto sessions = g_app.captureManager->GetActiveSessions();
    int newSel = -1;

    for (size_t i = 0; i < sessions.size(); i++) {
        auto* s = sessions[i];
        LVITEM lvi = {}; lvi.mask = LVIF_TEXT; lvi.iItem = (int)i;
        lvi.pszText = (LPWSTR)s->processName.c_str();
        int idx = ListView_InsertItem(g_app.hRecordingList, &lvi);
        wchar_t pidStr[32]; swprintf_s(pidStr, L"%lu", s->processId);
        ListView_SetItemText(g_app.hRecordingList, idx, 1, pidStr);
        ListView_SetItemText(g_app.hRecordingList, idx, 2, s->monitorOnly ? (LPWSTR)L"[Solo monitoreo]" : (LPWSTR)s->outputFile.c_str());
        std::wstring sz = s->monitorOnly ? L"N/A" : FormatFileSize(s->bytesWritten);
        ListView_SetItemText(g_app.hRecordingList, idx, 3, (LPWSTR)sz.c_str());
        if (sel >= 0 && s->processId == selPID) newSel = idx;
    }

    if (newSel >= 0) {
        UINT mask = LVIS_SELECTED | (g_app.isAppActive ? LVIS_FOCUSED : 0);
        ListView_SetItemState(g_app.hRecordingList, newSel, mask, mask);
    }
    EnsureRecordingListFocusItem();
    g_app.captureButtonStops = ListView_GetNextItem(g_app.hRecordingList, -1, LVNI_SELECTED) >= 0;
    SetWindowText(g_app.hStartBtn, g_app.captureButtonStops ? L"&Detener captura" : L"&Iniciar captura");

    if (sessions.size() >= 2) {
        ShowWindow(g_app.hStopAllBtn, SW_SHOW); EnableWindow(g_app.hStopAllBtn, TRUE);
        ShowWindow(g_app.hPauseAllBtn, SW_SHOW); ShowWindow(g_app.hResumeAllBtn, SW_SHOW);
        int paused = 0, resumed = 0;
        for (auto* s : sessions) { if (s->capture) { if (s->capture->IsPaused()) paused++; else resumed++; } }
        EnableWindow(g_app.hPauseAllBtn, resumed > 0);
        EnableWindow(g_app.hResumeAllBtn, paused > 0);
    } else {
        ShowWindow(g_app.hStopAllBtn, SW_HIDE); EnableWindow(g_app.hStopAllBtn, FALSE);
        ShowWindow(g_app.hPauseAllBtn, SW_HIDE); EnableWindow(g_app.hPauseAllBtn, FALSE);
        ShowWindow(g_app.hResumeAllBtn, SW_HIDE); EnableWindow(g_app.hResumeAllBtn, FALSE);
    }
}

void EnsureRecordingListFocusItem() {
    int fi = ListView_GetNextItem(g_app.hRecordingList, -1, LVNI_FOCUSED);
    if (fi < 0) {
        int si = ListView_GetNextItem(g_app.hRecordingList, -1, LVNI_SELECTED);
        if (si >= 0) {
            ListView_SetItemState(g_app.hRecordingList, si, LVIS_FOCUSED, LVIS_FOCUSED);
        } else if (ListView_GetItemCount(g_app.hRecordingList) > 0) {
            ListView_SetItemState(g_app.hRecordingList, 0, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
        }
    }
}

// ========== Dispositivos ==========
void BrowseOutputFolder() {
    IFileDialog* pfd = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pfd));
    if (SUCCEEDED(hr)) {
        DWORD opts; pfd->GetOptions(&opts); pfd->SetOptions(opts | FOS_PICKFOLDERS);
        if (SUCCEEDED(pfd->Show(g_app.hWnd))) {
            IShellItem* psi; if (SUCCEEDED(pfd->GetResult(&psi))) {
                PWSTR p; if (SUCCEEDED(psi->GetDisplayName(SIGDN_FILESYSPATH, &p))) {
                    SetWindowText(g_app.hOutputPath, p); CoTaskMemFree(p);
                } psi->Release();
            }
        } pfd->Release();
    }
}

void PopulatePassthroughDevices() {
    if (!g_app.audioDeviceEnum || !g_app.audioDeviceEnum->EnumerateDevices()) return;
    SendMessage(g_app.hPassthroughDeviceCombo, CB_RESETCONTENT, 0, 0);
    const auto& devs = g_app.audioDeviceEnum->GetDevices();
    int def = -1;
    for (size_t i = 0; i < devs.size(); i++) {
        std::wstring name = devs[i].friendlyName;
        if (devs[i].isDefault) { name += L" (Predeterminado)"; def = (int)i; }
        SendMessage(g_app.hPassthroughDeviceCombo, CB_ADDSTRING, 0, (LPARAM)name.c_str());
    }
    SendMessage(g_app.hPassthroughDeviceCombo, CB_SETCURSEL, def >= 0 ? def : 0, 0);
    OnPassthroughCheckboxChanged();
}

void OnPassthroughCheckboxChanged() {
    BOOL checked = (SendMessage(g_app.hPassthroughCheckbox, BM_GETCHECK, 0, 0) == BST_CHECKED);
    ShowWindow(g_app.hPassthroughDeviceLabel, checked ? SW_SHOW : SW_HIDE);
    ShowWindow(g_app.hPassthroughDeviceCombo, checked ? SW_SHOW : SW_HIDE);
    EnableWindow(g_app.hMonitorOnlyCheckbox, checked);
    if (!checked && SendMessage(g_app.hMonitorOnlyCheckbox, BM_GETCHECK, 0, 0) == BST_CHECKED) {
        SendMessage(g_app.hMonitorOnlyCheckbox, BM_SETCHECK, BST_UNCHECKED, 0);
        OnMonitorOnlyCheckboxChanged();
    }
}

void OnMonitorOnlyCheckboxChanged() {
    BOOL mon = (SendMessage(g_app.hMonitorOnlyCheckbox, BM_GETCHECK, 0, 0) == BST_CHECKED);
    BOOL en = !mon;
    EnableWindow(g_app.hOutputPath, en); EnableWindow(g_app.hBrowseBtn, en);
    ShowWindow(g_app.hRecordingModeLabel, en ? SW_SHOW : SW_HIDE);
    ShowWindow(g_app.hRecordingModeCombo, en ? SW_SHOW : SW_HIDE);
}

void PopulateMicrophoneDevices() {
    if (!g_app.audioDeviceEnum || !g_app.audioDeviceEnum->EnumerateInputDevices()) return;
    ListView_DeleteAllItems(g_app.hMicrophoneDeviceList);
    const auto& devs = g_app.audioDeviceEnum->GetInputDevices();
    std::vector<size_t> sorted; sorted.reserve(devs.size());
    for (size_t i = 0; i < devs.size(); i++) sorted.push_back(i);
    std::sort(sorted.begin(), sorted.end(), [&](size_t a, size_t b) {
        if (devs[a].isDefault != devs[b].isDefault) return devs[a].isDefault;
        return StrCmpLogicalW(devs[a].friendlyName.c_str(), devs[b].friendlyName.c_str()) < 0;
    });
    int def = -1;
    for (size_t li = 0; li < sorted.size(); li++) {
        size_t di = sorted[li];
        std::wstring name = devs[di].friendlyName;
        if (devs[di].isDefault) { name = L"Predeterminado: " + name; def = (int)li; }
        LVITEM lvi = {}; lvi.mask = LVIF_TEXT | LVIF_PARAM; lvi.iItem = (int)li;
        lvi.pszText = (LPWSTR)name.c_str(); lvi.lParam = (LPARAM)di;
        ListView_InsertItem(g_app.hMicrophoneDeviceList, &lvi);
    }
    if (def >= 0) {
        ListView_SetCheckState(g_app.hMicrophoneDeviceList, def, TRUE);
    } else if (!devs.empty()) {
        ListView_SetCheckState(g_app.hMicrophoneDeviceList, 0, TRUE);
    }
    OnMicrophoneCheckboxChanged();
}

void OnMicrophoneCheckboxChanged() {
    BOOL checked = (SendMessage(g_app.hMicrophoneCheckbox, BM_GETCHECK, 0, 0) == BST_CHECKED);
    ShowWindow(g_app.hMicrophoneDeviceLabel, checked ? SW_SHOW : SW_HIDE);
    ShowWindow(g_app.hMicrophoneDeviceList, checked ? SW_SHOW : SW_HIDE);
}

std::vector<size_t> GetCheckedMicrophoneDeviceIndices() {
    std::vector<size_t> r;
    int cnt = ListView_GetItemCount(g_app.hMicrophoneDeviceList);
    for (int i = 0; i < cnt; i++) {
        if (ListView_GetCheckState(g_app.hMicrophoneDeviceList, i)) {
            LVITEM item = {}; item.mask = LVIF_PARAM; item.iItem = i;
            if (ListView_GetItem(g_app.hMicrophoneDeviceList, &item)) r.push_back((size_t)item.lParam);
        }
    }
    return r;
}

std::vector<std::wstring> GetCheckedMicrophoneDeviceIds() {
    std::vector<std::wstring> r;
    if (!g_app.audioDeviceEnum) return r;
    const auto& devs = g_app.audioDeviceEnum->GetInputDevices();
    for (size_t idx : GetCheckedMicrophoneDeviceIndices())
        if (idx < devs.size()) r.push_back(devs[idx].deviceId);
    return r;
}

// ========== Bandeja del sistema ==========
void AddTrayIcon() {
    ZeroMemory(&g_app.nid, sizeof(g_app.nid));
    g_app.nid.cbSize = sizeof(g_app.nid);
    g_app.nid.hWnd = g_app.hWnd;
    g_app.nid.uID = IDI_TRAY_ICON;
    g_app.nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_app.nid.uCallbackMessage = WM_TRAYICON;
    g_app.nid.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wcscpy_s(g_app.nid.szTip, L"ML Recorder");
    Shell_NotifyIcon(NIM_ADD, &g_app.nid);
    g_app.isMinimizedToTray = true;
}

void RemoveTrayIcon() {
    if (g_app.isMinimizedToTray) { Shell_NotifyIcon(NIM_DELETE, &g_app.nid); g_app.isMinimizedToTray = false; }
}

void ShowTrayContextMenu() {
    HMENU h = CreatePopupMenu();
    AppendMenu(h, MF_STRING, IDM_TRAY_SHOW, L"Mostrar ventana");
    AppendMenu(h, MF_SEPARATOR, 0, nullptr);
    AppendMenu(h, MF_STRING, IDM_TRAY_EXIT, L"Salir");
    SetMenuDefaultItem(h, IDM_TRAY_SHOW, FALSE);
    POINT pt; GetCursorPos(&pt);
    SetForegroundWindow(g_app.hWnd);
    TrackPopupMenu(h, TPM_BOTTOMALIGN | TPM_LEFTALIGN, pt.x, pt.y, 0, g_app.hWnd, nullptr);
    DestroyMenu(h);
}

void ShowWindowFromTray() { RemoveTrayIcon(); ShowWindow(g_app.hWnd, SW_RESTORE); SetForegroundWindow(g_app.hWnd); }
void HideWindowToTray() { ShowWindow(g_app.hWnd, SW_HIDE); AddTrayIcon(); }
