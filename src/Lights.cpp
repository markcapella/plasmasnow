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
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <sys/time.h>
#include <vector>

#include <X11/Intrinsic.h>
#include <gtk/gtk.h>

#include "Flags.h"
#include "Lights.h"
#include "Utils.h"
#include "windows.h"

using namespace std;


/** ***********************************************************
 ** Module globals and consts.
 **/

// Lightbulb defs.
const int LIGHTBULB_SIZE_WIDTH = 40;
const int LIGHTBULB_SIZE_HEIGHT = 57;
const int LIGHTBULB_SPACING_WIDTH = 15;

// Bulb position arrays.
std::vector<int> mLightXPos;
std::vector<int> mLightYPos;

// Bulb color arrays.
std::vector<GdkRGBA> mBrightBulbColor;
std::vector<GdkRGBA> mNormalBulbColor;
std::vector<GdkRGBA> mDarkBulbColor;

// Gray themed template XPM (mLightShape).
#include "Pixmaps/lightBulb.xpm"

// Ued to track screen scale changes.
int mCurrentAppScale = 100;

// Debug for speed timings.
int mDebugSetterCnt = 5;
int mDebugDrawCnt = 5;

struct timeval mDebugStartTime;
struct timeval mDebugEndTime;


/** ***********************************************************
 ** This method initializes the Lights module.
 **/
void initLightsModule() {

    setAllBulbPositions();
    setAllBulbColors();

    addMethodToMainloop(PRIORITY_DEFAULT,
        TIME_BETWEEN_LIGHTS_UPDATES,
        changeRandomBulbColors);
}

/** ***********************************************************
 ** This method sets each bulbs initial x/y position.
 **/
void setAllBulbPositions() {

    mLightXPos.clear();
    mLightYPos.clear();

    for (int i = 0; i < getBulbCount(); i++) {
        const int xPos = getFirstBulbXPos() +
            (i * (LIGHTBULB_SIZE_WIDTH +
            LIGHTBULB_SPACING_WIDTH));
        const int yPos = getYPosForBulbNumber(i);

        mLightXPos.push_back(xPos);
        mLightYPos.push_back(yPos);
    }
}

/** ***********************************************************
 ** This method sets each bulbs initial 3-color theme.
 **/
void setAllBulbColors() {

    gettimeofday(&mDebugStartTime, NULL);

    mBrightBulbColor.clear();
    mNormalBulbColor.clear();
    mDarkBulbColor.clear();

    for (int i = 0; i < getBulbCount(); i++) {
        mBrightBulbColor.push_back(
            getBrightGreenRandomColor());
        mNormalBulbColor.push_back(
            getNormalGreenRandomColor());
        mDarkBulbColor.push_back(
            getDarkGreenRandomColor());
    }

    if (mDebugSetterCnt-- > 0) {
        gettimeofday(&mDebugEndTime, NULL);
        const long seconds  = mDebugEndTime.tv_sec -
            mDebugStartTime.tv_sec;
        const long useconds = mDebugEndTime.tv_usec -
            mDebugStartTime.tv_usec;
        const double duration = seconds +
            useconds / 1000000.0;
        printf("setAllBulbColors() Elapsed time: %f  ms.\n",
            duration);
    }
}

/** ***********************************************************
 ** This method changes the bulbs randomly to produce
 ** blinking effect.
 **/
gboolean changeRandomBulbColors(__attribute__
    ((unused)) void* data) {

    if (Flags.shutdownRequested) {
        return false;
    }

    // Change 1 out of 5 bulbs.
    for (int i = 0; i < getBulbCount(); i++) {
        if (randint(5) == 0) {
            mBrightBulbColor[i] = getBrightGreenRandomColor();
            mNormalBulbColor[i] = getNormalGreenRandomColor();
            mDarkBulbColor[i] = getDarkGreenRandomColor();
        }
    }
    return true;
}

/** ***********************************************************
 ** This method draws the bulbs onto the display screen.
 **/
void drawLightsFrame(cairo_t* cc) {
    if (!Flags.showGreenLights) {
        return;
    }

    gettimeofday(&mDebugStartTime, NULL);

    cairo_save(cc);
    cairo_set_line_width(cc, 1);
    cairo_set_antialias(cc, CAIRO_ANTIALIAS_NONE);

    for (int i = 0; i < getBulbCount(); i++) {
        // Create XPM from 3-color themed bulb.
        char** tempBulbXPM;
        int unusedLineCount;
        createColoredBulb(mBrightBulbColor[i],
            mNormalBulbColor[i], mDarkBulbColor[i],
            &tempBulbXPM, &unusedLineCount);

        // Create colored surface from XPM.
        GdkPixbuf* tempBulbPixbuf =
            gdk_pixbuf_new_from_xpm_data(
                (const char**) tempBulbXPM);
        destroyColoredBulb(tempBulbXPM);

        cairo_surface_t* coloredLightSurface =
            gdk_cairo_surface_create_from_pixbuf(
                tempBulbPixbuf, 0, NULL);
        g_clear_object(&tempBulbPixbuf);

        // Draw colored surface.
        cairo_set_source_surface(cc, coloredLightSurface,
            mLightXPos[i], mLightYPos[i]);
        my_cairo_paint_with_alpha(cc, (0.01 *
            (100 - Flags.Transparency)));
        cairo_surface_destroy(coloredLightSurface);
    }

    cairo_restore(cc);

    if (mDebugDrawCnt-- > 0) {
        gettimeofday(&mDebugEndTime, NULL);
        const long seconds  = mDebugEndTime.tv_sec -
            mDebugStartTime.tv_sec;
        const long useconds = mDebugEndTime.tv_usec -
            mDebugStartTime.tv_usec;
        const double duration = seconds +
            useconds / 1000000.0;
        printf("drawLightsFrame() Elapsed time: %f  ms.\n",
            duration);
    }
}

/** ***********************************************************
 ** This method erases the bulbs from the display screen.
 **/
void eraseLightsFrame() {
    if (!Flags.showGreenLights) {
        return;
    }

    for (int i = 0; i < getBulbCount(); i++) {
        clearDisplayArea(mGlobal.display, mGlobal.SnowWin,
            mLightXPos[i], mLightYPos[i],
            LIGHTBULB_SIZE_WIDTH, LIGHTBULB_SIZE_HEIGHT,
            mGlobal.xxposures);
    }
}

/** ***********************************************************
 ** This method checks for performs user changes of
 ** Lights module settings.
 **/
void updateLightsUserSettings() {
    // Update pref.
    UIDO(showGreenLights,);

    // Detect app scale change.
    if (appScalesHaveChanged(&mCurrentAppScale)) {
        setAllBulbPositions();
        setAllBulbColors();
    }
}

/** ***********************************************************
 ** This method returns the number of bulbs
 ** to be displayed on screen.
 **/
int getBulbCount() {
    const int oneBulbAndColumnWidth =
        LIGHTBULB_SIZE_WIDTH + LIGHTBULB_SPACING_WIDTH;

    const int totalBulbAndColumns =
        mGlobal.SnowWinWidth / oneBulbAndColumnWidth;

    return totalBulbAndColumns;
}

/** ***********************************************************
 ** This method returns the X position of the first (left)
 ** bulb on screen.
 **/
int getFirstBulbXPos() {
    const int oneBulbAndColumnWidth =
        LIGHTBULB_SIZE_WIDTH + LIGHTBULB_SPACING_WIDTH;

    const int totalBulbAndColumns =
        mGlobal.SnowWinWidth / oneBulbAndColumnWidth;

    const int widthForMargins = mGlobal.SnowWinWidth -
        (totalBulbAndColumns * oneBulbAndColumnWidth) +
        LIGHTBULB_SPACING_WIDTH;

    return widthForMargins / 2;
}

/** ***********************************************************
 ** This method returns a bulbs Y position on screen,
 ** based on bulb number.
 **/
int getYPosForBulbNumber(int bulbNumber) {
    const bool isEven = 
        ((int) (bulbNumber / 2) * 2 == bulbNumber);

    return isEven ? 40 : 55;
}

/** ***********************************************************
 ** Helper methods for green color shading.
 **/
GdkRGBA getBrightGreenRandomColor() {
    const int BRIGHT_GREEN = 252;
    const int newColor = getRoughColor(BRIGHT_GREEN);

    return (GdkRGBA) { .red = 0x0,
        .green = (gdouble) newColor,
        .blue = 0x0, .alpha = 0x0 };
}

GdkRGBA getNormalGreenRandomColor() {
    const int NORMAL_GREEN = 176;
    const int newColor = getRoughColor(NORMAL_GREEN);

    return (GdkRGBA) { .red = 0x0,
        .green = (gdouble) newColor,
        .blue = 0x0, .alpha = 0x0 };
}

GdkRGBA getDarkGreenRandomColor() {
    const int DARK_GREEN = 132;
    const int newColor = getRoughColor(DARK_GREEN);

    return (GdkRGBA) { .red = 0x0,
        .green = (gdouble) newColor,
        .blue = 0x0, .alpha = 0x0 };
}

/** ***********************************************************
 ** This method sets a color randomly within a range
 ** of a start color to provide a twinkling effect.
 **/
int getRoughColor(int color) {
    const int RANGE_ABOVE_AND_BELOW = 45;

    const int newColor = randint(2) == 0 ?
        color + randint(RANGE_ABOVE_AND_BELOW) :
        color - randint(RANGE_ABOVE_AND_BELOW);

    return min(max(newColor, 0x00), 0xff);
}

/** ***********************************************************
 ** This method creates a 3-colored XPM bulb
 ** from a gray template.
 **/
void createColoredBulb(GdkRGBA bright, GdkRGBA normal,
     GdkRGBA dark, char*** targetXPM,
    int* targetLineCount) {

    // string brightColorValue = "#00ff00";
    char brightColorValue[16];
    char normalColorValue[16];
    char darkColorValue[16];

    snprintf(brightColorValue, 16, "#%02x%02x%02x",
        (int) bright.red, (int) bright.green,
        (int) bright.blue);
    snprintf(normalColorValue, 16, "#%02x%02x%02x",
        (int) normal.red, (int) normal.green,
        (int) normal.blue);
    snprintf(darkColorValue, 16, "#%02x%02x%02x",
        (int) dark.red, (int) dark.green,
        (int) dark.blue);

    // Get source image attributes.
    const int headerStrings = 1;
    const int colorIndex = 4;

    int colorStrings, dataStrings;
    sscanf(mLightShape[0], "%*d %d %d",
        &dataStrings, &colorStrings);

    const int totalStrings = headerStrings +
        colorStrings + dataStrings;

    // Allocate space for output target XPM.
    *targetXPM = (char**) malloc(sizeof(char*) * totalStrings);
    char** targetXPMArray = *targetXPM;

    // Copy all line data before the three color
    // strings to be created from mLightShape source
    // to target XPM return area.
    for (int i = 0; i < colorIndex; i++) {
        targetXPMArray[i] = strdup(mLightShape[i]);
    }

    // Create the three color strings to XPM return area.
    const char* brightColorKey = "- c ";
    const char* normalColorKey = "% c ";
    const char* darkColorKey = ", c ";

    targetXPMArray[colorIndex + 0] = (char*) malloc(
        strlen(brightColorKey) + strlen(brightColorValue) + 1);
    targetXPMArray[colorIndex + 0][0] = '\0';
    strcat(targetXPMArray[colorIndex + 0], brightColorKey);
    strcat(targetXPMArray[colorIndex + 0], brightColorValue);

    targetXPMArray[colorIndex + 1] = (char*) malloc(
        strlen(normalColorKey) + strlen(normalColorValue) + 1);
    targetXPMArray[colorIndex + 1][0] = '\0';
    strcat(targetXPMArray[colorIndex + 1], normalColorKey);
    strcat(targetXPMArray[colorIndex + 1], normalColorValue);

    targetXPMArray[colorIndex + 2] = (char*) malloc(
        strlen(darkColorKey) + strlen(darkColorValue) + 1);
    targetXPMArray[colorIndex + 2][0] = '\0';
    strcat(targetXPMArray[colorIndex + 2], darkColorKey);
    strcat(targetXPMArray[colorIndex + 2], darkColorValue);

    // Copy all line data after the three color
    // strings that were created, from mLightShape
    // source to XPM return area.
    const int CHANGED_COLORS = 3;
    const int NEXT_COLOR_INDEX =
        colorIndex + CHANGED_COLORS;

    for (int i = NEXT_COLOR_INDEX;
        i < totalStrings; i++) {
        targetXPMArray[i] = strdup(mLightShape[i]);
    }

    // Return resulting XPM Line count.
    *targetLineCount = totalStrings;
}

/** ***********************************************************
 ** This method destroys a previously created
 ** 3-colored XPM bulb.
 **/
void destroyColoredBulb(char** xpmStrings) {
    int dataStrings, colorStrings;
    sscanf(xpmStrings[0], "%*d %d %d",
        &dataStrings, &colorStrings);

    for (int i = 0; i < (dataStrings +
        colorStrings + 1); i++) {
        free(xpmStrings[i]);
    }

    free(xpmStrings);
}
