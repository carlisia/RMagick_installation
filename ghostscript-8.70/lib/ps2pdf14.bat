@echo off
@rem $Id: ps2pdf14.bat 6300 2005-12-28 19:56:24Z giles $

rem Convert PostScript to PDF 1.4 (Acrobat 5-and-later compatible).

echo -dCompatibilityLevel#1.4 >_.at
goto bot

rem Pass arguments through a file to avoid overflowing the command line.
:top
echo %1 >>_.at
shift
:bot
if not %3/==/ goto top
call ps2pdfxx %1 %2
