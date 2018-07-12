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
		auto list = Observation()->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::PROTOSS_NEXUS));
		nexus = list.front();


		const Unit* mineral_target = FindNearestMineralPatch(nexus->pos);
		Actions()->UnitCommand(nexus, ABILITY_ID::RALLY_NEXUS, mineral_target);
		Actions()->UnitCommand(nexus, ABILITY_ID::TRAIN_PROBE);

		auto probes = Observation()->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::PROTOSS_PROBE));
		bob = probes.front();

		expansions = sc2::search::CalculateExpansionLocations(observation, Query());

		// Determine choke and proxy locations
		const GameInfo& game_info = Observation()->GetGameInfo();
		const auto & starts = game_info.enemy_start_locations;
		if (starts.size() > 1) {
			proxy = cog(starts);
			auto scout = *(++Observation()->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::PROTOSS_PROBE)).begin());
			for (auto loc : starts) {
				Actions()->UnitCommand(scout, ABILITY_ID::SMART, loc, true);
			}
			target = proxy;
		}
		else {
			target = game_info.enemy_start_locations.front();
			proxy = (.67 * target + .33*nexus->pos);
			//proxy = FindFarthestBase(nexus->pos,target);
			proxy = FindNearestBase(proxy);
		}



		//if (game_info.)
		choke = (.2 * target + .8*nexus->pos);

		Actions()->UnitCommand(bob, ABILITY_ID::SMART, proxy);
	}


	virtual void OnUnitHasAttacked(const Unit* unit) final {

		if (unit->unit_type == UNIT_TYPEID::PROTOSS_ZEALOT) {
			auto targets = FindEnemiesInRange(unit->pos, 100);

			Units weak;
			Units drones;
			Units pylons;
			for (auto t : targets) {
				if ((t->health + t->shield) / (t->health_max + t->shield_max) < 0.7) {
					weak.push_back(t);
				}
				if (t->unit_type == UNIT_TYPEID::PROTOSS_PROBE || t->unit_type == UNIT_TYPEID::TERRAN_SCV || t->unit_type == UNIT_TYPEID::ZERG_DRONE) {
					drones.push_back(t);
				}
				if (t->unit_type == UNIT_TYPEID::TERRAN_SUPPLYDEPOT || t->unit_type == UNIT_TYPEID::TERRAN_SUPPLYDEPOTLOWERED || t->unit_type == UNIT_TYPEID::PROTOSS_PYLON) {
					pylons.push_back(t);
				}
			}
			const Unit * enemy = nullptr;
			if (!weak.empty()) {
				enemy = chooseClosest(unit, weak);
				//std::cout << "Chose a weak enemy" << std::endl;
			}
			else if (unit->shield > 15) {
				if (!drones.empty()) {
					enemy = chooseClosest(unit, drones);
					//std::cout << "Chose a drone enemy" << std::endl;
				}
				else if (!pylons.empty()) {
					enemy = chooseClosest(unit, pylons);
					//std::cout << "Chose a pylon enemy" << std::endl;
				}
			}
			if (enemy != nullptr) {
				Actions()->UnitCommand(unit, ABILITY_ID::ATTACK_ATTACK, enemy);
			}
			/*		if (unit->orders.size() != 0) {
			auto order = unit->orders.front();
			if (order.ability_id == ABILITY_ID::ATTACK_ATTACK) {
			Actions()->UnitCommand(unit, ABILITY_ID::MOVE, order.target_pos);

			}
			}*/
			//std::cout << "on CD " << unit->tag << " " << unit->weapon_cooldown << std::endl;
		}
	}

	virtual void OnUnitReadyAttack(const Unit* unit) final {
		if (unit->unit_type == UNIT_TYPEID::PROTOSS_ZEALOT && unit->orders.size() != 0) {
			auto order = unit->orders.front();
			//Actions()->UnitCommand(unit, ABILITY_ID::ATTACK_ATTACK, order.target_pos);
			//std::cout << "off CD " << unit->tag << std::endl;
		}
	}
	//void OnError 
	virtual void OnError(const std::vector<ClientError>& client_errors, const std::vector<std::string>& protocol_errors) {
		for (auto ce : client_errors) {
			std::cout << (int)ce << std::endl;
		}
		for (auto ce : protocol_errors) {
			std::cout << ce << std::endl;
		}
	}
	virtual void OnUnitAttacked(const Unit* unit) final {
		if (unit->unit_type == UNIT_TYPEID::PROTOSS_PROBE && unit->alliance == Unit::Alliance::Self && unit != bob) {
			auto list = Observation()->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::PROTOSS_PROBE));
			int att = 0;
			for (auto unit : list) {
				for (auto order : unit->orders) {
					if (order.ability_id == ABILITY_ID::ATTACK_ATTACK) {
						att++;
					}
				}
			}
			if (att < 2) {
				if (bob != nullptr) list.erase(std::remove(list.begin(), list.end(), bob), list.end());
				auto reaper = FindNearestEnemy(Observation()->GetStartLocation());
				list.resize(3);
				if (reaper != nullptr) {
					if (reaper->unit_type == UNIT_TYPEID::TERRAN_SCV) {
						list.resize(2);
						Actions()->UnitCommand(nexus, ABILITY_ID::TRAIN_PROBE, true);
					}
					Actions()->UnitCommand(list, ABILITY_ID::ATTACK_ATTACK, reaper->pos);
				}
			}
			if (unit->shield == 0 && nexus != nullptr)
				Actions()->UnitCommand(unit, ABILITY_ID::HARVEST_GATHER, FindNearestMineralPatch(nexus->pos));
		}
		else {
			if (unit->unit_type == UNIT_TYPEID::PROTOSS_ZEALOT) {
				if (unit->shield >= 20 && unit->shield < 25) {
					auto nmy = FindNearestEnemy(unit->pos);
					auto vec = unit->pos - nmy->pos;

					Actions()->UnitCommand(unit, ABILITY_ID::MOVE, unit->pos + (vec / 2));
					//std::cout << "ouch run away" << std::endl;
				}
				else {
					Actions()->UnitCommand(unit, ABILITY_ID::ATTACK_ATTACK, unit->pos);
				}
			}
		}
	}

	virtual void OnUnitEnterVision(const Unit* u) final {
		if (target == proxy && u->alliance == Unit::Alliance::Enemy && u->health_max >= 200 && !u->is_flying) {
			auto pottarget = FindNearestBase(u->pos);
			if (Distance2D(pottarget, target) < 20) {
				target = pottarget;
			}
			else {
				target = u->pos;
			}
		}
	}

	virtual void OnUnitDestroyed(const Unit* unit) final {
		if (bob == unit) {
			bob = nullptr;
			for (auto probe : Observation()->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::PROTOSS_PROBE))) {
				if (probe->is_alive) {
					bob = probe;
					break;
				}
			}
		}
		else if (nexus == unit) {
			nexus = nullptr;
		}
		else if (unit->unit_type == UNIT_TYPEID::PROTOSS_PROBE && unit->alliance == Unit::Alliance::Self) {
			auto list = Observation()->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::PROTOSS_PROBE));
			list.erase(std::remove(list.begin(), list.end(), bob), list.end());
			auto reaper = FindNearestEnemy(Observation()->GetStartLocation());
			list.resize(3);
			if (reaper != nullptr) {
				if (reaper->unit_type == UNIT_TYPEID::TERRAN_SCV) {
					list.resize(2);
					Actions()->UnitCommand(nexus, ABILITY_ID::TRAIN_PROBE, true);
				}
				Actions()->UnitCommand(list, ABILITY_ID::ATTACK_ATTACK, reaper->pos);
			}
		}
	}

	const Unit* FindNearestEnemy(const Point2D& start) {

		Units units = Observation()->GetUnits(Unit::Alliance::Enemy);
		float distance = std::numeric_limits<float>::max();
		const Unit* targete = nullptr;
		for (const auto& u : units) {
			//if (u->unit_type == UNIT_TYPEID::) {
			float d = DistanceSquared2D(u->pos, start);
			if (d < distance) {
				distance = d;
				targete = u;
			}
			//}
		}
		if (distance <= 200.0f) {
			return targete;
		}
		return nullptr;
	}

	const Point2D FindFarthestBase(const Point2D& start, const Point2D& start2) {
		float distance = 0;
		Point2D targetb;
		auto min = Observation()->GetGameInfo().playable_min;
		auto max = Observation()->GetGameInfo().playable_max;
		for (const auto& u : expansions) {
			if (u.x > min.x && u.x < max.x && u.y > min.y && u.y < max.y) {
				float d = DistanceSquared2D(u, start) * DistanceSquared2D(u, start2);
				if (d > distance) {
					distance = d;
					targetb = u;
				}
			}
		}
		return targetb;
	}

	const Point2D cog(const std::vector<Point2D>& points) {
		if (points.size() == 0) {
			return (Observation()->GetGameInfo().playable_min + Observation()->GetGameInfo().playable_max) / 2;
		}

		Point2D targetc = points.front();
		int div = 1;
		for (auto it = ++points.begin(); it != points.end(); ++it) {
			targetc += *it;
			div++;
		}
		targetc /= div;
		return targetc;
	}

	const Point2D FindNearestBase(const Point2D& start) {
		float distance = std::numeric_limits<float>::max();
		Point2D targetb;
		for (const auto& u : expansions) {
			float d = DistanceSquared2D(u, start);
			if (d < distance) {
				distance = d;
				targetb = u;
			}
		}
		return targetb;
	}
	int minerals = 0;
	int supplyleft = 0;

	virtual void OnStep() final {
		//		std::cout << Observation()->GetGameLoop() << std::endl;
		//		std::cout << Observation()->GetMinerals() << std::endl;
		/*	bool doit = false;
		if (bob != nullptr && !bob->orders.size() == 0) {
		for (const auto& order : bob->orders) {
		if (order.ability_id == ABILITY_ID::PATROL) {
		doit = true;
		break;
		}
		}
		} else {
		doit = true;
		}*/

		minerals = Observation()->GetMinerals();
		supplyleft = Observation()->GetFoodCap() - Observation()->GetFoodUsed();

		if (minerals >= 100 && Observation()->GetFoodCap() == 15 && Observation()->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::PROTOSS_PYLON)).empty()) {
			const Unit* min = FindNearestMineralPatch(nexus->pos);
			auto ptarg = nexus->pos + (nexus->pos - min->pos);
			auto probes = Observation()->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::PROTOSS_PROBE));
			int i = 0;
			for (auto p : probes) {
				if (i++ < 3)
					continue;
				if (p != bob) {
					Actions()->UnitCommand(p, ABILITY_ID::BUILD_PYLON, ptarg);
					break;
				}
			}
		}

		//if (doit) {
		TryBuildSupplyDepot();
		TryBuildBarracks();
		//}
		TryBuildUnits();
		if (Observation()->GetArmyCount() >= 7) {
			const GameInfo& game_info = Observation()->GetGameInfo();
			for (const auto & unit : Observation()->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::PROTOSS_ZEALOT))) {
				if (unit->orders.size() == 0) {
					if (Distance2D(unit->pos, target) < 15.0f) {
						target = proxy;
						auto list = Observation()->GetUnits(Unit::Alliance::Enemy);
						if (list.size() != 0) {
							int targetU = GetRandomInteger(0, list.size() - 1);
							if (!list[targetU]->is_flying) {
								Actions()->UnitCommand(unit, ABILITY_ID::ATTACK_ATTACK, list[targetU]->pos);
								continue;
							}
						}
						int targetU = GetRandomInteger(0, expansions.size() - 1);
						Actions()->UnitCommand(unit, ABILITY_ID::ATTACK_ATTACK, expansions[targetU]);
					}
					else {
						Actions()->UnitCommand(unit, ABILITY_ID::ATTACK_ATTACK, target);
					}
				}
			}
		}
		if (proxy == target) {
			for (const auto & u : Observation()->GetUnits(Unit::Alliance::Enemy)) {
				if (u->health_max >= 200) {
					OnUnitEnterVision(u);
					break;
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
			if (unit->assigned_harvesters < unit->ideal_harvesters)
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
				Actions()->UnitCommand(unit, ABILITY_ID::ATTACK_ATTACK, target);
			}
			break;
		}
		case UNIT_TYPEID::PROTOSS_PROBE: {
			if (nexus != nullptr) {
				const Unit* mineral_target = FindNearestMineralPatch(nexus->pos);
				if (!mineral_target) {
					break;
				}
				Actions()->UnitCommand(unit, ABILITY_ID::SMART, mineral_target);
			}
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
	Point2D target;
	const Unit * bob = nullptr;
	std::vector<Point3D> expansions;
	const Unit * nexus = nullptr;

	size_t CountUnitType(UNIT_TYPEID unit_type) {
		return Observation()->GetUnits(Unit::Alliance::Self, IsUnit(unit_type)).size();
	}

	void TryBuildUnits() {
		if (supplyleft >= 2 && minerals >= 100) {
			for (auto & gw : Observation()->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::PROTOSS_GATEWAY))) {
				if (gw->build_progress < 1) {
					continue;
				}
				if (gw->orders.size() == 0) {
					Actions()->UnitCommand(gw, ABILITY_ID::TRAIN_ZEALOT);
					supplyleft -= 2;
					minerals -= 100;
					bool isChronoed = false;
					for (auto buff : gw->buffs) {
						if (buff == sc2::BUFF_ID::TIMEWARPPRODUCTION) {
							isChronoed = true;
							break;
						}
					}
					if (!isChronoed) {
						auto nexl = Observation()->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::PROTOSS_NEXUS));
						if (nexl.size() != 0) {
							auto nexi = nexl.front();
							if (nexi->energy >= 50) {
								Actions()->UnitCommand(nexi, (ABILITY_ID)3755, gw);
							}
						}
					}
				}
				if (supplyleft >= 2 && minerals >= 100) {
					continue;
				}
				else {
					break;
				}
			}
		}
	}

	void smartAttack() {
		const GameInfo& game_info = Observation()->GetGameInfo();
		auto potTargets = expansions;
		for (const auto & unit : Observation()->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::PROTOSS_ZEALOT))) {
			if (Distance2D(unit->pos, target) < 15.0f) {
				if (proxy != target) {
					target = proxy;
				}
			}
			else {
				Actions()->UnitCommand(unit, ABILITY_ID::ATTACK_ATTACK, target);
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
		if (minerals < 100) {
			return false;
		}
		if (CountUnitType(UNIT_TYPEID::PROTOSS_PYLON) <= 1) {
			minerals -= 100;
			Actions()->UnitCommand(bob,
				ABILITY_ID::BUILD_PYLON,
				proxy);
			return true;
		}

		// If we are not supply capped, don't build a supply depot.
		if (observation->GetFoodUsed() < 20 && supplyleft > 2)
			return false;
		if (observation->GetFoodUsed() >= 20 && supplyleft > 6)
			return false;

		Units units = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::PROTOSS_PYLON));
		for (const auto& unit : units) {
			if (unit->build_progress < 1) {
				return false;
			}
		}



		// Try and build a depot. Find a random SCV and give it the order.
		const Unit* unit_to_build = bob;
		if (bob == nullptr) {
			return false;
		}
		for (const auto& order : bob->orders) {
			if (order.ability_id == ABILITY_ID::BUILD_PYLON) {
				return false;
			}
		}
		float rx = GetRandomScalar();
		float ry = GetRandomScalar();

		if (false && CountUnitType(UNIT_TYPEID::PROTOSS_PYLON) == 0) {
			minerals -= 100;
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
				auto candidate = Point2D(proxy.x + rx * (8.0f + gws.size() * 2), proxy.y + ry * (8.0f + gws.size() * 2));
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
					if (spaces >= 3 && Query()->PathingDistance(bob, candidate) < 20) {
						good = true;
					}
					else {
						candidate = Point2D(proxy.x + rx * 10.0f, proxy.y + ry * 10.0f);
					}
				}

				if (good) {
					minerals -= 100;
					Actions()->UnitCommand(unit_to_build,
						ABILITY_ID::BUILD_PYLON, candidate);
					return true;
				}
				else {
					return false;
				}

			}
			else {
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
				while (!good && iter++ < 10) {

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
					}
					else {
						candidate = Point2D(proxy.x + rx * 15.0f, proxy.y + ry * 15.0f);
					}
				}
				if (good) {
					minerals -= 100;
					Actions()->UnitCommand(unit_to_build,
						ABILITY_ID::BUILD_PYLON, candidate);
					return true;
				}
			}

			minerals -= 100;
			Actions()->UnitCommand(unit_to_build,
				ABILITY_ID::BUILD_PYLON,
				Point2D(proxy.x + rx * 8.0f, proxy.y + ry * 8.0f));

		}

		return true;
	}

	bool TryBuildBarracks() {


		if (CountUnitType(UNIT_TYPEID::PROTOSS_PYLON) < 1) {
			return false;
		}

		if (CountUnitType(UNIT_TYPEID::PROTOSS_GATEWAY) >= 4) {
			return false;
		}


		// If a unit already is building a supply structure of this type, do nothing.
		// Also get an scv to build the structure.

		if (bob == nullptr) {
			return false;
		}

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

	const Units FindEnemiesInRange(const Point2D& start, double radiusSquared) {
		Units units = Observation()->GetUnits(Unit::Alliance::Enemy, [start, radiusSquared](const Unit& unit) {
			return !unit.is_flying && DistanceSquared2D(unit.pos, start)< radiusSquared; });
		return units;
	}

	const Unit * chooseClosest(const Unit * unit, const Units & pot) {
		std::vector <sc2::QueryInterface::PathingQuery> queries;

		for (auto u : pot) {
			queries.push_back({ unit->tag, unit->pos, u->pos });
		}

		std::vector<float> distances = Query()->PathingDistance(queries);
		float distance = std::numeric_limits<float>::max();
		int ti = 0;
		for (int i = 0; i < distances.size(); i++) {
			if (distance > distances[i]) {
				distance = distances[i];
				ti = i;
			}
		}
		return pot[ti];
	}

	const Unit* FindNearestMineralPatch(const Point2D& start) {
		Units units = Observation()->GetUnits(Unit::Alliance::Neutral, [](const Unit& unit) {
			return unit.unit_type == UNIT_TYPEID::NEUTRAL_MINERALFIELD || unit.unit_type == UNIT_TYPEID::NEUTRAL_MINERALFIELD750 ||
				unit.unit_type == UNIT_TYPEID::NEUTRAL_RICHMINERALFIELD || unit.unit_type == UNIT_TYPEID::NEUTRAL_RICHMINERALFIELD750 ||
				unit.unit_type == UNIT_TYPEID::NEUTRAL_PURIFIERMINERALFIELD || unit.unit_type == UNIT_TYPEID::NEUTRAL_PURIFIERMINERALFIELD750 ||
				unit.unit_type == UNIT_TYPEID::NEUTRAL_PURIFIERRICHMINERALFIELD || unit.unit_type == UNIT_TYPEID::NEUTRAL_PURIFIERRICHMINERALFIELD750 ||
				unit.unit_type == UNIT_TYPEID::NEUTRAL_LABMINERALFIELD || unit.unit_type == UNIT_TYPEID::NEUTRAL_LABMINERALFIELD750 ||
				unit.unit_type == UNIT_TYPEID::NEUTRAL_BATTLESTATIONMINERALFIELD || unit.unit_type == UNIT_TYPEID::NEUTRAL_BATTLESTATIONMINERALFIELD750; });
		float distance = std::numeric_limits<float>::max();
		const Unit* targetm = nullptr;
		for (const auto& u : units) {
			float d = DistanceSquared2D(u->pos, start);
			if (d < distance) {
				distance = d;
				targetm = u;
			}
		}
		return targetm;
	}
};
