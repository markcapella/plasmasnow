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

plasmasnow=plasmasnow
if [ -x ./plasmasnow ]; then
   plasmasnow=./plasmasnow
fi
# test if 'plasmasnow -h' more or less works:
$plasmasnow -h | grep -q -i plasmasnow 
if [ $? -ne 0 ] ; then
   echo "Error in executing: $plasmasnow -h"
   exit 1
fi
# test if all default values are substituted:
$plasmasnow -h | grep -q DEFAULT_
if [ $? -eq 0 ] ; then
   echo "Not all default values are substituted:"
   $plasmasnow -h | grep DEFAULT_
   exit 1
fi


