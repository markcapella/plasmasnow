
// Std C.
#include <iostream>
#include <string>
#include <string.h>

// X11.
#include <X11/Xft/Xft.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>

// Application.
#include "MsgBox.h"

using namespace std;

/** ********************************************************
 ** Module globals and consts.
 **/

// Minor styling.
#define COLOR_RED "\033[0;31m"
#define COLOR_GREEN "\033[1;32m"
#define COLOR_YELLOW "\033[1;33m"
#define COLOR_BLUE "\033[1;34m"
#define COLOR_NORMAL "\033[0m"

Display* mDisplay;
Window mMsgBox;
XftFont* mFont;

const XftColor mFontColor = { .pixel = 0x0, .color = { 
    .red = 0xff, .green = 0xff,
    .blue = 0xff, .alpha = 0xffff } };


/** ********************************************************
 ** Module Entry.
 **/
extern "C"
void displayMessageBox(int xPos, int yPos, int width, int height,
    char* title, char* message) {

    // Open X11 display, ensure it's available.
    mDisplay = XOpenDisplay(NULL);
    if (mDisplay == NULL) {
       return;
    }

    mFont = XftFontOpenName(mDisplay, DefaultScreen(mDisplay),
        "-misc-fixed-medium-r-normal--15-140-75-75-c-90-iso8859-16");
    if (mFont == NULL) {
        cout << COLOR_RED << "\nMsgBox: Cannot open XftFont - "
            "FATAL.\n" << COLOR_NORMAL;
        exit(3);
    }

    // Create X11 window.
    mMsgBox = XCreateSimpleWindow(mDisplay,
        DefaultRootWindow(mDisplay), 0, 0, width, height,
        1, BlackPixel(mDisplay, 0), WhitePixel(mDisplay, 0));

    // Set title string.
    XTextProperty properties;
    properties.value = (unsigned char*) title;
    properties.encoding = XA_STRING;
    properties.format = 8;
    properties.nitems = strlen(title);
    XSetWMName(mDisplay, mMsgBox, &properties);

    // Set title icon.
    char* iconName = strdup("plasmasnowbox");
    XClassHint* classHint = XAllocClassHint();
    if (classHint) {
        classHint->res_class = iconName;
        classHint->res_name = iconName;
        XSetClassHint(mDisplay, mMsgBox, classHint);
    }
    XTextProperty iconProperty;
    XStringListToTextProperty(&iconName, 1, &iconProperty);
    XSetWMIconName(mDisplay, mMsgBox, &iconProperty);

    // Map (show) window.
    XMapWindow(mDisplay, mMsgBox);
    XMoveWindow(mDisplay, mMsgBox, xPos, yPos);

    // Select observable x11 events.
    // Select observable x11 client messages.
    XSelectInput(mDisplay, mMsgBox, ExposureMask);
    Atom mDeleteMessage = XInternAtom(mDisplay,
        "WM_DELETE_WINDOW", False);
    XSetWMProtocols(mDisplay, mMsgBox, &mDeleteMessage, 1);

    // Loop events until close event frees us.
    bool msgboxActive = true;
    while (msgboxActive) {
        XEvent event;
        XNextEvent(mDisplay, &event);

        switch (event.type) {
            case ClientMessage:
                if (event.xclient.data.l[0] == (long int) mDeleteMessage) {
                    msgboxActive = false;
                }
                break;

            case Expose:
                XftDraw* textDrawable = XftDrawCreate(mDisplay, mMsgBox,
                    DefaultVisual(mDisplay, DefaultScreen(mDisplay)),
                   DefaultColormap(mDisplay, DefaultScreen(mDisplay)));

                XftDrawString8(textDrawable, &mFontColor, mFont,
                    LEFT_MARGIN, TOP_MARGIN, (const FcChar8*) message,
                    strlen(message));

                break;
        }
    }

    // Close display & done.
    XUnmapWindow(mDisplay, mMsgBox);
    XDestroyWindow(mDisplay, mMsgBox);
    XCloseDisplay(mDisplay);
}
