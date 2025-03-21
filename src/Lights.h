/* -copyright-
#-# 
#-# plasmasnow: Let it snow on your desktop
#-# Copyright (C) 2024 Mark Capella
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
#pragma once

#include <gtk/gtk.h>

#include "PlasmaSnow.h"


/***********************************************************
 * Module Method stubs.
 */
typedef int LIGHT_COLOR_TYPE;

#ifdef __cplusplus
extern "C" {
#endif
     void initLightsModule();
     void uninitLightsModule();

     void drawLowerLightsFrame(cairo_t* cc);
     void drawUpperLightsFrame(cairo_t* cc);
     void drawLightsFrame(cairo_t* cc, int forLayer);

     gboolean updateLightsFrame(void*);
     void eraseLightsFrame();

     void onClickedShowLights();
     void onChangedLightsShape(GtkComboBoxText *combo,
          __attribute__((unused)) gpointer data);
     void onClickedLightColorRed();
     void onClickedLightColorLime();
     void onClickedLightColorPurple();
     void onClickedLightColorCyan();
     void onClickedLightColorGreen();
     void onClickedLightColorOrange();
     void onClickedLightColorBlue();
     void onClickedLightColorPink();

     void onLightsScreenSizeChanged();

     void setAllBulbPositions();
     void setAllBulbLayers();
     void setAllBulbColors();

     int getBulbCount();
     int getFirstBulbXPos();
     int getYPosForBulbNumber(int bulbNumber);

     bool areAnyLightsColorTypesActive();
     bool isLightColorTypeActive(LIGHT_COLOR_TYPE colorType);

     LIGHT_COLOR_TYPE getFirstActiveLightsColor();
     LIGHT_COLOR_TYPE getNextActiveLightsColor
          (LIGHT_COLOR_TYPE thisColor);

     void createColoredBulb(GdkRGBA bright,
          GdkRGBA normal, GdkRGBA dark,
          char*** targetXPM, int* targetLineCount);
     void destroyColoredBulb(char** xpmStrings);

     // Routine helpers.
     GdkRGBA getTwinklingBright(LIGHT_COLOR_TYPE colorType);
     GdkRGBA getTwinklingNormal(LIGHT_COLOR_TYPE colorType);
     GdkRGBA getTwinklingDark(LIGHT_COLOR_TYPE colorType);

     GdkRGBA getTwinklingRedBright();
     GdkRGBA getTwinklingRedNormal();
     GdkRGBA getTwinklingRedDark();

     GdkRGBA getTwinklingLimeBright();
     GdkRGBA getTwinklingLimeNormal();
     GdkRGBA getTwinklingLimeDark();

     GdkRGBA getTwinklingPurpleBright();
     GdkRGBA getTwinklingPurpleNormal();
     GdkRGBA getTwinklingPurpleDark();

     GdkRGBA getTwinklingCyanBright();
     GdkRGBA getTwinklingCyanNormal();
     GdkRGBA getTwinklingCyanDark();

     GdkRGBA getTwinklingGreenBright();
     GdkRGBA getTwinklingGreenNormal();
     GdkRGBA getTwinklingGreenDark();

     GdkRGBA getTwinklingOrangeBright();
     GdkRGBA getTwinklingOrangeNormal();
     GdkRGBA getTwinklingOrangeDark();

     GdkRGBA getTwinklingBlueBright();
     GdkRGBA getTwinklingBlueNormal();
     GdkRGBA getTwinklingBlueDark();

     GdkRGBA getTwinklingPinkBright();
     GdkRGBA getTwinklingPinkNormal();
     GdkRGBA getTwinklingPinkDark();

     GdkRGBA getTwinkledColorTypeFrom(GdkRGBA seed);
     double getFuzzyRGBInt(double color);

     // Getters & Setters for all Prefs.
     void setAllLightsPrefsDefaults();

     bool getShowLights();
     void putShowLights(bool boolValue);
     int getLightsShape();
     void putLightsShape(int intValue);

     bool getShowLightColorRed();
     void putShowLightColorRed(bool boolValue);
     const char* getLightColorRed();
     bool getShowLightColorLime();
     void putShowLightColorLime(bool boolValue);
     const char* getLightColorLime();
     bool getShowLightColorPurple();
     void putShowLightColorPurple(bool boolValue);
     const char* getLightColorPurple();
     bool getShowLightColorCyan();
     void putShowLightColorCyan(bool boolValue);
     const char* getLightColorCyan();
     bool getShowLightColorGreen();
     void putShowLightColorGreen(bool boolValue);
     const char* getLightColorGreen();
     bool getShowLightColorOrange();
     void putShowLightColorOrange(bool boolValue);
     const char* getLightColorOrange();
     bool getShowLightColorBlue();
     void putShowLightColorBlue(bool boolValue);
     const char* getLightColorBlue();
     bool getShowLightColorPink();
     void putShowLightColorPink(bool boolValue);
     const char* getLightColorPink();

#ifdef __cplusplus
}
#endif
