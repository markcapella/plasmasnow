/* xdo search implementation
 *
 * Lets you search windows by a query
 */

#include <regex.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include <X11/extensions/XTest.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xresource.h>
#include <X11/Xutil.h>

#include "xdo.h"
#include "xdo_search.h"


/** *********************************************************************
 ** Module globals and consts.
 **/

/* Set this to 1 for dev debugging */
const bool mDebugSearchXDO = false;


/** *********************************************************************
 ** This method ...
 **/
int _xdo_is_window_visible(const xdo_t* xdo, Window wid) {
    XWindowAttributes wattr;
    XGetWindowAttributes(xdo->xdpy, wid, &wattr);
    if (wattr.map_state != IsViewable) {
        return False;
    }

    return True;
}

/** *********************************************************************
 ** This method ...
 **/
int xdo_search_windows(const xdo_t* xdo, const xdo_search_t* search,
    Window **windowlist_ret, unsigned int *nwindows_ret) {
    int i = 0;

    unsigned int windowlist_size = 100;

    *nwindows_ret = 0;
    *windowlist_ret = (Window*) calloc(sizeof(Window),
        windowlist_size);

    if (search->searchmask & SEARCH_SCREEN) {
        Window root = RootWindow(xdo->xdpy, search->screen);

        if (check_window_match(xdo, root, search)) {
            (*windowlist_ret)[*nwindows_ret] = root;
            (*nwindows_ret)++;
        }

        find_matching_windows(xdo, root, search, windowlist_ret,
            nwindows_ret, &windowlist_size, 1);
        return XDO_SUCCESS;
    }

    const int screencount = ScreenCount(xdo->xdpy);
    for (int i = 0; i < screencount; i++) {
        Window root = RootWindow(xdo->xdpy, i);
        if (check_window_match(xdo, root, search)) {
            (*windowlist_ret)[*nwindows_ret] = root;
            (*nwindows_ret)++;
        }

        find_matching_windows(xdo, root, search, windowlist_ret,
            nwindows_ret, &windowlist_size, 1);
    }

    return XDO_SUCCESS;
}

/** *********************************************************************
 ** This method ...
 **/
void find_matching_windows(const xdo_t* xdo, Window window,
    const xdo_search_t *search, Window **windowlist_ret,
    unsigned int *nwindows_ret, unsigned int *windowlist_size,
    int current_depth) {

    Window dummy;
    Window *children;
    unsigned nchildren;

    /* Break early, if we have enough windows already. */
    if (search->limit > 0 && *nwindows_ret >= search->limit) {
        return;
    }

    /* Break if too deep */
    if (search->max_depth != -1 && current_depth > search->max_depth) {
        return;
    }

    Status success = XQueryTree(xdo->xdpy, window,
        &dummy, &dummy, &children, &nchildren);
    if (!success) {
        XFree(children);
        return;
    }

    /* Breadth first, check all children for matches */
    for (int i = 0; i < nchildren; i++) {
        Window child = children[i];
        if (!check_window_match(xdo, child, search)) {
            continue;
        }

        (*windowlist_ret)[*nwindows_ret] = child;
        (*nwindows_ret)++;
        if (search->limit > 0 && *nwindows_ret >= search->limit) {
            break;
        }

        if (*windowlist_size == *nwindows_ret) {
            *windowlist_size *= 2;
            *windowlist_ret = (Window*) realloc(*windowlist_ret,
                *windowlist_size * sizeof(Window));
        }
    }

    /* Now check children-children */
    if (search->max_depth == -1 || (current_depth + 1) <= search->max_depth) {
        for (int i = 0; i < nchildren; i++) {
            find_matching_windows(xdo, children[i], search,
                windowlist_ret, nwindows_ret, windowlist_size,
                current_depth + 1);
        }
    }

    XFree(children);
}

/** *********************************************************************
 ** This method ...
 **/
int check_window_match(const xdo_t* xdo, Window wid,
    const xdo_search_t* search) {
    regex_t title_re;
    regex_t class_re;
    regex_t classname_re;
    regex_t name_re;

    if (!compile_re(search->title, &title_re) ||
        !compile_re(search->winclass, &class_re) ||
        !compile_re(search->winclassname, &classname_re) ||
        !compile_re(search->winname, &name_re)) {
        regfree(&title_re);
        regfree(&class_re);
        regfree(&classname_re);
        regfree(&name_re);
        printf("plasmasnow: check_window_match() Fails.\n");
        return False;
    }

    int visible_ok, pid_ok, title_ok, name_ok, class_ok, classname_ok,
        desktop_ok;
    int visible_want, pid_want, title_want, name_want, class_want,
        classname_want, desktop_want;

    visible_ok = pid_ok = title_ok = name_ok = class_ok = classname_ok =
        desktop_ok = True;
    //(search->require == SEARCH_ANY ? False : True);

    desktop_want = search->searchmask & SEARCH_DESKTOP;
    visible_want = search->searchmask & SEARCH_ONLYVISIBLE;
    pid_want = search->searchmask & SEARCH_PID;
    title_want = search->searchmask & SEARCH_TITLE;
    name_want = search->searchmask & SEARCH_NAME;
    class_want = search->searchmask & SEARCH_CLASS;
    classname_want = search->searchmask & SEARCH_CLASSNAME;

    do {
        if (desktop_want) {
            long desktop = -1;

            /* We're modifying xdo here, but since we restore it, we're still
             * obeying the "const" contract. */
            int old_quiet = xdo->quiet;
            xdo_t* xdo2 = (xdo_t *)xdo;
            xdo2->quiet = 1;
            int ret = xdo_get_desktop_for_window(xdo2, wid, &desktop);
            xdo2->quiet = old_quiet;

            /* Desktop matched if we support desktop queries *and* the desktop
             * is equal */
            desktop_ok = (ret == XDO_SUCCESS && desktop == search->desktop);
        }

        /* Visibility is a hard condition, fail always if we wanted
         * only visible windows and this one isn't */
        if (visible_want && !_xdo_is_window_visible(xdo, wid)) {
            if (mDebugSearchXDO) {
                fprintf(stderr, "skip %ld visible\n", wid);
            }
            visible_ok = False;
            break;
        }

        if (pid_want && !_xdo_match_window_pid(xdo, wid, search->pid)) {
            if (mDebugSearchXDO) {
                fprintf(stderr, "skip %ld pid\n", wid);
            }
            pid_ok = False;
        }

        if (title_want && !_xdo_match_window_title(xdo, wid, &title_re)) {
            if (mDebugSearchXDO) {
                fprintf(stderr, "skip %ld title\n", wid);
            }
            title_ok = False;
        }

        if (name_want && !_xdo_match_window_name(xdo, wid, &name_re)) {
            if (mDebugSearchXDO) {
                fprintf(stderr, "skip %ld winname\n", wid);
            }
            name_ok = False;
        }

        if (class_want && !_xdo_match_window_class(xdo, wid, &class_re)) {
            if (mDebugSearchXDO) {
                fprintf(stderr, "skip %ld winclass\n", wid);
            }
            class_ok = False;
        }

        if (classname_want &&
            !_xdo_match_window_classname(xdo, wid, &classname_re)) {
            if (mDebugSearchXDO) {
                fprintf(stderr, "skip %ld winclassname\n", wid);
            }
            classname_ok = False;
        }
    } while (0);

    regfree(&title_re);
    regfree(&class_re);
    regfree(&classname_re);
    regfree(&name_re);

    if (mDebugSearchXDO) {
        fprintf(stderr,
            "win: %ld, pid:%d, title:%d, name:%d, class:%d, visible:%d\n", wid,
            pid_ok, title_ok, name_ok, class_ok, visible_ok);
    }

    switch (search->require) {
    case SEARCH_ALL:
        return visible_ok && pid_ok && title_ok && name_ok && class_ok &&
               classname_ok && desktop_ok;
        break;
    case SEARCH_ANY:
        return visible_ok &&
               ((pid_want && pid_ok) || (title_want && title_ok) ||
                   (name_want && name_ok) || (class_want && class_ok) ||
                   (classname_want && classname_ok)) &&
               desktop_ok;
        break;
    }

    fprintf(stderr,
        "Unexpected code reached. search->require is not valid? (%d); "
        "this may be a bug?\n",
        search->require);
    return False;
}

/** *********************************************************************
 ** This method ...
 **/
int _xdo_match_window_title(const xdo_t* xdo, Window window,
    regex_t *re) {
    return _xdo_match_window_name(xdo, window, re);
}

/** *********************************************************************
 ** This method ...
 **/
/* historically in xdo, 'match_name' matched the classhint 'name' which we
 * match in _xdo_match_window_classname. But really, most of the time 'name'
 * refers to the window manager name for the window, which is displayed in
 * the titlebar */
int _xdo_match_window_name(const xdo_t* xdo,
    Window window, regex_t *re) {

    int count = 0;
    char **list = NULL;
    XTextProperty tp;

    XGetWMName(xdo->xdpy, window, &tp);
    if (tp.nitems > 0) {
        Xutf8TextPropertyToTextList(xdo->xdpy, &tp, &list, &count);
        for (int i = 0; i < count; i++) {
            if (regexec(re, list[i], 0, NULL, 0) == 0) {
                XFreeStringList(list);
                XFree(tp.value);
                return True;
            }
        }
    } else {
        /* Treat windows with no names as empty strings */
        if (regexec(re, "", 0, NULL, 0) == 0) {
            XFreeStringList(list);
            XFree(tp.value);
            return True;
        }
    }

    XFreeStringList(list);
    XFree(tp.value);

    return False;
}

/** *********************************************************************
 ** This method ...
 **/
int _xdo_match_window_class(
    const xdo_t* xdo, Window window, regex_t *re) {
    XWindowAttributes attr;
    XClassHint classhint;
    XGetWindowAttributes(xdo->xdpy, window, &attr);

    if (XGetClassHint(xdo->xdpy, window, &classhint)) {
        // printf("%d: class %s\n", window, classhint.res_class);
        if ((classhint.res_class) &&
            (regexec(re, classhint.res_class, 0, NULL, 0) == 0)) {
            XFree(classhint.res_name);
            XFree(classhint.res_class);
            return True;
        }

        XFree(classhint.res_name);
        XFree(classhint.res_class);

    } else {
        /* Treat windows with no class as empty strings */
        if (regexec(re, "", 0, NULL, 0) == 0) {
            return True;
        }
    }
    return False;
}

/** *********************************************************************
 ** This method ...
 **/
int _xdo_match_window_classname(
    const xdo_t* xdo, Window window, regex_t *re) {
    XWindowAttributes attr;
    XClassHint classhint;
    XGetWindowAttributes(xdo->xdpy, window, &attr);

    if (XGetClassHint(xdo->xdpy, window, &classhint)) {
        if ((classhint.res_name) &&
            (regexec(re, classhint.res_name, 0, NULL, 0) == 0)) {
            XFree(classhint.res_name);
            XFree(classhint.res_class);
            return True;
        }

        XFree(classhint.res_name);
        XFree(classhint.res_class);

    } else {
        /* Treat windows with no class name as empty strings */
        if (regexec(re, "", 0, NULL, 0) == 0) {
            return True;
        }
    }
    return False;
}

/** *********************************************************************
 ** This method ...
 **/
int _xdo_match_window_pid(
    const xdo_t* xdo, Window window, const int pid) {
    int window_pid;

    window_pid = xdo_get_pid_window(xdo, window);
    if (pid == window_pid) {
        return True;
    } else {
        return False;
    }
}

/** *********************************************************************
 ** This method ...
 **/
int compile_re(const char* inPattern, regex_t* outRegex) {
    if (inPattern == NULL) {
        regcomp(outRegex, "^$", REG_EXTENDED | REG_ICASE);
        return true;
    }

    const int COMPILE_RESULT = regcomp(outRegex,
        inPattern, REG_EXTENDED | REG_ICASE);
    if (!COMPILE_RESULT) {
        printf("plasmasnow: Failed to compile regex (result "
            "%d): '%s'\n", COMPILE_RESULT, inPattern);
        return false;
    }

    return true;
}
