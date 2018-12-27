#pragma once

#include "sc2api/sc2_map_info.h"

// API for manipulation of grid data as a matrix of bool indexed by Position
namespace sc2util {

	// query pathing grid at given point
	bool isSet(const sc2::GameInfo & info, const sc2::ImageData & grid, const sc2::Point2D & point);
	bool isSet(const sc2::GameInfo & info, const sc2::ImageData & grid, const sc2::Point2DI & point);
	// updates 
	bool set(sc2::GameInfo & info, sc2::ImageData & grid, const sc2::Point2D & point, bool b);
	bool set(sc2::GameInfo & info, sc2::ImageData & grid, const sc2::Point2DI & point, bool b);

	bool setBuildingAt(sc2::GameInfo & info, sc2::ImageData & grid, const sc2::Point2D & pos, int footprint, bool b);
}