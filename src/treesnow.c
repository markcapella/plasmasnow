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

#include "PlasmaSnow.h"

#include "Blowoff.h"
#include "Flags.h"
#include "safe_malloc.h"
#include "scenery.h"
#include "Storm.h"
#include "StormItem.h"
#include "treesnow.h"
#include "Utils.h"
#include "wind.h"
#include "Windows.h"


/** *********************************************************************
 ** Module globals and consts.
 **/
bool mTreesnowBackgroundThreadIsActive = false;


/** *********************************************************************
 ** This method ...
 **/
void treesnow_init() {
    mGlobal.gSnowOnTreesRegion = cairo_region_create();
    addMethodToMainloop(PRIORITY_DEFAULT,
        time_snow_on_trees, execTreesnowBackgroundThread);
}

/** *********************************************************************
 ** This method ...
 **/
void InitSnowOnTrees() {
    // TODO: Huh?
    // Flags.MaxOnTrees+1: Avoid allocating zero bytes.

    mGlobal.SnowOnTrees = (XPoint*) realloc(mGlobal.SnowOnTrees,
        sizeof(*mGlobal.SnowOnTrees) * (Flags.MaxOnTrees + 1));

    if (mGlobal.OnTrees > Flags.MaxOnTrees) {
        mGlobal.OnTrees = Flags.MaxOnTrees;
    }
}

/** ***********************************************************
 ** This method checks for & performs user changes of
 ** Scenery (Treesnow module) settings.
 **/
void respondToTreesnowSettingsChanges() {
    UIDO(MaxOnTrees, clearGlobalSnowWindow(););
    UIDO(NoKeepSnowOnTrees, clearGlobalSnowWindow(););
}

/** *********************************************************************
 ** This method ...
 **/
int execTreesnowBackgroundThread() {
    mTreesnowBackgroundThreadIsActive = true;

    if (Flags.shutdownRequested) {
        mTreesnowBackgroundThreadIsActive = false;
        return FALSE;
    }

    if ((!isWorkspaceActive() || Flags.NoSnowFlakes ||
        Flags.NoKeepSnowOnTrees || Flags.NoTrees)) {
        mTreesnowBackgroundThreadIsActive = false;
        return TRUE;
    }

    if (mGlobal.Wind == 2) {
        ConvertOnTreeToFlakes();
    }

    bool mTreesnowBackgroundThreadIsActive = false;
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
                StormItem* flake = createStormItem(-1);
                flake->xRealPosition = mGlobal.SnowOnTrees[i].x;
                flake->yRealPosition = mGlobal.SnowOnTrees[i].y - 5 * j;
                flake->xVelocity = mGlobal.NewWind / 2;
                flake->yVelocity = 0;
                flake->survivesScreenEdges = false;
                addStormItem(flake);
            }
        }
    }

    mGlobal.OnTrees = 0;
    reinit_treesnow_region();
}

/** *********************************************************************
 ** This method ...
 **/
void reinit_treesnow_region() {
    cairo_region_destroy(mGlobal.gSnowOnTreesRegion);
    mGlobal.gSnowOnTreesRegion = cairo_region_create();
}

/** *********************************************************************
 ** This method draws the treesnow onto the display screen.
 **/
void treesnow_draw(cairo_t* cr) {
    if (!isWorkspaceActive() || Flags.NoSnowFlakes ||
        Flags.NoKeepSnowOnTrees || Flags.NoTrees) {
        return;
    }
    GdkRGBA color;

    gdk_rgba_parse(&color, Flags.StormItemColor1);
    cairo_set_source_rgba(cr, color.red, color.green,
        color.blue, ALPHA);
    gdk_cairo_region(cr, mGlobal.gSnowOnTreesRegion);
    cairo_fill(cr);
}
