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
// from the program xprop
/*
 * Copyright 2007 Kim woelders
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation, and
 * that the name of the copyright holders not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  The copyright holders make no representations
 * about the suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THIS SOFTWARE.
 */
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <pthread.h>

#include "clientwin.h"

static Atom atom_wm_state = None;

/*
 * Check if window has given property
 */
static Bool Window_Has_Property(Display *dpy, Window win, Atom atom) {
    Atom type_ret;
    int format_ret;
    unsigned char *prop_ret;
    unsigned long bytes_after, num_ret;

    type_ret = None;
    prop_ret = NULL;
    XGetWindowProperty(dpy, win, atom, 0, 0, False, AnyPropertyType, &type_ret,
        &format_ret, &num_ret, &bytes_after, &prop_ret);
    if (prop_ret) {
        XFree(prop_ret);
    }

    return (type_ret != None) ? True : False;
}

/*
 * Check if window is viewable
 */
static Bool Window_Is_Viewable(Display *dpy, Window win) {
    Bool ok;
    XWindowAttributes xwa;

    XGetWindowAttributes(dpy, win, &xwa);

#ifdef __cplusplus
    ok = (xwa.c_class == InputOutput) && (xwa.map_state == IsViewable);
#else
    ok = (xwa.class == InputOutput) && (xwa.map_state == IsViewable);
#endif

    return ok;
}

/*
 * Find a window that has WM_STATE set in the window tree below win.
 * Unmapped/unviewable windows are not considered valid matches.
 * Children are searched in top-down stacking order.
 * The first matching window is returned, None if no match is found.
 */
static Window Find_Client_In_Children(Display *dpy, Window win) {
    Window root, parent;
    Window *children;
    unsigned int n_children;
    int i;

    if (!XQueryTree(dpy, win, &root, &parent, &children, &n_children)) {
        return None;
    }
    if (!children) {
        return None;
    }

    /* Check each child for WM_STATE and other validity */
    win = None;
    for (i = (int)n_children - 1; i >= 0; i--) {
        if (!Window_Is_Viewable(dpy, children[i])) {
            children[i] = None; /* Don't bother descending into this one */
            continue;
        }
        if (!Window_Has_Property(dpy, children[i], atom_wm_state)) {
            continue;
        }

        /* Got one */
        win = children[i];
        goto done;
    }

    /* No children matched, now descend into each child */
    for (i = (int)n_children - 1; i >= 0; i--) {
        if (children[i] == None) {
            continue;
        }
        win = Find_Client_In_Children(dpy, children[i]);
        if (win != None) {
            break;
        }
    }

done:
    XFree(children);

    return win;
}

/*
 * Find virtual roots (_NET_VIRTUAL_ROOTS)
 */
static unsigned long *Find_Roots(Display *dpy, Window root, unsigned int *num) {
    Atom type_ret;
    int format_ret;
    unsigned char *prop_ret;
    unsigned long bytes_after, num_ret;
    Atom atom;

    *num = 0;
    atom = XInternAtom(dpy, "_NET_VIRTUAL_ROOTS", False);
    if (!atom) {
        return NULL;
    }

    type_ret = None;
    prop_ret = NULL;
    if (XGetWindowProperty(dpy, root, atom, 0, 0x7fffffff, False, XA_WINDOW,
            &type_ret, &format_ret, &num_ret, &bytes_after,
            &prop_ret) != Success) {
        return NULL;
    }

    if (prop_ret && type_ret == XA_WINDOW && format_ret == 32) {
        *num = num_ret;
        return ((unsigned long *)(void *)prop_ret);
    }
    if (prop_ret) {
        XFree(prop_ret);
    }

    return NULL;
}

/*
 * Find child window at pointer location
 */
static Window Find_Child_At_Pointer(Display *dpy, Window win) {
    Window root_return, child_return;
    int dummyi;
    unsigned int dummyu;

    XQueryPointer(dpy, win, &root_return, &child_return, &dummyi, &dummyi,
        &dummyi, &dummyi, &dummyu);

    return child_return;
}

/*
 * Find client window at pointer location
 *
 * root   is the root window.
 * subwin is the subwindow reported by a ButtonPress event on root.
 *
 * If the WM uses virtual roots subwin may be a virtual root.
 * If so, we descend the window stack at the pointer location and assume the
 * child is the client or one of its WM frame windows.
 * This will of course work only if the virtual roots are children of the real
 * root.
 */
Window Find_Client(Display *dpy, Window root, Window subwin) {
    unsigned long *roots;
    unsigned int i, n_roots;
    Window win;

    /* Check if subwin is a virtual root */
    roots = Find_Roots(dpy, root, &n_roots);
    for (i = 0; i < n_roots; i++) {
        if (subwin != roots[i]) {
            continue;
        }
        win = Find_Child_At_Pointer(dpy, subwin);
        if (win == None) {
            return subwin; /* No child - Return virtual root. */
        }
        subwin = win;
        break;
    }
    if (roots) {
        XFree(roots);
    }

    if (atom_wm_state == None) {
        atom_wm_state = XInternAtom(dpy, "WM_STATE", False);
        if (!atom_wm_state) {
            return subwin;
        }
    }

    /* Check if subwin has WM_STATE */
    if (Window_Has_Property(dpy, subwin, atom_wm_state)) {
        return subwin;
    }

    /* Attempt to find a client window in subwin's children */
    win = Find_Client_In_Children(dpy, subwin);
    if (win != None) {
        return win; /* Found a client */
    }

    /* Did not find a client */
    return subwin;
}
