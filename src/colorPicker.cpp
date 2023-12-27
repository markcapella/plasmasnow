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
            fprintf(stdout, "colorPicker.cpp: PlasmaColorDialog(CONSTRUCTOR) Starts.\n");

            mAlreadyInitialized = false;
            mAlreadyShowingQPickerDialog = false;
            mActiveCallerName = nullptr;
            mAlreadyTerminated = false;

            mColor = Qt::black;

            fprintf(stdout, "colorPicker.cpp: PlasmaColorDialog(CONSTRUCTOR) Finishes.\n");
        }

        void closeEvent(__attribute__((unused)) QCloseEvent *event) {
            fprintf(stdout, "colorPicker.cpp: closeEvent(%d) Starts.\n", isVisible());
            hide();

            fprintf(stdout, "colorPicker.cpp: closeEvent() Finishes.\n");
        }

        void reject() {
            fprintf(stdout, "colorPicker.cpp: reject(%d) Starts.\n", isVisible());
            hide();

            fprintf(stdout, "colorPicker.cpp: reject() Finishes.\n");
        }

        void accept() {
            fprintf(stdout, "colorPicker.cpp: accept(%d) Starts.\n", isVisible());
            setPlasmaColor(currentColor());

            hide();

            fprintf(stdout, "colorPicker.cpp: accept() Finishes.\n");
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
 ** Main app Globals.
 **
 **/
static int argc = 0;
static char **argv = nullptr;

static QApplication* mColorApp = new QApplication(argc, argv);
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
 ** Show the main dialog, allowing user interaction.
 **
 **/
extern "C"
bool showQPickerDialog(char* inElementTag, char* inColorString) {
    fprintf(stdout, "colorPicker.cpp: showQPickerDialog() Starts.\n");
    if (mColorDialog->isQPickerActive()) {
        fprintf(stdout,
            "colorPicker.cpp: showQPickerDialog() Finishes - Can\'t set color"
            " while QPickerDialog is already working.\n");
        return false;
    }

    QColor inColor(inColorString);
    if (inColor.isValid()) {
        mColorDialog->setPlasmaColor(inColor);
    }

    mColorDialog->setWindowTitle("Select Color");
    mColorApp->setWindowIcon(
        QIcon("/usr/local/share/pixmaps/plasmapicker.png"));

    mColorDialog->setAlreadyInitialized(true);
    mColorDialog->setQPickerActive(true);
    mColorDialog->setQPickerCallerName(inElementTag);
    mColorDialog->setOption(QColorDialog::DontUseNativeDialog);
    mColorDialog->setCurrentColor(inColor);
    mColorDialog->open();

    fprintf(stdout, "colorPicker.cpp: showQPickerDialog() Finishes.\n");
    return true;
}


/** *********************************************************************
 **
 ** ...
 **
 **/
extern "C"
void endQPickerDialog() {
    fprintf(stdout, "colorPicker.cpp: endQPickerDialog() Starts.\n");

    mColorDialog->setQPickerActive(false);
    mColorDialog->setQPickerCallerName(nullptr);

    fprintf(stdout, "colorPicker.cpp: endQPickerDialog() Finishes.\n");
}


/** *********************************************************************
 **
 ** Uninit all and terminate.
 **
 **/
extern "C"
void uninitQPickerDialog() {
    fprintf(stdout, "colorPicker.cpp: uninitQPickerDialog() Starts.\n");
    if (mColorApp != nullptr) {
        fprintf(stdout, "colorPicker.cpp: uninitQPickerDialog()    PAYLOAD Starts.\n");

        endQPickerDialog();
        mColorDialog->setAlreadyTerminated(true);

        mColorApp->closeAllWindows();
        mColorApp = nullptr;

        fprintf(stdout, "colorPicker.cpp: uninitQPickerDialog()    PAYLOAD Finishes.\n");
    }

    fprintf(stdout, "colorPicker.cpp: uninitQPickerDialog() Finishes.\n");
}
