@echo off
@rem $Id: gsnd.bat 6300 2005-12-28 19:56:24Z giles $

call gssetgs.bat
%GSC% -DNODISPLAY %1 %2 %3 %4 %5 %6 %7 %8 %9
