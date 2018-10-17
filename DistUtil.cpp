#include "DistUtil.h"

using namespace sc2;

namespace sc2util {

	void sortByDistanceTo(Units & units, const Point2D & start) {
		std::sort(units.begin(), units.end(), [start](const Unit * a, const Unit * b) { return DistanceSquared2D(start, a->pos) < DistanceSquared2D(start, b->pos); });
	}

	void sortByDistanceTo(std::vector<sc2::Point2D>& units, const sc2::Point2D & start)
	{
		std::sort(units.begin(), units.end(), [start](const auto & a, const auto & b) { return DistanceSquared2D(start, a) < DistanceSquared2D(start, b); });
	}

	std::vector<int> sortByDistanceTo(const std::valarray<float> & matrix, int ti, size_t sz) {
		std::vector<int> byDist;
		for (int i = 0; i < sz; i++) {
			byDist.push_back(i);
		}
		std::sort(byDist.begin(), byDist.end(), [matrix, ti, sz](int a, int b) { return matrix[ti*sz + a] < matrix[ti*sz + b]; });
		return byDist;
	}

	std::valarray<float> computeDistanceMatrix(const Units & units) {
		auto sz = units.size();
		std::valarray<float> matrix(sz *sz); // no more, no less, than a matrix		

		for (int i = 0; i < sz; i++) {
			// set diagonal to 0
			matrix[i*sz + i] = 0;
			for (int j = i + 1; j < sz; j++) {
				auto d = DistanceSquared2D(units[i]->pos, units[j]->pos);
				matrix[i*sz + j] = d;
				matrix[j*sz + i] = d;
			}
		}
		return matrix;
	}

	std::valarray<float> computeDistanceMatrix(const std::vector<Point3D> & expansions, QueryInterface * query) {
		std::vector <sc2::QueryInterface::PathingQuery> queries;
		auto sz = expansions.size();

		// create the set of queries 		
		for (int i = 0; i < sz; i++) {
			for (int j = i + 1; j < sz; j++) {
				queries.push_back({ 0, expansions[i], expansions[j] });
			}
		}
		// send the query and wait for answer
		std::vector<float> distances = query->PathingDistance(queries);

		for (auto & f : distances) {
			if (f == 0) {
				f = std::numeric_limits<float>::max();
			}
		}

		std::valarray<float> matrix(sz *sz); // no more, no less, than a matrix		
		auto dit = distances.begin();
		for (int i = 0; i < sz; i++) {
			// set diagonal to 0
			matrix[i*sz + i] = 0;
			for (int j = i + 1; j < sz; j++, dit++) {
				matrix[i*sz + j] = *dit;
				matrix[j*sz + i] = *dit;
			}
		}
		return matrix;
	}

	const Point2D cog(const std::vector<Point2D>& points) {
		if (points.size() == 0) {
			return Point2D(0, 0);
			// return (Observation()->GetGameInfo().playable_min + Observation()->GetGameInfo().playable_max) / 2;
		}
		Point2D targetc = points.front();
		float div = 1;
		for (auto it = ++points.begin(); it != points.end(); ++it) {
			targetc += *it;
			div++;
		}
		targetc /= div;
		return targetc;
	}

	const Unit * FindNearestUnit(const Point2D & start, const Units & units, float maxRangeSquared)
	{
		float distance = std::numeric_limits<float>::max();
		const Unit* targete = nullptr;
		for (const auto& u : units) {
			float d = DistanceSquared2D(u->pos, start);
			if (d < distance && d <= maxRangeSquared) {
				distance = d;
				targete = u;
			}
		}
		if (distance <= maxRangeSquared) {
			return targete;
		}
		return nullptr;
	}

}