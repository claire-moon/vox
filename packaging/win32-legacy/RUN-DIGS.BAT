@echo off
rem SPDX-License-Identifier: GPL-3.0-or-later
rem This deliberately uses only COMMAND.COM-era batch syntax.
if not exist DIGS95.EXE goto missing
DIGS95.EXE
goto end
:missing
echo DIGS: DIGS95.EXE is missing.
echo Extract the complete ZIP and run RUN-DIGS.BAT from its root folder.
pause
:end
