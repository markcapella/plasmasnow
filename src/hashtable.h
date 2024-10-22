/* -copyright-
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
*/
#pragma once

#ifdef __cplusplus
extern "C" {
#endif
extern void table_insert(unsigned int key, void *value);
extern void *table_get(unsigned int key);
extern void table_clear(void (*destroy)(void *p));

extern void set_insert(void *key);
extern void set_erase(void *key);
extern int set_count(void *key);
extern void set_clear(void);
extern void set_begin(void);
extern void *set_next(void);
extern unsigned int set_size(void);
#ifdef __cplusplus
}
#endif
