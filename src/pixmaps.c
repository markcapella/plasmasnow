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

#include "pixmaps.h"
#include "snow.h"
#include "snow_includes.h" // a generated file, containing #includes for snow xbm's and xpm's
#include "plasmasnow.h"
#include <pthread.h>
//                            and the definition of SNOW_ALL

#include "Pixmaps/BigSanta1.xpm"
#include "Pixmaps/BigSanta2.xpm"
#include "Pixmaps/BigSanta3.xpm"
#include "Pixmaps/BigSanta4.xpm"

static XPM_TYPE **BigSanta[] = {BigSanta1, BigSanta2, BigSanta3, BigSanta4};

#include "Pixmaps/BigSantaRudolf1.xpm"
#include "Pixmaps/BigSantaRudolf2.xpm"
#include "Pixmaps/BigSantaRudolf3.xpm"
#include "Pixmaps/BigSantaRudolf4.xpm"

static XPM_TYPE **BigSantaRudolf[] = {
    BigSantaRudolf1, BigSantaRudolf2, BigSantaRudolf3, BigSantaRudolf4};

#include "Pixmaps/BigSanta81.xpm"
#include "Pixmaps/BigSanta82.xpm"
#include "Pixmaps/BigSanta83.xpm"
#include "Pixmaps/BigSanta84.xpm"

static XPM_TYPE **BigSanta8[] = {
    BigSanta81, BigSanta82, BigSanta83, BigSanta84};

#include "Pixmaps/BigSantaRudolf81.xpm"
#include "Pixmaps/BigSantaRudolf82.xpm"
#include "Pixmaps/BigSantaRudolf83.xpm"
#include "Pixmaps/BigSantaRudolf84.xpm"

static XPM_TYPE **BigSantaRudolf8[] = {
    BigSantaRudolf81, BigSantaRudolf82, BigSantaRudolf83, BigSantaRudolf84};

#include "Pixmaps/MediumSanta1.xpm"
#include "Pixmaps/MediumSanta2.xpm"
#include "Pixmaps/MediumSanta3.xpm"
#include "Pixmaps/MediumSanta4.xpm"

static XPM_TYPE **MediumSanta[] = {
    MediumSanta1, MediumSanta2, MediumSanta3, MediumSanta4};

#include "Pixmaps/MediumSantaRudolf1.xpm"
#include "Pixmaps/MediumSantaRudolf2.xpm"
#include "Pixmaps/MediumSantaRudolf3.xpm"
#include "Pixmaps/MediumSantaRudolf4.xpm"

static XPM_TYPE **MediumSantaRudolf[] = {MediumSantaRudolf1, MediumSantaRudolf2,
    MediumSantaRudolf3, MediumSantaRudolf4};

#include "Pixmaps/RegularSanta1.xpm"
#include "Pixmaps/RegularSanta2.xpm"
#include "Pixmaps/RegularSanta3.xpm"
#include "Pixmaps/RegularSanta4.xpm"

static XPM_TYPE **RegularSanta[] = {
    RegularSanta1, RegularSanta2, RegularSanta3, RegularSanta4};

#include "Pixmaps/RegularSantaRudolf1.xpm"
#include "Pixmaps/RegularSantaRudolf2.xpm"
#include "Pixmaps/RegularSantaRudolf3.xpm"
#include "Pixmaps/RegularSantaRudolf4.xpm"

static XPM_TYPE **RegularSantaRudolf[] = {RegularSantaRudolf1,
    RegularSantaRudolf2, RegularSantaRudolf3, RegularSantaRudolf4};

#include "Pixmaps/AltSanta1.xpm"
#include "Pixmaps/AltSanta2.xpm"
#include "Pixmaps/AltSanta3.xpm"
#include "Pixmaps/AltSanta4.xpm"

static XPM_TYPE **AltSanta[] = {AltSanta1, AltSanta2, AltSanta3, AltSanta4};

#include "Pixmaps/AltSantaRudolf1.xpm"
#include "Pixmaps/AltSantaRudolf2.xpm"
#include "Pixmaps/AltSantaRudolf3.xpm"
#include "Pixmaps/AltSantaRudolf4.xpm"

static XPM_TYPE **AltSantaRudolf[] = {
    AltSantaRudolf1, AltSantaRudolf2, AltSantaRudolf3, AltSantaRudolf4};

XPM_TYPE ***Santas[MAXSANTA + 1][2] = {{RegularSanta, RegularSantaRudolf},
    {MediumSanta, MediumSantaRudolf}, {BigSanta, BigSantaRudolf},
    {BigSanta8, BigSantaRudolf8}, {AltSanta, AltSantaRudolf}};

// so: Santas[type][rudolf][animation]

#include "Pixmaps/tannenbaum.xpm"
#include "Pixmaps/tree.xpm"
#include "Pixmaps/greenTree.xpm"
#include "Pixmaps/huis4.xpm"
#include "Pixmaps/rendier.xpm"
#include "Pixmaps/eland.xpm"
#include "Pixmaps/snowtree.xpm"
#include "Pixmaps/polarbear.xpm"
#include "Pixmaps/candycane.xpm"
#include "Pixmaps/garland.xpm"
//------
#include "Pixmaps/extratree.xpm"
//------


XPM_TYPE **xpmtrees[NUM_ALL_SCENE_TYPES] = {
    tannenbaum_xpm, tree_xpm, greenTree_xpm, huis4_xpm,
    reindeer_xpm, eland_xpm, snowtree_xpm, polarbear_xpm,
    candycane_xpm, garland_xpm, extratree_xpm };


#include "Pixmaps/star.xbm"
StarMap starPix = {star_bits, None, star_height, star_width};

#include "Pixmaps/plasmasnow.xpm"
XPM_TYPE **plasmasnow_logo = (XPM_TYPE **) plasmasnow_xpm;

/* front bird */
#include "Pixmaps/bird1.xpm"
#include "Pixmaps/bird2.xpm"
#include "Pixmaps/bird3.xpm"
#include "Pixmaps/bird4.xpm"
#include "Pixmaps/bird5.xpm"
#include "Pixmaps/bird6.xpm"
#include "Pixmaps/bird7.xpm"
#include "Pixmaps/bird8.xpm"

/* side bird */
#include "Pixmaps/birdl1.xpm"
#include "Pixmaps/birdl2.xpm"
#include "Pixmaps/birdl3.xpm"
#include "Pixmaps/birdl4.xpm"
#include "Pixmaps/birdl5.xpm"
#include "Pixmaps/birdl6.xpm"
#include "Pixmaps/birdl7.xpm"
#include "Pixmaps/birdl8.xpm"

/* oblique bird */
#include "Pixmaps/birdd1.xpm"
#include "Pixmaps/birdd2.xpm"
#include "Pixmaps/birdd3.xpm"
#include "Pixmaps/birdd4.xpm"
#include "Pixmaps/birdd5.xpm"
#include "Pixmaps/birdd6.xpm"
#include "Pixmaps/birdd7.xpm"
#include "Pixmaps/birdd8.xpm"

XPM_TYPE **birds_xpm[] = {bird1_xpm, bird2_xpm, bird3_xpm, bird4_xpm, bird5_xpm,
    bird6_xpm, bird7_xpm, bird8_xpm, birdl1_xpm, birdl2_xpm, birdl3_xpm,
    birdl4_xpm, birdl5_xpm, birdl6_xpm, birdl7_xpm, birdl8_xpm, birdd1_xpm,
    birdd2_xpm, birdd3_xpm, birdd4_xpm, birdd5_xpm, birdd6_xpm, birdd7_xpm,
    birdd8_xpm};

#include "Pixmaps/moon1.xpm"
#include "Pixmaps/moon2.xpm"
XPM_TYPE **moons_xpm[] = {moon1_xpm, moon2_xpm};

#define SNOW(x) snow##x##_xpm,
XPM_TYPE **snow_xpm[] = {SNOW_ALL NULL};
/* Like:
 * XPM_TYPE **snow_xpm[] =
 * {
 *    snow00_xpm,
 *    snow01_xpm,
 *    NULL
 * }
 * */
#include "undefall.inc"
