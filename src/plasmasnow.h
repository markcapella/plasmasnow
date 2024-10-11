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

// XDO Lib.
#include "xdo.h"

// X11 Libs.
#include <X11/Intrinsic.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

// Gtk Libs.
#include <gtk/gtk.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif


/***********************************************************
 * App consts.
 */
#define FLAGSFILE ".plasmasnowrc"

#ifdef HAVE_XDBEALLOCATEBACKBUFFERNAME
#define XDBE_AVAILABLE
#endif

#ifdef NO_USE_BITS
#define BITS(n)
#else
#define BITS(n) :n
#endif

#define ALPHA (0.01 * (100 - Flags.Transparency))
#define XPM_TYPE const char

/***********************************************************
 * Timer consts.
 */
#define time_aurora 1.0 // time between update of aurora

// Time user is allowed to confirm app goes below all windows.
#define CONFIRM_BELOW_ALL_WINDOWS_EVENT_TIME 1.0
#define time_blowoff 0.50           // time between blow snow off windows
#define time_change_attr 60.0       // time between changing attraction point
#define time_clean 1.00             // time between cleaning desktop
#define time_desktop_type 2.0       // time between showing desktop type
#define time_display_dimensions 0.5 // Time between check of screen dimensions.
#define time_displaychanged 1.00    // time between checks if display has changed
#define time_emeteor 0.40           // time between meteors erasures

// Time between handling window configure events.
#define CONFIGURE_WINDOW_EVENT_TIME 0.1
#define time_flakecount 1.00        // time between updates of show flakecount
#define time_fuse 1.00              // time between testing on too much flakes
#define time_genflakes 0.10         // time between generation of flakes

// time between killing flakes (used in emergency only)
#define time_init_snow  0.2
#define time_initbaum 0.30          // time between check for (re)create trees
#define time_main_window 0.5        // time between checks for birds window
#define TIME_BETWEEN_LOAD_MONITOR_EVENTS  0.1  // time between cpu load measurements
#define time_meteor 3.00            // time between meteors
#define time_newwind 1.00           // time between changing wind
#define time_sendevent 0.5          // time between sendEvent() calls
#define time_sfallen 2.30           // time between smoothing of fallen snow
#define time_show_range_etc 0.50    // time between showing range etc.
#define time_snow_on_trees 0.50     // time between redrawings of snow on trees

// time between checks if flakes should be switched beteen default and
// vintage
#define time_switchflakes 0.2
#define time_testing 2.10  // time between testing code
#define TIME_BETWEEEN_UI_SETTINGS_UPDATES 0.25 // time between checking values from ui
#define time_umoon 0.04    // time between update position of moon
#define time_usanta 0.04   // time between update of santa position
#define time_ustar 2.00    // time between updating stars
#define TIME_BETWEEN_LIGHTS_UPDATES 0.5 // time between updating xmas Lights.
#define time_wind 0.10     // time between starting or ending wind
#define time_wupdate 0.02  // time between getting windows information
#define time_change_bottom 300.0 // time between changing desired heights

// time between adjusting height of bottom snow
#define time_adjust_bottom (time_change_bottom / 20)
// time between recompute fallen snow surfaces
#define TIME_BETWWEEN_FALLENSNOW_THREADS 0.20

// time between updates of snowflakes positions etc
#define time_snowflakes    (0.02 * mGlobal.cpufactor)

// time between updates of screen
#define time_draw_all      (0.04 * mGlobal.cpufactor)


/***********************************************************
 * SnowFlake consts.
 */
#define FLAKES_PER_SEC_PER_PIXEL 30
#define INITIALSCRPAINTSNOWDEPTH  8 // Painted in advance
#define INITIALYSPEED           120 // has to do with vertical flake speed
#define MAXBLOWOFFFACTOR        100

#define MAXXSTEP           2 // drift speed max
#define MAXYSTEP          10 // falling speed max
#define MAXWSENS         0.4 // sensibility of flakes for wind

#define SNOWFREE          25 // Must stay snowfree on display :)
#define SNOWSPEED        0.7 // the higher, the speedier the snow
#define WHIRL            150
#define MAXVISWORKSPACES 100 // should be enough...

typedef struct _Snow {
        float rx; // x position
        float ry; // y position

        GdkRGBA color;

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
} SnowFlake;

typedef struct _SnowMap {
        // Pixmap pixmap;
        cairo_surface_t* surface;
        unsigned int width BITS(16);
        unsigned int height BITS(16);
} SnowMap;


/***********************************************************
 * Xmas consts.
 */
#define MAXSANTA           4
#define PIXINANIMATION     4 // nr of santa animations
#define SANTASENS        0.2 // sensibility of Santa for wind

#define SANTASPEED0       12
#define SANTASPEED1       25
#define SANTASPEED2       50
#define SANTASPEED3       50
#define SANTASPEED4       70

typedef struct {
        int x;
        int y;

        int color;
} LightCoordinate;


/***********************************************************
 * Scenery consts.
 */
#define NUM_SCENE_COLOR_TREES 1
#define NUM_SCENE_GRID_ITEMS  9
#define NUM_BASE_SCENE_TYPES  NUM_SCENE_COLOR_TREES + NUM_SCENE_GRID_ITEMS

#define NUM_EXTRA_SCENE_ITEMS 1
#define NUM_ALL_SCENE_TYPES   NUM_BASE_SCENE_TYPES + NUM_EXTRA_SCENE_ITEMS

typedef struct SceneryInfo {
        int x; // x position
        int y; // y position

        int w; // width
        int h; // height

        cairo_surface_t* surface;
        float scale;
        unsigned int type BITS(8); // type (TreeType, -treetype)
        unsigned int rev BITS(1);  // reversed
} SceneryInfo;


/***********************************************************
 * Sky objects.
 */
typedef struct _MeteorMap {
        int x1, x2;
        int y1, y2;

        int active;
        int colornum;
} MeteorMap;

typedef struct _StarMap {
        unsigned char *starBits;

        Pixmap pixmap;

        int width;
        int height;
} StarMap;

typedef struct _StarCoordinate {
        int x;
        int y;

        int color;
} StarCoordinate;


/***********************************************************
 * App Window helper objects.
 */
typedef struct _WinInfo {
        Window window;
        long ws;           // workspace

        int x, y;          // x,y coordinates
        int xa, ya;        // x,y coordinates absolute
        unsigned int w, h; // width, height

        unsigned int sticky BITS(1); // is visible on all workspaces
        unsigned int dock BITS(1);   // is a "dock" (panel)
        unsigned int hidden BITS(1); // is hidden / iconized
} WinInfo;


/***********************************************************
 * Global Fallensnow helper objects.
 */
typedef struct _FallenSnow {
        WinInfo winInfo;          // winInfo None == bottom.
        struct _FallenSnow* next; // pointer to next item.

        cairo_surface_t* surface;
        cairo_surface_t* surface1;

        short int x, y;           // X, Y array.
        short int w, h;           // W, H array.

        int prevx, prevy;         // x, y of last draw.
        int prevw, prevh;         // w, h of last draw.

        GdkRGBA* columnColor;     // Color array.
        short int* snowHeight;    // actual heights.
        short int* maxSnowHeight; // desired heights.

} FallenSnow;


/***********************************************************
 * Global helper objects.
 */
extern struct _mGlobal {

        char *Language;

        int IsCompiz;
        int IsWayland;

        Bool isDoubleBuffered;
        Bool useDoubleBuffers;

        Bool hasDestopWindow;
        char* DesktopSession;

        Bool hasTransparentWindow;
        char* mPlasmaWindowTitle;


        int WindowOffsetX;
        int WindowOffsetY;
        float WindowScale;

        int WindowsChanged;

        Bool xxposures;
        int XscreensaverMode;
        int ForceRestart;
        double cpufactor;

        // Cairo defs.
        cairo_region_t *TreeRegion;
        cairo_region_t *gSnowOnTreesRegion;

        char Message[256];

        // Display defs.
        Display *display;
        xdo_t *xdo;
        int Screen;

        // Root window defs.
        Window Rootwindow;
        int Xroot;
        int Yroot;
        unsigned int Wroot;
        unsigned int Hroot;

        // Workspace defs.
        long CWorkSpace;
        long VisWorkSpaces[MAXVISWORKSPACES];
        int NVisWorkSpaces;
        long ChosenWorkSpace;

        // Snow defs.
        Window SnowWin;
        int SnowWinX;
        int SnowWinY;
        SnowMap *fluffpix;

        unsigned int MaxFlakeHeight;      /* Biggest flake */
        unsigned int MaxFlakeWidth;       /* Biggest flake */

        int FlakeCount;                  /* number of flakes */
        int FluffCount;                  /* number of fluff flakes */

        int SnowWinBorderWidth;
        int SnowWinWidth;
        int SnowWinHeight;
        int SnowWinDepth;

        XPoint* SnowOnTrees;
        int OnTrees;

        // 0 = None, 1 = blowSnow, 2 = blowSnow & blowSanta. :-)
        int Wind;

        // 0 = no, 1 = LTR, 2 = RTL.
        int Direction;

        float Whirl;
        double WhirlTimer;
        double WhirlTimerStart;
        float NewWind;
        float WindMax;

        // Santa defs.
        float ActualSantaSpeed;
        Region SantaPlowRegion;
        int SantaHeight;
        int SantaWidth;
        int SantaX;
        int SantaY;
        int SantaDirection; // 0: left to right, 1: right to left

        // Sky defs.
        double moonX;
        double moonY;
        double moonR; // radius of moon in pixels

        // Fallensnow defs.
        // Main WinInfo (Windows) list & helpers.
        int mWinInfoListLength;
        WinInfo* mWinInfoList;

        FallenSnow *FsnowFirst;
        int MaxScrSnowDepth;
        int RemoveFluff;
} mGlobal;

