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


int _xdo_is_window_visible(const xdo_t* xdo, Window wid);

int xdo_search_windows(const xdo_t* xdo, const xdo_search_t *search,
    Window **windowlist_ret, unsigned int *nwindows_ret);

void find_matching_windows(const xdo_t* xdo, Window window,
    const xdo_search_t *search, Window **windowlist_ret,
    unsigned int *nwindows_ret, unsigned int *windowlist_size,
    int current_depth);

int check_window_match(const xdo_t* xdo, Window wid,
    const xdo_search_t *search);

int _xdo_match_window_title(const xdo_t* xdo, Window window, regex_t *re);
int _xdo_match_window_name(const xdo_t* xdo, Window window, regex_t *re);
int _xdo_match_window_class(const xdo_t* xdo, Window window, regex_t *re);
int _xdo_match_window_classname(const xdo_t* xdo, Window window, regex_t *re);
int _xdo_match_window_pid(const xdo_t* xdo, Window window, int pid);

 int compile_re(const char* inPattern, regex_t* outRegex);
