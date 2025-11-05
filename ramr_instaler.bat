@echo off
chcp 65001 >nul
title RAMR Installer
color 0A
set "SCRIPT_DIR=%~dp0"
echo.
echo =============================
echo       RAMR Installer
echo =============================
echo.
echo RAMR Ver 0.02
echo Step 1: Checking admin rights...
net session >nul 2>&1
if errorlevel 1 (
    echo [ERROR] Please run this installer as Administrator!
    pause
    exit /b 1
)
echo [OK] Admin rights confirmed
echo.
echo Step 2: Checking for source file...
if not exist "%SCRIPT_DIR%ramr_interpreter.c" (
    echo [ERROR] ramr_interpreter.c not found in installer folder!
    pause
    exit /b 1
)
echo [OK] Source file found
echo.
echo Step 3: Checking for Tiny C Compiler...
set "COMPILER=%SCRIPT_DIR%tcc\tcc.exe"
if not exist "%COMPILER%" (
    echo [ERROR] tcc.exe not found in "tcc" folder!
    pause
    exit /b 1
)
echo [OK] TCC found
echo.
echo Step 4: Compiling interpreter...
"%COMPILER%" -o "%SCRIPT_DIR%ramr.exe" "%SCRIPT_DIR%ramr_interpreter.c" -ladvapi32 -lgdi32 -luser32

if not exist "%SCRIPT_DIR%ramr.exe" (
    echo [ERROR] Compilation failed!
    pause
    exit /b 1
)
echo [OK] Compiled successfully
echo.
set "RAMR_DIR=C:\RAMR"
echo Step 5: Creating installation folder at %RAMR_DIR%...
if not exist "%RAMR_DIR%" mkdir "%RAMR_DIR%"
mkdir "%RAMR_DIR%\Module"
echo [OK] Folder ready
echo.
echo Step 6: Copying files...
copy /y "%SCRIPT_DIR%ramr.exe" "%RAMR_DIR%\" >nul
if exist "%SCRIPT_DIR%examples" (
    xcopy "%SCRIPT_DIR%examples" "%RAMR_DIR%\examples\" /E /I /Y >nul
    echo [OK] Examples copied
)
echo [OK] Files installed
echo.
echo Step 7: Updating PATH...
echo %PATH% | find /i "%RAMR_DIR%" >nul
if errorlevel 1 (
    setx PATH "%PATH%;%RAMR_DIR%" /M >nul
    echo [OK] PATH updated
) else (
    echo [INFO] PATH already contains RAMR directory
)
echo.
echo Step 8: Registering .ramr file association...
set "ICON_FILE=%SCRIPT_DIR%icon.ico"
if exist "%ICON_FILE%" (
    echo [OK] Icon found
) else (
    echo [WARN] icon.ico not found! Skipping icon setup...
)
reg add "HKCR\.ramr" /ve /d "RAMR.File" /f >nul
reg add "HKCR\RAMR.File" /ve /d "RAMR Script File" /f >nul
if exist "%ICON_FILE%" (
    reg add "HKCR\RAMR.File\DefaultIcon" /ve /d "\""%ICON_FILE%\"",0" /f >nul
)
reg add "HKCR\RAMR.File\shell\open\command" /ve /d "\""%RAMR_DIR%\ramr.exe\"" \"%%1\"" /f >nul
powershell -NoLogo -NoProfile -ExecutionPolicy Bypass -Command "Start-Sleep -Milliseconds 300; ie4uinit.exe -show; RUNDLL32.EXE user32.dll,UpdatePerUserSystemParameters" >nul 2>&1
echo [OK] Explorer refreshed safely
echo.
echo ================================
echo   INSTALLATION COMPLETE!
echo ================================
echo Installed to: %RAMR_DIR%
echo.
echo Usage: ramr script.ramr
echo Restart CMD and try: ramr --help
echo.
pause
exit /b
