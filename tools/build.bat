@echo off
setlocal

set "VSPATH=%UC_VSPATH%"
if defined VSPATH goto :have_vs

if exist "D:\VisualStudio\Packages\VC\Auxiliary\Build\vcvars64.bat" (
    set "VSPATH=D:\VisualStudio\Packages"
    goto :have_vs
)

set "VSWHERE=C:\Progra~2\Microsoft Visual Studio\Installer\vswhere.exe"
if exist "%VSWHERE%" goto :query_vs

echo [build] vswhere.exe not found.
exit /b 1

:query_vs
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -property installationPath`) do set "VSPATH=%%i"
if not defined VSPATH (
    echo [build] Visual Studio installation not found.
    exit /b 1
)

:have_vs
call "%VSPATH%\VC\Auxiliary\Build\vcvars64.bat" >nul 2>nul
if errorlevel 1 (
    echo [build] Failed to initialize MSVC environment.
    exit /b 1
)

set "CMAKE=%VSPATH%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
set "NINJA_DIR=%VSPATH%\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja"
set "PATH=%NINJA_DIR%;%PATH%"

"%CMAKE%" -S "%~dp0.." -B "%~dp0..\build" -G Ninja -DCMAKE_BUILD_TYPE=Release
if errorlevel 1 exit /b 1

"%CMAKE%" --build "%~dp0..\build"
if errorlevel 1 exit /b 1

echo [build] OK
