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
#pragma once

typedef struct {
        unsigned shapeType;

        bool survivesScreenEdges;
        bool isFrozen;

        bool fluff;
        float flufftimer;
        float flufftime;

        // Position values.
        float xRealPosition;
        float yRealPosition;

        // Ater draw.
        int xIntPosition;
        int yIntPosition;

        // Physics.
        float massValue;
        float windSensitivity;
        float initialYVelocity;

        float xVelocity;
        float yVelocity;
} StormItem;


#ifdef __cplusplus
extern "C" {
#endif

    StormItem* createStormItem(int itemType, int typeColor);
    int getStormItemShapeType(int itemType, int typeColor);
    
    void addStormItem(StormItem*);

    int updateStormItemOnThread(StormItem*);

    int drawAllStormItems(cairo_t*);
    void eraseStormItem(StormItem*);
    int removeAllStormItems();
    void removeStormItem(StormItem*);

    bool isStormItemFallen(StormItem*,
        int xPosition, int yPosition);
    void setStormItemFluffState(StormItem*, float state);

    void logStormItem(StormItem*);

#ifdef __cplusplus
}
#endif
