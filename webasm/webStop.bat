@echo off
setlocal

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0webStop.ps1"

endlocal
