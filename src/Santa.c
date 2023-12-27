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

#include "Santa.h"
#include "debug.h"
#include "flags.h"
#include "ixpm.h"
#include "moon.h"
#include "pixmaps.h"
#include "safe_malloc.h"
#include "utils.h"
#include "wind.h"
#include "windows.h"
#include <X11/Intrinsic.h>
#include <X11/xpm.h>
#include <gtk/gtk.h>
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#define NOTACTIVE (!WorkspaceActive())
static int do_usanta();
static void init_Santa_surfaces(void);
static Region RegionCreateRectangle(int x, int y, int w, int h);
static void ResetSanta(void);
static void SetSantaSizeSpeed(void);
static void setSantaRegions(void);

static int CurrentSanta;
static Pixmap SantaMaskPixmap[PIXINANIMATION];
static Pixmap SantaPixmap[PIXINANIMATION];
static Region SantaRegion = 0;
static float SantaSpeed;
static float SantaXr;
static float SantaYr;
static int SantaYStep;
static int OldSantaX = 0; // the x value of Santa when he was last drawn
static int OldSantaY = 0; // the y value of Santa when he was last drawn
static const float LocalScale = 0.6;
static int MoonSeeking = 1;

//                                      size      rudolph  direction animation
static cairo_surface_t *Santa_surfaces[MAXSANTA + 1][2][2][PIXINANIMATION];

/* Speed for each Santa  in pixels/second*/
static float Speed[] = {
    SANTASPEED0, /* Santa 0 */
    SANTASPEED1, /* Santa 1 */
    SANTASPEED2, /* Santa 2 */
    SANTASPEED3, /* Santa 3 */
    SANTASPEED4, /* Santa 4 */
};

void Santa_ui() {
    UIDO(SantaSize, SetSantaSizeSpeed(););
    UIDO(Rudolf, SetSantaSizeSpeed(););
    UIDO(NoSanta, );
    UIDO(SantaSpeedFactor, SetSantaSizeSpeed(););
    UIDO(SantaScale, init_Santa_surfaces(); SetSantaSizeSpeed(););

    static int prev = 100;
    if (ScaleChanged(&prev)) {
        P("%d Santa_scale \n", global.counter);
        init_Santa_surfaces();
        SetSantaSizeSpeed();
    }
}

int Santa_draw(cairo_t *cr) {
    if (Flags.NoSanta) {
        return TRUE;
    }
    P("Santa_draw %d %d %d\n", global.counter++, global.SantaX,
        Flags.SantaSize);
    cairo_surface_t *surface;
    surface = Santa_surfaces[Flags.SantaSize][Flags.Rudolf]
                            [global.SantaDirection][CurrentSanta];
    cairo_set_source_surface(cr, surface, global.SantaX, global.SantaY);
    my_cairo_paint_with_alpha(cr, ALPHA);
    OldSantaX = global.SantaX;
    OldSantaY = global.SantaY;
    return TRUE;
}

void Santa_erase(cairo_t *cr) {
    P("Santa_erase %d %d\n", OldSantaX, OldSantaY);
    (void)cr;
    myXClearArea(global.display, global.SnowWin, OldSantaX, OldSantaY,
        global.SantaWidth + 1, global.SantaHeight, global.xxposures);
}

void Santa_init() {
    P("Santa_init\n");
    int i;
    for (i = 0; i < PIXINANIMATION; i++) {
        SantaPixmap[i] = 0;
        SantaMaskPixmap[i] = 0;
    }
    int j, k;
    for (i = 0; i < MAXSANTA + 1; i++) {
        for (j = 0; j < 2; j++) {
            for (k = 0; k < PIXINANIMATION; k++) {
                Santa_surfaces[i][j][0][k] = NULL;
                Santa_surfaces[i][j][1][k] = NULL;
            }
        }
    }

    SantaRegion = XCreateRegion();
    global.SantaPlowRegion = XCreateRegion();
    init_Santa_surfaces();
    SetSantaSizeSpeed();
    if (drand48() > 0.5) {
        global.SantaDirection = 0;
    } else {
        global.SantaDirection = 1;
    }
    ResetSanta();
    add_to_mainloop(PRIORITY_HIGH, time_usanta, do_usanta);
}

void init_Santa_surfaces() {
    P("init_Santa_surfaces\n");
    GdkPixbuf *pixbuf;
    int i, j, k;
    for (i = 0; i < MAXSANTA + 1; i++) {
        for (j = 0; j < 2; j++) {
            for (k = 0; k < PIXINANIMATION; k++) {
                int w, h;
                sscanf(Santas[i][j][k][0], "%d %d", &w, &h);
                w *= 0.01 * Flags.Scale * LocalScale * global.WindowScale *
                     0.01 * Flags.SantaScale;
                h *= 0.01 * Flags.Scale * LocalScale * global.WindowScale *
                     0.01 * Flags.SantaScale;
                P("%d init_Santa_surfaces %d %d %d %d %d\n", global.counter++,
                    i, j, k, w, h);
                pixbuf = gdk_pixbuf_new_from_xpm_data(
                    (const char **)Santas[i][j][k]);
                if (w < 1) {
                    w = 1;
                }
                if (h < 1) {
                    h = 1;
                }
                if (w == 1 && h == 1) {
                    h = 2;
                }
                GdkPixbuf *pixbufscaled0 =
                    gdk_pixbuf_scale_simple(pixbuf, w, h, GDK_INTERP_HYPER);

                GdkPixbuf *pixbufscaled1 = gdk_pixbuf_flip(pixbufscaled0, TRUE);

                if (Santa_surfaces[i][j][0][k]) {
                    cairo_surface_destroy(Santa_surfaces[i][j][0][k]);
                    cairo_surface_destroy(Santa_surfaces[i][j][1][k]);
                }

                Santa_surfaces[i][j][0][k] =
                    gdk_cairo_surface_create_from_pixbuf(
                        pixbufscaled0, 0, NULL);
                Santa_surfaces[i][j][1][k] =
                    gdk_cairo_surface_create_from_pixbuf(
                        pixbufscaled1, 0, NULL);
                g_clear_object(&pixbuf);
                g_clear_object(&pixbufscaled0);
                g_clear_object(&pixbufscaled1);
            }
        }
    }
    int ok = 1;
    char *path[PIXINANIMATION];
    const char *filenames[] = {
        "plasmasnow/pixmaps/santa1.xpm",
        "plasmasnow/pixmaps/santa2.xpm",
        "plasmasnow/pixmaps/santa3.xpm",
        "plasmasnow/pixmaps/santa4.xpm",
    };
    for (i = 0; i < PIXINANIMATION; i++) {
        path[i] = NULL;
        FILE *f = HomeOpen(filenames[i], "r", &path[i]);
        if (!f) {
            ok = 0;
            if (path[i]) {
                free(path[i]);
            }
            break;
        }
        fclose(f);
    }
    if (ok) {
        printf("Using external Santa: %s.\n", path[0]);
        printf("Use first Santa in menu to show Him.\n");
        fflush(stdout);
        /*
       if (!Flags.NoMenu)
       printf("Disabling menu.\n");
       Flags.NoMenu = 1;
       */
        int rc, i;
        char **santaxpm;
        for (i = 0; i < PIXINANIMATION; i++) {
            rc = XpmReadFileToData(path[i], &santaxpm);
            if (rc == XpmSuccess) {
                int w, h;
                sscanf(santaxpm[0], "%d %d", &w, &h);
                w *= 0.01 * Flags.Scale * LocalScale * global.WindowScale;
                h *= 0.01 * Flags.Scale * LocalScale * global.WindowScale;
                if (w < 1) {
                    w = 1;
                }
                if (h < 1) {
                    h = 1;
                }
                if (w == 1 && h == 1) {
                    h = 2;
                }
                pixbuf = gdk_pixbuf_new_from_xpm_data((const char **)santaxpm);
                GdkPixbuf *pixbufscaled0 =
                    gdk_pixbuf_scale_simple(pixbuf, w, h, GDK_INTERP_HYPER);
                GdkPixbuf *pixbufscaled1 = gdk_pixbuf_flip(pixbufscaled0, TRUE);
                cairo_surface_destroy(Santa_surfaces[0][0][0][i]);
                cairo_surface_destroy(Santa_surfaces[0][0][1][i]);
                Santa_surfaces[0][0][0][i] =
                    gdk_cairo_surface_create_from_pixbuf(
                        pixbufscaled0, 0, NULL);
                Santa_surfaces[0][0][1][i] =
                    gdk_cairo_surface_create_from_pixbuf(
                        pixbufscaled1, 0, NULL);
                XpmFree(santaxpm);
                g_clear_object(&pixbuf);
                g_clear_object(&pixbufscaled0);
                g_clear_object(&pixbufscaled1);
            } else {
                printf("Invalid external xpm for Santa given: %s\n", path[i]);
                exit(1);
            }
            free(path[i]);
        }
        Flags.SantaSize = 0;
        Flags.Rudolf = 0;
    }
    setSantaRegions();
}

void SetSantaSizeSpeed() {
    SantaSpeed = Speed[Flags.SantaSize];
    if (Flags.SantaSpeedFactor < 10) {
        SantaSpeed = 0.1 * SantaSpeed;
    } else {
        SantaSpeed = 0.01 * Flags.SantaSpeedFactor * SantaSpeed;
    }
    global.ActualSantaSpeed = SantaSpeed;
    // init_Santa_surfaces();
    cairo_surface_t *surface =
        Santa_surfaces[Flags.SantaSize][Flags.Rudolf][global.SantaDirection]
                      [CurrentSanta];
    global.SantaWidth = cairo_image_surface_get_width(surface);
    global.SantaHeight = cairo_image_surface_get_height(surface);
    setSantaRegions();
}

// update santa's coordinates and speed
int do_usanta() {
    P("do_usanta %d\n", counter++);

    if (Flags.Done) {
        return FALSE;
    }
#define RETURN                                                                 \
    do {                                                                       \
        return TRUE;                                                           \
    } while (0)
    if (NOTACTIVE) {
        RETURN;
    }
    if (Flags.NoSanta && !Flags.FollowSanta) {
        RETURN;
    }
    double yspeed;
    static int yspeeddir = 0;
    static double sdt = 0;
    static double dtt = 0;

    int oldx = global.SantaX;
    int oldy = global.SantaY;

    double dt = time_usanta;

    double santayrmin = 0;
    double santayrmax = global.SnowWinHeight * 0.33;

    // global.ActualSantaSpeed is the absolute value of Santa's speed
    // as is SantaSpeed
    if (global.SantaDirection == 0) {
        global.ActualSantaSpeed +=
            dt *
            (SANTASENS * global.NewWind + SantaSpeed - global.ActualSantaSpeed);
    } else {
        global.ActualSantaSpeed +=
            dt * (-SANTASENS * global.NewWind + SantaSpeed -
                     global.ActualSantaSpeed);
    }

    if (global.ActualSantaSpeed > 3 * SantaSpeed) {
        global.ActualSantaSpeed = 3 * SantaSpeed;
    } else if (global.ActualSantaSpeed < -2 * SantaSpeed) {
        global.ActualSantaSpeed = -2 * SantaSpeed;
    }

    if (global.SantaDirection == 0) {
        SantaXr += dt * global.ActualSantaSpeed;
    } else {
        SantaXr -= dt * global.ActualSantaSpeed;
    }

    P("SantaXr: %ld global.SnowWinWidth: %d\n", lrint(SantaXr),
        global.SnowWinWidth);
    if ((SantaXr >= global.SnowWinWidth && global.SantaDirection == 0) ||
        (SantaXr <= -global.SantaWidth && global.SantaDirection == 1)) {
        ResetSanta();
        oldx = global.SantaX;
        oldy = global.SantaY;
    }
    global.SantaX = lrintf(SantaXr);
    dtt += dt;
    if (dtt > 0.1) {
        dtt = 0;
        CurrentSanta++;
        if (CurrentSanta >= PIXINANIMATION) {
            CurrentSanta = 0;
        }
    }

    yspeed = global.ActualSantaSpeed / 4;
    sdt += dt;
    if (sdt > 2.0 * 50.0 / SantaSpeed || sdt > 2.0) {
        // time to change yspeed
        sdt = 0;
        yspeeddir = randint(3) - 1; //  -1, 0, 1
        if (SantaYr < santayrmin + 20) {
            yspeeddir = 2;
        }

        if (SantaYr > santayrmax - 20) {
            yspeeddir = -2;
        }
        int mooncy = global.moonY + Flags.MoonSize / 2;
        int ms;
        if (global.SantaDirection == 0) {
            ms = MoonSeeking && Flags.Moon &&
                 global.SantaX + global.SantaWidth <
                     global.moonX + Flags.MoonSize &&
                 global.SantaX + global.SantaWidth >
                     global.moonX - 300; // Santa likes to hover the moon
        } else {
            ms = MoonSeeking && Flags.Moon && global.SantaX > global.moonX &&
                 global.SantaX <
                     global.moonX + 300; // Santa likes to hover the moon
        }

        if (ms) {
            int dy = global.SantaY + global.SantaHeight / 2 - mooncy;
            if (dy < 0) {
                yspeeddir = 1;
            } else {
                yspeeddir = -1;
            }
            if (dy < -global.moonR / 2) { // todo
                yspeeddir = 3;
            } else if (dy > Flags.MoonSize / 2) {
                yspeeddir = -3;
            }
            P("moon seeking %f %f %d %f\n", SantaYr, moonY, yspeeddir,
                SantaSpeed);
        }
    }

    SantaYr += dt * yspeed * yspeeddir;
    if (SantaYr < santayrmin) {
        SantaYr = 0;
    }

    if (SantaYr > santayrmax) {
        SantaYr = santayrmax;
    }

    global.SantaY = lrintf(SantaYr);
    XOffsetRegion(SantaRegion, global.SantaX - oldx, global.SantaY - oldy);
    XOffsetRegion(
        global.SantaPlowRegion, global.SantaX - oldx, global.SantaY - oldy);

    RETURN;
}

void ResetSanta() {
    // Most of the times, Santa will reappear at the side where He disappears
    if (drand48() > 0.2) {
        global.SantaDirection = 1 - global.SantaDirection;
    }
    // place Santa somewhere before the left edge or after the right edge of the
    // screen
    int offset = global.SantaWidth * (drand48() + 2);
    if (global.SantaDirection == 1) {
        offset -= global.SantaWidth;
    }
    // offset = 0;
    if (global.SantaDirection == 0) {
        global.SantaX = -offset;
    } else {
        global.SantaX = global.SnowWinWidth + offset;
    }
    P("%d\n", global.SantaX);
    SantaXr = global.SantaX;
    global.SantaY = randint(global.SnowWinHeight / 3) + 40;
    // sometimes Santa is moon seeking, sometimes not
    MoonSeeking = drand48() > 0.5;
    P("MoonSeeking: %d\n", MoonSeeking);
    int ms;
    if (global.SantaDirection == 0) {
        ms = MoonSeeking && Flags.Moon && global.moonX < 400;
    } else {
        ms = MoonSeeking && Flags.Moon &&
             global.moonX > global.SnowWinWidth - 400;
    }

    if (ms) {
        P("moon seeking at start\n");
        global.SantaY = randint(Flags.MoonSize + 40) + global.moonY - 20;
    } else {
        global.SantaY = randint(global.SnowWinHeight / 3) + 40;
    }
    SantaYr = global.SantaY;
    SantaYStep = 1;
    CurrentSanta = 0;
    SetSantaSizeSpeed();
}

void SantaVisible() {
    global.SantaX = global.SnowWinWidth / 3;
    global.SantaY = global.SnowWinHeight / 6 + 40;
    SantaXr = global.SantaX;
    SantaYr = global.SantaY;
}

void setSantaRegions() {
    P("setSantaRegions %d %d %d %d\n", global.SantaX, global.SantaY,
        global.SantaWidth, global.SantaHeight);
    if (SantaRegion) {
        XDestroyRegion(SantaRegion);
    }
    SantaRegion = RegionCreateRectangle(
        global.SantaX, global.SantaY, global.SantaWidth, global.SantaHeight);

    if (global.SantaPlowRegion) {
        XDestroyRegion(global.SantaPlowRegion);
    }
    if (global.SantaDirection == 0) {
        global.SantaPlowRegion =
            RegionCreateRectangle(global.SantaX + global.SantaWidth,
                global.SantaY, 1, global.SantaHeight);
    } else {
        global.SantaPlowRegion = RegionCreateRectangle(
            global.SantaX - 1, global.SantaY, 1, global.SantaHeight);
    }
}

Region RegionCreateRectangle(int x, int y, int w, int h) {
    XPoint p[5];
    p[0].x = x;
    p[0].y = y;
    p[1].x = x + w;
    p[1].y = y;
    p[2].x = x + w;
    p[2].y = y + h;
    p[3].x = x;
    p[3].y = y + h;
    p[4].x = x;
    p[4].y = y;
    return XPolygonRegion(p, 5, EvenOddRule);
}
