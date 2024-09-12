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
#include <stdlib.h>

#include <gtk/gtk.h>


#include "Flags.h"
#include "moon.h"
#include "pixmaps.h"
#include "Utils.h"
#include "windows.h"


/** *********************************************************************
 ** Module globals and consts.
 **/
cairo_surface_t *moon_surface = NULL;
cairo_surface_t *halo_surface = NULL;

// radius of halo in pixels
double haloR;

double OldmoonX;
double OldmoonY;

float moonScale;

int xdirection = 1;
int ydirection = 1;

int prevw = 0;
int prevh = 0;

int prev;
GdkPixbuf *pixbuf, *pixbufscaled;


/** *********************************************************************
 ** This method ...
 **/
void updateMoonAppSettings() {
    UIDO(MoonSpeed, );
    UIDO(Halo, halo_erase(););
    UIDO(Moon, moon_erase(1););
    UIDO(MoonSize, init_moon_surface(););
    UIDO(MoonColor, init_moon_surface(););
    UIDO(HaloBright, init_halo_surface(););

    if (appScalesHaveChanged(&prev)) {
        moonScale = 0.01 * mGlobal.WindowScale * Flags.Scale;
        init_moon_surface();
        if (prevw > 0 && prevh > 0) {
            mGlobal.moonX = (float)mGlobal.moonX / prevw * mGlobal.SnowWinWidth;
            mGlobal.moonY = (float)mGlobal.moonY / prevh * mGlobal.SnowWinHeight;
        }
        prevw = mGlobal.SnowWinWidth;
        prevh = mGlobal.SnowWinHeight;
    }
}

/** *********************************************************************
 ** This method ...
 **/
void initMoonModule(void) {
    moonScale = (float)Flags.Scale * 0.01 * mGlobal.WindowScale;
    init_moon_surface();
    addMethodToMainloop(PRIORITY_DEFAULT, time_umoon, do_umoon);

    mGlobal.moonX = (mGlobal.SnowWinWidth - 2 * mGlobal.moonR) * drand48();
    mGlobal.moonY = mGlobal.moonR + drand48() * mGlobal.moonR;
}

/** *********************************************************************
 ** This method ...
 **/
int moon_draw(cairo_t *cr) {
    if (!Flags.Moon || !WorkspaceActive()) {
        return true;
    }

    cairo_set_source_surface(cr, moon_surface, mGlobal.moonX, mGlobal.moonY);
    paintCairoContextWithAlpha(cr, ALPHA);
    OldmoonX = mGlobal.moonX;
    OldmoonY = mGlobal.moonY;
    halo_draw(cr);
    return true;
}

/** *********************************************************************
 ** This method ...
 **/
int moon_erase(int force) {
    if (mGlobal.isDoubleBuffered) {
        return 0;
    }

    if (!force) {
        if (!Flags.Moon || !WorkspaceActive()) {
            return true;
        }
    }

    if (Flags.Halo) {
        halo_erase();
    } else {
        sanelyCheckAndClearDisplayArea(mGlobal.display, mGlobal.SnowWin, OldmoonX, OldmoonY,
            2 * mGlobal.moonR + 1, 2 * mGlobal.moonR + 1, mGlobal.xxposures);
    }
    return 0;
}

/** *********************************************************************
 ** This method ...
 **/
void init_moon_surface() {
    const GdkInterpType interpolation = GDK_INTERP_HYPER;

    if (Flags.MoonColor < 0) {
        Flags.MoonColor = 0;
    }
    if (Flags.MoonColor > 1) {
        Flags.MoonColor = 1;
    }

    // int whichmoon = Flags.MoonColor;
    int whichmoon = (Flags.MoonColor == 0) ? 1 : 0;
    pixbuf = gdk_pixbuf_new_from_xpm_data((const char **) moons_xpm[whichmoon]);

    // standard moon is some percentage of window width
    const float p = 30.0;
    mGlobal.moonR = p * Flags.MoonSize * 0.01 * moonScale;

    int w = mGlobal.moonR * 2;
    int h = w;
    if (moon_surface) {
        cairo_surface_destroy(moon_surface);
    }

    if (w < 1) {
        w = 1;
        h = 1;
    }

    if (w == 1 && h == 1) {
        h = 2;
    }

    pixbufscaled = gdk_pixbuf_scale_simple(pixbuf, w, h, interpolation);
    moon_surface = gdk_cairo_surface_create_from_pixbuf(pixbufscaled, 0, NULL);

    g_clear_object(&pixbuf);
    g_clear_object(&pixbufscaled);

    init_halo_surface();

    if (!mGlobal.isDoubleBuffered) {
        clearGlobalSnowWindow();
    }
}

/** *********************************************************************
 ** This method ...
 **/
int do_umoon() {
    if (Flags.shutdownRequested) {
        return false;
    }

    if (!Flags.Moon || !WorkspaceActive()) {
        return true;
    }

    if (!Flags.Moon) {
        return true;
    }

    mGlobal.moonX += xdirection * time_umoon * Flags.MoonSpeed / 60.0;
    mGlobal.moonY += 0.2 * ydirection * time_umoon * Flags.MoonSpeed / 60.0;

    if (mGlobal.moonX > mGlobal.SnowWinWidth - 2 * mGlobal.moonR) {
        mGlobal.moonX = mGlobal.SnowWinWidth - 2 * mGlobal.moonR;
        xdirection = -1;
    } else if (mGlobal.moonX < 2 * mGlobal.moonR) {
        mGlobal.moonX = 2 * mGlobal.moonR;
        xdirection = 1;
    }

    if (mGlobal.moonY > 2 * mGlobal.moonR) {
        mGlobal.moonY = 2 * mGlobal.moonR;
        ydirection = -1;
    } else if (mGlobal.moonY < 2 * mGlobal.moonR) {
        mGlobal.moonY = 2 * mGlobal.moonR;
        ydirection = 1;
    }

    return true;
}

/** *********************************************************************
 ** This method ...
 **/
void init_halo_surface() {
    if (halo_surface) {
        cairo_surface_destroy(halo_surface);
    }

    haloR = 1.8 * mGlobal.moonR;
    cairo_pattern_t* pattern = cairo_pattern_create_radial(haloR, haloR,
        mGlobal.moonR, haloR, haloR, haloR);

    const double bright = Flags.HaloBright * ALPHA * 0.01;
    cairo_pattern_add_color_stop_rgba(pattern,
        0.0, 234.0 / 255, 244.0 / 255, 252.0 / 255, bright);

    cairo_pattern_add_color_stop_rgba(pattern,
        1.0, 234.0 / 255, 244.0 / 255, 252.0 / 255, 0.0);

    halo_surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
        2 * haloR, 2 * haloR);

    cairo_t* halocr = cairo_create(halo_surface);
    cairo_set_source_rgba(halocr, 0, 0, 0, 0);
    cairo_paint(halocr);
    cairo_set_source(halocr, pattern);
    cairo_arc(halocr, haloR, haloR, haloR, 0, M_PI * 2);
    cairo_fill(halocr);
    cairo_destroy(halocr);

    cairo_pattern_destroy(pattern);
}

/** *********************************************************************
 ** This method ...
 **/
void halo_draw(cairo_t *cr) {
    if (!Flags.Halo) {
        return;
    }

    double xc = mGlobal.moonX + mGlobal.moonR;
    double yc = mGlobal.moonY + mGlobal.moonR;

    cairo_set_source_surface(cr, halo_surface, xc - haloR, yc - haloR);
    paintCairoContextWithAlpha(cr, ALPHA);
}

/** *********************************************************************
 ** This method ...
 **/
void halo_erase() {
    const int x = OldmoonX + mGlobal.moonR - haloR;
    const int y = OldmoonY + mGlobal.moonR - haloR;
    const int w = 2 * haloR;
    const int h = w;

    sanelyCheckAndClearDisplayArea(mGlobal.display, mGlobal.SnowWin,
        x, y, w + 1, h + 1, mGlobal.xxposures);
}
