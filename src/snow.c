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

#include "blowoff.h"
#include "clocks.h"
#include "debug.h"
#include "fallensnow.h"
#include "flags.h"
#include "hashtable.h"
#include "ixpm.h"
#include "pixmaps.h"
#include "safe_malloc.h"
#include "scenery.h"
#include "snow.h"
#include "treesnow.h"
#include "ui.h"
#include "utils.h"
#include "wind.h"
#include "windows.h"


/***********************************************************
 * Externally provided to this Module.
 */

// Color Picker methods.
bool isQPickerActive();
char* getQPickerCallerName();
bool isQPickerVisible();
bool isQPickerTerminated();

int getQPickerRed();
int getQPickerBlue();
int getQPickerGreen();

void endQPickerDialog();


/***********************************************************
 * Module Method stubs.
 */

static int do_genflakes();

static void InitFlake(SnowFlake *flake);
static void InitFlakesPerSecond(void);
static void InitSnowColor(void);
static void InitSnowSpeedFactor(void);

static void init_snow_pix(void);

static void genxpmflake(char ***xpm, int w, int h);
static void add_random_flakes(int n);
static int  do_SwitchFlakes();

static void DelFlake(SnowFlake *flake);
static void EraseSnowFlake1(SnowFlake *flake);

static void SetSnowSize(void);

static void checkIfFlakeCollectsInFallenSnow(SnowFlake* flake,
    int xPosition, int yPosition, int flakeWidth);

static int do_UpdateSnowFlake(SnowFlake* flake);


/** *********************************************************************
 ** Module globals and consts.
 **/

#define NOTACTIVE (!WorkspaceActive() || Flags.NoSnowFlakes)
#define EXTRA_FLAKES 300

static const float LocalScale = 0.8;

static float FlakesPerSecond;

// 1: signal to flakes to kill themselves,
// & do not generate flakes.
static int KillFlakes = 0;

static float SnowSpeedFactor;

static SnowMap *snowPix;

static char ***plasmasnow_xpm = NULL;

static int NFlakeTypesVintage;
static int MaxFlakeTypes;

// Flake color helper methods.
GdkRGBA mFlakeColor;
int mFlakeColorToggle = 0;


/** *********************************************************************
 ** Init.
 **/
void snow_init() {
    int i;

    MaxFlakeTypes = 0;
    while (snow_xpm[MaxFlakeTypes]) {
        MaxFlakeTypes++;
    }
    NFlakeTypesVintage = MaxFlakeTypes;

    add_random_flakes(EXTRA_FLAKES); // will change MaxFlakeTypes
    //                            and create plasmasnow_xpm, containing
    //                            vintage and new flakes

    snowPix = (SnowMap *)malloc(MaxFlakeTypes * sizeof(SnowMap));

    P("MaxFlakeTypes: %d\n", MaxFlakeTypes);

    for (i = 0; i < MaxFlakeTypes; i++) {
        snowPix[i].surface = NULL;
    }

    // init_snow_surfaces();
    init_snow_pix();
    InitSnowSpeedFactor();
    InitFlakesPerSecond();
    InitSnowColor();
    InitSnowSpeedFactor();

    addMethodToMainloop(PRIORITY_DEFAULT, time_genflakes, do_genflakes);
    addMethodToMainloop(PRIORITY_DEFAULT, time_switchflakes, do_SwitchFlakes);

    // now we would like to be able to get rid of the snow xpms:
    /*
       for (i=0; i<MaxFlakeTypes; i++)
       xpm_destroy(plasmasnow_xpm[i]);
       free(plasmasnow_xpm);
       */
    // but we cannot: they are needed if user changes color
}

/** *********************************************************************
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

/** *********************************************************************
 ** This method ...
 **/
void snow_ui() {
    UIDO(
        NoSnowFlakes, if (Flags.NoSnowFlakes) { clearGlobalSnowWindow(); });

    UIDO(SnowFlakesFactor, InitFlakesPerSecond(););

    UIDOS(SnowColor, InitSnowColor(); clearGlobalSnowWindow(););
    if (isQPickerActive() && !strcmp(getQPickerCallerName(), "SnowColorTAG") &&
        !isQPickerVisible()) {
        static char cbuffer[16];
        snprintf(cbuffer, 16, "#%02x%02x%02x", getQPickerRed(),
            getQPickerGreen(), getQPickerBlue());

        static GdkRGBA color;
        gdk_rgba_parse(&color, cbuffer);
        free(Flags.SnowColor);
        rgba2color(&color, &Flags.SnowColor);

        endQPickerDialog();
    }

    UIDOS(SnowColor2, InitSnowColor(); clearGlobalSnowWindow(););
    if (isQPickerActive() && !strcmp(getQPickerCallerName(), "SnowColor2TAG") &&
        !isQPickerVisible()) {
        static char cbuffer[16];
        snprintf(cbuffer, 16, "#%02x%02x%02x", getQPickerRed(),
            getQPickerGreen(), getQPickerBlue());

        static GdkRGBA color;
        gdk_rgba_parse(&color, cbuffer);
        free(Flags.SnowColor2);
        rgba2color(&color, &Flags.SnowColor2);

        endQPickerDialog();
    }

    UIDO(SnowSpeedFactor, InitSnowSpeedFactor(););
    UIDO(FlakeCountMax, );
    UIDO(SnowSize, SetSnowSize(); Flags.VintageFlakes = 0;);

    static int prev = 100;
    if (appScalesHaveChanged(&prev)) {
        init_snow_pix();
    }
}

/** *********************************************************************
 ** This method ...
 **/
void init_snow_pix() {
    for (int flake = 0; flake < MaxFlakeTypes; flake++) {
        int w, h;
        sscanf(plasmasnow_xpm[flake][0], "%d %d", &w, &h);

        w *= 0.01 * Flags.Scale * LocalScale * mGlobal.WindowScale;
        h *= 0.01 * Flags.Scale * LocalScale * mGlobal.WindowScale;

        SnowMap *rp = &snowPix[flake];
        rp->width = w;
        rp->height = h;

        // Set color, and switch for next.
        char** data;
        int lines;
        xpm_set_color(plasmasnow_xpm[flake],
            &data, &lines, getNextFlakeColorAsString());

        // Create pixbuf.
        GdkPixbuf *pixbuf = gdk_pixbuf_new_from_xpm_data(
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
        GdkPixbuf *pixbufscaled = gdk_pixbuf_scale_simple(
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

/** *********************************************************************
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

/** *********************************************************************
 ** This method ...
 **/
int snow_draw(cairo_t *cr) {
    if (Flags.NoSnowFlakes) {
        return TRUE;
    }
    P("snow_draw %d\n", counter++);

    set_begin();
    SnowFlake *flake;
    while ((flake = (SnowFlake *) set_next())) {
        P("snow_draw %d %f\n", counter++, ALPHA);
        cairo_set_source_surface(
            cr, snowPix[flake->whatFlake].surface, flake->rx, flake->ry);
        double alpha = ALPHA;

        if (flake->fluff) {
            alpha *= (1 - flake->flufftimer / flake->flufftime);
        }

        if (alpha < 0) {
            alpha = 0;
        }

        if (mGlobal.isDoubleBuffered || !(flake->freeze || flake->fluff)) {
            my_cairo_paint_with_alpha(cr, alpha);
        }

        flake->ix = lrint(flake->rx);
        flake->iy = lrint(flake->ry);
    }
    return TRUE;
}

/** *********************************************************************
 ** This method ...
 **/
int snow_erase(int force) {
    if (!force && Flags.NoSnowFlakes) {
        return TRUE;
    }
    set_begin();
    SnowFlake *flake;
    int n = 0;
    while ((flake = (SnowFlake *)set_next())) {
        EraseSnowFlake1(flake);
        n++;
    }
    P("snow_erase %d %d\n", counter++, n);
    return TRUE;
}

/** *********************************************************************
 ** This method ...
 **/
int do_genflakes() {
    if (Flags.Done) {
        return FALSE;
    }

#define RETURN                                                                 \
    do {                                                                       \
        Prevtime = TNow;                                                       \
        return TRUE;                                                           \
    } while (0)

    static double Prevtime;
    static double sumdt;
    static int first_run = 1;
    double TNow = wallclock();

    if (KillFlakes) {
        RETURN;
    }

    if (NOTACTIVE) {
        RETURN;
    }

    if (first_run) {
        first_run = 0;
        Prevtime = wallclock();
        sumdt = 0;
    }

    double dt = TNow - Prevtime;

    // after suspend or sleep dt could have a strange value
    if (dt < 0 || dt > 10 * time_genflakes) {
        RETURN;
    }
    int desflakes = lrint((dt + sumdt) * FlakesPerSecond);
    P("desflakes: %lf %lf %d %lf %d\n", dt, sumdt, desflakes, FlakesPerSecond,
        mGlobal.FlakeCount);
    if (desflakes ==
        0) { // save dt for use next time: happens with low snowfall rate
        sumdt += dt;
    } else {
        sumdt = 0;
    }

    for (int i = 0; i < desflakes; i++) {
        MakeFlake(-1);
    }

    RETURN;
    // (void) d;

#undef RETURN
}

/** *********************************************************************
 ** This method ...
 **/
// if so: make the flake inactive.
// the bottom pixels of the snowflake are at y = NewY + (height of
// flake) the bottompixels are at x values NewX .. NewX+(width of
// flake)-1
// investigate if flake is in a not-hidden fallensnowarea on current
// workspace.

void checkIfFlakeCollectsInFallenSnow(SnowFlake* flake,
    int xPosition, int yPosition, int flakeWidth) {

    FallenSnow* fsnow = mGlobal.FsnowFirst;
    while (fsnow) {
        if (fsnow->winInfo.hidden) {
            fsnow = fsnow->next;
            continue;
        }
        if (fsnow->winInfo.window != None &&
            !isFallenSnowOnVisibleWorkspace(fsnow) &&
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

        // *******************************
        // Flake hits first FallenSnow & we're done.
        //
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
                    updateFallenSnowPartial(fsnow,
                        xPosition - fsnow->x, flakeWidth);
                }

                if (canSnowCollectOnWindowOrScreenBottom(fsnow)) {
                    fluffify(flake, .9);
                    if (!flake->fluff) {
                        DelFlake(flake);
                    }
                }
                return;
            }
        }

        // Otherwise, loop thru all.
        fsnow = fsnow->next;
    }
}

/** *********************************************************************
 ** This method ...
 **/
int do_UpdateSnowFlake(SnowFlake* flake) {
    if (NOTACTIVE) {
        return TRUE;
    }

    if ((flake->freeze || flake->fluff) && mGlobal.RemoveFluff) {
        P("removefluff\n");
        DelFlake(flake);
        EraseSnowFlake1(flake);
        return FALSE;
    }

    // handle fluff and KillFlakes
    if (KillFlakes || (flake->fluff && flake->flufftimer > flake->flufftime)) {
        DelFlake(flake);
        EraseSnowFlake1(flake);
        return FALSE;
    }

    // NewX/y.
    double FlakesDT = time_snowflakes;

    float NewX = flake->rx + (flake->vx * FlakesDT) * SnowSpeedFactor;
    float NewY = flake->ry + (flake->vy * FlakesDT) * SnowSpeedFactor;

    if (flake->fluff) {
        if (!flake->freeze) {
            flake->rx = NewX;
            flake->ry = NewY;
        }
        flake->flufftimer += FlakesDT;
        return TRUE;
    }

    int fckill = mGlobal.FlakeCount - mGlobal.FluffCount >= Flags.FlakeCountMax;
    if ((fckill && !flake->cyclic &&
            drand48() > 0.3) || // high probability to remove blown-off flake
        (fckill && drand48() > 0.9) // low probability to remove other flakes
    ) {
        fluffify(flake, 0.51);
        return TRUE;
    }

    /** **************************
     * update speed in x Direction
     */
    if (!Flags.NoWind) {
        P("flakevx1: %p %f\n", (void *)flake, flake->vx);

        float f = FlakesDT * flake->wsens / flake->m;
        if (f > 0.9) {
            f = 0.9;
        }
        if (f < -0.9) {
            f = -0.9;
        }

        flake->vx += f * (mGlobal.NewWind - flake->vx);

        static float speedxmaxes[] = { 100.0, 300.0, 600.0, };
        float speedxmax = 2 * speedxmaxes[mGlobal.Wind];

        if (fabs(flake->vx) > speedxmax) {
            P("flakevx:  %p %f %f %f %f %f %f\n", (void *)flake, flake->vx,
                FlakesDT, flake->wsens, mGlobal.NewWind, flake->vx, flake->m);

            if (flake->vx > speedxmax) {
                flake->vx = speedxmax;
            }
            if (flake->vx < -speedxmax) {
                flake->vx = -speedxmax;
            }

            P("vxa: %f\n", flake->vx);
        }
    }

    flake->vy += INITIALYSPEED * (drand48() - 0.4) * 0.1;
    if (flake->vy > flake->ivy * 1.5) {
        flake->vy = flake->ivy * 1.5;
    }

    if (flake->freeze) {
        return TRUE;
    }

    // Flake w/h.
    int flakew = snowPix[flake->whatFlake].width;
    int flakeh = snowPix[flake->whatFlake].height;

    if (flake->cyclic) {
        if (NewX < -flakew) {
            NewX += mGlobal.SnowWinWidth - 1;
        }
        if (NewX >= mGlobal.SnowWinWidth) {
            NewX -= mGlobal.SnowWinWidth;
        }
    } else if (NewX < 0 || NewX >= mGlobal.SnowWinWidth) {
        // not-cyclic flakes die when going left or right out of the window
        DelFlake(flake);
        return FALSE;
    }

    // remove flake if it falls below bottom of screen:
    if (NewY >= mGlobal.SnowWinHeight) {
        DelFlake(flake);
        return FALSE;
    }

    // Flake nx/ny.
    int nx = lrintf(NewX);
    int ny = lrintf(NewY);

    // Determine if non-fluffy-flake touches the fallen snow.
    if (!flake->fluff) {
        lockFallenSnowSemaphore();
        checkIfFlakeCollectsInFallenSnow(flake, nx, ny, flakew);
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
            return TRUE;
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

                SnowFlake *newflake;
                if (Flags.VintageFlakes) {
                    newflake = MakeFlake(0);
                } else {
                    newflake = MakeFlake(-1);
                }

                newflake->freeze = 1;
                newflake->rx = xfound;
                newflake->ry = yfound - snowPix[1].height * 0.3f;
                fluffify(newflake, 8);
                return TRUE;
            }
        }
    }

    flake->rx = NewX;
    flake->ry = NewY;
    return TRUE;
}

/** *********************************************************************
 ** This method creates snowflake from type.
 **
 **    0 < type <= SNOWFLAKEMAXTYPE.
 **/
SnowFlake* MakeFlake(int type) {
    mGlobal.FlakeCount++;
    SnowFlake *flake = (SnowFlake *) malloc(sizeof(SnowFlake));

    // If type < 0, create random type.
    if (type < 0) {
        if (Flags.VintageFlakes) {
            type = drand48() * NFlakeTypesVintage;
        } else {
            type = NFlakeTypesVintage + drand48() *
                (MaxFlakeTypes - NFlakeTypesVintage);
        }
    }

    flake->whatFlake = type;
    InitFlake(flake);

    addMethodWithArgToMainloop(PRIORITY_HIGH, time_snowflakes,
        (GSourceFunc) do_UpdateSnowFlake, flake);

    return flake;
}

/** *********************************************************************
 ** This method ...
 **/
void EraseSnowFlake1(SnowFlake *flake) {
    P("Erasesnowflake1\n");
    if (mGlobal.isDoubleBuffered) {
        return;
    }
    P("Erasesnowflake++++++++\n");
    int x = flake->ix - 1;
    int y = flake->iy - 1;
    int flakew = snowPix[flake->whatFlake].width + 2;
    int flakeh = snowPix[flake->whatFlake].height + 2;
    sanelyCheckAndClearDisplayArea(
        mGlobal.display, mGlobal.SnowWin, x, y, flakew, flakeh, mGlobal.xxposures);
}

/** *********************************************************************
 ** This method ...
 **/
// a call to this function must be followed by 'return FALSE' to remove this
// flake from the g_timeout callback
void DelFlake(SnowFlake *flake) {
    if (flake->fluff) {
        mGlobal.FluffCount--;
    }

    set_erase(flake);
    free(flake);
    mGlobal.FlakeCount--;
}

/** *********************************************************************
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

/** *********************************************************************
 ** This method ...
 **/
void InitFlakesPerSecond() {
    FlakesPerSecond = mGlobal.SnowWinWidth * 0.0015 * Flags.SnowFlakesFactor *
                      0.001 * FLAKES_PER_SEC_PER_PIXEL * SnowSpeedFactor;
    P("snowflakesfactor: %d %f %f\n", Flags.SnowFlakesFactor, FlakesPerSecond,
        SnowSpeedFactor);
}

/** *********************************************************************
 ** This method ...
 **/
void InitSnowColor() {
    init_snow_pix();
}

/** *********************************************************************
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

/** *********************************************************************
 ** This method ...
 **/
int do_initsnow() {
    P("initsnow %d %d\n", mGlobal.FlakeCount, counter++);
    if (Flags.Done) {
        return FALSE;
    }
    // first, kill all snowflakes
    KillFlakes = 1;

    // if FlakeCount != 0, there are still some flakes
    if (mGlobal.FlakeCount > 0) {
        return TRUE;
    }

    // signal that flakes may be generated
    KillFlakes = 0;

    return FALSE; // stop callback
                  // (void) d;
}

/** *********************************************************************
 ** This method ...
 **/
// generate random xpm for flake with dimensions wxh
// the flake will be rotated, so the w and h of the resulting xpm will
// be different from the input w and h.
void genxpmflake(char ***xpm, int w, int h) {
    P("%d genxpmflake %d %d\n", counter++, w, h);
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

    // for some reason, drawing of cairo_surfaces derived from 1x1 xpm slow down
    // the x server terribly. So, to be sure, I demand that none of
    // the dimensions is 1, and that the width is a multiple of 8.
    // Btw: genxpmflake rotates and compresses the original wxh xpm,
    // and sometimes that results in an xpm with both dimensions one.

    // Now, suddenly, nw doesn't seem to matter any more?
    // nw = ((nw-1)/8+1)*8;
    // nw = ((nw-1)/32+1)*32;
    P("%d nw nh: %d %d\n", counter++, nw, nh);
    // Ah! nh should be 2 at least ...
    // nw is not important
    //
    // Ok, I tried some things, and it seems that if both nw and nh are 1,
    // then we have trouble.
    // Some pages on the www point in the same direction.

    if (nw == 1 && nh == 1) {
        nh = 2;
    }

    assert(nh > 0);

    P("allocating %d\n", (nh + 3) * sizeof(char *));
    *xpm = (char **)malloc((nh + 3) * sizeof(char *));
    char **X = *xpm;

    X[0] = (char *)malloc(80 * sizeof(char));
    snprintf(X[0], 80, "%d %d 2 1", nw, nh);

    X[1] = strdup("  c None");
    X[2] = (char *)malloc(80 * sizeof(char));
    snprintf(X[2], 80, "%c c black", c);

    int offset = 3;
    P("allocating %d\n", nw + 1);
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

    P("max: %d %f %f %f %f\n", n, ymin, ymax, xmin, xmax);
    for (i = 0; i < n; i++) {
        X[offset + (int)(ya[i] - ymin)][(int)(xa[i] - xmin)] = c;
        P("%f %f\n", ya[i] - ymin, xa[i] - xmin);
    }
    free(x);
    free(y);
    free(xa);
    free(ya);
}

/** *********************************************************************
 ** This method ...
 **/
void add_random_flakes(int n) {
    int i;
    // create a new array with snow-xpm's:
    if (plasmasnow_xpm) {
        for (i = 0; i < MaxFlakeTypes; i++) {
            xpm_destroy(plasmasnow_xpm[i]);
        }
        free(plasmasnow_xpm);
    }
    if (n < 1) {
        n = 1;
    }
    char ***x;
    x = (char ***)malloc((n + NFlakeTypesVintage + 1) * sizeof(char **));
    int lines;
    // copy Rick's vintage flakes:
    for (i = 0; i < NFlakeTypesVintage; i++) {
        xpm_set_color((char **)snow_xpm[i], &x[i], &lines, "snow");
        // xpm_print((char**)snow_xpm[i]);
    }
    // add n flakes:
    for (i = 0; i < n; i++) {
        int w, h;
        int m = Flags.SnowSize;
        w = m + m * drand48();
        h = m + m * drand48();
        genxpmflake(&x[i + NFlakeTypesVintage], w, h);
    }
    MaxFlakeTypes = n + NFlakeTypesVintage;
    x[MaxFlakeTypes] = NULL;
    plasmasnow_xpm = x;
}

/** *********************************************************************
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

/** *********************************************************************
 ** This method ...
 **/
int do_SwitchFlakes() {
    // (void) d;
    static int prev = 0;
    if (Flags.VintageFlakes != prev) {
        P("SwitchFlakes\n");
        set_begin();
        SnowFlake *flake;
        while ((flake = (SnowFlake *)set_next())) {
            if (Flags.VintageFlakes) {
                flake->whatFlake = drand48() * NFlakeTypesVintage;
            } else {
                flake->whatFlake =
                    NFlakeTypesVintage +
                    drand48() * (MaxFlakeTypes - NFlakeTypesVintage);
            }
        }
        prev = Flags.VintageFlakes;
    }
    return TRUE;
}

/** *********************************************************************
 ** This method ...
 **/
void printflake(SnowFlake *flake) {
    printf("flake: %p rx: %6.0f ry: %6.0f vx: %6.0f vy: %6.0f ws: %6.0f fluff: "
           "%d freeze: %d ftr: %8.3f ft: %8.3f\n",
        (void *)flake, flake->rx, flake->ry, flake->vx, flake->vy, flake->wsens,
        flake->fluff, flake->freeze, flake->flufftimer, flake->flufftime);
}
