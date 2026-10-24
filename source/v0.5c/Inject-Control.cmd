@echo off
setlocal
cd /d "%~dp0"
if exist "build\Release\ControlToolkitInjector.exe" (
  pushd "build\Release"
  ControlToolkitInjector.exe
  set "RESULT=%errorlevel%"
  popd
) else (
  echo First run Build-Release.cmd to create the x64 injector and DLL.
  set "RESULT=1"
)
echo.
pause
exit /b %RESULT%
