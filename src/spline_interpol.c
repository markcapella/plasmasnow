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
 * */
#define GSL_INTERP_MESSAGE

#include <gsl/gsl_errno.h>
#include <gsl/gsl_spline.h>
#include <pthread.h>

#include "spline_interpol.h"


void spline_interpol(const double *px, int np, const double *py,
    const double *x, int nx, double *y) {
    int i;
    gsl_interp_accel *acc = gsl_interp_accel_alloc();
    gsl_spline *spline = gsl_spline_alloc(SPLINE_INTERP, np);
    gsl_spline_init(spline, px, py, np);

    for (i = 0; i < nx; i++) {
        y[i] = gsl_spline_eval(spline, x[i], acc);
    }

    gsl_spline_free(spline);
    gsl_interp_accel_free(acc);
}
