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
#include <gsl/gsl_sort.h>
#include <math.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <X11/Intrinsic.h>

#include <gtk/gtk.h>


#include "Flags.h"
#include "meteor.h"
#include "mygettext.h"
#include "plasmasnow.h"
#include "safe_malloc.h"
#include "Utils.h"
#include "version.h"
#include "windows.h"
#include "xdo.h"

void traceback()
#ifdef TRACEBACK_AVAILALBLE
{
    // see man backtrace
#define BT_BUF_SIZE 100
    void *buffer[BT_BUF_SIZE];
    int nptrs = backtrace(buffer, BT_BUF_SIZE);
    backtrace_symbols_fd(buffer, nptrs, STDOUT_FILENO);
}
#else
{
}
#endif

int IsReadableFile(char *path) {
    if (!path || access(path, R_OK) != 0) {
        return 0;
    }
    struct stat path_stat;
    stat(path, &path_stat);
    return S_ISREG(path_stat.st_mode);
}

FILE *HomeOpen(const char *file, const char *mode, char **path) {
    char *h = getenv("HOME");
    if (h == NULL) {
        return NULL;
    }
    char *home = strdup(h);
    (*path) = (char *)malloc(strlen(home) + strlen(file) + 2);
    strcpy(*path, home);
    strcat(*path, "/");
    strcat(*path, file);
    FILE *f = fopen(*path, mode);
    free(home);
    return f;
}

void clearGlobalSnowWindow() {
    // remove all our snow-related drawings
    XClearArea(mGlobal.display, mGlobal.SnowWin, 0, 0, 0, 0, True);

    // Yes this is hairy: also remove meteor.
    // It could be that a meteor region is still hanging around
    eraseMeteorFrame();

    XFlush(mGlobal.display);
}

ssize_t mywrite(int fd, const void *buf, size_t count) {
    const size_t m = 4096; // max per write
    size_t w = 0;          // # written chars
    char *b = (char *)buf;

    while (w < count) {
        size_t l = count - w;
        if (l > m) {
            l = m;
        }
        ssize_t x = write(fd, b + w, l);
        if (x < 0) {
            return -1;
        }
        w += x;
    }
    return 0;
}

/** *********************************************************************
 ** Module MAINLOOP methods.
 **/
void sanelyCheckAndClearDisplayArea(Display *dsp, Window win,
    int x, int y, int w, int h, Bool exposures) {
    if (w == 0 || h == 0 ||
        w < 0 || h < 0 ||
        w > 20000 || h > 20000) {
        traceback();
        return;
    }

    XClearArea(dsp, win, x, y, w, h, exposures);
}

float sq3(float x, float y, float z) { return x * x + y * y + z * z; }

float sq2(float x, float y) { return x * x + y * y; }

float fsignf(float x) {
    if (x > 0) {
        return 1.0f;
    }
    if (x < 0) {
        return -1.0f;
    }
    return 0.0f;
}

int ValidColor(const char *colorName) {
    XColor scrncolor;
    XColor exactcolor;
    int scrn = DefaultScreen(mGlobal.display);
    return (
        XAllocNamedColor(mGlobal.display, DefaultColormap(mGlobal.display, scrn),
            colorName, &scrncolor, &exactcolor));
}

Pixel AllocNamedColor(const char *colorName, Pixel dfltPix) {
    XColor scrncolor;
    XColor exactcolor;
    int scrn = DefaultScreen(mGlobal.display);
    if (XAllocNamedColor(mGlobal.display, DefaultColormap(mGlobal.display, scrn),
            colorName, &scrncolor, &exactcolor)) {
        return scrncolor.pixel;
    } else {
        return dfltPix;
    }
}

Pixel IAllocNamedColor(const char *colorName, Pixel dfltPix) {
    return AllocNamedColor(colorName, dfltPix) | 0xff000000;
}

int randint(int m) {
    if (m <= 0) {
        return 0;
    }
    return drand48() * m;
}
// https://www.alanzucconi.com/2015/09/16/how-to-sample-from-a-gaussian-distribution/
// Interesting but not used now in app
double gaussian(double mean, double std, double min, double max) {
    double x;
    do {
        double v1, v2, s;
        do {
            v1 = 2.0 * drand48() - 1.0;
            v2 = 2.0 * drand48() - 1.0;
            s = v1 * v1 + v2 * v2;
        } while (s >= 1.0 || s == 0);
        x = mean + v1 * sqrt((-2.0 * log(s)) / s) * std;
    } while (x < min || x > max);
    return x;
}

guint addMethodToMainloop(gint prio, float time, GSourceFunc func) {
    return g_timeout_add_full(
        prio, (int)1000 * (time * (0.95 + 0.1 * drand48())), func, NULL, NULL);
}

guint addMethodWithArgToMainloop(
    gint prio, float time, GSourceFunc func, gpointer datap) {
    return g_timeout_add_full(
        prio, (int)1000 * (time) * (0.95 + 0.1 * drand48()), func, datap, NULL);
}

void remove_from_mainloop(guint *tag) {
    if (*tag) {
        g_source_remove(*tag);
    }
    *tag = 0;
}

int is_little_endian(void) {
    volatile int endiantest = 1;
    return (*(char *)&endiantest) == 1;
}

void paintCairoContextWithAlpha(cairo_t* cc, double alpha) {
    if (alpha > 0.9) {
        cairo_paint(cc);
    } else {
        cairo_paint_with_alpha(cc, alpha);
    }
}

/** *********************************************************************
 ** This method pretty-prints to log the app name, version, Author.
 **/
void logAppVersion() {
    const int numberOfStars = strlen(PACKAGE_STRING) + 4;

    printf("\n   ");
    for (int i = 0; i < numberOfStars; i++) {
        printf("*");
    }
    printf("\n   * %s *\n", PACKAGE_STRING);
    printf("   ");
    for (int i = 0; i < numberOfStars; i++) {
        printf("*");
    }

    printf("\n\n%s\n", VERSIONBY);
}

/** *********************************************************************
 ** This method ...
 **/
void rgba2color(GdkRGBA *c, char **s) {
    *s = (char *)malloc(8);
    sprintf(*s, "#%02lx%02lx%02lx", lrint(c->red * 255), lrint(c->green * 255),
        lrint(c->blue * 255));
}

/** *********************************************************************
 ** This method ...
 **/
int appScalesHaveChanged(int* prevscale) {
    const int newscale = (const int)
        (Flags.Scale * mGlobal.WindowScale);

    if (*prevscale != (newscale)) {
        *prevscale = newscale;
        return true;
    }
    return false;
}

// create sorted n random numbers in interval [0.0, 1.0) such that
// adjacent numbers differ at least d.
// If the distance between two numers is initially less than d,
// a new random number is drawn. this is repeated at most 100 times.
// On failure, the array is filled with equidistant numbers.
// parameters:
// double *a: the array to be filled
// int n:     number of items in array
// double d:  minimum difference between items
// unsigned short *seed: NULL: use drand48()
//                       pointer to array of 3 unsigned shorts: use erand48()
//                       see man drand48
//
void randomuniqarray(double *a, int n, double d, unsigned short *seed) {
    const int debug = 0;
    int i;
    if (seed) {

        for (i = 0; i < n; i++) {
            a[i] = erand48(seed);
        }
    } else {
        for (i = 0; i < n; i++) {
            a[i] = drand48();
        }
    }
    gsl_sort(a, 1, n);
    if (debug) {
        printf("\n");
        for (i = 0; i < n; i++) {
            printf("x %d %f\n", i, a[i]);
        }
    }

    int changes = 0;
    while (1) {
        int changed = 0;
        for (i = 0; i < n - 1; i++) {
            if (fabs(a[i + 1] - a[i]) < d) {
                // draw a new random number for a[i]
                if (debug) {
                    printf("changed %d %f %f\n", i, a[i + 1], a[i]);
                }
                changed = 1;
                if (seed) {
                    a[i] = erand48(seed);
                } else {
                    a[i] = drand48();
                }
            }
        }
        if (!changed) {
            return;
        }
        if (changed) {
            changes++;
            if (changes > 100) {
                // failure to create proper array, fill it with
                // equidistant numbers in stead
                if (debug) {
                    printf("changes %d\n", changes);
                }
                int i;
                double s = 1.0 / n;
                for (i = 0; i < n; i++) {
                    a[i] = i * s;
                }
                return;
            } else {
                gsl_sort(a, 1, n);
            }
        }
    }
}

// inspired by Mr. Gauss, and adapted for app
float gaussf(float x, float mu, float sigma) {
    float y = (x - mu) / sigma;
    float y2 = y * y;
    return expf(-y2);
}
// guess language. return string like "en", "nl" or NULL if no language can
// be found
char *guess_language() {
    const char *tries[] = {"LANGUAGE", "LANG", "LC_ALL", "LC_MESSAGES",
        "LC_NAME", "LC_TIME", NULL};
    char *a, *b;
    int i = 0;
    while (tries[i]) {
        a = getenv(tries[i]);
        if (a && strlen(a) > 0) {
            b = strdup(a);
            char *p = strchr(b, '_');
            if (p) {
                *p = 0;
                return b;
            } else {
                return b;
            }
            free(b);
        }
        ++i;
    }
    return NULL;
}

// find largest window with name
Window largest_window_with_name(xdo_t *myxdo, const char *name) {
    xdo_search_t search;

    memset(&search, 0, sizeof(xdo_search_t));

    search.searchmask = SEARCH_NAME;
    search.winname = name;
    search.require = SEARCH_ANY;
    search.max_depth = 4;
    search.limit = 0;

    Window *windows = NULL;
    unsigned int nwindows;
    xdo_search_windows(myxdo, &search, &windows, &nwindows);

    Window w = 0;
    unsigned int maxsize = 0;
    for (unsigned int i = 0; i < nwindows; i++) {
        unsigned int width, height;
        xdo_get_window_size(myxdo, windows[i], &width, &height);

        const unsigned int size = width * height;
        if (size <= maxsize) {
            continue;
        }
        maxsize = size;

        w = windows[i];
    }

    if (windows) {
        free(windows);
    }

    return w;
}
