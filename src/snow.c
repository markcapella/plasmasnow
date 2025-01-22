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

#include <assert.h>
#include <math.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <gtk/gtk.h>

#include "plasmasnow.h"

#include "Blowoff.h"
#include "clocks.h"
#include "FallenSnow.h"
#include "Flags.h"
#include "hashtable.h"
#include "ixpm.h"
#include "MainWindow.h"
#include "pixmaps.h"
#include "safe_malloc.h"
#include "scenery.h"
#include "snow.h"
#include "treesnow.h"
#include "Utils.h"
#include "wind.h"
#include "windows.h"


/***********************************************************
 * Externally provided to this Module.
 */

// Color Picker methods.
bool isQPickerActive();
char* getQPickerColorTAG();
bool isQPickerVisible();
bool isQPickerTerminated();

int getQPickerRed();
int getQPickerBlue();
int getQPickerGreen();

void endQPickerDialog();


/***********************************************************
 ** Module globals and consts.
 **/
const int EXTRA_FLAKES = 300;

int first_run = 1;
int mKillFlakes = 0;

double mStormBackgroundPrevThreadStart = 0;
double sumdt = 0;

const float mStormScale = 0.8;

float FlakesPerSecond = 0.0;
float SnowSpeedFactor = 0.0;

const float mSpeedMaxValues[] = {
    100.0, 300.0, 600.0
};

SnowMap* snowPix = NULL;
char*** plasmasnow_xpm = NULL;

int MaxFlakeTypes = 0;
int NFlakeTypesVintage = 0;

// Flake color helper methods.
GdkRGBA mFlakeColor;
int mFlakeColorToggle = 0;


/***********************************************************
 ** Init.
 **/
void snow_init() {
    MaxFlakeTypes = 0;
    while (snow_xpm[MaxFlakeTypes]) {
        MaxFlakeTypes++;
    }
    NFlakeTypesVintage = MaxFlakeTypes;

    add_random_flakes(EXTRA_FLAKES);

    snowPix = (SnowMap*) malloc(
        MaxFlakeTypes * sizeof(SnowMap));
    for (int i = 0; i < MaxFlakeTypes; i++) {
        snowPix[i].surface = NULL;
    }

    // init_snow_surfaces();
    init_snow_pix();
    InitSnowSpeedFactor();
    InitFlakesPerSecond();
    InitSnowColor();
    InitSnowSpeedFactor();

    addMethodToMainloop(PRIORITY_DEFAULT,
        time_genflakes, execStormBackgroundThread);
}

/***********************************************************
 ** This method ...
 **/
void SetSnowSize() {
    add_random_flakes(EXTRA_FLAKES);
    // init_snow_surfaces();
    init_snow_pix();
    if (!mGlobal.isDoubleBuffered) {
        clearGlobalSnowWindow();
    }
}

/***********************************************************
 ** This method ...
 **/
void snow_ui() {
    UIDO(NoSnowFlakes,
        if (Flags.NoSnowFlakes) {
            clearGlobalSnowWindow();
        }
    );

    UIDO(SnowFlakesFactor, InitFlakesPerSecond(););

    UIDOS(SnowColor, InitSnowColor(); clearGlobalSnowWindow(););
    if (isQPickerActive() && !strcmp(getQPickerColorTAG(), "SnowColorTAG") &&
        !isQPickerVisible()) {
        static char cbuffer[16];
        snprintf(cbuffer, 16, "#%02x%02x%02x", getQPickerRed(),
            getQPickerGreen(), getQPickerBlue());

        static GdkRGBA color;
        gdk_rgba_parse(&color, cbuffer);
        free(Flags.SnowColor);
        rgba2color(&color, &Flags.SnowColor);

        endQPickerDialog();
        clearAllFallenSnowItems();
        clearGlobalSnowWindow();
    }

    UIDOS(SnowColor2, InitSnowColor(); clearGlobalSnowWindow(););
    if (isQPickerActive() && !strcmp(getQPickerColorTAG(), "SnowColor2TAG") &&
        !isQPickerVisible()) {
        static char cbuffer[16];
        snprintf(cbuffer, 16, "#%02x%02x%02x", getQPickerRed(),
            getQPickerGreen(), getQPickerBlue());

        static GdkRGBA color;
        gdk_rgba_parse(&color, cbuffer);
        free(Flags.SnowColor2);
        rgba2color(&color, &Flags.SnowColor2);

        endQPickerDialog();
        clearAllFallenSnowItems();
        clearGlobalSnowWindow();
    }

    UIDO(SnowSpeedFactor, InitSnowSpeedFactor(););
    UIDO(FlakeCountMax, );
    UIDO(SnowSize, SetSnowSize(); Flags.VintageFlakes = 0;);

    static int prev = 100;
    if (appScalesHaveChanged(&prev)) {
        init_snow_pix();
    }
}

/***********************************************************
 ** This method ...
 **/
void init_snow_pix() {
    for (int flake = 0; flake < MaxFlakeTypes; flake++) {

        int w, h;
        sscanf(plasmasnow_xpm[flake][0], "%d %d", &w, &h);

        w *= 0.01 * Flags.Scale * mStormScale * mGlobal.WindowScale;
        h *= 0.01 * Flags.Scale * mStormScale * mGlobal.WindowScale;

        SnowMap* rp = &snowPix[flake];
        rp->width = w;
        rp->height = h;

        // Set color, and switch for next.
        char** data;
        int lines;
        xpm_set_color(plasmasnow_xpm[flake],
            &data, &lines, getNextFlakeColorAsString());

        // Create pixbuf.
        GdkPixbuf* pixbuf = gdk_pixbuf_new_from_xpm_data(
            (const char**) data);
        xpm_destroy(data);

        // Guard W & H, then create.
        if (w < 1) {
            w = 1;
        }
        if (h < 1) {
            h = 1;
        }
        if (w == 1 && h == 1) {
            h = 2;
        }

        // Destroy & recreate surface.
        GdkPixbuf* pixbufscaled = gdk_pixbuf_scale_simple(
            pixbuf, w, h, GDK_INTERP_HYPER);
        if (rp->surface) {
            cairo_surface_destroy(rp->surface);
        }
        rp->surface = gdk_cairo_surface_create_from_pixbuf(
            pixbufscaled, 0, NULL);
        g_clear_object(&pixbufscaled);

        // Clear pixbuf.
        g_clear_object(&pixbuf);
    }

    mGlobal.fluffpix = &snowPix[MaxFlakeTypes - 1];
}

/***********************************************************
 ** This method is a helper for FlakeColor.
 **/
void setGlobalFlakeColor(GdkRGBA flakeColor) {
    mFlakeColor = flakeColor;
}

GdkRGBA getNextFlakeColorAsRGB() {
    // Toggle color switch.
    mFlakeColorToggle = (mFlakeColorToggle == 0) ? 1 : 0;

    // Update color.
    GdkRGBA nextColor;
    if (mFlakeColorToggle == 0) {
        gdk_rgba_parse(&nextColor, Flags.SnowColor);
    } else {
        gdk_rgba_parse(&nextColor, Flags.SnowColor2);
    }
    setGlobalFlakeColor(nextColor);

    return nextColor;
}

extern GdkRGBA getRGBFromString(char* colorString) {
    GdkRGBA result;
    gdk_rgba_parse(&result, colorString);
    return result;
}

char* getNextFlakeColorAsString() {
    // Toggle color switch.
    mFlakeColorToggle = (mFlakeColorToggle == 0) ? 1 : 0;

    // Update color.
    GdkRGBA nextColor;
    if (mFlakeColorToggle == 0) {
        gdk_rgba_parse(&nextColor, Flags.SnowColor);
    } else {
        gdk_rgba_parse(&nextColor, Flags.SnowColor2);
    }
    setGlobalFlakeColor(nextColor);

    // Return result as string.
    return (mFlakeColorToggle == 0) ?
        Flags.SnowColor : Flags.SnowColor2;
}

/***********************************************************
 ** This method ...
 **/
int snow_draw(cairo_t *cr) {
    if (Flags.NoSnowFlakes) {
        return true;
    }

    set_begin();
    SnowFlake* flake;

    while ((flake = (SnowFlake*) set_next())) {
        cairo_set_source_surface(cr, snowPix[flake->whatFlake].surface,
            flake->rx, flake->ry);

        double alpha = ALPHA;
        if (flake->fluff) {
            alpha *= (1 - flake->flufftimer / flake->flufftime);
        }
        if (alpha < 0) {
            alpha = 0;
        }

        if (mGlobal.isDoubleBuffered ||
            !(flake->freeze || flake->fluff)) {
            my_cairo_paint_with_alpha(cr, alpha);
        }

        flake->ix = lrint(flake->rx);
        flake->iy = lrint(flake->ry);
    }

    return true;
}

/***********************************************************
 ** This method erases all snow flake Storm Items.
 **/
int removeAllStormItemsInItemset() {
    set_begin();

    SnowFlake* flake;
    while ((flake = (SnowFlake*) set_next())) {
        eraseStormItem(flake);
    }

    return true;
}

/***********************************************************
 ** This method ...
 **/
int execStormBackgroundThread() {
    if (Flags.shutdownRequested) {
        return false;
    }

    double TNow = wallclock();
    if (mKillFlakes) {
        mStormBackgroundPrevThreadStart = TNow;
        return true;
    }

    if (!WorkspaceActive() || Flags.NoSnowFlakes) {
        mStormBackgroundPrevThreadStart = TNow;
        return true;
    }

    if (first_run) {
        first_run = 0;
        mStormBackgroundPrevThreadStart = wallclock();
        sumdt = 0;
    }

    double dt = TNow - mStormBackgroundPrevThreadStart;

    // Sanity check. Catches after suspend or sleep.
    // dt could have a strange value
    if (dt < 0 || dt > 10 * time_genflakes) {
        mStormBackgroundPrevThreadStart = TNow;
        return true;
    }

    const int DESIRED_FLAKES =
        lrint((dt + sumdt) * FlakesPerSecond);
    for (int i = 0; i < DESIRED_FLAKES; i++) {
        MakeFlake(-1);
    }
    sumdt = (DESIRED_FLAKES == 0) ?
        sumdt + dt : 0;

    mStormBackgroundPrevThreadStart = TNow;
    return true;
}

/***********************************************************
 ** This method updates window surfaces and / or desktop bottom
 ** if flake drops onto it.
 */
void updateFallenSurfacesWithFlake(SnowFlake* flake,
    int xPosition, int yPosition, int flakeWidth) {

    FallenSnow* fsnow = mGlobal.FsnowFirst;
    while (fsnow) {
        if (fsnow->winInfo.hidden) {
            fsnow = fsnow->next;
            continue;
        }

        if (fsnow->winInfo.window != None &&
            !isFallenSnowVisibleOnWorkspace(fsnow) &&
            !fsnow->winInfo.sticky) {
            fsnow = fsnow->next;
            continue;
        }

        if (xPosition < fsnow->x ||
            xPosition > fsnow->x + fsnow->w ||
            yPosition >= fsnow->y + 2) {
            fsnow = fsnow->next;
            continue;
        }

        // Flake hits first FallenSnow & we're done.
        int istart = xPosition - fsnow->x;
        if (istart < 0) {
            istart = 0;
        }
        int imax = istart + flakeWidth;
        if (imax > fsnow->w) {
            imax = fsnow->w;
        }

        for (int i = istart; i < imax; i++) {
            if (yPosition > fsnow->y - fsnow->snowHeight[i] - 1) {
                if (fsnow->snowHeight[i] < fsnow->maxSnowHeight[i]) {
                    updateFallenSnowWithSnow(fsnow,
                        xPosition - fsnow->x, flakeWidth);
                }

                if (canSnowCollectOnFallen(fsnow)) {
                    fluffify(flake, .9);
                    if (!flake->fluff) {
                        removeStormItemInItemset(flake);
                    }
                }
                return;
            }
        }

        // Otherwise, loop thru all.
        fsnow = fsnow->next;
    }
}

/***********************************************************
 ** This method ...
 **/
int execStormItemBackgroundThread(SnowFlake* flake) {
    if (!WorkspaceActive() || Flags.NoSnowFlakes) {
        return true;
    }

    if ((flake->freeze || flake->fluff) && mGlobal.RemoveFluff) {
        eraseStormItem(flake);
        removeStormItemInItemset(flake);
        return false;
    }

    // handle fluff and mKillFlakes
    if (mKillFlakes || (flake->fluff &&
            flake->flufftimer > flake->flufftime)) {
        eraseStormItem(flake);
        removeStormItemInItemset(flake);
        return false;
    }

    // Look ahead to the flakes new x/y position.
    const double flakesDT = time_snowflakes;
    float newFlakeXPos = flake->rx +
        (flake->vx * flakesDT) * SnowSpeedFactor;
    float newFlakeYPos = flake->ry +
        (flake->vy * flakesDT) * SnowSpeedFactor;

    // Update flake based on "fluff" status.
    if (flake->fluff) {
        if (!flake->freeze) {
            flake->rx = newFlakeXPos;
            flake->ry = newFlakeYPos;
        }
        flake->flufftimer += flakesDT;
        return true;
    }

    // Are we over flake max limit & trying to remove them ?
    const bool shouldKillFlake =
        ((mGlobal.FlakeCount - mGlobal.FluffCount) >=
            Flags.FlakeCountMax);

    // Can we remove them?
    if (shouldKillFlake) {
        if ((!flake->cyclic && drand48() > 0.3) ||
            (drand48() > 0.9)) {
            fluffify(flake, 0.51);
            return true;
        }
    }

    // Update flake speed in X Direction.
    if (!Flags.NoWind) {
        // Calc speed.
        float newXVel = flakesDT *
            flake->wsens / flake->m;

        if (newXVel > 0.9) {
            newXVel = 0.9;
        }
        if (newXVel < -0.9) {
            newXVel = -0.9;
        }

        // Apply speed limits.
        const float xVelMax = 2 * mSpeedMaxValues[mGlobal.Wind];
        flake->vx += newXVel * (mGlobal.NewWind - flake->vx);

        if (fabs(flake->vx) > xVelMax) {
            if (flake->vx > xVelMax) {
                flake->vx = xVelMax;
            }
            if (flake->vx < -xVelMax) {
                flake->vx = -xVelMax;
            }
        }
    }

    // Update flake speed in Y Direction.
    flake->vy += INITIALYSPEED * (drand48() - 0.4) * 0.1;
    if (flake->vy > flake->ivy * 1.5) {
        flake->vy = flake->ivy * 1.5;
    }

    // If flake is frozen, we're done.
    if (flake->freeze) {
        return true;
    }

    // Flake w/h.
    const int flakew = snowPix[flake->whatFlake].width;
    const int flakeh = snowPix[flake->whatFlake].height;

    // Update flake based on "cyclic" status.
    if (flake->cyclic) {
        if (newFlakeXPos < -flakew) {
            newFlakeXPos += mGlobal.SnowWinWidth - 1;
        }
        if (newFlakeXPos >= mGlobal.SnowWinWidth) {
            newFlakeXPos -= mGlobal.SnowWinWidth;
        }
    } else {
        // Non-cyclic means we remove it when it
        // goes left or right out of the window.
        if (newFlakeXPos < 0 ||
            newFlakeXPos >= mGlobal.SnowWinWidth) {
            removeStormItemInItemset(flake);
            return false;
        }
    }

    // Remove flake if it falls below bottom of screen.
    if (newFlakeYPos >= mGlobal.SnowWinHeight) {
        removeStormItemInItemset(flake);
        return false;
    }

    // Flake nx/ny.
    int nx = lrintf(newFlakeXPos);
    int ny = lrintf(newFlakeYPos);

    // Determine if non-fluffy-flake touches the fallen snow.
    if (!flake->fluff) {
        lockFallenSnowSemaphore();
        updateFallenSurfacesWithFlake(flake, nx, ny, flakew);
        unlockFallenSnowSemaphore();
    }

    // longRound fromFloats.
    int x = lrintf(flake->rx);
    int y = lrintf(flake->ry);

    if (mGlobal.Wind != 2 && !Flags.NoKeepSnowOnTrees && !Flags.NoTrees) {
        // check if flake is touching or in gSnowOnTreesRegion
        // if so: remove it

        cairo_rectangle_int_t grec = {x, y, flakew, flakeh};
        cairo_region_overlap_t in =
            cairo_region_contains_rectangle(mGlobal.gSnowOnTreesRegion, &grec);

        if (in == CAIRO_REGION_OVERLAP_PART || in == CAIRO_REGION_OVERLAP_IN) {
            fluffify(flake, 0.4);
            flake->freeze = 1;
            return true;
        }

        // check if flake is touching TreeRegion. If so: add snow to
        // gSnowOnTreesRegion.
        cairo_rectangle_int_t grect = {x, y, flakew, flakeh};
        in = cairo_region_contains_rectangle(mGlobal.TreeRegion, &grect);
        if (in == CAIRO_REGION_OVERLAP_PART) {
            // so, part of the flake is in TreeRegion.
            // For each bottom pixel of the flake:
            //   find out if bottompixel is in TreeRegion
            //   if so:
            //     move upwards until pixel is not in TreeRegion
            //     That pixel will be designated as snow-on-tree
            // Only one snow-on-tree pixel has to be found.

            int found = 0;
            int xfound, yfound;
            for (int i = 0; i < flakew; i++) {
                if (found) {
                    break;
                }

                int ybot = y + flakeh;
                int xbot = x + i;
                cairo_rectangle_int_t grect = {xbot, ybot, 1, 1};

                cairo_region_overlap_t in =
                    cairo_region_contains_rectangle(mGlobal.TreeRegion, &grect);

                // If bottom pixel not in TreeRegion, skip.
                if (in != CAIRO_REGION_OVERLAP_IN) {
                    continue;
                }

                // move upwards, until pixel is not in TreeRegion
                for (int j = ybot - 1; j >= y; j--) {
                    cairo_rectangle_int_t grect = {xbot, j, 1, 1};

                    cairo_region_overlap_t in = cairo_region_contains_rectangle(
                        mGlobal.TreeRegion, &grect);
                    if (in != CAIRO_REGION_OVERLAP_IN) {
                        // pixel (xbot,j) is snow-on-tree
                        found = 1;
                        cairo_rectangle_int_t grec;
                        grec.x = xbot;
                        int p = 1 + drand48() * 3;
                        grec.y = j - p + 1;
                        grec.width = p;
                        grec.height = p;
                        cairo_region_union_rectangle(mGlobal.gSnowOnTreesRegion,
                            &grec);

                        if (Flags.BlowSnow && mGlobal.OnTrees < Flags.MaxOnTrees) {
                            mGlobal.SnowOnTrees[mGlobal.OnTrees].x = grec.x;
                            mGlobal.SnowOnTrees[mGlobal.OnTrees].y = grec.y;
                            mGlobal.OnTrees++;
                        }

                        xfound = grec.x;
                        yfound = grec.y;
                        break;
                    }
                }
            }

            // TODO: Huh? who?
            // Do not erase: this gives bad effects
            // in fvwm-like desktops.
            if (found) {
                flake->freeze = 1;
                fluffify(flake, 0.6);

                SnowFlake* newflake;
                if (Flags.VintageFlakes) {
                    newflake = MakeFlake(0);
                } else {
                    newflake = MakeFlake(-1);
                }

                newflake->rx = xfound;
                newflake->ry = yfound - snowPix[1].height * 0.3f;
                newflake->freeze = 1;
                fluffify(newflake, 8);

                return true;
            }
        }
    }

    flake->rx = newFlakeXPos;
    flake->ry = newFlakeYPos;
    return true;
}

/***********************************************************
 ** This method creates snowflake from type.
 **
 **    0 < type <= SNOWFLAKEMAXTYPE.
 **/
SnowFlake* MakeFlake(int type) {

    static int mDebugSnowWhatFlake = 5;

    mGlobal.FlakeCount++;
    SnowFlake *flake = (SnowFlake *) malloc(sizeof(SnowFlake));

    // If type < 0, create random type.
    if (type < 0) {
        type = Flags.VintageFlakes ?
            drand48() * NFlakeTypesVintage :
            NFlakeTypesVintage + (drand48() *
                (MaxFlakeTypes - NFlakeTypesVintage));
    }
    flake->whatFlake = type;

    // Crashes this way
    if ((int) flake->whatFlake < 0) {
        if (mDebugSnowWhatFlake-- > 0) {
            printf("snow.c: MakeFlake(%lu) "
                "Has invalid negative type : %i.\n",
                (unsigned long) pthread_self(),
                (int) flake->whatFlake);
        }
    }
    if ((int) flake->whatFlake >= MaxFlakeTypes) {
        if (mDebugSnowWhatFlake-- > 0) {
            printf("snow.c: MakeFlake(%lu) "
                "Has invalid positive type : %i.\n",
                (unsigned long) pthread_self(),
                (int) flake->whatFlake);
        }
    }

    InitFlake(flake);

    addMethodWithArgToMainloop(PRIORITY_HIGH, time_snowflakes,
        (GSourceFunc) execStormItemBackgroundThread, flake);

    return flake;
}

/***********************************************************
 ** This method ...
 **/
void eraseStormItem(SnowFlake *flake) {
    if (mGlobal.isDoubleBuffered) {
        return;
    }

    int x = flake->ix - 1;
    int y = flake->iy - 1;
    int flakew = snowPix[flake->whatFlake].width + 2;
    int flakeh = snowPix[flake->whatFlake].height + 2;

    clearDisplayArea(mGlobal.display, mGlobal.SnowWin,
        x, y, flakew, flakeh, mGlobal.xxposures);
}

/***********************************************************
 ** This method ...
 **/
// a call to this function must be followed by 'return false' to remove this
// flake from the g_timeout callback
void removeStormItemInItemset(SnowFlake *flake) {
    if (flake->fluff) {
        mGlobal.FluffCount--;
    }

    set_erase(flake);
    free(flake);
    mGlobal.FlakeCount--;
}

/***********************************************************
 ** This method ...
 **/
void InitFlake(SnowFlake *flake) {
    flake->color = mFlakeColor;

    int flakew = snowPix[flake->whatFlake].width;
    int flakeh = snowPix[flake->whatFlake].height;

    flake->rx = randint(mGlobal.SnowWinWidth - flakew);
    flake->ry = -randint(mGlobal.SnowWinHeight / 10) - flakeh;

    flake->cyclic = 1;
    flake->fluff = 0;
    flake->flufftimer = 0;
    flake->flufftime = 0;

    flake->m = drand48() + 0.1;

    if (Flags.NoWind) {
        flake->vx = 0;
    } else {
        flake->vx = randint(mGlobal.NewWind) / 2;
    }

    flake->ivy = INITIALYSPEED * sqrt(flake->m);
    flake->vy = flake->ivy;

    flake->wsens = drand48() * MAXWSENS;
    flake->testing = 0;
    flake->freeze = 0;

    set_insert(flake);
}

/***********************************************************
 ** This method ...
 **/
void InitFlakesPerSecond() {
    FlakesPerSecond = mGlobal.SnowWinWidth * 0.0015 *
        Flags.SnowFlakesFactor * 0.001 *
        FLAKES_PER_SEC_PER_PIXEL * SnowSpeedFactor;
}

/***********************************************************
 ** This method ...
 **/
void InitSnowColor() {
    init_snow_pix();
}

/***********************************************************
 ** This method ...
 **/
void InitSnowSpeedFactor() {
    if (Flags.SnowSpeedFactor < 10) {
        SnowSpeedFactor = 0.01 * 10;
    } else {
        SnowSpeedFactor = 0.01 * Flags.SnowSpeedFactor;
    }
    SnowSpeedFactor *= SNOWSPEED;
}

/***********************************************************
 ** This method ...
 **/
int setKillFlakes() {
    if (Flags.shutdownRequested) {
        return true;
    }

    // if FlakeCount != 0, there are still some flakes
    if (mGlobal.FlakeCount > 0) {
        // Kill all snowflakes.
        mKillFlakes = 1;
        return true;
    }

    // Flakes may be generated.
    mKillFlakes = 0;
    return false;
}

/***********************************************************
 ** This method ...
 **/
// generate random xpm for flake with dimensions wxh
// the flake will be rotated, so the w and h of the resulting xpm will
// be different from the input w and h.

void genxpmflake(char ***xpm, int w, int h) {
    const char c = '.'; // imposed by xpm_set_color

    int nmax = w * h;
    float *x, *y;

    x = (float *)malloc(nmax * sizeof(float));
    y = (float *)malloc(nmax * sizeof(float));

    int i, j;
    float w2 = 0.5 * w;
    float h2 = 0.5 * h;

    y[0] = 0;
    x[0] = 0; // to have at least one pixel in the centre
    int n = 1;
    for (i = 0; i < h; i++) {
        float yy = i;
        if (yy > h2) {
            yy = h - yy;
        }
        float py = 2 * yy / h;
        for (j = 0; j < w; j++) {
            float xx = j;
            if (xx > w2) {
                xx = w - xx;
            }
            float px = 2 * xx / w;
            float p = 1.1 - (px * py);
            // printf("%d %d %f %f %f %f %f\n",j,i,y,x,px,py,p);
            if (drand48() > p) {
                if (n < nmax) {
                    y[n] = i - w2;
                    x[n] = j - h2;
                    n++;
                }
            }
        }
    }
    // rotate points with a random angle 0 .. pi
    float a = drand48() * 355.0 / 113.0;
    float *xa, *ya;
    xa = (float *)malloc(n * sizeof(float));
    ya = (float *)malloc(n * sizeof(float));

    for (i = 0; i < n; i++) {
        xa[i] = x[i] * cosf(a) - y[i] * sinf(a);
        ya[i] = x[i] * sinf(a) + y[i] * cosf(a);
    }

    float xmin = xa[0];
    float xmax = xa[0];
    float ymin = ya[0];
    float ymax = ya[0];

    for (i = 0; i < n; i++) {
        // smallest xa:
        if (xa[i] < xmin) {
            xmin = xa[i];
        }
        // etc ..
        if (xa[i] > xmax) {
            xmax = xa[i];
        }
        if (ya[i] < ymin) {
            ymin = ya[i];
        }
        if (ya[i] > ymax) {
            ymax = ya[i];
        }
    }

    int nw = ceilf(xmax - xmin + 1);
    int nh = ceilf(ymax - ymin + 1);
    if (nw == 1 && nh == 1) {
        nh = 2;
    }
    assert(nh > 0);

    *xpm = (char **)malloc((nh + 3) * sizeof(char *));
    char **X = *xpm;

    X[0] = (char *)malloc(80 * sizeof(char));
    snprintf(X[0], 80, "%d %d 2 1", nw, nh);

    X[1] = strdup("  c None");
    X[2] = (char *)malloc(80 * sizeof(char));
    snprintf(X[2], 80, "%c c black", c);

    int offset = 3;
    assert(nw >= 0);
    for (i = 0; i < nh; i++) {
        X[i + offset] = (char *)malloc((nw + 1) * sizeof(char));
    }

    for (i = 0; i < nh; i++) {
        for (j = 0; j < nw; j++) {
            X[i + offset][j] = ' ';
        }
        X[i + offset][nw] = 0;
    }

    for (i = 0; i < n; i++) {
        X[offset + (int)(ya[i] - ymin)][(int)(xa[i] - xmin)] = c;
    }

    free(x);
    free(y);
    free(xa);
    free(ya);
}

/***********************************************************
 ** This method ...
 **/
void add_random_flakes(int n) {
    if (n < 1) {
        n = 1;
    }

    // create a new array with snow-xpm's:
    if (plasmasnow_xpm) {
        for (int i = 0; i < MaxFlakeTypes; i++) {
            xpm_destroy(plasmasnow_xpm[i]);
        }
        free(plasmasnow_xpm);
    }

    char ***x;
    x = (char***) malloc((n + NFlakeTypesVintage + 1) * sizeof(char**));
    int lines;

    // copy Rick's vintage flakes:
    for (int i = 0; i < NFlakeTypesVintage; i++) {
        xpm_set_color((char **)snow_xpm[i], &x[i], &lines, "snow");
    }

    // add n flakes:
    for (int i = 0; i < n; i++) {
        int m = Flags.SnowSize;
        int w = m + m * drand48();
        int h = m + m * drand48();
        genxpmflake(&x[i + NFlakeTypesVintage], w, h);
    }

    MaxFlakeTypes = n + NFlakeTypesVintage;
    x[MaxFlakeTypes] = NULL;
    plasmasnow_xpm = x;
}

/***********************************************************
 ** This method ...
 **/
void fluffify(SnowFlake *flake, float t) {
    if (flake->fluff) {
        return;
    }
    flake->fluff = 1;

    flake->flufftimer = 0;
    if (t > 0.01) {
        flake->flufftime = t;
    } else {
        flake->flufftime = 0.01;
    }

    mGlobal.FluffCount++;
}

/***********************************************************
 ** This method ...
 **/
void printflake(SnowFlake *flake) {
    printf("flake: %p rx: %6.0f ry: %6.0f vx: %6.0f vy: %6.0f ws: %6.0f fluff: "
           "%d freeze: %d ftr: %8.3f ft: %8.3f\n",
        (void *)flake, flake->rx, flake->ry, flake->vx, flake->vy, flake->wsens,
        flake->fluff, flake->freeze, flake->flufftimer, flake->flufftime);
}
