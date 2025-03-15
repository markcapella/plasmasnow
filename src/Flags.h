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

#include <X11/Xlib.h>

#include "PlasmaSnow.h"

#include "doit.h"
#include "safe_malloc.h"


#define UIDO(_x, _y)                                                           \
    if (Flags._x != OldFlags._x) {                                             \
        if (Flags.Noisy) {                                                     \
            printf("%-16s %6d: %-22s %8d -> %8d\n", __FILE__, __LINE__, #_x,   \
                OldFlags._x, Flags._x);                                        \
            fflush(NULL);                                                      \
        }                                                                      \
        {_y} OldFlags._x = Flags._x;                                           \
        Flags.Changes++;                                                       \
    }

#define UIDOS(_x, _y)                                                          \
    if (strcmp(Flags._x, OldFlags._x)) {                                       \
        if (Flags.Noisy) {                                                     \
            printf("%-16s %6d: %-22s %8s -> %8s\n", __FILE__, __LINE__, #_x,   \
                OldFlags._x, Flags._x);                                        \
            fflush(NULL);                                                      \
        }                                                                      \
        {_y} free(OldFlags._x);                                                \
        OldFlags._x = strdup(Flags._x);                                        \
        Flags.Changes++;                                                       \
    }

#define DOIT_I(x, d, v) int x;
#define DOIT_L(x, d, v) unsigned long int x;
#define DOIT_S(x, d, v) char *x;

typedef struct _flags {
        DOITALL
        int dummy;
} FLAGS;
#undef DOIT_I
#undef DOIT_L
#undef DOIT_S

extern FLAGS Flags;
extern FLAGS OldFlags;
extern FLAGS DefaultFlags;
extern FLAGS VintageFlags;


#ifdef __cplusplus
extern "C" {
#endif

    void InitFlags();
    int HandleFlags(int argc, char *argv[]);
    void WriteFlags();

#ifdef __cplusplus
}
#endif

