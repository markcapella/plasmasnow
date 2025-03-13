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
     void setAllBulbPositions();
     void setAllBulbColors();
     void setAllBulbLayers();

     void respondToLightsSettingsChanges();

     void drawLowerLightsFrame(cairo_t* cc);
     void drawUpperLightsFrame(cairo_t* cc);
     void drawLightsFrame(cairo_t* cc, int forLayer);

     gboolean updateLightsFrame(void*);
     void eraseLightsFrame();

     int getBulbCount();
     int getFirstBulbXPos();
     int getYPosForBulbNumber(int bulbNumber);

     bool hasTheUserSelectedAnyColors();
     bool hasUserSelectedColor(LIGHT_COLOR_TYPE colorType);

     LIGHT_COLOR_TYPE getFirstUserSelectedColor();
     LIGHT_COLOR_TYPE getNextUserSelectedColorAfter(
          LIGHT_COLOR_TYPE thisColor);

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

#ifdef __cplusplus
}
#endif
