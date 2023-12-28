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
 *
 */
#pragma once

#include "plasmasnow.h"
#include <X11/Intrinsic.h>
#include <X11/Xlib.h>
#include <gtk/gtk.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>

#ifdef __GNUC__
#define Lock_fallen()                                                          \
    __extension__({                                                            \
        P("call lock_fallen\n");                                               \
        int retval = lock_fallen();                                            \
        retval;                                                                \
    })
#define Unlock_fallen()                                                        \
    __extension__({                                                            \
        P("call unlock_fallen\n");                                             \
        int retval = unlock_fallen();                                          \
        retval;                                                                \
    })
#define Lock_fallen_n(x, y)                                                    \
    __extension__({                                                            \
        P("call lock_fallen_n %d %d\n", x, *y);                                \
        int retval = lock_fallen_n(x, y);                                      \
        retval;                                                                \
    })
#else
#define Lock_fallen() lock_fallen()
#define Unlock_fallen() unlock_fallen()
#define Lock_fallen_n(x, y) lock_fallen_n(x, y)
#endif

extern void UpdateFallenSnowPartial(
    FallenSnow *fsnow, int x, int w;
); // used in snow.c

extern int HandleFallenSnow(FallenSnow *fsnow);

extern void fallensnow_init(void);
extern void fallensnow_draw(cairo_t *cr);
extern void fallensnow_erase(void);
extern void fallensnow_ui(void);

extern void InitFallenSnow(void);
extern void CleanFallen(Window id);
extern void CleanFallenArea(FallenSnow *fsnow, int x, int w);
extern void DrawFallen(FallenSnow *fsnow);

extern void GenerateFlakesFromFallen(FallenSnow *fsnow, int x, int w, float vy);
extern void UpdateFallenSnowWithWind(FallenSnow *fsnow, int w, int h);

extern void SetMaxScreenSnowDepth(void);
extern void SetMaxScreenSnowDepthWithLock(void);

extern void UpdateFallenSnowAtBottom(void);

extern int lock_fallen(void);
extern int unlock_fallen(void);

extern int lock_fallen_n(int n, int *c);
extern void fallen_sem_init(void);

extern int IsVisibleFallen(FallenSnow *fsnow);

// insert a node at the start of the list
extern void PushFallenSnow(
    FallenSnow **first, WinInfo *win, int x, int y, int w, int h);

// remove by id
extern int RemoveFallenSnow(FallenSnow **list, Window id);

// print list
extern void PrintFallenSnow(FallenSnow *list);

// free fallen
extern void FreeFallenSnow(FallenSnow *fallen);

// find fallensnow with id
extern FallenSnow *FindFallen(FallenSnow *first, Window id);
