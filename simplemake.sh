#!/bin/sh
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
#
# This is a script which compiles plasmasnow.
# Use and adapt this if the 
#   ./configure; make; make install
# suite does not work on your system
#

# Compilers:

# C compiler to compile .c sources:
CC=gcc

# C++ compiler to compile .cpp sources:
CXX=g++

# 
# You can also use the C++ compiler for all sources:
# CC=$CXX
#

# compile and link flags
FLAGS="-O2"

# if you have pkg-config working for gtk3:
# FLAGS="$FLAGS `pkg-config --cflags --libs gtk+-3.0`"
#
# if you have pkg-config working for gtk4:
FLAGS="$FLAGS `pkg-config --cflags --libs gtk4`"

### Qt Dev package
#
# Compiler Flags
QT_CDEFINES="-DQT_NO_DEBUG -DQT_CORE_LIB -DQT_GUI_LIB -DQT_WIDGETS_LIB"
QT_CFLAGS="-pipe -O2 -Wall -Wextra -D_REENTRANT -fPIC $(QT_CDEFINES)"
QT_CINCLUDES="-I/usr/include/x86_64-linux-gnu/qt5 -I/usr/include/x86_64-linux-gnu/qt5/QtCore -I/usr/include/x86_64-linux-gnu/qt5/QtGui -I/usr/include/x86_64-linux-gnu/qt5/QtWidgets"
FLAGS="$(QT_CFLAGS) $(QT_CINCLUDES) $(FLAGS)"

# Linker Flags
QT_LIBS="/usr/lib/x86_64-linux-gnu/libQt5Core.so /usr/lib/x86_64-linux-gnu/libQt5Gui.so /usr/lib/x86_64-linux-gnu/libQt5Widgets.so"
# FLAGS= ? ..............
#
### Qt Dev Package

# if you have pkg-config working for gmodule-2.0:
FLAGS="$FLAGS `pkg-config --cflags --libs gmodule-2.0`"
# NOTE: on my system, pkg-config expands to:
# -pthread -I/usr/include/glib-2.0 -I/usr/lib/x86_64-linux-gnu/glib-2.0/include -Wl,--export-dynamic -lgmodule-2.0 -pthread -lglib-2.0

# if you have pkg-config working for these: x11 xpm xt xproto
FLAGS="$FLAGS `pkg-config --cflags --libs x11 xpm xt xext xproto xinerama xtst xkbcommon`"
# NOTE: on my system, pkg-config expands to:
# -lXpm -lXt -lX11 -lXext -lXinerama -lXtst -lxkbcommon

FLAGS="$FLAGS `pkg-config --cflags --libs gsl`"
# NOTE: on my system, pkg-config expands to:
# -lgsl -lgslcblas -lm

# link flags for libmath:
FLAGS="$FLAGS -lm"

# following is needed by gtk3 to recognize the buttons:
# (Should be delivered by pkg-config --cflags --libs gmodule-2.0)
# FLAGS="$FLAGS -Wl,--export-dynamic"
# or:
# FLAGS="$FLAGS -rdynamic"

# comment out if your C++ compiler does not support unordered_map:
FLAGS="$FLAGS -DHAVE_UNORDERED_MAP"

# comment out if your C++ compiler does not support unordered_set:
FLAGS="$FLAGS -DHAVE_UNORDERED_SET"

version=`./getversion`
if [ "x$version" = x ]; then
   version="Unknown"
fi

FLAGS="$FLAGS -DVERSION=\"$version\""

FLAGS="$FLAGS -DLANGUAGES=\"\""

FLAGS="$FLAGS -I.."

echo "#define HAVE_GSL_INTERP_CSPLINE 1" > config.h

cd src || exit 1
echo "removing .o files:"
rm -f *.o

echo "creating changelog.inc:"
./tocc.sh < ../ChangeLog > changelog.inc

echo "creating tarfile.inc:"
echo "No tar file available" | ./toascii.sh > tarfile.inc || exit 1

echo "Creating snow_includes.h:"
./gen_snow_includes.sh .. || exit 1

echo "Creating ui_xml.h:"
./gen_ui_xml.sh .. || exit 1

echo "compiling C sources:"
$CC -c *.c $FLAGS || exit 1

echo "compiling C++ sources":
$CXX -c *.cpp $FLAGS || exit 1

echo "creating plasmasnow in directory $PWD:"
$CXX -o plasmasnow *.o $FLAGS || exit 1

echo "creating manpage in directory $PWD as plasmasnow.6:"
./plasmasnow -H > plasmasnow.6 || exit 1

echo
echo " ********************************************************************"
echo " ** It seems that you compiled plasmasnow successfully.                 **"
echo " ********************************************************************"
