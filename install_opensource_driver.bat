@echo off
setlocal EnableDelayedExpansion
title OpenSource ALFA AWUS036ACHM Driver Installer

:: 1. Check for Administrative privileges
net session >nul 2>&1
if %errorlevel% neq 0 (
    echo ====================================================================
    echo   Requesting Administrator Privileges...
    echo ====================================================================
    powershell -NoProfile -ExecutionPolicy Bypass -Command "Start-Process cmd.exe -ArgumentList '/k \"\"%~f0\"\"' -Verb RunAs"
    exit /b
)

:: 2. Set working directory to script location
cd /d "%~dp0"

echo ====================================================================
echo   Installing OpenSource ALFA AWUS036ACHM Windows 11 Driver
echo ====================================================================
echo.

:: 3. Terminate any previous hung installer processes
taskkill /F /IM pnputil.exe >nul 2>&1
taskkill /F /IM drvinst.exe >nul 2>&1

:: 4. Execute the PowerShell installer directly in this elevated console
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0install_opensource_driver.ps1"

echo.
echo ====================================================================
echo   Installation script execution finished.
echo ====================================================================
pause
