@echo off
chcp 65001 >nul
title BlackCat Disk Scanner — Build EXE

echo.
echo  ██████╗ ██╗      █████╗  ██████╗██╗  ██╗ ██████╗ █████╗ ████████╗
echo  ██╔══██╗██║     ██╔══██╗██╔════╝██║ ██╔╝██╔════╝██╔══██╗╚══██╔══╝
echo  ██████╔╝██║     ███████║██║     █████╔╝ ██║     ███████║   ██║
echo  ██╔══██╗██║     ██╔══██║██║     ██╔═██╗ ██║     ██╔══██║   ██║
echo  ██████╔╝███████╗██║  ██║╚██████╗██║  ██╗╚██████╗██║  ██║   ██║
echo  ╚═════╝ ╚══════╝╚═╝  ╚═╝ ╚═════╝╚═╝  ╚═╝ ╚═════╝╚═╝  ╚═╝   ╚═╝
echo.
echo  BlackCat Disk Scanner v4.0.1 — EXE Builder
echo  =========================================
echo.

set EXE_NAME=blackcat_Disk_Scanner
set PY_FILE=disk_scan.py
set DIST_EXE=dist\%EXE_NAME%.exe

:: ══════════════════════════════════════════════════════
:: [0] ปิด EXE เก่าก่อน (ถ้ากำลังรันอยู่)
:: ══════════════════════════════════════════════════════
echo [0/5] ปิด EXE เก่าที่อาจค้างอยู่...
taskkill /f /im "%EXE_NAME%.exe" >nul 2>&1
:: รอให้ OS ปล่อย file lock
timeout /t 2 /nobreak >nul

:: ลบ EXE เก่าออกก่อน build (ป้องกัน PermissionError)
if exist "%DIST_EXE%" (
    del /f /q "%DIST_EXE%" >nul 2>&1
    if exist "%DIST_EXE%" (
        echo  [ERROR] ลบ EXE เก่าไม่ได้ — อาจถูก Antivirus ล็อกไว้
        echo          กรุณาปิดโปรแกรมที่ใช้ไฟล์นี้อยู่ หรือ Exclude โฟลเดอร์นี้จาก AV
        pause
        exit /b 1
    )
    echo   OK  ลบ EXE เก่าแล้ว
) else (
    echo   OK  ไม่มี EXE เก่า
)

:: ══════════════════════════════════════════════════════
:: [1] ตรวจสอบ flet
:: ══════════════════════════════════════════════════════
echo.
echo [1/5] ตรวจสอบ flet...
python -c "import flet; print('  flet version:', flet.version.version)" 2>nul
if errorlevel 1 (
    echo  [!] ไม่พบ flet — กำลัง install...
    pip install flet
)

:: ══════════════════════════════════════════════════════
:: [2] ตรวจสอบ pyinstaller
:: ══════════════════════════════════════════════════════
echo [2/5] ตรวจสอบ PyInstaller...
python -c "import PyInstaller; print('  PyInstaller:', PyInstaller.__version__)" 2>nul
if errorlevel 1 (
    echo  [!] ไม่พบ PyInstaller — กำลัง install...
    pip install pyinstaller
)

:: ══════════════════════════════════════════════════════
:: [3] ตรวจสอบไฟล์ที่ต้องใช้
:: ══════════════════════════════════════════════════════
echo [3/5] ตรวจสอบไฟล์...

if not exist "%PY_FILE%" (
    echo  [ERROR] ไม่พบ %PY_FILE%!
    pause
    exit /b 1
)
echo   OK  %PY_FILE%

if not exist "assets" mkdir assets
echo   OK  assets\

:: ไม่มี icon ให้ข้ามไป ไม่ error
set ICON_ARG=
if exist "assets\ssd.ico" (
    set ICON_ARG=--icon "assets\ssd.ico"
    echo   OK  assets\ssd.ico
) else (
    echo   --  assets\ssd.ico ไม่พบ ^(ข้าม^)
)

:: ══════════════════════════════════════════════════════
:: [4] Build
:: ══════════════════════════════════════════════════════
echo.
echo [4/5] กำลัง build EXE...
echo  (อาจใช้เวลา 1-3 นาที กรุณารอ)
echo.

:: ลอง flet pack ก่อน
flet pack %PY_FILE% ^
    --name "%EXE_NAME%" ^
    --add-data "assets;assets" ^
    %ICON_ARG% ^
    --product-name "BlackCat Disk Scanner" ^
    --file-description "HDD/SSD Sector Scanner" ^
    --product-version "4.0.1.0" ^
    --file-version "4.0.1.0" ^
    --company-name "Kuroneko Tools"

if errorlevel 1 (
    echo.
    echo  [!] flet pack ไม่สำเร็จ — ลอง PyInstaller โดยตรง...
    echo.

    pyinstaller ^
        --onefile ^
        --windowed ^
        --name "%EXE_NAME%" ^
        --add-data "assets;assets" ^
        %ICON_ARG% ^
        --uac-admin ^
        --hidden-import flet ^
        --hidden-import flet.fastapi ^
        --collect-all flet ^
        --noconfirm ^
        %PY_FILE%
)

:: ══════════════════════════════════════════════════════
:: [5] ตรวจสอบผลลัพธ์
:: ══════════════════════════════════════════════════════
echo.
echo [5/5] ตรวจสอบผลลัพธ์...
echo.

if exist "%DIST_EXE%" (
    echo  =========================================
    echo   BUILD สำเร็จ!
    echo  =========================================
    echo   ไฟล์: %DIST_EXE%
    for %%A in ("%DIST_EXE%") do (
        set /a MB=%%~zA / 1048576
        echo   ขนาด: %%~zA bytes  ^(~!MB! MB^)
    )
    echo.
    echo   วิธีใช้: รัน %DIST_EXE% ในฐานะ Administrator
    echo           ^(EXE นี้ขอ UAC อัตโนมัติเพราะใส่ --uac-admin^)
    echo  =========================================
    explorer dist
) else (
    echo  [ERROR] ไม่พบ %DIST_EXE% — Build ล้มเหลว
    echo.
    echo  สาเหตุที่พบบ่อย:
    echo    1. Antivirus ล็อกไฟล์ — Exclude โฟลเดอร์ dist\ และ build\
    echo    2. EXE เก่ายังถูกใช้งาน — ปิดแล้วรัน build ใหม่
    echo    3. Python 3.14 อาจยังไม่ compatible กับ PyInstaller เวอร์ชันนี้
    echo       ลอง: pip install --upgrade pyinstaller
)

echo.
pause