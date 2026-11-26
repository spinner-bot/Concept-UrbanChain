@echo off
rem Launch Concept UrbanChain with the irregular 65-station city (city60).
setlocal
cd /d "%~dp0"

if exist "build\urbanchain.exe" goto :run
echo [city60] Building Concept UrbanChain...
call "%~dp0tools\build.bat"
if errorlevel 1 (
    echo [city60] Build failed.
    pause
    exit /b 1
)

:run
if not exist "data\city60.cuc4m" (
    echo [city60] data\city60.cuc4m not found.
    pause
    exit /b 1
)
if not exist "data\city60.cuc3l" (
    echo [city60] data\city60.cuc3l not found.
    pause
    exit /b 1
)

start "" /D "%~dp0" "%~dp0build\urbanchain.exe" --map "%~dp0data\city60.cuc4m" --lines "%~dp0data\city60.cuc3l" --lang zh
exit /b 0
