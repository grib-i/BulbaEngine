@echo off
setlocal EnableExtensions

set "INSTALL_DIR=C:\Bulba"
set "FOUND=0"
set "FAILED=0"

echo.
echo ========================================
echo       BULBA ENGINE UNINSTALLER
echo ========================================
echo.
echo Install directory: %INSTALL_DIR%
echo.

call :remove "%INSTALL_DIR%\bin\bulba_engine.dll"
call :remove "%INSTALL_DIR%\lib\bulba_engine.lib"

echo.

if "%FAILED%"=="1" (
    echo [ERROR] Failed to remove some files.
    echo Try running this script as Administrator.
    exit /b 1
)

if "%FOUND%"=="0" (
    echo [INFO] Bulba Engine library files were not found.
    exit /b 0
)

rem Remove directories only if they are empty.
if exist "%INSTALL_DIR%\bin" rmdir "%INSTALL_DIR%\bin" 2>nul
if exist "%INSTALL_DIR%\lib" rmdir "%INSTALL_DIR%\lib" 2>nul
if exist "%INSTALL_DIR%" rmdir "%INSTALL_DIR%" 2>nul

echo [SUCCESS] Bulba Engine uninstalled successfully.
echo.

exit /b 0


:remove
if not exist "%~1" exit /b 0

del /f /q "%~1" >nul 2>&1

if exist "%~1" (
    echo [ERROR] Could not remove: %~1
    set "FAILED=1"
) else (
    echo [OK] Removed: %~1
    set "FOUND=1"
)

exit /b 0
