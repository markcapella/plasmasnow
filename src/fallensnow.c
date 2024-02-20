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
#include <pthread.h>
#include <semaphore.h>
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "Santa.h"
#include "blowoff.h"
#include "debug.h"
#include "fallensnow.h"
#include "flags.h"
#include "safe_malloc.h"
#include "snow.h"
#include "spline_interpol.h"
#include "utils.h"
#include "wind.h"
#include "windows.h"
#include <X11/Intrinsic.h>
#include <X11/Xlib.h>
#include <gsl/gsl_errno.h>
#include <gsl/gsl_sort.h>
#include <gsl/gsl_spline.h>
#include <gtk/gtk.h>
#include <math.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>


/***********************************************************
 * Module Method stubs.
 */

/***********************************************************
 * Module globals and consts.
 */
GdkRGBA mFlakeColor;

/** ********************************************************
 ** Semaphore & lock helper methods.
 **/
static sem_t swap_sem;
static sem_t fallen_sem;

void initFallenSnowSemaphores() {
    sem_init(&swap_sem, 0, 1);
    sem_init(&fallen_sem, 0, 1);
}

void initFallenSnowModule() {
    initFallenSnowList();

    addMethodToMainloop(PRIORITY_DEFAULT, time_change_bottom, do_change_deshes);
    addMethodToMainloop(PRIORITY_DEFAULT, time_adjust_bottom, do_adjust_deshes);

    static pthread_t thread;
    pthread_create(&thread, NULL, doExecFallenSnowThread, NULL);
}

int lockFallenSnowSwapSemaphore() {
    return sem_wait(&swap_sem);
}
int unlockFallenSnowSwapSemaphore() {
    return sem_post(&swap_sem);
}

int lockFallenSnowBaseSemaphore() {
    return sem_wait(&fallen_sem);
}
int unlockFallenSnowBaseSemaphore() {
    return sem_post(&fallen_sem);
}

/** *********************************************************************
 ** This method lets a caller perform deferred locks.
 **/
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
        sem_wait(&fallen_sem) : sem_trywait(&fallen_sem);

    // Success clears tryCount for next time.
    if (resultCode == 0) {
        *tryCount = 0;
    }

    return resultCode;
}

/** *********************************************************************
 ** This method ...
 **/
void updateFallenSnowAtBottom() {
    // threads: locking by caller
    FallenSnow *fsnow = findFallenSnowListItem(mGlobal.FsnowFirst, 0);
    if (fsnow) {
        fsnow->y = mGlobal.SnowWinHeight;
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
void setMaxScreenSnowDepth() {
    // threads: locking by caller
    mGlobal.MaxScrSnowDepth = Flags.MaxScrSnowDepth;
    if (mGlobal.MaxScrSnowDepth > (int)(mGlobal.SnowWinHeight - SNOWFREE)) {
        printf("** Maximum snow depth set to %d\n",
            mGlobal.SnowWinHeight - SNOWFREE);
        mGlobal.MaxScrSnowDepth = mGlobal.SnowWinHeight - SNOWFREE;
    }
}

/** *********************************************************************
 ** This method ...
 **/
void doFallenSnowUISettingsUpdates() {
    UIDO(MaxWinSnowDepth, initFallenSnowList(); ClearScreen(););
    UIDO(MaxScrSnowDepth, setMaxScreenSnowDepthWithLock(); initFallenSnowList();
         ClearScreen(););
    UIDO(NoKeepSnowOnBottom, initFallenSnowList(); ClearScreen(););
    UIDO(NoKeepSnowOnWindows, initFallenSnowList(); ClearScreen(););
    UIDO(IgnoreTop, );
    UIDO(IgnoreBottom, );
}

/** *********************************************************************
 ** This method runs on a private thread.
 **/
void* doExecFallenSnowThread() {
    while (1) {
        if (Flags.Done) {
            pthread_exit(NULL);
        }

        if (WorkspaceActive() && !Flags.NoSnowFlakes &&
            (!Flags.NoKeepSnowOnWindows || !Flags.NoKeepSnowOnBottom)) {
            lockFallenSnowSemaphore();

            FallenSnow *fsnow = mGlobal.FsnowFirst;
            while (fsnow) {
                if (canSnowCollectOnWindowOrScreenBottom(fsnow)) {
                    drawFallenSnowListItem(fsnow);
                }
                fsnow = fsnow->next;
            }

            XFlush(mGlobal.display);

            swapFallenSnowListItemSurfaces();

            unlockFallenSnowSemaphore();
        }

        usleep((useconds_t)
            (TIME_BETWWEEN_FALLENSNOW_THREADS * 1000000));
    }

    return NULL;
}

/** *********************************************************************
 ** This method ...
 **/
void swapFallenSnowListItemSurfaces() {
    lockFallenSnowSwapSemaphore();

    FallenSnow *fsnow = mGlobal.FsnowFirst;
    while (fsnow) {
        cairo_surface_t *s = fsnow->surface1;
        fsnow->surface1 = fsnow->surface;
        fsnow->surface = s;
        fsnow = fsnow->next;
    }

    unlockFallenSnowSwapSemaphore();
}

/** *********************************************************************
 ** This method ...
 **/
// threads: locking by caller
// insert a node at the start of the list
void PushFallenSnow(
    FallenSnow **first, WinInfo *win, int x, int y, int w, int h) {
    // Too narrow windows results in complications.
    if (w < 3) {
        return;
    }

    // Computing splines etc.
    FallenSnow *p = (FallenSnow *) malloc(sizeof(FallenSnow));
    p->winInfo = *win;

    p->x = x;
    p->y = y;
    p->w = w;
    p->h = h;

    p->prevx = 0;
    p->prevy = 0;
    p->prevw = 10;
    p->prevh = 10;

    p->snowHeight = (short int *) malloc(sizeof(*(p->snowHeight)) * w);
    p->maxSnowHeight = (short int *) malloc(sizeof(*(p->maxSnowHeight)) * w);
    p->color = (GdkRGBA*) malloc(sizeof(*(p->color)) * w);

    p->surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
    p->surface1 = cairo_surface_create_similar(p->surface,
        CAIRO_CONTENT_COLOR_ALPHA, w, h);

    int l = 0;
    for (int i = 0; i < w; i++) {
        p->snowHeight[i] = 0;
        p->maxSnowHeight[i] = h;
        p->color[i] = mFlakeColor;

        static char cbuffer[16];
        snprintf(cbuffer, 16, "#%02x%02x%02x", (int) (mFlakeColor.red*255),
            (int) (mFlakeColor.green*255), (int) (mFlakeColor.blue*255));
        cbuffer[15] = '\x00';
        // fprintf(stdout, "fallensnow: PushFallenSnow() color code : %s\n",
        //     cbuffer);

        if (l++ > h) {
            l = 0;
        }
    }

    CreateDesh(p);

    p->next = *first;
    *first = p;
}

/** *********************************************************************
 ** This method ...
 **/
// pop from list
int PopFallenSnow(FallenSnow **list) {
    // threads: locking by caller
    FallenSnow *next_node = NULL;

    if (*list == NULL) {
        return 0;
    }

    next_node = (*list)->next;
    freeFallenSnowDisplayArea(*list);
    *list = next_node;
    return 1;
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
int do_change_deshes(__attribute__((unused)) void* dummy) {
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
    // threads: probably no need for lock, but to be sure:
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
// remove by id
int removeFallenSnowListItem(FallenSnow **list, Window id) {
    if (*list == NULL) {
        return 0;
    }

    // Do we hit on first item in List?
    FallenSnow *fallen = *list;
    if (fallen->winInfo.window == id) {
        fallen = fallen->next;
        freeFallenSnowDisplayArea(*list);
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
    freeFallenSnowDisplayArea(scratch);

    return 1;
}

/** *********************************************************************
 ** This method ...
 **/
FallenSnow *findFallenSnowListItem(FallenSnow *first, Window id) {
    // threads: locking by caller
    FallenSnow *fsnow = first;
    while (fsnow) {
        if (fsnow->winInfo.window == id) {
            return fsnow;
        }
        fsnow = fsnow->next;
    }
    return NULL;
}

/** *********************************************************************
 ** This method ...
 **/
void fallensnow_draw(cairo_t *cr) {
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

            fsnow->prevw = cairo_image_surface_get_width(fsnow->surface);
            fsnow->prevh = fsnow->h;
        }

        fsnow = fsnow->next;
    }

    unlockFallenSnowSwapSemaphore();
}

/** *********************************************************************
 ** This method ...
 **/
void drawFallenSnowListItem(FallenSnow *fsnow) {
    // threads: locking done by caller
    if (fsnow->winInfo.window == 0 ||
        (!fsnow->winInfo.hidden &&
            //(fsnow->winInfo.ws == mGlobal.CWorkSpace || fsnow->winInfo.sticky)))
            (isFallenSnowOnVisibleWorkspace(fsnow) || fsnow->winInfo.sticky))) {

        // do not interfere with Santa
        if (!Flags.NoSanta) {
            int in = XRectInRegion(mGlobal.SantaPlowRegion, fsnow->x,
                fsnow->y - fsnow->h, fsnow->w, fsnow->h);

            if (in == RectangleIn || in == RectanglePart) {
                // determine front of Santa in fsnow
                int xfront;
                if (mGlobal.SantaDirection == 0) {
                    xfront = mGlobal.SantaX + mGlobal.SantaWidth - fsnow->x;
                } else {
                    xfront = mGlobal.SantaX - fsnow->x;
                }

                // determine back of Santa in fsnow, Santa can move backwards in
                // strong wind
                int xback;
                if (mGlobal.SantaDirection == 0) {
                    xback = xfront - mGlobal.SantaWidth;
                } else {
                    xback = xfront + mGlobal.SantaWidth;
                }

                // clearing determines the amount of generated ploughing snow
                const int clearing = 10;
                float vy = -1.5 * mGlobal.ActualSantaSpeed;
                if (vy > 0) {
                    vy = -vy;
                }
                if (vy < -100.0) {
                    vy = -100;
                }

                // This is cool ! Santa plows window snow !!
                if (mGlobal.ActualSantaSpeed > 0) {
                    if (mGlobal.SantaDirection == 0) {
                        generateFallenSnowFlakes(fsnow, xfront, clearing, vy);
                        eraseFallenSnowOnDisplay(fsnow, xback - clearing,
                            mGlobal.SantaWidth + 2 * clearing);
                    } else {
                        generateFallenSnowFlakes(
                            fsnow, xfront - clearing, clearing, vy);
                        eraseFallenSnowOnDisplay(fsnow, xback + clearing,
                            mGlobal.SantaWidth + 2 * clearing);
                    }
                }

                if (mGlobal.SantaDirection == 0) {
                    int i;
                    for (i = 0; i < fsnow->w; i++) {
                        if (i < xfront + clearing && i >= xback - clearing) {
                            fsnow->snowHeight[i] = 0;
                        }
                    }
                } else {
                    int i;
                    for (i = 0; i < fsnow->w; i++) {
                        if (i > xfront - clearing && i <= xback + clearing) {
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
void eraseFallenSnowOnDisplay(FallenSnow *fsnow, int xstart, int w) {
    if (mGlobal.isDoubleBuffered) {
        return;
    }

    int x = fsnow->prevx;
    int y = fsnow->prevy;
    if (mGlobal.isDoubleBuffered) {
        return;
    }

    sanelyCheckAndClearDisplayArea(mGlobal.display, mGlobal.SnowWin,
        x + xstart, y, w, fsnow->h + mGlobal.MaxFlakeHeight,
        mGlobal.xxposures);
}

/** *********************************************************************
 ** This method ...
 **/
// clean area for fallensnow with id
void eraseFallenSnowOnWindow(Window id) {
    FallenSnow* fsnow = mGlobal.FsnowFirst;
    while (fsnow) {
        if (fsnow->winInfo.window == id) {
            eraseFallenSnowOnDisplay(fsnow, 0, fsnow->w);
            break;
        }
        fsnow = fsnow->next;
    }
}

/** *********************************************************************
 ** This method ...
 **/
// threads: locking by caller
void createFallenSnowDisplayArea(FallenSnow *fsnow) {
    GdkRGBA color;

    cairo_t *cr = cairo_create(fsnow->surface1);

    int h = fsnow->h;
    int w = fsnow->w;

    short int *snowHeight = fsnow->snowHeight;
    int id = fsnow->winInfo.window;

    cairo_set_antialias(cr, CAIRO_ANTIALIAS_DEFAULT);
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);

    gdk_rgba_parse(&color, Flags.SnowColor);
    cairo_set_source_rgb(cr, color.red, color.green, color.blue);

    // clear surface1
    cairo_save(cr);
    cairo_set_source_rgba(cr, 0, 0, 0, 0);
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_paint(cr);
    cairo_restore(cr);

    {
        // compute averages for 10 points, draw spline through them
        // and use that to draw fallensnow

        const int m = 10;
        int nav = 3 + (w - 2) / m;

        double *av = (double *)malloc(nav * sizeof(double));
        double *x = (double *)malloc(nav * sizeof(double));

        int i;
        for (i = 0; i < nav - 3; i++) {
            int j;
            double s = 0;
            for (j = 0; j < m; j++) {
                s += snowHeight[m * i + j];
            }
            av[i + 1] = s / m;
            x[i + 1] = m * i + 0.5 * m;
        }

        x[0] = 0;
        if (id == 0) {
            av[0] = av[1];
        } else {
            av[0] = 0;
        }

        int k = nav - 3;
        int mk = m * k;
        double s = 0;
        for (i = mk; i < w; i++) {
            s += snowHeight[i];
        }

        av[k + 1] = s / (w - mk);
        x[k + 1] = mk + 0.5 * (w - mk - 1);

        if (id == 0) {
            av[nav - 1] = av[nav - 2];
        } else {
            av[nav - 1] = 0;
        }
        x[nav - 1] = w - 1;

        gsl_interp_accel *acc = gsl_interp_accel_alloc();
        gsl_spline *spline = gsl_spline_alloc(SPLINE_INTERP, nav);
        gsl_spline_init(spline, x, av, nav);

        cairo_set_line_width(cr, 1);

        cairo_set_antialias(cr, CAIRO_ANTIALIAS_DEFAULT);

        if (0) {
            cairo_move_to(cr, x[0], h - av[0]);
            for (i = 0; i < w; i++) {
                cairo_line_to(cr, i, h - gsl_spline_eval(spline, i, acc));
            }
            cairo_line_to(cr, w - 1, h);
            cairo_line_to(cr, 0, h);
            cairo_close_path(cr);
            cairo_stroke_preserve(cr);
            cairo_fill(cr);

        } else {
            // draw fallensnow: a move_to, followed by line_to's, followed by
            // close_path and fill. to prevent a permanent bottom snow-line,
            // even if there has no snow fallen on certain parts, only handle
            // regions where snow has been fallen.
            enum { searching, drawing };
            int state = searching; // searching for snowHeight[] > 0
            int i;
            int startpos;
            for (i = 0; i < w; ++i) {
                int val = gsl_spline_eval(spline, i, acc);
                switch (state) {
                case searching:
                    if (val != 0) {
                        startpos = i;
                        cairo_move_to(cr, i, h);
                        cairo_line_to(cr, i, h);
                        cairo_line_to(cr, i, h - val);
                        state = drawing;
                    }
                    break;
                case drawing:
                    cairo_line_to(cr, i, h - val);
                    if (val == 0 || i == w - 1) {
                        cairo_line_to(cr, i, h);
                        cairo_line_to(cr, startpos, h);
                        cairo_close_path(cr);
                        cairo_stroke_preserve(cr);
                        cairo_fill(cr);
                        state = searching;
                    }
                    break;
                }
            }
        }

        if (0) { // draw averages
            int i;

            cairo_save(cr);
            cairo_set_source_rgba(cr, 1, 0, 0, 1);

            for (i = 0; i < nav; i++) {
                cairo_rectangle(cr, x[i], h - av[i] - 4, 4, 4);
            }

            cairo_fill(cr);
            cairo_restore(cr);
        }

        gsl_spline_free(spline);
        gsl_interp_accel_free(acc);

        free(x);
        free(av);
    }

    cairo_destroy(cr);
}

/** *********************************************************************
 ** This method ...
 **/
// threads: locking by caller
void freeFallenSnowDisplayArea(FallenSnow *fallen) {
    free(fallen->snowHeight);
    free(fallen->maxSnowHeight);

    cairo_surface_destroy(fallen->surface);
    cairo_surface_destroy(fallen->surface1);

    free(fallen);
}

/** *********************************************************************
 ** This method ...
 **/
void generateFallenSnowFlakes(FallenSnow *fsnow, int x, int w, float vy) {
    // threads: locking by caller
    if (!Flags.BlowSnow || Flags.NoSnowFlakes) {
        return;
    }

    // animation of fallen fallen snow
    // x-values x..x+w are transformed in flakes, vertical speed vy
    int i;
    int ifirst = x;
    if (ifirst < 0) {
        ifirst = 0;
    }
    if (ifirst > fsnow->w) {
        ifirst = fsnow->w;
    }
    int ilast = x + w;
    if (ilast < 0) {
        ilast = 0;
    }
    if (ilast > fsnow->w) {
        ilast = fsnow->w;
    }

    for (i = ifirst; i < ilast; i += 1) {
        int j;
        for (j = 0; j < fsnow->snowHeight[i]; j++) {
            int k, kmax = BlowOff();
            for (k = 0; k < kmax; k++) {
                float p = 0;
                p = drand48();
                // In X11, (mGlobal.hasTransparentWindow!=1) we want not too much
                // generated flakes
                // Otherwize, we go for more dramatic effects
                // But, it appeared that, if mGlobal.hasTransparentWindow==1, too much snow
                // is generated, choking the x server.
                if (p < 0.15) {
                    SnowFlake *flake = MakeFlake(-1);
                    flake->rx = fsnow->x + i + 16 * (drand48() - 0.5);
                    flake->ry = fsnow->y - j - 8;
                    if (Flags.NoWind) {
                        flake->vx = 0;
                    } else {
                        flake->vx = mGlobal.NewWind / 8;
                    }
                    flake->vy = vy;
                    flake->cyclic = 0;
                }
            }
        }
    }
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
        sanelyCheckAndClearDisplayArea(mGlobal.display, mGlobal.SnowWin,
            fsnow->x + x, fsnow->y - fsnow->snowHeight[x],
            1, 1, mGlobal.xxposures);
    }

    fsnow->snowHeight[x]--;
}

/** *********************************************************************
 ** This method clears and inits the FallenSnow list of items.
 **/
void initFallenSnowList() {
    // Lock, & remove all FallenSnow List Items.
    lockFallenSnowSemaphore();
    while (mGlobal.FsnowFirst) {
        PopFallenSnow(&mGlobal.FsnowFirst);
    }

    // Create FallenSnow item for bottom of screen.
    WinInfo *NullWindow = (WinInfo *) malloc(sizeof(WinInfo));
    memset(NullWindow, 0, sizeof(WinInfo));

    PushFallenSnow(&mGlobal.FsnowFirst, NullWindow,
        0, mGlobal.SnowWinHeight,
        mGlobal.SnowWinWidth, mGlobal.MaxScrSnowDepth);

    // Unlock & exit.
    free(NullWindow);
    unlockFallenSnowSemaphore();
}

/** *********************************************************************
 ** This method ...
 **/
// removes some fallen snow from fsnow, w pixels. If fallensnowheight < h: no
// removal also add snowflakes
void updateFallenSnowWithWind(FallenSnow *fsnow, int w, int h) {
    // threads: locking by caller
    int i;
    int x = randint(fsnow->w - w);
    for (i = x; i < x + w; i++) {
        if (fsnow->snowHeight[i] > h) {
            // animation of blown off snow
            if (!Flags.NoWind && mGlobal.Wind != 0 && drand48() > 0.5) {
                int j, jmax = BlowOff();
                for (j = 0; j < jmax; j++) {
                    SnowFlake *flake = MakeFlake(-1);
                    flake->rx = fsnow->x + i;
                    flake->ry = fsnow->y - fsnow->snowHeight[i] - drand48() * 4;
                    flake->vx = 0.25 * fsignf(mGlobal.NewWind) * mGlobal.WindMax;
                    flake->vy = -10;
                    flake->cyclic =
                        (fsnow->winInfo.window ==
                            0); // not cyclic for Windows, cyclic for bottom
                    P("%d:\n", counter++);
                }
                eraseFallenSnowAtPixel(fsnow, i);
            }
        }
    }
}

/** *********************************************************************
 ** This method ...
 **/
void updateFallenSnowPartial(FallenSnow* fsnow, int position, int width) {
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
    tempHeightArray = (short int *) malloc(sizeof(*tempHeightArray) * (width + 2));

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
            fsnow->snowHeight[i] = amountToRaiseHeight + (tempHeightArray[k - 1] + tempHeightArray[k + 1]) / 2;
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
// If no window, result depends on screen bottom option.
// If window hidden, don't handle.
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
