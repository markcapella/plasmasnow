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
#define xsnow_snow 1
#define xsnow_santa 2
#define xsnow_scenery 3
#define xsnow_celestials 4
#define xsnow_birds 5
#define xsnow_settings 6

/*         code           type               name              modifier */
#define ALL_TOGGLES                                                            \
    BUTTON(togglecode, xsnow_celestials, Aurora, 1)                            \
    BUTTON(togglecode, xsnow_celestials, AuroraLeft, 0)                        \
    BUTTON(togglecode, xsnow_celestials, AuroraMiddle, 1)                      \
    BUTTON(togglecode, xsnow_celestials, AuroraRight, 0)                       \
    BUTTON(togglecode, xsnow_birds, FollowSanta, 1)                            \
    BUTTON(togglecode, xsnow_birds, ShowAttrPoint, 1)                          \
    BUTTON(togglecode, xsnow_birds, ShowBirds, 1)                              \
    BUTTON(togglecode, xsnow_settings, AllWorkspaces, 1)                       \
    BUTTON(togglecode, xsnow_settings, BelowAll, 1)                            \
    BUTTON(togglecode, xsnow_settings, BelowConfirm, 0)                        \
    BUTTON(togglecode, xsnow_settings, ThemeXsnow, 1)                          \
    BUTTON(togglecode, xsnow_celestials, NoMeteors, -1) /*i*/                  \
    BUTTON(togglecode, xsnow_celestials, Halo, 1)                              \
    BUTTON(togglecode, xsnow_celestials, Moon, 1)                              \
    BUTTON(togglecode, xsnow_santa, NoSanta, -1) /*i*/                         \
    BUTTON(togglecode, xsnow_snow, BlowSnow, 1)                                \
    BUTTON(togglecode, xsnow_snow, NoFluffy, -1)          /*i*/                \
    BUTTON(togglecode, xsnow_snow, NoKeepSBot, -1)        /*i*/                \
    BUTTON(togglecode, xsnow_snow, NoKeepSWin, -1)        /*i*/                \
    BUTTON(togglecode, xsnow_snow, NoKeepSnowOnTrees, -1) /*i*/                \
    BUTTON(togglecode, xsnow_snow, NoSnowFlakes, -1)      /*i*/                \
    BUTTON(togglecode, xsnow_celestials, Stars, 1)                             \
    BUTTON(togglecode, xsnow_scenery, NoTrees, -1) /*i*/                       \
    BUTTON(togglecode, xsnow_scenery, Overlap, 1)                              \
    BUTTON(togglecode, xsnow_celestials, NoWind, -1) /*i*/                     \
    BUTTON(togglecode, xsnow_settings, BlackBackground, 1)                     \
    BUTTON(togglecode, xsnow_settings, Outline, 1)                             \
    BUTTON(togglecode, xsnow_celestials, MoonColor, 1)

#define ALL_SCALES                                                             \
    BUTTON(scalecode, xsnow_birds, Anarchy, 1)                                 \
    BUTTON(scalecode, xsnow_birds, AttrFactor, 1)                              \
    BUTTON(scalecode, xsnow_birds, BirdsScale, 1)                              \
    BUTTON(scalecode, xsnow_birds, BirdsSpeed, 1)                              \
    BUTTON(scalecode, xsnow_birds, DisWeight, 1)                               \
    BUTTON(scalecode, xsnow_birds, FollowWeight, 1)                            \
    BUTTON(scalecode, xsnow_birds, Nbirds, 1)                                  \
    BUTTON(scalecode, xsnow_birds, Neighbours, 1)                              \
    BUTTON(scalecode, xsnow_birds, PrefDistance, 1)                            \
    BUTTON(scalecode, xsnow_birds, ViewingDistance, 1)                         \
    BUTTON(scalecode, xsnow_birds, AttrSpace, 1)                               \
    BUTTON(scalecode, xsnow_settings, CpuLoad, 1)                              \
    BUTTON(scalecode, xsnow_settings, OffsetS, -1) /*i*/                       \
    BUTTON(scalecode, xsnow_settings, OffsetY, -1) /*i*/                       \
    BUTTON(scalecode, xsnow_settings, Transparency, 1)                         \
    BUTTON(scalecode, xsnow_settings, Scale, 1)                                \
    BUTTON(scalecode, xsnow_settings, IgnoreTop, 1)                            \
    BUTTON(scalecode, xsnow_settings, IgnoreBottom, 1)                         \
    BUTTON(scalecode, xsnow_celestials, AuroraSpeed, 1)                        \
    BUTTON(scalecode, xsnow_celestials, AuroraBrightness, 1)                   \
    BUTTON(scalecode, xsnow_celestials, AuroraWidth, 1)                        \
    BUTTON(scalecode, xsnow_celestials, AuroraHeight, 1)                       \
    BUTTON(scalecode, xsnow_celestials, AuroraBase, 1)                         \
    BUTTON(scalecode, xsnow_celestials, HaloBright, 1)                         \
    BUTTON(scalecode, xsnow_celestials, MeteorFrequency, 1)                    \
    BUTTON(scalecode, xsnow_celestials, MoonSize, 1)                           \
    BUTTON(scalecode, xsnow_celestials, MoonSpeed, 1)                          \
    BUTTON(scalecode, xsnow_santa, SantaSpeedFactor, 1)                        \
    BUTTON(scalecode, xsnow_santa, SantaScale, 1)                              \
    BUTTON(scalecode, xsnow_snow, BlowOffFactor, 1)                            \
    BUTTON(scalecode, xsnow_snow, FlakeCountMax, 1)                            \
    BUTTON(scalecode, xsnow_snow, MaxOnTrees, 1)                               \
    BUTTON(scalecode, xsnow_snow, MaxScrSnowDepth, 1)                          \
    BUTTON(scalecode, xsnow_snow, MaxWinSnowDepth, 1)                          \
    BUTTON(scalecode, xsnow_snow, SnowFlakesFactor, 1)                         \
    BUTTON(scalecode, xsnow_snow, SnowSize, 1)                                 \
    BUTTON(scalecode, xsnow_snow, SnowSpeedFactor, 1)                          \
    BUTTON(scalecode, xsnow_snow, WhirlFactor, 1)                              \
    BUTTON(scalecode, xsnow_snow, WhirlTimer, 1)                               \
    BUTTON(scalecode, xsnow_celestials, NStars, 1)                             \
    BUTTON(scalecode, xsnow_scenery, DesiredNumberOfTrees, 1)                  \
    BUTTON(scalecode, xsnow_scenery, TreeFill, 1)                              \
    BUTTON(scalecode, xsnow_scenery, TreeScale, 1)

#define ALL_FILECHOOSERS BUTTON(filecode, xsnow_settings, BackgroundFile, 1)

#define BUTTON(code, type, name, m) code(type, name, m)

#define ALL_BUTTONS                                                            \
    ALL_TOGGLES                                                                \
    ALL_SCALES                                                                 \
    ALL_FILECHOOSERS
