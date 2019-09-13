#pragma once

#include "MapTopology.h"

class BuildingPlacer {
	std::vector<int> reserved;
	int width;
	int height;
	const MapTopology * map;
public :
	void init(const sc2::ObservationInterface * initial, sc2::QueryInterface * query, const MapTopology * map, sc2::DebugInterface * debug = nullptr);
	// query build grid at given point
	bool Placement(const sc2::GameInfo & info, const sc2::Point2D & point) const;
	bool PlacementI(const sc2::GameInfo & info, const sc2::Point2DI & pointI) const;
	bool PlacementB(const sc2::GameInfo & info, const sc2::Point2D & point, int footprint) const;
	// higher level API : find a list of locations for a building, at or around a given location
	// specify additional constraints to sort the result	

	// updating the grid
	// add (b=true) or remove (b=false) a building of footprint size, at given position (center of the unit/unit pos)
	bool setBuildingAt(sc2::GameInfo & info, const sc2::Point2D & pos, int foot, bool b);

	// reserve some tile
	void reserve(const sc2::Point2D & point);
	// reserve mineral/gas tiles for an expansion index
	void reserve(int expIndex);
	void reserveVector(const sc2::Point2D & start, const sc2::Point2D & vec);
	void reserveCliffSensitive(int expIndex, const sc2::ObservationInterface* obs, const sc2::GameInfo & info);
#ifdef DEBUG
	void debug(sc2::DebugInterface * debug, const sc2::ObservationInterface * obs, const sc2::GameInfo & info);
#endif // DEBUG
};
