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

rem Keep the documented root-level share/ drop-in path useful even though
rem SDL_GetBasePath() resolves from bin/. An explicit user setting still wins.
if not defined DIGS_GAMECONTROLLERDB if exist "%ROOT%share\digs\controllers\gamecontrollerdb.txt" set "DIGS_GAMECONTROLLERDB=%ROOT%share\digs\controllers\gamecontrollerdb.txt"

start "DIGS" /D "%ROOT%bin" "%ROOT%bin\digs_demo.exe" %*
