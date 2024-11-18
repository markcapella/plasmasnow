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

#include "plasmasnow.h"


extern void addWindowsModuleToMainloop(void);

#ifdef __cplusplus
extern "C" {
#endif
int WorkspaceActive();
#ifdef __cplusplus
}
#endif

extern void initDisplayDimensions();
extern void updateDisplayDimensions(void);

extern void SetBackground(void);

extern int getXineramaScreenInfo(Display *display,
    int xscreen, int *x, int *y, int *w, int *h);

/***********************************************************
 * Module Method stubs.
 */
// Workspace.
void DetermineVisualWorkspaces();
int do_sendevent();
extern int updateWindowsList();

//**
// Active & Focused X11 Window Helper methods.
extern Window getActiveX11Window();
Window getFocusedX11Window();
int getFocusedX11XPos();
int getFocusedX11YPos();

//**
// ActiveApp member helper methods.
void clearAllActiveAppFields();

extern Window getActiveAppWindow();
void setActiveAppWindow(Window);
Window getParentOfActiveAppWindow();

int getActiveAppXPos();
void setActiveAppXPos(int);

int getActiveAppYPos();
void setActiveAppYPos(int);

//**
// Windows life-cycle methods.
extern void onCursorChange(XEvent*);
extern void onAppWindowChange(Window);

extern void onWindowCreated(XEvent*);
extern void onWindowReparent(XEvent*);
extern void onWindowChanged(XEvent*);

extern void onWindowMapped(XEvent*);
extern void onWindowFocused(XEvent*);
extern void onWindowBlurred(XEvent*);
extern void onWindowUnmapped(XEvent*);

extern void onWindowDestroyed(XEvent*);

//**
// Windows life-cycle helper methods.
bool isMouseClickedAndHeldInWindow();

//**
// Window dragging methods.
void clearAllDragFields();

extern bool isWindowBeingDragged();
void setIsWindowBeingDragged(bool);

Window getWindowBeingDragged();
void setWindowBeingDragged(Window);

Window getActiveAppDragWindowCandidate();
void setActiveAppDragWindowCandidate(Window);

Window getDragWindowOf(Window);

void getWinInfoList();
WinInfo* findWinInfoByWindowId(Window id);

//**
// Debug methods.
void logCurrentTimestamp();
void logWindowAndAllParents(Window window);