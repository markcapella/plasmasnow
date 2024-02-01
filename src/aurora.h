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

#include <gtk/gtk.h>
#define AURORA_POINTS 8                // shape
#define AURORA_H 6                     // height
#define AURORA_A 5                     // alpha
#define AURORA_AA 200                  // high-frequency alpha
#define AURORA_S 2 * AURORA_POINTS + 1 // slant

typedef struct _aurora_t {
        double y;
        int x;
} aurora_t;

typedef struct _fuzz_t {
        double y;
        double a;
        double h;
        int x;
} fuzz_t;

typedef struct _AuroraMap {
        int width;   // size of aurora surface
        int base;    // base of aurora surface
        int x, y;    // coordinates of aurora painting in screen coordinates
        int xoffset; // value to add to x value of aurora
        int w;       // width of aurora painting
        float hmax;  // max height of corona
        float fuzzleft, fuzzright; // left and right fading
        double alpha, dalpha;      // transparency and transparency change
        double theta, dtheta;      // rotation and rotation change

        double points[AURORA_POINTS]; // Points used for drawing spline bottom.
        double dpoints[AURORA_POINTS]; // Aurora delta (change) points.

        double h[AURORA_H], dh[AURORA_H]; // spline points for height of aurora
        double a[AURORA_A], da[AURORA_A]; // spline points for alpha of aurora

        double aa[AURORA_AA];
        double daa[AURORA_AA]; // spline points for high freqency alpha of aurora

        double slant[AURORA_S];
        double dslant[AURORA_S]; // spline points for slant factor of aurora

        double slantmax;

        int nz;      // number of points defining aurora

        double *zh;  // computed values from 'h' (nz of them)
        double *za;  // computed values from 'a' (nz of them)
        double *zaa; // computed values from 'aa' (nz of them)

        aurora_t *z; // values computed from 'points'

        int step;    // step size in pixels computing aurora

        fuzz_t *fuzz;
        int lfuzz, nfuzz;

} AuroraMap;

extern void aurora_init(void);
extern void aurora_ui(void);
extern void aurora_draw(cairo_t *cr);
extern void aurora_erase(void);
extern void aurora_sem_init(void);
