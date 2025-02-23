#                                               -*- Autoconf -*-

# Process this file with autoconf to produce a configure script.
#
# -copyright-
#-# 
#-# plasmasnow: Let it snow on your desktop
#-# Copyright (C) 1984,1988,1990,1993-1995,2000-2001 Rick Jansen
#-# 	      2019,2020,2021,2022,2023 Willem Vermin
#-#          2024 Mark Capella
#-# 
#-# This program is free software: you can redistribute it and/or modify
#-# it under the terms of the GNU General Public License as published by
#-# the Free Software Foundation, either version 3 of the License, or
#-# (at your option) any later version.
#-# 
#-# This program is distributed in the hope that it will be useful,
#-# but WITHOUT ANY WARRANTY; without even the implied warranty of
#-# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#-# GNU General Public License for more details.
#-# 
#-# You should have received a copy of the GNU General Public License
#-# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#-# 

AC_PREREQ([2.69])
AC_INIT([plasmasnow], [2025.02.22], [markcapella@twcny.rr.com])
AC_CONFIG_SRCDIR([src/PlasmaSnow.h])
AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

# When creating a package (e.g. debian)
# Get date from debian/changelog or something like that: (thanks to Bernhard M. Wiedemann)
# First: check if SOURCE_DATE_EPOCH is already defined
# if not: use the file 'bootdate' which should contain the output of 'date +%s'

s="$SOURCE_DATE_EPOCH"

if test "x$s" = x ; then
   AC_MSG_WARN([SOURCE_DATE_EPOCH is not defined])
   if test -f "$srcdir/bootdate" ; then
      AC_MSG_NOTICE([Try to read SOURCE_DATE_EPOCH from file '$srcdir/bootdate'])
      s=`head -n1 "$srcdir/bootdate"`
      xd=`date -u -d "@$s" 2>/dev/null || date -u -r "$s" 2>/dev/null || echo "Invalid date format"`
      AC_MSG_NOTICE([Read SOURCE_DATE_EPOCH=$s, i.e: $xd ])
   fi
else
   xd=`date -u -d "@$s" 2>/dev/null || date -u -r "$s" 2>/dev/null || echo "Invalid date format"`
   AC_MSG_NOTICE([Given SOURCE_DATE_EPOCH=$s, i.e: $xd ])
fi

# final checks on the validity of SOURCE_DATE_EPOCH
# - must be an positive integer
# - date must be within -20 years and +1 year from current date
# if these are not fulfilled, use `date +%s`

x=$(echo "$s" | tr -dc "[0-9]" )
good=no
now=`date "+%s"`

if test "x$x" != x ; then
   firstdate=$( expr "$now" - 631152000 )
   lastdate=$(  expr "$now" + 31536000  )
   if test "$x" -gt "$firstdate" -a "$x" -lt "$lastdate" ; then
      good=yes
   fi
fi

if test x"$good" = xyes ; then
   SOURCE_DATE_EPOCH="$x"
else
   SOURCE_DATE_EPOCH="$now"
   AC_MSG_WARN([SOURCE_DATE_EPOCH set from current date])
fi

xd=`date -u -d "@$SOURCE_DATE_EPOCH" 2>/dev/null || date -u -r "$SOURCE_DATE_EPOCH" 2>/dev/null || echo "Invalid date format"`
AC_MSG_NOTICE([Using SOURCE_DATE_EPOCH=$SOURCE_DATE_EPOCH, i.e: $xd ])

DATE_FMT="+%Y-%m-%d"
DATE=`date -u -d "@$SOURCE_DATE_EPOCH" "$DATE_FMT" 2>/dev/null || date -u -r "$SOURCE_DATE_EPOCH" "$DATE_FMT" 2>/dev/null || date -u "$DATE_FMT"`
AC_SUBST([DATE])

# gettext
AM_GNU_GETTEXT([external])
#AM_GNU_GETTEXT_VERSION([0.21])
AC_SUBST([LOCALEDIR],[$localedir])
AM_CONDITIONAL([USE_NLS],[test "x$USE_NLS" = "xyes"])

# Check if we can create a reproducible 'make dist'.
# If yes: replace the original am__tar (Make's recipe to make a tarball) by
#  a recipe that creates a reproducible tarball

tardir=conftestdir
mkdir "$tardir"
echo "foo" > $tardir/test

AC_MSG_NOTICE([Checking the capabilities of the tar program...])
"${TAR-tar}" --format=posix --pax-option=exthdr.name=%d/PaxHeaders/%f,delete=atime,delete=ctime \ 
	  --owner=0 --group=0 --numeric-owner --mtime="@${SOURCE_DATE_EPOCH}" --sort=name -cf - "$tardir" > /dev/null

if test $? -eq 0 ; then
   AC_MSG_NOTICE([The program tar is usable to create a reproducible tar ball.])
   am__tar='$${TAR-tar} --format=posix --pax-option=exthdr.name=%d/PaxHeaders/%f,delete=atime,delete=ctime \ 
	  --owner=0 --group=0 --numeric-owner --mtime="'"@${SOURCE_DATE_EPOCH}"'" --sort=name -chf - "$$tardir"'
else
   AC_MSG_WARN([The program tar is not usable to create a reproducible build.])
   AC_MSG_WARN([If a reproducible build is desired, use environment variable 'TAR' to point to a pax-enabled tar,])
   AC_MSG_WARN([or use the flag '--disable-selfrep'.])
fi

rm -r "$tardir"

AC_ARG_ENABLE(selfrep, [AS_HELP_STRING([--enable-selfrep],[Build with self replicating mode @<:@default=yes@:>@])],[],[enable_selfrep=yes])
AM_CONDITIONAL([MAKESELFREP],[test "x$enable_selfrep" = "xno"])

AC_SUBST([PACKAGE_VERSION])

# ar:
AM_PROG_AR

# Checks for programs.
AC_PROG_CC([cc gcc])
AC_PROG_CXX([c++ g++])
AC_PROG_INSTALL
AC_PROG_RANLIB

LIBS="-lm"
# Checks for header files.
AC_PATH_X
AC_CHECK_HEADERS([assert.h ctype.h math.h pthread.h signal.h],[],AC_MSG_ERROR([Exiting]))
AC_CHECK_HEADERS([stdarg.h stdio.h stdlib.h string.h unistd.h],[],AC_MSG_ERROR([Exiting]))
AC_CHECK_HEADERS([execinfo.h])

AC_LANG_PUSH([C++])
AC_CHECK_HEADERS([iostream],[],AC_MSG_ERROR([Check your C++ compiler. Exiting]))
AC_CHECK_HEADERS([unordered_map])
AC_CHECK_HEADERS([unordered_set])
AC_LANG_POP([C++])

PKG_CHECK_MODULES(GTK, [gtk+-3.0 gmodule-2.0])
PKG_CHECK_MODULES(QT, [Qt5Core])
PKG_CHECK_MODULES(X11, [x11 xft xpm xt xext xproto xinerama xtst xkbcommon])
PKG_CHECK_MODULES(GSL, [gsl])

m4_include([m4/ax_pthread.m4])
AX_PTHREAD()

AC_CHECK_FUNCS([backtrace])
LIBS="$X11_LIBS $GSL_LIBS"

# check for availability of double buffering
AC_CHECK_FUNCS([XdbeAllocateBackBufferName])
#check for availability of xinerama
AC_CHECK_FUNCS([XineramaQueryScreens])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_SIZE_T
AC_CHECK_HEADER_STDBOOL

# Checks for library functions.
# check for AC_FUNC_MALLOC has detrimental effects on some systems
#  symptom: undefined reference to rpl_malloc
#  see also: https://github.com/maxmind/libmaxminddb/pull/152
#AC_FUNC_MALLOC
#AC_FUNC_REALLOC
#AC_FUNC_ALLOCA

AC_CHECK_FUNCS([alarm gettimeofday sqrt strchr strdup strstr strtol])
AC_CHECK_FUNCS([gsl_interp_steffen gsl_interp_akima gsl_interp_cspline gsl_interp_linear])

AC_CONFIG_FILES([Makefile src/Makefile src/Pixmaps/Makefile data/Makefile po/Makefile.in afterburner/Makefile])
AC_OUTPUT
