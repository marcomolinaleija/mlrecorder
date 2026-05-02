#pragma once

#include <stdint.h>

#ifdef _WIN32
#if defined(MLR_CORE_BUILD)
#define MLR_API __declspec(dllexport)
#else
#define MLR_API __declspec(dllimport)
#endif
#else
#define MLR_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef enum MLR_AudioFormat {
    MLR_FORMAT_WAV = 0,
    MLR_FORMAT_MP3 = 1,
    MLR_FORMAT_OPUS = 2,
    MLR_FORMAT_FLAC = 3
} MLR_AudioFormat;

typedef enum MLR_Result {
    MLR_OK = 0,
    MLR_E_INVALID_ARGUMENT = -1,
    MLR_E_NOT_INITIALIZED = -2,
    MLR_E_WRONG_THREAD = -3,
    MLR_E_ALREADY_CAPTURING = -4,
    MLR_E_START_FAILED = -5,
    MLR_E_STOP_FAILED = -6,
    MLR_E_PROCESS_NOT_FOUND = -7,
    MLR_E_PROCESS_ISOLATION_UNAVAILABLE = -8,
    MLR_E_INTERNAL = -9,
    MLR_E_DEVICE_NOT_FOUND = -10,
    MLR_E_MIXED_RECORDING_FAILED = -11,
    MLR_E_NOT_CAPTURING = -12
} MLR_Result;

typedef struct MLR_ProcessInfo {
    uint32_t process_id;
    const char* process_name_utf8;
    const char* window_title_utf8;
    int has_active_audio;
} MLR_ProcessInfo;

typedef int (*MLR_ProcessCallback)(const MLR_ProcessInfo* info, void* user_data);

typedef struct MLR_InputDeviceInfo {
    const char* device_id_utf8;
    const char* friendly_name_utf8;
    int is_default;
} MLR_InputDeviceInfo;

typedef int (*MLR_InputDeviceCallback)(const MLR_InputDeviceInfo* info, void* user_data);

// Initialize runtime. Must be called before any capture/list operation.
MLR_API int mlr_initialize(void);

// Shutdown runtime and stop all active captures.
MLR_API void mlr_shutdown(void);

// Returns 1 if initialized, 0 otherwise.
MLR_API int mlr_is_initialized(void);

// Human-readable description for a result code.
MLR_API const char* mlr_result_to_string(int code);

// Last error string (UTF-8). Valid until next API call.
MLR_API const char* mlr_get_last_error(void);

// List running processes. Returns number of processes emitted, or negative error code.
// Strings in MLR_ProcessInfo are only valid during the callback invocation.
MLR_API int mlr_list_processes(int only_active_audio, MLR_ProcessCallback callback, void* user_data);

// List input devices (microphones). Returns number of devices emitted, or negative error code.
// Strings in MLR_InputDeviceInfo are only valid during callback invocation.
MLR_API int mlr_list_input_devices(MLR_InputDeviceCallback callback, void* user_data);

// Start capture for a specific process ID to a concrete output file path.
MLR_API int mlr_start_capture_to_file(
    uint32_t process_id,
    const char* output_file_utf8,
    int format,
    uint32_t bitrate,
    int skip_silence,
    const char* passthrough_device_id_utf8,
    int monitor_only,
    int strict_process_isolation);

// Start capture for a process and auto-generate filename inside output directory.
MLR_API int mlr_start_capture_to_directory(
    uint32_t process_id,
    const char* output_dir_utf8,
    int format,
    uint32_t bitrate,
    int skip_silence,
    const char* passthrough_device_id_utf8,
    int monitor_only,
    int strict_process_isolation);

MLR_API int mlr_stop_capture(uint32_t process_id);
MLR_API void mlr_stop_all_captures(void);

// Returns 1 if capturing, 0 if not, or negative error code.
MLR_API int mlr_is_capturing(uint32_t process_id);

// Pause an active capture session without closing the output file.
// Returns MLR_E_NOT_CAPTURING if the session does not exist.
MLR_API int mlr_pause_capture(uint32_t process_id);

// Resume a previously paused capture session.
// Returns MLR_E_NOT_CAPTURING if the session does not exist.
MLR_API int mlr_resume_capture(uint32_t process_id);

// Returns 1 if the session exists and is paused, 0 otherwise (including not capturing).
MLR_API int mlr_is_paused(uint32_t process_id);

// Set volume for an active session by PID. Volume range: [0.0, 1.0].
MLR_API int mlr_set_capture_volume(uint32_t process_id, float volume_0_to_1);

// Returns active session count, or negative error code.
MLR_API int mlr_get_active_session_count(void);

// Microphone capture helpers. If input_device_id_utf8 is null/empty, default input is used.
MLR_API int mlr_start_microphone_capture_to_file(
    const char* input_device_id_utf8,
    const char* output_file_utf8,
    int format,
    uint32_t bitrate,
    int skip_silence,
    int monitor_only);

MLR_API int mlr_start_microphone_capture_to_directory(
    const char* input_device_id_utf8,
    const char* output_dir_utf8,
    int format,
    uint32_t bitrate,
    int skip_silence,
    int monitor_only);

MLR_API int mlr_stop_microphone_capture(const char* input_device_id_utf8);
MLR_API void mlr_stop_all_microphone_captures(void);

// Pause/resume a microphone capture session. NULL/empty input_device_id uses the default device.
// Returns MLR_E_NOT_CAPTURING if the session does not exist.
MLR_API int mlr_pause_microphone_capture(const char* input_device_id_utf8);
MLR_API int mlr_resume_microphone_capture(const char* input_device_id_utf8);

// Returns 1 if the microphone session is paused, 0 otherwise (including not capturing).
MLR_API int mlr_is_microphone_paused(const char* input_device_id_utf8);

// Mixed recording (mix all active sessions into one output file).
MLR_API int mlr_enable_mixed_recording_to_file(
    const char* output_file_utf8,
    int format,
    uint32_t bitrate);

MLR_API int mlr_enable_mixed_recording_to_directory(
    const char* output_dir_utf8,
    int format,
    uint32_t bitrate,
    const char* base_name_utf8);

MLR_API void mlr_disable_mixed_recording(void);
MLR_API int mlr_is_mixed_recording_active(void);

#ifdef __cplusplus
}
#endif
