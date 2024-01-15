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
#include "stars.h"
#include "debug.h"
#include "flags.h"
#include "pixmaps.h"
#include "safe_malloc.h"
#include "utils.h"
#include "windows.h"
#include <X11/Intrinsic.h>
#include <gtk/gtk.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#define NOTACTIVE (!WorkspaceActive())

static int
    NStars; // is copied from Flags.NStars in init_stars. We cannot have that
//                               // NStars is changed outside init_stars
static StarCoordinate *Stars = NULL;
static char *StarColor[STARANIMATIONS] = {
    (char *)"gold", (char *)"gold1", (char *)"gold4", (char *)"orange"};
static int do_ustars();
static void set_star_surfaces(void);

static const int StarSize = 9;
static const float LocalScale = 0.8;

static cairo_surface_t *surfaces[STARANIMATIONS];

void stars_init() {
    int i;
    init_stars();
    for (i = 0; i < STARANIMATIONS; i++) {
        surfaces[i] = NULL;
    }
    set_star_surfaces();
    add_to_mainloop(PRIORITY_DEFAULT, time_ustar, do_ustars);
}

void set_star_surfaces() {
    int i;
    for (i = 0; i < STARANIMATIONS; i++) {
        float size =
            LocalScale * global.WindowScale * 0.01 * Flags.Scale * StarSize;
        size *= 0.2 * (1 + 4 * drand48());
        if (size < 1) {
            size = 1;
        }
        if (surfaces[i]) {
            cairo_surface_destroy(surfaces[i]);
        }
        surfaces[i] =
            cairo_image_surface_create(CAIRO_FORMAT_ARGB32, size, size);
        cairo_t *cr = cairo_create(surfaces[i]);
        cairo_set_line_width(cr, 1.0 * size / StarSize);
        GdkRGBA color;
        gdk_rgba_parse(&color, StarColor[i]);
        cairo_set_source_rgba(
            cr, color.red, color.green, color.blue, color.alpha);
        cairo_move_to(cr, 0, 0);
        cairo_line_to(cr, size, size);
        cairo_move_to(cr, 0, size);
        cairo_line_to(cr, size, 0);
        cairo_move_to(cr, 0, size / 2);
        cairo_line_to(cr, size, size / 2);
        cairo_move_to(cr, size / 2, 0);
        cairo_line_to(cr, size / 2, size);
        cairo_stroke(cr);

        cairo_destroy(cr);
    }
}

void init_stars() {
    int i;
    NStars = Flags.NStars;
    P("initstars %d\n", NStars);
    // Nstars+1: we do not allocate 0 bytes
    Stars = (StarCoordinate *)realloc(Stars, (NStars + 1) * sizeof(StarCoordinate));
    REALLOC_CHECK(Stars);
    for (i = 0; i < NStars; i++) {
        StarCoordinate *star = &Stars[i];
        star->x = randint(global.SnowWinWidth);
        star->y = randint(global.SnowWinHeight / 4);
        star->color = randint(STARANIMATIONS);
        P("stars_init %d %d %d\n", star->x, star->y, star->color);
    }
    // set_star_surfaces();
}

void stars_draw(cairo_t *cr) {
    if (!Flags.Stars) {
        return;
    }
    int i;
    cairo_save(cr);
    cairo_set_line_width(cr, 1);
    cairo_set_antialias(cr, CAIRO_ANTIALIAS_NONE);
    for (i = 0; i < NStars; i++) {
        P("stars_draw i: %d %d %d\n", i, NStars, counter++);
        StarCoordinate *star = &Stars[i];
        int x = star->x;
        int y = star->y;
        int color = star->color;
        cairo_set_source_surface(cr, surfaces[color], x, y);
        my_cairo_paint_with_alpha(cr, ALPHA);
    }

    cairo_restore(cr);
}

void stars_erase() {
    if (!Flags.Stars) {
        return;
    }
    int i;
    for (i = 0; i < NStars; i++) {
        P("stars_erase i: %d %d %d\n", i, NStars, counter++);
        StarCoordinate *star = &Stars[i];
        int x = star->x;
        int y = star->y;
        sanelyCheckAndClearDisplayArea(global.display, global.SnowWin, x, y, StarSize, StarSize,
            global.xxposures);
    }
}

void stars_ui() {
    UIDO(NStars, init_stars(); ClearScreen(););
    UIDO(Stars, ClearScreen(););

    static int prev = 100;
    P("stars_ui %d\n", prev);
    if (appScalesHaveChanged(&prev)) {
        set_star_surfaces();
        init_stars();
        P("stars_ui changed\n");
    }
}

int do_ustars() {
    if (Flags.Done) {
        return FALSE;
    }
    if (NOTACTIVE) {
        return TRUE;
    }
    int i;
    for (i = 0; i < NStars; i++) {
        if (drand48() > 0.8) {
            Stars[i].color = randint(STARANIMATIONS);
        }
    }
    return TRUE;
    // (void) d;
}
