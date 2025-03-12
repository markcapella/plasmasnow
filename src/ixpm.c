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
#include "ixpm.h"
#include "debug.h"
#include "safe_malloc.h"
#include "Utils.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


cairo_region_t* gregionfromxpm(const char **data, int flop, float scale) {
    int w, h;
    sscanf(data[0], "%d %d", &w, &h);
    P("gregionfromxpm: w:%d h:%d\n", w, h);

    GdkPixbuf *pixbuf;
    GdkPixbuf *pixbuf1 = gdk_pixbuf_new_from_xpm_data(data);
    if (flop) {
        pixbuf = gdk_pixbuf_flip(pixbuf1, 1);
        g_clear_object(&pixbuf1);
    } else {
        pixbuf = pixbuf1;
    }

    int iw = w * scale;
    if (iw < 1) {
        iw = 1;
    }
    int ih = h * scale;
    if (ih < 1) {
        ih = 1;
    }
    if (iw == 1 && ih == 1) {
        ih = 2;
    }
    GdkPixbuf *pixbufscaled =
        gdk_pixbuf_scale_simple(pixbuf, iw, ih, GDK_INTERP_HYPER);
    cairo_surface_t *surface =
        gdk_cairo_surface_create_from_pixbuf(pixbufscaled, 0, NULL);
    g_clear_object(&pixbuf);
    g_clear_object(&pixbufscaled);

    cairo_region_t *region = gdk_cairo_region_create_from_surface(surface);
    cairo_surface_destroy(surface);
    return region;
}

// given color and xmpdata **data of a monocolored picture like:
//
// XPM_TYPE *snow06_xpm[] = {
///* columns rows colors chars-per-pixel */
//"3 3 2 1 ",
//"  c none",
//". c black",
///* pixels */
//". .",
//" . ",
//". ."
//};
// change the second color to color and put the result in out.
// lines will become the number of lines in out, comes in handy
// when wanting to free out.
void xpm_set_color(char **data, char ***out, int *lines, const char *color) {
    int n;

    sscanf(data[0], "%*d %d", &n);
    assert(n + 3 > 0);

    *out = (char **)malloc(sizeof(char *) * (n + 3));

    char **x = *out;
    int j;
    for (j = 0; j < 2; j++) {
        x[j] = strdup(data[j]);
    }

    x[2] = (char *)malloc(5 + strlen(color));
    x[2][0] = '\0';

    strcat(x[2], ". c ");
    strcat(x[2], color);
    P("c: [%s]\n", x[2]);

    for (j = 3; j < n + 3; j++) {
        x[j] = strdup(data[j]);
        P("%d %s\n", j, x[j]);
    }
    *lines = n + 3;
}

void xpm_destroy(char **data) {
    int h, nc;
    sscanf(data[0], "%*d %d %d", &h, &nc);
    int i;
    for (i = 0; i < h + nc + 1; i++) {
        free(data[i]);
    }
    free(data);
}
