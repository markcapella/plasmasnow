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


/***********************************************************
 * Module Method stubs.
 */

// Public.
#ifdef __cplusplus
extern "C" {
#endif

    // Getters.
    const char* getPref(const char* prefName);
    void clearPref(const char* prefName);

    bool getBoolPref(const char* prefName, bool defaultValue);
    int getIntPref(const char* prefName, int defaultValue);
    const char* getStringPref(const char* prefName,
        const char* defaultValue);

    // Setters.
    void putBoolPref(const char* prefName, bool boolValue);
    void putIntPref(const char* prefName, int intValue);
    void putStringPref(const char* prefName, const char* cptrValue);

    // Private Helpers.
    void getPrefsMapFromPrefsFile();
    void putPrefsMapToPrefsFile();

    void logAllPrefsInMap(char* msg);
    void logPrefsFile();

#ifdef __cplusplus
}
#endif
