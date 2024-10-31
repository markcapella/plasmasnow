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

#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#include <X11/Intrinsic.h>
#include <X11/xpm.h>
#include <gtk/gtk.h>

#include "debug.h"
#include "Flags.h"
#include "ixpm.h"
#include "moon.h"
#include "pixmaps.h"
#include "safe_malloc.h"
#include "Santa.h"
#include "Utils.h"
#include "wind.h"
#include "windows.h"


const float LocalScale = 0.6;

int CurrentSanta;

Pixmap SantaMaskPixmap[PIXINANIMATION];
Pixmap SantaPixmap[PIXINANIMATION];

Region SantaRegion = 0;

float SantaSpeed;
float SantaXr;
float SantaYr;

int SantaYStep;

// Santa when last drawn.
int OldSantaX = 0;
int OldSantaY = 0;

int MoonSeeking = 1;

static cairo_surface_t* Santa_surfaces
    [MAXSANTA + 1][2][2][PIXINANIMATION];

// Speed for each Santa  in pixels/second.
static float Speed[] = {
    SANTASPEED0, SANTASPEED1, SANTASPEED2,
    SANTASPEED3, SANTASPEED4
};


/** *********************************************************************
 ** This method ...
 **/
void Santa_ui() {
    UIDO(SantaSize, SetSantaSizeSpeed(););
    UIDO(Rudolf, SetSantaSizeSpeed(););
    UIDO(NoSanta, );
    UIDO(SantaSpeedFactor, SetSantaSizeSpeed(););
    UIDO(SantaScale, init_Santa_surfaces(); SetSantaSizeSpeed(););

    static int prev = 100;
    if (appScalesHaveChanged(&prev)) {
        init_Santa_surfaces();
        SetSantaSizeSpeed();
    }
}

/** *********************************************************************
 ** This method ...
 **/
int Santa_draw(cairo_t *cr) {
    if (Flags.NoSanta) {
        return TRUE;
    }

    cairo_surface_t *surface;
    surface = Santa_surfaces[Flags.SantaSize][Flags.Rudolf]
                            [mGlobal.SantaDirection][CurrentSanta];
    cairo_set_source_surface(cr, surface, mGlobal.SantaX, mGlobal.SantaY);
    my_cairo_paint_with_alpha(cr, ALPHA);
    OldSantaX = mGlobal.SantaX;
    OldSantaY = mGlobal.SantaY;
    return TRUE;
}

/** *********************************************************************
 ** This method ...
 **/
void Santa_erase(cairo_t* cc) {

    clearDisplayArea(mGlobal.display, mGlobal.SnowWin,
        OldSantaX, OldSantaY,
        mGlobal.SantaWidth + 1, mGlobal.SantaHeight,
        mGlobal.xxposures);
}

/** *********************************************************************
 ** This method ...
 **/
void Santa_init() {
    for (int i = 0; i < PIXINANIMATION; i++) {
        SantaPixmap[i] = 0;
        SantaMaskPixmap[i] = 0;
    }

    for (int i = 0; i < MAXSANTA + 1; i++) {
        for (int j = 0; j < 2; j++) {
            for (int k = 0; k < PIXINANIMATION; k++) {
                Santa_surfaces[i][j][0][k] = NULL;
                Santa_surfaces[i][j][1][k] = NULL;
            }
        }
    }

    SantaRegion = XCreateRegion();
    mGlobal.SantaPlowRegion = XCreateRegion();

    init_Santa_surfaces();
    SetSantaSizeSpeed();

    if (drand48() > 0.5) {
        mGlobal.SantaDirection = 0;
    } else {
        mGlobal.SantaDirection = 1;
    }

    ResetSanta();
    addMethodToMainloop(PRIORITY_HIGH, time_usanta, do_usanta);
}

/** *********************************************************************
 ** This method ...
 **/
void init_Santa_surfaces() {

    for (int i = 0; i < MAXSANTA + 1; i++) {
        for (int j = 0; j < 2; j++) {
            for (int k = 0; k < PIXINANIMATION; k++) {

                int w, h;
                sscanf(Santas[i][j][k][0], "%d %d", &w, &h);
                w *= 0.01 * Flags.Scale * LocalScale * mGlobal.WindowScale *
                     0.01 * Flags.SantaScale;
                h *= 0.01 * Flags.Scale * LocalScale * mGlobal.WindowScale *
                     0.01 * Flags.SantaScale;

                if (w < 1) {
                    w = 1;
                }
                if (h < 1) {
                    h = 1;
                }
                if (w == 1 && h == 1) {
                    h = 2;
                }

                GdkPixbuf* pixbuf = gdk_pixbuf_new_from_xpm_data(
                    (const char**) Santas[i][j][k]);

                GdkPixbuf* pixbufscaled0 = gdk_pixbuf_scale_simple(
                    pixbuf, w, h, GDK_INTERP_HYPER);
                g_clear_object(&pixbuf);

                GdkPixbuf* pixbufscaled1 =
                    gdk_pixbuf_flip(pixbufscaled0, TRUE);

                if (Santa_surfaces[i][j][0][k]) {
                    cairo_surface_destroy(Santa_surfaces[i][j][0][k]);
                    cairo_surface_destroy(Santa_surfaces[i][j][1][k]);
                }

                Santa_surfaces[i][j][0][k] =
                    gdk_cairo_surface_create_from_pixbuf(
                        pixbufscaled0, 0, NULL);
                g_clear_object(&pixbufscaled0);

                Santa_surfaces[i][j][1][k] =
                    gdk_cairo_surface_create_from_pixbuf(
                        pixbufscaled1, 0, NULL);
                g_clear_object(&pixbufscaled1);
            }
        }
    }

    char* path[PIXINANIMATION];
    const char* filenames[] = {
        "plasmasnow/pixmaps/santa1.xpm",
        "plasmasnow/pixmaps/santa2.xpm",
        "plasmasnow/pixmaps/santa3.xpm",
        "plasmasnow/pixmaps/santa4.xpm",
    };

    for (int  i = 0; i < PIXINANIMATION; i++) {
        path[i] = NULL;
        FILE* file = HomeOpen(filenames[i], "r", &path[i]);
        if (!file) {
            if (path[i]) {
                free(path[i]);
            }
            setSantaRegions();
            return;
        }

        fclose(file);
    }

    int rc, i;
    char **santaxpm;

    for (i = 0; i < PIXINANIMATION; i++) {
        rc = XpmReadFileToData(path[i], &santaxpm);

        int w, h;
        sscanf(santaxpm[0], "%d %d", &w, &h);
        w *= 0.01 * Flags.Scale * LocalScale * mGlobal.WindowScale;
        h *= 0.01 * Flags.Scale * LocalScale * mGlobal.WindowScale;

        if (w < 1) {
            w = 1;
        }
        if (h < 1) {
            h = 1;
        }
        if (w == 1 && h == 1) {
            h = 2;
        }

        GdkPixbuf* pixbuf = gdk_pixbuf_new_from_xpm_data(
            (const char**) santaxpm);
        XpmFree(santaxpm);

        GdkPixbuf* pixbufscaled0 =
            gdk_pixbuf_scale_simple(pixbuf, w, h, GDK_INTERP_HYPER);
        g_clear_object(&pixbuf);

        GdkPixbuf* pixbufscaled1 = gdk_pixbuf_flip(pixbufscaled0, TRUE);

        cairo_surface_destroy(Santa_surfaces[0][0][0][i]);
        cairo_surface_destroy(Santa_surfaces[0][0][1][i]);

        Santa_surfaces[0][0][0][i] = gdk_cairo_surface_create_from_pixbuf(
            pixbufscaled0, 0, NULL);
        Santa_surfaces[0][0][1][i] = gdk_cairo_surface_create_from_pixbuf(
            pixbufscaled1, 0, NULL);

        g_clear_object(&pixbufscaled0);
        g_clear_object(&pixbufscaled1);

        free(path[i]);
    }

    Flags.SantaSize = 0;
    Flags.Rudolf = 0;

    setSantaRegions();
}

/** *********************************************************************
 ** This method ...
 **/
void SetSantaSizeSpeed() {
    SantaSpeed = Speed[Flags.SantaSize];
    if (Flags.SantaSpeedFactor < 10) {
        SantaSpeed = 0.1 * SantaSpeed;
    } else {
        SantaSpeed = 0.01 * Flags.SantaSpeedFactor * SantaSpeed;
    }
    mGlobal.ActualSantaSpeed = SantaSpeed;
    // init_Santa_surfaces();
    cairo_surface_t *surface =
        Santa_surfaces[Flags.SantaSize][Flags.Rudolf][mGlobal.SantaDirection]
                      [CurrentSanta];
    mGlobal.SantaWidth = cairo_image_surface_get_width(surface);
    mGlobal.SantaHeight = cairo_image_surface_get_height(surface);
    setSantaRegions();
}

/** *********************************************************************
 ** This method ...
 **/
// update santa's coordinates and speed
int do_usanta() {
    if (Flags.shutdownRequested) {
        return FALSE;
    }

#define RETURN \
    do { \
        return TRUE; \
    } while (0)

    if (!WorkspaceActive()) {
        RETURN;
    }
    if (Flags.NoSanta && !Flags.FollowSanta) {
        RETURN;
    }

    double yspeed;
    static int yspeeddir = 0;

    static double sdt = 0;
    static double dtt = 0;
    double dt = time_usanta;

    int oldx = mGlobal.SantaX;
    int oldy = mGlobal.SantaY;

    double santayrmin = 0;
    double santayrmax = mGlobal.SnowWinHeight * 0.33;


    // mGlobal.ActualSantaSpeed is the absolute value
    // of Santa's speed as is SantaSpeed.
    if (mGlobal.SantaDirection == 0) {
        mGlobal.ActualSantaSpeed += dt * (SANTASENS * mGlobal.NewWind +
            SantaSpeed - mGlobal.ActualSantaSpeed);
    } else {
        mGlobal.ActualSantaSpeed += dt * ((-SANTASENS) * mGlobal.NewWind +
            SantaSpeed - mGlobal.ActualSantaSpeed);
    }

    if (mGlobal.ActualSantaSpeed > 3 * SantaSpeed) {
        mGlobal.ActualSantaSpeed = 3 * SantaSpeed;
    } else if (mGlobal.ActualSantaSpeed < -2 * SantaSpeed) {
        mGlobal.ActualSantaSpeed = -2 * SantaSpeed;
    }

    if (mGlobal.SantaDirection == 0) {
        SantaXr += dt * mGlobal.ActualSantaSpeed;
    } else {
        SantaXr -= dt * mGlobal.ActualSantaSpeed;
    }

    if ((SantaXr >= mGlobal.SnowWinWidth && mGlobal.SantaDirection == 0) ||
        (SantaXr <= -mGlobal.SantaWidth && mGlobal.SantaDirection == 1)) {
        ResetSanta();
        oldx = mGlobal.SantaX;
        oldy = mGlobal.SantaY;
    }
    mGlobal.SantaX = lrintf(SantaXr);
    dtt += dt;
    if (dtt > 0.1) {
        dtt = 0;
        CurrentSanta++;
        if (CurrentSanta >= PIXINANIMATION) {
            CurrentSanta = 0;
        }
    }

    yspeed = mGlobal.ActualSantaSpeed / 4;
    sdt += dt;
    if (sdt > 2.0 * 50.0 / SantaSpeed || sdt > 2.0) {
        // time to change yspeed
        sdt = 0;
        yspeeddir = randint(3) - 1; //  -1, 0, 1
        if (SantaYr < santayrmin + 20) {
            yspeeddir = 2;
        }

        if (SantaYr > santayrmax - 20) {
            yspeeddir = -2;
        }
        int mooncy = mGlobal.moonY + Flags.MoonSize / 2;
        int ms;
        if (mGlobal.SantaDirection == 0) {
            ms = MoonSeeking && Flags.Moon &&
                 mGlobal.SantaX + mGlobal.SantaWidth <
                     mGlobal.moonX + Flags.MoonSize &&
                 mGlobal.SantaX + mGlobal.SantaWidth >
                     mGlobal.moonX - 300; // Santa likes to hover the moon
        } else {
            ms = MoonSeeking && Flags.Moon && mGlobal.SantaX > mGlobal.moonX &&
                 mGlobal.SantaX <
                     mGlobal.moonX + 300; // Santa likes to hover the moon
        }

        if (ms) {
            int dy = mGlobal.SantaY + mGlobal.SantaHeight / 2 - mooncy;
            if (dy < 0) {
                yspeeddir = 1;
            } else {
                yspeeddir = -1;
            }
            if (dy < -mGlobal.moonR / 2) { // todo
                yspeeddir = 3;
            } else if (dy > Flags.MoonSize / 2) {
                yspeeddir = -3;
            }
        }
    }

    SantaYr += dt * yspeed * yspeeddir;
    if (SantaYr < santayrmin) {
        SantaYr = 0;
    }

    if (SantaYr > santayrmax) {
        SantaYr = santayrmax;
    }
    mGlobal.SantaY = lrintf(SantaYr);

    XOffsetRegion(SantaRegion, mGlobal.SantaX - oldx, mGlobal.SantaY - oldy);
    XOffsetRegion(mGlobal.SantaPlowRegion, mGlobal.SantaX - oldx,
        mGlobal.SantaY - oldy);

    RETURN;
}

/** *********************************************************************
 ** This method ...
 **/
void ResetSanta() {
    // Most of the times, Santa will reappear at the side where He disappears
    if (drand48() > 0.2) {
        mGlobal.SantaDirection = 1 - mGlobal.SantaDirection;
    }
    // place Santa somewhere before the left edge or after the right edge of the
    // screen
    int offset = mGlobal.SantaWidth * (drand48() + 2);
    if (mGlobal.SantaDirection == 1) {
        offset -= mGlobal.SantaWidth;
    }
    // offset = 0;
    if (mGlobal.SantaDirection == 0) {
        mGlobal.SantaX = -offset;
    } else {
        mGlobal.SantaX = mGlobal.SnowWinWidth + offset;
    }

    SantaXr = mGlobal.SantaX;
    mGlobal.SantaY = randint(mGlobal.SnowWinHeight / 3) + 40;
    MoonSeeking = drand48() > 0.5;

    int ms;
    if (mGlobal.SantaDirection == 0) {
        ms = MoonSeeking && Flags.Moon && mGlobal.moonX < 400;
    } else {
        ms = MoonSeeking && Flags.Moon &&
             mGlobal.moonX > mGlobal.SnowWinWidth - 400;
    }

    if (ms) {
        mGlobal.SantaY = randint(Flags.MoonSize + 40) + mGlobal.moonY - 20;
    } else {
        mGlobal.SantaY = randint(mGlobal.SnowWinHeight / 3) + 40;
    }

    SantaYr = mGlobal.SantaY;
    SantaYStep = 1;
    CurrentSanta = 0;

    SetSantaSizeSpeed();
}

/** *********************************************************************
 ** This method ...
 **/
void SantaVisible() {
    mGlobal.SantaX = mGlobal.SnowWinWidth / 3;
    mGlobal.SantaY = mGlobal.SnowWinHeight / 6 + 40;
    SantaXr = mGlobal.SantaX;
    SantaYr = mGlobal.SantaY;
}

/** *********************************************************************
 ** This method ...
 **/
void setSantaRegions() {
    if (SantaRegion) {
        XDestroyRegion(SantaRegion);
    }

    SantaRegion = RegionCreateRectangle(
        mGlobal.SantaX, mGlobal.SantaY, mGlobal.SantaWidth, mGlobal.SantaHeight);

    if (mGlobal.SantaPlowRegion) {
        XDestroyRegion(mGlobal.SantaPlowRegion);
    }

    if (mGlobal.SantaDirection == 0) {
        mGlobal.SantaPlowRegion =
            RegionCreateRectangle(mGlobal.SantaX + mGlobal.SantaWidth,
                mGlobal.SantaY, 1, mGlobal.SantaHeight);
    } else {
        mGlobal.SantaPlowRegion = RegionCreateRectangle(
            mGlobal.SantaX - 1, mGlobal.SantaY, 1, mGlobal.SantaHeight);
    }
}

/** *********************************************************************
 ** This method ...
 **/
Region RegionCreateRectangle(int x, int y, int w, int h) {
    XPoint p[5];
    p[0].x = x;
    p[0].y = y;
    p[1].x = x + w;
    p[1].y = y;
    p[2].x = x + w;
    p[2].y = y + h;
    p[3].x = x;
    p[3].y = y + h;
    p[4].x = x;
    p[4].y = y;
    return XPolygonRegion(p, 5, EvenOddRule);
}
