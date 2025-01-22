#include "ColorCodes.h"
#include "WindowVector.h"


/** *********************************************************************
 ** WindowVector lifecycle methods.
 **
 ** Forked & extended from:
 **     https://www.sanfoundry.com/c-program-implement-vector/
 **/
void windowVectorInit(WindowVector* v) {
    v->mCapacity = WINDOWVECTOR_INIT_CAPACITY;
    v->mItemSize = 0;
    v->mWindowList = malloc(sizeof(Window) * v->mCapacity);
}

int windowVectorSize(WindowVector* v) {
    return v->mItemSize;
}

void windowVectorResize(WindowVector* v, int newCapacity) {
    Window* newWindowList = realloc(v->mWindowList,
        sizeof(Window) * newCapacity);

    if (newWindowList) {
        v->mWindowList = newWindowList;
        v->mCapacity = newCapacity;
    }
}

void windowVectorFree(WindowVector* v) {
    free(v->mWindowList);
}

/** *********************************************************************
 ** WindowVector Getters / Setters / Helper methods.
 **/
bool windowVectorExists(WindowVector* v, Window window) {
    for (int i = 0; i < v->mItemSize; i++) {
        if (v->mWindowList[i] == window) {
            return true;
        }
    }
    return false;
}

bool windowVectorAdd(WindowVector* v, Window window) {
    if (windowVectorExists(v, window)) {
        return false;
    }

    if (v->mCapacity == v->mItemSize) {
        windowVectorResize(v, v->mCapacity * 2);
    }

    v->mWindowList[v->mItemSize++] = window;
    return true;
}

void windowVectorDelete(WindowVector* v, int index) {
    if (index < 0 || index >= v->mItemSize) {
        return;
    }

    // Remove item and terminate.
    v->mWindowList[index] = None;
    for (int i = index; i < v->mItemSize - 1; i++) {
        v->mWindowList[i] = v->mWindowList[i + 1];
        v->mWindowList[i + 1] = None;
    }

    // Calc new size & maybe re-claim down.
    v->mItemSize--;
    if (v->mItemSize > 0 &&
        v->mItemSize == v->mCapacity / 4) {
        windowVectorResize(v, v->mCapacity / 2);
    }
}

Window windowVectorGet(WindowVector* v, int index) {
    if (index >= 0 && index < v->mItemSize) {
        return v->mWindowList[index];
    }
    return None;
}

bool windowVectorSet(WindowVector* v, int index, Window window) {
    if (windowVectorExists(v, window)) {
        return false;
    }

    if (index >= 0 && index < v->mItemSize) {
        v->mWindowList[index] = window;
    }

    return true;
}

void windowVectorLog(WindowVector* v) {
    for (int i = 0; i < v->mItemSize; i++) {
        printf("[0x%08lx]  ", (long unsigned) v->mWindowList[i]);
    }
    printf("\n");
}
