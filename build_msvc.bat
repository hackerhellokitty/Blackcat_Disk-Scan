@echo off
setlocal EnableDelayedExpansion

:: ============================================================
::  BlackCat Disk Scanner  -  MSVC Build Script
::  Targets: Visual Studio 18 Insiders / MSVC 14.50 (x64)
:: ============================================================

set BUILD_DIR=build_c
set DIST_DIR=dist_c
set EXE_NAME=blackcat_disk_scanner.exe

:: ---- Locate vcvarsall.bat ----
set VCVARS=

:: VS 18 Insiders (first choice)
if exist "C:\Program Files\Microsoft Visual Studio\2022\Preview\VC\Auxiliary\Build\vcvarsall.bat" (
    set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Preview\VC\Auxiliary\Build\vcvarsall.bat"
    goto :found_vc
)

:: VS 18 Enterprise Insiders
if exist "C:\Program Files\Microsoft Visual Studio\18\Insiders\VC\Auxiliary\Build\vcvarsall.bat" (
    set "VCVARS=C:\Program Files\Microsoft Visual Studio\18\Insiders\VC\Auxiliary\Build\vcvarsall.bat"
    goto :found_vc
)

:: VS 2022 Enterprise fallback
if exist "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvarsall.bat" (
    set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvarsall.bat"
    goto :found_vc
)

:: VS 2022 Community fallback
if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" (
    set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat"
    goto :found_vc
)

echo [ERROR] Could not find vcvarsall.bat. Please install Visual Studio 2022 or later.
exit /b 1

:found_vc
echo [INFO] Using MSVC environment: %VCVARS%
call "%VCVARS%" x64
if errorlevel 1 (
    echo [ERROR] vcvarsall.bat failed.
    exit /b 1
)

:: ---- Check for CMake ----
where cmake >nul 2>&1
if errorlevel 1 (
    echo [ERROR] cmake not found in PATH. Please install CMake 3.20+.
    exit /b 1
)

:: ---- Check for Ninja (optional but preferred) ----
set GENERATOR=Ninja
where ninja >nul 2>&1
if errorlevel 1 (
    echo [WARN] Ninja not found; falling back to Visual Studio generator.
    set GENERATOR=Visual Studio 17 2022
)

:: ---- Create build directory ----
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

:: ---- Configure ----
echo.
echo [INFO] Configuring CMake (generator: %GENERATOR%) ...
if "%GENERATOR%"=="Ninja" (
    cmake -S . -B "%BUILD_DIR%" ^
        -G "Ninja" ^
        -DCMAKE_BUILD_TYPE=Release ^
        -DCMAKE_C_COMPILER=cl ^
        -DCMAKE_MAKE_PROGRAM=ninja
) else (
    cmake -S . -B "%BUILD_DIR%" ^
        -G "Visual Studio 17 2022" ^
        -A x64
)

if errorlevel 1 (
    echo [ERROR] CMake configuration failed.
    exit /b 1
)

:: ---- Build ----
echo.
echo [INFO] Building (Release) ...
cmake --build "%BUILD_DIR%" --config Release --parallel

if errorlevel 1 (
    echo [ERROR] Build failed.
    exit /b 1
)

:: ---- Collect output ----
if not exist "%DIST_DIR%" mkdir "%DIST_DIR%"

:: Ninja puts exe in build_c/
if exist "%BUILD_DIR%\%EXE_NAME%" (
    copy /Y "%BUILD_DIR%\%EXE_NAME%" "%DIST_DIR%\%EXE_NAME%"
    goto :copied
)

:: MSVC generator puts exe in build_c/Release/
if exist "%BUILD_DIR%\Release\%EXE_NAME%" (
    copy /Y "%BUILD_DIR%\Release\%EXE_NAME%" "%DIST_DIR%\%EXE_NAME%"
    goto :copied
)

echo [WARN] Could not locate built executable to copy.
goto :done

:copied
echo.
echo [SUCCESS] Build complete.
echo           Output: %DIST_DIR%\%EXE_NAME%

:done
endlocal
