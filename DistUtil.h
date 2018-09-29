#pragma once

#include <valarray>
#include "sc2api/sc2_interfaces.h"

namespace sc2util {

	void sortByDistanceTo(sc2::Units & units, const sc2::Point2D & start);
	std::vector<int> sortByDistanceTo(std::valarray<float> matrix, int ti, size_t sz);
	std::valarray<float> computeDistanceMatrix(const std::vector<sc2::Point3D> expansions, sc2::QueryInterface * query);
	const sc2::Point2D cog(const std::vector<sc2::Point2D>& points);
}
