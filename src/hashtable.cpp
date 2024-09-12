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
#include <pthread.h>
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "hashtable.h"

#ifdef HAVE_UNORDERED_MAP
#include <unordered_map>
#define MAP std::unordered_map
#else
#include <map>
#define MAP std::map
#pragma message __FILE__                                                       \
    ": Using map for the hash table, because unordered_map is not available."
#endif

static MAP<unsigned int, void *> table;

extern "C" {
void table_insert(unsigned int key, void *value) { table[key] = value; }
void *table_get(unsigned int key) { return (table[key]); }
void table_clear(void (*destroy)(void *p)) {
    for (MAP<unsigned int, void *>::iterator it = table.begin();
         it != table.end(); ++it) {
        destroy(it->second);
        it->second = 0;
    }
}
}

#ifdef HAVE_UNORDERED_SET
#include <unordered_set>
#define SET std::unordered_set
#else
#include <set>
#pragma message __FILE__ ": Using set, because unordered_set is not available."
#define SET std::set
#endif

static SET<void *> myset;
static SET<void *>::iterator myset_iter;

extern "C" {
void set_insert(void *key) { myset.insert(key); }
void set_erase(void *key) { myset.erase(key); }
int set_count(void *key) { return myset.count(key); }
void set_clear() { myset.clear(); }
/* example:
 *    set_begin();
 *    void *p;
 *    while ( (p = set_next()) )
 *    {
 *       printf("p=%p\n",p);
 *    }
 */
void set_begin() { myset_iter = myset.begin(); }
void *set_next() {
    if (myset_iter == myset.end()) {
        return 0;
    }
    void *v = *myset_iter;
    myset_iter++;
    return v;
}
unsigned int set_size() { return myset.size(); }
}
