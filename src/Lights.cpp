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
#include "Windows.h"

using namespace std;


/** ***********************************************************
 ** Module globals and consts.
 **/
const int LIGHT_SPACING_WIDTH = 15;

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

// Light color types. 8 selectable from
// MainWindow buttons list.
const int MAX_LIGHT_COLOR_TYPES = 8;

const LIGHT_COLOR_TYPE GRAYED = -1;
const LIGHT_COLOR_TYPE RED = 0;
const LIGHT_COLOR_TYPE LIME = 1;
const LIGHT_COLOR_TYPE PURPLE = 2;
const LIGHT_COLOR_TYPE CYAN = 3;
const LIGHT_COLOR_TYPE GREEN = 4;
const LIGHT_COLOR_TYPE ORANGE = 5;
const LIGHT_COLOR_TYPE BLUE = 6;
const LIGHT_COLOR_TYPE PINK = 7;

// Gray themed LightShapes.
#include "Pixmaps/lightBulb.xpm"
#include "Pixmaps/easterEggPlain.xpm"
#include "Pixmaps/easterEgg.xpm"

// Shape array.
XPM_TYPE** mLightShapeList[] = {
    mLightShape,
    mEasterEggPlainShape,
    mEasterEggShape
};

// Lights thread handler.
bool mLightsThreadActive = false;
guint mLightsThreadId = 0;

// Bulb position arrays.
std::vector<int> mLightXPos;
std::vector<int> mLightYPos;
std::vector<int> mLightLayer;

// Bulb color arrays.
std::vector<GdkRGBA> mBulbColorBright;
std::vector<GdkRGBA> mBulbColorNormal;
std::vector<GdkRGBA> mBulbColorDark;


/** ***********************************************************
 ** Module global Public.
 **/

/** ***********************************************************
 ** This method responds to user changing lights module
 ** bulb shape.
 **/
void onLightsShapeChange(GtkComboBoxText *combo,
     __attribute__((unused)) gpointer data) {
    if (Flags.ShowLights) {
        uninitLightsModule();
    }

    Flags.LightsShape = gtk_combo_box_get_active(
        GTK_COMBO_BOX(combo));
    WriteFlags();

    if (Flags.ShowLights) {
        initLightsModule();
    }
}

/** ***********************************************************
 ** This method responds to the user changing all other
 ** lights module settings.
 **/
void respondToLightsSettingsChanges() {
    // Update pref.
    UIDO(ShowLights, setAllBulbLayers(););

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
}

/** ***********************************************************
 ** This method responds to OS screen size changes.
 **
 ** Lights module just "restrings" them left to right
 ** to fit the new screen width.
 **/
void respondToScreenSizeChanges() {
    setAllBulbPositions();
}

/** ***********************************************************
 ** This method draws the bulbs onto the display screen.
 **/
void drawLowerLightsFrame(cairo_t* cc) {
    drawLightsFrame(cc, 0);
}
void drawUpperLightsFrame(cairo_t* cc) {
    drawLightsFrame(cc, 1);
}

/** ***********************************************************
 ** This method updates the bulbs randomly to produce
 ** Twinkling effect.
 **/
gboolean updateLightsFrame(__attribute__ ((unused))
    void* data) {

    // Change 1 out of 5 bulbs.
    int colorType = getFirstUserSelectedColor();
    if (colorType != GRAYED) {
        for (int i = 0; i < getBulbCount(); i++) {
            if (randomIntegerUpTo(5) == 0) {
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

    // Get width & height of user selected shape.
    int lightWidth = 0, lightHeight = 0;
    sscanf(mLightShapeList[Flags.LightsShape][0], "%d %d",
        &lightWidth, &lightHeight);

    for (int i = 0; i < getBulbCount(); i++) {
        clearDisplayArea(mGlobal.display, mGlobal.SnowWin,
            mLightXPos[i], mLightYPos[i],
            lightWidth, lightHeight,
            mGlobal.xxposures);
    }
}

/** ***********************************************************
 ** Module global Private.
 **/

/** ***********************************************************
 ** This method initializes the lazy-loaded Lights module.
 **/
void initLightsModule() {
    // Set all structures.
    setAllBulbPositions();
    setAllBulbLayers();
    setAllBulbColors();

    // Start the update thread.
    const float LIGHTS_THREAD_DELAY_IN_MS = 0.5;
    mLightsThreadId = addMethodToMainloop(PRIORITY_DEFAULT,
        LIGHTS_THREAD_DELAY_IN_MS, updateLightsFrame);

    mLightsThreadActive = true;
}

/** ***********************************************************
 ** This method uninitializes the Lights module.
 **/
void uninitLightsModule() {
    // Clear the update thread.
    g_source_remove(mLightsThreadId);

    // Clear all structures.
    mLightXPos.clear();
    mLightYPos.clear();
    mLightLayer.clear();

    mBulbColorBright.clear();
    mBulbColorNormal.clear();
    mBulbColorDark.clear();

    mLightsThreadActive = false;
}

/** ***********************************************************
 ** This method sets each bulbs initial x/y position.
 **/
void setAllBulbPositions() {
    mLightXPos.clear();
    mLightYPos.clear();

    // Get width of user selected shape.
    int lightWidth = 0;
    sscanf(mLightShapeList[Flags.LightsShape][0], "%d",
        &lightWidth);

    for (int i = 0; i < getBulbCount(); i++) {
        const int XPOS = getFirstBulbXPos() + (i *
            (lightWidth + LIGHT_SPACING_WIDTH));
        const int YPOS = getYPosForBulbNumber(i);

        mLightXPos.push_back(XPOS);
        mLightYPos.push_back(YPOS);
    }
}

/** ***********************************************************
 ** This method sets each bulbs Upper/Lower layer flag.
 **/
void setAllBulbLayers() {
    mLightLayer.clear();

    for (int i = 0; i < getBulbCount(); i++) {
        mLightLayer.push_back(randomIntegerUpTo(2));
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
 ** This method draws the bulbs onto the display screen.
 **/
void drawLightsFrame(cairo_t* cc, int forLayer) {
    if (!Flags.ShowLights) {
        return;
    }

    // Lazy load Lights module on requested draw.
    if (!mLightsThreadActive) {
        initLightsModule();
    }

    cairo_save(cc);
    cairo_set_line_width(cc, 1);
    cairo_set_antialias(cc, CAIRO_ANTIALIAS_NONE);

    for (int i = 0; i < getBulbCount(); i++) {
        if (mLightLayer[i] != forLayer) {
            continue;
        }

        // Create XPM from 3-color themed light.
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
 ** This method returns the number of bulbs
 ** to be displayed on screen.
 **/
int getBulbCount() {
    // Get width of user selected shape.
    int lightWidth = 0;
    sscanf(mLightShapeList[Flags.LightsShape][0], "%d",
        &lightWidth);

    const int ONE_LIGHT_AND_COLUMN_WIDTH =
        lightWidth + LIGHT_SPACING_WIDTH;
    const int TOTAL_LIGHT_AND_COLUMNS =
        mGlobal.SnowWinWidth / ONE_LIGHT_AND_COLUMN_WIDTH;

    return TOTAL_LIGHT_AND_COLUMNS;
}

/** ***********************************************************
 ** This method returns the X position of the first (left)
 ** light on screen.
 **/
int getFirstBulbXPos() {
    // Get width of user selected shape.
    int lightWidth = 0;
    sscanf(mLightShapeList[Flags.LightsShape][0], "%d",
        &lightWidth);

    const int ONE_LIGHT_AND_COLUMN_WIDTH =
        lightWidth + LIGHT_SPACING_WIDTH;
    const int TOTAL_LIGHT_AND_COLUMNS =
        mGlobal.SnowWinWidth / ONE_LIGHT_AND_COLUMN_WIDTH;

    const int WIDTH_FOR_MARGINS = mGlobal.SnowWinWidth -
        (TOTAL_LIGHT_AND_COLUMNS * ONE_LIGHT_AND_COLUMN_WIDTH) +
        LIGHT_SPACING_WIDTH;
    return WIDTH_FOR_MARGINS / 2;
}

/** ***********************************************************
 ** This method returns a bulbs Y position on screen,
 ** based on light number.
 **/
int getYPosForBulbNumber(int lightNumber) {
    const bool IS_LIGHT_NUMBER_EVEN =
        ((int) (lightNumber / 2) * 2 == lightNumber);

    return IS_LIGHT_NUMBER_EVEN ? 40 : 55;
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
bool hasUserSelectedColor(LIGHT_COLOR_TYPE colorType) {

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
LIGHT_COLOR_TYPE getFirstUserSelectedColor() {
    // Find @ least one.
    return hasTheUserSelectedAnyColors() ?
        getNextUserSelectedColorAfter(GRAYED) : GRAYED;
}

/** ***********************************************************
 ** This method returns the next colortype available
 ** other than the one mentioned.
 **/
LIGHT_COLOR_TYPE getNextUserSelectedColorAfter(
    LIGHT_COLOR_TYPE thisColor) {
    // Find @ least one.
    if (!hasTheUserSelectedAnyColors()) {
        return GRAYED;
    }

    // Convert thisColor to an index, and search.
    int thisColorIndex = (int) thisColor;

    while (true) {
        thisColorIndex++;
        if (thisColorIndex == MAX_LIGHT_COLOR_TYPES) {
            thisColorIndex = getFirstUserSelectedColor();
        }

        if (hasUserSelectedColor((LIGHT_COLOR_TYPE)
            thisColorIndex)) {
            return thisColorIndex;
        }
    }
}

/** ***********************************************************
 ** This method creates a 3-colored XPM light
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
    const int HEADERSTRING_COUNT = 1;
    const int COLORSTRING_INDEX = 4;

    int dataStrings, colorStrings;
    sscanf(mLightShapeList[Flags.LightsShape][0], "%*d %d %d",
        &dataStrings, &colorStrings);
    const int ALL_STRINGS_COUNT = HEADERSTRING_COUNT +
        colorStrings + dataStrings;

    // Allocate space for output target XPM.
    *targetXPM = (char**) malloc(sizeof(char*) * ALL_STRINGS_COUNT);
    char** targetXPMArray = *targetXPM;

    // Copy all line data before the three color
    // strings to be created from shape source to
    // target the XPM return area.
    for (int i = 0; i < COLORSTRING_INDEX; i++) {
        targetXPMArray[i] = strdup(mLightShapeList[
            Flags.LightsShape][i]);
    }

    // Create the three color strings to XPM return area.
    const char* BRIGHT_COLOR_KEY = "- c ";
    const char* NORMAL_COLOR_KEY = "% c ";
    const char* DARK_COLOR_KEY = ", c ";

    targetXPMArray[COLORSTRING_INDEX + 0] = (char*) malloc(
        strlen(BRIGHT_COLOR_KEY) + strlen(brightColorValue) + 1);
    targetXPMArray[COLORSTRING_INDEX + 0][0] = '\0';
    strcat(targetXPMArray[COLORSTRING_INDEX + 0], BRIGHT_COLOR_KEY);
    strcat(targetXPMArray[COLORSTRING_INDEX + 0], brightColorValue);

    targetXPMArray[COLORSTRING_INDEX + 1] = (char*) malloc(
        strlen(NORMAL_COLOR_KEY) + strlen(normalColorValue) + 1);
    targetXPMArray[COLORSTRING_INDEX + 1][0] = '\0';
    strcat(targetXPMArray[COLORSTRING_INDEX + 1], NORMAL_COLOR_KEY);
    strcat(targetXPMArray[COLORSTRING_INDEX + 1], normalColorValue);

    targetXPMArray[COLORSTRING_INDEX + 2] = (char*) malloc(
        strlen(DARK_COLOR_KEY) + strlen(darkColorValue) + 1);
    targetXPMArray[COLORSTRING_INDEX + 2][0] = '\0';
    strcat(targetXPMArray[COLORSTRING_INDEX + 2], DARK_COLOR_KEY);
    strcat(targetXPMArray[COLORSTRING_INDEX + 2], darkColorValue);

    // Copy all line data after the three color
    // strings that were created, from shape source
    // to XPM return area.
    const int CHANGED_COLORS = 3;
    const int NEXT_COLOR_INDEX = COLORSTRING_INDEX +
        CHANGED_COLORS;

    for (int i = NEXT_COLOR_INDEX;
        i < ALL_STRINGS_COUNT; i++) {
        targetXPMArray[i] = strdup(mLightShapeList[
            Flags.LightsShape][i]);
    }

    // Return resulting XPM Line count.
    *targetLineCount = ALL_STRINGS_COUNT;
}

/** ***********************************************************
 ** This method destroys a previously created
 ** 3-colored XPM light.
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
 ** Bright light colorType.
 **/
GdkRGBA getTwinklingBright(LIGHT_COLOR_TYPE colorType) {
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
 ** Normal light colorType.
 **/
GdkRGBA getTwinklingNormal(LIGHT_COLOR_TYPE colorType) {
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
 ** Dark light colorType.
 **/
GdkRGBA getTwinklingDark(
    LIGHT_COLOR_TYPE colorType) {
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
            getFuzzyRGBInt(seed.red),
        .green =
            seed.green == 0x00 ? 0x00 :
            seed.green == 0xff ? 0xff :
            getFuzzyRGBInt(seed.green),
        .blue =
            seed.blue == 0x00 ? 0x00 :
            seed.blue == 0xff ? 0xff :
            getFuzzyRGBInt(seed.blue),
        .alpha = 0x0
    };
}

/** ***********************************************************
 ** Slightly randomize an R / G / B int from a GdkRGBA.
 **/
double getFuzzyRGBInt(double color) {
    const int RANGE_ABOVE_AND_BELOW = 45;

    const int newColor = randomIntegerUpTo(2) == 0 ?
        color + randomIntegerUpTo(RANGE_ABOVE_AND_BELOW) :
        color - randomIntegerUpTo(RANGE_ABOVE_AND_BELOW);

    return min(max(newColor, 0x00), 0xff);
}
