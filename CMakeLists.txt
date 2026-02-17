cmake_minimum_required(VERSION 3.15)
project(MLRecorder VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Find packages
# Opus detection workaround (Config file reports version 0)
find_package(Opus CONFIG)
if(NOT TARGET Opus::opus)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(Opus REQUIRED IMPORTED_TARGET opus)
    if(NOT TARGET Opus::opus)
        add_library(Opus::opus ALIAS PkgConfig::Opus)
    endif()
endif()

# Ogg detection workaround
find_package(Ogg CONFIG)
if(NOT TARGET Ogg::ogg)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(Ogg REQUIRED IMPORTED_TARGET ogg)
    if(NOT TARGET Ogg::ogg)
        add_library(Ogg::ogg ALIAS PkgConfig::Ogg)
    endif()
endif()

# FLAC detection workaround
find_package(FLAC CONFIG)
if(NOT TARGET FLAC::FLAC)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(libflac REQUIRED IMPORTED_TARGET flac)
    if(NOT TARGET FLAC::FLAC)
        add_library(FLAC::FLAC ALIAS PkgConfig::libflac)
    endif()
    
    pkg_check_modules(libflacpp REQUIRED IMPORTED_TARGET flac++)
    if(NOT TARGET FLAC::FLAC++)
        add_library(FLAC::FLAC++ ALIAS PkgConfig::libflacpp)
    endif()
endif()

# nlohmann_json detection workaround
find_package(nlohmann_json CONFIG)
if(NOT TARGET nlohmann_json::nlohmann_json)
   find_package(PkgConfig REQUIRED)
   pkg_check_modules(nlohmann_json REQUIRED IMPORTED_TARGET nlohmann_json)
   if(NOT TARGET nlohmann_json::nlohmann_json)
       add_library(nlohmann_json::nlohmann_json ALIAS PkgConfig::nlohmann_json)
   endif()
endif()

# Shared core sources
set(CORE_SOURCES
    src/AudioCapture.cpp
    src/AudioDeviceEnumerator.cpp
    src/ProcessEnumerator.cpp
    src/WavWriter.cpp
    src/Mp3Encoder.cpp
    src/OpusEncoder.cpp
    src/FlacEncoder.cpp
    src/CaptureManager.cpp
    src/AudioMixer.cpp
)

set(CORE_API_SOURCES
    src/MLRecorderAPI.cpp
)

# GUI application sources
set(APP_SOURCES
    src/main.cpp
    src/App.cpp
    src/AppLogic.cpp
    src/AppSettings.cpp
    resource.rc
)

set(CORE_HEADERS
    include/App.h
    include/MLRecorderAPI.h
    include/AudioCapture.h
    include/ProcessEnumerator.h
    include/WavWriter.h
    include/Mp3Encoder.h
    include/OpusEncoder.h
    include/FlacEncoder.h
    include/CaptureManager.h
    include/AudioDeviceEnumerator.h
    include/AudioMixer.h
    include/resource.h
)

set(COMMON_WINDOWS_LIBS
    Avrt.lib
    Mmdevapi.lib
    Ksuser.lib
    Mfplat.lib
    Mfreadwrite.lib
    Mfuuid.lib
    Ole32.lib
    Shell32.lib
    Shlwapi.lib
    Propsys.lib
    RuntimeObject.lib
    Delayimp.lib
)

set(CORE_THIRD_PARTY_LIBS
    Opus::opus
    Ogg::ogg
    FLAC::FLAC FLAC::FLAC++
)

# Build core DLL with C ABI for wrappers
add_library(mlrecorder_core SHARED
    ${CORE_SOURCES}
    ${CORE_API_SOURCES}
    ${CORE_HEADERS}
)

target_include_directories(mlrecorder_core PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(mlrecorder_core PRIVATE
    ${COMMON_WINDOWS_LIBS}
    ${CORE_THIRD_PARTY_LIBS}
)

target_compile_definitions(mlrecorder_core PRIVATE
    MLR_CORE_BUILD
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

set_target_properties(mlrecorder_core PROPERTIES
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
)

# Build existing GUI executable
add_executable(MLRecorder WIN32
    ${CORE_SOURCES}
    ${APP_SOURCES}
    ${CORE_HEADERS}
)

target_include_directories(MLRecorder PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(MLRecorder PRIVATE
    ${COMMON_WINDOWS_LIBS}
    Comctl32.lib
    ${CORE_THIRD_PARTY_LIBS}
    nlohmann_json::nlohmann_json
)

# Enable Windows subsystem and static runtime
set_target_properties(MLRecorder PROPERTIES
    WIN32_EXECUTABLE TRUE
    LINK_FLAGS "/DELAYLOAD:api-ms-win-core-winrt-l1-1-0.dll"
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
)

# Add compile definitions
target_compile_definitions(MLRecorder PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

# Enable warnings
if(MSVC)
    target_compile_options(mlrecorder_core PRIVATE /W4)
    target_compile_options(MLRecorder PRIVATE /W4)
else()
    target_compile_options(mlrecorder_core PRIVATE -Wall -Wextra)
    target_compile_options(MLRecorder PRIVATE -Wall -Wextra)
endif()

# Install artifacts useful for SDK/wrapper distribution
install(TARGETS MLRecorder mlrecorder_core
    RUNTIME DESTINATION .
    LIBRARY DESTINATION .
)
install(FILES include/MLRecorderAPI.h DESTINATION include)
