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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

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

#include "blowoff.h"
#include "debug.h"
#include "fallensnow.h"
#include "Flags.h"
#include "safe_malloc.h"
#include "Santa.h"
#include "snow.h"
#include "spline_interpol.h"
#include "Utils.h"
#include "wind.h"
#include "windows.h"


const int MINIMUM_SPLINE_WIDTH = 3;

// Semaphore & lock members.
sem_t mFallenSnowSwapSemaphore;
sem_t mFallenSnowBaseSemaphore;

int mDebugGenerateCnt = 15;
int mDebugMsgMax = 5;


/***********************************************************
 * Helper methods for semaphores.
 */
void initFallenSnowSemaphores() {
    sem_init(&mFallenSnowSwapSemaphore, 0, 1);
    sem_init(&mFallenSnowBaseSemaphore, 0, 1);
}

// Swap semaphores.
int lockFallenSnowSwapSemaphore() {
    return sem_wait(&mFallenSnowSwapSemaphore);
}
int unlockFallenSnowSwapSemaphore() {
    return sem_post(&mFallenSnowSwapSemaphore);
}

// Base semaphores.
int lockFallenSnowBaseSemaphore() {
    return sem_wait(&mFallenSnowBaseSemaphore);
}
int unlockFallenSnowBaseSemaphore() {
    return sem_post(&mFallenSnowBaseSemaphore);
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
        sem_wait(&mFallenSnowBaseSemaphore) : sem_trywait(&mFallenSnowBaseSemaphore);

    // Success clears tryCount for next time.
    if (resultCode == 0) {
        *tryCount = 0;
    }

    return resultCode;
}

/** *********************************************************************
 ** This method ...
 **/
void initFallenSnowModule() {
    clearAllFallenSnowItems();

    addMethodToMainloop(PRIORITY_DEFAULT,
        time_change_bottom, do_change_deshes);
    addMethodToMainloop(PRIORITY_DEFAULT,
        time_adjust_bottom, do_adjust_deshes);

    // Start main background thread looper & exit.
    static pthread_t thread;
    pthread_create(&thread, NULL, startFallenSnowBackgroundThread, NULL);
}

/** *********************************************************************
 ** This method clears and inits the FallenSnow list of items.
 **/
void clearAllFallenSnowItems() {
    lockFallenSnowSemaphore();

    // Clear all fallen snow areas.
    while (mGlobal.FsnowFirst) {
        popFallenSnowItem(&mGlobal.FsnowFirst);
    }

    // Allocate temp empty WinInfo.
    WinInfo* tempWinInfo = (WinInfo*) malloc(sizeof(WinInfo));
    memset(tempWinInfo, 0, sizeof(WinInfo));

    // Push first fallen area with zero-ed WinInfo
    // into list as desktop.
    pushFallenSnowItem(&mGlobal.FsnowFirst,
        tempWinInfo, 0, mGlobal.SnowWinHeight,
        mGlobal.SnowWinWidth, mGlobal.MaxScrSnowDepth);

    free(tempWinInfo);
    unlockFallenSnowSemaphore();
}

/** *********************************************************************
 ** This method is a private thread looper.
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
 ** This method is the main "draw frame" routine for fallen snow.
 **/
void execFallenSnowBackgroundThread() {
    if (!WorkspaceActive()) {
        return;
    }

    if (Flags.NoSnowFlakes) {
        return;
    }
 
    if (Flags.NoKeepSnowOnWindows &&
        Flags.NoKeepSnowOnBottom) {
        return;
    }

    lockFallenSnowSemaphore();

    // Draw all fallen snow areas.
    FallenSnow* fallenSnowItem = mGlobal.FsnowFirst;
    while (fallenSnowItem) {
        if (canSnowCollectOnWindowOrScreenBottom(fallenSnowItem)) {
            drawFallenSnowItem(fallenSnowItem);
        }
        fallenSnowItem = fallenSnowItem->next;
    }

    XFlush(mGlobal.display);
    swapFallenSnowListItemSurfaces();

    unlockFallenSnowSemaphore();
}

/** *********************************************************************
 ** This method ...
 **/
void doFallenSnowUserSettingUpdates() {
    UIDO(MaxWinSnowDepth,
        clearAllFallenSnowItems();
        clearGlobalSnowWindow();
    );
    UIDO(MaxScrSnowDepth,
        setMaxScreenSnowDepthWithLock();
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
 ** This method ...
 **/
void setMaxScreenSnowDepth() {
    mGlobal.MaxScrSnowDepth = Flags.MaxScrSnowDepth;

    if (mGlobal.MaxScrSnowDepth >
        (int) (mGlobal.SnowWinHeight - SNOWFREE)) {
        mGlobal.MaxScrSnowDepth =
            mGlobal.SnowWinHeight - SNOWFREE;
    }
}

/** *********************************************************************
 ** This method ...
 **/
void setMaxScreenSnowDepthWithLock() {
    lockFallenSnowSemaphore();
    setMaxScreenSnowDepth();
    unlockFallenSnowSemaphore();
}

/** *********************************************************************
 ** This method ...
 **/
int canSnowCollectOnWindowOrScreenBottom(FallenSnow *fsnow) {
    if (fsnow->winInfo.window == None) {
        return !Flags.NoKeepSnowOnBottom;
    }

    if (fsnow->winInfo.hidden) {
        return false;
    }

    if (!fsnow->winInfo.sticky &&
        !isFallenSnowOnVisibleWorkspace(fsnow)) {
        return false;
    }

    return !Flags.NoKeepSnowOnWindows;
}

/** *********************************************************************
 ** This method ...
 **/
int isFallenSnowOnVisibleWorkspace(FallenSnow *fsnow) {
    if (fsnow) {
        for (int i = 0; i < mGlobal.NVisWorkSpaces; i++) {
            if (mGlobal.VisWorkSpaces[i] == fsnow->winInfo.ws) {
                return true;
            }
        }
    }

    return false;
}

/** *********************************************************************
 ** Animation of blown off snow.
 **
 ** This method removes some fallen snow from fsnow, w pixels.
 ** If fallensnowheight < h: no removal.
 **
 ** Also add snowflakes.
 **/
void updateFallenSnowWithWind(FallenSnow *fsnow, int w, int h) {
    const int x = randint(fsnow->w - w);

    for (int i = x; i < x + w; i++) {
        if (fsnow->snowHeight[i] > h) {

            if (!Flags.NoWind && mGlobal.Wind != 0 && drand48() > 0.5) {
                const int numberOfFlakesToMake = getNumberOfFlakesToBlowoff();
                for (int j = 0; j < numberOfFlakesToMake; j++) {
                    SnowFlake* flake = MakeFlake(-1);

                    flake->rx = fsnow->x + i;
                    flake->ry = fsnow->y - fsnow->snowHeight[i] - drand48() * 4;
                    flake->vx = 0.25 * fsignf(mGlobal.NewWind) * mGlobal.WindMax;
                    flake->vy = -10;

                    // Not cyclic for Windows, cyclic for bottom.
                    flake->cyclic = (fsnow->winInfo.window == 0);
                }
                eraseFallenSnowAtPixel(fsnow, i);
            }
        }
    }
}

/** *********************************************************************
 ** This method ...
 **/
void updateFallenSnowPartial(FallenSnow* fsnow,
    int position, int width) {

    if (!WorkspaceActive() || Flags.NoSnowFlakes ||
        (Flags.NoKeepSnowOnWindows && Flags.NoKeepSnowOnBottom)) {
        return;
    }

    if (!canSnowCollectOnWindowOrScreenBottom(fsnow)) {
        return;
    }

    // ****************
    // tempHeightArray will contain the snowHeight values
    // corresponding with position-1 .. position+width (inclusive).
    short int* tempHeightArray;
    tempHeightArray = (short int*)
        malloc(sizeof(* tempHeightArray) * (width + 2));

    // ****************
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
            tempHeightArray[k++] = fsnow->snowHeight[0];
        } else if (i >= fsnow->w) {
            tempHeightArray[k++] = fsnow->snowHeight[fsnow->w - 1];
        } else {
            tempHeightArray[k++] = fsnow->snowHeight[i];
        }
    }

    // ****************
    // Raise FallenSnow values.
    int amountToRaiseHeight;
    if (fsnow->snowHeight[imin] < fsnow->maxSnowHeight[imin] / 4) {
        amountToRaiseHeight = 4;
    } else if (fsnow->snowHeight[imin] < fsnow->maxSnowHeight[imin] / 2) {
        amountToRaiseHeight = 2;
    } else {
        amountToRaiseHeight = 1;
    }

    k = 1;
    for (int i = imin; i < imax; i++) {
        if ((fsnow->maxSnowHeight[i] > tempHeightArray[k]) &&
            (tempHeightArray[k - 1] >= tempHeightArray[k] ||
                tempHeightArray[k + 1] >= tempHeightArray[k])) {
            fsnow->snowHeight[i] = amountToRaiseHeight +
                (tempHeightArray[k - 1] + tempHeightArray[k + 1]) / 2;
        }
        k++;
    }

    //  ****************
    // tempHeightArray will contain the new snowHeight values
    // corresponding with position-1..position+width.
    k = 0;
    for (int i = imin - 1; i <= imax; i++) {
        if (i < 0) {
            tempHeightArray[k++] = fsnow->snowHeight[0];
        } else if (i >= fsnow->w) {
            tempHeightArray[k++] = fsnow->snowHeight[fsnow->w - 1];
        } else {
            tempHeightArray[k++] = fsnow->snowHeight[i];
        }
    }

    //  ****************
    // And now some smoothing.
    k = 1;
    for (int i = imin; i < imax; i++) {
        int sum = 0;
        for (int j = k - 1; j <= k + 1; j++) {
            sum += tempHeightArray[j];
        }
        fsnow->snowHeight[i] = sum / 3;
        k++;
    }

    free(tempHeightArray);
}

/** *********************************************************************
 ** This method ...
 **/
void updateFallenSnowAtBottom() {
    // threads: locking by caller
    FallenSnow *fsnow = findFallenSnowItemByWindow(
        mGlobal.FsnowFirst, 0);
    if (fsnow) {
        fsnow->y = mGlobal.SnowWinHeight;
    }
}

/** *********************************************************************
 ** This method ...
 **/
void generateFallenSnowFlakes(FallenSnow* fsnow,
    int xPos, int xWidth, float vy, bool limitToMax) {

    // threads: locking by caller
    if (!Flags.BlowSnow || Flags.NoSnowFlakes) {
        return;
    }

    int ifirst = xPos;
    if (ifirst < 0) {
        ifirst = 0;
    }
    if (ifirst > fsnow->w) {
        ifirst = fsnow->w;
    }

    int ilast = xPos + xWidth;
    if (ilast < 0) {
        ilast = 0;
    }
    if (ilast > fsnow->w) {
        ilast = fsnow->w;
    }

    const int maxValueDuringGenerate =
        (int) ((float) Flags.FlakeCountMax * 0.9);

    for (int i = ifirst; i < ilast; i++) {
        for (int j = 0; j < fsnow->snowHeight[i]; j++) {
            const int kmax = getNumberOfFlakesToBlowoff();

            for (int k = 0; k < kmax; k++) {
                if (drand48() >= 0.15) {
                    continue;
                }

                // Avoid runaway flake generation especially during plowing.
                if (limitToMax &&
                    mGlobal.FlakeCount >= maxValueDuringGenerate) {
                    printf("generateFallenSnowFlakes() before MakeFlake : "
                        "Actual: %i Max during generate: %i flakes.\n",
                        mGlobal.FlakeCount, maxValueDuringGenerate);
                    return;
                }

                SnowFlake* flake = MakeFlake(-1);
                flake->cyclic = 0;
                flake->rx = fsnow->x + i + 16 * (drand48() - 0.5);
                flake->ry = fsnow->y - j - 8;
                flake->vx = (Flags.NoWind) ? 0 : mGlobal.NewWind / 8;
                flake->vy = vy;
            }
        }
    }
}

/** *********************************************************************
 ** This method pushes a node onto the list.
 **/
void pushFallenSnowItem(FallenSnow** fallenSnowArray,
    WinInfo* winInfo, int x, int y, int w, int h) {

    // TODO: "Too-narrow" windows cause issues.
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

    fallenSnowListItem->surface = cairo_image_surface_create(
        CAIRO_FORMAT_ARGB32, w, h);
    fallenSnowListItem->surface1 = cairo_surface_create_similar(
        fallenSnowListItem->surface, CAIRO_CONTENT_COLOR_ALPHA,
        w, h);

    // Allocate arrays.
    fallenSnowListItem->columnColor   = (GdkRGBA *)
        malloc(sizeof(*(fallenSnowListItem->columnColor)) * w);
    fallenSnowListItem->snowHeight    = (short int *)
        malloc(sizeof(*(fallenSnowListItem->snowHeight)) * w);
    fallenSnowListItem->maxSnowHeight = (short int *)
        malloc(sizeof(*(fallenSnowListItem->maxSnowHeight)) * w);

    // Fill arrays.
    for (int i = 0; i < w; i++) {
        fallenSnowListItem->columnColor[i] =
            getNextFlakeColorAsRGB();
        fallenSnowListItem->snowHeight[i] = 0;
        fallenSnowListItem->maxSnowHeight[i] = h;
    }

    CreateDesh(fallenSnowListItem);

    fallenSnowListItem->next = *fallenSnowArray;
    *fallenSnowArray = fallenSnowListItem;
}

/** *********************************************************************
 ** This method pops a node from the start of the list.
 **/
void popFallenSnowItem(FallenSnow** list) {
    if (*list == NULL) {
        return;
    }

    FallenSnow* next_node = (*list)->next;
    freeFallenSnowItem(*list);

    *list = next_node;
}

/** *********************************************************************
 ** This method ...
 **/
void freeFallenSnowItem(FallenSnow* fallen) {
    free(fallen->columnColor);
    free(fallen->snowHeight);
    free(fallen->maxSnowHeight);

    cairo_surface_destroy(fallen->surface);
    cairo_surface_destroy(fallen->surface1);

    free(fallen);
}

/** *********************************************************************
 ** This method ...
 **/
void CreateDesh(FallenSnow *p) {
    #define N 6

    // threads: locking by caller
    int i;
    int w = p->w;
    int h = p->h;
    int id = p->winInfo.window;
    short int *maxSnowHeight = p->maxSnowHeight;

    double splinex[N];
    double spliney[N];

    randomuniqarray(splinex, N, 0.0000001, NULL);
    for (i = 0; i < N; i++) {
        splinex[i] *= (w - 1);
        spliney[i] = drand48();
    }

    splinex[0] = 0;
    splinex[N - 1] = w - 1;
    if (id == 0) { // bottom
        spliney[0] = 1.0;
        spliney[N - 1] = 1.0;
    } else {
        spliney[0] = 0;
        spliney[N - 1] = 0;
    }

    double *x = (double *) malloc(w * sizeof(double));
    double *y = (double *) malloc(w * sizeof(double));
    for (i = 0; i < w; i++) {
        x[i] = i;
    }

    spline_interpol(splinex, N, spliney, x, w, y);
    for (i = 0; i < w; i++) {
        maxSnowHeight[i] = h * y[i];
        if (maxSnowHeight[i] < 2) {
            maxSnowHeight[i] = 2;
        }
    }

    free(x);
    free(y);
}

/** *********************************************************************
 ** This method ...
 **/
// change to desired heights
int do_change_deshes() {
    static int lockcounter;
    if (softLockFallenSnowBaseSemaphore(3, &lockcounter)) {
        return TRUE;
    }

    FallenSnow *fsnow = mGlobal.FsnowFirst;
    while (fsnow) {
        CreateDesh(fsnow);
        fsnow = fsnow->next;
    }

    unlockFallenSnowSemaphore();
    return TRUE;
}

/** *********************************************************************
 ** This method ...
 **/
int do_adjust_deshes(__attribute__((unused))void* dummy) {
    // TODO: threads: probably no need for
    // lock, but to be sure:
    lockFallenSnowSemaphore();

    FallenSnow *fsnow = mGlobal.FsnowFirst;
    while (fsnow) {
        int i;
        int adjustments = 0;
        for (i = 0; i < fsnow->w; i++) {
            int d = fsnow->snowHeight[i] - fsnow->maxSnowHeight[i];
            if (d > 0) {
                int c = 1;
                adjustments++;
                fsnow->snowHeight[i] -= c;
            }
        }
        P("adjustments: %d\n", adjustments);
        fsnow = fsnow->next;
    }

    unlockFallenSnowSemaphore();
    return TRUE;
}

/** *********************************************************************
 ** This method ...
 **/
// threads: locking by caller
void createFallenSnowDisplayArea(FallenSnow* fsnow) {
    cairo_t* cr = cairo_create(fsnow->surface1);

    short int *snowHeight = fsnow->snowHeight;

    cairo_set_antialias(cr, CAIRO_ANTIALIAS_DEFAULT);
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);

    // Clear surface1.
    cairo_save(cr);
    cairo_set_source_rgba(cr, 0, 0, 0, 0);
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_paint(cr);
    cairo_restore(cr);

    // MAIN SPLINE adjustment loop.
    // Compute averages for 10 points, draw spline through them
    // and use that to draw fallensnow
    const int fallenSnowItemWidth = fsnow->w;
    const int fallenSnowItemHeight = fsnow->h;

    const int NUMBER_OF_POINTS_FOR_AVERAGE = 10;
    const int NUMBER_OF_AVERAGE_POINTS = MINIMUM_SPLINE_WIDTH +
        (fallenSnowItemWidth - 2) / NUMBER_OF_POINTS_FOR_AVERAGE;

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
            averageSnowHeight += snowHeight[NUMBER_OF_POINTS_FOR_AVERAGE * i + j];
        }

        averageHeightList[i + 1] = averageSnowHeight / NUMBER_OF_POINTS_FOR_AVERAGE;
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
    for (int i = mk; i < fallenSnowItemWidth; i++) {
        averageSnowHeight += snowHeight[i];
    }

    averageHeightList[k + 1] = averageSnowHeight / (fallenSnowItemWidth - mk);
    averageXPosList[k + 1] = mk + 0.5 * (fallenSnowItemWidth - mk - 1);

    // Desktop.
    if (fsnow->winInfo.window == None) {
        averageHeightList[NUMBER_OF_AVERAGE_POINTS - 1] =
            averageHeightList[NUMBER_OF_AVERAGE_POINTS - 2];
    } else {
        averageHeightList[NUMBER_OF_AVERAGE_POINTS - 1] = 0;
    }
    averageXPosList[NUMBER_OF_AVERAGE_POINTS - 1] =
        fallenSnowItemWidth - 1;

    cairo_set_line_width(cr, 1);
    cairo_set_antialias(cr, CAIRO_ANTIALIAS_DEFAULT);

    // GSL - GNU Scientific Library for graph draws.
    // Accelerator object, a kind of iterator.
    gsl_interp_accel* accelerator = gsl_interp_accel_alloc();
    gsl_spline* spline = gsl_spline_alloc(SPLINE_INTERP,
        NUMBER_OF_AVERAGE_POINTS);
    gsl_spline_init(spline, averageXPosList, averageHeightList,
        NUMBER_OF_AVERAGE_POINTS);

    enum { SEARCHING, drawing };
    int state = SEARCHING;
    int foundDrawPosition;

    cairo_set_source_rgb(cr, fsnow->columnColor[0].red,
        fsnow->columnColor[0].green, fsnow->columnColor[0].blue);

    for (int i = 0; i < fallenSnowItemWidth; ++i) {

        int nextValue = gsl_spline_eval(spline, i, accelerator);

        switch (state) {
            case SEARCHING:
                if (nextValue != 0) {
                    foundDrawPosition = i;
                    cairo_move_to(cr, i, fallenSnowItemHeight);
                    cairo_line_to(cr, i, fallenSnowItemHeight);
                    cairo_line_to(cr, i, fallenSnowItemHeight - nextValue);
                    state = drawing;
                }
                break;

            case drawing:
                cairo_line_to(cr, i, fallenSnowItemHeight - nextValue);
                if (nextValue == 0 || i == fallenSnowItemWidth - 1) {
                    cairo_line_to(cr, i, fallenSnowItemHeight);
                    cairo_line_to(cr, foundDrawPosition, fallenSnowItemHeight);
                    cairo_close_path(cr);
                    cairo_stroke_preserve(cr);
                    cairo_fill(cr);
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
 ** This method ...
 **/
void cairoDrawAllFallenSnowItems(cairo_t *cr) {
    if (!WorkspaceActive() || Flags.NoSnowFlakes ||
        (Flags.NoKeepSnowOnWindows && Flags.NoKeepSnowOnBottom)) {
        return;
    }

    lockFallenSnowSwapSemaphore();

    FallenSnow *fsnow = mGlobal.FsnowFirst;
    while (fsnow) {
        if (canSnowCollectOnWindowOrScreenBottom(fsnow)) {
            cairo_set_source_surface(cr, fsnow->surface,
                fsnow->x, fsnow->y - fsnow->h);
            my_cairo_paint_with_alpha(cr, ALPHA);

            fsnow->prevx = fsnow->x;
            fsnow->prevy = fsnow->y - fsnow->h + 1;

            fsnow->prevw = cairo_image_surface_get_width(
                fsnow->surface);
            fsnow->prevh = fsnow->h;
        }

        fsnow = fsnow->next;
    }

    unlockFallenSnowSwapSemaphore();
}

/** *********************************************************************
 ** This method ...
 **/
void eraseFallenSnowAtPixel(FallenSnow *fsnow, int x) {
    // threads: locking by caller
    if (fsnow->snowHeight[x] <= 0) {
        return;
    }

    if (!mGlobal.isDoubleBuffered) {
        clearDisplayArea(mGlobal.display, mGlobal.SnowWin,
            fsnow->x + x, fsnow->y - fsnow->snowHeight[x],
            1, 1, mGlobal.xxposures);
    }

    fsnow->snowHeight[x]--;
}

/** *********************************************************************
 ** This method ...
 **/
void eraseFallenSnowOnDisplay(FallenSnow *fsnow, int xstart, int w) {
    if (mGlobal.isDoubleBuffered) {
        return;
    }

    int x = fsnow->prevx;
    int y = fsnow->prevy;
    if (mGlobal.isDoubleBuffered) {
        return;
    }

    clearDisplayArea(mGlobal.display, mGlobal.SnowWin,
        x + xstart, y, w, fsnow->h + mGlobal.MaxFlakeHeight,
        mGlobal.xxposures);
}

/** *********************************************************************
 ** This method ...
 **/
FallenSnow* findFallenSnowItemByWindow(FallenSnow* first,
    Window window) {
    FallenSnow* fallenSnowItem = first;

    while (fallenSnowItem) {
        if (fallenSnowItem->winInfo.window == window) {
            return fallenSnowItem;
        }
        fallenSnowItem = fallenSnowItem->next;
    }

    return NULL;
}

/** *********************************************************************
 ** This method ...
 **/
void drawFallenSnowItem(FallenSnow* fsnow) {
    const int SNOW_TO_PLOW = 1;

    if (fsnow->winInfo.window == None ||
        (!fsnow->winInfo.hidden && (fsnow->winInfo.sticky ||
            isFallenSnowOnVisibleWorkspace(fsnow)))) {

        // do not interfere with Santa
        if (!Flags.NoSanta && mGlobal.SantaPlowRegion) {
            const bool isIn = XRectInRegion(mGlobal.SantaPlowRegion,
                fsnow->x, fsnow->y - fsnow->h, fsnow->w, fsnow->h);

            // Determine front & back of Santa in fsnow.
            // Santa can back of Santa in fsnow. Santa can
            if (isIn == RectangleIn || isIn == RectanglePart) {
                const int santaFront = (mGlobal.SantaDirection == 0) ?
                    mGlobal.SantaX + mGlobal.SantaWidth - fsnow->x :
                    mGlobal.SantaX - fsnow->x;
                const int santaRear = (mGlobal.SantaDirection == 0) ?
                    santaFront - mGlobal.SantaWidth :
                    santaFront + mGlobal.SantaWidth;

                float vy = -1.5 * mGlobal.ActualSantaSpeed;
                if (vy > 0) {
                    vy = -vy;
                }
                if (vy < -100.0) {
                    vy = -100;
                }

                if (mGlobal.ActualSantaSpeed > 0) {
                    if (mGlobal.SantaDirection == 0) {
                        generateFallenSnowFlakes(fsnow, santaFront,
                            SNOW_TO_PLOW, vy, true);
                        eraseFallenSnowOnDisplay(fsnow, santaRear -
                            SNOW_TO_PLOW, mGlobal.SantaWidth + 2 *
                            SNOW_TO_PLOW);
                    } else {
                        generateFallenSnowFlakes(fsnow, santaFront -
                            SNOW_TO_PLOW, SNOW_TO_PLOW, vy, true);
                        eraseFallenSnowOnDisplay(fsnow, santaRear +
                            SNOW_TO_PLOW, mGlobal.SantaWidth + 2 *
                            SNOW_TO_PLOW);
                    }
                }

                if (mGlobal.SantaDirection == 0) {
                    for (int i = 0; i < fsnow->w; i++) {
                        if (i < santaFront + SNOW_TO_PLOW &&
                            i >= santaRear - SNOW_TO_PLOW) {
                            fsnow->snowHeight[i] = 0;
                        }
                    }
                } else {
                    int i;
                    for (i = 0; i < fsnow->w; i++) {
                        if (i > santaFront - SNOW_TO_PLOW &&
                            i <= santaRear + SNOW_TO_PLOW) {
                            fsnow->snowHeight[i] = 0;
                        }
                    }
                }

                XFlush(mGlobal.display);
            }
        }

        createFallenSnowDisplayArea(fsnow);
    }
}

/** *********************************************************************
 ** This method ...
 **/
void swapFallenSnowListItemSurfaces() {
    lockFallenSnowSwapSemaphore();

    FallenSnow* fsnow = mGlobal.FsnowFirst;
    while (fsnow) {
        cairo_surface_t* tempSurface = fsnow->surface1;
        fsnow->surface1 = fsnow->surface;
        fsnow->surface = tempSurface;

        fsnow = fsnow->next;
    }

    unlockFallenSnowSwapSemaphore();
}

/** *********************************************************************
 ** This method ...
 **/
// clean area for fallensnow with id
void eraseFallenSnowListItem(Window window) {
    FallenSnow* fsnow = mGlobal.FsnowFirst;
    while (fsnow) {
        if (fsnow->winInfo.window == window) {
            eraseFallenSnowOnDisplay(fsnow, 0, fsnow->w);
            break;
        }
        fsnow = fsnow->next;
    }
}

/** *********************************************************************
 ** This method ...
 **/
// remove by id
int removeFallenSnowListItem(FallenSnow **list, Window id) {
    if (*list == NULL) {
        return 0;
    }

    // Do we hit on first item in List?
    FallenSnow *fallen = *list;
    if (fallen->winInfo.window == id) {
        fallen = fallen->next;
        freeFallenSnowItem(*list);
        *list = fallen; // ?
        return 1;
    }

    // Loop through remainder of list items.
    FallenSnow* scratch = NULL;
    while (true) {
        if (fallen->next == NULL) {
            return 0;
        }

        scratch = fallen->next;
        if (scratch->winInfo.window == id) {
            break;
        }

        fallen = fallen->next;
    }

    fallen->next = scratch->next;
    freeFallenSnowItem(scratch);

    return 1;
}

/** *********************************************************************
 ** This method removes all FallenSnow after a window moves out from
 ** under it but we don't know which one it was.
 **/
void removeAllFallenSnowWindows() {
    WinInfo* winInfoListItem = mGlobal.mWinInfoList;
    for (int i = 0; i < mGlobal.mWinInfoListLength; i++) {
        removeFallenSnowFromWindow(winInfoListItem[i].window);
    }
}

/** *********************************************************************
 ** This method removes FallenSnow after a window moves out from
 ** under it.
 **/
void removeFallenSnowFromWindow(Window window) {
    FallenSnow* fallenListItem = findFallenSnowItemByWindow(
        mGlobal.FsnowFirst, window);
    if (!fallenListItem) {
        return;
    }

    lockFallenSnowSemaphore();

    eraseFallenSnowOnDisplay(fallenListItem, 0,
        fallenListItem->w);

    generateFallenSnowFlakes(fallenListItem, 0,
        fallenListItem->w, -10.0, false);

    eraseFallenSnowListItem(fallenListItem->winInfo.window);
    removeFallenSnowListItem(&mGlobal.FsnowFirst,
        fallenListItem->winInfo.window);

    unlockFallenSnowSemaphore();
}

/** *********************************************************************
 ** This method ...
 **/
void updateFallenSnowRegionsWithLock() {
    lockFallenSnowSemaphore();
    updateFallenSnowRegions();
    unlockFallenSnowSemaphore();
}

/** *********************************************************************
 ** This method ...
 **/
void updateFallenSnowRegions() {
    FallenSnow *fsnow;

    // add fallensnow regions:
    WinInfo* addWin = mGlobal.mWinInfoList;
    for (int i = 0; i < mGlobal.mWinInfoListLength; i++) {
        fsnow = findFallenSnowItemByWindow(mGlobal.FsnowFirst, addWin->window);
        if (fsnow) {
            fsnow->winInfo = *addWin;
            if ((!fsnow->winInfo.sticky) &&
                fsnow->winInfo.ws != mGlobal.CWorkSpace) {
                eraseFallenSnowOnDisplay(fsnow, 0, fsnow->w);
            }
        }

        // window found in mWinInfoList, but not in list of fallensnow,
        // add it, but not if we are snowing or birding in this window
        // (Desktop for example) and also not if this window has y <= 0
        // and also not if this window is a "dock"
        if (!fsnow) {
            if (addWin->window != mGlobal.SnowWin &&
                addWin->y > 0 && !(addWin->dock)) {
                if ((int) (addWin->w) == mGlobal.SnowWinWidth &&
                    addWin->x == 0 && addWin->y < 100) {
                    continue;
                }

                // Avoid adding new regions as windows drag realtime.
                if (isWindowBeingDragged()) {
                    continue;
                }

                pushFallenSnowItem(&mGlobal.FsnowFirst, addWin,
                    addWin->x + Flags.OffsetX, addWin->y + Flags.OffsetY,
                    addWin->w + Flags.OffsetW, Flags.MaxWinSnowDepth);
            }
        }

        addWin++;
    }

    // Count fallensnow regions.
    FallenSnow* tempFallen = mGlobal.FsnowFirst;
    int numberFallen = 0;
    while (tempFallen) {
        numberFallen++;
        tempFallen = tempFallen->next;
    }

    // Allocate + 1, prevent allocation of zero bytes.
    int ntoremove = 0;
    long int* toremove = (long int*)
        malloc(sizeof(*toremove) * (numberFallen + 1));

    // Determine fallen regions to be removed.
    fsnow = mGlobal.FsnowFirst;
    while (fsnow) {
        if (fsnow->winInfo.window != 0) {
            // Test if fsnow->winInfo.window has become hidden.
            if (fsnow->winInfo.hidden) {
                eraseFallenSnowOnDisplay(fsnow, 0, fsnow->w);
                generateFallenSnowFlakes(fsnow, 0, fsnow->w, -10.0, false);
                toremove[ntoremove++] = fsnow->winInfo.window;
                fsnow = fsnow->next;
                continue;
            }

            // Test if fsnow->winInfo.window has become closed.
            WinInfo* removeWin = findWinInfoByWindowId(
                fsnow->winInfo.window);
            if (!removeWin) {
                generateFallenSnowFlakes(fsnow, 0, fsnow->w, -10.0, false);
                toremove[ntoremove++] = fsnow->winInfo.window;
                fsnow = fsnow->next;
                continue;
            }

            // Do we have a large window?
            const float WIN_WIDTH_LIMIT =
                mGlobal.SnowWinWidth * 0.8;
            if (removeWin->w <= WIN_WIDTH_LIMIT) {
                fsnow = fsnow->next;
                continue;
            }

            // Large Window Ignores - Top.
            if (removeWin->ya < Flags.IgnoreTop) {
                generateFallenSnowFlakes(fsnow, 0, fsnow->w, -10.0, false);
                toremove[ntoremove++] = fsnow->winInfo.window;
                fsnow = fsnow->next;
                continue;
            }

            // Large Window Ignores - Bottom.
            const int REMOVE_POS = (int) mGlobal.SnowWinHeight -
                removeWin->ya;
            if (REMOVE_POS < Flags.IgnoreBottom) {
                generateFallenSnowFlakes(fsnow, 0, fsnow->w, -10.0, false);
                toremove[ntoremove++] = fsnow->winInfo.window;
                fsnow = fsnow->next;
                continue;
            }
        }

        fsnow = fsnow->next;
    }

    // Test if window has been moved or resized.
    WinInfo* movedWin = mGlobal.mWinInfoList;
    for (int i = 0; i < mGlobal.mWinInfoListLength; i++) {
        fsnow = findFallenSnowItemByWindow(mGlobal.FsnowFirst, movedWin->window);
        if (fsnow) {
            if (fsnow->x != movedWin->x + Flags.OffsetX ||
                fsnow->y != movedWin->y + Flags.OffsetY ||
                (unsigned int) fsnow->w != movedWin->w + Flags.OffsetW) {

                eraseFallenSnowOnDisplay(fsnow, 0, fsnow->w);
                generateFallenSnowFlakes(fsnow, 0, fsnow->w, -10.0, false);

                toremove[ntoremove++] = fsnow->winInfo.window;

                fsnow->x = movedWin->x + Flags.OffsetX;
                fsnow->y = movedWin->y + Flags.OffsetY;
                XFlush(mGlobal.display);
            }
        }

        movedWin++;
    }

    // Remove all identified fallen regions.
    for (int i = 0; i < ntoremove; i++) {
        eraseFallenSnowListItem(toremove[i]);
        removeFallenSnowListItem(&mGlobal.FsnowFirst, toremove[i]);
    }
    free(toremove);
}

/** *********************************************************************
 ** This method ...
 **/
// print list
void logAllFallenSnowDisplayAreas(FallenSnow *list) {
    FallenSnow *fallen = list;

    while (fallen != NULL) {
        int sumact = 0;
        int i;
        for (i = 0; i < fallen->w; i++) {
            sumact += fallen->snowHeight[i];
        }
        printf("id:%#10lx ws:%4ld x:%6d y:%6d w:%6d sty:%2d hid:%2d sum:%8d\n",
            fallen->winInfo.window, fallen->winInfo.ws, fallen->x, fallen->y, fallen->w,
            fallen->winInfo.sticky, fallen->winInfo.hidden, sumact);
        fallen = fallen->next;
    }
}
