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

#include <stdio.h>
#include <ctype.h>
#include <stdbool.h>

#include <pthread.h>

#include <X11/Intrinsic.h>
#include <X11/extensions/Xinerama.h>

#include <gdk/gdkx.h>
#include <gtk/gtk.h>

#include "windows.h"
#include "debug.h"
#include "flags.h"

#include "dsimple.h"
#include "fallensnow.h"

#include "mygettext.h"
#include "safe_malloc.h"
#include "transwindow.h"

#include "utils.h"
#include "wmctrl.h"
#include "xdo.h"

#include "plasmasnow.h"
#include "scenery.h"



/***********************************************************
 * Module Method stubs.
 */
// Workspace.
static void DetermineVisualWorkspaces();
static int do_sendevent();
extern int updateWindowsList();

 // Windows.
extern void onWindowCreated(Window);
extern void onWindowDestroyed(Window);
extern void onWindowMapped(Window);
extern void onWindowUnmapped(Window);

// Window dragging methods.
void clearWindowDragState();
void setWindowDragState();

// Window dragging helpers.
extern Window getDragWindowOf(Window);

bool mWindowDragging = false;
extern bool isWindowDraggingActive();
void setWindowDraggingActive();
void setWindowDraggingInactive();

Window mWindowBeingDragged = None;
Window getWindowBeingDragged();
void setWindowBeingDragged(Window);

// ColorPicker methods.
void uninitQPickerDialog();
extern void applicationFinish(void);

//extern int getTmpLogFile();

/** *********************************************************************
 ** Module globals and consts.
 **/

// Workspace on which transparent window is placed.
static long TransWorkSpace = -SOMENUMBER;

// Main WinInfo (Windows) List.
static int mWinInfoListLength = 0;
static WinInfo* mWinInfoList = NULL;

extern void getWinInfoList();

/** *********************************************************************
 ** This method scans all WinInfos for a requested ID.
 **/
WinInfo* findWinInfoByWindowId(Window reqID) {
    WinInfo* winInfoItem = mWinInfoList;
    for (int i = 0; i < mWinInfoListLength; i++) {
        if (winInfoItem->id == reqID) {
            return winInfoItem;
        }
        winInfoItem++;
    }

    return NULL;
}

/** *********************************************************************
 ** This method ...
 **/
void addWindowsModuleToMainloop() {
    if (mGlobal.hasDestopWindow) {
        DetermineVisualWorkspaces();
        addMethodToMainloop(PRIORITY_DEFAULT, time_wupdate, updateWindowsList);
    }

    if (!mGlobal.isDoubleBuffered) {
        addMethodToMainloop(PRIORITY_DEFAULT, time_sendevent, do_sendevent);
    }
}

/** *********************************************************************
 ** This method ...
 **/
int WorkspaceActive() {
    // ah, so difficult ...
    if (Flags.AllWorkspaces) {
        return 1;
    }

    for (int i = 0; i < mGlobal.NVisWorkSpaces; i++) {
        if (mGlobal.VisWorkSpaces[i] == mGlobal.ChosenWorkSpace) {
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
    event.display = mGlobal.display;
    event.window = mGlobal.SnowWin;

    event.x = 0;
    event.y = 0;
    event.width = mGlobal.SnowWinWidth;
    event.height = mGlobal.SnowWinHeight;

    XSendEvent(mGlobal.display, mGlobal.SnowWin, True, Expose, (XEvent *) &event);
    return TRUE;
}

/** *********************************************************************
 ** This method ...
 **/
void DetermineVisualWorkspaces() {
    P("%d Entering DetermineVisualWorkspaces\n", mGlobal.counter++);

    static XClassHint class_hints;
    static XSetWindowAttributes attr;
    static long valuemask;
    static long hints[5] = {2, 0, 0, 0, 0};
    static Atom motif_hints;
    static XSizeHints wmsize;

    if (!mGlobal.hasDestopWindow) {
        mGlobal.NVisWorkSpaces = 1;
        mGlobal.VisWorkSpaces[0] = mGlobal.CWorkSpace;
        return;
    }

    static Window probeWindow = 0;
    if (probeWindow) {
        XDestroyWindow(mGlobal.display, probeWindow);
    } else {

        // Probe window
        attr.background_pixel = WhitePixel(mGlobal.display, mGlobal.Screen);
        attr.border_pixel = WhitePixel(mGlobal.display, mGlobal.Screen);
        attr.event_mask = ButtonPressMask;
        valuemask = CWBackPixel | CWBorderPixel | CWEventMask;
        class_hints.res_name = (char *) "plasmasnow";
        class_hints.res_class = (char *) "plasmasnow";
        motif_hints = XInternAtom(mGlobal.display, "_MOTIF_WM_HINTS", False);
        wmsize.flags = USPosition | USSize;
    }

    int number;
    XineramaScreenInfo *info = XineramaQueryScreens(mGlobal.display, &number);
    if (number == 1 || info == NULL) {
        mGlobal.NVisWorkSpaces = 1;
        mGlobal.VisWorkSpaces[0] = mGlobal.CWorkSpace;
        return;
    }

    // This is for bspwm and possibly other tiling window magagers.
    // Determine which workspaces are visible: place a window (probeWindow)
    // in each xinerama screen, and ask in which workspace the window
    // is located.
    probeWindow = XCreateWindow(mGlobal.display, mGlobal.Rootwindow, 1, 1, 1, 1,
        10, DefaultDepth(mGlobal.display, mGlobal.Screen), InputOutput,
        DefaultVisual(mGlobal.display, mGlobal.Screen), valuemask, &attr);
    XSetClassHint(mGlobal.display, probeWindow, &class_hints);

    // to prevent the user to determine the intial position (in twm for example)
    XSetWMNormalHints(mGlobal.display, probeWindow, &wmsize);

    XChangeProperty(mGlobal.display, probeWindow, motif_hints, motif_hints, 32,
        PropModeReplace, (unsigned char *) &hints, 5);
    xdo_map_window(mGlobal.xdo, probeWindow);

    mGlobal.NVisWorkSpaces = number;
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
        xdo_move_window(mGlobal.xdo, probeWindow, xm, ym);
        xdo_wait_for_window_map_state(mGlobal.xdo, probeWindow, IsViewable);
        long desktop;
        int rc = xdo_get_desktop_for_window(mGlobal.xdo, probeWindow, &desktop);
        if (rc == XDO_ERROR) {
            desktop = mGlobal.CWorkSpace;
        }
        P("desktop: %ld rc: %d\n", desktop, rc);
        mGlobal.VisWorkSpaces[i] = desktop;

        if (desktop != prev) {
            // this is for the case that the xinerama screens belong to
            // different workspaces, as seems to be the case in e.g. bspwm
            if (prev >= 0) {
                mGlobal.WindowOffsetX = 0;
                mGlobal.WindowOffsetY = 0;
            }
            prev = desktop;
        }
    }
    xdo_unmap_window(mGlobal.xdo, probeWindow);
}

/** *********************************************************************
 ** This method ...
 **/
void updateFallenSnowRegionsWithLock() {
    lockFallenSnowSemaphore();
    updateFallenSnowRegions();
    unlockFallenSnowSemaphore();
}

/** *********************************************************************
 ** This method ...
 **/
void updateFallenSnowRegions() {
    FallenSnow *fsnow;

    // add fallensnow regions:
    WinInfo* addWin = mWinInfoList;
    for (int i = 0; i < mWinInfoListLength; i++) {
        fsnow = findFallenSnowListItem(mGlobal.FsnowFirst, addWin->id);
        if (fsnow) {
            fsnow->win = *addWin;
            if ((!fsnow->win.sticky) &&
                fsnow->win.ws != mGlobal.CWorkSpace) {
                eraseFallenSnowOnDisplay(fsnow, 0, fsnow->w);
            }
        }

        // window found in mWinInfoList, but not in list of fallensnow,
        // add it, but not if we are snowing or birding in this window
        // (Desktop for example) and also not if this window has y <= 0
        // and also not if this window is a "dock"
        if (!fsnow) {
            if (addWin->id != mGlobal.SnowWin &&
                addWin->y > 0 && !(addWin->dock)) {
                if ((int) (addWin->w) == mGlobal.SnowWinWidth &&
                    addWin->x == 0 && addWin->y < 100) {
                    continue;
                }

                if (isWindowDraggingActive() &&
                    addWin->id == getWindowBeingDragged()) {
                    // fprintf(stdout, "windows.c: updateFallenSnowRegions()" 
                    //     "Window being dragged.\n");
                    continue;
                }

                // fprintf(stdout, "windows.c: updateFallenSnowRegions() "
                //     "adding a FallenSnow (%lu)\n", addWin->id);
                PushFallenSnow(&mGlobal.FsnowFirst, addWin,
                    addWin->x + Flags.OffsetX, addWin->y + Flags.OffsetY,
                    addWin->w + Flags.OffsetW, Flags.MaxWinSnowDepth);
            }
        }

        addWin++;
    }

    // Count fallensnow regions.
    fsnow = mGlobal.FsnowFirst;
    int nf = 0;
    while (fsnow) {
        nf++;
        fsnow = fsnow->next;
    }

    // nf+1: prevent allocation of zero bytes
    int ntoremove = 0;
    long int *toremove = (long int *) malloc(sizeof(*toremove) * (nf + 1));

    fsnow = mGlobal.FsnowFirst;
    while (fsnow) {
        if (fsnow->win.id != 0) {
            WinInfo* removeWin = findWinInfoByWindowId(fsnow->win.id);
            if (!removeWin ||
                (removeWin->w > 0.8 * mGlobal.SnowWinWidth && removeWin->ya < Flags.IgnoreTop) ||
                (removeWin->w > 0.8 * mGlobal.SnowWinWidth &&
                    (int)mGlobal.SnowWinHeight - removeWin->ya < Flags.IgnoreBottom)) {
                generateFallenSnowFlakes(fsnow, 0, fsnow->w, -10.0);
                toremove[ntoremove++] = fsnow->win.id;
            }

            // test if fsnow->win.id is hidden. If so: clear the area and notify
            // in fsnow we have to test that here, because the hidden status of
            // the window can change
            if (fsnow->win.hidden) {
                eraseFallenSnowOnDisplay(fsnow, 0, fsnow->w);
                generateFallenSnowFlakes(fsnow, 0, fsnow->w, -10.0);
                toremove[ntoremove++] = fsnow->win.id;
            }
        }

        fsnow = fsnow->next;
    }

    // Test if window has been moved or resized.
    WinInfo* movedWin = mWinInfoList;
    for (int i = 0; i < mWinInfoListLength; i++) {
        fsnow = findFallenSnowListItem(mGlobal.FsnowFirst, movedWin->id);
        if (fsnow) {
            if (fsnow->x != movedWin->x + Flags.OffsetX ||
                fsnow->y != movedWin->y + Flags.OffsetY ||
                (unsigned int) fsnow->w != movedWin->w + Flags.OffsetW) {
                // fprintf(stdout, "windows.c: updateFallenSnowRegions() Updating due "
                //     "to MOVED window : %li\n", movedWin->id);

                eraseFallenSnowOnDisplay(fsnow, 0, fsnow->w);
                generateFallenSnowFlakes(fsnow, 0, fsnow->w, -10.0);
                toremove[ntoremove++] = fsnow->win.id;

                fsnow->x = movedWin->x + Flags.OffsetX;
                fsnow->y = movedWin->y + Flags.OffsetY;
                XFlush(mGlobal.display);
            }
        }

        movedWin++;
    }

    for (int i = 0; i < ntoremove; i++) {
        eraseFallenSnowOnWindow(toremove[i]);
        removeFallenSnowListItem(&mGlobal.FsnowFirst, toremove[i]);
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
void initDisplayDimensions() {
    unsigned int w, h;
    int x, y;
    xdo_get_window_location(mGlobal.xdo, mGlobal.Rootwindow, &x, &y, NULL);
    xdo_get_window_size(mGlobal.xdo, mGlobal.Rootwindow, &w, &h);

    P("initDisplayDimensions root: %p %d %d %d %d\n", (void *)mGlobal.Rootwindow,
        x, y, w, h);

    mGlobal.Xroot = x;
    mGlobal.Yroot = y;
    mGlobal.Wroot = w;
    mGlobal.Hroot = h;

    {
        char resultMsg[128];
        snprintf(resultMsg, sizeof(resultMsg),
            "windows: initDisplayDimensions() -> updateDisplayDimensions() before.\n");
        fprintf(stdout, "%s", resultMsg);
    }
    updateDisplayDimensions();
    {
        char resultMsg[128];
        snprintf(resultMsg, sizeof(resultMsg),
            "windows: initDisplayDimensions() -> updateDisplayDimensions() after.\n");
        fprintf(stdout, "%s", resultMsg);
    }
}

/** *********************************************************************
 ** This method ...
 **/
void updateDisplayDimensions() {
    P("Displaydimensions\n");
    lockFallenSnowSemaphore();

    //
    xdo_wait_for_window_map_state(mGlobal.xdo, mGlobal.SnowWin, IsViewable);

    Window root;
    int x, y;
    unsigned int w, h, b, d;
    int rc = XGetGeometry(
        mGlobal.display, mGlobal.SnowWin, &root, &x, &y, &w, &h, &b, &d);
    if (rc == 0) {
        P("Oeps\n");
        I("\nSnow window %#lx has disappeared, it seems. I quit.\n",
            mGlobal.SnowWin);
        uninitQPickerDialog();
        applicationFinish();
        exit(1);
        return;
    }

    //
    mGlobal.SnowWinWidth = w;
    mGlobal.SnowWinHeight = h + Flags.OffsetS;

    P("updateDisplayDimensions: SnowWinX: %d Y:%d W:%d H:%d\n", mGlobal.SnowWinX,
        mGlobal.SnowWinY, mGlobal.SnowWinWidth, mGlobal.SnowWinHeight);

    mGlobal.SnowWinBorderWidth = b;
    mGlobal.SnowWinDepth = d;

    updateFallenSnowAtBottom();

    {
        char resultMsg[128];
        snprintf(resultMsg, sizeof(resultMsg),
            "windows: updateDisplayDimensions() Redraws trees before.\n");
        fprintf(stdout, "%s", resultMsg);
    }
    RedrawTrees();
    {
        char resultMsg[128];
        snprintf(resultMsg, sizeof(resultMsg),
            "windows: updateDisplayDimensions() Redraws trees after.\n");
        fprintf(stdout, "%s", resultMsg);
    }

    setMaxScreenSnowDepth();
    if (!mGlobal.isDoubleBuffered) {
        ClearScreen();
    }

    unlockFallenSnowSemaphore();
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

    int w = mGlobal.SnowWinWidth;
    int h = mGlobal.SnowWinHeight;
    Display *display = mGlobal.display;
    Window window = mGlobal.SnowWin;
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
    if (Flags.NoKeepSnowOnWindows) {
        return TRUE;
    }

    static int lockcounter = 0;
    if (softLockFallenSnowBaseSemaphore(3, &lockcounter)) {
        return TRUE;
    }

    // Once in a while, we force updating windows.
    static int wcounter = 0;
    wcounter++;
    if (wcounter > 9) {
        mGlobal.WindowsChanged = 1;
        wcounter = 0;
    }
    if (!mGlobal.WindowsChanged) {
        unlockFallenSnowSemaphore();
        return TRUE;
    }
    mGlobal.WindowsChanged = 0;

    // Get current workspace.
    long currentWorkSpace = getCurrentWorkspace();
    if (currentWorkSpace < 0) {
        Flags.Done = 1;
        unlockFallenSnowSemaphore();
        {   const char* logMsg = "windows: updateWindowsList() Finishes.\n";
            fprintf(stdout, "%s", logMsg);
        }
        return TRUE;
    }

    // Update on Workspace change.
    mGlobal.CWorkSpace = currentWorkSpace;
    if (currentWorkSpace != PrevWorkSpace) {
        PrevWorkSpace = currentWorkSpace;
        DetermineVisualWorkspaces();
    }

    // Special hack too keep mGlobal.SnowWin below (needed for example in
    // FVWM/xcompmgr, where mGlobal.SnowWin is not click-through)
    // logAllWindowsStackedTopToBottom();

    //XWindowChanges changes;
    //changes.stack_mode = Below;
    //XConfigureWindow(mGlobal.display, mGlobal.SnowWin,
    //    CWStackMode, &changes);

    // logAllWindowsStackedTopToBottom();

    if (isWindowDraggingActive()) {
        updateFallenSnowRegions();
        unlockFallenSnowSemaphore();
        return true;
    }

    // Update windows list. Free any current.
    // Get new list, error if none.
    getWinInfoList();

    for (int i = 0; i < mWinInfoListLength; i++) {
        mWinInfoList[i].x += mGlobal.WindowOffsetX - mGlobal.SnowWinX;
        mWinInfoList[i].y += mGlobal.WindowOffsetY - mGlobal.SnowWinY;
    }

    WinInfo* winInfo = findWinInfoByWindowId(mGlobal.SnowWin);
    if (mGlobal.hasTransparentWindow && winInfo) {
        if (winInfo->ws > 0) {
            TransWorkSpace = winInfo->ws;
        }
    }

    if (mGlobal.SnowWin != mGlobal.Rootwindow) {
        if (!mGlobal.hasTransparentWindow && !winInfo) {
            Flags.Done = 1;
        }
    }

    updateFallenSnowRegions();

    unlockFallenSnowSemaphore();
    /*
    {   const char* logMsg = "windows: updateWindowsList() Finishes.\n";
        write(getTmpLogFile(), logMsg, strlen(logMsg));
    }
    */
    return TRUE;
}

/** *********************************************************************
 ** This method ...
 **/
void getWinInfoList() {
    if (mWinInfoList) {
        free(mWinInfoList);
    }

    getX11WindowsList(&mWinInfoList, &mWinInfoListLength);
}

/** *********************************************************************
 ** This method handles X11 Windows being created.
 **
 ** (Rough life-cycle order.)
 **/
void onWindowCreated(__attribute__((unused)) Window window) {
    //{   const char* logMsg =
    //        "windows: onWindowCreated() Starts.\n";
    //    fprintf(stdout, "%s", logMsg);
    //}

    // Update our list to include the new one.
    getWinInfoList();

    //{   const char* logMsg =
    //        "windows: onWindowCreated() Finishes.\n";
    //    fprintf(stdout, "%s", logMsg);
    //}
}

/** *********************************************************************
 ** This method handles X11 Windows being made visible to view.
 **
 ** Our main job is to determine if user is dragging a window,
 ** and to immediately clear it's fallensnow.
 **/
void onWindowMapped(__attribute__((unused)) Window window) {
    //{   const char* logMsg = "windows: onWindowMapped() Starts.\n";
    //    fprintf(stdout, "%s", logMsg);
    //}

    // Determine window drag state.
    setWindowDragState();

    // Clear snow from window that just started dragging.
    if (isWindowDraggingActive()) {
        FallenSnow* fsnow = findFallenSnowListItem(mGlobal.FsnowFirst,
            getWindowBeingDragged());
        if (fsnow) {
            lockFallenSnowSemaphore();
            eraseFallenSnowOnDisplay(fsnow, 0, fsnow->w);

            generateFallenSnowFlakes(fsnow, 0, fsnow->w, -10.0);
            eraseFallenSnowOnWindow(fsnow->win.id);
            removeFallenSnowListItem(&mGlobal.FsnowFirst, fsnow->win.id);
            unlockFallenSnowSemaphore();
        }
    }

    //{   const char* logMsg = "windows: onWindowMapped() Finishes.\n";
    //    fprintf(stdout, "%s", logMsg);
    //}
}

/** *********************************************************************
 ** This method handles X11 Windows being Hidden from view.
 **
 ** Our main job is to clear window drag state.
 **/
void onWindowUnmapped(__attribute__((unused)) Window window) {
    //{   const char* logMsg =
    //        "windows: onWindowUnmapped() Starts.\n";
    //        fprintf(stdout, "%s", logMsg);
    //}

    // Clear window drag state.
    clearWindowDragState();

    //{   const char* logMsg =
    //        "windows: onWindowUnmapped() Finishes.\n";
    //    fprintf(stdout, "%s", logMsg);
    //}
}

/** *********************************************************************
 ** This method handles X11 Windows being destroyed.
 **
 ** (Rough life-cycle order.)
 **/
void onWindowDestroyed(__attribute__((unused)) Window window) {
    //{   const char* logMsg =
    //        "windows: onWindowDestroyed() Starts.\n";
    //    fprintf(stdout, "%s", logMsg);
    //}

    // Update our list to reflect the destroyed one.
    getWinInfoList();

    //{   const char* logMsg =
    //        "windows: onWindowDestroyed() Finishes.\n";
    //    fprintf(stdout, "%s", logMsg);
    //}
}

/** *********************************************************************
 ** This method clears window drag state.
 **/
void clearWindowDragState() {

    // We're not dragging.
    setWindowDraggingInactive();
    setWindowBeingDragged(None);
}

/** *********************************************************************
 ** This method determines window drag state.
 **/
void setWindowDragState() {
    // Find the focused window.
    Window focusedWindow;
    int focusedWindowState;
    XGetInputFocus(mGlobal.display, &focusedWindow, &focusedWindowState);

    //  Find the focused window pointer click state.
    Window root_return, child_return;
    int root_x_return, root_y_return, win_x_return, win_y_return;
    unsigned int pointerState;
    if (!XQueryPointer(mGlobal.display, focusedWindow, &root_return, &child_return,
        &root_x_return, &root_y_return, &win_x_return, &win_y_return,
        &pointerState)) {
        return;
    }

    // If click-state isn't clicked-down, or we can't find a dragging
    // window, we're not dragging. 
    const unsigned int POINTER_CLICKDOWN = 256;
    if (pointerState != POINTER_CLICKDOWN) {
        return;
    }
    Window dragWindow = getDragWindowOf(focusedWindow);
    if (dragWindow == None) {
        return;
    }

    // We're dragging.
    setWindowBeingDragged(dragWindow);
    setWindowDraggingActive();
}

/** *********************************************************************
 ** This method determines which window is being dragged, by
 ** returning the supplied window or the first parent window in
 ** its ancestor chain that is in mWinInfoList.
 **
 **/
Window getDragWindowOf(Window window) {
    Window windowNode = window;

    while (TRUE) {
        // Check current node in windows list.
        WinInfo* windowListItem = mWinInfoList;
        for (int i = 0; i < mWinInfoListLength; i++) {
            if (windowNode == windowListItem->id) {
                return windowNode;
            }
            windowListItem++;
        }

        // Current window node not in list. Move up it's parent
        // ancestor chain and re-test.
        Window root, parent;
        Window* children = NULL;
        unsigned int windowChildCount;
        if (!(XQueryTree(mGlobal.display, windowNode,
                &root, &parent, &children, &windowChildCount))) {
            return None;
        }
        if (children) {
            XFree((char *) children);
        }
        windowNode = parent;
    }
}

/** *********************************************************************
 ** These methods are window drag-state helpers.
 **
 **/
bool isWindowDraggingActive() {
    return mWindowDragging;
}

void setWindowDraggingActive() {
    mWindowDragging = true;
}

void setWindowDraggingInactive() {
    mWindowDragging = false;
}

extern Window getWindowBeingDragged() {
    return mWindowBeingDragged;
}

void setWindowBeingDragged(Window window) {
    mWindowBeingDragged = window;
}
