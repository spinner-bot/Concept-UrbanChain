@echo off
rem Launch Concept UrbanChain with the 96-station generated city (deep plan).
setlocal
cd /d "%~dp0"

if exist "build\urbanchain.exe" goto :run
echo [city96] Building Concept UrbanChain...
call "%~dp0tools\build.bat"
if errorlevel 1 (
    echo [city96] Build failed.
    pause
    exit /b 1
)

:run
if not exist "data\city96.cuc4m" (
    echo [city96] data\city96.cuc4m not found.
    pause
    exit /b 1
)
if not exist "data\city96.cuc3l" (
    echo [city96] data\city96.cuc3l not found.
    pause
    exit /b 1
)

start "" /D "%~dp0" "%~dp0build\urbanchain.exe" --map "%~dp0data\city96.cuc4m" --lines "%~dp0data\city96.cuc3l" --lang zh
exit /b 0
