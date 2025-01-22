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
#include "plasmasnow.h"
#include "Flags.h"
#include "Utils.h"
#include "windows.h"


/** *********************************************************************
 ** Module globals and consts.
 **/
int mBlowOffLockCounter = 0;


/** *********************************************************************
 ** This method initializes the Blowoff module.
 **/
void initBlowoffModule() {
    addMethodToMainloop(PRIORITY_DEFAULT,
        TIME_BETWEEN_BLOWOFF_FRAMES,
        updateBlowoffFrame);
}

/** *********************************************************************
 ** This method checks for & performs user changes of
 ** Blowoff module settings.
 **/
void updateBlowoffUserSettings() {
    UIDO(BlowSnow, );
    UIDO(BlowOffFactor, );
}

/** *********************************************************************
 ** This method gets a random number up to blowOff factor
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
    if (!Flags.BlowSnow) {
        return true;
    }

    if (softLockFallenSnowBaseSemaphore(3, &mBlowOffLockCounter)) {
        return true;
    }

    // Loop through all fallen.
    FallenSnow *fsnow = mGlobal.FsnowFirst;
    while (fsnow) {
        if (canSnowCollectOnFallen(fsnow)) {
            if (fsnow->winInfo.window == 0 ||
                (!fsnow->winInfo.hidden &&
                (isFallenSnowVisibleOnWorkspace(fsnow) || fsnow->winInfo.sticky))) {
                updateFallenSnowWithWind(fsnow, fsnow->w / 4, fsnow->h / 4);
            }
        }
        fsnow = fsnow->next;
    }

    unlockFallenSnowSemaphore();
    return true;
}
