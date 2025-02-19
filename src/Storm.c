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

#include <assert.h>
#include <math.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include <gtk/gtk.h>

#include "Blowoff.h"
#include "clocks.h"
#include "ColorPicker.h"
#include "FallenSnow.h"
#include "Flags.h"
#include "hashtable.h"
#include "ixpm.h"
#include "MainWindow.h"
#include "pixmaps.h"
#include "PlasmaSnow.h"
#include "safe_malloc.h"
#include "snow_includes.h"
#include "Storm.h"
#include "StormItemSurface.h"
#include "treesnow.h"
#include "Utils.h"
#include "wind.h"
#include "Windows.h"


/***********************************************************
 ** Module globals and consts.
 **/
const int RANDOM_STORMITEM_COUNT = 300;
const int STORMITEMS_PERSEC_PERPIXEL = 30;

#define INITIAL_Y_SPEED 120

const double MAX_WIND_SENSITIVITY = 0.4;
const float STORM_ITEM_SPEED_ADJUSTMENT = 0.7;
const float STORM_ITEM_SIZE_ADJUSTMENT = 0.8;

bool mUpdateStormThreadInitialized = false;
double mUpdateStormThreadPrevTime = 0;
double mUpdateStormThreadStartTime = 0;
bool mStallingNewStormItems = false;

const float mWindSpeedMaxArray[] =
    { 100.0, 300.0, 600.0 };

float mStormItemsPerSecond = 0.0;
float mStormItemsSpeedFactor = 0.0;
int mPreviousStormScale = 100;

int mAllStormItemsShapeCount = 0;
char*** mAllStormItemsShapeList = NULL;

StormItemSurface* mAllStormItemSurfacesList = NULL;


#define STORM_SHAPE_FILENAME(x) snow##x##_xpm,
int mResourcesShapeCount = 0;
XPM_TYPE** mResourcesShapes[] = {ALL_STORM_FILENAMES NULL};
#undef STORM_SHAPE_FILENAME

// Flake color helper methods.
int mStormItemColorToggle = 0;
GdkRGBA mStormItemColor;


/***********************************************************
 ** This method initializes the storm module.
 **/
void initStormModule() {
    mResourcesShapeCount = getResourcesShapeCount();

    getAllStormItemsShapeList();
    getAllStormItemSurfacesList();
    setAllStormItemsShapeSizeAndColor();

    setStormItemSpeed();
    setStormItemsPerSecond();

    addMethodToMainloop(PRIORITY_DEFAULT,
        TIME_BETWEEN_STORM_THREAD_UPDATES,
        updateStormOnThread);
}

/** ***********************************************************
 ** Helper returns count of locally available XPM StormItems.
 **/
int getResourcesShapeCount() {
    int count = 0;
    while (mResourcesShapes[count]) {
        count++;
    }

    return count;
}

/***********************************************************
 ** This method loads and/or creates stormItem
 ** pixmap images combined from pre-loaded images
 ** and runtime generated ones.
 **/
void getAllStormItemsShapeList() {
    // Remove any existing Shape list.
    if (mAllStormItemsShapeList) {
        for (int i = 0; i < mAllStormItemsShapeCount; i++) {
            xpm_destroy(mAllStormItemsShapeList[i]);
        }
        free(mAllStormItemsShapeList);
    }

    // Create new combined Shape list.
    const int RESOURCES_SHAPE_COUNT =
        getResourcesShapeCount();
    mAllStormItemsShapeCount = RESOURCES_SHAPE_COUNT +
        RANDOM_STORMITEM_COUNT;
    char*** newShapesList = (char***) malloc(
        mAllStormItemsShapeCount * sizeof(char**));

    // First, set colors for loaded resource images.
    int lineCount;
    for (int i = 0; i < mResourcesShapeCount; i++) {
        xpm_set_color((char**) mResourcesShapes[i],
            &newShapesList[i], &lineCount, "snow");
    }

    // Then create new StormItems (snow) generated randomly.
    for (int i = 0; i < RANDOM_STORMITEM_COUNT; i++) {
        const int SIZE = Flags.ShapeSizeFactor;
        const int WIDTH = SIZE + (SIZE * drand48());
        const int HEIGHT = SIZE + (SIZE * drand48());
        createRandomStormItemShape(WIDTH, HEIGHT,
            &newShapesList[i + mResourcesShapeCount]);
    }

    mAllStormItemsShapeList = newShapesList;
}

/***********************************************************
 ** This method generated a random xpm pixmap
 ** with dimensions xpmWidth x xpmHeight.
 **
 ** The stormItem will be rotated, so the w and h of the resulting
 ** xpm will be different from the input w and h.
 **/
void createRandomStormItemShape(int w, int h, char*** xpm) {
    const char c = '.'; // imposed by xpm_set_color

    int nmax = w * h;
    float *x, *y;

    x = (float *)malloc(nmax * sizeof(float));
    y = (float *)malloc(nmax * sizeof(float));

    int i, j;
    float w2 = 0.5 * w;
    float h2 = 0.5 * h;

    y[0] = 0;
    x[0] = 0; // to have at least one pixel in the centre
    int n = 1;
    for (i = 0; i < h; i++) {
        float yy = i;
        if (yy > h2) {
            yy = h - yy;
        }
        float py = 2 * yy / h;
        for (j = 0; j < w; j++) {
            float xx = j;
            if (xx > w2) {
                xx = w - xx;
            }
            float px = 2 * xx / w;
            float p = 1.1 - (px * py);
            // printf("%d %d %f %f %f %f %f\n",j,i,y,x,px,py,p);
            if (drand48() > p) {
                if (n < nmax) {
                    y[n] = i - w2;
                    x[n] = j - h2;
                    n++;
                }
            }
        }
    }
    // rotate points with a random angle 0 .. pi
    float a = drand48() * 355.0 / 113.0;
    float *xa, *ya;
    xa = (float *)malloc(n * sizeof(float));
    ya = (float *)malloc(n * sizeof(float));

    for (i = 0; i < n; i++) {
        xa[i] = x[i] * cosf(a) - y[i] * sinf(a);
        ya[i] = x[i] * sinf(a) + y[i] * cosf(a);
    }

    float xmin = xa[0];
    float xmax = xa[0];
    float ymin = ya[0];
    float ymax = ya[0];

    for (i = 0; i < n; i++) {
        // smallest xa:
        if (xa[i] < xmin) {
            xmin = xa[i];
        }
        // etc ..
        if (xa[i] > xmax) {
            xmax = xa[i];
        }
        if (ya[i] < ymin) {
            ymin = ya[i];
        }
        if (ya[i] > ymax) {
            ymax = ya[i];
        }
    }

    int nw = ceilf(xmax - xmin + 1);
    int nh = ceilf(ymax - ymin + 1);
    if (nw == 1 && nh == 1) {
        nh = 2;
    }
    assert(nh > 0);

    *xpm = (char **)malloc((nh + 3) * sizeof(char *));
    char **X = *xpm;

    X[0] = (char *)malloc(80 * sizeof(char));
    snprintf(X[0], 80, "%d %d 2 1", nw, nh);

    X[1] = strdup("  c None");
    X[2] = (char *)malloc(80 * sizeof(char));
    snprintf(X[2], 80, "%c c black", c);

    int offset = 3;
    assert(nw >= 0);
    for (i = 0; i < nh; i++) {
        X[i + offset] = (char *)malloc((nw + 1) * sizeof(char));
    }

    for (i = 0; i < nh; i++) {
        for (j = 0; j < nw; j++) {
            X[i + offset][j] = ' ';
        }
        X[i + offset][nw] = 0;
    }

    for (i = 0; i < n; i++) {
        X[offset + (int)(ya[i] - ymin)][(int)(xa[i] - xmin)] = c;
    }

    free(x);
    free(y);

    free(xa);
    free(ya);
}

/** ***********************************************************
 ** This method initializes a ShapeSurface array for each Resource
 ** shape & Random shape in the StormItem Shapes Array.
 **/
void getAllStormItemSurfacesList() {
    mAllStormItemSurfacesList = (StormItemSurface*) malloc(
        mAllStormItemsShapeCount * sizeof(StormItemSurface));

    for (int i = 0; i < mAllStormItemsShapeCount; i++) {
        mAllStormItemSurfacesList[i].surface = NULL;
    }
}

/***********************************************************
 ** This method updates module based on User pref settings.
 **/
void respondToStormSettingsChanges() {
    UIDO(NoSnowFlakes,
        if (Flags.NoSnowFlakes) {
            clearGlobalSnowWindow();
        }
    );

    UIDO(SnowFlakesFactor,
        setStormItemsPerSecond();
    );

    UIDO(mStormItemsSpeedFactor,
        setStormItemSpeed();
    );

    UIDO(FlakeCountMax, );

    UIDO(ShapeSizeFactor,
        resetAllStormItemsShapeSizeAndColor();
        Flags.VintageFlakes = 0;
    );

    UIDOS(StormItemColor1,
        setAllStormItemsShapeSizeAndColor();
        clearGlobalSnowWindow();
    );

    if (isQPickerActive() && !strcmp(getQPickerColorTAG(),
        "SnowColorTAG") && !isQPickerVisible()) {
        char cbuffer[16];
        snprintf(cbuffer, 16, "#%02x%02x%02x", getQPickerRed(),
            getQPickerGreen(), getQPickerBlue());
        GdkRGBA color;
        gdk_rgba_parse(&color, cbuffer);
        free(Flags.StormItemColor1);
        rgba2color(&color, &Flags.StormItemColor1);
        endQPickerDialog();
        clearAllFallenSnowItems();
        clearGlobalSnowWindow();
    }

    UIDOS(StormItemColor2,
        setAllStormItemsShapeSizeAndColor();
        clearGlobalSnowWindow();
    );

    if (isQPickerActive() && !strcmp(getQPickerColorTAG(),
        "SnowColor2TAG") && !isQPickerVisible()) {
        char cbuffer[16];
        snprintf(cbuffer, 16, "#%02x%02x%02x", getQPickerRed(),
            getQPickerGreen(), getQPickerBlue());
        GdkRGBA color;
        gdk_rgba_parse(&color, cbuffer);
        free(Flags.StormItemColor2);
        rgba2color(&color, &Flags.StormItemColor2);
        endQPickerDialog();
        clearAllFallenSnowItems();
        clearGlobalSnowWindow();
    }

    if (appScalesHaveChanged(&mPreviousStormScale)) {
        setAllStormItemsShapeSizeAndColor();
    }
}

/***********************************************************
 ** This method sets the desired speed of the StormItem.
 **/
void setStormItemSpeed() {
    if (Flags.mStormItemsSpeedFactor < 10) {
        mStormItemsSpeedFactor = 0.01 * 10;
    } else {
        mStormItemsSpeedFactor = 0.01 * Flags.mStormItemsSpeedFactor;
    }
    mStormItemsSpeedFactor *= STORM_ITEM_SPEED_ADJUSTMENT;
}

/***********************************************************
 ** This method sets a Items-Per-Second throttle rate.
 **/
void setStormItemsPerSecond() {
    mStormItemsPerSecond = mGlobal.SnowWinWidth * 0.0015 *
        Flags.SnowFlakesFactor * 0.001 *
        STORMITEMS_PERSEC_PERPIXEL * mStormItemsSpeedFactor;
}

/***********************************************************
 ** This method sets the StormItem "fluff" state.
 **/
void setStormItemFluffState(StormItem* stormItem, float t) {
    if (stormItem->fluff) {
        return;
    }

    // Fluff it.
    stormItem->fluff = true;
    stormItem->flufftimer = 0;
    stormItem->flufftime = MAX(0.01, t);

    mGlobal.FluffCount++;
}

/***********************************************************
 ** This method sets the desired size of the StormItem.
 **/
void resetAllStormItemsShapeSizeAndColor() {
    getAllStormItemsShapeList();
    setAllStormItemsShapeSizeAndColor();

    if (!mGlobal.isDoubleBuffered) {
        clearGlobalSnowWindow();
    }
}

/** ***********************************************************
 ** This method updates stormItem pixmap image attributes
 ** such as color.
 **/
void setAllStormItemsShapeSizeAndColor() {
    for (int i = 0; i < mAllStormItemsShapeCount; i++) {
        // Get item base w/h, and rando shrink item size w/h.
        // Items stuck on sceneyRealPosition removes themself this way.
        int itemWidth, itemHeight;
        sscanf(mAllStormItemsShapeList[i][0], "%d %d",
            &itemWidth, &itemHeight);

        // Guard stormItem w/h.
        const float SIZE_ADJUST = Flags.Scale *
            mGlobal.WindowScale * STORM_ITEM_SIZE_ADJUSTMENT *
            0.01;

        itemWidth = MAX(itemWidth * SIZE_ADJUST, 1);
        itemHeight = MAX(itemHeight * SIZE_ADJUST, 1);
        if (itemWidth == 1 && itemHeight == 1) {
            itemHeight = 2;
        }

        // Reset each Shape base to StormColor.
        char** shapeString;
        int lineCount;
        xpm_set_color(mAllStormItemsShapeList[i], &shapeString,
            &lineCount, getNextStormShapeColorAsString());

        // Get new item draw surface & destroy existing surface.
        StormItemSurface* itemSurface =
            &mAllStormItemSurfacesList[i];
        if (itemSurface->surface) {
            cairo_surface_destroy(itemSurface->surface);
        }

        // Create new surface from base.
        GdkPixbuf* newStormItemSurface =
            gdk_pixbuf_new_from_xpm_data((const char**)
                shapeString);
        xpm_destroy(shapeString);

        // Scale it to new size.
        GdkPixbuf* pixbufscaled = gdk_pixbuf_scale_simple(
            newStormItemSurface, itemWidth, itemHeight,
            GDK_INTERP_HYPER);

        itemSurface->width = itemWidth;
        itemSurface->height = itemHeight;
        itemSurface->surface = gdk_cairo_surface_create_from_pixbuf(
            pixbufscaled, 0, NULL);

        g_clear_object(&pixbufscaled);
        g_clear_object(&newStormItemSurface);
    }
}

/***********************************************************
 ** This method adds a batch of items to the Storm window.
 **/
int updateStormOnThread() {
    if (Flags.shutdownRequested) {
        return false;
    }

    const double NOW = wallclock();
    if (mStallingNewStormItems) {
        mUpdateStormThreadPrevTime = NOW;
        return true;
    }

    if (!WorkspaceActive() || Flags.NoSnowFlakes) {
        mUpdateStormThreadPrevTime = NOW;
        return true;
    }

    // Initialize method globals.
    if (!mUpdateStormThreadInitialized) {
        mUpdateStormThreadPrevTime = NOW;
        mUpdateStormThreadStartTime = 0;
        mUpdateStormThreadInitialized = true;
    }

    // Sanity check. Catches after suspend or sleep.
    // UPDATE_ELAPSED_TIME could have a strange value.
    const double UPDATE_ELAPSED_TIME = NOW -
        mUpdateStormThreadPrevTime;
    if (UPDATE_ELAPSED_TIME < 0 ||
        UPDATE_ELAPSED_TIME > 10 * TIME_BETWEEN_STORM_THREAD_UPDATES) {
        mUpdateStormThreadPrevTime = NOW;
        printf("plasmasnow: Storm.c: updateStormOnThread() "
            "detected a thread stall (?).\n");
        return true;
    }

    // Start time when actually generating flakes.
    const int DESIRED_FLAKES = lrint((UPDATE_ELAPSED_TIME +
        mUpdateStormThreadStartTime) * mStormItemsPerSecond);
    if (DESIRED_FLAKES != 0) {
        for (int i = 0; i < DESIRED_FLAKES; i++) {
            StormItem* flake = createStormItem(-1);
            addStormItemToItemset(flake);
        }
        mUpdateStormThreadPrevTime = NOW;
        mUpdateStormThreadStartTime = 0;
        return true;
    }

    // Start time when not generating flakes.
    mUpdateStormThreadPrevTime = NOW;
    mUpdateStormThreadStartTime += UPDATE_ELAPSED_TIME;
    return true;
}

/***********************************************************
 ** This method creates a basic StormItem from itemType.
 **/
StormItem* createStormItem(int itemType) {

    // If itemType < 0, create random itemType.
    const int RESOURCES_SHAPE_COUNT =
        getResourcesShapeCount();
    if (itemType < 0) {
        itemType = Flags.VintageFlakes ?
            RESOURCES_SHAPE_COUNT * drand48() :
            RESOURCES_SHAPE_COUNT + drand48() *
                (mAllStormItemsShapeCount - RESOURCES_SHAPE_COUNT);
    }

    StormItem* stormItem = (StormItem*) malloc(sizeof(StormItem));
    stormItem->shapeType = itemType;

    // DEBUG Crashes this way
    static int mDebugSnowWhatFlake = 5;
    if ((int) stormItem->shapeType < 0) {
        if (mDebugSnowWhatFlake-- > 0) {
            printf("plasmasnow: Storm.c: createStormItem(%lu) "
                "BAD !!! Invalid negative itemType : %i.\n",
                (unsigned long) pthread_self(),
                (int) stormItem->shapeType);
        }
    }
    if ((int) stormItem->shapeType >= mAllStormItemsShapeCount) {
        if (mDebugSnowWhatFlake-- > 0) {
            printf("plasmasnow: Storm.c: createStormItem(%lu) "
                "BAD !!! Invalid positive itemType : %i.\n",
                (unsigned long) pthread_self(),
                (int) stormItem->shapeType);
        }
    }

    const int ITEM_WIDTH = mAllStormItemSurfacesList
        [stormItem->shapeType].width;
    const int ITEM_HEIGHT = mAllStormItemSurfacesList
        [stormItem->shapeType].height;

    stormItem->xRealPosition = randint(
        mGlobal.SnowWinWidth - ITEM_WIDTH);
    stormItem->yRealPosition = -randint(
        mGlobal.SnowWinHeight / 10) - ITEM_HEIGHT;

    stormItem->color = mStormItemColor;
    stormItem->survivesScreenEdges = true;
    stormItem->isFrozen = 0;

    stormItem->fluff = 0;
    stormItem->flufftimer = 0;
    stormItem->flufftime = 0;

    stormItem->xVelocity = (Flags.NoWind) ?
        0 : randint(mGlobal.NewWind) / 2;
    stormItem->yVelocity = stormItem->initialYVelocity;

    stormItem->massValue = drand48() + 0.1;
    stormItem->initialYVelocity = INITIAL_Y_SPEED *
        sqrt(stormItem->massValue);
    stormItem->windSensitivity = drand48() *
        MAX_WIND_SENSITIVITY;

    return stormItem;
}

/***********************************************************
 ** Itemset hashtable helper - Add new all items.
 **/
void addStormItemToItemset(StormItem* stormItem) {
    set_insert(stormItem);

    addMethodWithArgToMainloop(PRIORITY_HIGH,
        TIME_BETWEEN_STORMITEM_THREAD_UPDATES,
        (GSourceFunc) updateStormItemOnThread, stormItem);

    mGlobal.stormItemCount++;
}

/***********************************************************
 ** This method updates a stormItem object.
 **/
int updateStormItemOnThread(StormItem* stormItem) {
    if (!WorkspaceActive() || Flags.NoSnowFlakes) {
        return true;
    }

    // Candidate for removal?
    if (mGlobal.RemoveFluff) {
        if (stormItem->fluff || stormItem->isFrozen) {
            eraseStormItemInItemset(stormItem);
            removeStormItemInItemset(stormItem);
            return false;
        }
    }

    // Candidate for removal?
    if (mStallingNewStormItems) {
        eraseStormItemInItemset(stormItem);
        removeStormItemInItemset(stormItem);
        return false;
    }

    // Candidate for removal?
    if (stormItem->fluff &&
        stormItem->flufftimer > stormItem->flufftime) {
        eraseStormItemInItemset(stormItem);
        removeStormItemInItemset(stormItem);
        return false;
    }

    // Look ahead to the flakes new x/y position.
    const double stormItemUpdateTime =
        TIME_BETWEEN_STORMITEM_THREAD_UPDATES;

    float newFlakeXPos = stormItem->xRealPosition +
        (stormItem->xVelocity * stormItemUpdateTime) *
        mStormItemsSpeedFactor;
    float newFlakeYPos = stormItem->yRealPosition +
        (stormItem->yVelocity * stormItemUpdateTime) *
        mStormItemsSpeedFactor;

    // Update stormItem based on "fluff" status.
    if (stormItem->fluff) {
        if (!stormItem->isFrozen) {
            stormItem->xRealPosition = newFlakeXPos;
            stormItem->yRealPosition = newFlakeYPos;
        }
        stormItem->flufftimer += stormItemUpdateTime;
        return true;
    }

    // Low probability to remove each, High probability
    // To remove blown-off.
    const bool SHOULD_KILL_STORMITEMS =
        ((mGlobal.stormItemCount - mGlobal.FluffCount) >=
            Flags.FlakeCountMax);

    if (SHOULD_KILL_STORMITEMS) {
        if ((!stormItem->survivesScreenEdges && drand48() > 0.3) ||
            (drand48() > 0.9)) {
            setStormItemFluffState(stormItem, 0.51);
            return true;
        }
    }

    // Update stormItem speed in X Direction.
    if (!Flags.NoWind) {
        // Calc speed.
        float newXVel = stormItemUpdateTime *
            stormItem->windSensitivity / stormItem->massValue;
        if (newXVel > 0.9) {
            newXVel = 0.9;
        }
        if (newXVel < -0.9) {
            newXVel = -0.9;
        }
        stormItem->xVelocity += newXVel *
            (mGlobal.NewWind - stormItem->xVelocity);

        // Apply speed limits.
        const float xVelMax = mWindSpeedMaxArray[mGlobal.Wind] * 2;
        if (fabs(stormItem->xVelocity) > xVelMax) {
            if (stormItem->xVelocity > xVelMax) {
                stormItem->xVelocity = xVelMax;
            }
            if (stormItem->xVelocity < -xVelMax) {
                stormItem->xVelocity = -xVelMax;
            }
        }
    }

    // Update stormItem speed in Y Direction.
    stormItem->yVelocity += INITIAL_Y_SPEED * (drand48() - 0.4) * 0.1;
    if (stormItem->yVelocity > stormItem->initialYVelocity * 1.5) {
        stormItem->yVelocity = stormItem->initialYVelocity * 1.5;
    }

    // If stormItem is frozen, we're done.
    if (stormItem->isFrozen) {
        return true;
    }

    // Flake w/h.
    const int ITEM_WIDTH = mAllStormItemSurfacesList
        [stormItem->shapeType].width;
    const int ITEM_HEIGHT = mAllStormItemSurfacesList
        [stormItem->shapeType].height;

    // Update stormItem based on status.
    if (stormItem->survivesScreenEdges) {
        if (newFlakeXPos < -ITEM_WIDTH) {
            newFlakeXPos += mGlobal.SnowWinWidth - 1;
        }
        if (newFlakeXPos >= mGlobal.SnowWinWidth) {
            newFlakeXPos -= mGlobal.SnowWinWidth;
        }
    } else {
        // Remove it when it goes left or right
        // out of the window.
        if (newFlakeXPos < 0 ||
            newFlakeXPos >= mGlobal.SnowWinWidth) {
            removeStormItemInItemset(stormItem);
            return false;
        }
    }

    // Remove stormItem if it falls below bottom of screen.
    if (newFlakeYPos >= mGlobal.SnowWinHeight) {
        removeStormItemInItemset(stormItem);
        return false;
    }

    // New Flake position as Ints.
    const int NEW_STORMITEM_INT_X_POS = lrintf(newFlakeXPos);
    const int NEW_STORMITEM_INT_Y_POS = lrintf(newFlakeYPos);

    // Fallen interaction.
    if (!stormItem->fluff) {
        lockFallenSnowSemaphore();
        if (isStormItemFallen(stormItem, NEW_STORMITEM_INT_X_POS,
                NEW_STORMITEM_INT_Y_POS)) {
            removeStormItemInItemset(stormItem);
            unlockFallenSnowSemaphore();
            return false;
        }
        unlockFallenSnowSemaphore();
    }

    // Flake NEW_STORMITEM_INT_X_POS/NEW_STORMITEM_INT_Y_POS.
    const int NEW_STORMITEM_REAL_X_POS = lrintf(stormItem->xRealPosition);
    const int NEW_STORMITEM_REAL_Y_POS = lrintf(stormItem->yRealPosition);

    // check if stormItem is touching or in gSnowOnTreesRegion
    // if so: remove it
    if (mGlobal.Wind != 2 &&
        !Flags.NoKeepSnowOnTrees &&
        !Flags.NoTrees) {
        cairo_rectangle_int_t grec = {NEW_STORMITEM_REAL_X_POS,
            NEW_STORMITEM_REAL_Y_POS, ITEM_WIDTH, ITEM_HEIGHT};
        cairo_region_overlap_t in = cairo_region_contains_rectangle(
            mGlobal.gSnowOnTreesRegion, &grec);
        if (in == CAIRO_REGION_OVERLAP_PART ||
            in == CAIRO_REGION_OVERLAP_IN) {
            setStormItemFluffState(stormItem, 0.4);
            stormItem->isFrozen = 1;
            return true;
        }

        // check if stormItem is touching TreeRegion. If so: add snow to
        // gSnowOnTreesRegion.
        cairo_rectangle_int_t grect = {
            NEW_STORMITEM_REAL_X_POS, NEW_STORMITEM_REAL_Y_POS,
            ITEM_WIDTH, ITEM_HEIGHT};
        in = cairo_region_contains_rectangle(mGlobal.TreeRegion, &grect);

        // so, part of the stormItem is in TreeRegion.
        // For each bottom pixel of the stormItem:
        //   find out if bottompixel is in TreeRegion
        //   if so:
        //     move upwards until pixel is not in TreeRegion
        //     That pixel will be designated as snow-on-tree
        // Only one snow-on-tree pixel has to be found.
        if (in == CAIRO_REGION_OVERLAP_PART) {
            int found = 0;
            int xfound, yfound;
            for (int i = 0; i < ITEM_WIDTH; i++) {
                if (found) {
                    break;
                }

                int ybot = NEW_STORMITEM_REAL_Y_POS + ITEM_HEIGHT;
                int xbot = NEW_STORMITEM_REAL_X_POS + i;
                cairo_rectangle_int_t grect = {xbot, ybot, 1, 1};

                cairo_region_overlap_t in =
                    cairo_region_contains_rectangle(mGlobal.TreeRegion, &grect);

                // If bottom pixel not in TreeRegion, skip.
                if (in != CAIRO_REGION_OVERLAP_IN) {
                    continue;
                }

                // move upwards, until pixel is not in TreeRegion
                for (int j = ybot - 1; j >= NEW_STORMITEM_REAL_Y_POS; j--) {
                    cairo_rectangle_int_t grect = {xbot, j, 1, 1};

                    cairo_region_overlap_t in = cairo_region_contains_rectangle(
                        mGlobal.TreeRegion, &grect);
                    if (in != CAIRO_REGION_OVERLAP_IN) {
                        // pixel (xbot,j) is snow-on-tree
                        found = 1;
                        cairo_rectangle_int_t grec;
                        grec.x = xbot;
                        int p = 1 + drand48() * 3;
                        grec.y = j - p + 1;
                        grec.width = p;
                        grec.height = p;
                        cairo_region_union_rectangle(mGlobal.gSnowOnTreesRegion,
                            &grec);

                        if (Flags.BlowSnow && mGlobal.OnTrees < Flags.MaxOnTrees) {
                            mGlobal.SnowOnTrees[mGlobal.OnTrees].x = grec.x;
                            mGlobal.SnowOnTrees[mGlobal.OnTrees].y = grec.y;
                            mGlobal.OnTrees++;
                        }

                        xfound = grec.x;
                        yfound = grec.y;
                        break;
                    }
                }
            }

            // TODO: Huh? who?
            // Do not erase: this gives bad effects
            // in fvwm-like desktops.
            if (found) {
                stormItem->isFrozen = 1;
                setStormItemFluffState(stormItem, 0.6);

                StormItem* newflake = (Flags.VintageFlakes) ?
                    createStormItem(0) : createStormItem(-1);

                newflake->xRealPosition = xfound;
                newflake->yRealPosition = yfound -
                    mAllStormItemSurfacesList[1].height * 0.3f;
                newflake->isFrozen = 1;
                setStormItemFluffState(newflake, 8);

                addStormItemToItemset(newflake);
                return true;
            }
        }
    }

    stormItem->xRealPosition = newFlakeXPos;
    stormItem->yRealPosition = newFlakeYPos;
    return true;
}

/***********************************************************
 ** This method stalls new StormItem creation events.
 **/
int stallCreatingStormItems() {
    if (Flags.shutdownRequested) {
        return false;
    }

    mStallingNewStormItems = (mGlobal.stormItemCount > 0);
    return mStallingNewStormItems;
}

/***********************************************************
 ** Itemset hashtable helper - Draw all items.
 **/
int drawAllStormItemsInItemset(cairo_t* cr) {
    if (Flags.NoSnowFlakes) {
        return true;
    }

    set_begin();

    StormItem* stormItem;
    while ((stormItem = (StormItem*) set_next())) {
        cairo_set_source_surface(cr,
            mAllStormItemSurfacesList[stormItem->shapeType].surface,
            stormItem->xRealPosition, stormItem->yRealPosition);

        // Fluff across time, and guard the lower bound.
        double alpha = (0.01 * (100 - Flags.Transparency));
        if (stormItem->fluff) {
            alpha *= (1 - stormItem->flufftimer / stormItem->flufftime);
        }
        if (alpha < 0) {
            alpha = 0;
        }

        // Draw.
        if (mGlobal.isDoubleBuffered ||
            !(stormItem->isFrozen || stormItem->fluff)) {
            my_cairo_paint_with_alpha(cr, alpha);
        }

        // Then update.
        stormItem->xIntPosition = lrint(stormItem->xRealPosition);
        stormItem->yIntPosition = lrint(stormItem->yRealPosition);
    }

    return true;
}

/***********************************************************
 ** This method erases a single stormItem pixmap from the display.
 **/
void eraseStormItemInItemset(StormItem* stormItem) {
    if (mGlobal.isDoubleBuffered) {
        return;
    }

    const int xPos = stormItem->xIntPosition - 1;
    const int yPos = stormItem->yIntPosition - 1;

    const int ITEM_WIDTH = mAllStormItemSurfacesList
        [stormItem->shapeType].width + 2;
    const int ITEM_HEIGHT = mAllStormItemSurfacesList
        [stormItem->shapeType].height + 2;

    clearDisplayArea(mGlobal.display, mGlobal.SnowWin,
        xPos, yPos, ITEM_WIDTH, ITEM_HEIGHT, mGlobal.xxposures);
}

/***********************************************************
 ** Itemset hashtable helper - Remove all items from the list.
 **/
int removeAllStormItemsInItemset() {
    set_begin();

    StormItem* stormItem;
    while ((stormItem = (StormItem*) set_next())) {
        eraseStormItemInItemset(stormItem);
    }

    return true;
}

/***********************************************************
 ** Itemset hashtable helper - Remove a specific item from the list.
 **
 ** Calls to this method from the mainloop() *MUST* be followed by
 ** a 'return false;' to remove this stormItem from the g_timeout
 ** callback.
 **/
void removeStormItemInItemset(StormItem* stormItem) {
    if (stormItem->fluff) {
        mGlobal.FluffCount--;
    }

    set_erase(stormItem);
    free(stormItem);

    mGlobal.stormItemCount--;
}

/***********************************************************
 ** These are helper methods for ItemColor.
 **/
void setStormShapeColor(GdkRGBA itemColor) {
    mStormItemColor = itemColor;
}

/***********************************************************
 ** These are helper methods for ItemColor.
 **/
GdkRGBA getNextStormShapeColorAsRGB() {
    // Toggle color switch.
    mStormItemColorToggle = (mStormItemColorToggle == 0) ? 1 : 0;

    // Update color.
    GdkRGBA nextColor;
    if (mStormItemColorToggle == 0) {
        gdk_rgba_parse(&nextColor, Flags.StormItemColor1);
    } else {
        gdk_rgba_parse(&nextColor, Flags.StormItemColor2);
    }
    setStormShapeColor(nextColor);

    return nextColor;
}

/***********************************************************
 ** These are helper methods for ItemColor.
 **/
GdkRGBA getRGBStormShapeColorFromString(char* colorString) {
    GdkRGBA result;
    gdk_rgba_parse(&result, colorString);
    return result;
}

/***********************************************************
 ** These are helper methods for ItemColor.
 **/
char* getNextStormShapeColorAsString() {
    // Toggle color switch.
    mStormItemColorToggle = (mStormItemColorToggle == 0) ? 1 : 0;

    // Update color.
    GdkRGBA nextColor;
    if (mStormItemColorToggle == 0) {
        gdk_rgba_parse(&nextColor, Flags.StormItemColor1);
    } else {
        gdk_rgba_parse(&nextColor, Flags.StormItemColor2);
    }
    setStormShapeColor(nextColor);

    // Return result as string.
    return (mStormItemColorToggle == 0) ?
        Flags.StormItemColor1 : Flags.StormItemColor2;
}

/***********************************************************
 ** This method updates window surfaces and / or desktop bottom
 ** if stormItem drops onto it.
 */
bool isStormItemFallen(StormItem* stormItem,
    int xPosition, int yPosition) {

    FallenSnow* fsnow = mGlobal.FsnowFirst;
    while (fsnow) {
        if (fsnow->winInfo.hidden) {
            fsnow = fsnow->next;
            continue;
        }

        if (fsnow->winInfo.window != None &&
            !isFallenSnowVisibleOnWorkspace(fsnow) &&
            !fsnow->winInfo.sticky) {
            fsnow = fsnow->next;
            continue;
        }

        if (xPosition < fsnow->x ||
            xPosition > fsnow->x + fsnow->w ||
            yPosition >= fsnow->y + 2) {
            fsnow = fsnow->next;
            continue;
        }

        // Flake hits first FallenSnow & we're done.
        // StormItem hits first FallenItem & we're done.
        const int ITEM_WIDTH = mAllStormItemSurfacesList
            [stormItem->shapeType].width;

        int istart = xPosition - fsnow->x;
        if (istart < 0) {
            istart = 0;
        }
        int imax = istart + ITEM_WIDTH;
        if (imax > fsnow->w) {
            imax = fsnow->w;
        }

        for (int i = istart; i < imax; i++) {
            if (yPosition > fsnow->y - fsnow->snowHeight[i] - 1) {
                if (fsnow->snowHeight[i] < fsnow->maxSnowHeight[i]) {
                    updateFallenSnowWithSnow(fsnow,
                        xPosition - fsnow->x, ITEM_WIDTH);
                }

                if (canSnowCollectOnFallen(fsnow)) {
                    setStormItemFluffState(stormItem, .9);
                    if (!stormItem->fluff) {
                        return true;
                    }
                }
                return false;
            }
        }

        // Otherwise, loop thru all.
        fsnow = fsnow->next;
    }

    return false;
}

/***********************************************************
 ** This method prints a StormItem's detail.
 **/
void logStormItem(StormItem* stormItem) {
    printf("plasmasnow: Storm.c: stormItem: "
        "%p xRealPos: %6.0f yRealPos: %6.0f xVelocity: %6.0f yVelocity: %6.0f ws: %f "
        "isFrozen: %d fluff: %6.0d ftr: %8.3f ft: %8.3f\n",
        (void*) stormItem, stormItem->xRealPosition, stormItem->yRealPosition,
        stormItem->xVelocity, stormItem->yVelocity,
        stormItem->windSensitivity, stormItem->isFrozen,
        stormItem->fluff, stormItem->flufftimer, stormItem->flufftime);
}
