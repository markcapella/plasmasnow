
/**
 * Minimally wrap a PNG file with x11 accessors.
 */

// Std C and c++.
#include <fstream>
#include <iostream>
#include <png.h>
#include <string>
#include <stdexcept>
#include <vector>

using namespace std;

// X11.
#include <X11/extensions/Xrender.h>

// Application.
#include "xPngWrapper.h"


/**
 * Class instantiation.
 */
xPngWrapper::xPngWrapper(const vector<string>& pngFileNames) {
    // Wrap first file found to exist from
    // supplied candidate list.
    FILE* pngFile = getFileFromList(pngFileNames);
    if (!pngFile) {
        mErrorStatus = "No candidate PNG "
            "files exist.";
        return;
    }

    png_init_io(mPngFileReadStruct, pngFile);
    png_set_sig_bytes(mPngFileReadStruct, 8);
    png_read_info(mPngFileReadStruct, mPngFileInfoStruct);

    mWidth = png_get_image_width(mPngFileReadStruct,
        mPngFileInfoStruct);
    mHeight = png_get_image_height(mPngFileReadStruct,
        mPngFileInfoStruct);
    mColorType = png_get_color_type(mPngFileReadStruct,
        mPngFileInfoStruct);
    mBitDepth = png_get_bit_depth(mPngFileReadStruct,
        mPngFileInfoStruct);

    // Set up transformations for a consistent
    // RGBA 8-bit format.
    if (mBitDepth == 16) {
        cout << COLOR_YELLOW << "xPngWrapper: "
            "Downconverting from 16bit to 8bit." <<
            COLOR_NORMAL << "\n";
        png_set_strip_16(mPngFileReadStruct);
    }

    if (mColorType == PNG_COLOR_TYPE_PALETTE) {
        cout << COLOR_YELLOW << "xPngWrapper: "
            "Setting palette to rgb." <<
            COLOR_NORMAL << "\n";
        png_set_palette_to_rgb(mPngFileReadStruct);
    }

    if (mColorType == PNG_COLOR_TYPE_GRAY &&
        mBitDepth < 8) {
        cout << COLOR_YELLOW << "xPngWrapper: "
            "Expanding Gray (1,2,4) to (8)." <<
            COLOR_NORMAL << "\n";
        png_set_expand_gray_1_2_4_to_8(
        mPngFileReadStruct);
    }

    if (png_get_valid(mPngFileReadStruct,
        mPngFileInfoStruct, PNG_INFO_tRNS)) {
        cout << COLOR_YELLOW << "xPngWrapper: "
            "Setting tRNS to Alpha." <<
            COLOR_NORMAL << "\n";
        png_set_tRNS_to_alpha(mPngFileReadStruct);
    }

    if (mColorType == PNG_COLOR_TYPE_RGB ||
        mColorType == PNG_COLOR_TYPE_GRAY ||
        mColorType == PNG_COLOR_TYPE_PALETTE) {
        cout << COLOR_YELLOW << "xPngWrapper: "
            "Setting filler to 0xff." <<
            COLOR_NORMAL << "\n";
        png_set_filler(mPngFileReadStruct, 0xFF,
            PNG_FILLER_AFTER);
    }

    if (mColorType == PNG_COLOR_TYPE_GRAY ||
        mColorType == PNG_COLOR_TYPE_GRAY_ALPHA) {
        cout << COLOR_YELLOW << "xPngWrapper: "
            "Setting gray to RGB." <<
            COLOR_NORMAL << "\n";
        png_set_gray_to_rgb(mPngFileReadStruct);
    }

    // First two values are array w/h.
    mPngData.push_back(getWidth());
    mPngData.push_back(getHeight());

    // Update libpng info, the get Png
    // rowbytes & allocate a row buffer.
    png_read_update_info(mPngFileReadStruct,
        mPngFileInfoStruct);
    png_uint_32 rowBytes = png_get_rowbytes(
        mPngFileReadStruct, mPngFileInfoStruct);
    vector<png_byte> pngRowData(rowBytes);

    // Copy the PNG data into the Icon.
    for (png_uint_32 h = 0; h < getHeight(); ++h) {
        png_read_row(mPngFileReadStruct,
            pngRowData.data(), nullptr);
        for (png_uint_32 x = 0;
            x < rowBytes; x += 4) {
            const XRenderColor rcRGBA = {
                .red = pngRowData[x + 0],
                .green = pngRowData[x + 1],
                .blue = pngRowData[x + 2],
                .alpha = pngRowData[x + 3]
            };
            const unsigned long COLOR_PIXEL =
                (static_cast<unsigned long>
                    (rcRGBA.alpha) << 24) |
                (static_cast<unsigned long>
                    (rcRGBA.red)   << 16) |
                (static_cast<unsigned long>
                    (rcRGBA.green) <<  8) |
                (static_cast<unsigned long>
                    (rcRGBA.blue)  <<  0);
            mPngData.push_back(COLOR_PIXEL);
        }
    }

    // Cleanup, exit.
    png_read_end(mPngFileReadStruct, nullptr);
    png_destroy_read_struct(&mPngFileReadStruct,
        &mPngFileInfoStruct, nullptr);
    fclose(pngFile);
}

xPngWrapper::~xPngWrapper() {
}

/**
 * Public class getters.
 */
int xPngWrapper::getWidth() {
    return mWidth;
}

int xPngWrapper::getHeight() {
    return mHeight;
}

int xPngWrapper::getColorType() {
    return mColorType;
}

int xPngWrapper::getBitDepth() {
    return mBitDepth;
}

/**
 * Public helpers to return wrapper status.
 */
bool xPngWrapper::hasErrorStatus() {
    return mErrorStatus != "";
}
string xPngWrapper::errorStatus() {
    return mErrorStatus;
}

/**
 * Public to return data suitable to load
 * x11 window _NET_WM_ICON property.
 */
vector<unsigned long> 
xPngWrapper::getPngData() {
    return mPngData;
}

/**
 * This method returns the first png file
 * found to actually exist from a list of
 * candidate names.
 */
FILE* xPngWrapper::getFileFromList(
    const vector<string>& pngFileNames) {

    for (const string &pngFileName : pngFileNames) {
        FILE* pngFile = fopen(pngFileName.c_str(), "rb");
        if (!pngFile) {
            continue;
        }

        //The PNG file is not a valid PNG file.
        png_byte pngFileHeader[8];
        fread(pngFileHeader, 1, 8, pngFile);
        if (png_sig_cmp(pngFileHeader, 0, 8)) {
            fclose(pngFile);
            continue;
        }

        // libpng can't be inited to handle the PNG file.
        mPngFileReadStruct = png_create_read_struct(
            PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
        if (!mPngFileReadStruct) {
            fclose(pngFile);
            continue;
        }

        // The PNG file cannot be opened by libpng.
        mPngFileInfoStruct = png_create_info_struct(
            mPngFileReadStruct);
        if (!mPngFileInfoStruct) {
            png_destroy_read_struct(&mPngFileReadStruct,
                nullptr, nullptr);
            fclose(pngFile);
            continue;
        }

        // The PNG file cannot be read by libpng.
        if (setjmp(png_jmpbuf(mPngFileReadStruct))) {
            png_destroy_read_struct(&mPngFileReadStruct,
                &mPngFileInfoStruct, nullptr);
            fclose(pngFile);
            continue;
        }

        // Success!
        cout << COLOR_YELLOW << "xPngWrapper: "
            "Wrapped file: " << pngFileName <<
            COLOR_NORMAL << endl;
        return pngFile; // Open
    }

    // Default, fail.
    return nullptr;
}
