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
#pragma once
#include "config.h"

#ifdef HAVE_GSL_INTERP_STEFFEN
// steffen's method prevent overshoots:
#define SPLINE_INTERP gsl_interp_steffen

#elif HAVE_GSL_INTERP_AKIMA
// akima's method tries to prevent overshoots:
#ifdef GSL_INTERP_MESSAGE
#pragma message "Using interpolation with Akima's method"
#endif
#define SPLINE_INTERP gsl_interp_akima

#elif HAVE_GSL_INTERP_CSPLINE
// classic spline with overshoots:
#ifdef GSL_INTERP_MESSAGE
#pragma message "Using interpolation with classic spline"
#endif
#define SPLINE_INTERP gsl_interp_cspline

#else
// linear interpolation:
#ifdef GSL_INTERP_MESSAGE
#pragma message "Using linear interpolation"
#endif
#define SPLINE_INTERP gsl_interp_linear
#endif

void spline_interpol(const double *p, int np, const double *py, const double *x,
    int nx, double *y);
