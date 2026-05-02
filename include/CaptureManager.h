#pragma once

#include "AudioCapture.h"
#include "AudioMixer.h"
#include "WavWriter.h"
#include "Mp3Encoder.h"
#include "OpusEncoder.h"
#include "FlacEncoder.h"
#include <memory>
#include <map>
#include <mutex>
#include <thread>
#include <atomic>
#include <chrono>
#include <functional>

enum class AudioFormat {
    WAV,
    MP3,
    OPUS,
    FLAC
};

struct CaptureSession {
    DWORD processId;
    std::wstring processName;
    std::wstring outputFile;
    AudioFormat format;
    std::unique_ptr<AudioCapture> capture;
    std::unique_ptr<WavWriter> wavWriter;
    std::unique_ptr<Mp3Encoder> mp3Encoder;
    std::unique_ptr<OpusOggEncoder> opusEncoder;
    std::unique_ptr<FlacEncoder> flacEncoder;
    bool isActive;
    UINT64 bytesWritten;
    bool skipSilence;
    bool monitorOnly;

    // Stats
    std::chrono::steady_clock::time_point startTime;
    std::chrono::duration<double> totalPausedDuration{};
    std::chrono::steady_clock::time_point pauseStartTime;
    bool isPausedForTiming = false;
};

class CaptureManager {
public:
    CaptureManager();
    ~CaptureManager();

    // Start capturing from a process
    bool StartCapture(DWORD processId, const std::wstring& processName,
                     const std::wstring& outputPath, AudioFormat format,
                     UINT32 bitrate = 0, bool skipSilence = false,
                     const std::wstring& passthroughDeviceId = L"",
                     bool monitorOnly = false);

    // Start capturing from an audio device (microphone/line-in)
    bool StartCaptureFromDevice(DWORD sessionId, const std::wstring& deviceName,
                                const std::wstring& deviceId, bool isInputDevice,
                                const std::wstring& outputPath, AudioFormat format,
                                UINT32 bitrate = 0, bool skipSilence = false,
                                bool monitorOnly = false);

    // Enable mixed recording (all processes will be mixed into one file)
    bool EnableMixedRecording(const std::wstring& outputPath, AudioFormat format, UINT32 bitrate = 0);

    // Disable mixed recording
    void DisableMixedRecording();

    // Check if mixed recording is active
    bool IsMixedRecordingActive() const;

    // Stop capturing from a specific process
    bool StopCapture(DWORD processId);

    // Pause/resume a specific capture session
    bool PauseCapture(DWORD processId);
    bool ResumeCapture(DWORD processId);
    bool IsPaused(DWORD processId) const;

    // Set volume multiplier [0.0, 1.0] for an active session.
    bool SetVolume(DWORD processId, float volume);

    // Stats: bytes written and net recording duration (excludes paused time).
    bool GetSessionStats(DWORD processId, uint64_t& outBytes, double& outSeconds, bool& outIsPaused) const;

    // Callback fired (from the capture thread) when a session ends unexpectedly.
    // Pass nullptr to clear. Not thread-safe with concurrent recording start/stop.
    void SetSessionEndedCallback(std::function<void(DWORD)> callback);

    // Stop all captures
    void StopAllCaptures();

    // Pause all captures
    void PauseAllCaptures();

    // Resume all captures
    void ResumeAllCaptures();

    // Get active capture sessions
    std::vector<CaptureSession*> GetActiveSessions();

    // Check if a process is being captured
    bool IsCapturing(DWORD processId) const;

private:
    void OnAudioData(DWORD processId, const BYTE* data, UINT32 size);
    void MixerThread();

    std::map<DWORD, std::unique_ptr<CaptureSession>> m_sessions;
    std::mutex m_mutex;

    std::function<void(DWORD)> m_sessionEndedCallback;
    std::mutex m_callbackMutex;

    // Mixed recording members
    bool m_mixedRecordingEnabled;
    std::unique_ptr<AudioMixer> m_mixer;
    std::unique_ptr<WavWriter> m_mixedWavWriter;
    std::unique_ptr<Mp3Encoder> m_mixedMp3Encoder;
    std::unique_ptr<OpusOggEncoder> m_mixedOpusEncoder;
    std::unique_ptr<FlacEncoder> m_mixedFlacEncoder;
    AudioFormat m_mixedFormat;
    std::unique_ptr<std::thread> m_mixerThread;
    std::atomic<bool> m_mixerThreadRunning;
    std::mutex m_mixerMutex;
};
