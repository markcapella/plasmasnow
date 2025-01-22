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
#include "MainWindow.h"
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

const int BRIGHT_COLOR = 252;
const int NORMAL_COLOR = 176;
const int DARK_COLOR = 132;

const int BRIGHT_GRAY = 0xa8;
const int NORMAL_GRAY = 0xdb;
const int DARK_GRAY = 0xb4;

const GdkRGBA BRIGHT_GRAYED_RGBA {
    .red = BRIGHT_GRAY, .green = BRIGHT_GRAY,
    .blue = BRIGHT_GRAY, .alpha = 0x00
};
const GdkRGBA NORMAL_GRAYED_RGBA {
    .red = NORMAL_GRAY, .green = NORMAL_GRAY,
    .blue = NORMAL_GRAY, .alpha = 0x00
};
const GdkRGBA DARK_GRAYED_RGBA {
    .red = DARK_GRAY, .green = DARK_GRAY,
    .blue = DARK_GRAY, .alpha = 0x00
};

// Light color types.
const int MAX_BULB_COLOR_TYPES = 8;

const BULB_COLOR_TYPE GRAYED = -1;
const BULB_COLOR_TYPE RED = 0;
const BULB_COLOR_TYPE LIME = 1;
const BULB_COLOR_TYPE PURPLE = 2;
const BULB_COLOR_TYPE CYAN = 3;
const BULB_COLOR_TYPE GREEN = 4;
const BULB_COLOR_TYPE ORANGE = 5;
const BULB_COLOR_TYPE BLUE = 6;
const BULB_COLOR_TYPE PINK = 7;

// Gray themed template XPM (mLightShape).
#include "Pixmaps/lightBulb.xpm"

// Ued to track screen scale changes.
int mCurrentAppScale = 100;

// Bulb position arrays.
std::vector<int> mLightXPos;
std::vector<int> mLightYPos;

// Bulb color arrays.
std::vector<GdkRGBA> mBulbColorBright;
std::vector<GdkRGBA> mBulbColorNormal;
std::vector<GdkRGBA> mBulbColorDark;


/** ***********************************************************
 ** This method initializes the Lights module.
 **/
void initLightsModule() {
    setAllBulbPositions();
    setAllBulbColors();

    addMethodToMainloop(PRIORITY_DEFAULT,
        TIME_BETWEEN_LIGHTS_FRAMES,
        twinkleLightsFrame);
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
    mBulbColorBright.clear();
    mBulbColorNormal.clear();
    mBulbColorDark.clear();

    int colorType = getFirstUserSelectedColor();

    for (int i = 0; i < getBulbCount(); i++) {
        mBulbColorBright.push_back(
            getTwinklingBright(colorType));
        mBulbColorNormal.push_back(
            getTwinklingNormal(colorType));
        mBulbColorDark.push_back(
            getTwinklingDark(colorType));

        if (colorType != GRAYED) {
            colorType = getNextUserSelectedColorAfter(colorType);
        }
    }
}

/** ***********************************************************
 ** This method checks for & performs user changes of
 ** Lights module settings.
 **/
void updateLightsUserSettings() {
    // Update pref.
    UIDO(ShowLights,);

    UIDO(ShowLightColorRed,
        eraseLightsFrame(); setAllBulbColors(););
    UIDO(ShowLightColorLime,
        eraseLightsFrame(); setAllBulbColors(););
    UIDO(ShowLightColorPurple,
        eraseLightsFrame(); setAllBulbColors(););
    UIDO(ShowLightColorCyan,
        eraseLightsFrame(); setAllBulbColors(););
    UIDO(ShowLightColorGreen,
        eraseLightsFrame(); setAllBulbColors(););
    UIDO(ShowLightColorOrange,
        eraseLightsFrame(); setAllBulbColors(););
    UIDO(ShowLightColorBlue,
        eraseLightsFrame(); setAllBulbColors(););
    UIDO(ShowLightColorPink,
        eraseLightsFrame(); setAllBulbColors(););

    // Detect app scale change.
    if (appScalesHaveChanged(&mCurrentAppScale)) {
        setAllBulbPositions(); setAllBulbColors();
    }
}

/** ***********************************************************
 ** This method draws the bulbs onto the display screen.
 **/
void drawLightsFrame(cairo_t* cc) {
    if (!Flags.ShowLights) {
        return;
    }

    cairo_save(cc);
    cairo_set_line_width(cc, 1);
    cairo_set_antialias(cc, CAIRO_ANTIALIAS_NONE);

    for (int i = 0; i < getBulbCount(); i++) {
        // Create XPM from 3-color themed bulb.
        char** tempBulbXPM;
        int unusedLineCount;

        createColoredBulb(mBulbColorBright[i],
            mBulbColorNormal[i], mBulbColorDark[i],
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
}

/** ***********************************************************
 ** This method changes the bulbs randomly to produce
 ** blinking effect.
 **/
gboolean twinkleLightsFrame(__attribute__
    ((unused)) void* data) {
    if (Flags.shutdownRequested) {
        return false;
    }

    // Change 1 out of 5 bulbs.
    int colorType = getFirstUserSelectedColor();
    if (colorType != GRAYED) {
        for (int i = 0; i < getBulbCount(); i++) {
            if (randint(5) == 0) {
                mBulbColorBright[i] = getTwinklingBright(colorType);
                mBulbColorNormal[i] = getTwinklingNormal(colorType);
                mBulbColorDark[i] = getTwinklingDark(colorType);
            }
            colorType = getNextUserSelectedColorAfter(colorType);
        }
    }

    return true;
}

/** ***********************************************************
 ** This method erases the bulbs from the display screen.
 **/
void eraseLightsFrame() {
    if (!Flags.ShowLights) {
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
 ** This method returns true if the user has selected
 ** at least one colortype button for display when bulbs
 ** are on. Helps determine if bulbs should be grayed.
 **/
bool hasTheUserSelectedAnyColors() {
    return shouldShowLightColorRed() || shouldShowLightColorLime() ||
        shouldShowLightColorPurple() || shouldShowLightColorCyan() ||
        shouldShowLightColorGreen() || shouldShowLightColorOrange() ||
        shouldShowLightColorBlue() || shouldShowLightColorPink();
}

/** ***********************************************************
 ** This method determines if a colortype is available
 ** at a start point as selected by the user list.
 **/
bool hasUserSelectedColor(BULB_COLOR_TYPE colorType) {

    switch (colorType) {
        case RED: return shouldShowLightColorRed();
        case LIME: return shouldShowLightColorLime();
        case PURPLE: return shouldShowLightColorPurple();
        case CYAN: return shouldShowLightColorCyan();
        case GREEN: return shouldShowLightColorGreen();
        case ORANGE: return shouldShowLightColorOrange();
        case BLUE: return shouldShowLightColorBlue();
        case PINK: return shouldShowLightColorPink();

        default:
            return false;
    }
}

/** ***********************************************************
 ** This method returns the first colortype available
 ** as selected by the user.
 **/
BULB_COLOR_TYPE getFirstUserSelectedColor() {
    // Find @ least one.
    return hasTheUserSelectedAnyColors() ?
        getNextUserSelectedColorAfter(GRAYED) : GRAYED;
}

/** ***********************************************************
 ** This method returns the next colortype available
 ** other than the one mentioned.
 **/
BULB_COLOR_TYPE getNextUserSelectedColorAfter(
    BULB_COLOR_TYPE thisColor) {

    // Find @ least one.
    if (!hasTheUserSelectedAnyColors()) {
        return GRAYED;
    }

    // Convert thisColor to an index, and search.
    int thisColorIndex = (int) thisColor;

    while (true) {
        thisColorIndex++;
        if (thisColorIndex == MAX_BULB_COLOR_TYPES) {
            thisColorIndex = getFirstUserSelectedColor();
        }

        if (hasUserSelectedColor((BULB_COLOR_TYPE)
            thisColorIndex)) {
            return thisColorIndex;
        }
    }
}

/** ***********************************************************
 ** This method creates a 3-colored XPM bulb
 ** from a gray template. All the magic. :-)
 **/
void createColoredBulb(GdkRGBA bright, GdkRGBA normal,
     GdkRGBA dark, char*** targetXPM,
    int* targetLineCount) {

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

/** ***********************************************************
 ** This method gets a "twinkling" version of a
 ** Bright bulb colorType.
 **/
GdkRGBA getTwinklingBright(BULB_COLOR_TYPE colorType) {

    switch (colorType) {
        case RED: return getTwinklingRedBright();
        case LIME: return getTwinklingLimeBright();
        case PURPLE: return getTwinklingPurpleBright();
        case CYAN: return getTwinklingCyanBright();
        case GREEN: return getTwinklingGreenBright();
        case ORANGE: return getTwinklingOrangeBright();
        case BLUE: return getTwinklingBlueBright();
        case PINK: return getTwinklingPinkBright();

        case GRAYED:
        default:
            return BRIGHT_GRAYED_RGBA;

    }
}

/** ***********************************************************
 ** This method gets a "twinkling" version of a
 ** Normal bulb colorType.
 **/
GdkRGBA getTwinklingNormal(BULB_COLOR_TYPE colorType) {

    switch (colorType) {
        case RED: return getTwinklingRedNormal();
        case LIME: return getTwinklingLimeNormal();
        case PURPLE: return getTwinklingPurpleNormal();
        case CYAN: return getTwinklingCyanNormal();
        case GREEN: return getTwinklingGreenNormal();
        case ORANGE: return getTwinklingOrangeNormal();
        case BLUE: return getTwinklingBlueNormal();
        case PINK: return getTwinklingPinkNormal();

        case GRAYED:
        default:
            return NORMAL_GRAYED_RGBA;
    }
}

/** ***********************************************************
 ** This method gets a "twinkling" version of a
 ** Dark bulb colorType.
 **/
GdkRGBA getTwinklingDark(
    BULB_COLOR_TYPE colorType) {

    switch (colorType) {
        case RED: return getTwinklingRedDark();
        case LIME: return getTwinklingLimeDark();
        case PURPLE: return getTwinklingPurpleDark();
        case CYAN: return getTwinklingCyanDark();
        case GREEN: return getTwinklingGreenDark();
        case ORANGE: return getTwinklingOrangeDark();
        case BLUE: return getTwinklingBlueDark();
        case PINK: return getTwinklingPinkDark();

        case GRAYED:
        default:
            return DARK_GRAYED_RGBA;
    }
}

/** ***********************************************************
 ** Helper methods for Red bulbs.
 **/
GdkRGBA getTwinklingRedBright() {
    const GdkRGBA seed = {
        .red = BRIGHT_COLOR, .green = 0x00,
        .blue = 0x00, .alpha = 0x00
    };
    return getTwinkledColorTypeFrom(seed);
}
GdkRGBA getTwinklingRedNormal() {
    const GdkRGBA seed = {
        .red = NORMAL_COLOR, .green = 0x00,
        .blue = 0x00, .alpha = 0x00
    };
    return getTwinkledColorTypeFrom(seed);
}
GdkRGBA getTwinklingRedDark() {
    const GdkRGBA seed = {
        .red = DARK_COLOR, .green = 0x00,
        .blue = 0x00, .alpha = 0x00
    };
    return getTwinkledColorTypeFrom(seed);
}

/** ***********************************************************
 ** Helper methods for Lime bulbs.
 **/
GdkRGBA getTwinklingLimeBright() {
    const GdkRGBA seed = {
        .red = BRIGHT_COLOR, .green = 0xff,
        .blue = 0x00, .alpha = 0x00
    };
    return getTwinkledColorTypeFrom(seed);
}
GdkRGBA getTwinklingLimeNormal() {
    const GdkRGBA seed = {
        .red = NORMAL_COLOR, .green = 0xff,
        .blue = 0x00, .alpha = 0x00
    };
    return getTwinkledColorTypeFrom(seed);
}
GdkRGBA getTwinklingLimeDark() {
    const GdkRGBA seed = {
        .red = DARK_COLOR, .green = 0xff,
        .blue = 0x00, .alpha = 0x00
    };
    return getTwinkledColorTypeFrom(seed);
}

/** ***********************************************************
 ** Helper methods for Purple bulbs.
 **/
GdkRGBA getTwinklingPurpleBright() {
    const GdkRGBA seed = {
        .red = BRIGHT_COLOR, .green = 0x00,
        .blue = 0xff, .alpha = 0x00
    };
    return getTwinkledColorTypeFrom(seed);
}
GdkRGBA getTwinklingPurpleNormal() {
    const GdkRGBA seed = {
        .red = NORMAL_COLOR, .green = 0x00,
        .blue = 0xff, .alpha = 0x00
    };
    return getTwinkledColorTypeFrom(seed);
}
GdkRGBA getTwinklingPurpleDark() {
    const GdkRGBA seed = {
        .red = DARK_COLOR, .green = 0x00,
        .blue = 0xff, .alpha = 0x00
    };
    return getTwinkledColorTypeFrom(seed);
}

/** ***********************************************************
 ** Helper methods for Cyan bulbs.
 **/
GdkRGBA getTwinklingCyanBright() {
    const GdkRGBA seed = {
        .red = BRIGHT_COLOR, .green = 0xff,
        .blue = 0xff, .alpha = 0x00
    };
    return getTwinkledColorTypeFrom(seed);
}
GdkRGBA getTwinklingCyanNormal() {
    const GdkRGBA seed = {
        .red = NORMAL_COLOR, .green = 0xff,
        .blue = 0xff, .alpha = 0x00
    };
    return getTwinkledColorTypeFrom(seed);
}
GdkRGBA getTwinklingCyanDark() {
    const GdkRGBA seed = {
        .red = DARK_COLOR, .green = 0xff,
        .blue = 0xff, .alpha = 0x00
    };
    return getTwinkledColorTypeFrom(seed);
}

/** ***********************************************************
 ** Helper methods for Green bulbs.
 **/
GdkRGBA getTwinklingGreenBright() {
    const GdkRGBA seed = {
        .red = 0x00, .green = BRIGHT_COLOR,
        .blue = 0x00, .alpha = 0x00
    };
    return getTwinkledColorTypeFrom(seed);
}
GdkRGBA getTwinklingGreenNormal() {
    const GdkRGBA seed = {
        .red = 0x00, .green = NORMAL_COLOR,
        .blue = 0x00, .alpha = 0x00
    };
    return getTwinkledColorTypeFrom(seed);
}
GdkRGBA getTwinklingGreenDark() {
    const GdkRGBA seed = {
        .red = 0x00, .green = DARK_COLOR,
        .blue = 0x00, .alpha = 0x00
    };
    return getTwinkledColorTypeFrom(seed);
}

/** ***********************************************************
 ** Helper methods for Orange bulbs.
 **/
GdkRGBA getTwinklingOrangeBright() {
    const GdkRGBA seed = {
        .red = 0xff, .green = BRIGHT_COLOR,
        .blue = 0x00, .alpha = 0x00
    };
    return getTwinkledColorTypeFrom(seed);
}
GdkRGBA getTwinklingOrangeNormal() {
    const GdkRGBA seed = {
        .red = 0xff, .green = NORMAL_COLOR,
        .blue = 0x00, .alpha = 0x00
    };
    return getTwinkledColorTypeFrom(seed);
}
GdkRGBA getTwinklingOrangeDark() {
    const GdkRGBA seed = {
        .red = 0xff, .green = DARK_COLOR,
        .blue = 0x00, .alpha = 0x00
    };
    return getTwinkledColorTypeFrom(seed);
}

/** ***********************************************************
 ** Helper methods for Blue bulbs.
 **/
GdkRGBA getTwinklingBlueBright() {
    const GdkRGBA seed = {
        .red = 0x00, .green = BRIGHT_COLOR,
        .blue = 0xff, .alpha = 0x00
    };
    return getTwinkledColorTypeFrom(seed);
}
GdkRGBA getTwinklingBlueNormal() {
    const GdkRGBA seed = {
        .red = 0x00, .green = NORMAL_COLOR,
        .blue = 0xff, .alpha = 0x00
    };
    return getTwinkledColorTypeFrom(seed);
}
GdkRGBA getTwinklingBlueDark() {
    const GdkRGBA seed = {
        .red = 0x00, .green = DARK_COLOR,
        .blue = 0xff, .alpha = 0x00
    };
    return getTwinkledColorTypeFrom(seed);
}

/** ***********************************************************
 ** Helper methods for Pink bulbs.
 **/
GdkRGBA getTwinklingPinkBright() {
    const GdkRGBA seed = {
        .red = 0xff, .green = BRIGHT_COLOR,
        .blue = 0xff, .alpha = 0x00
    };
    return getTwinkledColorTypeFrom(seed);
}
GdkRGBA getTwinklingPinkNormal() {
    const GdkRGBA seed = {
        .red = 0xff, .green = NORMAL_COLOR,
        .blue = 0xff, .alpha = 0x00
    };
    return getTwinkledColorTypeFrom(seed);
}
GdkRGBA getTwinklingPinkDark() {
    const GdkRGBA seed = {
        .red = 0xff, .green = DARK_COLOR,
        .blue = 0xff, .alpha = 0x00
    };
    return getTwinkledColorTypeFrom(seed);
}

/** ***********************************************************
 ** Main "Twinkle method". Slightly randomize each
 ** R / G / B int in the GdkRGBA.
 **/
GdkRGBA getTwinkledColorTypeFrom(GdkRGBA seed) {
    return (GdkRGBA) {
        .red =
            seed.red == 0x00 ? 0x00 :
            seed.red == 0xff ? 0xff :
            (gdouble) getFuzzyRGBInt(seed.red),
        .green =
            seed.green == 0x00 ? 0x00 :
            seed.green == 0xff ? 0xff :
            (gdouble) getFuzzyRGBInt(seed.green),
        .blue =
            seed.blue == 0x00 ? 0x00 :
            seed.blue == 0xff ? 0xff :
            (gdouble) getFuzzyRGBInt(seed.blue),
        .alpha = 0x0
    };
}

/** ***********************************************************
 ** Slightly randomize an R / G / B int from a GdkRGBA.
 **/
int getFuzzyRGBInt(int color) {
    const int RANGE_ABOVE_AND_BELOW = 45;

    const int newColor = randint(2) == 0 ?
        color + randint(RANGE_ABOVE_AND_BELOW) :
        color - randint(RANGE_ABOVE_AND_BELOW);

    return min(max(newColor, 0x00), 0xff);
}
