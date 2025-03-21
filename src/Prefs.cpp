/* -copyright-
#-# 
#-# plasmasnow: Let it snow on your desktop
#-# Copyright (C) 2024 Mark Capella
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
#include <fstream>
#include <string>

#include <bits/stdc++.h>

using namespace std;
#include "Prefs.h"


/** ***********************************************************
 ** Module globals and consts.
 **/
const string PREFS_FILE_NAME = ".plasmasnowPrefsrc";

bool mPrefsLoaded = false;
vector<pair<string, string>> mPrefsList;


/** ***********************************************************
 ** Prefs Lifecycle Helpers.
 **/
const char* getPref(const char* prefName) {
    if (!mPrefsLoaded) {
        getPrefsMapFromPrefsFile();
    }

    string prefNameString = prefName;
    for (vector<pair<string, string>>::iterator it =
        mPrefsList.begin(); it != mPrefsList.end(); it++) {
        if ((*it).first.compare(prefNameString) == 0) {
            return (*it).second.c_str();
        }
    }
    return "";
}

void clearPref(const char* prefName) {
    string prefNameString = prefName;

    for (vector<pair<string, string>>::iterator it =
        mPrefsList.begin(); it != mPrefsList.end(); it++) {
        if ((*it).first.compare(prefNameString) == 0) {
            mPrefsList.erase(it);
            putPrefsMapToPrefsFile();
            return;
        }
    }
}

/** ***********************************************************
 ** Getters for Prefs.
 **/
bool getBoolPref(const char* prefName, bool defaultValue) {
    const string PREF_VALUE_STRING = getPref(prefName);

    return PREF_VALUE_STRING.empty() ? defaultValue :
        PREF_VALUE_STRING == "true";
}

int getIntPref(const char* prefName, int defaultValue) {
    const string PREF_VALUE_STRING = getPref(prefName);

    return PREF_VALUE_STRING.empty() ? defaultValue :
        stoi(PREF_VALUE_STRING);
}

const char* getStringPref(const char* prefName,
    const char* defaultValue) {
    const string PREF_VALUE_STRING = getPref(prefName);

    return PREF_VALUE_STRING.empty() ? defaultValue :
        getPref(prefName);
}

/** ***********************************************************
 ** Setters for Prefs.
 **/
void putBoolPref(const char* prefName, bool boolValue) {
    const string PREF_NAME = prefName;
    const string BOOL_VALUE = boolValue ? "true" : "false";

    if (!mPrefsLoaded) {
        getPrefsMapFromPrefsFile();
    }

    for (vector<pair<string, string>>::iterator it =
        mPrefsList.begin(); it != mPrefsList.end(); it++) {
        if ((*it).first.compare(PREF_NAME) == 0) {
            mPrefsList.erase(it);
            break;
        }
    }

    mPrefsList.push_back(make_pair(PREF_NAME, BOOL_VALUE));
    putPrefsMapToPrefsFile();
}

void putIntPref(const char* prefName, int intValue) {
    const string PREF_NAME = prefName;
    const string INT_VALUE = to_string(intValue);

    if (!mPrefsLoaded) {
        getPrefsMapFromPrefsFile();
    }

    for (vector<pair<string, string>>::iterator it =
        mPrefsList.begin(); it != mPrefsList.end(); it++) {
        if ((*it).first.compare(PREF_NAME) == 0) {
            mPrefsList.erase(it);
            break;
        }
    }

    mPrefsList.push_back(make_pair(PREF_NAME, INT_VALUE));
    putPrefsMapToPrefsFile();
}

void putStringPref(const char* prefName, const char* cptrValue) {
    const string PREF_NAME = prefName;
    const string CPTR_VALUE = cptrValue;

    if (!mPrefsLoaded) {
        getPrefsMapFromPrefsFile();
    }

    for (vector<pair<string, string>>::iterator it =
        mPrefsList.begin(); it != mPrefsList.end(); it++) {
        if ((*it).first.compare(PREF_NAME) == 0) {
            mPrefsList.erase(it);
            break;
        }
    }

    mPrefsList.push_back(make_pair(PREF_NAME, CPTR_VALUE));
    putPrefsMapToPrefsFile();
}

/** ***********************************************************
 ** This method lazy loads Prefs Name/Value pairs from
 ** file storage into internal map.
 **/
void getPrefsMapFromPrefsFile() {
    if (mPrefsLoaded) {
        return;
    }

    // Ensure file ready for read.
    string prefsFileName = getenv("HOME");
    prefsFileName += "/" + PREFS_FILE_NAME;
    fstream prefsFile;

    prefsFile.open(prefsFileName, ios::in);
    if (!prefsFile.is_open()) {
        mPrefsLoaded = true;
        return;
    }

    // Loop & read all in.
    for (string nameString; getline(prefsFile, nameString);) {
        string valueString; getline(prefsFile, valueString);
        if (!valueString.empty()) {
            mPrefsList.push_back(
                make_pair(nameString, valueString));
        }
    }
    prefsFile.close();

    mPrefsLoaded = true;
}

/** ***********************************************************
 ** This method flushes Prefs Name/Value pairs from
 ** internal map to file storage.
 **/
void putPrefsMapToPrefsFile() {
    // Ensure file ready for update.
    string prefsFileName = getenv("HOME");
    prefsFileName += "/" + PREFS_FILE_NAME;

    fstream prefsFile;
    prefsFile.open(prefsFileName, ios::out);
    if (!prefsFile.is_open()) {
        return;
    }

    // Loop & write all out.
    for (vector<pair<string, string>>::iterator it =
        mPrefsList.begin(); it != mPrefsList.end(); it++) {
        prefsFile << (*it).first << "\n";
        prefsFile << (*it).second << "\n";
    }
    prefsFile << "### Generated File, "
        "delete if corrupted ###" << "\n";

    prefsFile.close();
}

/** ***********************************************************
 ** This method prints Prefs Name/Value pairs from
 ** internal map.
 **/
void logAllPrefsInMap(char* msg) {
    for (vector<pair<string, string>>::iterator it =
        mPrefsList.begin(); it != mPrefsList.end(); it++) {
        cout << "{ \"" << (*it).first.c_str() <<
            "\" : \"" << (*it).second.c_str() << "\" }\n";
    }
}

/** ***********************************************************
 ** This method prints Prefs Name/Value pairs from
 ** file storage.
 **/
void logPrefsFile() {
    // Ensure file ready for read.
    string prefsFileName = getenv("HOME");
    prefsFileName += "/" + PREFS_FILE_NAME;
    fstream prefsFile;

    prefsFile.open(prefsFileName, ios::in);
    if (!prefsFile.is_open()) {
        return;
    }

    // Loop & read all in.
    for (string nameString; getline(prefsFile, nameString);) {
        string valueString; getline(prefsFile, valueString);
        if (!valueString.empty()) {
        }
    }
    prefsFile.close();
}
