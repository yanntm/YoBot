#pragma once


#include "sc2api/sc2_interfaces.h"
#include "sc2api/sc2_agent.h"
#include "sc2api/sc2_map_info.h"
#include "sc2lib/sc2_lib.h"

#include "MapTopology.h"

#define DllExport   __declspec( dllexport )  

using namespace sc2;

// A Yo Agent, is an agent that already has some nice stuff it knows about world and game state
// that is not generally available in API
class YoAgent : public Agent {
public : 
	void initialize() {
		map.init(Observation(), Query(), Debug());
	}
protected :
	MapTopology map;
};

