#pragma once

#include "sc2api/sc2_interfaces.h"
#include "sc2api/sc2_client.h"
#include <unordered_map>



class HarvesterStrategy
{
	enum HarvestState {
		MovingToMineral, //in-transit to mineral-crystal
		GatheringMineral, //actively mining the crystal with ability
		ReturningMineral, //returning cargo to the nearest TownHall
	};
	enum MovementState {
		Entering, // we just go the trigger to move
		Accelerating, // we have smart clicked once
		Accelerated, // we have smart clicked twice
		Coasting, // yeah babe ! we are mineral walking here
		Approaching, // start clicking in front to avoid slow down
		Reached // we got close enough to trigger final action
	};
	enum OverlapState {
		Overlapping, // we are mineral walking, and overlapping a buddy. no tricks like becoming material again.
		Distinct // ok to target ground, nothing bad should happen
	};
	struct WorkerState {
		HarvestState harvest;
		MovementState move;
		OverlapState overlap;
	};
	const sc2::Unit * nexus;
	const sc2::Units minerals;
	std::unordered_map<sc2::Tag, sc2::Tag> workerAssignedMinerals; //for each worker, which crystal are they assigned?
	std::unordered_map<sc2::Tag, sc2::Point2D> magicSpots; //for each crystal, where is the magic spot?
	std::unordered_map<sc2::Tag, WorkerState> workerStates; //what each worker is doing ('task', not 'job')

	sc2::Point2D calcMagicSpot(const sc2::Unit * mineral);
	// update and cleanup workerStates, create & init states for new hires, cleanup guys that got fired
	// i.e. maintenance operations on workerStates & workerAssignedMinerals
	void updateRoster(const sc2::Units & current);
public:
	
	int getIdealHarvesters();
	int getCurrentHarvesters();

	HarvesterStrategy(const sc2::Unit * nexus, const sc2::Units & minerals);
	void OnStep(const sc2::Units & workers);
	void OnUnitDestroyed(const sc2::Unit *dead);

#ifdef DEBUG
	long totalMined; // total trips successfully done
	std::vector<int> roundtrips; // accumulated probe timings
	std::unordered_map<sc2::Tag, int> lastReturnedFrame; // timing per probe

	void PrintDebug(sc2::DebugInterface * debug);
#endif
};

