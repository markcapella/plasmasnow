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

#include "meteor.h"
#include "clocks.h"
#include "debug.h"
#include "flags.h"
#include "snow.h"
#include "ui.h"
#include "utils.h"
#include "windows.h"
#include "plasmasnow.h"
#include <X11/Intrinsic.h>
#include <gtk/gtk.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#define NOTACTIVE (!WorkspaceActive())

static int do_emeteor(gpointer data);
static int do_meteor();

#define NUMCOLORS 5
static GdkRGBA colors[NUMCOLORS];
static MeteorMap meteor;

void meteor_init() {
    meteor.x1 = 0;
    meteor.x2 = 0;
    meteor.y1 = 0;
    meteor.y2 = 0;
    meteor.active = 0;
    meteor.colornum = 0;

    gdk_rgba_parse(&colors[0], "#f0e0e0");
    gdk_rgba_parse(&colors[1], "#e02020");
    gdk_rgba_parse(&colors[2], "#f0a020");
    gdk_rgba_parse(&colors[3], "#f0d0a0");
    gdk_rgba_parse(&colors[4], "#f0d040");

    addMethodWithArgToMainloop(PRIORITY_DEFAULT, time_emeteor, do_emeteor, NULL);
    addMethodToMainloop(PRIORITY_DEFAULT, 0.1, do_meteor);
}

void meteor_ui() {
    UIDO(NoMeteors, );
    UIDO(MeteorFrequency, );
}

void meteor_draw(cairo_t *cr) {
    P("meteor_draw %d %d\n", counter++, meteor.active);
    if (!meteor.active) {
        return;
    }
    cairo_save(cr);

    int c = meteor.colornum;
    cairo_set_source_rgba(
        cr, colors[c].red, colors[c].green, colors[c].blue, ALPHA);
    cairo_set_line_width(cr, 2);
    cairo_set_antialias(cr, CAIRO_ANTIALIAS_DEFAULT);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_move_to(cr, meteor.x1, meteor.y1);
    cairo_line_to(cr, meteor.x2, meteor.y2);
    cairo_stroke(cr);

    cairo_restore(cr);
}

void meteor_erase() { do_emeteor(NULL); }

int do_emeteor(gpointer data) {
    (void)data;
    P("do_emeteor %d\n", mGlobal.counter++);
    if (Flags.Done) {
        return FALSE;
    }
    if (!meteor.active || NOTACTIVE) {
        return TRUE;
    }
    if (!mGlobal.isDoubleBuffered) {
        int x = meteor.x1;
        int y = meteor.y1;
        int w = meteor.x2 - x;
        int h = meteor.y2 - y;
        if (w < 0) {
            x += w;
            w = -w;
        }
        if (h < 0) {
            y += h;
            h = -h;
        }
        x -= 1;
        y -= 1;
        w += 2;
        h += 2;
        sanelyCheckAndClearDisplayArea(
            mGlobal.display, mGlobal.SnowWin, x, y, w, h, mGlobal.xxposures);
    }
    meteor.active = 0;
    return TRUE;
}

int do_meteor() {
    // (void) d;
    P("do_meteor %d\n", mGlobal.counter++);
    if (Flags.Done) {
        return FALSE;
    }

    if (!(NOTACTIVE || meteor.active || Flags.NoMeteors)) {
        meteor.x1 = randint(mGlobal.SnowWinWidth);
        meteor.y1 = randint(mGlobal.SnowWinHeight / 4);
        meteor.x2 = meteor.x1 + mGlobal.SnowWinWidth / 10 -
                    randint(mGlobal.SnowWinWidth / 5);
        if (meteor.x2 == meteor.x1) {
            meteor.x2 += 5;
        }
        meteor.y2 = meteor.y1 + mGlobal.SnowWinHeight / 5 -
                    randint(mGlobal.SnowWinHeight / 5);
        if (meteor.y2 == meteor.y1) {
            meteor.y2 += 5;
        }
        meteor.active = 1;
        meteor.colornum = drand48() * NUMCOLORS;
    }

    if (Flags.MeteorFrequency < 0 || Flags.MeteorFrequency > 100) {
        Flags.MeteorFrequency = DefaultFlags.MeteorFrequency;
    }
    float t = (0.5 + drand48()) *
              (Flags.MeteorFrequency * (0.1 - time_meteor) / 100 + time_meteor);
    P("do_meteor %f\n", t);
    addMethodToMainloop(PRIORITY_DEFAULT, t, do_meteor);
    return FALSE;
}
