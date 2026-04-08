
#pragma once

/**
 * Minimally wrap a PNG file with x11 accessors.
 */

// Std C and c++.
#include <png.h>
#include <string>
#include <vector>

using namespace std;

// X11.
#include <X11/extensions/Xrender.h>

// Module consts.
#define COLOR_RED "\033[0;31m"
#define COLOR_GREEN "\033[1;32m"
#define COLOR_YELLOW "\033[1;33m"
#define COLOR_BLUE "\033[1;34m"
#define COLOR_NORMAL "\033[0m"


/**
 * Class def.
 */
class xPngWrapper {
    public:
        xPngWrapper(const vector<string>& pngFileNames);
        ~xPngWrapper();

        int getWidth();
        int getHeight();
        int getColorType();
        int getBitDepth();

        vector<unsigned long> getPngData();

        bool hasErrorStatus();
        string errorStatus();

    private:
        FILE* getFileFromList(const vector<string>&
            pngFileNames);

        const vector<string> mPngFileName;

        png_structp mPngFileReadStruct;
        png_infop mPngFileInfoStruct;

        png_uint_32 mWidth{0};
        png_uint_32 mHeight{0};
        int mColorType{0};
        int mBitDepth{0};
        vector<unsigned long> mPngData{};

        string mErrorStatus{};
};
