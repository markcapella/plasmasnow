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
#include <ctype.h>
#include <pthread.h>
#include <stdbool.h>

#include <X11/Intrinsic.h>
#include <X11/extensions/Xinerama.h>

#include <gdk/gdkx.h>
#include <gtk/gtk.h>

#include "Application.h"
#include "ColorCodes.h"
#include "Flags.h"
#include "StormWindow.h"
#include "Utils.h"
#include "Windows.h"


/**
 * Module globals and consts.
 */
GtkWidget* mGtkStormWindow = NULL;
GdkWindow* mGdkStormWindow;
Window mX11StormWindow = None;

GtkWidget* getGtkStormWindow() {
    return mGtkStormWindow;
}
Window* getX11StormWindow() {
    return &mX11StormWindow;
}

int mX11StormWindowXPos = 0;
int getWantMoveToX() {
    return mX11StormWindowXPos;
}

int mX11StormWindowYPos = 0;
int getWantMoveToY() {
    return mX11StormWindowYPos;
}

guint mStormWindowGUID = 0;
guint mCairoWindowGUID = 0;

/**
 * This method starts / creates the main storm window.
 */
bool canStartStormWindow() {
    mGlobal.Rootwindow = DefaultRootWindow(
        mGlobal.display);

    mGlobal.hasDestopWindow = false;
    mGlobal.hasStormWindow = false;

    mGlobal.useDoubleBuffers = false;

    mGlobal.xxposures = false;
    mGlobal.XscreensaverMode = false;


    mGlobal.isDoubleBuffered = false;

    if (!canCreateCompositedWindow()) {
        return false;
    }

    mGlobal.WindowOffsetX = getWantMoveToX();
    mGlobal.WindowOffsetY = getWantMoveToY();

    mGlobal.isDoubleBuffered = mGlobal.hasStormWindow ||
        mGlobal.useDoubleBuffers;

    xdo_move_window(mGlobal.xdo, mGlobal.SnowWin,
        getWantMoveToX(), getWantMoveToY());

    initDisplayDimensions();

    mGlobal.SnowWinX = getWantMoveToX();
    mGlobal.SnowWinY = getWantMoveToY();

    SetWindowScale();

    if (mGlobal.XscreensaverMode && !Flags.BlackBackground) {
        setWorkspaceBackground();
    }
    return true;
}

/**
 * Init the storm window variously.
 */
bool canCreateCompositedWindow() {
    // Normal start, create new transparent StormWindow
    // that passes events through also.
    mGlobal.hasDestopWindow = true;

    mGtkStormWindow = (GtkWidget*) g_object_new(
        GTK_TYPE_MESSAGE_DIALOG, "use-header-bar", false,
        "message-type", GTK_MESSAGE_OTHER, "buttons",
        GTK_BUTTONS_NONE, NULL);
    if (GTK_IS_BIN(mGtkStormWindow)) {
        GtkWidget* child = gtk_bin_get_child(GTK_BIN(
            mGtkStormWindow));
        if (child) {
            gtk_container_remove(GTK_CONTAINER(
                mGtkStormWindow), child);
        }
    }

    // Set window decorations.
    gtk_widget_set_can_focus(mGtkStormWindow, false);
    gtk_window_set_title(GTK_WINDOW(mGtkStormWindow),
         "plasmasnowstorm");
    gtk_window_set_decorated(GTK_WINDOW(mGtkStormWindow), false);
    gtk_window_set_type_hint(GTK_WINDOW(mGtkStormWindow),
        GDK_WINDOW_TYPE_HINT_POPUP_MENU);

    // Create in compositing windows.
    GdkScreen* compositingScreen =
        gtk_widget_get_screen(mGtkStormWindow);
    if (!gdk_screen_is_composited(compositingScreen)) {
        return false;
    }

    gtk_widget_set_visual(mGtkStormWindow,
        gdk_screen_get_rgba_visual(compositingScreen));
    decorateStormWindow();

    mGlobal.SnowWin = mX11StormWindow;
    mGlobal.hasStormWindow = true;
    mGlobal.isDoubleBuffered = true;

    g_signal_connect(mGtkStormWindow, "draw",
        G_CALLBACK(handleStormWindowDrawEvents), NULL);
    return true;
}

/**
 * This method creates the main Storm Window.
 */
bool decorateStormWindow() {
    gtk_widget_set_app_paintable(mGtkStormWindow, true);
    gtk_window_set_decorated(GTK_WINDOW(mGtkStormWindow), false);
    gtk_window_set_accept_focus(GTK_WINDOW(mGtkStormWindow), false);

    g_signal_connect(mGtkStormWindow, "draw",
        G_CALLBACK(setStormWindowAttributes), NULL);

    // Reset, then set the StormWindow attributres.
    GObject* STORM_WINDOW = G_OBJECT(mGtkStormWindow);
    g_object_steal_data(STORM_WINDOW, "trans_sticky");
    g_object_steal_data(STORM_WINDOW, "trans_done");

    if (Flags.AllWorkspaces) {
        g_object_set_data(STORM_WINDOW, "trans_sticky", "true");
    }

    // Set full screen if so desired:
    bool useXineramaWindow = false;
    int xineramaWindowPosX = 0;
    int xineramaWindowPosY = 0;
    int xineramaWindowWidth = 0;
    int xineramaWindowHeight = 0;

    if (Flags.Screen < 0) {
        XWindowAttributes attr;
        XGetWindowAttributes(mGlobal.display,
            DefaultRootWindow(mGlobal.display), &attr);
        gtk_widget_set_size_request(GTK_WIDGET(mGtkStormWindow),
            attr.width, attr.height);
        xineramaWindowWidth = attr.width;
        xineramaWindowHeight = attr.height;

    } else {
        useXineramaWindow = getXineramaScreenInfo(mGlobal.display,
            Flags.Screen, &xineramaWindowPosX, &xineramaWindowPosY,
            &xineramaWindowWidth, &xineramaWindowHeight);
        if (useXineramaWindow) {
            gtk_widget_set_size_request(GTK_WIDGET(mGtkStormWindow),
                xineramaWindowWidth, xineramaWindowHeight);
        }
    }

    // Show StormWindow, set helper handles.
    gtk_widget_show_all(mGtkStormWindow);
    mGdkStormWindow = gtk_widget_get_window(mGtkStormWindow);
    mX11StormWindow = gdk_x11_window_get_xid(mGdkStormWindow);

    // Gnome needs this as dock or it snows on top of
    // things. KDE needs it as NOT a dock, or it snows
    // on top of things.
    if (isThisAGnomeSession()) {
        gdk_window_set_type_hint(mGdkStormWindow, GDK_WINDOW_TYPE_HINT_DOCK);
    }

    // Resize & new values for xineramaWindow.
    mX11StormWindowXPos = xineramaWindowPosX;
    mX11StormWindowYPos = xineramaWindowPosY;

    // Resize to.
    if (mX11StormWindow) {
        XResizeWindow(mGlobal.display, mX11StormWindow,
            xineramaWindowWidth, xineramaWindowHeight);
        XFlush(mGlobal.display);
    }

    // Seems sometimes to be necessary with nvidia.
    //usleep(200000);
    gtk_widget_hide(mGtkStormWindow);
    gtk_widget_show_all(mGtkStormWindow);

    if (Flags.Screen < 0) {
        gtk_window_move(GTK_WINDOW(mGtkStormWindow), 0, 0);
    } else if (useXineramaWindow) {
        gtk_window_move(GTK_WINDOW(mGtkStormWindow),
            mX11StormWindowXPos, mX11StormWindowYPos);
    }

    setStormWindowAttributes(mGtkStormWindow);
    g_object_steal_data(G_OBJECT(mGtkStormWindow), "trans_done");
    return true;
}

/**
 *
 * For some reason, in some environments the 'below' and 'stick'
 * properties disappear. It works again, if we express our
 * wishes after starting gtk_main and the best place is in the
 * draw event.
 *
 * We want to reset the settings at least once to be sure.
 * Things like sticky and below should be stored in the
 * widget beforehand.
 *
 * TODO:
 * Lotsa code fix during thread inititaliztion by MJC may
 * have fixed this. Tinker and test.
 */
int setStormWindowAttributes(GtkWidget* widget) {
    enum {
        rep = 1,
        nrep
    };
    static char something[nrep];

    char* p = (char*) g_object_get_data(
        G_OBJECT(widget), "trans_done");
    if (!p) {
        p = &something[0];
    }
    if (p - &something[0] >= rep) {
        return false;
    }
    p++;

    g_object_set_data(G_OBJECT(widget), "trans_done", p);
    GdkWindow *gdk_window1 = gtk_widget_get_window(widget);

    // does not work as expected.
    const int Usepassthru = 0;
    if (Usepassthru) {
        gdk_window_set_pass_through(gdk_window1, true);
    } else {
        cairo_region_t *cairo_region1 = cairo_region_create();
        gdk_window_input_shape_combine_region(gdk_window1, cairo_region1, 0, 0);
        cairo_region_destroy(cairo_region1);
    }

    // Keep us on bottom, or it snows over things.
    setStormWindowBelow();

    // Set the Trans Window Sticky Flag.
    if (g_object_get_data(G_OBJECT(widget), "trans_sticky")) {
        gtk_window_stick(GTK_WINDOW(widget));
    } else {
        gtk_window_unstick(GTK_WINDOW(widget));
    }

    return false;
}

/**
 * ...
 */
void setStormWindowBelow() {
    if (isWindowOnBottomByNetWMState(mX11StormWindow)) {
        return;
    }
    gtk_window_set_keep_above(GTK_WINDOW(mGtkStormWindow), false);
    gtk_window_set_keep_below(GTK_WINDOW(mGtkStormWindow), true);
}

/**
 * This method...
 */
void connectStormWindowDraw() {
    if (mGlobal.hasStormWindow) {
        if (mStormWindowGUID) {
            g_source_remove(mStormWindowGUID);
        }
        mStormWindowGUID = addMethodWithArgToMainloop(
            PRIORITY_HIGH, time_draw_all,
            drawStormWindow, getGtkStormWindow());
        return;
    }

    if (mCairoWindowGUID) {
        g_source_remove(mCairoWindowGUID);
    }
    mCairoWindowGUID = addMethodWithArgToMainloop(PRIORITY_HIGH,
        time_draw_all, drawCairoWindow, getCairoWindow());
}

/**
 * Set the Storm Window sticky state.
 */
void setStormWindowStickyState(bool isSticky) {
    if (!mGlobal.hasStormWindow) {
        return;
    }

    GtkWindow* STORM_WINDOW =
        GTK_WINDOW(getGtkStormWindow());
    if (isSticky) {
        gtk_window_stick(STORM_WINDOW);
    } else {
        gtk_window_unstick(STORM_WINDOW);
    }
}
