@echo off
rem Launch Concept UrbanChain with the simulated city demo and planned lines.
setlocal
cd /d "%~dp0"

if exist "build\urbanchain.exe" goto :run
echo [city] Building Concept UrbanChain...
call "%~dp0tools\build.bat"
if errorlevel 1 (
    echo [city] Build failed.
    pause
    exit /b 1
)

:run
if not exist "data\city_demo.cuc4m" (
    echo [city] data\city_demo.cuc4m not found.
    pause
    exit /b 1
)
if not exist "data\city_demo.cuc3l" (
    echo [city] data\city_demo.cuc3l not found.
    pause
    exit /b 1
)

start "" /D "%~dp0" "%~dp0build\urbanchain.exe" --map "%~dp0data\city_demo.cuc4m" --lines "%~dp0data\city_demo.cuc3l" --lang zh
exit /b 0
