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
#include <stdbool.h>

#include <X11/Intrinsic.h>
#include <X11/xpm.h>

#include <gtk/gtk.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "csvpos.h"
#include "debug.h"
#include "fallensnow.h"
#include "flags.h"
#include "ixpm.h"
#include "pixmaps.h"
#include "safe_malloc.h"
#include "scenery.h"
#include "treesnow.h"
#include "utils.h"
#include "windows.h"


/***********************************************************
 * Externally provided to this Module.
 */
bool isQPickerActive();
char* getQPickerCallerName();
bool isQPickerVisible();
bool isQPickerTerminated();
int getQPickerRed();
int getQPickerBlue();
int getQPickerGreen();
void endQPickerDialog();


/***********************************************************
 * Module globals and consts.
 */
#define DEFAULTTREETYPE 2

static int NTrees = 0;
static int TreeRead = 0;

static int NtreeTypes = 0;
static int *TreeType = NULL;

static int TreeWidth[NUM_ALL_SCENE_TYPES];
static int TreeHeight[NUM_ALL_SCENE_TYPES];

static int ExternalTree = False;
static bool mSceneryNeedsInit = true;

static char **TreeXpm = NULL;

static Pixmap mColorableTreePixmap[NUM_ALL_SCENE_TYPES] [2];
static Pixmap TreeMaskPixmap[NUM_ALL_SCENE_TYPES] [2];

static SceneryInfo** mSceneryInfoArray = NULL;

static float treeScale = 1.0;
static const float LocalScale = 0.7;

// Scale for items with low y-coordinate.
static float MinScale = 0.6;

static int mRemoveFluffAttempts = 0;
static int mCurrentAppScale = 100;


/***********************************************************
 * Main Scenery methods.
 */
void initSceneryModule() {
    int *a;
    int n;
    csvpos(Flags.TreeType, &a, &n);
    int *b = (int *) malloc(sizeof(int) * n);

    int m = 0;
    for (int i = 0; i < n; i++) {
        if (a[i] >= 0 && a[i] <= NUM_SCENE_GRID_ITEMS) {
            b[m] = a[i];
            m++;
        }
    }

    free(Flags.TreeType);
    vsc(&Flags.TreeType, b, m);
    WriteFlags();

    free(a);
    free(b);

    setSceneryScale();

    mGlobal.TreeRegion = cairo_region_create();
    initSceneryPixmaps();

    addMethodToMainloop(PRIORITY_DEFAULT, time_initbaum,
        updateSceneryFrame);
}

/***********************************************************
 * This method ...
 */
void setSceneryScale() {
    treeScale = LocalScale * 0.01 * Flags.Scale *
        mGlobal.WindowScale;
}

/***********************************************************
 * This method ...
 */
void initSceneryPixmaps() {
    XpmAttributes attributes;
    attributes.valuemask = XpmDepth;
    attributes.depth = mGlobal.SnowWinDepth;

    for (int i = 0; i < 2; i++) {
        for (int tt = 0; tt <= NUM_BASE_SCENE_TYPES; tt++) {
            iXpmCreatePixmapFromData(mGlobal.display,
                mGlobal.SnowWin, xpmtrees[tt],
                &mColorableTreePixmap[tt][i],
                &TreeMaskPixmap[tt][i], &attributes, i);

            sscanf(xpmtrees[tt][0], "%d %d",
                &TreeWidth[tt], &TreeHeight[tt]);
        }
    }

    mGlobal.OnTrees = 0;
}

/***********************************************************
 * This method ...
 */
void initSceneryModuleSurfaces() {
    for (int i = 0; i < NTrees; i++) {
        SceneryInfo* tree = mSceneryInfoArray[i];

        // Destroy existing surface.
        if (tree->surface) {
            cairo_surface_destroy(tree->surface);
        }

        // Create new surface.
        if (TreeRead) {
            tree->surface = getNewScenerySurface(tree->rev,
                (const char**) TreeXpm,
                tree->scale);
        } else {
            tree->surface = getNewScenerySurface(tree->rev,
                (const char**) xpmtrees[tree->type],
                tree->scale);
        }
    }
}

/***********************************************************
 * This method ...
 */
cairo_surface_t* getNewScenerySurface(int flip,
    const char **xpm, float scale) {

    GdkPixbuf *pixbuf, *pixbuf1;
    pixbuf1 = gdk_pixbuf_new_from_xpm_data(
        (const char**) xpm);

    if (flip) {
        pixbuf = gdk_pixbuf_flip(pixbuf1, 1);
        g_clear_object(&pixbuf1);
    } else {
        pixbuf = pixbuf1;
    }

    int w, h;
    sscanf(xpm[0], "%d %d", &w, &h);

    w *= scale;
    h *= scale;
    if (w < 1) {
        w = 1;
    }
    if (h < 1) {
        h = 1;
    }
    if (w == 1 && h == 1) {
        h = 2;
    }

    GdkPixbuf* pixbufscaled =
        gdk_pixbuf_scale_simple(pixbuf, w, h, GDK_INTERP_HYPER);

    cairo_surface_t* surface = 
        gdk_cairo_surface_create_from_pixbuf(pixbufscaled, 0, NULL);

    g_clear_object(&pixbuf);
    g_clear_object(&pixbufscaled);

    return surface;
}

/***********************************************************
 * Fallen snow and trees must have been initialized
 * tree coordinates and so are recalculated here.
 */
int updateSceneryFrame() {
    if (Flags.Done) {
        return FALSE;
    }

    // ALways do fluff.
    if (mGlobal.RemoveFluff && mRemoveFluffAttempts++ > 2) {
        mGlobal.RemoveFluff = 0;
    }

    if (Flags.NoTrees) {
        return TRUE;
    }
    if (!mSceneryNeedsInit) {
        return TRUE;
    }

    mRemoveFluffAttempts = 0;
    mGlobal.RemoveFluff = 1;

    // Clear the snow window, free scenery objects.
    clearGlobalSnowWindow();

    mSceneryNeedsInit = false;
    for (int i = 0; i < NTrees; i++) {
        free(mSceneryInfoArray[i]);
    }
    free(mSceneryInfoArray);
    mSceneryInfoArray = NULL;

    cairo_region_destroy(mGlobal.gSnowOnTreesRegion);
    cairo_region_destroy(mGlobal.TreeRegion);

    mGlobal.gSnowOnTreesRegion = cairo_region_create();
    mGlobal.TreeRegion = cairo_region_create();

    // determine which trees are to be used
    int *tmptreetype, ntemp;
    if (TreeRead) {
        TreeType = (int *)realloc(TreeType, 1 * sizeof(*TreeType));
        REALLOC_CHECK(TreeType);
        TreeType[0] = 0;
    } else {
        if (!strcmp("all", Flags.TreeType))
        // user wants all treetypes
        {
            ntemp = NUM_ALL_SCENE_TYPES;
            tmptreetype = (int *)malloc(sizeof(*tmptreetype) * ntemp);
            int i;
            for (i = 0; i < ntemp; i++) {
                tmptreetype[i] = i;
            }
        } else if (strlen(Flags.TreeType) == 0) {
            ntemp = NUM_SCENE_GRID_ITEMS;
            tmptreetype = (int *)malloc(sizeof(*tmptreetype) * ntemp);
            int i;
            for (i = 0; i < ntemp; i++) {
                tmptreetype[i] = i + 1;
            }
        } else {
            // decode string like "1,1,3,2,4"
            csvpos(Flags.TreeType, &tmptreetype, &ntemp);
        }

        NtreeTypes = 0;
        for (int i = 0; i < ntemp; i++) {
            if (tmptreetype[i] >= 0 && tmptreetype[i] <= NUM_SCENE_GRID_ITEMS) {
                int j;
                int unique = 1;
                // investigate if this is already contained in TreeType.
                // if so, do not use it. Note that this algorithm is not
                // good scalable, when ntemp is large (100 ...) one should
                // consider an algorithm involving qsort()
                //
                for (j = 0; j < NtreeTypes; j++) {
                    if (tmptreetype[i] == TreeType[j]) {
                        unique = 0;
                        break;
                    }
                }
                if (unique) {
                    TreeType = (int *)realloc(
                        TreeType, (NtreeTypes + 1) * sizeof(*TreeType));
                    REALLOC_CHECK(TreeType);
                    TreeType[NtreeTypes] = tmptreetype[i];
                    NtreeTypes++;
                }
            }
        }

        if (NtreeTypes == 0) {
            TreeType = (int *)realloc(TreeType, sizeof(*TreeType));
            REALLOC_CHECK(TreeType);
            TreeType[0] = DEFAULTTREETYPE;
            NtreeTypes++;
        }

        free(tmptreetype);
    }

    // determine placement of trees and NTrees:
    NTrees = 0;
    for (int i = 0; i < 4 * Flags.DesiredNumberOfTrees; i++) {
        if (NTrees >= Flags.DesiredNumberOfTrees) {
            break;
        }

        int tt = TreeType[randint(NtreeTypes)];

        int w = TreeWidth[tt];
        int h = TreeHeight[tt];

        int y1 = mGlobal.SnowWinHeight -
            mGlobal.MaxScrSnowDepth - h * treeScale;
        int y2 = mGlobal.SnowWinHeight *
            (1.0 - 0.01 * Flags.TreeFill);

        if (y2 > y1) {
            y1 = y2 + 1;
        }

        int x = randint(mGlobal.SnowWinWidth - w * treeScale);
        int y = y1 - randint(y1 - y2);

        float myScale = (1 - MinScale) * (y - y2) /
            (y1 - y2) + MinScale;
        myScale *= treeScale * 0.01 * Flags.TreeScale;

        cairo_rectangle_int_t grect = {x - 1, y - 1,
            (int) (myScale * w + 2), (int)(myScale * h + 2)};
        cairo_region_overlap_t in = cairo_region_contains_rectangle(
            mGlobal.TreeRegion, &grect);

        // No overlap considerations?
        if (!mGlobal.isDoubleBuffered || !Flags.Overlap) {
            if (in == CAIRO_REGION_OVERLAP_IN ||
                in == CAIRO_REGION_OVERLAP_PART) {
                continue; // Skiptree
            }
        }

        // Check overlap conditions.
        SceneryInfo* tree = (SceneryInfo*)
            malloc(sizeof(SceneryInfo));
        tree->type = tt;

        tree->x = x;
        tree->y = y;

        tree->w = w;
        tree->h = h;

        tree->rev = (drand48() > 0.5);
        tree->scale = myScale;
        tree->surface = NULL;

        cairo_region_t *r;
        switch (tt) {
            case -SOMENUMBER:
                r = gregionfromxpm((const char**) TreeXpm,
                    tree->rev, tree->scale);
                break;
            default:
                r = gregionfromxpm(xpmtrees[tt], tree->rev,
                    tree->scale);
                break;
        }

        cairo_region_translate(r, x, y);
        cairo_region_union(mGlobal.TreeRegion, r);
        cairo_region_destroy(r);

        NTrees++;
        mSceneryInfoArray = (SceneryInfo**)
            realloc(mSceneryInfoArray, NTrees * sizeof(SceneryInfo*));
        REALLOC_CHECK(mSceneryInfoArray);
        mSceneryInfoArray[NTrees - 1] = tree;
    }

    // Sort using y+h values of trees, so that higher
    // trees are painted first.
    qsort(mSceneryInfoArray, NTrees, sizeof(*mSceneryInfoArray),
        compareTrees);

    initSceneryModuleSurfaces();
    updateColorTree();

    mGlobal.OnTrees = 0;
    return TRUE;
}

/***********************************************************
 * This method ...
 */
int compareTrees(const void *a, const void *b) {
    SceneryInfo* ta = *(SceneryInfo**) a;
    SceneryInfo* tb = *(SceneryInfo**) b;

    return ta->y + ta->h * ta->scale -
           tb->y - tb->h * tb->scale;
}

/***********************************************************
 * This method applies TreeColor to xpmtree[0].
 */
void updateColorTree() {
    if (ExternalTree) {
        return;
    }

    // Change colorstring in xpm.
    const int imageStringLength = TreeHeight[0] + 3;
    char** imageString = (char**) malloc(
        imageStringLength * sizeof(char*));

    // XPM indexs @ 0, 1 & 2.
    imageString[0] = strdup(xpmtrees[0][0]);
    imageString[1] = strdup(xpmtrees[0][1]);

    imageString[2] = strdup(". c ");
    imageString[2] = (char*) realloc(imageString[2],
        strlen(imageString[2]) + strlen(Flags.TreeColor) + 1);
    strcat(imageString[2], Flags.TreeColor);

    // XPM indexs @ 3 .. n.
    for (int i = 3; i < imageStringLength; i++) {
        imageString[i] = strdup(xpmtrees[0][i]);
    }

    XpmAttributes attributes;
    attributes.valuemask = XpmDepth;
    attributes.depth = mGlobal.SnowWinDepth;

    for (int i = 0; i < 2; i++) {
        XFreePixmap(mGlobal.display, mColorableTreePixmap[0][i]);

        iXpmCreatePixmapFromData(mGlobal.display, mGlobal.SnowWin,
            (const char**) imageString, &mColorableTreePixmap[0][i],
            &TreeMaskPixmap[0][i], &attributes, i);
        sscanf(xpmtrees[0][0], "%d %d", &TreeWidth[0], &TreeHeight[0]);
    }

    for (int i = 0; i < NTrees; i++) {
        SceneryInfo *tree = mSceneryInfoArray[i];
        if (tree->type == 0) {
            tree->surface = getNewScenerySurface(tree->rev,
                (const char**) imageString, tree->scale);
        }
    }

    // Free each imageString and the base.
    for (int i = 0; i < imageStringLength; i++) {
        free(imageString[i]);
    }
    free(imageString);
}

/***********************************************************
 * This method ...
 */
int drawSceneryFrame(cairo_t *cr) {
    if (Flags.NoTrees) {
        return TRUE;
    }

    for (int i = 0; i < NTrees; i++) {
        SceneryInfo *tree = mSceneryInfoArray[i];
        cairo_set_source_surface(cr, tree->surface, tree->x, tree->y);
        my_cairo_paint_with_alpha(cr, ALPHA);
    }

    return TRUE;
}

/***********************************************************
 * This method ...
 */
void updateSceneryUserSettings() {
    UIDOS(TreeType, clearAndRedrawScenery(););
    UIDO(DesiredNumberOfTrees, clearAndRedrawScenery(););
    UIDO(TreeFill, clearAndRedrawScenery(););
    UIDO(TreeScale, clearAndRedrawScenery(););
    UIDO(NoTrees, if (!mGlobal.isDoubleBuffered) clearAndRedrawScenery(););

    UIDOS(TreeColor, updateColorTree(););

    if (isQPickerActive() &&
        !strcmp(getQPickerCallerName(), "TreeColorTAG") &&
        !isQPickerVisible()) {
        static char cbuffer[16];
        snprintf(cbuffer, 16, "#%02x%02x%02x", getQPickerRed(),
            getQPickerGreen(), getQPickerBlue());

        static GdkRGBA color;
        gdk_rgba_parse(&color, cbuffer);
        free(Flags.TreeColor);
        rgba2color(&color, &Flags.TreeColor);

        endQPickerDialog();
    }

    UIDO(Overlap, clearAndRedrawScenery(););

    if (appScalesHaveChanged(&mCurrentAppScale)) {
        setSceneryScale();
        clearAndRedrawScenery();
    }
}

/***********************************************************
 * This method ...
 */
void clearAndRedrawScenery() {
    mSceneryNeedsInit = true;

    reinit_treesnow_region(); // treesnow.c.
    clearGlobalSnowWindow();
}
