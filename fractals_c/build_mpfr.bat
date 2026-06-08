@echo off

:: Set up Visual Studio Build Tools environment (Community 18, x64)
call "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat" -arch=x64
if %ERRORLEVEL% NEQ 0 (
    echo Failed to set up Visual Studio build environment.
    pause
    exit /b %ERRORLEVEL%
)

cd /d "%~dp0"

:: Build Mandelbrot with MPFR support
if exist mandelbrot.exe del /f /q mandelbrot.exe

echo Building Mandelbrot Set Explorer with MSVC + MPFR...
cl /O2 /MD /DUSE_MPFR ^
   main.c renderer.c mandelbrot.c big_mandelbrot.c ^
   user32.lib gdi32.lib opengl32.lib ^
   /I"%USERPROFILE%\vcpkg\installed\x64-windows\include" ^
   /link /LIBPATH:"%USERPROFILE%\vcpkg\installed\x64-windows\lib" mpfr.lib gmp.lib

if %ERRORLEVEL% NEQ 0 (
    echo Build failed with error code %ERRORLEVEL%.
    pause
    exit /b %ERRORLEVEL%
)

echo.
echo Build succeeded! Run mandelbrot.exe to launch.
pause
