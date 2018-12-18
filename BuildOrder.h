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
			BUILDING, // being built 
			BUSY, // building something, traveling somewhere
			MINING_VESPENE,
			MINING_MINERALS,
			FREE // true for combat units all the time
		};
		UnitId type;
		UnitState state;
		int time_to_free; // -1 means we stay in state (e.g. mining) until new orders come in
		UnitInstance(UnitId type);
		UnitInstance(UnitId type, UnitState state, int time_to_free) :type(type), state(state), time_to_free(time_to_free) {}
		void print(std::ostream & out) const;
	};
	std::string to_string(const UnitInstance::UnitState & state);
	class GameState {
		std::vector<UnitInstance> units;
		
		float minerals;
		mutable float mps;
		float vespene; 
		mutable float vps;
		int timestamp;

	public :
		GameState(const std::vector<UnitInstance> & units = {}, int minerals = 0, int vespene = 0) : units(units), minerals(minerals), mps(-1.0), vespene(vespene), vps(-1.0), timestamp(0) {}
		const std::vector<UnitInstance> & getUnits() const { return units; }
		bool hasFreeUnit(UnitId unit) const; // must be free; 
		bool hasFinishedUnit(UnitId unit) const; // must be finished; 
		void addUnit(UnitId unit);
		void addUnit(const UnitInstance & unit);
		float getMineralsPerSecond() const;
		float getVespenePerSecond() const;
		float getMinerals() const { return minerals; }
		float & getMinerals() { return minerals; }
		float getVespene() const { return vespene; }
		float & getVespene()  { return vespene; }
		int getAvailableSupply() const;
		int getUsedSupply() const;
		int getMaxSupply() const;
		void stepForward(int secs);
		bool waitForResources(int mins, int vesp, std::pair<int,int> * waited = nullptr);
		bool waitforUnitCompletion(UnitId id);
		bool waitforUnitFree(UnitId id);
		bool waitforFreeSupply(int needed);
		int probesToSaturation() const;
		bool assignProbe(UnitInstance::UnitState state);
		bool assignFreeUnit(UnitId type, UnitInstance::UnitState state, int time);
		int getTimeStamp() const { return timestamp; }
		void print(std::ostream & out) const;
		int countUnit(UnitId id) const;
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
		const GameState & getInitial() const { return initial; }
	};

	enum BuildAction {
		BUILD,
		TRANSFER_VESPENE,
		TRANSFER_MINERALS,
		CHRONO
	};

	class BuildItem {
		BuildAction action;
		UnitId target;
		int time;
	public :		
#ifdef DEBUG
		int timeMin=0, timeVesp=0, timePre=0, timeFree=0, timeFood=0;
		int totalWait() const  { return timeMin + timeVesp + timePre + timeFree + timeFood;}
		void clearTimes() { timeMin =  timeVesp = timePre =  timeFree =  timeFood = 0; }
#endif
		// compute time to completion, from a given game state, assuming that the action is possible
		// make sure this is the case, since this just steps simulation forward
		// updates the game state
		void executeBuildItem(GameState & s);
		void print(std::ostream & out) const;
		
		BuildItem(UnitId id) :action(BUILD), target(id),time(0) {}
		BuildItem(BuildAction action) :action(action), target(UnitId::INVALID), time(0) {}

		BuildAction getAction() const { return action; }
		UnitId getTarget() const { return target; }
		void setTime(int ttime) { time = ttime; }
		int getTime() const { return time; }
		bool operator== (const BuildItem & other) const { return action == other.action && target == other.target ; }		
	};

	class BuildOrder {
		GameState initial;
		std::deque<BuildItem> items;
		
		GameState final;
		GameState current;
		int nextItem;
	public :
		void print(std::ostream & out);
		template<typename T> 
		void addItem(T tocreate)
		{
			items.emplace_back(BuildItem(tocreate));
		}
		template<typename T>
		void addItemFront(T tocreate)
		{
			items.push_front(BuildItem(tocreate));
		}
		template<typename T>
		void insertItem(T tocreate, int index)
		{
			items.insert(items.begin() + index, BuildItem(tocreate));
		}
		const std::deque<BuildItem> & getItems() const { return items; }
		std::deque<BuildItem> & getItems() { return items; }
		GameState & getFinal() { return final; }
		const GameState & getFinal() const { return final; }
		void removeItem(int i) { items.erase(items.begin() + i); }
		void swapItems(int i, int j) { std::swap(items[i], items[j]); }
	};

	class BuildGoal {
		// using index of Unit to get desired quantity
		std::vector<int> desiredPerUnit;
		int framesToCompletion; // 0 indicates ASAP
	public :
		BuildGoal(int deadline);
		void addUnit(UnitId id, int qty);
		void print(std::ostream & out) const;
		int getQty(int index) const { return desiredPerUnit[index]; }
	};

}