#include "MapTopology.h"

#include "sc2api/sc2_agent.h"
#include "sc2lib/sc2_lib.h"
#include "DistUtil.h"
#include "UnitTypes.h"
#include <algorithm>

using namespace std;
using namespace sc2;
using namespace sc2util;

void MapTopology::init(const sc2::ObservationInterface * initial, sc2::QueryInterface * query, sc2::DebugInterface * debug)
{
	const GameInfo& game_info = initial->GetGameInfo();
	// compute a good place to put a nexus on the map : minerals and gas
	{
		expansions = MapTopology::CalculateExpansionLocations(initial, query);
		// sometimes we get bad stuff out of here
		auto min = game_info.playable_min;
		auto max = game_info.playable_max;
		expansions.erase(
			remove_if(expansions.begin(), expansions.end(), [min, max](const Point3D & u) { return !(u.x > min.x && u.x < max.x && u.y > min.y && u.y < max.y); })
			, expansions.end());

	}
	// potential main bases as provided by map
	const auto & starts = game_info.start_locations;
	// Determine choke and proxy locations

	size_t sz = expansions.size();

	// for the next steps, we want pathing info; targetting the center of a nexus will not work, so moving a bit off center is good.
	vector<Point3D> expcopy = expansions;
	for (auto & p : expcopy) {
		Units closeMins = initial->GetUnits(Unit::Alliance::Neutral, [p](const Unit & u) { return  IsMineral(u.unit_type) && Distance2D(u.pos, p) < 10.0f; });
		sortByDistanceTo(closeMins, p);
		// take the four closest

		if (closeMins.size() >= 4) {
			Point3D cog = (closeMins[0]->pos + closeMins[1]->pos + closeMins[2]->pos + closeMins[3]->pos) / 4;
			p += (p - cog);
		}
	}

	std::valarray<float> matrix = computeDistanceMatrix(expcopy, query);

#ifdef DEBUG
	if (debug != nullptr) {
		for (int i = 0; i < sz; i++) {
			for (int j = i + 1; j < sz; j++) {
				auto color = (matrix[i*sz + j] > 100000.0f) ? Colors::Red : Colors::Green;
				debug->DebugLineOut(expcopy[i] + Point3D(0, 0, 0.5f), expcopy[j] + Point3D(0, 0, 0.5f), color);
				debug->DebugTextOut(std::to_string(matrix[i*sz + j]), (expcopy[i] + Point3D(0, 0, 0.2f) + expcopy[j]) / 2, Colors::Green);
			}
		}
	}
#endif // DEBUG

	// Ok now tag expansion locations 
	for (size_t startloc = 0, max = game_info.start_locations.size(); startloc < max; startloc++) {
		const auto & sloc = game_info.start_locations[startloc];
		//first find the expansion that is the main base
		float distance = std::numeric_limits<float>::max();
		int ti = 0;
		for (int i = 0; i < sz; i++) {
			auto d = DistanceSquared2D(sloc, expansions[i]);
			if (distance > d) {
				distance = d;
				ti = i;
			}
		}
		mainBases.push_back(ti);
	}

	for (int baseIndex = 0; baseIndex < mainBases.size(); baseIndex++) {
		int ti = mainBases[baseIndex];
		// next look for the closest bases to this main
		std::vector<int> byDist = sortByDistanceTo(matrix, ti, sz);

		int maxCloseBaseIndex = 2;
		float dClosest = matrix[ti*sz + byDist[1]];
		for (int i = 2; i < sz; i++) {
			if (matrix[ti*sz + byDist[i]] > 1.5 * dClosest) {
				break;
			}
			else {
				maxCloseBaseIndex++;
			}
		}

		int nat = byDist[1];
		int pocket = -1;

		int nmyStart = (baseIndex == 0) ? mainBases[1] : mainBases[0];
		// are there several nat candidates ?
		if (maxCloseBaseIndex > 2) {
			// the first one of these that is closer to enemy base than the main base is a nat
			for (int i = 1; i < maxCloseBaseIndex; i++) {

				if (matrix[nmyStart*sz + byDist[i]] < matrix[nmyStart*sz + ti]) {
					nat = byDist[i];
				}
				else {
					pocket = byDist[i];
				}
			}
		}
		naturalBases.push_back(nat);

		// the one of these two that is closer to our base
		pocketBases.push_back(pocket);

		// next look for the closest bases to this nat 
		std::vector<int> byDistNat = sortByDistanceTo(matrix, nat, sz);
		byDistNat.erase(
			std::remove_if(byDistNat.begin(), byDistNat.end(), [nat, pocket, ti](int v) { return v == nat || v == pocket || v == ti; })
			, byDistNat.end());

		// limit to close bases
		float dCloseNat = matrix[nat*sz + byDistNat[0]];
		int tokeep = 1;
		for (int i = 1; i < sz; i++) {
			if (matrix[nat*sz + byDistNat[i]] > 1.5 * dCloseNat) {
				break;
			}
			else {
				tokeep++;
			}
		}
		byDistNat.resize(tokeep);


		int proxy;
		// choose the one that is furthest from line linking ourBase to his.
		float distance = 0;
		// distance P1P2 for denominator
		const auto & P1 = expansions[nmyStart];
		const auto & P2 = expansions[ti];
		float denom = Distance2D(P1, P2);
		for (int base : byDistNat) {
			const auto & X0 = expansions[base];
			// Equation is straight off of : https://en.wikipedia.org/wiki/Distance_from_a_point_to_a_line#Line_defined_by_two_points
			float d = std::abs((P2.y - P1.y)*X0.x - (P2.x - P1.x)*X0.y + P2.x*P1.y - P2.y*P1.x) / denom;
			if (d > distance) {
				distance = d;
				proxy = base;
			}
		}
		proxyBases.push_back(proxy);

	}

	auto myStart = initial->GetStartLocation();
	for (int i = 0; i < starts.size(); i++) {
		if (DistanceSquared2D(starts[i], myStart) < 5.0f) {
			ourBaseStartLocIndex = i;
		}
	}

#ifdef DEBUG
	debugMap(debug);
	if (debug != nullptr) debug->SendDebug();
#endif // DEBUG

}

bool MapTopology::hasPockets() const {
	return pocketBases[0] != -1;
}

const sc2::Point3D & MapTopology::getPosition(Player p, BaseType b) const {
	int id = p == ally ? ourBaseStartLocIndex : (ourBaseStartLocIndex == 0) ? 1 : 0;

	switch (b) {
	case nat: return expansions[naturalBases[id]];
	case proxy: return expansions[proxyBases[id]];
	case pocket:
		if (hasPockets()) {
			return expansions[pocketBases[id]];
		}
		else {
			return expansions[naturalBases[id]];
		}
	case main: 
	default :	return expansions[mainBases[id]];
	}
}

const Point3D & MapTopology::FindNearestBase(const Point3D& start) const {
	float distance = std::numeric_limits<float>::max();
	const Point3D * targetb = &expansions[0];
	for (const auto& u : expansions) {
		float d = DistanceSquared2D(u, start);
		float deltaz = abs(start.z - u.z);
		if (d < distance && deltaz < 1.0f) {
			distance = d;
			targetb = &u;
		}
	}
	return *targetb;
}

void MapTopology::debugMap(DebugInterface * debug) {
	if (debug == nullptr) {
		return;
	}
	int i = 0;
	for (const auto & e : expansions) {
		debug->DebugSphereOut(e + Point3D(0, 0, 0.1f), 2.25f, Colors::Red);
		std::string text = "expo" + std::to_string(i++);
		debug->DebugTextOut(text, sc2::Point3D(e.x, e.y, e.z + 2), Colors::Green);
	}

	for (int startloc = 0, max = mainBases.size(); startloc < max; startloc++) {
		debug->DebugTextOut("main" + std::to_string(startloc), expansions[mainBases[startloc]] + Point3D(0, 2, .5), Colors::Green);
		debug->DebugTextOut("nat" + std::to_string(startloc), expansions[naturalBases[startloc]] + Point3D(0, 2, .5), Colors::Green);
		if (pocketBases[startloc] != -1) debug->DebugTextOut("pocket" + std::to_string(startloc), expansions[pocketBases[startloc]] + Point3D(0, 2, .5), Colors::Green);
		debug->DebugTextOut("proxy" + std::to_string(startloc), expansions[proxyBases[startloc]] + Point3D(0, 2, .5), Colors::Green);
	}
}

static const float PI = 3.1415927f;
// Directly taken from sc2_search.cc of sc2api
size_t CalculateQueries(float radius, float step_size, const Point2D& center, std::vector<QueryInterface::PlacementQuery>& queries) {
	Point2D current_grid, previous_grid(std::numeric_limits<float>::max(), std::numeric_limits<float>::max());
	size_t valid_queries = 0;
	// Find a buildable location on the circumference of the sphere
	float loc = 0.0f;
	while (loc < 360.0f) {
		Point2D point = Point2D(
			(radius * std::cos((loc * PI) / 180.0f)) + center.x,
			(radius * std::sin((loc * PI) / 180.0f)) + center.y);

		QueryInterface::PlacementQuery query(ABILITY_ID::BUILD_COMMANDCENTER, point);

		current_grid = Point2D(floor(point.x), floor(point.y));

		if (previous_grid != current_grid) {
			queries.push_back(query);
			++valid_queries;
		}

		previous_grid = current_grid;
		loc += step_size;
	}

	return valid_queries;
}


// Adapted (patched) with respect to version of sc_search.cc of the sc2api
std::vector<Point3D> MapTopology::CalculateExpansionLocations(const ObservationInterface* observation, QueryInterface* query) {
	search::ExpansionParameters parameters = sc2::search::ExpansionParameters();
	Units resources = observation->GetUnits(
		[](const Unit& unit) {
		return unit.unit_type == UNIT_TYPEID::NEUTRAL_MINERALFIELD || unit.unit_type == UNIT_TYPEID::NEUTRAL_MINERALFIELD750 ||
			unit.unit_type == UNIT_TYPEID::NEUTRAL_RICHMINERALFIELD || unit.unit_type == UNIT_TYPEID::NEUTRAL_RICHMINERALFIELD750 ||
			unit.unit_type == UNIT_TYPEID::NEUTRAL_PURIFIERMINERALFIELD || unit.unit_type == UNIT_TYPEID::NEUTRAL_PURIFIERMINERALFIELD750 ||
			unit.unit_type == UNIT_TYPEID::NEUTRAL_PURIFIERRICHMINERALFIELD || unit.unit_type == UNIT_TYPEID::NEUTRAL_PURIFIERRICHMINERALFIELD750 ||
			unit.unit_type == UNIT_TYPEID::NEUTRAL_LABMINERALFIELD || unit.unit_type == UNIT_TYPEID::NEUTRAL_LABMINERALFIELD750 ||
			unit.unit_type == UNIT_TYPEID::NEUTRAL_BATTLESTATIONMINERALFIELD || unit.unit_type == UNIT_TYPEID::NEUTRAL_BATTLESTATIONMINERALFIELD750 ||
			unit.unit_type == UNIT_TYPEID::NEUTRAL_VESPENEGEYSER || unit.unit_type == UNIT_TYPEID::NEUTRAL_PROTOSSVESPENEGEYSER ||
			unit.unit_type == UNIT_TYPEID::NEUTRAL_SPACEPLATFORMGEYSER || unit.unit_type == UNIT_TYPEID::NEUTRAL_PURIFIERVESPENEGEYSER ||
			unit.unit_type == UNIT_TYPEID::NEUTRAL_SHAKURASVESPENEGEYSER || unit.unit_type == UNIT_TYPEID::NEUTRAL_RICHVESPENEGEYSER;
	}
	);


	std::vector<Point3D> expansion_locations;
	std::vector<std::pair<Point3D, std::vector<Unit> > > clusters = search::Cluster(resources, parameters.cluster_distance_);

	std::vector<size_t> query_size;
	std::vector<QueryInterface::PlacementQuery> queries;
	for (size_t i = 0; i < clusters.size(); ++i) {
		std::pair<Point3D, std::vector<Unit> >& cluster = clusters[i];
		if (parameters.debug_) {
			for (auto r : parameters.radiuses_) {
				parameters.debug_->DebugSphereOut(cluster.first, r, Colors::Green);
			}
		}

		// Get the required queries for this cluster.
		size_t query_count = 0;
		for (auto r : parameters.radiuses_) {
			query_count += CalculateQueries(r, parameters.circle_step_size_, cluster.first, queries);
		}

		query_size.push_back(query_count);
	}

	std::vector<bool> results = query->Placement(queries);

	// Edit the results : allow to build in command structure existing location
	Units commandStructures = observation->GetUnits(
		[](const Unit& unit) {
		return unit.unit_type == UNIT_TYPEID::TERRAN_COMMANDCENTER || unit.unit_type == UNIT_TYPEID::TERRAN_ORBITALCOMMAND || unit.unit_type == UNIT_TYPEID::TERRAN_PLANETARYFORTRESS ||
			unit.unit_type == UNIT_TYPEID::PROTOSS_NEXUS ||
			unit.unit_type == UNIT_TYPEID::ZERG_HATCHERY || unit.unit_type == UNIT_TYPEID::ZERG_LAIR || unit.unit_type == UNIT_TYPEID::ZERG_HIVE;
	});
	for (auto cc : commandStructures) {
		for (int i = 0; i < queries.size(); ++i) {
			if (DistanceSquared2D(cc->pos, queries[i].target_pos) < 1.0f) {
				results[i] = true;
			}
		}
	}

	size_t start_index = 0;
	for (int i = 0; i < clusters.size(); ++i) {
		std::pair<Point3D, std::vector<Unit> >& cluster = clusters[i];

		// to store the distances to gas per valid position
		std::vector<float> dposgas;
		dposgas.reserve(query_size[i]);
		// first traverse and find minimal distance to gas as well as distance of each cluster and store it 
		float dgasmin = std::numeric_limits<float>::max();
		for (size_t j = start_index, e = start_index + query_size[i]; j < e; ++j) {
			if (!results[j]) {
				continue;
			}

			Point2D& p = queries[j].target_pos;

			float dgas = 0;
			// instead sum distances to all minerals/gas in the cluster
			for (const auto & unit : cluster.second) {
				// distance squared is faster and does not change min/max results
				if (unit.unit_type == UNIT_TYPEID::NEUTRAL_VESPENEGEYSER || unit.unit_type == UNIT_TYPEID::NEUTRAL_PROTOSSVESPENEGEYSER ||
					unit.unit_type == UNIT_TYPEID::NEUTRAL_SPACEPLATFORMGEYSER || unit.unit_type == UNIT_TYPEID::NEUTRAL_PURIFIERVESPENEGEYSER ||
					unit.unit_type == UNIT_TYPEID::NEUTRAL_SHAKURASVESPENEGEYSER || unit.unit_type == UNIT_TYPEID::NEUTRAL_RICHVESPENEGEYSER) {
					dgas += DistanceSquared2D(p, unit.pos);
				}
			}
			if (dgas < dgasmin) {
				dgasmin = dgas;
			}
			dposgas.push_back(dgas);
		}

		float distance = std::numeric_limits<float>::max();
		Point2D closest;
		// For each query for the cluster minimum distance location that is valid.
		for (size_t j = start_index, e = start_index + query_size[i], index = 0; j < e; ++j) {
			if (!results[j]) {
				continue;
			}
			// index only incremented for valid positions
			if (dposgas[index++] > dgasmin) {
				continue;
			}
			Point2D& p = queries[j].target_pos;

			float d = Distance2D(p, cluster.first);
			if (d < distance) {
				distance = d;
				closest = p;
			}
		}

		Point3D expansion(closest.x, closest.y, cluster.second.begin()->pos.z);

		if (parameters.debug_) {
			parameters.debug_->DebugSphereOut(expansion, 0.35f, Colors::Red);
		}

		expansion_locations.push_back(expansion);
		start_index += query_size[i];
	}

	return expansion_locations;
}

