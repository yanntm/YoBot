#pragma once

#include <valarray>
#include "sc2api/sc2_interfaces.h"

namespace sc2util {
	// basic sorting, updates units
	void sortByDistanceTo(sc2::Units & units, const sc2::Point2D & start);
	// sorting using a distance matrix, with respect to one element ti
	std::vector<int> sortByDistanceTo(std::valarray<float> matrix, int ti, size_t sz);
	// computing a distance matrix, with actual pathing queries
	std::valarray<float> computeDistanceMatrix(const std::vector<sc2::Point3D> expansions, sc2::QueryInterface * query);
	// utility for center of gravity
	const sc2::Point2D cog(const std::vector<sc2::Point2D>& points);
}
