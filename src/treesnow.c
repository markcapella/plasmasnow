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

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#include <X11/Intrinsic.h>

#include <gtk/gtk.h>

#include "blowoff.h"
#include "debug.h"
#include "Flags.h"
#include "plasmasnow.h"
#include "safe_malloc.h"
#include "scenery.h"
#include "snow.h"
#include "treesnow.h"
#include "Utils.h"
#include "wind.h"
#include "windows.h"


static int do_snow_on_trees();
static void ConvertOnTreeToFlakes(void);

void treesnow_init() {
    mGlobal.gSnowOnTreesRegion = cairo_region_create();
    addMethodToMainloop(PRIORITY_DEFAULT, time_snow_on_trees, do_snow_on_trees);
}

void treesnow_draw(cairo_t *cr) {
    if (!WorkspaceActive() || Flags.NoSnowFlakes ||
        Flags.NoKeepSnowOnTrees || Flags.NoTrees) {
        return;
    }
    GdkRGBA color;
    gdk_rgba_parse(&color, Flags.SnowColor);
    cairo_set_source_rgba(cr, color.red, color.green, color.blue, ALPHA);
    gdk_cairo_region(cr, mGlobal.gSnowOnTreesRegion);
    cairo_fill(cr);
}

void treesnow_ui() {
    UIDO(MaxOnTrees, clearGlobalSnowWindow(););
    UIDO(NoKeepSnowOnTrees, clearGlobalSnowWindow(););
}

int do_snow_on_trees() {
    if (Flags.shutdownRequested) {
        return FALSE;
    }
    if ((!WorkspaceActive() || Flags.NoSnowFlakes ||
        Flags.NoKeepSnowOnTrees || Flags.NoTrees)) {
        return TRUE;
    }
    if (mGlobal.Wind == 2) {
        ConvertOnTreeToFlakes();
    }

    return TRUE;
}

/***********************************************************
 * This method blows snow off trees.
 */
void ConvertOnTreeToFlakes() {
    for (int i = 0; i < mGlobal.OnTrees; i++) {
        for (int j = 0; j < 2; j++) {
            int numberOfFlakesToMake = getNumberOfFlakesToBlowoff();
            for (int k = 0; k < numberOfFlakesToMake; k++) {
                SnowFlake* flake = MakeFlake(-1);
                flake->rx = mGlobal.SnowOnTrees[i].x;
                flake->ry = mGlobal.SnowOnTrees[i].y - 5 * j;
                flake->vx = mGlobal.NewWind / 2;
                flake->vy = 0;
                flake->cyclic = 0;
            }
        }
    }

    mGlobal.OnTrees = 0;
    reinit_treesnow_region();
}

void reinit_treesnow_region() {
    cairo_region_destroy(mGlobal.gSnowOnTreesRegion);
    mGlobal.gSnowOnTreesRegion = cairo_region_create();
}

void InitSnowOnTrees() {
    // TODO: Huh?
    // Flags.MaxOnTrees+1: Avoid allocating zero bytes.

    mGlobal.SnowOnTrees = (XPoint*) realloc(mGlobal.SnowOnTrees,
        sizeof(*mGlobal.SnowOnTrees) * (Flags.MaxOnTrees + 1));

    if (mGlobal.OnTrees > Flags.MaxOnTrees) {
        mGlobal.OnTrees = Flags.MaxOnTrees;
    }
}
