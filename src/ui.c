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
 *       - for few buttons: a css class. Example: BelowConfirm
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
 *     made to for example scenery_ui() which is supposed to handle flags
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

#include "Santa.h"
#include "birds.h"
#include "clocks.h"
#include "csvpos.h"
#include "flags.h"
#include "mygettext.h"
#include "pixmaps.h"
#include "safe_malloc.h"
#include "snow.h"
#include "ui.h"
#include "ui_xml.h"
#include "utils.h"
#include "version.h"
#include "windows.h"
#include "plasmasnow.h"
#include <X11/extensions/Xinerama.h>
#include <assert.h>
#include <glib.h>
#include <glib/gprintf.h>
#include <gtk/gtk.h>
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


/***********************************************************
 * Module Method stubs.
 */
static void initAllButtonValues(void);
static void set_santa_buttons(void);
static void set_tree_buttons(void);

static void applyMainWindowCSSTheme();
static void updateMainWindowTheme(void);

static void birdscb(GtkWidget *w, void *m);
static void yesyes(GtkWidget *w, gpointer data);
static void nono(GtkWidget *w, gpointer data);

static void onClickedActivateXScreenSaver(GtkApplication *app);

static void setTabDefaults(int tab);

static void handle_screen(void);
static void handleFileChooserPreview(
    GtkFileChooser *file_chooser, gpointer data);
static void setLabelText(GtkLabel *label, const gchar *str);

// to be used if gtk version is too low
// returns 1: user clicked 'Run with ...'
// returns 0: user clicked 'Quit'
static int RC;

extern Window getDragWindowOf(Window);
//extern int getTmpLogFile();
extern void getWinInfoList();
extern void logAllWindowsStackedTopToBottom();

extern void doRaiseWindow(char* argString);
extern void doLowerWindow(char* argString);

static gboolean handleMainWindowStateEvents(
    GtkWidget *widget, GdkEventWindowState *event, gpointer user_data);

bool startQPickerDialog(char* callerTag, char* colorAsString);
int getQPickerRed();
int getQPickerGreen();
int getQPickerBlue();


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

#define SANTA_ALL SANTA2(0) SANTA2(1) SANTA2(2) SANTA2(3) SANTA2(4)


#define PREFIX_TREE "tree-"
#define TREE_ALL TREE(0) TREE(1) TREE(2) TREE(3) TREE(4) TREE(5) TREE(6) TREE(7) TREE(8) TREE(9)


#define DEFAULT(name) DefaultFlags.name
#define VINTAGE(name) VintageFlags.name

static GtkBuilder *builder;

static GtkWidget *range;

static GtkContainer *birdsgrid;
static GtkContainer *moonbox;
static GtkImage *preview;
static int Nscreens;
static int HaveXinerama;

#define nsbuffer 1024
static char sbuffer[nsbuffer];

static int ui_running = False;
static int human_interaction = 1;


static GtkWidget *mMainWindow;
static GtkStyleContext *mStyleContext;

static char *lang[100];
static int nlang;


/** *********************************************************************
 ** UI Main Methods.
 **/
void updateMainWindowUI() {
    UIDO(mAppTheme, updateMainWindowTheme(););
    UIDO(Screen, handle_screen(););
    UIDO(Outline, ClearScreen(););
    UIDOS(Language, handle_language(1););
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

void handle_screen() {
    P("handle_screen:%d\n", Flags.Screen);

    if (HaveXinerama && Nscreens > 1) {
        mGlobal.ForceRestart = 1;
    }
}

void handle_language(int restart) {
    P("handle_language: %s\n", Flags.Language);

    if (!strcmp(Flags.Language, "sys")) {
        unsetenv("LANGUAGE");
    } else {
        setenv("LANGUAGE", Flags.Language, 1);
    }

    if (restart) {
        mGlobal.ForceRestart = 1;
    }
}

/** *********************************************************************
 ** Main WindowState event handler.
 **/
gboolean handleMainWindowStateEvents(
    __attribute__((unused)) GtkWidget* widget,
    GdkEventWindowState* event,
    __attribute__((unused)) gpointer user_data) {
    {   char resultMsg[128];
        snprintf(resultMsg, sizeof(resultMsg),
            "ui: handleMainWindowStateEvents() event->type : %i Starts.\n",
            event->type);
        fprintf(stdout, "%s", resultMsg);
    }

    if (event->type == GDK_WINDOW_STATE) {
        if (event->new_window_state & GDK_WINDOW_STATE_ICONIFIED) {
            // const char* PLASMA_DESKTOP = "plasma";
            //if (strncmp(mGlobal.DesktopSession,
            //        PLASMA_DESKTOP, strlen(PLASMA_DESKTOP)) == 0) {
            //    doLowerWindow(mGlobal.mPlasmaLayerName);
            //    return true;
            //}
        }

        //if (event->new_window_state & GDK_WINDOW_STATE_FOCUSED) {
        //    doRaiseWindow("plasmasnow");
        //    doLowerWindow(mGlobal.mPlasmaLayerName);
        //}
    }

    // getWinInfoList();

    // Else all done.
    {   char resultMsg[128];
        snprintf(resultMsg, sizeof(resultMsg),
            "ui: handleMainWindowStateEvents() event->type : %i Finishes.\n\n",
            event->type);
        fprintf(stdout, "%s", resultMsg);
    }

    return false;
}

/** *********************************************************************
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

static void set_santa_buttons() {
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

/** *********************************************************************
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

/** *********************************************************************
 ** Create all button stubs.
 **/

#define togglecode(type, name, m) NEWLINE GtkWidget *name;
#define scalecode togglecode
#define colorcode togglecode
#define filecode togglecode

static struct _button {

        ALL_BUTTONS

        // QColorDialog "Widgets".
        GtkWidget *SnowColor;
        GtkWidget *SnowColor2;
        GtkWidget *BirdsColor;
        GtkWidget *TreeColor;

} Button;

#include "undefall.inc"

/** *********************************************************************
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
    Button.SnowColor =
        (GtkWidget *) gtk_builder_get_object(builder, "id-SnowColor");
    Button.SnowColor2 =
        (GtkWidget *) gtk_builder_get_object(builder, "id-SnowColor2");
    Button.BirdsColor =
        (GtkWidget *) gtk_builder_get_object(builder, "id-BirdsColor");
    Button.TreeColor =
        (GtkWidget *) gtk_builder_get_object(builder, "id-TreeColor");
}

#include "undefall.inc"

/** *********************************************************************
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

// QColorDialog "Widgets".
void onClickedSnowColor() {
    if (!human_interaction) {
        return;
    }
    startQPickerDialog("SnowColorTAG", Flags.SnowColor);
}

void onClickedSnowColor2() {
    if (!human_interaction) {
        return;
    }
    startQPickerDialog("SnowColor2TAG", Flags.SnowColor2);
}

void onClickedBirdsColor() {
    if (!human_interaction) {
        return;
    }
    startQPickerDialog("BirdsColorTAG", Flags.BirdsColor);
}

void onClickedTreeColor() {
    if (!human_interaction) {
        return;
    }
    startQPickerDialog("TreeColorTAG", Flags.TreeColor);
}

#pragma GCC diagnostic pop
#include "undefall.inc"

/** *********************************************************************
 ** Init all buttons values.
 **/

#define togglecode(type, name, m)                                              \
    NEWLINE P("toggle %s %s %d %d\n", #name, #type, m, Flags.name);            \
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
    NEWLINE P("range %s %s %d %d\n", #name, #type, m, Flags.name);             \
    NEWLINE gtk_range_set_value(                                               \
        GTK_RANGE(Button.name), m *((gdouble)Flags.name));

#define colorcode(type, name, m)                                               \
    NEWLINE P("color %s %s %d %s\n", #name, #type m, Flags.name);              \
    NEWLINE                                                                    \
    NEWLINE gdk_rgba_parse(&color, Flags.name);                                \
    NEWLINE gtk_color_chooser_set_rgba(                                        \
        GTK_COLOR_CHOOSER(Button.name), &color);                               \
    NEWLINE

#define filecode(type, name, m)                                                \
    NEWLINE P("file %s %s %d %s\n", #name, #type, m, Flags.name);              \
    NEWLINE gtk_file_chooser_set_filename(                                     \
        GTK_FILE_CHOOSER(Button.name), Flags.BackgroundFile);

static void initAllButtonValues() {
    GdkRGBA color;

    ALL_BUTTONS

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

    // QColorDialog Widgets.
    gdk_rgba_parse(&color, Flags.SnowColor);
    gtk_widget_override_background_color(
        Button.SnowColor, GTK_STATE_FLAG_NORMAL, &color);

    gdk_rgba_parse(&color, Flags.SnowColor2);
    gtk_widget_override_background_color(
        Button.SnowColor2, GTK_STATE_FLAG_NORMAL, &color);

    gdk_rgba_parse(&color, Flags.BirdsColor);
    gtk_widget_override_background_color(
        Button.BirdsColor, GTK_STATE_FLAG_NORMAL, &color);

    gdk_rgba_parse(&color, Flags.TreeColor);
    gtk_widget_override_background_color(
        Button.TreeColor, GTK_STATE_FLAG_NORMAL, &color);

#pragma GCC diagnostic pop
}

#include "undefall.inc"

/** *********************************************************************
 ** Hook all buttons to their action methods.
 **/

#define togglecode(type, name, m)                                              \
    NEWLINE P("%s %s\n", #name, #type);                                        \
    NEWLINE g_signal_connect(G_OBJECT(Button.name), "toggled",                 \
        G_CALLBACK(buttoncb(type, name)), NULL);

#define scalecode(type, name, m)                                               \
    NEWLINE P("%s %s\n", #name, #type);                                        \
    NEWLINE g_signal_connect(G_OBJECT(Button.name), "value-changed",           \
        G_CALLBACK(buttoncb(type, name)), NULL);

#define colorcode(type, name, m)                                               \
    NEWLINE P("%s %s\n", #name, #type);                                        \
    NEWLINE g_signal_connect(G_OBJECT(Button.name), "color-set",               \
        G_CALLBACK(buttoncb(type, name)), NULL);

#define filecode(type, name, m)                                                \
    NEWLINE P("%s %s\n", #name, #type);                                        \
    NEWLINE g_signal_connect(G_OBJECT(Button.name), "file-set",                \
        G_CALLBACK(buttoncb(type, name)), NULL);

static void connectAllButtonSignals() {

    ALL_BUTTONS

    // QColorDialog "Widgets".
    g_signal_connect(G_OBJECT(Button.SnowColor), "toggled",
        G_CALLBACK(onClickedSnowColor), NULL);
    g_signal_connect(G_OBJECT(Button.SnowColor2), "toggled",
        G_CALLBACK(onClickedSnowColor2), NULL);
    g_signal_connect(G_OBJECT(Button.BirdsColor), "toggled",
        G_CALLBACK(onClickedBirdsColor), NULL);
    g_signal_connect(G_OBJECT(Button.TreeColor), "toggled",
        G_CALLBACK(onClickedTreeColor), NULL);
}

#include "undefall.inc"

/** *********************************************************************
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

/** *********************************************************************
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

/** *********************************************************************
 ** Helpers.
 **/

static void set_tree_buttons() {

#define TREE(x)                                                                \
    NEWLINE gtk_toggle_button_set_active(                                      \
        GTK_TOGGLE_BUTTON(tree_buttons.tree_##x.button), FALSE);

    TREE_ALL;

#include "undefall.inc"
    int *a, n;
    csvpos(Flags.TreeType, &a, &n);

    for (int i = 0; i < n; i++) {
        switch (a[i]) {

#define TREE(x)                                                                \
    NEWLINE case x:                                                            \
    gtk_toggle_button_set_active(                                              \
        GTK_TOGGLE_BUTTON(tree_buttons.tree_##x.button), TRUE);                \
    NEWLINE break;

            TREE_ALL;

#include "undefall.inc"
        }
    }

    free(a);
}

typedef struct _general_button {
        GtkWidget *button;
} general_button;


/** *********************************************************************
 ** Helpers.
 **/

MODULE_EXPORT
void combo_screen(GtkComboBoxText *combo, gpointer data) {
    (void)data;
    int num = gtk_combo_box_get_active(GTK_COMBO_BOX(combo));
    P("combo_screen:%d\n", num);
    Flags.Screen = num - 1;
}

MODULE_EXPORT
void onSelectedLanguageButton(GtkComboBoxText *combo, gpointer data) {
    (void)data;
    int num = gtk_combo_box_get_active(GTK_COMBO_BOX(combo));
    P("onSelectedLanguageButton:%d\n", num);
    Flags.Language = strdup(lang[num]);
}

/** *********************************************************************
 ** Helpers.
 **/
static void init_general_buttons() {
    setLabelText(GTK_LABEL(gtk_builder_get_object(builder, "id-version")),
        "plasmasnow-" VERSION);
}

/** *********************************************************************
 ** Helpers.
 **/
typedef struct _snow_button {
        GtkWidget *button;
} snow_button;

/** *********************************************************************
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

/** *********************************************************************
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
        Flags.SnowColor = DefaultFlags.SnowColor;
    }
    if (tab == 1) {
        Flags.SnowColor2 = DefaultFlags.SnowColor2;
    }
    if (tab == 5) {
        Flags.BirdsColor = DefaultFlags.BirdsColor;
    }
    if (tab == 3) {
        Flags.TreeColor = DefaultFlags.TreeColor;
    }

    switch (tab) {
        case plasmasnow_scenery:
            free(Flags.TreeType);
            Flags.TreeType = strdup(DEFAULT(TreeType));
            break;

        case plasmasnow_snow:
            Flags.VintageFlakes = 0;
            break;

        case plasmasnow_santa:
            Flags.SantaSize = DEFAULT(SantaSize);
            Flags.Rudolf = DEFAULT(Rudolf);
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

/** *********************************************************************
 ** Helpers.
 **/

static void init_buttons() {
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

/** *********************************************************************
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

/** *********************************************************************
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

/** *********************************************************************
 ** Main UI Form control.
 **/

void initUIClass() {
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

    gtk_window_set_title(GTK_WINDOW(mMainWindow), mGlobal.mPlasmaLayerName);
    //gtk_window_set_title(GTK_WINDOW(mMainWindow), "");
    if (getenv("plasmasnow_RESTART")) {
        gtk_window_set_position(GTK_WINDOW(mMainWindow),
            GTK_WIN_POS_CENTER_ALWAYS);
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

    XineramaScreenInfo *xininfo =
        XineramaQueryScreens(mGlobal.display, &Nscreens);
    if (xininfo == NULL) {
        P("No xinerama...\n");
        HaveXinerama = 0;
    } else {
        HaveXinerama = 1;
    }

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
        P("token: %s\n", token);
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

/** *********************************************************************
 ** CSS related code for MainWindow styling.
 **/

// Set the style provider for the widgets
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

/** *********************************************************************
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

/** *********************************************************************
 **
 **/
void birdscb(GtkWidget *w, void *m) {
    gtk_widget_set_sensitive(w, !(int *) m);
}

/** *********************************************************************
 **
 **/
void ui_gray_birds(int m) {
    if (!ui_running) {
        return;
    }
    gtk_container_foreach(birdsgrid, birdscb, &m);
    gtk_container_foreach(moonbox, birdscb, &m);
}

/** *********************************************************************
 ** Helpers.
 **/

/** *********************************************************************
 ** ... .
 **/
char *ui_gtk_version() {
    static char s[20];
    snprintf(s, 20, "%d.%d.%d", gtk_get_major_version(),
        gtk_get_minor_version(), gtk_get_micro_version());
    return s;
}

/** *********************************************************************
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

/** *********************************************************************
 ** ... .
 **/
int ui_checkgtk() {
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

/** *********************************************************************
 ** ... .
 **/
int ui_run_nomenu() {
    GtkApplication *app;

#define MY_GLIB_VERSION                                                        \
    (100000000 * GLIB_MAJOR_VERSION + 10000 * GLIB_MINOR_VERSION +             \
        GLIB_MICRO_VERSION)

#if MY_GLIB_VERSION >= 200730003
#define XXFLAGS G_APPLICATION_DEFAULT_FLAGS
#else
#define XXFLAGS G_APPLICATION_FLAGS_NONE
#endif

    app = gtk_application_new("plasmasnowApp", XXFLAGS);
    g_signal_connect(app, "activate",
        G_CALLBACK(onClickedActivateXScreenSaver), NULL);

    g_application_run(G_APPLICATION(app), 0, NULL);

    g_object_unref(app);
    return RC;
}

/** *********************************************************************
 ** ... .
 **/
static void onClickedActivateXScreenSaver(GtkApplication *app) {
    GtkWidget *window;
    GtkWidget *grid;
    GtkWidget *button;
    GtkWidget *label;

    /* create a new window, and set its title */
    window = gtk_application_window_new(app);

    gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
    gtk_window_set_title(GTK_WINDOW(window), "plasmaSnow");
    gtk_window_set_decorated(GTK_WINDOW(window), FALSE);
    gtk_window_set_keep_above(GTK_WINDOW(window), TRUE);
    gtk_container_set_border_width(GTK_CONTAINER(window), 10);

    /* Here we construct the container that is going pack our buttons */
    grid = gtk_grid_new();

    /* Pack the container in the window */
    gtk_container_add(GTK_CONTAINER(window), grid);

    snprintf(sbuffer, nsbuffer,
        "You are using GTK-%s, but you need at least GTK-%s to view\n"
        "the user interface.\n"
        "Use the option '-nomenu' to disable the user interface.\n"
        "If you want to try the user interface anyway, use the flag '-checkgtk "
        "0'.\n\n"
        "See 'man plasmasnow' or 'plasmasnow -h' to see the command line options.\n"
        "Alternatively, you could edit ~/.plasmasnowrc to set options.\n",
        ui_gtk_version(), ui_gtk_required());
    label = gtk_label_new(sbuffer);

    /* Place the label in cell (0,0) and make it fill 2 cells horizontally */
    gtk_grid_attach(GTK_GRID(grid), label, 0, 0, 2, 1);

    button = gtk_button_new_with_label("Run without user interface");
    g_signal_connect(button, "clicked", G_CALLBACK(yesyes), window);

    /* Place the first button in the grid cell (0, 1), and make it fill
     * just 1 cell horizontally and vertically (ie no spanning)
     */
    gtk_grid_attach(GTK_GRID(grid), button, 0, 1, 1, 1);

    button = gtk_button_new_with_label("Quit");
    g_signal_connect(button, "clicked", G_CALLBACK(nono), window);

    /* Place the second button in the grid cell (1, 1), and make it fill
     * just 1 cell horizontally and vertically (ie no spanning)
     */
    gtk_grid_attach(GTK_GRID(grid), button, 1, 1, 1, 1);

    /* Now that we are done packing our widgets, we show them all
     * in one go, by calling gtk_widget_show_all() on the window.
     * This call recursively calls gtk_widget_show() on all widgets
     * that are contained in the window, directly or indirectly.
     */
    gtk_widget_show_all(window);
}

/** *********************************************************************
 ** ... .
 **/
void setLabelText(GtkLabel *label, const gchar *str) {
    if (ui_running) {
        gtk_label_set_text(label, str);
    }
}

/** *********************************************************************
 ** ... .
 **/
void yesyes(GtkWidget *w, gpointer window) {
    RC = (w != NULL);
    gtk_widget_destroy(GTK_WIDGET(window));
}

/** *********************************************************************
 ** ... .
 **/
void nono(GtkWidget *w, gpointer window) {
    RC = (w == NULL);
    gtk_widget_destroy(GTK_WIDGET(window));
}

/** *********************************************************************
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

    P("Tree_Type set to %s\n", Flags.TreeType);
}

MODULE_EXPORT
void onClickedQuitApplication() {
    Flags.Done = 1;
}

MODULE_EXPORT
void onClickedActivateWind() {
    P("onClickedActivateWind\n");
    Flags.WindNow = 1;
}

MODULE_EXPORT
void onClickedActivateScreensaver() {
    int rc = system("xscreensaver-command -activate");
    (void)rc;
}

/** *********************************************************************
 ** ui.glade Form Helpers - All DEFAULT button actions.
 **/

MODULE_EXPORT
void onClickedSetSnowDefaults() { setTabDefaults(plasmasnow_snow); }

MODULE_EXPORT
void onClickedSetSantaDefaults() { setTabDefaults(plasmasnow_santa); }

MODULE_EXPORT
void onClickedSetSceneryDefaults() { setTabDefaults(plasmasnow_scenery); }

MODULE_EXPORT
void onClickedSetCelestialsDefaults() { setTabDefaults(plasmasnow_celestials); }

MODULE_EXPORT
void onClickedSetBirdsDefaults() {
    P("onClickedDefaultBirds\n");
    setTabDefaults(plasmasnow_birds);
}

MODULE_EXPORT
void onClickedSetAdvancedDefaults() { setTabDefaults(plasmasnow_settings); }

MODULE_EXPORT
void onClickedSetAllDefaults() {
    setTabDefaults(plasmasnow_settings);

    setTabDefaults(plasmasnow_snow);
    setTabDefaults(plasmasnow_santa);
    setTabDefaults(plasmasnow_scenery);
    setTabDefaults(plasmasnow_celestials);
    setTabDefaults(plasmasnow_birds);
    // set_belowall_default();
}
