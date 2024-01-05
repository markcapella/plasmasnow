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


        void closeEvent(__attribute__((unused)) QCloseEvent *event) {
            hide();
        }

        void reject() {
            hide();
        }

        void accept() {
            setPlasmaColor(currentColor());
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
