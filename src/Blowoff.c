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
#include <stdio.h>
#include <stdlib.h>

#include <gtk/gtk.h>

#include "Blowoff.h"
#include "FallenSnow.h"
#include "Flags.h"
#include "PlasmaSnow.h"
#include "Utils.h"
#include "Windows.h"


/** *********************************************************************
 ** Module globals and consts.
 **/
int mBlowOffLockCounter = 0;

/** *********************************************************************
 ** This method initializes the Blowoff module.
 **/
void initBlowoffModule() {
    addMethodToMainloop(PRIORITY_DEFAULT,
        TIME_BETWEEN_BLOWOFF_FRAME_UPDATES,
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
 ** This method gets a random number up to blowoff factor
 ** for each blowoff event.
 **/
int getNumberOfFlakesToBlowoff() {
    return Flags.BlowOffFactor * 0.04 * drand48();
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
    if (!WorkspaceActive()) {
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
            bool wasFallenBlownBySantaPlowing = false;
            if (!Flags.NoSanta && mGlobal.SantaPlowRegion) {
                const int ACTUAL_FALLEN_MAX_HEIGHT =
                    getMaximumFallenSnowColumnHeight(fsnow);
                const bool IS_IT_IN = XRectInRegion(mGlobal.SantaPlowRegion,
                    fsnow->x, fsnow->y - ACTUAL_FALLEN_MAX_HEIGHT,
                    fsnow->w, ACTUAL_FALLEN_MAX_HEIGHT);
                if (IS_IT_IN == RectangleIn || IS_IT_IN == RectanglePart) {
                    blowoffPlowedSnowFromFallen(fsnow);
                    wasFallenBlownBySantaPlowing = true;
                }
            }

            // Check for normal blowoff interaction.
            if (Flags.BlowSnow && !wasFallenBlownBySantaPlowing &&
                randint(5) == 0) {
                blowoffSnowFromFallen(fsnow, fsnow->w / 4, fsnow->h / 4);
            }
        }

        fsnow = fsnow->next;
    }

    unlockFallenSnowBaseSemaphore();
    return true;
}
