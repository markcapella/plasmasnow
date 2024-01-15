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
 *
 */

// Change to 1 for better analysis
#define dosync 0 // synchronise X-server.

#include <pthread.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "mygettext.h"

#include <X11/Intrinsic.h>
#include <X11/extensions/Xdbe.h>
#include <X11/extensions/Xinerama.h>
#include <cairo-xlib.h>

#include <ctype.h>
#include <gsl/gsl_version.h>
#include <gtk/gtk.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Santa.h"
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
#include "safe_malloc.h"
#include "scenery.h"
#include "selfrep.h"
#include "snow.h"
#include "stars.h"
#include "transwindow.h"
#include "treesnow.h"
#include "ui.h"
#include "utils.h"
#include "version.h"
#include "wind.h"
#include "windows.h"
#include "wmctrl.h"
#include "xdo.h"

#include "plasmasnow.h"

#include "vroot.h"


/***********************************************************
 * Module Method stubs.
 */
static void HandleCpuFactor();
static void RestartDisplay();
static void SigHandler(int);

static int handleX11ErrorEvent(Display*, XErrorEvent*);
static void getX11Window(Window*);
static int HandleX11Cairo();

static void drawit(cairo_t*);
static int do_drawit(void*);
static int do_draw_all(gpointer);

static void restart_do_draw_all();
static gboolean on_draw_event(GtkWidget*, cairo_t*, gpointer);

static void rectangle_draw(cairo_t*);

static int StartWindow();
static void SetWindowScale();

static int handlePendingX11Events();
static int onTimerEventDisplayChanged();

static void mybindtestdomain();
static void set_below_above();

static void GetDesktopSession();
static void DoAllWorkspaces();

static int doAllUISettingsUpdates();
static int do_stopafter();
static int do_display_dimensions();
static int do_testing();


// Windows.
void onWindowCreated(Window);
void onWindowDestroyed(Window);
void onWindowMapped(Window);
void onWindowUnmapped(Window);

// ColorPicker methods.
void uninitQPickerDialog();

/** *********************************************************************
 ** Module globals and consts.
 **/
#ifdef DEBUG
#undef DEBUG
#endif

#define BMETHOD XdbeBackground

struct _global global;

bool mMainWindowNeedsReconfiguration = true;

cairo_t *CairoDC = NULL;
cairo_surface_t *CairoSurface = NULL;

char Copyright[] =
    "\nplasmasnow\nCopyright 2023 Mark Capella and "
    "1984,1988,1990,1993-1995,2000-2001 by Rick Jansen, all "
    "rights reserved, 2019,2020 also by Willem Vermin\n";


static char **Argv;
static int Argc;

static int DoRestart = 0;
static guint draw_all_id = 0;
static guint drawit_id = 0;
static GtkWidget *TransA = NULL;

static char *SnowWinName = NULL;

static int wantx = 0;
static int wanty = 0;
static int mIsSticky = 0;
static int X11cairo = 0;
static int PrevW = 0;
static int PrevH = 0;

static const char *BlackColor = "black";
static Pixel BlackPix;

static int mX11Error_Count = 0;
const int mX11Error_maxBeforeTermination = 1000;


/** *********************************************************************
 ** main.c: 
 **/
int main_c(int argc, char *argv[]) {
    signal(SIGINT, SigHandler);
    signal(SIGTERM, SigHandler);
    signal(SIGHUP, SigHandler);

    srand48((int) (fmod(wallcl() * 1.0e6, 1.0e8)));

    memset(&global, 0, sizeof(global));

    XInitThreads();

    initFallenSnowSemaphores();
    aurora_sem_init();
    birds_sem_init();

    global.counter = 0;
    global.cpufactor = 1.0;
    global.WindowScale = 1.0;

    global.MaxFlakeHeight = 0;
    global.MaxFlakeWidth = 0;

    global.FlakeCount = 0; /* # active flakes */
    global.FluffCount = 0;

    global.SnowWin = 0;
    global.WindowOffsetX = 0;
    global.WindowOffsetY = 0;

    global.DesktopSession = NULL;
    global.CWorkSpace = 0;
    global.ChosenWorkSpace = 0;
    global.NVisWorkSpaces = 1;
    global.VisWorkSpaces[0] = 0;
    global.WindowsChanged = 0;
    global.ForceRestart = 0;

    global.FsnowFirst = NULL;
    global.MaxScrSnowDepth = 0;
    global.RemoveFluff = 0;

    global.SnowOnTrees = NULL;
    global.OnTrees = 0;

    global.moonX = 1000;
    global.moonY = 80;

    global.Wind = 0;
    global.Direction = 0;
    global.WindMax = 500.0;
    global.NewWind = 100.0;

    global.HaltedByInterrupt = 0;
    global.Message[0] = 0;

    global.SantaPlowRegion = 0;

    InitFlags();

    // we search for flags that only produce output to stdout,
    // to enable to run in a non-X environment, in which case
    // gtk_init() would fail.
    int i;
    for (i = 0; i < argc; i++) {
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
            Thanks();
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

    Argv = (char **)malloc((argc + 1) * sizeof(char **));
    Argc = 0;
    for (i = 0; i < argc; i++) {
        if ((strcmp("-screen", argv[i]) == 0) ||
            (strcmp("-lang", argv[i]) == 0)) {
            i++; // found -screen or -lang
        } else {
            Argv[Argc++] = strdup(argv[i]);
        }
    }
    Argv[Argc] = NULL;

    GetDesktopSession();
    if (!strncasecmp(global.DesktopSession, "bspwm", 5)) {
        printf(_("For optimal resuts, add to your %s:\n"), "bspwmrc");
        printf("   bspc rule -a plasmasnow state=floating border=off\n");
    }

    printf("GTK version: %s\n", ui_gtk_version());
    printf("GSL version: ");
#ifdef GSL_VERSION
    printf("%s\n", GSL_VERSION);
#else
    printf(_("unknown\n"));
#endif

    // Circumvent wayland problems:before starting gtk: make sure that the
    // gdk-x11 backend is used.

    setenv("GDK_BACKEND", "x11", 1);
    if (getenv("WAYLAND_DISPLAY") && getenv("WAYLAND_DISPLAY")[0]) {
        printf(_("Detected Wayland desktop\n"));
        global.IsWayland = 1;
    } else {
        global.IsWayland = 0;
    }

    gtk_init(&argc, &argv);
    if (!Flags.NoConfig) {
        WriteFlags();
    }

    if (Flags.BelowAllForce) {
        Flags.BelowAll = 0;
    }

    if (Flags.CheckGtk && !ui_checkgtk() && !Flags.NoMenu) {
        printf(_("plasmasnow needs gtk version >= %s, found version %s \n"),
            ui_gtk_required(), ui_gtk_version());

        if (!ui_run_nomenu()) {
            uninitQPickerDialog();
            Thanks();
            return 0;
        }

        Flags.NoMenu = 1;
        printf("%s %s", _("Continuing with flag"), "'-nomenu'\n");
    }

    global.display = XOpenDisplay(Flags.DisplayName);
    global.xdo = xdo_new_with_opened_display(global.display, NULL, 0);
    if (global.xdo == NULL) {
        I("xdo problems\n");
        exit(1);
    }
    global.xdo->debug = 0;

    XSynchronize(global.display, dosync);

    XSetErrorHandler(handleX11ErrorEvent);

    int screen = DefaultScreen(global.display);
    global.Screen = screen;
    global.Black = BlackPixel(global.display, screen);
    global.White = WhitePixel(global.display, screen);

    { // if somebody messed up colors in .plasmasnowrc:
        if (!ValidColor(Flags.SnowColor)) {
            Flags.SnowColor = strdup(DefaultFlags.SnowColor);
        }
        if (!ValidColor(Flags.SnowColor2)) {
            Flags.SnowColor2 = strdup(DefaultFlags.SnowColor2);
        }
        if (!ValidColor(Flags.BirdsColor)) {
            Flags.BirdsColor = strdup(DefaultFlags.BirdsColor);
        }
        if (!ValidColor(Flags.TreeColor)) {
            Flags.TreeColor = strdup(DefaultFlags.TreeColor);
        }
        WriteFlags();
    }

    InitSnowOnTrees();

    if (global.display == NULL) {
        if (Flags.DisplayName == NULL) {
            Flags.DisplayName = getenv("DISPLAY");
        }
        fprintf(stderr, _("%s: cannot connect to X server %s\n"), argv[0],
            Flags.DisplayName ? Flags.DisplayName : _("(default)"));
        exit(1);
    }

    if (!StartWindow()) {
        return 1;
    }

#define DOIT_I(x, d, v) OldFlags.x = Flags.x;
#define DOIT_L(x, d, v) DOIT_I(x, d, v);
#define DOIT_S(x, d, v) OldFlags.x = strdup(Flags.x);
    DOITALL;
#include "undefall.inc"

    OldFlags.FullScreen = !Flags.FullScreen;

    BlackPix = AllocNamedColor(BlackColor, global.Black);

    if (global.Desktop) {
        XSelectInput(global.display, global.Rootwindow,
            StructureNotifyMask | SubstructureNotifyMask);
    } else {
        XSelectInput(global.display, global.SnowWin,
            StructureNotifyMask | SubstructureNotifyMask);
    }

    ClearScreen();
    if (!Flags.NoMenu && !global.XscreensaverMode) {
        initUIClass();
        ui_gray_erase(global.Trans);
        ui_set_sticky(Flags.AllWorkspaces);
    }

    Flags.Done = 0;
    windows_init();

    moon_init();
    aurora_init();
    Santa_init();
    birds_init();
    scenery_init();
    snow_init();
    meteor_init();
    wind_init();
    stars_init();
    blowoff_init();
    treesnow_init();
    addLoadMonitorToMainloop();
    initFallenSnowModule();

    add_to_mainloop(PRIORITY_DEFAULT, time_displaychanged,
        onTimerEventDisplayChanged);
    add_to_mainloop(PRIORITY_DEFAULT, CONFIGURE_WINDOW_EVENT_TIME,
        handlePendingX11Events);
    add_to_mainloop(PRIORITY_DEFAULT, time_testing, do_testing);
    add_to_mainloop(PRIORITY_DEFAULT, time_display_dimensions,
        do_display_dimensions);

    add_to_mainloop(PRIORITY_HIGH, TIME_BETWEEEN_UI_SETTINGS_UPDATES, doAllUISettingsUpdates);

    if (Flags.StopAfter > 0) {
        add_to_mainloop(PRIORITY_DEFAULT, Flags.StopAfter, do_stopafter);
    }

    HandleCpuFactor();

    fflush(stdout);

    DoAllWorkspaces(); // to set global.ChosenWorkSpace

    P("Entering main loop\n");
    // main loop
    gtk_main();

    if (SnowWinName) {
        free(SnowWinName);
    }

    XClearWindow(global.display, global.SnowWin);
    XFlush(global.display);
    XCloseDisplay(global.display);

    if (DoRestart) {
        sleep(0);
        printf("plasmasnow %s: ", _("restarting"));

        int k = 0;
        while (Argv[k]) {
            printf("%s ", Argv[k++]);
        }
        printf("\n");
        fflush(NULL);

        setenv("plasmasnow_RESTART", "yes", 1);
        execvp(Argv[0], Argv);

    } else {
        uninitQPickerDialog();
        Thanks();
    }

    return 0;
}

/** *********************************************************************
 ** 
 **/
void set_below_above() {
    XWindowChanges changes;

    // to be sure: we do it in gtk mode and window mode
    if (Flags.BelowAll) {
        if (TransA) {
            setbelow(GTK_WINDOW(TransA));
        }

        changes.stack_mode = Below;
        if (global.SnowWin) {
            XConfigureWindow(global.display, global.SnowWin,
                CWStackMode, &changes);
        }
    } else {
        if (TransA) {
            setabove(GTK_WINDOW(TransA));
        }

        changes.stack_mode = Above;
        if (global.SnowWin) {
            XConfigureWindow(global.display, global.SnowWin,
                CWStackMode, &changes);
        }
    }
}

/** *********************************************************************
 ** Get our X11 Window. If none is available, ask the user to select one.
 **/
void getX11Window(Window *resultWin) {
    if (Flags.WindowId) {
        *resultWin = Flags.WindowId;
        return;
    }

    // Ask user ask to point to a window and click to select.
    if (Flags.XWinInfoHandling) {
        printf(_("plasmasnow: getX11Window() Point to a window and click ...\n"));
        fflush(stdout);

        // Wait for user to click mouse.
        int rc = xdo_select_window_with_click(global.xdo, resultWin);
        if (rc != XDO_ERROR) {
            // Var "resultWin" has result.
            return;
        }

        fprintf(stderr, "plasmasnow: getX11Window() Window detection failed.\n");
        exit(1);
    }

    // No window was available.
    *resultWin = 0;
    return;
}

/** *********************************************************************
 ** 
 **/
void GetDesktopSession() {
    if (global.DesktopSession == NULL) {
        char *desktopsession = NULL;
        const char *desktops[] = {"DESKTOP_SESSION", "XDG_SESSION_DESKTOP",
            "XDG_CURRENT_DESKTOP", "GDMSESSION", NULL};

        int i;
        for (i = 0; desktops[i]; i++) {
            desktopsession = getenv(desktops[i]);
            if (desktopsession) {
                break;
            }
        }
        if (desktopsession) {
            printf(_("Detected desktop session: %s\n"), desktopsession);
        } else {
            printf(_("Could not determine desktop session\n"));
            desktopsession = (char *)"unknown_desktop_session";
        }

        global.DesktopSession = strdup(desktopsession);
    }
}

/** *********************************************************************
 ** This method ...
 **/
int StartWindow() {
    global.Rootwindow = DefaultRootWindow(global.display);
    global.Desktop = 0;

    global.UseDouble = 0;
    global.IsDouble = 0;

    global.Trans = 0;
    global.xxposures = 0;
    global.XscreensaverMode = 0;

    // see if user chooses window
    Window xwin;
    getX11Window(&xwin);
    if (xwin) {
        P("StartWindow xwin%#lx\n", xwin);
        global.SnowWin = xwin;
        X11cairo = 1;

    } else if (Flags.ForceRoot) {
        // user specified to run on root window
        printf(_("Trying to snow in root window\n"));
        global.SnowWin = global.Rootwindow;
        if (getenv("XSCREENSAVER_WINDOW")) {
            global.XscreensaverMode = 1;
            global.SnowWin = strtol(getenv("XSCREENSAVER_WINDOW"), NULL, 0);
            global.Rootwindow = global.SnowWin;
        }
        X11cairo = 1;

    } else {
        // try to create a transparent clickthrough window
        GtkWidget *gtkwin = gtk_window_new(GTK_WINDOW_TOPLEVEL);

        gtk_widget_set_can_focus(gtkwin, TRUE);
        gtk_window_set_decorated(GTK_WINDOW(gtkwin), FALSE);
        gtk_window_set_type_hint(
            GTK_WINDOW(gtkwin), GDK_WINDOW_TYPE_HINT_POPUP_MENU);

        if (make_trans_window(global.display, gtkwin,
            Flags.Screen, Flags.AllWorkspaces, Flags.BelowAll,
            1, NULL, &xwin, &wantx, &wanty)) {
            global.Trans = 1;
            global.IsDouble = 1;
            global.Desktop = 1;
            TransA = gtkwin;
            GtkWidget *drawing_area = gtk_drawing_area_new();
            gtk_container_add(GTK_CONTAINER(TransA), drawing_area);
            g_signal_connect(TransA, "draw", G_CALLBACK(on_draw_event), NULL);

            P("calling set_below_above\n");
            set_below_above();
            global.SnowWin = xwin;

            printf(_("\nUsing transparent window\n"));
            P("wantx, wanty: %d %d\n", wantx, wanty);
            if (!strncasecmp(global.DesktopSession, "fvwm", 4) ||
                !strncasecmp(global.DesktopSession, "lxqt", 4)) {
                printf(_("The transparent snow-window is probably not "
                         "click-through, alas..\n"));
            }
        } else {
            global.Desktop = 1;
            X11cairo = 1;
            // use rootwindow, pcmanfm or Desktop:
            printf(_("Cannot create transparent window\n"));
            if (!strcasecmp(global.DesktopSession, "enlightenment")) {
                printf(_("NOTE: plasmasnow will probably run, but some glitches are "
                         "to be expected.\n"));
            }
            if (!strcasecmp(global.DesktopSession, "twm")) {
                printf("NOTE: you probably need to tweak 'Lift snow on "
                       "windows' in the 'settings' panel.\n");
            }
            // if envvar DESKTOP_SESSION == LXDE, search for window with name
            // pcmanfm largest_window_with_name uses name argument as regex,
            // hence the '^' and '$'. Note that the name is still
            // case-insensitive
            if (!strncmp(global.DesktopSession, "LXDE", 4) &&
                (xwin = largest_window_with_name(global.xdo, "^pcmanfm$"))) {
                printf(_("LXDE session found, using window 'pcmanfm'.\n"));
                P("lxdefound: %d %#lx\n", lxdefound, *xwin);
            } else if ((xwin = largest_window_with_name(
                            global.xdo, "^Desktop$"))) {
                printf(_("Using window 'Desktop'.\n"));
            } else {
                printf(_("Using root window\n"));
                xwin = global.Rootwindow;
            }
            global.SnowWin = xwin;
            int winw, winh;
            if (Flags.Screen >= 0 && global.Desktop) {
                xinerama(
                    global.display, Flags.Screen, &wantx, &wanty, &winw, &winh);
            }
        }
    }

    if (X11cairo) {
        HandleX11Cairo();

        drawit_id = add_to_mainloop1(PRIORITY_HIGH, time_draw_all, do_drawit, CairoDC);
        global.WindowOffsetX = 0;
        global.WindowOffsetY = 0;

    } else {
        global.WindowOffsetX = wantx;
        global.WindowOffsetY = wanty;
    }

    global.IsDouble = global.Trans || global.UseDouble;

    XTextProperty x;
    if (XGetWMName(global.display, xwin, &x)) {
        SnowWinName = strdup((char *) x.value);
    } else {
        SnowWinName = strdup("no name");
    }
    XFree(x.value);

    if (!X11cairo) {
        xdo_move_window(global.xdo, global.SnowWin, wantx, wanty);
        P("wantx wanty: %d %d\n", wantx, wanty);
    }

    xdo_wait_for_window_map_state(global.xdo, global.SnowWin, IsViewable);

    InitDisplayDimensions();

    global.SnowWinX = wantx;
    global.SnowWinY = wanty;
    printf(_("\nSnowing in %#lx: %s %d+%d %dx%d\n"), global.SnowWin, SnowWinName,
        global.SnowWinX, global.SnowWinY, global.SnowWinWidth,
        global.SnowWinHeight);

    PrevW = global.SnowWinWidth;
    PrevH = global.SnowWinHeight;
    printf(_("global.WindowOffsetX, global.WindowOffsetY: %d %d\n"),
        global.WindowOffsetX, global.WindowOffsetY);

    fflush(stdout);

    SetWindowScale();

    if (global.XscreensaverMode && !Flags.BlackBackground) {
        SetBackground();
    }

    return TRUE;
}

/** *********************************************************************
 ** Cairo specific. do_display_dimensions() main_c->StartWindow()
 **/
int HandleX11Cairo() {
    int resultValue;

#ifdef XDBE_AVAILABLE
    int dodouble = Flags.UseDouble;
#else
    int dodouble = 0;
#endif

    unsigned int w, h;
    xdo_get_window_size(global.xdo, global.SnowWin, &w, &h);

#ifdef XDBE_AVAILABLE
    if (dodouble) {
        static Drawable backBuf = 0;
        if (backBuf) {
            XdbeDeallocateBackBufferName(global.display, backBuf);
        }
        backBuf = XdbeAllocateBackBufferName(global.display,
            global.SnowWin, BMETHOD);

        if (CairoSurface) {
            cairo_surface_destroy(CairoSurface);
        }

        Visual *visual = DefaultVisual(global.display,
            DefaultScreen(global.display));
        CairoSurface = cairo_xlib_surface_create(global.display,
            backBuf, visual, w, h);
        printf(_("Using double buffer: %#lx. %dx%d\n"), backBuf, w, h);

        global.UseDouble = 1;
        global.IsDouble = 1;

        resultValue = TRUE;
    }
#endif

    if (!dodouble) {
        printf(_("NOT using double buffering:"));

        Visual *visual = DefaultVisual(global.display,
            DefaultScreen(global.display));
        CairoSurface = cairo_xlib_surface_create(
            global.display, global.SnowWin, visual, w, h);

        if (Flags.UseDouble) {
            printf(_(" because double buffering is not available "
                     "on this system.\n"));
        } else {
            printf(_(" on your request.\n"));
        }
        printf(_("NOTE: expect some flicker.\n"));

        fflush(stdout);
        resultValue = FALSE;
    }

    if (CairoDC) {
        cairo_destroy(CairoDC);
    }

    CairoDC = cairo_create(CairoSurface);
    cairo_xlib_surface_set_size(CairoSurface, w, h);
    global.SnowWinWidth = w;
    global.SnowWinHeight = h;

    if (Flags.Screen >= 0 && global.Desktop) {
        int winx, winy, winw, winh;
        if (xinerama(global.display, Flags.Screen,
            &winx, &winy, &winw, &winh)) {
            global.SnowWinX = winx;
            global.SnowWinY = winy;
            global.SnowWinWidth = winw;
            global.SnowWinHeight = winh;
        }

        P("clipsnow %d %d %d %d\n", global.SnowWinX, global.SnowWinY,
            global.SnowWinWidth, global.SnowWinHeight);
        cairo_rectangle(CairoDC, global.SnowWinX, global.SnowWinY,
            global.SnowWinWidth, global.SnowWinHeight);
        cairo_clip(CairoDC);
    }

    return resultValue;
}

/** *********************************************************************
 ** Set the UI Main Window Sticky Flag.
 **/
void set_sticky(int isSticky) {
    if (!global.Trans) {
        return;
    }

    mIsSticky = isSticky;
    if (mIsSticky) {
        gtk_window_stick(GTK_WINDOW(TransA));
    } else {
        gtk_window_unstick(GTK_WINDOW(TransA));
    }
}

void DoAllWorkspaces() {
    if (Flags.AllWorkspaces) {
        P("stick\n");
        set_sticky(1);
    } else {
        P("unstick\n");
        set_sticky(0);
        if (Flags.Screen >= 0) {
            global.ChosenWorkSpace = global.VisWorkSpaces[Flags.Screen];
        } else {
            global.ChosenWorkSpace = global.VisWorkSpaces[0];
        }
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
    scenery_ui();
    birds_ui();

    snow_ui();

    meteor_ui();
    wind_ui();
    stars_ui();
    doFallenSnowUISettingsUpdates();
    blowoff_ui();
    treesnow_ui();
    moon_ui();
    aurora_ui();
    updateMainWindowUI();

    UIDO(CpuLoad, HandleCpuFactor(););
    UIDO(Transparency, );
    UIDO(Scale, );
    UIDO(OffsetS, DisplayDimensions(););
    UIDO(OffsetY, updateFallenSnowRegionsWithLock(););
    UIDO(NoFluffy, ClearScreen(););
    UIDO(AllWorkspaces, DoAllWorkspaces(););
    UIDO(BelowAll, set_below_above(););
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

    if (!global.Desktop) {
        return -1;
    }

    // I(_("Refresh due to change of screen or language settings ...\n"));
    if (global.ForceRestart) {
        DoRestart = 1;
        Flags.Done = 1;
        return -1;
    }

    // P("width height: %d %d %d %d\n", w, h, global.Wroot, global.Hroot);
    // I(_("Refresh due to change of display settings...\n"));
    Display *display = XOpenDisplay(Flags.DisplayName);
    Screen *screen = DefaultScreenOfDisplay(display);

    unsigned int w = WidthOfScreen(screen);
    unsigned int h = HeightOfScreen(screen);
    if (global.Wroot != w || global.Hroot != h) {
        DoRestart = 1;
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

    XFlush(global.display);
    while (XPending(global.display)) {
        XEvent event;
        XNextEvent(global.display, &event);

        switch (event.type) {
            case ConfigureNotify:
                global.WindowsChanged++;
                if (event.xconfigure.window == global.SnowWin) {
                    mMainWindowNeedsReconfiguration = true;
                }
                break;

            case CreateNotify:
                onWindowCreated(event.xcreatewindow.window);
                break;

            case MapNotify:
                global.WindowsChanged++;
                onWindowMapped(event.xmap.window);
                break;

            case UnmapNotify:
                global.WindowsChanged++;
                onWindowUnmapped(event.xunmap.window);
                break;

            case DestroyNotify:
                onWindowDestroyed(event.xdestroywindow.window);
                break;
        }
    }

    return TRUE;
}

/** *********************************************************************
 ** This method ...
 **/
void RestartDisplay() {
    P("Restartdisplay: %d W: %d H: %d\n", global.counter++, global.SnowWinWidth,
        global.SnowWinHeight);

    fflush(stdout);
    initFallenSnowList();
    init_stars();
    EraseTrees();

    if (!Flags.NoKeepSnowOnTrees && !Flags.NoTrees) {
        reinit_treesnow_region();
    }

    if (!Flags.NoTrees) {
        cairo_region_destroy(global.TreeRegion);
        global.TreeRegion = cairo_region_create();
    }

    if (!global.IsDouble) {
        ClearScreen();
    }
}

/** *********************************************************************
 ** This method ...
 **/
int do_testing() {
    if (Flags.Done) {
        return FALSE;
    }
    return TRUE;

    int xret, yret;
    unsigned int wret, hret;

    xdo_get_window_location(global.xdo, global.SnowWin, &xret, &yret, NULL);
    xdo_get_window_size(global.xdo, global.SnowWin, &wret, &hret);

    P("%d wxh %d %d %d %d    %d %d %d %d \n", global.counter++, global.SnowWinX,
        global.SnowWinY, global.SnowWinWidth, global.SnowWinHeight, xret, yret,
        wret, hret);
    P("WorkspaceActive, chosen: %d %ld\n", WorkspaceActive(),
        global.ChosenWorkSpace);
    P("vis:");

    for (int i = 0; i < global.NVisWorkSpaces; i++) {
        printf(" %ld", global.VisWorkSpaces[i]);
    }
    printf("\n");

    printf("offsets: %d %d\n", global.WindowOffsetX, global.WindowOffsetY);
    global.counter++;
    for (int i = 0; i < global.NVisWorkSpaces; i++) {
        printf("%d: visible: %d %ld\n", global.counter, i, global.VisWorkSpaces[i]);
    }

    P("current workspace: %ld\n", global.CWorkSpace);
    return TRUE;
}

/** *********************************************************************
 ** This method ...
 **/
void SigHandler(int signum) {
    global.HaltedByInterrupt = signum;
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

    // If we notice too many X11 errors, print any global.Message
    // and set app termination flag.
    msg[60] = '\x0'; fprintf(stdout, "%s", msg);
    if (++mX11Error_Count > mX11Error_maxBeforeTermination) {
        snprintf(global.Message, sizeof(global.Message),
            _("More than %d errors, I quit!"), mX11Error_maxBeforeTermination);
        Flags.Done = 1;
    }

    return 0;
}

/** *********************************************************************
 ** This method ...
 **/
// the draw callback
gboolean on_draw_event(__attribute__((unused)) GtkWidget *widget,
    cairo_t *cr, __attribute__((unused)) gpointer user_data) {
    drawit(cr);
    return FALSE;
}

int do_drawit(void *cr) {
    P("do_drawit %d %p\n", global.counter++, cr);
    drawit((cairo_t *)cr);
    return TRUE;
}

void drawit(cairo_t *cr) {
    static int counter = 0;
    P("drawit %d %p\n", global.counter++, (void *)cr);
    // Due to instabilities at the start of app: spurious detection of
    //   user intervention; resizing of screen; etc.
    //   placement of scenery, stars etc is repeated a few times at the
    //   start of app.
    // This is not harmful, but a bit annoying, so we do not draw anything
    //   the first few times this function is called.
    if (counter * time_draw_all < 1.5) {
        counter++;
        return;
    }

    if (Flags.Done) {
        return;
    }

    if (global.UseDouble) {
        XdbeSwapInfo swapInfo;
        swapInfo.swap_window = global.SnowWin;
        swapInfo.swap_action = BMETHOD;
        XdbeSwapBuffers(global.display, &swapInfo, 1);
    } else if (!global.IsDouble) {
        XFlush(global.display);
        moon_erase(0);
        Santa_erase(cr);
        stars_erase(); // not really necessary
        birds_erase(0);
        snow_erase(1);
        aurora_erase();
        XFlush(global.display);
    }

    /* clear snowwindow test */
    /*
       cairo_set_source_rgba(cr, 0, 0, 0, 1);
       cairo_set_line_width(cr, 1);

       cairo_rectangle(cr, 0, 0, global.SnowWinWidth, global.SnowWinHeight);
       cairo_fill(cr);
       */

    cairo_save(cr);
    int tx = 0;
    int ty = 0;
    if (X11cairo) {
        tx = global.SnowWinX;
        ty = global.SnowWinY;
    }
    P("tx ty: %d %d\n", tx, ty);
    cairo_translate(cr, tx, ty);

    int skipit = !WorkspaceActive();

    if (!skipit) {
        stars_draw(cr);

        P("moon\n");
        moon_draw(cr);
        aurora_draw(cr);
        meteor_draw(cr);
        scenery_draw(cr);
    }

    P("birds %d\n", counter++);
    birds_draw(cr);

    if (skipit) {
        goto end;
    };

    fallensnow_draw(cr);

    if (!Flags.FollowSanta ||
        !Flags.ShowBirds) { // if Flags.FollowSanta: drawing of Santa is done in
                            // birds_draw()
        Santa_draw(cr);
    }

    treesnow_draw(cr);

    snow_draw(cr);

end:
    if (Flags.Outline) {
        rectangle_draw(cr);
    }

    cairo_restore(cr);

    XFlush(global.display);
}


void SetWindowScale() {
    // assuming a standard screen of 1024x576, we suggest to use the scalefactor
    // WindowScale
    float x = global.SnowWinWidth / 1000.0;
    float y = global.SnowWinHeight / 576.0;

    if (x < y) {
        global.WindowScale = x;
    } else {
        global.WindowScale = y;
    }
    P("WindowScale: %f\n", global.WindowScale);
}

/** *********************************************************************
 ** This method ...
 **/
int do_display_dimensions() {
    if (Flags.Done) {
        return FALSE;
    }

    if (!mMainWindowNeedsReconfiguration) {
        return TRUE;
    }
    mMainWindowNeedsReconfiguration = false;

    P("%d do_display_dimensions %dx%d %dx%d\n", global.counter++,
        global.SnowWinWidth, global.SnowWinHeight, PrevW, PrevH);
    DisplayDimensions();

    if (!global.Trans) {
        HandleX11Cairo();
    }

    if (PrevW != global.SnowWinWidth || PrevH != global.SnowWinHeight) {
        printf(_("Window size changed, now snowing in in %#lx: %s %d+%d %dx%d\n"),
            global.SnowWin, SnowWinName, global.SnowWinX, global.SnowWinY,
            global.SnowWinWidth, global.SnowWinHeight);

        P("Calling RestartDisplay\n");
        RestartDisplay();

        PrevW = global.SnowWinWidth;
        PrevH = global.SnowWinHeight;
        SetWindowScale();
    }

    fflush(stdout);
    return TRUE;
}

/** *********************************************************************
 ** This method ...
 **/
int do_draw_all(gpointer widget) {
    if (Flags.Done) {
        return FALSE;
    }
    P("do_draw_all %d %p\n", global.counter++, (void *)widget);

    // this will result in a call off on_draw_event():
    gtk_widget_queue_draw(GTK_WIDGET(widget));
    return TRUE;
}

/** *********************************************************************
 ** This method ...
 **/
// handle callbacks for things whose timings depend on cpufactor
void HandleCpuFactor() {
    P("handlecpufactor %f %f %d\n", oldcpufactor, cpufactor, counter++);

    // re-add things whose timing is dependent on cpufactor
    if (Flags.CpuLoad <= 0) {
        global.cpufactor = 1;
    } else {
        global.cpufactor = 100.0 / Flags.CpuLoad;
    }

    add_to_mainloop(PRIORITY_HIGH, time_init_snow, do_initsnow);

    restart_do_draw_all();
}

/** *********************************************************************
 ** This method ...
 **/
void restart_do_draw_all() {
    if (global.Trans) {
        if (draw_all_id) {
            g_source_remove(draw_all_id);
        }
        draw_all_id =
            add_to_mainloop1(PRIORITY_HIGH, time_draw_all, do_draw_all, TransA);
        P("started do_draw_all %d %p %f\n", draw_all_id, (void *) TransA,
            time_draw_all);
    } else {
        if (drawit_id) {
            g_source_remove(drawit_id);
        }
        drawit_id =
            add_to_mainloop1(PRIORITY_HIGH, time_draw_all, do_drawit, CairoDC);
        P("started do_drawit %d %p %f\n", drawit_id, (void *)CairoDC,
            time_draw_all);
    }
}

/** *********************************************************************
 ** This method ...
 **/
void rectangle_draw(cairo_t *cr) {
    const int lw = 8;
    cairo_save(cr);
    cairo_set_source_rgba(cr, 1, 1, 0, 0.5);
    cairo_rectangle(cr, lw / 2, lw / 2, global.SnowWinWidth - lw,
        global.SnowWinHeight - lw);
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
    global.Language = guess_language();
#ifdef HAVE_GETTEXT
    // One would assume that gettext() uses the environment variable LOCPATH
    // to find locales, but no, it does not.
    // So, we scan LOCPATH for paths, use them and see if gettext gets a
    // translation for a text that is known to exist in the ui and should be
    // translated to something different.

    char *startlocale;
    (void)startlocale;

    char *r = setlocale(LC_ALL, "");
    if (r) {
        startlocale = strdup(r);
    } else {
        startlocale = NULL;
    }
    P("startlc: %s\n", startlocale);
    // if (startlocale == NULL)
    //   startlocale = strdup("en_US.UTF-8");
    P("startlc: %s\n", startlocale);
    P("LC_ALL: %s\n", getenv("LC_ALL"));
    P("LANG: %s\n", getenv("LANG"));
    P("LANGUAGE: %s\n", getenv("LANGUAGE"));
    textdomain(TEXTDOMAIN);

    char *textd = bindtextdomain(TEXTDOMAIN, LOCALEDIR);
    (void)textd;
    P("textdomain:%s\n", textd);

    char *locpath = getenv("LOCPATH");
    P("LOCPATH: %s\n", locpath);
    P("LOCALEDIR: %s\n", LOCALEDIR);
    if (locpath) {
        char *initial_textdomain = strdup(bindtextdomain(TEXTDOMAIN, NULL));
        char *mylocpath = strdup(locpath);
        int translation_found = False;
        P("Initial textdomain: %s\n", initial_textdomain);

        char *q = mylocpath;
        while (1) {
            char *p = strsep(&q, ":");
            if (!p) {
                break;
            }
            bindtextdomain(TEXTDOMAIN, p);
            // char *trans = _(TESTSTRING);
            P("trying '%s' '%s' '%s'\n", p, TESTSTRING, trans);
            if (strcmp(_(TESTSTRING), TESTSTRING)) {
                // translation found, we are happy
                P("Translation: %s\n", trans);
                translation_found = True;
                break;
            }
        }
        if (!translation_found) {
            P("No translation found in %s\n", locpath);
            bindtextdomain(TEXTDOMAIN, initial_textdomain);
        }
        free(mylocpath);
        free(initial_textdomain);
    }

    if (0) {
        if (strcmp(Flags.Language, "sys")) // if "sys" , do not change locale
        {                                  // Language != sys
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
        } else // Language == sys
        {
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
        // setenv("LANG","nl_NL.UTF8",1);
        // setenv("LANGUAGE","nl",1);
    }

    P("textdomain: %s\n", textdomain(NULL));
    P("bindtextdomain: %s\n", bindtextdomain(TEXTDOMAIN, NULL));
    // bind_textdomain_codeset(TEXTDOMAIN,"UTF-8");

#endif
}
