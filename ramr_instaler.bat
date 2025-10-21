@echo off
chcp 65001 >nul
setlocal enabledelayedexpansion

title RAMR Installer

:ADMIN_CHECK
echo.
echo RAMR Interpreter Installer v1.0
echo ================================
echo.

:: Проверка прав администратора
fltmc >nul 2>&1 || (
    echo [X] Требуются права администратора
    echo.
    echo Запустите этот файл от имени администратора:
    echo 1. Правый клик на файле
    echo 2. "Запуск от имени администратора"
    echo.
    pause
    exit /b 1
)

echo [✓] Права администратора подтверждены
echo.

:CHECK_GCC
echo [1/6] Поиск компилятора GCC...
where gcc >nul 2>&1
if %errorLevel% neq  (
    echo [X] GCC не найден в системе!
    echo.
    echo Установите MinGW:
    echo 1. Скачайте с: https://sourceforge.net/projects/mingw/
    echo 2. Установите пакет mingw32-gcc-g++
    echo 3. Добавьте C:\MinGW\bin в системный PATH
    echo 4. Перезапустите установщик
    echo.
    pause
    exit /b 1
)
echo [✓] GCC найден
echo.

:COMPILE
echo [2/6] Компиляция интерпретатора...
gcc -std=c11 -O2 -o ramr.exe ramr_interpreter.c
if not exist "ramr.exe" (
    echo [X] Ошибка компиляции!
    echo Проверьте наличие файла ramr_interpreter.c
    pause
    exit /b 1
)
echo [✓] Интерпретатор скомпилирован
echo.

:INSTALL_DIR
echo [3/6] Создание папки установки...
set "RAMR_DIR=C:\Program Files\RAMR"
if not exist "%RAMR_DIR%" (
    mkdir "%RAMR_DIR%"
    if %errorLevel% neq  (
        set "RAMR_DIR=C:\RAMR"
        mkdir "%RAMR_DIR%"
    )
)
echo [✓] Папка: %RAMR_DIR%
echo.

:COPY_FILES
echo [4/6] Копирование файлов...
copy "ramr.exe" "%RAMR_DIR%\" >nul
if exist "examples" (
    xcopy "examples" "%RAMR_DIR%\examples\" /E /I /Y >nul
    echo [✓] Примеры скопированы
)
echo [✓] Файлы установлены
echo.

:UPDATE_PATH
echo [5/6] Обновление системного PATH...
for /f "skip=2 tokens=1,2*" %%A in ('reg query "HKLM\SYSTEM\CurrentControlSet\Control\Session Manager\Environment" /v Path 2^>nul') do (
    if /i "%%A"=="Path" (
        set "CURRENT_PATH=%%C"
    )
)

echo %CURRENT_PATH% | find /i "%RAMR_DIR%" >nul
if %errorLevel% neq  (
    set "NEW_PATH=%CURRENT_PATH%;%RAMR_DIR%"
    setx PATH "!NEW_PATH!" /M >nul
    echo [✓] PATH обновлен
) else (
    echo [✓] PATH уже содержит RAMR
)
echo.

:CREATE_SHORTCUTS
echo [6/6] Создание ярлыков...
set "DESKTOP=%USERPROFILE%\Desktop"
set "START_MENU=%APPDATA%\Microsoft\Windows\Start Menu\Programs"

:: Ярлык на рабочем столе
powershell -Command "$s=(New-Object -COM WScript.Shell).CreateShortcut('%DESKTOP%\RAMR.lnk');$s.TargetPath='cmd.exe';$s.Arguments='/k cd /d \"%RAMR_DIR%\" && ramr';$s.WorkingDirectory='%RAMR_DIR%';$s.Description='RAMR Programming Language';$s.IconLocation='%RAMR_DIR%\ramr.exe,0';$s.Save()" >nul 2>&1

:: Ярлык в меню Пуск
powershell -Command "$s=(New-Object -COM WScript.Shell).CreateShortcut('%START_MENU%\RAMR.lnk');$s.TargetPath='cmd.exe';$s.Arguments='/k cd /d \"%RAMR_DIR%\" && ramr';$s.WorkingDirectory='%RAMR_DIR%';$s.Description='RAMR Programming Language';$s.IconLocation='%RAMR_DIR%\ramr.exe,0';$s.Save()" >nul 2>&1

echo [✓] Ярлыки созданы
echo.

:SUCCESS
echo ================================
echo     УСТАНОВКА ЗАВЕРШЕНА!
echo ================================
echo.
echo Установлено в: %RAMR_DIR%
echo.
echo Доступные команды:
echo   ramr script.ramr     - запуск скрипта
echo   ramr                 - интерактивный режим
echo.
echo Перезапустите командную строку и введите: ramr --help
echo.
pause
