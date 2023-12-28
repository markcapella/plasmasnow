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

// calls macro's for elements of FLAGS
// DOIT_I is for x that should be output as 1234
// DOIT_S is for x that is a char*
// DOIT_L is for x that should be output as 0x123456
// parameters are: name, default value, vintage value

// these flags are not written to the config file and
// are no part of the ui (except BelowAll)
#define DOITALL                                                                \
    DOIT_I(BelowAll, 1, 1)                                                     \
    DOIT_I(BelowAllForce, 0, 0) /* to know if the -above flag was used */      \
    DOIT_I(BelowConfirm, 0, 0)  /*not a button or parameter */                 \
    DOIT_I(Changes, 0, 0)       /* not a parameter or button */                \
    DOIT_I(CheckGtk, 1, 1)                                                     \
    DOIT_I(Defaults, 0, 0)                                                     \
    DOIT_I(Desktop, 0, 0)                                                      \
    DOIT_I(Done, 0, 0)                                                         \
    DOIT_I(ForceRoot, 0, 0)                                                    \
    DOIT_I(FullScreen, 0, 0)                                                   \
    DOIT_I(HideMenu, 0, 0)                                                     \
    DOIT_I(NoConfig, 0, 0)                                                     \
    DOIT_I(NoMenu, 0, 0)                                                       \
    DOIT_I(Noisy, 0, 0)                                                        \
    DOIT_I(StopAfter, -1, -1)                                                  \
    DOIT_I(UseDouble, 1, 1)                                                    \
    DOIT_I(WindNow, 0, 0)                                                      \
    DOIT_I(XWinInfoHandling, 0, 0)                                             \
    DOIT_L(WindowId, 0, 0)                                                     \
    DOIT_S(DisplayName, "", "")                                                \
    DOIT

// following flags are written to the config file and
// are part of the ui

// Note: Screen should not be included in the command
// line parameters at refresh

#define DOIT                                                                   \
    DOIT_I(Aurora, 1, 0)                                                       \
    DOIT_I(AuroraSpeed, 50, 50)                                                \
    DOIT_I(AuroraBrightness, 15, 15)                                           \
    DOIT_I(AuroraWidth, 60, 60)                                                \
    DOIT_I(AuroraHeight, 30, 30)                                               \
    DOIT_I(AuroraBase, 50, 50)                                                 \
    DOIT_I(AuroraLeft, 0, 0)                                                   \
    DOIT_I(AuroraMiddle, 1, 1)                                                 \
    DOIT_I(AuroraRight, 0, 0)                                                  \
    DOIT_I(AllWorkspaces, 1, 1)                                                \
    DOIT_I(BlowOffFactor, 40, 40)                                              \
    DOIT_I(BlowSnow, 1, 0)                                                     \
    DOIT_I(CpuLoad, 100, 100)                                                  \
    DOIT_I(Transparency, 0, 0)                                                 \
    DOIT_I(Screen, -1, -1)                                                     \
    DOIT_I(Scale, 100, 100)                                                    \
    DOIT_I(DesiredNumberOfTrees, 10, 6)                                        \
    DOIT_I(FlakeCountMax, 300, 300)                                            \
    DOIT_I(Halo, 1, 1)                                                         \
    DOIT_I(HaloBright, 25, 25)                                                 \
    DOIT_I(MaxOnTrees, 200, 200)                                               \
    DOIT_I(MaxScrSnowDepth, 50, 50)                                            \
    DOIT_I(MaxWinSnowDepth, 30, 30)                                            \
    DOIT_I(MeteorFrequency, 40, 40)                                            \
    DOIT_I(Moon, 1, 0)                                                         \
    DOIT_I(MoonSpeed, 120, 120)                                                \
    DOIT_I(MoonSize, 100, 100)                                                 \
    DOIT_I(MoonColor, 0, 0)                                                    \
    DOIT_I(NoFluffy, 0, 0)                                                     \
    DOIT_I(NoKeepSBot, 0, 0)                                                   \
    DOIT_I(NoKeepSnow, 0, 0)                                                   \
    DOIT_I(NoKeepSnowOnTrees, 0, 1)                                            \
    DOIT_I(NoKeepSWin, 0, 0)                                                   \
    DOIT_I(NoMeteors, 0, 1)                                                    \
    DOIT_I(NoSanta, 0, 0)                                                      \
    DOIT_I(NoSnowFlakes, 0, 0)                                                 \
    DOIT_I(NoTrees, 0, 0)                                                      \
    DOIT_I(NoWind, 0, 0)                                                       \
    DOIT_I(NStars, 20, 0)                                                      \
    DOIT_I(OffsetS, 0, 0)                                                      \
    DOIT_I(OffsetW, -8, -8)                                                    \
    DOIT_I(OffsetX, 4, 4)                                                      \
    DOIT_I(OffsetY, 0, 0)                                                      \
    DOIT_I(Overlap, 1, 0)                                                      \
    DOIT_I(Rudolf, 1, 1)                                                       \
    DOIT_I(SantaSize, 3, 2)                                                    \
    DOIT_I(SantaSpeedFactor, 100, 100)                                         \
    DOIT_I(SantaScale, 100, 100)                                               \
    DOIT_I(SnowFlakesFactor, 100, 15)                                          \
    DOIT_I(SnowSize, 8, 8)                                                     \
    DOIT_I(SnowSpeedFactor, 100, 100)                                          \
    DOIT_I(Stars, 1, 0)                                                        \
    DOIT_I(ThemeXsnow, 1, 1)                                                   \
    DOIT_I(Outline, 0, 0)                                                      \
    DOIT_I(TreeFill, 30, 30)                                                   \
    DOIT_I(TreeScale, 100, 100)                                                \
    DOIT_I(VintageFlakes, 0, 1) /* internal flag */                            \
    DOIT_I(WhirlFactor, 100, 100)                                              \
    DOIT_I(WhirlTimer, 30, 30)                                                 \
    DOIT_I(IgnoreTop, 0, 0)                                                    \
    DOIT_I(IgnoreBottom, 0, 0)                                                 \
                                                                               \
    DOIT_S(SnowColor, "white", "white")                                        \
    DOIT_S(SnowColor2, "cyan", "cyan")                                         \
    DOIT_S(BirdsColor, "blue", "blue")                                         \
    DOIT_S(TreeColor, "red", "red")                                            \
                                                                               \
    DOIT_S(TreeType, "1,2,3,4,5,6,7,", "0,")                                   \
                                                                               \
    DOIT_I(Anarchy, 50, 50)                                                    \
    DOIT_I(AttrFactor, 100, 100)                                               \
    DOIT_I(BirdsScale, 100, 100)                                               \
    DOIT_I(AttrSpace, 40, 40)                                                  \
    DOIT_I(BirdsSpeed, 100, 100)                                               \
    DOIT_I(DisWeight, 20, 20)                                                  \
    DOIT_I(FollowWeight, 30, 30)                                               \
    DOIT_I(FollowSanta, 0, 0)                                                  \
    DOIT_I(Nbirds, 70, 70)                                                     \
    DOIT_I(Neighbours, 7, 7)                                                   \
    DOIT_I(PrefDistance, 40, 40)                                               \
    DOIT_I(ShowAttrPoint, 0, 0)                                                \
    DOIT_I(ShowBirds, 1, 0)                                                    \
    DOIT_I(ViewingDistance, 40, 40)                                            \
                                                                               \
    DOIT_S(BackgroundFile, " ", " ")                                           \
    DOIT_I(BlackBackground, 0, 0)                                              \
                                                                               \
    DOIT_S(Language, "sys", "sys")
