@echo off
setlocal

echo ====================================================================
echo   Building OpenSource-AWUS036ACHM Windows 11 Driver & Control Tool
echo ====================================================================

call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"

echo.
echo [1/3] Building awus_control.exe and set_friendly_name.exe...
cd /d "%~dp0tools\awus_control"
cl.exe /nologo /W4 /O2 /Fe:awus_control.exe main.c
if %ERRORLEVEL% NEQ 0 (
    echo [-] Error building awus_control.exe
    exit /b %ERRORLEVEL%
)
echo [+] awus_control.exe built successfully.

cd /d "%~dp0tools"
cl.exe /nologo /W4 /O2 /Fe:set_friendly_name.exe set_friendly_name.c setupapi.lib advapi32.lib user32.lib
if %ERRORLEVEL% NEQ 0 (
    echo [-] Error building set_friendly_name.exe
    exit /b %ERRORLEVEL%
)
echo [+] set_friendly_name.exe built successfully.

echo.
echo [2/3] Building awus036achm.sys (Kernel-Mode Driver)...
if exist "%~dp0bin\Release\awus036achm\awus036achm.cat" del /f /q "%~dp0bin\Release\awus036achm\awus036achm.cat" >nul 2>&1
"C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" "%~dp0driver\awus036achm.vcxproj" /p:Configuration=Release /p:Platform=x64 /p:VCTargetsPath="%~dp0tools\custom_v170\\"
if %ERRORLEVEL% NEQ 0 (
    echo [-] Error building awus036achm.sys
    exit /b %ERRORLEVEL%
)
echo [+] awus036achm.sys built successfully.

echo.
echo [3/3] Generating and Signing Driver Catalog (awus036achm.cat)...
"C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x86\Inf2Cat.exe" /driver:"%~dp0bin\Release\awus036achm" /os:10_X64,10_NI_X64,10_GE_X64 >nul 2>&1
"C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x86\signtool.exe" sign /ph /fd sha256 /sha1 F41A2363730CE778BA094C04A0D9D1EFA0F6C6BE "%~dp0bin\Release\awus036achm\awus036achm.cat" >nul 2>&1
copy /y "%~dp0bin\Release\awus036achm\awus036achm.cat" "%~dp0driver\awus036achm.cat" >nul
copy /y "%~dp0bin\Release\awus036achm\awus036achm.cer" "%~dp0driver\awus036achm.cer" >nul
copy /y "%~dp0bin\Release\awus036achm\awus036achm.inf" "%~dp0driver\awus036achm.inf" >nul
echo [+] awus036achm.cat generated and signed with SHA-256.

echo.
echo ====================================================================
echo   Build and Signing Pipeline Finished!
echo   Package ready for Device Manager in: %~dp0driver\
echo     - awus036achm.inf
echo     - awus036achm.sys (Signed)
echo     - awus036achm.cat (Signed)
echo     - awus036achm.cer (Certificate)
echo ====================================================================
