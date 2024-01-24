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
#include <X11/Intrinsic.h>
#include <X11/xpm.h>
#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif


/***********************************************************
 * Module Method stubs.
 */
static int do_initbaum();
static void ReInitTree0(void);
static void InitTreePixmaps(void);

static cairo_surface_t *tree_surface(int, const char**, float);
static void create_tree_surfaces(void);

static void create_tree_dimensions(int tt);
static int compartrees(const void *a, const void *b);
static void setScale(void);

bool isQPickerActive();
char* getQPickerCallerName();
bool isQPickerVisible();
bool isQPickerTerminated();
int getQPickerRed();
int getQPickerBlue();
int getQPickerGreen();
void endQPickerDialog();


/***********************************************************
 * Module consts.
 */
#define DEFAULTTREETYPE 2
#define NOTACTIVE (!WorkspaceActive())

static int NTrees = 0;
static int TreeRead = 0;

static int NtreeTypes = 0;
static int *TreeType = NULL;

static int TreeWidth[NUM_ALL_SCENE_TYPES];
static int TreeHeight[NUM_ALL_SCENE_TYPES];

static int ExternalTree = False;
static int Newtrees = 1;

static char **TreeXpm = NULL;

static Pixmap mColorableTreePixmap[NUM_ALL_SCENE_TYPES] [2];
static Pixmap TreeMaskPixmap[NUM_ALL_SCENE_TYPES] [2];

static Treeinfo **Trees = NULL;

static float treeScale = 1.0;
static const float LocalScale = 0.7; // correction scale: if scenery is always
                                     // too small, enlarge this and vice versa
static float MinScale = 0.6;         // scale for items with low y-coordinate


/***********************************************************
 * Main Scenery methods.
 */
void scenery_init() {
    int *a;
    int n;
    csvpos(Flags.TreeType, &a, &n);
    int *b = (int *)malloc(sizeof(int) * n);

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

    setScale();
    mGlobal.TreeRegion = cairo_region_create();
    InitTreePixmaps();

    addMethodToMainloop(PRIORITY_DEFAULT, time_initbaum, do_initbaum);
}

/***********************************************************
 * Main Scenery method.
 */
void setScale() {
    treeScale = LocalScale * 0.01 * Flags.Scale * mGlobal.WindowScale;
}

/***********************************************************
 * Main Scenery method.
 */
int compartrees(const void *a, const void *b) {
    Treeinfo *ta = *(Treeinfo **)a;
    Treeinfo *tb = *(Treeinfo **)b;
    P("compartrees %d %d %d %d\n", ta->y, tb->y, ta->h, tb->h);
    return ta->y + ta->h * ta->scale - tb->y - tb->h * tb->scale;
}

/***********************************************************
 * Main Scenery method.
 */
int scenery_draw(cairo_t *cr) {
    int i;

    if (Flags.NoTrees) {
        return TRUE;
    }
    for (i = 0; i < NTrees; i++) {
        Treeinfo *tree = Trees[i];
        P("scenery: %d\n", tree->y + tree->h);
        cairo_set_source_surface(cr, tree->surface, tree->x, tree->y);
        my_cairo_paint_with_alpha(cr, ALPHA);
    }
    return TRUE;
}

/***********************************************************
 * Main Scenery method.
 */
void scenery_ui() {
    UIDOS(TreeType, RedrawTrees(););
    UIDO(DesiredNumberOfTrees, RedrawTrees(););
    UIDO(TreeFill, RedrawTrees(););
    UIDO(TreeScale, RedrawTrees(););
    UIDO(NoTrees, if (!mGlobal.isDoubleBuffered) RedrawTrees(););

    UIDOS(TreeColor, ReInitTree0(););
    if (isQPickerActive() && !strcmp(getQPickerCallerName(), "TreeColorTAG") &&
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

    UIDO(Overlap, RedrawTrees(););

    static int prev = 100;
    if (appScalesHaveChanged(&prev)) {
        setScale();
        RedrawTrees();
    }
}

/***********************************************************
 * Main Scenery method.
 */
void RedrawTrees() {
    Newtrees = 1; // this signals initbaum to recreate the trees
    reinit_treesnow_region();
    ClearScreen();
}

/***********************************************************
 * Main Scenery method.
 */
void EraseTrees() {
    RedrawTrees();
}

/***********************************************************
 * Main Scenery method.
 */
cairo_surface_t*
tree_surface(int flip, const char **xpm, float scale) {
    P("xpm[2]: %s\n", xpm[2]);
    GdkPixbuf *pixbuf, *pixbuf1;
    pixbuf1 = gdk_pixbuf_new_from_xpm_data((const char **)xpm);
    if (flip) {
        pixbuf = gdk_pixbuf_flip(pixbuf1, 1);
        g_clear_object(&pixbuf1);
    } else {
        pixbuf = pixbuf1;
    }

    int w, h;
    sscanf(xpm[0], "%d %d", &w, &h);
    P("tree_surface: %d %d %f\n", w, h, scale);
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
    GdkPixbuf *pixbufscaled =
        gdk_pixbuf_scale_simple(pixbuf, w, h, GDK_INTERP_HYPER);
    cairo_surface_t *surface =
        gdk_cairo_surface_create_from_pixbuf(pixbufscaled, 0, NULL);
    g_clear_object(&pixbuf);
    g_clear_object(&pixbufscaled);
    return surface;
}

/***********************************************************
 * Main Scenery method.
 */
void create_tree_dimensions(int tt) {
    sscanf(xpmtrees[tt][0], "%d %d", &TreeWidth[tt], &TreeHeight[tt]);
}

static int count = 0;

/***********************************************************
 * Fallen snow and trees must have been initialized
 * tree coordinates and so are recalculated here, in anticipation
 * of a changed window size. The function returns immediately
 * if Newtrees==0, otherwize an attempt is done to place
 * the DesiredNumberOfTrees
 */
int do_initbaum() {
    if (Flags.Done) {
        return FALSE;
    }

    if (mGlobal.RemoveFluff && count++ > 2) {
        mGlobal.RemoveFluff = 0;
    }
    if (Flags.NoTrees || Newtrees == 0) {
        return TRUE;
    }

    count = 0;
    mGlobal.RemoveFluff = 1;
    ClearScreen();

    Newtrees = 0;
    for (int i = 0; i < NTrees; i++) {
        free(Trees[i]);
    }

    free(Trees);
    Trees = NULL;

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

        int y1 = mGlobal.SnowWinHeight - mGlobal.MaxScrSnowDepth - h * treeScale;
        int y2 = mGlobal.SnowWinHeight * (1.0 - 0.01 * Flags.TreeFill);
        if (y2 > y1) {
            y1 = y2 + 1;
        }

        int x = randint(mGlobal.SnowWinWidth - w * treeScale);
        int y = y1 - randint(y1 - y2);

        float myScale = (1 - MinScale) * (y - y2) / (y1 - y2) + MinScale;
        myScale *= treeScale * 0.01 * Flags.TreeScale;

        cairo_rectangle_int_t grect = {
            x - 1, y - 1, (int)(myScale * w + 2), (int)(myScale * h + 2)};
        cairo_region_overlap_t in =
            cairo_region_contains_rectangle(mGlobal.TreeRegion, &grect);

        // No overlap considerations?
        if (!mGlobal.isDoubleBuffered || !Flags.Overlap) {
            if (in == CAIRO_REGION_OVERLAP_IN ||
                in == CAIRO_REGION_OVERLAP_PART) {
                continue; // Skiptree
            }
        }

        // Check overlap conditions.
        Treeinfo *tree = (Treeinfo *) malloc(sizeof(Treeinfo));
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
                r = gregionfromxpm((const char **)TreeXpm, tree->rev, tree->scale);
                break;
            default:
                r = gregionfromxpm(xpmtrees[tt], tree->rev, tree->scale);
                break;
        }

        cairo_region_translate(r, x, y);
        cairo_region_union(mGlobal.TreeRegion, r);
        cairo_region_destroy(r);

        NTrees++;
        Trees = (Treeinfo **)realloc(Trees, NTrees * sizeof(Treeinfo *));
        REALLOC_CHECK(Trees);
        Trees[NTrees - 1] = tree;
    }

    // sort using y+h values of trees, so that higher trees are painted first
    qsort(Trees, NTrees, sizeof(*Trees), compartrees);

    create_tree_surfaces();
    ReInitTree0();

    mGlobal.OnTrees = 0;
    return TRUE;
}

/***********************************************************
 * Main Scenery method.
 */
void create_tree_surfaces() {
    int i;
    for (i = 0; i < NTrees; i++) {
        Treeinfo *tree = Trees[i];
        if (tree->surface) {
            cairo_surface_destroy(tree->surface);
        }
        if (TreeRead) {
            tree->surface =
                tree_surface(tree->rev, (const char **)TreeXpm, tree->scale);
        } else {
            tree->surface = tree_surface(
                tree->rev, (const char **)xpmtrees[tree->type], tree->scale);
        }
    }
}

/***********************************************************
 * Main Scenery method.
 */
void InitTreePixmaps() {

    XpmAttributes attributes;
    attributes.valuemask = XpmDepth;
    attributes.depth = mGlobal.SnowWinDepth;

    char *path = NULL;
    FILE *f = HomeOpen("plasmasnow/pixmaps/tree.xpm", "r", &path);

    if (f) {
        // there seems to be a local definition of tree
        // set TreeType to some number, so we can respond accordingly
        TreeType = (int *)realloc(TreeType, sizeof(*TreeType));
        REALLOC_CHECK(TreeType);

        NtreeTypes = 1;
        TreeRead = 1;

        int rc = XpmReadFileToData(path, &TreeXpm);

        if (rc == XpmSuccess) {
            int i;
            for (i = 0; i < 2; i++) {
                P("iXpmCreatePixmapFromData %d\n", i);
                iXpmCreatePixmapFromData(mGlobal.display, mGlobal.SnowWin,
                    (const char **) TreeXpm,
                    &mColorableTreePixmap[0][i], &TreeMaskPixmap[0][i],
                    &attributes, i);
                create_tree_dimensions(0);
            }
            sscanf(*TreeXpm, "%d %d", &TreeWidth[0], &TreeHeight[0]);
            P("treexpm %d %d\n", TreeWidth[0], TreeHeight[0]);
            printf("using external tree: %s\n", path);
            fflush(stdout);
            ExternalTree = True;
            /*
               if (!Flags.NoMenu)
               printf("Disabling menu.\n");
               Flags.NoMenu = 1;
               */
        } else {
            printf("Invalid external xpm for tree given: %s\n", path);
            exit(1);
        }
        fclose(f);

    } else {
        for (int i = 0; i < 2; i++) {
            for (int tt = 0; tt <= NUM_BASE_SCENE_TYPES; tt++) {
                iXpmCreatePixmapFromData(mGlobal.display, mGlobal.SnowWin,
                    xpmtrees[tt],
                    &mColorableTreePixmap[tt][i], &TreeMaskPixmap[tt][i],
                    &attributes, i);
                sscanf(
                    xpmtrees[tt][0], "%d %d", &TreeWidth[tt], &TreeHeight[tt]);
                create_tree_dimensions(tt);
            }
        }
    }

    if (path) {
        free(path);
        path = NULL;
    }

    mGlobal.OnTrees = 0;
}

/***********************************************************
 * Main Scenery method.
 * apply TreeColor to xpmtree[0] and xpmtree[1]
 */
void ReInitTree0() {
    if (ExternalTree) {
        return;
    }

    int n = TreeHeight[0] + 3;
    char **xpmtmp = (char **)  malloc(n * sizeof(char *));
    // Change colorstring in xpm and vintage version xpm
    for (int j = 0; j < 2; j++) {
        xpmtmp[j] = strdup(xpmtrees[0][j]);
    }

    xpmtmp[2] = strdup(". c ");
    xpmtmp[2] = (char *) realloc(
        xpmtmp[2], strlen(xpmtmp[2]) + strlen(Flags.TreeColor) + 1);
    strcat(xpmtmp[2], Flags.TreeColor);

    for (int j = 3; j < n; j++) {
        xpmtmp[j] = strdup(xpmtrees[0][j]);
    }

    XpmAttributes attributes;
    attributes.valuemask = XpmDepth;
    attributes.depth = mGlobal.SnowWinDepth;
    for (int i = 0; i < 2; i++) {
        XFreePixmap(mGlobal.display, mColorableTreePixmap[0][i]);
        iXpmCreatePixmapFromData(mGlobal.display, mGlobal.SnowWin,
            (const char **) xpmtmp,
            &mColorableTreePixmap[0][i], &TreeMaskPixmap[0][i],
            &attributes, i);
        create_tree_dimensions(0);
    }

    for (int i = 0; i < NTrees; i++) {
        Treeinfo *tree = Trees[i];
        if (tree->type == 0) {
            tree->surface = tree_surface(tree->rev,
                (const char **) xpmtmp, tree->scale);
        }
    }

    for (int j = 0; j < n; j++) {
        free(xpmtmp[j]);
    }

    free(xpmtmp);
}
