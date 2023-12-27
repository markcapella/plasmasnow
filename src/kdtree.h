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
/*
This file is part of ``kdtree'', a library for working with kd-trees.
Copyright (C) 2007-2011 John Tsiombikas <nuclear@member.fsf.org>

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.
3. The name of the author may not be used to endorse or promote products
   derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
OF SUCH DAMAGE.
*/
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct kdtree;
struct kdres;

/* create a kd-tree for "k"-dimensional data */
extern struct kdtree *kd_create(int k);

/* free the struct kdtree */
extern void kd_free(struct kdtree *tree);

/* remove all the elements from the tree */
extern void kd_clear(struct kdtree *tree);

/* if called with non-null 2nd argument, the function provided
 * will be called on data pointers (see kd_insert) when nodes
 * are to be removed from the tree.
 */
extern void kd_data_destructor(struct kdtree *tree, void (*destr)(void *));

/* insert a node, specifying its position, and optional data */
extern int kd_insert(struct kdtree *tree, const double *pos, void *data);
extern int kd_insertf(struct kdtree *tree, const float *pos, void *data);
extern int kd_insert3(
    struct kdtree *tree, double x, double y, double z, void *data);
extern int kd_insert3f(
    struct kdtree *tree, float x, float y, float z, void *data);

/* Find the nearest node from a given point.
 *
 * This function returns a pointer to a result set with at most one element.
 */
extern struct kdres *kd_nearest(struct kdtree *tree, const double *pos);
extern struct kdres *kd_nearestf(struct kdtree *tree, const float *pos);
extern struct kdres *kd_nearest3(
    struct kdtree *tree, double x, double y, double z);
extern struct kdres *kd_nearest3f(
    struct kdtree *tree, float x, float y, float z);

/* Find the N nearest nodes from a given point.
 *
 * This function returns a pointer to a result set, with at most N elements,
 * which can be manipulated with the kd_res_* functions.
 * The returned pointer can be null as an indication of an error. Otherwise
 * a valid result set is always returned which may contain 0 or more elements.
 * The result set must be deallocated with kd_res_free after use.
 */
/*
extern struct kdres *kd_nearest_n(struct kdtree *tree, const double *pos, int
num); extern struct kdres *kd_nearest_nf(struct kdtree *tree, const float *pos,
int num); extern struct kdres *kd_nearest_n3(struct kdtree *tree, double x,
double y, double z); extern struct kdres *kd_nearest_n3f(struct kdtree *tree,
float x, float y, float z);
*/

/* Find any nearest nodes from a given point within a range.
 *
 * This function returns a pointer to a result set, which can be manipulated
 * by the kd_res_* functions.
 * The returned pointer can be null as an indication of an error. Otherwise
 * a valid result set is always returned which may contain 0 or more elements.
 * The result set must be deallocated with kd_res_free after use.
 */
extern struct kdres *kd_nearest_range(
    struct kdtree *tree, const double *pos, double range);
extern struct kdres *kd_nearest_rangef(
    struct kdtree *tree, const float *pos, float range);
extern struct kdres *kd_nearest_range3(
    struct kdtree *tree, double x, double y, double z, double range);
extern struct kdres *kd_nearest_range3f(
    struct kdtree *tree, float x, float y, float z, float range);

/* frees a result set returned by kd_nearest_range() */
extern void kd_res_free(struct kdres *set);

/* returns the size of the result set (in elements) */
extern int kd_res_size(struct kdres *set);

/* rewinds the result set iterator */
extern void kd_res_rewind(struct kdres *set);

/* returns non-zero if the set iterator reached the end after the last element
 */
extern int kd_res_end(struct kdres *set);

/* advances the result set iterator, returns non-zero on success, zero if
 * there are no more elements in the result set.
 */
extern int kd_res_next(struct kdres *set);

/* returns the data pointer (can be null) of the current result set item
 * and optionally sets its position to the pointers(s) if not null.
 */
extern void *kd_res_item(struct kdres *set, double *pos);
extern void *kd_res_itemf(struct kdres *set, float *pos);
extern void *kd_res_item3(struct kdres *set, double *x, double *y, double *z);
extern void *kd_res_item3f(struct kdres *set, float *x, float *y, float *z);

/* equivalent to kd_res_item(set, 0) */
extern void *kd_res_item_data(struct kdres *set);

#ifdef __cplusplus
}
#endif
