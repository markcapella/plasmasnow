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

#include "plasmasnow.h"
#include <gtk/gtk.h>

int setKillFlakes();
SnowFlake* MakeFlake(int type);

int snow_draw(cairo_t *cr);
void snow_init();
void snow_ui();

void fluffify(SnowFlake *flake, float t);
void printflake(SnowFlake *flake);
int removeAllStormItemsInItemset();

void setGlobalFlakeColor(GdkRGBA);
char* getNextFlakeColorAsString();
GdkRGBA getNextFlakeColorAsRGB();
GdkRGBA getRGBFromString(char* colorString);

int execStormBackgroundThread();

void InitFlake(SnowFlake *flake);
void InitFlakesPerSecond();
void InitSnowColor();
void InitSnowSpeedFactor();

void init_snow_pix();

void genxpmflake(char ***xpm, int w, int h);
void add_random_flakes(int n);

void removeStormItemInItemset(SnowFlake* flake);
void eraseStormItem(SnowFlake* flake);

void SetSnowSize();

void updateFallenSurfacesWithFlake(SnowFlake* flake,
    int xPosition, int yPosition, int flakeWidth);

int execStormItemBackgroundThread(SnowFlake* flake);
