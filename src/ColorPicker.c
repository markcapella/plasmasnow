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
bool mColorPickerActive = false;
char* mColorPickerConsumer;

bool mColorPickerResultAvailable = false;
int mColorPickerResultRed = 0;
int mColorPickerResultGreen = 0;
int mColorPickerResultBlue = 0;
int mColorPickerResultAlpha = 0;

XImage* mColorPickerImage;
XpmAttributes mColorPickerAttributes;

XImage* mColorPickerWindowImage;
Window mColorPickerWindow;


/** ********************************************************
 ** This method starts a ColorPicker widget.
 **/
void startColorPicker(char* consumerTag,
    int xPos, int yPos) {
    if (isColorPickerActive()) {
        return;
    }
    mColorPickerActive = true;

    // Start ColorPicker.
    mColorPickerConsumer = strdup(consumerTag);
    mColorPickerResultAvailable = false;
    mColorPickerResultRed = 0;
    mColorPickerResultGreen = 0;
    mColorPickerResultBlue = 0;
    mColorPickerResultAlpha = 0;

    // Read XPM ColorPicker from file.
    mColorPickerAttributes.valuemask = XpmSize;
    XpmReadFileToImage(mGlobal.display,
        "/usr/local/share/pixmaps/plasmasnowcolorpicker.xpm",
        &mColorPickerImage, NULL, &mColorPickerAttributes);

    // Determine where to position ColorPicker.
    const int SCREEN_WIDTH = WidthOfScreen(
        DefaultScreenOfDisplay(mGlobal.display));
    const int SCREEN_HEIGHT = HeightOfScreen(
        DefaultScreenOfDisplay(mGlobal.display));

    const int ADJUST_X_POS_FOR_OVERLAP = -105;
    const int ADJUST_Y_POS_FOR_OVERLAP = 100;

    int mColorPickerXPos = getMainWindowXPos() +
       xPos + ADJUST_X_POS_FOR_OVERLAP;
    int mColorPickerYPos = getMainWindowYPos() +
       yPos + ADJUST_Y_POS_FOR_OVERLAP ;

    const int COLOR_PICKER_END_X_POS = mColorPickerXPos +
        mColorPickerAttributes.width - 1;
    const int COLOR_PICKER_END_Y_POS = mColorPickerYPos +
        mColorPickerAttributes.height - 1;

    // Sanity check ColorPicker window positions. If
    // window edges off screen at all, center it instead.
    const bool IS_COLORPICKER_CENTERED =
        mColorPickerXPos < 0 || mColorPickerYPos < 0 ||
        COLOR_PICKER_END_X_POS >= SCREEN_WIDTH ||
        COLOR_PICKER_END_Y_POS >= SCREEN_HEIGHT;

    if (IS_COLORPICKER_CENTERED) {
        mColorPickerXPos = (SCREEN_WIDTH -
            mColorPickerAttributes.width) / 2;
        mColorPickerYPos = (SCREEN_HEIGHT -
            mColorPickerAttributes.height) / 2;
    }

    // Get initial ColorPicker window image from current screen.
    mColorPickerWindowImage = XGetImage(mGlobal.display,
        DefaultRootWindow(mGlobal.display), mColorPickerXPos,
        mColorPickerYPos, mColorPickerAttributes.width,
        mColorPickerAttributes.height, XAllPlanes(), ZPixmap);

    // Merge the ColorPicker image into the Window image.
    addColorPickerToWindowImage(mColorPickerWindowImage,
        mColorPickerImage, IS_COLORPICKER_CENTERED);

    // Create our X11 Window to host the merged Image.
    mColorPickerWindow = XCreateSimpleWindow(mGlobal.display,
        DefaultRootWindow(mGlobal.display), 0, 0,
        mColorPickerAttributes.width, mColorPickerAttributes.height,
        1, WhitePixel(mGlobal.display, 0),
        WhitePixel(mGlobal.display, 0));

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
        mColorPickerXPos, mColorPickerYPos);
    XSelectInput(mGlobal.display, mColorPickerWindow,
        ExposureMask);

    // Show SplashImage in the window. Consume X11 events.
    // Respond to Expose event for DRAW.
    int exposedEventCount = 0;
    const int EXPOSED_EVENT_COUNT_NEEDED =
        isThisAGnomeSession() ? 1 : 3;
    bool finalEventReceived = false;

    while (!finalEventReceived) {
        XEvent event;
        XNextEvent(mGlobal.display, &event);
        switch (event.type) {
            case Expose:
                if (++exposedEventCount >=
                    EXPOSED_EVENT_COUNT_NEEDED) {
                    XPutImage(mGlobal.display, mColorPickerWindow,
                        XCreateGC(mGlobal.display, mColorPickerWindow,
                        0, 0), mColorPickerWindowImage, 0, 0, 0, 0,
                        mColorPickerAttributes.width,
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
    if (!isColorPickerActive()) {
        return;
    }
    mColorPickerActive = false;

    mColorPickerConsumer = strdup("");
    mColorPickerResultAvailable = false;
    mColorPickerResultRed = 0;
    mColorPickerResultGreen = 0;
    mColorPickerResultBlue = 0;
    mColorPickerResultAlpha = 0;

    // Close & destroy window, display.
    XFree(mColorPickerWindowImage);
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
    return mColorPickerActive;
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
 ** Updates the ColorPicker XImage with background color
 ** where transparent, and optionally "undraws" the top
 ** pointer arrow.
 **/
void addColorPickerToWindowImage(XImage* windowImage,
    XImage* pickerImage, bool shouldHideArrow) {
    for (int h = 0; h < pickerImage->height; h++) {
        const int H_OFFSET = h * pickerImage->width * 4;

        for (int w = 0; w < pickerImage->width; w++) {
            const int W_OFFSET = H_OFFSET + (w * 4);

            const unsigned char fromByte0 =
                *((pickerImage->data) + W_OFFSET + 0);
            const unsigned char fromByte1 =
                *((pickerImage->data) + W_OFFSET + 1);
            const unsigned char fromByte2 =
                *((pickerImage->data) + W_OFFSET + 2);
            const unsigned char fromByte3 =
                *((pickerImage->data) + W_OFFSET + 3);

            // Don't draw from transparent bytes.
            if (fromByte0 == 0xf0 && fromByte1 == 0xfb &&
                fromByte2 == 0xea) {
                continue;
            }

            // Is ColorPicker "arraw pointer" shown ?
            if (shouldHideArrow) {
                // This "erases" arrow from pickerImage.
                if (h < 25) {
                    continue;
                }
                // This draws a black line under where the
                // arrow would be to complete the outline.
                if (h == 25) {
                    if (w >= 97 && w <= 145) {
                        *((windowImage->data) + W_OFFSET + 0) = 0x00;
                        *((windowImage->data) + W_OFFSET + 1) = 0x00;
                        *((windowImage->data) + W_OFFSET + 2) = 0x00;
                        *((windowImage->data) + W_OFFSET + 3) = 0xff;
                        continue;
                    }
                }
            }

            // Copy Blue, Green, Red, Alpha bytes.
            *((windowImage->data) + W_OFFSET + 0) = fromByte0;
            *((windowImage->data) + W_OFFSET + 1) = fromByte1;
            *((windowImage->data) + W_OFFSET + 2) = fromByte2;
            *((windowImage->data) + W_OFFSET + 3) = fromByte3;
        }
    }
}
