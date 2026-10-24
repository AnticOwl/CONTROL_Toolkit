@echo off
setlocal
cd /d "%~dp0"

echo [1/2] Configuring x64 Release project...
cmake -S . -B build -A x64
if errorlevel 1 goto :fail

echo [2/2] Building Release...
cmake --build build --config Release
if errorlevel 1 goto :fail

echo.
echo Build OK:
echo   %CD%\build\Release\ControlToolkit.dll
echo.
pause
exit /b 0

:fail
echo.
echo BUILD FAILED.
echo.
pause
exit /b 1
