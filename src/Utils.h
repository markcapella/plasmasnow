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

#define SOMENUMBER 42
#define PRIORITY_DEFAULT G_PRIORITY_LOW
#define PRIORITY_HIGH G_PRIORITY_DEFAULT

#include <X11/Intrinsic.h>
#include <gtk/gtk.h>
#include <math.h>

#ifdef HAVE_EXECINFO_H
#ifdef HAVE_BACKTRACE
#include <execinfo.h>
#define TRACEBACK_AVAILALBLE
#endif
#endif

#include "xdo.h"

guint addMethodWithArgToMainloop(gint prio,
    float time, GSourceFunc method, gpointer arg);

FILE *HomeOpen(const char *file, const char *mode, char **path);

void remove_from_mainloop(guint *tag);
void clearGlobalSnowWindow(void);
float fsignf(float x);


float sq2(float x, float y);
float sq3(float x, float y, float z);

Pixel IAllocNamedColor(const char *colorName, Pixel dfltPix);
Pixel AllocNamedColor(const char *colorName, Pixel dfltPix);

#ifdef __cplusplus
extern "C" {
#endif
    guint addMethodToMainloop(gint prio, float time,
        GSourceFunc method);
    int randint(int m);
    void clearDisplayArea(Display* display,
        Window win, int x, int y, int w, int h,
        int exposures);
    void my_cairo_paint_with_alpha(cairo_t* cc, double alpha);
    int appScalesHaveChanged(int* prev);
#ifdef __cplusplus
}
#endif

void rgba2color(GdkRGBA* c, char** s);


int IsReadableFile(char *path);
int ValidColor(const char *color);
ssize_t mywrite(int fd, const void *buf, size_t count);
float gaussf(float x, float mu, float sigma);
char *guess_language(void);
Window largest_window_with_name(xdo_t *myxdo, const char *name);
void fill_xdo_search(xdo_search_t *search);

// obtain normally distributed number. The number will be between min and max:
double gaussian(
    double mean, double standard_deviation, double min, double max);

int is_little_endian(void);
void logAppVersion(void);

void randomuniqarray(double *a, int n, double d, unsigned short *seed);

void traceback(void);
