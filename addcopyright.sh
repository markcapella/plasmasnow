#!/bin/sh
# -copyright-
#-# 
#-# plasmasnow: Let it snow on your desktop
#-# Copyright (C) 1984,1988,1990,1993-1995,2000-2001 Rick Jansen
#-# 	      2019,2020,2021,2022,2023 Willem Vermin
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
crfile=`mktemp`
cat << eof | sed 's/^/#-# /' > "$crfile"

plasmasnow: Let it snow on your desktop
Copyright (C) 1984,1988,1990,1993-1995,2000-2001 Rick Jansen
	      2019,2020,2021,2022,2023 Willem Vermin

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.

eof
n=0
while [ "$1" ] ; do
   f="$1"
   shift
   txt="-""copyright-"
   if file --mime "$f" | grep -q binary ; then
      echo "$f: binary"
      continue
   fi
   if ! grep -q -- "$txt" "$f" ; then 
      echo "$f: no $txt"
      continue
   fi
   # following 2 sed commands take care that only the first '-copyright-'
   # will be followed by the copyright text
   # Notice that sed requires a newline after the filename of the 'r' command
   sed -i "/^\s*#-#/d" "$f"
   sed -i "/$txt/{r $crfile
:a;n;ba;}" "$f"
   n=`expr $n + 1`
done
echo "$n files copyrighted"
