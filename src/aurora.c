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

#include "aurora.h"
#include "clocks.h"
#include "debug.h"
#include "flags.h"
#include "safe_malloc.h"
#include "snow.h"
#include "spline_interpol.h"
#include "utils.h"
#include "windows.h"
#include "plasmasnow.h"
#include <X11/Intrinsic.h>
#include <assert.h>
#include <errno.h>
#include <gsl/gsl_errno.h>
#include <gsl/gsl_spline.h>
#include <gtk/gtk.h>
#include <math.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>

#define NOTACTIVE (!WorkspaceActive())

static void *do_aurora(void *);

static void aurora_setparms(AuroraMap *a);

static void aurora_changeparms(AuroraMap *a);
static void aurora_computeparms(AuroraMap *a);
static void create_aurora_base(const double *y, int n, double *slant,
    int nslant, double theta, int nw, int np, aurora_t **z, int *nz);
static double cscale(double d, int imax, float ah, double az, double h);

static GdkRGBA color;
static const char *AuroraColor = "green";
static AuroraMap a;
static const double turnfuzz = 0.015;
static double alphamax = 0.7;

// do_aurora_real will paint using aurora_cr, which will be connected to
// aurora_surface1 Using a semaphore, aurora_surface1 will be coped to
// aurora_surface which is put on the screen in aurora_draw
//
static cairo_surface_t *aurora_surface = NULL;
static cairo_surface_t *aurora_surface1 = NULL;
static cairo_t *aurora_cr = NULL;

static sem_t copy_sem;
static sem_t comp_sem;
static sem_t init_sem;

static int lock_comp(void);
static int unlock_comp(void);
static int lock_init(void);
static int unlock_init(void);
static int lock_copy(void);
static int unlock_copy(void);

static unsigned short
    xsubi[3]; // this is used by erand48() in the compute thread

void aurora_init() {
    // lock everything
    lock_init();

    static int firstcall = 1;
    if (!firstcall) {
        if (!mGlobal.isDoubleBuffered) {
            aurora_erase();
        }
    }

    if (!gdk_rgba_parse(&color, AuroraColor)) {
        gdk_rgba_parse(&color, "rgb(255,165,0)");
    }

    if (Flags.AuroraWidth > 100) {
        Flags.AuroraWidth = 100;
    } else if (Flags.AuroraWidth < 10) {
        Flags.AuroraWidth = 10;
    }

    if (Flags.AuroraHeight > 100) {
        Flags.AuroraHeight = 100;
    } else if (Flags.AuroraHeight < 0) {
        Flags.AuroraHeight = 0;
    }

    if (Flags.AuroraBase > 95) {
        Flags.AuroraBase = 95;
    } else if (Flags.AuroraBase < 10) {
        Flags.AuroraBase = 10;
    }

    // the aurora surface can be somewhat larger than SnowWinWidth
    // and will be placed somewhat to the left
    int f = turnfuzz * mGlobal.SnowWinWidth;
    a.width = mGlobal.SnowWinWidth * Flags.AuroraWidth * 0.01 + f;
    a.base = mGlobal.SnowWinHeight * Flags.AuroraBase * 0.01;
    aurora_setparms(&a);

    if (aurora_surface) {
        cairo_surface_destroy(aurora_surface);
    }
    if (aurora_surface1) {
        cairo_surface_destroy(aurora_surface1);
    }
    if (aurora_cr) {
        cairo_destroy(aurora_cr);
    }

    aurora_surface =
        cairo_image_surface_create(CAIRO_FORMAT_ARGB32, a.width, a.base);
    aurora_surface1 =
        cairo_image_surface_create(CAIRO_FORMAT_ARGB32, a.width, a.base);
    aurora_cr = cairo_create(aurora_surface1);

    static pthread_t thread = 0;
    if (firstcall) {
        firstcall = 0;
        a.z = NULL;
        a.zh = NULL;
        a.za = NULL;
        a.zaa = NULL;
        a.fuzz = NULL;
        a.lfuzz = 0;
        xsubi[0] = drand48() * 100;
        xsubi[1] = drand48() * 100;
        xsubi[2] = drand48() * 100;
        pthread_create(&thread, NULL, do_aurora, &a);
    }

    unlock_init();
}

void aurora_sem_init() {
    sem_init(&comp_sem, 0, 1);
    sem_init(&init_sem, 0, 1);
    sem_init(&copy_sem, 0, 1);
}


int lock_comp() { return sem_wait(&comp_sem); }
int unlock_comp() { return sem_post(&comp_sem); }
int lock_init() { return sem_wait(&init_sem); }
int unlock_init() { return sem_post(&init_sem); }
int lock_copy() { return sem_wait(&copy_sem); }
int unlock_copy() { return sem_post(&copy_sem); }


void aurora_ui() {
    UIDO(Aurora, );

    UIDO(AuroraBase, aurora_init(););
    UIDO(AuroraHeight, aurora_init(););
    UIDO(AuroraWidth, aurora_init(););

    UIDO(AuroraBrightness, );
    UIDO(AuroraSpeed, );

    UIDO(AuroraLeft, aurora_init(); );
    UIDO(AuroraMiddle, aurora_init(););
    UIDO(AuroraRight, aurora_init(););
}


void aurora_draw(cairo_t *cr) {
    P("aurora_draw %d %d\n", Flags.Aurora, mGlobal.counter++);
    if (Flags.Aurora == 0) {
        return;
    }
    lock_copy();
    cairo_set_source_surface(cr, aurora_surface, a.x, 0);
    double alpha = a.alpha * 0.02 * Flags.AuroraBrightness;
    if (alpha > 1) {
        alpha = 1;
    } else if (alpha < 0) {
        alpha = 0;
    }
    P("alphas: %d %f %f %f\n", Flags.AuroraBrightness, alpha, a.alpha,
        ALPHA * alpha);
    cairo_paint_with_alpha(cr, ALPHA * alpha);
    unlock_copy();
}

void aurora_erase() {
    P("aurora_erase %d %d %d %d \n", a.x, a.y, a.width, a.height);
    sanelyCheckAndClearDisplayArea(mGlobal.display, mGlobal.SnowWin, a.x, a.y, a.width, a.base,
        mGlobal.xxposures);
}

void *do_aurora(void *d) {
    // (void) d;
    P("do_aurora %d %d\n", Flags.Aurora, mGlobal.counter++);
    while (1) {
        if (Flags.Done) {
            pthread_exit(NULL);
        }
        if (!(Flags.Aurora == 0 || NOTACTIVE)) {

            lock_comp();
            P("%d do_aurora\n", mGlobal.counter++);
            lock_init();
            AuroraMap *a = (AuroraMap *)d;
            int j;
            aurora_changeparms(a);

            cairo_save(aurora_cr);

            cairo_set_antialias(aurora_cr, CAIRO_ANTIALIAS_NONE);
            cairo_set_line_cap(aurora_cr, CAIRO_LINE_CAP_BUTT);
            cairo_set_line_cap(aurora_cr, CAIRO_LINE_CAP_SQUARE);
            cairo_set_line_cap(aurora_cr, CAIRO_LINE_CAP_ROUND);
            // clean the surface. Not needed with pthreads
            // cairo_set_source_rgba(aurora_cr,0,0,0,0);
            // cairo_set_operator(aurora_cr,CAIRO_OPERATOR_SOURCE);
            // cairo_paint(aurora_cr);
            // cairo_restore(aurora_cr);
            // cairo_save(aurora_cr);
            int imax = 100;

            // see https://www.cairographics.org/operators/
            // cairo_set_operator(aurora_cr,CAIRO_OPERATOR_DARKEN);
            // cairo_set_operator(aurora_cr,CAIRO_OPERATOR_COLOR_DODGE);
            // cairo_set_operator(aurora_cr,CAIRO_OPERATOR_COLOR_BURN);
            // cairo_set_operator(aurora_cr,CAIRO_OPERATOR_SOFT_LIGHT);
            // cairo_set_operator(aurora_cr,CAIRO_OPERATOR_LIGHTEN);
            // cairo_set_operator(aurora_cr,CAIRO_OPERATOR_EXCLUSION);
            // cairo_set_operator(aurora_cr,CAIRO_OPERATOR_LIGHTEN);
            // cairo_set_operator(aurora_cr,CAIRO_OPERATOR_SOURCE); // would be
            // nice , but doesn't work ..
            // cairo_set_operator(aurora_cr,CAIRO_OPERATOR_HSL_HUE);
            // cairo_set_operator(aurora_cr,CAIRO_OPERATOR_HSL_SATURATION);
            // cairo_set_operator(aurora_cr,CAIRO_OPERATOR_HSL_COLOR);
            // cairo_set_operator(aurora_cr,CAIRO_OPERATOR_HSL_LUMINOSITY);

            cairo_set_operator(aurora_cr, CAIRO_OPERATOR_DIFFERENCE);
            // cairo_set_operator(aurora_cr,CAIRO_OPERATOR_SCREEN);
            // cairo_set_operator(aurora_cr,CAIRO_OPERATOR_XOR);

            // draw aurora surface rectangle
            if (0) {
                cairo_set_source_rgba(
                    aurora_cr, color.red, color.green, color.blue, 1);
                cairo_set_line_width(aurora_cr, 4);
                cairo_move_to(aurora_cr, 0, 0);
                cairo_line_to(aurora_cr, a->width, 0);
                cairo_line_to(aurora_cr, a->width, a->base);
                cairo_line_to(aurora_cr, 0, a->base);
                cairo_line_to(aurora_cr, 0, 0);

                cairo_stroke(aurora_cr);
            }

            cairo_set_line_width(aurora_cr, a->step);
            cairo_pattern_t *vpattern =
                cairo_pattern_create_linear(0, 0, 0, imax);

            //                                         height r   g b a
            cairo_pattern_add_color_stop_rgba(
                vpattern, 0.0, 1, 0, 1, 0.05); // purple top
            cairo_pattern_add_color_stop_rgba(
                vpattern, 0.2, 1, 0, 1, 0.15); // purple top
            cairo_pattern_add_color_stop_rgba(
                vpattern, 0.3, 0, 1, 0, 0.2); // green
            cairo_pattern_add_color_stop_rgba(vpattern, 0.7, 0, 1, 0, 0.6);
            cairo_pattern_add_color_stop_rgba(
                vpattern, 0.8, 0.2, 1, 0, 0.8); // yellow-ish
            cairo_pattern_add_color_stop_rgba(vpattern, 1.0, 0.1, 1, 0, 0.0);

            cairo_surface_t *vertsurf =
                cairo_image_surface_create(CAIRO_FORMAT_ARGB32, a->step, imax);
            cairo_t *vertcr = cairo_create(vertsurf);
            cairo_set_antialias(vertcr, CAIRO_ANTIALIAS_NONE);
            cairo_set_source(vertcr, vpattern);
            cairo_rectangle(vertcr, 0, 0, a->step, imax);
            cairo_fill(vertcr);

            int zmin = a->step * a->z[0].x;
            int zmax = a->step * a->z[0].x;
            for (j = 0; j < a->nz; j++) {
                double alpha = 1.0;
                double d = 100;
                double scale =
                    cscale(imax, d, a->hmax, a->zh[j], Flags.AuroraHeight);

                P("scale: %f\n", scale);
                P("theta: %f\n", a->theta);
                // P("scale: %d %f %f %f %f\n",k,p[k],h[k],d,scale);
                cairo_surface_set_device_scale(vertsurf, 1, scale);
                P("height: %d %f\n", j,
                    scale * cairo_image_surface_get_height(vertsurf));
                if (1) {
                    // draw aurora
                    cairo_set_source_surface(aurora_cr, vertsurf,
                        a->step * a->z[j].x, a->z[j].y - imax / scale);
                    cairo_paint_with_alpha(aurora_cr, alpha * a->za[j]);
                }
                if (0) {
                    // draw one aurora pillar
                    cairo_set_source_surface(aurora_cr, vertsurf, 2000, 400);
                    cairo_paint(aurora_cr);
                }
                P("zj: %d\n", a->step * z[j].x);
                if (a->step * a->z[j].x < zmin) {
                    zmin = a->z[j].x;
                }
                if (a->step * a->z[j].x > zmax) {
                    zmax = a->z[j].x;
                }
                // k++;
            }
            P("zmin: %d %d \n", zmin, zmax);
            if (1) {
                // draw fuzz
                for (j = 0; j < a->nfuzz; j++) {
                    double alpha = 1.0;
                    double d = 100;
                    double scale = cscale(
                        imax, d, a->hmax, a->fuzz[j].h, Flags.AuroraHeight);
                    P("fuzz[j]: %d %d %g %g %g\n", j, a->fuzz[j].x,
                        a->fuzz[j].y, a->fuzz[j].a, a->fuzz[j].h);
                    P("scale: %d %d %d %f\n", j, a->fuzz[j].x, a->nfuzz, scale);
                    P("theta: %f\n", a->theta);
                    cairo_surface_set_device_scale(vertsurf, 1, scale);
                    P("height: %d %f\n", j,
                        scale * cairo_image_surface_get_height(vertsurf));
                    if (1) {
                        // draw fuzz
                        cairo_set_source_surface(aurora_cr, vertsurf,
                            a->step * a->fuzz[j].x,
                            a->fuzz[j].y - imax / scale);
                        cairo_paint_with_alpha(aurora_cr, alpha * a->fuzz[j].a);
                        // yes: draw it twice to get the same luminosity as
                        // aurora near turning points
                        cairo_set_source_surface(aurora_cr, vertsurf,
                            a->step * a->fuzz[j].x,
                            a->fuzz[j].y - imax / scale);
                        cairo_paint_with_alpha(aurora_cr, alpha * a->fuzz[j].a);
                    }
                }
            }
            if (0) {
                // draw base of aurora
                cairo_save(aurora_cr);
                cairo_set_operator(aurora_cr, CAIRO_OPERATOR_OVER);
                cairo_set_source_rgba(aurora_cr, 1, 1, 0, 1);
                for (j = 0; j < a->nz; j++) {
                    cairo_rectangle(
                        aurora_cr, a->step * a->z[j].x, a->z[j].y - 2, 1, 1);
                }
                cairo_fill(aurora_cr);
                cairo_set_source_rgba(aurora_cr, 1, 0, 0, 1);
                for (j = 0; j < a->nfuzz; j++) {
                    cairo_rectangle(aurora_cr, a->step * a->fuzz[j].x,
                        a->fuzz[j].y - 2, 1, 1);
                }
                cairo_fill(aurora_cr);
                cairo_restore(aurora_cr);
            }

            cairo_destroy(vertcr);
            cairo_surface_destroy(vertsurf);
            cairo_pattern_destroy(vpattern);
            cairo_restore(aurora_cr);
            // free(p);

            P("%d do_aurora surface %d %d\n", mGlobal.counter++, a->width,
                a->base);
            // make available just created surface as aurora_surface
            // and create a new aurora_surface1
            lock_copy();
            cairo_surface_destroy(aurora_surface);
            cairo_destroy(aurora_cr);
            aurora_surface = aurora_surface1;
            unlock_copy();
            aurora_surface1 = cairo_image_surface_create(
                CAIRO_FORMAT_ARGB32, a->width, a->base);
            aurora_cr = cairo_create(aurora_surface1);
            unlock_comp();
            unlock_init();
        }
        P("%d speed %d %f\n", mGlobal.counter++, Flags.AuroraSpeed,
            1000000 * time_aurora / (0.2 * Flags.AuroraSpeed));
        usleep((useconds_t) (1.0e6 * time_aurora / (0.2 * Flags.AuroraSpeed)));
    }
    return NULL;
}

double cscale(double d, int imax, float ah, double az, double h) {
    double scale = imax / d;
    double s = 1.8 - 0.016 * h;

    scale = s * imax / ah / az;

    if (scale < 0.125) {
        scale = 0.125;
    }
    if (scale > 4) {
        scale = 4;
    }
    P("scale1: %f\n", scale);
    return scale;
}

void aurora_setparms(AuroraMap *a) {
    int f = turnfuzz * mGlobal.SnowWinWidth + 2;
    a->w = a->width - 2 * f; // Flags.AuroraWidth*0.01*mGlobal.SnowWinWidth;

    a->xoffset = f;
    if (Flags.AuroraLeft) {
        a->x = -f / 2; // a->x = f;
    } else if (Flags.AuroraMiddle) {
        a->x = (mGlobal.SnowWinWidth - a->width) / 2; // + f;
    } else {                                         // AuroraRight assumed
        a->x = mGlobal.SnowWinWidth - a->width;       // - f;
    }

    a->y = 0;

    int i;
    for (i = 0; i < AURORA_POINTS; i++) {
        a->points[i] = 0.2 + 0.4 * drand48();
        a->dpoints[i] = (2 * (i % 2) - 1) * 0.0005;
        P("a.dpoints %d %f\n", i, a.dpoints[i]);
    }

    a->slantmax = 3;
    for (i = 0; i < AURORA_S; i++) {
        a->slant[i] = a->slantmax * (2 * drand48() - 1);
        a->dslant[i] = (2 * (i % 2) - 1) * 0.02;
    }

    a->fuzzleft = 0.1;
    a->fuzzright = 0.1;
    a->alpha = drand48() * alphamax;
    a->dalpha = 0.011;
    a->theta = drand48() * 360;
    a->dtheta = 0.2;

    a->hmax = 0.6 * a->base;
    if (a->hmax < 3) {
        a->hmax = 3;
    }
    for (i = 0; i < AURORA_H; i++) {
        a->h[i] = 0.8 * drand48() + 0.2;
        a->dh[i] = (2 * (i % 2) - 1) * 0.01;
    }
    for (i = 0; i < AURORA_A; i++) {
        a->a[i] = 0.5 * drand48() + 0.5;
        a->da[i] = (2 * (i % 2) - 1) * 0.01;
    }
    for (i = 0; i < AURORA_AA; i++) {
        a->aa[i] = drand48();
        a->daa[i] = (2 * (i % 2) - 1) * 0.01;
    }

    a->step = 1;
    aurora_computeparms(a);
}

void aurora_changeparms(AuroraMap *a) {
    int i;
    if (1) {
        a->alpha += a->dalpha;
        if (a->alpha > alphamax) {
            a->dalpha = -fabs(a->dalpha);
        } else if (a->alpha < 0.4) {
            a->dalpha = fabs(a->dalpha);
        }
        P("a,alpha: %f\n", a->alpha);
    }

    if (1) {
        // global shape of aurora
        for (i = 0; i < AURORA_POINTS; i++) {
            a->points[i] += a->dpoints[i] * erand48(xsubi);
            if (a->points[i] > 1) {
                a->points[i] = 1;
                a->dpoints[i] = -fabs(a->dpoints[i]);
            } else if (a->points[i] < 0) {
                a->points[i] = 0;
                a->dpoints[i] = fabs(a->dpoints[i]);
            }
        }
    }

    if (1) {
        // rotation angle, including some not used methods
        if (0) {
            if (a->theta > -8 && a->theta < 8) {
                a->theta += 0.2 * a->dtheta;
            } else {
                a->theta += a->dtheta;
                // if (a->theta > 40)
                if (a->theta > 360) {
                    a->theta = 360;
                    a->dtheta = -fabs(a->dtheta);
                }
                // else if(a->theta < -40)
                else if (a->theta < 0) {
                    a->theta = 0;
                    a->dtheta = fabs(a->dtheta);
                }
            }
        }
        if (0) {
            double dt;
            // around 90 and 270 degrees, the changes are relatively large, that
            // is why cos() is used.
            dt = fabs(cos(M_PI * a->theta / 180.0)) + 0.2 * a->dtheta;
            a->theta += dt * a->dtheta * (erand48(xsubi) + 0.5);
            if (a->theta > 360) {
                a->theta -= 360;
            }
            P("a.theta: %f %f %f\n", a->theta, dt, a->dtheta);
        }
        if (1) {
            double dt = 0.2;
            a->theta += dt * a->dtheta * (erand48(xsubi) + 0.5);
            if (a->theta < 175) {
                a->theta = 175;
                a->dtheta = fabs(a->dtheta);
            } else if (a->theta > 185) {
                a->theta = 185;
                a->dtheta = -fabs(a->dtheta);
            }
            P("a.theta: %f\n", a->theta);
        }
    }
    // slant
    if (1) {
        for (i = 0; i < AURORA_S; i++) {
            a->slant[i] += a->dslant[i] * erand48(xsubi);
            if (a->slant[i] > a->slantmax) {
                a->slant[i] = a->slantmax;
                a->dslant[i] = -fabs(a->dslant[i]);
            } else if (a->slant[i] < -a->slantmax) {
                a->slant[i] = -a->slantmax;
                a->dslant[i] = fabs(a->dslant[i]);
            }
            P("slant: %d %f\n", i, a->slant[i]);
        }
    }

    if (1) {
        // height of aurora
        for (i = 0; i < AURORA_H; i++) {
            a->h[i] += a->dh[i] * erand48(xsubi);
            if (a->h[i] > 1) {
                a->h[i] = 1;
                a->dh[i] = -fabs(a->dh[i]);
            } else if (a->h[i] < 0.2) {
                a->h[i] = 0.2;
                a->dh[i] = fabs(a->dh[i]);
            }
        }
    }
    if (1) {
        // transparency of aurora
        for (i = 0; i < AURORA_A; i++) {
            a->a[i] += a->da[i] * erand48(xsubi);
            if (a->a[i] > 1.2) {
                a->a[i] = 1.2;
                a->da[i] = -fabs(a->da[i]);
            } else if (a->a[i] < 0.5) {
                a->a[i] = 0.5;
                a->da[i] = fabs(a->da[i]);
            }
        }
    }
    if (1) {
        // high frequency transparency of aurora
        for (i = 0; i < AURORA_AA; i++) {
            a->aa[i] += a->daa[i] * erand48(xsubi);
            if (a->aa[i] > 1.2) {
                a->aa[i] = 1.2;
                a->daa[i] = -fabs(a->daa[i]);
            } else if (a->aa[i] < 0.1) {
                a->aa[i] = 0.1;
                a->daa[i] = fabs(a->daa[i]);
            }
        }
    }
    aurora_computeparms(a);
}

void aurora_computeparms(AuroraMap *a) {
    int i;
    if (a->z) {
        free(a->z);
        a->z = NULL;
    }
    int np = a->w / a->step;
    create_aurora_base(a->points, AURORA_POINTS, a->slant, AURORA_S,
        a->theta * M_PI / 180.0, 0, np, &(a->z), &(a->nz));
    // normalize z.y values and z.x
    double ymin = a->z[0].y;
    double ymax = a->z[0].y;
    for (i = 0; i < a->nz; i++) {
        if (a->z[i].y < ymin) {
            ymin = a->z[i].y;
        }
        if (a->z[i].y > ymax) {
            ymax = a->z[i].y;
        }
        a->z[i].x += a->xoffset;
    }

    if (1) { // prevent too large jumps in position of aurora
        static double ymin_old = 1;
        static double ymax_old = 1;
        static const double dmax = 0.01;
        if (fabs(ymin - ymin_old) < 0.3) {
            if ((ymin - ymin_old) > dmax) {
                P("ymin %f\n", ymin - ymin_old);
                ymin = ymin_old + dmax;
            } else if ((ymin - ymin_old) < -dmax) {
                P("ymin %f\n", ymin - ymin_old);
                ymin = ymin_old - dmax;
            }
        }
        if (fabs(ymax - ymax_old) < 0.3) {
            if ((ymax - ymax_old) > dmax) {
                P("ymax %f\n", ymax - ymax_old);
                ymax = ymax_old + dmax;
            } else if ((ymax - ymax_old) < -dmax) {
                P("ymax %f\n", ymax - ymax_old);
                ymax = ymax_old - dmax;
            }
        }
        ymin_old = ymin;
        ymax_old = ymax;
    }

    double d = ymax - ymin;
    if (d < 0.1) {
        d = 0.1;
    }
    double s = (a->base - a->hmax) / d;
    P("ymin %f %f %f %f\n", ymin, ymax, d, s);
    for (i = 0; i < a->nz; i++) {
        a->z[i].y = a->base - (a->z[i].y - ymin) * s;
        P("a->z[i].y: %d %f\n", i, z[i].y);
    }

    // height
    double dx;
    double px[AURORA_H];
    for (i = 0; i < AURORA_H; i++) {
        px[i] = i;
    }
    if (a->zh) {
        free(a->zh);
        a->zh = NULL;
    }
    double *x = (double *)malloc(a->nz * sizeof(double));
    a->zh = (double *)malloc(a->nz * sizeof(double));
    dx = (double)(AURORA_H - 1) / (double)(a->nz - 1);
    for (i = 0; i < a->nz; i++) {
        x[i] = i * dx;
    }
    x[a->nz - 1] = px[AURORA_H - 1];
    spline_interpol(px, AURORA_H, a->h, x, a->nz, a->zh);

    // alpha values
    double pa[AURORA_A];
    for (i = 0; i < AURORA_A; i++) {
        pa[i] = i;
    }
    if (a->za) {
        free(a->za);
        a->za = NULL;
    }
    a->za = (double *)malloc(a->nz * sizeof(double));
    dx = (double)(AURORA_A - 1) / (double)(a->nz - 1);
    for (i = 0; i < a->nz; i++) {
        x[i] = i * dx;
    }
    x[a->nz - 1] = pa[AURORA_A - 1];
    spline_interpol(pa, AURORA_A, a->a, x, a->nz, a->za);

    // high frequency alpha
    double paa[AURORA_AA];
    for (i = 0; i < AURORA_AA; i++) {
        paa[i] = i;
    }
    if (a->zaa) {
        free(a->zaa);
        a->zaa = NULL;
    }
    a->zaa = (double *)malloc(a->nz * sizeof(double));
    dx = (double)(AURORA_AA - 1) / (double)(a->nz - 1);
    for (i = 0; i < a->nz; i++) {
        x[i] = i * dx;
    }
    x[a->nz - 1] = paa[AURORA_AA - 1];
    spline_interpol(paa, AURORA_AA, a->aa, x, a->nz, a->zaa);

    free(x);

    for (i = 0; i < a->nz; i++) {
        double alpha = 1;
        if (i < a->fuzzleft * a->nz) {
            alpha = ((float)i) / (a->fuzzleft * a->nz);
        }
        if (i > a->nz * (1 - a->fuzzright)) {
            alpha = ((float)(a->nz - i)) / (a->fuzzright * a->nz);
            P("alpha: %d %f\n", i, alpha);
        }
        alpha += 0.05 * a->zaa[i];
        a->za[i] *= alpha;
    }

    if (1) {
        // add fuzz on turning points, second method
        // add some points with diminishing alpha
        a->nfuzz = 0;
        int f = turnfuzz * mGlobal.SnowWinWidth;
        int d0 = a->z[1].x - a->z[0].x;
        i = 0;
        while (1) {
            i++;
            if (i >= a->nz - 1) {
                break;
            }
            int d = a->z[i + 1].x - a->z[i].x;

            if (d0 != d) {
                if (a->lfuzz <= a->nfuzz + f) {
                    a->lfuzz += 4 * f;
                    a->fuzz =
                        (fuzz_t *)realloc(a->fuzz, a->lfuzz * sizeof(fuzz_t));
                }
                P("d0 d: %d %d %d %d %d\n", a->z[i].x, d0, d, a->nfuzz,
                    a->lfuzz);
                if (d0 > 0) // edge on the right side
                {
                    P("Right side... %d %d %d %ld\n", a->z[i].x, a->nfuzz,
                        a->lfuzz, sizeof(fuzz_t));
                    int jmax = i + f;
                    if (jmax > a->nz - 1) {
                        jmax = a->nz - 1;
                    }
                    int jj = jmax;

                    int j;
                    for (j = i; j < jj; j++) {
                        if (a->z[j + 1].x - a->z[j].x != d) {
                            break;
                        }
                    }
                    if (jmax > j) {
                        P("adjust jmax: %d %d\n", jmax, j);
                        jmax = j;
                    }

                    jj = i - jmax;
                    if (jj < 1) {
                        jj = 1;
                    }
                    for (j = i - 1; j > jj; j--) {
                        P("a d: %d %d\n", d, (a->z[j - 1].x - a->z[j].x));
                        if (a->z[j - 1].x - a->z[j].x != d) {
                            P("break %d\n", j);
                            break;
                        }
                    }
                    if (jmax + j > 2 * i) {
                        P("Adjust\n");
                        jmax = 2 * i - j;
                    }

                    for (j = i; j < jmax; j++) {
                        a->fuzz[a->nfuzz].x = a->z[i].x + j - i;
                        a->fuzz[a->nfuzz].y = a->z[i].y;
                        double alpha =
                            a->za[i] * (double)(jmax - j) / (double)(jmax - i);
                        if (j == i) {
                            alpha *= 0.5;
                        }
                        a->fuzz[a->nfuzz].a = alpha;
                        a->fuzz[a->nfuzz].h = a->zh[i];
                        (a->nfuzz)++;
                    }
                } else // edge on the left side
                {
                    P("Left side... %d %d %d %ld\n", a->z[i].x, a->nfuzz,
                        a->lfuzz, sizeof(fuzz_t));
                    int jmin = i - f;
                    if (jmin < 1) {
                        jmin = 1;
                    }
                    int jj = i + f;
                    if (jj > a->nz - 1) {
                        jj = a->nz - 1;
                    }
                    int j;
                    for (j = i + 1; j < jj; j++) {
                        if (a->z[j].x - a->z[j - 1].x != d) {
                            break;
                        }
                    }
                    if (j + jmin < 2 * i) {
                        P("adjust jmin:  %d %d %d %d\n", jmin, j, a->z[i].x,
                            mGlobal.counter++);
                        jmin = 2 * i - j;
                    }

                    jj = i - jmin;
                    if (jj < 1) {
                        jj = 1;
                    }
                    for (j = i; j > jj; j--) {
                        P("a d: %d %d\n", d, (a->z[j - 1].x - a->z[j].x));
                        if (a->z[j - 1].x - a->z[j].x != d) {
                            P("break %d\n", j);
                            break;
                        }
                    }
                    if (j > jmin) {
                        jmin = j;
                    }

                    for (j = i; j > jmin; j--) {
                        a->fuzz[a->nfuzz].x = a->z[i].x - i + j;
                        a->fuzz[a->nfuzz].y = a->z[i].y;
                        double alpha =
                            a->za[i] * (double)(j - jmin) / (double)(i - jmin);
                        if (j == i) {
                            alpha *= 0.5;
                        }
                        a->fuzz[a->nfuzz].a = alpha;
                        a->fuzz[a->nfuzz].h = a->zh[i];
                        (a->nfuzz)++;
                    }
                }
            }
            d0 = d;
        }
    }
}

// https://www.gnu.org/software/gsl/doc/html/interp.html#sec-interpolation
// creates base for aurora
// double *y: input y coordinates of points to be used for interpolation
// int n: input: number of y points  e.g. 10 (n >= 3)
// double *slant: input: slants to be used (e.g 0..4)
// nslant: number of slant points, e.g. 2*n
// theta: rotation angle to be applied radials, e.g. 3.14/4.0
// int nw: input, length of scratch array each element 2 doubles, e.g. 1000
// (np*n should be enough)
//         if nw <= 0, np*n is used
// int np: input, length of output x-axis, e.g. 100
//     NOTE: the length of the output array z will in
//     general be larger than np.
// aurora_t **z: output: x and y coordinates of the requested aurora
//   this array is malloc'd in this function, and can be freed
//   using free()
// int *nz: output: number of output values in z
// The output values are available as in:
// for (i=0; i<nz; i++)
// {
//    int x    = z[i].x;
//    double y = z[i].y;
// }
//   Note that the same x value can occur more than once

void create_aurora_base(const double *y, int n, double *slant, int nslant,
    double theta, int nw, int np, aurora_t **z, int *nz) {
    int i;
    struct pq {
            double x;
            double y;
    } *p;
    double *x = (double *)malloc(n * sizeof(double));
    double *s = (double *)malloc(nslant * sizeof(double));
    for (i = 0; i < n; i++) {
        x[i] = i;
    }
    for (i = 0; i < nslant; i++) {
        s[i] = i;
    }

    gsl_interp_accel *acc = gsl_interp_accel_alloc();
    gsl_spline *spline = gsl_spline_alloc(SPLINE_INTERP, n);
    gsl_spline_init(spline, x, y, n);

    gsl_interp_accel *accs = gsl_interp_accel_alloc();
    gsl_spline *splines = gsl_spline_alloc(SPLINE_INTERP, nslant);
    gsl_spline_init(splines, s, slant, nslant);

    if (nw <= 0) {
        nw = np * n;
    }

    double dx = ((double)(n - 1)) / (nw - 1);
    p = (struct pq *)malloc(nw * sizeof(struct pq));
    double costheta = cos(theta);
    double sintheta = sin(theta);
    struct pq *pp = p;
    pp->x = -1.234; // Compiler warns that possibly pp->x will not be
                    // initialized ...
    pp->y = -1.234; // Same for this one.
                    // so we give them values here, which will be overwritten
                    // later on.

    for (i = 0; i < nw; i++) {
        double xi;
        if (i == nw - 1) {
            xi = x[n - 1];
        } else {
            xi = i * dx;
        }
        double yi = gsl_spline_eval(spline, xi, acc);
        double sl = gsl_spline_eval(splines, xi, accs);
        double ai = xi - sl * yi;
        pp->x = ai * costheta - yi * sintheta;
        pp->y = ai * sintheta + yi * costheta;
        pp++;
    }

    pp = p;
    double pmin = pp->x;
    double pmax = pp->x;
    for (i = 0; i < nw; i++) {
        double px = pp->x;
        if (px < pmin) {
            pmin = px;
        }
        if (px > pmax) {
            pmax = px;
        }
        pp++;
    }

    pp = p;
    double d = (pmax - pmin) / np;
    int m1 = (pp->x - pmin) / d;
    int lz = 1;
    m1 = (pp->x - pmin) / d;
    aurora_t *pz = (aurora_t *)malloc(lz * sizeof(aurora_t));
    pz[0].y = pp->y;
    pz[0].x = pp->x;

    int k = 1;
    for (i = 1; i < nw; i++) {
        int m;
        pp++;
        m = (pp->x - pmin) / d;

        if (k + abs(m - m1) >= lz) {
            lz += nw + abs(m - m1);
            pz = (aurora_t *)realloc(pz, lz * sizeof(aurora_t));
            REALLOC_CHECK(pz);
        }

        int j;
        if (m > m1) {
            for (j = m1 + 1; j <= m; j++) {
                pz[k].y = pp->y;
                pz[k].x = j;
                k++;
            }
            m1 = m;
        } else if (m < m1) {
            for (j = m1 - 1; j >= m; j--) {
                pz[k].y = pp->y;
                pz[k].x = j;
                k++;
            }
            m1 = m;
        }
    }
    *nz = k;
    *z = (aurora_t *)realloc(pz, k * sizeof(aurora_t));
    gsl_spline_free(spline);
    gsl_interp_accel_free(acc);
    gsl_spline_free(splines);
    gsl_interp_accel_free(accs);
    free(p);
    free(x);
    free(s);
}
