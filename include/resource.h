#pragma once

// ========== Control IDs ==========

// Proceso list
#define IDC_PROCESS_LIST            1001
#define IDC_PROCESS_LIST_LABEL      1002
#define IDC_REFRESH_BTN             1003
#define IDC_SHOW_AUDIO_ONLY_CHECKBOX 1004

// Micrófono
#define IDC_MICROPHONE_CHECKBOX     1010
#define IDC_MICROPHONE_DEVICE_COMBO 1011
#define IDC_MICROPHONE_DEVICE_LABEL 1012

// Grabación
#define IDC_OUTPUT_PATH             1020
#define IDC_OUTPUT_PATH_LABEL       1021
#define IDC_BROWSE_BTN              1022
#define IDC_RECORDING_MODE_COMBO    1023
#define IDC_RECORDING_MODE_LABEL    1024

// Botones de control
#define IDC_START_BTN               1030
#define IDC_STOP_ALL_BTN            1031
#define IDC_PAUSE_ALL_BTN           1032
#define IDC_RESUME_ALL_BTN          1033

// Grabaciones activas
#define IDC_RECORDING_LIST          1040
#define IDC_RECORDING_LIST_LABEL    1041

// Barra de estado
#define IDC_STATUS_TEXT              1050

// Monitoreo
#define IDC_PASSTHROUGH_CHECKBOX    1060
#define IDC_PASSTHROUGH_DEVICE_COMBO 1061
#define IDC_PASSTHROUGH_DEVICE_LABEL 1062
#define IDC_MONITOR_ONLY_CHECKBOX   1063

// Volumen
#define IDC_PROCESS_VOLUME_SLIDER   1070
#define IDC_PROCESS_VOLUME_LABEL    1071
#define IDC_MICROPHONE_VOLUME_SLIDER 1072
#define IDC_MICROPHONE_VOLUME_LABEL 1073

// ========== Diálogo Calidad de Audio ==========
#define IDD_QUALITY_DIALOG          2000
#define IDC_FORMAT_COMBO            2001
#define IDC_FORMAT_LABEL            2002
#define IDC_MP3_BITRATE_COMBO       2003
#define IDC_MP3_BITRATE_LABEL       2004
#define IDC_OPUS_BITRATE_COMBO      2005
#define IDC_OPUS_BITRATE_LABEL      2006
#define IDC_FLAC_COMPRESSION_COMBO  2007
#define IDC_FLAC_COMPRESSION_LABEL  2008
#define IDC_SKIP_SILENCE_CHECKBOX   2009
#define IDC_QUALITY_OK_BTN          2010
#define IDC_QUALITY_CANCEL_BTN      2011

// ========== Menú IDs ==========
#define IDM_MAINMENU                3000
#define IDM_FILE_OPEN_FOLDER        3001
#define IDM_FILE_EXIT               3002
#define IDM_CONFIG_QUALITY          3003
#define IDM_HELP_ABOUT              3004

// ========== Tray Icon ==========
#define WM_TRAYICON                 (WM_USER + 100)
#define IDI_TRAY_ICON               4001
#define IDM_TRAY_SHOW               4002
#define IDM_TRAY_EXIT               4003

// ========== Aceleradores ==========
#define IDR_ACCELERATOR             5001
