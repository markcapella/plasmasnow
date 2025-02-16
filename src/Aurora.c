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

#include <assert.h>
#include <errno.h>
#include <math.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>

#include <X11/Intrinsic.h>

#include <gtk/gtk.h>

#include <gsl/gsl_errno.h>
#include <gsl/gsl_spline.h>

#include "Aurora.h"
#include "clocks.h"
#include "Flags.h"
#include "PlasmaSnow.h"
#include "safe_malloc.h"
#include "spline_interpol.h"
#include "Storm.h"
#include "Utils.h"
#include "Windows.h"


/** *********************************************************************
 ** Module globals and consts.
 **/
bool mAuroraHasBeenInitialized = false;
pthread_t mThread = 0;

sem_t mAuroraSemaphore;
sem_t mInitializeSemaphore;
sem_t mCopySemaphore;

AuroraMap mAuroraMap;
const double turnfuzz = 0.015;
double alphamax = 0.7;

cairo_surface_t* aurora_surface = NULL;
cairo_surface_t* aurora_surface1 = NULL;

cairo_t* aurora_cr = NULL;

unsigned short xsubi[3];


/** *********************************************************************
 ** This method ...
 **/
void lazyInitAuroraModule() {
    if (!Flags.ShowAurora) {
        return;
    }

    // lock everything
    lock_init();

    if (mAuroraHasBeenInitialized) {
        if (!mGlobal.isDoubleBuffered) {
            eraseAuroraFrame();
        }
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
    const int FUZZ = turnfuzz * mGlobal.SnowWinWidth;
    mAuroraMap.width = mGlobal.SnowWinWidth * Flags.AuroraWidth * 0.01 + FUZZ;

    // Base from 10 .. 95 --> 95 .. 10.
    const int INVERTED_LIFT = (10 + 95) - Flags.AuroraBase;
    mAuroraMap.base = mGlobal.SnowWinHeight * INVERTED_LIFT * 0.01;

    aurora_setparms(&mAuroraMap);

    // Remove existing surfaces.
    if (aurora_surface) {
        cairo_surface_destroy(aurora_surface);
    }
    if (aurora_surface1) {
        cairo_surface_destroy(aurora_surface1);
    }
    if (aurora_cr) {
        cairo_destroy(aurora_cr);
    }

    // Create new ones.
    aurora_surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
        mAuroraMap.width, mAuroraMap.base);
    aurora_surface1 = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
        mAuroraMap.width, mAuroraMap.base);
    aurora_cr = cairo_create(aurora_surface1);

    if (!mAuroraHasBeenInitialized) {
        mAuroraHasBeenInitialized = true;
        mAuroraMap.z = NULL;
        mAuroraMap.zh = NULL;
        mAuroraMap.za = NULL;
        mAuroraMap.zaa = NULL;
        mAuroraMap.fuzz = NULL;
        mAuroraMap.lfuzz = 0;
        xsubi[0] = drand48() * 100;
        xsubi[1] = drand48() * 100;
        xsubi[2] = drand48() * 100;
        pthread_create(&mThread, NULL, do_aurora, &mAuroraMap);
    }

    unlock_init();
}

/** *********************************************************************
 ** This method ...
 **/
void aurora_ui() {
    UIDO(ShowAurora, lazyInitAuroraModule(););

    UIDO(AuroraBase, lazyInitAuroraModule(););
    UIDO(AuroraHeight, lazyInitAuroraModule(););
    UIDO(AuroraWidth, lazyInitAuroraModule(););

    UIDO(AuroraBrightness, lazyInitAuroraModule(););
    UIDO(AuroraSpeed, lazyInitAuroraModule(););

    UIDO(AuroraLeft, lazyInitAuroraModule(););
    UIDO(AuroraMiddle, lazyInitAuroraModule(););
    UIDO(AuroraRight, lazyInitAuroraModule(););
}

/** *********************************************************************
 ** This method ...
 **/
void aurora_draw(cairo_t *cr) {
    if (!Flags.ShowAurora) {
        return;
    }

    lock_copy();
    cairo_set_source_surface(cr, aurora_surface, mAuroraMap.x, 0);

    double alpha = mAuroraMap.alpha * 0.02 * Flags.AuroraBrightness;
    if (alpha > 1) {
        alpha = 1;
    } else if (alpha < 0) {
        alpha = 0;
    }

    cairo_paint_with_alpha(cr, ALPHA * alpha);
    unlock_copy();
}

/** *********************************************************************
 ** This method ...
 **/
void eraseAuroraFrame() {
    clearDisplayArea(mGlobal.display, mGlobal.SnowWin,
        mAuroraMap.x, mAuroraMap.y, mAuroraMap.width, mAuroraMap.base,
        mGlobal.xxposures);
}

/** *********************************************************************
 ** This method ...
 **/
void* do_aurora(void* d) {
    while (true) {
        if (Flags.shutdownRequested) {
            pthread_exit(NULL);
        }

        if (!Flags.ShowAurora || !WorkspaceActive()) {
            usleep((useconds_t) (1.0e6 * time_aurora /
                (0.2 * Flags.AuroraSpeed)));
            continue;
        }

        lock_comp();
        lock_init();

        AuroraMap* auroraMap = (AuroraMap*) d;
        aurora_changeparms(auroraMap);

        cairo_save(aurora_cr);
        cairo_set_antialias(aurora_cr, CAIRO_ANTIALIAS_NONE);
        cairo_set_line_cap(aurora_cr, CAIRO_LINE_CAP_BUTT);
        cairo_set_line_cap(aurora_cr, CAIRO_LINE_CAP_SQUARE);
        cairo_set_line_cap(aurora_cr, CAIRO_LINE_CAP_ROUND);

        // see https://www.cairographics.org/operators/
        cairo_set_operator(aurora_cr, CAIRO_OPERATOR_DIFFERENCE);

        const int I_MAX = 100;
        cairo_set_line_width(aurora_cr, auroraMap->step);
        cairo_pattern_t* vpattern =
            cairo_pattern_create_linear(0, 0, 0, I_MAX);

        cairo_pattern_add_color_stop_rgba(vpattern, 0.0, 1, 0, 1, 0.05);
        cairo_pattern_add_color_stop_rgba(vpattern, 0.2, 1, 0, 1, 0.15);
        cairo_pattern_add_color_stop_rgba(vpattern, 0.3, 0, 1, .8, 0.2);
        cairo_pattern_add_color_stop_rgba(vpattern, 0.7, 0, 1, .8, 0.6);
        cairo_pattern_add_color_stop_rgba(vpattern, 0.8, 0.2, 1, 0, 0.8);
        cairo_pattern_add_color_stop_rgba(vpattern, 1.0, 0.1, 1, 0, 0.0);

        cairo_surface_t* vertsurf = cairo_image_surface_create(
            CAIRO_FORMAT_ARGB32, auroraMap->step, I_MAX);
        cairo_t* vertcr = cairo_create(vertsurf);

        cairo_set_antialias(vertcr, CAIRO_ANTIALIAS_NONE);
        cairo_set_source(vertcr, vpattern);
        cairo_rectangle(vertcr, 0, 0, auroraMap->step, I_MAX);
        cairo_fill(vertcr);

        int zmin = auroraMap->step * auroraMap->z[0].x;
        int zmax = auroraMap->step * auroraMap->z[0].x;

        for (int j = 0; j < auroraMap->nz; j++) {
            double alpha = 1.0;
            double d = 100;
            double scale = cscale(I_MAX, d,
                auroraMap->hmax, auroraMap->zh[j], Flags.AuroraHeight);
            cairo_surface_set_device_scale(vertsurf, 1, scale);

            // draw aurora
            cairo_set_source_surface(aurora_cr, vertsurf,
                auroraMap->step * auroraMap->z[j].x,
                auroraMap->z[j].y - I_MAX / scale);
            cairo_paint_with_alpha(aurora_cr, alpha * auroraMap->za[j]);

            // draw one aurora pillar
            if (auroraMap->step * auroraMap->z[j].x < zmin) {
                zmin = auroraMap->z[j].x;
            }
            if (auroraMap->step * auroraMap->z[j].x > zmax) {
                zmax = auroraMap->z[j].x;
            }
        }

        // draw fuzz
        for (int j = 0; j < auroraMap->nfuzz; j++) {
            double alpha = 1.0;
            double d = 100;
            double scale = cscale(
                I_MAX, d, auroraMap->hmax, auroraMap->fuzz[j].h, Flags.AuroraHeight);
            cairo_surface_set_device_scale(vertsurf, 1, scale);
            if (1) {
                cairo_set_source_surface(aurora_cr, vertsurf,
                    auroraMap->step * auroraMap->fuzz[j].x,
                    auroraMap->fuzz[j].y - I_MAX / scale);
                cairo_paint_with_alpha(aurora_cr, alpha * auroraMap->fuzz[j].a);
                cairo_set_source_surface(aurora_cr, vertsurf,
                    auroraMap->step * auroraMap->fuzz[j].x,
                    auroraMap->fuzz[j].y - I_MAX / scale);
                cairo_paint_with_alpha(aurora_cr, alpha * auroraMap->fuzz[j].a);
            }
        }

        cairo_destroy(vertcr);
        cairo_surface_destroy(vertsurf);
        cairo_pattern_destroy(vpattern);
        cairo_restore(aurora_cr);

        lock_copy();
            cairo_surface_destroy(aurora_surface);
            cairo_destroy(aurora_cr);
            aurora_surface = aurora_surface1;
        unlock_copy();

        aurora_surface1 = cairo_image_surface_create(
            CAIRO_FORMAT_ARGB32, auroraMap->width, auroraMap->base);
        aurora_cr = cairo_create(aurora_surface1);

        unlock_comp();
        unlock_init();

        usleep((useconds_t) (1.0e6 * time_aurora /
            (0.2 * Flags.AuroraSpeed)));
    }

    return NULL;
}

/** *********************************************************************
 ** This method ...
 **/
void aurora_setparms(AuroraMap* auroraMap) {
    int f = turnfuzz * mGlobal.SnowWinWidth + 2;
    auroraMap->w = auroraMap->width - 2 * f; // Flags.AuroraWidth*0.01*mGlobal.SnowWinWidth;

    auroraMap->xoffset = f;
    if (Flags.AuroraLeft) {
        auroraMap->x = -f / 2; // auroraMap->x = f;
    } else if (Flags.AuroraMiddle) {
        auroraMap->x = (mGlobal.SnowWinWidth - auroraMap->width) / 2; // + f;
    } else {                                         // AuroraRight assumed
        auroraMap->x = mGlobal.SnowWinWidth - auroraMap->width;       // - f;
    }

    auroraMap->y = 0;

    for (int i = 0; i < AURORA_POINTS; i++) {
        auroraMap->points[i] = 0.2 + 0.4 * drand48();
        auroraMap->dpoints[i] = (2 * (i % 2) - 1) * 0.0005;
    }

    auroraMap->slantmax = 3;
    for (int i = 0; i < AURORA_S; i++) {
        auroraMap->slant[i] = auroraMap->slantmax * (2 * drand48() - 1);
        auroraMap->dslant[i] = (2 * (i % 2) - 1) * 0.02;
    }

    auroraMap->fuzzleft = 0.1;
    auroraMap->fuzzright = 0.1;
    auroraMap->alpha = drand48() * alphamax;
    auroraMap->dalpha = 0.011;
    auroraMap->theta = drand48() * 360;
    auroraMap->dtheta = 0.2;

    auroraMap->hmax = 0.6 * auroraMap->base;
    if (auroraMap->hmax < 3) {
        auroraMap->hmax = 3;
    }
    for (int i = 0; i < AURORA_H; i++) {
        auroraMap->h[i] = 0.8 * drand48() + 0.2;
        auroraMap->dh[i] = (2 * (i % 2) - 1) * 0.01;
    }
    for (int i = 0; i < AURORA_A; i++) {
        auroraMap->a[i] = 0.5 * drand48() + 0.5;
        auroraMap->da[i] = (2 * (i % 2) - 1) * 0.01;
    }
    for (int i = 0; i < AURORA_AA; i++) {
        auroraMap->aa[i] = drand48();
        auroraMap->daa[i] = (2 * (i % 2) - 1) * 0.01;
    }

    auroraMap->step = 1;
    aurora_computeparms(auroraMap);
}

/** *********************************************************************
 ** This method ...
 **/
void aurora_changeparms(AuroraMap* auroraMap) {
    auroraMap->alpha += auroraMap->dalpha;
    if (auroraMap->alpha > alphamax) {
        auroraMap->dalpha = -fabs(auroraMap->dalpha);
    } else if (auroraMap->alpha < 0.4) {
        auroraMap->dalpha = fabs(auroraMap->dalpha);
    }

    // global shape of aurora
    for (int i = 0; i < AURORA_POINTS; i++) {
        auroraMap->points[i] += auroraMap->dpoints[i] * erand48(xsubi);
        if (auroraMap->points[i] > 1) {
            auroraMap->points[i] = 1;
            auroraMap->dpoints[i] = -fabs(auroraMap->dpoints[i]);
        } else if (auroraMap->points[i] < 0) {
            auroraMap->points[i] = 0;
            auroraMap->dpoints[i] = fabs(auroraMap->dpoints[i]);
        }
    }

    // rotation angle
    double dt = 0.2;
    auroraMap->theta += dt * auroraMap->dtheta * (erand48(xsubi) + 0.5);
    if (auroraMap->theta < 175) {
        auroraMap->theta = 175;
        auroraMap->dtheta = fabs(auroraMap->dtheta);
    } else if (auroraMap->theta > 185) {
        auroraMap->theta = 185;
        auroraMap->dtheta = -fabs(auroraMap->dtheta);
    }

    // slant
    for (int i = 0; i < AURORA_S; i++) {
        auroraMap->slant[i] += auroraMap->dslant[i] * erand48(xsubi);
        if (auroraMap->slant[i] > auroraMap->slantmax) {
            auroraMap->slant[i] = auroraMap->slantmax;
            auroraMap->dslant[i] = -fabs(auroraMap->dslant[i]);
        } else if (auroraMap->slant[i] < -auroraMap->slantmax) {
            auroraMap->slant[i] = -auroraMap->slantmax;
            auroraMap->dslant[i] = fabs(auroraMap->dslant[i]);
        }
    }

    // height of aurora
    for (int i = 0; i < AURORA_H; i++) {
        auroraMap->h[i] += auroraMap->dh[i] * erand48(xsubi);
        if (auroraMap->h[i] > 1) {
            auroraMap->h[i] = 1;
            auroraMap->dh[i] = -fabs(auroraMap->dh[i]);
        } else if (auroraMap->h[i] < 0.2) {
            auroraMap->h[i] = 0.2;
            auroraMap->dh[i] = fabs(auroraMap->dh[i]);
        }
    }

    // transparency of aurora
    for (int i = 0; i < AURORA_A; i++) {
        auroraMap->a[i] += auroraMap->da[i] * erand48(xsubi);
        if (auroraMap->a[i] > 1.2) {
            auroraMap->a[i] = 1.2;
            auroraMap->da[i] = -fabs(auroraMap->da[i]);
        } else if (auroraMap->a[i] < 0.5) {
            auroraMap->a[i] = 0.5;
            auroraMap->da[i] = fabs(auroraMap->da[i]);
        }
    }

    // high frequency transparency of aurora
    for (int i = 0; i < AURORA_AA; i++) {
        auroraMap->aa[i] += auroraMap->daa[i] * erand48(xsubi);
        if (auroraMap->aa[i] > 1.2) {
            auroraMap->aa[i] = 1.2;
            auroraMap->daa[i] = -fabs(auroraMap->daa[i]);
        } else if (auroraMap->aa[i] < 0.1) {
            auroraMap->aa[i] = 0.1;
            auroraMap->daa[i] = fabs(auroraMap->daa[i]);
        }
    }

    aurora_computeparms(auroraMap);
}

/** *********************************************************************
 ** This method ...
 **/
void aurora_computeparms(AuroraMap* auroraMap) {
    if (auroraMap->z) {
        free(auroraMap->z);
        auroraMap->z = NULL;
    }

    int np = auroraMap->w / auroraMap->step;
    create_aurora_base(auroraMap->points, AURORA_POINTS,
        auroraMap->slant, AURORA_S, auroraMap->theta * M_PI / 180.0,
        0, np, &(auroraMap->z), &(auroraMap->nz));

    // normalize z.y values and z.x
    double ymin = auroraMap->z[0].y;
    double ymax = auroraMap->z[0].y;
    for (int i = 0; i < auroraMap->nz; i++) {
        if (auroraMap->z[i].y < ymin) {
            ymin = auroraMap->z[i].y;
        }
        if (auroraMap->z[i].y > ymax) {
            ymax = auroraMap->z[i].y;
        }
        auroraMap->z[i].x += auroraMap->xoffset;
    }

    // prevent too large jumps in position of aurora
    static double ymin_old = 1;
    static double ymax_old = 1;
    static const double dmax = 0.01;

    if (fabs(ymin - ymin_old) < 0.3) {
        if ((ymin - ymin_old) > dmax) {
            ymin = ymin_old + dmax;
        } else if ((ymin - ymin_old) < -dmax) {
            ymin = ymin_old - dmax;
        }
    }
    if (fabs(ymax - ymax_old) < 0.3) {
        if ((ymax - ymax_old) > dmax) {
            ymax = ymax_old + dmax;
        } else if ((ymax - ymax_old) < -dmax) {
            ymax = ymax_old - dmax;
        }
    }
    ymin_old = ymin;
    ymax_old = ymax;

    double d = ymax - ymin;
    if (d < 0.1) {
        d = 0.1;
    }
    double s = (auroraMap->base - auroraMap->hmax) / d;
    for (int i = 0; i < auroraMap->nz; i++) {
        auroraMap->z[i].y = auroraMap->base - (auroraMap->z[i].y - ymin) * s;
    }

    // height
    double dx;
    double px[AURORA_H];

    for (int i = 0; i < AURORA_H; i++) {
        px[i] = i;
    }

    if (auroraMap->zh) {
        free(auroraMap->zh);
        auroraMap->zh = NULL;
    }

    double *x = (double *)malloc(auroraMap->nz * sizeof(double));
    auroraMap->zh = (double *)malloc(auroraMap->nz * sizeof(double));
    dx = (double)(AURORA_H - 1) / (double)(auroraMap->nz - 1);

    for (int i = 0; i < auroraMap->nz; i++) {
        x[i] = i * dx;
    }
    x[auroraMap->nz - 1] = px[AURORA_H - 1];
    spline_interpol(px, AURORA_H, auroraMap->h, x, auroraMap->nz, auroraMap->zh);

    // alpha values
    double pa[AURORA_A];
    for (int i = 0; i < AURORA_A; i++) {
        pa[i] = i;
    }
    if (auroraMap->za) {
        free(auroraMap->za);
        auroraMap->za = NULL;
    }
    auroraMap->za = (double *)malloc(auroraMap->nz * sizeof(double));
    dx = (double)(AURORA_A - 1) / (double)(auroraMap->nz - 1);
    for (int i = 0; i < auroraMap->nz; i++) {
        x[i] = i * dx;
    }
    x[auroraMap->nz - 1] = pa[AURORA_A - 1];
    spline_interpol(pa, AURORA_A, auroraMap->a, x, auroraMap->nz, auroraMap->za);

    // high frequency alpha
    double paa[AURORA_AA];
    for (int i = 0; i < AURORA_AA; i++) {
        paa[i] = i;
    }
    if (auroraMap->zaa) {
        free(auroraMap->zaa);
        auroraMap->zaa = NULL;
    }
    auroraMap->zaa = (double *)malloc(auroraMap->nz * sizeof(double));
    dx = (double)(AURORA_AA - 1) / (double)(auroraMap->nz - 1);
    for (int i = 0; i < auroraMap->nz; i++) {
        x[i] = i * dx;
    }
    x[auroraMap->nz - 1] = paa[AURORA_AA - 1];
    spline_interpol(paa, AURORA_AA, auroraMap->aa, x, auroraMap->nz, auroraMap->zaa);

    free(x);

    for (int i = 0; i < auroraMap->nz; i++) {
        double alpha = 1;
        if (i < auroraMap->fuzzleft * auroraMap->nz) {
            alpha = ((float)i) / (auroraMap->fuzzleft * auroraMap->nz);
        }
        if (i > auroraMap->nz * (1 - auroraMap->fuzzright)) {
            alpha = ((float)(auroraMap->nz - i)) / (auroraMap->fuzzright * auroraMap->nz);
        }
        alpha += 0.05 * auroraMap->zaa[i];
        auroraMap->za[i] *= alpha;
    }

    if (1) {
        // add fuzz on turning points, second method
        // add some points with diminishing alpha
        auroraMap->nfuzz = 0;
        int f = turnfuzz * mGlobal.SnowWinWidth;
        int d0 = auroraMap->z[1].x - auroraMap->z[0].x;

        int j;
        int i = 0;
        while (1) {
            i++;
            if (i >= auroraMap->nz - 1) {
                break;
            }

            int d = auroraMap->z[i + 1].x - auroraMap->z[i].x;

            if (d0 != d) {
                if (auroraMap->lfuzz <= auroraMap->nfuzz + f) {
                    auroraMap->lfuzz += 4 * f;
                    auroraMap->fuzz =
                        (fuzz_t *)realloc(auroraMap->fuzz, auroraMap->lfuzz * sizeof(fuzz_t));
                }

                if (d0 > 0) // edge on the right side
                {
                    int jmax = i + f;
                    if (jmax > auroraMap->nz - 1) {
                        jmax = auroraMap->nz - 1;
                    }
                    int jj = jmax;

                    for (j = i; j < jj; j++) {
                        if (auroraMap->z[j + 1].x - auroraMap->z[j].x != d) {
                            break;
                        }
                    }

                    if (jmax > j) {
                        jmax = j;
                    }

                    jj = i - jmax;
                    if (jj < 1) {
                        jj = 1;
                    }
                    for (int j = i - 1; j > jj; j--) {
                        if (auroraMap->z[j - 1].x - auroraMap->z[j].x != d) {
                            break;
                        }
                    }
                    if (jmax + j > 2 * i) {
                        jmax = 2 * i - j;
                    }

                    for (int j = i; j < jmax; j++) {
                        auroraMap->fuzz[auroraMap->nfuzz].x = auroraMap->z[i].x + j - i;
                        auroraMap->fuzz[auroraMap->nfuzz].y = auroraMap->z[i].y;
                        double alpha =
                            auroraMap->za[i] * (double)(jmax - j) / (double)(jmax - i);
                        if (j == i) {
                            alpha *= 0.5;
                        }
                        auroraMap->fuzz[auroraMap->nfuzz].a = alpha;
                        auroraMap->fuzz[auroraMap->nfuzz].h = auroraMap->zh[i];
                        (auroraMap->nfuzz)++;
                    }
                } else // edge on the left side
                {
                    int jmin = i - f;
                    if (jmin < 1) {
                        jmin = 1;
                    }
                    int jj = i + f;
                    if (jj > auroraMap->nz - 1) {
                        jj = auroraMap->nz - 1;
                    }
                    int j;
                    for (int j = i + 1; j < jj; j++) {
                        if (auroraMap->z[j].x - auroraMap->z[j - 1].x != d) {
                            break;
                        }
                    }
                    if (j + jmin < 2 * i) {
                        jmin = 2 * i - j;
                    }

                    jj = i - jmin;
                    if (jj < 1) {
                        jj = 1;
                    }
                    for (int j = i; j > jj; j--) {
                        if (auroraMap->z[j - 1].x - auroraMap->z[j].x != d) {
                            break;
                        }
                    }
                    if (j > jmin) {
                        jmin = j;
                    }

                    for (int j = i; j > jmin; j--) {
                        auroraMap->fuzz[auroraMap->nfuzz].x = auroraMap->z[i].x - i + j;
                        auroraMap->fuzz[auroraMap->nfuzz].y = auroraMap->z[i].y;
                        double alpha =
                            auroraMap->za[i] * (double)(j - jmin) / (double)(i - jmin);
                        if (j == i) {
                            alpha *= 0.5;
                        }
                        auroraMap->fuzz[auroraMap->nfuzz].a = alpha;
                        auroraMap->fuzz[auroraMap->nfuzz].h = auroraMap->zh[i];
                        (auroraMap->nfuzz)++;
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
// for (int i=0; i<nz; i++)
// {
//    int x    = z[i].x;
//    double y = z[i].y;
// }
//   Note that the same x value can occur more than once

void create_aurora_base(const double *y, int n, double *slant, int nslant,
    double theta, int nw, int np, aurora_t **z, int *nz) {
    struct pq {
            double x;
            double y;
    } *p;

    double *x = (double *)malloc(n * sizeof(double));
    double *s = (double *)malloc(nslant * sizeof(double));

    for (int i = 0; i < n; i++) {
        x[i] = i;
    }
    for (int i = 0; i < nslant; i++) {
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

    for (int i = 0; i < nw; i++) {
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

    for (int i = 0; i < nw; i++) {
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
    for (int i = 1; i < nw; i++) {
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
            for (int j = m1 + 1; j <= m; j++) {
                pz[k].y = pp->y;
                pz[k].x = j;
                k++;
            }
            m1 = m;
        } else if (m < m1) {
            for (int j = m1 - 1; j >= m; j--) {
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

/** *********************************************************************
 ** This method ...
 **/
double cscale(double d, int scaleMax, float ah, double az, double h) {
    double scale = scaleMax / d;
    double s = 1.8 - 0.016 * h;

    scale = s * scaleMax / ah / az;

    if (scale < 0.125) {
        scale = 0.125;
    }
    if (scale > 4) {
        scale = 4;
    }

    return scale;
}

/** *********************************************************************
 ** This method ...
 **/
void aurora_sem_init() {
    sem_init(&mAuroraSemaphore, 0, 1);
    sem_init(&mInitializeSemaphore, 0, 1);
    sem_init(&mCopySemaphore, 0, 1);
}

/** *********************************************************************
 ** Helper methods for module locks.
 **/
int lock_comp() {
    return sem_wait(&mAuroraSemaphore);
}
int unlock_comp() {
    return sem_post(&mAuroraSemaphore);
}

int lock_init() {
    return sem_wait(&mInitializeSemaphore);
}
int unlock_init() {
    return sem_post(&mInitializeSemaphore);
}

int lock_copy() {
    return sem_wait(&mCopySemaphore);
}
int unlock_copy() {
    return sem_post(&mCopySemaphore);
}

