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

/* name of button will be    Button.NStars
 * glade:
 * id will be                stars-NStars
 * name of call back will be button_stars_NStars
 */
#define plasmasnow_snow 1
#define plasmasnow_santa 2
#define plasmasnow_scenery 3
#define plasmasnow_celestials 4
#define plasmasnow_birds 5
#define plasmasnow_settings 6

/*         code           type               name              modifier */
#define ALL_TOGGLES                                                                 \
    BUTTON(togglecode, plasmasnow_celestials, ShowAurora, 1) \
    BUTTON(togglecode, plasmasnow_celestials, AuroraLeft, 0)                        \
    BUTTON(togglecode, plasmasnow_celestials, AuroraMiddle, 1)                      \
    BUTTON(togglecode, plasmasnow_celestials, AuroraRight, 0)                       \
    BUTTON(togglecode, plasmasnow_birds, FollowSanta, 1)                            \
    BUTTON(togglecode, plasmasnow_birds, ShowAttrPoint, 1)                          \
    BUTTON(togglecode, plasmasnow_birds, ShowBirds, 1)                              \
    BUTTON(togglecode, plasmasnow_celestials, NoMeteors, -1) /*i*/                  \
    BUTTON(togglecode, plasmasnow_celestials, Halo, 1)                              \
    BUTTON(togglecode, plasmasnow_celestials, Moon, 1)                              \
    BUTTON(togglecode, plasmasnow_santa, NoSanta, -1) /*i*/                         \
    BUTTON(togglecode, plasmasnow_santa, ShowLights, 1) \
    BUTTON(togglecode, plasmasnow_snow, BlowSnow, 1)                                \
    BUTTON(togglecode, plasmasnow_snow, NoKeepSnowOnBottom, -1)        /*i*/        \
    BUTTON(togglecode, plasmasnow_snow, NoKeepSnowOnWindows, -1)        /*i*/       \
    BUTTON(togglecode, plasmasnow_snow, NoKeepSnowOnTrees, -1) /*i*/                \
    BUTTON(togglecode, plasmasnow_snow, NoSnowFlakes, -1)      /*i*/                \
    BUTTON(togglecode, plasmasnow_celestials, Stars, 1) \
    BUTTON(togglecode, plasmasnow_scenery, NoTrees, -1) /*i*/                       \
    BUTTON(togglecode, plasmasnow_scenery, Overlap, 1)                              \
    BUTTON(togglecode, plasmasnow_celestials, NoWind, -1) /*i*/                     \
    \
    BUTTON(togglecode, plasmasnow_settings, AllWorkspaces, 1) \
    BUTTON(togglecode, plasmasnow_settings, mAppTheme, 1) \
    BUTTON(togglecode, plasmasnow_settings, Outline, 1) \
    BUTTON(togglecode, plasmasnow_settings, ShowSplashScreen, 1) \
    BUTTON(togglecode, plasmasnow_settings, BlackBackground, 1) \
    \
    BUTTON(togglecode, plasmasnow_celestials, MoonColor, 1)

#define ALL_SCALES                                                                  \
    BUTTON(scalecode, plasmasnow_birds, Anarchy, 1)                                 \
    BUTTON(scalecode, plasmasnow_birds, AttrFactor, 1)                              \
    BUTTON(scalecode, plasmasnow_birds, BirdsScale, 1)                              \
    BUTTON(scalecode, plasmasnow_birds, BirdsSpeed, 1)                              \
    BUTTON(scalecode, plasmasnow_birds, DisWeight, 1)                               \
    BUTTON(scalecode, plasmasnow_birds, FollowWeight, 1)                            \
    BUTTON(scalecode, plasmasnow_birds, Nbirds, 1)                                  \
    BUTTON(scalecode, plasmasnow_birds, Neighbours, 1)                              \
    BUTTON(scalecode, plasmasnow_birds, PrefDistance, 1)                            \
    BUTTON(scalecode, plasmasnow_birds, ViewingDistance, 1)                         \
    BUTTON(scalecode, plasmasnow_birds, AttrSpace, 1)                               \
    \
    BUTTON(scalecode, plasmasnow_settings, CpuLoad, 1)                              \
    BUTTON(scalecode, plasmasnow_settings, Transparency, 1)                         \
    BUTTON(scalecode, plasmasnow_settings, Scale, 1)                                \
    BUTTON(scalecode, plasmasnow_settings, OffsetS, -1) /*i*/                       \
    BUTTON(scalecode, plasmasnow_settings, OffsetY, -1) /*i*/                       \
    BUTTON(scalecode, plasmasnow_settings, IgnoreTop, 1)                            \
    BUTTON(scalecode, plasmasnow_settings, IgnoreBottom, 1)                         \
    \
    BUTTON(scalecode, plasmasnow_celestials, AuroraSpeed, 1)                        \
    BUTTON(scalecode, plasmasnow_celestials, AuroraBrightness, 1)                   \
    BUTTON(scalecode, plasmasnow_celestials, AuroraWidth, 1)                        \
    BUTTON(scalecode, plasmasnow_celestials, AuroraHeight, 1)                       \
    BUTTON(scalecode, plasmasnow_celestials, AuroraBase, 1)                         \
    BUTTON(scalecode, plasmasnow_celestials, HaloBright, 1)                         \
    BUTTON(scalecode, plasmasnow_celestials, MeteorFrequency, 1)                    \
    BUTTON(scalecode, plasmasnow_celestials, MoonSize, 1)                           \
    BUTTON(scalecode, plasmasnow_celestials, MoonSpeed, 1)                          \
    BUTTON(scalecode, plasmasnow_santa, SantaSpeedFactor, 1)                        \
    BUTTON(scalecode, plasmasnow_santa, SantaScale, 1)                              \
    BUTTON(scalecode, plasmasnow_snow, BlowOffFactor, 1)                            \
    BUTTON(scalecode, plasmasnow_snow, FlakeCountMax, 1)                            \
    BUTTON(scalecode, plasmasnow_snow, MaxOnTrees, 1)                               \
    BUTTON(scalecode, plasmasnow_snow, MaxScrSnowDepth, 1)                          \
    BUTTON(scalecode, plasmasnow_snow, MaxWinSnowDepth, 1)                          \
    BUTTON(scalecode, plasmasnow_snow, SnowFlakesFactor, 1)                         \
    BUTTON(scalecode, plasmasnow_snow, ShapeSizeFactor, 1)                                 \
    BUTTON(scalecode, plasmasnow_snow, mStormItemsSpeedFactor, 1)                          \
    BUTTON(scalecode, plasmasnow_snow, WhirlFactor, 1)                              \
    BUTTON(scalecode, plasmasnow_snow, WhirlTimer, 1)                               \
    BUTTON(scalecode, plasmasnow_celestials, NStars, 1)                             \
    BUTTON(scalecode, plasmasnow_scenery, DesiredNumberOfTrees, 1)                  \
    BUTTON(scalecode, plasmasnow_scenery, TreeFill, 1)                              \
    BUTTON(scalecode, plasmasnow_scenery, TreeScale, 1)

#define ALL_FILECHOOSERS BUTTON(filecode, plasmasnow_settings, BackgroundFile, 1)

#define BUTTON(code, type, name, m) code(type, name, m)

#define ALL_BUTTONS                                                            \
    ALL_TOGGLES                                                                \
    ALL_SCALES                                                                 \
    ALL_FILECHOOSERS
