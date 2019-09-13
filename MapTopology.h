#pragma once

#include <vector>
#include "sc2api/sc2_map_info.h"
#include "sc2api/sc2_agent.h"
#include "sc2api/sc2_unit.h"


class MapTopology
{
	
	// map "topology" consists in tagging the expansions as being one of the following
	std::vector<int> mainBases;
	std::vector<int> pocketBases;
	std::vector<int> naturalBases;
	std::vector<int> proxyBases;
	
	

	std::vector<std::vector<sc2::Point2D>> hardPointsPer;
	// each base has a staging area, an open ground to group units before attacking
	std::vector<sc2::Point2D> stagingFor;	
	
	// Calculates expansion locations, this call can take on the order of 100ms since it makes blocking queries to SC2 so call it once and cache the reults.
	// it is modified from original provided by sc2API
	static std::vector<std::pair<sc2::Point3D, sc2::Units > > CalculateExpansionLocations(const sc2::ObservationInterface* observation, sc2::QueryInterface* query);
	// compute hard points in mineral lines
	std::vector<sc2::Point2D> ComputeHardPointsInMinerals(int expansionIndex, const sc2::ObservationInterface * obs, sc2::QueryInterface * query, sc2::DebugInterface * debug);	
	int width;
	int height;
public:
	int ourBaseStartLocIndex;
	// raw cache for locations computed for expansions, indexed by expansion index
	std::vector<sc2::Point3D> expansions;
	std::vector<sc2::Units> resourcesPer;	
	std::vector< std::vector<int> > distanceSortedBasesPerPlayer;
	// a main is a starting location
	// a nat is the place you would put your second CC normally
	// a proxy is an expo that is not immediately obvious to defender, but not too far
	// a pocket occurs when a base is reachable only by passing through your main
	enum BaseType {main,nat,proxy,pocket};
	enum Player {ally, enemy};
	// note asking for pocket will yield nat if no pocket found
	const sc2::Point3D & getPosition(Player p, BaseType b) const;
	int getExpansionIndex(Player p, BaseType b) const;	
	// true iff we found pocket  bases
	bool hasPockets() const;
	// looking for a base ?
	int FindNearestBaseIndex(const sc2::Point3D& start) const;
	int FindNearestBaseIndex(const sc2::Point2D& start) const;
	const sc2::Point3D & FindNearestBase(const sc2::Point3D& start) const;
	// looking for resources ?
	const sc2::Unit * FindNearestMineral(const sc2::Point3D& start) const;
	const std::vector<sc2::Point2D> & FindHardPointsInMinerals(int expansionIndex) const;
	// call this at game start to build up the info
	void init(const sc2::ObservationInterface * initial, sc2::QueryInterface * query, sc2::DebugInterface * debug=nullptr);
#ifdef DEBUG
	// call this to see what the topology thinks in debug mode
	void debugMap(sc2::DebugInterface * debug, const sc2::ObservationInterface * obs);
	void debugPath(const std::vector<sc2::Point2DI> path, sc2::DebugInterface * debug,const sc2::ObservationInterface *obs);
#endif
};
