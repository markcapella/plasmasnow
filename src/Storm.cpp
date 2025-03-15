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

#include <stdbool.h>

#include <X11/Xlib.h>

// Plasmasnow headers.
#include "PlasmaSnow.h"

#include "ClockHelper.h"
#include "ColorPicker.h"
#include "FallenSnow.h"
#include "Flags.h"
#include "ixpm.h"
#include "MainWindow.h"
#include "snow_includes.h"
#include "Storm.h"
#include "StormItem.h"
#include "StormItemSurface.h"
#include "Utils.h"
#include "Windows.h"


/***********************************************************
 ** Module globals and consts.
 **/
const int RANDOM_STORMITEM_COUNT = 300;
const int STORMITEMS_PERSEC_PERPIXEL = 30;

const float STORM_ITEM_SPEED_ADJUSTMENT = 0.7;
const float STORM_ITEM_SIZE_ADJUSTMENT = 0.8;

bool mUpdateStormThreadInitialized = false;
double mUpdateStormThreadPrevTime = 0;
double mUpdateStormThreadStartTime = 0;

bool mStallingNewStormItems = false;

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

bool mStormBackgroundThreadIsActive = false;


/***********************************************************
 ** This method initializes the storm module.
 **/
void initStormModule() {
    mResourcesShapeCount = getResourcesShapeCount();

    getAllStormItemsShapeList();
    getAllStormItemSurfacesList();
    setAllStormItemsShapeSizeAndColor();

    setStormItemsSpeedFactor();
    setStormItemsPerSecond();

    addMethodToMainloop(PRIORITY_DEFAULT,
        TIME_BETWEEN_STORM_THREAD_UPDATES,
        (GSourceFunc) updateStormOnThread);
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

/** ***********************************************************
 ** Storm class getter/setters for mAllStormItemsShapeCount.
 **/
int getAllStormItemsShapeCount() {
    return mAllStormItemsShapeCount;
}
void setAllStormItemsShapeCount(int count) {
    mAllStormItemsShapeCount = count;
}

/***********************************************************
 ** This method loads and/or creates stormItem
 ** pixmap images combined from pre-loaded images
 ** and runtime generated ones.
 **/
void getAllStormItemsShapeList() {
    // Remove any existing Shape list.
    if (mAllStormItemsShapeList) {
        for (int i = 0; i < getAllStormItemsShapeCount(); i++) {
            xpm_destroy(mAllStormItemsShapeList[i]);
        }
        free(mAllStormItemsShapeList);
    }

    // Create new combined Shape list.
    const int RESOURCES_SHAPE_COUNT =
        getResourcesShapeCount();
    setAllStormItemsShapeCount(RESOURCES_SHAPE_COUNT +
        RANDOM_STORMITEM_COUNT);
    char*** newShapesList = (char***) malloc(
        getAllStormItemsShapeCount() * sizeof(char**));

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
        getRandomStormItemShape(WIDTH, HEIGHT,
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
void getRandomStormItemShape(int w, int h, char*** xpm) {
    const char c = '.';

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

    *xpm = (char **)malloc((nh + 3) * sizeof(char *));
    char **X = *xpm;

    X[0] = (char *)malloc(80 * sizeof(char));
    snprintf(X[0], 80, "%d %d 2 1", nw, nh);

    X[1] = strdup("  c None");
    X[2] = (char *)malloc(80 * sizeof(char));
    snprintf(X[2], 80, "%c c black", c);

    int offset = 3;
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
        getAllStormItemsShapeCount() * sizeof(StormItemSurface));

    for (int i = 0; i < getAllStormItemsShapeCount(); i++) {
        mAllStormItemSurfacesList[i].surface = NULL;
    }
}

int getStormItemSurfaceWidth(unsigned shapeType) {
    return mAllStormItemSurfacesList
        [shapeType].width;
}

int getStormItemSurfaceHeight(unsigned shapeType) {
    return mAllStormItemSurfacesList
        [shapeType].height;
}

cairo_surface_t* getStormItemSurface(
    unsigned shapeType) {
    return mAllStormItemSurfacesList
        [shapeType].surface;
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
        setStormItemsSpeedFactor();
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

    if (isColorPickerActive() &&
        isColorPickerConsumer(getStormItemColor1Tag()) &&
        isColorPickerResultAvailable()) {
        char sbuffer[16];
        snprintf(sbuffer, 16, "#%02x%02x%02x",
            getColorPickerResultRed(),
            getColorPickerResultGreen(),
            getColorPickerResultBlue());
        GdkRGBA color;
        gdk_rgba_parse(&color, sbuffer);
        free(Flags.StormItemColor1);
        rgba2color(&color, &Flags.StormItemColor1);
        clearColorPicker();
        clearAllFallenSnowItems();
        clearGlobalSnowWindow();
    }

    UIDOS(StormItemColor2,
        setAllStormItemsShapeSizeAndColor();
        clearGlobalSnowWindow();
    );

    if (isColorPickerActive() &&
        isColorPickerConsumer(getStormItemColor2Tag()) &&
        isColorPickerResultAvailable()) {
        char sbuffer[16];
        snprintf(sbuffer, 16, "#%02x%02x%02x",
            getColorPickerResultRed(),
            getColorPickerResultGreen(),
            getColorPickerResultBlue());
        GdkRGBA color;
        gdk_rgba_parse(&color, sbuffer);
        free(Flags.StormItemColor2);
        rgba2color(&color, &Flags.StormItemColor2);
        clearColorPicker();
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
float getStormItemsSpeedFactor() {
    return mStormItemsSpeedFactor;
}

/***********************************************************
 ** This method sets the desired speed of the StormItem.
 **/
void setStormItemsSpeedFactor() {
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
        STORMITEMS_PERSEC_PERPIXEL * getStormItemsSpeedFactor();
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
    for (int i = 0; i < getAllStormItemsShapeCount(); i++) {
        // Get item base w/h, and rando shrink w/h.
        // Items stuck on sceney removes itself this way.
        int itemWidth, itemHeight;
        sscanf(mAllStormItemsShapeList[i][0], "%d %d",
            &itemWidth, &itemHeight);

        // Guard stormItem w/h.
        const float SIZE_ADJUST = Flags.Scale *
            mGlobal.WindowScale *
            STORM_ITEM_SIZE_ADJUSTMENT *
            0.01;

        itemWidth = MAX(itemWidth * SIZE_ADJUST, 1);
        itemHeight = MAX(itemHeight * SIZE_ADJUST, 1);
        if (itemWidth == 1 && itemHeight == 1) {
            itemHeight = 2;
        }

        // Reset each Shape base to StormColor.
        char** shapeString;
        int lineCount;
        xpm_set_color(mAllStormItemsShapeList[i],
            &shapeString, &lineCount,
            getNextStormShapeColorAsString());

        // Get new item draw surface &
        // destroy existing surface.
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
        itemSurface->surface =
            gdk_cairo_surface_create_from_pixbuf(
                pixbufscaled, 0, NULL);

        g_clear_object(&pixbufscaled);
        g_clear_object(&newStormItemSurface);
    }
}

/***********************************************************
 ** This method adds a batch of items to the Storm window.
 **/
int updateStormOnThread() {
    mStormBackgroundThreadIsActive = true;

    if (Flags.shutdownRequested) {
        mStormBackgroundThreadIsActive = false;
        return false;
    }

    const double NOW = getWallClockMono();
    if (mStallingNewStormItems) {
        mUpdateStormThreadPrevTime = NOW;
        mStormBackgroundThreadIsActive = false;
        return true;
    }

    if (!isWorkspaceActive() || Flags.NoSnowFlakes) {
        mUpdateStormThreadPrevTime = NOW;
        mStormBackgroundThreadIsActive = false;
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
        mStormBackgroundThreadIsActive = false;
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
        mStormBackgroundThreadIsActive = false;
        return true;
    }

    // Start time when not generating flakes.
    mUpdateStormThreadPrevTime = NOW;
    mUpdateStormThreadStartTime += UPDATE_ELAPSED_TIME;

    mStormBackgroundThreadIsActive = false;
    return true;
}

/** ***********************************************************
 ** Getter for mStallingNewStormItems.
 **/
bool getStallingNewStormItems() {
    return mStallingNewStormItems;
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
 ** These are helper methods for StormShapeColor.
 **/
GdkRGBA getStormShapeColor() {
    return mStormItemColor;
}

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
GdkRGBA getRGBAFromString(char* colorString) {
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
