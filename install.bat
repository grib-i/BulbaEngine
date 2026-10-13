@echo off
setlocal EnableExtensions

set "PROJECT_NAME=bulba"
set "BUILD_TYPE=Debug"
if /I "%~1"=="release" set "BUILD_TYPE=Release"
if /I "%~1"=="debug" set "BUILD_TYPE=Debug"

if not defined VCPKG_ROOT if defined VCPKG_INSTALLATION_ROOT set "VCPKG_ROOT=%VCPKG_INSTALLATION_ROOT%"
if not defined VCPKG_ROOT if exist "C:\vcpkg\scripts\buildsystems\vcpkg.cmake" set "VCPKG_ROOT=C:\vcpkg"

if not defined VCPKG_ROOT (
    echo [ERROR] VCPKG_ROOT is not set.
    echo Set it to the folder containing scripts\buildsystems\vcpkg.cmake.
    exit /b 1
)

if not exist "%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake" (
    echo [ERROR] vcpkg CMake toolchain not found: %VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake
    exit /b 1
)

if not exist "%VCPKG_ROOT%\vcpkg.exe" (
    if not exist "%VCPKG_ROOT%\bootstrap-vcpkg.bat" (
        echo [ERROR] vcpkg bootstrap script not found in %VCPKG_ROOT%
        exit /b 1
    )
    echo [1/4] Bootstrapping vcpkg...
    call "%VCPKG_ROOT%\bootstrap-vcpkg.bat" -disableMetrics
    if errorlevel 1 exit /b 1
)

if not defined VULKAN_SDK (
    echo [ERROR] VULKAN_SDK is not set. Install the Vulkan SDK first.
    exit /b 1
)

if not exist "%VULKAN_SDK%\Bin\glslc.exe" (
    echo [ERROR] glslc.exe not found under %VULKAN_SDK%\Bin
    exit /b 1
)

where cmake >nul 2>&1
if errorlevel 1 (
    echo [ERROR] CMake not found in PATH
    exit /b 1
)

set "BUILD_APP=OFF"
if /I "%BUILD_TYPE%"=="Debug" set "BUILD_APP=ON"
set "BUILD_DIR=build\windows"

echo ========================================
echo BulbaEngine - Windows / MSVC x64
echo Build type: %BUILD_TYPE%
echo vcpkg:      %VCPKG_ROOT%
echo Vulkan SDK: %VULKAN_SDK%
echo ========================================
echo.

echo [2/4] Configuring with vcpkg manifest dependencies...
cmake -S . -B "%BUILD_DIR%" -G "Visual Studio 17 2022" -A x64 ^
  "-DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake" ^
  -DVCPKG_TARGET_TRIPLET=x64-windows-static ^
  -DPROJECT_NAME=%PROJECT_NAME% ^
  -DBLB_BUILD_SHARED=ON ^
  -DBLB_BUILD_APP=%BUILD_APP%
if errorlevel 1 (
    echo [ERROR] CMake configuration failed
    exit /b 1
)

echo.
echo [3/4] Building %BUILD_TYPE%...
cmake --build "%BUILD_DIR%" --config %BUILD_TYPE% --parallel
if errorlevel 1 (
    echo [ERROR] Build failed
    exit /b 1
)

if /I "%BUILD_TYPE%"=="Debug" (
    if not exist "%BUILD_DIR%\Debug\%PROJECT_NAME%.exe" (
        echo [ERROR] Sample executable was not created
        exit /b 1
    )
    echo.
    echo [4/4] Starting the sample application...
    "%BUILD_DIR%\Debug\%PROJECT_NAME%.exe"
    if errorlevel 1 (
        echo [ERROR] Sample application exited with an error
        exit /b 1
    )
    exit /b 0
)

echo.
echo [4/4] Installing the shared library to C:\Bulba...
cmake --install "%BUILD_DIR%" --config Release --prefix C:\Bulba
if errorlevel 1 (
    echo [ERROR] Installation failed
    exit /b 1
)

if not exist "C:\Bulba\bin\bulba_engine.dll" (
    echo [ERROR] Installed DLL was not found
    exit /b 1
)

echo.
echo [SUCCESS] BulbaEngine installed to C:\Bulba
endlocal
exit /b 0
