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

#include "blowoff.h"
#include "debug.h"
#include "fallensnow.h"
#include "Flags.h"
#include "Utils.h"
#include "windows.h"
#include "plasmasnow.h"
#include <gtk/gtk.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>


static int do_blowoff();

void blowoff_init() {
    addMethodToMainloop(PRIORITY_DEFAULT, time_blowoff, do_blowoff);
}

void blowoff_ui() {
    UIDO(BlowOffFactor, );
    UIDO(BlowSnow, );
}

void blowoff_draw() {
    // nothing to draw here
}

// Get a random number up to blowOff factor
// for each blowoff event.
int getNumberOfFlakesToBlowoff() {
    return 0.04 * Flags.BlowOffFactor * drand48();
}

// determine if fallensnow should be handled for fsnow
int do_blowoff() {
    if (Flags.shutdownRequested) {
        return FALSE;
    }
    if (!WorkspaceActive() || !Flags.BlowSnow) {
        return TRUE;
    }
    static int lockcounter = 0;
    if (softLockFallenSnowBaseSemaphore(3, &lockcounter)) {
        return TRUE;
    }
    FallenSnow *fsnow = mGlobal.FsnowFirst;
    while (fsnow) {
        P("blowoff ...\n");
        if (canSnowCollectOnWindowOrScreenBottom(fsnow) && !Flags.NoSnowFlakes) {
            if (fsnow->winInfo.window == 0 ||
                (!fsnow->winInfo.hidden &&
                    //(fsnow->winInfo.ws == mGlobal.CWorkSpace ||
                    //fsnow->winInfo.sticky)))
                    (isFallenSnowOnVisibleWorkspace(fsnow) || fsnow->winInfo.sticky))) {
                updateFallenSnowWithWind(fsnow, fsnow->w / 4, fsnow->h / 4);
            }
        }
        fsnow = fsnow->next;
    }
    unlockFallenSnowSemaphore();
    return TRUE;
    // (void) d;
}
