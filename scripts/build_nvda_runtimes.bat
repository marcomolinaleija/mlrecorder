@echo off
setlocal

set "ROOT=%~dp0.."
set "X64_BUILD_DIR=%ROOT%\build-release"
set "X86_BUILD_DIR=%ROOT%\build-release-x86-vs18"
set "X64_TRIPLET=x64-windows"
set "X86_TRIPLET=x86-windows"
set "VCPKG_TOOLCHAIN=%VCPKG_TOOLCHAIN_FILE%"

if "%VCPKG_TOOLCHAIN%"=="" (
	set "VCPKG_TOOLCHAIN=C:/vcpkg/scripts/buildsystems/vcpkg.cmake"
)

where cmake >nul 2>nul
if %ERRORLEVEL% NEQ 0 (
	echo [ERROR] CMake not found in PATH.
	exit /b 1
)

if not exist "%VCPKG_TOOLCHAIN%" (
	echo [ERROR] vcpkg toolchain file not found:
	echo   %VCPKG_TOOLCHAIN%
	echo Set VCPKG_TOOLCHAIN_FILE to a valid path.
	exit /b 1
)

echo ==================================================
echo Building MLRecorder x64 runtime for NVDA addon
echo ==================================================

if exist "%X64_BUILD_DIR%\CMakeCache.txt" (
	echo Using existing x64 configure: %X64_BUILD_DIR%
) else (
	echo Configuring x64 with Ninja...
	cmake -S "%ROOT%" -B "%X64_BUILD_DIR%" ^
		-G Ninja ^
		-DCMAKE_BUILD_TYPE=Release ^
		-DCMAKE_TOOLCHAIN_FILE="%VCPKG_TOOLCHAIN%" ^
		-DVCPKG_TARGET_TRIPLET=%X64_TRIPLET% || goto :fail
)

echo Building x64...
cmake --build "%X64_BUILD_DIR%" --config Release || goto :fail

if not exist "%X64_BUILD_DIR%\bin\mlrecorder_core.dll" (
	echo [ERROR] x64 output missing:
	echo   %X64_BUILD_DIR%\bin\mlrecorder_core.dll
	goto :fail
)

echo ==================================================
echo Building MLRecorder x86 runtime for NVDA addon
echo ==================================================

set "VS_GENERATOR="
cmake --help | findstr /c:"Visual Studio 18 2026" >nul && set "VS_GENERATOR=Visual Studio 18 2026"
if "%VS_GENERATOR%"=="" (
	cmake --help | findstr /c:"Visual Studio 17 2022" >nul && set "VS_GENERATOR=Visual Studio 17 2022"
)

if "%VS_GENERATOR%"=="" (
	echo [ERROR] No compatible Visual Studio generator found.
	echo Install Visual Studio Build Tools (2022/2026) with C++ workload.
	goto :fail
)

if exist "%X86_BUILD_DIR%\CMakeCache.txt" (
	echo Using existing x86 configure: %X86_BUILD_DIR%
) else (
	echo Configuring x86 with "%VS_GENERATOR%"...
	cmake -S "%ROOT%" -B "%X86_BUILD_DIR%" ^
		-G "%VS_GENERATOR%" ^
		-A Win32 ^
		-DCMAKE_BUILD_TYPE=Release ^
		-DCMAKE_TOOLCHAIN_FILE="%VCPKG_TOOLCHAIN%" ^
		-DVCPKG_TARGET_TRIPLET=%X86_TRIPLET% || goto :fail
)

echo Building x86...
cmake --build "%X86_BUILD_DIR%" --config Release || goto :fail

if not exist "%X86_BUILD_DIR%\bin\Release\mlrecorder_core.dll" (
	if exist "%X86_BUILD_DIR%\bin\mlrecorder_core.dll" (
		echo Using x86 output from:
		echo   %X86_BUILD_DIR%\bin\mlrecorder_core.dll
	) else (
		echo [ERROR] x86 output missing:
		echo   %X86_BUILD_DIR%\bin\Release\mlrecorder_core.dll
		goto :fail
	)
)

echo.
echo [OK] Runtime build completed for both architectures.
echo x64: %X64_BUILD_DIR%\bin\mlrecorder_core.dll
if exist "%X86_BUILD_DIR%\bin\Release\mlrecorder_core.dll" (
	echo x86: %X86_BUILD_DIR%\bin\Release\mlrecorder_core.dll
) else (
	echo x86: %X86_BUILD_DIR%\bin\mlrecorder_core.dll
)
exit /b 0

:fail
echo [ERROR] Runtime build failed.
exit /b 1
