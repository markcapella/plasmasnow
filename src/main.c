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
#ifdef HAVE_CONFIG_H
    #include <config.h>
#endif

// Std C Lib headers.
#include <ctype.h>
#include <fcntl.h>
#include <math.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

// X11 headers.
#include <X11/Intrinsic.h>
#include <X11/extensions/Xdbe.h>
#include <X11/extensions/Xinerama.h>
#include <X11/extensions/Xfixes.h>
#include <X11/cursorfont.h>

// Other library headers.
#include <gtk/gtk.h>
#include <gsl/gsl_version.h>
#include <cairo-xlib.h>

// Plasmasnow headers.
#include "plasmasnow.h"

#include "aurora.h"
#include "birds.h"
#include "blowoff.h"
#include "clocks.h"
#include "debug.h"
#include "docs.h"
#include "dsimple.h"
#include "fallensnow.h"
#include "flags.h"
#include "loadmeasure.h"
#include "mainstub.h"
#include "meteor.h"
#include "moon.h"
#include "mygettext.h"
#include "safe_malloc.h"
#include "Santa.h"
#include "scenery.h"
#include "selfrep.h"
#include "snow.h"
#include "stars.h"
#include "transwindow.h"
#include "treesnow.h"
#include "wmctrl.h"
#include "version.h"
#include "wind.h"
#include "windows.h"
#include "ui.h"
#include "utils.h"
#include "vroot.h"
#include "xdo.h"


/***********************************************************
 * Externally provided to this Module.
 */

// Windows:
void setAppBelowAllWindows();
void setAppAboveAllWindows();

Window getActiveX11Window();
Window getActiveAppWindow();

void onCursorChange(XEvent*);
void onAppWindowChange(Window);

void onWindowCreated(XEvent*);
void onWindowReparent(XEvent*);
void onWindowChanged(XEvent*);

void onWindowMapped(XEvent*);
void onWindowFocused(XEvent*);
void onWindowBlurred(XEvent*);
void onWindowUnmapped(XEvent*);

void onWindowDestroyed(XEvent*);

bool isWindowBeingDragged();

// Snow:
// Flake color helper methods.
void setGlobalFlakeColor(GdkRGBA);
GdkRGBA getRGBFromString(char* colorString);


/***********************************************************
 * Module Method stubs.
 */
static void HandleCpuFactor();
static void RestartDisplay();
static void SigHandler(int);

static int handleX11ErrorEvent(Display*, XErrorEvent*);
static void getX11Window(Window*);
static int drawCairoWindow(void*);
static void handleX11CairoDisplayChange();

static void drawCairoWindowInternal(cairo_t*);
static int drawTransparentWindow(gpointer);
static void addWindowDrawMethodToMainloop();
static gboolean handleTransparentWindowDrawEvents(
    GtkWidget*, cairo_t*, gpointer);
static void rectangle_draw(cairo_t*);

static int StartWindow();

static void SetWindowScale();
static int handlePendingX11Events();
static int onTimerEventDisplayChanged();

static void mybindtestdomain();
extern void setAppAboveOrBelowAllWindows();

static void setmGlobalDesktopSession();
static void DoAllWorkspaces();
extern void setTransparentWindowAbove(GtkWindow* window);
extern int updateWindowsList();

extern void doRaiseWindow(char* argString);
extern void doLowerWindow(char* argString);

static int doAllUISettingsUpdates();
static int do_stopafter();
static int handleDisplayConfigurationChange();

extern void getWinInfoList();

// ColorPicker methods.
void uninitQPickerDialog();


/** *********************************************************************
 ** Module globals and consts.
 **/

// Change to 1 for better analysis.
#define DO_SYNCH_DEBUG 0

struct _mGlobal mGlobal;
static char **Argv;
static int Argc;

Bool mMainWindowNeedsReconfiguration = true;
static Bool mDoRestartDueToDisplayChange = 0;

static char* mSnowWindowTitlebarName = NULL;

static guint mTransparentWindowGUID = 0;
static guint mCairoWindowGUID = 0;

static GtkWidget *mTransparentWindow = NULL;

static bool mX11CairoEnabled = false;
cairo_t *mCairoWindow = NULL;
cairo_surface_t *mCairoSurface = NULL;

int xfixes_event_base_ = -1;

static int mIsSticky = 0;

static int wantx = 0;
static int wanty = 0;

static int mPrevSnowWinWidth = 0;
static int mPrevSnowWinHeight = 0;

static int mX11Error_Count = 0;
const int mX11Error_maxBeforeTermination = 1000;

extern void endApplication();

/** *********************************************************************
 ** main.c: 
 **/
int startApplication(int argc, char *argv[]) {
    fprintf(stdout, "main: startApplication() Starts.\n\n");

    signal(SIGINT, SigHandler);
    signal(SIGTERM, SigHandler);
    signal(SIGHUP, SigHandler);

    // Set up random,
    srand48((int) (fmod(wallcl() * 1.0e6, 1.0e8)));

    // Cleaar space for app Global struct.
    memset(&mGlobal, 0, sizeof(mGlobal));

    XInitThreads();
    initFallenSnowSemaphores();
    aurora_sem_init();
    birds_sem_init();

    // Titlebar string, but not one.
    mGlobal.mPlasmaWindowTitle = "plasmaSnow";

    mGlobal.cpufactor = 1.0;
    mGlobal.WindowScale = 1.0;

    mGlobal.MaxFlakeHeight = 0;
    mGlobal.MaxFlakeWidth = 0;

    mGlobal.FlakeCount = 0; /* # active flakes */
    mGlobal.FluffCount = 0;

    mGlobal.SnowWin = 0;
    mGlobal.WindowOffsetX = 0;
    mGlobal.WindowOffsetY = 0;

    mGlobal.DesktopSession = NULL;
    mGlobal.CWorkSpace = 0;
    mGlobal.ChosenWorkSpace = 0;
    mGlobal.NVisWorkSpaces = 1;
    mGlobal.VisWorkSpaces[0] = 0;
    mGlobal.WindowsChanged = 0;
    mGlobal.ForceRestart = 0;

    mGlobal.FsnowFirst = NULL;
    mGlobal.MaxScrSnowDepth = 0;
    mGlobal.RemoveFluff = 0;

    mGlobal.SnowOnTrees = NULL;
    mGlobal.OnTrees = 0;

    mGlobal.moonX = 1000;
    mGlobal.moonY = 80;

    mGlobal.Wind = 0;
    mGlobal.Direction = 0;
    mGlobal.WindMax = 500.0;
    mGlobal.NewWind = 100.0;

    mGlobal.HaltedByInterrupt = 0;
    mGlobal.Message[0] = 0;

    mGlobal.SantaPlowRegion = 0;

    InitFlags();

    // we search for flags that only produce output to stdout,
    // to enable to run in a non-X environment, in which case
    // gtk_init() would fail.
    for (int i = 0; i < argc; i++) {
        char *arg = argv[i];

        if (!strcmp(arg, "-h") || !strcmp(arg, "-help")) {
            docs_usage(0);
            return 0;
        }

        if (!strcmp(arg, "-H") || !strcmp(arg, "-manpage")) {
            docs_usage(1);
            return 0;
        }

        if (!strcmp(arg, "-v") || !strcmp(arg, "-version")) {
            PrintVersion();
            return 0;

        } else if (!strcmp(arg, "-changelog")) {
            displayPlasmaSnowDocumentation();
            return 0;

        }
#ifdef SELFREP
        else if (!strcmp(arg, "-selfrep")) {
            selfrep();
            return 0;

        }
#endif
    }

    int rc = HandleFlags(argc, argv);

    handle_language(0);
    mybindtestdomain();

    switch (rc) {
        case -1: // wrong flag
            uninitQPickerDialog();
            endApplication();
            return 1;
            break;

        case 1: // manpage or help
            return 0;
            break;

        default:
            break;
    }

    PrintVersion();
    printf(_("Available languages are: %s\n"), LANGUAGES);

    // Make a copy of all flags, before gtk_init() removes some.
    // We need this at app refresh. remove: -screen n -lang c.

    Argv = (char **) malloc((argc + 1) * sizeof(char **));
    Argc = 0;
    for (int i = 0; i < argc; i++) {
        if ((strcmp("-screen", argv[i]) == 0) ||
            (strcmp("-lang", argv[i]) == 0)) {
            i++; // found -screen or -lang
        } else {
            Argv[Argc++] = strdup(argv[i]);
        }
    }
    Argv[Argc] = NULL;

    setmGlobalDesktopSession();
    //if (!strncasecmp(mGlobal.DesktopSession, "bspwm", 5)) {
    //    fprintf(stdout, "For optimal resuts, add to your %s:\n", "bspwmrc");
    //    fprintf(stdout, "   bspc rule -a plasmasnow state=floating border=off\n");
    //}

    printf("GTK version: %s\n", ui_gtk_version());
    #ifdef GSL_VERSION
        fprintf(stdout, "GSL version: %s\n", GSL_VERSION);
    #else
        fprintf(stdout, "GSL version: UNKNOWN\n");
    #endif

    // Circumvent wayland problems. Before starting gtk,
    // ensure that the gdk-x11 backend is used.
    setenv("GDK_BACKEND", "x11", 1);
    mGlobal.IsWayland = getenv("WAYLAND_DISPLAY") &&
        getenv("WAYLAND_DISPLAY")[0];
    fprintf(stdout, "Wayland desktop %s detected.\n",
        mGlobal.IsWayland ? "was" : "was not");

    // Init GTK and ensure valid version.
    gtk_init(&argc, &argv);
    if (!Flags.NoConfig) {
        WriteFlags();
    }
    if (!isGtkVersionValid()) {
        printf(_("plasmasnow needs gtk version >= %s, found version %s \n"),
            ui_gtk_required(), ui_gtk_version());
        uninitQPickerDialog();
        endApplication();
        return 0;
    }

    mGlobal.display = XOpenDisplay(Flags.DisplayName);

    mGlobal.xdo = xdo_new_with_opened_display(mGlobal.display, NULL, 0);
    if (mGlobal.xdo == NULL) {
        I("xdo problems\n");
        exit(1);
    }
    mGlobal.xdo->debug = 0;

    XSynchronize(mGlobal.display, DO_SYNCH_DEBUG);
    XSetErrorHandler(handleX11ErrorEvent);

    mGlobal.Screen = DefaultScreen(mGlobal.display);

    // Default any snow colors,
    // a user may have set in .plasmasnowrc.
    int wasThereAnInvalidColor = false;

    if (!ValidColor(Flags.SnowColor)) {
        Flags.SnowColor = strdup(DefaultFlags.SnowColor);
        wasThereAnInvalidColor = true;
    }
    if (!ValidColor(Flags.SnowColor2)) {
        Flags.SnowColor2 = strdup(DefaultFlags.SnowColor2);
        wasThereAnInvalidColor = true;
    }
    if (!ValidColor(Flags.BirdsColor)) {
        Flags.BirdsColor = strdup(DefaultFlags.BirdsColor);
        wasThereAnInvalidColor = true;
    }
    if (!ValidColor(Flags.TreeColor)) {
        Flags.TreeColor = strdup(DefaultFlags.TreeColor);
        wasThereAnInvalidColor = true;
    }
    if (wasThereAnInvalidColor) {
        WriteFlags();
    }

    setGlobalFlakeColor(getRGBFromString(Flags.SnowColor));

    if (mGlobal.display == NULL) {
        fprintf(stdout, "plasmasnow: cannot connect to X server.\n"),
        exit(1);
    }

    // Start Main GUI Window.
    InitSnowOnTrees();
    if (!StartWindow()) {
        return 1;
    }

    // Init all Global Flags.
    #define DOIT_I(x, d, v) OldFlags.x = Flags.x;
    #define DOIT_L(x, d, v) DOIT_I(x, d, v);
    #define DOIT_S(x, d, v) OldFlags.x = strdup(Flags.x);

        DOITALL;
    #include "undefall.inc"

    OldFlags.FullScreen = !Flags.FullScreen;

    // Request all interesting X11 events.
    const Window eventWindow = (mGlobal.hasDestopWindow) ?
        mGlobal.Rootwindow : mGlobal.SnowWin;

    XSelectInput(mGlobal.display, eventWindow,
        StructureNotifyMask | SubstructureNotifyMask |
        FocusChangeMask);
    XFixesSelectCursorInput(mGlobal.display, eventWindow,
        XFixesDisplayCursorNotifyMask);

    clearGlobalSnowWindow();

    if (!Flags.NoMenu && !mGlobal.XscreensaverMode) {
        initUIClass();
        ui_set_sticky(Flags.AllWorkspaces);
    }

    Flags.Done = 0;

    updateWindowsList();
    getWinInfoList();

    addWindowsModuleToMainloop();

    // Init app modules.
    snow_init();
    initFallenSnowModule();
    blowoff_init();
    wind_init();

    Santa_init();

    initSceneryModule();
    treesnow_init();

    birds_init();

    initStarsModule();
    initMeteorModule();
    aurora_init();
    moon_init();

    addLoadMonitorToMainloop();

    addMethodToMainloop(PRIORITY_DEFAULT, time_displaychanged,
        onTimerEventDisplayChanged);
    addMethodToMainloop(PRIORITY_DEFAULT, CONFIGURE_WINDOW_EVENT_TIME,
        handlePendingX11Events);
    addMethodToMainloop(PRIORITY_DEFAULT, time_display_dimensions,
        handleDisplayConfigurationChange);
    addMethodToMainloop(PRIORITY_HIGH, TIME_BETWEEEN_UI_SETTINGS_UPDATES,
        doAllUISettingsUpdates);
    if (Flags.StopAfter > 0) {
        addMethodToMainloop(PRIORITY_DEFAULT, Flags.StopAfter,
            do_stopafter);
    }

    HandleCpuFactor();
    fflush(stdout);

    // Set mGlobal.ChosenWorkSpace
    DoAllWorkspaces();

    //***************************************************
    // main loop
    //***************************************************
    fprintf(stdout, "%s", "\nmain: startApplication() "
        "gtk_main() Starts.\n\n");

    gtk_main();

    //****************
    // Uninit and maybe restart.
    fprintf(stdout, "main: startApplication() gtk_main() "
        "Finishes.\n");

    // Clear vars, close window.
    if (mSnowWindowTitlebarName) {
        free(mSnowWindowTitlebarName);
    }
    XClearWindow(mGlobal.display, mGlobal.SnowWin);
    XFlush(mGlobal.display);
    XCloseDisplay(mGlobal.display);

    //****************
    // Normal app shutdown.
    uninitQPickerDialog();

    // If Restarting due to display change.
    if (mDoRestartDueToDisplayChange) {
        printf("\nplasmasnow Restarting Starts.\n");

        sleep(0);
        for (int i = 0; Argv[i]; i++) {
            printf("%s ", Argv[i]);
        }
        printf("\n");

        fflush(NULL);
        setenv("plasmasnow_RESTART", "yes", 1);
        execvp(Argv[0], Argv);

        printf("plasmasnow Restarting Finishes.\n");
        return 0;
    }

    endApplication();

    const char* endMsg = "main: startApplication() Finishes.\n\n";
    fprintf(stdout, "%s", endMsg);

    return 0;
}

/** *********************************************************************
 ** This method ...
 **/
void endApplication(void) {
    if (mGlobal.HaltedByInterrupt) {
        fprintf(stdout, "\nplasmasnow: Caught signal %d\n",
            mGlobal.HaltedByInterrupt);
    }

    if (strlen(mGlobal.Message)) {
        fprintf(stdout, "\n%s\n", mGlobal.Message);
    }

    fprintf(stdout, "\nThanks for using plasmasnow\n");
    fflush(stdout);
}

/** *********************************************************************
 ** Set the app x11 window order based on user pref.
 **/
void setAppAboveOrBelowAllWindows() {
    setAppBelowAllWindows();
}

/** *********************************************************************
 ** Get our X11 Window, ask the user to select one.
 **/
void getX11Window(Window *resultWin) {
    // Ask user ask to point to a window and click to select.
    if (Flags.XWinInfoHandling) {
        printf(_("plasmasnow: getX11Window() Point to a window and click ...\n"));
        fflush(stdout);

        // Wait for user to click mouse.
        int rc = xdo_select_window_with_click(mGlobal.xdo, resultWin);
        if (rc != XDO_ERROR) {
            // Var "resultWin" has result.
            return;
        }

        fprintf(stderr, "plasmasnow: getX11Window() "
            "Window detection failed.\n");
        exit(1);
    }

    // No window was available.
    *resultWin = 0;
    return;
}

/** *********************************************************************
 ** 
 **/
void setmGlobalDesktopSession() {
    // Early out if already set.
    if (mGlobal.DesktopSession != NULL) {
        return;
    }

    const char *DESKTOPS[] = {"DESKTOP_SESSION",
        "XDG_SESSION_DESKTOP", "XDG_CURRENT_DESKTOP",
        "GDMSESSION", NULL};

    for (int i = 0; DESKTOPS[i]; i++) {
        char* desktopsession = NULL;
        desktopsession = getenv(DESKTOPS[i]);
        if (desktopsession) {
            mGlobal.DesktopSession = strdup(desktopsession);
            return;
        }
    }

    mGlobal.DesktopSession =
        (char *) "unknown_desktop_session";
}

/** *********************************************************************
 ** This method ...
 **/
int StartWindow() {
    mGlobal.Rootwindow = DefaultRootWindow(mGlobal.display);

    mGlobal.hasDestopWindow = false;
    mGlobal.hasTransparentWindow = false;

    mGlobal.useDoubleBuffers = false;
    mGlobal.isDoubleBuffered = false;

    mGlobal.xxposures = false;
    mGlobal.XscreensaverMode = false;

    // Special Startup - User wants to specify a Snow window.
    Window xwin = Flags.WindowId;
    if (!xwin) {
        getX11Window(&xwin);
    }

    if (xwin) {
        mX11CairoEnabled = true;
        mGlobal.SnowWin = xwin;

    // Special Startup - User wants to run in root window.
    } else if (Flags.ForceRoot) {
        mX11CairoEnabled = true;
        mGlobal.SnowWin = mGlobal.Rootwindow;

        // User wants a screensaver too.
        if (getenv("XSCREENSAVER_WINDOW")) {
            mGlobal.XscreensaverMode = true;
            mGlobal.SnowWin = strtol(getenv("XSCREENSAVER_WINDOW"), NULL, 0);
            mGlobal.Rootwindow = mGlobal.SnowWin;
        }

    // Normal Startup.
    } else {
        // Try to create a transparent clickthrough window.
        GtkWidget* newGTKWindow = gtk_window_new(GTK_WINDOW_TOPLEVEL);
        gtk_widget_set_can_focus(newGTKWindow, TRUE);
        gtk_window_set_decorated(GTK_WINDOW(newGTKWindow), FALSE);
        gtk_window_set_type_hint(GTK_WINDOW(newGTKWindow),
            GDK_WINDOW_TYPE_HINT_POPUP_MENU);

        // xwin might be our transparent window ...
        if (createTransparentWindow(mGlobal.display, newGTKWindow,
            Flags.Screen, Flags.AllWorkspaces, true,
            true, NULL, &xwin, &wantx, &wanty)) {
            mTransparentWindow = newGTKWindow;

            mGlobal.SnowWin = xwin;
            mGlobal.hasTransparentWindow = true;
            mGlobal.hasDestopWindow = true;

            mGlobal.isDoubleBuffered = true;

            GtkWidget *drawing_area = gtk_drawing_area_new();
            gtk_container_add(GTK_CONTAINER(mTransparentWindow), drawing_area);

            g_signal_connect(mTransparentWindow, "draw",
                G_CALLBACK(handleTransparentWindowDrawEvents), NULL);

        // xwin might be our rootwindow, pcmanfm or Desktop:
        } else {
            mGlobal.hasDestopWindow = true;
            mX11CairoEnabled = true;

            if (!strncmp(mGlobal.DesktopSession, "LXDE", 4) &&
                (xwin = largest_window_with_name(mGlobal.xdo, "^pcmanfm$"))) {
            } else if ((xwin = largest_window_with_name(mGlobal.xdo, "^Desktop$"))) {
            } else {
                xwin = mGlobal.Rootwindow;
            }

            mGlobal.SnowWin = xwin;
            int winw, winh;

            if (Flags.Screen >= 0 && mGlobal.hasDestopWindow) {
                getXineramaScreenInfo(mGlobal.display, Flags.Screen,
                    &wantx, &wanty, &winw, &winh);
            }
        }
    }

    // Start window Cairo specific.
    if (mX11CairoEnabled) {
        handleX11CairoDisplayChange();
        mCairoWindowGUID = addMethodWithArgToMainloop(
            PRIORITY_HIGH, time_draw_all, drawCairoWindow, mCairoWindow);
        mGlobal.WindowOffsetX = 0;
        mGlobal.WindowOffsetY = 0;
    } else {
        mGlobal.WindowOffsetX = wantx;
        mGlobal.WindowOffsetY = wanty;
    }

    mGlobal.isDoubleBuffered = mGlobal.hasTransparentWindow ||
        mGlobal.useDoubleBuffers;

    mSnowWindowTitlebarName = strdup("no name");
    XTextProperty titleBarName;
    if (XGetWMName(mGlobal.display, mGlobal.SnowWin, &titleBarName)) {
        mSnowWindowTitlebarName = strdup((char *) titleBarName.value);
    }
    XFree(titleBarName.value);

    if (!mX11CairoEnabled) {
        xdo_move_window(mGlobal.xdo, mGlobal.SnowWin, wantx, wanty);
    }

    xdo_wait_for_window_map_state(mGlobal.xdo, mGlobal.SnowWin, IsViewable);
    initDisplayDimensions();

    // Report log.
    mGlobal.SnowWinX = wantx;
    mGlobal.SnowWinY = wanty;
    printf(_("\nSnowing in %#lx: %s %d+%d %dx%d\n"), mGlobal.SnowWin, mSnowWindowTitlebarName,
        mGlobal.SnowWinX, mGlobal.SnowWinY, mGlobal.SnowWinWidth,
        mGlobal.SnowWinHeight);

    mPrevSnowWinWidth = mGlobal.SnowWinWidth;
    mPrevSnowWinHeight = mGlobal.SnowWinHeight;
    printf(_("mGlobal.WindowOffsetX, mGlobal.WindowOffsetY: %d %d\n"),
        mGlobal.WindowOffsetX, mGlobal.WindowOffsetY);

    fflush(stdout);
    SetWindowScale();
    if (mGlobal.XscreensaverMode && !Flags.BlackBackground) {
        SetBackground();
    }

    return TRUE;
}

/** *********************************************************************
 ** Cairo specific. handleDisplayConfigurationChange() startApplication->StartWindow()
 **/
void handleX11CairoDisplayChange() {
#ifdef XDBE_AVAILABLE
    int dodouble = Flags.useDoubleBuffers;
#else
    int dodouble = 0;
#endif

    unsigned int w, h;
    xdo_get_window_size(mGlobal.xdo, mGlobal.SnowWin, &w, &h);

#ifdef XDBE_AVAILABLE
    if (dodouble) {
        static Drawable backBuf = 0;
        if (backBuf) {
            XdbeDeallocateBackBufferName(mGlobal.display, backBuf);
        }
        backBuf = XdbeAllocateBackBufferName(mGlobal.display,
            mGlobal.SnowWin, XdbeBackground);

        if (mCairoSurface) {
            cairo_surface_destroy(mCairoSurface);
        }

        Visual *visual = DefaultVisual(mGlobal.display,
            DefaultScreen(mGlobal.display));
        mCairoSurface = cairo_xlib_surface_create(mGlobal.display,
            backBuf, visual, w, h);

        mGlobal.useDoubleBuffers = true;
        mGlobal.isDoubleBuffered = true;
    }
#endif

    if (!dodouble) {
        Visual *visual = DefaultVisual(mGlobal.display,
            DefaultScreen(mGlobal.display));
        mCairoSurface = cairo_xlib_surface_create(
            mGlobal.display, mGlobal.SnowWin, visual, w, h);
    }

    {   // Destroy & create new Cairo Window.
        if (mCairoWindow) {
            cairo_destroy(mCairoWindow);
        }
        mCairoWindow = cairo_create(mCairoSurface);
        cairo_xlib_surface_set_size(mCairoSurface, w, h);
    }

    mGlobal.SnowWinWidth = w;
    mGlobal.SnowWinHeight = h;

    if (Flags.Screen >= 0 && mGlobal.hasDestopWindow) {
        int winx, winy, winw, winh;
        if (getXineramaScreenInfo(mGlobal.display, Flags.Screen,
            &winx, &winy, &winw, &winh)) {
            mGlobal.SnowWinX = winx;
            mGlobal.SnowWinY = winy;
            mGlobal.SnowWinWidth = winw;
            mGlobal.SnowWinHeight = winh;
        }

        cairo_rectangle(mCairoWindow, mGlobal.SnowWinX, mGlobal.SnowWinY,
            mGlobal.SnowWinWidth, mGlobal.SnowWinHeight);
        cairo_clip(mCairoWindow);
    }
}

/** *********************************************************************
 ** Set the Transparent Window Sticky Flag.
 **/
void setTransparentWindowStickyState(int isSticky) {
    if (!mGlobal.hasTransparentWindow) {
        return;
    }

    mIsSticky = isSticky;
    if (mIsSticky) {
        gtk_window_stick(GTK_WINDOW(mTransparentWindow));
    } else {
        gtk_window_unstick(GTK_WINDOW(mTransparentWindow));
    }
}

/** *********************************************************************
 ** TODO:
 **/
void DoAllWorkspaces() {
    if (Flags.AllWorkspaces) {
        setTransparentWindowStickyState(1);
    } else {
        setTransparentWindowStickyState(0);

        mGlobal.ChosenWorkSpace = (Flags.Screen >= 0) ?
            mGlobal.VisWorkSpaces[Flags.Screen] :
            mGlobal.VisWorkSpaces[0];
    }

    ui_set_sticky(Flags.AllWorkspaces);
}

/** *********************************************************************
 * here we are handling the buttons in ui
 * Ok, this is a long list, and could be implemented more efficient.
 * But, doAllUISettingsUpdates is called not too frequently, so....
 * Note: if changes != 0, the settings will be written to .plasmasnowrc
 **/
int doAllUISettingsUpdates() {
    if (Flags.Done) {
        gtk_main_quit();
    }

    if (Flags.NoMenu) {
        return TRUE;
    }

    Santa_ui();
    updateSceneryUserSettings();
    birds_ui();

    snow_ui();

    updateMeteorUserSettings();
    wind_ui();
    updateStarsUserSettings();
    doFallenSnowUserSettingUpdates();
    blowoff_ui();
    treesnow_ui();
    moon_ui();
    aurora_ui();
    updateMainWindowUI();

    UIDO(CpuLoad, HandleCpuFactor(););
    UIDO(Transparency, );
    UIDO(Scale, );
    UIDO(OffsetS, updateDisplayDimensions(););
    UIDO(OffsetY, updateFallenSnowRegionsWithLock(););
    UIDO(AllWorkspaces, DoAllWorkspaces(););

    UIDOS(BackgroundFile, );
    UIDO(BlackBackground, );

    if (Flags.Changes > 0) {
        WriteFlags();
        set_buttons();
        P("-----------Changes: %d\n", Flags.Changes);
        Flags.Changes = 0;
    }

    return TRUE;
}

/** *********************************************************************
 * If we are snowing in the desktop, we check if the size has changed,
 * this can happen after changing of the displays settings
 * If the size has been changed, we refresh the app.
 **/
int onTimerEventDisplayChanged() {
    if (Flags.Done) {
        return -1;
    }

    if (!mGlobal.hasDestopWindow) {
        return -1;
    }

    // I(_("Refresh due to change of screen or language settings ...\n"));
    if (mGlobal.ForceRestart) {
        mDoRestartDueToDisplayChange = 1;
        Flags.Done = 1;
        return -1;
    }

    // P("width height: %d %d %d %d\n", w, h, mGlobal.Wroot, mGlobal.Hroot);
    // I(_("Refresh due to change of display settings...\n"));
    Display *display = XOpenDisplay(Flags.DisplayName);
    Screen *screen = DefaultScreenOfDisplay(display);

    unsigned int w = WidthOfScreen(screen);
    unsigned int h = HeightOfScreen(screen);
    if (mGlobal.Wroot != w || mGlobal.Hroot != h) {
        mDoRestartDueToDisplayChange = 1;
        Flags.Done = 1;
    }

    XCloseDisplay(display);
    return -1;
}

/** *********************************************************************
 ** This method handles xlib11 event notifications.
 ** Undergoing heavy renovation 2024 Rocks !
 **
 ** https://www.x.org/releases/current/doc/libX11/libX11/libX11.html#Events
 **/
int handlePendingX11Events() {
    if (Flags.Done) {
        return FALSE;
    }

    XFlush(mGlobal.display);
    while (XPending(mGlobal.display)) {
        XEvent event;
        XNextEvent(mGlobal.display, &event);

        // Check for Active window change through loop.
        const Window activeX11Window = getActiveX11Window();
        if (getActiveAppWindow() != activeX11Window) {
            onAppWindowChange(activeX11Window);
        }

        // Perform X11 event action.
        switch (event.type) {
            case CreateNotify:
                onWindowCreated(&event);
                break;

            case ReparentNotify:
                onWindowReparent(&event);
                break;

            case ConfigureNotify:
                onWindowChanged(&event);
                if (!isWindowBeingDragged()) {
                    mGlobal.WindowsChanged++;
                    if (event.xconfigure.window == mGlobal.SnowWin) {
                        mMainWindowNeedsReconfiguration = true;
                    }
                }
                break;

            case MapNotify:
                mGlobal.WindowsChanged++;
                onWindowMapped(&event);
                break;

            case FocusIn:
                onWindowFocused(&event);
                break;

            case FocusOut:
                onWindowBlurred(&event);
                break;

            case UnmapNotify:
                mGlobal.WindowsChanged++;
                onWindowUnmapped(&event);
                break;

            case DestroyNotify:
                onWindowDestroyed(&event);
                break;

            default:
                // Perform XFixes action.
                int xfixes_event_base;
                int xfixes_error_base;
                if (XFixesQueryExtension(mGlobal.display,
                    &xfixes_event_base, &xfixes_error_base)) {
                    switch (event.type - xfixes_event_base) {
                        case XFixesCursorNotify:
                            onCursorChange(&event);
                            break;
                    }
                }
                break;
        }
    }

    return TRUE;
}

/** *********************************************************************
 ** This method ...
 **/
void RestartDisplay() {
    fflush(stdout);

    initFallenSnowListWithDesktop();
    initStarsModuleArrays();
    clearAndRedrawScenery();

    if (!Flags.NoKeepSnowOnTrees && !Flags.NoTrees) {
        reinit_treesnow_region();
    }

    if (!Flags.NoTrees) {
        cairo_region_destroy(mGlobal.TreeRegion);
        mGlobal.TreeRegion = cairo_region_create();
    }

    if (!mGlobal.isDoubleBuffered) {
        clearGlobalSnowWindow();
    }
}

/** *********************************************************************
 ** This method ...
 **/
void SigHandler(int signum) {
    mGlobal.HaltedByInterrupt = signum;
    Flags.Done = 1;
}

/** *********************************************************************
 ** This method traps and handles X11 errors.
 **
 ** Primarily, we close the app if the system doesn't seem sane.
 **/
int handleX11ErrorEvent(Display *dpy, XErrorEvent *err) {
    const int MAX_MESSAGE_BUFFER_LENGTH = 1024;
    char msg[MAX_MESSAGE_BUFFER_LENGTH];
    XGetErrorText(dpy, err->error_code, msg, sizeof(msg));
    msg[MAX_MESSAGE_BUFFER_LENGTH - 1] = '\x0'; // EOString

    // If we notice too many X11 errors, print any mGlobal.Message
    // and set app termination flag.
    msg[60] = '\x0'; fprintf(stdout, "%s", msg);
    if (++mX11Error_Count > mX11Error_maxBeforeTermination) {
        snprintf(mGlobal.Message, sizeof(mGlobal.Message),
            _("More than %d errors, I quit!"), mX11Error_maxBeforeTermination);
        Flags.Done = 1;
    }

    return 0;
}

/** *********************************************************************
 ** This method id the draw callback.
 **/
gboolean handleTransparentWindowDrawEvents(
    __attribute__((unused)) GtkWidget *widget, cairo_t *cr,
    __attribute__((unused)) gpointer user_data) {

    drawCairoWindowInternal(cr);
    return FALSE;
}

/** *********************************************************************
 ** This method ...
 **/
int drawCairoWindow(void *cr) {
    drawCairoWindowInternal((cairo_t *) cr);
    return TRUE;
}

/** *********************************************************************
 ** Due to instabilities at the start of app, stars is
 ** repeated a few times. This is not harmful. We do not draw
 ** anything the first few times this function is called.
 **/
void drawCairoWindowInternal(cairo_t *cr) {
    // Instabilities (?).
    static int counter = 0;
    if (counter * time_draw_all < 1.5) {
        counter++;
        return;
    }
    if (Flags.Done) {
        return;
    }

    // Do all module clears.
    if (mGlobal.useDoubleBuffers) {
        XdbeSwapInfo swapInfo;
        swapInfo.swap_window = mGlobal.SnowWin;
        swapInfo.swap_action = XdbeBackground;
        XdbeSwapBuffers(mGlobal.display, &swapInfo, 1);

    } else if (!mGlobal.isDoubleBuffered) {
        XFlush(mGlobal.display);
        moon_erase(0);
        Santa_erase(cr);
        eraseStarsFrame();
        birds_erase(0);
        snow_erase(1);
        aurora_erase();
        XFlush(mGlobal.display);
    }

    // Do cairo.
    cairo_save(cr);

    int tx = 0;
    int ty = 0;
    if (mX11CairoEnabled) {
        tx = mGlobal.SnowWinX;
        ty = mGlobal.SnowWinY;
    }
    cairo_translate(cr, tx, ty);

    // Do all module draws.
    if (WorkspaceActive()) {
        drawStarsFrame(cr);
        moon_draw(cr);
        aurora_draw(cr);
        drawMeteorFrame(cr);
        drawSceneryFrame(cr);
        birds_draw(cr);
        cairoDrawAllFallenSnowItems(cr);
        if (!Flags.ShowBirds || !Flags.FollowSanta) {
            // If Flags.FollowSanta, drawing of Santa
            // is done in Birds module.
            Santa_draw(cr);
        }
        treesnow_draw(cr);
        snow_draw(cr);
    }

    // Draw app window outline.
    if (Flags.Outline) {
        rectangle_draw(cr);
    }

    cairo_restore(cr);
    XFlush(mGlobal.display);
}

/** *********************************************************************
 ** This method ...
 **/
void SetWindowScale() {
    // assuming a standard screen of 1024x576, we suggest to use the scalefactor
    // WindowScale
    float x = mGlobal.SnowWinWidth / 1000.0;
    float y = mGlobal.SnowWinHeight / 576.0;

    if (x < y) {
        mGlobal.WindowScale = x;
    } else {
        mGlobal.WindowScale = y;
    }
    P("WindowScale: %f\n", mGlobal.WindowScale);
}

/** *********************************************************************
 ** This method ...
 **/
int handleDisplayConfigurationChange() {
    if (Flags.Done) {
        return FALSE;
    }

    if (!mMainWindowNeedsReconfiguration) {
        return TRUE;
    }
    mMainWindowNeedsReconfiguration = false;

    if (!mGlobal.hasTransparentWindow) {
        handleX11CairoDisplayChange();
    }

    if (mPrevSnowWinWidth != mGlobal.SnowWinWidth ||
        mPrevSnowWinHeight != mGlobal.SnowWinHeight) {
        updateDisplayDimensions();
        RestartDisplay();
        mPrevSnowWinWidth = mGlobal.SnowWinWidth;
        mPrevSnowWinHeight = mGlobal.SnowWinHeight;
        SetWindowScale();
    }

    fflush(stdout);
    return TRUE;
}

/** *********************************************************************
 ** This method ...
 **/
int drawTransparentWindow(gpointer widget) {
    if (Flags.Done) {
        return FALSE;
    }

    // this will result in a call off handleTransparentWindowDrawEvents():
    gtk_widget_queue_draw(GTK_WIDGET(widget));
    return TRUE;
}

/** *********************************************************************
 ** This method handles callbacks for cpufactor
 **/
void HandleCpuFactor() {
    if (Flags.CpuLoad <= 0) {
        mGlobal.cpufactor = 1;
    } else {
        mGlobal.cpufactor = 100.0 / Flags.CpuLoad;
    }

    addMethodToMainloop(PRIORITY_HIGH, time_init_snow, do_initsnow);

    addWindowDrawMethodToMainloop();
}

/** *********************************************************************
 ** This method...
 **/
void addWindowDrawMethodToMainloop() {
    if (mGlobal.hasTransparentWindow) {
        if (mTransparentWindowGUID) {
            g_source_remove(mTransparentWindowGUID);
        }
        mTransparentWindowGUID = addMethodWithArgToMainloop(PRIORITY_HIGH,
            time_draw_all, drawTransparentWindow, mTransparentWindow);
        return;
    }

    if (mCairoWindowGUID) {
        g_source_remove(mCairoWindowGUID);
    }

    mCairoWindowGUID = addMethodWithArgToMainloop(PRIORITY_HIGH,
        time_draw_all, drawCairoWindow, mCairoWindow);
}

/** *********************************************************************
 ** This method ...
 **/
void rectangle_draw(cairo_t *cr) {
    const int lw = 8;

    cairo_save(cr);

    cairo_set_source_rgba(cr, 1, 1, 0, 0.5);

    cairo_rectangle(cr, lw / 2, lw / 2, mGlobal.SnowWinWidth - lw,
        mGlobal.SnowWinHeight - lw);

    cairo_set_line_width(cr, lw);
    cairo_stroke(cr);
    cairo_restore(cr);
}

/** *********************************************************************
 ** This method ...
 **/
int do_stopafter() {
    Flags.Done = 1;
    printf(_("Halting because of flag %s\n"), "-stopafter");

    return FALSE;
}

/** *********************************************************************
 ** This method ...
 **/
void mybindtestdomain() {
    mGlobal.Language = guess_language();

#ifdef HAVE_GETTEXT
    // One would assume that gettext() uses the environment variable LOCPATH
    // to find locales, but no, it does not.
    // So, we scan LOCPATH for paths, use them and see if gettext gets a
    // translation for a text that is known to exist in the ui and should be
    // translated to something different.

    char* startlocale;
    (void) startlocale;

    char* resultLocale = setlocale(LC_ALL, "");
    startlocale = strdup(resultLocale ? resultLocale : NULL);

    textdomain(TEXTDOMAIN);
    bindtextdomain(TEXTDOMAIN, LOCALEDIR);

    char* locpath = getenv("LOCPATH");
    if (locpath) {
        char *initial_textdomain = strdup(bindtextdomain(TEXTDOMAIN, NULL));
        char *mylocpath = strdup(locpath);
        int translation_found = False;

        char *q = mylocpath;
        while (1) {
            char *p = strsep(&q, ":");
            if (!p) {
                break;
            }
            bindtextdomain(TEXTDOMAIN, p);

            if (strcmp(_(TESTSTRING), TESTSTRING)) {
                translation_found = True;
                break;
            }
        }
        if (!translation_found) {
            bindtextdomain(TEXTDOMAIN, initial_textdomain);
        }

        free(mylocpath);
        free(initial_textdomain);
    }

    // Ugly debug.
    if (0) {
        if (strcmp(Flags.Language, "sys")) {
            char lc[100];
            if (!strcmp(Flags.Language, "en")) {
                strcpy(lc, "en_US.UTF-8");
            } else {
                strcpy(lc, Flags.Language);
                strcat(lc, "_");

                int i;
                int l = strlen(lc);
                for (i = 0; i < (int)strlen(Flags.Language); i++) {
                    lc[l + i] = toupper(lc[i]);
                }
                lc[l + i] = 0;

                strcat(lc, ".UTF-8");
            }

            // in order for this to work, no gettext call must have been
            // made yet:

            // setenv("LC_ALL",lc,1);
            // setenv("LANG",lc,1);
            setenv("LANGUAGE", Flags.Language, 1);
            P("LC_ALL: %s\n", getenv("LC_ALL"));
            P("LANG: %s\n", getenv("LANG"));
            P("LANGUAGE: %s\n", getenv("LANGUAGE"));

        } else {
            unsetenv("LANGUAGE");
            unsetenv("LC_ALL");
            if (0) {
                char *l = getenv("LANG");
                if (l && !getenv("LANGUAGE")) {
                    char *lang = strdup(l);
                    P("lang: %s\n", lang);
                    char *p = strchr(lang, '_');
                    if (p) {
                        *p = 0; // TODO: wrong
                        P("lang: %s\n", lang);
                        setenv("LANGUAGE", lang, 1);
                    }
                    free(lang);
                }
            }
            P("LC_ALL: %s\n", getenv("LC_ALL"));
            P("LANG: %s\n", getenv("LANG"));
            P("LANGUAGE: %s\n", getenv("LANGUAGE"));
        }
    }
#endif

}

/** *********************************************************************
 ** Set BelowAll. To be sure, we push down the Transparent GTK Window
 **               and our actual app Snow Window.
 **/
void setAppBelowAllWindows() {
    doLowerWindow(mGlobal.mPlasmaWindowTitle);
    logAllWindowsStackedTopToBottom();
}

