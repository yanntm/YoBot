#pragma once


#include "sc2api/sc2_interfaces.h"
#include "sc2api/sc2_agent.h"
#include "sc2api/sc2_map_info.h"
#include "sc2lib/sc2_lib.h"

#include "MapTopology.h"
#include <deque>

#define DllExport   __declspec( dllexport )  


struct Order {
	const sc2::Unit * subject;
	sc2::ABILITY_ID ability;
	const sc2::Unit * targetU;
	sc2::Point2D targetP;
	enum { SELF, POS, UNIT } tag;
	
	Order(const sc2::Unit * subject, sc2::ABILITY_ID ability) : subject(subject), ability(ability), targetU(nullptr),tag(SELF) {};
	Order(const sc2::Unit * subject, sc2::ABILITY_ID ability, const sc2::Unit * target) :subject(subject), ability(ability), targetU(target),tag(UNIT) {}
	Order(const sc2::Unit * subject, sc2::ABILITY_ID ability, const sc2::Point2D & target) :subject(subject), ability(ability), targetU(nullptr),targetP(target),tag(POS) {}
};

// A Yo Agent, is an agent that already has some nice stuff it knows about world and game state
// that is not generally available in API
class YoAgent : public sc2::Agent{
public : 
	virtual ~YoAgent() = default;

	void initialize() {
		map.init(Observation(), Query(), Debug());
	}
	// the onStep function from Agent, sets up additional knowledge gained, then call OnYoStep, then call DispatchOrders
	virtual void OnStep() final;
	// the step function for concrete YoAgents
	virtual void OnYoStep() = 0;



	//! Issues a command to a unit. Self targeting.
	//!< \param unit The unit to send the command to.
	//!< \param ability The ability id of the command.
	void UnitCommand(const sc2::Unit* unit, sc2::AbilityID ability) { orders.push_back(Order(unit,ability)); }

	//! Issues a command to a unit. Targets a point.
	//!< \param unit The unit to send the command to.
	//!< \param ability The ability id of the command.
	//!< \param point The 2D world position to target.
	void UnitCommand(const sc2::Unit* unit, sc2::AbilityID ability, const sc2::Point2D& point) { orders.push_back(Order(unit, ability,point)); }

	//! Issues a command to a unit. Targets another unit.
	//!< \param unit The unit to send the command to.
	//!< \param ability The ability id of the command.
	//!< \param target The unit that is a target of the unit getting the command.
	void UnitCommand(const sc2::Unit* unit, sc2::AbilityID ability, const sc2::Unit* target) { orders.push_back(Order(unit, ability, target)); }
protected :
	MapTopology map;
	std::deque<Order> orders;
	int minerals = 0;
	int gas = 0;
	int supplyleft = 0;
private :
	void updateFromObservation(const sc2::ObservationInterface *obs);
	void dispatchOrders();
};

