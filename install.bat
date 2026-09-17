@echo off
setlocal

set PROJECT_NAME=bulba
set BUILD_TYPE=Debug

if /I "%1"=="release" set BUILD_TYPE=Release

echo ================================
echo BulbaEngine
echo Build type: %BUILD_TYPE%
echo ================================
echo.

where cmake >nul 2>&1
if errorlevel 1 (
    echo [ERROR] CMake not found
    exit /b 1
)

where ninja >nul 2>&1
if errorlevel 1 (
    echo [ERROR] Ninja not found
    exit /b 1
)

echo [OK] Required tools found
echo.

if /I "%BUILD_TYPE%"=="Debug" (
    echo [1/3] Configuring Debug...

    cmake -S . -B build\debug ^
      -G Ninja ^
      -DCMAKE_BUILD_TYPE=Debug ^
      -DPROJECT_NAME=%PROJECT_NAME% ^
      -DBLB_BUILD_APP=ON ^
      -DBLB_BUILD_SHADERS=ON

    if errorlevel 1 (
        echo [ERROR] CMake configuration failed
        exit /b 1
    )

    echo [OK] CMake configuration
    echo.

    echo [2/3] Building Debug...

    cmake --build build\debug --parallel

    if errorlevel 1 (
        echo [ERROR] Debug build failed
        exit /b 1
    )

    echo [OK] Debug build

    if not exist "build\debug\%PROJECT_NAME%.exe" (
        echo [ERROR] Debug binary was not created
        exit /b 1
    )

    echo [OK] Debug binary found
    echo.

    echo [3/3] Running test binary...

    "build\debug\%PROJECT_NAME%.exe"

    if errorlevel 1 (
        echo [ERROR] Debug application exited with an error
        exit /b 1
    )

    echo.
    echo ================================
    echo [SUCCESS] Debug build completed
    echo ================================
    exit /b 0
)

echo [1/3] Configuring Release...

cmake -S . -B build\release ^
  -G Ninja ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DPROJECT_NAME=%PROJECT_NAME% ^
  -DBLB_BUILD_APP=OFF ^
  -DBLB_BUILD_SHADERS=ON ^
  -DCMAKE_INSTALL_PREFIX=C:\Bulba

if errorlevel 1 (
    echo [ERROR] CMake configuration failed
    exit /b 1
)

echo [OK] CMake configuration
echo.

echo [2/3] Building Release...

cmake --build build\release --parallel

if errorlevel 1 (
    echo [ERROR] Release build failed
    exit /b 1
)

echo [OK] Release build

if not exist "build\release\bulba_engine.dll" (
    echo [ERROR] DLL was not created
    exit /b 1
)

echo [OK] Release DLL found
echo.

echo [3/3] Installing...

cmake --install build\release

if errorlevel 1 (
    echo [ERROR] Installation failed
    exit /b 1
)

if not exist "C:\Bulba\bin\bulba_engine.dll" (
    echo [ERROR] Installed DLL was not found
    exit /b 1
)

echo [OK] Installation completed

echo.
echo ================================
echo [SUCCESS] BulbaEngine installed
echo ================================

endlocal
exit /b 0
