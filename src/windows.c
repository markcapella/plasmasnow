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


static long TransWorkSpace =
    -SOMENUMBER; // workspace on which transparent window is placed

static int mNumberOfWindows;
static WinInfo *mWindowsList = NULL;

static void DetermineVisualWorkspaces(void);

static int do_sendevent();
static int do_wupdate();

// ColorPicker
void uninitQPickerDialog();

void windows_ui() {}

void windows_draw() {
    // nothing to draw
}

void windows_init() {
    if (global.Desktop) {
        DetermineVisualWorkspaces();
        add_to_mainloop(PRIORITY_DEFAULT, time_wupdate, do_wupdate);
    }
    if (!global.IsDouble) {
        add_to_mainloop(PRIORITY_DEFAULT, time_sendevent, do_sendevent);
    }
}

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

int do_wupdate() {
    static long PrevWorkSpace = -123;
    if (Flags.Done) {
        return FALSE;
    }

    if (Flags.NoKeepSWin) {
        return TRUE;
    }

    static int lockcounter = 0;
    if (Lock_fallen_n(3, &lockcounter)) {
        P("lock counter: %d\n", lockcounter);
        return TRUE;
    }

    // once in a while, we force updating windows
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

    long r;
    r = GetCurrentWorkspace();
    if (r >= 0) {
        global.CWorkSpace = r;
        if (r != PrevWorkSpace) {
            P("workspace changed from %ld to %ld\n", PrevWorkSpace, r);
            PrevWorkSpace = r;
            DetermineVisualWorkspaces();
        }
    } else {
        I("Cannot get current workspace\n");
        Flags.Done = 1;
        Unlock_fallen();
        return TRUE;
    }

    // P("Update windows\n");
    if (mWindowsList) {
        free(mWindowsList);
    }

    // Special hack too keep global.SnowWin below (needed for example in
    // FVWM/xcompmgr, where global.SnowWin is not click-through)
    if (Flags.BelowAll) {
        XWindowChanges changes;
        changes.stack_mode = Below;
        XConfigureWindow(global.display, global.SnowWin, CWStackMode, &changes);
    }

    // I("Cannot get windows\n");
    if (GetWindows(&mWindowsList, &mNumberOfWindows) < 0) {
        Flags.Done = 1;
        Unlock_fallen();
        return TRUE;
    }

    // P("SnowWinX SnowWinY: %d %d\n", global.SnowWinX, global.SnowWinY);
    for (int i = 0; i < mNumberOfWindows; i++) {
        // WinInfo *w = &mWindowsList[i];
        mWindowsList[i].x += global.WindowOffsetX - global.SnowWinX;
        mWindowsList[i].y += global.WindowOffsetY - global.SnowWinY;
    }

    // Take care of the situation that the transparent window changes from
    // workspace, which can happen if in a dynamic number of workspaces
    // environment a workspace is emptied.
    WinInfo *winfo = FindWindow(mWindowsList, mNumberOfWindows, global.SnowWin);

    // check also on valid winfo: after toggling 'below'
    // winfo is nil sometimes
    if (global.Trans && winfo) {
        // in xfce and maybe others, workspace info is not to be found
        // in our transparent window. winfo->ws will be 0, and we keep
        // the same value for TransWorkSpace.
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

void DetermineVisualWorkspaces() {
    P("%d Entering DetermineVisualWorkspaces\n", global.counter++);
    static Window ProbeWindow = 0;
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

    if (ProbeWindow) {
        XDestroyWindow(global.display, ProbeWindow);
    } else {
        P("Creating attrs for ProbeWindow\n");
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
    // Determine which workspaces are visible: place a window (ProbeWindow)
    // in each xinerama screen, and ask in which workspace the window
    // is located.
    ProbeWindow = XCreateWindow(global.display, global.Rootwindow, 1, 1, 1, 1,
        10, DefaultDepth(global.display, global.Screen), InputOutput,
        DefaultVisual(global.display, global.Screen), valuemask, &attr);
    XSetClassHint(global.display, ProbeWindow, &class_hints);

    // to prevent the user to determine the intial position (in twm for example)
    XSetWMNormalHints(global.display, ProbeWindow, &wmsize);

    XChangeProperty(global.display, ProbeWindow, motif_hints, motif_hints, 32,
        PropModeReplace, (unsigned char *)&hints, 5);
    xdo_map_window(global.xdo, ProbeWindow);

    global.NVisWorkSpaces = number;
    int i;
    int prev = -SOMENUMBER;
    for (i = 0; i < number; i++) {
        int x = info[i].x_org;
        int y = info[i].y_org;
        int w = info[i].width;
        int h = info[i].height;

        // place ProbeWindow in the center of xinerama screen[i]

        int xm = x + w / 2;
        int ym = y + h / 2;
        P("movewindow: %d %d\n", xm, ym);
        xdo_move_window(global.xdo, ProbeWindow, xm, ym);
        xdo_wait_for_window_map_state(global.xdo, ProbeWindow, IsViewable);
        long desktop;
        int rc = xdo_get_desktop_for_window(global.xdo, ProbeWindow, &desktop);
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
    xdo_unmap_window(global.xdo, ProbeWindow);
}

void UpdateFallenSnowRegionsWithLock() {
    Lock_fallen();
    UpdateFallenSnowRegions();
    Unlock_fallen();
}

// Have a look at the windows we are snowing on
// Also update of fallensnow area's
void UpdateFallenSnowRegions() {
    // threads: locking by caller
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
            if (addWin->id != global.SnowWin && addWin->y > 0 && !(addWin->dock)) {
                if ((int) (addWin->w) == global.SnowWinWidth &&
                    addWin->x == 0 && addWin->y < 100) {
                    /* maybe a transparent xpenguins window? */
                } else {
                    PushFallenSnow(&global.FsnowFirst, addWin,
                    addWin->x + Flags.OffsetX, addWin->y + Flags.OffsetY,
                    addWin->w + Flags.OffsetW, Flags.MaxWinSnowDepth);
                }
            }
        }

        addWin++;
    }

    // remove fallensnow regions
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
            P("%#lx hidden:%d\n", fsnow->win.id, fsnow->win.hidden);
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

                CleanFallenArea(fsnow, 0, fsnow->w);
                GenerateFlakesFromFallen(fsnow, 0, fsnow->w, -10.0);
                toremove[ntoremove++] = fsnow->win.id;

                fsnow->x = movedWin->x + Flags.OffsetX;
                fsnow->y = movedWin->y + Flags.OffsetY;
                XFlush(global.display);
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

// gets location and size of xinerama screen xscreen, -1: full screen
// returns the number of xinerama screens
int xinerama(Display *display, int xscreen, int *x, int *y, int *w, int *h) {
    int number;
    XineramaScreenInfo *info = XineramaQueryScreens(display, &number);
    if (info == NULL) {
        I("No xinerama...\n");
        return FALSE;
    } else {
        int scr = xscreen;
        if (scr > number - 1) {
            scr = number - 1;
        }

        int i;
        for (i = 0; i < number; i++) {
            P("number: %d\n", info[i].screen_number);
            P("   x_org:  %d\n", info[i].x_org);
            P("   y_org:  %d\n", info[i].y_org);
            P("   width:  %d\n", info[i].width);
            P("   height: %d\n", info[i].height);
        }

        if (scr < 0) {
            // set x,y to 0,0
            // set width and height to maximum values found
            int i;
            *x = 0;
            *y = 0;
            *w = 0;
            *h = 0;
            for (i = 0; i < number; i++) {
                if (info[i].width > *w) {
                    *w = info[i].width;
                }
                if (info[i].height > *h) {
                    *h = info[i].height;
                }
            }
        } else {

            *x = info[scr].x_org;
            *y = info[scr].y_org;
            *w = info[scr].width;
            *h = info[scr].height;
        }
        P("Xinerama window: %d+%d %dx%d\n", *x, *y, *w, *h);

        XFree(info);
    }
    return number;
}

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
