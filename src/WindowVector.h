
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include <X11/Xlib.h>


/** *********************************************************************
 ** WindowVector lifecycle methods.
 **
 ** Forked & extended from:
 **     https://www.sanfoundry.com/c-program-implement-vector/
 **/
#ifndef WINDOWVECTOR_H
    #define WINDOWVECTOR_H

    #define VECTOR_TYPE Window

    #define WINDOWVECTOR_INIT_CAPACITY 4

    typedef struct {
        int mCapacity;

        int mItemSize;
        VECTOR_TYPE* mWindowList;
    } WindowVector;

    // Accessors.
    void windowVectorInit(WindowVector*);
    int windowVectorSize(WindowVector*);
    void windowVectorResize(WindowVector*, int);
    void windowVectorFree(WindowVector*);

    bool windowVectorExists(WindowVector*, VECTOR_TYPE);
    bool windowVectorAdd(WindowVector*, VECTOR_TYPE);
    void windowVectorDelete(WindowVector*, int);

    VECTOR_TYPE windowVectorGet(WindowVector*, int);
    bool windowVectorSet(WindowVector*, int, VECTOR_TYPE);

    void windowVectorLog(WindowVector*);

#endif // * WINDOWVECTOR_H *
