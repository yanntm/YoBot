#pragma once

#include <string>
#include <vector>
#include "sc2api/sc2_unit.h"

namespace suboo {

	using UnitId = sc2::UNIT_TYPEID ;

	struct Unit {
		int index;
		UnitId type;
		std::string name;
		int mineral_cost;
		int vespene_cost;		

		// provides/negative = needs
		int food_provided;

		// building/unit producing the unit
		UnitId builder;

		// prerequisites
		UnitId prereq;

		// time to produce the unit
		int production_time;

		// estimated time to/from construction site or 0
		int travel_time;

		enum BuilderEffect {
			BUSY, // producer is busy during the full given production time (e.g. terran scv, production building)
			TRAVEL, // producer gets a travel_time second BUSY downtime (travel time/cast time) only (e.g. toss probe, larvae spawn)
			CONSUME // producer takes a travel_time seconds BUSY, then build consumes the producer (e.g. zerg drone, move worker to/from gas action)
		};
		BuilderEffect effect;

		Unit(int index, UnitId type, const std::string & name, int mineral_cost, int vespene_cost, int food_provided, UnitId builder, UnitId prereq, int production_time, int travel_time, BuilderEffect effect):
			index(index),type(type),name(name), mineral_cost(mineral_cost), vespene_cost(vespene_cost),food_provided(food_provided),builder(builder),prereq(prereq),production_time(production_time),effect(effect) 
		{}
	};

	struct UnitInstance {
		enum UnitState {
			BUSY, // being built, building something, traveling somewhere
			MINING_VESPENE,
			MINING_GAS,
			FREE // true for combat units all the time
		};
		UnitId type;
		UnitState state;
		int time_to_free; // -1 means we stay in state (e.g. mining) until new orders come in
		UnitInstance(UnitId type) : type(type),state(FREE),time_to_free(0) {}		
	};

	class GameState {
		std::vector<UnitInstance> units;
		
		int minerals;
		int vespene; 
		int timestamp;
	public :
		GameState(const std::vector<UnitInstance> & units = {}, int minerals=0, int vespene=0) : units(units), minerals(minerals), vespene(vespene), timestamp(0) {}
		const std::vector<UnitInstance> & getUnits() const { return units; }
	};

	class TechTree {
		std::vector<Unit> units;
		GameState initial;
		std::vector<int> indexes; // correspondance from UnitId to unit index
		TechTree();
	public:
		// singleton
		static const TechTree & getTechTree();
		int getUnitIndex(UnitId id) const;
		const Unit & getUnitById(UnitId id) const;
		const Unit & getUnitByIndex(int index) const;
		TechTree(const TechTree &) = delete;
		size_t size() const { return indexes.size(); }
	};

	enum BuildAction {
		BUILD,
		TRANSFER_VESPENE,
		CHRONO
	};

	class BuildItem {
		BuildAction action;
		UnitId target;
	public :
		// from a given GameState, the set of actions necessary to make this action possible
		// derived from prerequisites of the build item + game state
		// waiting for gas/gold to accumulate is ok
		// answer is empty if waiting is all we need to do or we can do the action immediately.
		std::vector<BuildAction> makePossible(const GameState & s);
		// compute time to completion, from a given game state, assuming that the action is possible
		// make sure this is the case by calling above make Possible first, since this just steps simulation forward
		// updates the game state
		void executeBuildItem(GameState & s);
		void print(std::ostream & out);
		
		BuildItem(UnitId id) :action(BUILD), target(id) {}
	};

	class BuildOrder {
		GameState initial;
		std::vector<BuildItem> items;

		std::vector<int> timings;
		GameState final;
		GameState current;
		int nextItem;
	public :
		void print(std::ostream & out);
		void addItem(UnitId tocreate);
	};

	class BuildGoal {
		// using index of Unit to get desired quantity
		std::vector<int> desiredPerUnit;
		int framesToCompletion; // 0 indicates ASAP
	public :
		BuildGoal(int deadline);
		void addUnit(UnitId id, int qty);
		void print(std::ostream & out);
		int getQty(int index) const { return desiredPerUnit[index]; }
	};

}