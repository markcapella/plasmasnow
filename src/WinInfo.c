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

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "ColorCodes.h"
#include "dsimple.h"
#include "safe_malloc.h"
#include "windows.h"
#include "WinInfo.h"
#include "vroot.h"


/** *********************************************************************
 ** Module globals and consts.
 **/
#define MAX_TITLE_STRING_LENGTH 40
char mWinInfoTitleOfWindow[MAX_TITLE_STRING_LENGTH + 1];


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
        winInfoItem->ws = getWorkspaceOfWindow(winInfoItem->window);
        winInfoItem->sticky = isWindowSticky(winInfoItem->ws,
            winInfoItem);
        winInfoItem->dock = isWindowDock(winInfoItem);

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
 ** This method determines if a window is visible on a workspace.
 **/
long int getWorkspaceOfWindow(Window window) {
    long int result = 0;

    Atom type;
    int format;
    unsigned long nitems, unusedBytes;
    unsigned char *properties = NULL;

    XGetWindowProperty(mGlobal.display, window,
        XInternAtom(mGlobal.display, "_NET_WM_DESKTOP", False),
        0, 1, False, AnyPropertyType, &type, &format, &nitems,
        &unusedBytes, &properties);

    if (type != XA_CARDINAL) {
        XFree(properties);
        properties = NULL;

        XGetWindowProperty(mGlobal.display, window,
            XInternAtom(mGlobal.display, "_WIN_WORKSPACE", False),
            0, 1, False, AnyPropertyType, &type, &format, &nitems,
            &unusedBytes, &properties);
    }

    if (properties) {
        result = *(long*) (void*) properties;
        XFree(properties);
    }

    return result;
}

/** *********************************************************************
 ** This method returns the number of the current workspace,
 ** where the OS allows multiple / virtual workspaces.
 **/
long int getCurrentWorkspaceNumber() {
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

/** *********************************************************************
 ** This method checks if a window is hidden.
 **/
bool isWindowHidden(Window window, int windowMapState) {
    // If desktop isn't visible, all windows are hidden.
    if (!isDesktopVisible()) {
        return true;
    }

    if (windowMapState != IsViewable) {
        return true;
    }
    if (isWindowHiddenByNetWMState(window)) {
        return true;
    }
    if (isWindowHiddenByWMState(window)) {
        return true;
    }

    return false;
}

/** *********************************************************************
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

/** *********************************************************************
 ** This method checks "_NET_WM_STATE" for window HIDDEN attribute.
 **/
bool isWindowHiddenByNetWMState(Window window) {
    bool result = false;

    Atom type;
    int format;
    unsigned long nitems, unusedBytes;
    unsigned char *properties = NULL;

    XGetWindowProperty(mGlobal.display, window,
        XInternAtom(mGlobal.display, "_NET_WM_STATE", False),
        0, (~0L), False, AnyPropertyType, &type, &format,
        &nitems, &unusedBytes, &properties);

    if (format == 32) {
        for (unsigned long i = 0; i < nitems; i++) {
            char *nameString = XGetAtomName(mGlobal.display,
                ((Atom *) (void *) properties) [i]);
            if (strcmp(nameString, "_NET_WM_STATE_HIDDEN") == 0) {
                result = true;
                XFree(nameString);
                break;
            }
            XFree(nameString);
        }
    }
    XFree(properties);

    return result;
}

/** *********************************************************************
 ** This method checks "WM_STATE" for window HIDDEN attribute.
 **/
bool isWindowHiddenByWMState(Window window) {
    bool result = false;

    Atom type;
    int format;
    unsigned long nitems, unusedBytes;
    unsigned char *properties = NULL;

    XGetWindowProperty(mGlobal.display, window,
        XInternAtom(mGlobal.display, "WM_STATE", False),
        0, (~0L), False, AnyPropertyType, &type, &format,
        &nitems, &unusedBytes, &properties);

    if (format == 32 && nitems >= 1) {
        if (* (long*) (void*) properties != NormalState) {
            result = true;
        }
    }
    XFree(properties);

    return result;
}

/** *********************************************************************
 ** This method checks if a window is sticky.
 **/
bool isWindowSticky(long workSpace, WinInfo* winInfoItem) {
    // Needed in KDE and LXDE.
    if (workSpace == -1) {
        return true;
    }

    bool result = false;

    Atom type;
    int format;
    unsigned long nitems, unusedBytes;
    unsigned char *properties = NULL;

    XGetWindowProperty(mGlobal.display, winInfoItem->window,
        XInternAtom(mGlobal.display, "_NET_WM_STATE", False),
        0, (~0L), False, AnyPropertyType, &type, &format,
        &nitems, &unusedBytes, &properties);

    if (type == XA_ATOM) {
        for (unsigned long int i = 0; i < nitems; i++) {
            char *nameString = XGetAtomName(mGlobal.display,
                ((Atom *) (void *) properties) [i]);
            if (strcmp(nameString, "_NET_WM_STATE_STICKY") == 0) {
                result = true;
                XFree(nameString);
                break;
            }
            XFree(nameString);
        }
    }
    XFree(properties);

    return result;
}

/** *********************************************************************
 ** This method checks is a window is a dock.
 **/
bool isWindowDock(WinInfo* winInfoItem) {
    bool result = false;

    Atom type;
    int format;
    unsigned long nitems, unusedBytes;
    unsigned char *properties = NULL;

    XGetWindowProperty(mGlobal.display, winInfoItem->window,
        XInternAtom(mGlobal.display, "_NET_WM_WINDOW_TYPE", False),
        0, (~0L), False, AnyPropertyType, &type, &format,
        &nitems, &unusedBytes, &properties);

    if (format == 32) {
        for (int i = 0; (unsigned long)i < nitems; i++) {
            char *nameString = XGetAtomName(mGlobal.display,
                ((Atom *) (void *) properties) [i]);
            if (strcmp(nameString, "_NET_WM_WINDOW_TYPE_DOCK") == 0) {
                result = true;
                XFree(nameString);
                break;
            }
            XFree(nameString);
        }
    }
    XFree(properties);

    return result;
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

/** *********************************************************************
 ** This method prints a requested windows WinInfo struct.
 **/
void logWinInfoForWindow(Window window) {
    // Normal case, get WinInfo item and log it.
    WinInfo* winInfoItem = getWinInfoForWindow(window);
    if (!winInfoItem) {
        printf("[0x%08lx]  X11 winInfo not found.\n", window);
        return;
    }

    // Get printable title.
    setWinInfoTitleOfWindow(window);

    // Print it.
    printf("[0x%08lx]  %s  %2li  "
        " %5d , %-5d %5d x %-5d  %s%s%s\n",
        winInfoItem->window,
        getWinInfoTitleOfWindow(),
        winInfoItem->ws,
        winInfoItem->xa, winInfoItem->ya,
        winInfoItem->w, winInfoItem->h,
        winInfoItem->dock ? "dock " : "",
        winInfoItem->sticky ? "sticky " : "",
        winInfoItem->hidden ? "hidden" : "");
}

/** *********************************************************************
 ** Getter for mWinInfoTitleOfWindow.
 **/
char* getWinInfoTitleOfWindow() {
    return mWinInfoTitleOfWindow;
}

/** *********************************************************************
 ** Setter for mWinInfoTitleOfWindow.
 **
 ** Set mWinInfoTitleOfWindow. Create a 40-char formatted
 ** title (name) c-string with a hard length, replacing
 ** unprintables with SPACE, padding right with SPACE,
 ** and preserving null terminator.
 **/
void setWinInfoTitleOfWindow(Window window) {
    XTextProperty titleBarName;

    int outP = 0;
    if (XGetWMName(mGlobal.display, window, &titleBarName)) {
        const char* NAME_PTR = (char*) titleBarName.value;
        const int NAME_LEN = strlen(NAME_PTR);

        for (; outP < NAME_LEN && outP < MAX_TITLE_STRING_LENGTH; outP++) {
            mWinInfoTitleOfWindow[outP] = isprint(*(NAME_PTR + outP)) ?
                *(NAME_PTR + outP) : ' ';
        }
        XFree(titleBarName.value);
    }

    for (; outP < MAX_TITLE_STRING_LENGTH; outP++) {
        mWinInfoTitleOfWindow[outP] = ' ';
    }
    mWinInfoTitleOfWindow[outP] = '\0';
}

/** *********************************************************************
 ** This is a helper method for debugging.
 **/
void logWinAttrForWindow(Window window) {
    WinInfo* winInfoItem = getWinInfoForWindow(window);
    if (!winInfoItem) {
        printf("[0x%08lx]  X11 winInfo not found.\n", window);
        return;
    }

    // Get printable title & attributes.
    setWinInfoTitleOfWindow(window);
    XWindowAttributes attr;
    XGetWindowAttributes(mGlobal.display, window, &attr);

    // Print it.
    printf("[0x%08lx]  %s       %5d , %-5d %5d x %-5d  "
        "Bw: %5d  Dp: %5d  Map: %5d  Vi: %s  Sc: %s  Bs: %5d  "
         "MI? %s  Pl: %s  Pi: %s\n",
        winInfoItem->window, getWinInfoTitleOfWindow(),
        attr.x, attr.y, attr.width, attr.height,
        attr.border_width, attr.depth, attr.map_state,
        attr.visual ? "YES" : "NO ",
        attr.screen ? "YES" : "NO ",
        attr.backing_store,
        attr.map_installed ? "YES" : "NO ",
        attr.backing_planes ? "YES" : "NO ",
        attr.backing_pixel ? "YES" : "NO "
    );
}
