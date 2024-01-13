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

#include "windows.h"

#include "debug.h"
#include "dsimple.h"
#include "fallensnow.h"
#include "flags.h"
#include "mygettext.h"
#include "safe_malloc.h"
#include "scenery.h"
#include "transwindow.h"
#include "utils.h"
#include "wmctrl.h"
#include "xdo.h"
#include "plasmasnow.h"

#include <X11/Intrinsic.h>
#include <X11/extensions/Xinerama.h>

#include <ctype.h>

#include <gdk/gdkx.h>
#include <gtk/gtk.h>

#include <pthread.h>
#include <stdio.h>


/***********************************************************
 * Module Method stubs.
 */

// Workspace.
static void DetermineVisualWorkspaces();
static int do_sendevent();
static int updateWindowsList();

 // Windows.
extern void onWindowCreated(Window);
extern void onWindowDestroyed(Window);
extern void onWindowMapped(Window);
extern void onWindowUnmapped(Window);

// Window dragging methods.
Window mWindowBeingDragged = None;
void setWindowBeingDragged(Window);
Window getDragWindowOf(Window);
extern Window getWindowBeingDragged();

// ColorPicker methods.
void uninitQPickerDialog();


/** *********************************************************************
 ** Module globals and consts.
 **/

// Workspace on which transparent window is placed.
static long TransWorkSpace = -SOMENUMBER;

// Main WinInfo (Windows) List.
static int mNumberOfWindows;
static WinInfo* mWindowsList = NULL;


/** *********************************************************************
 ** This method ...
 **/
void windows_init() {
    if (global.Desktop) {
        DetermineVisualWorkspaces();
        add_to_mainloop(PRIORITY_DEFAULT, time_wupdate, updateWindowsList);
    }
    if (!global.IsDouble) {
        add_to_mainloop(PRIORITY_DEFAULT, time_sendevent, do_sendevent);
    }
}

/** *********************************************************************
 ** This method ...
 **/
int WorkspaceActive() {
    P("global.Trans etc %d %d %d %d\n", Flags.AllWorkspaces, global.Trans,
        global.CWorkSpace == TransWorkSpace,
        Flags.AllWorkspaces || !global.Trans ||
            global.CWorkSpace == TransWorkSpace);
    // ah, so difficult ...
    if (Flags.AllWorkspaces) {
        return 1;
    }
    int i;
    for (i = 0; i < global.NVisWorkSpaces; i++) {
        if (global.VisWorkSpaces[i] == global.ChosenWorkSpace) {
            return 1;
        }
    }
    return 0;
}

/** *********************************************************************
 ** This method ...
 **/
int do_sendevent() {
    XExposeEvent event;

    event.type = Expose;
    event.send_event = True;
    event.display = global.display;
    event.window = global.SnowWin;
    event.x = 0;
    event.y = 0;
    event.width = global.SnowWinWidth;
    event.height = global.SnowWinHeight;

    XSendEvent(global.display, global.SnowWin, True, Expose, (XEvent *)&event);
    return TRUE;
}

/** *********************************************************************
 ** This method ...
 **/
void DetermineVisualWorkspaces() {
    P("%d Entering DetermineVisualWorkspaces\n", global.counter++);
    static XClassHint class_hints;
    static XSetWindowAttributes attr;
    static long valuemask;
    static long hints[5] = {2, 0, 0, 0, 0};
    static Atom motif_hints;
    static XSizeHints wmsize;

    if (!global.Desktop) {
        global.NVisWorkSpaces = 1;
        global.VisWorkSpaces[0] = global.CWorkSpace;
        return;
    }

    static Window probeWindow = 0;
    if (probeWindow) {
        XDestroyWindow(global.display, probeWindow);
    } else {
        P("Creating attrs for probeWindow\n");
        attr.background_pixel = WhitePixel(global.display, global.Screen);
        attr.border_pixel = WhitePixel(global.display, global.Screen);
        attr.event_mask = ButtonPressMask;
        valuemask = CWBackPixel | CWBorderPixel | CWEventMask;
        class_hints.res_name = (char *)"plasmasnow";
        class_hints.res_class = (char *)"plasmasnow";
        motif_hints = XInternAtom(global.display, "_MOTIF_WM_HINTS", False);
        wmsize.flags = USPosition | USSize;
    }

    int number;
    XineramaScreenInfo *info = XineramaQueryScreens(global.display, &number);
    if (number == 1 || info == NULL) {
        global.NVisWorkSpaces = 1;
        global.VisWorkSpaces[0] = global.CWorkSpace;
        return;
    }

    // This is for bspwm and possibly other tiling window magagers.
    // Determine which workspaces are visible: place a window (probeWindow)
    // in each xinerama screen, and ask in which workspace the window
    // is located.
    probeWindow = XCreateWindow(global.display, global.Rootwindow, 1, 1, 1, 1,
        10, DefaultDepth(global.display, global.Screen), InputOutput,
        DefaultVisual(global.display, global.Screen), valuemask, &attr);
    XSetClassHint(global.display, probeWindow, &class_hints);

    // to prevent the user to determine the intial position (in twm for example)
    XSetWMNormalHints(global.display, probeWindow, &wmsize);

    XChangeProperty(global.display, probeWindow, motif_hints, motif_hints, 32,
        PropModeReplace, (unsigned char *) &hints, 5);
    xdo_map_window(global.xdo, probeWindow);

    global.NVisWorkSpaces = number;
    int prev = -SOMENUMBER;
    for (int i = 0; i < number; i++) {
        int x = info[i].x_org;
        int y = info[i].y_org;
        int w = info[i].width;
        int h = info[i].height;

        // place probeWindow in the center of xinerama screen[i]

        int xm = x + w / 2;
        int ym = y + h / 2;
        P("movewindow: %d %d\n", xm, ym);
        xdo_move_window(global.xdo, probeWindow, xm, ym);
        xdo_wait_for_window_map_state(global.xdo, probeWindow, IsViewable);
        long desktop;
        int rc = xdo_get_desktop_for_window(global.xdo, probeWindow, &desktop);
        if (rc == XDO_ERROR) {
            desktop = global.CWorkSpace;
        }
        P("desktop: %ld rc: %d\n", desktop, rc);
        global.VisWorkSpaces[i] = desktop;

        if (desktop != prev) {
            // this is for the case that the xinerama screens belong to
            // different workspaces, as seems to be the case in e.g. bspwm
            if (prev >= 0) {
                global.WindowOffsetX = 0;
                global.WindowOffsetY = 0;
            }
            prev = desktop;
        }
    }
    xdo_unmap_window(global.xdo, probeWindow);
}

/** *********************************************************************
 ** This method ...
 **/
void UpdateFallenSnowRegionsWithLock() {
    Lock_fallen();
    UpdateFallenSnowRegions();
    Unlock_fallen();
}

/** *********************************************************************
 ** This method ...
 **/
void UpdateFallenSnowRegions() {
    FallenSnow *fsnow;

    // add fallensnow regions:
    WinInfo* addWin = mWindowsList;
    for (int i = 0; i < mNumberOfWindows; i++) {
        fsnow = FindFallen(global.FsnowFirst, addWin->id);
        if (fsnow) {
            fsnow->win = *addWin;
            if ((!fsnow->win.sticky) &&
                fsnow->win.ws != global.CWorkSpace) {
                CleanFallenArea(fsnow, 0, fsnow->w);
            }
        }

        // window found in mWindowsList, but not in list of fallensnow,
        // add it, but not if we are snowing or birding in this window
        // (Desktop for example) and also not if this window has y <= 0
        // and also not if this window is a "dock"
        if (!fsnow) {
            if (addWin->id != global.SnowWin &&
                addWin->y > 0 &&
                !(addWin->dock)) {

                if ((int) (addWin->w) == global.SnowWinWidth &&
                    addWin->x == 0 && addWin->y < 100) {
                    break;
                }

                //XWindowAttributes addWinAtrributes;
                //XGetWindowAttributes(global.display, addWin->id, &addWinAtrributes);
                //if (addWinAtrributes.map_state == IsUnviewable) {
                    //fprintf(stdout, "windows.c: UpdateFallenSnowRegions() window (%lu)  "
                    //    "window map_state IsUnViewable: %i\n", addWin->id,
                    //    (addWinAtrributes.map_state == IsUnviewable));
                    //break;
                //}

                //fprintf(stdout, "windows.c: UpdateFallenSnowRegions() "
                //    "No snow on transient window.\n");
                if (addWin->id == getWindowBeingDragged()) {
                    break;
                }

                // fprintf(stdout, "windows.c: UpdateFallenSnowRegions() "
                //     "adding a FallenSnow (%lu)\n", addWin->id);
                PushFallenSnow(&global.FsnowFirst, addWin,
                addWin->x + Flags.OffsetX, addWin->y + Flags.OffsetY,
                addWin->w + Flags.OffsetW, Flags.MaxWinSnowDepth);
            }
        }

        addWin++;
    }

    // Count fallensnow regions.
    fsnow = global.FsnowFirst;
    int nf = 0;
    while (fsnow) {
        nf++;
        fsnow = fsnow->next;
    }

    // nf+1: prevent allocation of zero bytes
    int ntoremove = 0;
    long int *toremove = (long int *) malloc(sizeof(*toremove) * (nf + 1));

    fsnow = global.FsnowFirst;
    while (fsnow) {
        if (fsnow->win.id != 0) {
            WinInfo* removeWin = FindWindow(mWindowsList, mNumberOfWindows, fsnow->win.id);
            if (!removeWin ||
                (removeWin->w > 0.8 * global.SnowWinWidth && removeWin->ya < Flags.IgnoreTop) ||
                (removeWin->w > 0.8 * global.SnowWinWidth &&
                    (int)global.SnowWinHeight - removeWin->ya < Flags.IgnoreBottom)) {
                GenerateFlakesFromFallen(fsnow, 0, fsnow->w, -10.0);
                toremove[ntoremove++] = fsnow->win.id;
            }

            // test if fsnow->win.id is hidden. If so: clear the area and notify
            // in fsnow we have to test that here, because the hidden status of
            // the window can change
            if (fsnow->win.hidden) {
                CleanFallenArea(fsnow, 0, fsnow->w);
                GenerateFlakesFromFallen(fsnow, 0, fsnow->w, -10.0);
                toremove[ntoremove++] = fsnow->win.id;
            }
        }

        fsnow = fsnow->next;
    }

    // Test if window has been moved or resized.
    WinInfo* movedWin = mWindowsList;
    for (int i = 0; i < mNumberOfWindows; i++) {
        fsnow = FindFallen(global.FsnowFirst, movedWin->id);
        if (fsnow) {
            if (fsnow->x != movedWin->x + Flags.OffsetX ||
                fsnow->y != movedWin->y + Flags.OffsetY ||
                (unsigned int) fsnow->w != movedWin->w + Flags.OffsetW) {
                //fprintf(stdout, "windows.c: UpdateFallenSnowRegions() Updating due "
                //    "to MOVED window : %li\n", movedWin->id);

                CleanFallenArea(fsnow, 0, fsnow->w);
                GenerateFlakesFromFallen(fsnow, 0, fsnow->w, -10.0);
                toremove[ntoremove++] = fsnow->win.id;

                fsnow->x = movedWin->x + Flags.OffsetX;
                fsnow->y = movedWin->y + Flags.OffsetY;
                XFlush(global.display);

                continue;
            }
        }

        movedWin++;
    }

    for (int i = 0; i < ntoremove; i++) {
        CleanFallen(toremove[i]);
        RemoveFallenSnow(&global.FsnowFirst, toremove[i]);
    }
    free(toremove);
}

/** *********************************************************************
 ** This method gets the location and size of xinerama screen xscreen.
 **/
int xinerama(Display *display, int xscreen, int *x, int *y, int *w, int *h) {
    int screenInfoArrayLength;
    XineramaScreenInfo *screenInfoArray = XineramaQueryScreens(display, &screenInfoArrayLength);
    if (screenInfoArray == NULL) {
        return FALSE;
    }

    // If screen found, use it's x/y, h/w.
    int boundedXscreen = (xscreen > screenInfoArrayLength - 1) ?
        screenInfoArrayLength - 1 : xscreen;
    if (boundedXscreen >= 0) {
        *w = screenInfoArray[boundedXscreen].width;
        *h = screenInfoArray[boundedXscreen].height;

        *x = screenInfoArray[boundedXscreen].x_org;
        *y = screenInfoArray[boundedXscreen].y_org;

        XFree(screenInfoArray);
        return screenInfoArrayLength;
    }

    // If not found, try to determine w/h.
    *w = 0; *h = 0;
    for (int i = 0; i < screenInfoArrayLength; i++) {
        if (screenInfoArray[i].width > *w) {
            *w = screenInfoArray[i].width;
        }
        if (screenInfoArray[i].height > *h) {
            *h = screenInfoArray[i].height;
        }
    }
    *x = 0; *y = 0;

    XFree(screenInfoArray);
    return screenInfoArrayLength;
}

/** *********************************************************************
 ** This method ...
 **/
void InitDisplayDimensions() {
    unsigned int w, h;
    int x, y;
    xdo_get_window_location(global.xdo, global.Rootwindow, &x, &y, NULL);
    xdo_get_window_size(global.xdo, global.Rootwindow, &w, &h);

    P("InitDisplayDimensions root: %p %d %d %d %d\n", (void *)global.Rootwindow,
        x, y, w, h);

    global.Xroot = x;
    global.Yroot = y;
    global.Wroot = w;
    global.Hroot = h;

    DisplayDimensions();
}

/** *********************************************************************
 ** This method ...
 **/
void DisplayDimensions() {
    P("Displaydimensions\n");
    Lock_fallen();
    unsigned int w, h, b, d;
    int x, y;
    Window root;

    xdo_wait_for_window_map_state(global.xdo, global.SnowWin, IsViewable);

    int rc = XGetGeometry(
        global.display, global.SnowWin, &root, &x, &y, &w, &h, &b, &d);

    if (rc == 0) {
        P("Oeps\n");
        I("\nSnow window %#lx has disappeared, it seems. I quit.\n",
            global.SnowWin);
        uninitQPickerDialog();
        Thanks();
        exit(1);
        return;
    }

    global.SnowWinWidth = w;
    global.SnowWinHeight = h + Flags.OffsetS;

    P("DisplayDimensions: SnowWinX: %d Y:%d W:%d H:%d\n", global.SnowWinX,
        global.SnowWinY, global.SnowWinWidth, global.SnowWinHeight);

    global.SnowWinBorderWidth = b;
    global.SnowWinDepth = d;

    UpdateFallenSnowAtBottom();

    RedrawTrees();

    SetMaxScreenSnowDepth();
    if (!global.IsDouble) {
        ClearScreen();
    }
    Unlock_fallen();
}

/** *********************************************************************
 ** This method ...
 **/
void SetBackground() {
    char *f = Flags.BackgroundFile;
    if (!IsReadableFile(f)) {
        return;
    }

    printf(_("Setting background from %s\n"), f);

    int w = global.SnowWinWidth;
    int h = global.SnowWinHeight;
    Display *display = global.display;
    Window window = global.SnowWin;
    int screen_num = DefaultScreen(display);
    int depth = DefaultDepth(display, screen_num);

    GdkPixbuf *pixbuf;
    pixbuf = gdk_pixbuf_new_from_file_at_scale(f, w, h, FALSE, NULL);
    if (!pixbuf) {
        return;
    }
    int n_channels = gdk_pixbuf_get_n_channels(pixbuf);

    guchar *pixels = gdk_pixbuf_get_pixels(pixbuf);
    P("pad: %d %d\n", XBitmapPad(display), depth);

    unsigned char *pixels1 =
        (unsigned char *)malloc(w * h * 4 * sizeof(unsigned char));
    // https://gnome.pages.gitlab.gnome.org/gdk-pixbuf/gdk-pixbuf/class.Pixbuf.html
    //

    int rowstride = gdk_pixbuf_get_rowstride(pixbuf);
    P("rowstride: %d\n", rowstride);
    int i, j;
    int k = 0;
    if (is_little_endian()) {
        for (i = 0; i < h; i++) {
            for (j = 0; j < w; j++) {
                guchar *p = &pixels[i * rowstride + j * n_channels];
                pixels1[k++] = p[2];
                pixels1[k++] = p[1];
                pixels1[k++] = p[0];
                pixels1[k++] = 0xff;
            }
        }
    } else {
        I("Big endian system, swapping bytes in background.\n");
        I("Let me know if this is not OK.\n");
        for (i = 0; i < h; i++) {
            for (j = 0; j < w; j++) {
                guchar *p = &pixels[i * rowstride + j * n_channels];
                pixels1[k++] = 0xff;
                pixels1[k++] = p[0];
                pixels1[k++] = p[1];
                pixels1[k++] = p[2];
            }
        }
    }

    XImage *ximage;
    ximage = XCreateImage(display, DefaultVisual(display, screen_num), depth,
        ZPixmap, 0, (char *)pixels1, w, h, XBitmapPad(display), 0);
    XInitImage(ximage);
    Pixmap pixmap;
    pixmap =
        XCreatePixmap(display, window, w, h, DefaultDepth(display, screen_num));

    GC gc;
    gc = XCreateGC(display, pixmap, 0, 0);
    XPutImage(display, pixmap, gc, ximage, 0, 0, 0, 0, w, h);

    P("setwindowbackground\n");
    XSetWindowBackgroundPixmap(display, window, pixmap);
    g_object_unref(pixbuf);
    XFreePixmap(display, pixmap);
    XDestroyImage(ximage);
    // free(pixels1);  //This is already freed by XDestroyImage
    return;
}

/** *********************************************************************
 ** Module MAINLOOP methods.
 **/
/** *********************************************************************
 ** This method is called periodically from the UI mainloop to update
 ** our internal X11 Windows array. (Laggy huh).
 **/
int updateWindowsList() {
    static long PrevWorkSpace = -123;
    if (Flags.Done) {
        return FALSE;
    }
    if (Flags.NoKeepSWin) {
        return TRUE;
    }

    static int lockcounter = 0;
    if (Lock_fallen_n(3, &lockcounter)) {
        return TRUE;
    }

    // Once in a while, we force updating windows.
    static int wcounter = 0;
    wcounter++;
    if (wcounter > 9) {
        global.WindowsChanged = 1;
        wcounter = 0;
    }
    if (!global.WindowsChanged) {
        Unlock_fallen();
        return TRUE;
    }
    global.WindowsChanged = 0;

    // Get current workspace.
    long currentWorkSpace = GetCurrentWorkspace();
    if (currentWorkSpace < 0) {
        Flags.Done = 1;
        Unlock_fallen();
        return TRUE;
    }

    // Update on Workspace change.
    global.CWorkSpace = currentWorkSpace;
    if (currentWorkSpace != PrevWorkSpace) {
        PrevWorkSpace = currentWorkSpace;
        DetermineVisualWorkspaces();
    }

    // Special hack too keep global.SnowWin below (needed for example in
    // FVWM/xcompmgr, where global.SnowWin is not click-through)
    if (Flags.BelowAll) {
        XWindowChanges changes;
        changes.stack_mode = Below;
        XConfigureWindow(global.display, global.SnowWin, CWStackMode, &changes);
    }

    // Update windows list. Free any current.
    if (mWindowsList) {
        free(mWindowsList);
    }
    // Get new list, error if none.
    if (getX11WindowsList(&mWindowsList, &mNumberOfWindows) < 0) {
        Flags.Done = 1;
        Unlock_fallen();
        return TRUE;
    }
    // Update list x/y.
    for (int i = 0; i < mNumberOfWindows; i++) {
        mWindowsList[i].x += global.WindowOffsetX - global.SnowWinX;
        mWindowsList[i].y += global.WindowOffsetY - global.SnowWinY;
    }

    // Take care of the situation that the transparent window changes from
    // workspace, which can happen if in a dynamic number of workspaces
    // environment a workspace is emptied.
    // check also on valid winfo: after toggling 'below'
    // winfo is nil sometimes
    // in xfce and maybe others, workspace info is not to be found
    // in our transparent window. winfo->ws will be 0, and we keep
    // the same value for TransWorkSpace.
    WinInfo *winfo = FindWindow(mWindowsList, mNumberOfWindows, global.SnowWin);
    if (global.Trans && winfo) {
        if (winfo->ws > 0) {
            TransWorkSpace = winfo->ws;
        }
    }

    // if (!TransA && !winfo)  // let op
    if (global.SnowWin != global.Rootwindow) {
        if (!global.Trans && !winfo) {
            Flags.Done = 1;
        }
    }

    UpdateFallenSnowRegions();

    Unlock_fallen();
    return TRUE;
}


/** *********************************************************************
 ** Module EXTERN methods.
 **/

/** *********************************************************************
 ** This method handles X11 Windows being created.
 **
 ** (Rough life-cycle order.)
 **/
void onWindowCreated(__attribute__((unused)) Window window) {
    // Print all window IDs.  --DEBUG--
    /*
    WinInfo* windowListItem = mWindowsList;
    for (int i = 0; i < mNumberOfWindows; i++) {
        {
            Window root, parent;
            Window* children = NULL;
            unsigned int windowChildCount;
            if (XQueryTree(global.display, windowListItem->id,
                    &root, &parent, &children, &windowChildCount)) {
                fprintf(stdout, "windows.c: onWindowCreated(%li)   root : %li  "
                    "WinInfo.id: %li   WinInfo.hidden: %u  #children : %u  parent : %li\n",
                    window, root, windowListItem->id, windowListItem->hidden,
                    windowChildCount, parent);
                if (children) {
                    XFree((char *) children);
                }
            }
        }
        windowListItem++;
    }
    */
}


/** *********************************************************************
 ** This method handles X11 Windows being made visible to view.
 **
 ** (Rough life-cycle order.)
 **/
void onWindowMapped(__attribute__((unused)) Window window) {
    Lock_fallen();

    /*
    {
        Window root, parent;
        Window* childList = NULL;
        unsigned int childCount;
        if (XQueryTree(global.display, window,
                &root, &parent, &childList, &childCount)) {
            fprintf(stdout, "windows.c: onWindowMapped(%li)     parent : %li  "
                "root : %li  #childItem : %u  ",
                window, parent, root, childCount);
            Window* thisChild = childList;
            for (unsigned int i = 0; i < childCount; i++) {
                fprintf(stdout, ": %lu  ", thisChild[i]);
            }
            fprintf(stdout, "\n");
            XFree((char *) childList);
        }
    }
    */

    // Find the focused window and the current focus state.
    Window focusedWindow;
    int focusWindowState;
    XGetInputFocus(global.display, &focusedWindow, &focusWindowState);
    //fprintf(stdout, "windows.c: onWindowMapped(%li)   Focused Window WinInfo.id: %li \n",
    //    window, focusedWindow);

    // Get the parent of the focused window.
    //Window focusedRoot, focusedParent;
    //Window *focusedChildren = NULL;
    //unsigned int focusedChildrenCount;
    //if (XQueryTree(global.display, focusedWindow,
    //        &focusedRoot, &focusedParent, &focusedChildren, &focusedChildrenCount)) {
        //fprintf(stdout, "windows.c: onWindowMapped(%li)       Focused Window parent : %li \n",
        //    window, focusedParent);
    //    if (focusedChildren) {
    //        XFree((char *) focusedChildren);
    //    }
    //}

    Window root_return, child_return;
    int root_x_return, root_y_return, win_x_return, win_y_return;
    unsigned int pointerState;
    const unsigned int POINTER_CLICKDOWN = 256;
    if (XQueryPointer(global.display, focusedWindow, &root_return, &child_return,
        &root_x_return, &root_y_return, &win_x_return, &win_y_return,
        &pointerState)) {
        if (pointerState == POINTER_CLICKDOWN) {
            //fprintf(stdout, "windows.c: onWindowMapped(%li)       "
            //    "focusedWindow Pointer State : %u.\n", window, pointerState);
            setWindowBeingDragged(getDragWindowOf(focusedWindow));
        } else {
            //fprintf(stdout, "windows.c: onWindowMapped(%li)       "
            //    "Can\'t start drag, unknown Pointer State : %u.\n",
            //    window, pointerState);
        }
    } else {
        //fprintf(stdout, "windows.c: onWindowMapped(%li)       "
        //    "Can\'t query pointer for focusedWindow.", window);
    }

    // Found window being "dragged". If it has a fallensnow region,
    // Erase it now, don't wait for final drop to complete a "MOVE"
    // event (handled much later in UpdateFallenSnowRegions()).
    FallenSnow* fsnow = FindFallen(global.FsnowFirst, getWindowBeingDragged());
    if (fsnow) {
        //fprintf(stdout, "windows.c: onWindowMapped(%li) "
        //    "Window Going Boof!\n", window);
        CleanFallenArea(fsnow, 0, fsnow->w);
        GenerateFlakesFromFallen(fsnow, 0, fsnow->w, -10.0);
        CleanFallen(fsnow->win.id);
        RemoveFallenSnow(&global.FsnowFirst, fsnow->win.id);
    }

    Unlock_fallen();
}

/** *********************************************************************
 ** This method handles X11 Windows being Hidden from view.
 **
 ** (Rough life-cycle order.)
 **/
void onWindowUnmapped(__attribute__((unused)) Window window) {
    Lock_fallen();
    setWindowBeingDragged(None);
    Unlock_fallen();
}

/** *********************************************************************
 ** This method handles X11 Windows being destroyed.
 **
 ** (Rough life-cycle order.)
 **/
void onWindowDestroyed(__attribute__((unused)) Window window) {
}


/** *********************************************************************
 ** This method
 **
 **/
void setWindowBeingDragged(Window window) {
    //fprintf(stdout, "windows.c: setWindowBeingDragged(%li).\n", window);
    mWindowBeingDragged = window;
}

/** *********************************************************************
 ** This method
 **
 **/
extern Window getWindowBeingDragged() {
    return mWindowBeingDragged;
}

/** *********************************************************************
 ** This method determines which window is being dragged.
 **
 **/
Window getDragWindowOf(Window window) {
    Window windowNode = window;
    //fprintf(stdout, "windows.c: getDragWindowOf() Called to find : %li!\n", windowNode);

    while (TRUE) {
        // Check current node in windows list.
        WinInfo* windowListItem = mWindowsList;
        for (int i = 0; i < mNumberOfWindows; i++) {
            if (windowNode == windowListItem->id) {
                //fprintf(stdout, "windows.c: getDragWindowOf() Found %li!\n", windowNode);
                return windowNode;
            }
            windowListItem++;
        }

        // Current window node not in list. Move up to parent and loop.
        Window root, parent;
        Window* children = NULL;
        unsigned int windowChildCount;
        if (!(XQueryTree(global.display, windowNode,
                &root, &parent, &children, &windowChildCount))) {
            //fprintf(stdout, "windows.c: getDragWindowOf() no parent found, end of chain.\n");
            return None;
        }
        if (children) {
            XFree((char *) children);
        }
        windowNode = parent;
        //fprintf(stdout, "windows.c: getDragWindowOf() Checking parent : %li.\n", windowNode);
    }
}
