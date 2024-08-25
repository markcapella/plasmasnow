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

#include <stdbool.h>

#include <X11/Xlib.h>

#include "plasmasnow.h"

/***********************************************************
 * Module Method stubs.
 */
extern long int getCurrentWorkspace();
long int getWindowWorkspace(Window window);

extern WinInfo* findWinInfoByWindowId(Window id);

extern void logAllWinInfoStructs(Display *dpy, WinInfo *windows, int nwin);

extern void doRaiseWindow(char* argString);
extern void doLowerWindow(char* argString);

Window getWindowMatchName(char* name);
unsigned long getX11StackedWindowsList(Window** wins);

extern void logWindow(Window);
extern void logAllWindowsStackedTopToBottom();

unsigned long getRootWindowProperty(Atom prop, Window **wins);
extern void getX11WindowsList(WinInfo** winInfolist, int *listCount);

void getRawWindowsList(WinInfo** winInfolist, int *listCount);
void getFinishedWindowsList(WinInfo** winInfolist, int *listCount);

bool isWindow_Hidden(Window window, int windowMapState);
extern bool is_NET_WM_STATE_Hidden(Window window);
extern bool is_WM_STATE_Hidden(Window window);

bool isDesktop_Visible();

bool isWindow_Sticky(long workSpace, WinInfo*);
bool isWindow_Dock(WinInfo*);
