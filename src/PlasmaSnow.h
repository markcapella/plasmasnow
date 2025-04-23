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

#include <stdbool.h>

// X11 Libs.
#include <X11/Intrinsic.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

// Gtk Libs.
#include <gtk/gtk.h>

// App Libs.
#include "FallenSnow.h"
#include "xdo.h"


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
#define TIME_BETWEEEN_UI_SETTINGS_UPDATES  0.25

#define TIME_BETWEEN_STORM_THREAD_UPDATES 0.10         // time between generation of flakes
#define TIME_BETWEEN_BLOWOFF_FRAME_UPDATES 0.1
#define TIME_BETWEEN_LOADMEASURE_UPDATES   0.1
#define TIME_BETWEEN_STORMITEM_THREAD_UPDATES    (0.02 * mGlobal.cpufactor)


#define CONFIRM_BELOW_ALL_WINDOWS_EVENT_TIME 1.0

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

// time between killing flakes (used in emergency only)
#define time_init_snow  0.2
#define time_initbaum 0.30          // time between check for (re)create trees
#define time_main_window 0.5        // time between checks for birds window
#define time_meteor 3.00            // time between meteors
#define time_newwind 1.00           // time between changing wind
#define time_sendevent 0.5          // time between sendEvent() calls
#define time_sfallen 2.30           // time between smoothing of fallen snow
#define time_show_range_etc 0.50    // time between showing range etc.
#define TIME_BETWEEN_SCENERY_BLOWOFF_FRAME_UPDATES 0.50     // time between redrawings of snow on trees

#define time_testing 2.10  // time between testing code
#define time_umoon 0.04    // time between update position of moon
#define time_usanta 0.04   // time between update of santa position
#define time_ustar 2.00    // time between updating stars
#define time_wind 0.10     // time between starting or ending wind
#define time_wupdate 0.02  // time between getting windows information

// time between recompute fallen snow surfaces
#define TIME_BETWWEEN_FALLENSNOW_THREADS 0.01


// time between updates of screen
#define time_draw_all      (0.04 * mGlobal.cpufactor)


/***********************************************************
 * StormItem consts.
 */
#define INITIALSCRPAINTSNOWDEPTH  8 // Painted in advance
#define MAXBLOWOFFFACTOR        100

#define WHIRL            150
#define MAXVISWORKSPACES 100 // should be enough...


/***********************************************************
 * Santa consts.
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
 * Global helper objects.
 */
extern struct _mGlobal {
        bool noSplashScreen;
        char *Language;

        int IsCompiz;
        int IsWayland;

        bool isDoubleBuffered;
        bool useDoubleBuffers;

        bool hasDestopWindow;
        char* DesktopSession;

        bool hasTransparentWindow;
        char* mPlasmaWindowTitle;

        int WindowOffsetX;
        int WindowOffsetY;
        float WindowScale;

        int WindowsChanged;

        bool xxposures;
        int XscreensaverMode;
        int ForceRestart;
        double cpufactor;

        // Cairo defs.
        cairo_region_t *TreeRegion;
        cairo_region_t *gSnowOnTreesRegion;

        // Display defs.
        Display* display;
        xdo_t *xdo;
        int Screen;

        // Root window defs.
        Window Rootwindow;
        int Xroot;
        int Yroot;
        unsigned int Wroot;
        unsigned int Hroot;

        // Workspace defs.
        long currentWS;
        long visualWSList[MAXVISWORKSPACES];
        int visualWSCount;
        long ChosenWorkSpace;

        // Snow defs.
        Window SnowWin;
        int SnowWinX;
        int SnowWinY;

        unsigned int MaxFlakeHeight; /* Biggest flake */
        unsigned int MaxFlakeWidth;  /* Biggest flake */

        int stormItemCount;          /* number of flakes */
        int FluffCount;              /* number of fluff flakes */

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
        int winInfoListLength;
        WinInfo* winInfoList;

        FallenSnow* FsnowFirst;
        int MaxScrSnowDepth;
        int RemoveFluff;
} mGlobal;

