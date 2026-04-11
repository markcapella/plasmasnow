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
// Std C Lib headers.
#include <ctype.h>
#include <fcntl.h>
#include <malloc.h>
#include <math.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
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
#include "PlasmaSnow.h"

#include "Application.h"
#include "birds.h"
#include "Blowoff.h"
#include "ClockHelper.h"
#include "ColorCodes.h"
#include "ColorPicker.h"
#include "docs.h"
#include "dsimple.h"
#include "FallenSnow.h"
#include "Flags.h"
#include "Lights.h"
#include "LoadMeasure.h"
#include "MainWindow.h"
#include "meteor.h"
#include "moon.h"
#include "mygettext.h"
#include "safe_malloc.h"
#include "Santa.h"
#include "scenery.h"
#include "selfrep.h"
#include "Storm.h"
#include "Stars.h"
#include "StormItem.h"
#include "StormWindow.h"
#include "treesnow.h"
#include "WinInfo.h"
#include "version.h"
#include "wind.h"
#include "Windows.h"
#include "Utils.h"
#include "vroot.h"
#include "xdo.h"


/** ************************************************
 ** Module globals and consts.
 **/
const bool ARE_WE_DEBUGGING = false;

struct _mGlobal mGlobal;

cairo_t* mCairoWindow = NULL;
cairo_surface_t* mCairoSurface = NULL;

int mIsSticky = 0;
int mPrevSnowWinWidth = 0;
int mPrevSnowWinHeight = 0;

int mX11MaxErrorCount = 500;
int mX11ErrorCount = 0;
int mX11LastErrorCode = 0;

bool mMainWindowNeedsReconfiguration = true;

bool mDoRestartDueToDisplayChange = 0;
char** mRestartArgs;


/** ************************************************
 ** Application linker entry.
 **/
int main(int argc, char *argv[]) {
    if (!canStartApplication(argc, argv)) {
        return false;
    }

    stopApplication();
    return false;
}

/** ************************************************
 ** Main application start.
 **/
int canStartApplication(int argc, char *argv[]) {
    signal(SIGINT, appShutdownHook);
    signal(SIGTERM, appShutdownHook);
    signal(SIGHUP, appShutdownHook);
    signal(SIGKILL, appShutdownHook);

    // Seed random.
    srand48((int) (fmod(getWallClockReal() * 1.0e6, 1.0e8)));
    initPlasmaSnowGlobal();

    // Inits.
    XInitThreads();
    initFallenSnowSemaphores();
    birds_sem_init();

    InitFlags();
    for (int i = 0; i < argc; i++) {
        char *arg = argv[i];

        if (!strcmp(arg, "-h") || !strcmp(arg, "-help")) {
            docs_usage(0);
            return false; // Done.
        }

        if (!strcmp(arg, "-H") || !strcmp(arg, "-manpage")) {
            docs_usage(1);
            return false; // Done.
        }

        if (!strcmp(arg, "-v") || !strcmp(arg, "-version")) {
            logAppVersion();
            return false; // Done.

        } else if (!strcmp(arg, "-changelog")) {
            displayPlasmaSnowDocumentation();
            return false; // Done.

        }
        #ifdef SELFREP
        else if (!strcmp(arg, "-selfrep")) {
            selfrep();
            return false; // Done.

        }
        #endif
    }

    int rc = HandleFlags(argc, argv);
    handle_language(0);
    mybindtestdomain();

    switch (rc) {
        case -1: // wrong flag
            clearColorPicker();
            return true; // Error.
            break;

        case 1: // manpage or help
            return false;
            break;

        default:
            break;
    }

    // Make a copy of all flags, before gtk_init() removes some.
    // We need this at app refresh. remove: -screen n -lang c.
    mRestartArgs = (char**) malloc((argc + 1) * sizeof(char**));

    int newCount = 0;
    for (int i = 0; i < argc; i++) {
        if ((strcmp("-screen", argv[i]) == 0) ||
            (strcmp("-lang", argv[i]) == 0)) {
            i++;
        } else {
            mRestartArgs[newCount++] = strdup(argv[i]);
        }
    }

    mRestartArgs[newCount] = NULL;

    // Log info, version checks.
    logAppVersion();

    printf("Available languages are:\n%s.\n\n", LANGUAGES);
    printf("GTK version : %s\n", ui_gtk_version());
    printf("GTK required: %s\n\n", ui_gtk_required());

    if (!isGtkVersionValid()) {
        printf("%splasmasnow: GTK Version is insufficient "
            "- FATAL.%s\n", COLOR_RED, COLOR_NORMAL);
        return true; // Error!
    }

    printf("%splasmasnow: Desktop %s detected.%s\n\n", 
        COLOR_BLUE, getDesktopSession() ?
            getDesktopSession() : "was not", COLOR_NORMAL);

    // Init GTK & x11 backend, ensure valid version.
    setenv("GDK_BACKEND", "x11", 1);
    gtk_init(&argc, &argv);

    // Write current flags set with any user
    // changes from this run.
    if (!Flags.NoConfig) {
        WriteFlags();
    }

    // Check for session error.
    const char* SESSION_TYPE = getenv("XDG_SESSION_TYPE");
    if (strcmp(SESSION_TYPE, "x11") != 0) {
        printf("%splasmasnow: No X11 Session type "
            "is detected, FATAL.%s\n",
            COLOR_RED, COLOR_NORMAL);

        printf("%splasmasnow: Environment var "
            "$XDG_SESSION_TYPE: |%s|.%s\n",
            COLOR_YELLOW, SESSION_TYPE, COLOR_NORMAL);
        return true; // Error.
    }

    // Check for display error.
    const char* WAYLAND_DISPLAY = getenv("WAYLAND_DISPLAY");
    if (WAYLAND_DISPLAY && strlen(WAYLAND_DISPLAY) > 0) {
        const char* TEMP = WAYLAND_DISPLAY ?
            WAYLAND_DISPLAY : "";
        printf("%splasmasnow: Wayland Display Manager "
            "is detected, FATAL.%s\n",
            COLOR_RED, COLOR_NORMAL);

        printf("%splasmasnow: Environment var "
            "$WAYLAND_DISPLAY: |%s|.%s\n",
            COLOR_YELLOW, SESSION_TYPE, COLOR_NORMAL);
        return true; // Error.
    }

    mGlobal.display = XOpenDisplay(Flags.DisplayName);
    if (mGlobal.display == NULL) {
        printf("%splasmasnow: X11 Does not seem to be "
            "available (Are you Wayland?) - FATAL.%s\n",
            COLOR_RED, COLOR_NORMAL);
        return true; // Error.
    }
    mGlobal.Screen = DefaultScreen(mGlobal.display);

    XSynchronize(mGlobal.display, ARE_WE_DEBUGGING);
    XSetErrorHandler(handleX11ErrorEvent);

    // Ensure XDO Access also.
    mGlobal.xdo = xdo_new_with_opened_display(
        mGlobal.display, NULL, 0);
    if (mGlobal.xdo == NULL) {
        printf("plasmasnow: XDO reports no displays - FATAL.\n");
        XCloseDisplay(mGlobal.display);
        return true; // Error.
    }
    mGlobal.xdo->debug = ARE_WE_DEBUGGING;

    // Default any colors a user may have set in .plasmasnowrc.
    int wasThereAnInvalidColor = false;
    if (!ValidColor(Flags.StormItemColor1)) {
        Flags.StormItemColor1 = strdup(
            DefaultFlags.StormItemColor1);
        wasThereAnInvalidColor = true;
    }
    if (!ValidColor(Flags.StormItemColor2)) {
        Flags.StormItemColor2 = strdup(
            DefaultFlags.StormItemColor2);
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

    // Start Storming.
    populateWinInfoHelper();

    if (!canStartStormWindow()) {
        printf("%splasmasnow: Could not find a Window Manager "
           "with an active window compositor - FATAL.%s\n",
           COLOR_RED, COLOR_NORMAL);
        XCloseDisplay(mGlobal.display);
        return true; // Error.
    }

    setOldFlagsFromFlags();
    OldFlags.FullScreen = !Flags.FullScreen;

    // Request all interesting X11 events.
    const Window EVENT_WINDOW = (mGlobal.hasDestopWindow) ?
        mGlobal.Rootwindow : mGlobal.SnowWin;
    XSelectInput(mGlobal.display, EVENT_WINDOW,
        StructureNotifyMask | SubstructureNotifyMask |
        FocusChangeMask | ButtonPressMask);

    clearGlobalSnowWindow();
    if (!Flags.NoMenu && !mGlobal.XscreensaverMode) {
        createMainWindow();
        ui_set_sticky(Flags.AllWorkspaces);
    }

    // Init app modules & log window status.
    Flags.shutdownRequested = 0;
    addWindowsModuleToMainloop();
    initStormModule();

    // Init other modules.
    initFallenSnowModule();
    initBlowoffModule();
    wind_init();
    Santa_init();
    initLightsModule();
    InitSnowOnTrees();
    treesnow_init();
    initSceneryModule();
    birds_init();
    initStarsModule();
    initMeteorModule();
    moon_init();

    startLoadMeasureBackgroundThread();

    addMethodToMainloop(PRIORITY_DEFAULT, time_displaychanged,
        (GSourceFunc) onTimerEventDisplayChanged);

    addMethodToMainloop(PRIORITY_DEFAULT, time_display_dimensions,
        (GSourceFunc) handleDisplayConfigurationChange);

    addMethodToMainloop(PRIORITY_DEFAULT, CONFIGURE_WINDOW_EVENT_TIME,
        (GSourceFunc) handlePendingX11Events);

    addMethodToMainloop(PRIORITY_HIGH, TIME_BETWEEEN_UI_SETTINGS_UPDATES,
        (GSourceFunc) doAllUISettingsUpdates);

    if (Flags.StopAfter > 0) {
        addMethodToMainloop(PRIORITY_DEFAULT, Flags.StopAfter,
            (GSourceFunc) do_stopafter);
    }

    HandleCpuFactor();
    connectStormWindowDraw();
    respondToWorkspaceSettingsChange();

    printf("%s\nplasmasnow: Starts.%s\n",
        COLOR_BLUE, COLOR_NORMAL);

    gtk_main();

    printf("%s\nplasmasnow: Finishes.%s\n",
        COLOR_BLUE, COLOR_NORMAL);

    // Uninit & done.
    removeFallenSnowFromAllWindows();
    clearColorPicker();
    uninitLightsModule();

    XClearWindow(mGlobal.display, mGlobal.SnowWin);
    XFlush(mGlobal.display);
    XCloseDisplay(mGlobal.display);

    return true;
}

/** ************************************************
 ** Main application stop.
 **/
void stopApplication() {
    printf("%s\nThanks for using plasmasnow, you rock !%s\n",
        COLOR_GREEN, COLOR_NORMAL);

    // If Restarting due to display change.
    if (mDoRestartDueToDisplayChange) {
        sleep(0);
        setenv("plasmasnow_RESTART", "yes", 1);
        execvp(mRestartArgs[0], mRestartArgs);
    }
}

/**
 * Memset and init all mGlobal struct fields.
 */
void initPlasmaSnowGlobal() {
    memset(&mGlobal, 0, sizeof(mGlobal));

    mGlobal.mPlasmaWindowTitle = "plasmasnow";

    mGlobal.cpufactor = 1.0;
    mGlobal.WindowScale = 1.0;

    mGlobal.MaxFlakeHeight = 0;
    mGlobal.MaxFlakeWidth = 0;

    mGlobal.stormItemCount = 0;
    mGlobal.FluffCount = 0;

    mGlobal.SnowWin = None;
    mGlobal.WindowOffsetX = 0;
    mGlobal.WindowOffsetY = 0;

    mGlobal.currentWS = 0;
    mGlobal.ChosenWorkSpace = 0;
    mGlobal.visualWSCount = 1;
    mGlobal.visualWSList[0] = 0;
    mGlobal.WindowsChanged = 0;
    mGlobal.ForceRestart = 0;
    mGlobal.MaxScrSnowDepth = 0;

    mGlobal.winInfoListLength = 0;
    mGlobal.winInfoList = NULL;

    mGlobal.Wind = 0;
    mGlobal.Direction = 0;
    mGlobal.WindMax = 500.0;
    mGlobal.NewWind = 100.0;

    mGlobal.FsnowFirst = NULL;

    mGlobal.SantaPlowRegion = 0;
    mGlobal.SnowOnTrees = NULL;
    mGlobal.OnTrees = 0;
    mGlobal.RemoveFluff = 0;

    mGlobal.moonX = 1000;
    mGlobal.moonY = 80;
}

/** ************************************************
 ** This method gets the desktop session type from env vars.
 **/
char* getDesktopSession() {
    const char* DESKTOPS[] = {
        "DESKTOP_SESSION", "XDG_SESSION_DESKTOP",
        "XDG_CURRENT_DESKTOP", "GDMSESSION", NULL};

    char* desktopsession;
    for (int i = 0; DESKTOPS[i]; i++) {
        desktopsession = getenv(DESKTOPS[i]);
        if (desktopsession) {
            break;
        }
    }

    return desktopsession;
}

/**
 * ...
 */
cairo_t* getCairoWindow() {
    return mCairoWindow;
}

/** ************************************************
 ** Cairo specific.
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

        cairo_rectangle(mCairoWindow,
            mGlobal.SnowWinX, mGlobal.SnowWinY,
            mGlobal.SnowWinWidth, mGlobal.SnowWinHeight);
        cairo_clip(mCairoWindow);
    }
}

/** ************************************************
 ** TODO:
 **/
void respondToWorkspaceSettingsChange() {
    if (Flags.AllWorkspaces) {
        setStormWindowStickyState(true);
    } else {
        setStormWindowStickyState(false);

        mGlobal.ChosenWorkSpace = (Flags.Screen >= 0) ?
            mGlobal.visualWSList[Flags.Screen] :
            mGlobal.visualWSList[0];
    }

    ui_set_sticky(Flags.AllWorkspaces);
}

/** ************************************************
 * here we are handling the buttons in ui
 * Ok, this is a long list, and could be implemented more efficient.
 * But, doAllUISettingsUpdates is called not too frequently, so....
 * Note: if changes != 0, the settings will be written to .plasmasnowrc
 **/
int doAllUISettingsUpdates() {
    if (Flags.shutdownRequested) {
        gtk_main_quit();
    }

    if (Flags.NoMenu) {
        return true;
    }

    respondToStormSettingsChanges();
    respondToBlowoffSettingsChanges();
    respondToFallenSnowSettingsChanges();
    respondToScenerySettingsChanges();
    respondToStarsSettingsChanges();
    respondToMeteorSettingsChanges();

    Santa_ui();
    birds_ui();
    wind_ui();
    respondToTreesnowSettingsChanges();
    respondToMoonSettingsChanges();
    updateMainWindowUI();

    // updateAdvancedUserSettings.
    UIDO(CpuLoad, HandleCpuFactor(););
    UIDO(Transparency, );
    UIDO(Scale, );
    UIDO(OffsetS, updateDisplayDimensions(););
    UIDO(OffsetY, lockFallenSnowBaseSemaphore();
        doAllFallenSnowWinInfoUpdates();
        unlockFallenSnowBaseSemaphore(););
    UIDO(AllWorkspaces, respondToWorkspaceSettingsChange(););
    UIDOS(BackgroundFile, );
    UIDO(BlackBackground, );

    // Write flags prefs if they've changed.
    if (Flags.Changes > 0) {
        WriteFlags();
        set_buttons();
        Flags.Changes = 0;
    }

    return true;
}

/** ************************************************
 * If we are snowing in the desktop, we check if the size has changed,
 * this can happen after changing of the displays settings
 * If the size has been changed, we refresh the app.
 **/
int onTimerEventDisplayChanged() {
    if (Flags.shutdownRequested) {
        return -1;
    }

    if (!mGlobal.hasDestopWindow) {
        return -1;
    }

    if (mGlobal.ForceRestart) {
        mDoRestartDueToDisplayChange = 1;
        Flags.shutdownRequested = 1;
        return -1;
    }

    Display *display = XOpenDisplay(Flags.DisplayName);
    Screen *screen = DefaultScreenOfDisplay(display);

    unsigned int w = WidthOfScreen(screen);
    unsigned int h = HeightOfScreen(screen);
    if (mGlobal.Wroot != w || mGlobal.Hroot != h) {
        mDoRestartDueToDisplayChange = 1;
        Flags.shutdownRequested = 1;
    }

    XCloseDisplay(display);
    return -1;
}

/** ************************************************
 ** This method handles xlib11 event notifications.
 ** Undergoing heavy renovation 2024 Rocks !
 **
 ** https://www.x.org/releases/current/doc/
 **     libX11/libX11/libX11.html#Events
 **/
int handlePendingX11Events() {
    if (Flags.shutdownRequested) {
        return false;
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
                onConfigureNotify(&event);
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

            case ClientMessage:
                onWindowClientMessage(&event);
                break;

            case ButtonPress:
                if (!isColorPickerActive()) {
                    break;
                }

                Window root_return, child_return;
                int root_x_return, root_y_return;
                int xPosResult, yPosResult;
                unsigned int pointerState;
                if (XQueryPointer(mGlobal.display,
                    DefaultRootWindow(mGlobal.display), &root_return,
                    &child_return, &root_x_return, &root_y_return,
                    &xPosResult, &yPosResult, &pointerState)) {

                    XImage* windowImage = XGetImage(mGlobal.display,
                        root_return, root_x_return, root_y_return,
                        1, 1, XAllPlanes(), XYPixmap);
                    if (windowImage) {
                        XColor c;
                        c.pixel = XGetPixel(windowImage, 0, 0);
                        XQueryColor(mGlobal.display,
                            DefaultColormap(mGlobal.display,
                                DefaultScreen (mGlobal.display)), &c);
                        setColorPickerResultRed(c.red / 256);
                        setColorPickerResultGreen(c.green / 256);
                        setColorPickerResultBlue(c.blue / 256);
                        setColorPickerResultAlpha(0);
                        XFree(windowImage);
                    }
                }
                setColorPickerResultAvailable(true);
                XUngrabPointer(mGlobal.display, CurrentTime);
                break;

            default:
                break;
        }
    }

    return true;
}

/** ************************************************
 ** This method ...
 **/
void RestartDisplay() {
    fflush(stdout);

    clearAllFallenSnowItems();

    initStarsModuleArrays();
    onLightsScreenSizeChanged();
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

/** ************************************************
 ** This method logs signal event shutdowns as fyi.
 **/
void appShutdownHook(int signalNumber) {
    printf("%splasmasnow: Signal Handler Shutdown "
        "by : [ %i ].%s\n", COLOR_YELLOW,
        signalNumber, COLOR_NORMAL);

    Flags.shutdownRequested = true;
    stopApplication();
}

/** ************************************************
 ** This method traps and handles X11 errors.
 **
 ** Primarily, we close the app if the system
 ** doesn't seem sane, & silence x11 errors that
 ** are handled by the app.
 **/
int handleX11ErrorEvent(Display* dpy, XErrorEvent* event) {
    // Save error & quit early if simply BadWindow.
    mX11LastErrorCode = event->error_code;
    if (mX11LastErrorCode == BadWindow ||
        mX11LastErrorCode == BadAccess ||
        mX11LastErrorCode == BadMatch) {
        return 0;
    }

    // Print the error message of the event.
    const int MAX_MESSAGE_BUFFER_LENGTH = 60;
    char msg[MAX_MESSAGE_BUFFER_LENGTH];
    XGetErrorText(dpy, event->error_code, msg, sizeof(msg));
    printf("%splasmasnow::Application handleX11ErrorEvent() %s.%s\n",
        COLOR_RED, msg, COLOR_NORMAL);

    // Halt after too many errors.
    if (mX11ErrorCount++ > mX11MaxErrorCount) {
        printf("\n%splasmasnow: Shutting down due to excessive "
            "X11 errors.%s\n", COLOR_RED, COLOR_NORMAL);
        Flags.shutdownRequested = true;
    }

    return 0;
}

/** ************************************************
 ** This method id the draw callback.
 **/
gboolean handleStormWindowDrawEvents(
    __attribute__((unused)) GtkWidget* widget, cairo_t* cc,
    __attribute__((unused)) gpointer user_data) {

    drawCairoWindowInternal(cc);
    return false;
}

/** ************************************************
 ** This method ...
 **/
int drawCairoWindow(void* cc) {
    drawCairoWindowInternal((cairo_t*) cc);
    return true;
}

/** ************************************************
 ** Due to instabilities at the start of app, stars is
 ** repeated a few times. This is not harmful. We do not draw
 ** anything the first few times this function is called.
 **/
void drawCairoWindowInternal(cairo_t* cc) {
    // Instabilities (?).
    static int counter = 0;
    if (counter * time_draw_all < 1.5) {
        counter++;
        return;
    }
    if (Flags.shutdownRequested) {
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
        eraseStarsFrame();
        moon_erase(0);
        eraseLightsFrame();

        removeAllStormItems();
        Santa_erase(cc);
        birds_erase(0);
        XFlush(mGlobal.display);
    }

    // Do cairo.
    cairo_save(cc);

    int tx = 0;
    int ty = 0;
    cairo_translate(cc, tx, ty);

    // Do all module draws.
    if (isWorkspaceActive()) {
        drawStarsFrame(cc);
        drawMeteorFrame(cc);
        moon_draw(cc);

        drawLowerLightsFrame(cc);
        drawSceneryFrame(cc);
        treesnow_draw(cc);
        drawAllStormItems(cc);

        // If Flags.FollowSanta, Santa drawn in Birds.
        if (!Flags.ShowBirds || !Flags.FollowSanta) {
            Santa_draw(cc);
        }
        birds_draw(cc);

        drawFallenSnowFrame(cc);
        drawUpperLightsFrame(cc);
    }

    // Draw app window outline.
    if (Flags.Outline) {
        rectangle_draw(cc);
    }

    cairo_restore(cc);
    XFlush(mGlobal.display);
}

/** ************************************************
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
}

/**
 * Helper method gets the X11 stacked windows list.
 * Window[0] is desktop.
 */
unsigned long
getX11StackedWindowsList(Window** windows) {
    return getRootWindowProperty(XInternAtom(mGlobal.display,
        "_NET_CLIENT_LIST_STACKING", False), windows);
}

/**
 * Helper method gets a requested root window property.
 */
unsigned long
getRootWindowProperty(Atom property, Window** windows) {
    Atom da;
    int di;
    unsigned long len;
    unsigned long dl;
    unsigned char* list;

    if (XGetWindowProperty(mGlobal.display, DefaultRootWindow(
        mGlobal.display), property, 0L, 1024, False, XA_WINDOW,
        &da, &di, &len, &dl, &list) != Success) {
        *windows = NULL;
        return 0;
    }

    *windows = (Window*) list;
    return len;
}

/** ************************************************
 ** This method ...
 **/
int handleDisplayConfigurationChange() {
    if (Flags.shutdownRequested) {
        return false;
    }

    if (!mMainWindowNeedsReconfiguration) {
        return true;
    }
    mMainWindowNeedsReconfiguration = false;

    if (!mGlobal.hasStormWindow) {
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
    return true;
}

/** ************************************************
 ** This method ...
 **/
int drawStormWindow(gpointer widget) {
    if (Flags.shutdownRequested) {
        return false;
    }

    // this will result in a call off handleStormWindowDrawEvents():
    gtk_widget_queue_draw(GTK_WIDGET(widget));
    return true;
}

/** ************************************************
 ** This method handles callbacks for cpufactor
 **/
void HandleCpuFactor() {
    if (Flags.CpuLoad <= 0) {
        mGlobal.cpufactor = 1;
    } else {
        mGlobal.cpufactor = 100.0 / Flags.CpuLoad;
    }

    addMethodToMainloop(PRIORITY_HIGH, time_init_snow,
        (GSourceFunc) stallCreatingStormItems);
}

/** ************************************************
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

/** ************************************************
 ** This method ...
 **/
int do_stopafter() {
    Flags.shutdownRequested = 1;
    printf(_("Halting because of flag %s\n"), "-stopafter");

    return false;
}

/**
 * One would assume that gettext() uses the environment
 * variable LOCPATH to find locales, but no, it does not.
 * So, we scan LOCPATH for paths, use them and see if gettext
 * gets a translation for a text that is known to exist in
 * the ui and should be translated to something different.
 */
void mybindtestdomain() {
    mGlobal.Language = guess_language();

#ifdef HAVE_GETTEXT
    char* startlocale;
    (void) startlocale;
    char* resultLocale = setlocale(LC_ALL, "");
    startlocale = strdup(resultLocale ? resultLocale : NULL);

    textdomain(TEXTDOMAIN);
    bindtextdomain(TEXTDOMAIN, LOCALEDIR);

    char* locpath = getenv("LOCPATH");
    if (locpath) {
        char *initial_textdomain = strdup(
            bindtextdomain(TEXTDOMAIN, NULL));
        char *mylocpath = strdup(locpath);
        bool translation_found = false;

        char *q = mylocpath;
        while (true) {
            char *p = strsep(&q, ":");
            if (!p) {
                break;
            }

            bindtextdomain(TEXTDOMAIN, p);
            if (strcmp(_(TESTSTRING), TESTSTRING)) {
                translation_found = true;
                break;
            }
        }

        if (!translation_found) {
            bindtextdomain(TEXTDOMAIN, initial_textdomain);
        }
        free(mylocpath);
        free(initial_textdomain);
    }
#endif
}

/** ************************************************
 ** This method checks if the desktop is currently visible.
 **/
bool isDesktopVisible() {
    bool result = true;

    Atom type;
    int format;
    unsigned long nitems, unusedBytes;
    unsigned char *properties = NULL;

    XGetWindowProperty(mGlobal.display, mGlobal.Rootwindow,
        XInternAtom(mGlobal.display, "_NET_SHOWING_DESKTOP", False),
        0, (~0L), False, AnyPropertyType, &type, &format,
        &nitems, &unusedBytes, &properties);

    if (format == 32 && nitems >= 1) {
        if (*(long *) (void *) properties == 1) {
            result = false;
        }
    }
    XFree(properties);

    return result;
}

/** ************************************************
 ** This method returns the number of the current workspace,
 ** where the OS allows multiple / virtual workspaces.
 **/
long int getCurrentWorkspace() {
    Atom type;
    int format;
    unsigned long nitems, unusedBytes;
    unsigned char *properties;

    if (mGlobal.IsCompiz) {
        properties = NULL;
        XGetWindowProperty(mGlobal.display,
            DefaultRootWindow(mGlobal.display),
            XInternAtom(mGlobal.display, "_NET_DESKTOP_VIEWPORT", False),
            0, 2, False, AnyPropertyType, &type,
            &format, &nitems, &unusedBytes, &properties);

        // Set result code.
        int resultCode = -1;
        if (type == XA_CARDINAL && nitems == 2) {
            resultCode = ((long *) (void *) properties)[0] +
                (((long *) (void *) properties)[1] << 16);
        }

        XFree(properties);
        return resultCode;
    }

    // In Wayland, the actual number of current workspace can only
    // be obtained if user has done some workspace-switching
    // we return zero if the workspace number cannot be determined.
    XGetWindowProperty(mGlobal.display,
        DefaultRootWindow(mGlobal.display),
        XInternAtom(mGlobal.display, "_NET_CURRENT_DESKTOP", False),
        0, 1, False, AnyPropertyType, &type, &format, &nitems,
        &unusedBytes, &properties);

    if (type != XA_CARDINAL) {
        XFree(properties);
        XGetWindowProperty(mGlobal.display,
            DefaultRootWindow(mGlobal.display),
            XInternAtom(mGlobal.display, "_WIN_WORKSPACE", False),
            0, 1, False, AnyPropertyType, &type, &format, &nitems,
            &unusedBytes, &properties);
    }

    // Set result code.
    int resultCode = -1;
    if (type == XA_CARDINAL) {
        resultCode = *(long *) (void *) properties;
    } else {
        if (mGlobal.IsWayland) {
            resultCode = 0;
        }
    }

    XFree(properties);
    return resultCode;
}

/** ************************************************
 ** This method determines is we're a Gnome session, vs KDE.
 **/
bool isThisAGnomeSession() {
    char* desktop = getenv("XDG_SESSION_DESKTOP");
    for (char* eachChar = desktop; *eachChar; ++eachChar) {
        *eachChar = tolower(*eachChar);
    }
    return (strstr(desktop, "gnome") ||
            strstr(desktop, "ubuntu"));
}
