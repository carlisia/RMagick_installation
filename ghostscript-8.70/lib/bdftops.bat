@echo off
@rem $Id: bdftops.bat 6300 2005-12-28 19:56:24Z giles $

call gssetgs.bat
%GSC% -q -dBATCH -dNODISPLAY -- bdftops.ps %1 %2 %3 %4 %5 %6 %7 %8 %9
