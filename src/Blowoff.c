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
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include <gtk/gtk.h>

#include "Blowoff.h"
#include "FallenSnow.h"
#include "Flags.h"
#include "PlasmaSnow.h"
#include "Prefs.h"
#include "Utils.h"
#include "Windows.h"


/** *********************************************************************
 ** Module globals and consts.
 **/
int mBlowOffLockCounter = 0;

// Prefs values.
const char* SHOW_DRIP_PREFNAME = "ShowDrip";
const bool SHOW_DRIP_DEFAULT = true;


/** *********************************************************************
 ** This method initializes the Blowoff module.
 **/
void initBlowoffModule() {
    addMethodToMainloop(PRIORITY_DEFAULT,
        TIME_BETWEEN_SCENERY_BLOWOFF_FRAME_UPDATES,
        updateBlowoffFrame);
}

/** *********************************************************************
 ** This method checks for & performs user changes of
 ** Blowoff module settings.
 **/
void respondToBlowoffSettingsChanges() {
    UIDO(BlowSnow, );
    UIDO(BlowOffFactor, );
}

/** *********************************************************************
 ** This method updates each fallensnow with wind.
 **/
int updateBlowoffFrame() {
    if (Flags.shutdownRequested) {
        return false;
    }

    if (Flags.NoSnowFlakes) {
        return true;
    }
    if (!isWorkspaceActive()) {
        return true;
    }

    if (softLockFallenSnowBaseSemaphore(3,
        &mBlowOffLockCounter)) {
        return true;
    }

    // Loop through all fallen.
    FallenSnow *fsnow = mGlobal.FsnowFirst;
    while (fsnow) {
        if (canSnowCollectOnFallen(fsnow) &&
            isFallenSnowVisible(fsnow)) {

            // Check for Santa Blowoff interaction.
            if (!Flags.NoSanta && mGlobal.SantaPlowRegion) {
                const bool IS_IT_IN = XRectInRegion(mGlobal.SantaPlowRegion,
                    fsnow->x, fsnow->y - fsnow->tallestColumnHeight,
                    fsnow->w, fsnow->tallestColumnHeight);
                if (IS_IT_IN == RectangleIn ||
                    IS_IT_IN == RectanglePart) {
                    blowoffPlowedSnowFromFallen(fsnow);
                    fsnow = fsnow->next;
                    continue;
                }
            }

            // Check for normal blowoff interaction.
            if (Flags.BlowSnow && randomIntegerUpTo(6) == 0) {
                blowoffSnowFromFallen(fsnow, fsnow->w / 4, fsnow->h / 4);
                fsnow = fsnow->next;
                continue;
            }

            // Check for normal blowoff interaction.
            if (getShowDrip() && fsnow->winInfo.window &&
                randomIntegerUpTo(40) == 0 &&
                canFallenSnowDripRain(fsnow)) {
                dripRainFromFallen(fsnow);
                fsnow = fsnow->next;
                continue;
            }
        }

        fsnow = fsnow->next;
    }

    unlockFallenSnowBaseSemaphore();
    return true;
}

/** *********************************************************************
 ** This method gets a random number up to blowoff factor
 ** for each Blowoff event.
 **/
int getNumberOfFlakesToBlowoff() {
    return Flags.BlowOffFactor * 0.04 * drand48();
}

/** *********************************************************************
 ** This method gets a random number up to blowoff factor
 ** for each Plowoff event.
 **/
int getNumberOfFlakesToPlowoff() {
    return 15 + randomIntegerUpTo(6);
}

/** *********************************************************************
 ** This method gets a random number up to blowoff factor
 ** for each Dropoff event.
 **/
int getNumberOfFlakesToDropoff() {
    return Flags.BlowOffFactor * 0.04 * drand48();
}

/** ***********************************************************
 ** Getters & Setters for all Prefs values.
 **/
bool getShowDrip() {
    return getBoolPref(SHOW_DRIP_PREFNAME,
        SHOW_DRIP_DEFAULT);
}
