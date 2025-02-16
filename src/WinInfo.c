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
/*
 * This works with EWHM/NetWM compatible X Window managers,
 * so enlightenment (for example) is a problem.
 * In enlightenment there is no way to tell if a window is minimized,
 * and on which workspace the focus is.
 * There would be one advantage of enlightenment: you can tell easily
 * if a window is on the screen (minimized or not) by looking at
 * __E_WINDOW_MAPPED
 */

#include <ctype.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

// X11 headers.
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

// Other library headers.
#include <gtk/gtk.h>

// Plasmasnow headers.
#include "Application.h"
#include "ColorCodes.h"
#include "dsimple.h"
#include "safe_malloc.h"
#include "Windows.h"
#include "WinInfo.h"
#include "vroot.h"


/** *********************************************************************
 ** This method scans all WinInfos for a requested ID.
 **/
WinInfo* getWinInfoForWindow(Window window) {
    WinInfo* winInfoItem = mGlobal.winInfoList;
    for (int i = 0; i < mGlobal.winInfoListLength; i++) {
        if (winInfoItem->window == window) {
            return winInfoItem;
        }
        winInfoItem++;
    }

    return NULL;
}

/** *********************************************************************
 ** This method populates the global WinInfo list.
 **/
void getWinInfoForAllWindows() {
    if (mGlobal.winInfoList) {
        free(mGlobal.winInfoList);
    }

    getInitialWinInfoList(&mGlobal.winInfoList,
        &mGlobal.winInfoListLength);
    getFinalWinInfoList(&mGlobal.winInfoList,
        &mGlobal.winInfoListLength);
}

/** *********************************************************************
 ** This method gets our initial winInfoList list from 1 of 3 places.
 **/
void getInitialWinInfoList(WinInfo** winInfoList,
    int* numberOfWindows) {

    // #1 Look for list in NET_CLIENT.
    Atom type;
    int format;
    long unsigned int nchildren;
    unsigned long unusedBytes;
    Window* children;

    XGetWindowProperty(mGlobal.display, DefaultRootWindow(mGlobal.display),
        XInternAtom(mGlobal.display, "_NET_CLIENT_LIST", False),
        0, 1000000, False, AnyPropertyType, &type,
        &format, &nchildren, &unusedBytes, (unsigned char **) &children);

    if (type == XA_WINDOW && nchildren > 0) {
        (*winInfoList) = (WinInfo*) malloc(nchildren* sizeof(WinInfo));
        (*numberOfWindows) = nchildren;

        WinInfo* addWinInfo = *winInfoList;
        Window* addWindow = children;
        for (long unsigned int i = 0; i <= nchildren; i++) {
            addWinInfo->window = *addWindow;
            addWinInfo++;
            addWindow++;
        }

        XFree(children);
        return;
    }
    XFree(children);

    // #2, Else look for list in WIN_CLIENT.
    children = NULL;
    XGetWindowProperty(mGlobal.display, DefaultRootWindow(mGlobal.display),
        XInternAtom(mGlobal.display, "_WIN_CLIENT_LIST", False),
        0, 1000000, False, AnyPropertyType, &type,
        &format, &nchildren, &unusedBytes, (unsigned char **) &children);

    if (type == XA_WINDOW && nchildren > 0) {
        (*winInfoList) = (WinInfo*) malloc(nchildren* sizeof(WinInfo));
        (*numberOfWindows) = nchildren;

        WinInfo* addWinInfo = *winInfoList;
        Window* addWindow = children;
        for (long unsigned int i = 0; i <= nchildren; i++) {
            addWinInfo->window = *addWindow;
            addWinInfo++;
            addWindow++;
        }

        XFree(children);
        return;
    }
    XFree(children);

    // #3, Finally, use Query tree.
    children = NULL;
    Window unused;
    unsigned int queryChildrenCount;

    XQueryTree(mGlobal.display, DefaultRootWindow(mGlobal.display),
        &unused, &unused, &children, &queryChildrenCount);

    if (queryChildrenCount > 0) {
        (*winInfoList) = (WinInfo*)
            malloc(queryChildrenCount* sizeof(WinInfo));
        (*numberOfWindows) = queryChildrenCount;

        WinInfo* addWinInfo = *winInfoList;
        Window* addWindow = children;
        for (long unsigned int i = 0; i <= nchildren; i++) {
            addWinInfo->window = *addWindow;
            addWinInfo++;
            addWindow++;
        }
    }
    XFree(children);
}

/** *********************************************************************
 ** This method completes population of our winInfoList.
 **/
void getFinalWinInfoList(WinInfo** winInfoList,
    int* numberOfWindows) {

    WinInfo *winInfoItem = (*winInfoList);
    for (int i = 0; i < *numberOfWindows; i++) {
        // Set WinInfo "workspace", "sticky", and "dock" attributes.
        winInfoItem->ws = isWindowVisibleOnWorkspace(winInfoItem->window);
        winInfoItem->sticky = isWindowSticky(winInfoItem->window,
            winInfoItem->ws);
        winInfoItem->dock = isWindowDock(winInfoItem->window);

        // Set WinInfo "X pos", "Y pos", and "hidden" attribute.
        XWindowAttributes windowAttributes;
        XGetWindowAttributes(mGlobal.display, winInfoItem->window,
            &windowAttributes);
        winInfoItem->w = windowAttributes.width;
        winInfoItem->h = windowAttributes.height;
        winInfoItem->hidden = isWindowHidden(winInfoItem->window,
            windowAttributes.map_state);

        // Save for later frame extent calculations.
        int initialWinAttr_XPos = windowAttributes.x;
        int initialWinAttr_YPos = windowAttributes.y;

        // Set WinInfo "X / Y actual" attributes.
        int xr, yr;
        Window child_return;
        XTranslateCoordinates(mGlobal.display, winInfoItem->window,
            mGlobal.Rootwindow, 0, 0, &xr, &yr, &child_return);
        winInfoItem->xa = xr - initialWinAttr_XPos;
        winInfoItem->ya = yr - initialWinAttr_YPos;

        // Set WinInfo "X / Y position" attributes.
        XTranslateCoordinates(mGlobal.display, winInfoItem->window,
            mGlobal.SnowWin, 0, 0, &(winInfoItem->x),
            &(winInfoItem->y), &child_return);

        // Apply WinInfo frame extent adjustments.
        enum { NET, GTK };
        int wintype = GTK;

        Atom type;
        int format;
        unsigned long nitems = 0;
        unsigned long unusedBytes;
        unsigned char* properties;

        XGetWindowProperty(mGlobal.display, winInfoItem->window,
            XInternAtom(mGlobal.display, "_GTK_FRAME_EXTENTS", False),
            0, 4, False, AnyPropertyType, &type, &format,
            &nitems, &unusedBytes, &properties);

        if (nitems != 4) {
            XFree(properties);
            properties = NULL;

            wintype = NET;
            XGetWindowProperty(mGlobal.display, winInfoItem->window,
                XInternAtom(mGlobal.display, "_NET_FRAME_EXTENTS", False),
                0, 4, False, AnyPropertyType, &type, &format,
                &nitems, &unusedBytes, &properties);
        }

        if (nitems == 4 && format == 32 && type) {
            long* frameExtent;
            frameExtent = (long *) (void *) properties;
            switch (wintype) {
                case NET:
                    winInfoItem->x -= frameExtent[0];
                    winInfoItem->y -= frameExtent[2];
                    winInfoItem->w += frameExtent[0] + frameExtent[1];
                    winInfoItem->h += frameExtent[2] + frameExtent[3];
                    break;
                case GTK:
                    winInfoItem->x += frameExtent[0];
                    winInfoItem->y += frameExtent[2];
                    winInfoItem->w -= (frameExtent[0] + frameExtent[1]);
                    winInfoItem->h -= (frameExtent[2] + frameExtent[3]);
                    break;
            }
        } else {
            winInfoItem->x = initialWinAttr_XPos;
            winInfoItem->y = initialWinAttr_YPos;
        }

        // Free resources, bump pointer, loop.
        XFree(properties);
        winInfoItem++;
    }

    return;
}

/** *********************************************************************
 ** This method prints column headings for WinInfo structs.
 **/
void logWinInfoStructColumns() {
    printf("%s---window---  Titlebar Name"
        "                             WS   "
        "---Position-- -----Size----  Attributes%s\n",
        COLOR_GREEN, COLOR_NORMAL);
}

/** *********************************************************************
 ** This method prints all WinInfo structs.
 **/
void logAllWinInfoStructs() {
    logWinInfoStructColumns();

    WinInfo* winInfoItem = mGlobal.winInfoList;
    for (int i = 0; i < mGlobal.winInfoListLength; i++) {
        logWinInfoForWindow(winInfoItem->window);
        winInfoItem++;
    }
}
