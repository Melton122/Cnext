@echo off
REM Cnext Compiler Launcher
REM Usage: cnext [command] [arguments]

set "CNEXT_DIR=%~dp0"
set "PATH=%CNEXT_DIR%bin;%PATH%"

"%CNEXT_DIR%bin\cnext.exe" %*
