#include "Pathing.h"
#include "ImageUtil.h"

using namespace sc2;

namespace sc2util {
	// code for these two is taken from 5minBot, by Archiatrus.
	bool Pathable(const sc2::GameInfo & info, const sc2::Point2D & point)
	{
		return isSet(info, info.pathing_grid, point);
	}
	bool Pathable(const sc2::GameInfo & info, const sc2::Point2DI & point)
	{
		return isSet(info, info.pathing_grid, point);
	}
	bool setPathable(sc2::GameInfo & info, const sc2::Point2D & pointI, bool b) {
		return set(info, info.pathing_grid, pointI, b);
	}
	bool setBuildingAt(sc2::GameInfo & info, const sc2::Point2D & pos, int footprint, bool b)
	{		
		return setBuildingAt(info, info.pathing_grid,pos,footprint,b);
	}
}

