@echo off
setlocal

set "ROOT=%~dp0.."
set "BUILD_BIN=%ROOT%\build-release\bin"
set "PKG_BIN=%ROOT%\python\mlrecorder\bin"

if not exist "%BUILD_BIN%\mlrecorder_core.dll" (
    echo [ERROR] Missing %BUILD_BIN%\mlrecorder_core.dll
    echo Build Release first.
    exit /b 1
)

if not exist "%PKG_BIN%" mkdir "%PKG_BIN%" || exit /b 1

copy /y "%BUILD_BIN%\mlrecorder_core.dll" "%PKG_BIN%\" >nul
copy /y "%BUILD_BIN%\FLAC.dll" "%PKG_BIN%\" >nul
copy /y "%BUILD_BIN%\ogg.dll" "%PKG_BIN%\" >nul
copy /y "%BUILD_BIN%\opus.dll" "%PKG_BIN%\" >nul

echo Staged DLLs into:
echo   %PKG_BIN%

exit /b 0
