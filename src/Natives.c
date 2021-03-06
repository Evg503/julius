#include "Natives.h"

#include "Building.h"
#include "core/calc.h"
#include "Grid.h"
#include "Terrain.h"

#include "Data/Building.h"
#include "Data/CityInfo.h"
#include "Data/Constants.h"
#include "Data/Grid.h"
#include "Data/Scenario.h"
#include "Data/Settings.h"

#include "building/list.h"
#include "graphics/image.h"

static void determineMeetingCenter();

void Natives_init()
{
	int nativeGraphic = image_group(ID_Graphic_NativeBuilding);
	int gridOffset = Data_Settings_Map.gridStartOffset;
	for (int y = 0; y < Data_Settings_Map.height; y++, gridOffset += Data_Settings_Map.gridBorderSize) {
		for (int x = 0; x < Data_Settings_Map.width; x++, gridOffset++) {
			if (!(Data_Grid_terrain[gridOffset] & Terrain_Building) || Data_Grid_buildingIds[gridOffset]) {
				continue;
			}
			
			int randomBit = Data_Grid_random[gridOffset] & 1;
			int buildingType;
			if (Data_Grid_graphicIds[gridOffset] == Data_Scenario.nativeGraphics.hut) {
				buildingType = BUILDING_NATIVE_HUT;
				Data_Grid_graphicIds[gridOffset] = nativeGraphic;
			} else if (Data_Grid_graphicIds[gridOffset] == Data_Scenario.nativeGraphics.hut + 1) {
				buildingType = BUILDING_NATIVE_HUT;
				Data_Grid_graphicIds[gridOffset] = nativeGraphic + 1;
			} else if (Data_Grid_graphicIds[gridOffset] == Data_Scenario.nativeGraphics.meetingCenter) {
				buildingType = BUILDING_NATIVE_MEETING;
				Data_Grid_graphicIds[gridOffset] = nativeGraphic + 2;
			} else if (Data_Grid_graphicIds[gridOffset] == Data_Scenario.nativeGraphics.crops) {
				buildingType = BUILDING_NATIVE_CROPS;
				Data_Grid_graphicIds[gridOffset] = image_group(ID_Graphic_FarmCrops) + randomBit;
			} else { //unknown building
				Terrain_removeBuildingFromGrids(0, x, y);
				continue;
			}
			int buildingId = Building_create(buildingType, x, y);
			Data_Grid_buildingIds[gridOffset] = buildingId;
			struct Data_Building *b = &Data_Buildings[buildingId];
			b->state = BuildingState_InUse;
			switch (buildingType) {
				case BUILDING_NATIVE_CROPS:
					b->data.industry.progress = randomBit;
					break;
				case BUILDING_NATIVE_MEETING:
					b->sentiment.nativeAnger = 100;
					Data_Grid_buildingIds[gridOffset + 1] = buildingId;
					Data_Grid_buildingIds[gridOffset + 162] = buildingId;
					Data_Grid_buildingIds[gridOffset + 163] = buildingId;
					Terrain_markNativeLand(b->x, b->y, 2, 6);
					if (!Data_CityInfo.nativeMainMeetingCenterX) {
						Data_CityInfo.nativeMainMeetingCenterX = b->x;
						Data_CityInfo.nativeMainMeetingCenterY = b->y;
					}
					break;
				case BUILDING_NATIVE_HUT:
					b->sentiment.nativeAnger = 100;
					b->figureSpawnDelay = randomBit;
					Terrain_markNativeLand(b->x, b->y, 1, 3);
					break;
			}
		}
	}
	
	determineMeetingCenter();
}

static void determineMeetingCenter()
{
	// gather list of meeting centers
	building_list_small_clear();
	for (int i = 1; i < MAX_BUILDINGS; i++) {
		if (BuildingIsInUse(i) && Data_Buildings[i].type == BUILDING_NATIVE_MEETING) {
			building_list_small_add(i);
		}
	}
	int total_meetings = building_list_small_size();
	if (total_meetings <= 0) {
		return;
	}
	const int *meetings = building_list_small_items();
	// determine closest meeting center for hut
	for (int i = 1; i < MAX_BUILDINGS; i++) {
		if (BuildingIsInUse(i) && Data_Buildings[i].type == BUILDING_NATIVE_HUT) {
			int minDist = 1000;
			int minMeetingId = 0;
			for (int n = 0; n < total_meetings; n++) {
				int meetingId = meetings[n];
				int dist = calc_maximum_distance(Data_Buildings[i].x, Data_Buildings[i].y,
					Data_Buildings[meetingId].x, Data_Buildings[meetingId].y);
				if (dist < minDist) {
					minDist = dist;
					minMeetingId = meetingId;
				}
			}
			Data_Buildings[i].subtype.nativeMeetingCenterId = minMeetingId;
		}
	}
}

void Natives_checkLand()
{
	Grid_andByteGrid(Data_Grid_edge, Edge_NoNativeLand);
	if (Data_CityInfo.nativeAttackDuration) {
		Data_CityInfo.nativeAttackDuration--;
	}
	for (int i = 1; i < MAX_BUILDINGS; i++) {
		if (!BuildingIsInUse(i)) {
			continue;
		}
		struct Data_Building *b = &Data_Buildings[i];
		int size, radius;
		if (b->type == BUILDING_NATIVE_HUT) {
			size = 1;
			radius = 3;
		} else if (b->type == BUILDING_NATIVE_MEETING) {
			size = 2;
			radius = 6;
		} else {
			continue;
		}
		if (b->sentiment.nativeAnger >= 100) {
			Terrain_markNativeLand(b->x, b->y, size, radius);
			if (Terrain_hasBuildingOnNativeLand(b->x, b->y, size, radius)) {
				Data_CityInfo.nativeAttackDuration = 2;
			}
		} else {
			b->sentiment.nativeAnger++;
		}
	}
}
