#pragma once

#include "sc2api/sc2_interfaces.h"
#include "YoAction.h"

class CombatStrategy
{
	enum CombatState {
		Regrouping, // regrouping with other close by units
		Attacking, // all in, go for it
		Skirmishing, // do damage if not threatened/ high shields
		Cornering // pursuing an enemy
	};

	sc2::Units squad;
	sc2::Point2D target;
	sc2::Point2D rally;
	std::unordered_map<sc2::Tag, CombatState> soldierStates; //what each soldier is doing 
	// update and cleanup workerStates, create & init states for new hires, cleanup guys that got fired
	// i.e. maintenance operations on combatStates
	bool updateRoster(const sc2::Units & current);
public :
	void initialize(const sc2::ObservationInterface * obs);
	void OnStep(const sc2::ObservationInterface * obs, sc2::YoAction * actions);	
#ifdef DEBUG
	void PrintDebug(sc2::DebugInterface * debug, const sc2::ObservationInterface * obs) const;
#endif
};