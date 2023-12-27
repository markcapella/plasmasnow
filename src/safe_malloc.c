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
#include "safe_malloc.h"
#include <X11/Xlib.h>
#include <gsl/gsl_errno.h>
#include <gsl/gsl_spline.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

// #define USEMUTEX

#ifdef USEMUTEX
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
#define MUTEXLOCK pthread_mutex_lock(&mutex)
#define MUTEXUNLOCK pthread_mutex_unlock(&mutex)
#else
#define MUTEXLOCK
#define MUTEXUNLOCK
#endif

void *safe_malloc(size_t size) {
    MUTEXLOCK;
    void *p = malloc(size);
    if (p == NULL) {
        fprintf(stderr, "Fatal: failed to allocate %zu bytes.\n", size);
        abort();
    }
    MUTEXUNLOCK;
    return p;
}
void *safe_realloc(void *ptr, size_t size) {
    MUTEXLOCK;
    void *p = realloc(ptr, size);
    if (p == NULL) {
        fprintf(stderr, "Fatal: failed to reallocate %zu bytes.\n", size);
        abort();
    }
    MUTEXUNLOCK;
    return p;
}
void safe_free(void *ptr) {
    MUTEXLOCK;
    free(ptr);
    MUTEXUNLOCK;
}
void safe_gsl_spline_free(gsl_spline *spline) {
    MUTEXLOCK;
    gsl_spline_free(spline);
    MUTEXUNLOCK;
}
void safe_gsl_interp_accel_free(gsl_interp_accel *acc) {
    MUTEXLOCK;
    gsl_interp_accel_free(acc);
    MUTEXUNLOCK;
}
int safe_XFree(void *data) {
    MUTEXLOCK;
    return XFree(data);
    MUTEXUNLOCK;
}
