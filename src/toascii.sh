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
cat << eof > toascii.c
#include <stdio.h>
int main()
{
   int c;
   int i = 0;
   while((c = getchar()) != EOF)
   {
      if (i>20)
      {
	 i = 1;
	 printf("\n");
      }
      printf("%d,",c);
      i++;
   }
}
eof
${CC:-cc} $CFLAGS -o toascii toascii.c >/dev/null 2>&1 
./toascii
rm -f toascii toascii.c
