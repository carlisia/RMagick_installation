#  Copyright (C) 2001-2006 Artifex Software, Inc.
#  All Rights Reserved.
#
#  This software is provided AS-IS with no warranty, either express or
#  implied.
#
#  This software is distributed under license and may not be copied, modified
#  or distributed except as expressly authorized under the terms of that
#  license.  Refer to licensing information at http://www.artifex.com/
#  or contact Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134,
#  San Rafael, CA  94903, U.S.A., +1(415)492-9861, for further information.
#
# $Id: Makefile.in 9752 2009-05-21 23:46:45Z marcos $
# makefile template for/from the autoconf build.
# Makefile.  Generated from Makefile.in by configure.

# ------------------------------- Options ------------------------------- #

####### The following are the only parts of the file you should need to edit.

# Define the directory for the final executable, and the
# source, generated intermediate file, and object directories
# for the graphics library (GL) and the PostScript/PDF interpreter (PS).

BINDIR=./bin
GLSRCDIR=./base
GLGENDIR=./obj
GLOBJDIR=./obj
PSSRCDIR=./psi
PSLIBDIR=./lib
PSRESDIR=./Resource
PSGENDIR=./obj
PSOBJDIR=./obj
CONTRIBDIR=./contrib

# Do not edit the next group of lines.

include $(GLSRCDIR)/version.mak
DD=$(GLGENDIR)/
GLD=$(GLGENDIR)/
PSD=$(PSGENDIR)/

# ------ Generic options ------ #

# Define the installation commands and target directories for
# executables and files.  The commands are only relevant to `make install';
# the directories also define the default search path for the
# initialization files (gs_*.ps) and the fonts.

INSTALL = $(GLSRCDIR)/instcopy -c
INSTALL_PROGRAM = $(INSTALL) -m 755
INSTALL_DATA = $(INSTALL) -m 644
INSTALL_SHARED = 

prefix = /usr/local
exec_prefix = ${prefix}
bindir = ${exec_prefix}/bin
scriptdir = $(bindir)
includedir = ${prefix}/include
libdir = ${exec_prefix}/lib
mandir = ${datarootdir}/man
man1ext = 1
man1dir = $(mandir)/man$(man1ext)
datadir = ${datarootdir}
datarootdir = ${prefix}/share

# The following must be substituted using ${datarootdir} and ${exec_prefix}/lib
# to avoid adding RPM generation paths (CUPS STR #1112)
gsdir = ${datarootdir}/ghostscript
gsdatadir = $(gsdir)/$(GS_DOT_VERSION)
gssharedir = ${exec_prefix}/lib/ghostscript/$(GS_DOT_VERSION)
gsincludedir = ${prefix}/include/ghostscript/

docdir=$(gsdatadir)/doc
exdir=$(gsdatadir)/examples
GS_DOCDIR=$(docdir)

# Define install location for 'cups' device/filter support
CUPSCFLAGS= 
CUPSLIBS= cupsimage cups z pthread resolv
CUPSLIBDIRS=
CUPSSERVERBIN=/usr/libexec/cups
CUPSSERVERROOT=/private/etc/cups
CUPSDATA=/usr/share/cups
CUPSPDFTORASTER=1

# Define the default directory/ies for the runtime
# initialization and font files.  Separate multiple directories with a :.

GS_LIB_DEFAULT=$(gsdatadir)/Resource/Init:$(gsdatadir)/lib:$(gsdatadir)/Resource/Font:$(gsdir)/fonts:${datarootdir}/fonts/default/ghostscript:${datarootdir}/fonts/default/Type1:${datarootdir}/fonts/default/TrueType:/usr/lib/DPS/outline/base:/usr/openwin/lib/X11/fonts/Type1:/usr/openwin/lib/X11/fonts/TrueType:/usr/share/cups/fonts

# Define the default directory for cached data files
# this must be a single path.

GS_CACHE_DIR="~/.ghostscript/cache/"

# Define whether or not searching for initialization files should always
# look in the current directory first.  This leads to well-known security
# and confusion problems, but users insist on it.
# NOTE: this also affects searching for files named on the command line:
# see the "File searching" section of Use.htm for full details.
# Because of this, setting SEARCH_HERE_FIRST to 0 is not recommended.

SEARCH_HERE_FIRST=1

# Define the name of the interpreter initialization file.
# (There is no reason to change this.)

GS_INIT=gs_init.ps

# Choose generic configuration options.

# -DDEBUG
#	includes debugging features (-Z switch) in the code.
#	  Code runs substantially slower even if no debugging switches
#	  are set.

GENOPT=

# Choose capability options.

# -DHAVE_MKSTEMP
#	uses mkstemp instead of mktemp
#		This uses the more secure temporary file creation call
#		Enable this if it is available on your platform.
# -DHAVE_HYPOT
#	use the system hypot() call
#
# -DHAVE_FILE64
#	use marked versions of the stdio FILE calls, fopen64() et al.
#
# -DHAVE_MKSTEMP64
#	use non-standard function mkstemp64()

CAPOPT= -DHAVE_MKSTEMP -DHAVE_HYPOT   

# Define the name of the executable file.

GS=gs

# Define the directories for debugging and profiling binaries, relative to
# the standard binaries.

DEBUGRELDIR=../debugobj
PGRELDIR=../pgobj

# Define the directory where the IJG JPEG library sources are stored,
# and the major version of the library that is stored there.
# You may need to change this if the IJG library version changes.
# See jpeg.mak for more information.

JSRCDIR=jpeg

# Note: if a shared library is used, it may not contain the
# D_MAX_BLOCKS_IN_MCU patch, and thus may not be able to read
# some older JPEG streams that violate the standard. If the JPEG
# library built from local sources, the patch will be applied.

SHARE_JPEG=0
JPEG_NAME=jpeg

# Define the directory where the PNG library sources are stored,
# and the version of the library that is stored there.
# You may need to change this if the libpng version changes.
# See libpng.mak for more information.

SHARE_LIBPNG=0
PNGSRCDIR=libpng
LIBPNG_NAME=png

# Define the directory where the zlib sources are stored.
# See zlib.mak for more information.

SHARE_ZLIB=0
ZSRCDIR=zlib
#ZLIB_NAME=gz
ZLIB_NAME=z

# Choose shared or compiled in libjbig2dec and source location
JBIG2_LIB=jbig2dec
SHARE_JBIG2=0
JBIG2SRCDIR=jbig2dec

# uncomment the following three lines and one of the last two to
# compile in the Luratech ldf_jb2 codec
#JBIG2_LIB=luratech
#SHARE_JBIG2=0
#JBIG2SRCDIR=ldf_jb2
#JBIG2_CFLAGS=-DUSE_LDF_JB2 -DLINUX
#JBIG2_CFLAGS=-DUSE_LDF_JB2 -DMAC -DMAC_OS_X_BUILD


# Choose the library to use for (JPXDecode support)
# whether to link to an external build or compile in from source
# and source location and configuration flags for compiling in
JPX_LIB=jasper
SHARE_JPX=0
JPXSRCDIR=jasper
JPX_CFLAGS=-DJAS_CONFIGURE

# uncomment the following three lines and one of the last two to
# compile in the Luratech lwf_jp2 codec
#JPX_LIB=luratech
#SHARE_JPX=0
#JPXSRCDIR=lwf_jp2
#JPX_CFLAGS=-DUSE_LWF_JP2 -DLINUX
#JPX_CFLAGS=-DUSE_LWF_JP2 -DMAC -DMAC_OS_X_BUILD

# Define the directory where the icclib source are stored.
# See icclib.mak for more information

ICCSRCDIR=icclib

# Define the directory where the ijs source is stored,
# and the process forking method to use for the server.
# See ijs.mak for more information.
 
IJSSRCDIR=ijs
IJSEXECTYPE=unix

# Define the directory where the imdi library source is stored.
# See devs.mak for more information.

IMDISRCDIR=imdi

# Define how to build the library archives.  (These are not used in any
# standard configuration.)

AR=ar
ARFLAGS=qc
RANLIB=ranlib

# ------ Platform-specific options ------ #

# Define the name of the C compiler.

CC=gcc

# Define the name of the linker for the final link step.
# Normally this is the same as the C compiler.

CCLD=$(CC)

# Define the default gcc flags.
GCFLAGS=  -Wall -Wstrict-prototypes -Wundef -Wmissing-declarations -Wmissing-prototypes -Wwrite-strings -Wno-strict-aliasing -Wdeclaration-after-statement -fno-builtin -fno-common -DHAVE_STDINT_H -DGX_COLOR_INDEX_TYPE="unsigned long int" 

# Define the added flags for standard, debugging, profiling 
# and shared object builds.

CFLAGS_STANDARD= -O2
CFLAGS_DEBUG=-g -O0
CFLAGS_PROFILE=-pg  -O2
CFLAGS_SO=

# Define the other compilation flags.  Add at most one of the following:
#	-DBSD4_2 for 4.2bsd systems.
#	-DSYSV for System V or DG/UX.
# 	-DSYSV -D__SVR3 for SCO ODT, ISC Unix 2.2 or before,
#	   or any System III Unix, or System V release 3-or-older Unix.
#	-DSVR4 -DSVR4_0 (not -DSYSV) for System V release 4.0.
#	-DSVR4 (not -DSYSV) for System V release 4.2 (or later) and Solaris 2.
# XCFLAGS can be set from the command line.
# We don't include -ansi, because this gets in the way of the platform-
#   specific stuff that <math.h> typically needs; nevertheless, we expect
#   gcc to accept ANSI-style function prototypes and function definitions.
XCFLAGS=

# fontconfig flags, used by unix-aux.mak
FONTCONFIG_CFLAGS=
FONTCONFIG_LIBS=

# defines from autoconf; note that we don't use these at present.
ACDEFS=-DPACKAGE_NAME=\"\" -DPACKAGE_TARNAME=\"\" -DPACKAGE_VERSION=\"\" -DPACKAGE_STRING=\"\" -DPACKAGE_BUGREPORT=\"\" -DHAVE_DIRENT_H=1 -DSTDC_HEADERS=1 -DHAVE_SYS_TYPES_H=1 -DHAVE_SYS_STAT_H=1 -DHAVE_STDLIB_H=1 -DHAVE_STRING_H=1 -DHAVE_MEMORY_H=1 -DHAVE_STRINGS_H=1 -DHAVE_INTTYPES_H=1 -DHAVE_STDINT_H=1 -DHAVE_UNISTD_H=1 -DHAVE_ERRNO_H=1 -DHAVE_FCNTL_H=1 -DHAVE_LIMITS_H=1 -DHAVE_MEMORY_H=1 -DHAVE_STDLIB_H=1 -DHAVE_STRING_H=1 -DHAVE_STRINGS_H=1 -DHAVE_SYS_IOCTL_H=1 -DHAVE_SYS_PARAM_H=1 -DHAVE_SYS_TIME_H=1 -DHAVE_SYSLOG_H=1 -DHAVE_UNISTD_H=1 -DHAVE_STRUCT_STAT_ST_BLOCKS=1 -DHAVE_ST_BLOCKS=1 -DTIME_WITH_SYS_TIME=1 -DSIZEOF_UNSIGNED_LONG_INT=8 -DHAVE_LIBM=1 -DUSE_LIBICONV_NATIVE=1 -DHAVE_LIBDL=1 -DHAVE_LIBX11=1 -DHAVE_LIBXEXT=1 -DHAVE_LIBXT=1 -DHAVE_MKSTEMP=1 -DHAVE_HYPOT=1 -DHAVE_FORK=1 -DHAVE_VFORK=1 -DHAVE_WORKING_VFORK=1 -DHAVE_WORKING_FORK=1 -DHAVE_STDLIB_H=1 -DHAVE_MALLOC=1 -DRETSIGTYPE=void -DHAVE_VPRINTF=1 -DHAVE_BZERO=1 -DHAVE_DUP2=1 -DHAVE_FLOOR=1 -DHAVE_GETTIMEOFDAY=1 -DHAVE_MEMCHR=1 -DHAVE_MEMMOVE=1 -DHAVE_MEMSET=1 -DHAVE_MKDIR=1 -DHAVE_MKFIFO=1 -DHAVE_MODF=1 -DHAVE_POW=1 -DHAVE_PUTENV=1 -DHAVE_RINT=1 -DHAVE_SETENV=1 -DHAVE_SQRT=1 -DHAVE_STRCHR=1 -DHAVE_STRERROR=1 -DHAVE_STRRCHR=1 -DHAVE_STRSPN=1 -DHAVE_STRSTR=1

CFLAGS=$(CFLAGS_STANDARD) $(GCFLAGS) $(XCFLAGS)

# Define platform flags for ld.
# SunOS 4.n may need -Bstatic.
# Solaris 2.6 (and possibly some other versions) with any of the SHARE_
# parameters set to 1 may need
#	-R /usr/local/xxx/lib:/usr/local/lib
# giving the full path names of the shared library directories.
# XLDFLAGS can be set from the command line.
XLDFLAGS=-L/usr/X11/lib

LDFLAGS= $(XLDFLAGS)

# Define any extra libraries to link into the executable.
# ISC Unix 2.2 wants -linet.
# SCO Unix needs -lsocket if you aren't including the X11 driver.
# SVR4 may need -lnsl.
# Solaris may need -lnsl -lsocket -lposix4.
# (Libraries required by individual drivers are handled automatically.)

EXTRALIBS=-ldl -lm  -liconv -lstdc++  

# Define the standard libraries to search at the end of linking.
# Most platforms require -lpthread for the POSIX threads library;
# on FreeBSD, change -lpthread to -lc_r; BSDI and perhaps some others
# include pthreads in libc and don't require any additional library.
# All reasonable platforms require -lm, but Rhapsody and perhaps one or
# two others fold libm into libc and don't require any additional library.

STDLIBS=-lpthread -lm

# Define the include switch(es) for the X11 header files.
# This can be null if handled in some other way (e.g., the files are
# in /usr/include, or the directory is supplied by an environment variable)

XINCLUDE=

# Define the directory/ies and library names for the X11 library files.
# XLIBDIRS is for ld and should include -L; XLIBDIR is for LD_RUN_PATH
# (dynamic libraries on SVR4) and should not include -L.
# Newer SVR4 systems can use -R in XLIBDIRS rather than setting XLIBDIR.
# Both can be null if these files are in the default linker search path.

# Solaris and other SVR4 systems with dynamic linking probably want
#XLIBDIRS=-L/usr/openwin/lib -R/usr/openwin/lib
# X11R6 (on any platform) may need
#XLIBS=Xt SM ICE Xext X11

# We use the autoconf macro AC_PATH_XTRA which defines X_LIBS with
# the -L (or whatever). It also defines X_PRE_LIBS and X_EXTRA_LIBS 
# all three of which are stripped and slotted into XLIBS below.
# Usually however, all but X_LIBS are empty on modern platforms.
XLIBDIRS=-L/usr/X11/lib
XLIBDIR=
#XLIBS=Xt Xext X11
XLIBS= Xt SM ICE Xext X11

# Define the .dev module that implements thread and synchronization
# primitives for this platform.

# If POSIX sync primitives are used, also change the STDLIBS to include
# the pthread library. Otherwise use SYNC=nosync
#SYNC=posync
#SYNC=nosync
SYNC=posync

# programs we use
RM=rm -f

# ------ Dynamic loader options ------- #
SOC_CFLAGS	=	
SOC_LIBS	=	
SOC_LOADER	=	dxmainc.c


CAIRO_CFLAGS	=	
CAIRO_LIBS	=	

# ------ Devices and features ------ #

# Choose the language feature(s) to include.  See gs.mak for details.

FEATURE_DEVS=$(PSD)psl3.dev $(PSD)pdf.dev $(PSD)dpsnext.dev $(PSD)ttfont.dev $(PSD)epsf.dev $(GLD)pipe.dev $(PSD)fapi.dev $(PSD)jbig2.dev $(PSD)jpx.dev
#FEATURE_DEVS=$(PSD)psl3.dev $(PSD)pdf.dev
#FEATURE_DEVS=$(PSD)psl3.dev $(PSD)pdf.dev $(PSD)dpsnext.dev $(PSD)ttfont.dev $(PSD)rasterop.dev $(GLD)pipe.dev
# The following is strictly for testing.
FEATURE_DEVS_ALL=$(PSD)psl3.dev $(PSD)pdf.dev $(PSD)dpsnext.dev $(PSD)ttfont.dev $(PSD)rasterop.dev $(PSD)double.dev $(PSD)trapping.dev $(PSD)stocht.dev $(GLD)pipe.dev $(PSD)jbig2.dev $(PSD)jpx.dev
#FEATURE_DEVS=$(FEATURE_DEVS_ALL)

# The list of resources to be included in the %rom% file system.
# This is in the top makefile since the file descriptors are platform specific
RESOURCE_LIST=Resource/CMap/ Resource/ColorSpace/ Resource/Decoding/ Resource/Font/ Resource/ProcSet/ Resource/IdiomSet/ Resource/CIDFont/

# Choose whether to compile the .ps initialization files into the executable.
# See gs.mak for details.

COMPILE_INITS=1

# Choose whether to store band lists on files or in memory.
# The choices are 'file' or 'memory'.

BAND_LIST_STORAGE=file

# Choose which compression method to use when storing band lists in memory.
# The choices are 'lzw' or 'zlib'.

BAND_LIST_COMPRESSOR=zlib

# Choose the implementation of file I/O: 'stdio', 'fd', or 'both'.
# See gs.mak and sfxfd.c for more details.

FILE_IMPLEMENTATION=stdio

# Choose the implementation of stdio: '' for file I/O and 'c' for callouts
# See gs.mak and ziodevs.c/ziodevsc.c for more details.

STDIO_IMPLEMENTATION=c

# List of default devices, in order of priority. They need not be 
# present in the actual build.
GS_DEV_DEFAULT="x11alpha x11 bbox"

# Fallback default device.  This is set to 'display' by 
# unix-dll.mak when building a shared object.
DISPLAY_DEV=$(DD)bbox.dev

# Choose the device(s) to include.  See devs.mak for details,
# devs.mak and contrib.mak for the list of available devices.

#DEVICE_DEVS=$(DISPLAY_DEV) $(DD)x11.dev $(DD)x11alpha.dev $(DD)x11cmyk.dev $(DD)x11gray2.dev $(DD)x11gray4.dev $(DD)x11mono.dev
#DEVICE_DEVS=$(DISPLAY_DEV) $(DD)x11.dev $(DD)x11alpha.dev $(DD)x11cmyk.dev $(DD)x11mono.dev $(DD)x11_.dev $(DD)x11alt_.dev $(DD)x11cmyk2.dev $(DD)x11cmyk4.dev $(DD)x11cmyk8.dev $(DD)x11rg16x.dev $(DD)x11rg32x.dev $(DD)x11gray2.dev $(DD)x11gray4.dev
#DEVICE_DEVS=$(DISPLAY_DEV) $(DD)x11.dev $(DD)x11alpha.dev $(DD)x11cmyk.dev $(DD)x11mono.dev $(DD)x11_.dev $(DD)x11alt_.dev $(DD)x11cmyk2.dev $(DD)x11cmyk4.dev $(DD)x11cmyk8.dev $(DD)x11rg16x.dev $(DD)x11rg32x.dev $(DD)x11gray2.dev $(DD)x11gray4.dev

#DEVICE_DEVS1=$(DD)bmpmono.dev $(DD)bmpgray.dev $(DD)bmpsep1.dev $(DD)bmpsep8.dev $(DD)bmp16.dev $(DD)bmp256.dev $(DD)bmp16m.dev $(DD)bmp32b.dev
#DEVICE_DEVS2=#$(DD)gdevjbig2.dev $(DD)gdevjpx.dev #$(DD)rinkj.dev
#DEVICE_DEVS3=$(DD)deskjet.dev $(DD)djet500.dev $(DD)laserjet.dev $(DD)ljetplus.dev $(DD)ljet2p.dev $(DD)ljet3.dev $(DD)ljet3d.dev $(DD)ljet4.dev $(DD)ljet4d.dev $(DD)lj5mono.dev $(DD)lj5gray.dev
#DEVICE_DEVS4=$(DD)cdeskjet.dev $(DD)cdjcolor.dev $(DD)cdjmono.dev $(DD)cdj550.dev $(DD)pj.dev $(DD)pjxl.dev $(DD)pjxl300.dev
#DEVICE_DEVS5=$(DD)uniprint.dev $(DD)ijs.dev
#DEVICE_DEVS6=$(DD)bj10e.dev $(DD)bj200.dev $(DD)bjc600.dev $(DD)bjc800.dev
#DEVICE_DEVS7=$(DD)faxg3.dev $(DD)faxg32d.dev $(DD)faxg4.dev $(DD)cfax.dev
#DEVICE_DEVS8=$(DD)pcxmono.dev $(DD)pcxgray.dev $(DD)pcx16.dev $(DD)pcx256.dev $(DD)pcx24b.dev $(DD)pcxcmyk.dev
#DEVICE_DEVS9=$(DD)pbm.dev $(DD)pbmraw.dev $(DD)pgm.dev $(DD)pgmraw.dev $(DD)pgnm.dev $(DD)pgnmraw.dev $(DD)pnm.dev $(DD)pnmraw.dev $(DD)ppm.dev $(DD)ppmraw.dev $(DD)pkm.dev $(DD)pkmraw.dev $(DD)pksm.dev $(DD)pksmraw.dev
#DEVICE_DEVS10=$(DD)tiffcrle.dev $(DD)tiffg3.dev $(DD)tiffg32d.dev $(DD)tiffg4.dev $(DD)tifflzw.dev $(DD)tiffpack.dev
#DEVICE_DEVS11=$(DD)tiff12nc.dev $(DD)tiff24nc.dev $(DD)tiffgray.dev $(DD)tiff32nc.dev $(DD)tiffsep.dev
#DEVICE_DEVS12=$(DD)psmono.dev $(DD)psgray.dev $(DD)psrgb.dev $(DD)bit.dev $(DD)bitrgb.dev $(DD)bitcmyk.dev
#DEVICE_DEVS13=$(DD)pngmono.dev $(DD)pnggray.dev $(DD)png16.dev $(DD)png256.dev $(DD)png16m.dev
#DEVICE_DEVS13=$(DD)png48.dev $(DD)png16m.dev $(DD)pnggray.dev $(DD)pngmono.dev $(DD)png256.dev $(DD)png16.dev $(DD)pngalpha.dev
#DEVICE_DEVS14=$(DD)jpeg.dev $(DD)jpeggray.dev
#DEVICE_DEVS15=$(DD)pdfwrite.dev $(DD)pswrite.dev $(DD)ps2write.dev $(DD)epswrite.dev $(DD)txtwrite.dev $(DD)pxlmono.dev $(DD)pxlcolor.dev
#DEVICE_DEVS16=$(DD)bbox.dev

#DEVICE_DEVS17=
#DEVICE_DEVS18=
#DEVICE_DEVS19=
#DEVICE_DEVS20=$(DD)cljet5.dev $(DD)cljet5c.dev
#DEVICE_DEVS21=$(DD)spotcmyk.dev $(DD)devicen.dev $(DD)xcf.dev $(DD)psdcmyk.dev $(DD)psdrgb.dev
DEVICE_DEVS=$(DISPLAY_DEV) $(DD)x11.dev $(DD)x11alpha.dev $(DD)x11cmyk.dev $(DD)x11mono.dev $(DD)x11_.dev $(DD)x11alt_.dev $(DD)x11cmyk2.dev $(DD)x11cmyk4.dev $(DD)x11cmyk8.dev $(DD)x11rg16x.dev $(DD)x11rg32x.dev $(DD)x11gray2.dev $(DD)x11gray4.dev
DEVICE_DEVS1=$(DD)bbox.dev $(DD)bit.dev $(DD)bitcmyk.dev $(DD)bitrgb.dev $(DD)bitrgbtags.dev $(DD)bmp16.dev $(DD)bmp16m.dev $(DD)bmp256.dev $(DD)bmp32b.dev $(DD)bmpgray.dev $(DD)bmpmono.dev $(DD)bmpsep1.dev $(DD)bmpsep8.dev $(DD)ccr.dev $(DD)cfax.dev $(DD)cgm24.dev $(DD)cgm8.dev $(DD)cgmmono.dev $(DD)cif.dev $(DD)devicen.dev $(DD)dfaxhigh.dev $(DD)dfaxlow.dev $(DD)epswrite.dev $(DD)fax.dev $(DD)faxg3.dev $(DD)faxg32d.dev $(DD)faxg4.dev $(DD)imdi.dev $(DD)inferno.dev $(DD)jpeg.dev $(DD)jpegcmyk.dev $(DD)jpeggray.dev $(DD)mag16.dev $(DD)mag256.dev $(DD)mgr4.dev $(DD)mgr8.dev $(DD)mgrgray2.dev $(DD)mgrgray4.dev $(DD)mgrgray8.dev $(DD)mgrmono.dev $(DD)miff24.dev $(DD)pam.dev $(DD)pbm.dev $(DD)pbmraw.dev $(DD)pcx16.dev $(DD)pcx24b.dev $(DD)pcx256.dev $(DD)pcx2up.dev $(DD)pcxcmyk.dev $(DD)pcxgray.dev $(DD)pcxmono.dev $(DD)pdfwrite.dev $(DD)pgm.dev $(DD)pgmraw.dev $(DD)pgnm.dev $(DD)pgnmraw.dev $(DD)pkm.dev $(DD)pkmraw.dev $(DD)pksm.dev $(DD)pksmraw.dev $(DD)plan9bm.dev $(DD)png16.dev $(DD)png16m.dev $(DD)png256.dev $(DD)pngalpha.dev $(DD)pnggray.dev $(DD)pngmono.dev $(DD)pnm.dev $(DD)pnmraw.dev $(DD)ppm.dev $(DD)ppmraw.dev $(DD)ps2write.dev $(DD)psdcmyk.dev $(DD)psdf.dev $(DD)psdrgb.dev $(DD)psgray.dev $(DD)psmono.dev $(DD)psrgb.dev $(DD)pswrite.dev $(DD)sgirgb.dev $(DD)simdi.dev $(DD)spotcmyk.dev $(DD)sunhmono.dev $(DD)tfax.dev $(DD)tiff12nc.dev $(DD)tiff24nc.dev $(DD)tiff32nc.dev $(DD)tiffcrle.dev $(DD)tiffg3.dev $(DD)tiffg32d.dev $(DD)tiffg4.dev $(DD)tiffgray.dev $(DD)tifflzw.dev $(DD)tiffpack.dev $(DD)tiffs.dev $(DD)tiffsep.dev $(DD)wtscmyk.dev $(DD)wtsimdi.dev $(DD)xcf.dev 
DEVICE_DEVS2=$(DD)alc1900.dev $(DD)alc2000.dev $(DD)alc4000.dev $(DD)alc4100.dev $(DD)alc8500.dev $(DD)alc8600.dev $(DD)alc9100.dev $(DD)ap3250.dev $(DD)appledmp.dev $(DD)atx23.dev $(DD)atx24.dev $(DD)atx38.dev $(DD)bj10e.dev $(DD)bj10v.dev $(DD)bj10vh.dev $(DD)bj200.dev $(DD)bjc600.dev $(DD)bjc800.dev $(DD)bjc880j.dev $(DD)bjccmyk.dev $(DD)bjccolor.dev $(DD)bjcgray.dev $(DD)bjcmono.dev $(DD)cdeskjet.dev $(DD)cdj1600.dev $(DD)cdj500.dev $(DD)cdj550.dev $(DD)cdj670.dev $(DD)cdj850.dev $(DD)cdj880.dev $(DD)cdj890.dev $(DD)cdj970.dev $(DD)cdjcolor.dev $(DD)cdjmono.dev $(DD)cdnj500.dev $(DD)chp2200.dev $(DD)cljet5.dev $(DD)cljet5c.dev $(DD)cljet5pr.dev $(DD)coslw2p.dev $(DD)coslwxl.dev $(DD)cp50.dev $(DD)declj250.dev $(DD)deskjet.dev $(DD)dj505j.dev $(DD)djet500.dev $(DD)djet500c.dev $(DD)dl2100.dev $(DD)dnj650c.dev $(DD)epl2050.dev $(DD)epl2050p.dev $(DD)epl2120.dev $(DD)epl2500.dev $(DD)epl2750.dev $(DD)epl5800.dev $(DD)epl5900.dev $(DD)epl6100.dev $(DD)epl6200.dev $(DD)eplcolor.dev $(DD)eplmono.dev $(DD)eps9high.dev $(DD)eps9mid.dev $(DD)epson.dev $(DD)epsonc.dev $(DD)escp.dev $(DD)escpage.dev $(DD)fmlbp.dev $(DD)fmpr.dev $(DD)fs600.dev $(DD)gdi.dev $(DD)hl1240.dev $(DD)hl1250.dev $(DD)hl7x0.dev $(DD)hpdj1120c.dev $(DD)hpdj310.dev $(DD)hpdj320.dev $(DD)hpdj340.dev $(DD)hpdj400.dev $(DD)hpdj500.dev $(DD)hpdj500c.dev $(DD)hpdj510.dev $(DD)hpdj520.dev $(DD)hpdj540.dev $(DD)hpdj550c.dev $(DD)hpdj560c.dev $(DD)hpdj600.dev $(DD)hpdj660c.dev $(DD)hpdj670c.dev $(DD)hpdj680c.dev $(DD)hpdj690c.dev $(DD)hpdj850c.dev $(DD)hpdj855c.dev $(DD)hpdj870c.dev $(DD)hpdj890c.dev $(DD)hpdjplus.dev $(DD)hpdjportable.dev $(DD)ibmpro.dev $(DD)imagen.dev $(DD)iwhi.dev $(DD)iwlo.dev $(DD)iwlq.dev $(DD)jetp3852.dev $(DD)jj100.dev $(DD)la50.dev $(DD)la70.dev $(DD)la75.dev $(DD)la75plus.dev $(DD)laserjet.dev $(DD)lbp310.dev $(DD)lbp320.dev $(DD)lbp8.dev $(DD)lex2050.dev $(DD)lex3200.dev $(DD)lex5700.dev $(DD)lex7000.dev $(DD)lips2p.dev $(DD)lips3.dev $(DD)lips4.dev $(DD)lips4v.dev $(DD)lj250.dev $(DD)lj3100sw.dev $(DD)lj4dith.dev $(DD)lj4dithp.dev $(DD)lj5gray.dev $(DD)lj5mono.dev $(DD)ljet2p.dev $(DD)ljet3.dev $(DD)ljet3d.dev $(DD)ljet4.dev $(DD)ljet4d.dev $(DD)ljet4pjl.dev $(DD)ljetplus.dev $(DD)ln03.dev $(DD)lp1800.dev $(DD)lp1900.dev $(DD)lp2000.dev $(DD)lp2200.dev $(DD)lp2400.dev $(DD)lp2500.dev $(DD)lp2563.dev $(DD)lp3000c.dev $(DD)lp7500.dev $(DD)lp7700.dev $(DD)lp7900.dev $(DD)lp8000.dev $(DD)lp8000c.dev $(DD)lp8100.dev $(DD)lp8200c.dev $(DD)lp8300c.dev $(DD)lp8300f.dev $(DD)lp8400f.dev $(DD)lp8500c.dev $(DD)lp8600.dev $(DD)lp8600f.dev $(DD)lp8700.dev $(DD)lp8800c.dev $(DD)lp8900.dev $(DD)lp9000b.dev $(DD)lp9000c.dev $(DD)lp9100.dev $(DD)lp9200b.dev $(DD)lp9200c.dev $(DD)lp9300.dev $(DD)lp9400.dev $(DD)lp9500c.dev $(DD)lp9600.dev $(DD)lp9600s.dev $(DD)lp9800c.dev $(DD)lps4500.dev $(DD)lps6500.dev $(DD)lq850.dev $(DD)lx5000.dev $(DD)lxm3200.dev $(DD)lxm5700m.dev $(DD)m8510.dev $(DD)md1xMono.dev $(DD)md2k.dev $(DD)md50Eco.dev $(DD)md50Mono.dev $(DD)md5k.dev $(DD)mj500c.dev $(DD)mj6000c.dev $(DD)mj700v2c.dev $(DD)mj8000c.dev $(DD)ml600.dev $(DD)necp6.dev $(DD)npdl.dev $(DD)oce9050.dev $(DD)oki182.dev $(DD)oki4w.dev $(DD)okiibm.dev $(DD)oprp.dev $(DD)opvp.dev $(DD)paintjet.dev $(DD)pcl3.dev $(DD)photoex.dev $(DD)picty180.dev $(DD)pj.dev $(DD)pjetxl.dev $(DD)pjxl.dev $(DD)pjxl300.dev $(DD)pr1000.dev $(DD)pr1000_4.dev $(DD)pr150.dev $(DD)pr201.dev $(DD)pxlcolor.dev $(DD)pxlmono.dev $(DD)r4081.dev $(DD)rinkj.dev $(DD)rpdl.dev $(DD)samsunggdi.dev $(DD)sj48.dev $(DD)st800.dev $(DD)stcolor.dev $(DD)t4693d2.dev $(DD)t4693d4.dev $(DD)t4693d8.dev $(DD)tek4696.dev $(DD)uniprint.dev $(DD)xes.dev 
DEVICE_DEVS3=$(DD)cups.dev
DEVICE_DEVS4=$(DD)ijs.dev
DEVICE_DEVS5=$(DD)omni.dev
DEVICE_DEVS6=$(DD)png48.dev $(DD)png16m.dev $(DD)pnggray.dev $(DD)pngmono.dev $(DD)png256.dev $(DD)png16.dev $(DD)pngalpha.dev
DEVICE_DEVS7=$(PSD)jbig2.dev
DEVICE_DEVS8=$(DD)svgwrite.dev
DEVICE_DEVS9=
DEVICE_DEVS10=
DEVICE_DEVS11=
DEVICE_DEVS12=
DEVICE_DEVS13=
DEVICE_DEVS14=
DEVICE_DEVS15=
DEVICE_DEVS16=
DEVICE_DEVS17=
DEVICE_DEVS18=
DEVICE_DEVS19=
DEVICE_DEVS20=
DEVICE_DEVS21=


# Shared library target to build.
GS_SHARED_OBJS=

# ---------------------------- End of options --------------------------- #

# Define the name of the partial makefile that specifies options --
# used in dependencies.

MAKEFILE=Makefile
TOP_MAKEFILES=$(MAKEFILE) $(GLSRCDIR)/unixhead.mak

# Define the auxiliary program dependency. We don't use this.

AK=

# Define the compilation rules and flags.

CCFLAGS=$(GENOPT) $(CAPOPT) $(CFLAGS)
CC_=$(CC) $(CCFLAGS)
CCAUX=$(CC) $(GCFLAGS)
CC_LEAF=$(CC_)
# note gcc can't use -fomit-frame-pointer with -pg.
CC_LEAF_PG=$(CC_)
# These are the specific warnings we have to turn off to compile those
# specific few files that need this.  We may turn off others in the future.
CC_NO_WARN=$(CC_)
CC_SHARED=$(CC_) 

# ---------------- End of platform-specific section ---------------- #

INSTALL_CONTRIB=install-contrib-extras
include $(GLSRCDIR)/unixhead.mak
include $(GLSRCDIR)/gs.mak
# psromfs.mak must precede lib.mak
include $(PSSRCDIR)/psromfs.mak
include $(GLSRCDIR)/lib.mak
include $(PSSRCDIR)/int.mak
include $(GLSRCDIR)/jpeg.mak
# zlib.mak must precede libpng.mak
include $(GLSRCDIR)/zlib.mak
include $(GLSRCDIR)/libpng.mak
include $(GLSRCDIR)/jbig2.mak
include $(GLSRCDIR)/jasper.mak
include $(GLSRCDIR)/ldf_jb2.mak
include $(GLSRCDIR)/lwf_jp2.mak
include $(GLSRCDIR)/icclib.mak
include $(GLSRCDIR)/ijs.mak
include $(GLSRCDIR)/devs.mak
include $(GLSRCDIR)/contrib.mak
include $(GLSRCDIR)/unix-aux.mak
include $(GLSRCDIR)/unixlink.mak
include $(GLSRCDIR)/unix-dll.mak
include $(GLSRCDIR)/unix-end.mak
include $(GLSRCDIR)/unixinst.mak
include contrib/contrib.mak
include cups/cups.mak

# Clean up after the autotools scripts
# configure.ac and Makefile.in are symlinks from a subdir 
# (created by autogen.sh) and so are safe to remove
distclean : clean config-clean soclean debugclean
	-$(RM_) -r $(BINDIR) $(GLOBJDIR) $(PSOBJDIR)
	-$(RM_) -r autom4te.cache
	-$(RM_) config.log config.status
	-$(RM_) Makefile

maintainer-clean : distclean
	-$(RM_) configure.ac
	-$(RM_) configure
	-$(RM_) Makefile.in

check : default
	$(NO_OP)

