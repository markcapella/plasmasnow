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
#include <cstdio>
#include <ctype.h>
#include <malloc.h>
#include <unistd.h>

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/xpm.h>
#include <X11/Xutil.h>

#include <gtk/gtk.h>

#include <cairo-xlib.h>

#include "Application.h"
#include "Flags.h"
#include "PlasmaSnow.h"
#include "SplashPage.h"


/** ********************************************************
 ** Module globals and consts.
 **/
Screen* mScreen;
unsigned int mScreenWidth = 0;
unsigned int mScreenHeight = 0;

Window mSplashWindow;
XImage* mSplashImage;
XpmAttributes mSplashAttributes;


/** ********************************************************
 ** This method shows the SplashPage x11 window.
 **/
void showSplashPage() {
    if (mGlobal.noSplashScreen ||
        !Flags.ShowSplashScreen) {
        return;
    }

    // Display initial memory useage to log.
    mScreen = DefaultScreenOfDisplay(mGlobal.display);
    mScreenWidth = WidthOfScreen(mScreen);
    mScreenHeight = HeightOfScreen(mScreen);

    // Read XPM SplashImage from file.
    mSplashAttributes.valuemask = XpmSize;
    XpmReadFileToImage(mGlobal.display,
        "/usr/local/share/pixmaps/plasmasnowsplash.xpm",
        &mSplashImage, NULL, &mSplashAttributes);

    // Determine where to center Splash.
    const int CENTERED_X_POS = (mScreenWidth -
        mSplashAttributes.width) / 2;
    const int CENTERED_Y_POS = (mScreenHeight -
        mSplashAttributes.height) / 2;

    // Create our X11 Window to host the splash image.
    mSplashWindow = XCreateSimpleWindow(mGlobal.display,
        DefaultRootWindow(mGlobal.display), 0, 0, mSplashAttributes.width,
        mSplashAttributes.height, 1, WhitePixel(mGlobal.display, 0),
        WhitePixel(mGlobal.display, 0));

    // Set window to dock (no titlebar or close button).
    const Atom WINDOW_TYPE = XInternAtom(mGlobal.display,
        "_NET_WM_WINDOW_TYPE", False);
    const long TYPE_VALUE = XInternAtom(mGlobal.display,
        "_NET_WM_WINDOW_TYPE_DOCK", False);
    XChangeProperty(mGlobal.display, mSplashWindow, WINDOW_TYPE, XA_ATOM,
        32, PropModeReplace, (unsigned char*) &TYPE_VALUE, 1);

    // Map, then position window.
    XMapWindow(mGlobal.display, mSplashWindow);
    XMoveWindow(mGlobal.display, mSplashWindow,
        CENTERED_X_POS, CENTERED_Y_POS);

    // Gnome && KDE get different X11 event counts that
    // determine when the SplashPage has been completely DRAWN.
    const int EXPOSED_EVENTS_NEEDED =
        isThisAGnomeSession() ? 1 : 3;

    // Show SplashImage in the window. Consume X11 events.
    // Respond to Expose event for DRAW.
    XSelectInput(mGlobal.display, mSplashWindow, ExposureMask);
    int exposedEventCount = 0;

    bool finalEventReceived = false;
    while (!finalEventReceived) {
        XEvent event;
        XNextEvent(mGlobal.display, &event);
        switch (event.type) {
            case Expose:
                if (++exposedEventCount >= EXPOSED_EVENTS_NEEDED) {
                    finalEventReceived = true;
                    XPutImage(mGlobal.display, mSplashWindow,
                        XCreateGC(mGlobal.display, mSplashWindow, 0, 0),
                        mSplashImage, 0, 0, 0, 0,
                        mSplashAttributes.width,
                        mSplashAttributes.height);
                }
        }
    }
    XFlush(mGlobal.display);
}

/** ********************************************************
 ** This method hides the SplashPage x11 window.
 **/
void hideSplashPage() {
    if (mGlobal.noSplashScreen ||
        !Flags.ShowSplashScreen) {
        return;
    }

    // Close & destroy window, display.
    XpmFreeAttributes(&mSplashAttributes);
    XFree(mSplashImage);
    XUnmapWindow(mGlobal.display, mSplashWindow);
    XDestroyWindow(mGlobal.display, mSplashWindow);
}