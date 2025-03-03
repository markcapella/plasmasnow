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

#include <ctype.h>
#include <math.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include <X11/Intrinsic.h>
#include <X11/Xlib.h>

#include <gsl/gsl_errno.h>
#include <gsl/gsl_sort.h>
#include <gsl/gsl_spline.h>

#include <gtk/gtk.h>

#include "Blowoff.h"
#include "ColorCodes.h"
#include "FallenSnow.h"
#include "Flags.h"
#include "safe_malloc.h"
#include "Santa.h"
#include "spline_interpol.h"
#include "Storm.h"
#include "Utils.h"
#include "WindowVector.h"
#include "wind.h"
#include "Windows.h"
#include "WinInfo.h"


/** *********************************************************************
 ** Module globals and consts.
 **/
const int MINIMUM_SPLINE_WIDTH = 3;

// Must-stay-snowfree area in the display.
const int MAX_DESKTOP_SNOWFREE_HEIGHT = 25;

// Semaphore & lock members.
sem_t mFallenSnowSwapSemaphore;
sem_t mFallenSnowBaseSemaphore;

int mFallenSnowMaxColumnHeightThreadLockCounter;

pthread_t mFallenSnowBackgroundThread;

WindowVector mDeferredWindowRemovesList;


/** *********************************************************************
 ** This method initializes the FallenSnow module.
 **/
void initFallenSnowModule() {
    clearAllFallenSnowItems();

    addMethodToMainloop(PRIORITY_DEFAULT,
        time_change_bottom, updateFallenSnowMaxColumnHeightThread);

    addMethodToMainloop(PRIORITY_DEFAULT,
        time_adjust_bottom, updateFallenSnowColumnHeightThread);

    // Start main background thread looper & exit.
    pthread_create(&mFallenSnowBackgroundThread, NULL,
        startFallenSnowBackgroundThread, NULL);
}

/** *********************************************************************
 ** This method sets ColumnMaxHeightList for all FallenSnow items.
 **/
int updateFallenSnowMaxColumnHeightThread() {
    if (softLockFallenSnowBaseSemaphore(3,
        &mFallenSnowMaxColumnHeightThreadLockCounter)) {
        return true;
    }

    FallenSnow* fsnow = mGlobal.FsnowFirst;
    while (fsnow) {
        setColumnMaxHeightListForFallen(fsnow);
        fsnow = fsnow->next;
    }

    unlockFallenSnowBaseSemaphore();
    return true;
}

/** *********************************************************************
 ** This method sets ColumnMaxHeightList for a FallenSnow item.
 ** threads: locking by caller
 **/
void setColumnMaxHeightListForFallen(FallenSnow* fallen) {
    const int W = fallen->w;
    const int H = fallen->h;
    const int WINDOW = fallen->winInfo.window;

    short int* columnMaxHeightList = fallen->columnMaxHeightList;

    #define N 6
    double splinex[N];
    double spliney[N];
    randomuniqarray(splinex, N, 0.0000001, NULL);

    for (int i = 0; i < N; i++) {
        splinex[i] *= (W - 1);
        spliney[i] = drand48();
    }

    splinex[0] = 0;
    splinex[N - 1] = W - 1;

    if (WINDOW == None) {
        spliney[0] = 1.0;
        spliney[N - 1] = 1.0;
    } else {
        spliney[0] = 0;
        spliney[N - 1] = 0;
    }

    double* xIndexArray = (double*) malloc(W * sizeof(double));
    for (int i = 0; i < W; i++) {
        xIndexArray[i] = i;
    }

    double* yResultArray = (double*) malloc(W * sizeof(double));
    spline_interpol(splinex, N, spliney, xIndexArray, W, yResultArray);
    free(xIndexArray);

    for (int i = 0; i < W; i++) {
        columnMaxHeightList[i] = H * yResultArray[i];
        if (columnMaxHeightList[i] < 2) {
            columnMaxHeightList[i] = 2;
        }
    }

    free(yResultArray);
}

/** *********************************************************************
 ** This method changes a fallen snow items ColumnHeightList height down.
 **/
int updateFallenSnowColumnHeightThread(__attribute__((unused))
    void* dummy) {
    lockFallenSnowBaseSemaphore();

    FallenSnow* fsnow = mGlobal.FsnowFirst;
    while (fsnow) {
        for (int i = 0; i < fsnow->w; i++) {
            if (fsnow->columnHeightList[i] >
                fsnow->columnMaxHeightList[i]) {
                fsnow->columnHeightList[i]--;
            }
        }
        fsnow = fsnow->next;
    }

    unlockFallenSnowBaseSemaphore();
    return true;
}

/** *********************************************************************
 ** This method is FallenSnow background thread looper.
 **/
void* startFallenSnowBackgroundThread() {
    while (true) {
        if (Flags.shutdownRequested) {
            pthread_exit(NULL);
        }

        // Main thread method.
        execFallenSnowBackgroundThread();
        usleep((useconds_t)
            TIME_BETWWEEN_FALLENSNOW_THREADS * 1000000);
    }

    return NULL;
}

/** *********************************************************************
 ** This method is FallenSnow background thread executor.
 **/
int execFallenSnowBackgroundThread() {
    if (!WorkspaceActive()) {
        return 0;
    }
    if (Flags.NoSnowFlakes) {
        return 0;
    }
     if (Flags.NoKeepSnowOnWindows &&
        Flags.NoKeepSnowOnBottom) {
        return 0;
    }

    lockFallenSnowBaseSemaphore();

    // Draw all fallen snow areas.
    FallenSnow* fsnow = mGlobal.FsnowFirst;
    while (fsnow) {
        // If each fallensnow is visible, check for Santa
        // Plow interaction, then render for later draw.
        if (canSnowCollectOnFallen(fsnow) &&
            isFallenSnowVisible(fsnow)) {
            removePlowedSnowFromFallen(fsnow);
            renderFallenSnow(fsnow);
        }
        fsnow = fsnow->next;
    }
    swapFallenSnowRenderedSurfaces();

    unlockFallenSnowBaseSemaphore();
    mFallenSnowBackgroundThread = false;
}

/** *********************************************************************
 ** This method determines if a fallensnow item can collect snow.
 **/
int canSnowCollectOnFallen(FallenSnow* fsnow) {
    // Is collecting for the desktop?
    if (fsnow->winInfo.window == None) {
        return !Flags.NoKeepSnowOnBottom;
    }

    if (fsnow->winInfo.hidden) {
        return false;
    }

    if (!fsnow->winInfo.sticky &&
        !isFallenSnowVisibleOnWorkspace(fsnow)) {
        return false;
    }

    return !Flags.NoKeepSnowOnWindows;
}

/** *********************************************************************
 ** This method determines if the fallensnow item is
 ** visible in the current workspace.
 **/
bool isFallenSnowVisible(FallenSnow* fsnow) {
    if (fsnow->winInfo.window == None) {
        return true;
    }

    if (fsnow->winInfo.hidden) {
        return false;
    }
    if (fsnow->winInfo.sticky) {
        return true;
    }
    if (isFallenSnowVisibleOnWorkspace(fsnow)) {
        return true;
    }

    return false;
}

/** *********************************************************************
 ** This method determines if the fallensnow item is
 ** visible in the current workspace.
 **/
bool isFallenSnowVisibleOnWorkspace(FallenSnow* fsnow) {
    if (!fsnow) {
        return false;
    }

    for (int i = 0; i < mGlobal.visualWSCount; i++) {
        if (mGlobal.visualWSList[i] == fsnow->winInfo.ws) {
            return true;
        }
    }
    return false;
}

/** *********************************************************************
 ** This method updates fallensnow items with impact
 ** of Santas sled ploughing. Santa plows facing forward.
 **/
void removePlowedSnowFromFallen(FallenSnow* fsnow) {
    if (Flags.NoSanta || !mGlobal.SantaPlowRegion) {
        return;
    }
    if (mGlobal.ActualSantaSpeed <= 0) {
        return;
    }

    const int ACTUAL_FALLEN_MAX_HEIGHT =
        getMaximumFallenSnowColumnHeight(fsnow);
    const bool IS_IT_IN = XRectInRegion(mGlobal.SantaPlowRegion,
        fsnow->x, fsnow->y - ACTUAL_FALLEN_MAX_HEIGHT,
        fsnow->w, ACTUAL_FALLEN_MAX_HEIGHT);
    if (IS_IT_IN != RectangleIn && IS_IT_IN != RectanglePart) {
        return;
    }

    // Santa facing right.
    if (mGlobal.SantaDirection == 0) {
        const int SNOW_TO_PLOW = 8;
        const int SANTA_FRONT = mGlobal.SantaX - fsnow->x +
            mGlobal.SantaWidth;
        const int SANTA_REAR = SANTA_FRONT - mGlobal.SantaWidth / 4;
        eraseFallenSnowPartial(fsnow, SANTA_REAR - SNOW_TO_PLOW,
            mGlobal.SantaWidth + 2 * SNOW_TO_PLOW);
        for (int i = 0; i < fsnow->w; i++) {
            if (i < SANTA_FRONT + SNOW_TO_PLOW &&
                i >= SANTA_REAR - SNOW_TO_PLOW) {
                fsnow->columnHeightList[i] = 0;
            }
        }
        return;
    }

    // Santa facing left.
    const int SNOW_TO_PLOW = 8;
    const int SANTA_FRONT = mGlobal.SantaX - fsnow->x -
        SNOW_TO_PLOW;
    const int SANTA_REAR = SANTA_FRONT + mGlobal.SantaWidth / 4;
    eraseFallenSnowPartial(fsnow, SANTA_REAR + SNOW_TO_PLOW,
        mGlobal.SantaWidth + 2 * SNOW_TO_PLOW);
    for (int i = 0; i < fsnow->w; i++) {
        if (i > SANTA_FRONT - SNOW_TO_PLOW &&
            i <= SANTA_REAR + SNOW_TO_PLOW) {
            fsnow->columnHeightList[i] = 0;
        }
    }
}

/** *********************************************************************
 ** This method renders a new fallensnow image onto renderedSurfaceB.
 ** threads: locking by caller
 **/
void renderFallenSnow(FallenSnow* fsnow) {
    cairo_t* cr = cairo_create(fsnow->renderedSurfaceB);

    cairo_set_antialias(cr, CAIRO_ANTIALIAS_DEFAULT);
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);

    cairo_save(cr);
    cairo_set_source_rgba(cr, 0, 0, 0, 0);
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_paint(cr);
    cairo_restore(cr);

    // MAIN SPLINE adjustment loop.
    // Compute averages for 10 points, draw spline through them
    // and use that to draw fallensnow
    const int FALLEN_WIDTH = fsnow->w;
    const int FALLEN_HEIGHT = fsnow->h;

    const short int* FALLEN_SNOW_HEIGHT = fsnow->columnHeightList;

    const int NUMBER_OF_POINTS_FOR_AVERAGE = 10;
    const int NUMBER_OF_AVERAGE_POINTS = MINIMUM_SPLINE_WIDTH +
        (FALLEN_WIDTH - 2) / NUMBER_OF_POINTS_FOR_AVERAGE;

    // Allocate array.
    double* averageHeightList = (double*)
        malloc(NUMBER_OF_AVERAGE_POINTS * sizeof(double));
    averageHeightList[0] = 0;

    double* averageXPosList  = (double*)
        malloc(NUMBER_OF_AVERAGE_POINTS * sizeof(double));
    averageXPosList[0] = 0;

    for (int i = 0; i < NUMBER_OF_AVERAGE_POINTS -
        MINIMUM_SPLINE_WIDTH; i++) {

        double averageSnowHeight = 0;
        for (int j = 0; j < NUMBER_OF_POINTS_FOR_AVERAGE; j++) {
            averageSnowHeight +=
                FALLEN_SNOW_HEIGHT[NUMBER_OF_POINTS_FOR_AVERAGE * i + j];
        }

        averageHeightList[i + 1] =
            averageSnowHeight / NUMBER_OF_POINTS_FOR_AVERAGE;
        averageXPosList[i + 1] = NUMBER_OF_POINTS_FOR_AVERAGE * i +
            NUMBER_OF_POINTS_FOR_AVERAGE * 0.5;
    }

    // Desktop.
    if (fsnow->winInfo.window == None) {
        averageHeightList[0] = averageHeightList[1];
    }

    // FINAL SPLINE adjustment loop.
    int k = NUMBER_OF_AVERAGE_POINTS - MINIMUM_SPLINE_WIDTH;
    int mk = NUMBER_OF_POINTS_FOR_AVERAGE * k;

    double averageSnowHeight = 0;
    for (int i = mk; i < FALLEN_WIDTH; i++) {
        averageSnowHeight += FALLEN_SNOW_HEIGHT[i];
    }

    averageHeightList[k + 1] = averageSnowHeight / (FALLEN_WIDTH - mk);
    averageXPosList[k + 1] = mk + 0.5 * (FALLEN_WIDTH - mk - 1);

    // Desktop.
    if (fsnow->winInfo.window == None) {
        averageHeightList[NUMBER_OF_AVERAGE_POINTS - 1] =
            averageHeightList[NUMBER_OF_AVERAGE_POINTS - 2];
    } else {
        averageHeightList[NUMBER_OF_AVERAGE_POINTS - 1] = 0;
    }
    averageXPosList[NUMBER_OF_AVERAGE_POINTS - 1] =
        FALLEN_WIDTH - 1;

    cairo_set_line_width(cr, 1);
    cairo_set_antialias(cr, CAIRO_ANTIALIAS_DEFAULT);

    gsl_interp_accel* accelerator = gsl_interp_accel_alloc();

    gsl_spline* spline = gsl_spline_alloc(SPLINE_INTERP,
        NUMBER_OF_AVERAGE_POINTS);
    gsl_spline_init(spline, averageXPosList, averageHeightList,
        NUMBER_OF_AVERAGE_POINTS);

    // Set surface topmost outline color.
    const GdkRGBA OUTLINE_COLOR_WHITE = (GdkRGBA) {
        .red = 1.0, .green = 1.0, .blue = 1.0, .alpha = 0.0 };

    // Set initial topmost outline color and size.
    cairo_set_line_width(cr, 6);
    cairo_set_source_rgb(cr, OUTLINE_COLOR_WHITE.red,
        OUTLINE_COLOR_WHITE.green, OUTLINE_COLOR_WHITE.blue);

    // Start rendering.
    enum { SEARCHING, DRAWING };
    int state = SEARCHING;
    int foundDrawPosition = 0;

    for (int i = 0; i < FALLEN_WIDTH; ++i) {
        const int nextValue =
            gsl_spline_eval(spline, i, accelerator);

        switch (state) {
            case SEARCHING:
                if (nextValue != 0) {
                    foundDrawPosition = i;
                    cairo_move_to(cr, i, FALLEN_HEIGHT);
                    cairo_line_to(cr, i, FALLEN_HEIGHT);
                    cairo_line_to(cr, i, FALLEN_HEIGHT - nextValue);

                    state = DRAWING;
                }
                break;

            case DRAWING:
                cairo_line_to(cr, i, FALLEN_HEIGHT - nextValue);
                if (nextValue == 0 || i == FALLEN_WIDTH - 1) {
                    cairo_line_to(cr, i, FALLEN_HEIGHT);
                    cairo_line_to(cr, foundDrawPosition, FALLEN_HEIGHT);
                    cairo_close_path(cr);
                    cairo_stroke_preserve(cr);

                    // Change to surface fill color and size.
                    cairo_set_line_width(cr, 1);
                    cairo_set_source_rgb(cr, fsnow->surfaceColor.red,
                        fsnow->surfaceColor.green, fsnow->surfaceColor.blue);
                    cairo_fill(cr);

                    // Reset to topmost outline color and size.
                    cairo_set_line_width(cr, 6);
                    cairo_set_source_rgb(cr, OUTLINE_COLOR_WHITE.red,
                        OUTLINE_COLOR_WHITE.green, OUTLINE_COLOR_WHITE.blue);

                    state = SEARCHING;
                }
                break;
        }
    }

    gsl_spline_free(spline);
    gsl_interp_accel_free(accelerator);

    free(averageXPosList);
    free(averageHeightList);

    cairo_destroy(cr);
}

/** *********************************************************************
 ** This method swaps fallen snow rendered areas B <---> A.
 **/
void swapFallenSnowRenderedSurfaces() {
    lockFallenSnowSwapSemaphore();

    FallenSnow* fsnow = mGlobal.FsnowFirst;
    while (fsnow) {
        cairo_surface_t* tempSurface = fsnow->renderedSurfaceB;
        fsnow->renderedSurfaceB = fsnow->renderedSurfaceA;
        fsnow->renderedSurfaceA = tempSurface;

        fsnow = fsnow->next;
    }

    unlockFallenSnowSwapSemaphore();
}

/** *********************************************************************
 ** This method checks for & performs user changes of
 ** FallenSnow module settings.
 **/
void respondToFallenSnowSettingsChanges() {
    UIDO(MaxWinSnowDepth,
        clearAllFallenSnowItems();
        clearGlobalSnowWindow();
    );
    UIDO(MaxScrSnowDepth,
        lockFallenSnowBaseSemaphore();
        updateFallenSnowDesktopItemDepth();
        unlockFallenSnowBaseSemaphore();
        clearAllFallenSnowItems();
        clearGlobalSnowWindow();
    );
    UIDO(NoKeepSnowOnBottom,
        clearAllFallenSnowItems();
        clearGlobalSnowWindow();
    );
    UIDO(NoKeepSnowOnWindows,
        clearAllFallenSnowItems();
        clearGlobalSnowWindow();
    );

    UIDO(IgnoreTop, );
    UIDO(IgnoreBottom, );
}

/** *********************************************************************
 ** This method sets global fallensnow item maximum depth &
 ** adjusts for a screen "SNOWFREE" area.
 **/
void updateFallenSnowDesktopItemDepth() {
    const int ALLOWED_DEPTH = mGlobal.SnowWinHeight -
        MAX_DESKTOP_SNOWFREE_HEIGHT;

    mGlobal.MaxScrSnowDepth =
        MIN(Flags.MaxScrSnowDepth, ALLOWED_DEPTH);
}

/** *********************************************************************
 ** This method sets sets desktops fallensnow item maximum height.
 **/
void updateFallenSnowDesktopItemHeight() {
    FallenSnow* fsnow = findFallenSnowItemByWindow(None);
    if (fsnow) {
        fsnow->y = mGlobal.SnowWinHeight;
    }
}

/** *********************************************************************
 ** This method updates fallensnow items with impact
 ** of a flake.
 **/
void updateFallenSnowWithSnow(FallenSnow* fsnow,
    int position, int width) {

    if (!WorkspaceActive() || Flags.NoSnowFlakes ||
        (Flags.NoKeepSnowOnWindows && Flags.NoKeepSnowOnBottom)) {
        return;
    }

    if (!canSnowCollectOnFallen(fsnow)) {
        return;
    }

    // tempHeightArray will contain the columnHeightList values
    // corresponding with position-1 .. position+width (inclusive).
    short int* tempHeightArray;
    tempHeightArray = (short int*)
        malloc(sizeof(* tempHeightArray) * (width + 2));

    int imin = position;
    if (imin < 0) {
        imin = 0;
    }
    int imax = position + width;
    if (imax > fsnow->w) {
        imax = fsnow->w;
    }

    int k = 0;
    for (int i = imin - 1; i <= imax; i++) {
        if (i < 0) {
            tempHeightArray[k++] = fsnow->columnHeightList[0];
        } else if (i >= fsnow->w) {
            tempHeightArray[k++] = fsnow->columnHeightList[fsnow->w - 1];
        } else {
            tempHeightArray[k++] = fsnow->columnHeightList[i];
        }
    }

    // Raise FallenSnow values.
    int amountToRaiseHeight;
    if (fsnow->columnHeightList[imin] <
        fsnow->columnMaxHeightList[imin] / 4) {
        amountToRaiseHeight = 4;
    } else if (fsnow->columnHeightList[imin] <
        fsnow->columnMaxHeightList[imin] / 2) {
        amountToRaiseHeight = 2;
    } else {
        amountToRaiseHeight = 1;
    }

    k = 1;
    for (int i = imin; i < imax; i++) {
        if ((fsnow->columnMaxHeightList[i] > tempHeightArray[k]) &&
            (tempHeightArray[k - 1] >= tempHeightArray[k] ||
                tempHeightArray[k + 1] >= tempHeightArray[k])) {
            fsnow->columnHeightList[i] = amountToRaiseHeight +
                (tempHeightArray[k - 1] + tempHeightArray[k + 1]) / 2;
        }
        k++;
    }

    // tempHeightArray will contain the new columnHeightList values
    // corresponding with position-1..position+width.
    k = 0;
    for (int i = imin - 1; i <= imax; i++) {
        if (i < 0) {
            tempHeightArray[k++] = fsnow->columnHeightList[0];
        } else if (i >= fsnow->w) {
            tempHeightArray[k++] = fsnow->columnHeightList[fsnow->w - 1];
        } else {
            tempHeightArray[k++] = fsnow->columnHeightList[i];
        }
    }

    // And now some smoothing.
    k = 1;
    for (int i = imin; i < imax; i++) {
        int sum = 0;
        for (int j = k - 1; j <= k + 1; j++) {
            sum += tempHeightArray[j];
        }
        fsnow->columnHeightList[i] = sum / 3;
        k++;
    }

    free(tempHeightArray);
}

/** *********************************************************************
 ** This method updates fallensnow items with impact
 ** of wind.
 **/
void blowoffSnowFromFallen(FallenSnow* fsnow,
    int w, int h) {
    const int x = randint(fsnow->w - w);

    for (int i = x; i < x + w; i++) {
        if (fsnow->columnHeightList[i] > h) {

            if (!Flags.NoWind && mGlobal.Wind != 0 && drand48() > 0.5) {
                const int numberOfFlakesToMake =
                    getNumberOfFlakesToBlowoff();
                for (int j = 0; j < numberOfFlakesToMake; j++) {
                    StormItem* flake = createStormItem(-1);
                    flake->survivesScreenEdges =
                        (fsnow->winInfo.window == 0);

                    flake->xRealPosition = fsnow->x + i;
                    flake->yRealPosition = fsnow->y -
                        fsnow->columnHeightList[i] - drand48() * 4;

                    flake->xVelocity = 0.25 * fsignf(mGlobal.NewWind) *
                        mGlobal.WindMax;
                    flake->yVelocity = -10;

                    addStormItemToItemset(flake);
                }
                eraseFallenSnowWindPixel(fsnow, i);
            }
        }
    }
}

/** *********************************************************************
 ** This method erases a single screen pixel.
 ** threads: locking by caller
 **/
void eraseFallenSnowWindPixel(FallenSnow *fsnow, int x) {
    if (fsnow->columnHeightList[x] <= 0) {
        return;
    }

    if (!mGlobal.isDoubleBuffered) {
        clearDisplayArea(mGlobal.display, mGlobal.SnowWin,
            fsnow->x + x, fsnow->y - fsnow->columnHeightList[x],
            1, 1, mGlobal.xxposures);
    }

    fsnow->columnHeightList[x]--;
}

/** *********************************************************************
 ** This method updates fallensnow items with impact
 ** of Santas sled ploughing. Santa plows facing forward.
 **/
void blowoffPlowedSnowFromFallen(FallenSnow* fsnow) {
    if (mGlobal.ActualSantaSpeed <= 0) {
        return;
    }

    // Santa facing right.
    if (mGlobal.SantaDirection == 0) {
        const int PLOW_X = mGlobal.SantaX + mGlobal.SantaWidth;
        const int PLOW_Y1 = mGlobal.SantaY +
            (fsnow->y - mGlobal.SantaY) / 2;
        const int PLOW_Y2 = fsnow->y;
        createPlowedStormItems(fsnow, PLOW_X, PLOW_Y1, PLOW_Y2);
        return;
    }

    // Santa facing left.
    const int PLOW_X = mGlobal.SantaX;
    const int PLOW_Y1 = mGlobal.SantaY +
        (fsnow->y - mGlobal.SantaY) / 2;
    const int PLOW_Y2 = fsnow->y;
    createPlowedStormItems(fsnow, PLOW_X, PLOW_Y1, PLOW_Y2);
}

/** *********************************************************************
 ** This method generates two streams of blowoff
 ** for Santa plowing animation. threads: locking by caller
 **/
void createPlowedStormItems(FallenSnow* fsnow, int xPos,
    int yPos1, int yPos2) {
    if (!Flags.BlowSnow || Flags.NoSnowFlakes) {
        return;
    }

    // Create up to 10 blown items in an upper and
    // lower stream.
    const int PLOWED_ITEMS_TO_CREATE = 10;

    for (int i = 0; i < PLOWED_ITEMS_TO_CREATE; i++) {
        StormItem* flake = createStormItem(-1);
        flake->survivesScreenEdges = false;
        flake->xRealPosition = xPos;
        flake->yRealPosition = yPos1;
        flake->xVelocity = -0.1 * fsignf(mGlobal.NewWind) *
            mGlobal.WindMax;
        flake->yVelocity = -randint(6) - 10;
        addStormItemToItemset(flake);

        StormItem* flake2 = createStormItem(-1);
        flake2->survivesScreenEdges = false;
        flake2->xRealPosition = xPos;
        flake2->yRealPosition = yPos2;
        flake2->xVelocity = -0.1 * fsignf(mGlobal.NewWind) *
            mGlobal.WindMax;
        flake2->yVelocity = -randint(6) - 10;
        addStormItemToItemset(flake2);
    }
}

/** *********************************************************************
 ** This method is the main "draw frame" routine for fallen snow.
 ** Draw all rendered fallensnow from surfaceA.
 **/
void drawFallenSnowFrame(cairo_t* cr) {
    if (!WorkspaceActive() || Flags.NoSnowFlakes ||
        (Flags.NoKeepSnowOnWindows && Flags.NoKeepSnowOnBottom)) {
        return;
    }

    lockFallenSnowSwapSemaphore();

    FallenSnow* fsnow = mGlobal.FsnowFirst;
    while (fsnow) {
        if (canSnowCollectOnFallen(fsnow)) {
            cairo_set_source_surface(cr, fsnow->renderedSurfaceA,
                fsnow->x, fsnow->y - fsnow->h);
            my_cairo_paint_with_alpha(cr, ALPHA);

            fsnow->prevx = fsnow->x;
            fsnow->prevy = fsnow->y - fsnow->h + 1;

            fsnow->prevw = cairo_image_surface_get_width(
                fsnow->renderedSurfaceA);
            fsnow->prevh = fsnow->h;
        }

        fsnow = fsnow->next;
    }

    unlockFallenSnowSwapSemaphore();
}

/** *********************************************************************
 ** This method returns a fallensnow area for a window.
 **/
FallenSnow* findFallenSnowItemByWindow(Window window) {
    FallenSnow* fallenSnow = mGlobal.FsnowFirst;

    while (fallenSnow) {
        if (fallenSnow->winInfo.window == window) {
            return fallenSnow;
        }
        fallenSnow = fallenSnow->next;
    }

    return NULL;
}

/** *********************************************************************
 ** This method erases a fallensnow display area.
 **/
void eraseFallenSnow(FallenSnow* fsnow) {
    eraseFallenSnowPartial(fsnow, 0, fsnow->w);
}

/** *********************************************************************
 ** This method erases a fallensnow display area.
 **/
void eraseFallenSnowPartial(FallenSnow* fsnow,
    int xstart, int width) {
    if (mGlobal.isDoubleBuffered) {
        return;
    }

    clearDisplayArea(mGlobal.display, mGlobal.SnowWin,
        fsnow->prevx + xstart, fsnow->prevy, width,
        fsnow->h + mGlobal.MaxFlakeHeight, mGlobal.xxposures);
}

/** *********************************************************************
 ** This method returns the largest actual column height
 ** in a FallenSnow list.
 **/
int getMaximumFallenSnowColumnHeight(FallenSnow* fsnow) {
    int maxHeight = 0;
    for (int i = 0; i < fsnow->w; i++) {
        if (maxHeight < fsnow->columnHeightList[i]) {
            maxHeight = fsnow->columnHeightList[i];
        }
    }
    return maxHeight;
}

/** *********************************************************************
 ** When dragging a window we shake fallen snow off it.
 ** Here, we don't know the window, so we shake them all off.
 **/
void removeFallenSnowFromAllWindows() {
    WinInfo* winInfoListItem = mGlobal.winInfoList;
    for (int i = 0; i < mGlobal.winInfoListLength; i++) {
        removeFallenSnowFromWindow(winInfoListItem[i].window);
    }
}

/** *********************************************************************
 ** This method shakes the fallen snow off a window.
 **/
void removeFallenSnowFromWindow(Window window) {
    FallenSnow* fallenListItem =
        findFallenSnowItemByWindow(window);
    if (!fallenListItem) {
        return;
    }

    lockFallenSnowBaseSemaphore();

    eraseFallenSnow(fallenListItem);
    generateFallenSnowFlakes(fallenListItem, 0,
        fallenListItem->w, -15.0, false);
    removeAndFreeFallenSnowForWindow(&mGlobal.FsnowFirst,
        fallenListItem->winInfo.window);

    unlockFallenSnowBaseSemaphore();
}

/** *********************************************************************
 ** This method removes a fallen snow item from the linked list.
 **/
int removeAndFreeFallenSnowForWindow(FallenSnow** list,
    Window id) {
    if (*list == NULL) {
        return false;
    }

    // Do we hit on first item in List?
    FallenSnow *fallen = *list;
    if (fallen->winInfo.window == id) {
        fallen = fallen->next;
        freeFallenSnowItem(*list);
        *list = fallen;
        return true;
    }

    // Loop through remainder of items.
    FallenSnow* scratch = NULL;
    while (true) {
        if (fallen->next == NULL) {
            return false;
        }

        scratch = fallen->next;
        if (scratch->winInfo.window == id) {
            break;
        }

        fallen = fallen->next;
    }

    fallen->next = scratch->next;
    freeFallenSnowItem(scratch);

    return true;
}

/** *********************************************************************
 ** This method generates snow blowoff and drops.
 ** threads: locking by caller
 **/
void generateFallenSnowFlakes(FallenSnow* fsnow,
    int xPos, int xWidth, float yVelocity, bool limitToMax) {
    if (!Flags.BlowSnow || Flags.NoSnowFlakes) {
        return;
    }

    const int X_LEFT  = MIN(MAX(0, xPos), fsnow->w);
    const int X_RIGHT = MIN(MAX(0, xPos + xWidth), fsnow->w);

    // 90% of hard limit.
    const int MAX_FLAKES_TO_GENERATE =
        (int) ((float) Flags.FlakeCountMax * 0.9);

    for (int i = X_LEFT; i < X_RIGHT; i++) {
        const int J_MAX = fsnow->columnHeightList[i];

        for (int j = 0; j < J_MAX; j++) {
            const int kMax = getNumberOfFlakesToBlowoff();

            for (int k = 0; k < kMax; k++) {
                if (drand48() >= 0.15) {
                    continue;
                }

                // Avoid runaway flake generation during plowing.
                if (limitToMax &&
                    mGlobal.stormItemCount >= MAX_FLAKES_TO_GENERATE) {
                    return;
                }

                StormItem* flake = createStormItem(-1);
                flake->survivesScreenEdges = false;
                flake->xRealPosition = fsnow->x + i +
                    16 * (drand48() - 0.5);
                flake->yRealPosition = fsnow->y - j - 8;
                flake->xVelocity = (Flags.NoWind) ?
                    0 : mGlobal.NewWind / 8;
                flake->yVelocity = yVelocity;
                addStormItemToItemset(flake);
            }
        }
    }
}

/** *********************************************************************
 ** WinInfo change watchers.
 **
 ** This method determines fallen region actions for cases of window
 ** add ins, hides./removes, programmatic removes etc.
 **
 ** Convienience "vector" methods ignore dup adds. Some windows such as
 ** chrome move position on minimize, duplicating themselves otherwise
 ** and causing two fallen snow drops.
 **/
void doAllFallenSnowWinInfoUpdates() {
    doWinInfoWSHides();
    doWinInfoInitialAdds();

    windowVectorInit(&mDeferredWindowRemovesList);

    doWinInfoRemoves();
    doWinInfoProgrammaticRemoves();

    windowVectorFree(&mDeferredWindowRemovesList);
}

/** *********************************************************************
 ** This method determines which fallen snow items to hide
 ** due to WS switch.
 **/
void doWinInfoWSHides() {
    WinInfo* addedWinInfo = mGlobal.winInfoList;

    for (int i = 0; i < mGlobal.winInfoListLength; i++) {
        FallenSnow* fsnow = findFallenSnowItemByWindow(
            addedWinInfo->window);

        if (fsnow) {
            fsnow->winInfo = *addedWinInfo;
            if (fsnow->winInfo.ws != mGlobal.currentWS &&
                !fsnow->winInfo.sticky) {
                eraseFallenSnow(fsnow);
            }
        }

        addedWinInfo++;
    }
}

/** *********************************************************************
 ** This method determines new fallen regions to be added-in to
 ** a window.
 **/
void doWinInfoInitialAdds() {
    WinInfo* addedWinInfo = mGlobal.winInfoList;

    for (int i = 0; i < mGlobal.winInfoListLength; i++) {
        FallenSnow* fsnow = findFallenSnowItemByWindow(
            addedWinInfo->window);

        if (!fsnow) {
            if (addedWinInfo->window != mGlobal.SnowWin &&
                addedWinInfo->y > 0 && !addedWinInfo->dock &&
                !isWindowBeingDragged()) {

                // When Chrome starts, it has a transparent window for
                // several seconds. This causes a fallensnow to collect
                // snow above a seemingly missing window. To fix, we'll
                // delay the fallensnow item add until Chrome actually
                // provides (draws) any window data.
                XClassHint classHints;
                XGetClassHint(mGlobal.display, addedWinInfo->window,
                    &classHints);
                if (strcmp(classHints.res_name, "google-chrome") == 0 &&
                    strcmp(classHints.res_class, "Google-chrome") == 0) {
                    // Bypass if transparent (matches background).
                    if (windowIsTransparent(addedWinInfo->window)) {
                        addedWinInfo++;
                        continue;
                    }
                }

                // Add new item.
                pushFallenSnowItem(&mGlobal.FsnowFirst,
                    addedWinInfo, addedWinInfo->x + Flags.OffsetX,
                    addedWinInfo->y + Flags.OffsetY,
                    addedWinInfo->w + Flags.OffsetW,
                    Flags.MaxWinSnowDepth);
            }
        }

        addedWinInfo++;
    }
}

/** *********************************************************************
 ** This method determines if a window is transparent to the user to help
 ** with a Chrome startup bug.
 **
 ** The window is transparent if a representative block of data
 ** at a certain window position is all zeros.
 **
 ** You can check the whole window size, I prefer this
 ** shorter / faster version.
 **/
bool windowIsTransparent(Window window) {
    const int BLOCK_SIZE = 3;

    const int XPOS = 100;
    const int YPOS = 100;

    // If there is no window, it's transparent.
    XImage* windowImage = XGetImage(mGlobal.display, window,
        XPOS, YPOS, BLOCK_SIZE, BLOCK_SIZE, XAllPlanes(), ZPixmap);
    if (!windowImage) {
        return true;
    }

    // If any data item isn't zero, it's not transparent.
    for (int i = 0; i < BLOCK_SIZE * BLOCK_SIZE; i++) {
        const unsigned int DATA =
            *((windowImage->data) + i);
        if (DATA) {
            return false;
        }
    }
    return true;
}

/** *********************************************************************
 ** This method determines fallen regions to be erased & removed
 ** when base window is hidden or removed.
 **/
void doWinInfoRemoves() {
    FallenSnow* fsnow = mGlobal.FsnowFirst;

    while (fsnow) {
        if (fsnow->winInfo.window == None) {
            fsnow = fsnow->next;
            continue;
        }

        // Test if fsnow->winInfo.window has become hidden.
        if (fsnow->winInfo.hidden) {
            // Get printable title.
            if (windowVectorAdd(&mDeferredWindowRemovesList,
                fsnow->winInfo.window)) {
                eraseFallenSnow(fsnow);
                generateFallenSnowFlakes(fsnow, 0, fsnow->w,
                    15.0, false);
                removeAndFreeFallenSnowForWindow(
                    &mGlobal.FsnowFirst, fsnow->winInfo.window);
            }
            fsnow = fsnow->next;
            continue;
        }

        // Test if window has become closed.
        WinInfo* removeWinInfo = getWinInfoForWindow(
            fsnow->winInfo.window);
        if (!removeWinInfo) {
            if (windowVectorAdd(&mDeferredWindowRemovesList,
                fsnow->winInfo.window)) {
                eraseFallenSnow(fsnow);
                generateFallenSnowFlakes(fsnow, 0, fsnow->w,
                    15.0, false);
                removeAndFreeFallenSnowForWindow(
                    &mGlobal.FsnowFirst, fsnow->winInfo.window);
            }
            fsnow = fsnow->next;
            continue;
        }

        // Do we have a large window near top?
        const float WIN_WIDTH_LIMIT =
            mGlobal.SnowWinWidth * 0.8;

        if (removeWinInfo->w > WIN_WIDTH_LIMIT &&
            removeWinInfo->ya < Flags.IgnoreTop) {
            if (windowVectorAdd(&mDeferredWindowRemovesList,
                fsnow->winInfo.window)) {
                eraseFallenSnow(fsnow);
                generateFallenSnowFlakes(fsnow, 0, fsnow->w,
                    -15.0, false);
                removeAndFreeFallenSnowForWindow(
                    &mGlobal.FsnowFirst, fsnow->winInfo.window);
            }
            fsnow = fsnow->next;
            continue;
        }

        // Do we have a large window near bottom?
        const int REMOVE_POS =
            (int) mGlobal.SnowWinHeight - removeWinInfo->ya;

        if (removeWinInfo->w > WIN_WIDTH_LIMIT &&
            REMOVE_POS < Flags.IgnoreBottom) {
            if (windowVectorAdd(&mDeferredWindowRemovesList,
                fsnow->winInfo.window)) {
                eraseFallenSnow(fsnow);
                generateFallenSnowFlakes(fsnow, 0, fsnow->w,
                    -15.0, false);
                removeAndFreeFallenSnowForWindow(
                    &mGlobal.FsnowFirst, fsnow->winInfo.window);
            }
            fsnow = fsnow->next;
            continue;
        }

        fsnow = fsnow->next;
    }
}

/** *********************************************************************
 ** This method determines fallen regions with Windows that were
 ** moved programatically.
 **/
void doWinInfoProgrammaticRemoves() {
    WinInfo* removeWinInfo = mGlobal.winInfoList;

    for (int i = 0; i < mGlobal.winInfoListLength; i++) {
        FallenSnow* fsnow = findFallenSnowItemByWindow(
            removeWinInfo->window);

        if (fsnow) {
            if (fsnow->x != removeWinInfo->x + Flags.OffsetX ||
                fsnow->y != removeWinInfo->y + Flags.OffsetY ||
                (unsigned int) fsnow->w !=
                    removeWinInfo->w + Flags.OffsetW) {

                if (windowVectorAdd(&mDeferredWindowRemovesList,
                    fsnow->winInfo.window)) {
                    eraseFallenSnow(fsnow);
                    generateFallenSnowFlakes(fsnow, 0, fsnow->w,
                        20.0, false);
                    removeAndFreeFallenSnowForWindow(
                        &mGlobal.FsnowFirst, fsnow->winInfo.window);
                }
                fsnow->x = removeWinInfo->x + Flags.OffsetX;
                fsnow->y = removeWinInfo->y + Flags.OffsetY;
            }
        }

        removeWinInfo++;
    }
}

/** *********************************************************************
 ** This method returns the number of FallenSnow items
 ** in the linked list.
 **/
int getFallenSnowItemcount() {
    int numberFallen = 0;

    for (FallenSnow* temp = mGlobal.FsnowFirst;
        temp; temp = temp->next) {
        numberFallen++;
    }

    return numberFallen;
}

/** *********************************************************************
 ** This method clears and inits the FallenSnow list.
 ** The initial list contains a single FallenSnow for Desktop.
 **/
void clearAllFallenSnowItems() {
    lockFallenSnowBaseSemaphore();

    // Clear all fallen snow areas.
    while (mGlobal.FsnowFirst) {
        popAndFreeFallenSnowItem(&mGlobal.FsnowFirst);
    }

    // Push Desktop FallenSnow with dummy WinInfo.
    WinInfo* tempWinInfo = (WinInfo*) malloc(sizeof(WinInfo));
    memset(tempWinInfo, 0, sizeof(WinInfo));

    pushFallenSnowItem(&mGlobal.FsnowFirst,
        tempWinInfo, 0, mGlobal.SnowWinHeight,
        mGlobal.SnowWinWidth, mGlobal.MaxScrSnowDepth);
    free(tempWinInfo);

    unlockFallenSnowBaseSemaphore();
}

/** *********************************************************************
 ** This method creates and adds a new FallenSnow item
 ** onto the linked-list.
 **/
void pushFallenSnowItem(FallenSnow** fallenSnowArray,
    WinInfo* winInfo, int x, int y, int w, int h) {
    // "Too-narrow" windows cause issues.
    if (w < MINIMUM_SPLINE_WIDTH) {
        return;
    }

    // Allocate new object.
    FallenSnow* fallenSnowListItem = (FallenSnow*)
        malloc(sizeof(FallenSnow));

    fallenSnowListItem->winInfo = *winInfo;

    fallenSnowListItem->x = x;
    fallenSnowListItem->y = y;
    fallenSnowListItem->w = w;
    fallenSnowListItem->h = h;

    fallenSnowListItem->prevx = 0;
    fallenSnowListItem->prevy = 0;
    fallenSnowListItem->prevw = 10;
    fallenSnowListItem->prevh = 10;

    fallenSnowListItem->surfaceColor = getNextStormShapeColorAsRGB();

    fallenSnowListItem->renderedSurfaceA =
        cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
    fallenSnowListItem->renderedSurfaceB =
        cairo_surface_create_similar(fallenSnowListItem->renderedSurfaceA,
        CAIRO_CONTENT_COLOR_ALPHA, w, h);

    // Allocate arrays.
    fallenSnowListItem->columnHeightList    = (short int *)
        malloc(sizeof(*(fallenSnowListItem->columnHeightList)) * w);
    fallenSnowListItem->columnMaxHeightList = (short int *)
        malloc(sizeof(*(fallenSnowListItem->columnMaxHeightList)) * w);

    // Fill arrays & init Maximums.
    for (int i = 0; i < w; i++) {
        fallenSnowListItem->columnHeightList[i] = 0;
        fallenSnowListItem->columnMaxHeightList[i] = h;
    }
    setColumnMaxHeightListForFallen(fallenSnowListItem);

    fallenSnowListItem->next = *fallenSnowArray;
    *fallenSnowArray = fallenSnowListItem;
}

/** *********************************************************************
 ** This method pops a node from the start of the list.
 **/
void popAndFreeFallenSnowItem(FallenSnow** list) {
    if (*list == NULL) {
        return;
    }

    FallenSnow* next_node = (*list)->next;
    freeFallenSnowItem(*list);
    *list = next_node;
}

/** *********************************************************************
 ** This method frees all of a fallensnows memory allocations.
 **/
void freeFallenSnowItem(FallenSnow* fallen) {
    free(fallen->columnHeightList);
    free(fallen->columnMaxHeightList);

    cairo_surface_destroy(fallen->renderedSurfaceA);
    cairo_surface_destroy(fallen->renderedSurfaceB);

    free(fallen);
}

/** *********************************************************************
 * Helper methods for Thread locking.
 */
void initFallenSnowSemaphores() {
    sem_init(&mFallenSnowBaseSemaphore, 0, 1);
    sem_init(&mFallenSnowSwapSemaphore, 0, 1);
}

int softLockFallenSnowBaseSemaphore(
    int maxSoftTries, int* tryCount) {
    int resultCode;

    // Guard tryCount and bump.
    if (*tryCount < 0) {
        *tryCount = 0;
    }
    (*tryCount)++;

    // Set resultCode from soft or hard wait.
    resultCode = (*tryCount > maxSoftTries) ?
        sem_wait(&mFallenSnowBaseSemaphore) :
        sem_trywait(&mFallenSnowBaseSemaphore);

    // Success clears tryCount for next time.
    if (resultCode == 0) {
        *tryCount = 0;
    }

    return resultCode;
}

// Base semaphores.
int lockFallenSnowBaseSemaphore() {
    return sem_wait(&mFallenSnowBaseSemaphore);
}
int unlockFallenSnowBaseSemaphore() {
    return sem_post(&mFallenSnowBaseSemaphore);
}

// Swap semaphores.
int lockFallenSnowSwapSemaphore() {
    return sem_wait(&mFallenSnowSwapSemaphore);
}
int unlockFallenSnowSwapSemaphore() {
    return sem_post(&mFallenSnowSwapSemaphore);
}

/** *********************************************************************
 ** This method logs a request fallen snow area.
 **/
void logAllFallenSnowItems() {
    for (FallenSnow* fallen = mGlobal.FsnowFirst;
        fallen; fallen = fallen->next) {

        logFallenSnowItem(fallen);
    }
    printf("\n");
}

void logFallenSnowItem(FallenSnow* fallen) {
    printf("id: %#10lx  ws: %4ld  x: %6d  y: %6d  "
        "w: %6d  h: %6d  sty: %2d  hid: %2d\n",
        fallen->winInfo.window, fallen->winInfo.ws,
        fallen->x, fallen->y, fallen->w, fallen->h,
        fallen->winInfo.sticky, fallen->winInfo.hidden);
}
