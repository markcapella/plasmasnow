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

// required GTK version for running the ui.
// (from ui.xml, made using glade)
#define GTK_MAJOR 3
#define GTK_MINOR 20
#define GTK_MICRO 0


void createMainWindow();
void applyMainWindowCSSTheme();
void addBusyStyleClass();
void removeBusyStyleClass();
void updateMainWindowTheme();

int getMainWindowWidth();
int getMainWindowHeight();
int getMainWindowXPos();
int getMainWindowYPos();

void updateMainWindowUI();
int ui_run_nomenu();
void handle_screen();

void init_buttons();
void initAllButtonValues();
void set_buttons();
void connectAllButtonSignals();
void set_santa_buttons();
void set_tree_buttons();


void birdscb(GtkWidget* w, void* m);
void ui_gray_birds(int m);
void ui_set_birds_header(const char *text);

void setTabDefaults(int tab);
void setLabelText(GtkLabel* label, const gchar* str);
void handle_language(int restart);
void ui_set_celestials_header(const char *text);
void ui_set_sticky(int x);
void ui_gray_ww(const int m);
void ui_gray_below(const int m);

void handleFileChooserPreview(GtkFileChooser* file_chooser,
    gpointer data);
gboolean handleMainWindowStateEvents(GtkWidget* widget,
    GdkEventWindowState* event, gpointer user_data);

int isGtkVersionValid();
char* ui_gtk_version();
char* ui_gtk_required();

// Helpers for LightColors Control Panel settings.
#ifdef __cplusplus
extern "C" {
#endif
	bool shouldShowLightColorRed();
	bool shouldShowLightColorLime();
	bool shouldShowLightColorPurple();
	bool shouldShowLightColorCyan();
	bool shouldShowLightColorGreen();
	bool shouldShowLightColorOrange();
	bool shouldShowLightColorBlue();
	bool shouldShowLightColorPink();
#ifdef __cplusplus
}
#endif

