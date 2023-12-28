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
#include "utils.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// from the xpm package:
static void xpmCreatePixmapFromImage(
    Display *display, Drawable d, XImage *ximage, Pixmap *pixmap_return) {
    GC gc;
    XGCValues values;

    P("XCreatePixmap\n");
    *pixmap_return =
        XCreatePixmap(display, d, ximage->width, ximage->height, ximage->depth);
    /* set fg and bg in case we have an XYBitmap */
    values.foreground = 1;
    values.background = 0;
    gc = XCreateGC(
        display, *pixmap_return, GCForeground | GCBackground, &values);

    XPutImage(display, *pixmap_return, gc, ximage, 0, 0, 0, 0, ximage->width,
        ximage->height);

    XFreeGC(display, gc);
}

void paintit(XImage *img, long int color) {
    int x, y;
    for (y = 0; y < img->height; y++) {
        for (x = 0; x < img->width; x++) {
            XPutPixel(img, x, y, color);
        }
    }
}

// reverse characters in string, characters taken in chunks of l
// if you know what I mean
static void strrevert(char *s, size_t l) {
    assert(l > 0);
    size_t n = strlen(s) / l;
    size_t i;
    char *c = (char *)malloc(l * sizeof(*c));
    char *a = s;
    char *b = s + strlen(s) - l;
    for (i = 0; i < n / 2; i++) {
        strncpy(c, a, l);
        strncpy(a, b, l);
        strncpy(b, c, l);
        a += l;
        b -= l;
    }
    free(c);
}

//
//  equal to XpmCreatePixmapFromData, with extra flags:
//  flop: if 1, reverse the data horizontally
//  Extra: 0xff000000 is added to the pixmap data
//
int iXpmCreatePixmapFromData(Display *display, Drawable d, const char *data[],
    Pixmap *p, Pixmap *s, XpmAttributes *attr, int flop) {
    int rc, lines, i, ncolors, height, w;
    char **idata;

    sscanf(data[0], "%*s %d %d %d", &height, &ncolors, &w);
    lines = height + ncolors + 1;
    assert(lines > 0);
    idata = (char **)malloc(lines * sizeof(*idata));
    for (i = 0; i < lines; i++) {
        idata[i] = strdup(data[i]);
    }
    if (flop) {
        // flop the image data
        for (i = 1 + ncolors; i < lines; i++) {
            strrevert(idata[i], w);
        }
    }

    XImage *ximage = NULL, *shapeimage = NULL;
    rc = XpmCreateImageFromData(display, idata, &ximage, &shapeimage, attr);
    // NOTE: shapeimage is only created if color None is defined ...
    P("ximage shapeimage: %p %p\n", ximage, shapeimage);
    if (rc != 0) {
        I("rc from XpmCreateImageFromData: ");
        switch (rc) {
        case 1:
            printf("XpmColorError\n");
            for (i = 0; i < lines; i++) {
                printf("\"%s\",\n", idata[i]);
            }
            break;
        case -1:
            printf("XpmOpenFailed\n");
            break;
        case -2:
            printf("XpmFileInvalid\n");
            break;
        case -3:
            printf("XpmNoMemory: maybe issue with width of data: w=%d\n", w);
            break;
        case -4:
            printf("XpmColorFailed\n");
            for (i = 0; i < lines; i++) {
                printf("\"%s\",\n", idata[i]);
            }
            break;
        default:
            printf("%d\n", rc);
            break;
        }
        printf("exiting\n");
        fflush(NULL);
        abort();
    }
    XAddPixel(ximage, 0xff000000);
    if (p && ximage) {
        xpmCreatePixmapFromImage(display, d, ximage, p);
    }
    if (s && shapeimage) {
        xpmCreatePixmapFromImage(display, d, shapeimage, s);
    }
    if (ximage) {
        XDestroyImage(ximage);
    }
    if (shapeimage) {
        XDestroyImage(shapeimage);
    }
    for (i = 0; i < lines; i++) {
        free(idata[i]);
    }
    free(idata);
    return rc;
}

// given xpmdata **data, add the non-transparent pixels to Region r
Region regionfromxpm(const char **data, int flop, float scale) {
    (void)
        scale; // todo gdk_cairo_region_create_from_surface kan worden gebruikt
    int w, h, nc, n;
    Region r = XCreateRegion();
    // width, height, #colors, $chars to code color
    sscanf(*data, "%d %d %d %d", &w, &h, &nc, &n);
    // find color "None":
    int i;
    char *code = (char *)"";
    int offset = nc + 1;
    for (i = 1; i <= nc; i++) {
        char s[101];
        P("%s\n", data[i]);
        sscanf(data[i] + n, "%*s %100s", s);
        P("%s\n", s);
        if (!strcasecmp(s, "None")) {
            code = strndup(data[i], n);
            break;
        }
    }
    XRectangle rect;
    rect.width = 1;
    rect.height = 1;
    int y;
    for (y = 0; y < h; y++) {
        int x;
        char *s = strdup(data[y + offset]);
        if (flop) {
            strrevert(s, n);
        }
        for (x = 0; x < w; x++) {
            if (strncmp(s + n * x, code, n)) {
                rect.x = x;
                rect.y = y;
                XUnionRectWithRegion(&rect, r, r);
            }
        }
        free(s);
    }
    free(code);
    return r;
}

cairo_region_t *gregionfromxpm(const char **data, int flop, float scale) {
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

/*
 * converts xpm data to bitmap
 * xpm:        input xpm data
 * bitsreturn: output bitmap, allocated by this function
 * wreturn:    width of bitmap
 * hreturn:    height of bitmap
 * lreturn:    length of bitmap
 *
 * Return value: 1: OK, 0: not OK.
 * BUGS: this code has not been tested on a big endian system
 */
int xpmtobits(char *xpm[], unsigned char **bitsreturn, int *wreturn,
    int *hreturn, int *lreturn) {
    int nc, cpp, w, h;

    // unsigned char *bits = (unsigned char*) malloc(sizeof(unsigned char)*1);
    unsigned char *bits = NULL;
    if (sscanf(xpm[0], "%d %d %d %d", &w, &h, &nc, &cpp) != 4) {
        return 0;
    }
    if (cpp <= 0 || w < 0 || h < 0 || nc < 0) {
        return 0;
    }
    *wreturn = w;
    *hreturn = h;
    int l = ((int)w + 7) / 8; // # chars needed for this w
    *lreturn = l * h;
    // l*h+1: we do not want allocate 0 bytes
    bits = (unsigned char *)realloc(bits, sizeof(unsigned char) * (l * h + 1));
    REALLOC_CHECK(bits);
    *bitsreturn = bits;
    int i;
    for (i = 0; i < l * h; i++) {
        bits[i] = 0;
    }

    char *code = (char *)malloc(sizeof(char) * cpp);
    for (i = 0; i < cpp; i++) {
        code[i] = ' ';
    }

    int offset = nc + 1;
    for (i = 1; i <= nc; i++) {
        char s[101];
        if (strlen(xpm[i]) > (size_t)cpp + 6) {
            sscanf(xpm[i] + cpp, "%*s %100s", s);
            if (!strcasecmp(s, "none")) {
                free(code);
                code = strndup(xpm[i], cpp);
                break;
            }
        }
    }
    int y;
    unsigned char c = 0;
    int j = 0;
    if (is_little_endian()) {
        for (y = 0; y < h; y++) // little endian
        {
            int x, k = 0;
            const char *s = xpm[y + offset];
            int l = strlen(s);
            for (x = 0; x < w; x++) {
                c >>= 1;
                if (cpp * x + cpp <= l) {
                    if (strncmp(s + cpp * x, code, cpp)) {
                        c |= 0x80;
                    }
                }
                k++;
                if (k == 8) {
                    bits[j++] = c;
                    k = 0;
                }
            }
            if (k) {
                bits[j++] = c >> (8 - k);
            }
        }
    } else {
        for (y = 0; y < h; y++) // big endian  NOT tested
        {
            int x, k = 0;
            const char *s = xpm[y + offset];
            int l = strlen(s);
            for (x = 0; x < w; x++) {
                c <<= 1;
                if (cpp * x + cpp <= l) {
                    if (strncmp(s + cpp * x, code, cpp)) {
                        c |= 0x01;
                    }
                }
                k++;
                if (k == 8) {
                    bits[j++] = c;
                    k = 0;
                }
            }
            if (k) {
                bits[j++] = c << (8 - k);
            }
        }
    }

    free(code);
    return 1;
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

void xpm_print(char **xpm) {
    int w, h, nc;
    sscanf(xpm[0], "%d %d %d", &w, &h, &nc);
    int i, j;
    printf("%s\n", xpm[0]);
    for (i = 1; i < 1 + nc; i++) {
        printf("%s\n", xpm[i]);
    }
    for (i = 0; i < 2 * w + 2; i++) {
        printf("_");
    }
    printf("\n");
    for (i = 0; i < h; i++) {
        printf("|");
        for (j = 0; j < w; j++) {
            printf("%2c", xpm[i + nc + 1][j]);
        }
        printf("|");
        printf("\n");
    }
    for (i = 0; i < 2 * w + 2; i++) {
        printf("-");
    }
    printf("\n");
}
