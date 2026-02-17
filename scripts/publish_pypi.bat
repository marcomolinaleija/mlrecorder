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
%PY% -m twine upload dist\mlrecorder-*.whl || goto :fail

popd >nul
echo Upload to PyPI completed.
exit /b 0

:fail
popd >nul
echo [ERROR] Upload to PyPI failed.
exit /b 1
