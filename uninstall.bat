@echo off
chcp 65001 >nul
echo.
echo RAMR Uninstaller
echo ================
echo.

:: Проверка прав администратора
fltmc >nul 2>&1 || (
    echo [ОШИБКА] Запустите от имени администратора
    pause
    exit /b 1
)

set "RAMR_DIR=C:\Program Files\RAMR"
if not exist "%RAMR_DIR%" (
    set "RAMR_DIR=C:\RAMR"
)

echo Удаление RAMR из системы...
echo.

:: Удаление из PATH
echo [1/3] Удаление из системного PATH...
for /f "skip=2 tokens=1,2*" %%A in ('reg query "HKLM\SYSTEM\CurrentControlSet\Control\Session Manager\Environment" /v Path 2^>nul') do (
    if /i "%%A"=="Path" (
        set "CURRENT_PATH=%%C"
    )
)

set "NEW_PATH=%CURRENT_PATH:%RAMR_DIR%=%"
set "NEW_PATH=%NEW_PATH:;;=%"
setx PATH "%NEW_PATH%" /M >nul
echo [✓] PATH обновлен

:: Удаление файлов
echo [2/3] Удаление файлов...
if exist "%RAMR_DIR%" (
    rmdir /s /q "%RAMR_DIR%"
    echo [✓] Файлы удалены
)

:: Удаление ярлыков
echo [3/3] Удаление ярлыков..."
set "DESKTOP=%USERPROFILE%\Desktop"
set "START_MENU=%APPDATA%\Microsoft\Windows\Start Menu\Programs"

del "%DESKTOP%\RAMR.lnk" 2>nul
del "%START_MENU%\RAMR.lnk" 2>nul
echo [✓] Ярлыки удалены

echo.
echo ================================
echo    Удаление завершено!
echo ================================
echo.
pause
