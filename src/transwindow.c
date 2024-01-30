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
#include <pthread.h>
#include "debug.h"
#include "windows.h"
#include "transwindow.h"
#include <stdbool.h>

#include <X11/Intrinsic.h>
#include <X11/extensions/Xinerama.h>

#include <gdk/gdkx.h>
#include <gtk/gtk.h>


static int resetVolatileTransparentWindowAttributes(
    GtkWidget *widget);

// extern int getTmpLogFile();

/*
 * creates transparent window using gtk3/cairo.
 *
 * transparentGTKWindow: (input)  GtkWidget to create transparent window in
 * xscreen:     (input)  <0: full-screen  else xinerama screen number
 * sticky:      (input)  visible on all workspaces or not
 * below:       (input)  1: below all other windows 2: above all other windows
 *                       0: no action
 * dock:        (input)  make it a 'dock' window: no decoration and
 *                       not interfering with app.
 *
 * gdk_window:  (output) GdkWindow created
 * x11_window:  (output) Window X11 created: (output)
 *
 * xpenguins NOTE: with dock=1, gtk ignores the value of below:
 * window is above all other windows
 *
 * NOTE: with decorations set to TRUE (see gtk_window_set_decorated()),
 * the window is not click-through in Gnome.
 *
 * So: dock = 1 is good for Gnome, or call gtk_window_set_decorated(w,FALSE)
 * before this function.
 *
 */

int createTransparentWindow(Display *display,
    GtkWidget *transparentGTKWindow, int xscreen,
    int sticky, int below, int dock, GdkWindow **gdk_window,
    Window *x11_window, int *wantx, int *wanty) {

    {   const char* logMsg =
            "transparentwindow: createTransparentWindow() Starts.\n";
        fprintf(stdout, "%s", logMsg);
    }

    if (gdk_window) {
        *gdk_window = NULL;
    }
    if (x11_window) {
        *x11_window = None;
    }

    gtk_widget_set_app_paintable(transparentGTKWindow, TRUE);

    // essential in Gnome:
    gtk_window_set_decorated(GTK_WINDOW(transparentGTKWindow), FALSE);

    // essential everywhere:
    gtk_window_set_accept_focus(GTK_WINDOW(transparentGTKWindow), FALSE);

    // take care that 'below' and 'sticky' are taken care of in gtk_main loop:
    g_signal_connect(transparentGTKWindow, "draw",
        G_CALLBACK(resetVolatileTransparentWindowAttributes), NULL);

    // remove our things from transparentGTKWindow:
    g_object_steal_data(G_OBJECT(transparentGTKWindow), "trans_sticky");
    g_object_steal_data(G_OBJECT(transparentGTKWindow), "trans_below");
    g_object_steal_data(G_OBJECT(transparentGTKWindow), "trans_nobelow");
    g_object_steal_data(G_OBJECT(transparentGTKWindow), "trans_done");

    static char somechar;
    if (sticky) {
        g_object_set_data(G_OBJECT(transparentGTKWindow), "trans_sticky", &somechar);
    }

    switch (below) {
        case 0:
            g_object_set_data(G_OBJECT(transparentGTKWindow), "trans_nobelow", &somechar);
            break;
        case 1:
            g_object_set_data(G_OBJECT(transparentGTKWindow), "trans_below", &somechar);
            break;
    }

    /* To check if the display supports alpha channels, get the visual */
    GdkScreen *screen = gtk_widget_get_screen(transparentGTKWindow);
    if (!gdk_screen_is_composited(screen)) {
        gtk_window_close(GTK_WINDOW(transparentGTKWindow));
        {   const char* logMsg =
                "transparentwindow: createTransparentWindow() Finishes - ERROR.\n";
            fprintf(stdout, "%s", logMsg);
        }
        return FALSE;
    }

    // Ensure the widget (the window, actually) can take RGBA
    gtk_widget_set_visual(transparentGTKWindow, gdk_screen_get_rgba_visual(screen));

    int winx, winy; // desired position of window
    int winw, winh; // desired size of window
    int wantxin = (xscreen >= 0);

    // set full screen if so desired:
    if (xscreen < 0) {
        XWindowAttributes attr;
        XGetWindowAttributes(display, DefaultRootWindow(display), &attr);
        P("width, height %d %d\n", attr.width, attr.height);
        gtk_widget_set_size_request(
            GTK_WIDGET(transparentGTKWindow), attr.width, attr.height);
        winx = 0;
        winy = 0;
        winw = attr.width;
        winh = attr.height;
    } else {
        wantxin = xinerama(display, xscreen, &winx, &winy, &winw, &winh);
        if (wantxin) {
            gtk_widget_set_size_request(GTK_WIDGET(transparentGTKWindow), winw, winh);
        }
    }

    gtk_widget_show_all(transparentGTKWindow);

    // so that apps like this will ignore this window:
    GdkWindow *gdkwin = gtk_widget_get_window(GTK_WIDGET(transparentGTKWindow));
    if (dock) {
        gdk_window_set_type_hint(gdkwin, GDK_WINDOW_TYPE_HINT_DOCK);
    }

    gdk_window_show(gdkwin);

    if (x11_window) {
        *x11_window = gdk_x11_window_get_xid(gdkwin);

        char resultMsg[1024];
        snprintf(resultMsg, sizeof(resultMsg), "transparentwindow: createTransparentWindow()"
            "The X11 id of mTransparentWindow is : 0x%08lx\n", *x11_window);
        fprintf(stdout, "%s", resultMsg);

        XResizeWindow(display, *x11_window, winw, winh);
        XFlush(display);
    }

    if (gdk_window) {
        *gdk_window = gdkwin;
    }

    *wantx = winx;
    *wanty = winy;
    usleep(200000); // seems sometimes to be necessary with nvidia

    gtk_widget_hide(transparentGTKWindow);
    gtk_widget_show_all(transparentGTKWindow);

    if (xscreen < 0) {
        gtk_window_move(GTK_WINDOW(transparentGTKWindow), 0, 0);
    } else if (wantxin) {
        gtk_window_move(GTK_WINDOW(transparentGTKWindow), winx, winy);
    }

    resetVolatileTransparentWindowAttributes(transparentGTKWindow);
    g_object_steal_data(G_OBJECT(transparentGTKWindow), "trans_done");

    {   const char* logMsg =
            "transparentwindow: createTransparentWindow() Finishes.\n";
        fprintf(stdout, "%s", logMsg);
    }

    return TRUE;
}

/** *********************************************************************
 ** 
 **/
// for some reason, in some environments the 'below' and 'stick' properties
// disappear. It works again, if we express our wishes after starting gtk_main
// and the best place is in the draw event.
//
// We want to reset the settings at least once to be sure.
// Things like sticky and below should be stored in the widget beforehand.
// Use the value of p itself, not what it points to.
// Following the C standard, we have to use an array to subtract pointers.

int resetVolatileTransparentWindowAttributes(GtkWidget *widget) {
    /*
    {   const char* logMsg =
            "transparentwindow: resetVolatileTransparentWindowAttributes() Starts.\n";
        write(getTmpLogFile(), logMsg, strlen(logMsg));
    }
    */

    // must be >= 0, and is equal to the number of times the settings
    // will be done when called more than once
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
        /*
        {   const char* logMsg =
                "transparentwindow: resetVolatileTransparentWindowAttributes() Finishes early.\n";
            write(getTmpLogFile(), logMsg, strlen(logMsg));
        }
        */
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
    /*
    {   const char* logMsg =
            "transparentwindow: resetVolatileTransparentWindowAttributes() Finishes.\n";
        write(getTmpLogFile(), logMsg, strlen(logMsg));
    }
    */
    return FALSE;
}

/** *********************************************************************
 ** 
 **/
void setTransparentWindowBelow(__attribute__((unused)) GtkWindow *window) {
    {   const char* logMsg =
            "transparentwindow: setTransparentWindowBelow() Starts.\n";
        fprintf(stdout, "%s", logMsg);
    }

    gtk_window_set_keep_above(GTK_WINDOW(window), false);

    {   const char* logMsg =
            "transparentwindow: setTransparentWindowBelow() "
            "keep_above FALSE finished.\n";
        fprintf(stdout, "%s", logMsg);
    }

    gtk_window_set_keep_below(GTK_WINDOW(window), true);

    {   const char* logMsg =
            "transparentwindow: setTransparentWindowBelow() "
            "keep_below TRUE finished.\n";
        fprintf(stdout, "%s", logMsg);
    }

    //doLowerWindow(mGlobal.mPlasmaWindowTitle);

    {   const char* logMsg =
            "transparentwindow: setTransparentWindowBelow() Finishes.\n";
        fprintf(stdout, "%s", logMsg);
    }
}

/** *********************************************************************
 ** 
 **/
void setTransparentWindowAbove(__attribute__((unused)) GtkWindow *window) {
    {   const char* logMsg =
            "transparentwindow: setTransparentWindowAbove() Starts,\n";
        fprintf(stdout, "%s", logMsg);
    }

    gtk_window_set_keep_below(GTK_WINDOW(window), false);

    {   const char* logMsg =
            "transparentwindow: setTransparentWindowAbove() "
            "keep_below FALSE finished.\n";
        fprintf(stdout, "%s", logMsg);
    }

    gtk_window_set_keep_above(GTK_WINDOW(window), true);

    {   const char* logMsg =
            "transparentwindow: setTransparentWindowAbove() "
            "keep_above TRUE finished.\n";
        fprintf(stdout, "%s", logMsg);
    }

    doRaiseWindow(mGlobal.mPlasmaWindowTitle);

    {   const char* logMsg =
            "transparentwindow: setTransparentWindowAbove() "
            "Finishes.\n";
        fprintf(stdout, "%s", logMsg);
    }
}
