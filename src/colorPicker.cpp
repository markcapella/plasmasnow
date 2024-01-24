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

/** *********************************************************************
 **
 ** Qt-style colorPicker.cpp.
 **
 **/
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#include <iostream>
using namespace std;

#include <QtCore/QString>
#include <QtWidgets/QApplication>
#include <QtWidgets/QColorDialog>
#include <QtWidgets/QWidget>
#include <QtGui/QIcon>
#include <QtCore/QStandardPaths>


/** *********************************************************************
 **
 ** Main Color Picker class def.
 **
 **/
class PlasmaColorDialog : public QColorDialog {
    private:
        bool mAlreadyInitialized;
        bool mAlreadyShowingQPickerDialog;
        char* mActiveCallerName;
        bool mAlreadyTerminated;

        QColor mColor;

    public:
        PlasmaColorDialog() {
            mAlreadyInitialized = false;
            mAlreadyShowingQPickerDialog = false;
            mActiveCallerName = nullptr;
            mAlreadyTerminated = false;

            mColor = Qt::black;
        }

        void reject() {
            hide();
        }
        void accept() {
            setPlasmaColor(currentColor());
            hide();
        }
        void closeEvent(__attribute__((unused)) QCloseEvent *event) {
            hide();
        }

        bool isAlreadyInitialized() {
            return mAlreadyInitialized;
        }
        void setAlreadyInitialized(bool initState) {
            mAlreadyInitialized = initState;
        }

        bool isQPickerActive() {
            return mAlreadyShowingQPickerDialog;
        }
        void setQPickerActive(bool showing) {
            mAlreadyShowingQPickerDialog = showing;
        }

        char* getQPickerCallerName() {
            return mActiveCallerName;
        }
        void setQPickerCallerName(char* callerName) {
            mActiveCallerName = callerName;
        }

        bool isAlreadyTerminated() {
            return mAlreadyTerminated;
        }
        void setAlreadyTerminated(bool termState) {
            mAlreadyTerminated = termState;
        }

        QColor getPlasmaColor() {
            return mColor;
        }
        void setPlasmaColor(QColor color) {
            mColor = color;
        }
};


/** *********************************************************************
 **
 ** Main Globals.
 **
 **/
static int argc = 1;
static std::string argv[] = { "" };
static QApplication* mColorApp = new QApplication(argc, (char**) argv);

static PlasmaColorDialog* mColorDialog = new PlasmaColorDialog();


/** *********************************************************************
 **
 ** Simple Main class extern getter / setters.
 **
 **/
extern "C"
bool isQPickerActive() {
    return mColorDialog->isQPickerActive();
};

extern "C"
char* getQPickerCallerName() {
    return mColorDialog->getQPickerCallerName();
}

extern "C"
bool isQPickerVisible() {
    return mColorDialog->isVisible();
}

extern "C"
bool isQPickerTerminated() {
    return mColorDialog->isAlreadyTerminated();
};


extern "C"
int getQPickerRed() {
    return mColorDialog->getPlasmaColor().red();
};

extern "C"
int getQPickerGreen() {
    return mColorDialog->getPlasmaColor().green();
};

extern "C"
int getQPickerBlue() {
    return mColorDialog->getPlasmaColor().blue();
};


/** *********************************************************************
 **
 ** Show the main colorpicker dialog box, allowing user interaction.
 **
 **/
extern "C"
bool startQPickerDialog(char* inElementTag, char* inColorString) {
    // Early out if we're alreaady active.
    if (mColorDialog->isQPickerActive()) {
        return false;
    }

    // Create initial dialog globals.
    mColorDialog->setWindowTitle("Select Color");
    mColorApp->setWindowIcon(
        QIcon("/usr/local/share/pixmaps/plasmapicker.png"));
    mColorDialog->setOption(QColorDialog::DontUseNativeDialog);
    mColorDialog->setQPickerActive(true);
    mColorDialog->setAlreadyInitialized(true);

    // Create this dialogs specifics.
    mColorDialog->setQPickerCallerName(inElementTag);
    QColor inColor(inColorString);
    if (inColor.isValid()) {
        mColorDialog->setPlasmaColor(inColor);
    }
    mColorDialog->setCurrentColor(mColorDialog->getPlasmaColor());

    mColorDialog->open();
    return true;
}


/** *********************************************************************
 **
 ** Close the main colorPicker dialog box.
 **
 **/
extern "C"
void endQPickerDialog() {
    mColorDialog->setQPickerActive(false);
    mColorDialog->setQPickerCallerName(nullptr);
}


/** *********************************************************************
 **
 ** Uninit all and terminate.
 **
 **/
extern "C"
void uninitQPickerDialog() {
    if (mColorApp != nullptr) {
        endQPickerDialog();
        mColorDialog->setAlreadyTerminated(true);

        mColorApp->closeAllWindows();
        mColorApp = nullptr;
    }
}
