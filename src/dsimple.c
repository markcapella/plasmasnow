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

   Copyright 1993, 1998  The Open Group

   Permission to use, copy, modify, distribute, and sell this software and its
   documentation for any purpose is hereby granted without fee, provided that
   the above copyright notice appear in all copies and that both that
   copyright notice and this permission notice appear in supporting
   documentation.

   The above copyright notice and this permission notice shall be included
   in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
   OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
   IN NO EVENT SHALL THE OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR
   OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
   ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
   OTHER DEALINGS IN THE SOFTWARE.

   Except as contained in this notice, the name of The Open Group shall
   not be used in advertising or otherwise to promote the sale, use or
   other dealings in this Software without prior written authorization
   from The Open Group.

*/

#include <X11/Xlib.h>
#include <X11/Xos.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "vroot.h"
/*
 * Other_stuff.h: Definitions of routines in other_stuff.
 *
 * Written by Mark Lillibridge.   Last updated 7/1/87
 */

#include "clientwin.h"
#include "dsimple.h"

static int screen = 0;
static Display *dpy = NULL;
static void Fatal_Error(const char *, ...) _X_NORETURN
    _X_ATTRIBUTE_PRINTF(1, 2);

static char *Get_Display_Name(int *, char **);
static Display *Open_Display(const char *);
static void Close_Display();

// added:
static void usage() {
    printf("Problems in %s, exiting.\n", __FILE__);
    exit(1);
}

/*
 * Just_display: A group of routines designed to make the writting of simple
 *               X11 applications which open a display but do not open
 *               any windows much faster and easier.  Unless a routine says
 *               otherwise, it may be assumed to require program_name, dpy,
 *               and screen already defined on entry.
 *
 * Written by Mark Lillibridge.   Last updated 7/1/87
 */

/* This stuff is defined in the calling program by just_display.h */
const char* program_name = "plasmasnow";

/*
 * Get_Display_Name (argc, argv) Look for -display, -d, or host:dpy (obselete)
 * If found, remove it from command line.  Don't go past a lone -.
 */
char *Get_Display_Name(int *pargc, /* MODIFIED */
    char **argv)                   /* MODIFIED */
{
    int argc = *pargc;
    char **pargv = argv + 1;
    char *displayname = NULL;
    int i;

    for (i = 1; i < argc; i++) {
        char *arg = argv[i];

        if (!strcmp(arg, "-display") || !strcmp(arg, "-d")) {
            if (++i >= argc) {
                usage();
            }

            displayname = argv[i];
            *pargc -= 2;
            continue;
        }
        if (!strcmp(arg, "-")) {
            while (i < argc) {
                *pargv++ = argv[i++];
            }
            break;
        }
        *pargv++ = arg;
    }

    *pargv = NULL;
    return (displayname);
}

/*
 * Open_Display: Routine to open a display with correct error handling.
 *               Does not require dpy or screen defined on entry.
 */
Display *Open_Display(const char *display_name) {
    Display *d;

    d = XOpenDisplay(display_name);
    if (d == NULL) {
        fprintf(stderr, "%s:  unable to open display '%s'\n", program_name,
            XDisplayName(display_name));
        exit(1);
    }

    return (d);
}

/*
 * Setup_Display_And_Screen: This routine opens up the correct display (i.e.,
 *                           it calls Get_Display_Name) and then stores a
 *                           pointer to it in dpy.  The default screen
 *                           for this display is then stored in screen.
 *                           Does not require dpy or screen defined.
 */
void Setup_Display_And_Screen(int *argc, /* MODIFIED */
    char **argv)                         /* MODIFIED */
{
    char *displayname = NULL;

    displayname = Get_Display_Name(argc, argv);
    dpy = Open_Display(displayname);
    screen = XDefaultScreen(dpy);
}

/*
 * Close_Display: Close display
 */
void Close_Display(void) {
    if (dpy == NULL) {
        return;
    }

    XCloseDisplay(dpy);
    dpy = NULL;
}

/*
 * Open_Font: This routine opens a font with error handling.
 */
XFontStruct *Open_Font(const char *name) {
    XFontStruct *font;

    if (!(font = XLoadQueryFont(dpy, name))) {
        Fatal_Error("Unable to open font %s!", name);
    }

    return (font);
}

/*
 * Select_Window_Args: a rountine to provide a common interface for
 *                     applications that need to allow the user to select one
 *                     window on the screen for special consideration.
 *                     This routine implements the following command line
 *                     arguments:
 *
 *                       -root            Selects the root window.
 *                       -id <id>         Selects window with id <id>. <id> may
 *                                        be either in decimal or hex.
 *                       -name <name>     Selects the window with name <name>.
 *
 *                     Call as Select_Window_Args(&argc, argv) in main before
 *                     parsing any of your program's command line arguments.
 *                     Select_Window_Args will remove its arguments so that
 *                     your program does not have to worry about them.
 *                     The window returned is the window selected or 0 if
 *                     none of the above arguments was present.  If 0 is
 *                     returned, Select_Window should probably be called after
 *                     all command line arguments, and other setup is done.
 *                     For examples of usage, see xwininfo, xwd, or xprop.
 */
Window Select_Window_Args(int *rargc, char **argv)
#define ARGC (*rargc)
{
    int nargc = 1;
    int argc;
    char **nargv;
    Window w = 0;

    nargv = argv + 1;
    argc = ARGC;
#define OPTION argv[0]
#define NXTOPTP ++argv, --argc > 0
#define NXTOPT                                                                 \
    if (++argv, --argc == 0)                                                   \
    usage()
#define COPYOPT nargv++[0] = OPTION, nargc++

    while (NXTOPTP) {
        if (!strcmp(OPTION, "-")) {
            COPYOPT;
            while (NXTOPTP) {
                COPYOPT;
            }
            break;
        }
        if (!strcmp(OPTION, "-root")) {
            w = RootWindow(dpy, screen);
            continue;
        }
        if (!strcmp(OPTION, "-name")) {
            NXTOPT;
            w = Window_With_Name(dpy, RootWindow(dpy, screen), OPTION);
            if (!w) {
                Fatal_Error("No window with name %s exists!", OPTION);
            }
            continue;
        }
        if (!strcmp(OPTION, "-id")) {
            NXTOPT;
            w = 0;
            sscanf(OPTION, "0x%lx", &w);
            if (!w) {
                sscanf(OPTION, "%lu", &w);
            }
            if (!w) {
                Fatal_Error("Invalid window id format: %s.", OPTION);
            }
            continue;
        }
        COPYOPT;
    }
    ARGC = nargc;

    return (w);
}

/*
 * Other_stuff: A group of routines which do common X11 tasks.
 *
 * Written by Mark Lillibridge.   Last updated 7/1/87
 */

/*
 * Routine to let user select a window using the mouse
 */

Window Select_Window(Display *dpy, int descend) {
    int status;
    Cursor cursor;
    XEvent event;
    Window target_win = None, root = RootWindow(dpy, screen);
    int buttons = 0;

    /* Make the target cursor */
    cursor = XCreateFontCursor(dpy, XC_crosshair);

    /* Grab the pointer using target cursor, letting it room all over */
    status = XGrabPointer(dpy, root, False, ButtonPressMask | ButtonReleaseMask,
        GrabModeSync, GrabModeAsync, root, cursor, CurrentTime);
    if (status != GrabSuccess) {
        Fatal_Error("Can't grab the mouse.");
    }

    /* Let the user select a window... */
    while ((target_win == None) || (buttons != 0)) {
        /* allow one more event */
        XAllowEvents(dpy, SyncPointer, CurrentTime);
        XWindowEvent(dpy, root, ButtonPressMask | ButtonReleaseMask, &event);
        switch (event.type) {
        case ButtonPress:
            if (target_win == None) {
                target_win = event.xbutton.subwindow; /* window selected */
                if (target_win == None) {
                    target_win = root;
                }
            }
            buttons++;
            break;
        case ButtonRelease:
            if (buttons >
                0) { /* there may have been some down before we started */
                buttons--;
            }
            break;
        }
    }

    XUngrabPointer(dpy, CurrentTime); /* Done with pointer */

    if (!descend || (target_win == root)) {
        return (target_win);
    }

    target_win = Find_Client(dpy, root, target_win);

    return (target_win);
}

/*
 * Window_With_Name: routine to locate a window with a given name on a display.
 *                   If no window with the given name is found, 0 is returned.
 *                   If more than one window has the given name, the first
 *                   one found will be returned.  Only top and its subwindows
 *                   are looked at.  Normally, top should be the RootWindow.
 */
Window Window_With_Name(Display *dpy, Window top, const char *name) {
    Window *children, dummy;
    unsigned int nchildren;
    int i;
    Window w = 0;
    char *window_name;

    // this leaks memory:
    // if (XFetchName(dpy, top, &window_name) && !strcmp(window_name, name))
    //  return(top);
    //  therefore:
    if (XFetchName(dpy, top, &window_name) && !strcmp(window_name, name)) {
        XFree(window_name);
        return (top);
    }
    XFree(window_name);

    if (!XQueryTree(dpy, top, &dummy, &dummy, &children, &nchildren)) {
        return (0);
    }

    for (i = 0; (unsigned int)i < nchildren; i++) {
        w = Window_With_Name(dpy, children[i], name);
        if (w) {
            break;
        }
    }
    if (children) {
        XFree((char *)children);
    }
    return (w);
}

/*
 * Standard fatal error routine - call like printf but maximum of 7 arguments.
 * Does not require dpy or screen defined.
 */
void Fatal_Error(const char *msg, ...) {
    va_list args;
    fflush(stdout);
    fflush(stderr);
    fprintf(stderr, "%s: error: ", program_name);
    va_start(args, msg);
    vfprintf(stderr, msg, args);
    va_end(args);
    fprintf(stderr, "\n");
    Close_Display();
    exit(EXIT_FAILURE);
}
