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
		bool had_mineral;
	};
	const sc2::Unit * nexus;
	sc2::Units minerals;
	sc2::Units allminerals;
	std::unordered_map<sc2::Tag, int> workerAssignedMinerals; //for each worker, which crystal are they assigned?
	std::unordered_map<sc2::Tag, sc2::Point2D> magicSpots; //for each crystal, where is the magic spot?
	std::unordered_map<sc2::Tag, sc2::Point2D> magicNexusSpots; //for each crystal, where is the magic spot?
	std::unordered_map<sc2::Tag, WorkerState> workerStates; //what each worker is doing ('task', not 'job')

	// a spot close to mineral, good for accelerating to
	static sc2::Point2D calcMagicSpot(const sc2::Unit * mineral, const sc2::Unit* nexus);
	// a spot close to nexus, good for accelerating to
	static sc2::Point2D calcNexusMagicSpot(const sc2::Unit* mineral, const sc2::Unit* nexus, const sc2::GameInfo & info);
	// update and cleanup workerStates, create & init states for new hires, cleanup guys that got fired
	// i.e. maintenance operations on workerStates & workerAssignedMinerals
	void updateRoster(const sc2::Units & current);
	// compute a distribution/pairing of workers to minerals
	void assignTargets(const sc2::Units & workers);
	std::vector<int> allocateTargets(const sc2::Units & probes, const sc2::Units & mins, std::function<int(const sc2::Unit *)>&toAlloc, std::unordered_map<sc2::Tag, int> current = {});
public:
	
	int getIdealHarvesters();
	int getCurrentHarvesters();

	void initialize(const sc2::Unit * nexus, const sc2::Units & minerals, const sc2::ObservationInterface * obs);
	
	void OnStep(const sc2::Units & workers, sc2::ActionInterface * actions, bool inDanger = false);

#ifdef DEBUG
	long frame = 0;
	long totalMined = 0; // total trips successfully done
	std::vector<int> roundtrips; // accumulated probe timings
	std::unordered_map<sc2::Tag, int> lastReturnedFrame; // timing per probe
	std::unordered_map<sc2::Tag, int> lastTripTime; // timing per probe
	void PrintDebug(sc2::DebugInterface * debug, const sc2::ObservationInterface * obs);
#endif
};

