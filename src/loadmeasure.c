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

#include "loadmeasure.h"
#include "clocks.h"
#include "debug.h"
#include "flags.h"
#include "ui.h"
#include "utils.h"
#include "plasmasnow.h"
#include <gtk/gtk.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

static int do_loadmeasure();

int loadmeasure_ui() {
    // nothing to do
    return 0;
}
void loadmeasure_draw() {
    // nothing to draw
    //
}

void loadmeasure_init() {
    add_to_mainloop(PRIORITY_DEFAULT, time_measure, do_loadmeasure);
}

// changes background color of ui if load to high
int do_loadmeasure() {
    double tnow = wallclock();
    static double tprev;
    static int count = 0;
    static int status = 0;
    static int warncount = 0;

    if (tnow - tprev > 1.2 * time_measure) {
        // if (tnow-tprev > 0.9*time_measure)   // for testing purposes
        count++;
    } else {
        count--;
    }
    P(" %d %d %f %f\n", count, status, time_measure, tnow - tprev);

    if (count > 10) {
        if (status == 0) {
            if (warncount < 5) {
                warncount++;
                P("pink %d %d %f %f\n", count, status, time_measure,
                    tnow - tprev);
                printf("system is too busy, suggest to lower 'cpu load' in "
                       "'settings'\n");
                printf(" or have a look at 'snow': 'Intensity', 'Max # of "
                       "flakes', ...\n");
                printf(" or specify a smaller number of birds in 'birds'\n");
            }
            if (!Flags.NoMenu) {
                ui_background(1);
            }
            status = 1;
        }
        count = 0;
    }
    if (count < -10) {
        if (status == 1) {
            P("white %d %d %f %f\n", count, status, time_measure, tnow - tprev);
            if (!Flags.NoMenu) {
                ui_background(0);
            }
            status = 0;
        }
        count = 0;
    }
    tprev = tnow;
    return TRUE;
    // (void) d;
}
