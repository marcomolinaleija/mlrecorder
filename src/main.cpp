#include "App.h"
#include <roapi.h>

#pragma comment(lib, "comctl32.lib")
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

AppState g_app;

const wchar_t CLASS_NAME[] = L"MLRecorderWindow";
const UINT WM_APP_RESTORE_FOCUS = WM_APP + 1;

bool IsWindows8OrGreater() {
    typedef LONG (WINAPI* RtlGetVersionPtr)(PRTL_OSVERSIONINFOW);
    HMODULE hNtdll = GetModuleHandleW(L"ntdll.dll");
    if (!hNtdll) return false;
    RtlGetVersionPtr fn = (RtlGetVersionPtr)GetProcAddress(hNtdll, "RtlGetVersion");
    if (!fn) return false;
    RTL_OSVERSIONINFOW osvi = { sizeof(osvi) };
    if (fn(&osvi) == 0)
        return (osvi.dwMajorVersion > 6) || (osvi.dwMajorVersion == 6 && osvi.dwMinorVersion >= 2);
    return false;
}

bool SupportsProcessCapture() {
    typedef LONG (WINAPI* RtlGetVersionPtr)(PRTL_OSVERSIONINFOW);
    HMODULE hNtdll = GetModuleHandleW(L"ntdll.dll");
    if (!hNtdll) return false;
    RtlGetVersionPtr fn = (RtlGetVersionPtr)GetProcAddress(hNtdll, "RtlGetVersion");
    if (!fn) return false;
    RTL_OSVERSIONINFOW osvi = { sizeof(osvi) };
    if (fn(&osvi) == 0)
        return (osvi.dwMajorVersion == 10 && osvi.dwBuildNumber >= 19041) || (osvi.dwMajorVersion > 10);
    return false;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_ACTIVATE: {
        if (LOWORD(wParam) == WA_INACTIVE) {
            g_app.hLastFocusedCtrl = GetFocus();
            g_app.restoreFocusOnActivate = true;
        } else if (g_app.restoreFocusOnActivate) {
            HWND target = g_app.hLastFocusedCtrl;
            if (!target || !IsWindow(target))
                target = g_app.supportsProcessCapture ? g_app.hProcessList : g_app.hStartBtn;
            if (target && IsWindow(target))
                PostMessage(hwnd, WM_APP_RESTORE_FOCUS, (WPARAM)target, 0);
            g_app.restoreFocusOnActivate = false;
        }
        return 0;
    }

    case WM_ACTIVATEAPP:
        g_app.isAppActive = (wParam != 0);
        if (g_app.isAppActive && g_app.restoreFocusOnActivate) {
            HWND target = g_app.hLastFocusedCtrl;
            if (!target || !IsWindow(target))
                target = g_app.supportsProcessCapture ? g_app.hProcessList : g_app.hStartBtn;
            if (target && IsWindow(target))
                PostMessage(hwnd, WM_APP_RESTORE_FOCUS, (WPARAM)target, 0);
            g_app.restoreFocusOnActivate = false;
        } else if (!g_app.isAppActive) {
            g_app.restoreFocusOnActivate = true;
        }
        return 0;

    case WM_APP_RESTORE_FOCUS: {
        HWND target = (HWND)wParam;
        if (target && IsWindow(target)) {
            SetFocus(target);
            if (target == g_app.hRecordingList) EnsureRecordingListFocusItem();
        }
        return 0;
    }

    case WM_SETFOCUS: {
        HWND target = g_app.hLastFocusedCtrl;
        if (!target || !IsWindow(target))
            target = g_app.supportsProcessCapture ? g_app.hProcessList : g_app.hStartBtn;
        SetFocus(target);
        if (target == g_app.hRecordingList) EnsureRecordingListFocusItem();
        return 0;
    }

    case WM_CREATE:
        SetMenu(hwnd, CreateMainMenu());
        InitializeControls(hwnd);
        LoadSettings();
        RefreshProcessList();
        SetFocus(g_app.supportsProcessCapture ? g_app.hProcessList : g_app.hStartBtn);
        SetTimer(hwnd, 1, 500, nullptr);
        PostMessage(hwnd, WM_USER + 1, 0, 0);
        return 0;

    case WM_USER + 1:
        PopulatePassthroughDevices();
        PopulateMicrophoneDevices();
        return 0;

    case WM_NOTIFY: {
        LPNMHDR hdr = (LPNMHDR)lParam;
        if (hdr && hdr->hwndFrom == g_app.hRecordingList && hdr->code == LVN_KEYDOWN) {
            NMLVKEYDOWN* key = (NMLVKEYDOWN*)lParam;
            if (key->wVKey == VK_TAB) {
                bool shiftDown = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
                HWND next = GetNextDlgTabItem(hwnd, g_app.hRecordingList, shiftDown);
                if (next && IsWindow(next)) {
                    SendMessage(hwnd, WM_NEXTDLGCTL, (WPARAM)next, TRUE);
                    return TRUE;
                }
            }
        }
        break;
    }

    case WM_COMMAND: {
        int wmId = LOWORD(wParam);
        int wmEvent = HIWORD(wParam);
        switch (wmId) {
        case IDC_REFRESH_BTN:
            RefreshProcessList();
            break;
        case IDC_START_BTN:
            SetFocus(g_app.hStartBtn);
            if (g_app.captureButtonStops) StopCapture(); else StartCapture();
            break;
        case IDC_STOP_ALL_BTN:
            if (MessageBox(g_app.hWnd, L"\u00bfDetener todas las capturas activas?", L"Confirmar", MB_YESNO | MB_ICONQUESTION) == IDYES) {
                g_app.captureManager->StopAllCaptures();
                g_app.captureManager->DisableMixedRecording();
                EnableWindow(g_app.hStopAllBtn, FALSE);
                ShowWindow(g_app.hStopAllBtn, SW_HIDE);
                UpdateRecordingList();
                SetWindowText(g_app.hStatusText, L"Todas las capturas detenidas.");
                SetFocus(g_app.supportsProcessCapture ? g_app.hProcessList : g_app.hStartBtn);
            }
            break;
        case IDC_PAUSE_ALL_BTN:
            g_app.captureManager->PauseAllCaptures();
            SetWindowText(g_app.hStatusText, L"Todas las capturas pausadas.");
            UpdateRecordingList();
            SetFocus(g_app.supportsProcessCapture ? g_app.hProcessList : g_app.hStartBtn);
            break;
        case IDC_RESUME_ALL_BTN:
            g_app.captureManager->ResumeAllCaptures();
            SetWindowText(g_app.hStatusText, L"Todas las capturas reanudadas.");
            UpdateRecordingList();
            SetFocus(g_app.supportsProcessCapture ? g_app.hProcessList : g_app.hStartBtn);
            break;
        case IDC_BROWSE_BTN:
            BrowseOutputFolder();
            break;
        case IDC_SHOW_AUDIO_ONLY_CHECKBOX:
            if (wmEvent == BN_CLICKED) {
                RefreshProcessList();
                if (g_app.supportsProcessCapture) SetFocus(g_app.hProcessList);
            }
            break;
        case IDC_PASSTHROUGH_CHECKBOX:
            if (wmEvent == BN_CLICKED) OnPassthroughCheckboxChanged();
            break;
        case IDC_MONITOR_ONLY_CHECKBOX:
            if (wmEvent == BN_CLICKED) OnMonitorOnlyCheckboxChanged();
            break;
        case IDC_MICROPHONE_CHECKBOX:
            if (wmEvent == BN_CLICKED) OnMicrophoneCheckboxChanged();
            break;

        // Menú
        case IDM_FILE_OPEN_FOLDER: {
            wchar_t path[MAX_PATH];
            GetWindowText(g_app.hOutputPath, path, MAX_PATH);
            std::wstring folder = NormalizeOutputPath(path);
            if (!folder.empty()) {
                ShellExecuteW(nullptr, L"open", folder.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
            }
            break;
        }
        case IDM_FILE_EXIT:
            PostMessage(hwnd, WM_CLOSE, 0, 0);
            break;
        case IDM_CONFIG_QUALITY:
            ShowQualityDialog(hwnd);
            break;
        case IDM_HELP_ABOUT:
            MessageBox(hwnd,
                L"ML Recorder v1.0\n\n"
                L"Grabador de audio por proceso.\n"
                L"Captura audio aislado de cada aplicaci\u00f3n,\n"
                L"audio del sistema y micr\u00f3fono.\n\n"
                L"Basado en WASAPI (Windows Audio Session API).\n"
                L"Compatible con lectores de pantalla.",
                L"Acerca de ML Recorder", MB_OK | MB_ICONINFORMATION);
            break;

        case IDM_TRAY_SHOW:
            ShowWindowFromTray();
            break;
        case IDM_TRAY_EXIT:
            RemoveTrayIcon();
            PostQuitMessage(0);
            break;
        }
        return 0;
    }

    case WM_TRAYICON: {
        switch (lParam) {
        case WM_LBUTTONUP:
        case WM_LBUTTONDBLCLK:
            ShowWindowFromTray();
            break;
        case WM_RBUTTONUP:
            ShowTrayContextMenu();
            break;
        }
        return 0;
    }

    case WM_SIZE: {
        if (wParam == SIZE_MINIMIZED) { HideWindowToTray(); return 0; }

        RECT rc;
        GetClientRect(hwnd, &rc);
        int w = rc.right, h = rc.bottom;
        int topOffset = g_app.supportsProcessCapture ? 235 : 10;

        if (g_app.supportsProcessCapture) {
            SetWindowPos(g_app.hProcessListLabel, nullptr, 10, 5, 200, 20, SWP_NOZORDER);
            SetWindowPos(g_app.hProcessList, nullptr, 10, 25, w - 20, 160, SWP_NOZORDER);
            SetWindowPos(g_app.hRefreshBtn, nullptr, 10, 190, 100, 25, SWP_NOZORDER);
            SetWindowPos(g_app.hShowAudioOnlyCheckbox, nullptr, 120, 193, 320, 20, SWP_NOZORDER);
        }

        // Micrófono
        SetWindowPos(g_app.hMicrophoneCheckbox, nullptr, 10, topOffset, 170, 20, SWP_NOZORDER);
        SetWindowPos(g_app.hMicrophoneDeviceLabel, nullptr, 190, topOffset, 120, 20, SWP_NOZORDER);
        SetWindowPos(g_app.hMicrophoneDeviceList, nullptr, 310, topOffset - 3, w - 320, 60, SWP_NOZORDER);

        int grabOffset = topOffset + 65;

        // Carpeta de salida
        SetWindowPos(g_app.hOutputPathLabel, nullptr, 10, grabOffset, 120, 20, SWP_NOZORDER);
        SetWindowPos(g_app.hOutputPath, nullptr, 10, grabOffset + 20, w - 110, 25, SWP_NOZORDER);
        SetWindowPos(g_app.hBrowseBtn, nullptr, w - 90, grabOffset + 20, 80, 25, SWP_NOZORDER);

        // Modo + Monitoreo
        SetWindowPos(g_app.hRecordingModeLabel, nullptr, 10, grabOffset + 55, 160, 20, SWP_NOZORDER);
        SetWindowPos(g_app.hRecordingModeCombo, nullptr, 170, grabOffset + 52, 180, 200, SWP_NOZORDER);
        SetWindowPos(g_app.hPassthroughCheckbox, nullptr, 370, grabOffset + 55, 150, 20, SWP_NOZORDER);
        SetWindowPos(g_app.hPassthroughDeviceLabel, nullptr, 530, grabOffset + 55, 100, 20, SWP_NOZORDER);
        SetWindowPos(g_app.hPassthroughDeviceCombo, nullptr, 530, grabOffset + 52, 200, 200, SWP_NOZORDER);
        SetWindowPos(g_app.hMonitorOnlyCheckbox, nullptr, 370, grabOffset + 78, 250, 20, SWP_NOZORDER);

        // Botones
        int btnY = grabOffset + 105;
        SetWindowPos(g_app.hStartBtn, nullptr, 10, btnY, 200, 30, SWP_NOZORDER);
        SetWindowPos(g_app.hStopAllBtn, nullptr, 220, btnY, 120, 30, SWP_NOZORDER);
        SetWindowPos(g_app.hPauseAllBtn, nullptr, 350, btnY, 120, 30, SWP_NOZORDER);
        SetWindowPos(g_app.hResumeAllBtn, nullptr, 480, btnY, 120, 30, SWP_NOZORDER);

        // Grabaciones activas
        int listY = btnY + 40;
        SetWindowPos(g_app.hRecordingListLabel, nullptr, 10, listY, 200, 20, SWP_NOZORDER);
        SetWindowPos(g_app.hRecordingList, nullptr, 10, listY + 20, w - 20, h - listY - 60, SWP_NOZORDER);

        // Estado
        SetWindowPos(g_app.hStatusText, nullptr, 10, h - 30, w - 20, 25, SWP_NOZORDER);
        return 0;
    }

    case WM_DESTROY:
        RemoveTrayIcon();
        SaveSettings();
        g_app.captureManager->StopAllCaptures();
        PostQuitMessage(0);
        return 0;

    case WM_TIMER:
        if (wParam == 1) UpdateRecordingList();
        return 0;
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow) {
    g_app.hInst = hInstance;
    g_app.supportsProcessCapture = SupportsProcessCapture();

    HRESULT hr;
    g_app.useWinRT = IsWindows8OrGreater();
    if (g_app.useWinRT) {
        hr = RoInitialize(RO_INIT_SINGLETHREADED);
        if (FAILED(hr) && hr != RPC_E_CHANGED_MODE && hr != S_FALSE) return 0;
    } else {
        hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        if (FAILED(hr) && hr != RPC_E_CHANGED_MODE && hr != S_FALSE) return 0;
    }

    INITCOMMONCONTROLSEX icex = { sizeof(icex), ICC_LISTVIEW_CLASSES | ICC_STANDARD_CLASSES | ICC_BAR_CLASSES };
    InitCommonControlsEx(&icex);

    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    RegisterClass(&wc);

    g_app.hWnd = CreateWindowEx(0, CLASS_NAME,
        g_app.supportsProcessCapture ? L"ML Recorder - Grabaci\u00f3n por Proceso" : L"ML Recorder - Audio del Sistema",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 850, 700,
        nullptr, nullptr, hInstance, nullptr);

    if (!g_app.hWnd) return 0;

    g_app.processEnum = std::make_unique<ProcessEnumerator>();
    g_app.captureManager = std::make_unique<CaptureManager>();
    g_app.audioDeviceEnum = std::make_unique<AudioDeviceEnumerator>();

    g_app.hAccel = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDR_ACCELERATOR));

    ShowWindow(g_app.hWnd, nCmdShow);
    UpdateWindow(g_app.hWnd);

    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0)) {
        if (!TranslateAccelerator(g_app.hWnd, g_app.hAccel, &msg)) {
            if (!IsDialogMessage(g_app.hWnd, &msg)) {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
    }

    if (g_app.useWinRT) RoUninitialize(); else CoUninitialize();
    return 0;
}
