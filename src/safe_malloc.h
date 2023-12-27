/* -copyright-
#-#
#-# plasmasnow: Let it snow on your desktop
#-# Copyright (C) 1984,1988,1990,1993-1995,2000-2001 Rick Jansen
#-# 	      2019,2020,2021,2022,2023 Willem Vermin
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
#include <gsl/gsl_errno.h>
#include <gsl/gsl_spline.h>
#include <stdio.h>
extern void *safe_malloc(size_t size);
extern void *safe_realloc(void *ptr, size_t size);
extern void safe_free(void *ptr);
extern void safe_gsl_spline_free(gsl_spline *spline);
extern void safe_gsl_interp_accel_free(gsl_interp_accel *acc);
extern int safe_XFree(void *data);

#define REALLOC_CHECK(x)                                                       \
    if (x == NULL) {                                                           \
        fprintf(stderr, "Realloc error in %s:%d\n", __FILE__, __LINE__);       \
        exit(1);                                                               \
    }                                                                          \
    struct dummy_to_require_semicolon

#define MALLOC_CHECK(x)                                                        \
    if (x == NULL) {                                                           \
        fprintf(stderr, "Malloc error in %s:%d\n", __FILE__, __LINE__);        \
        exit(1);                                                               \
    }                                                                          \
    struct dummy_to_require_semicolon
