#pragma once

#include <vector>
#include "MapTopology.h"
#include "sc2api/sc2_interfaces.h"
#include "YoAgent.h"


/* Easier sc2 style signature */
std::vector<sc2::Point2DI> AstarSearchPath(sc2::Point2DI start, sc2::Point2DI end, const sc2::GameInfo & info, float * weights);
std::vector<sc2::Point2DI> AstarSearchPath(sc2::Point2D start, sc2::Point2D end, const sc2::GameInfo & info, float * weights);
float * computeWeightMap(const sc2::GameInfo & info, const sc2::UnitTypes & types, const YoAgent::UnitsMap & allEnemies);

/* Original signature */
bool astar(
	const float* weights, const int h, const int w,
	const int start, const int goal, bool diag_ok,
	int* paths);
