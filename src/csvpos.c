/* -copyright-
#-#
#-# plasmasnow: Let it snow on your desktop
#-# Copyright (C) 1984,1988,1990,1993-1995,2000-2001 Rick Jansen
#-# 	      2019,2020,2021,2022,2023 Willem Vermin
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
#include "csvpos.h"
#include "safe_malloc.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
// given s like "9,2,0" fill k with 9,2,0, set n to 3.
// all values positive, if negative 0 is inserted
// k is allocated in csvpos, free it with free(3)
void csvpos(char *s, int **k, int *n) {
    char *p = s;
    char *q;
    (*n) = 0;
    (*k) = (int *)malloc(sizeof(**k));
    while (p) {
        q = p;
        int i = strtol(p, &q, 0);
        if (p == q) {
            break;
        }
        if (i < 0) {
            i = 0;
        }
        (*n)++;
        (*k) = (int *)realloc(*k, sizeof(**k) * (*n));
        (*k)[(*n) - 1] = i;
        p = strchr(p, ',');
        if (p) {
            p++;
        }
    }
}
void vsc(char **s, int *k, int n) {
    *s = strdup("");
    int i;
    char p[256];
    int l = strlen(*s);
    for (i = 0; i < n; i++) {
        snprintf(p, 250, "%d,", k[i]);
        l += strlen(p);
        *s = (char *)realloc(*s, l + 1);
        strcat(*s, p);
    }
}
