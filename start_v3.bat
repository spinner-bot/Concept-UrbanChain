@echo off
rem Concept UrbanChain launcher.
rem   start_v3.bat            build if needed, then launch
rem   start_v3.bat --rebuild  force a rebuild, then launch
setlocal
cd /d "%~dp0"

if /I "%~1"=="--rebuild" goto :build
if exist "build\urbanchain.exe" goto :run

:build
echo [start] Building Concept UrbanChain...
call "%~dp0tools\build.bat"
if errorlevel 1 (
    echo [start] Build failed. See the output above.
    pause
    exit /b 1
)

:run
if not exist "build\urbanchain.exe" (
    echo [start] build\urbanchain.exe was not found.
    pause
    exit /b 1
)

start "" /D "%~dp0" "%~dp0build\urbanchain.exe"
exit /b 0
