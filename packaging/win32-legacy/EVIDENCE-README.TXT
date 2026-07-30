Legacy Win32 evidence
=====================

BUILD-INFO.TXT records the source commit, native legacy-host status, and cross
compiler. PE-IMPORTS.TXT is the exact i686-w64-mingw32-objdump -p output for
DIGS95.EXE.

The packager rejects non-PE32 output, non-4.0 PE header versions, unsupported
DLL imports, API-set DLLs, and common post-NT4 API imports. These static
checks establish a narrow binary baseline only.

Before describing the package as supported on a period operating system, add
separate QA evidence from real Windows 95 OSR2 or 98 SE, Windows NT 4 SP6, and
Windows XP SP3 guest runs. Do not treat Wine or a modern Windows smoke as
period-platform acceptance.
