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
//
#include <assert.h>
#include <math.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include <gtk/gtk.h>

#include "birdglobals.h"
#include "birds.h"
#include "ClockHelper.h"
#include "ColorPicker.h"
#include "doitb.h"
#include "Flags.h"
#include "hashtable.h"
#include "ixpm.h"
#include "kdtree.h"
#include "MainWindow.h"
#include "pixmaps.h"
#include "Santa.h"
#include "Utils.h"
#include "Windows.h"

#define NWINGS 8
#define NBIRDPIXBUFS (3 * NWINGS)


/* Surface to store current scribbles */

static GdkPixbuf *bird_pixbufs[NBIRDPIXBUFS];
static cairo_surface_t *attrsurface = NULL;

static int
    Nbirds; // is copied from Flags.Nbirds in init_birds. We cannot have that
//                  // Nbirds is changed outside init_birds

typedef struct _Birdtype {
        float x, y, z; // position, meters
        // x: horizontal
        // y: in/out screen
        // z: vertical
        float sx, sy, sz;       // velocity, m/sec
        int ix, iy, iz, iw, ih; // pixels
        // ix .. in pixels, used to store previous screen parameters
        int wingstate, orient;
        int drawable; // is in drawable range
        // comes in handy at erasing a bird:
        int prevx, prevy, prevw, prevh, prevdrawable;
} BirdType;

struct _blobals blobals;

static void createAttractionPointSurface(void);
static void birds_init_color(void);
static void birds_set_attraction_point_relative(float x, float y, float z);
static void birds_set_scale(void);
static void birds_set_speed(void);
static void clear_flags(void);
static void show_attr(void);
static float attr_maxz(float y);

static int randomlyChangeAttractionPoint();
static int do_main_window();
static int do_update_pos_birds();
static void *updateBirdSpeed();
static int do_wings();

static void init_bird_pixbufs(const char *color);
static void main_window(void);
static void normalize_speed(BirdType *bird, float speed);
static void prefxyz(BirdType *bird, float d, float e, float x, float y, float z,
    float *prefx, float *prefy, float *prefz);
static void r2i(BirdType *bird);
static void i2r(BirdType *bird);
static int attrbird_erase(int force);
static void init_birds(int start);
static void init_bird(BirdType *bird);
static int lock(void);
static int unlock(void);

static float time_update_pos_birds = 0.01;
static float time_update_speed_birds = 0.20;
static float time_wings = 0.10;

static struct kdtree *kd = NULL;

static BirdType *birds = NULL;
static BirdType attrbird;

static sem_t sem;


void birds_ui() {

    UIDO(ShowBirds, lock(); birds_erase(1); unlock(); attrbird_erase(1););

    UIDO(Neighbours, );
    UIDO(Anarchy, );
    UIDO(PrefDistance, );
    UIDO(ViewingDistance, createAttractionPointSurface(););
    UIDO(BirdsSpeed, birds_set_speed(););
    UIDO(AttrFactor, );
    UIDO(DisWeight, );
    UIDO(FollowWeight, );
    UIDO(BirdsScale, birds_set_scale(););
    UIDO(ShowAttrPoint, attrbird_erase(1););
    UIDO(AttrSpace, show_attr(););

    UIDOS(BirdsColor,
        birds_init_color();
        clearGlobalSnowWindow();
    );

    if (isColorPickerActive() &&
        isColorPickerConsumer(getBirdsColorTag()) &&
        isColorPickerResultAvailable()) {
        char sbuffer[16];
        snprintf(sbuffer, 16, "#%02x%02x%02x",
            getColorPickerResultRed(),
            getColorPickerResultGreen(),
            getColorPickerResultBlue());
        GdkRGBA color;
        gdk_rgba_parse(&color, sbuffer);
        free(Flags.BirdsColor);
        rgba2color(&color, &Flags.BirdsColor);
        clearColorPicker();
    }

    UIDO(Nbirds,
        int start = OldFlags.Nbirds;
        if (Flags.Nbirds <= 0) Flags.Nbirds = 1;
        if (Flags.Nbirds > NBIRDS_MAX) Flags.Nbirds = NBIRDS_MAX;

         init_birds(start);
    );

    UIDO(FollowSanta, if (!Flags.FollowSanta) randomlyChangeAttractionPoint(NULL););
}

void birds_sem_init() { sem_init(&sem, 0, 1); }
int lock() { return sem_wait(&sem); }
int unlock() { return sem_post(&sem); }


static
void normalize_speed(BirdType *bird, float speed) {
    float v2 = sq3(bird->sx, bird->sy, bird->sz);
    if (fabsf(v2) < 1.0e-10) {
        v2 = blobals.meanspeed;
    }
    float a = speed / sqrtf(v2);
    bird->sx *= a;
    bird->sy *= a;
    bird->sz *= a;
}

static float scale(float y) {
    float s;
    if (y != 0) {
        s = 0.005 * (100 - Flags.ViewingDistance) * blobals.maxy / y;
    } else {
        s = 1.0e6;
    }

    return s;
}

// #define CO_REAL
//  given bird, compute screen coordinates ix and iz, and depth iy
void r2i(BirdType *bird) {
    if (bird->y > Flags.ViewingDistance / 8) {
        bird->drawable = 1;
        float f = scale(bird->y);

#ifdef CO_REAL
        // classical camera obscura, inverted image:
        float x = f * (blobals.xc - bird->x) + blobals.xc;
        float z = f * (blobals.zc - bird->z) + blobals.zc;
#else
        // alternative camera obscura, not inverted image:
        float x = f * (bird->x - blobals.xc) + blobals.xc;
        float z = f * (bird->z - blobals.zc) + blobals.zc;
#endif

        bird->ix = blobals.ax * x;
        bird->iy = blobals.ay * bird->y;
        bird->iz = blobals.az * z;
    } else {
        bird->drawable = 0;
    }
}

// compute bird(x,iy, z) given bird(ix, y, iz)
static void i2r(BirdType *bird) {
    float f = scale(bird->y);
#ifdef CO_REAL
    bird->x =
        (blobals.ax * blobals.xc - bird->ix) / (blobals.ax * f) + blobals.xc;
    bird->z =
        (blobals.az * blobals.zc - bird->iz) / (blobals.az * f) + blobals.zc;
#else
    bird->x =
        (bird->ix - blobals.ax * blobals.xc) / (blobals.ax * f) + blobals.xc;
    bird->z =
        (bird->iz - blobals.az * blobals.zc) / (blobals.az * f) + blobals.zc;
#endif
    bird->iy = blobals.ay * bird->y;
}

// given:
// bird
// distance d
// optimal distance e
// coordinates of other bird x,y,z
// compute optimal coordinates for bird: prefx, prefy, prefz
void prefxyz(BirdType *bird, float d, float e, float x, float y, float z,
    float *prefx, float *prefy, float *prefz) {
    *prefx = e * (bird->x - x) / d + x;
    *prefy = e * (bird->y - y) / d + y;
    *prefz = e * (bird->z - z) / d + z;
}

// create a attraction point surface in attrsurface
// is called when user changes drawing scale
// and when attraction point is changed
void createAttractionPointSurface() {
    if (attrsurface) {
        cairo_surface_destroy(attrsurface);
    }
    r2i(&attrbird);
    float f = scale(attrbird.y) * 4.0e-6 * blobals.bird_scale *
              Flags.BirdsScale * blobals.maxix;

    attrsurface =
        cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 2 * f + 2, 2 * f + 2);
    cairo_t *cr = cairo_create(attrsurface);
    cairo_set_source_rgba(cr, 0.914, 0.592, 0.04, 0.6);
    cairo_arc(cr, f + 1, f + 1, f, 0, 2 * M_PI);
    cairo_fill(cr);
    cairo_destroy(cr);
}

void birds_set_scale() { createAttractionPointSurface(); }

void *updateBirdSpeed() {
    while (1) {
        if (!(Flags.shutdownRequested || (!Flags.ShowBirds || blobals.freeze || !isWorkspaceActive()))) {

            lock();
            kd_free(kd);
            kd = kd_create(3);

            int i;

            for (i = 0; i < Nbirds; i++) {
                BirdType *bird = &birds[i];
                kd_insert3f(kd, bird->x, bird->y, bird->z, bird);
            }

            int sumnum = 0;
            float summeandist = 0;
            for (i = 0; i < Nbirds; i++) {
                if (drand48() < Flags.Anarchy * 0.01) {
                    continue;
                }
                BirdType *bird = &birds[i];

                struct kdres *result = kd_nearest_range3f(
                    kd, bird->x, bird->y, bird->z, blobals.range);
                float sumsx = 0;
                float sumsy = 0;
                float sumsz = 0;
                float sumprefx = 0;
                float sumprefy = 0;
                float sumprefz = 0;
                float sumdist = 0;
                int num = 0;
                while (!kd_res_end(result)) {
                    float x, y, z;
                    BirdType *b = (BirdType *)kd_res_item3f(result, &x, &y, &z);
                    kd_res_next(result);
                    if (bird == b) {
                        continue;
                    }
                    num++;

                    // sum the speeds of neighbour birds:

                    sumsx += b->sx;
                    sumsy += b->sy;
                    sumsz += b->sz;

                    float dist = sqrtf((bird->x - x) * (bird->x - x) +
                                       (bird->y - y) * (bird->y - y) +
                                       (bird->z - z) * (bird->z - z));

                    float prefx = 0, prefy = 0, prefz = 0;

                    if (dist > 1e-6) {
                        prefxyz(bird, dist, Flags.PrefDistance, x, y, z, &prefx,
                            &prefy, &prefz);
                    }
                    sumprefx += prefx;
                    sumprefy += prefy;
                    sumprefz += prefz;
                    sumdist += dist;
                }
                kd_res_free(result);

                // meanprefx,y,z: mean optimal coordinates with respect to other
                // birds
                float meanprefx, meanprefy, meanprefz, meandist;

                if (num > 0) {
                    meanprefx = sumprefx / num;
                    meanprefy = sumprefy / num;
                    meanprefz = sumprefz / num;
                    meandist = sumdist / num;
                    summeandist += meandist;
                }
                sumnum += num;
                // adjust speed to other birds, p is weight for own speed
                if (num > 0) {
                    int p = (100 - Flags.FollowWeight) * 0.1;
                    bird->sx = (sumsx + p * bird->sx) / (p + 1 + num);
                    bird->sy = (sumsy + p * bird->sy) / (p + 1 + num);
                    bird->sz = (sumsz + p * bird->sz) / (p + 1 + num);
                }
                // adjust speed to obtain desired distance to other birds
                if (num > 0) {
                    float q = Flags.DisWeight * 0.4;
                    bird->sx += q * (meanprefx - bird->x);
                    bird->sy += q * (meanprefy - bird->y);
                    bird->sz += q * (meanprefz - bird->z);
                }

                // attraction of center:

                float dx = attrbird.x - bird->x;
                float dy = attrbird.y - bird->y;
                float dz = attrbird.z - bird->z;

                float f = Flags.AttrFactor * 0.01f * 0.05f;

                bird->sx += f * dx;
                bird->sy += f * dy;
                bird->sz += f * dz;

                // limit vertical speed

                const float phs = 0.8;
                float hs = sqrtf(sq2(bird->sx, bird->sy));
                if (fabs(bird->sz) > phs * hs) {
                    bird->sz = fsignf(bird->sz) * phs * hs;
                }

                // randomize:
                {
                    const float p = 0.4; //  0<=p<=1 the higher the more random
                    bird->sx += bird->sx * p * drand48();
                    bird->sy += bird->sy * p * drand48();
                    bird->sz += bird->sz * p * drand48();
                }

                normalize_speed(
                    bird, blobals.meanspeed * (0.9 + drand48() * 0.2));
            }
            float meannum = (float)sumnum / (float)Nbirds;
            blobals.mean_distance = summeandist / Nbirds;

            if (meannum < Flags.Neighbours) {
                if (blobals.range < 0.1) {
                    blobals.range = 0.1;
                }
                if (meannum < Nbirds - 1) {
                    blobals.range *= 1.1;
                }
                if (blobals.range > blobals.maxrange) {
                    blobals.range /= 1.1;
                }
            } else {
                blobals.range /= 1.1;
            }

            unlock();
        }
        usleep((useconds_t) (time_update_speed_birds * 1.0e6));
    }
    return NULL;
    // (void) d;
}

int do_update_pos_birds() {
    if (Flags.shutdownRequested) {
        return false;
    }
    if (!Flags.ShowBirds || blobals.freeze || !isWorkspaceActive()) {
        return true;
    }

    lock();

    double dt;
    dt = time_update_pos_birds;

    int i;
    for (i = 0; i < Nbirds; i++) {
        BirdType *bird = &birds[i];

        bird->x += dt * bird->sx;
        bird->y += dt * bird->sy;
        bird->z += dt * bird->sz;
    }

    unlock();

    return TRUE;
    // (void) d;
}

int birds_draw(cairo_t *cr) {
    if (!Flags.ShowBirds || blobals.freeze || !isWorkspaceActive()) {
        return true;
    }

    int before;
    int i;

    lock();

    for (before = 0; before < 2; before++) {
        if (before && Flags.FollowSanta) {
            static int prevSantasize = -1;
            Santa_draw(cr);
            attrbird.ix = mGlobal.SantaX + mGlobal.SantaWidth / 2;
            attrbird.iz = mGlobal.SantaY + mGlobal.SantaHeight / 2;
            switch (Flags.SantaSize) {
            case 0:
                attrbird.y = blobals.maxy * 1.5;
                break;
            case 1:
                attrbird.y = blobals.maxy * 1.0;
                break;
            default:
                attrbird.y = blobals.maxy * 0.5;
                break;
            }
            i2r(&attrbird);
            if (prevSantasize != Flags.SantaSize) {
                prevSantasize = Flags.SantaSize;
                createAttractionPointSurface();
            }
        }
        if (before && Flags.ShowAttrPoint) {
            r2i(&attrbird);

            int mx = cairo_image_surface_get_width(attrsurface);
            int mz = cairo_image_surface_get_height(attrsurface);

            if (attrbird.drawable) {
                cairo_set_source_surface(cr, attrsurface, attrbird.ix - mx / 2,
                    attrbird.iz - mz / 2);
                my_cairo_paint_with_alpha(cr, ALPHA);
            }
            attrbird.prevx = attrbird.ix - mx / 2;
            attrbird.prevy = attrbird.iz - mz / 2;
            attrbird.prevw = mx;
            attrbird.prevh = mz;
            // #define TESTBIRDS
#ifdef TESTBIRDS
            {
                // show the three types of birds flying in the centre
                // useful at creating bird xpm's
                static BirdType testbird;
                testbird = birds[0];
                testbird.x = attrbird.x;
                testbird.y = attrbird.y;
                testbird.z = attrbird.z;
                int i;
                int centerbird = 0;
                for (i = 0; i < 3; i++) {
                    GdkPixbuf *bird_pixbuf =
                        bird_pixbufs[testbird.wingstate + i * NWINGS];
                    int iw = 400;
                    int ih = (float)iw * gdk_pixbuf_get_height(bird_pixbuf) /
                             (float)gdk_pixbuf_get_width(bird_pixbuf);
                    GdkPixbuf *pixbuf = 0;
                    const GdkInterpType interpolation = GDK_INTERP_HYPER;
                    if (iw < 1) {
                        iw = 1;
                    }
                    if (ih < 1) {
                        ih = 1;
                    }
                    pixbuf = gdk_pixbuf_scale_simple(
                        bird_pixbuf, iw, ih, interpolation);
                    cairo_surface_t *surface =
                        gdk_cairo_surface_create_from_pixbuf(pixbuf, 0, NULL);
                    r2i(&testbird);
                    cairo_set_source_surface(cr, surface,
                        testbird.ix + (i - centerbird) * (iw + 20),
                        testbird.iz);
                    my_cairo_paint_with_alpha(cr, ALPHA);
                    g_clear_object(&pixbuf);
                    cairo_surface_destroy(surface);
                }
            }
#endif
        }
        for (i = 0; i < Nbirds; i++) {
            BirdType *bird = &birds[i];

            if (before) // before attraction point
            {
                if (bird->y > attrbird.y) {
                    continue;
                }
            } else // behind attraction point
            {
                if (bird->y <= attrbird.y) {
                    continue;
                }
            }
            // draw:

            r2i(bird);

            bird->prevdrawable = bird->drawable;
            if (bird->drawable) {
                float p = scale(bird->y);

                cairo_surface_t *surface;
                int iw, ih, nw;
                nw = bird->wingstate;

                int orient = 0 * NWINGS;

                float sxz = fabsf(bird->sx);
                float sy = fabs(bird->sy);
                if (sxz > 1.73 * sy) {
                    orient = 2 * NWINGS; // 1*NWINGS
                } else if (sy > 1.73 * sxz) {
                    orient = 0 * NWINGS;
                } else {
                    orient = 0 * NWINGS;
                }

                // canonical:
                // if (sxz > 1.73*sy)
                //    orient = 1*NWINGS;
                //                          aside
                //                            ***
                //                         *********
                //                         *********
                //                         *********
                //                            ***
                //
                // else if(sy > 1.73*sxz)
                //    orient = 0*NWINGS
                //                          front
                //                                 **
                //                        ********************
                //                                 **
                // else
                //    orient = 2*NWINGS
                //                          oblique
                //                             ***
                //                        *************
                //                             ***
                //

                GdkPixbuf *bird_pixbuf = bird_pixbufs[nw + orient];
                iw = p * blobals.bird_scale * Flags.BirdsScale * 6.0e-6 *
                     blobals.maxix;

                ih = (float)iw * gdk_pixbuf_get_height(bird_pixbuf) /
                     (float)gdk_pixbuf_get_width(bird_pixbuf);
                // do not draw very large birds (would be bad for cache use)
                // and do not draw vanishing small birds
                if (ih > blobals.maxiz * 0.2 ||
                    ih <= 0) // iw is always larger than ih, we don't have to
                             // check iw
                {
                    continue;
                }

                const GdkInterpType interpolation = GDK_INTERP_HYPER;
                // since we are caching the surfaces, we go for the highest
                // quality

                // logarithmic caching
                const double k =
                    log(1.2); // should be log(1.05) ... log(1.5). The higher,
                              // the less cache will be used
                unsigned int key =
                    ((unsigned int)(log(iw) / k) << 8) + nw + orient;

                if (!table_get(key)) {
                    static int table_counter = 0;
                    static double cache = 0;
                    table_counter++;
                    cache += iw * ih;

                    if (iw < 1) {
                        iw = 1;
                    }
                    if (ih < 1) {
                        ih = 1;
                    }
                    GdkPixbuf *pixbuf = gdk_pixbuf_scale_simple(
                        bird_pixbuf, iw, ih, interpolation);
                    table_insert(key,
                        gdk_cairo_surface_create_from_pixbuf(pixbuf, 0, NULL));
                    g_clear_object(&pixbuf);
                }
                surface = (cairo_surface_t *)table_get(key);

                int mx = cairo_image_surface_get_width(surface);
                int mz = cairo_image_surface_get_height(surface);
                cairo_set_source_surface(
                    cr, surface, bird->ix - mx / 2, bird->iz - mz / 2);
                my_cairo_paint_with_alpha(cr, ALPHA);
                bird->prevx = bird->ix - mx / 2;
                bird->prevy = bird->iz - mz / 2;
                bird->prevw = mx;
                bird->prevh = mz;
            } else {
                static int skipped = 0;
                skipped++;
            }
        } // i-loop
    }     // before-loop

    unlock();

    return TRUE;
}

int birds_erase(int force) {
    // locking by caller
    if (mGlobal.isDoubleBuffered) {
        return TRUE;
    }
    if (!force) {
        if (!Flags.ShowBirds || blobals.freeze || !isWorkspaceActive()) {
            return true;
        }
    }

    int i;
    for (i = 0; i < Nbirds; i++) {
        BirdType *bird = &birds[i];
        if (bird->prevdrawable && bird->prevw != 0 && bird->prevh != 0) {
            clearDisplayArea(mGlobal.display, mGlobal.SnowWin, bird->prevx,
                bird->prevy, bird->prevw, bird->prevh, mGlobal.xxposures);
        }
    }

    attrbird_erase(0);

    return TRUE;
}

int attrbird_erase(int force) {
    if (mGlobal.isDoubleBuffered) {
        return TRUE;
    }
    if (!force) {
        if (!Flags.ShowBirds || blobals.freeze || !isWorkspaceActive()) {
            return true;
        }
    }

    static int px = -10000;
    static int py = -10000;
    static int pw = -10000;

    if (force ||
        (attrbird.prevw && (attrbird.prevx != px || attrbird.prevy != py ||
                               attrbird.prevw != pw))) {
        px = attrbird.prevx;
        py = attrbird.prevy;
        pw = attrbird.prevw;
        clearDisplayArea(mGlobal.display, mGlobal.SnowWin, px, py, pw, attrbird.prevh,
            mGlobal.xxposures);
    }
    return TRUE;
}

void init_birds(int start) {
    lock();

    int i;
    if (!mGlobal.isDoubleBuffered) {
        birds_erase(1);
    }

    // Bbirds+1 to prevent allocating zero bytes:
    birds = (BirdType *)realloc(birds, sizeof(BirdType) * (Flags.Nbirds + 1));
    REALLOC_CHECK(birds);
    if (kd) {
        kd_free(kd);
    }
    kd = kd_create(3);
    Nbirds = Flags.Nbirds;
    for (i = start; i < Nbirds; i++) {
        BirdType *bird = &birds[i];
        bird->x = drand48() * blobals.maxx;
        bird->y = drand48() * blobals.maxy;
        bird->z = drand48() * blobals.maxz;

        double r = drand48();
        if (r > 0.75) {
            bird->x += blobals.maxx;
        } else if (r > 0.50) {
            bird->x -= blobals.maxx;
        } else if (r > 0.25) {
            bird->y += blobals.maxy;
        } else {
            bird->y -= blobals.maxy;
        }

        bird->iw = 1;
        bird->ih = 1;
        r2i(bird);

        bird->sx = (0.5 - drand48());
        bird->sy = (0.5 - drand48());
        bird->sz = (0.5 - drand48());
        normalize_speed(bird, blobals.meanspeed);
        bird->drawable = 1;
        bird->wingstate = drand48() * NWINGS;
        bird->prevdrawable = 0;
        bird->prevw = 0;
        bird->prevh = 0;
        bird->prevx = 0;
        bird->prevy = 0;

        kd_insert3f(kd, bird->x, bird->y, bird->z, NULL);
    }

    unlock();
}

static int do_wings() {
    if (Flags.shutdownRequested) {
        return FALSE;
    }
    if (!Flags.ShowBirds || blobals.freeze || !isWorkspaceActive()) {
        return true;
    }

    lock();

    int i;
    for (i = 0; i < Nbirds; i++) {
        BirdType *bird = &birds[i];
        bird->wingstate++;
        if (bird->wingstate >= NWINGS) {
            bird->wingstate = 0;
        }
    }

    unlock();

    return TRUE;
    // (void) d;
}

void birds_set_attraction_point_relative(float x, float y, float z) {

    attrbird.x = blobals.maxx * x;
    attrbird.y = blobals.maxy * y;
    attrbird.z = blobals.maxz * z;
}

void clear_flags() {
#define DOITB(what, type) blobals.what##_changed = 0;
    DOITALLB();
#include "undefall.inc"
#define DOITB(what) blobals.what##_requested = 0;
    BUTTONALL();
#include "undefall.inc"
}

void birds_set_speed() {
    blobals.meanspeed = Flags.BirdsSpeed * 0.01 * blobals.maxx * 0.05;
}

int do_main_window() {
    // (void) d;

    if (blobals.maxix != (int)mGlobal.SnowWinWidth ||
        blobals.maxiz != (int)mGlobal.SnowWinHeight) {
        main_window();
        randomlyChangeAttractionPoint(NULL);
    }

    return TRUE;
}

static void main_window() {
    blobals.maxix = mGlobal.SnowWinWidth;
    blobals.maxiz = mGlobal.SnowWinHeight;
    blobals.maxiy = (blobals.maxix + blobals.maxiz) / 2;

    blobals.maxz = blobals.maxx * (float)blobals.maxiz / (float)blobals.maxix;
    blobals.maxy = blobals.maxx * (float)blobals.maxiy / (float)blobals.maxix;
    blobals.xc = (blobals.maxx - blobals.ox) / 2;
    blobals.zc = (blobals.maxz - blobals.oz) / 2;

    blobals.ax = blobals.maxix / blobals.maxx;
    blobals.ay = blobals.maxiy / blobals.maxy;
    blobals.az = blobals.maxiz / blobals.maxz;
}

void birds_init_color() {
    int i;
    for (i = 0; i < NBIRDPIXBUFS; i++) {
        g_object_unref(bird_pixbufs[i]);
    }
    init_bird_pixbufs(Flags.BirdsColor);
    table_clear((void (*)(void *))cairo_surface_destroy);
}

static void init_bird_pixbufs(const char *color) {
    int i;
    for (i = 0; i < NBIRDPIXBUFS; i++) {
        char **x;
        int lines;
        xpm_set_color((char **)birds_xpm[i], &x, &lines, color);
        bird_pixbufs[i] = gdk_pixbuf_new_from_xpm_data((const char **)x);
        xpm_destroy(x);
    }
}

void init_bird(BirdType *bird) {
    bird->x = 0;
    bird->y = 0;
    bird->z = 0;
    bird->sx = 0;
    bird->sy = 0;
    bird->sz = 0;
    bird->ix = 0;
    bird->iy = 0;
    bird->iz = 0;
    bird->iw = 0;
    bird->ih = 0;
    bird->wingstate = 0;
    bird->orient = 0;
    bird->drawable = 0;
    bird->prevx = 0;
    bird->prevy = 0;
    bird->prevw = 0;
    bird->prevh = 0;
    bird->prevdrawable = 0;
}

float attr_maxz(float y) {
    // convert screen coordinate iz to fractional birds coordinate z
    // Create a dummy bird:
    BirdType bird;
    init_bird(&bird); // just to keep cppcheck happy
    bird.ix = 100;    // does not matter what
    bird.y = y;       // distance in the centre of maxy
    bird.iz = blobals.maxiz * 0.01 *
              Flags.AttrSpace; // the desired maximum vertical screen value
    i2r(&bird);
    return bird.z;
}

void show_attr() {
    // used during update AttrSpace
    attrbird_erase(1);

    birds_set_attraction_point_relative(
        0.5, 0.5, attr_maxz(0.5 * blobals.maxy) / blobals.maxz);

    r2i(&attrbird);
    createAttractionPointSurface();
}

    /***********************************************************
     ** ... .
     **/

    // move attraction point in the range
    // x: 0.1 .. 0.8
    // y: 0.4 .. 0.6
    // z: dependent on Flags.AttrSpace
    // determine random x, y and z values for the attraction point
    // for x and y: determine screen coordinates and convert
    // these to bird coordinates.
    // Note that birds_set_attraction_point_relative() expects
    // fractional coordinates

int randomlyChangeAttractionPoint() {
    if (Flags.shutdownRequested) {
        return FALSE;
    }
    if (Flags.FollowSanta) {
        return TRUE;
    }

    float y = 0.4 + drand48() * 0.2;

    attrbird.ix = (0.1 + drand48() * 0.8) * blobals.maxix;
    attrbird.y = y * blobals.maxy;
    attrbird.iz = 120; // of no importance

    float z = drand48() * attr_maxz(attrbird.y) / blobals.maxz;

    i2r(&attrbird);

    float x = attrbird.x / blobals.maxx;

    attrbird_erase(1);
    birds_set_attraction_point_relative(x, y, z);
    r2i(&attrbird);

    createAttractionPointSurface();
    return TRUE;
}

void birds_init() {
    init_bird_pixbufs("black"); // just to have pixbufs we can throw away
    birds_init_color();
    static int running = 0;

    if (running) {
        main_window();
        return;
    } else {
        running = 1;

        blobals.neighbours_max = 100;
        blobals.range = 20;
        blobals.freeze = 0;
        blobals.maxx = 1000; // meters
        blobals.bird_scale = 64;

        blobals.prefdweight = 1;

        clear_flags();
        addMethodToMainloop(PRIORITY_HIGH, time_update_pos_birds,
            do_update_pos_birds);
        addMethodToMainloop(PRIORITY_HIGH, time_wings,
            do_wings);
        addMethodToMainloop(PRIORITY_DEFAULT, time_change_attr,
            randomlyChangeAttractionPoint);
        addMethodToMainloop(PRIORITY_DEFAULT, time_main_window,
            do_main_window);

        static pthread_t thread;

        pthread_create(&thread, NULL, updateBirdSpeed, NULL);
        main_window();
    }

    attrbird.x = blobals.maxx / 2;
    attrbird.y = blobals.maxy / 2;
    attrbird.z = blobals.maxz / 2;
    attrbird.prevx = 0;
    attrbird.prevy = 0;
    attrbird.prevw = 10;
    attrbird.prevh = 10;

    blobals.meanspeed = 0;

    blobals.ox = 0;
    blobals.oy = 0;
    blobals.oz = 0;

    blobals.maxrange = blobals.maxx - blobals.ox + blobals.maxy - blobals.oy +
                       blobals.maxz - blobals.oz;

    birds_set_speed();

    init_birds(0);
    randomlyChangeAttractionPoint(NULL);

    createAttractionPointSurface();
}
