@echo off
setlocal
if not exist "%~dp0GRID-ARVx.exe" (
  echo GRID-ARVx.exe is missing. Extract the entire folder first.
  pause
  exit /b 3
)
start "" "%~dp0GRID-ARVx.exe"
