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

#include "wmctrl.h"
#include "debug.h"
#include "dsimple.h"
#include "safe_malloc.h"
#include "windows.h"
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "vroot.h"

static void FindWindows(Display *display, Window window,
    long unsigned int *nwindows, Window **windows);
static void FindWindows_r(Display *display, Window window,
    long unsigned int *nwindows, Window **windows);

/* this one is not needed any more, but I keep the source */
void FindWindows(Display *display, Window window, long unsigned int *nwindows,
    Window **windows) {
    *nwindows = 0;
    *windows = NULL;

    FindWindows_r(display, window, nwindows, windows);
    for (int i = 0; i < (int)(*nwindows); i++) {
        P("window: %#lx\n", (*windows)[i]);
    }
}

void FindWindows_r(Display *display, Window window, long unsigned int *nwindows,
    Window **windows) {
    Window root, parent, *children;
    unsigned int nchildren;
    int i;
    static int generations = 0;

    XQueryTree(display, window, &root, &parent, &children, &nchildren);

    ++(*nwindows);
    *windows = (Window *)realloc(*windows, (*nwindows) * sizeof(Window *));
    (*windows)[*nwindows - 1] = window;

    if (nchildren) {
        Window *child;

        ++generations;

        for (i = 0, child = children; i < (int)nchildren; ++i, ++child) {
            FindWindows_r(display, *child, nwindows, windows);
        }

        --generations;
        XFree(children);
    }

    return;
}

long int GetCurrentWorkspace() {
    Atom type;
    int format;
    unsigned long nitems, b;
    unsigned char *properties;
    long int r;
    Display *getdisplay;

    static Atom atom_net_desktop_viewport;
    static Atom atom_net_current_desktop;
    static Atom atom_win_workspace;

    int firstcall = 1;
    if (firstcall) {
        firstcall = 0;
        getdisplay = global.display;

        atom_net_desktop_viewport =
            XInternAtom(getdisplay, "_NET_DESKTOP_VIEWPORT", False);
        atom_net_current_desktop =
            XInternAtom(getdisplay, "_NET_CURRENT_DESKTOP", False);
        atom_win_workspace = XInternAtom(getdisplay, "_WIN_WORKSPACE", False);
    }

    P("GetCurrentWorkspace %p %d\n", (void *)getdisplay, counter++);
    if (global.IsCompiz) {
        P("compiz\n");
        properties = NULL;
        XGetWindowProperty(getdisplay, DefaultRootWindow(getdisplay),
            atom_net_desktop_viewport, 0, 2, False, AnyPropertyType, &type,
            &format, &nitems, &b, &properties);
        if (type != XA_CARDINAL || nitems != 2) {
            r = -1;
        } else {
            // we have the x-y coordinates of the workspace, we hussle this
            // into one long number:
            r = ((long *)(void *)properties)[0] +
                (((long *)(void *)properties)[1] << 16);
        }
        if (properties) {
            XFree(properties);
        }
    } else {
        properties = NULL;
        XGetWindowProperty(getdisplay, DefaultRootWindow(getdisplay),
            atom_net_current_desktop, 0, 1, False, AnyPropertyType, &type,
            &format, &nitems, &b, &properties);
        P("type: %ld %ld\n", type, XA_CARDINAL);
        P("properties: %d %d %d %ld\n", properties[0], properties[1], format,
            nitems);
        if (type != XA_CARDINAL) {
            P("and again %ld ...\n", type);
            if (properties) {
                XFree(properties);
            }
            XGetWindowProperty(getdisplay, DefaultRootWindow(getdisplay),
                atom_win_workspace, 0, 1, False, AnyPropertyType, &type,
                &format, &nitems, &b, &properties);
        }
        if (type != XA_CARDINAL) {
            if (global.IsWayland) {
                // in Wayland, the actual number of current workspace can only
                // be obtained if user has done some workspace-switching
                // we return zero if the workspace number cannot be determined

                r = 0;
            } else {
                r = -1;
            }
            r = 0; // second thought: always return 0 here
                   //        so things will run in enlightenment also
                   //        more or less ;-)
        } else {
            r = *(long *)(void *)properties; // see man XGetWindowProperty
        }
        if (properties) {
            XFree(properties);
        }
    }
    P("wmctrl: nitems: %ld ws: %d\n", nitems, r);

    return r;
}

int GetWindows(WinInfo **windows, int *nwin) {
    Atom type;
    int format;
    unsigned long b;
    unsigned char *properties = NULL;
    (*windows) = NULL;
    static Display *getdisplay;
    ;
    Window *children;
    long unsigned int nchildren;

    static Atom atom_gtk_frame_extents;
    static Atom atom_net_client_list;
    static Atom atom_net_frame_extents;
    static Atom atom_net_showing_desktop;
    static Atom atom_net_wm_desktop;
    static Atom atom_net_wm_state;
    static Atom atom_net_wm_window_type;
    static Atom atom_win_client_list;
    static Atom atom_win_workspace;
    static Atom atom_wm_state;

    static int firstcall = 1;
    if (firstcall) {
        firstcall = 0;
        getdisplay = global.display;

        atom_gtk_frame_extents =
            XInternAtom(getdisplay, "_GTK_FRAME_EXTENTS", False);
        atom_net_client_list =
            XInternAtom(getdisplay, "_NET_CLIENT_LIST", False);
        atom_net_frame_extents =
            XInternAtom(getdisplay, "_NET_FRAME_EXTENTS", False);
        atom_net_showing_desktop =
            XInternAtom(getdisplay, "_NET_SHOWING_DESKTOP", False);
        atom_net_wm_desktop = XInternAtom(getdisplay, "_NET_WM_DESKTOP", False);
        atom_net_wm_state = XInternAtom(getdisplay, "_NET_WM_STATE", False);
        atom_net_wm_window_type =
            XInternAtom(getdisplay, "_NET_WM_WINDOW_TYPE", False);
        atom_win_client_list =
            XInternAtom(getdisplay, "_WIN_CLIENT_LIST", False);
        atom_win_workspace = XInternAtom(getdisplay, "_WIN_WORKSPACE", False);
        atom_wm_state = XInternAtom(getdisplay, "WM_STATE", False);
    }

    XGetWindowProperty(getdisplay, DefaultRootWindow(getdisplay),
        atom_net_client_list, 0, 1000000, False, AnyPropertyType, &type,
        &format, &nchildren, &b, (unsigned char **)&children);
    if (type == XA_WINDOW) {
        P("_NET_CLIENT_LIST succeeded\n");
    } else {
        P("No _NET_CLIENT_LIST, trying _WIN_CLIENT_LIST\n");
        if (children) {
            XFree(children);
            children = NULL;
        }
        XGetWindowProperty(getdisplay, DefaultRootWindow(getdisplay),
            atom_win_client_list, 0, 1000000, False, AnyPropertyType, &type,
            &format, &nchildren, &b, (unsigned char **)&children);
        if (type == XA_WINDOW) {
            P("_WIN_CLIENT_LIST succeeded\n");
        }
    }
    if (type != XA_WINDOW) {
        P("No _WIN_CLIENT_LIST, trying XQueryTree\n");
        if (children) {
            XFree(children);
            children = NULL;
        }
        if (0) {
            FindWindows(getdisplay,
                RootWindow(getdisplay, DefaultScreen(getdisplay)), &nchildren,
                (Window **)&properties);
        } else {
            Window dummy;
            unsigned int n;
            XQueryTree(getdisplay, DefaultRootWindow(getdisplay), &dummy,
                &dummy, &children, &n);
            nchildren = n;
        }
    }
    P("----------------------------------------- nchildren: %ld\n", nchildren);
    (*nwin) = nchildren;
    (*windows) = NULL;
    if (nchildren > 0) {
        (*windows) = (WinInfo *)malloc(nchildren * sizeof(WinInfo));
    }
    WinInfo *w = (*windows);
    int k = 0;

    // and yet another check if window is hidden (needed e.g. in KDE/plasma
    // after 'hide all windows')
    int globalhidden = 0;
    {
        if (w) {
            P("hidden3 %d %#lx\n", counter++, w->id);
        } else {
            P("hidden3 %d %#lx\n", counter++, w);
        }

        if (atom_net_showing_desktop) {
            Atom type;
            unsigned long nitems, b;
            int format;
            unsigned char *properties = NULL;
            P("hidden3 try _NET_SHOWING_DESKTOP\n");
            XGetWindowProperty(getdisplay, global.Rootwindow,
                atom_net_showing_desktop, 0, (~0L), False, AnyPropertyType,
                &type, &format, &nitems, &b, &properties);
            if (format == 32 && nitems >= 1) {
                if (*(long *)(void *)properties == 1) {
                    globalhidden = 1;
                }
                P("hidden3 hidden:%d\n", globalhidden);
            }
            if (properties) {
                XFree(properties);
            }
        }
    }

    unsigned long i;
    for (i = 0; i < nchildren; i++) {
        int x0, y0, xr, yr;
        unsigned int depth;

        w->id = children[i];

        XWindowAttributes winattr;
        XGetWindowAttributes(getdisplay, w->id, &winattr);

        x0 = winattr.x;
        y0 = winattr.y;
        w->w = winattr.width;
        w->h = winattr.height;
        depth = winattr.depth;

        P("%d %#lx %d %d %d %d %d\n", counter++, w->id, x0, y0, w->w, w->h,
            depth);
        // if this window is showing nothing, we ignore it:
        if (depth == 0) {
            continue;
        }

        Window child_return;
        XTranslateCoordinates(getdisplay, w->id, global.Rootwindow, 0, 0, &xr,
            &yr, &child_return);
        w->xa = xr - x0;
        w->ya = yr - y0;
        P("%d %#lx %d %d %d %d %d\n", counter++, w->id, w->xa, w->ya, w->w,
            w->h, depth);

        XTranslateCoordinates(getdisplay, w->id, global.SnowWin, 0, 0, &(w->x),
            &(w->y), &child_return);

        enum { NET, GTK };
        Atom type;
        int format;
        unsigned long nitems, b;
        unsigned char *properties = NULL;
        XGetWindowProperty(getdisplay, w->id, atom_net_wm_desktop, 0, 1, False,
            AnyPropertyType, &type, &format, &nitems, &b, &properties);
        if (type != XA_CARDINAL) {
            if (properties) {
                XFree(properties);
            }
            properties = NULL;
            XGetWindowProperty(getdisplay, w->id, atom_win_workspace, 0, 1,
                False, AnyPropertyType, &type, &format, &nitems, &b,
                &properties);
        }
        if (properties) {
            w->ws = *(long *)(void *)properties;
            if (properties) {
                XFree(properties);
            }
        } else {
            w->ws = 0;
        }
        // maybe this window is sticky:
        w->sticky = 0;
        properties = NULL;
        nitems = 0;
        XGetWindowProperty(getdisplay, w->id, atom_net_wm_state, 0, (~0L),
            False, AnyPropertyType, &type, &format, &nitems, &b, &properties);
        if (type == XA_ATOM) {
            int i;
            for (i = 0; (unsigned long)i < nitems; i++) {
                char *s = NULL;
                s = XGetAtomName(getdisplay, ((Atom *)(void *)properties)[i]);
                if (!strcmp(s, "_NET_WM_STATE_STICKY")) {
                    P("%#lx is sticky\n", w->id);
                    w->sticky = 1;
                    if (s) {
                        XFree(s);
                    }
                    break;
                }
                if (s) {
                    XFree(s);
                }
            }
        }
        // another sticky test, needed in KDE en LXDE:
        if (w->ws == -1) {
            w->sticky = 1;
        }
        if (properties) {
            XFree(properties);
        }

        // check if window is a "dock".
        w->dock = 0;
        properties = NULL;
        nitems = 0;
        XGetWindowProperty(getdisplay, w->id, atom_net_wm_window_type, 0, (~0L),
            False, AnyPropertyType, &type, &format, &nitems, &b, &properties);
        if (format == 32) {
            int i;
            for (i = 0; (unsigned long)i < nitems; i++) {
                char *s = NULL;
                s = XGetAtomName(getdisplay, ((Atom *)(void *)properties)[i]);
                if (!strcmp(s, "_NET_WM_WINDOW_TYPE_DOCK")) {
                    P("%#lx is dock %d\n", w->id, counter++);
                    w->dock = 1;
                    if (s) {
                        XFree(s);
                    }
                    break;
                }
                if (s) {
                    XFree(s);
                }
            }
        }
        if (properties) {
            XFree(properties);
        }

        // check if window is hidden
        w->hidden = globalhidden;
        if (!w->hidden) {
            if (winattr.map_state != IsViewable) {
                P("map_state: %#lx %d\n", w->id, winattr.map_state);
                w->hidden = 1;
            }
        }
        // another check on hidden
        if (!w->hidden) {
            properties = NULL;
            nitems = 0;
            XGetWindowProperty(getdisplay, w->id, atom_net_wm_state, 0, (~0L),
                False, AnyPropertyType, &type, &format, &nitems, &b,
                &properties);
            if (format == 32) {
                unsigned long i;
                for (i = 0; i < nitems; i++) {
                    char *s = NULL;
                    s = XGetAtomName(
                        getdisplay, ((Atom *)(void *)properties)[i]);
                    if (!strcmp(s, "_NET_WM_STATE_HIDDEN")) {
                        P("%#lx is hidden %d\n", w->id, counter++);
                        w->hidden = 1;
                        if (s) {
                            XFree(s);
                        }
                        break;
                    }
                    if (s) {
                        XFree(s);
                    }
                }
            }
            if (properties) {
                XFree(properties);
            }
        }

        // yet another check if window is hidden:
        if (!w->hidden) {
            P("hidden2 %#lx\n", w->id);
            properties = NULL;
            nitems = 0;
            XGetWindowProperty(getdisplay, w->id, atom_wm_state, 0, (~0L),
                False, AnyPropertyType, &type, &format, &nitems, &b,
                &properties);
            if (format == 32 && nitems >= 1) {
                // see https://tronche.com/gui/x/icccm/sec-4.html#s-4.1.3.1
                // WithDrawnState: 0
                // NormalState:    1
                // IconicState:    3
                if (*(long *)(void *)properties != NormalState) {
                    w->hidden = 1;
                }
            }
            if (properties) {
                XFree(properties);
            }
        }

        properties = NULL;
        nitems = 0;

        // first try to get adjustments for _GTK_FRAME_EXTENTS
        if (atom_gtk_frame_extents) {
            XGetWindowProperty(getdisplay, w->id, atom_gtk_frame_extents, 0, 4,
                False, AnyPropertyType, &type, &format, &nitems, &b,
                &properties);
        }
        int wintype = GTK;
        // if not succesfull, try _NET_FRAME_EXTENTS
        if (nitems != 4) {
            if (properties) {
                XFree(properties);
            }
            properties = NULL;
            P("trying net...\n");
            XGetWindowProperty(getdisplay, w->id, atom_net_frame_extents, 0, 4,
                False, AnyPropertyType, &type, &format, &nitems, &b,
                &properties);
            wintype = NET;
        }
        P("nitems: %ld %ld %d\n", type, nitems, format);
        if (nitems == 4 && format == 32 && type) // adjust x,y,w,h of window
        {
            long
                *r; // borderleft, borderright, top decoration, bottomdecoration
            r = (long *)(void *)properties;
            P("RRRR: %ld %ld %ld %ld\n", r[0], r[1], r[2], r[3]);
            switch (wintype) {
            case NET:
                P("NET\n");
                w->x -= r[0];
                w->y -= r[2];
                w->w += r[0] + r[1];
                w->h += r[2] + r[3];
                break;
            case GTK:
                P("%d: GTK\n", global.counter++);
                w->x += r[0];
                w->y += r[2];
                w->w -= (r[0] + r[1]);
                w->h -= (r[2] + r[3]);
                break;
            default:
                I("plasmasnow encountered a serious problem, exiting ...\n");
                exit(1);
                break;
            }
            P("%d: NET/GTK: %#lx %d %d %d %d\n", w->id, w->ws, w->x, w->y, w->w,
                w->h);
        } else {
            // this is a problem....
            // In for example TWM, neither NET nor GTK is the case.
            // Let us try this one:
            w->x = x0;
            w->y = y0;
            P("%d %#lx %d %d\n", counter++, w->id, w->x, w->y);
        }
        if (properties) {
            XFree(properties);
        }
        w++;
        k++;
    }
    if (properties) {
        XFree(properties);
    }
    if (children) {
        XFree(children);
    }
    (*nwin) = k;
    P("%d\n", global.counter++); // printwindows(getdisplay,*windows,*nwin);
    return 1;
}

WinInfo *FindWindow(WinInfo *windows, int nwin, Window id) {
    WinInfo *w = windows;
    int i;
    for (i = 0; i < nwin; i++) {
        if (w->id == id) {
            return w;
        }
        w++;
    }
    return NULL;
}

void printwindows(Display *dpy, WinInfo *windows, int nwin) {
    WinInfo *w = windows;
    int i;
    for (i = 0; i < nwin; i++) {
        char *name;
        XFetchName(dpy, w->id, &name);
        if (!name) {
            name = strdup("No name");
        }
        if (strlen(name) > 20) {
            name[20] = '\0';
        }
        printf("id:%#10lx ws:%3ld x:%6d y:%6d xa:%6d ya:%6d w:%6d h:%6d "
               "sticky:%d dock:%d hidden:%d name:%s\n",
            w->id, w->ws, w->x, w->y, w->xa, w->ya, w->w, w->h, w->sticky,
            w->dock, w->hidden, name);
        XFree(name);
        w++;
    }
    return;
}
