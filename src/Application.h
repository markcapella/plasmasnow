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

/***********************************************************
 * Module Method stubs.
 */
// Windows:
Window getActiveX11Window();
Window getActiveAppWindow();

void onCursorChange(XEvent*);
void onAppWindowChange(Window);

void onWindowCreated(XEvent*);
void onWindowReparent(XEvent*);
void onWindowChanged(XEvent*);

void onWindowMapped(XEvent*);
void onWindowFocused(XEvent*);
void onWindowBlurred(XEvent*);
void onWindowUnmapped(XEvent*);

void onWindowDestroyed(XEvent*);

bool isWindowBeingDragged();

// Flake color helper methods.
void setGlobalFlakeColor(GdkRGBA);
GdkRGBA getRGBFromString(char* colorString);

void HandleCpuFactor();
void RestartDisplay();
void appShutdownHook(int);

int handleX11ErrorEvent(Display*, XErrorEvent*);
void getX11Window(Window*);
int drawCairoWindow(void*);
void handleX11CairoDisplayChange();

void drawCairoWindowInternal(cairo_t* cc);
int drawTransparentWindow(gpointer);
void addWindowDrawMethodToMainloop();
gboolean handleTransparentWindowDrawEvents(
    GtkWidget*, cairo_t*, gpointer);
void rectangle_draw(cairo_t*);

int StartWindow();

void SetWindowScale();
int handlePendingX11Events();
int onTimerEventDisplayChanged();

void mybindtestdomain();

char* getDesktopSession();
void respondToWorkspaceSettingsChange();
void setTransparentWindowAbove(GtkWindow* window);
int updateWindowsList();

int doAllUISettingsUpdates();
int do_stopafter();
int handleDisplayConfigurationChange();

void getWinInfoForAllWindows();
