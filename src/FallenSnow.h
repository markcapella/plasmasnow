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
#include <stdbool.h>
#include <stdlib.h>

#include <X11/Intrinsic.h>
#include <X11/Xlib.h>

#include <gtk/gtk.h>

#include "plasmasnow.h"


// Fallensnow lifecycle helpers.
void initFallenSnowModule();

void* startFallenSnowBackgroundThread();
int execFallenSnowBackgroundThread();
void swapFallenSnowRenderedSurfacesBToA();

// User settings hleprs.
void updateFallenSnowUserSettings();
void updateFallenSnowDesktopItemDepth();
void updateFallenSnowDesktopItemHeight();

// Snow interactions.
void updateFallenSnowWithSnow(FallenSnow*, int x, int w);
int canSnowCollectOnFallen(FallenSnow*);
int isFallenSnowVisibleOnWorkspace(FallenSnow*);
void collectSnowOnFallen(FallenSnow*);
void renderFallenSnowSurfaceB(FallenSnow*);

// Santa interactions.
void updateFallenSnowWithSanta(FallenSnow*);

// Wind interactions.
void updateFallenSnowWithWind(FallenSnow*, int w, int h);
void eraseFallenSnowWindPixel(FallenSnow*, int x);

// FallenSnow Linked list Helpers.
int getFallenSnowItemcount();
void clearAllFallenSnowItems();
void logAllFallenSnowItems();

void pushFallenSnowItem(FallenSnow**, WinInfo*,
    int x, int y, int w, int h);
void popAndFreeFallenSnowItem(FallenSnow**);
void freeFallenSnowItem(FallenSnow*);

// Desh method helpers.
void CreateDesh(FallenSnow*);
int do_change_deshes();
int do_adjust_deshes();

// Fallensnow by Window helpers.
FallenSnow* findFallenSnowItemByWindow(Window);
void eraseFallenSnowPartial(FallenSnow*, int x, int w);
void removeFallenSnowFromAllWindows();
void removeFallenSnowFromWindow(Window);
int removeAndFreeFallenSnowForWindow(FallenSnow**,
    Window);

// WinInfo change watchers.
void doAllFallenSnowWinInfoUpdates();
void doWinInfoWSHides();
void doWinInfoInitialAdds();
void doWinInfoRemoves();
void doWinInfoProgrammaticRemoves();

// Helper for Blown, Dropped, and Plowed Snow.
void generateFallenSnowFlakes(FallenSnow* fsnow,
    int xPos, int xWidth, float vy, bool limitToMax);

// Main "draw frame" routine for fallen snow.
void drawFallenSnowFrame(cairo_t*);

// Semaphore helpers.
void initFallenSnowSemaphores();
int lockFallenSnowBaseSemaphore();
int softLockFallenSnowBaseSemaphore(
    int maxSoftTries, int* tryCount);
int unlockFallenSnowBaseSemaphore();

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

int unlockFallenSnowSwapSemaphore();
int lockFallenSnowSwapSemaphore();
