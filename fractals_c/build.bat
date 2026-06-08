@echo off

:: Set up Visual Studio Build Tools environment
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat"

if %ERRORLEVEL% NEQ 0 (
    echo Failed to set up build environment.
    pause
    exit /b %ERRORLEVEL%
)

:: Remove previous executable if it exists
if exist mandelbrot.exe del /f /q mandelbrot.exe

echo Building Mandelbrot Set Explorer with MSVC...
cl /O2 /MD /Fe:mandelbrot.exe main.c renderer.c mandelbrot.c user32.lib gdi32.lib opengl32.lib
if %ERRORLEVEL% NEQ 0 (
    echo Build failed with error code %ERRORLEVEL%.
    pause
    exit /b %ERRORLEVEL%
)

echo Build succeeded!
echo Run mandelbrot.exe to launch.
