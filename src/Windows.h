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
#pragma once
#include <stdbool.h>

#include <X11/Intrinsic.h>
#include <gtk/gtk.h>

#include "PlasmaSnow.h"


/***********************************************************
 * Module Method stubs.
 */

#ifdef __cplusplus
extern "C" {
#endif

    void addWindowsModuleToMainloop(void);

    int WorkspaceActive();

    void initDisplayDimensions();
    void updateDisplayDimensions(void);

    void setWorkspaceBackground(void);
    int getXineramaScreenInfo(Display *display,
        int xscreen, int *x, int *y, int *w, int *h);

    // Workspace.
    void getCurrentWorkspaceData();
    int do_sendevent();
    int updateWindowsList();

    // Active & Focused X11 Window Helper methods.
    Window getActiveX11Window();
    Window getFocusedX11Window();
    int getFocusedX11XPos();
    int getFocusedX11YPos();

    // ActiveApp member helper methods.
    void clearAllActiveAppFields();

    Window getActiveAppWindow();
    void setActiveAppWindow(Window);
    Window getParentOfActiveAppWindow();

    int getActiveAppXPos();
    void setActiveAppXPos(int);

    int getActiveAppYPos();
    void setActiveAppYPos(int);

    // Windows life-cycle methods.
    void onCursorChange(XEvent*);
    void onAppWindowChange(Window);

    void onWindowCreated(XEvent*);
    void onWindowReparent(XEvent*);
    void onConfigureNotify(XEvent*);

    void onWindowMapped(XEvent*);
    void onWindowFocused(XEvent*);
    void onWindowBlurred(XEvent*);
    void onWindowUnmapped(XEvent*);

    void onWindowDestroyed(XEvent*);
    void onWindowClientMessage(XEvent*);

    // Windows life-cycle helper methods.
    bool isMouseClickedAndHeldInWindow();

    // Window dragging methods.
    void clearAllDragFields();

    bool isWindowBeingDragged();
    void setIsWindowBeingDragged(bool);

    Window getWindowBeingDragged();
    void setWindowBeingDragged(Window);

    Window getActiveAppDragWindowCandidate();
    void setActiveAppDragWindowCandidate(Window);

    Window getDragWindowOf(Window);

    char* getTitleOfWindow();
    void setTitleOfWindow(Window);

    long int isWindowVisibleOnWorkspace(Window window);
    bool isWindowHidden(Window window, int windowMapState);
    bool isWindowHiddenByNetWMState(Window window);
    bool isWindowHiddenByWMState(Window window);

    bool isWindowSticky(Window window, long workSpace);
    bool isWindowDock(Window window);

    // Debug methods.
    void logWinInfoForWindow(Window);
    void logWinAttrForWindow(Window);

    void logCurrentTimestamp();
    void logWindowAndAllParents(Window window);

#ifdef __cplusplus
}
#endif