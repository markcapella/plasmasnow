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
#include "StormWindow.h"
#include "Windows.h"


/** *********************************************************************
 ** Module globals and consts.
 **/

/** *********************************************************************
 ** This method creates the main Storm Window.
 **/
bool createStormWindow(Display* display, GtkWidget* stormWindow,
    int xscreen, int sticky, int below, GdkWindow** gdk_window,
    Window* x11_window, int* wantx, int* wanty) {

    // Guard the outputs.
    if (gdk_window) {
        *gdk_window = NULL;
    }
    if (x11_window) {
        *x11_window = None;
    }

    // Implement window.
    gtk_widget_set_app_paintable(stormWindow, TRUE);
    gtk_window_set_decorated(GTK_WINDOW(stormWindow), FALSE);
    gtk_window_set_accept_focus(GTK_WINDOW(stormWindow), FALSE);

    g_signal_connect(stormWindow, "draw",
        G_CALLBACK(setStormWindowAttributes), NULL);

    // Remove our things from inputStormWindow:
    g_object_steal_data(G_OBJECT(stormWindow), "trans_sticky");
    g_object_steal_data(G_OBJECT(stormWindow), "trans_below");
    g_object_steal_data(G_OBJECT(stormWindow), "trans_nobelow");
    g_object_steal_data(G_OBJECT(stormWindow), "trans_done");

    // Reset our things.  :-/
    static char somechar;
    if (sticky) {
        g_object_set_data(G_OBJECT(stormWindow),
            "trans_sticky", &somechar);
    }
    switch (below) {
        case 0:
            g_object_set_data(G_OBJECT(stormWindow),
                "trans_nobelow", &somechar);
            break;
        case 1:
            g_object_set_data(G_OBJECT(stormWindow),
                "trans_below", &somechar);
            break;
    }

    /* To check if the display supports alpha channels, get the visual */
    GdkScreen* screen = gtk_widget_get_screen(stormWindow);
    if (!gdk_screen_is_composited(screen)) {
        gtk_window_close(GTK_WINDOW(stormWindow));
        return false;
    }

    // Ensure the widget (the window, actually) can take RGBA.
    gtk_widget_set_visual(stormWindow,
        gdk_screen_get_rgba_visual(screen));

    // set full screen if so desired:
    bool useXineramaWindow = false;
    int winx, winy; // desired position of window
    int winw, winh; // desired size of window

    if (xscreen < 0) {
        XWindowAttributes attr;
        XGetWindowAttributes(display, DefaultRootWindow(display),
            &attr);
        gtk_widget_set_size_request(GTK_WIDGET(
            stormWindow), attr.width, attr.height);
        winx = 0;
        winy = 0;
        winw = attr.width;
        winh = attr.height;
    } else {
        useXineramaWindow = getXineramaScreenInfo(display, xscreen,
            &winx, &winy, &winw, &winh);
        if (useXineramaWindow) {
            gtk_widget_set_size_request(GTK_WIDGET(
                stormWindow), winw, winh);
        }
    }

    // Show.
    gtk_widget_show_all(stormWindow);
    GdkWindow* gdkwin = gtk_widget_get_window(
        GTK_WIDGET(stormWindow));

    // Gnome needs this as dock or it snows on top of
    // things. KDE needs it as NOT a dock, or it snows
    // on top of things.
    if (isThisAGnomeSession()) {
        gdk_window_set_type_hint(gdkwin,
            GDK_WINDOW_TYPE_HINT_DOCK);
    }

    // Set return values.
    if (x11_window) {
        *x11_window = gdk_x11_window_get_xid(gdkwin);
    }
    if (gdk_window) {
        *gdk_window = gdkwin;
    }
    *wantx = winx;
    *wanty = winy;

    // Resize & new values for winw / winh.
    if (x11_window) {
        XResizeWindow(display, *x11_window, winw, winh);
        XFlush(display);
    }

    // Seems sometimes to be necessary with nvidia.
    usleep(200000);

    gtk_widget_hide(stormWindow);
    gtk_widget_show_all(stormWindow);

    if (xscreen < 0) {
        gtk_window_move(GTK_WINDOW(stormWindow), 0, 0);
    } else if (useXineramaWindow) {
        gtk_window_move(GTK_WINDOW(stormWindow), winx, winy);
    }

    setStormWindowAttributes(stormWindow);
    g_object_steal_data(G_OBJECT(stormWindow), "trans_done");

    return true;
}

/** *********************************************************************
 **
 ** for some reason, in some environments the 'below' and 'stick'
 ** properties disappear. It works again, if we express our
 ** wishes after starting gtk_main and the best place is in the
 ** draw event.
 ** 
 ** We want to reset the settings at least once to be sure.
 ** Things like sticky and below should be stored in the
 ** widget beforehand.
 **
 ** TODO:
 ** Lotsa code fix during thread inititaliztion by MJC may
 ** have fixed this. Tinker and test.
 **/
int setStormWindowAttributes(GtkWidget *widget) {
    enum {
        rep = 1,
        nrep
    };
    static char something[nrep];

    char *p = (char *) g_object_get_data(G_OBJECT(widget), "trans_done");
    if (!p) {
        p = &something[0];
    }

    if (p - &something[0] >= rep) {
        return FALSE;
    }

    p++;
    g_object_set_data(G_OBJECT(widget), "trans_done", p);
    GdkWindow *gdk_window1 = gtk_widget_get_window(widget);

    // does not work as expected.
    const int Usepassthru = 0;
    if (Usepassthru) {
        gdk_window_set_pass_through(gdk_window1, TRUE);
    } else {
        cairo_region_t *cairo_region1 = cairo_region_create();
        gdk_window_input_shape_combine_region(gdk_window1, cairo_region1, 0, 0);
        cairo_region_destroy(cairo_region1);
    }

    if (!g_object_get_data(G_OBJECT(widget), "trans_nobelow")) {
        if (g_object_get_data(G_OBJECT(widget), "trans_below")) {
            setTransparentWindowBelow(GTK_WINDOW(widget));
        } else {
            setTransparentWindowAbove(GTK_WINDOW(widget));
        }
    }

    // Set the Trans Window Sticky Flag.
    if (g_object_get_data(G_OBJECT(widget), "trans_sticky")) {
        gtk_window_stick(GTK_WINDOW(widget));
    } else {
        gtk_window_unstick(GTK_WINDOW(widget));
    }

    return FALSE;
}

/** *********************************************************************
 ** 
 **/
void setTransparentWindowBelow(__attribute__((unused)) GtkWindow *window) {
    gtk_window_set_keep_above(GTK_WINDOW(window), false);
    gtk_window_set_keep_below(GTK_WINDOW(window), true);
}

/** *********************************************************************
 ** 
 **/
void setTransparentWindowAbove(__attribute__((unused)) GtkWindow *window) {
    gtk_window_set_keep_below(GTK_WINDOW(window), false);
    gtk_window_set_keep_above(GTK_WINDOW(window), true);
}
