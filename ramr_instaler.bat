@echo off
chcp 65001
setlocal enabledelayedexpansion

title RAMR Installer

echo.
echo RAMR Interpreter Installer v1.0
echo ================================
echo.

:: Проверка прав администратора
echo [1/7] Проверка прав администратора...
fltmc >nul 2>&1
if not %errorLevel% == 0 (
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

:: Проверка наличия исходного файла
echo [2/7] Проверка исходного кода...
if not exist "ramr_interpreter.c" (
    echo [X] Файл ramr_interpreter.c не найден!
    echo Убедитесь, что он находится в той же папке
    pause
    exit /b 1
)
echo [✓] Исходный код найден
echo.

:: Проверка GCC
echo [3/7] Поиск компилятора GCC...
where gcc >nul 2>&1
if not %errorLevel% == 0 (
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

:: Компиляция
echo [4/7] Компиляция интерпретатора...
gcc -std=c11 -O2 -o ramr.exe ramr_interpreter.c
if not exist "ramr.exe" (
    echo [X] Ошибка компиляции!
    echo Проверьте наличие файла ramr_interpreter.c
    echo и установку GCC
    pause
    exit /b 1
)
echo [✓] Интерпретатор скомпилирован
echo.

:: Создание папки установки
echo [5/7] Создание папки установки...
set "RAMR_DIR=C:\RAMR"
if not exist "%RAMR_DIR%" (
    mkdir "%RAMR_DIR%"
    if not %errorLevel% == 0 (
        echo [X] Не удалось создать папку %RAMR_DIR%
        pause
        exit /b 1
    )
)
echo [✓] Папка создана: %RAMR_DIR%
echo.

:: Копирование файлов
echo [6/7] Копирование файлов...
copy "ramr.exe" "%RAMR_DIR%\"
if not exist "%RAMR_DIR%\ramr.exe" (
    echo [X] Не удалось скопировать ramr.exe
    pause
    exit /b 1
)

if exist "examples" (
    xcopy "examples" "%RAMR_DIR%\examples\" /E /I /Y >nul
    echo [✓] Примеры скопированы
)
echo [✓] Файлы установлены
echo.

:: Обновление PATH
echo [7/7] Обновление системного PATH...
set "CURRENT_PATH="
for /f "tokens=2,*" %%A in ('reg query "HKLM\SYSTEM\CurrentControlSet\Control\Session Manager\Environment" /v Path 2^>nul') do set "CURRENT_PATH=%%B"

if "!CURRENT_PATH!"=="" (
    echo [!] Не удалось прочитать текущий PATH
    set "CURRENT_PATH=%PATH%"
)

echo !CURRENT_PATH! | find /i "%RAMR_DIR%" >nul
if not !errorLevel! == 0 (
    set "NEW_PATH=!CURRENT_PATH!;%RAMR_DIR%"
    setx PATH "!NEW_PATH!" /M >nul
    if not %errorLevel% == 0 (
        echo [!] Не удалось обновить PATH автоматически
        echo Добавьте вручную в PATH: %RAMR_DIR%
    ) else (
        echo [✓] PATH обновлен
    )
) else (
    echo [✓] PATH уже содержит RAMR
)
echo.

:: Создание ярлыков
echo [8/8] Создание ярлыков...
set "DESKTOP=%USERPROFILE%\Desktop"
set "START_MENU=%APPDATA%\Microsoft\Windows\Start Menu\Programs"

powershell -Command "$s=(New-Object -COM WScript.Shell).CreateShortcut('%DESKTOP%\RAMR.lnk');$s.TargetPath='cmd.exe';$s.Arguments='/k cd /d \"%RAMR_DIR%\" && echo RAMR Interpreter && ramr';$s.WorkingDirectory='%RAMR_DIR%';$s.Description='RAMR Programming Language';$s.Save()" 2>nul
if exist "%DESKTOP%\RAMR.lnk" (
    echo [✓] Ярлык на рабочем столе создан
) else (
    echo [!] Не удалось создать ярлык на рабочем столе
)

powershell -Command "$s=(New-Object -COM WScript.Shell).CreateShortcut('%START_MENU%\RAMR.lnk');$s.TargetPath='cmd.exe';$s.Arguments='/k cd /d \"%RAMR_DIR%\" && echo RAMR Interpreter && ramr';$s.WorkingDirectory='%RAMR_DIR%';$s.Description='RAMR Programming Language';$s.Save()" 2>nul
if exist "%START_MENU%\RAMR.lnk" (
    echo [✓] Ярлык в меню Пуск создан
) else (
    echo [!] Не удалось создать ярлык в меню Пуск
)

echo.
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
echo Нажмите любую клавишу для выхода...
pause >nul
