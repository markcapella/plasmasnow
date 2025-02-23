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
#include <malloc.h>
#include <unistd.h>

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/xpm.h>
#include <X11/Xutil.h>

#include "Flags.h"
#include "PlasmaSnow.h"
#include "SplashPage.h"


/** ********************************************************
 ** Module globals and consts.
 **/
Window mSplashWindow;
bool mSplashWindowMapped = false;

XImage* mSplashImage;

int mExposeEventCount = 0;


/** ********************************************************
 ** This method shows the SplashPage x11 window.
 **/
void showSplashPage() {
    if (!Flags.ShowSplashScreen) {
        return;
    }

    // Read XPM SplashImage from file.
    XpmAttributes attr;
    attr.valuemask = 0;
    attr.bitmap_format = ZPixmap;
    attr.valuemask |= XpmBitmapFormat;
    attr.valuemask |= XpmSize;
    attr.valuemask |= XpmCharsPerPixel;
    attr.valuemask |= XpmReturnPixels;
    attr.valuemask |= XpmReturnAllocPixels;
    attr.exactColors = False;
    attr.valuemask |= XpmExactColors;
    attr.closeness = 30000;
    attr.valuemask |= XpmCloseness;
    attr.alloc_close_colors = False;
    attr.valuemask |= XpmAllocCloseColors;

    XpmReadFileToImage(mGlobal.display,
        "/usr/local/share/pixmaps/plasmasnowsplash.xpm",
        &mSplashImage, NULL, &attr);

    // Create our X11 Window to host the image.
    mSplashWindow = XCreateSimpleWindow(mGlobal.display,
        DefaultRootWindow(mGlobal.display), 0, 0, attr.width, attr.height,
        1, WhitePixel(mGlobal.display, 0), WhitePixel(mGlobal.display, 0));

    // Set window to dock (no titlebar or close button).
    const Atom WINDOW_TYPE = XInternAtom(mGlobal.display,
        "_NET_WM_WINDOW_TYPE", False);
    const long TYPE_VALUE = XInternAtom(mGlobal.display,
        "_NET_WM_WINDOW_TYPE_DOCK", False);
    XChangeProperty(mGlobal.display, mSplashWindow, WINDOW_TYPE, XA_ATOM,
        32, PropModeReplace, (unsigned char*) &TYPE_VALUE, 1);

    // Add the image to the host window.
    XPutImage(mGlobal.display, mSplashWindow, XCreateGC(mGlobal.display,
        mSplashWindow, 0, 0), mSplashImage, 0, 0, 0, 0,
        attr.width, attr.height);

    // Show SplashImage in the window.
    XSelectInput(mGlobal.display, mSplashWindow, ExposureMask);
    Atom mDeleteMessage = XInternAtom(mGlobal.display,
        "WM_DELETE_WINDOW", False);
    XSetWMProtocols(mGlobal.display, mSplashWindow, &mDeleteMessage, 1);

    const int FINAL_EXPOSE_EVENT_COUNT = 3;
    bool finalEventReceived = false;

    while (!finalEventReceived) {
        if (!mSplashWindowMapped) {
            XMapWindow(mGlobal.display, mSplashWindow);
            mSplashWindowMapped = true;
        }

        XEvent event;
        XNextEvent(mGlobal.display, &event);
        switch (event.type) {
            case ClientMessage:
                if (event.xclient.data.l[0] ==
                        (long int) mDeleteMessage) {
                    finalEventReceived = true;
                }
                break;

            case Expose:
                if (event.xexpose.window != mSplashWindow) {
                    break;
                }

                mExposeEventCount++;
                XPutImage(mGlobal.display, mSplashWindow, XCreateGC(mGlobal.display,
                    mSplashWindow, 0, 0), mSplashImage, 0, 0, 0, 0,
                    attr.width, attr.height);
                if (mExposeEventCount >= FINAL_EXPOSE_EVENT_COUNT) {
                    finalEventReceived = true;
                }
                break;
        }
    }
}

/** ********************************************************
 ** This method hides the SplashPage x11 window.
 **/
void hideSplashPage() {
    if (!Flags.ShowSplashScreen) {
        return;
    }

    // Close & destroy window, display.
    XUnmapWindow(mGlobal.display, mSplashWindow);
    XDestroyWindow(mGlobal.display, mSplashWindow);
}