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
#include "snow.h"
#include "spline_interpol.h"
#include "Utils.h"
#include "WindowVector.h"
#include "wind.h"
#include "windows.h"
#include "WinInfo.h"


/** *********************************************************************
 ** Module globals and consts.
 **/

// Must stay snowfree area on display :)
const int MAX_DESKTOP_SNOWFREE_HEIGHT = 25;
const int MINIMUM_SPLINE_WIDTH = 3;

// Semaphore & lock members.
sem_t mFallenSnowSwapSemaphore;
sem_t mFallenSnowBaseSemaphore;

pthread_t mFallenSnowBackgroundThread;

WindowVector mDeferredWindowRemovesList;


/** *********************************************************************
 ** This method initializes the FallenSnow module.
 **/
void initFallenSnowModule() {
    clearAllFallenSnowItems();

    addMethodToMainloop(PRIORITY_DEFAULT,
        time_change_bottom, do_change_deshes);

    addMethodToMainloop(PRIORITY_DEFAULT,
        time_adjust_bottom, do_adjust_deshes);

    // Start main background thread looper & exit.
    pthread_create(&mFallenSnowBackgroundThread, NULL,
        startFallenSnowBackgroundThread, NULL);
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

    lockFallenSnowSemaphore();

    // Draw all fallen snow areas.
    FallenSnow* fallenSnowItem = mGlobal.FsnowFirst;
    while (fallenSnowItem) {
        if (canSnowCollectOnFallen(fallenSnowItem)) {
            collectSnowOnFallen(fallenSnowItem);
        }
        fallenSnowItem = fallenSnowItem->next;
    }

    XFlush(mGlobal.display);
    swapFallenSnowRenderedSurfacesBToA();

    unlockFallenSnowSemaphore();
}

/** *********************************************************************
 ** This method swaps fallen snow rendered areas B <---> A.
 **/
void swapFallenSnowRenderedSurfacesBToA() {
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
void updateFallenSnowUserSettings() {
    UIDO(MaxWinSnowDepth,
        clearAllFallenSnowItems();
        clearGlobalSnowWindow();
    );
    UIDO(MaxScrSnowDepth,
        lockFallenSnowSemaphore();
        updateFallenSnowDesktopItemDepth();
        unlockFallenSnowSemaphore();
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

    // tempHeightArray will contain the snowHeight values
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
            tempHeightArray[k++] = fsnow->snowHeight[0];
        } else if (i >= fsnow->w) {
            tempHeightArray[k++] = fsnow->snowHeight[fsnow->w - 1];
        } else {
            tempHeightArray[k++] = fsnow->snowHeight[i];
        }
    }

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
 ** visible on the current workspace.
 **/
int isFallenSnowVisibleOnWorkspace(FallenSnow* fsnow) {
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
 ** This method collects a fallen snow item from the display &
 ** performs Santa collision detection & actions.
 **/
void collectSnowOnFallen(FallenSnow* fsnow) {
    if (fsnow->winInfo.window == None ||
        (!fsnow->winInfo.hidden && (fsnow->winInfo.sticky ||
        isFallenSnowVisibleOnWorkspace(fsnow)))) {

        // Check for Santa interaction.
        if (!Flags.NoSanta && mGlobal.SantaPlowRegion) {
            updateFallenSnowWithSanta(fsnow);
        }

        renderFallenSnowSurfaceB(fsnow);
    }
}

/** *********************************************************************
 ** This method renders a new fallensnow image onto renderedSurfaceB.
 ** threads: locking by caller
 **/
void renderFallenSnowSurfaceB(FallenSnow* fsnow) {
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

    const short int* FALLEN_SNOW_HEIGHT = fsnow->snowHeight;

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

    cairo_set_source_rgb(cr, fsnow->columnColor[0].red,
        fsnow->columnColor[0].green, fsnow->columnColor[0].blue);

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
 ** This method updates fallensnow items with impact
 ** of Santas sled ploughing.
 **/
void updateFallenSnowWithSanta(FallenSnow* fsnow) {
    const int SNOW_TO_PLOW = 5;

    const bool IS_IT_IN = XRectInRegion(mGlobal.SantaPlowRegion,
        fsnow->x, fsnow->y - fsnow->h, fsnow->w, fsnow->h);
    if (IS_IT_IN != RectangleIn && IS_IT_IN != RectanglePart) {
        return;
    }

    const int SANTA_FRONT = (mGlobal.SantaDirection == 0) ?
        mGlobal.SantaX + mGlobal.SantaWidth - fsnow->x :
        mGlobal.SantaX - fsnow->x;
    const int SANTA_REAR = (mGlobal.SantaDirection == 0) ?
        SANTA_FRONT - mGlobal.SantaWidth :
        SANTA_FRONT + mGlobal.SantaWidth;

    float vy = -1.5 * mGlobal.ActualSantaSpeed;
    if (vy > 0) {
        vy = -vy;
    }
    if (vy < -100.0) {
        vy = -100;
    }

    // Santa plows facing forward.
    if (mGlobal.ActualSantaSpeed > 0) {
        if (mGlobal.SantaDirection == 0) {
            generateFallenSnowFlakes(fsnow, SANTA_FRONT,
                SNOW_TO_PLOW, vy, true);
            eraseFallenSnowPartial(fsnow, SANTA_REAR -
                SNOW_TO_PLOW, mGlobal.SantaWidth + 2 *
                SNOW_TO_PLOW);
            for (int i = 0; i < fsnow->w; i++) {
                if (i < SANTA_FRONT + SNOW_TO_PLOW &&
                    i >= SANTA_REAR - SNOW_TO_PLOW) {
                    fsnow->snowHeight[i] = 0;
                }
            }
        } else {
            generateFallenSnowFlakes(fsnow, SANTA_FRONT -
                SNOW_TO_PLOW, SNOW_TO_PLOW, vy, true);
            eraseFallenSnowPartial(fsnow, SANTA_REAR +
                SNOW_TO_PLOW, mGlobal.SantaWidth + 2 *
                SNOW_TO_PLOW);
            for (int i = 0; i < fsnow->w; i++) {
                if (i > SANTA_FRONT - SNOW_TO_PLOW &&
                    i <= SANTA_REAR + SNOW_TO_PLOW) {
                    fsnow->snowHeight[i] = 0;
                }
            }
        }
    }

    XFlush(mGlobal.display);
}

/** *********************************************************************
 ** This method updates fallensnow items with impact
 ** of wind.
 **/
void updateFallenSnowWithWind(FallenSnow* fsnow,
    int w, int h) {
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
    lockFallenSnowSemaphore();

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

    unlockFallenSnowSemaphore();
}

/** *********************************************************************
 ** This method logs a request fallen snow area.
 **/
void logAllFallenSnowItems() {

    printf("logAllFallenSnowItems() Starts.\n");

    for (FallenSnow* fallen = mGlobal.FsnowFirst;
        fallen; fallen = fallen->next) {

        printf("id: %#10lx  ws: %4ld  x: %6d  y: %6d  "
            "w: %6d  h: %6d  sty: %2d  hid: %2d\n",
            fallen->winInfo.window, fallen->winInfo.ws,
            fallen->x, fallen->y, fallen->w, fallen->h,
            fallen->winInfo.sticky, fallen->winInfo.hidden);
    }
    printf("\n");
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

    fallenSnowListItem->renderedSurfaceA =
        cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
    fallenSnowListItem->renderedSurfaceB =
        cairo_surface_create_similar(fallenSnowListItem->renderedSurfaceA,
        CAIRO_CONTENT_COLOR_ALPHA, w, h);

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
    free(fallen->columnColor);
    free(fallen->snowHeight);
    free(fallen->maxSnowHeight);

    cairo_surface_destroy(fallen->renderedSurfaceA);
    cairo_surface_destroy(fallen->renderedSurfaceB);

    free(fallen);
}

/** *********************************************************************
 ** This method creates a fallen snow items desh.
 ** threads: locking by caller
 **/
void CreateDesh(FallenSnow* fallen) {
    #define N 6

    int w = fallen->w;
    int h = fallen->h;

    int id = fallen->winInfo.window;
    short int *maxSnowHeight = fallen->maxSnowHeight;

    double splinex[N];
    double spliney[N];

    randomuniqarray(splinex, N, 0.0000001, NULL);
    for (int i = 0; i < N; i++) {
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
    for (int i = 0; i < w; i++) {
        x[i] = i;
    }

    double *y = (double *) malloc(w * sizeof(double));
    spline_interpol(splinex, N, spliney, x, w, y);
    free(x);

    for (int i = 0; i < w; i++) {
        maxSnowHeight[i] = h * y[i];
        if (maxSnowHeight[i] < 2) {
            maxSnowHeight[i] = 2;
        }
    }
    free(y);
}

/** *********************************************************************
 ** This method changes a fallen snow items desh.
 **/
int do_change_deshes() {
    static int mBlowOffLockCounter;
    if (softLockFallenSnowBaseSemaphore(3, &mBlowOffLockCounter)) {
        return true;
    }

    FallenSnow* fsnow = mGlobal.FsnowFirst;
    while (fsnow) {
        CreateDesh(fsnow);
        fsnow = fsnow->next;
    }

    unlockFallenSnowSemaphore();
    return true;
}

/** *********************************************************************
 ** This method changes a fallen snow items desh height down.
 **/
int do_adjust_deshes(__attribute__((unused))void* dummy) {
    lockFallenSnowSemaphore();

    FallenSnow *fsnow = mGlobal.FsnowFirst;
    while (fsnow) {
        for (int i = 0; i < fsnow->w; i++) {
            const int HEIGHT_ABOVE_MAX = fsnow->snowHeight[i] -
                fsnow->maxSnowHeight[i];
            if (HEIGHT_ABOVE_MAX > 0) {
                fsnow->snowHeight[i]--;
            }
        }

        fsnow = fsnow->next;
    }

    unlockFallenSnowSemaphore();
    return true;
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

    lockFallenSnowSemaphore();

    generateFallenSnowFlakes(fallenListItem, 0,
        fallenListItem->w, -15.0, false);
    eraseFallenSnowPartial(fallenListItem, 0,
        fallenListItem->w);
    removeAndFreeFallenSnowForWindow(&mGlobal.FsnowFirst,
        fallenListItem->winInfo.window);

    unlockFallenSnowSemaphore();
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
            if (fsnow->winInfo.ws != mGlobal.currentWorkspace &&
                !fsnow->winInfo.sticky) {
                eraseFallenSnowPartial(fsnow, 0, fsnow->w);
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
                eraseFallenSnowPartial(fsnow, 0, fsnow->w);
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
                eraseFallenSnowPartial(fsnow, 0, fsnow->w);
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
                eraseFallenSnowPartial(fsnow, 0, fsnow->w);
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
                eraseFallenSnowPartial(fsnow, 0, fsnow->w);
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
                    eraseFallenSnowPartial(fsnow, 0, fsnow->w);
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
 ** This method generates snow blowoff and drops.
 ** threads: locking by caller
 **/
void generateFallenSnowFlakes(FallenSnow* fsnow,
    int xPos, int xWidth, float vy, bool limitToMax) {
    if (!Flags.BlowSnow || Flags.NoSnowFlakes) {
        return;
    }

    const int xLeftPos = MIN(MAX(0, xPos), fsnow->w);
    const int xRightPos = MIN(MAX(0, xPos + xWidth), fsnow->w);

    const int MAX_FLAKES_TO_GENERATE =
        (int) ((float) Flags.FlakeCountMax * 0.9);

    for (int i = xLeftPos; i < xRightPos; i++) {
        for (int j = 0; j < fsnow->snowHeight[i]; j++) {

            const int kMax = getNumberOfFlakesToBlowoff();
            for (int k = 0; k < kMax; k++) {
                if (drand48() >= 0.15) {
                    continue;
                }

                // Avoid runaway flake generation during plowing.
                if (limitToMax &&
                    mGlobal.FlakeCount >= MAX_FLAKES_TO_GENERATE) {
                    return;
                }

                SnowFlake* flake = MakeFlake(-1);
                flake->cyclic = 0;
                flake->rx = fsnow->x + i + 16 * (drand48() - 0.5);
                flake->ry = fsnow->y - j - 8;
                flake->vx = (Flags.NoWind) ?
                    0 : mGlobal.NewWind / 8;
                flake->vy = vy;
            }
        }
    }
}

/** *********************************************************************
 ** This method erases a fallensnow display area.
 **/
void eraseFallenSnowPartial(FallenSnow* fsnow,
    int xstart, int w) {
    if (mGlobal.isDoubleBuffered) {
        return;
    }

    clearDisplayArea(mGlobal.display, mGlobal.SnowWin,
        fsnow->prevx + xstart, fsnow->prevy,
        w, fsnow->h + mGlobal.MaxFlakeHeight,
        mGlobal.xxposures);
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

    FallenSnow *fsnow = mGlobal.FsnowFirst;
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

/***********************************************************
 * Helper methods for Thread locking.
 */
void initFallenSnowSemaphores() {
    sem_init(&mFallenSnowBaseSemaphore, 0, 1);
    sem_init(&mFallenSnowSwapSemaphore, 0, 1);
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
        sem_wait(&mFallenSnowBaseSemaphore) :
        sem_trywait(&mFallenSnowBaseSemaphore);

    // Success clears tryCount for next time.
    if (resultCode == 0) {
        *tryCount = 0;
    }

    return resultCode;
}

// Swap semaphores.
int lockFallenSnowSwapSemaphore() {
    return sem_wait(&mFallenSnowSwapSemaphore);
}
int unlockFallenSnowSwapSemaphore() {
    return sem_post(&mFallenSnowSwapSemaphore);
}

