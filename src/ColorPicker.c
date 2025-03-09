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
#include <string.h>
#include <stdbool.h>

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/xpm.h>

#include <gtk/gtk.h>

#include "Application.h"
#include "ColorPicker.h"
#include "MainWindow.h"
#include "PlasmaSnow.h"


/***********************************************************
 ** Main Globals & initialization.
 **/
bool mColorPicker = false;
char* mColorPickerConsumer;

bool mColorPickerResultAvailable = false;
int mColorPickerResultRed = 0;
int mColorPickerResultGreen = 0;
int mColorPickerResultBlue = 0;
int mColorPickerResultAlpha = 0;

XImage* mColorPickerImage;
XpmAttributes mColorPickerAttributes;

XImage* mRootImage;

Window mColorPickerWindow;

/** ********************************************************
 ** This method starts a ColorPicker widget.
 **/
void startColorPicker(char* consumerTag, int xPos, int yPos) {
    if (mColorPicker == true) {
        return;
    }
    mColorPicker = true;

    // Set ColorPicker.
    mColorPickerConsumer = strdup(consumerTag);
    mColorPickerResultAvailable = false;
    mColorPickerResultRed = 0;
    mColorPickerResultGreen = 0;
    mColorPickerResultBlue = 0;
    mColorPickerResultAlpha = 0;

    // Read XPM SplashImage from file.
    mColorPickerAttributes.valuemask = XpmSize;
    XpmReadFileToImage(mGlobal.display,
        "/usr/local/share/pixmaps/plasmasnowcolorpicker.xpm",
        &mColorPickerImage, NULL, &mColorPickerAttributes);

    // Determine where to center Picker.
    const int ADJUST_X_POS_FOR_OVERLAP = -105;
    const int ADJUST_Y_POS_FOR_OVERLAP = 100;

    const int CENTERED_X_POS = getMainWindowXPos() +
        xPos + ADJUST_X_POS_FOR_OVERLAP;
    const int CENTERED_Y_POS = getMainWindowYPos() +
        yPos + ADJUST_Y_POS_FOR_OVERLAP ;

    // Get root image under Splash & merge for transparency.
    mRootImage = XGetImage(mGlobal.display,
        DefaultRootWindow(mGlobal.display), CENTERED_X_POS,
        CENTERED_Y_POS, mColorPickerAttributes.width,
        mColorPickerAttributes.height, XAllPlanes(), ZPixmap);
    copyTransparentXImage(mColorPickerImage, mRootImage);

    // Create our X11 Window to host the Picker Image.
    mColorPickerWindow = XCreateSimpleWindow(mGlobal.display,
        DefaultRootWindow(mGlobal.display), 0, 0,
        mColorPickerAttributes.width, mColorPickerAttributes.height,
        1, WhitePixel(mGlobal.display, 0), WhitePixel(mGlobal.display, 0));

    // Set X11 Window to dock (no titlebar or close button).
    const Atom WINDOW_TYPE = XInternAtom(mGlobal.display,
        "_NET_WM_WINDOW_TYPE", False);
    const long TYPE_VALUE = XInternAtom(mGlobal.display,
        "_NET_WM_WINDOW_TYPE_DOCK", False);
    XChangeProperty(mGlobal.display, mColorPickerWindow, WINDOW_TYPE,
        XA_ATOM, 32, PropModeReplace, (unsigned char*) &TYPE_VALUE, 1);

    // Map, then position window.
    XMapWindow(mGlobal.display, mColorPickerWindow);
    XMoveWindow(mGlobal.display, mColorPickerWindow,
        CENTERED_X_POS, CENTERED_Y_POS);

    // Gnome && KDE get different X11 event counts that
    // determine when the SplashPage has been completely DRAWN.
    const int EXPOSED_EVENT_COUNT_NEEDED =
        isThisAGnomeSession() ? 1 : 3;

    // Show SplashImage in the window. Consume X11 events.
    // Respond to Expose event for DRAW.
    XSelectInput(mGlobal.display, mColorPickerWindow,
        ExposureMask);
    int exposedEventCount = 0;

    bool finalEventReceived = false;
    while (!finalEventReceived) {
        XEvent event;
        XNextEvent(mGlobal.display, &event);
        switch (event.type) {
            case Expose:
                if (++exposedEventCount >= EXPOSED_EVENT_COUNT_NEEDED) {
                    XPutImage(mGlobal.display, mColorPickerWindow,
                        XCreateGC(mGlobal.display, mColorPickerWindow, 0, 0),
                        mRootImage, 0, 0, 0, 0, mColorPickerAttributes.width,
                        mColorPickerAttributes.height);
                    finalEventReceived = true;
                }
                break;
        }
    }
    XFlush(mGlobal.display);
}

/** ********************************************************
 ** This method clears / completes a ColorPicker widget.
 **/
void clearColorPicker() {
    if (mColorPicker == false) {
        return;
    }
    mColorPicker = false;

    mColorPickerConsumer = strdup("");
    mColorPickerResultAvailable = false;
    mColorPickerResultRed = 0;
    mColorPickerResultGreen = 0;
    mColorPickerResultBlue = 0;
    mColorPickerResultAlpha = 0;

    // Close & destroy window, display.
    XFree(mRootImage);
    XpmFreeAttributes(&mColorPickerAttributes);
    XFree(mColorPickerImage);

    XUnmapWindow(mGlobal.display, mColorPickerWindow);
    XDestroyWindow(mGlobal.display, mColorPickerWindow);
}

/** ********************************************************
 ** This method returns is the widget is actively displayed
 ** in ColorPicker mode.
 **/
bool isColorPickerActive() {
    return mColorPicker;
}

/** ********************************************************
 ** This method returns the name tag of the element
 ** we're picking a new color for.
 **/
bool isColorPickerConsumer(char* consumerTag) {
    return strcmp(mColorPickerConsumer, consumerTag) == 0;
}

/** ********************************************************
 ** This method returns whether gthe user has selected a
 ** color from the picker for the element we're changing.
 **/
bool isColorPickerResultAvailable() {
    return mColorPickerResultAvailable;
}
void setColorPickerResultAvailable(bool value) {
    mColorPickerResultAvailable = value;
}

/** ********************************************************
 ** Helper methods for setting and retrieving result values
 ** for the RED RGBA attribute.
 **/
int getColorPickerResultRed() {
    return mColorPickerResultAvailable ?
        mColorPickerResultRed : 0;
}
void setColorPickerResultRed(int value) {
    mColorPickerResultRed = value;
}

/** ********************************************************
 ** Helper methods for setting and retrieving result values
 ** for the GREEN RGBA attribute.
 **/
int getColorPickerResultGreen() {
    return mColorPickerResultAvailable ?
        mColorPickerResultGreen : 0;
}
void setColorPickerResultGreen(int value) {
    mColorPickerResultGreen = value;
}

/** ********************************************************
 ** Helper methods for setting and retrieving result values
 ** for the BLUE RGBA attribute.
 **/
int getColorPickerResultBlue() {
    return mColorPickerResultAvailable ?
        mColorPickerResultBlue : 0;
}
void setColorPickerResultBlue(int value) {
    mColorPickerResultBlue = value;
}

/** ********************************************************
 ** Helper methods for setting and retrieving result values
 ** for the ALPHA RGBA attribute.
 **/
int getColorPickerResultAlpha() {
    return mColorPickerResultAvailable ?
        mColorPickerResultAlpha : 0;
}
void setColorPickerResultAlpha(int value) {
    mColorPickerResultAlpha = value;
}

/** ********************************************************
 ** Copies source XPM onto target XPM.
 **/
void copyTransparentXImage(XImage* fromImage, XImage* toImage) {
    for (int h = 0; h < fromImage->height; h++) {
        const int rowI = h * fromImage->width * 4;

        for (int w = 0; w < fromImage->width; w++) {
            const int byteI = rowI + (w * 4);

            const unsigned char fromByte0 =
                *((fromImage->data) + byteI + 0);
            const unsigned char fromByte1 =
                *((fromImage->data) + byteI + 1);
            const unsigned char fromByte2 =
                *((fromImage->data) + byteI + 2);
            const unsigned char fromByte3 =
                *((fromImage->data) + byteI + 3);

            // Ignore this color, considered transparent.
            if (fromByte0 == 0xf0 && fromByte1 == 0xfb &&
                fromByte2 == 0xea) {
                continue;
            }

            *((toImage->data) + byteI + 0) = fromByte0; // Blue.
            *((toImage->data) + byteI + 1) = fromByte1; // Green.
            *((toImage->data) + byteI + 2) = fromByte2; // Red.
            *((toImage->data) + byteI + 3) = fromByte3;
        }
    }
}

/** ********************************************************
 ** Helper method to debug the window contents.
 **/
void debugXImage(char* tag, XImage* image) {
    printf("\ndebugXImage() width / height : %d x %d\n",
        image->width, image->height);
    printf("debugXImage() xoffset, format : %d, %d\n\n",
        image->xoffset, image->format);

    for (int h = 0; h < image->height; h++) {
        if (h > 3) {
            return;
        }

        const int rowI = h * image->width * 4;
        printf("debugXImage() %s : ", tag);

        for (int w = 0; w < image->width; w++) {
            if (w > 5) {
                break;
            }

            const int byteI = rowI + (w * 4);
            const unsigned char byte0 = *((image->data) + byteI + 0);
            const unsigned char byte1 = *((image->data) + byteI + 1);
            const unsigned char byte2 = *((image->data) + byteI + 2);
            const unsigned char byte3 = *((image->data) + byteI + 3);

            printf("[%02x %02x %02x %02x]  ",
                byte0, byte1, byte2, byte3);
        }

        printf("\n");
    }
}
