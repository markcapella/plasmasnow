
#pragma once

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
#define VECTOR_TYPE Window

typedef struct {
    int mCapacity;

    int mItemSize;
    VECTOR_TYPE* mWindowList;
} WindowVector;

#define WINDOWVECTOR_INIT_CAPACITY 4


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
