@echo off
setlocal EnableExtensions

set "INSTALL_DIR=C:\Bulba"

echo.
echo ========================================
echo       BULBA ENGINE UNINSTALLER
echo ========================================
echo.
echo Install directory: %INSTALL_DIR%
echo.

powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "$ErrorActionPreference='Continue'; $root=$env:INSTALL_DIR; if(-not(Test-Path -LiteralPath $root)){Write-Host '[INFO] Install directory not found.'; exit 0}; $files=@(Get-ChildItem -LiteralPath $root -Recurse -File -Force -ErrorAction SilentlyContinue | Where-Object { $_.FullName -match 'bulba_engine' -or $_.FullName -match '\\include\\' } | Select-Object -ExpandProperty FullName -Unique); if($files.Count -eq 0){Write-Host '[INFO] No matching installed files found.'; exit 0}; Write-Host ('Found ' + $files.Count + ' file(s):'); $files | ForEach-Object {Write-Host ('  ' + $_)}; Write-Host ''; $answer=Read-Host 'Do you want to remove these files? [y/N]'; if($answer -notmatch '^(y|yes)$'){Write-Host '[INFO] Uninstallation cancelled.'; exit 0}; $failed=$false; foreach($file in $files){try{Remove-Item -LiteralPath $file -Force -ErrorAction Stop; Write-Host ('[OK] Removed: ' + $file)}catch{Write-Host ('[ERROR] Could not remove: ' + $file); $failed=$true}}; $dirs=@(Get-ChildItem -LiteralPath $root -Directory -Recurse -Force -ErrorAction SilentlyContinue | Sort-Object FullName -Descending); foreach($dir in $dirs){try{Remove-Item -LiteralPath $dir.FullName -Force -ErrorAction Stop}catch{}}; try{Remove-Item -LiteralPath $root -Force -ErrorAction Stop}catch{}; if($failed){Write-Host '[ERROR] Some files could not be removed.'; exit 1}else{Write-Host '[SUCCESS] Bulba Engine uninstalled successfully.'; exit 0}"

if errorlevel 1 (
    echo.
    echo [ERROR] Uninstallation failed.
    echo Try running this script as Administrator.
    exit /b 1
)

echo.
exit /b 0
