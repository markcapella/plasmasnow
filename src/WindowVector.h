
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include <X11/Xlib.h>


#ifndef WINDOWVECTOR_H
    #define WINDOWVECTOR_H

    #define WINDOWVECTOR_INIT_CAPACITY 4

    typedef struct {
        int mCapacity;

        int mItemSize;
        Window* mWindowList;
    } WindowVector;

    void windowVectorInit(WindowVector*);
    int windowVectorSize(WindowVector*);
    void windowVectorResize(WindowVector*, int);
    void windowVectorFree(WindowVector*);

    bool windowVectorExists(WindowVector*, Window);
    bool windowVectorAdd(WindowVector*, Window);
    void windowVectorDelete(WindowVector*, int);

    Window windowVectorGet(WindowVector*, int);
    bool windowVectorSet(WindowVector*, int, Window);

    void windowVectorLog(WindowVector*);

#endif // * WINDOWVECTOR_H *
