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
#include <stdio.h>
#include <stdlib.h>

#include <gtk/gtk.h>

#include "PlasmaSnow.h"

#include "ClockHelper.h"
#include "Flags.h"
#include "Utils.h"
#include "wind.h"
#include "Windows.h"


static void SetWhirl();
static void SetWindTimer();
static int do_wind();
static int do_newwind();

void wind_init() {
    SetWhirl();
    SetWindTimer();
    addMethodToMainloop(PRIORITY_DEFAULT, time_newwind, do_newwind);
    addMethodToMainloop(PRIORITY_DEFAULT, time_wind, do_wind);
}

void wind_ui() {
    UIDO(NoWind, mGlobal.Wind = 0; mGlobal.NewWind = 0;);
    UIDO(WhirlFactor, SetWhirl(););
    UIDO(WhirlTimer, SetWindTimer(););
    if (Flags.WindNow) {
        Flags.WindNow = 0;
        mGlobal.Wind = 2;
    }
}

void draw_wind() {
    // Nothing to draw
}

int do_newwind() {
    if (Flags.shutdownRequested) {
        return FALSE;
    }
    if (!isWorkspaceActive()) {
        return TRUE;
    }
    //
    // the speed of newwind is pixels/second
    // at steady Wind, eventually all flakes get this speed.
    //
    if (Flags.NoWind) {
        return TRUE;
    }
    static double t0 = -1;
    if (t0 < 0) {
        t0 = getWallClockMono();
        return TRUE;
    }

    float r;
    switch (mGlobal.Wind) {
    case (0):
    default:
        r = drand48() * mGlobal.Whirl;
        mGlobal.NewWind += r - mGlobal.Whirl / 2;
        if (mGlobal.NewWind > mGlobal.WindMax) {
            mGlobal.NewWind = mGlobal.WindMax;
        }
        if (mGlobal.NewWind < -mGlobal.WindMax) {
            mGlobal.NewWind = -mGlobal.WindMax;
        }
        break;
    case (1):
        mGlobal.NewWind = mGlobal.Direction * 0.6 * mGlobal.Whirl;
        break;
    case (2):
        mGlobal.NewWind = mGlobal.Direction * 1.2 * mGlobal.Whirl;
        break;
    }
    return TRUE;
}

int do_wind() {
    if (Flags.shutdownRequested) {
        return FALSE;
    }
    if (!isWorkspaceActive()) {
        return TRUE;
    }
    if (Flags.NoWind) {
        return TRUE;
    }
    static int first = 1;
    static double prevtime;

    double NOW = getWallClockMono();
    if (first) {
        prevtime = NOW;
        ;
        first = 0;
    }

    // on the average, this function will do something
    // after WhirlTimer secs

    if ((NOW - prevtime) < 2 * mGlobal.WhirlTimer * drand48()) {
        return TRUE;
    }

    prevtime = NOW;

    if (drand48() > 0.65) // Now for some of Rick's magic:
    {
        if (drand48() > 0.4) {
            mGlobal.Direction = 1;
        } else {
            mGlobal.Direction = -1;
        }
        mGlobal.Wind = 2;
        mGlobal.WhirlTimer = 5;
        //               next time, this function will be active
        //               after on average 5 secs
    } else {
        if (mGlobal.Wind == 2) {
            mGlobal.Wind = 1;
            mGlobal.WhirlTimer = 3;
            //                   next time, this function will be active
            //                   after on average 3 secs
        } else {
            mGlobal.Wind = 0;
            mGlobal.WhirlTimer = mGlobal.WhirlTimerStart;
            //                   next time, this function will be active
            //                   after on average WhirlTimerStart secs
        }
    }
    return TRUE;
}

void SetWhirl() { mGlobal.Whirl = 0.01 * Flags.WhirlFactor * WHIRL; }

void SetWindTimer() {
    mGlobal.WhirlTimerStart = Flags.WhirlTimer;
    if (mGlobal.WhirlTimerStart < 3) {
        mGlobal.WhirlTimerStart = 3;
    }
    mGlobal.WhirlTimer = mGlobal.WhirlTimerStart;
}
