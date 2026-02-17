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

pushd "%ROOT%" >nul

%PY% -m pip install --upgrade twine || goto :fail
%PY% -m twine upload --repository testpypi dist\mlrecorder-*.whl || goto :fail

popd >nul
echo Upload to TestPyPI completed.
exit /b 0

:fail
popd >nul
echo [ERROR] Upload to TestPyPI failed.
exit /b 1
