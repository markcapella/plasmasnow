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

/* How to implement a new button
 *
 * The generation of code to add a button and/or a flag is dependent
 * on definitions in 'doit.h' and 'buttons.h'.
 *
 * doit.h
 *
 *   definition of names of flags, together with default values and vintage
 *   values example: DOIT_I(HaloBright           ,25         ,25         )
 *
 *   DOIT_I: for flags with an integer value
 *   DOIT_L: for flags with a large value (for example a window-id)
 *   DOIT_S: for flags with a char* value (colors, mostly)
 *
 *   Macro DOIT will call macro's that are not meant for read/write from
 *   .plasmasnowrc Macro DOIT_ALL calls all DOIT_* macro's This will result in:
 *
 *   flags.h: creation of member HaloBright in type FLAGS  (see flags.h) see
 *   flags.c: definition of default value in DefaultFlags.HaloBright  (25)
 *            definition of vintage value in VintageFlags.Halobright  (0)
 *            definition of WriteFlags() to write the flags to .plasmasnowrc
 *            definition of ReadFlags() to read flags from .plasmasnowrc
 *
 * buttons.h
 *   definition of button-related entities.
 *
 *   example:
 *     BUTTON(scalecode      ,plasmasnow_celestials  ,HaloBright           ,1  )
 *     this takes care that flag 'HaloBright' is associated with a button
 *     in the 'celestials' tab with the glade-id 'id-HaloBright' and that a
 *     value of 1 is used in the expansion of scalecode. In this case, the
 * button should be a GtkScale button.
 *
 *   The macro ALL_BUTTONS takes care that scalecode is called as
 *     scalecode(plasmasnow_celestials,HaloBright,1)
 *     and that all other BUTTON macro's are called
 *
 *   The following types of buttons are implemented:
 *     GtkScale (macro scalecode)
 *     GtkToggle(macro togglecode)
 *     GtkColor (macro colorcode)
 *
 *   In this way, the following items are generated:
 *
 *     ui.c:
 *       define type Buttons, containing all flags in buttons.h
 *       associate the elements of Buttons with the corresponding
 *         glade-id's
 *
 *       define call-backs
 *         these call backs have names like 'button_plasmasnow_celestials_HaloBright'
 *         the code ensures that for example Flags.HaloBright gets the value
 *         of the corresponding button.
 *
 *       create a function initAllButtonValues(), that sets all buttons in the
 * state defined by the corresponding Flags. For example, if Flags.HaloBright =
 * 40, the corresponding GtkScale button will be set to this value.
 *
 *       connects signals of buttons to the corresponding call-backs, for
 *         example, button with glade-id 'id-HaloBright', when changed, will
 * result in a call of button_plasmasnow_celestials_HaloBright().
 *
 *       create function setTabDefaults(int tab, int vintage) that gives the
 *         buttons in the given tab (for example 'plasmasnow_celestials') and the
 *         corresponding flags their default (vintage = 0) or vintage
 *
 * (vintage=1) value. One will notice, that some buttons need extra care, for
 * example flag TreeType in plasmasnow_scenery.
 *
 *   glade, ui.glade
 *
 *     Glade is used to maintain 'ui.glade', where the creation of the tabs and
 * the placement of the buttons is arranged. For the buttons in 'buttons.h' a
 * callback is arranged in 'ui.c', so in general there is no need to do
 * something with the 'signals' properties of these buttons. Things that are
 * needed:
 *
 *       - the id, for example: id-HaloBright
 *       - button text, maybe using a GtkLabel
 *       - tooltip
 *       - for scale buttons: a GtkScale, defining for example min and max
 * values
 *       - placement
 *       - for few buttons: a css class.
 *
 *     In Makefile.am, ui.glade is converted to an include file: ui_xml.h
 *     So, when compiled, the program does not need an external file for it's
 *     GtkBuilder.
 *
 *   Handling of changed flags.
 *
 *     In 'flags.h' the macros UIDO and UIDOS are defined. They take care of the
 *     standard action to be used when a flag has been changed: simply copy
 *     the new value to OldFlags and increment Flags.Changes. OldFlags is
 *     initialized at the start of the program, and is used to check if a flag
 * has been changed.
 *
 *     UIDO (for integer valued flags) and UIDOS (for char* valued flags) take
 *     two parameters:
 *
 *     - the name of the flag to check
 *     - C-code to execute if the value of the flag has been changed.
 *
 *     In main.c the flags in the 'settings' tab are handled, and calls are
 *     made to for example respondToScenerySettingsChanges() which is supposed to handle flags
 *     related with the 'scenery' tab. If Flags.Changes > 0, the flags are
 * written to .plasmasnowrc.
 *
 *   Documentation of flags
 *
 *     This is taken care of in 'docs.c'.
 *
 */

#include "buttons.h"
#include <pthread.h>

// undef NEWLINE if one wants to examine the by cpp generated code:
// cpp  ui.c | sed 's/NEWLINE/\n/g'

#define NEWLINE
// #undef NEWLINE
#ifdef NEWLINE

#include <assert.h>
#include <math.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <X11/extensions/Xinerama.h>

#include <glib.h>
#include <glib/gprintf.h>

#include <gtk/gtk.h>

#include "Application.h"
#include "birds.h"
#include "clocks.h"
#include "ColorCodes.h"
#include "ColorPicker.h"
#include "csvpos.h"
#include "Flags.h"
#include "MainWindow.h"
#include "mygettext.h"
#include "pixmaps.h"
#include "PlasmaSnow.h"
#include "safe_malloc.h"
#include "Santa.h"
#include "Storm.h"
#include "ui_xml.h"
#include "Utils.h"
#include "version.h"
#include "Windows.h"


/***********************************************************
 * Module consts.
 */
#ifndef DEBUG
#define DEBUG
#endif
// Flip and rebuild :)
#undef DEBUG

#include "debug.h"
#endif /* NEWLINE */

#ifdef __cplusplus
#define MODULE_EXPORT extern "C" G_MODULE_EXPORT
#else
#define MODULE_EXPORT G_MODULE_EXPORT
#endif


#define PREFIX_SANTA "santa-"

#define SANTA2(x) SANTA(x) SANTA(x##r)

#define SANTA_ALL \
    SANTA2(0) SANTA2(1) SANTA2(2) \
    SANTA2(3) SANTA2(4)


#define PREFIX_TREE "tree-"
#define TREE_ALL \
    TREE(0) TREE(1) TREE(2) TREE(3) TREE(4) \
    TREE(5) TREE(6) TREE(7) TREE(8) TREE(9)


#define DEFAULT(name) DefaultFlags.name
#define VINTAGE(name) VintageFlags.name

GtkBuilder* builder;
GtkWidget* range;
GtkContainer* birdsgrid;
GtkContainer* moonbox;
GtkImage* preview;

int Nscreens;
int HaveXinerama;

#define nsbuffer 1024
char sbuffer[nsbuffer];

int ui_running = False;
int human_interaction = 1;

GtkWidget* mMainWindow;
GtkStyleContext* mStyleContext;

char* lang[100];
int nlang;


/***********************************************************
 ** UI Main Methods.
 **/
void updateMainWindowUI() {
    UIDOS(Language, handle_language(1););

    UIDO(Screen, handle_screen(););
    UIDO(mAppTheme, updateMainWindowTheme(););
    UIDO(Outline, clearGlobalSnowWindow(););

    UIDO(ShowSplashScreen,);
}

void handle_screen() {
    if (HaveXinerama && Nscreens > 1) {
        mGlobal.ForceRestart = 1;
    }
}

void handle_language(int restart) {
    if (!strcmp(Flags.Language, "sys")) {
        unsetenv("LANGUAGE");
    } else {
        setenv("LANGUAGE", Flags.Language, 1);
    }

    if (restart) {
        mGlobal.ForceRestart = 1;
    }
}

/***********************************************************
 ** Main WindowState event handler.
 **/
gboolean handleMainWindowStateEvents(
    __attribute__((unused)) GtkWidget* widget,
    __attribute__((unused)) GdkEventWindowState* event,
    __attribute__((unused)) gpointer user_data) {

    // Convenient App level Event hook.
    //if (event->type == GDK_WINDOW_STATE) {
    //    if (event->new_window_state & GDK_WINDOW_STATE_ICONIFIED) {
    //    }
    //}

    return false;
}

/***********************************************************
 ** Santa helpers.
 **/

typedef struct {
        char *imid;
        GtkWidget *button;
        gdouble value;
} SantaButton;

// NBUTTONS is number of Santas to choose from.
#define NBUTTONS (2 * (MAXSANTA + 1))
#define SANTA(x) NEWLINE SantaButton santa_##x;

static struct {
        SANTA_ALL
} SantaButtons;
#include "undefall.inc"

#define SANTA(x) NEWLINE &SantaButtons.santa_##x,

static SantaButton *santa_barray[NBUTTONS] = {SANTA_ALL};
#include "undefall.inc"

static void init_santa_buttons() {

#define SANTA(x) \
    NEWLINE SantaButtons.santa_##x.button = \
        GTK_WIDGET(gtk_builder_get_object(builder, PREFIX_SANTA #x));

    SANTA_ALL;

#include "undefall.inc"

#define SANTA(x) \
    NEWLINE gtk_widget_set_name(SantaButtons.santa_##x.button, PREFIX_SANTA #x);

    SANTA_ALL;

#include "undefall.inc"
}

void set_santa_buttons() {
    int n = 2 * Flags.SantaSize;
    if (Flags.Rudolf) {
        n++;
    }
    if (n < NBUTTONS) {
        gtk_toggle_button_set_active(
            GTK_TOGGLE_BUTTON(santa_barray[n]->button), TRUE);
    }
}

MODULE_EXPORT
void button_santa(GtkWidget *w) {
    if (!human_interaction) {
        return;
    }

    if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w))) {
        return;
    }

    const gchar *s = gtk_widget_get_name(w) + strlen(PREFIX_SANTA);
    int santa_type = atoi(s);
    int have_rudolf = ('r' == s[strlen(s) - 1]);

    Flags.SantaSize = santa_type;
    Flags.Rudolf = have_rudolf;

    SantaVisible();
}

/***********************************************************
 ** Tree helpers.
 **/

typedef struct _tree_button {

        GtkWidget *button;

} tree_button;

#define TREE(x) NEWLINE tree_button tree_##x;

static struct _tree_buttons {

        TREE_ALL

} tree_buttons;

#include "undefall.inc"

/***********************************************************
 ** Create all button stubs.
 **/

#define togglecode(type, name, m) NEWLINE GtkWidget *name;
#define scalecode togglecode
#define colorcode togglecode
#define filecode togglecode

static struct _button {

        ALL_BUTTONS

        // QColorDialog "Widgets".
        GtkWidget* StormItemColor1;
        GtkWidget* StormItemColor2;

        GtkWidget* BirdsColor;
        GtkWidget* TreeColor;

        GtkWidget* LightColorRed;
        GtkWidget* LightColorLime;
        GtkWidget* LightColorPurple;
        GtkWidget* LightColorCyan;
        GtkWidget* LightColorGreen;
        GtkWidget* LightColorOrange;
        GtkWidget* LightColorBlue;
        GtkWidget* LightColorPink;

        GtkWidget* ShowLightColorRed;
        GtkWidget* ShowLightColorLime;
        GtkWidget* ShowLightColorPurple;
        GtkWidget* ShowLightColorCyan;
        GtkWidget* ShowLightColorGreen;
        GtkWidget* ShowLightColorOrange;
        GtkWidget* ShowLightColorBlue;
        GtkWidget* ShowLightColorPink;
} Button;

#include "undefall.inc"

/***********************************************************
 ** Get all button form ID & accessors.
 **/

#define ID "id"

#define togglecode(type, name, m)                                              \
    NEWLINE Button.name =                                                      \
        GTK_WIDGET(gtk_builder_get_object(builder, ID "-" #name));

#define scalecode togglecode
#define colorcode togglecode
#define filecode togglecode

static void getAllButtonFormIDs() {
    ALL_BUTTONS

    // QColorDialog "Widgets".
    Button.StormItemColor1 = (GtkWidget*)
        gtk_builder_get_object(builder, "id-StormItemColor1");
    Button.StormItemColor2 = (GtkWidget*)
        gtk_builder_get_object(builder, "id-StormItemColor2");

    Button.BirdsColor = (GtkWidget*)
        gtk_builder_get_object(builder, "id-BirdsColor");
    Button.TreeColor = (GtkWidget*)
        gtk_builder_get_object(builder, "id-TreeColor");

    Button.LightColorRed = (GtkWidget*)
        gtk_builder_get_object(builder, "id-LightColorRed");
    Button.LightColorLime = (GtkWidget*)
        gtk_builder_get_object(builder, "id-LightColorLime");
    Button.LightColorPurple = (GtkWidget*)
        gtk_builder_get_object(builder, "id-LightColorPurple");
    Button.LightColorCyan = (GtkWidget*)
        gtk_builder_get_object(builder, "id-LightColorCyan");
    Button.LightColorGreen = (GtkWidget*)
        gtk_builder_get_object(builder, "id-LightColorGreen");
    Button.LightColorOrange = (GtkWidget*)
        gtk_builder_get_object(builder, "id-LightColorOrange");
    Button.LightColorBlue = (GtkWidget*)
        gtk_builder_get_object(builder, "id-LightColorBlue");
    Button.LightColorPink = (GtkWidget*)
        gtk_builder_get_object(builder, "id-LightColorPink");

    Button.ShowLightColorRed = (GtkWidget*)
        gtk_builder_get_object(builder, "id-ShowLightColorRed");
    Button.ShowLightColorLime = (GtkWidget*)
        gtk_builder_get_object(builder, "id-ShowLightColorLime");
    Button.ShowLightColorPurple = (GtkWidget*)
        gtk_builder_get_object(builder, "id-ShowLightColorPurple");
    Button.ShowLightColorCyan = (GtkWidget*)
        gtk_builder_get_object(builder, "id-ShowLightColorCyan");
    Button.ShowLightColorGreen = (GtkWidget*)
        gtk_builder_get_object(builder, "id-ShowLightColorGreen");
    Button.ShowLightColorOrange = (GtkWidget*)
        gtk_builder_get_object(builder, "id-ShowLightColorOrange");
    Button.ShowLightColorBlue = (GtkWidget*)
        gtk_builder_get_object(builder, "id-ShowLightColorBlue");
    Button.ShowLightColorPink = (GtkWidget*)
        gtk_builder_get_object(builder, "id-ShowLightColorPink");
}

#include "undefall.inc"

/***********************************************************
 ** Define all button SET methods.
 **/

#define buttoncb(type, name) button_##type##_##name

#define togglecode(type, name, m)                                              \
    NEWLINE MODULE_EXPORT void buttoncb(type, name)(GtkWidget * w) NEWLINE {   \
        NEWLINE if (!human_interaction) return;                                \
        NEWLINE gint active =                                                  \
            gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w));                \
        NEWLINE if (active) Flags.name = TRUE;                                 \
        else Flags.name = FALSE;                                               \
        NEWLINE if (m < 0) Flags.name = !Flags.name;                           \
        NEWLINE                                                                \
    }

#define scalecode(type, name, m)                                               \
    NEWLINE MODULE_EXPORT void buttoncb(type, name)(GtkWidget * w) NEWLINE {   \
        NEWLINE if (!human_interaction) return;                                \
        NEWLINE gdouble value;                                                 \
        NEWLINE value = gtk_range_get_value(GTK_RANGE(w));                     \
        NEWLINE Flags.name = m * lrint(value);                                 \
        NEWLINE                                                                \
    }

#define colorcode(type, name, m)                                               \
    NEWLINE MODULE_EXPORT void buttoncb(type, name)(GtkWidget * w) NEWLINE {   \
        NEWLINE if (!human_interaction) {                                      \
            NEWLINE return;                                                    \
            NEWLINE                                                            \
        }                                                                      \
        NEWLINE                                                                \
        NEWLINE GdkRGBA color;                                                 \
        NEWLINE gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(w), &color);      \
        NEWLINE free(Flags.name);                                              \
        NEWLINE rgba2color(&color, &Flags.name);                               \
        NEWLINE                                                                \
    }

#define filecode(type, name, m)                                                \
    NEWLINE MODULE_EXPORT void buttoncb(type, name)(GtkWidget * w) NEWLINE {   \
        NEWLINE if (!human_interaction) return;                                \
        NEWLINE gchar *filename;                                               \
        NEWLINE filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(w)); \
        NEWLINE free(Flags.name);                                              \
        NEWLINE Flags.name = strdup(filename);                                 \
        NEWLINE g_free(filename);                                              \
        NEWLINE                                                                \
    }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

ALL_BUTTONS

/***********************************************************
 ** Helpers for LightColors Control Panel settings.
 **/
void onClickedStormItemColor1() {
    if (!human_interaction) {
        return;
    }

    Cursor cursor = XCreateFontCursor(mGlobal.display, 130);
    int grabbedPointer = XGrabPointer(mGlobal.display,
        DefaultRootWindow(mGlobal.display), false,
        ButtonPressMask, GrabModeAsync, GrabModeAsync,
        None, cursor, CurrentTime);
    if (grabbedPointer == GrabSuccess) {
        GtkAllocation allocation;
        gtk_widget_get_allocation(Button.StormItemColor1, &allocation);
        startColorPicker("StormItemColor1",
            allocation.x,
            allocation.y);
    }
}

void onClickedStormItemColor2() {
    if (!human_interaction) {
        return;
    }

    Cursor cursor = XCreateFontCursor(mGlobal.display, 130);
    int grabbedPointer = XGrabPointer(mGlobal.display,
        DefaultRootWindow(mGlobal.display), false,
        ButtonPressMask, GrabModeAsync, GrabModeAsync,
        None, cursor, CurrentTime);
    if (grabbedPointer == GrabSuccess) {
        GtkAllocation allocation;
        gtk_widget_get_allocation(Button.StormItemColor2, &allocation);
        startColorPicker("StormItemColor2",
            allocation.x,
            allocation.y);
    }
}

void onClickedBirdsColor() {
    if (!human_interaction) {
        return;
    }

    Cursor cursor = XCreateFontCursor(mGlobal.display, 130);
    int grabbedPointer = XGrabPointer(mGlobal.display,
        DefaultRootWindow(mGlobal.display), false,
        ButtonPressMask, GrabModeAsync, GrabModeAsync,
        None, cursor, CurrentTime);
    if (grabbedPointer == GrabSuccess) {
        GtkAllocation allocation;
        gtk_widget_get_allocation(Button.BirdsColor, &allocation);
        startColorPicker("BirdsColor",
            allocation.x,
            allocation.y);
    }
}

void onClickedTreeColor() {
    if (!human_interaction) {
        return;
    }

    Cursor cursor = XCreateFontCursor(mGlobal.display, 130);
    int grabbedPointer = XGrabPointer(mGlobal.display,
        DefaultRootWindow(mGlobal.display), false,
        ButtonPressMask, GrabModeAsync, GrabModeAsync,
        None, cursor, CurrentTime);
    if (grabbedPointer == GrabSuccess) {
        GtkAllocation allocation;
        gtk_widget_get_allocation(Button.TreeColor, &allocation);
        startColorPicker("TreeColor",
            allocation.x,
            allocation.y);
    }
}

void onClickedLightColorRed() {
    if (human_interaction) {
        Flags.ShowLightColorRed = !Flags.ShowLightColorRed;
    }
}
void onClickedLightColorLime() {
    if (human_interaction) {
        Flags.ShowLightColorLime = !Flags.ShowLightColorLime;
    }
}
void onClickedLightColorPurple() {
    if (human_interaction) {
        Flags.ShowLightColorPurple = !Flags.ShowLightColorPurple;
    }
}
void onClickedLightColorCyan() {
    if (human_interaction) {
        Flags.ShowLightColorCyan = !Flags.ShowLightColorCyan;
    }
}
void onClickedLightColorGreen() {
    if (human_interaction) {
        Flags.ShowLightColorGreen = !Flags.ShowLightColorGreen;
    }
}
void onClickedLightColorOrange() {
    if (human_interaction) {
        Flags.ShowLightColorOrange = !Flags.ShowLightColorOrange;
    }
}
void onClickedLightColorBlue() {
    if (human_interaction) {
        Flags.ShowLightColorBlue = !Flags.ShowLightColorBlue;
    }
}
void onClickedLightColorPink() {
    if (human_interaction) {
        Flags.ShowLightColorPink = !Flags.ShowLightColorPink;
    }
}

/***********************************************************
 ** Helper for ...
 **/
bool shouldShowLightColorRed() {
    return Flags.ShowLightColorRed;
}
bool shouldShowLightColorLime() {
    return Flags.ShowLightColorLime;
}
bool shouldShowLightColorPurple() {
    return Flags.ShowLightColorPurple;
}
bool shouldShowLightColorCyan() {
    return Flags.ShowLightColorCyan;
}
bool shouldShowLightColorGreen() {
    return Flags.ShowLightColorGreen;
}
bool shouldShowLightColorOrange() {
    return Flags.ShowLightColorOrange;
}
bool shouldShowLightColorBlue() {
    return Flags.ShowLightColorBlue;
}
bool shouldShowLightColorPink() {
    return Flags.ShowLightColorPink;
}

#pragma GCC diagnostic pop
#include "undefall.inc"

/***********************************************************
 ** Init all buttons values.
 **/
#define togglecode(type, name, m)                                              \
    NEWLINE if (m) {                                                           \
        NEWLINE if (m > 0) {                                                   \
            NEWLINE gtk_toggle_button_set_active(                              \
                NEWLINE GTK_TOGGLE_BUTTON(Button.name), Flags.name);           \
            NEWLINE                                                            \
        }                                                                      \
        else {                                                                 \
            NEWLINE gtk_toggle_button_set_active(                              \
                NEWLINE GTK_TOGGLE_BUTTON(Button.name), !Flags.name);          \
            NEWLINE                                                            \
        }                                                                      \
        NEWLINE                                                                \
    }

#define scalecode(type, name, m)                                               \
    NEWLINE gtk_range_set_value(                                               \
        GTK_RANGE(Button.name), m *((gdouble)Flags.name));

#define colorcode(type, name, m)                                               \
    NEWLINE                                                                    \
    NEWLINE gdk_rgba_parse(&color, Flags.name);                                \
    NEWLINE gtk_color_chooser_set_rgba(                                        \
        GTK_COLOR_CHOOSER(Button.name), &color);                               \
    NEWLINE

#define filecode(type, name, m)                                                \
    NEWLINE gtk_file_chooser_set_filename(                                     \
        GTK_FILE_CHOOSER(Button.name), Flags.BackgroundFile);

void initAllButtonValues() {
    GdkRGBA color;

    ALL_BUTTONS

    // QColorDialog Widgets.
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"

    gdk_rgba_parse(&color, Flags.StormItemColor1);
    gtk_widget_override_background_color(
        Button.StormItemColor1, GTK_STATE_FLAG_NORMAL, &color);

    gdk_rgba_parse(&color, Flags.StormItemColor2);
    gtk_widget_override_background_color(
        Button.StormItemColor2, GTK_STATE_FLAG_NORMAL, &color);

    gdk_rgba_parse(&color, Flags.BirdsColor);
    gtk_widget_override_background_color(
        Button.BirdsColor, GTK_STATE_FLAG_NORMAL, &color);

    gdk_rgba_parse(&color, Flags.TreeColor);
    gtk_widget_override_background_color(
        Button.TreeColor, GTK_STATE_FLAG_NORMAL, &color);

    // Lights module.
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(
        Button.ShowLightColorRed), Flags.ShowLightColorRed);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(
        Button.LightColorRed), Flags.ShowLightColorRed);
    gdk_rgba_parse(&color, Flags.LightColorRed);
    gtk_widget_override_background_color(Button.LightColorRed,
        GTK_STATE_FLAG_NORMAL, &color);

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(
        Button.ShowLightColorLime), Flags.ShowLightColorLime);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(
        Button.LightColorLime), Flags.ShowLightColorLime);
    gdk_rgba_parse(&color, Flags.LightColorLime);
    gtk_widget_override_background_color(Button.LightColorLime,
        GTK_STATE_FLAG_NORMAL, &color);

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(
        Button.ShowLightColorPurple), Flags.ShowLightColorPurple);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(
        Button.LightColorPurple), Flags.ShowLightColorPurple);
    gdk_rgba_parse(&color, Flags.LightColorPurple);
    gtk_widget_override_background_color(Button.LightColorPurple,
        GTK_STATE_FLAG_NORMAL, &color);

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(
        Button.ShowLightColorCyan), Flags.ShowLightColorCyan);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(
        Button.LightColorCyan), Flags.ShowLightColorCyan);
    gdk_rgba_parse(&color, Flags.LightColorCyan);
    gtk_widget_override_background_color(Button.LightColorCyan,
        GTK_STATE_FLAG_NORMAL, &color);

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(
        Button.ShowLightColorGreen), Flags.ShowLightColorGreen);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(
        Button.LightColorGreen), Flags.ShowLightColorGreen);
    gdk_rgba_parse(&color, Flags.LightColorGreen);
    gtk_widget_override_background_color(Button.LightColorGreen,
        GTK_STATE_FLAG_NORMAL, &color);

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(
        Button.ShowLightColorOrange), Flags.ShowLightColorOrange);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(
        Button.LightColorOrange), Flags.ShowLightColorOrange);
    gdk_rgba_parse(&color, Flags.LightColorOrange);
    gtk_widget_override_background_color(Button.LightColorOrange,
        GTK_STATE_FLAG_NORMAL, &color);

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(
        Button.ShowLightColorBlue), Flags.ShowLightColorBlue);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(
        Button.LightColorBlue), Flags.ShowLightColorBlue);
    gdk_rgba_parse(&color, Flags.LightColorBlue);
    gtk_widget_override_background_color(Button.LightColorBlue,
        GTK_STATE_FLAG_NORMAL, &color);

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(
        Button.ShowLightColorPink), Flags.ShowLightColorPink);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(
        Button.LightColorPink), Flags.ShowLightColorPink);
    gdk_rgba_parse(&color, Flags.LightColorPink);
    gtk_widget_override_background_color(Button.LightColorPink,
        GTK_STATE_FLAG_NORMAL, &color);

    #pragma GCC diagnostic pop
}

#include "undefall.inc"

/***********************************************************
 ** Hook all buttons to their action methods.
 **/

#define togglecode(type, name, m)                                              \
    NEWLINE g_signal_connect(G_OBJECT(Button.name), "toggled",                 \
        G_CALLBACK(buttoncb(type, name)), NULL);

#define scalecode(type, name, m)                                               \
    NEWLINE g_signal_connect(G_OBJECT(Button.name), "value-changed",           \
        G_CALLBACK(buttoncb(type, name)), NULL);

#define colorcode(type, name, m)                                               \
    NEWLINE g_signal_connect(G_OBJECT(Button.name), "color-set",               \
        G_CALLBACK(buttoncb(type, name)), NULL);

#define filecode(type, name, m)                                                \
    NEWLINE g_signal_connect(G_OBJECT(Button.name), "file-set",                \
        G_CALLBACK(buttoncb(type, name)), NULL);


void connectAllButtonSignals() {

    ALL_BUTTONS

    // QColorDialog "Widgets".
    g_signal_connect(G_OBJECT(Button.StormItemColor1), "toggled",
        G_CALLBACK(onClickedStormItemColor1), NULL);
    g_signal_connect(G_OBJECT(Button.StormItemColor2), "toggled",
        G_CALLBACK(onClickedStormItemColor2), NULL);
    g_signal_connect(G_OBJECT(Button.BirdsColor), "toggled",
        G_CALLBACK(onClickedBirdsColor), NULL);
    g_signal_connect(G_OBJECT(Button.TreeColor), "toggled",
        G_CALLBACK(onClickedTreeColor), NULL);

    g_signal_connect(G_OBJECT(Button.LightColorRed), "toggled",
        G_CALLBACK(onClickedLightColorRed), NULL);
    g_signal_connect(G_OBJECT(Button.LightColorLime), "toggled",
        G_CALLBACK(onClickedLightColorLime), NULL);
    g_signal_connect(G_OBJECT(Button.LightColorPurple), "toggled",
        G_CALLBACK(onClickedLightColorPurple), NULL);
    g_signal_connect(G_OBJECT(Button.LightColorCyan), "toggled",
        G_CALLBACK(onClickedLightColorCyan), NULL);
    g_signal_connect(G_OBJECT(Button.LightColorGreen), "toggled",
        G_CALLBACK(onClickedLightColorGreen), NULL);
    g_signal_connect(G_OBJECT(Button.LightColorOrange), "toggled",
        G_CALLBACK(onClickedLightColorOrange), NULL);
    g_signal_connect(G_OBJECT(Button.LightColorBlue), "toggled",
        G_CALLBACK(onClickedLightColorBlue), NULL);
    g_signal_connect(G_OBJECT(Button.LightColorPink), "toggled",
        G_CALLBACK(onClickedLightColorPink), NULL);
}

#include "undefall.inc"

/***********************************************************
 ** Tree helpers.
 **/

static void initTreeButtons() {

#define TREE(x)                                                                \
    NEWLINE tree_buttons.tree_##x.button =                                     \
        GTK_WIDGET(gtk_builder_get_object(builder, PREFIX_TREE #x));

    TREE_ALL;

#include "undefall.inc"

#define TREE(x)                                                                \
    NEWLINE gtk_widget_set_name(tree_buttons.tree_##x.button, PREFIX_TREE #x);

    TREE_ALL;

#include "undefall.inc"
}

/***********************************************************
 ** Pixmap helpers.
 **/

static void init_santa_pixmaps() {
#define SANTA(x)                                                               \
    NEWLINE SantaButtons.santa_##x.imid = (char *)PREFIX_SANTA #x "-imid";

    SANTA_ALL;

#include "undefall.inc"

    GtkImage *image;
    GdkPixbuf *pixbuf;

    for (int i = 0; i < NBUTTONS; i++) {
        pixbuf = gdk_pixbuf_new_from_xpm_data(
            (const char **)Santas[i / 2][i % 2][0]);
        image =
            GTK_IMAGE(gtk_builder_get_object(builder, santa_barray[i]->imid));

        gtk_image_set_from_pixbuf(image, pixbuf);
        g_object_unref(pixbuf);
    }
}

static void init_tree_pixmaps() {
    GtkImage *image;
    GdkPixbuf *pixbuf;
#define TREE(x)                                                                \
    NEWLINE pixbuf = gdk_pixbuf_new_from_xpm_data((const char **)xpmtrees[x]); \
    NEWLINE image =                                                            \
        GTK_IMAGE(gtk_builder_get_object(builder, "treeimage" #x));            \
    NEWLINE gtk_image_set_from_pixbuf(image, pixbuf);                          \
    NEWLINE g_object_unref(pixbuf);

    TREE_ALL;

#include "undefall.inc"
}

static void init_hello_pixmaps() {
    // pixbuf1 = gdk_pixbuf_scale_simple(pixbuf, 64, 64, GDK_INTERP_BILINEAR);

    GtkImage *aboutPlasmaLogo =
        GTK_IMAGE(gtk_builder_get_object(builder, "id-plasmasnowLogo"));
    GdkPixbuf *aboutPlasmaPixbug =
        gdk_pixbuf_new_from_xpm_data((XPM_TYPE **) plasmasnow_logo);
    gtk_image_set_from_pixbuf(aboutPlasmaLogo, aboutPlasmaPixbug);

    g_object_unref(aboutPlasmaPixbug);
}

static void init_pixmaps() {
    init_hello_pixmaps();

    init_santa_pixmaps();
    init_tree_pixmaps();
}

/***********************************************************
 ** Helpers.
 **/

void set_tree_buttons() {

#define TREE(x) \
    NEWLINE gtk_toggle_button_set_active( \
        GTK_TOGGLE_BUTTON(tree_buttons.tree_##x.button), FALSE);

    TREE_ALL;
    #include "undefall.inc"

    int* a, n;
    csvpos(Flags.TreeType, &a, &n);

    for (int i = 0; i < n; i++) {
        switch (a[i]) {
            #define TREE(x) \
                NEWLINE case x: \
                gtk_toggle_button_set_active( \
                    GTK_TOGGLE_BUTTON(tree_buttons.tree_##x.button), TRUE); \
                NEWLINE break;

            TREE_ALL;
            #include "undefall.inc"
        }
    }

    free(a);
}

/***********************************************************
 ** Helpers.
 **/

MODULE_EXPORT
void combo_screen(GtkComboBoxText *combo, gpointer data) {
    const int num = gtk_combo_box_get_active(
        GTK_COMBO_BOX(combo));

    Flags.Screen = num - 1;
}

MODULE_EXPORT
void onSelectedLanguageButton(GtkComboBoxText *combo, gpointer data) {
    const int num = gtk_combo_box_get_active(
        GTK_COMBO_BOX(combo));

    Flags.Language = strdup(lang[num]);
}

/***********************************************************
 ** Helpers.
 **/
static void init_general_buttons() {
    setLabelText(GTK_LABEL(gtk_builder_get_object(builder,
        "id-version")), "plasmasnow-" VERSION);
}

/***********************************************************
 ** Helpers.
 **/
typedef struct _snow_button {
        GtkWidget *button;
} snow_button;

/***********************************************************
 ** Helpers.
 **/

void ui_set_birds_header(const char *text) {
    if (!ui_running) {
        return;
    }
    GtkWidget *birds_header =
        GTK_WIDGET(gtk_builder_get_object(builder, "birds-header"));
    setLabelText(GTK_LABEL(birds_header), text);
}

void ui_set_celestials_header(const char *text) {
    if (!ui_running) {
        return;
    }
    GtkWidget *celestials_header =
        GTK_WIDGET(gtk_builder_get_object(builder, "celestials-header"));
    char *a = strdup(gtk_label_get_text(GTK_LABEL(celestials_header)));
    a = (char *)realloc(a, strlen(a) + 2 + strlen(text));
    REALLOC_CHECK(a);
    strcat(a, "\n");
    strcat(a, text);
    setLabelText(GTK_LABEL(celestials_header), a);
    free(a);
}

/***********************************************************
 ** Set default tabs.
 **/

void setTabDefaults(int tab) {
    int h = human_interaction = 0;

    // don't want to clear backgroundfile
    char *background = strdup(Flags.BackgroundFile);

#define togglecode(type, name, m)                                              \
    NEWLINE if (type == tab) Flags.name = DEFAULT(name);
#define scalecode togglecode
#define colorcode(type, name, m)                                               \
    NEWLINE if (type == tab) NEWLINE {                                         \
        free(Flags.name);                                                      \
        Flags.name = strdup(DEFAULT(name));                                    \
    }
#define filecode colorcode

    ALL_BUTTONS;

#include "undefall.inc"

    if (tab == 1) {
        Flags.StormItemColor1 = DefaultFlags.StormItemColor1;
        Flags.StormItemColor2 = DefaultFlags.StormItemColor2;
    }

    // Lights module on Tab #2 (Xmas/Santa).
    if (tab == 2) {
        // Default which light colors are active.
        Flags.ShowLightColorRed = DefaultFlags.ShowLightColorRed;
        Flags.ShowLightColorLime = DefaultFlags.ShowLightColorLime;
        Flags.ShowLightColorPurple = DefaultFlags.ShowLightColorPurple;
        Flags.ShowLightColorCyan = DefaultFlags.ShowLightColorCyan;
        Flags.ShowLightColorGreen = DefaultFlags.ShowLightColorGreen;
        Flags.ShowLightColorOrange = DefaultFlags.ShowLightColorOrange;
        Flags.ShowLightColorBlue = DefaultFlags.ShowLightColorBlue;
        Flags.ShowLightColorPink = DefaultFlags.ShowLightColorPink;

        // Apply the light color button "active" states.
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(
            Button.ShowLightColorRed), Flags.ShowLightColorRed);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(
            Button.ShowLightColorLime), Flags.ShowLightColorLime);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(
            Button.ShowLightColorPurple), Flags.ShowLightColorPurple);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(
            Button.ShowLightColorCyan), Flags.ShowLightColorCyan);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(
            Button.ShowLightColorGreen), Flags.ShowLightColorGreen);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(
            Button.ShowLightColorOrange), Flags.ShowLightColorOrange);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(
            Button.ShowLightColorBlue), Flags.ShowLightColorBlue);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(
            Button.ShowLightColorPink), Flags.ShowLightColorPink);

        // Default the light colors.
        Flags.LightColorRed = DefaultFlags.LightColorRed;
        Flags.LightColorLime = DefaultFlags.LightColorLime;
        Flags.LightColorPurple = DefaultFlags.LightColorPurple;
        Flags.LightColorCyan = DefaultFlags.LightColorCyan;
        Flags.LightColorGreen = DefaultFlags.LightColorGreen;
        Flags.LightColorOrange = DefaultFlags.LightColorOrange;
        Flags.LightColorBlue = DefaultFlags.LightColorBlue;
        Flags.LightColorPink = DefaultFlags.LightColorPink;

        // Apply the light colors.
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wdeprecated-declarations"

        GdkRGBA color;

        gdk_rgba_parse(&color, Flags.LightColorRed);
        gtk_widget_override_background_color(Button.LightColorRed,
            GTK_STATE_FLAG_NORMAL, &color);
        gdk_rgba_parse(&color, Flags.LightColorLime);
        gtk_widget_override_background_color(Button.LightColorLime,
            GTK_STATE_FLAG_NORMAL, &color);
        gdk_rgba_parse(&color, Flags.LightColorPurple);
        gtk_widget_override_background_color(Button.LightColorPurple,
            GTK_STATE_FLAG_NORMAL, &color);
        gdk_rgba_parse(&color, Flags.LightColorCyan);
        gtk_widget_override_background_color(Button.LightColorCyan,
            GTK_STATE_FLAG_NORMAL, &color);
        gdk_rgba_parse(&color, Flags.LightColorGreen);
        gtk_widget_override_background_color(Button.LightColorGreen,
            GTK_STATE_FLAG_NORMAL, &color);
        gdk_rgba_parse(&color, Flags.LightColorOrange);
        gtk_widget_override_background_color(Button.LightColorOrange,
            GTK_STATE_FLAG_NORMAL, &color);
        gdk_rgba_parse(&color, Flags.LightColorBlue);
        gtk_widget_override_background_color(Button.LightColorBlue,
            GTK_STATE_FLAG_NORMAL, &color);
        gdk_rgba_parse(&color, Flags.LightColorPink);
        gtk_widget_override_background_color(Button.LightColorPink,
            GTK_STATE_FLAG_NORMAL, &color);

        #pragma GCC diagnostic pop
    }

    if (tab == 3) {
        Flags.TreeColor = DefaultFlags.TreeColor;
    }

    if (tab == 5) {
        Flags.BirdsColor = DefaultFlags.BirdsColor;
    }

    switch (tab) {
        case plasmasnow_snow:
            Flags.VintageFlakes = 0;
            break;

        case plasmasnow_santa:
            Flags.SantaSize = DEFAULT(SantaSize);
            Flags.Rudolf = DEFAULT(Rudolf);
            break;

        case plasmasnow_scenery:
            free(Flags.TreeType);
            Flags.TreeType = strdup(DEFAULT(TreeType));
            break;

        case plasmasnow_settings:
            free(Flags.BackgroundFile);
            Flags.BackgroundFile = strdup(background);
            Flags.Screen = DEFAULT(Screen);
            break;
    }

    set_buttons();
    human_interaction = h;
    free(background);
}

/***********************************************************
 ** Helpers.
 **/

void init_buttons() {
    getAllButtonFormIDs();

    init_santa_buttons();

    initTreeButtons();

    init_general_buttons();
}

void set_buttons() {
    human_interaction = 0;

    initAllButtonValues();
    set_santa_buttons();
    set_tree_buttons();

    human_interaction = 1;
}

/***********************************************************
 ** Set the UI Main Window Sticky Flag.
 **/
void ui_set_sticky(int stickyFlag) {
    if (!ui_running) {
        return;
    }
    if (stickyFlag) {
        gtk_window_stick(GTK_WINDOW(mMainWindow));
    } else {
        gtk_window_unstick(GTK_WINDOW(mMainWindow));
    }
}

/***********************************************************
 ** https://docs.gtk.org/gtk3/iface.FileChooser.html
 **/

void handleFileChooserPreview(GtkFileChooser *file_chooser, gpointer data) {
    GtkWidget *preview;
    char *filename;
    GdkPixbuf *pixbuf;
    gboolean have_preview;

    preview = GTK_WIDGET(data);
    filename = gtk_file_chooser_get_preview_filename(file_chooser);
    if (!IsReadableFile(filename)) {
        g_free(filename);
        return;
    }

    int w = mGlobal.SnowWinWidth / 10;

    // pixbuf = gdk_pixbuf_new_from_file_at_size (filename, 128, 128, NULL);
    pixbuf = gdk_pixbuf_new_from_file_at_size(filename, w, w, NULL);
    have_preview = (pixbuf != NULL);
    g_free(filename);

    gtk_image_set_from_pixbuf(GTK_IMAGE(preview), pixbuf);
    if (pixbuf) {
        g_object_unref(pixbuf);
    }

    gtk_file_chooser_set_use_preview_label(file_chooser, FALSE);
    gtk_file_chooser_set_preview_widget_active(file_chooser, have_preview);
}

/***********************************************************
 ** Main UI Form control.
 **/
void createMainWindow() {
    ui_running = True;

    builder = gtk_builder_new_from_string(plasmasnow_xml, -1);
    #ifdef HAVE_GETTEXT
        gtk_builder_set_translation_domain(builder, TEXTDOMAIN);
    #endif
    gtk_builder_connect_signals(builder, NULL);

    range = GTK_WIDGET(gtk_builder_get_object(builder, "birds-range"));
    birdsgrid = GTK_CONTAINER(gtk_builder_get_object(builder, "grid_birds"));
    moonbox = GTK_CONTAINER(gtk_builder_get_object(builder, "moon-box"));

    // Main application window.
    mMainWindow = GTK_WIDGET(gtk_builder_get_object(builder, "id-MainWindow"));

    g_signal_connect(G_OBJECT(mMainWindow), "window-state-event",
        G_CALLBACK(handleMainWindowStateEvents), NULL);

    g_signal_connect(G_OBJECT(mMainWindow), "configure-event",
        G_CALLBACK(handleMainWindowStateEvents), NULL);
    g_signal_connect(G_OBJECT(mMainWindow), "focus-in-event",
        G_CALLBACK(handleMainWindowStateEvents), NULL);
    g_signal_connect(G_OBJECT(mMainWindow), "focus-out-event",
        G_CALLBACK(handleMainWindowStateEvents), NULL);
    g_signal_connect(G_OBJECT(mMainWindow), "map-event",
        G_CALLBACK(handleMainWindowStateEvents), NULL);
    g_signal_connect(G_OBJECT(mMainWindow), "unmap-event",
        G_CALLBACK(handleMainWindowStateEvents), NULL);
    g_signal_connect(G_OBJECT(mMainWindow), "property-notify-event",
        G_CALLBACK(handleMainWindowStateEvents), NULL);
    g_signal_connect(G_OBJECT(mMainWindow), "visibility-notify-event",
        G_CALLBACK(handleMainWindowStateEvents), NULL);

    mStyleContext = gtk_widget_get_style_context(mMainWindow);
    applyMainWindowCSSTheme();

    gtk_window_set_title(GTK_WINDOW(mMainWindow),
        mGlobal.mPlasmaWindowTitle);
    if (getenv("plasmasnow_RESTART")) {
        gtk_window_set_position(GTK_WINDOW(mMainWindow),
            GTK_WIN_POS_CENTER_ALWAYS);
    }

    // Gnome needs to be centered. KDE does
    // it for you.
    if (isThisAGnomeSession()) {
        const int CENTERED_X_POS = (mGlobal.SnowWinWidth -
            getMainWindowWidth()) / 2;
        const int CENTERED_Y_POS = (mGlobal.SnowWinHeight -
            getMainWindowHeight()) / 2;
        gtk_window_move(GTK_WINDOW(mMainWindow),
            CENTERED_X_POS, CENTERED_Y_POS);
    }

    gtk_widget_show_all(mMainWindow);

    init_buttons();
    connectAllButtonSignals();
    init_pixmaps();
    set_buttons();

    preview = GTK_IMAGE(gtk_image_new());
    gtk_file_chooser_set_preview_widget(
        GTK_FILE_CHOOSER(Button.BackgroundFile), GTK_WIDGET(preview));

    g_signal_connect(GTK_FILE_CHOOSER(Button.BackgroundFile), "update-preview",
        G_CALLBACK(handleFileChooserPreview), preview);

    XineramaScreenInfo* xininfo =
        XineramaQueryScreens(mGlobal.display, &Nscreens);
    HaveXinerama = (xininfo == NULL) ? 0 : 1;

    /**
     ** Monitors.
     **/
    GtkComboBoxText *ScreenButton;
    ScreenButton =
        GTK_COMBO_BOX_TEXT(gtk_builder_get_object(builder, "id-Screen"));

    if (Nscreens < 2) {
        gtk_widget_set_sensitive(GTK_WIDGET(ScreenButton), FALSE);
        Flags.Screen = -1;
    }
    if (Flags.Screen < -1) {
        Flags.Screen = -1;
    }
    if (Flags.Screen >= Nscreens) {
        Flags.Screen = Nscreens - 1;
    }

    gtk_combo_box_text_remove_all(ScreenButton);
    gtk_combo_box_text_append_text(ScreenButton, _("all monitors"));

    char *s = NULL;
    int p = 0;
    int i;
    for (i = 0; i < Nscreens; i++) {
        snprintf(sbuffer, nsbuffer, _("monitor %d"), i);
        s = (char *)realloc(s, p + 1 + strlen(sbuffer));
        strcpy(&s[p], sbuffer);
        gtk_combo_box_text_append_text(ScreenButton, &s[p]);
        p += 1 + strlen(sbuffer);
    }

    XFree(xininfo);

    gtk_combo_box_set_active(GTK_COMBO_BOX(ScreenButton), Flags.Screen + 1);
    g_signal_connect(ScreenButton, "changed", G_CALLBACK(combo_screen), NULL);

    /**
     ** Languages.
     **/
    GtkComboBoxText *LangButton;
    LangButton = GTK_COMBO_BOX_TEXT(gtk_builder_get_object(builder, "id-Lang"));

    strcpy(sbuffer, _("Available languages are: "));
    strcat(sbuffer, LANGUAGES);
    strcat(sbuffer, ".\n");
    strcat(sbuffer, _("Use \"sys\" for your default language.\n"));
    strcat(sbuffer, _("See also the man page."));
    gtk_widget_set_tooltip_text(GTK_WIDGET(LangButton), sbuffer);

    gtk_combo_box_text_remove_all(LangButton);
    gtk_combo_box_text_append_text(LangButton, "sys");
    lang[0] = strdup("sys");

    char *languages = strdup(LANGUAGES);
    char *token = strtok(languages, " ");
    nlang = 1;
    while (token != NULL) {
        lang[nlang] = strdup(token);
        gtk_combo_box_text_append_text(LangButton, lang[nlang]);
        nlang++;
        token = strtok(NULL, " ");
    }

    gtk_combo_box_set_active(GTK_COMBO_BOX(LangButton), 0);
    for (i = 0; i < nlang; i++) {
        if (!strcmp(lang[i], Flags.Language)) {
            gtk_combo_box_set_active(GTK_COMBO_BOX(LangButton), i);
            break;
        }
    }

    g_signal_connect(
        LangButton, "changed", G_CALLBACK(onSelectedLanguageButton), NULL);
    if (strlen(LANGUAGES) == 0) {
        gtk_widget_destroy(GTK_WIDGET(LangButton));
    }

    /**
     ** And lastly, hide us if starting minimized.
     **/
    if (Flags.HideMenu) {
        gtk_window_iconify(GTK_WINDOW(mMainWindow));
    }
}

/***********************************************************
 ** CSS related code for MainWindow styling.
 **/
static void applyCSSToWindow(
    GtkWidget *widget, GtkCssProvider *cssstyleProvider) {
    gtk_style_context_add_provider(gtk_widget_get_style_context(widget),
        GTK_STYLE_PROVIDER(cssstyleProvider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    // For container widgets, apply to every child widget on the container
    if (GTK_IS_CONTAINER(widget)) {
        gtk_container_forall(GTK_CONTAINER(widget),
            (GtkCallback)applyCSSToWindow, cssstyleProvider);
    }
}

void applyMainWindowCSSTheme() {
    const char* MAIN_WINDOW_CSS =
        "button.radio                { min-width:        10px;    }"
        "button.confirm              { background:       #FFFF00; }"
        "scale                       { padding:          1em;     }"

        ".mAppBusy stack             { background:       #FFC0CB; }"
        ".mAppBusy .cpuload slider   { background:       #FF0000; }"

        ".button                     { background:       #CCF0D8; }"

        ".plasmaColor   *                        { color:            #065522; }"
        ".plasmaColor   *                        { border-color:     #B4EEB4; }"

        ".plasmaColor   headerbar                { background:       #B3F4CA; }"
        ".plasmaColor   stack                    { background:       #EAFBF0; }"

        ".plasmaColor   *:disabled *             { color:            #8FB39B; }"

        ".plasmaColor   button.radio             { background:       #E2FDEC; }"
        ".plasmaColor   button.toggle            { background:       #E2FDEC; }"
        ".plasmaColor   button.confirm           { background-color: #FFFF00; }"
        ".plasmaColor   button:active            { background:       #0DAB44; }"
        ".plasmaColor   button:checked           { background:   springgreen; }"

        ".plasmaColor   radiobutton:active       { background:       #0DAB44; }"
        ".plasmaColor   radiobutton:checked      { background:       #6AF69B; }"

        ".plasmaColor   scale trough             { background:       #0DAB44; }"
        ".plasmaColor   scale trough highlight   { background:       #313ae4; }"

        ".plasmaNoColor *                        { color:            #065522; }"
        ".plasmaNoColor *                        { border-color:     #B4EEB4; }"
        ".plasmaNoColor *:disabled *             { color:            #8FB39B; }"

        ".plasmaNoColor button.radio             { background:       #efedeb; }"
        ".plasmaNoColor button.toggle            { background:       #f0efed; }"
        ".plasmaNoColor button:active            { background:       #c2bebb; }"
        ".plasmaNoColor button:checked           { background:       #d1cdca; }"
        ".plasmaNoColor button.confirm           { background-color: #FFFF00; }"
    ;

    static GtkCssProvider* cssProvider = NULL;
    if (!cssProvider) {
        cssProvider  = gtk_css_provider_new();
        gtk_css_provider_load_from_data(cssProvider, MAIN_WINDOW_CSS, -1, NULL);
        applyCSSToWindow(mMainWindow, cssProvider);
    }

    updateMainWindowTheme();
}

void updateMainWindowTheme() {
    if (!ui_running) {
        return;
    }
    if (Flags.mAppTheme) {
        gtk_style_context_add_class(mStyleContext, "plasmaColor");
        gtk_style_context_remove_class(mStyleContext, "plasmaNoColor");
    } else {
        gtk_style_context_remove_class(mStyleContext, "plasmaColor");
        gtk_style_context_add_class(mStyleContext, "plasmaNoColor");
    }
}

/***********************************************************
 ** "Busy" Style class getter / setters.
 **/
void addBusyStyleClass() {
    if (!ui_running) {
        return;
    }
    gtk_style_context_add_class(mStyleContext, "mAppBusy");
}

void removeBusyStyleClass() {
    if (!ui_running) {
        return;
    }
    gtk_style_context_remove_class(mStyleContext, "mAppBusy");
}

/***********************************************************
 ** Helpers for main window. Return height, width.
 **/
int getMainWindowWidth() {
    int mainWindowWidth, mainWindowHeight;
    gtk_window_get_size(GTK_WINDOW(mMainWindow),
            &mainWindowWidth, &mainWindowHeight);

    return mainWindowWidth;
}

int getMainWindowHeight() {
    int mainWindowWidth, mainWindowHeight;
    gtk_window_get_size(GTK_WINDOW(mMainWindow),
            &mainWindowWidth, &mainWindowHeight);

    return mainWindowHeight;
}

/***********************************************************
 ** Helpers for main window. Return X & Y Position.
 **/
int getMainWindowXPos() {
    int mainWindowXpos, mainWindowYPos;
    gtk_window_get_position(GTK_WINDOW(mMainWindow),
            &mainWindowXpos, &mainWindowYPos);

    return mainWindowXpos;
}

int getMainWindowYPos() {
    int mainWindowXpos, mainWindowYPos;
    gtk_window_get_position(GTK_WINDOW(mMainWindow),
            &mainWindowXpos, &mainWindowYPos);

    return mainWindowYPos;
}

/***********************************************************
 **
 **/
void birdscb(GtkWidget *w, void *m) {
    gtk_widget_set_sensitive(w, !(int *) m);
}

/***********************************************************
 **
 **/
void ui_gray_birds(int m) {
    if (!ui_running) {
        return;
    }
    gtk_container_foreach(birdsgrid, birdscb, &m);
    gtk_container_foreach(moonbox, birdscb, &m);
}

/***********************************************************
 ** Helpers.
 **/

/***********************************************************
 ** ... .
 **/
char *ui_gtk_version() {
    static char s[20];
    snprintf(s, 20, "%d.%d.%d", gtk_get_major_version(),
        gtk_get_minor_version(), gtk_get_micro_version());
    return s;
}

/***********************************************************
 ** ... .
 **/
char *ui_gtk_required() {
    static char s[20];
    snprintf(s, 20, "%d.%d.%d", GTK_MAJOR, GTK_MINOR, GTK_MICRO);
    return s;
}

// returns:
// 0: gtk version in use too low
// 1: gtk version in use OK

/***********************************************************
 ** ... .
 **/
int isGtkVersionValid() {
    if ((int) gtk_get_major_version() > GTK_MAJOR) {
        return 1;
    }
    if ((int) gtk_get_major_version() < GTK_MAJOR) {
        return 0;
    }
    if ((int) gtk_get_minor_version() > GTK_MINOR) {
        return 1;
    }
    if ((int) gtk_get_minor_version() < GTK_MINOR) {
        return 0;
    }
    if ((int) gtk_get_micro_version() >= GTK_MICRO) {
        return 1;
    }

    return 0;
}

/***********************************************************
 ** ... .
 **/
void setLabelText(GtkLabel *label, const gchar *str) {
    if (ui_running) {
        gtk_label_set_text(label, str);
    }
}

/***********************************************************
 ** ui.glade Form Helpers - All button actions.
 **/

// onClickedTreeButton.
MODULE_EXPORT
void onClickedTreeButton(GtkWidget *w) {
    if (!human_interaction) {
        return;
    }

    const gchar *s = gtk_widget_get_name(w) + strlen(PREFIX_TREE);
    int p = atoi(s);

    int *a;
    int n;
    csvpos(Flags.TreeType, &a, &n);

    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w))) {
        a = (int *) realloc(a, sizeof(*a) * (n + 1));
        REALLOC_CHECK(a);
        a[n] = p;
        n++;
    } else {
        for (int i = 0; i < n; i++) {
            if (a[i] == p) {
                a[i] = -1;
            }
        }
    }

    int *b = (int *)malloc(sizeof(*b) * n);

    int m = 0;
    for (int i = 0; i < n; i++) {
        int j;
        int unique = (a[i] >= 0);

        if (unique) {
            for (j = 0; j < m; j++) {
                if (a[i] == b[j]) {
                    unique = 0;
                    break;
                }
            }
        }

        if (unique) {
            b[m] = a[i];
            m++;
        }
    }

    free(Flags.TreeType);
    vsc(&Flags.TreeType, b, m);

    free(a);
    free(b);
}

MODULE_EXPORT
void onClickedQuitApplication() {
    Flags.shutdownRequested = 1;
}

MODULE_EXPORT
void onClickedActivateWind() {
    Flags.WindNow = 1;
}

MODULE_EXPORT
void onClickedActivateScreensaver() {
    int rc = system("xscreensaver-command -activate");
    (void)rc;
}

/***********************************************************
 ** ui.glade Form Helpers - All DEFAULT button actions.
 **/

MODULE_EXPORT
void onClickedSetSnowDefaults() {
    setTabDefaults(plasmasnow_snow);
}

MODULE_EXPORT
void onClickedSetSantaDefaults() {
    setTabDefaults(plasmasnow_santa);
}

MODULE_EXPORT
void onClickedSetSceneryDefaults() {
    setTabDefaults(plasmasnow_scenery);
}

MODULE_EXPORT
void onClickedSetBirdsDefaults() {
    setTabDefaults(plasmasnow_birds);
}

MODULE_EXPORT
void onClickedSetCelestialsDefaults() {
    setTabDefaults(plasmasnow_celestials);
}

MODULE_EXPORT
void onClickedSetAdvancedDefaults() {
    setTabDefaults(plasmasnow_settings);
}

MODULE_EXPORT
void onClickedSetAllDefaults() {
    setTabDefaults(plasmasnow_snow);
    setTabDefaults(plasmasnow_santa);
    setTabDefaults(plasmasnow_scenery);
    setTabDefaults(plasmasnow_birds);
    setTabDefaults(plasmasnow_celestials);
    setTabDefaults(plasmasnow_settings);
    // set_belowall_default();
}

