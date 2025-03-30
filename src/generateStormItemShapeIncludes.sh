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
root="${1:-..}"
out="StormItemShapeIncludes.h"

echo "#pragma once" > "$out"
echo "/* -""copyright-" >> "$out"
echo "*/" >> "$out"

ls "$root/src/Pixmaps"/stormItem*.xpm | sed "s/^/#include \"/;s/$/\"/" >> "$out"

echo "#define ALL_STORMITEM_SHAPEFILE_NAMES \\" >> "$out"
for i in $(seq `ls "$root/src/Pixmaps"/stormItem*.xpm | wc -l`) ; do 
   printf 'STORMITEM_SHAPEFILE_NAME(%d) \\\n' `expr $i - 1` ;
done >> "$out"

echo >> "$out"

if [ -x "$root/addcopyright.sh" ] ; then \
   "$root/addcopyright.sh" "$out"  ; fi
