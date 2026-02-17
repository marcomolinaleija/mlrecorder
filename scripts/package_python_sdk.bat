@echo off
setlocal

set "ROOT=%~dp0.."
set "BUILD_BIN=%ROOT%\build-release\bin"
set "DIST_ROOT=%ROOT%\dist\mlrecorder-python"

if not exist "%BUILD_BIN%\mlrecorder_core.dll" (
    echo [ERROR] Missing %BUILD_BIN%\mlrecorder_core.dll
    echo Build release first:
    echo   cmake --build build-release --config Release
    exit /b 1
)

if exist "%DIST_ROOT%" rmdir /s /q "%DIST_ROOT%"

mkdir "%DIST_ROOT%" || exit /b 1
mkdir "%DIST_ROOT%\python" || exit /b 1
mkdir "%DIST_ROOT%\include" || exit /b 1

copy /y "%BUILD_BIN%\mlrecorder_core.dll" "%DIST_ROOT%\" >nul
copy /y "%BUILD_BIN%\FLAC.dll" "%DIST_ROOT%\" >nul
copy /y "%BUILD_BIN%\ogg.dll" "%DIST_ROOT%\" >nul
copy /y "%BUILD_BIN%\opus.dll" "%DIST_ROOT%\" >nul

if exist "%BUILD_BIN%\mlrecorder_core.pdb" (
    copy /y "%BUILD_BIN%\mlrecorder_core.pdb" "%DIST_ROOT%\" >nul
)

call "%~dp0stage_python_wheel_dlls.bat" >nul || exit /b 1

xcopy /e /i /y "%ROOT%\python" "%DIST_ROOT%\python" >nul
copy /y "%ROOT%\include\MLRecorderAPI.h" "%DIST_ROOT%\include\" >nul

for /r "%DIST_ROOT%\python" %%F in (*.pyc) do del /q "%%F" >nul 2>nul
for /d /r "%DIST_ROOT%\python" %%D in (__pycache__) do rmdir /s /q "%%D" >nul 2>nul

echo.
echo Package created at:
echo   %DIST_ROOT%
echo.
echo Contents:
echo   mlrecorder_core.dll + runtime deps (FLAC.dll, ogg.dll, opus.dll)
echo   python\ wrapper + examples
echo   include\MLRecorderAPI.h
echo.
echo Note: target machines still need Microsoft VC++ Redistributable 2015-2022 x64.

exit /b 0
