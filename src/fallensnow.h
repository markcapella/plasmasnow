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

#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>

#include <X11/Intrinsic.h>
#include <X11/Xlib.h>

#include <gtk/gtk.h>

#include "plasmasnow.h"


// Semaphore helpers.
extern void initFallenSnowSemaphores();

// Swap semaphores.
int lockFallenSnowSwapSemaphore();
int unlockFallenSnowSwapSemaphore();

// Base semaphores.
extern int lockFallenSnowBaseSemaphore();
extern int softLockFallenSnowBaseSemaphore(
    int maxSoftTries, int* tryCount);
extern int unlockFallenSnowBaseSemaphore();

#ifndef __GNUC__
    #define lockFallenSnowSemaphore() \
        lockFallenSnowBaseSemaphore()
    #define unlockFallenSnowSemaphore() \
        unlockFallenSnowBaseSemaphore()
    #define softLockFallenSnowSemaphore(maxSoftTries,tryCount) \
        softLockFallenSnowBaseSemaphore(maxSoftTries,tryCount)
#else
    #define lockFallenSnowSemaphore() __extension__({ \
        int retval = lockFallenSnowBaseSemaphore(); \
        retval; })
    #define unlockFallenSnowSemaphore() __extension__({ \
        int retval = unlockFallenSnowBaseSemaphore(); \
        retval; })
    #define softLockFallenSnowSemaphore( \
        maxSoftTries,tryCount) __extension__({ \
        int retval = softLockFallenSnowBaseSemaphore( \
            maxSoftTries,tryCount); \
        retval; })
#endif

//
extern void initFallenSnowModule();
extern void initFallenSnowListWithDesktop();

void* execFallenSnowThread();
void updateAllFallenSnowOnThread();

extern void doFallenSnowUserSettingUpdates();

extern void setMaxScreenSnowDepth();
extern void setMaxScreenSnowDepthWithLock();

extern int canSnowCollectOnWindowOrScreenBottom(FallenSnow*);
extern int isFallenSnowOnVisibleWorkspace(FallenSnow*);

extern void updateFallenSnowWithWind(FallenSnow*, int w, int h);
extern void updateFallenSnowPartial(FallenSnow*, int x, int w);
extern void updateFallenSnowAtBottom();

extern void generateFallenSnowFlakes(FallenSnow*,
    int x, int w, float vy);

// FallenSnow Stack Helpers.
extern void pushFallenSnowItem(FallenSnow**, WinInfo*,
    int x, int y, int w, int h);
void popFallenSnowItem(FallenSnow**);

// Desh method helpers.
void CreateDesh(FallenSnow*);
int do_change_deshes();
int do_adjust_deshes();

//
void createFallenSnowDisplayArea(FallenSnow*);
extern void cairoDrawAllFallenSnowItems(cairo_t*);
void eraseFallenSnowAtPixel(FallenSnow*, int x);

extern void eraseFallenSnowOnDisplay(FallenSnow*, int x, int w);
extern void freeFallenSnowItemMemory(FallenSnow*);

extern FallenSnow* findFallenSnowItemByWindow(FallenSnow*, Window);
extern void drawFallenSnowItem(FallenSnow*);

void swapFallenSnowListItemSurfaces();

extern void eraseFallenSnowListItem(Window);
extern int removeFallenSnowListItem(FallenSnow**, Window);

void removeAllFallenSnowWindows();
void removeFallenSnowFromWindow(Window);

extern void updateFallenSnowRegionsWithLock(void);
extern void updateFallenSnowRegions(void);

// Debug support.
extern void logAllFallenSnowDisplayAreas(FallenSnow*);
