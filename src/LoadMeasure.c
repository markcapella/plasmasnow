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
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#include <gtk/gtk.h>

#include "ClockHelper.h"
#include "Flags.h"
#include "LoadMeasure.h"
#include "MainWindow.h"
#include "PlasmaSnow.h"
#include "Utils.h"


/***********************************************************
 * Module consts.
 */
const int LOAD_PRESSURE_LOW  = -10;
const int LOAD_PRESSURE_HIGH =  10;
const int WARNING_COUNT_MAX = 3;
const float EXCESSIVE_LOAD_MONITOR_TIME_PCT = 1.2;

bool mIsSystemBusy = false;

int mWarningCount = 0;
int mLoadPressure = 0;

double mLoadMeasurePrevThreadStart = 0;


/** *********************************************************************
 ** Add update method to mainloop.
 **/
void startLoadMeasureBackgroundThread() {
    addMethodToMainloop(PRIORITY_DEFAULT,
        TIME_BETWEEN_LOADMEASURE_UPDATES,
        updateLoadMeasureThread);
}

/** *********************************************************************
 ** Periodically check app performance.
 **
 ** Enable or disable CSS "Busy" Style.
 **/
int updateLoadMeasureThread() {
    double tnow = getWallClockMono();
    if ((tnow - mLoadMeasurePrevThreadStart) >
        (TIME_BETWEEN_LOADMEASURE_UPDATES * EXCESSIVE_LOAD_MONITOR_TIME_PCT)) {
        mLoadPressure++;
    } else {
        mLoadPressure--;
    }
    mLoadMeasurePrevThreadStart = tnow;

    if (mLoadPressure > LOAD_PRESSURE_HIGH) {
        if (!mIsSystemBusy) {
            mIsSystemBusy = true;
            if (mWarningCount < WARNING_COUNT_MAX) {
                mWarningCount++;
            }
 
           if (!Flags.NoMenu) {
                addBusyStyleClass();
            }
        }
        mLoadPressure = 0;
        return true;
    }

    if (mLoadPressure < LOAD_PRESSURE_LOW) {
        if (mIsSystemBusy) {
            mIsSystemBusy = false;
            if (!Flags.NoMenu) {
                removeBusyStyleClass();
            }
        }
        mLoadPressure = 0;
    }

    return true;
}
