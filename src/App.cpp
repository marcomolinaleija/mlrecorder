#include "App.h"

// ========== Menú principal ==========
HMENU CreateMainMenu() {
    HMENU hMenu = CreateMenu();
    HMENU hFile = CreatePopupMenu();
    AppendMenu(hFile, MF_STRING, IDM_FILE_OPEN_FOLDER, L"&Abrir carpeta de grabaciones\tCtrl+O");
    AppendMenu(hFile, MF_SEPARATOR, 0, nullptr);
    AppendMenu(hFile, MF_STRING, IDM_FILE_EXIT, L"&Salir\tAlt+F4");
    AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hFile, L"&Archivo");

    HMENU hConfig = CreatePopupMenu();
    AppendMenu(hConfig, MF_STRING, IDM_CONFIG_QUALITY, L"&Calidad de audio...\tCtrl+Q");
    AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hConfig, L"&Configuraci\u00f3n");

    HMENU hHelp = CreatePopupMenu();
    AppendMenu(hHelp, MF_STRING, IDM_HELP_ABOUT, L"&Acerca de ML Recorder...");
    AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hHelp, L"Ay&uda");

    return hMenu;
}

// ========== Inicialización de controles ==========
void InitializeControls(HWND hwnd) {
    DWORD vis = g_app.supportsProcessCapture ? (WS_CHILD | WS_VISIBLE) : WS_CHILD;

    // Lista de procesos
    g_app.hProcessListLabel = CreateWindow(L"STATIC", L"&Procesos disponibles:",
        vis | SS_LEFT, 10, 5, 200, 20, hwnd, (HMENU)IDC_PROCESS_LIST_LABEL, g_app.hInst, nullptr);

    g_app.hProcessList = CreateWindowEx(WS_EX_CLIENTEDGE, WC_LISTVIEW, L"",
        vis | WS_TABSTOP | LVS_REPORT, 10, 25, 810, 160,
        hwnd, (HMENU)IDC_PROCESS_LIST, g_app.hInst, nullptr);
    ListView_SetExtendedListViewStyle(g_app.hProcessList, LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT);

    LVCOLUMN lvc = {};
    lvc.mask = LVCF_TEXT | LVCF_WIDTH;
    lvc.cx = 200; lvc.pszText = (LPWSTR)L"Nombre del proceso";
    ListView_InsertColumn(g_app.hProcessList, 0, &lvc);
    lvc.cx = 60; lvc.pszText = (LPWSTR)L"PID";
    ListView_InsertColumn(g_app.hProcessList, 1, &lvc);
    lvc.cx = 280; lvc.pszText = (LPWSTR)L"T\u00edtulo de ventana";
    ListView_InsertColumn(g_app.hProcessList, 2, &lvc);
    lvc.cx = 250; lvc.pszText = (LPWSTR)L"Ruta";
    ListView_InsertColumn(g_app.hProcessList, 3, &lvc);

    g_app.hRefreshBtn = CreateWindow(L"BUTTON", L"Refrescar (F5)",
        vis | WS_TABSTOP | BS_PUSHBUTTON, 10, 190, 100, 25,
        hwnd, (HMENU)IDC_REFRESH_BTN, g_app.hInst, nullptr);

    g_app.hShowAudioOnlyCheckbox = CreateWindow(L"BUTTON", L"Solo procesos con audio activo",
        vis | WS_TABSTOP | BS_AUTOCHECKBOX, 120, 193, 320, 20,
        hwnd, (HMENU)IDC_SHOW_AUDIO_ONLY_CHECKBOX, g_app.hInst, nullptr);
    SendMessage(g_app.hShowAudioOnlyCheckbox, BM_SETCHECK, BST_CHECKED, 0);

    // Micrófono
    g_app.hMicrophoneCheckbox = CreateWindow(L"BUTTON", L"Capturar &micr\u00f3fono",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX, 10, 235, 170, 20,
        hwnd, (HMENU)IDC_MICROPHONE_CHECKBOX, g_app.hInst, nullptr);

    g_app.hMicrophoneDeviceLabel = CreateWindow(L"STATIC", L"Dispositivos de entrada:",
        WS_CHILD | SS_LEFT, 190, 235, 120, 20,
        hwnd, (HMENU)IDC_MICROPHONE_DEVICE_LABEL, g_app.hInst, nullptr);

    g_app.hMicrophoneDeviceList = CreateWindowEx(WS_EX_CLIENTEDGE, WC_LISTVIEW, L"",
        WS_CHILD | WS_TABSTOP | LVS_REPORT, 310, 232, 520, 60,
        hwnd, (HMENU)IDC_MICROPHONE_DEVICE_COMBO, g_app.hInst, nullptr);
    ListView_SetExtendedListViewStyle(g_app.hMicrophoneDeviceList, LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT);

    LVCOLUMN lvcMic = {};
    lvcMic.mask = LVCF_TEXT | LVCF_WIDTH;
    lvcMic.cx = 500; lvcMic.pszText = (LPWSTR)L"Micr\u00f3fonos";
    ListView_InsertColumn(g_app.hMicrophoneDeviceList, 0, &lvcMic);

    // Carpeta de salida
    g_app.hOutputPathLabel = CreateWindow(L"STATIC", L"&Carpeta de salida:",
        WS_CHILD | WS_VISIBLE | SS_LEFT, 10, 300, 120, 20,
        hwnd, (HMENU)IDC_OUTPUT_PATH_LABEL, g_app.hInst, nullptr);

    g_app.hOutputPath = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", GetDefaultOutputPath().c_str(),
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_LEFT | ES_AUTOHSCROLL, 10, 320, 730, 25,
        hwnd, (HMENU)IDC_OUTPUT_PATH, g_app.hInst, nullptr);

    g_app.hBrowseBtn = CreateWindow(L"BUTTON", L"&Examinar...",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, 750, 320, 80, 25,
        hwnd, (HMENU)IDC_BROWSE_BTN, g_app.hInst, nullptr);

    // Modo de grabación
    g_app.hRecordingModeLabel = CreateWindow(L"STATIC", L"Modo de grabaci\u00f3n:",
        WS_CHILD | WS_VISIBLE | SS_LEFT, 10, 355, 160, 20,
        hwnd, (HMENU)IDC_RECORDING_MODE_LABEL, g_app.hInst, nullptr);

    g_app.hRecordingModeCombo = CreateWindow(WC_COMBOBOX, L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST, 170, 352, 180, 200,
        hwnd, (HMENU)IDC_RECORDING_MODE_COMBO, g_app.hInst, nullptr);
    SendMessage(g_app.hRecordingModeCombo, CB_ADDSTRING, 0, (LPARAM)L"Archivos separados");
    SendMessage(g_app.hRecordingModeCombo, CB_ADDSTRING, 0, (LPARAM)L"Archivo combinado");
    SendMessage(g_app.hRecordingModeCombo, CB_ADDSTRING, 0, (LPARAM)L"Ambos");
    SendMessage(g_app.hRecordingModeCombo, CB_SETCURSEL, 0, 0);

    // Monitoreo
    g_app.hPassthroughCheckbox = CreateWindow(L"BUTTON", L"Monitorear audio",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX, 370, 355, 150, 20,
        hwnd, (HMENU)IDC_PASSTHROUGH_CHECKBOX, g_app.hInst, nullptr);

    g_app.hPassthroughDeviceLabel = CreateWindow(L"STATIC", L"Dispositivo:",
        WS_CHILD | SS_LEFT, 530, 355, 80, 20,
        hwnd, (HMENU)IDC_PASSTHROUGH_DEVICE_LABEL, g_app.hInst, nullptr);

    g_app.hPassthroughDeviceCombo = CreateWindow(WC_COMBOBOX, L"",
        WS_CHILD | WS_TABSTOP | CBS_DROPDOWNLIST, 530, 352, 200, 200,
        hwnd, (HMENU)IDC_PASSTHROUGH_DEVICE_COMBO, g_app.hInst, nullptr);

    g_app.hMonitorOnlyCheckbox = CreateWindow(L"BUTTON", L"Solo monitorear (sin grabar)",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX, 370, 378, 250, 20,
        hwnd, (HMENU)IDC_MONITOR_ONLY_CHECKBOX, g_app.hInst, nullptr);

    // Botones de control
    g_app.hStartBtn = CreateWindow(L"BUTTON", L"&Iniciar captura",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, 10, 405, 200, 30,
        hwnd, (HMENU)IDC_START_BTN, g_app.hInst, nullptr);

    g_app.hStopAllBtn = CreateWindow(L"BUTTON", L"&Detener todo",
        WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON | WS_DISABLED, 220, 405, 120, 30,
        hwnd, (HMENU)IDC_STOP_ALL_BTN, g_app.hInst, nullptr);

    g_app.hPauseAllBtn = CreateWindow(L"BUTTON", L"Pausar todo",
        WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON | WS_DISABLED, 350, 405, 120, 30,
        hwnd, (HMENU)IDC_PAUSE_ALL_BTN, g_app.hInst, nullptr);

    g_app.hResumeAllBtn = CreateWindow(L"BUTTON", L"Reanudar todo",
        WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON | WS_DISABLED, 480, 405, 120, 30,
        hwnd, (HMENU)IDC_RESUME_ALL_BTN, g_app.hInst, nullptr);

    // Grabaciones activas
    g_app.hRecordingListLabel = CreateWindow(L"STATIC", L"Grabaciones activas:",
        WS_CHILD | WS_VISIBLE | SS_LEFT, 10, 445, 200, 20,
        hwnd, (HMENU)IDC_RECORDING_LIST_LABEL, g_app.hInst, nullptr);

    g_app.hRecordingList = CreateWindowEx(WS_EX_CLIENTEDGE, WC_LISTVIEW, L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | LVS_REPORT | LVS_SINGLESEL, 10, 465, 810, 150,
        hwnd, (HMENU)IDC_RECORDING_LIST, g_app.hInst, nullptr);

    lvc.cx = 180; lvc.pszText = (LPWSTR)L"Proceso";
    ListView_InsertColumn(g_app.hRecordingList, 0, &lvc);
    lvc.cx = 80; lvc.pszText = (LPWSTR)L"PID";
    ListView_InsertColumn(g_app.hRecordingList, 1, &lvc);
    lvc.cx = 380; lvc.pszText = (LPWSTR)L"Archivo";
    ListView_InsertColumn(g_app.hRecordingList, 2, &lvc);
    lvc.cx = 100; lvc.pszText = (LPWSTR)L"Tama\u00f1o";
    ListView_InsertColumn(g_app.hRecordingList, 3, &lvc);

    // Barra de estado
    g_app.hStatusText = CreateWindowEx(WS_EX_CLIENTEDGE, L"STATIC",
        L"Listo. Seleccione procesos y haga clic en Iniciar captura.",
        WS_CHILD | WS_VISIBLE | SS_LEFT, 10, 625, 810, 25,
        hwnd, (HMENU)IDC_STATUS_TEXT, g_app.hInst, nullptr);
}

// ========== Lista de procesos ==========
void RefreshProcessList() {
    if (!g_app.supportsProcessCapture) {
        SetWindowText(g_app.hStatusText, L"Modo de captura de audio del sistema. La captura por proceso no est\u00e1 disponible.");
        return;
    }

    std::vector<DWORD> checkedPIDs;
    int itemCount = ListView_GetItemCount(g_app.hProcessList);
    for (int i = 0; i < itemCount; i++) {
        if (ListView_GetCheckState(g_app.hProcessList, i)) {
            wchar_t pidStr[32];
            ListView_GetItemText(g_app.hProcessList, i, 1, pidStr, 32);
            checkedPIDs.push_back((DWORD)_wtoi(pidStr));
        }
    }

    ListView_DeleteAllItems(g_app.hProcessList);
    g_app.processes = g_app.processEnum->GetAllProcesses();

    std::sort(g_app.processes.begin(), g_app.processes.end(),
        [](const ProcessInfo& a, const ProcessInfo& b) {
            return StrCmpLogicalW(a.processName.c_str(), b.processName.c_str()) < 0;
        });

    bool showAudioOnly = (SendMessage(g_app.hShowAudioOnlyCheckbox, BM_GETCHECK, 0, 0) == BST_CHECKED);
    std::vector<DWORD> activePids;
    if (showAudioOnly) {
        activePids = g_app.processEnum->GetActiveAudioSessionPIDs();
    }
    UpdateProcessListLabel();
    int displayed = 0;

    if (!showAudioOnly) {
        LVITEM lvi = {}; lvi.mask = LVIF_TEXT; lvi.iItem = 0;
        lvi.pszText = (LPWSTR)L"[Audio del Sistema - Todos los procesos]";
        int idx = ListView_InsertItem(g_app.hProcessList, &lvi);
        ListView_SetItemText(g_app.hProcessList, idx, 1, (LPWSTR)L"0");
        ListView_SetItemText(g_app.hProcessList, idx, 2, (LPWSTR)L"Capturar todo el audio del sistema");
        ListView_SetItemText(g_app.hProcessList, idx, 3, (LPWSTR)L"Loopback del sistema");
        for (DWORD pid : checkedPIDs) {
            if (pid == 0) { ListView_SetCheckState(g_app.hProcessList, idx, TRUE); break; }
        }
        displayed++;
    }

    for (auto& proc : g_app.processes) {
        if (proc.windowTitle.empty()) proc.windowTitle = g_app.processEnum->GetWindowTitle(proc.processId);
        if (showAudioOnly) {
            bool isActive = false;
            for (DWORD pid : activePids) {
                if (pid == proc.processId) { isActive = true; break; }
            }
            proc.hasActiveAudio = isActive;
            if (!proc.hasActiveAudio) continue;
        }

        LVITEM lvi = {}; lvi.mask = LVIF_TEXT; lvi.iItem = displayed;
        lvi.pszText = (LPWSTR)proc.processName.c_str();
        int idx = ListView_InsertItem(g_app.hProcessList, &lvi);

        wchar_t pidStr[32];
        swprintf_s(pidStr, L"%lu", proc.processId);
        ListView_SetItemText(g_app.hProcessList, idx, 1, pidStr);
        ListView_SetItemText(g_app.hProcessList, idx, 2, (LPWSTR)proc.windowTitle.c_str());
        ListView_SetItemText(g_app.hProcessList, idx, 3, (LPWSTR)proc.executablePath.c_str());

        for (DWORD pid : checkedPIDs) {
            if (proc.processId == pid) { ListView_SetCheckState(g_app.hProcessList, idx, TRUE); break; }
        }
        displayed++;
    }

    std::wstring msg = L"Lista actualizada. Mostrando " + std::to_wstring(displayed) + L" proceso(s)";
    if (showAudioOnly) msg += L" con audio activo";
    msg += L".";
    SetWindowText(g_app.hStatusText, msg.c_str());
}

void UpdateProcessListLabel() {
    if (!g_app.supportsProcessCapture || !g_app.hProcessListLabel) return;
    bool audioOnly = (SendMessage(g_app.hShowAudioOnlyCheckbox, BM_GETCHECK, 0, 0) == BST_CHECKED);
    SetWindowText(g_app.hProcessListLabel, audioOnly ? L"&Procesos con audio activo:" : L"&Procesos disponibles:");
}
