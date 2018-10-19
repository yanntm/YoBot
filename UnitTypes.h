#pragma once

#include "sc2api/sc2_interfaces.h"

namespace sc2util {
	// useful functions for filtering and classifying units
	
	// all mineral types
	bool IsMineral(sc2::UNIT_TYPEID type);
	// all geyser types
	bool IsVespene(sc2::UNIT_TYPEID type);
	// workers + mules
	bool IsWorkerType(sc2::UNIT_TYPEID type);
	// if it produces workers, it's a centre
	bool IsCommandStructure(sc2::UNIT_TYPEID type);
	// if it's a building that attacks you
	bool isStaticDefense(sc2::UNIT_TYPEID type);
	// a moving enemy that hits from range (TODO : list very incomplete)
	bool IsRangedUnit(sc2::UNIT_TYPEID type);
	// any building
	bool IsBuilding(sc2::UNIT_TYPEID type);
	// any moving enemy with a weapon
	bool IsArmyUnitType(sc2::UNIT_TYPEID type);

	bool isChronoed(const sc2::Unit * unit);
}