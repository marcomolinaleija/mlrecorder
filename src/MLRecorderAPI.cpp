#include "MLRecorderAPI.h"

#include "AudioDeviceEnumerator.h"
#include "CaptureManager.h"
#include "ProcessEnumerator.h"

#include <windows.h>
#include <roapi.h>
#include <shlobj.h>

#include <algorithm>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace {

constexpr DWORD kMicSessionBase = 0xFFFF0000u;

struct RuntimeState {
    std::mutex mutex;
    bool initialized = false;
    DWORD ownerThreadId = 0;
    bool roInitializedByUs = false;
    bool coInitializedByUs = false;
    std::string lastError;
    std::unique_ptr<CaptureManager> captureManager;
    std::unique_ptr<ProcessEnumerator> processEnumerator;
    std::unique_ptr<AudioDeviceEnumerator> audioDeviceEnumerator;
    std::unordered_map<std::wstring, DWORD> activeInputSessions;
};

RuntimeState& State() {
    static RuntimeState s;
    return s;
}

void SetLastErrorLocked(const std::string& message) {
    State().lastError = message;
}

std::string WideToUtf8(const std::wstring& input) {
    if (input.empty()) {
        return std::string();
    }
    const int size = WideCharToMultiByte(CP_UTF8, 0, input.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (size <= 1) {
        return std::string();
    }
    std::string output(static_cast<size_t>(size - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, input.c_str(), -1, &output[0], size, nullptr, nullptr);
    return output;
}

std::wstring Utf8ToWide(const char* input) {
    if (input == nullptr || *input == '\0') {
        return std::wstring();
    }
    const int size = MultiByteToWideChar(CP_UTF8, 0, input, -1, nullptr, 0);
    if (size <= 1) {
        return std::wstring();
    }
    std::wstring output(static_cast<size_t>(size - 1), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, input, -1, &output[0], size);
    return output;
}

std::wstring SanitizeFileName(const std::wstring& name) {
    if (name.empty()) {
        return L"Process";
    }

    std::wstring sanitized = name;
    const wchar_t* invalidChars = L"\\/:*?\"<>|";
    for (wchar_t& c : sanitized) {
        if (wcschr(invalidChars, c) != nullptr) {
            c = L'_';
        }
    }

    while (!sanitized.empty() && (sanitized.back() == L' ' || sanitized.back() == L'.')) {
        sanitized.pop_back();
    }
    if (sanitized.empty()) {
        return L"Process";
    }
    return sanitized;
}

bool EnsureDirectoryExists(const std::wstring& directoryPath) {
    if (directoryPath.empty()) {
        return false;
    }

    const int result = SHCreateDirectoryExW(nullptr, directoryPath.c_str(), nullptr);
    if (result == ERROR_SUCCESS || result == ERROR_ALREADY_EXISTS) {
        const DWORD attrs = GetFileAttributesW(directoryPath.c_str());
        return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0;
    }
    return false;
}

std::wstring GetFileExtension(AudioFormat format) {
    switch (format) {
    case AudioFormat::MP3:
        return L".mp3";
    case AudioFormat::OPUS:
        return L".opus";
    case AudioFormat::FLAC:
        return L".flac";
    case AudioFormat::WAV:
    default:
        return L".wav";
    }
}

std::wstring TimestampForFilename() {
    SYSTEMTIME st{};
    GetLocalTime(&st);
    wchar_t buffer[64] = {0};
    swprintf_s(
        buffer,
        L"%04d_%02d_%02d-%02d_%02d_%02d",
        st.wYear,
        st.wMonth,
        st.wDay,
        st.wHour,
        st.wMinute,
        st.wSecond);
    return buffer;
}

AudioFormat ConvertFormat(int format, bool* ok) {
    if (ok != nullptr) {
        *ok = true;
    }
    switch (format) {
    case MLR_FORMAT_WAV:
        return AudioFormat::WAV;
    case MLR_FORMAT_MP3:
        return AudioFormat::MP3;
    case MLR_FORMAT_OPUS:
        return AudioFormat::OPUS;
    case MLR_FORMAT_FLAC:
        return AudioFormat::FLAC;
    default:
        if (ok != nullptr) {
            *ok = false;
        }
        return AudioFormat::WAV;
    }
}

bool IsWindows8OrGreater() {
    typedef LONG(WINAPI * RtlGetVersionPtr)(PRTL_OSVERSIONINFOW);
    HMODULE hNtdll = GetModuleHandleW(L"ntdll.dll");
    if (hNtdll == nullptr) {
        return false;
    }

    RtlGetVersionPtr fn = reinterpret_cast<RtlGetVersionPtr>(GetProcAddress(hNtdll, "RtlGetVersion"));
    if (fn == nullptr) {
        return false;
    }

    RTL_OSVERSIONINFOW osvi = {sizeof(osvi)};
    if (fn(&osvi) != 0) {
        return false;
    }

    return (osvi.dwMajorVersion > 6) || (osvi.dwMajorVersion == 6 && osvi.dwMinorVersion >= 2);
}

int EnsureInitializedAndOwnedThreadLocked() {
    RuntimeState& state = State();
    if (!state.initialized) {
        SetLastErrorLocked("MLRecorder runtime is not initialized");
        return MLR_E_NOT_INITIALIZED;
    }
    if (state.ownerThreadId != GetCurrentThreadId()) {
        SetLastErrorLocked("API call must run on the same thread used for mlr_initialize");
        return MLR_E_WRONG_THREAD;
    }
    return MLR_OK;
}

bool ResolveProcessNameLocked(uint32_t processId, std::wstring* processName) {
    if (processName == nullptr) {
        return false;
    }

    if (processId == 0) {
        *processName = L"SystemAudio";
        return true;
    }

    RuntimeState& state = State();
    const std::vector<ProcessInfo> processes = state.processEnumerator->GetAllProcesses();
    for (const ProcessInfo& process : processes) {
        if (process.processId == processId) {
            *processName = process.processName;
            return true;
        }
    }
    return false;
}

bool ResolveInputDeviceLocked(
    const std::wstring& requestedDeviceId,
    AudioDeviceInfo* outDevice,
    size_t* outIndex) {
    RuntimeState& state = State();
    if (!state.audioDeviceEnumerator) {
        state.audioDeviceEnumerator = std::make_unique<AudioDeviceEnumerator>();
    }

    if (!state.audioDeviceEnumerator->EnumerateInputDevices()) {
        return false;
    }

    const std::vector<AudioDeviceInfo>& devices = state.audioDeviceEnumerator->GetInputDevices();
    if (devices.empty()) {
        return false;
    }

    size_t selectedIndex = static_cast<size_t>(-1);
    if (!requestedDeviceId.empty()) {
        for (size_t i = 0; i < devices.size(); i++) {
            if (devices[i].deviceId == requestedDeviceId) {
                selectedIndex = i;
                break;
            }
        }
    } else {
        for (size_t i = 0; i < devices.size(); i++) {
            if (devices[i].isDefault) {
                selectedIndex = i;
                break;
            }
        }
        if (selectedIndex == static_cast<size_t>(-1)) {
            selectedIndex = 0;
        }
    }

    if (selectedIndex == static_cast<size_t>(-1) || selectedIndex >= devices.size()) {
        return false;
    }

    if (outDevice != nullptr) {
        *outDevice = devices[selectedIndex];
    }
    if (outIndex != nullptr) {
        *outIndex = selectedIndex;
    }
    return true;
}

DWORD BuildMicSessionId(size_t deviceIndex) {
    return kMicSessionBase + static_cast<DWORD>(deviceIndex);
}

void RemoveInputSessionBySessionIdLocked(DWORD sessionId) {
    RuntimeState& state = State();
    for (auto it = state.activeInputSessions.begin(); it != state.activeInputSessions.end(); ++it) {
        if (it->second == sessionId) {
            state.activeInputSessions.erase(it);
            return;
        }
    }
}

int StartCaptureLocked(
    uint32_t processId,
    const std::wstring& processName,
    const std::wstring& outputPath,
    AudioFormat format,
    uint32_t bitrate,
    bool skipSilence,
    const std::wstring& passthroughDeviceId,
    bool monitorOnly,
    bool strictIsolation) {
    RuntimeState& state = State();

    if (state.captureManager->IsCapturing(processId)) {
        SetLastErrorLocked("Process is already being captured");
        return MLR_E_ALREADY_CAPTURING;
    }

    const bool started = state.captureManager->StartCapture(
        processId,
        processName,
        outputPath,
        format,
        bitrate,
        skipSilence,
        passthroughDeviceId,
        monitorOnly);

    if (!started) {
        SetLastErrorLocked("Failed to start capture");
        return MLR_E_START_FAILED;
    }

    if (strictIsolation && processId != 0) {
        const std::vector<CaptureSession*> sessions = state.captureManager->GetActiveSessions();
        for (CaptureSession* session : sessions) {
            if (session == nullptr || session->processId != processId || session->capture == nullptr) {
                continue;
            }

            if (!session->capture->IsProcessSpecific()) {
                state.captureManager->StopCapture(processId);
                SetLastErrorLocked("Process-specific loopback is unavailable for this process/OS. Capture aborted due to strict isolation.");
                return MLR_E_PROCESS_ISOLATION_UNAVAILABLE;
            }
            break;
        }
    }

    SetLastErrorLocked(std::string());
    return MLR_OK;
}

int StartMicrophoneCaptureLocked(
    const std::wstring& requestedDeviceId,
    const std::wstring& outputPath,
    AudioFormat format,
    uint32_t bitrate,
    bool skipSilence,
    bool monitorOnly) {
    RuntimeState& state = State();

    AudioDeviceInfo inputDevice{};
    size_t inputIndex = 0;
    if (!ResolveInputDeviceLocked(requestedDeviceId, &inputDevice, &inputIndex)) {
        SetLastErrorLocked("Input device not found");
        return MLR_E_DEVICE_NOT_FOUND;
    }

    const DWORD sessionId = BuildMicSessionId(inputIndex);
    if (state.captureManager->IsCapturing(sessionId)) {
        SetLastErrorLocked("Input device is already being captured");
        return MLR_E_ALREADY_CAPTURING;
    }

    const bool started = state.captureManager->StartCaptureFromDevice(
        sessionId,
        inputDevice.friendlyName,
        inputDevice.deviceId,
        true,
        outputPath,
        format,
        bitrate,
        skipSilence,
        monitorOnly);

    if (!started) {
        SetLastErrorLocked("Failed to start microphone capture");
        return MLR_E_START_FAILED;
    }

    state.activeInputSessions[inputDevice.deviceId] = sessionId;
    SetLastErrorLocked(std::string());
    return MLR_OK;
}

}  // namespace

int mlr_initialize(void) {
    RuntimeState& state = State();
    std::lock_guard<std::mutex> lock(state.mutex);

    if (state.initialized) {
        if (state.ownerThreadId != GetCurrentThreadId()) {
            SetLastErrorLocked("Runtime already initialized on another thread");
            return MLR_E_WRONG_THREAD;
        }
        return MLR_OK;
    }

    if (IsWindows8OrGreater()) {
        const HRESULT hr = RoInitialize(RO_INIT_SINGLETHREADED);
        if (SUCCEEDED(hr) || hr == S_FALSE) {
            state.roInitializedByUs = true;
        }
    }

    // CoInitializeEx is still needed for non-WinRT COM usage.
    const HRESULT coHr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (SUCCEEDED(coHr) || coHr == S_FALSE) {
        state.coInitializedByUs = true;
    } else if (coHr == RPC_E_CHANGED_MODE) {
        // COM already initialized in another mode by the host process.
        state.coInitializedByUs = false;
    } else {
        if (state.roInitializedByUs) {
            RoUninitialize();
            state.roInitializedByUs = false;
        }
        SetLastErrorLocked("Failed to initialize COM runtime");
        return MLR_E_INTERNAL;
    }

    try {
        state.captureManager = std::make_unique<CaptureManager>();
        state.processEnumerator = std::make_unique<ProcessEnumerator>();
        state.audioDeviceEnumerator = std::make_unique<AudioDeviceEnumerator>();
    } catch (...) {
        if (state.coInitializedByUs) {
            CoUninitialize();
            state.coInitializedByUs = false;
        }
        if (state.roInitializedByUs) {
            RoUninitialize();
            state.roInitializedByUs = false;
        }
        SetLastErrorLocked("Failed to allocate runtime objects");
        return MLR_E_INTERNAL;
    }

    state.ownerThreadId = GetCurrentThreadId();
    state.initialized = true;
    SetLastErrorLocked(std::string());
    return MLR_OK;
}

void mlr_shutdown(void) {
    RuntimeState& state = State();
    std::lock_guard<std::mutex> lock(state.mutex);

    if (!state.initialized) {
        return;
    }

    if (state.ownerThreadId != GetCurrentThreadId()) {
        SetLastErrorLocked("mlr_shutdown must be called from the initialization thread");
        return;
    }

    if (state.captureManager) {
        state.captureManager->StopAllCaptures();
        state.captureManager->DisableMixedRecording();
    }
    state.activeInputSessions.clear();
    state.captureManager.reset();
    state.processEnumerator.reset();
    state.audioDeviceEnumerator.reset();

    if (state.coInitializedByUs) {
        CoUninitialize();
        state.coInitializedByUs = false;
    }
    if (state.roInitializedByUs) {
        RoUninitialize();
        state.roInitializedByUs = false;
    }

    state.ownerThreadId = 0;
    state.initialized = false;
    state.lastError.clear();
}

int mlr_is_initialized(void) {
    RuntimeState& state = State();
    std::lock_guard<std::mutex> lock(state.mutex);
    return state.initialized ? 1 : 0;
}

const char* mlr_result_to_string(int code) {
    switch (code) {
    case MLR_OK:
        return "OK";
    case MLR_E_INVALID_ARGUMENT:
        return "Invalid argument";
    case MLR_E_NOT_INITIALIZED:
        return "Runtime not initialized";
    case MLR_E_WRONG_THREAD:
        return "Called from wrong thread";
    case MLR_E_ALREADY_CAPTURING:
        return "Already capturing";
    case MLR_E_START_FAILED:
        return "Failed to start capture";
    case MLR_E_STOP_FAILED:
        return "Failed to stop capture";
    case MLR_E_PROCESS_NOT_FOUND:
        return "Process not found";
    case MLR_E_PROCESS_ISOLATION_UNAVAILABLE:
        return "Process isolation unavailable";
    case MLR_E_INTERNAL:
        return "Internal error";
    case MLR_E_DEVICE_NOT_FOUND:
        return "Input device not found";
    case MLR_E_MIXED_RECORDING_FAILED:
        return "Mixed recording failed";
    default:
        return "Unknown error";
    }
}

const char* mlr_get_last_error(void) {
    RuntimeState& state = State();
    std::lock_guard<std::mutex> lock(state.mutex);
    static thread_local std::string threadLocalCopy;
    threadLocalCopy = state.lastError;
    return threadLocalCopy.c_str();
}

int mlr_list_processes(int only_active_audio, MLR_ProcessCallback callback, void* user_data) {
    RuntimeState& state = State();
    std::lock_guard<std::mutex> lock(state.mutex);

    const int initStatus = EnsureInitializedAndOwnedThreadLocked();
    if (initStatus != MLR_OK) {
        return initStatus;
    }
    if (callback == nullptr) {
        SetLastErrorLocked("callback is null");
        return MLR_E_INVALID_ARGUMENT;
    }

    std::unordered_set<DWORD> activeSet;
    if (only_active_audio != 0) {
        const std::vector<DWORD> activePids = state.processEnumerator->GetActiveAudioSessionPIDs();
        activeSet.insert(activePids.begin(), activePids.end());
    }

    int emitted = 0;
    const std::vector<ProcessInfo> processes = state.processEnumerator->GetAllProcesses();
    for (const ProcessInfo& process : processes) {
        const bool hasActiveAudio = activeSet.find(process.processId) != activeSet.end();
        if (only_active_audio != 0 && !hasActiveAudio) {
            continue;
        }

        std::wstring title = state.processEnumerator->GetWindowTitle(process.processId);
        std::string processNameUtf8 = WideToUtf8(process.processName);
        std::string titleUtf8 = WideToUtf8(title);

        MLR_ProcessInfo info{};
        info.process_id = process.processId;
        info.process_name_utf8 = processNameUtf8.c_str();
        info.window_title_utf8 = titleUtf8.c_str();
        info.has_active_audio = hasActiveAudio ? 1 : 0;

        const int callbackResult = callback(&info, user_data);
        emitted++;
        if (callbackResult != 0) {
            break;
        }
    }

    SetLastErrorLocked(std::string());
    return emitted;
}

int mlr_list_input_devices(MLR_InputDeviceCallback callback, void* user_data) {
    RuntimeState& state = State();
    std::lock_guard<std::mutex> lock(state.mutex);

    const int initStatus = EnsureInitializedAndOwnedThreadLocked();
    if (initStatus != MLR_OK) {
        return initStatus;
    }
    if (callback == nullptr) {
        SetLastErrorLocked("callback is null");
        return MLR_E_INVALID_ARGUMENT;
    }

    if (!state.audioDeviceEnumerator) {
        state.audioDeviceEnumerator = std::make_unique<AudioDeviceEnumerator>();
    }
    if (!state.audioDeviceEnumerator->EnumerateInputDevices()) {
        SetLastErrorLocked("Failed to enumerate input devices");
        return MLR_E_INTERNAL;
    }

    int emitted = 0;
    const std::vector<AudioDeviceInfo>& devices = state.audioDeviceEnumerator->GetInputDevices();
    for (const AudioDeviceInfo& device : devices) {
        std::string idUtf8 = WideToUtf8(device.deviceId);
        std::string nameUtf8 = WideToUtf8(device.friendlyName);
        MLR_InputDeviceInfo info{};
        info.device_id_utf8 = idUtf8.c_str();
        info.friendly_name_utf8 = nameUtf8.c_str();
        info.is_default = device.isDefault ? 1 : 0;

        const int callbackResult = callback(&info, user_data);
        emitted++;
        if (callbackResult != 0) {
            break;
        }
    }

    SetLastErrorLocked(std::string());
    return emitted;
}

int mlr_start_capture_to_file(
    uint32_t process_id,
    const char* output_file_utf8,
    int format,
    uint32_t bitrate,
    int skip_silence,
    const char* passthrough_device_id_utf8,
    int monitor_only,
    int strict_process_isolation) {
    RuntimeState& state = State();
    std::lock_guard<std::mutex> lock(state.mutex);

    const int initStatus = EnsureInitializedAndOwnedThreadLocked();
    if (initStatus != MLR_OK) {
        return initStatus;
    }

    bool formatOk = false;
    const AudioFormat audioFormat = ConvertFormat(format, &formatOk);
    if (!formatOk) {
        SetLastErrorLocked("Invalid format value");
        return MLR_E_INVALID_ARGUMENT;
    }

    if (monitor_only == 0 && (output_file_utf8 == nullptr || *output_file_utf8 == '\0')) {
        SetLastErrorLocked("output_file_utf8 is required when monitor_only is false");
        return MLR_E_INVALID_ARGUMENT;
    }

    std::wstring processName;
    if (!ResolveProcessNameLocked(process_id, &processName)) {
        SetLastErrorLocked("Process not found");
        return MLR_E_PROCESS_NOT_FOUND;
    }

    const std::wstring outputPath = Utf8ToWide(output_file_utf8);
    const std::wstring passthroughDeviceId = Utf8ToWide(passthrough_device_id_utf8);
    return StartCaptureLocked(
        process_id,
        processName,
        outputPath,
        audioFormat,
        bitrate,
        skip_silence != 0,
        passthroughDeviceId,
        monitor_only != 0,
        strict_process_isolation != 0);
}

int mlr_start_capture_to_directory(
    uint32_t process_id,
    const char* output_dir_utf8,
    int format,
    uint32_t bitrate,
    int skip_silence,
    const char* passthrough_device_id_utf8,
    int monitor_only,
    int strict_process_isolation) {
    RuntimeState& state = State();
    std::lock_guard<std::mutex> lock(state.mutex);

    const int initStatus = EnsureInitializedAndOwnedThreadLocked();
    if (initStatus != MLR_OK) {
        return initStatus;
    }

    bool formatOk = false;
    const AudioFormat audioFormat = ConvertFormat(format, &formatOk);
    if (!formatOk) {
        SetLastErrorLocked("Invalid format value");
        return MLR_E_INVALID_ARGUMENT;
    }

    if (monitor_only == 0 && (output_dir_utf8 == nullptr || *output_dir_utf8 == '\0')) {
        SetLastErrorLocked("output_dir_utf8 is required when monitor_only is false");
        return MLR_E_INVALID_ARGUMENT;
    }

    std::wstring processName;
    if (!ResolveProcessNameLocked(process_id, &processName)) {
        SetLastErrorLocked("Process not found");
        return MLR_E_PROCESS_NOT_FOUND;
    }

    std::wstring outputFilePath;
    if (monitor_only == 0) {
        std::wstring outputDir = Utf8ToWide(output_dir_utf8);
        if (!EnsureDirectoryExists(outputDir)) {
            SetLastErrorLocked("Failed to create output directory");
            return MLR_E_INVALID_ARGUMENT;
        }

        std::wstring baseName = processName;
        const size_t exePos = baseName.find(L".exe");
        if (exePos != std::wstring::npos) {
            baseName = baseName.substr(0, exePos);
        }
        baseName = SanitizeFileName(baseName);

        if (!outputDir.empty() && outputDir.back() != L'\\' && outputDir.back() != L'/') {
            outputDir += L"\\";
        }
        outputFilePath = outputDir + baseName + L"-" + TimestampForFilename() + GetFileExtension(audioFormat);
    }

    const std::wstring passthroughDeviceId = Utf8ToWide(passthrough_device_id_utf8);
    return StartCaptureLocked(
        process_id,
        processName,
        outputFilePath,
        audioFormat,
        bitrate,
        skip_silence != 0,
        passthroughDeviceId,
        monitor_only != 0,
        strict_process_isolation != 0);
}

int mlr_stop_capture(uint32_t process_id) {
    RuntimeState& state = State();
    std::lock_guard<std::mutex> lock(state.mutex);

    const int initStatus = EnsureInitializedAndOwnedThreadLocked();
    if (initStatus != MLR_OK) {
        return initStatus;
    }

    if (!state.captureManager->StopCapture(process_id)) {
        SetLastErrorLocked("Capture session not found or could not be stopped");
        return MLR_E_STOP_FAILED;
    }

    RemoveInputSessionBySessionIdLocked(process_id);

    SetLastErrorLocked(std::string());
    return MLR_OK;
}

void mlr_stop_all_captures(void) {
    RuntimeState& state = State();
    std::lock_guard<std::mutex> lock(state.mutex);

    if (!state.initialized || state.ownerThreadId != GetCurrentThreadId()) {
        return;
    }
    state.captureManager->StopAllCaptures();
    state.captureManager->DisableMixedRecording();
    state.activeInputSessions.clear();
}

int mlr_is_capturing(uint32_t process_id) {
    RuntimeState& state = State();
    std::lock_guard<std::mutex> lock(state.mutex);

    const int initStatus = EnsureInitializedAndOwnedThreadLocked();
    if (initStatus != MLR_OK) {
        return initStatus;
    }
    return state.captureManager->IsCapturing(process_id) ? 1 : 0;
}

int mlr_set_capture_volume(uint32_t process_id, float volume_0_to_1) {
    RuntimeState& state = State();
    std::lock_guard<std::mutex> lock(state.mutex);

    const int initStatus = EnsureInitializedAndOwnedThreadLocked();
    if (initStatus != MLR_OK) {
        return initStatus;
    }

    if (volume_0_to_1 < 0.0f || volume_0_to_1 > 1.0f) {
        SetLastErrorLocked("volume must be in range [0.0, 1.0]");
        return MLR_E_INVALID_ARGUMENT;
    }

    std::vector<CaptureSession*> sessions = state.captureManager->GetActiveSessions();
    for (CaptureSession* session : sessions) {
        if (session != nullptr && session->processId == process_id && session->capture != nullptr) {
            session->capture->SetVolume(volume_0_to_1);
            SetLastErrorLocked(std::string());
            return MLR_OK;
        }
    }

    SetLastErrorLocked("Capture session not found");
    return MLR_E_PROCESS_NOT_FOUND;
}

int mlr_get_active_session_count(void) {
    RuntimeState& state = State();
    std::lock_guard<std::mutex> lock(state.mutex);

    const int initStatus = EnsureInitializedAndOwnedThreadLocked();
    if (initStatus != MLR_OK) {
        return initStatus;
    }
    const std::vector<CaptureSession*> sessions = state.captureManager->GetActiveSessions();
    return static_cast<int>(sessions.size());
}

int mlr_start_microphone_capture_to_file(
    const char* input_device_id_utf8,
    const char* output_file_utf8,
    int format,
    uint32_t bitrate,
    int skip_silence,
    int monitor_only) {
    RuntimeState& state = State();
    std::lock_guard<std::mutex> lock(state.mutex);

    const int initStatus = EnsureInitializedAndOwnedThreadLocked();
    if (initStatus != MLR_OK) {
        return initStatus;
    }

    bool formatOk = false;
    const AudioFormat audioFormat = ConvertFormat(format, &formatOk);
    if (!formatOk) {
        SetLastErrorLocked("Invalid format value");
        return MLR_E_INVALID_ARGUMENT;
    }

    if (monitor_only == 0 && (output_file_utf8 == nullptr || *output_file_utf8 == '\0')) {
        SetLastErrorLocked("output_file_utf8 is required when monitor_only is false");
        return MLR_E_INVALID_ARGUMENT;
    }

    const std::wstring requestedDeviceId = Utf8ToWide(input_device_id_utf8);
    const std::wstring outputPath = Utf8ToWide(output_file_utf8);
    return StartMicrophoneCaptureLocked(
        requestedDeviceId,
        outputPath,
        audioFormat,
        bitrate,
        skip_silence != 0,
        monitor_only != 0);
}

int mlr_start_microphone_capture_to_directory(
    const char* input_device_id_utf8,
    const char* output_dir_utf8,
    int format,
    uint32_t bitrate,
    int skip_silence,
    int monitor_only) {
    RuntimeState& state = State();
    std::lock_guard<std::mutex> lock(state.mutex);

    const int initStatus = EnsureInitializedAndOwnedThreadLocked();
    if (initStatus != MLR_OK) {
        return initStatus;
    }

    bool formatOk = false;
    const AudioFormat audioFormat = ConvertFormat(format, &formatOk);
    if (!formatOk) {
        SetLastErrorLocked("Invalid format value");
        return MLR_E_INVALID_ARGUMENT;
    }

    if (monitor_only == 0 && (output_dir_utf8 == nullptr || *output_dir_utf8 == '\0')) {
        SetLastErrorLocked("output_dir_utf8 is required when monitor_only is false");
        return MLR_E_INVALID_ARGUMENT;
    }

    const std::wstring requestedDeviceId = Utf8ToWide(input_device_id_utf8);
    std::wstring outputPath;
    if (monitor_only == 0) {
        std::wstring outputDir = Utf8ToWide(output_dir_utf8);
        if (!EnsureDirectoryExists(outputDir)) {
            SetLastErrorLocked("Failed to create output directory");
            return MLR_E_INVALID_ARGUMENT;
        }

        AudioDeviceInfo inputDevice{};
        if (!ResolveInputDeviceLocked(requestedDeviceId, &inputDevice, nullptr)) {
            SetLastErrorLocked("Input device not found");
            return MLR_E_DEVICE_NOT_FOUND;
        }

        if (!outputDir.empty() && outputDir.back() != L'\\' && outputDir.back() != L'/') {
            outputDir += L"\\";
        }
        outputPath =
            outputDir + SanitizeFileName(inputDevice.friendlyName) + L"-" + TimestampForFilename() + GetFileExtension(audioFormat);
    }

    return StartMicrophoneCaptureLocked(
        requestedDeviceId,
        outputPath,
        audioFormat,
        bitrate,
        skip_silence != 0,
        monitor_only != 0);
}

int mlr_stop_microphone_capture(const char* input_device_id_utf8) {
    RuntimeState& state = State();
    std::lock_guard<std::mutex> lock(state.mutex);

    const int initStatus = EnsureInitializedAndOwnedThreadLocked();
    if (initStatus != MLR_OK) {
        return initStatus;
    }

    const std::wstring requestedDeviceId = Utf8ToWide(input_device_id_utf8);
    AudioDeviceInfo inputDevice{};
    size_t inputIndex = 0;
    if (!ResolveInputDeviceLocked(requestedDeviceId, &inputDevice, &inputIndex)) {
        SetLastErrorLocked("Input device not found");
        return MLR_E_DEVICE_NOT_FOUND;
    }

    DWORD sessionId = BuildMicSessionId(inputIndex);
    auto mapped = state.activeInputSessions.find(inputDevice.deviceId);
    if (mapped != state.activeInputSessions.end()) {
        sessionId = mapped->second;
    }

    if (!state.captureManager->StopCapture(sessionId)) {
        SetLastErrorLocked("Microphone capture session not found");
        return MLR_E_STOP_FAILED;
    }

    state.activeInputSessions.erase(inputDevice.deviceId);
    SetLastErrorLocked(std::string());
    return MLR_OK;
}

void mlr_stop_all_microphone_captures(void) {
    RuntimeState& state = State();
    std::lock_guard<std::mutex> lock(state.mutex);

    if (!state.initialized || state.ownerThreadId != GetCurrentThreadId()) {
        return;
    }

    std::vector<DWORD> sessionIds;
    sessionIds.reserve(state.activeInputSessions.size());
    for (const auto& it : state.activeInputSessions) {
        sessionIds.push_back(it.second);
    }
    state.activeInputSessions.clear();

    for (DWORD sessionId : sessionIds) {
        state.captureManager->StopCapture(sessionId);
    }
}

int mlr_enable_mixed_recording_to_file(
    const char* output_file_utf8,
    int format,
    uint32_t bitrate) {
    RuntimeState& state = State();
    std::lock_guard<std::mutex> lock(state.mutex);

    const int initStatus = EnsureInitializedAndOwnedThreadLocked();
    if (initStatus != MLR_OK) {
        return initStatus;
    }

    bool formatOk = false;
    const AudioFormat audioFormat = ConvertFormat(format, &formatOk);
    if (!formatOk) {
        SetLastErrorLocked("Invalid format value");
        return MLR_E_INVALID_ARGUMENT;
    }
    if (output_file_utf8 == nullptr || *output_file_utf8 == '\0') {
        SetLastErrorLocked("output_file_utf8 is required");
        return MLR_E_INVALID_ARGUMENT;
    }

    const std::wstring outputPath = Utf8ToWide(output_file_utf8);
    if (!state.captureManager->EnableMixedRecording(outputPath, audioFormat, bitrate)) {
        SetLastErrorLocked("Failed to enable mixed recording (need at least one active capture session)");
        return MLR_E_MIXED_RECORDING_FAILED;
    }

    SetLastErrorLocked(std::string());
    return MLR_OK;
}

int mlr_enable_mixed_recording_to_directory(
    const char* output_dir_utf8,
    int format,
    uint32_t bitrate,
    const char* base_name_utf8) {
    RuntimeState& state = State();
    std::lock_guard<std::mutex> lock(state.mutex);

    const int initStatus = EnsureInitializedAndOwnedThreadLocked();
    if (initStatus != MLR_OK) {
        return initStatus;
    }

    bool formatOk = false;
    const AudioFormat audioFormat = ConvertFormat(format, &formatOk);
    if (!formatOk) {
        SetLastErrorLocked("Invalid format value");
        return MLR_E_INVALID_ARGUMENT;
    }
    if (output_dir_utf8 == nullptr || *output_dir_utf8 == '\0') {
        SetLastErrorLocked("output_dir_utf8 is required");
        return MLR_E_INVALID_ARGUMENT;
    }

    std::wstring outputDir = Utf8ToWide(output_dir_utf8);
    if (!EnsureDirectoryExists(outputDir)) {
        SetLastErrorLocked("Failed to create output directory");
        return MLR_E_INVALID_ARGUMENT;
    }
    if (!outputDir.empty() && outputDir.back() != L'\\' && outputDir.back() != L'/') {
        outputDir += L"\\";
    }

    std::wstring baseName = Utf8ToWide(base_name_utf8);
    if (baseName.empty()) {
        baseName = L"Mixed";
    }
    baseName = SanitizeFileName(baseName);

    const std::wstring outputPath = outputDir + baseName + L"-" + TimestampForFilename() + GetFileExtension(audioFormat);
    if (!state.captureManager->EnableMixedRecording(outputPath, audioFormat, bitrate)) {
        SetLastErrorLocked("Failed to enable mixed recording (need at least one active capture session)");
        return MLR_E_MIXED_RECORDING_FAILED;
    }

    SetLastErrorLocked(std::string());
    return MLR_OK;
}

void mlr_disable_mixed_recording(void) {
    RuntimeState& state = State();
    std::lock_guard<std::mutex> lock(state.mutex);

    if (!state.initialized || state.ownerThreadId != GetCurrentThreadId()) {
        return;
    }
    state.captureManager->DisableMixedRecording();
}

int mlr_is_mixed_recording_active(void) {
    RuntimeState& state = State();
    std::lock_guard<std::mutex> lock(state.mutex);

    const int initStatus = EnsureInitializedAndOwnedThreadLocked();
    if (initStatus != MLR_OK) {
        return initStatus;
    }
    return state.captureManager->IsMixedRecordingActive() ? 1 : 0;
}
