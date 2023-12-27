/* -copyright-
#-#
#-# plasmasnow: Let it snow on your desktop
#-# Copyright (C) 1984,1988,1990,1993-1995,2000-2001 Rick Jansen
#-# 	      2019,2020,2021,2022,2023 Willem Vermin
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

#define DEFAULTTREETYPE 2

#define NOTACTIVE (!WorkspaceActive())

#include <pthread.h>

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
#include <X11/Intrinsic.h>
#include <X11/xpm.h>
#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

static int do_initbaum();
static void ReInitTree0(void);
static void InitTreePixmaps(void);

static cairo_surface_t *tree_surface(int, const char**, float);
static void create_tree_surfaces(void);

static void create_tree_dimensions(int tt);
static int compartrees(const void *a, const void *b);
static void setScale(void);






static int NTrees = 0;
static int NtreeTypes = 0;
static int *TreeType = NULL;
static int TreeRead = 0;

static int TreeWidth[MAXTREETYPE + 1];
static int TreeHeight[MAXTREETYPE + 1];

static int ExternalTree = False;
static int Newtrees = 1;

static char **TreeXpm = NULL;

static Pixmap TreePixmap[MAXTREETYPE + 1][2];
static Pixmap TreeMaskPixmap[MAXTREETYPE + 1][2];

static Treeinfo **Trees = NULL;

static float treeScale = 1.0;
static const float LocalScale = 0.7; // correction scale: if scenery is always
                                     // too small, enlarge this and vice versa
static float MinScale = 0.6;         // scale for items with low y-coordinate







bool isQPickerActive();
char* getQPickerCallerName();
bool isQPickerVisible();
bool isQPickerTerminated();

int getQPickerRed();
int getQPickerBlue();
int getQPickerGreen();

void endQPickerDialog();










void scenery_init() {
    {
        P("scenery_init\n");
        // sanitize Flags.TreeType
        int *a;
        int n;
        csvpos(Flags.TreeType, &a, &n);
        int i;
        int *b = (int *)malloc(sizeof(int) * n);
        int m = 0;
        for (i = 0; i < n; i++) {
            if (a[i] >= 0 && a[i] <= MAXTREETYPE - 1) {
                b[m] = a[i];
                m++;
            }
        }
        free(Flags.TreeType);
        vsc(&Flags.TreeType, b, m);
        WriteFlags();
        free(a);
        free(b);
    }
    P("treecolor: %s\n", Flags.TreeColor);
    setScale();
    P("treeScale: %f\n", treeScale);
    // global.TreeRegion = XCreateRegion();
    global.TreeRegion = cairo_region_create();
    InitTreePixmaps();
    add_to_mainloop(PRIORITY_DEFAULT, time_initbaum, do_initbaum);
}










void setScale() {
    treeScale = LocalScale * 0.01 * Flags.Scale * global.WindowScale;
}









int compartrees(const void *a, const void *b) {
    Treeinfo *ta = *(Treeinfo **)a;
    Treeinfo *tb = *(Treeinfo **)b;
    P("compartrees %d %d %d %d\n", ta->y, tb->y, ta->h, tb->h);
    return ta->y + ta->h * ta->scale - tb->y - tb->h * tb->scale;
}














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













void scenery_ui() {
    UIDOS(TreeType, RedrawTrees(););
    UIDO(DesiredNumberOfTrees, RedrawTrees(););
    UIDO(TreeFill, RedrawTrees(););
    UIDO(TreeScale, RedrawTrees(););
    UIDO(NoTrees, if (!global.IsDouble) RedrawTrees(););

    UIDOS(TreeColor, ReInitTree0(););
    if (isQPickerActive() && !strcmp(getQPickerCallerName(), "TreeColorTAG") &&
        !isQPickerVisible()) {
        static char cbuffer[16];
        snprintf(cbuffer, 16, "#%02x%02x%02x", getQPickerRed(),
            getQPickerGreen(), getQPickerBlue());
        fprintf(stdout, "ui.c: [qcolorcode]    TreeColor [%s].\n", cbuffer);

        static GdkRGBA color;
        gdk_rgba_parse(&color, cbuffer);
        free(Flags.TreeColor);
        rgba2color(&color, &Flags.TreeColor);

        endQPickerDialog();
    }

    UIDO(Overlap, RedrawTrees(););

    static int prev = 100;
    if (ScaleChanged(&prev)) {
        setScale();
        RedrawTrees();
    }
}







void RedrawTrees() {
    Newtrees = 1; // this signals initbaum to recreate the trees
    reinit_treesnow_region();
    ClearScreen();
}





void EraseTrees() { RedrawTrees(); }



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

void create_tree_dimensions(int tt) {
    sscanf(xpmtrees[tt][0], "%d %d", &TreeWidth[tt], &TreeHeight[tt]);
}






// fallen snow and trees must have been initialized
// tree coordinates and so are recalculated here, in anticipation
// of a changed window size
// The function returns immediately if Newtrees==0, otherwize

// an attempt
// is done to place the DesiredNumberOfTrees
int do_initbaum() {

    if (Flags.Done) {
        return FALSE;
    }

    static int count = 0;
    if (global.RemoveFluff && count++ > 2) {
        global.RemoveFluff = 0;
    }

    if (Flags.NoTrees || Newtrees == 0) {
        return TRUE;
    }

    ClearScreen();

    Newtrees = 0;
    int i, h, w;

    for (i = 0; i < NTrees; i++) {
        free(Trees[i]);
    }

    free(Trees);
    Trees = NULL;

    global.RemoveFluff = 1;
    count = 0;

    cairo_region_destroy(global.gSnowOnTreesRegion);
    cairo_region_destroy(global.TreeRegion);

    global.gSnowOnTreesRegion = cairo_region_create();
    global.TreeRegion = cairo_region_create();

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
            ntemp = 1 + MAXTREETYPE;
            tmptreetype = (int *)malloc(sizeof(*tmptreetype) * ntemp);
            int i;
            for (i = 0; i < ntemp; i++) {
                tmptreetype[i] = i;
            }
        } else if (strlen(Flags.TreeType) == 0)
        // default: use 1..MAXTREETYPE-1
        {
            ntemp = MAXTREETYPE - 1;
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
        for (i = 0; i < ntemp; i++) {
            if (tmptreetype[i] >= 0 && tmptreetype[i] <= MAXTREETYPE - 1) {
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
    for (i = 0; i < 4 * Flags.DesiredNumberOfTrees; i++) {
        if (NTrees >= Flags.DesiredNumberOfTrees) {
            break;
        }


        //#ifdef USE_EXTRATREE
        //      if (global.Language && !strcmp(global.Language,"ru") && drand48() < 0.3)
        //     tt = MAXTREETYPE;
        //      if (drand48() < 0.02)
        //     tt = MAXTREETYPE;
        //#endif

        int tt = TreeType[randint(NtreeTypes)];
        w = TreeWidth[tt];
        h = TreeHeight[tt];
        int y1 = global.SnowWinHeight - global.MaxScrSnowDepth - h * treeScale;
        int y2 = global.SnowWinHeight * (1.0 - 0.01 * Flags.TreeFill);
        if (y2 > y1) {
            y1 = y2 + 1;
        }
        int x = randint(global.SnowWinWidth - w * treeScale);
        int y = y1 - randint(y1 - y2);
        float myScale = (1 - MinScale) * (y - y2) / (y1 - y2) + MinScale;
        myScale *= treeScale * 0.01 * Flags.TreeScale;
        cairo_rectangle_int_t grect = {
            x - 1, y - 1, (int)(myScale * w + 2), (int)(myScale * h + 2)};
        cairo_region_overlap_t in =
            cairo_region_contains_rectangle(global.TreeRegion, &grect);

        // no overlap considerations if:
        if (!global.IsDouble || !Flags.Overlap) {
            if (in == CAIRO_REGION_OVERLAP_IN ||
                in == CAIRO_REGION_OVERLAP_PART) {
                P("skiptree\n");
                continue;
            }
        }

        int flop = (drand48() > 0.5);
        Treeinfo *tree = (Treeinfo *)malloc(sizeof(Treeinfo));
        tree->x = x;
        tree->y = y;
        tree->w = w;
        tree->h = h;
        tree->type = tt;
        tree->rev = flop;
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
        cairo_region_union(global.TreeRegion, r);
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

    global.OnTrees = 0;
    return TRUE;
}




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




void InitTreePixmaps() {

    XpmAttributes attributes;
    attributes.valuemask = XpmDepth;
    attributes.depth = global.SnowWinDepth;

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
                iXpmCreatePixmapFromData(global.display, global.SnowWin,
                    (const char **)TreeXpm, &TreePixmap[0][i],
                    &TreeMaskPixmap[0][i], &attributes, i);
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
        int i;
        for (i = 0; i < 2; i++) {
            int tt;
            for (tt = 0; tt <= MAXTREETYPE; tt++) {
                iXpmCreatePixmapFromData(global.display, global.SnowWin,
                    xpmtrees[tt], &TreePixmap[tt][i], &TreeMaskPixmap[tt][i],
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

    global.OnTrees = 0;
}




// apply TreeColor to xpmtree[0] and xpmtree[1]
void ReInitTree0() {
    if (ExternalTree) {
        return;
    }

    XpmAttributes attributes;
    attributes.valuemask = XpmDepth;
    attributes.depth = global.SnowWinDepth;

    int i;
    int n = TreeHeight[0] + 3;
    char **xpmtmp = (char **)  malloc(n * sizeof(char *));

    int j;
    for (j = 0; j < 2; j++) {
        xpmtmp[j] = strdup(xpmtrees[0][j]);
    }

    xpmtmp[2] = strdup(". c ");
    xpmtmp[2] = (char *)realloc(
        xpmtmp[2], strlen(xpmtmp[2]) + strlen(Flags.TreeColor) + 1);
    strcat(xpmtmp[2], Flags.TreeColor);
    for (j = 3; j < n; j++) {
        xpmtmp[j] = strdup(xpmtrees[0][j]);
    }


    for (i = 0; i < 2; i++) {
        XFreePixmap(global.display, TreePixmap[0][i]);
        iXpmCreatePixmapFromData(global.display, global.SnowWin,
            (const char **)xpmtmp, &TreePixmap[0][i], &TreeMaskPixmap[0][i],
            &attributes, i);
        create_tree_dimensions(0);
    }

    for (i = 0; i < NTrees; i++) {
        Treeinfo *tree = Trees[i];

        if (tree->type == 0) {
            tree->surface =
                tree_surface(tree->rev, (const char **)xpmtmp, tree->scale);
        }
    }

    for (j = 0; j < n; j++) {
        free(xpmtmp[j]);
    }

    free(xpmtmp);
}
