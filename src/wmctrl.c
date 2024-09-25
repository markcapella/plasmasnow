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

#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "ColorCodes.h"
#include "debug.h"
#include "dsimple.h"
#include "safe_malloc.h"
#include "windows.h"
#include "wmctrl.h"
#include "vroot.h"


/** *********************************************************************
 ** This method ...
 **/
void getX11WindowsList(WinInfo** winInfoList, int* numberOfWindows) {
    (*numberOfWindows) = 0;
    (*winInfoList) = NULL;

    getRawWindowsList(winInfoList, numberOfWindows);
    getFinishedWindowsList(winInfoList, numberOfWindows);

    return;
}

/** *********************************************************************
 ** This method gets our initial winInfoList list from 1 of 3 places.
 **/
void getRawWindowsList(WinInfo** winInfoList, int* numberOfWindows) {

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
        children = NULL;
        return;
    }

    if (children) {
        XFree(children);
        children = NULL;
    }

    // #2, Else look for list in WIN_CLIENT.
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
        children = NULL;
        return;
    }

    if (children) {
        XFree(children);
        children = NULL;
    }

    // #3, Finally, use Query tree.
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

    if (children) {
        XFree(children);
        children = NULL;
    }
}

/** *********************************************************************
 ** This method ...
 **/
void getFinishedWindowsList(WinInfo** winInfoList,
    int* numberOfWindows) {

    WinInfo *winInfoItem = (*winInfoList);
    for (int i = 0; i < *numberOfWindows; i++) {
        // Set WinInfo "workspace", "sticky", and "dock" attributes.
        winInfoItem->ws = getWindowWorkspace(winInfoItem->window);
        winInfoItem->sticky = isWindow_Sticky(winInfoItem->ws,
            winInfoItem);
        winInfoItem->dock = isWindow_Dock(winInfoItem);

        // Set WinInfo "X pos", "Y pos", and "hidden" attribute.
        XWindowAttributes windowAttributes;
        XGetWindowAttributes(mGlobal.display, winInfoItem->window,
            &windowAttributes);
        winInfoItem->w = windowAttributes.width;
        winInfoItem->h = windowAttributes.height;
        winInfoItem->hidden = isWindow_Hidden(winInfoItem->window,
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
            if (properties) {
                XFree(properties);
            }
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
        if (properties) {
            XFree(properties);
        }
        winInfoItem++;
    }

    return;
}

/** *********************************************************************
 ** This method determines if a window is visible on a workspace.
 **/
long int getWindowWorkspace(Window window) {
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
        if (properties) {
            XFree(properties);
        }
        properties = NULL;
        XGetWindowProperty(mGlobal.display, window,
            XInternAtom(mGlobal.display, "_WIN_WORKSPACE", False),
            0, 1, False, AnyPropertyType, &type, &format, &nitems,
            &unusedBytes, &properties);
    }

    if (properties) {
        result = *(long *) (void *) properties;
        if (properties) {
            XFree(properties);
        }
    }

    return result;
}

/** *********************************************************************
 ** This method ...
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

        // We have the x-y coordinates of the workspace, we hussle this
        // into one long number and return as the result.
        int resultCode = -1;
        if (type == XA_CARDINAL && nitems == 2) {
            resultCode = ((long *) (void *) properties)[0] +
                (((long *) (void *) properties)[1] << 16);
        }

        if (properties) {
            XFree(properties);
        }
        return resultCode;
    }

    XGetWindowProperty(mGlobal.display,
        DefaultRootWindow(mGlobal.display),
        XInternAtom(mGlobal.display, "_NET_CURRENT_DESKTOP", False),
        0, 1, False, AnyPropertyType, &type, &format, &nitems,
        &unusedBytes, &properties);

    if (type != XA_CARDINAL) {
        if (properties) {
            XFree(properties);
        }
        XGetWindowProperty(mGlobal.display,
            DefaultRootWindow(mGlobal.display),
            XInternAtom(mGlobal.display, "_WIN_WORKSPACE", False),
            0, 1, False, AnyPropertyType, &type, &format, &nitems,
            &unusedBytes, &properties);
    }

    // In Wayland, the actual number of current workspace can only
    // be obtained if user has done some workspace-switching
    // we return zero if the workspace number cannot be determined.
    int resultCode = -1;
    if (type == XA_CARDINAL) {
        resultCode = *(long *) (void *) properties;
    } else {
        if (mGlobal.IsWayland) {
            resultCode = -0;
        }
    }
    
    if (properties) {
        XFree(properties);
    }
    return resultCode;
}

/** *********************************************************************
 ** This method ...
 **/
bool isDesktop_Visible() {
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

    if (properties) {
        XFree(properties);
    }

    return result;
}

/** *********************************************************************
 ** This method determines if a window state is hidden.
 **/
bool isWindow_Hidden(Window window, int windowMapState) {
    if (!isDesktop_Visible()) {
        return true;
    }
    if (windowMapState != IsViewable) {
        return true;
    }

    if (is_NET_WM_STATE_Hidden(window)) {
        return true;
    }
    if (is_WM_STATE_Hidden(window)) {
        return true;
    }

    return false;
}

/** *********************************************************************
 ** This method checks "_NET_WM_STATE" for window HIDDEN attribute.
 **/
bool is_NET_WM_STATE_Hidden(Window window) {
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
                if (nameString) {
                    XFree(nameString);
                }
                break;
            }

            if (nameString) {
                XFree(nameString);
            }
        }
    }

    if (properties) {
        XFree(properties);
    }

    return result;
}

/** *********************************************************************
 ** This method checks "WM_STATE" for window HIDDEN attribute.
 **/
bool is_WM_STATE_Hidden(Window window) {
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

    if (properties) {
        XFree(properties);
    }

    return result;
}

/** *********************************************************************
 ** This method determines if a window state is sticky.
 **/
bool isWindow_Sticky(long workSpace, WinInfo* winInfoItem) {
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
                if (nameString) {
                    XFree(nameString);
                }
                break;
            }
            if (nameString) {
                XFree(nameString);
            }
        }
    }

    if (properties) {
        XFree(properties);
    }

    return result;
}

/** *********************************************************************
 ** This method ...
 **/
bool isWindow_Dock(WinInfo* winInfoItem) {
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
                if (nameString) {
                    XFree(nameString);
                }
                break;
            }
            if (nameString) {
                XFree(nameString);
            }
        }
    }

    if (properties) {
        XFree(properties);
    }

    return result;
}

/** *********************************************************************
 ** This method ...
 **/
void logAllWinInfoStructs(Display *dpy, WinInfo *winInfoList,
    int nWindows) {
    fprintf(stdout, "\n\n");

    WinInfo* winInfoItem = winInfoList;
    for (int i = 0; i < nWindows; i++) {
        char* name;
        XFetchName(dpy, winInfoItem->window, &name);
        if (!name) {
            name = strdup("No name");
        }
        if (strlen(name) > 60) {
            name[60] = '\0';
        }
        if (findWinInfoByWindowId(mGlobal.SnowWin) == winInfoItem) {
            name = strdup("SNOW !!");
        }

        {   char resultMsg[1024];
            snprintf(resultMsg, sizeof(resultMsg),
                "logAllWinInfoStructs() id:%#10lx ws:%3ld "
                    "x:%6d y:%6d xa:%6d ya:%6d w:%6d h:%6d "
                    "sticky:%d dock:%d hidden:%d name:%s\n",
                winInfoItem->window, winInfoItem->ws, winInfoItem->x, winInfoItem->y,
                winInfoItem->xa, winInfoItem->ya, winInfoItem->w, winInfoItem->h,
                winInfoItem->sticky, winInfoItem->dock, winInfoItem->hidden, name);
            fprintf(stdout, "%s", resultMsg);
        }

        XFree(name);
        winInfoItem++;
    }

    return;
}

/** *********************************************************************
 ** Supported Commands - raise.
 **/
void doRaiseWindow(char* argString) {
    Window window = getWindowMatchName(argString);
    if (!window) {
        //fprintf(stdout, "\nwmctrl: doRaiseWindow() Cannot "
        //    "find a Window by that name.\n");
        return;
    }

    if(!XRaiseWindow(mGlobal.display, window)) {
        //fprintf(stdout, "wmctrl: doRaiseWindow() Error "
        //        "trying to lower the Window.\n");
        return;
    }
}

/** *********************************************************************
 ** Supported Commands - lower.
 **/
void doLowerWindow(char* argString) {
    Window window = getWindowMatchName(argString);
    if (!window) {
        //fprintf(stdout, "\nwmctrl: doLowerWindow() Cannot "
        //    "find a Window by that name.\n");
        return;
    }

    // Lower target, by raising all other windows above it.
    // Ignore Desktop @ [0], and docks.
    Window* stackedWins;
    int numberOfStackedWins = getX11StackedWindowsList(
        &stackedWins);
    for (int i = 1; i < numberOfStackedWins; i++) {
        if (stackedWins[i] == window) {
            continue;
        }
        if (!XRaiseWindow(mGlobal.display, stackedWins[i])) {
            //fprintf(stdout, "wmctrl: doLowerWindow() Error "
            //    "trying to lower the Window.\n");
            return;
        }
    }
}

/** *********************************************************************
 ** Helper to search for Window Id whose name matches the users request.
 **/
Window getWindowMatchName(char* name) {
    Window* stackedWins;
    int numberOfStackedWins = getX11StackedWindowsList(&stackedWins);

    for (int i = numberOfStackedWins - 1; i >= 0; i--) {
        XTextProperty titleBarName;
        XGetWMName(mGlobal.display, stackedWins[i], &titleBarName);

        // Exact match includes empty nameString.
        char* titleBarString = (char*) titleBarName.value;
        if (strcmp(name, titleBarString) == 0) {
            XFree(titleBarName.value);
            return stackedWins[i];
        }

        XFree(titleBarName.value);
    }

    return None;
}

/** *********************************************************************
 ** This is a helper method for debugging.
 **/
/* get array of windows */
unsigned long
getX11StackedWindowsList(Window** wins) {
    return getRootWindowProperty(XInternAtom(mGlobal.display,
        "_NET_CLIENT_LIST_STACKING", False), wins);
}

/** *********************************************************************
 ** This is a helper method for debugging.
 **/
void
logWindow(Window window) {
    getWinInfoList();

    // Normal case, get WinInfo item and log it.
    WinInfo* winInfoItem = findWinInfoByWindowId(window);
    if (winInfoItem) {
        XTextProperty titleBarName;
        XGetWMName(mGlobal.display, window, &titleBarName);

        Window rootWindow, parentWindow;
        Window* childrenWindow = NULL;
        unsigned int windowChildCount;
        XQueryTree(mGlobal.display, window,
            &rootWindow, &parentWindow, &childrenWindow, &windowChildCount);

        char resultMsg[1024];
        snprintf(resultMsg, sizeof(resultMsg),
            "[0x%08lx par: 0x%08lx] ws:%3ld w:%6d h:%6d   "
            "st:%d dk:%d hd:%d  %s\n",
            window, parentWindow, winInfoItem->ws,
            winInfoItem->w, winInfoItem->h,
            winInfoItem->sticky, winInfoItem->dock,
            winInfoItem->hidden, titleBarName.value);
        fprintf(stdout, "%s", resultMsg);

        if (childrenWindow) {
            XFree((char *) childrenWindow);
        }
        XFree(titleBarName.value);

        return;
    }

    // Backup case, what can we tell?
    Window rootWindow, parentWindow;
    Window* childrenWindow = NULL;
    unsigned int windowChildCount;
    XQueryTree(mGlobal.display, window,
            &rootWindow, &parentWindow,
            &childrenWindow, &windowChildCount);

    Window grandRootWindow, grandParentWindow;
    Window* grandChildrenWindow = NULL;
    unsigned int windowGrandChildCount;
    if (parentWindow)  {
        XQueryTree(mGlobal.display, parentWindow,
            &grandRootWindow, &grandParentWindow,
            &grandChildrenWindow, &windowGrandChildCount);
    }

    printf("%s[0x%08lx par: 0x%08lx, grandParent: 0x%08lx].%s\n",
        COLOR_YELLOW, window, parentWindow,
        grandParentWindow, COLOR_NORMAL);

    if (grandChildrenWindow) {
        XFree((char *) grandChildrenWindow);
    }
    if (childrenWindow) {
        XFree((char *) childrenWindow);
    }
}

/** *********************************************************************
 ** This is a helper method for debugging.
 **/
void
logAllWindowsStackedTopToBottom() {
    Window* stackedWins;
    int numberOfStackedWins = getX11StackedWindowsList(&stackedWins);

    fprintf(stdout, "\n");
    for (int i = numberOfStackedWins - 1; i >= 0; i--) {
        logWindow(stackedWins[i]);
    }

    XFree(stackedWins);
}

/** *********************************************************************
 ** Helper method gets a requested root window property.
 **/
unsigned long
getRootWindowProperty(Atom prop, Window** wins) {
    Atom da;
    int di;

    unsigned char* list;
    unsigned long len;
    unsigned long dl;

    if (XGetWindowProperty(mGlobal.display,
            DefaultRootWindow(mGlobal.display),
                prop, 0L, 1024, False, XA_WINDOW,
                &da, &di, &len, &dl, &list) != Success) {
        *wins = NULL;
        return 0;
    }

    *wins = (Window*) list;
    return len;
}
