#include "App.h"

// ========== Utilidades ==========
std::wstring GetDefaultOutputPath() {
    wchar_t path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPath(nullptr, CSIDL_MYDOCUMENTS, nullptr, 0, path))) {
        std::wstring r = path; r += L"\\MLRecorder";
        CreateDirectory(r.c_str(), nullptr);
        return r;
    }
    return L"C:\\MLRecorder";
}

std::wstring FormatFileSize(UINT64 bytes) {
    const wchar_t* u[] = { L"B", L"KB", L"MB", L"GB" };
    int i = 0; double s = (double)bytes;
    while (s >= 1024.0 && i < 3) { s /= 1024.0; i++; }
    wchar_t buf[64]; swprintf_s(buf, L"%.2f %s", s, u[i]); return buf;
}

std::wstring NormalizeOutputPath(const std::wstring& path) {
    std::wstring r = path;
    auto sp = [](wchar_t c) { return c == L' ' || c == L'\t' || c == L'\r' || c == L'\n'; };
    while (!r.empty() && sp(r.front())) r.erase(r.begin());
    while (!r.empty() && sp(r.back())) r.pop_back();
    if (r.size() >= 2 && r.front() == L'"' && r.back() == L'"') r = r.substr(1, r.size() - 2);
    while (!r.empty() && sp(r.front())) r.erase(r.begin());
    while (!r.empty() && sp(r.back())) r.pop_back();
    return r;
}

bool EnsureDirectoryExists(const std::wstring& path) {
    if (path.empty()) return false;
    int r = SHCreateDirectoryExW(nullptr, path.c_str(), nullptr);
    if (r == ERROR_SUCCESS || r == ERROR_ALREADY_EXISTS) {
        DWORD a = GetFileAttributesW(path.c_str());
        return (a != INVALID_FILE_ATTRIBUTES) && (a & FILE_ATTRIBUTE_DIRECTORY);
    }
    return false;
}

std::wstring SanitizeFileName(const std::wstring& name) {
    if (name.empty()) return L"Dispositivo";
    std::wstring s = name;
    const wchar_t* inv = L"\\/:*?\"<>|";
    for (auto& c : s) { if (wcschr(inv, c)) c = L'_'; }
    while (!s.empty() && (s.back() == L'.' || s.back() == L' ')) s.pop_back();
    return s.empty() ? L"Dispositivo" : s;
}

std::string WStringToString(const std::wstring& w) {
    if (w.empty()) return {};
    int sz = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string r(sz - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, &r[0], sz, nullptr, nullptr);
    return r;
}

std::wstring StringToWString(const std::string& s) {
    if (s.empty()) return {};
    int sz = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring r(sz - 1, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &r[0], sz);
    return r;
}

// ========== Configuración (Persistencia) ==========
std::wstring GetSettingsFilePath() {
    wchar_t path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPath(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, path))) {
        std::wstring d = path; d += L"\\MLRecorder";
        CreateDirectory(d.c_str(), nullptr);
        return d + L"\\settings.json";
    }
    return L"settings.json";
}

void LoadSettings() {
    std::wstring sp = GetSettingsFilePath();
    std::ifstream f(sp);
    if (!f.is_open()) return;
    try {
        json s = json::parse(f);
        if (s.contains("outputPath") && s["outputPath"].is_string())
            SetWindowTextW(g_app.hOutputPath, StringToWString(s["outputPath"]).c_str());
        if (s.contains("format") && s["format"].is_number_integer()) {
            int v = s["format"]; if (v >= 0 && v <= 3) g_app.formatIndex = v;
        }
        if (s.contains("mp3Bitrate") && s["mp3Bitrate"].is_number_integer()) {
            int v = s["mp3Bitrate"]; if (v >= 0 && v <= 3) g_app.mp3BitrateIndex = v;
        }
        if (s.contains("opusBitrate") && s["opusBitrate"].is_number_integer()) {
            int v = s["opusBitrate"]; if (v >= 0 && v <= 4) g_app.opusBitrateIndex = v;
        }
        if (s.contains("flacCompression") && s["flacCompression"].is_number_integer()) {
            int v = s["flacCompression"]; if (v >= 0 && v <= 8) g_app.flacCompressionIndex = v;
        }
        if (s.contains("skipSilence") && s["skipSilence"].is_boolean())
            g_app.skipSilence = s["skipSilence"];
        if (s.contains("passthrough") && s["passthrough"].is_boolean())
            SendMessage(g_app.hPassthroughCheckbox, BM_SETCHECK, s["passthrough"].get<bool>() ? BST_CHECKED : BST_UNCHECKED, 0);
        if (s.contains("passthroughDeviceIndex") && s["passthroughDeviceIndex"].is_number_integer()) {
            int v = s["passthroughDeviceIndex"];
            if (v >= 0 && v < SendMessage(g_app.hPassthroughDeviceCombo, CB_GETCOUNT, 0, 0))
                SendMessage(g_app.hPassthroughDeviceCombo, CB_SETCURSEL, v, 0);
        }
        if (s.contains("monitorOnly") && s["monitorOnly"].is_boolean())
            SendMessage(g_app.hMonitorOnlyCheckbox, BM_SETCHECK, s["monitorOnly"].get<bool>() ? BST_CHECKED : BST_UNCHECKED, 0);
        if (s.contains("recordingMode") && s["recordingMode"].is_number_integer()) {
            int v = s["recordingMode"]; if (v >= 0 && v <= 2) SendMessage(g_app.hRecordingModeCombo, CB_SETCURSEL, v, 0);
        }
        if (s.contains("captureMicrophone") && s["captureMicrophone"].is_boolean())
            SendMessage(g_app.hMicrophoneCheckbox, BM_SETCHECK, s["captureMicrophone"].get<bool>() ? BST_CHECKED : BST_UNCHECKED, 0);
        if (s.contains("processVolume") && s["processVolume"].is_number()) {
            float v = s["processVolume"]; if (v >= 0 && v <= 100) g_app.processVolume = v;
        }
        if (s.contains("microphoneVolume") && s["microphoneVolume"].is_number()) {
            float v = s["microphoneVolume"]; if (v >= 0 && v <= 100) g_app.microphoneVolume = v;
        }
    } catch (...) {}
    f.close();
    OnPassthroughCheckboxChanged();
    OnMonitorOnlyCheckboxChanged();
    OnMicrophoneCheckboxChanged();
}

void SaveSettings() {
    json s;
    wchar_t op[MAX_PATH]; GetWindowTextW(g_app.hOutputPath, op, MAX_PATH);
    s["outputPath"] = WStringToString(op);
    s["format"] = g_app.formatIndex;
    s["mp3Bitrate"] = g_app.mp3BitrateIndex;
    s["opusBitrate"] = g_app.opusBitrateIndex;
    s["flacCompression"] = g_app.flacCompressionIndex;
    s["skipSilence"] = g_app.skipSilence;
    s["passthrough"] = (SendMessage(g_app.hPassthroughCheckbox, BM_GETCHECK, 0, 0) == BST_CHECKED);
    s["passthroughDeviceIndex"] = (int)SendMessage(g_app.hPassthroughDeviceCombo, CB_GETCURSEL, 0, 0);
    s["monitorOnly"] = (SendMessage(g_app.hMonitorOnlyCheckbox, BM_GETCHECK, 0, 0) == BST_CHECKED);
    s["recordingMode"] = (int)SendMessage(g_app.hRecordingModeCombo, CB_GETCURSEL, 0, 0);
    s["captureMicrophone"] = (SendMessage(g_app.hMicrophoneCheckbox, BM_GETCHECK, 0, 0) == BST_CHECKED);
    s["processVolume"] = g_app.processVolume;
    s["microphoneVolume"] = g_app.microphoneVolume;
    std::ofstream f(GetSettingsFilePath());
    if (f.is_open()) { f << s.dump(4); f.close(); }
}

// ========== Diálogo de calidad de audio ==========

static HWND s_hQualFormatCombo = nullptr;
static HWND s_hQualMp3BitrateCombo = nullptr;
static HWND s_hQualMp3BitrateLabel = nullptr;
static HWND s_hQualOpusBitrateCombo = nullptr;
static HWND s_hQualOpusBitrateLabel = nullptr;
static HWND s_hQualFlacCompCombo = nullptr;
static HWND s_hQualFlacCompLabel = nullptr;
static HWND s_hQualSkipSilence = nullptr;

static void OnQualFormatChanged() {
    int f = (int)SendMessage(s_hQualFormatCombo, CB_GETCURSEL, 0, 0);
    ShowWindow(s_hQualMp3BitrateLabel, f == 1 ? SW_SHOW : SW_HIDE);
    ShowWindow(s_hQualMp3BitrateCombo, f == 1 ? SW_SHOW : SW_HIDE);
    ShowWindow(s_hQualOpusBitrateLabel, f == 2 ? SW_SHOW : SW_HIDE);
    ShowWindow(s_hQualOpusBitrateCombo, f == 2 ? SW_SHOW : SW_HIDE);
    ShowWindow(s_hQualFlacCompLabel, f == 3 ? SW_SHOW : SW_HIDE);
    ShowWindow(s_hQualFlacCompCombo, f == 3 ? SW_SHOW : SW_HIDE);
}

static LRESULT CALLBACK QualityDialogProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        int y = 15;
        CreateWindow(L"STATIC", L"&Formato de audio:", WS_CHILD | WS_VISIBLE | SS_LEFT,
            15, y, 150, 20, hwnd, nullptr, g_app.hInst, nullptr);
        s_hQualFormatCombo = CreateWindow(WC_COMBOBOX, L"",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST,
            170, y - 3, 180, 200, hwnd, (HMENU)200, g_app.hInst, nullptr);
        SendMessage(s_hQualFormatCombo, CB_ADDSTRING, 0, (LPARAM)L"WAV (sin compresi\u00f3n)");
        SendMessage(s_hQualFormatCombo, CB_ADDSTRING, 0, (LPARAM)L"MP3");
        SendMessage(s_hQualFormatCombo, CB_ADDSTRING, 0, (LPARAM)L"Opus");
        SendMessage(s_hQualFormatCombo, CB_ADDSTRING, 0, (LPARAM)L"FLAC (sin p\u00e9rdida)");
        SendMessage(s_hQualFormatCombo, CB_SETCURSEL, g_app.formatIndex, 0);

        y += 35;
        s_hQualMp3BitrateLabel = CreateWindow(L"STATIC", L"Tasa de bits MP3:",
            WS_CHILD | SS_LEFT, 15, y, 150, 20, hwnd, nullptr, g_app.hInst, nullptr);
        s_hQualMp3BitrateCombo = CreateWindow(WC_COMBOBOX, L"",
            WS_CHILD | WS_TABSTOP | CBS_DROPDOWNLIST,
            170, y - 3, 180, 200, hwnd, (HMENU)201, g_app.hInst, nullptr);
        SendMessage(s_hQualMp3BitrateCombo, CB_ADDSTRING, 0, (LPARAM)L"128 kbps");
        SendMessage(s_hQualMp3BitrateCombo, CB_ADDSTRING, 0, (LPARAM)L"192 kbps");
        SendMessage(s_hQualMp3BitrateCombo, CB_ADDSTRING, 0, (LPARAM)L"256 kbps");
        SendMessage(s_hQualMp3BitrateCombo, CB_ADDSTRING, 0, (LPARAM)L"320 kbps");
        SendMessage(s_hQualMp3BitrateCombo, CB_SETCURSEL, g_app.mp3BitrateIndex, 0);

        s_hQualOpusBitrateLabel = CreateWindow(L"STATIC", L"Tasa de bits Opus:",
            WS_CHILD | SS_LEFT, 15, y, 150, 20, hwnd, nullptr, g_app.hInst, nullptr);
        s_hQualOpusBitrateCombo = CreateWindow(WC_COMBOBOX, L"",
            WS_CHILD | WS_TABSTOP | CBS_DROPDOWNLIST,
            170, y - 3, 180, 200, hwnd, (HMENU)202, g_app.hInst, nullptr);
        SendMessage(s_hQualOpusBitrateCombo, CB_ADDSTRING, 0, (LPARAM)L"64 kbps");
        SendMessage(s_hQualOpusBitrateCombo, CB_ADDSTRING, 0, (LPARAM)L"96 kbps");
        SendMessage(s_hQualOpusBitrateCombo, CB_ADDSTRING, 0, (LPARAM)L"128 kbps");
        SendMessage(s_hQualOpusBitrateCombo, CB_ADDSTRING, 0, (LPARAM)L"192 kbps");
        SendMessage(s_hQualOpusBitrateCombo, CB_ADDSTRING, 0, (LPARAM)L"256 kbps");
        SendMessage(s_hQualOpusBitrateCombo, CB_SETCURSEL, g_app.opusBitrateIndex, 0);

        s_hQualFlacCompLabel = CreateWindow(L"STATIC", L"Compresi\u00f3n FLAC:",
            WS_CHILD | SS_LEFT, 15, y, 150, 20, hwnd, nullptr, g_app.hInst, nullptr);
        s_hQualFlacCompCombo = CreateWindow(WC_COMBOBOX, L"",
            WS_CHILD | WS_TABSTOP | CBS_DROPDOWNLIST,
            170, y - 3, 180, 200, hwnd, (HMENU)203, g_app.hInst, nullptr);
        for (int i = 0; i <= 8; i++) {
            wchar_t buf[16]; swprintf_s(buf, L"Nivel %d", i);
            SendMessage(s_hQualFlacCompCombo, CB_ADDSTRING, 0, (LPARAM)buf);
        }
        SendMessage(s_hQualFlacCompCombo, CB_SETCURSEL, g_app.flacCompressionIndex, 0);

        y += 40;
        s_hQualSkipSilence = CreateWindow(L"BUTTON", L"&Omitir silencio",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
            15, y, 200, 20, hwnd, (HMENU)204, g_app.hInst, nullptr);
        SendMessage(s_hQualSkipSilence, BM_SETCHECK, g_app.skipSilence ? BST_CHECKED : BST_UNCHECKED, 0);

        y += 35;
        CreateWindow(L"BUTTON", L"Aceptar", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
            80, y, 90, 28, hwnd, (HMENU)IDOK, g_app.hInst, nullptr);
        CreateWindow(L"BUTTON", L"Cancelar", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
            185, y, 90, 28, hwnd, (HMENU)IDCANCEL, g_app.hInst, nullptr);

        OnQualFormatChanged();
        SetFocus(s_hQualFormatCombo);
        return 0;
    }
    case WM_COMMAND:
        if (LOWORD(wp) == 200 && HIWORD(wp) == CBN_SELCHANGE) { OnQualFormatChanged(); return 0; }
        if (LOWORD(wp) == IDOK) {
            g_app.formatIndex = (int)SendMessage(s_hQualFormatCombo, CB_GETCURSEL, 0, 0);
            g_app.mp3BitrateIndex = (int)SendMessage(s_hQualMp3BitrateCombo, CB_GETCURSEL, 0, 0);
            g_app.opusBitrateIndex = (int)SendMessage(s_hQualOpusBitrateCombo, CB_GETCURSEL, 0, 0);
            g_app.flacCompressionIndex = (int)SendMessage(s_hQualFlacCompCombo, CB_GETCURSEL, 0, 0);
            g_app.skipSilence = (SendMessage(s_hQualSkipSilence, BM_GETCHECK, 0, 0) == BST_CHECKED);
            SetWindowLongPtr(hwnd, GWLP_USERDATA, 1);
            DestroyWindow(hwnd);
            return 0;
        }
        if (LOWORD(wp) == IDCANCEL) { DestroyWindow(hwnd); return 0; }
        break;
    case WM_CLOSE:
        DestroyWindow(hwnd); return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

void ShowQualityDialog(HWND parent) {
    static bool reg = false;
    const wchar_t* cn = L"MLRecorderQualityDlg";
    if (!reg) {
        WNDCLASS wc = {}; wc.lpfnWndProc = QualityDialogProc; wc.hInstance = g_app.hInst;
        wc.lpszClassName = cn; wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        RegisterClass(&wc); reg = true;
    }
    HWND dlg = CreateWindowEx(WS_EX_DLGMODALFRAME | WS_EX_TOPMOST, cn, L"Calidad de audio",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        (GetSystemMetrics(SM_CXSCREEN) - 380) / 2, (GetSystemMetrics(SM_CYSCREEN) - 220) / 2,
        380, 220, parent, nullptr, g_app.hInst, nullptr);
    if (!dlg) return;
    EnableWindow(parent, FALSE);
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        if (!IsWindow(dlg)) break;
        if (!IsDialogMessage(dlg, &msg)) { TranslateMessage(&msg); DispatchMessage(&msg); }
        if (!IsWindow(dlg)) break;
    }
    EnableWindow(parent, TRUE);
    SetForegroundWindow(parent);
}
