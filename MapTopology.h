#pragma once

#include <vector>
#include "sc2api/sc2_map_info.h"
#include "sc2api/sc2_agent.h"

class MapTopology
{
	
	// map "topology" consists in tagging the expansions as being one of the following
	std::vector<int> mainBases;
	std::vector<int> pocketBases;
	std::vector<int> naturalBases;
	std::vector<int> proxyBases;
	// each base has a staging area, an open ground to group units before attacking
	std::vector<sc2::Point2D> stagingFor;	
	int ourBaseStartLocIndex;
public:
	// raw cache for locations computed for expansions
	std::vector<sc2::Point3D> expansions;
	// a main is a starting location
	// a nat is the place you would put your second CC normally
	// a proxy is an expo that is not immediately obvious to defender, but not too far
	// a pocket occurs when a base is reachable only by passing through your main
	enum BaseType {main,nat,proxy,pocket};
	enum Player {ally, enemy};
	// note asking for pocket will yield nat if no pocket found
	const sc2::Point3D & getPosition(Player p, BaseType b) const;
	// true iff we found pocket  bases
	bool hasPockets() const;
	// looking for a base ?
	const sc2::Point3D & FindNearestBase(const sc2::Point3D& start) const;
	// call this at game start to build up the info
	void init(const sc2::ObservationInterface * initial, sc2::QueryInterface * query, sc2::DebugInterface * debug=nullptr);
	// call this to see what the topology thinks in debug mode
	void debugMap(sc2::DebugInterface * debug);
};

