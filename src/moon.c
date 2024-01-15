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

#include "moon.h"
#include "debug.h"
#include "flags.h"
#include "pixmaps.h"
#include "utils.h"
#include "windows.h"
#include <gtk/gtk.h>
#include <math.h>
#include <pthread.h>
#include <stdlib.h>

#define LEAVE_IF_INACTIVE                                                      \
    if (!Flags.Moon || !WorkspaceActive())                                     \
    return TRUE

static int do_umoon();
static void init_moon_surface(void);
static void init_halo_surface(void);
static void halo_draw(cairo_t *cr);
static void halo_erase();

static cairo_surface_t *moon_surface = NULL;
static cairo_surface_t *halo_surface = NULL;
static double haloR; // radius of halo in pixels

static double OldmoonX;
static double OldmoonY;

static float moonScale;

void moon_init(void) {
    moonScale = (float)Flags.Scale * 0.01 * global.WindowScale;
    init_moon_surface();
    add_to_mainloop(PRIORITY_DEFAULT, time_umoon, do_umoon);
    /*
    if (global.SnowWinWidth > 400*moonScale)
    {
       global.moonX = moonScale*200+drand48()*(global.SnowWinWidth -
    400*moonScale - 2*global.moonR);
    }
    */
    global.moonX = (global.SnowWinWidth - 2 * global.moonR) * drand48();
    global.moonY = global.moonR + drand48() * global.moonR;
    P("moonX moonY: %f %f\n", global.moonX, global.moonY);
}

int moon_draw(cairo_t *cr) {
    LEAVE_IF_INACTIVE;
    P("moon_draw %d %d %d\n", global.counter++, (int)global.moonX,
        (int)global.moonY);
    cairo_set_source_surface(cr, moon_surface, global.moonX, global.moonY);
    my_cairo_paint_with_alpha(cr, ALPHA);
    OldmoonX = global.moonX;
    OldmoonY = global.moonY;
    halo_draw(cr);
    return TRUE;
}

int moon_erase(int force) {
    if (global.IsDouble) {
        return 0;
    }
    if (!force) {
        LEAVE_IF_INACTIVE;
    }
    if (Flags.Halo) {
        halo_erase();
    } else {
        sanelyCheckAndClearDisplayArea(global.display, global.SnowWin, OldmoonX, OldmoonY,
            2 * global.moonR + 1, 2 * global.moonR + 1, global.xxposures);
    }
    return 0;
}

void moon_ui() {
    UIDO(MoonSpeed, );
    UIDO(Halo, halo_erase(););
    UIDO(Moon, moon_erase(1););
    UIDO(MoonSize, init_moon_surface(););
    UIDO(MoonColor, init_moon_surface(););
    UIDO(HaloBright, init_halo_surface(););

    static int prevw = 0;
    static int prevh = 0;

    static int prev;
    if (appScalesHaveChanged(&prev)) {
        P("%d moonscale\n", global.counter);
        moonScale = 0.01 * global.WindowScale * Flags.Scale;
        init_moon_surface();
        if (prevw > 0 && prevh > 0) {
            global.moonX = (float)global.moonX / prevw * global.SnowWinWidth;
            global.moonY = (float)global.moonY / prevh * global.SnowWinHeight;
        }
        prevw = global.SnowWinWidth;
        prevh = global.SnowWinHeight;
    }
}

static void init_moon_surface() {
    P("init_moon_surface %d\n", Flags.MoonColor);
    const GdkInterpType interpolation = GDK_INTERP_HYPER;

    if (Flags.MoonColor < 0) {
        Flags.MoonColor = 0;
    }
    if (Flags.MoonColor > 1) {
        Flags.MoonColor = 1;
    }

    static GdkPixbuf *pixbuf, *pixbufscaled;

    // int whichmoon = Flags.MoonColor;
    int whichmoon = (Flags.MoonColor == 0) ? 1 : 0;
    pixbuf = gdk_pixbuf_new_from_xpm_data((const char **) moons_xpm[whichmoon]);

    // standard moon is some percentage of window width
    const float p = 30.0;
    global.moonR = p * Flags.MoonSize * 0.01 * moonScale;

    int w = global.moonR * 2;
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

    if (!global.IsDouble) {
        ClearScreen();
    }
}

int do_umoon() {
    // (void) d;
    static int xdirection = 1;
    static int ydirection = 1;
    if (Flags.Done) {
        return FALSE;
    }
    LEAVE_IF_INACTIVE;
    if (!Flags.Moon) {
        return TRUE;
    }

    global.moonX += xdirection * time_umoon * Flags.MoonSpeed / 60.0;
    global.moonY += 0.2 * ydirection * time_umoon * Flags.MoonSpeed / 60.0;

    if (global.moonX > global.SnowWinWidth - 2 * global.moonR) {
        global.moonX = global.SnowWinWidth - 2 * global.moonR;
        xdirection = -1;
    } else if (global.moonX < 2 * global.moonR) {
        global.moonX = 2 * global.moonR;
        xdirection = 1;
    }

    if (global.moonY > 2 * global.moonR) {
        global.moonY = 2 * global.moonR;
        ydirection = -1;
    } else if (global.moonY < 2 * global.moonR) {
        global.moonY = 2 * global.moonR;
        ydirection = 1;
    }

    P("Moon pos: %d %d\n", (int)global.moonX, (int)global.moonY);
    return TRUE;
}

void init_halo_surface() {
    if (halo_surface) {
        cairo_surface_destroy(halo_surface);
    }
    cairo_pattern_t *pattern;
    haloR = 1.8 * global.moonR;
    P("halo_draw %f %f \n", global.moonR, haloR);
    pattern = cairo_pattern_create_radial(
        haloR, haloR, global.moonR, haloR, haloR, haloR);
    double bright = Flags.HaloBright * ALPHA * 0.01;
    // cairo_pattern_add_color_stop_rgba(pattern, 0.0, 1.0, 1.0, 1.0,
    // 0.4*ALPHA);
    // cairo_pattern_add_color_stop_rgba(pattern, 1.0, 1.0, 1.0, 1.0, 0.0);
    // cairo_pattern_add_color_stop_rgba(pattern, 0.0, 0.9725, 0.9725, 1.0,
    // 0.4*ALPHA); cairo_pattern_add_color_stop_rgba(pattern, 1.0, 0.9725,
    // 0.9725, 1.0, 0.0); cairo_pattern_add_color_stop_rgba(pattern, 0.0,
    // 0.96078, 0.96078, 0.96078, 0.4*ALPHA);
    // cairo_pattern_add_color_stop_rgba(pattern, 1.0, 0.96078, 0.96078,
    // 0.96078, 0.0); cairo_pattern_add_color_stop_rgba(pattern, 0.0, 0.94,
    // 0.94, 0.9, 0.4*ALPHA); cairo_pattern_add_color_stop_rgba(pattern, 1.0,
    // 0.94, 0.94, 0.9, 0.0); cairo_pattern_add_color_stop_rgba(pattern, 0.0,
    // 244.0/255, 241.0/255, 201.0/255, 0.4*ALPHA);
    // cairo_pattern_add_color_stop_rgba(pattern, 1.0, 244.0/255, 241.0/255,
    // 201.0/255, 0.0); cairo_pattern_add_color_stop_rgba(pattern, 0.0,
    // 245.0/255, 243.0/255, 206.0/255, 0.4*ALPHA);
    // cairo_pattern_add_color_stop_rgba(pattern, 1.0, 245.0/255, 243.0/255,
    // 206.0/255, 0.0);
    cairo_pattern_add_color_stop_rgba(
        pattern, 0.0, 234.0 / 255, 244.0 / 255, 252.0 / 255, bright);
    cairo_pattern_add_color_stop_rgba(
        pattern, 1.0, 234.0 / 255, 244.0 / 255, 252.0 / 255, 0.0);

    // GdkPixbuf *pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8, 2*haloR,
    // 2*haloR);
    halo_surface =
        cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 2 * haloR, 2 * haloR);
    cairo_t *halocr = cairo_create(halo_surface);
    // gdk_cairo_set_source_pixbuf(halocr, pixbuf, 0, 0);

    cairo_set_source_rgba(halocr, 0, 0, 0, 0);
    cairo_paint(halocr);
    cairo_set_source(halocr, pattern);
    cairo_arc(halocr, haloR, haloR, haloR, 0, M_PI * 2);
    cairo_fill(halocr);

    cairo_destroy(halocr);
    cairo_pattern_destroy(pattern);
    // g_clear_object         (&pixbuf);
}

void halo_draw(cairo_t *cr) {
    if (!Flags.Halo) {
        return;
    }
    P("halo_draw %f %f \n", global.moonR, haloR);
    double xc = global.moonX + global.moonR;
    double yc = global.moonY + global.moonR;

    cairo_set_source_surface(cr, halo_surface, xc - haloR, yc - haloR);
    my_cairo_paint_with_alpha(cr, ALPHA);
}

void halo_erase() {
    int x = OldmoonX + global.moonR - haloR;
    int y = OldmoonY + global.moonR - haloR;
    int w = 2 * haloR;
    int h = w;
    sanelyCheckAndClearDisplayArea(
        global.display, global.SnowWin, x, y, w + 1, h + 1, global.xxposures);
}
