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
 *
 */
#pragma once

#include "plasmasnow.h"
#include <X11/Intrinsic.h>
#include <X11/Xlib.h>
#include <gtk/gtk.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>


// Semaphore helpers.
extern void initFallenSnowSemaphores();

int lockFallenSnowSwapSemaphore();
int unlockFallenSnowSwapSemaphore();

extern int lockFallenSnowBaseSemaphore();
extern int unlockFallenSnowBaseSemaphore();
extern int softLockFallenSnowBaseSemaphore(
    int maxSoftTries, int* tryCount);

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


void *doExecFallenSnowThread();

extern void initFallenSnowModule();
extern void doFallenSnowUISettingsUpdates();
extern void initFallenSnowList();

extern void setMaxScreenSnowDepth();
extern void setMaxScreenSnowDepthWithLock();
extern int isFallenSnowOnVisibleWorkspace(FallenSnow *fsnow);

extern void eraseFallenSnowOnWindow(Window id);
extern void eraseFallenSnowOnDisplay(FallenSnow *fsnow,
    int x, int w);
void eraseFallenSnowAtPixel(FallenSnow *fsnow, int x);

extern FallenSnow *findFallenSnowListItem(
    FallenSnow *first, Window id);

extern void fallensnow_draw(cairo_t *cr);
extern void drawFallenSnowListItem(FallenSnow *fsnow);
void swapFallenSnowListItemSurfaces();

extern int canSnowCollectOnWindowOrScreenBottom(
    FallenSnow *fsnow);

extern void updateFallenSnowWithWind(FallenSnow *fsnow,
    int w, int h);
extern void updateFallenSnowAtBottom();
extern void updateFallenSnowPartial(FallenSnow *fsnow,
    int x, int w);

extern void generateFallenSnowFlakes(FallenSnow *fsnow,
    int x, int w, float vy);

int PopFallenSnow(FallenSnow **list);
extern void PushFallenSnow(FallenSnow **first, WinInfo *win,
    int x, int y, int w, int h);

void CreateDesh(FallenSnow *p);
int do_change_deshes(void *dummy);
int do_adjust_deshes(void *dummy);

void createFallenSnowDisplayArea(FallenSnow *fsnow);
extern void freeFallenSnowDisplayArea(FallenSnow *fallen);
extern int removeFallenSnowListItem(FallenSnow **list, Window id);
extern void logAllFallenSnowDisplayAreas(FallenSnow *list);
