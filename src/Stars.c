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

#include "Flags.h"
#include "pixmaps.h"
#include "safe_malloc.h"
#include "Stars.h"
#include "Utils.h"
#include "windows.h"


/** *********************************************************************
 ** Module globals and consts.
 **/

#define STARANIMATIONS 4

const int STAR_SIZE = 9;
const float LOCAL_SCALE = 0.8;

int mNumberOfStars;

StarCoordinate* mStarCoordinates = NULL;

char* mStarColorArray[STARANIMATIONS] =
    { (char*) "gold", (char*) "gold1",
      (char*) "gold4", (char*) "orange"};

cairo_surface_t* mStarSurfaceArray[STARANIMATIONS];


/** *********************************************************************
 ** This method initializes the Stars module.
 **/
void initStarsModule() {
    initStarsModuleArrays();

    // Clear and set mStarSurfaceArray.
    for (int i = 0; i < STARANIMATIONS; i++) {
        mStarSurfaceArray[i] = NULL;
    }
    initStarsModuleSurfaces();

    addMethodToMainloop(PRIORITY_DEFAULT, time_ustar, updateStarsFrame);
}

/** *********************************************************************
 ** This method updates Stars module settings between
 ** Erase and Draw cycles.
 **/
void initStarsModuleArrays() {
    mNumberOfStars = Flags.NStars;

    mStarCoordinates = (StarCoordinate*) realloc(mStarCoordinates,
        (mNumberOfStars + 1) * sizeof(StarCoordinate));
    REALLOC_CHECK(mStarCoordinates);

    for (int i = 0; i < mNumberOfStars; i++) {
        StarCoordinate* star = &mStarCoordinates[i];
        star->x = randint(mGlobal.SnowWinWidth);
        star->y = randint(mGlobal.SnowWinHeight / 4);
        star->color = randint(STARANIMATIONS);
    }
}

/** *********************************************************************
 ** This method inits cairo surfaces.
 **/
void initStarsModuleSurfaces() {
    for (int i = 0; i < STARANIMATIONS; i++) {
        float size = LOCAL_SCALE * mGlobal.WindowScale *
            0.01 * Flags.Scale * STAR_SIZE;

        size *= 0.2 * (1 + 4 * drand48());
        if (size < 1) {
            size = 1;
        }

        // Release and recreate surfaces.
        if (mStarSurfaceArray[i]) {
            cairo_surface_destroy(mStarSurfaceArray[i]);
        }
        mStarSurfaceArray[i] = cairo_image_surface_create(
            CAIRO_FORMAT_ARGB32, size, size);

        cairo_t *cr = cairo_create(mStarSurfaceArray[i]);
        cairo_set_line_width(cr, 1.0 * size / STAR_SIZE);

        GdkRGBA color;
        gdk_rgba_parse(&color, mStarColorArray[i]);

        cairo_set_source_rgba(cr, color.red,
            color.green, color.blue, color.alpha);

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

/** *********************************************************************
 ** This method erases a single Stars frame.
 **/
void eraseStarsFrame() {
    if (!Flags.Stars) {
        return;
    }

    for (int i = 0; i < mNumberOfStars; i++) {
        StarCoordinate* star = &mStarCoordinates[i];
        clearDisplayArea(mGlobal.display, mGlobal.SnowWin,
            star->x, star->y, STAR_SIZE, STAR_SIZE,
            mGlobal.xxposures);
    }
}

/** *********************************************************************
 ** This method updates Stars module between
 ** Erase and Draw cycles.
 **/
int updateStarsFrame() {
    if (Flags.shutdownRequested) {
        return FALSE;
    }
    if (!WorkspaceActive()) {
        return TRUE;
    }

    // Change color of 1/5 stars.
    for (int i = 0; i < mNumberOfStars; i++) {
        if (randint(5) == 0) {
            StarCoordinate* star = &mStarCoordinates[i];
            star->color = randint(STARANIMATIONS);
        }
    }

    // Change position of 1/50 stars.
    for (int i = 0; i < mNumberOfStars; i++) {
        if (randint(50) == 0) {
            StarCoordinate* star = &mStarCoordinates[i];
            star->x = randint(mGlobal.SnowWinWidth);
            star->y = randint(mGlobal.SnowWinHeight / 4);
        }
    }

    return TRUE;
}

/** *********************************************************************
 ** This method draws a single Stars frame.
 **/
void drawStarsFrame(cairo_t *cr) {
    if (!Flags.Stars) {
        return;
    }

    cairo_save(cr);

    cairo_set_line_width(cr, 1);
    cairo_set_antialias(cr, CAIRO_ANTIALIAS_NONE);

    for (int i = 0; i < mNumberOfStars; i++) {
        StarCoordinate* star = &mStarCoordinates[i];

        cairo_set_source_surface(cr,
            mStarSurfaceArray[star->color], star->x, star->y);
        my_cairo_paint_with_alpha(cr,
            (0.01 * (100 - Flags.Transparency)));
    }

    cairo_restore(cr);
}

/** *********************************************************************
 ** This method updates the Stars module with
 ** refreshed user settings.
 **/
void updateStarsUserSettings() {
    UIDO(NStars, initStarsModuleArrays(); clearGlobalSnowWindow(););
    UIDO(Stars, clearGlobalSnowWindow(););

    static int prev = 100;
    if (appScalesHaveChanged(&prev)) {
        initStarsModuleSurfaces();
        initStarsModuleArrays();
    }
}
