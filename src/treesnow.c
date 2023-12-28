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

#include "treesnow.h"
#include "blowoff.h"
#include "debug.h"
#include "flags.h"
#include "safe_malloc.h"
#include "scenery.h"
#include "snow.h"
#include "utils.h"
#include "wind.h"
#include "windows.h"
#include "plasmasnow.h"
#include <X11/Intrinsic.h>
#include <gtk/gtk.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#define NOTACTIVE                                                              \
    (!WorkspaceActive() || Flags.NoSnowFlakes || Flags.NoKeepSnowOnTrees || Flags.NoTrees)

static int do_snow_on_trees();
static void ConvertOnTreeToFlakes(void);

void treesnow_init() {
    global.gSnowOnTreesRegion = cairo_region_create();
    add_to_mainloop(PRIORITY_DEFAULT, time_snow_on_trees, do_snow_on_trees);
}

void treesnow_draw(cairo_t *cr) {
    if (NOTACTIVE) {
        return;
    }
    GdkRGBA color;
    gdk_rgba_parse(&color, Flags.SnowColor);
    cairo_set_source_rgba(cr, color.red, color.green, color.blue, ALPHA);
    gdk_cairo_region(cr, global.gSnowOnTreesRegion);
    cairo_fill(cr);
}

void treesnow_ui() {
    UIDO(MaxOnTrees, ClearScreen(););
    UIDO(NoKeepSnowOnTrees, ClearScreen(););
}

int do_snow_on_trees() {
    P("do_snow_on_trees %d\n", counter++);
    if (Flags.Done) {
        return FALSE;
    }
    if (NOTACTIVE) {
        return TRUE;
    }
    if (global.Wind == 2) {
        ConvertOnTreeToFlakes();
    }
    return TRUE;
    // (void) d;
}

// blow snow off trees
void ConvertOnTreeToFlakes() {
    P("ConvertOnTreeToFlakes %d\n", global.OnTrees);
    int i;
    for (i = 0; i < global.OnTrees; i++) {
        int j;
        for (j = 0; j < 2; j++) {
            int k, kmax = BlowOff();
            for (k = 0; k < kmax; k++) {
                Snow *flake = MakeFlake(-1);
                flake->rx = global.SnowOnTrees[i].x;
                flake->ry = global.SnowOnTrees[i].y - 5 * j;
                flake->vy = 0;
                flake->vx = global.NewWind / 2;
                flake->cyclic = 0;
            }
        }
    }
    global.OnTrees = 0;
    reinit_treesnow_region();
}

void reinit_treesnow_region() {
    cairo_region_destroy(global.gSnowOnTreesRegion);
    global.gSnowOnTreesRegion = cairo_region_create();
}

void InitSnowOnTrees() {
    // Flags.MaxOnTrees+1: prevent allocation of zero bytes
    global.SnowOnTrees = (XPoint *)realloc(global.SnowOnTrees,
        sizeof(*global.SnowOnTrees) * (Flags.MaxOnTrees + 1));
    if (global.OnTrees > Flags.MaxOnTrees) {
        global.OnTrees = Flags.MaxOnTrees;
    }
}
