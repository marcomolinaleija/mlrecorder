#pragma once

#include <windows.h>
#include <commctrl.h>
#include <shlwapi.h>
#include <shlobj.h>
#include <shellapi.h>
#include <string>
#include <vector>
#include <memory>
#include <fstream>
#include <algorithm>
#include <nlohmann/json.hpp>

#include "resource.h"
#include "ProcessEnumerator.h"
#include "CaptureManager.h"
#include "AudioDeviceEnumerator.h"

using json = nlohmann::json;

// ========== Estado global de la aplicación ==========
struct AppState {
    HINSTANCE hInst = nullptr;
    HWND hWnd = nullptr;
    HACCEL hAccel = nullptr;

    // Controles principales
    HWND hProcessList = nullptr;
    HWND hProcessListLabel = nullptr;
    HWND hRefreshBtn = nullptr;
    HWND hShowAudioOnlyCheckbox = nullptr;

    // Micrófono
    HWND hMicrophoneCheckbox = nullptr;
    HWND hMicrophoneDeviceList = nullptr;
    HWND hMicrophoneDeviceLabel = nullptr;

    // Grabación
    HWND hOutputPath = nullptr;
    HWND hOutputPathLabel = nullptr;
    HWND hBrowseBtn = nullptr;
    HWND hRecordingModeCombo = nullptr;
    HWND hRecordingModeLabel = nullptr;

    // Botones de control
    HWND hStartBtn = nullptr;
    HWND hStopAllBtn = nullptr;
    HWND hPauseAllBtn = nullptr;
    HWND hResumeAllBtn = nullptr;

    // Grabaciones activas
    HWND hRecordingList = nullptr;
    HWND hRecordingListLabel = nullptr;

    // Monitoreo
    HWND hPassthroughCheckbox = nullptr;
    HWND hPassthroughDeviceCombo = nullptr;
    HWND hPassthroughDeviceLabel = nullptr;
    HWND hMonitorOnlyCheckbox = nullptr;

    // Volumen
    HWND hProcessVolumeSlider = nullptr;
    HWND hProcessVolumeLabel = nullptr;
    HWND hMicrophoneVolumeSlider = nullptr;
    HWND hMicrophoneVolumeLabel = nullptr;

    // Estado
    HWND hStatusText = nullptr;
    HWND hLastFocusedCtrl = nullptr;

    // Configuración de calidad (guardada internamente, editada en diálogo)
    int formatIndex = 0;           // 0=WAV, 1=MP3, 2=Opus, 3=FLAC
    int mp3BitrateIndex = 1;       // 0=128, 1=192, 2=256, 3=320
    int opusBitrateIndex = 2;      // 0=64, 1=96, 2=128, 3=192, 4=256
    int flacCompressionIndex = 5;  // 0-8
    bool skipSilence = false;

    // Volumen
    float processVolume = 100.0f;
    float microphoneVolume = 100.0f;

    // Lógica
    std::unique_ptr<ProcessEnumerator> processEnum;
    std::unique_ptr<CaptureManager> captureManager;
    std::unique_ptr<AudioDeviceEnumerator> audioDeviceEnum;
    std::vector<ProcessInfo> processes;

    bool isAppActive = true;
    bool captureButtonStops = false;
    bool restoreFocusOnActivate = false;
    bool supportsProcessCapture = false;
    bool useWinRT = false;

    // Bandeja del sistema
    NOTIFYICONDATA nid = {};
    bool isMinimizedToTray = false;
};

extern AppState g_app;

// ========== Funciones de la aplicación ==========

// Inicialización
void InitializeControls(HWND hwnd);
HMENU CreateMainMenu();

// Lista de procesos
void RefreshProcessList();
void UpdateProcessListLabel();

// Captura
void StartCapture();
void StopCapture();
void UpdateCaptureVolumes();
void UpdateRecordingList();
void EnsureRecordingListFocusItem();

// Navegación
void BrowseOutputFolder();

// Dispositivos
void PopulatePassthroughDevices();
void OnPassthroughCheckboxChanged();
void OnMonitorOnlyCheckboxChanged();
void PopulateMicrophoneDevices();
void OnMicrophoneCheckboxChanged();

// Bandeja del sistema
void AddTrayIcon();
void RemoveTrayIcon();
void ShowTrayContextMenu();
void ShowWindowFromTray();
void HideWindowToTray();

// Configuración
void LoadSettings();
void SaveSettings();
std::wstring GetSettingsFilePath();

// Diálogo de calidad
void ShowQualityDialog(HWND parent);

// Utilidades
std::wstring GetDefaultOutputPath();
std::wstring FormatFileSize(UINT64 bytes);
std::wstring NormalizeOutputPath(const std::wstring& path);
bool EnsureDirectoryExists(const std::wstring& path);
std::wstring SanitizeFileName(const std::wstring& name);
std::string WStringToString(const std::wstring& wstr);
std::wstring StringToWString(const std::string& str);

// Helpers de micrófono
std::vector<size_t> GetCheckedMicrophoneDeviceIndices();
std::vector<std::wstring> GetCheckedMicrophoneDeviceIds();
