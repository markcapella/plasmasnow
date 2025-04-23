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

#include "WinInfo.h"


/***********************************************************
 * Global Fallensnow helper objects.
 */
typedef struct _FallenSnow {
        WinInfo winInfo;          // winInfo None == bottom.
        struct _FallenSnow* next; // pointer to next item.

        short int x, y;           // X, Y array.
        short int w, h;           // W, H array.

        int prevx, prevy;         // x, y of last draw.
        int prevw, prevh;         // w, h of last draw.

        int snowColor;

        cairo_surface_t* renderedSurfaceA;
        cairo_surface_t* renderedSurfaceB;

        unsigned tallestColumnIndex;
        unsigned tallestColumnHeight;
        unsigned tallestColumnMax;

        short int* columnHeightList;    // actual heights.
        short int* columnMaxHeightList; // desired heights.

} FallenSnow;


#ifdef __cplusplus
extern "C" {
#endif

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

    // Dripping rain interactions.
    bool canFallenSnowDripRain(FallenSnow*);
    void dripRainFromFallen(FallenSnow*);
    bool doesFallenDripFromLeft(FallenSnow*);

    // Santa plow interations.
    void blowoffPlowedSnowFromFallen(FallenSnow* fsnow);
    void createPlowedStormItems(FallenSnow* fsnow, int xPos,
        int yPos1, int yPos2);

    // Main "draw frame" routine for fallen snow.
    void drawFallenSnowFrame(cairo_t*);

    // Fallensnow general helpers.
    FallenSnow* getFallenForWindow(Window);
    void eraseFallenSnow(FallenSnow*);
    void eraseFallenSnowPartial(FallenSnow*, int x, int w);
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

#ifdef __cplusplus
}
#endif