#pragma once

#include "sc2api/sc2_map_info.h"


namespace sc2util {
	// query pathing grid at given point
	bool Pathable(const sc2::GameInfo & info, const sc2::Point2D & point);
	bool Pathable(const sc2::GameInfo & info, const sc2::Point2DI & point);
	// updates 	
	// add (b=true) or remove (b=false) a building of footprint size, at given position (center of the unit/unit pos)
	bool setBuildingAt(sc2::GameInfo & info, const sc2::Point2D & pos, int foot, bool b);
}