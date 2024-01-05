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

#include "xdo.h"
#include <X11/Intrinsic.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <gtk/gtk.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_XDBEALLOCATEBACKBUFFERNAME
#define XDBE_AVAILABLE
#endif

#ifdef NO_USE_BITS
#define BITS(n)
#else
#define BITS(n) :n
#endif

#define FLAGSFILE ".plasmasnowrc"
#define FLAKES_PER_SEC_PER_PIXEL 30
#define INITIALSCRPAINTSNOWDEPTH 8 /* Painted in advance */
#define INITIALYSPEED 120          // has to do with vertical flake speed
#define MAXBLOWOFFFACTOR 100
#define MAXSANTA 4         // santa types 0..4
#define MAXTANNENPLACES 10 // number of trees
#define MAXTREETYPE 10    // treetypes: 0..MAXTREETYPE
#define MAXWSENS 0.4       // sensibility of flakes for wind
#define MAXXSTEP 2         /* drift speed max */
#define MAXYSTEP 10        /* falling speed max */
#define PIXINANIMATION 4   // nr of santa animations
#define SANTASENS 0.2      // sensibility of Santa for wind
#define SANTASPEED0 12
#define SANTASPEED1 25
#define SANTASPEED2 50
#define SANTASPEED3 50
#define SANTASPEED4 70
// #define SNOWFLAKEMAXTYPE 13  // type is 0..SNOWFLAKEMAXTYPE
#define SNOWFREE 25   /* Must stay snowfree on display :) */
#define SNOWSPEED 0.7 // the higher, the speedier the snow
#define WHIRL 150
#define MAXVISWORKSPACES 100 // should be enough...

// timers

#define time_aurora 1.0 // time between update of aurora
#define CONFIRM_BELOW_ALL_WINDOWS_EVENT_TIME                                   \
    1.0 // Time user is allowed to confirm app goes below all windows.
#define time_blowoff 0.50           // time between blow snow off windows
#define time_change_attr 60.0       // time between changing attraction point
#define time_clean 1.00             // time between cleaning desktop
#define time_desktop_type 2.0       // time between showing desktop type
#define time_display_dimensions 0.5 // time between check of screen dimensions
#define time_displaychanged 1.00 // time between checks if display has changed
#define time_emeteor 0.40        // time between meteors erasures
#define CONFIGURE_WINDOW_EVENT_TIME                                            \
    0.50                     // Time between handling window configure events.
#define time_flakecount 1.00 // time between updates of show flakecount
#define time_fuse 1.00       // time between testing on too much flakes
#define time_genflakes 0.10  // time between generation of flakes
#define time_init_snow                                                         \
    0.2 // time between killing flakes (used in emergency only)
#define time_initbaum 0.30       // time between check for (re)create trees
#define time_initstars 1.00      // time between check for (re)create stars
#define time_main_window 0.5     // time between checks for birds window
#define time_measure 0.1         // time between cpu load measurements
#define time_meteor 3.00         // time between meteors
#define time_newwind 1.00        // time between changing wind
#define time_sendevent 0.5       // time between sendEvent() calls
#define time_sfallen 2.30        // time between smoothing of fallen snow
#define time_show_range_etc 0.50 // time between showing range etc.
#define time_snow_on_trees 0.50  // time between redrawings of snow on trees
#define time_star 0.50           // time between drawing stars
#define time_switchflakes                                                      \
    0.2 // time between checks if flakes should be switched beteen default and
        // vintage
#define time_testing 2.10  // time between testing code
#define time_ui_check 0.25 // time between checking values from ui
#define time_umoon 0.04    // time between update position of moon
#define time_usanta 0.04   // time between update of santa position
#define time_ustar 2.00    // time between updating stars
#define time_wind 0.10     // time between starting or ending wind
#define time_wupdate 0.20  // time between getting windows information

#define time_change_bottom 300.0 // time between changing desired heights
#define time_adjust_bottom                                                     \
    (time_change_bottom / 20) // time between adjusting height of bottom snow
#define time_fallen 0.20      // time between recompute fallen snow surfaces
#define time_snowflakes                                                        \
    (0.02 *                                                                    \
        global.cpufactor) // time between updates of snowflakes positions etc
#define time_draw_all                                                          \
    (0.04 * global.cpufactor) // time between updates of screen

#define ALPHA (0.01 * (100 - Flags.Transparency))
#define XPM_TYPE const char
/* ------------------------------------------------------------------ */

typedef struct _WinInfo {
        Window id;
        int x, y;          // x,y coordinates
        int xa, ya;        // x,y coordinates absolute
        unsigned int w, h; // width, height
        long ws;           // workspace

        unsigned int sticky BITS(1); // is visible on all workspaces
        unsigned int dock BITS(1);   // is a "dock" (panel)
        unsigned int hidden BITS(1); // is hidden (iconified)
} WinInfo;


typedef struct _FallenSnow {
        WinInfo win; // WinInfo of window, win.id == 0 if snow at bottom
        int x, y;    // Coordinates of fallen snow, y for bottom of fallen snow
        int w, h;    // width, max height of fallen snow
        int prevx, prevy;          // x,y of last draw
        int prevw, prevh;          // w,h of last draw
        short int *acth;           // actual heights
        short int *desh;           // desired heights
        struct _FallenSnow *next;  // pointer to next item
        cairo_surface_t *surface;  //
        cairo_surface_t *surface1; //
} FallenSnow;


typedef struct _MeteorMap {
        int x1, x2, y1, y2, active, colornum;
} MeteorMap;

typedef struct _StarMap {
        unsigned char *starBits;
        Pixmap pixmap;
        int width;
        int height;
} StarMap;

typedef struct _Skoordinaten {
        int x;
        int y;
        int color;
} Skoordinaten;

typedef struct Treeinfo {
        int x; // x position
        int y; // y position
        int w; // width
        int h; // height
        cairo_surface_t *surface;
        float scale;
        unsigned int type BITS(8); // type (TreeType, -treetype)
        unsigned int rev BITS(1);  // reversed
} Treeinfo;

typedef struct _Snow {
        float rx; // x position
        float ry; // y position
        int ix;
        int iy;                       // position after draw
        float vx;                     // speed in x-direction, pixels/second
        float vy;                     // speed in y-direction, pixels/second
        float m;                      // mass of flake
        float ivy;                    // initial speed in y direction
        float wsens;                  // wind dependency factor
        float flufftimer;             // fluff timeout timer
        float flufftime;              // fluff timeout
        unsigned int whatFlake;       // snowflake index
        unsigned int cyclic BITS(1);  // flake is cyclic
        unsigned int fluff BITS(1);   // flake is in fluff state
        unsigned int freeze BITS(1);  // flake does not move
        unsigned int testing BITS(2); // for testing purposes

} Snow;

typedef struct _SnowMap {
        // Pixmap pixmap;
        cairo_surface_t *surface;
        unsigned int width BITS(16);
        unsigned int height BITS(16);
} SnowMap;

extern struct _global {
        SnowMap *fluffpix;
        int counter;
        unsigned int xxposures BITS(1);
        unsigned int Desktop BITS(1);
        unsigned int Trans BITS(1);
        unsigned int UseDouble BITS(1);
        unsigned int IsDouble BITS(1);

        int XscreensaverMode;

        double cpufactor;

        float ActualSantaSpeed;
        Region SantaPlowRegion;
        int SantaHeight;
        int SantaWidth;
        int SantaX;
        int SantaY;
        int SantaDirection; // 0: left to right, 1: right to left

        float WindowScale;

        unsigned int MaxFlakeHeight; /* Biggest flake */
        unsigned int MaxFlakeWidth;  /* Biggest flake */
        int FlakeCount;                  /* number of flakes */
        int FluffCount;                  /* number of fluff flakes */

        Display *display;
        xdo_t *xdo;
        int Screen;
        Window SnowWin;
        int SnowWinBorderWidth;
        int SnowWinWidth;
        int SnowWinHeight;
        int WindowOffsetX; // when using the root window for drawing, we need
        //                                   these offsets to correct for the
        //                                   position of the windows.
        int WindowOffsetY;
        int SnowWinDepth;
        char *DesktopSession;
        int IsCompiz;
        int IsWayland;
        long CWorkSpace; // long? Yes, in compiz we take the placement of the
                         // desktop
        //                                     which can easily be > 16 bits
        long
            VisWorkSpaces[MAXVISWORKSPACES]; // these workspaces are visible. In
                                             // bspwm (and possibly other tiling
        //                                 widowmanagers) when app is running
        //                                 full-screen, the different xinerama
        //                                 screens each cover another workspace.
        int NVisWorkSpaces; // number of VisWorkSpaces
        long
            ChosenWorkSpace; // workspace that is chosen as workspace to snow in
        Window Rootwindow;
        int Xroot;
        int Yroot;
        unsigned int Wroot;
        unsigned int Hroot;
        int SnowWinX;
        int SnowWinY;
        int WindowsChanged;
        int ForceRestart;

        FallenSnow *FsnowFirst;
        int MaxScrSnowDepth;
        int RemoveFluff;

        double moonX;
        double moonY;
        double moonR; // radius of moon in pixels

        // Region          TreeRegion;
        cairo_region_t *TreeRegion;

        cairo_region_t *gSnowOnTreesRegion;
        XPoint *SnowOnTrees;
        int OnTrees;

        Pixel Black;
        Pixel White;

        int Wind;
        // Wind = 0: no wind
        // Wind = 1: wind only affecting snow
        // Wind = 2: wind affecting snow and santa
        // Direction =  0: no wind direction I guess
        // Direction =  1: wind from left to right
        // Direction = -1: wind from right to left
        int Direction;
        float Whirl;
        double WhirlTimer;
        double WhirlTimerStart;
        float NewWind;
        float WindMax;

        int HaltedByInterrupt;
        char Message[256];

        char *Language;
} global;

extern int set_sticky(int s);
