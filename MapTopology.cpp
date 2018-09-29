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
		expansions = sc2::search::CalculateExpansionLocations(initial, query);
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
	for (auto & p : expansions) {
		Units closeMins = initial->GetUnits(Unit::Alliance::Neutral, [p](const Unit & u) { return  IsMineral(u.unit_type) && Distance2D(u.pos, p) < 10.0f; });
		sortByDistanceTo(closeMins, p);
		// take the four closest

		if (closeMins.size() >= 4) {
			Point3D cog = (closeMins[0]->pos + closeMins[1]->pos + closeMins[2]->pos + closeMins[3]->pos) / 4;
			p += (p - cog);
		}
	}

	std::valarray<float> matrix = computeDistanceMatrix(expansions,query);

#ifdef DEBUG
	if (debug != nullptr) {
		for (int i = 0; i < sz; i++) {
			for (int j = i + 1; j < sz; j++) {
				auto color = (matrix[i*sz + j] > 100000.0f) ? Colors::Red : Colors::Green;
				debug->DebugLineOut(expansions[i] + Point3D(0, 0, 0.5f), expansions[j] + Point3D(0, 0, 0.5f), color);
				debug->DebugTextOut(std::to_string(matrix[i*sz + j]), (expansions[i] + Point3D(0, 0, 0.2f) + expansions[j]) / 2, Colors::Green);
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
	int id = p==ally ? ourBaseStartLocIndex : (ourBaseStartLocIndex == 0) ? 1 : 0;
	
	switch (b) {
	case main: return expansions[mainBases[id]];
	case nat: return expansions[naturalBases[id]];
	case proxy: return expansions[proxyBases[id]];
	case pocket: 
		if (hasPockets()) {
			return expansions[pocketBases[id]];
		}
		else {
			return expansions[naturalBases[id]];
		}
	}
}

const Point3D & MapTopology::FindNearestBase(const Point3D& start) const {
	float distance = std::numeric_limits<float>::max();
	const Point3D * targetb = & expansions[0];
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

	for (int startloc = 0, max=mainBases.size(); startloc < max; startloc++) {
		debug->DebugTextOut("main" + std::to_string(startloc), expansions[mainBases[startloc]] + Point3D(0, 2, .5), Colors::Green);
		debug->DebugTextOut("nat" + std::to_string(startloc), expansions[naturalBases[startloc]] + Point3D(0, 2, .5), Colors::Green);
		if (pocketBases[startloc] != -1) debug->DebugTextOut("pocket" + std::to_string(startloc), expansions[pocketBases[startloc]] + Point3D(0, 2, .5), Colors::Green);
		debug->DebugTextOut("proxy" + std::to_string(startloc), expansions[proxyBases[startloc]] + Point3D(0, 2, .5), Colors::Green);
	}
}