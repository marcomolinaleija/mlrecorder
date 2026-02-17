@echo off
setlocal

set "ROOT=%~dp0.."
set "PY="

where py >nul 2>nul
if %ERRORLEVEL% EQU 0 (
    set "PY=py -3"
) else (
    where python >nul 2>nul
    if %ERRORLEVEL% EQU 0 (
        set "PY=python"
    )
)

if "%PY%"=="" (
    echo [ERROR] Python launcher not found.
    exit /b 1
)

call "%~dp0stage_python_wheel_dlls.bat" || exit /b 1

pushd "%ROOT%" >nul

%PY% -m pip show build >nul 2>nul
if %ERRORLEVEL% NEQ 0 (
    echo Installing package 'build'...
    %PY% -m pip install --upgrade build || goto :fail
)

%PY% -m build --wheel || goto :fail

popd >nul
echo Wheel built in:
echo   %ROOT%\dist
exit /b 0

:fail
popd >nul
echo [ERROR] Wheel build failed.
exit /b 1
