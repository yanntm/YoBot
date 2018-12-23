#pragma once


#include "sc2api/sc2_interfaces.h"
#include "sc2api/sc2_agent.h"
#include "sc2api/sc2_map_info.h"
#include "sc2lib/sc2_lib.h"

#include "MapTopology.h"
#include "Placement.h"
#include <deque>
#include <unordered_map>

#include "YoAction.h"

#define DllExport   __declspec( dllexport )  


struct Order {
	const sc2::Unit * subject;
	sc2::ABILITY_ID ability;
	const sc2::Unit * targetU;
	sc2::Point2D targetP;
	enum { SELF, POS, UNIT } tag;
	// priorities currently : 0 = VITAL, 1 = EVADE/COMBAT, 2 = NORMAL, 3 = BACKGROUND 
	// in case of conflict lowest priority order for a unit goes through
	int priority;
	
	Order(const sc2::Unit * subject, sc2::ABILITY_ID ability, int priority=2) : subject(subject), ability(ability), targetU(nullptr),tag(SELF),priority(priority){};
	Order(const sc2::Unit * subject, sc2::ABILITY_ID ability, const sc2::Unit * target, int priority = 2) :subject(subject), ability(ability), targetU(target),tag(UNIT), priority(priority) {}
	Order(const sc2::Unit * subject, sc2::ABILITY_ID ability, const sc2::Point2D & target, int priority = 2) :subject(subject), ability(ability), targetU(nullptr),targetP(target),tag(POS), priority(priority) {}
};

// A Yo Agent, is an agent that already has some nice stuff it knows about world and game state
// that is not generally available in API
class YoAgent : public sc2::Agent{
	sc2::YoAction * actions;
public : 
	virtual ~YoAgent() { delete actions; }

	void initialize() {
		actions = new sc2::YoAction(sc2::Agent::Actions());
		map.init(Observation(), Query(), Debug());
		placer.init(Observation(), Query(), &map, Debug());
	}
	sc2::ActionInterface * Actions() {
		return actions;
	}
	sc2::YoAction * YoActions() {
		return actions;
	}

	// the onStep function from Agent, sets up additional knowledge gained, then call OnYoStep, then call DispatchOrders
	virtual void OnStep() final;
	// the step function for concrete YoAgents
	virtual void OnYoStep() = 0;

	virtual void OnUnitEnterVision(const sc2::Unit* u);
	virtual void OnUnitDestroyed(const sc2::Unit* unit);

	//! Issues a command to a unit. Self targeting.
	//!< \param unit The unit to send the command to.
	//!< \param ability The ability id of the command.
	void UnitCommand(const sc2::Unit* unit, sc2::AbilityID ability, int p = 2) { orders.push_back(Order(unit,ability,p)); }

	//! Issues a command to a unit. Targets a point.
	//!< \param unit The unit to send the command to.
	//!< \param ability The ability id of the command.
	//!< \param point The 2D world position to target.
	void UnitCommand(const sc2::Unit* unit, sc2::AbilityID ability, const sc2::Point2D& point, int p = 2) { orders.push_back(Order(unit, ability,point,p)); }

	//! Issues a command to a unit. Targets another unit.
	//!< \param unit The unit to send the command to.
	//!< \param ability The ability id of the command.
	//!< \param target The unit that is a target of the unit getting the command.
	void UnitCommand(const sc2::Unit* unit, sc2::AbilityID ability, const sc2::Unit* target, int p=2) { orders.push_back(Order(unit, ability, target,p)); }

	typedef std::unordered_map<sc2::Tag, const sc2::Unit *> UnitsMap;
	const UnitsMap & allEnemies() const;
protected :
	MapTopology map;
	BuildingPlacer placer;
	std::deque<Order> orders;
	int minerals = 0;
	int gas = 0;
	int supplyleft = 0;
	
private :
	std::unordered_map<sc2::Tag, const sc2::Unit *> enemies;
	void updateFromObservation(const sc2::ObservationInterface *obs);
	void dispatchOrders();
};

