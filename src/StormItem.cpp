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

#include <math.h>
#include <stdbool.h>
#include <stdlib.h>

#include "FallenSnow.h"
#include "Flags.h"
#include "hashtable.h"
#include "pixmaps.h"
#include "PlasmaSnow.h"
#include "Storm.h"
#include "StormItem.h"
#include "Utils.h"
#include "Windows.h"


/***********************************************************
 ** Module globals and consts.
 **/
#define INITIAL_Y_SPEED 120

const double MAX_WIND_SENSITIVITY = 0.4;

bool mStormItemBackgroundThreadIsActive = false;

const float mWindSpeedMaxArray[] =
    { 100.0, 300.0, 600.0 };


/***********************************************************
 ** This method creates a basic StormItem from itemType.
 **/
StormItem* createStormItem(int itemType) {

    // If itemType < 0, create random itemType.
    const int RESOURCES_SHAPE_COUNT =
        getResourcesShapeCount();
    if (itemType < 0) {
        itemType = Flags.VintageFlakes ?
            RESOURCES_SHAPE_COUNT * drand48() :
            RESOURCES_SHAPE_COUNT + drand48() *
                (getAllStormItemsShapeCount() - RESOURCES_SHAPE_COUNT);
    }

    StormItem* stormItem = (StormItem*) malloc(sizeof(StormItem));
    stormItem->shapeType = itemType;

    // DEBUG Crashes this way
    static int mDebugSnowWhatFlake = 5;
    if ((int) stormItem->shapeType < 0) {
        if (mDebugSnowWhatFlake-- > 0) {
            printf("plasmasnow: Storm.c: createStormItem(%lu) "
                "BAD !!! Invalid negative itemType : %i.\n",
                (unsigned long) pthread_self(),
                (int) stormItem->shapeType);
        }
    }
    if ((int) stormItem->shapeType >= getAllStormItemsShapeCount()) {
        if (mDebugSnowWhatFlake-- > 0) {
            printf("plasmasnow: Storm.c: createStormItem(%lu) "
                "BAD !!! Invalid positive itemType : %i.\n",
                (unsigned long) pthread_self(),
                (int) stormItem->shapeType);
        }
    }

    const int ITEM_WIDTH = getStormItemSurfaceWidth(stormItem->shapeType);
    const int ITEM_HEIGHT = getStormItemSurfaceHeight(stormItem->shapeType);

    stormItem->xRealPosition = randomIntegerUpTo(
        mGlobal.SnowWinWidth - ITEM_WIDTH);
    stormItem->yRealPosition = -randomIntegerUpTo(
        mGlobal.SnowWinHeight / 10) - ITEM_HEIGHT;

    stormItem->color = getStormShapeColor();
    stormItem->survivesScreenEdges = true;
    stormItem->isFrozen = 0;

    stormItem->fluff = 0;
    stormItem->flufftimer = 0;
    stormItem->flufftime = 0;

    stormItem->xVelocity = (Flags.NoWind) ?
        0 : randomIntegerUpTo(mGlobal.NewWind) / 2;
    stormItem->yVelocity = stormItem->initialYVelocity;

    stormItem->massValue = drand48() + 0.1;
    stormItem->initialYVelocity = INITIAL_Y_SPEED *
        sqrt(stormItem->massValue);
    stormItem->windSensitivity = drand48() *
        MAX_WIND_SENSITIVITY;

    return stormItem;
}

/***********************************************************
 ** Itemset hashtable helper - Add new all items.
 **/
void addStormItemToItemset(StormItem* stormItem) {
    set_insert(stormItem);

    addMethodWithArgToMainloop(PRIORITY_HIGH,
        TIME_BETWEEN_STORMITEM_THREAD_UPDATES,
        (GSourceFunc) updateStormItemOnThread, stormItem);

    mGlobal.stormItemCount++;
}

/***********************************************************
 ** This method updates a stormItem object.
 **/
int updateStormItemOnThread(StormItem* stormItem) {
    mStormItemBackgroundThreadIsActive = true;

    if (!isWorkspaceActive() || Flags.NoSnowFlakes) {
        mStormItemBackgroundThreadIsActive = false;
        return true;
    }

    // Candidate for removal?
    if (mGlobal.RemoveFluff) {
        if (stormItem->fluff || stormItem->isFrozen) {
            eraseStormItemInItemset(stormItem);
            removeStormItemInItemset(stormItem);
            mStormItemBackgroundThreadIsActive = false;
            return false;
        }
    }

    // Candidate for removal?
    if (getStallingNewStormItems()) {
        eraseStormItemInItemset(stormItem);
        removeStormItemInItemset(stormItem);
        mStormItemBackgroundThreadIsActive = false;
        return false;
    }

    // Candidate for removal?
    if (stormItem->fluff &&
        stormItem->flufftimer > stormItem->flufftime) {
        eraseStormItemInItemset(stormItem);
        removeStormItemInItemset(stormItem);
        mStormItemBackgroundThreadIsActive = false;
        return false;
    }

    // Look ahead to the flakes new x/y position.
    const double stormItemUpdateTime =
        TIME_BETWEEN_STORMITEM_THREAD_UPDATES;

    float newFlakeXPos = stormItem->xRealPosition +
        (stormItem->xVelocity * stormItemUpdateTime) *
        getStormItemsSpeedFactor();
    float newFlakeYPos = stormItem->yRealPosition +
        (stormItem->yVelocity * stormItemUpdateTime) *
        getStormItemsSpeedFactor();

    // Update stormItem based on "fluff" status.
    if (stormItem->fluff) {
        if (!stormItem->isFrozen) {
            stormItem->xRealPosition = newFlakeXPos;
            stormItem->yRealPosition = newFlakeYPos;
        }
        stormItem->flufftimer += stormItemUpdateTime;
        mStormItemBackgroundThreadIsActive = false;
        return true;
    }

    // Low probability to remove each, High probability
    // To remove blown-off.
    const bool SHOULD_KILL_STORMITEMS =
        ((mGlobal.stormItemCount - mGlobal.FluffCount) >=
            Flags.FlakeCountMax);

    if (SHOULD_KILL_STORMITEMS) {
        if ((!stormItem->survivesScreenEdges && drand48() > 0.3) ||
            (drand48() > 0.9)) {
            setStormItemFluffState(stormItem, 0.51);
            mStormItemBackgroundThreadIsActive = false;
            return true;
        }
    }

    // Update stormItem speed in X Direction.
    if (!Flags.NoWind) {
        // Calc speed.
        float newXVel = stormItemUpdateTime *
            stormItem->windSensitivity / stormItem->massValue;
        if (newXVel > 0.9) {
            newXVel = 0.9;
        }
        if (newXVel < -0.9) {
            newXVel = -0.9;
        }
        stormItem->xVelocity += newXVel *
            (mGlobal.NewWind - stormItem->xVelocity);

        // Apply speed limits.
        const float xVelMax = mWindSpeedMaxArray[mGlobal.Wind] * 2;
        if (fabs(stormItem->xVelocity) > xVelMax) {
            if (stormItem->xVelocity > xVelMax) {
                stormItem->xVelocity = xVelMax;
            }
            if (stormItem->xVelocity < -xVelMax) {
                stormItem->xVelocity = -xVelMax;
            }
        }
    }

    // Update stormItem speed in Y Direction.
    stormItem->yVelocity += INITIAL_Y_SPEED * (drand48() - 0.4) * 0.1;
    if (stormItem->yVelocity > stormItem->initialYVelocity * 1.5) {
        stormItem->yVelocity = stormItem->initialYVelocity * 1.5;
    }

    // If stormItem is frozen, we're done.
    if (stormItem->isFrozen) {
        mStormItemBackgroundThreadIsActive = false;
        return true;
    }

    // Flake w/h.
    const int ITEM_WIDTH = getStormItemSurfaceWidth(stormItem->shapeType);
    const int ITEM_HEIGHT = getStormItemSurfaceHeight(stormItem->shapeType);

    // Update stormItem based on status.
    if (stormItem->survivesScreenEdges) {
        if (newFlakeXPos < -ITEM_WIDTH) {
            newFlakeXPos += mGlobal.SnowWinWidth - 1;
        }
        if (newFlakeXPos >= mGlobal.SnowWinWidth) {
            newFlakeXPos -= mGlobal.SnowWinWidth;
        }
    } else {
        // Remove it when it goes left or right
        // out of the window.
        if (newFlakeXPos < 0 ||
            newFlakeXPos >= mGlobal.SnowWinWidth) {
            removeStormItemInItemset(stormItem);
            mStormItemBackgroundThreadIsActive = false;
            return false;
        }
    }

    // Remove stormItem if it falls below bottom of screen.
    if (newFlakeYPos >= mGlobal.SnowWinHeight) {
        removeStormItemInItemset(stormItem);
        mStormItemBackgroundThreadIsActive = false;
        return false;
    }

    // New Flake position as Ints.
    const int NEW_STORMITEM_INT_X_POS = lrintf(newFlakeXPos);
    const int NEW_STORMITEM_INT_Y_POS = lrintf(newFlakeYPos);

    // Fallen interaction.
    if (!stormItem->fluff) {
        lockFallenSnowBaseSemaphore();
        if (isStormItemFallen(stormItem, NEW_STORMITEM_INT_X_POS,
                NEW_STORMITEM_INT_Y_POS)) {
            removeStormItemInItemset(stormItem);
            unlockFallenSnowBaseSemaphore();
            mStormItemBackgroundThreadIsActive = false;
            return false;
        }
        unlockFallenSnowBaseSemaphore();
    }

    // Flake NEW_STORMITEM_INT_X_POS/NEW_STORMITEM_INT_Y_POS.
    const int NEW_STORMITEM_REAL_X_POS = lrintf(stormItem->xRealPosition);
    const int NEW_STORMITEM_REAL_Y_POS = lrintf(stormItem->yRealPosition);

    // check if stormItem is touching or in gSnowOnTreesRegion
    // if so: remove it
    if (mGlobal.Wind != 2 &&
        !Flags.NoKeepSnowOnTrees &&
        !Flags.NoTrees) {
        cairo_rectangle_int_t grec = {NEW_STORMITEM_REAL_X_POS,
            NEW_STORMITEM_REAL_Y_POS, ITEM_WIDTH, ITEM_HEIGHT};
        cairo_region_overlap_t in = cairo_region_contains_rectangle(
            mGlobal.gSnowOnTreesRegion, &grec);
        if (in == CAIRO_REGION_OVERLAP_PART ||
            in == CAIRO_REGION_OVERLAP_IN) {
            setStormItemFluffState(stormItem, 0.4);
            stormItem->isFrozen = 1;
            mStormItemBackgroundThreadIsActive = false;
            return true;
        }

        // check if stormItem is touching TreeRegion. If so: add snow to
        // gSnowOnTreesRegion.
        cairo_rectangle_int_t grect = {
            NEW_STORMITEM_REAL_X_POS, NEW_STORMITEM_REAL_Y_POS,
            ITEM_WIDTH, ITEM_HEIGHT};
        in = cairo_region_contains_rectangle(mGlobal.TreeRegion, &grect);

        // so, part of the stormItem is in TreeRegion.
        // For each bottom pixel of the stormItem:
        //   find out if bottompixel is in TreeRegion
        //   if so:
        //     move upwards until pixel is not in TreeRegion
        //     That pixel will be designated as snow-on-tree
        // Only one snow-on-tree pixel has to be found.
        if (in == CAIRO_REGION_OVERLAP_PART) {
            int found = 0;
            int xfound, yfound;
            for (int i = 0; i < ITEM_WIDTH; i++) {
                if (found) {
                    break;
                }

                int ybot = NEW_STORMITEM_REAL_Y_POS + ITEM_HEIGHT;
                int xbot = NEW_STORMITEM_REAL_X_POS + i;
                cairo_rectangle_int_t grect = {xbot, ybot, 1, 1};

                cairo_region_overlap_t in =
                    cairo_region_contains_rectangle(mGlobal.TreeRegion, &grect);

                // If bottom pixel not in TreeRegion, skip.
                if (in != CAIRO_REGION_OVERLAP_IN) {
                    continue;
                }

                // move upwards, until pixel is not in TreeRegion
                for (int j = ybot - 1; j >= NEW_STORMITEM_REAL_Y_POS; j--) {
                    cairo_rectangle_int_t grect = {xbot, j, 1, 1};

                    cairo_region_overlap_t in = cairo_region_contains_rectangle(
                        mGlobal.TreeRegion, &grect);
                    if (in != CAIRO_REGION_OVERLAP_IN) {
                        // pixel (xbot,j) is snow-on-tree
                        found = 1;
                        cairo_rectangle_int_t grec;
                        grec.x = xbot;
                        int p = 1 + drand48() * 3;
                        grec.y = j - p + 1;
                        grec.width = p;
                        grec.height = p;
                        cairo_region_union_rectangle(mGlobal.gSnowOnTreesRegion,
                            &grec);

                        if (Flags.BlowSnow && mGlobal.OnTrees < Flags.MaxOnTrees) {
                            mGlobal.SnowOnTrees[mGlobal.OnTrees].x = grec.x;
                            mGlobal.SnowOnTrees[mGlobal.OnTrees].y = grec.y;
                            mGlobal.OnTrees++;
                        }

                        xfound = grec.x;
                        yfound = grec.y;
                        break;
                    }
                }
            }

            // TODO: Huh? who?
            // Do not erase: this gives bad effects
            // in fvwm-like desktops.
            if (found) {
                stormItem->isFrozen = 1;
                setStormItemFluffState(stormItem, 0.6);

                StormItem* newflake = (Flags.VintageFlakes) ?
                    createStormItem(0) : createStormItem(-1);

                newflake->xRealPosition = xfound;
                newflake->yRealPosition = yfound -
                    getStormItemSurfaceHeight(1) * 0.3f; // oh noes.

                newflake->isFrozen = 1;
                setStormItemFluffState(newflake, 8);

                addStormItemToItemset(newflake);
                mStormItemBackgroundThreadIsActive = false;
                return true;
            }
        }
    }

    stormItem->xRealPosition = newFlakeXPos;
    stormItem->yRealPosition = newFlakeYPos;

    mStormItemBackgroundThreadIsActive = false;
    return true;
}

/***********************************************************
 ** Itemset hashtable helper - Draw all items.
 **/
int drawAllStormItemsInItemset(cairo_t* cr) {
    if (Flags.NoSnowFlakes) {
        return true;
    }

    set_begin();

    StormItem* stormItem;
    while ((stormItem = (StormItem*) set_next())) {
        cairo_set_source_surface(cr,
            getStormItemSurface(stormItem->shapeType),
            stormItem->xRealPosition, stormItem->yRealPosition);

        // Fluff across time, and guard the lower bound.
        double alpha = (0.01 * (100 - Flags.Transparency));
        if (stormItem->fluff) {
            alpha *= (1 - stormItem->flufftimer / stormItem->flufftime);
        }
        if (alpha < 0) {
            alpha = 0;
        }

        // Draw.
        if (mGlobal.isDoubleBuffered ||
            !(stormItem->isFrozen || stormItem->fluff)) {
            my_cairo_paint_with_alpha(cr, alpha);
        }

        // Then update.
        stormItem->xIntPosition = lrint(stormItem->xRealPosition);
        stormItem->yIntPosition = lrint(stormItem->yRealPosition);
    }

    return true;
}

/***********************************************************
 ** This method erases a single stormItem pixmap from the display.
 **/
void eraseStormItemInItemset(StormItem* stormItem) {
    if (mGlobal.isDoubleBuffered) {
        return;
    }

    const int xPos = stormItem->xIntPosition - 1;
    const int yPos = stormItem->yIntPosition - 1;

    const int ITEM_WIDTH = getStormItemSurfaceWidth(stormItem->shapeType) + 2;
    const int ITEM_HEIGHT = getStormItemSurfaceHeight(stormItem->shapeType) + 2;

    clearDisplayArea(mGlobal.display, mGlobal.SnowWin,
        xPos, yPos, ITEM_WIDTH, ITEM_HEIGHT, mGlobal.xxposures);
}

/***********************************************************
 ** Itemset hashtable helper - Remove all items from the list.
 **/
int removeAllStormItemsInItemset() {
    set_begin();

    StormItem* stormItem;
    while ((stormItem = (StormItem*) set_next())) {
        eraseStormItemInItemset(stormItem);
    }

    return true;
}

/***********************************************************
 ** Itemset hashtable helper - Remove a specific item from the list.
 **
 ** Calls to this method from the mainloop() *MUST* be followed by
 ** a 'return false;' to remove this stormItem from the g_timeout
 ** callback.
 **/
void removeStormItemInItemset(StormItem* stormItem) {
    if (stormItem->fluff) {
        mGlobal.FluffCount--;
    }

    set_erase(stormItem);
    free(stormItem);

    mGlobal.stormItemCount--;
}

/***********************************************************
 ** This method updates window surfaces and / or desktop bottom
 ** if stormItem drops onto it.
 */
bool isStormItemFallen(StormItem* stormItem,
    int xPosition, int yPosition) {

    FallenSnow* fsnow = mGlobal.FsnowFirst;
    while (fsnow) {
        if (fsnow->winInfo.hidden) {
            fsnow = fsnow->next;
            continue;
        }

        if (fsnow->winInfo.window != None &&
            !isFallenSnowVisibleOnWorkspace(fsnow) &&
            !fsnow->winInfo.sticky) {
            fsnow = fsnow->next;
            continue;
        }

        if (xPosition < fsnow->x ||
            xPosition > fsnow->x + fsnow->w ||
            yPosition >= fsnow->y + 2) {
            fsnow = fsnow->next;
            continue;
        }

        // Flake hits first FallenSnow & we're done.
        // StormItem hits first FallenItem & we're done.
        const int ITEM_WIDTH = getStormItemSurfaceWidth(stormItem->shapeType);

        int istart = xPosition - fsnow->x;
        if (istart < 0) {
            istart = 0;
        }
        int imax = istart + ITEM_WIDTH;
        if (imax > fsnow->w) {
            imax = fsnow->w;
        }

        for (int i = istart; i < imax; i++) {
            if (yPosition > fsnow->y - fsnow->columnHeightList[i] - 1) {
                if (fsnow->columnHeightList[i] < fsnow->columnMaxHeightList[i]) {
                    updateFallenSnowWithSnow(fsnow,
                        xPosition - fsnow->x, ITEM_WIDTH);
                }

                if (canSnowCollectOnFallen(fsnow)) {
                    setStormItemFluffState(stormItem, .9);
                    if (!stormItem->fluff) {
                        return true;
                    }
                }
                return false;
            }
        }

        // Otherwise, loop thru all.
        fsnow = fsnow->next;
    }

    return false;
}

/***********************************************************
 ** This method sets the StormItem "fluff" state.
 **/
void setStormItemFluffState(StormItem* stormItem, float t) {
    if (stormItem->fluff) {
        return;
    }

    // Fluff it.
    stormItem->fluff = true;
    stormItem->flufftimer = 0;
    stormItem->flufftime = MAX(0.01, t);

    mGlobal.FluffCount++;
}

/***********************************************************
 ** This method prints a StormItem's detail.
 **/
void logStormItem(StormItem* stormItem) {
    printf("plasmasnow: Storm.c: stormItem: "
        "%p xRealPos: %6.0f yRealPos: %6.0f xVelocity: %6.0f yVelocity: %6.0f ws: %f "
        "isFrozen: %d fluff: %6.0d ftr: %8.3f ft: %8.3f\n",
        (void*) stormItem, stormItem->xRealPosition, stormItem->yRealPosition,
        stormItem->xVelocity, stormItem->yVelocity,
        stormItem->windSensitivity, stormItem->isFrozen,
        stormItem->fluff, stormItem->flufftimer, stormItem->flufftime);
}

