@echo off
rem ============================================================
rem  Smart Teaching Management System - build script
rem  (pure ASCII on purpose: cmd.exe misparses UTF-8 batch files
rem   when the code page is switched to 65001)
rem  Usage:  build.bat            build all + launch GUI
rem          build.bat nolaunch   build all only, keep window
rem ============================================================
setlocal enabledelayedexpansion

echo ============================================================
echo   Smart Teaching Management System  /  Build Script
echo ============================================================
echo.

rem ---------------- 1) locate the gcc compiler ----------------
set "GCC=gcc"
where gcc >nul 2>&1
if not errorlevel 1 goto found

echo [INFO] gcc is not in PATH, searching common install folders...
for %%D in (
    "C:\MinGW\bin"
    "C:\mingw\bin"
    "C:\mingw32\bin"
    "C:\mingw64\bin"
    "C:\msys64\mingw64\bin"
    "C:\msys64\mingw32\bin"
    "C:\TDM-GCC-64\bin"
    "C:\TDM-GCC-32\bin"
    "C:\w64devkit\bin"
    "C:\Dev-Cpp\MinGW64\bin"
    "C:\Program Files\Dev-Cpp\MinGW64\bin"
    "C:\Program Files (x86)\Dev-Cpp\MinGW64\bin"
    "C:\Program Files\CodeBlocks\MinGW\bin"
    "C:\Program Files (x86)\CodeBlocks\MinGW\bin"
) do (
    if exist "%%~D\gcc.exe" (
        set "PATH=%%~D;!PATH!"
        set "GCC=%%~D\gcc.exe"
        echo [OK] compiler found: %%~D\gcc.exe
        goto found
    )
)

echo.
echo [ERROR] gcc compiler not found. Please install it and add it to PATH:
echo.
echo   Step 1 - install (pick one, e.g. into C:\MinGW)
echo       A) portable : download w64devkit, extract to C:\w64devkit
echo       B) installer: download MinGW.org or TDM-GCC, install to C:\MinGW
echo       C) already have Dev-C++: compiler is at C:\Dev-Cpp\MinGW64\bin
echo.
echo   Step 2 - add the folder that CONTAINS gcc.exe to PATH
echo       This PC - right click Properties - Advanced system settings
echo       - Environment Variables - User variables - Path - Edit
echo       - New - type  C:\MinGW\bin  - OK
echo.
echo   Step 3 - verify: open a NEW command window and run:  gcc --version
echo.
echo   See section 3 of README.md for details.
echo.
pause
exit /b 1

:found
echo [compiler]
"%GCC%" --version | findstr /i "gcc"
echo.

rem ---------------- 2) build the three programs ----------------
echo [1/3] building Win32 GUI version      teaching_gui.exe ...
"%GCC%" gui_main.c teaching_core.c -o teaching_gui.exe -std=c11 -O2 -Wall -mwindows -lcomctl32 -lgdi32 -luser32 -lm
if errorlevel 1 goto fail

echo [2/3] building layered console version smart_teaching_split.exe ...
"%GCC%" console_main.c teaching_core.c -o smart_teaching_split.exe -std=c11 -O2 -Wall
if errorlevel 1 goto fail

echo [3/3] building single-file version     smart_teaching.exe ...
"%GCC%" smart_teaching.c -o smart_teaching.exe -std=c11 -O2 -Wall
if errorlevel 1 goto fail

echo.
echo ============================================================
echo   BUILD OK. Generated programs:
echo     teaching_gui.exe           GUI version (mouse, best for demo)
echo     smart_teaching_split.exe   layered console version
echo     smart_teaching.exe         single-file console version
echo ============================================================
echo.

if /i "%~1"=="nolaunch" goto done
echo starting the GUI version ...
start "" teaching_gui.exe
goto done

:fail
echo.
echo [ERROR] build failed. Please send the messages above for help.
pause
exit /b 1

:done
if /i "%~1"=="nolaunch" pause
endlocal
