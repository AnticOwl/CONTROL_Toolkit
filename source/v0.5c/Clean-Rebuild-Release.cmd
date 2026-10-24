@echo off
setlocal
cd /d "%~dp0"

if exist build (
  echo Removing old build directory...
  rmdir /s /q build
)

call Build-Release.cmd
