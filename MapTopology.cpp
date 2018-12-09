#include "MapTopology.h"

#include "sc2api/sc2_agent.h"
#include "DistUtil.h"
#include "UnitTypes.h"
#include <algorithm>

using namespace std;
using namespace sc2;
using namespace sc2util;

void MapTopology::init(const sc2::ObservationInterface * initial, sc2::QueryInterface * query, sc2::DebugInterface * debug)
{
	const GameInfo& game_info = initial->GetGameInfo();
	this->width = game_info.width;
	this->height = game_info.height;
	this->reserved = vector<int>(height*width, 0);

	// compute a good place to put a nexus on the map : minerals and gas
	{
		auto clust = MapTopology::CalculateExpansionLocations(initial, query);		
		// sometimes we get bad stuff out of here
		auto min = game_info.playable_min;
		auto max = game_info.playable_max;
		clust.erase(
			remove_if(clust.begin(), clust.end(), [min, max](const auto & p) { const auto & u = p.first; return !(u.x > min.x && u.x < max.x && u.y > min.y && u.y < max.y); })
			, clust.end());
		expansions.reserve(clust.size());
		resourcesPer.reserve(clust.size());
		for (const auto & p : clust) {
			expansions.push_back(p.first);
			resourcesPer.push_back(p.second);
		}
	}
	// compute hard points for each mineral line
	for (int i = 0; i < expansions.size(); i++) {
		hardPointsPer.emplace_back(ComputeHardPointsInMinerals(i,initial,query,debug));
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
		distanceSortedBasesPerPlayer.push_back(byDist);
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
	debugMap(debug,initial);
	if (debug != nullptr) debug->SendDebug();
#endif // DEBUG

}

bool MapTopology::hasPockets() const {
	return pocketBases[0] != -1;
}

const sc2::Point3D & MapTopology::getPosition(Player p, BaseType b) const {
	int id = getExpansionIndex(p,b);
	return expansions[id];
}
int MapTopology::getExpansionIndex(Player p, BaseType b) const {
	int id = p == ally ? ourBaseStartLocIndex : (ourBaseStartLocIndex == 0) ? 1 : 0;

	switch (b) {
	case nat: return naturalBases[id];
	case proxy: return proxyBases[id];
	case pocket:
		if (hasPockets()) {
			return pocketBases[id];
		}
		else {
			return naturalBases[id];
		}
	case main:
	default:	return mainBases[id];
	}
}

int MapTopology::FindNearestBaseIndex(const Point2D& start) const {
	float distance = std::numeric_limits<float>::max();
	int targetb = 0;
	int index = 0;
	for (const auto& u : expansions) {
		float d = DistanceSquared2D(u, start);		
		if (d < distance) {
			distance = d;
			targetb = index;
		}
		index++;
	}
	return targetb;
}

int MapTopology::FindNearestBaseIndex(const Point3D& start) const {
	float distance = std::numeric_limits<float>::max();
	int targetb = 0;
	int index = 0;
	for (const auto& u : expansions) {
		float d = DistanceSquared2D(u, start);
		float deltaz = abs(start.z - u.z);
		if (d < distance && deltaz < 1.0f) {
			distance = d;
			targetb = index;
		}
		index++;
	}
	return targetb;
}


const Point3D & MapTopology::FindNearestBase(const Point3D& start) const {
	return expansions[FindNearestBaseIndex(start)];
}

const Unit * MapTopology::FindNearestMineral(const sc2::Point3D & start) const 
{
	auto max = std::numeric_limits<float>::max();
	const Unit * target = nullptr;
	for (auto & v : resourcesPer) {
		for (auto & m : v) {

			if (IsMineral(m->unit_type)) {
				auto d = DistanceSquared2D(m->pos, start);
				if (d < max) {
					target = m;
					max = d;
				}
			}
		}
	}
	return target;
}

const std::vector<sc2::Point2D>& MapTopology::FindHardPointsInMinerals(int expansionIndex) const
{
	return hardPointsPer[expansionIndex];
}

void MapTopology::debugPath(const std::vector<sc2::Point2DI> path, DebugInterface * debug, const ObservationInterface *obs)
{
	if (debug == nullptr) {
		return;
	}
	for (int i = 0, e = path.size(); i < e-1; i++) {
		auto z = obs->TerrainHeight({ path[i].x + 0.5f, path[i].y + 0.5f });
		debug->DebugLineOut(			
			{ path[i].x + 0.5f,path[i].y + 0.5f, z+0.2f },
			{ path[i + 1].x + 0.5f,path[i + 1].y + 0.5f,z + 0.2f }, Colors::Green);
	}
}

void MapTopology::debugMap(DebugInterface * debug, const ObservationInterface * obs) {
	if (debug == nullptr) {
		return;
	}
	int i = 0;
	for (const auto & e : expansions) {
		for (const auto & min : resourcesPer[i]) {
			std::string text = std::to_string(i);
			debug->DebugTextOut(text, sc2::Point3D(min->pos.x, min->pos.y, min->pos.z + 0.8f), Colors::Yellow);
		}
		for (auto & p : hardPointsPer[i]) {
			std::string text = std::to_string(i);
			debug->DebugSphereOut(Point3D(p.x, p.y, expansions[i].z + 0.1f), 0.2, Colors::Red);
			debug->DebugTextOut(text, sc2::Point3D(p.x, p.y, expansions[i].z + 0.8f), Colors::Yellow);
		}
		debug->DebugSphereOut(e + Point3D(0, 0, 0.1f), 2.25f, Colors::Red);
		std::string text = "expo" + std::to_string(i++);
		debug->DebugTextOut(text, sc2::Point3D(e.x, e.y, e.z + 2), Colors::Green);
		
	}
	
	// kinda costly, disabled by default
	if (false) {
		int res = 0;
		for (int x = 0; x < width; x++) {
			for (int y = 0; y < height; y++) {
				if (reserved[y*width + x]) {
					
					// auto z = expansions[FindNearestBaseIndex(Point2D(x, y))].z;
					auto z = obs->TerrainHeight(Point2D(x, y));
					debug->DebugBoxOut(Point3D(x, y, z), Point3D(x + 1, y + 1, z + .3f), Colors::Red);
					res++;
				}
			}
		}
		debug->DebugTextOut("Reserved :" + to_string(res));
	}
	

	for (size_t startloc = 0, max = mainBases.size(); startloc < max; startloc++) {
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
// taken from sc2_search.cc of sc2API
// modified to use Units instead of copying into vector<Unit>
std::vector<std::pair<Point3D, Units > > Cluster(const Units& units, float distance_apart) {
	float squared_distance_apart = distance_apart * distance_apart;
	std::vector<std::pair<Point3D, Units > > clusters;
	for (size_t i = 0, e = units.size(); i < e; ++i) {
		const Unit& u = *units[i];

		float distance = std::numeric_limits<float>::max();
		std::pair<Point3D, Units >* target_cluster = nullptr;
		// Find the cluster this mineral patch is closest to.
		for (auto& cluster : clusters) {
			float d = DistanceSquared3D(u.pos, cluster.first);
			if (d < distance) {
				distance = d;
				target_cluster = &cluster;
			}
		}

		// If the target cluster is some distance away don't use it.
		if (distance > squared_distance_apart) {
			clusters.push_back(std::pair<Point3D, Units >(u.pos, Units{ &u }));
			continue;
		}

		// Otherwise append to that cluster and update it's center of mass.
		target_cluster->second.push_back(&u);
		size_t size = target_cluster->second.size();
		target_cluster->first = ((target_cluster->first * (float(size) - 1)) + u.pos) / float(size);
	}

	return clusters;
}


static bool touching(const Unit * a, const Unit *b) {
	if (a->vespene_contents != 0 && b->vespene_contents != 0) {
		return abs(a->pos.y - b->pos.y) <= 3 && abs(a->pos.x - b->pos.x) <= 3;
	}
	else if (a->vespene_contents != 0 || b->vespene_contents != 0) {
		return abs(a->pos.y - b->pos.y) <= 2 && abs(a->pos.x - b->pos.x) <= 2.5;
	}
	else {
		return abs(a->pos.y - b->pos.y) <= 1 && abs(a->pos.x - b->pos.x) <= 2;
	}
}

std::vector<Point2D> MapTopology::ComputeHardPointsInMinerals(int expansionIndex, const sc2::ObservationInterface * obs, sc2::QueryInterface * query, sc2::DebugInterface * debug)
{
	const sc2::GameInfo & info = obs->GetGameInfo();

	// compute the two closest to each
	auto & mins = resourcesPer[expansionIndex];
	auto mat = computeDistanceMatrix(mins);

	auto sz = mins.size();
	if (sz <= 3) {
		return {};
	}
	std::vector<int> orderedMins;

	// ok now find an edge of the cluster since they are in a line
	// an edge has the property that both it's neighbors are closer to each other than at least one of them to the edge
	for (int i = 0; i < sz; i++) {
		auto neighbors = sortByDistanceTo(mat, i, sz);
		bool edge = true;
		for (int j = 1; j < 6 && j < neighbors.size(); j++) {
			auto dij = mat[i*sz + neighbors[j]];
			for (int k = 1; k < j; k++) {
				auto dkj = mat[neighbors[k] * sz + neighbors[j]];
				if (dij < dkj) {
					edge = false;
					break;
				}
			}
			if (!edge)
				break;
		}
		if (edge) {
			orderedMins.push_back(i);
			break;
		}
		
/*		// am I on the edge of my two closest
		auto d1 = mat[i*sz + neighbors[1]];
		auto d2 = mat[i*sz + neighbors[2]];
		auto d3 = mat[i*sz + neighbors[3]];
		auto d12 = mat[neighbors[1] * sz + neighbors[2]];
		auto d13 = mat[neighbors[1] * sz + neighbors[3]];
		auto d23 = mat[neighbors[2] * sz + neighbors[3]];
		if (d12 < d2 && d23 < d3 && d13 < d3 && d14 < d4) {
			// the distance between them is less than  the distance to one of them.
			orderedMins.push_back(i);
			break;
		}*/
	}
	for (int i = 0; i < sz - 1; i++) {
		auto close = sortByDistanceTo(mat, orderedMins[i], sz);
		for (int j = 1; j < sz; j++) {
			if (std::find(orderedMins.begin(), orderedMins.end(), close[j]) == orderedMins.end()) {
				orderedMins.push_back(close[j]);
				break;
			}
		}
	}
#ifdef DEBUG
	if (true) {
		int ind = 0;
		for (auto m : mins) {
			auto & out = m->pos;
			debug->DebugTextOut(std::to_string(ind++), Point3D(out.x, out.y, out.z + 0.1f));
			debug->DebugTextOut(std::to_string(out.x)+","+to_string(out.y), Point3D(out.x, out.y-0.2f, out.z + 0.1f));
		}
		ind = 0;
		for (auto m : orderedMins) {
			auto & out = mins[m]->pos;
			debug->DebugTextOut(std::to_string(ind++), Point3D(out.x, out.y + 0.2, out.z + 0.1f));
		}
	}
#endif	
	
	// index preceding the hole
	std::vector<int> holes;
	for (int i = 0; i < sz - 1; i++) {
		if (!touching(mins[orderedMins[i]], mins[orderedMins[i + 1]])) {
			holes.push_back(i);
		}
	}

	
	// chosen
	
	std::vector<Point2D> elected;

	for (int hole = 0, hsz = holes.size(); hole < hsz; hole++) {
		// build grid location for 2x2 placement of a building between minerals
		// blocking a hole, but oustide the line.
		std::vector<Point2D> chosen;
		for (int tg = holes[hole]; tg != holes[hole] + 2; tg++) {
			auto & min = mins[orderedMins[tg]];
			// the adjacent possible locations			
			std::vector<Point2D> adj = {
				// right of min
				min->pos + Point2D(1.5f,-2) ,
				min->pos + Point2D(1.5f,-1) ,
				min->pos + Point2D(1.5f,0) ,
				min->pos + Point2D(1.5f,1) ,
				// left of min
				min->pos + Point2D(-2.5f,-2) ,
				min->pos + Point2D(-2.5f,-1) ,
				min->pos + Point2D(-2.5f,0) ,
				min->pos + Point2D(-2.5f,1) ,
				// below
				min->pos + Point2D(-1.5f,-2) ,
				min->pos + Point2D(-.5f,-2) ,
				min->pos + Point2D(0.5f,-2) ,
				// above
				min->pos + Point2D(-1.5f,1) ,
				min->pos + Point2D(-.5f,1) ,
				min->pos + Point2D(0.5f,1) ,
			};
			if (IsVespene(min->unit_type)) {
				auto mpos = min->pos ;
				adj = {
					// right of gas
					mpos + Point2D(2,-2) ,
					mpos + Point2D(2,-1) ,
					mpos + Point2D(2,0) ,
					mpos + Point2D(2,1) ,
					// left of gas
					mpos + Point2D(-3,-2) ,
					mpos + Point2D(-3,-1), 
					mpos + Point2D(-3,0) ,
					mpos + Point2D(-3,1) ,
					// below
					mpos + Point2D(-2,-3),
					mpos + Point2D(-1,-3),
					mpos + Point2D(0,-3),
					mpos + Point2D(1,-3),
					// above
					mpos + Point2D(-2,2) ,
					mpos + Point2D(-1,2) ,
					mpos + Point2D(0,2) ,
					mpos + Point2D(1,2) ,
				};
			}
#ifdef DEBUG
			for (auto & p : adj) {
				debug->DebugSphereOut(Point3D(p.x, p.y, expansions[expansionIndex].z + 0.1f), 0.1, Colors::Blue);
				debug->DebugTextOut(to_string(tg) ,Point3D(p.x, p.y, expansions[expansionIndex].z + 0.1f));
			}
#endif
			auto d = Distance2D(expansions[expansionIndex], min->pos);
			for (auto & p : adj) {
				if (Placement(info, p) && Distance2D(expansions[expansionIndex], p) >= d - .8f) {
					auto it = find(chosen.begin(), chosen.end(), p);
					if (it != chosen.end()) {
						elected.push_back(p);						
					}
					else {
						if (query->Placement(ABILITY_ID::BUILD_PYLON, p)) {
							chosen.push_back(p);
						}
					}
				}
			}
		}
#ifdef DEBUG
		for (auto & p : chosen) {
			debug->DebugSphereOut(Point3D(p.x, p.y, expansions[expansionIndex].z + 0.1f), 0.2, Colors::Green);
		}
		debug->SendDebug();
#endif		
	}
	Units copy;
	for (auto i : orderedMins) {
		copy.emplace_back(mins[i]);
	}
	mins = copy;


#ifdef DEBUG
	/*for (auto & p : chosen) {
			debug->DebugSphereOut(Point3D(p.x, p.y, expansions[expansionIndex].z + 0.1f), 0.1, Colors::Green);
		}*/

	for (auto & p : elected) {
		debug->DebugSphereOut(Point3D(p.x, p.y, expansions[expansionIndex].z + 0.1f), 0.2, Colors::Red);
		debug->DebugTextOut(std::to_string(p.x) + "," + std::to_string(p.y), Point3D(p.x, p.y, expansions[expansionIndex].z + 0.1f));
	}

	debug->SendDebug();
#endif
	
	return elected;
}

// Adapted (patched) with respect to version of sc_search.cc of the sc2api
std::vector<std::pair<Point3D, Units > > MapTopology::CalculateExpansionLocations(const ObservationInterface* observation, QueryInterface* query) {
	
	Units resources = observation->GetUnits(Unit::Alliance::Neutral,
		[](const Unit& unit) {
		return IsVespene(unit.unit_type) || IsMineral(unit.unit_type);
	}
	);


	
	auto clusters = Cluster(resources, 15.0f);
	auto expansion_locations = clusters;

	std::vector<size_t> query_size;
	std::vector<QueryInterface::PlacementQuery> queries;
	for (size_t i = 0; i < clusters.size(); ++i) {
		auto& cluster = clusters[i];

		// Get the required queries for this cluster.
		size_t query_count = 0;
		for (auto r : { 6.4f, 5.3f }) {
			query_count += CalculateQueries(r, 0.5f, cluster.first, queries);
		}

		query_size.push_back(query_count);
	}

	std::vector<bool> results = query->Placement(queries);

	// Edit the results : allow to build in command structure existing location
	Units commandStructures = observation->GetUnits(
		[](const Unit& unit) {
			return IsCommandStructure(unit.unit_type);
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
		auto& cluster = clusters[i];

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
				if (IsVespene(unit->unit_type)) {
					dgas += DistanceSquared2D(p, unit->pos);
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

		Point3D expansion(closest.x, closest.y, (*cluster.second.begin())->pos.z);		

		expansion_locations[i].first = expansion;

		start_index += query_size[i];
	}

	return expansion_locations;
}

bool MapTopology::Placement(const sc2::GameInfo & info, const sc2::Point2D & point) const
{
	return PlacementI(info, sc2::Point2DI((int)point.x, (int)point.y));
}


bool  MapTopology::PlacementI(const sc2::GameInfo & info, const sc2::Point2DI & pointI) const
{
	if (pointI.x < 0 || pointI.x >= width || pointI.y < 0 || pointI.y >= height)
	{
		return false;
	}
	if (reserved[pointI.y*width + pointI.x]) {
		return false;
	}
	unsigned char encodedPlacement = info.placement_grid.data[pointI.x + ((height - 1) - pointI.y) * width];
	bool decodedPlacement = encodedPlacement == 255 ? true : false;
	return decodedPlacement;
}

bool MapTopology::PlacementB(const sc2::GameInfo & info, const sc2::Point2D & point, int footprint) const
{
	auto inzone = sc2::Point2DI((int)point.x, (int)point.y);
	for (int x = 0; x < footprint; x++) {
		for (int y = 0; y < footprint; y++) {
			if (! PlacementI(info, Point2DI(inzone.x + x, inzone.y + y))) {
				return false;
			}
		}
	}
	return true;
}

void MapTopology::reserve(const sc2::Point2D & point)
{
	auto p = sc2::Point2DI((int)point.x, (int)point.y);
	reserved[p.y*width + p.x] = true;
}

void MapTopology::reserveVector(const Point2D & start, const Point2D & vec) {
	auto vnorm = vec;
	auto len = Distance2D(vnorm, { 0,0 });
	vnorm /= len;
	for (float f = 2; f < len; f += .25) {
		auto torem = start + vnorm * f;
		reserve(torem);
		reserve(torem + Point2D(1, 0));
		reserve(torem + Point2D(0, 1));
		reserve(torem + Point2D(-1, 0));
		reserve(torem + Point2D(0, -1));
	}
}

void MapTopology::reserve(int expIndex)
{
	for (auto & r : resourcesPer[expIndex]) {
		auto cc = expansions[expIndex];
		reserveVector(cc, (r->pos - cc));
	}
}

void MapTopology::reserveCliffSensitive(int expIndex, const ObservationInterface * obs)
{	
	auto info = obs->GetGameInfo();
	// scan tiles of the expansion (within 15.0), discard any that are within 5.0 of a cliff
	auto center = expansions[expIndex];
	int hx = center.x;
	int hy = center.y;
	auto h = obs->TerrainHeight(center);
	for (int i = -15; i <= 15; i++) {
		for (int j = -15; j <= 15; j++) {
			auto p = Point2D(hx + i, hy + j);
			if (obs->TerrainHeight(p) > h &&  Pathable(info, p)) {
				auto vec = center - p;
				auto vl = Distance2D(vec, Point2D(0,0));
				vec /= vl;
				vec *= 5;
				reserveVector(p, vec);
			}
		}
	}
}

namespace sc2util {
	// code for these two is taken from 5minBot, by Archiatrus.
	bool Pathable(const sc2::GameInfo & info, const sc2::Point2D & point)
	{
		sc2::Point2DI pointI((int)point.x, (int)point.y);
		return Pathable(info, pointI);
	}
	bool Pathable(const sc2::GameInfo & info, const sc2::Point2DI & pointI) {
		if (pointI.x < 0 || pointI.x >= info.width || pointI.y < 0 || pointI.y >= info.width)
		{
			return false;
		}
		unsigned char encodedPlacement = info.pathing_grid.data[pointI.x + ((info.height - 1) - pointI.y) * info.width];
		bool decodedPlacement = encodedPlacement == 255 ? false : true;
		return decodedPlacement;
	}


}