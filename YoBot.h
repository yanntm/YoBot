#pragma once
#include "sc2api/sc2_interfaces.h"
#include "sc2api/sc2_agent.h"
#include "sc2api/sc2_map_info.h"
#include "sc2lib/sc2_lib.h"

//#include "Strategys.h"
#define DllExport   __declspec( dllexport )  
using namespace sc2;

class YoBot : public Agent {
public:
	virtual void OnGameStart() final {
		std::cout << "YoBot will kill you!" << std::endl;
		const ObservationInterface* observation = Observation();

		nexus = nullptr;
		for (const Unit * unit : observation->GetUnits()) {
			switch (unit->unit_type.ToType()) {
			case UNIT_TYPEID::PROTOSS_NEXUS: {
				const Unit* mineral_target = FindNearestMineralPatch(unit->pos);
				if (!mineral_target) {
					break;
				}
				Actions()->UnitCommand(unit, ABILITY_ID::RALLY_NEXUS, mineral_target);
				Actions()->UnitCommand(unit, ABILITY_ID::TRAIN_PROBE);
				nexus = unit;
				break;
			}
			case UNIT_TYPEID::PROTOSS_PROBE: {
				if (bob == nullptr) {
					bob = unit;
				}
				break;
			}
			}
		}

		expansions = sc2::search::CalculateExpansionLocations(observation, Query());

		// Determine choke and proxy locations
		const GameInfo& game_info = Observation()->GetGameInfo();
		Point2D target = game_info.enemy_start_locations.front();

		proxy = (.67 * target + .33*nexus->pos);
		proxy = FindNearestBase(proxy);

		//if (game_info.)
		choke = (.2 * target + .8*nexus->pos);

		Actions()->UnitCommand(bob, ABILITY_ID::SMART, proxy);
	}

	virtual void OnUnitDestroyed(const Unit* unit) final {
		if (bob == unit) {
			bob = Observation()->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::PROTOSS_PROBE)).front();
		} else if (unit->unit_type == UNIT_TYPEID::PROTOSS_PROBE && unit->alliance == Unit::Alliance::Self) {
			auto list = Observation()->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::PROTOSS_PROBE));
			list.erase(std::remove(list.begin(), list.end(), bob), list.end()); 
			auto reaper = FindNearestEnemy(unit->pos);
			list.resize(10);
			if (reaper != nullptr) 
				Actions()->UnitCommand(list, ABILITY_ID::ATTACK_ATTACK, reaper->pos);
		}
	}

	const Unit* FindNearestEnemy(const Point2D& start) {
		Units units = Observation()->GetUnits(Unit::Alliance::Enemy);
		float distance = std::numeric_limits<float>::max();
		const Unit* target = nullptr;
		for (const auto& u : units) {
			//if (u->unit_type == UNIT_TYPEID::) {
				float d = DistanceSquared2D(u->pos, nexus->pos);
				if (d < distance ) {
					distance = d;
					target = u;
				}
			//}
		}
		if (distance <= 200.0f) {
			return target;
		}
		return nullptr;
	}

	const Point2D FindNearestBase(const Point2D& start) {
		Units units = Observation()->GetUnits(Unit::Alliance::Enemy);
		float distance = std::numeric_limits<float>::max();
		Point2D target;
		for (const auto& u : expansions	) {
			//if (u->unit_type == UNIT_TYPEID::) {
			float d = DistanceSquared2D(u, start);
			if (d < distance) {
				distance = d;
				target = u;
			}
			//}
		}
		return target;
	}

	virtual void OnStep() final {
		//		std::cout << Observation()->GetGameLoop() << std::endl;
		//		std::cout << Observation()->GetMinerals() << std::endl;
		bool doit = false;
		if (!bob->orders.size() == 0) {
			for (const auto& order : bob->orders) {
				if (order.ability_id == ABILITY_ID::PATROL) {
					doit = true;
					break;
				}
			}
		} else {
			doit = true;
		}

		if (doit) {
			TryBuildSupplyDepot();
			TryBuildBarracks();
		}
		TryBuildUnits();
		if (Observation()->GetArmyCount() >= 7) {
			const GameInfo& game_info = Observation()->GetGameInfo();
			for (const auto & unit : Observation()->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::PROTOSS_ZEALOT))) {
				if (unit->orders.size() == 0) {
					if (Distance2D(unit->pos, game_info.enemy_start_locations.front()) < 15.0f) {
						auto list = Observation()->GetUnits(Unit::Alliance::Enemy);
						if (list.size() != 0) {
							int target = GetRandomInteger(0, list.size()-1);
							if (!list[target]->is_flying) {
								Actions()->UnitCommand(unit, ABILITY_ID::ATTACK_ATTACK, list[target]->pos);
								continue;
							}
						}
						
						int target = GetRandomInteger(0, expansions.size() - 1);
						Actions()->UnitCommand(unit, ABILITY_ID::ATTACK_ATTACK, expansions[target]);
					}
					else {
						Actions()->UnitCommand(unit, ABILITY_ID::ATTACK_ATTACK, game_info.enemy_start_locations.front());
					}
				}
			}
		}

	}


	// In your bot class.
	virtual void OnUnitIdle(const Unit* unit) final {
		if (unit == bob) {
			Actions()->UnitCommand(unit, ABILITY_ID::PATROL, proxy);
			return;
		}
		switch (unit->unit_type.ToType()) {
		case UNIT_TYPEID::PROTOSS_NEXUS: {
			if (Observation()->GetFoodWorkers() <= 16)
				Actions()->UnitCommand(unit, ABILITY_ID::TRAIN_PROBE);			
			break;
		}
		case UNIT_TYPEID::PROTOSS_GATEWAY: {
			//			if (Observation()->GetFoodCap() >= Observation()->GetFoodUsed() - 2)
			//				Actions()->UnitCommand(unit, ABILITY_ID::TRAIN_ZEALOT);
			break;
		}
		case UNIT_TYPEID::PROTOSS_ZEALOT: {
			if (Observation()->GetArmyCount() >= 10) {
				const GameInfo& game_info = Observation()->GetGameInfo();
				Actions()->UnitCommand(unit, ABILITY_ID::ATTACK_ATTACK, game_info.enemy_start_locations.front());
			}
			break;
		}
		case UNIT_TYPEID::PROTOSS_PROBE: {
			const Unit* mineral_target = FindNearestMineralPatch(nexus->pos);
			if (!mineral_target) {
				break;
			}
			Actions()->UnitCommand(unit, ABILITY_ID::SMART, mineral_target);
			break;
		}
		default: {
			break;
		}
		}
	}
private:
	Point2D choke;
	Point2D proxy;
	const Unit * bob = nullptr;
	std::vector<Point3D> expansions;
	const Unit * nexus = nullptr;

	size_t CountUnitType(UNIT_TYPEID unit_type) {
		return Observation()->GetUnits(Unit::Alliance::Self, IsUnit(unit_type)).size();
	}

	void TryBuildUnits() {		
		if (Observation()->GetFoodCap() >= Observation()->GetFoodUsed() - 2 && Observation()->GetMinerals() >= 100) {
			for (auto & gw : Observation()->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::PROTOSS_GATEWAY))) {
				if (gw->build_progress < 1) {
					continue;
				}
				if (gw->orders.size() == 0) {
					Actions()->UnitCommand(gw, ABILITY_ID::TRAIN_ZEALOT);
					
					auto nexl = Observation()->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::PROTOSS_NEXUS));
					if (nexl.size() != 0) {
						auto nexi = nexl.front();
						if (nexi->energy >= 50) {
							Actions()->UnitCommand(nexi, (ABILITY_ID) 3755, gw);
						}
					}
					break;
				}				
			}
		}
	}

	void smartAttack() {
		const GameInfo& game_info = Observation()->GetGameInfo();
		auto potTargets = expansions;
		for (const auto & unit : Observation()->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::PROTOSS_ZEALOT))) {
			if (Distance2D(unit->pos, game_info.enemy_start_locations.front()) < 15.0f) {

			}
			else {
				Actions()->UnitCommand(unit, ABILITY_ID::ATTACK_ATTACK, game_info.enemy_start_locations.front());
			}
		}
	}

	bool TryBuildStructure(ABILITY_ID ability_type_for_structure, UNIT_TYPEID unit_type = UNIT_TYPEID::PROTOSS_PROBE) {
		const ObservationInterface* observation = Observation();

		// If a unit already is building a supply structure of this type, do nothing.
		// Also get an scv to build the structure.
		const Unit* unit_to_build = nullptr;
		Units units = observation->GetUnits(Unit::Alliance::Self, IsUnit(unit_type));
		for (const auto& unit : units) {
			for (const auto& order : unit->orders) {
				if (order.ability_id == ability_type_for_structure) {
					return false;
				}
			}

			if (unit->unit_type == unit_type) {
				unit_to_build = unit;
			}
		}
		if (unit_to_build == nullptr) {
			return false;
		}
		float rx = GetRandomScalar();
		float ry = GetRandomScalar();


		Actions()->UnitCommand(unit_to_build,
			ability_type_for_structure,
			Point2D(unit_to_build->pos.x + rx * 10.0f, unit_to_build->pos.y + ry * 10.0f));

		return true;
	}

	bool TryBuildSupplyDepot() {
		const ObservationInterface* observation = Observation();

		// If we are not supply capped, don't build a supply depot.
		if (observation->GetFoodUsed() < 20 && observation->GetFoodUsed() <= observation->GetFoodCap() - 2)
			return false;
		if (observation->GetFoodUsed() >= 20 && observation->GetFoodUsed() <= observation->GetFoodCap() - 6)
			return false;

		Units units = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::PROTOSS_PYLON));
		for (const auto& unit : units) {
			if (unit->build_progress < 1) {
				return false;
			}
		}

		// Try and build a depot. Find a random SCV and give it the order.
		const Unit* unit_to_build = bob;
		for (const auto& order : bob->orders) {
			if (order.ability_id == ABILITY_ID::BUILD_PYLON) {
				return false;
			}
		}
		float rx = GetRandomScalar();
		float ry = GetRandomScalar();

		if (false && CountUnitType(UNIT_TYPEID::PROTOSS_PYLON) == 0) {
			Actions()->UnitCommand(unit_to_build,
				ABILITY_ID::BUILD_PYLON,
				Point2D(unit_to_build->pos.x + rx * 5.0f, unit_to_build->pos.y + ry * 5.0f));
		}
		else {
			Units gws = Observation()->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::PROTOSS_GATEWAY));
			if (gws.size() != 0) {
				for (auto & gw : gws) {
					if (!gw->is_powered) {
						Actions()->UnitCommand(unit_to_build,
							ABILITY_ID::BUILD_PYLON,
							Point2D(gw->pos.x + rx * 3.0f, gw->pos.y + ry * 3.0f));
						return true;
						break;
					}
				}
				bool good = false;
				int iter = 0;
				auto candidate = Point2D(proxy.x + rx * (8.0f+ gws.size()*2), proxy.y + ry * (8.0f+ gws.size() * 2));
				while (!good && iter++ < 10) {

					std::vector<sc2::QueryInterface::PlacementQuery> queries;
					queries.reserve(5);
					queries.push_back(sc2::QueryInterface::PlacementQuery(ABILITY_ID::BUILD_PYLON, candidate));
					queries.push_back(sc2::QueryInterface::PlacementQuery(ABILITY_ID::BUILD_PYLON, candidate + sc2::Point2D(1, 0)));
					queries.push_back(sc2::QueryInterface::PlacementQuery(ABILITY_ID::BUILD_PYLON, candidate + sc2::Point2D(0, 1)));
					queries.push_back(sc2::QueryInterface::PlacementQuery(ABILITY_ID::BUILD_PYLON, candidate + sc2::Point2D(-1, 0)));
					queries.push_back(sc2::QueryInterface::PlacementQuery(ABILITY_ID::BUILD_PYLON, candidate + sc2::Point2D(0, -1)));

					auto q = Query();
					auto resp = q->Placement(queries);

					int spaces = 0;
					if (resp[0]) {
						for (auto & b : resp) {
							if (b) {
								spaces++;
							}
						}
					}
					if (spaces >= 3) {
						good = true;
					}
					else {
						candidate = Point2D(proxy.x + rx * 10.0f, proxy.y + ry * 10.0f);
					}
				}
				if (good) {
					Actions()->UnitCommand(unit_to_build,
						ABILITY_ID::BUILD_PYLON, candidate);
					return true;
				}
				else {
					return false;
				}

			} else {
				// make sure our first pylon has plenty of space around it
				Point2D candidate;
				if (true) {
					candidate = proxy;
					Actions()->UnitCommand(unit_to_build,
						ABILITY_ID::BUILD_PYLON, candidate);
					return true;
				}
				candidate = Point2D(proxy.x + rx * 15.0f, proxy.y + ry * 15.0f);
				bool good = false;
				int iter = 0;
				while (!good && iter++ < 10){
					
					std::vector<sc2::QueryInterface::PlacementQuery> queries;
					queries.reserve(5);
					queries.push_back(sc2::QueryInterface::PlacementQuery(ABILITY_ID::BUILD_GATEWAY, candidate));
					queries.push_back(sc2::QueryInterface::PlacementQuery(ABILITY_ID::BUILD_GATEWAY, candidate + sc2::Point2D(2, 0)));
					queries.push_back(sc2::QueryInterface::PlacementQuery(ABILITY_ID::BUILD_GATEWAY, candidate + sc2::Point2D(0, 2)));
					queries.push_back(sc2::QueryInterface::PlacementQuery(ABILITY_ID::BUILD_GATEWAY, candidate + sc2::Point2D(-2, 0)));
					queries.push_back(sc2::QueryInterface::PlacementQuery(ABILITY_ID::BUILD_GATEWAY, candidate + sc2::Point2D(0, -2)));

					auto q = Query();
					auto resp = q->Placement(queries);

					int spaces = 0;
					if (resp[0]) {
						for (auto & b : resp) {
							if (b) {
								spaces++;
							}
						}
					}
					if (spaces >= 3) {
						good = true;
					} else {
						candidate = Point2D(proxy.x + rx * 15.0f, proxy.y + ry * 15.0f); 						
					}
				}
				if (good) {
					Actions()->UnitCommand(unit_to_build,
						ABILITY_ID::BUILD_PYLON, candidate);
					return true;
				}
			}


			Actions()->UnitCommand(unit_to_build,
				ABILITY_ID::BUILD_PYLON,
				Point2D(proxy.x + rx * 8.0f, proxy.y + ry * 8.0f));

		}
		return true;
	}

	bool TryBuildBarracks() {
		const ObservationInterface* observation = Observation();

		if (CountUnitType(UNIT_TYPEID::PROTOSS_PYLON) < 1) {
			return false;
		}

		if (CountUnitType(UNIT_TYPEID::PROTOSS_GATEWAY) >= 5) {
			return false;
		}


		// If a unit already is building a supply structure of this type, do nothing.
		// Also get an scv to build the structure.


		for (const auto& order : bob->orders) {
			if (order.ability_id == ABILITY_ID::BUILD_GATEWAY) {
				return false;
			}
		}

		float rx = GetRandomScalar();
		float ry = GetRandomScalar();


		Point2D candidate = Point2D(bob->pos.x + rx * 15.0f, bob->pos.y + ry * 15.0f);
		std::vector<sc2::QueryInterface::PlacementQuery> queries;
		queries.reserve(5);
		queries.push_back(sc2::QueryInterface::PlacementQuery(ABILITY_ID::BUILD_GATEWAY, candidate));
		queries.push_back(sc2::QueryInterface::PlacementQuery(ABILITY_ID::BUILD_GATEWAY, candidate + sc2::Point2D(1, 0)));
		queries.push_back(sc2::QueryInterface::PlacementQuery(ABILITY_ID::BUILD_GATEWAY, candidate + sc2::Point2D(0, 1)));
		queries.push_back(sc2::QueryInterface::PlacementQuery(ABILITY_ID::BUILD_GATEWAY, candidate + sc2::Point2D(-1, 0)));
		queries.push_back(sc2::QueryInterface::PlacementQuery(ABILITY_ID::BUILD_GATEWAY, candidate + sc2::Point2D(0, -1)));

		auto q = Query();
		auto resp = q->Placement(queries);
		
		int spaces = 0;
		if (resp[0]) {
			for (auto & b : resp) {
				if (b) {
					spaces++;
				}
			}
		}

		if (spaces > 2)
		Actions()->UnitCommand(bob,
			ABILITY_ID::BUILD_GATEWAY,
			candidate);

		return true;
	}

	const Unit* FindNearestMineralPatch(const Point2D& start) {
		Units units = Observation()->GetUnits(Unit::Alliance::Neutral);
		float distance = std::numeric_limits<float>::max();
		const Unit* target = nullptr;
		for (const auto& u : units) {
			if (u->unit_type == UNIT_TYPEID::NEUTRAL_MINERALFIELD) {
				float d = DistanceSquared2D(u->pos, start);
				if (d < distance) {
					distance = d;
					target = u;
				}
			}
		}
		return target;
	}
};
