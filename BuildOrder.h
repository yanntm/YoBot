#pragma once

#include <string>
#include <vector>

namespace suboo {

	enum UnitId {
		NEXUS,
		PROBE,
		PYLON,
		GATEWAY,
		ZEALOT,
		NULLID
	};

	class Unit {
		UnitId type;
		std::string name;
		int mineral_cost;
		int vespene_cost;		

		// provides/negative = needs
		int food_provided;

		// prerequisites
		std::vector<UnitId> prereq;

		// building producing the unit
		UnitId production_tag;

		// time to produce the unit
		int production_time;

		// estimated time to/from construction site or 0
		int travel_time;

		enum producer_end {
			BUSY, // producer is busy during the full given production time (e.g. terran scv, production building)
			TRAVEL, // producer gets a 4 second BUSY downtime (travel time/cast time) only (e.g. toss probe, larvae spawn)
			CONSUME // producer takes 4 seconds BUSY, then build consumes the producer (e.g. zerg drone, move worker to/from gas action)
		};
	};

	class UnitInstance {
		UnitId type;
		enum UnitState {
			BUSY, // being built, building something, traveling somewhere
			MINING_VESPENE,
			MINING_GAS,
			FREE // true for combat units all the time
		};
		int time_to_free; // -1 means we stay in state (e.g. mining) until new orders come in
	};

	class GameState {
		std::vector<UnitInstance> state;
		int vespene;
		int minerals;
		int timestamp;
	};

	class TechTree {
		std::vector<Unit> units;
		GameState initial;
	};

	enum BuildAction {
		BUILD_NEXUS,
		BUILD_PROBE,
		BUILD_PYLON,
		BUILD_GATEWAY,
		BUILD_ZEALOT,
		MINE_VESPENE,
		MINE_MINERALS,
		CHRONO
	};

	class BuildItem {
		BuildAction action;
		UnitId target;

		// from a given GameState, the set of actions necessary to make this action possible
		// derived from prerequisites of the build item + game state
		// waiting for gas/gold to accumulate is ok
		// answer is empty if waiting is all we need to do or we can do the action immediately.
		std::vector<BuildAction> makePossible(const GameState & s);
		// compute time to completion, from a given game state, assuming that the action is possible
		// make sure this is the case by calling above make Possible first, since this just steps simulation forward
		// updates the game state
		void executeBuildItem(GameState & s);
	};

	class BuildOrder {
		GameState initial;
		std::vector<BuildItem> items;

		std::vector<int> timings;
		GameState final;
		GameState current;
		int nextItem;


	};

}