@echo off
rem SPDX-License-Identifier: GPL-3.0-or-later
setlocal
set "ROOT=%~dp0"

if not exist "%ROOT%bin\digs_demo.exe" (
    echo DIGS: missing bin\digs_demo.exe
    echo Extract the complete ZIP file, then run this launcher again.
    pause
    exit /b 1
)

start "DIGS" /D "%ROOT%bin" "%ROOT%bin\digs_demo.exe" %*
