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

#include "PlasmaSnow.h"


// Fallensnow lifecycle helpers.
void initFallenSnowModule();

// FallenSnow Max ColumnHeight Thread.
int updateFallenSnowMaxColumnHeightThread();
void setColumnMaxHeightListForFallen(FallenSnow*);

// FallenSnow columnHeight Thread.
int updateFallenSnowColumnHeightThread();

// Fallensnow main Thread.
void* startFallenSnowBackgroundThread();
int execFallenSnowBackgroundThread();
int canSnowCollectOnFallen(FallenSnow*);
bool isFallenSnowVisible(FallenSnow*);
bool isFallenSnowVisibleOnWorkspace(FallenSnow*);
void removePlowedSnowFromFallen(FallenSnow*);
void renderFallenSnow(FallenSnow*);
void swapFallenSnowRenderedSurfaces();

// User settings helpers.
void respondToFallenSnowSettingsChanges();
void updateFallenSnowDesktopItemDepth();
void updateFallenSnowDesktopItemHeight();

// Snow interactions.
void updateFallenSnowWithSnow(FallenSnow*, int x, int w);

// Wind interactions.
void blowoffSnowFromFallen(FallenSnow*, int w, int h);
void eraseFallenSnowWindPixel(FallenSnow*, int x);

// Santa plow interations.
void blowoffPlowedSnowFromFallen(FallenSnow* fsnow);
void createPlowedStormItems(FallenSnow* fsnow, int xPos,
    int yPos1, int yPos2);

// Main "draw frame" routine for fallen snow.
void drawFallenSnowFrame(cairo_t*);

// Fallensnow egeneral helpers.
FallenSnow* findFallenSnowItemByWindow(Window);
void eraseFallenSnow(FallenSnow*);
void eraseFallenSnowPartial(FallenSnow*, int x, int w);
int getMaximumFallenSnowColumnHeight(FallenSnow*);
void removeFallenSnowFromAllWindows();
void removeFallenSnowFromWindow(Window);
int removeAndFreeFallenSnowForWindow(FallenSnow**,
    Window);
void generateFallenSnowFlakes(FallenSnow* fsnow,
    int xPos, int xWidth, float vy, bool limitToMax);

// WinInfo change watchers.
void doAllFallenSnowWinInfoUpdates();
void doWinInfoWSHides();
void doWinInfoInitialAdds();
bool windowIsTransparent(Window chromeWindow);
void doWinInfoRemoves();
void doWinInfoProgrammaticRemoves();

// FallenSnow Linked list Helpers.
int getFallenSnowItemcount();
void clearAllFallenSnowItems();
void pushFallenSnowItem(FallenSnow**, WinInfo*,
    int x, int y, int w, int h);
void popAndFreeFallenSnowItem(FallenSnow**);
void freeFallenSnowItem(FallenSnow*);

// Lock support.
void initFallenSnowSemaphores();
int softLockFallenSnowBaseSemaphore(int softTrys, int* tryCount);
int lockFallenSnowBaseSemaphore();
int unlockFallenSnowSwapSemaphore();
int lockFallenSnowSwapSemaphore();
int unlockFallenSnowBaseSemaphore();

// Other module helpers.
void logAllFallenSnowItems();
void logFallenSnowItem(FallenSnow*);
