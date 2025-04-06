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
#include "StormItemShapeIncludes.h"
#include "Storm.h"
#include "StormItem.h"
#include "StormItemSurface.h"
#include "Utils.h"
#include "Windows.h"


/***********************************************************
 ** Module globals and consts.
 **/
// Number of unique random flake shapes to
// have available.
const int RANDOM_STORMITEM_COUNT = 50;

const float STORM_ITEM_SIZE_ADJUSTMENT = 0.8;
const float STORM_ITEM_SPEED_ADJUSTMENT = 0.7;
const int STORMITEMS_PERSEC_PERPIXEL = 30;

bool mUpdateStormThreadInitialized = false;
double mUpdateStormThreadPrevTime = 0;
double mUpdateStormThreadStartTime = 0;

bool mStallingNewStormItems = false;

float mStormItemsPerSecond = 0.0;
float mStormItemsSpeedFactor = 0.0;

int mAllStormItemsShapeCount = 0;
char*** mAllStormItemsShapeList = NULL;

StormItemSurface* mAllStormItemSurfacesList = NULL;

#define STORMITEM_SHAPEFILE_NAME(x) stormItem##x##Shape,
int mResourcesShapeCount = 0;
XPM_TYPE** mResourcesShapes[] = {
    ALL_STORMITEM_SHAPEFILE_NAMES
    NULL
};
#undef STORMITEM_SHAPEFILE_NAME

// StormItem color helper methods.
int mStormItemColorToggle = 0;
GdkRGBA mStormItemColor;

int mPreviousStormScale = 100;
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

/** *********************************************************************
 ** This method generated a random xpm pixmap
 ** with dimensions xpmWidth x xpmHeight.
 **
 ** The stormItem will be rotated, so the w and h of the resulting
 ** xpm will be different from the input w and h.
 **/
void getRandomStormItemShape(int xpmWidth, int xpmHeight,
    char*** xpm) {

    // Initialize with @ least one pixel.
    const int XPM_ITEM_COUNT = xpmWidth * xpmHeight;
    const int XPM_BYTE_COUNT = XPM_ITEM_COUNT * sizeof(float);

    float* itemXArray = (float*) malloc(XPM_BYTE_COUNT);
    float* itemYArray = (float*) malloc(XPM_BYTE_COUNT);

    int itemArrayLength = 1;
    itemXArray[0] = 0;
    itemYArray[0] = 0;

    // Pre-calc for faster loop.
    const float HALF_XPM_WIDTH = 0.5 * xpmWidth;
    const float HALF_XPM_HEIGHT = 0.5 * xpmHeight;

    // Loop.
    for (int h = 0; h < xpmHeight; h++) {
        const float ROTATE_HEIGHT = (h > HALF_XPM_HEIGHT) ?
            xpmHeight - h : h;
        const float PROBABILITY_Y = 2 * ROTATE_HEIGHT /
            xpmHeight;

        for (int w = 0; w < xpmWidth; w++) {
            const float ROTATE_WIDTH = (w > HALF_XPM_WIDTH) ?
                xpmWidth - w : w;
            const float PROBABILITY_X = 2 * ROTATE_WIDTH /
                xpmWidth;

            // Maybe push arrayItem.
            const float THIS_PROBABILITY = 1.1 -
                (PROBABILITY_X * PROBABILITY_Y);
            if (drand48() <= THIS_PROBABILITY) {
                continue;
            }

            if (itemArrayLength >= XPM_ITEM_COUNT) {
                continue;
            }
            itemYArray[itemArrayLength] = h - HALF_XPM_WIDTH;
            itemXArray[itemArrayLength] = w - HALF_XPM_HEIGHT;
            itemArrayLength++;
         }
    }

    // Rotate points with a random angle 0 .. pi. (Rick Magic?)
    const float randomAngle = drand48() * M_PI;
    const float cosRandomAngle = cosf(randomAngle);
    const float sinRandomAngle = sinf(randomAngle);

    // Init array & null terminate.
    const int ROTATED_BYTE_COUNT = itemArrayLength * sizeof(float);
    float* rotatedXArray = (float*) malloc(ROTATED_BYTE_COUNT);
    float* rotatedYArray = (float*) malloc(ROTATED_BYTE_COUNT);

    // Copy array and rotate.
    for (int i = 0; i < itemArrayLength; i++) {
        rotatedXArray[i] = itemXArray[i] * cosRandomAngle -
            itemYArray[i] * sinRandomAngle;
        rotatedYArray[i] = itemXArray[i] * sinRandomAngle +
            itemYArray[i] * cosRandomAngle;
    }

    // Find min height and width of rotated XPM Image.
    float xmin = rotatedXArray[0];
    float xmax = rotatedXArray[0];
    float ymin = rotatedYArray[0];
    float ymax = rotatedYArray[0];

    for (int i = 0; i < itemArrayLength; i++) {
        if (rotatedXArray[i] < xmin) {
            xmin = rotatedXArray[i];
        }
        if (rotatedXArray[i] > xmax) {
            xmax = rotatedXArray[i];
        }
        if (rotatedYArray[i] < ymin) {
            ymin = rotatedYArray[i];
        }
        if (rotatedYArray[i] > ymax) {
            ymax = rotatedYArray[i];
        }
    }

    // Expand 1x1 image to 1x2.
    int nw = ceilf(xmax - xmin + 1);
    int nh = ceilf(ymax - ymin + 1);
    if (nw == 1 && nh == 1) {
        nh = 2;
    }

    // Create XPM commmand strings. 1 Size header
    // and two header color codes.
    const int NUMBER_HEADER_STRINGS = 3;
    const int XPM_STRING_COUNT = (NUMBER_HEADER_STRINGS + nh) *
        sizeof(char*);

    *xpm = (char**) malloc(XPM_STRING_COUNT);

    char** xpmOutString = *xpm;
    xpmOutString[0] = (char*) malloc(80 * sizeof(char));
    snprintf(xpmOutString[0], 80, "%d %d 2 1", nw, nh);

    xpmOutString[1] = strdup("  c None");

    xpmOutString[2] = (char*) malloc(80 * sizeof(char));
    const char PERIOD_CHAR = '.';
    snprintf(xpmOutString[2], 80, "%c c black", PERIOD_CHAR);

    for (int i = 0; i < nh; i++) {
        xpmOutString[i + NUMBER_HEADER_STRINGS] =
            (char*) malloc((nw + 1) * sizeof(char));
    }

    for (int i = 0; i < nh; i++) {
        for (int j = 0; j < nw; j++) {
            xpmOutString[i + NUMBER_HEADER_STRINGS] [j] = ' ';
        }
        xpmOutString[i + NUMBER_HEADER_STRINGS] [nw] = 0;
    }

    for (int i = 0; i < itemArrayLength; i++) {
        const int FY = (int) rotatedYArray[i] - ymin +
            NUMBER_HEADER_STRINGS;
        const int FX = (int) rotatedXArray[i] - xmin;

        xpmOutString[FY][FX] = PERIOD_CHAR;
    }

    free(itemXArray);
    free(itemYArray);

    free(rotatedXArray);
    free(rotatedYArray);
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

    UIDO(ShapeSizeFactor,
        resetAllStormItemsShapeSizeAndColor();
        Flags.VintageFlakes = 0;
    );
    UIDO(mStormItemsSpeedFactor,
        setStormItemsSpeedFactor();
    );

    UIDO(FlakeCountMax, );
    UIDO(SnowFlakesFactor,
        setStormItemsPerSecond();
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
    const int DESIRED_STORMITEMS = lrint((UPDATE_ELAPSED_TIME +
        mUpdateStormThreadStartTime) * mStormItemsPerSecond);
    if (DESIRED_STORMITEMS) {
        for (int i = 0; i < DESIRED_STORMITEMS; i++) {
            StormItem* flake = createStormItem(-1);
            addStormItem(flake);
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
