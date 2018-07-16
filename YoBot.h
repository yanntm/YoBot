#pragma once
#include "sc2api/sc2_interfaces.h"
#include "sc2api/sc2_agent.h"
#include "sc2api/sc2_map_info.h"
#include "sc2lib/sc2_lib.h"

#include <valarray>

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
		
		TrySpreadProbes();

		const GameInfo& game_info = Observation()->GetGameInfo();
		const auto & starts = game_info.start_locations;
		{
			expansions = sc2::search::CalculateExpansionLocations(observation, Query());
			// sometimes we get bad stuff out of here
			auto min = Observation()->GetGameInfo().playable_min;
			auto max = Observation()->GetGameInfo().playable_max;
			remove_if(expansions.begin(), expansions.end(),[min, max](const Point3D & u) { return !(u.x > min.x && u.x < max.x && u.y > min.y && u.y < max.y); });

		}

		// Determine choke and proxy locations
		computeMapTopology(starts, expansions);

		auto myStart = Observation()->GetStartLocation();
		for (int i = 0; i < starts.size(); i++) {
			if (DistanceSquared2D(starts[i], myStart) < 5.0f) {
				ourBaseStartLocIndex = i;
			}
		}
		ourBaseExpansionIndex = mainBases[ourBaseStartLocIndex];


		if (game_info.enemy_start_locations.size() > 1) {
			proxy = cog(game_info.enemy_start_locations);
			scout = *(++Observation()->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::PROTOSS_PROBE)).begin());
			// return minerals
			Actions()->UnitCommand(scout, ABILITY_ID::SMART, nexus, true);
			Actions()->UnitCommand(scout, ABILITY_ID::SMART, game_info.enemy_start_locations[0], true);
			scouted = 0;
			target = proxy;
		}
		else {
			target = game_info.enemy_start_locations.front();

			if (ourBaseStartLocIndex == 0) {
				proxy = expansions[proxyBases[1]];
			}
			else {
				proxy = expansions[proxyBases[0]];
			}

			//proxy = (.67 * target + .33*nexus->pos);
			//proxy = FindFarthestBase(nexus->pos,target);
			//proxy = FindNearestBase(proxy);
		}

		baseRazed = false;

		//if (game_info.)
		choke = (.2f * target + .8f*nexus->pos);

		

		Actions()->UnitCommand(bob, ABILITY_ID::SMART, proxy);


#ifdef DEBUG
		{
			int i = 0;
			for (const auto & e : expansions) {
				Debug()->DebugSphereOut(e + Point3D(0, 0, 0.1f), 2.25f, Colors::Red);
				std::string text = "expo" + std::to_string(i++);
				Debug()->DebugTextOut(text, sc2::Point3D(e.x, e.y, e.z + 2), Colors::Green);
			}

			for (int startloc = 0; startloc < starts.size(); startloc++) {
				Debug()->DebugTextOut("main" + std::to_string(startloc), expansions[mainBases[startloc]] + Point3D(0, 2, .5), Colors::Green);
				Debug()->DebugTextOut("nat" + std::to_string(startloc), expansions[naturalBases[startloc]] + Point3D(0, 2, .5), Colors::Green);
				if (pocketBases[startloc] != -1) Debug()->DebugTextOut("pocket" + std::to_string(startloc), expansions[pocketBases[startloc]] + Point3D(0, 2, .5), Colors::Green);
				Debug()->DebugTextOut("proxy" + std::to_string(startloc), expansions[proxyBases[startloc]] + Point3D(0, 2, .5), Colors::Green);
			}

			Debug()->SendDebug();
		}
#endif // DEBUG
	}


	virtual void OnUnitHasAttacked(const Unit* unit) final {


		if (unit->unit_type == UNIT_TYPEID::PROTOSS_ZEALOT) {
			auto targets = FindEnemiesInRange(unit->pos, 10);

			bool isToss = true;
			for (auto r : Observation()->GetGameInfo().player_info) {
				if (r.race_requested != Protoss) {
					isToss = false;
					break;
				}
			}

			Units weak;
			Units drones;
			Units pylons;

			for (auto t : targets) {
				if (t->unit_type == UNIT_TYPEID::ZERG_EGG || t->unit_type == UNIT_TYPEID::ZERG_BROODLING || t->unit_type == UNIT_TYPEID::ZERG_LARVA) {
					continue;
				}
				if ((t->health + t->shield) / (t->health_max + t->shield_max) < 0.7  &&  t->build_progress == 1.0f  &&  (!isToss || t->is_powered )) {
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
		else if (unit->unit_type == UNIT_TYPEID::PROTOSS_PROBE) {
			OnUnitIdle(unit);

/*			if (unit->orders.size() > 1 && unit->orders[1].ability_id != ABILITY_ID::ATTACK) {
				// this means we just got out of a opportunistic attack
				sendUnitCommand(unit, unit->orders[1]);
			}
			else {
				
			}*/
			//OnUnitAttacked(unit);
			
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
			auto close = FindEnemiesInRange(nexus->pos, 5);
			if (att < 2*close.size() && att <= list.size() *.75f ) {
				if (bob != nullptr) list.erase(std::remove(list.begin(), list.end(), bob), list.end());
				auto targets = FindEnemiesInRange(nexus->pos, 6);

				//auto reaper = FindNearestEnemy(Observation()->GetStartLocation());
				auto reaper = FindWeakestUnit(targets);
				float sum = 0;
				for (const auto & p : list) {
					sum += p->health + p->shield;
				}
				sum /= list.size();
				// the probes with less than half of average life are pulled
				std::remove_if(list.begin(), list.end(), [sum](const Unit * p) { return p->health + p->shield <= sum / 2 ; });
				sortByDistanceTo(list, reaper->pos);
				std::sort(list.begin(), list.end(), [](const Unit * a, const Unit *b) { return a->health + a->shield > b->health + b->shield; });

				if (list.size() > 3) 
					list.resize(3);
				if (reaper != nullptr) {
					if (close.size() == 1 && reaper->unit_type == UNIT_TYPEID::TERRAN_SCV  || reaper->unit_type == UNIT_TYPEID::ZERG_DRONE || reaper->unit_type == UNIT_TYPEID::PROTOSS_PROBE) {
						list.resize(1);
						//Actions()->UnitCommand(nexus, ABILITY_ID::TRAIN_PROBE, true);
					}
					Actions()->UnitCommand(list, ABILITY_ID::ATTACK_ATTACK, reaper->pos);
				}
			}
			if (unit->shield == 0 && nexus != nullptr) {
				const auto & nmy = FindNearestUnit(unit->pos, close);
				if (nmy != nullptr) {
					auto v = unit->pos - nmy->pos;
					v /= Distance2D(Point2D(0, 0), v);
					v *= 3.0f;
					Actions()->UnitCommand(unit, ABILITY_ID::SMART, FindNearestMineralPatch(unit->pos +v));
				}
				else {
					Actions()->UnitCommand(unit, ABILITY_ID::SMART, FindNearestMineralPatch(nexus->pos ));
				}
			}
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
				for (auto friendly : FindFriendliesInRange(unit->pos,4)) {
					OnUnitIdle(friendly);
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
			if (scout != nullptr) {
				OnUnitIdle(scout);
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
		else if (unit->alliance == Unit::Alliance::Enemy && IsCommandStructure( unit->unit_type) ) {
			if (Distance2D(target, unit->pos) < 20) {
				baseRazed = true;
			}
		}
	}

	const Unit* FindWeakestUnit(const Units & units) {
		float distance = std::numeric_limits<float>::max();
		const Unit* targete = nullptr;
		for (const auto& u : units) {
			
			float d = u->health + u->shield;
			if (d < distance ) {
				distance = d;
				targete = u;
			}
			
		}
		return targete;
	}

	// default max is quite large, it amounts to roughly 15 game unit i.e. a  circle roughly the surface of a base
	const Unit* FindNearestUnit(const Point2D& start, const Units & units, float maxRangeSquared=200.0f) {		
		float distance = std::numeric_limits<float>::max();
		const Unit* targete = nullptr;
		for (const auto& u : units) {
			//if (u->unit_type == UNIT_TYPEID::) {
			float d = DistanceSquared2D(u->pos, start);
			if (d < distance && d <= maxRangeSquared) {
				distance = d;
				targete = u;
			}
			//}
		}
		if (distance <= maxRangeSquared) {
			return targete;
		}
		return nullptr;
	}

	const Unit* FindNearestEnemy(const Point2D& start) {
		return FindNearestUnit(start, Observation()->GetUnits(Unit::Alliance::Enemy));		
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
		float div = 1;
		for (auto it = ++points.begin(); it != points.end(); ++it) {
			targetc += *it;
			div++;
		}
		targetc /= div;
		return targetc;
	}

	const Point3D FindNearestBase(const Point3D& start) {
		float distance = std::numeric_limits<float>::max();
		Point3D targetb;
		for (const auto& u : expansions) {
			float d = DistanceSquared2D(u, start);
			float deltaz = abs(start.z - u.z);
			if (d < distance && deltaz < 1.0f) {
				distance = d;
				targetb = u;
			}
		}
		return targetb;
	}
	int minerals = 0;
	int supplyleft = 0;

	virtual void OnStep() final {


#ifdef DEBUG
		{
			int i = 0;
			for (const auto & e : expansions) {
				Debug()->DebugSphereOut(e + Point3D(0, 0, 0.1f), 2.25f, Colors::Red);
				std::string text = "expo" + std::to_string(i++);
				Debug()->DebugTextOut(text, sc2::Point3D(e.x, e.y, e.z + 2), Colors::Green);
			}

			for (int startloc = 0; startloc < Observation()->GetGameInfo().start_locations.size(); startloc++) {
				Debug()->DebugTextOut("main" + std::to_string(startloc), expansions[mainBases[startloc]] + Point3D(0, 2, .5), Colors::Green);
				Debug()->DebugTextOut("nat" + std::to_string(startloc), expansions[naturalBases[startloc]] + Point3D(0, 2, .5), Colors::Green);
				if (pocketBases[startloc] != -1) Debug()->DebugTextOut("pocket" + std::to_string(startloc), expansions[pocketBases[startloc]] + Point3D(0, 2, .5), Colors::Green);
				Debug()->DebugTextOut("proxy" + std::to_string(startloc), expansions[proxyBases[startloc]] + Point3D(0, 2, .5), Colors::Green);
			}

			for (const auto & u : Observation()->GetUnits(Unit::Alliance::Self)) {
				if (! u->orders.empty()) {
					const auto & o = u->orders.front();
					if (o.target_unit_tag != 0) {
						auto pu = Observation()->GetUnit(o.target_unit_tag);
						if (pu != nullptr) {
							auto c = (pu->alliance == Unit::Alliance::Enemy) ? Colors::Red : Colors::Blue;
							Debug()->DebugLineOut(u->pos + Point3D(0, 0, 0.2f), pu->pos + Point3D(0, 0, 0.2f), c);
						}
					}
					else if (o.target_pos.x != 0 && o.target_pos.y != 0) {
						Debug()->DebugLineOut(u->pos + Point3D(0, 0, 0.2f), Point3D(o.target_pos.x, o.target_pos.y, 15.0f), Colors::Green);
					}
				}				
			}
			for (const auto & u : Observation()->GetUnits()) {
				/*
				if (IsMineral(u->unit_type)) {
					Point3D min = u->pos + Point3D(0, 0, 0.2f);
					min -= Point3D(u->radius, u->radius/ 4, 0);
					Point3D max = u->pos + Point3D(0, 0, 0.2f);
					max += Point3D(u->radius, u->radius / 4, u->radius/2);

					Debug()->DebugBoxOut(min, max, Colors::Blue);
				}
				else if (IsBuilding(u->unit_type)) {

				}
				*/
				if (u->alliance == Unit::Alliance::Neutral) {
					continue;
				}
				Debug()->DebugSphereOut(u->pos + Point3D(0, 0, 0.2f), u->radius, Colors::Green);
				float range = getRange(u);
				if (range != 0) {
					Debug()->DebugSphereOut(u->pos + Point3D(0, 0, 0.2f), range, Colors::Red);
				}
			}

			Debug()->SendDebug();
			sc2::SleepFor(20);

		}
#endif


		if (proxy != target) {
			if (FindEnemiesInRange(target, 18).empty() && baseRazed) {
				target = proxy;
				for (auto z : Observation()->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::PROTOSS_ZEALOT))) {
					OnUnitIdle(z);
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
		//sc2::SleepFor(44);





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
		if (Observation()->GetArmyCount() >= 7 || Observation()->GetFoodWorkers() < 5) {
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

		TryImmediateAttack(Observation()->GetUnits(Unit::Alliance::Self, [](const Unit & u) { return u.weapon_cooldown == 0; }));
		
		if (nexus != nullptr) {
			auto min = FindNearestMineralPatch(nexus->pos);
			for (auto probe : Observation()->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::PROTOSS_PROBE))) {
				if (probe != bob && probe != scout && Distance2D(nexus->pos, probe->pos) > 8.0f && ( probe->engaged_target_tag == 0 || !IsMineral(Observation()->GetUnit(probe->engaged_target_tag)->unit_type))) {
					Actions()->UnitCommand(probe, ABILITY_ID::HARVEST_GATHER, min);
				}
			}
		}

		AllocateAttackers();
	
	}


	void AllocateAttackers() {



	}

	std::vector<int> allocateTargets(const Units & probes, const Units & mins, int (*toAlloc) (const Unit *), bool keepCurrent=false) {
		std::unordered_map<Tag, int> targetIndexes;
		for (int i = 0, e = mins.size(); i < e; i++) {
			targetIndexes.insert_or_assign(mins[i]->tag, i);
		}
		std::unordered_map<Tag, int> unitIndexes;
		for (int i = 0, e = probes.size(); i < e; i++) {
			targetIndexes.insert_or_assign(probes[i]->tag, i);
		}
		std::vector<int> targets;
		targets.resize(probes.size(), -1);

		std::vector<std::vector<int>> attackers;
		attackers.resize(mins.size());

		//std::remove_if(mins.begin(), mins.end(), [npos](const Unit * u) { return  Distance2D(u->pos, npos) > 6.0f;  });
		std::vector<int> freeAgents;
		std::vector<int> freeMins;

		if (keepCurrent) {
			int i = 0;
			for (const auto & u : probes) {
				if (!u->orders.empty()) {
					const auto & o = u->orders.front();
					if (o.target_unit_tag != 0) {
						auto pu = targetIndexes.find(o.target_unit_tag);
						if (pu == targetIndexes.end()) {
							targets[i] = -1;
						}
						else {
							int ind = pu->second;
							targets[i] = ind;
							attackers[ind].push_back(i);
						}
					}
				}
				i++;
			}
			for (int i = 0, e = mins.size(); i < e; i++) {
				auto sz = attackers[i].size();
				int goodValue = toAlloc(mins[i]);
				if (sz > goodValue) {
					auto start = mins[i]->pos;
					std::sort(attackers[i].begin(), attackers[i].end(), [start, probes](int a, int b) { return DistanceSquared2D(start, probes[a]->pos) < DistanceSquared2D(start, probes[b]->pos); });

					for (int j = goodValue, e = attackers[i].size(); j < e; j++) {
						freeAgents.push_back(attackers[i][j]);
						targets[attackers[i][j]] = -1;
					}
					attackers[i].resize(goodValue);

				}
				else if (sz < goodValue) {
					freeMins.push_back(i);
				}
			}
		}
		else {
			for (int i = 0, e = probes.size(); i < e; i++) {
				freeAgents.push_back(i);
			}
			for (int i = 0, e = mins.size(); i < e; i++) {
				freeMins.push_back(i);
			}
		}

		
		

		if (!freeAgents.empty()) {
			Point2D cogp = probes[freeAgents.front()]->pos;
			int div = 1;
			for (auto it = ++freeAgents.begin(); it != freeAgents.end(); ++it) {
				cogp += probes[*it]->pos;
				div++;
			}
			cogp /= div;

			std::sort(freeAgents.begin(), freeAgents.end(), [cogp, probes](int a, int b) { return DistanceSquared2D(cogp, probes[a]->pos) < DistanceSquared2D(cogp, probes[b]->pos); });
		}
		while (!freeMins.empty() && !freeAgents.empty()) {
			int choice = freeAgents.back();
			freeAgents.pop_back();
			auto start = probes[choice]->pos;
			std::sort(freeMins.begin(), freeMins.end(), [start, mins](int a, int b) { return DistanceSquared2D(start, mins[a]->pos) > DistanceSquared2D(start, mins[b]->pos); });

			int mineral = freeMins.back();
			freeMins.pop_back();

			
			attackers[mineral].push_back(choice);
			targets[choice] = mineral;
			if (attackers[mineral].size() < toAlloc(mins[mineral])) {
				freeMins.insert(freeMins.begin(), mineral);
			}

		}

		return targets;

	}

	void TrySpreadProbes() {
		auto npos = nexus->pos;
		
		Units mins = Observation()->GetUnits(Unit::Alliance::Neutral, [npos](const Unit & u) { return  IsMineral(u.unit_type) &&   Distance2D(u.pos, npos) < 15.0f ; });
		Units probes = Observation()->GetUnits(Unit::Alliance::Self, [npos](const Unit & u) { return  u.unit_type == UNIT_TYPEID::PROTOSS_PROBE && Distance2D(u.pos, npos) < 15.0f ; });
		
		std::vector<int> targets = allocateTargets(probes, mins, [](const Unit *u) { return  2; });

		for (int att = 0, e = targets.size(); att < e; att++) {
			if (targets[att] != -1) {
				Actions()->UnitCommand(probes[att], ABILITY_ID::SMART, mins[targets[att]]);
			}
		}

	}

	float getRange(const Unit *z) {
		auto arms = Observation()->GetUnitTypeData().at(static_cast<uint32_t>(z->unit_type)).weapons;
		if (arms.empty()) {
			return 0;
		}
		float attRange = arms.front().range +z->radius;

											 // slight exaggeration is ok
											 //attRange *= 1.01f;

		if (z->unit_type == UNIT_TYPEID::PROTOSS_PROBE) {
			//attRange *= .6f;
		}

		return attRange;
	}

	void TryImmediateAttack(const Units & zeals) {
		
		for (const auto & z : zeals) {
			// weak drones only do this if close to nexus
			if (z->unit_type == UNIT_TYPEID::PROTOSS_PROBE &&  ( (z->shield + z->health <= 10) || (nexus==nullptr || Distance2D(nexus->pos, z->pos) > 5.0f) || z == bob)) {
				continue;
			} 
						
			float attRange = getRange(z);

			auto enemies = FindEnemiesInRange(z->pos, attRange);

			// make sure there is no more than 60 degree turn to attack it
			const float length =attRange;
			sc2::Point3D p1 = z->pos;			
			p1.x += length * std::cos(z->facing);
			p1.y += length * std::sin(z->facing); 
			std::remove_if(enemies.begin(), enemies.end(), [p1,attRange](const auto & u) {  return Distance2D(u->pos, p1) > attRange + u->radius;  });

			if (!enemies.empty()) {
				float max = 2000;
				const Unit * t = nullptr;
				for (auto nmy : enemies) {
					float f = nmy->health + nmy->shield;
					if (f <= max) {
						t = nmy;
						max = f;
					}
				}
				// requeue our previous order				
				if (z->orders.empty()) {
					Actions()->UnitCommand(z, ABILITY_ID::ATTACK_ATTACK, t);
				}
				else {
					auto order = z->orders.front();
					Actions()->UnitCommand(z, ABILITY_ID::ATTACK_ATTACK, t);
					sendUnitCommand(z, order, true);
				}
				
			}
		}
	}

	// In your bot class.
	virtual void OnUnitIdle(const Unit* unit) final {
		if (unit == scout) {
			
			if (proxy != target) {
				Actions()->UnitCommand(unit, ABILITY_ID::SMART, FindNearestMineralPatch(nexus->pos),false);
				scout = nullptr;
			}
			else {
				scouted++;
				if (scouted == Observation()->GetGameInfo().enemy_start_locations.size() - 1) {	
					target = Observation()->GetGameInfo().enemy_start_locations[scouted];
					Actions()->UnitCommand(unit, ABILITY_ID::SMART, FindNearestMineralPatch(nexus->pos), false);
					scout = nullptr;
				}
				else {
					Actions()->UnitCommand(unit, ABILITY_ID::SMART, Observation()->GetGameInfo().enemy_start_locations[scouted], false);
				}
			}
			return;
		}
		
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
	const Unit * scout = nullptr;
	int scouted = 0;
	std::vector<Point3D> expansions;
	const Unit * nexus = nullptr;
	bool baseRazed ;

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

	// map "topology" consists in tagging the expansions as being one of the following
	std::vector<int> mainBases;
	std::vector<int> pocketBases;
	std::vector<int> naturalBases;
	std::vector<int> proxyBases;
	// each base has a staging area, an open ground to group units before attacking
	std::vector<Point2D> stagingFor;
	int ourBaseExpansionIndex;
	int ourBaseStartLocIndex;


	void sortByDistanceTo(Units & units, const Point2D & start) {
		std::sort(units.begin(), units.end(), [start](const Unit * a, const Unit * b) { return DistanceSquared2D(start, a->pos) < DistanceSquared2D(start, b->pos); });
	}

	std::vector<int> sortByDistanceTo(std::valarray<float> matrix, int ti, size_t sz) {
		std::vector<int> byDist;
		for (int i = 0; i < sz; i++) {
			byDist.push_back(i);
		}
		std::sort(byDist.begin(), byDist.end(), [matrix, ti, sz](int a, int b) { return matrix[ti*sz + a] < matrix[ti*sz + b]; });
		return byDist;
	}

	void computeMapTopology(const std::vector<Point2D> & start_locations, std::vector<Point3D>  expansions) {
		std::vector <sc2::QueryInterface::PathingQuery> queries;
		size_t sz = expansions.size();
				
		// for the next steps, we want pathing info; targetting the center of a nexus will not work, so moving a bit off center is good.
		for (auto & p : expansions) {
			auto min = FindNearestMineralPatch(p);
			p += 0.8f *(p - min->pos);
		}

		// create the set of queries 		
		for (int i = 0; i < sz; i++) {			
			for (int j = i + 1; j < sz; j++) {
				queries.push_back({ 0, expansions[i], expansions[j] });
			}
		}
		// send the query and wait for answer
		std::vector<float> distances = Query()->PathingDistance(queries);

		for (auto & f : distances) {
			if (f == 0) {
				f = std::numeric_limits<float>::max();
			}
		}

		std::valarray<float> matrix(sz *sz); // no more, no less, than a matrix		
		auto dit = distances.begin();
		for (int i = 0 ; i < sz; i++) {
			// set diagonal to 0
			matrix[i*sz + i] = 0; 
			for (int j = i + 1; j < sz; j++, dit++) {
				matrix[i*sz + j] = * dit;
				matrix[j*sz + i] = * dit;
			}
		}

#ifdef DEBUG
		for (int i = 0; i < sz; i++) {
			for (int j = i + 1; j < sz; j++) {
				auto color = (matrix[i*sz + j] > 100000.0f) ? Colors::Red : Colors::Green;
				Debug()->DebugLineOut(expansions[i] + Point3D(0, 0, 0.5), expansions[j] + Point3D(0, 0, 0.5), color);
				Debug()->DebugTextOut(std::to_string(matrix[i*sz + j]), (expansions[i] + Point3D(0, 0, 0.2) + expansions[j]) / 2, Colors::Green);
			}
		}
#endif // DEBUG


		
		// Ok now tag expansion locations 
		for (int startloc = 0; startloc < start_locations.size(); startloc++) {
			const auto & sloc = start_locations[startloc];
			//first find the expansion that is the main base
			float distance = std::numeric_limits<float>::max();
			int ti = 0;
			for (int i = 0; i < sz; i++) {
				auto d = DistanceSquared2D(sloc, expansions[i]);
				if (distance > d) {
					distance = d;
					ti = i;
				}
			}
			mainBases.push_back(ti);
		}

		for (int baseIndex = 0; baseIndex < mainBases.size(); baseIndex++) {
			int ti = mainBases[baseIndex];
			// next look for the closest bases to this main
			std::vector<int> byDist = sortByDistanceTo(matrix,ti,sz);
			
			int maxCloseBaseIndex=2;
			float dClosest = matrix[ti*sz + byDist[1]];
			for (int i = 2; i < sz; i++) {				
				if (matrix[ti*sz + byDist[i]] > 1.5 * dClosest) {
					break;
				}
				else {
					maxCloseBaseIndex++;
				}
			}

			int nat=byDist[1];
			int pocket = -1;

			int nmyStart = (baseIndex == 0) ? mainBases[1] : mainBases[0];
			// are there several nat candidates ?
			if (maxCloseBaseIndex > 2) {
				// the first one of these that is closer to enemy base than the main base is a nat
				for (int i = 1; i < maxCloseBaseIndex; i++) {
					
					if (matrix[nmyStart*sz + byDist[i]] < matrix[nmyStart*sz + ti]) {
						nat = byDist[i];						
					}
					else {
						pocket = byDist[i];
					}
				}
			}
			naturalBases.push_back(nat);
			
			// the one of these two that is closer to our base
			pocketBases.push_back(pocket);
			
			// next look for the closest bases to this nat 
			std::vector<int> byDistNat = sortByDistanceTo(matrix, nat, sz);
			std::remove_if(byDistNat.begin(), byDistNat.end(),[nat, pocket, ti](int v) { return v == nat || v==pocket || v==ti; });

			// limit to close bases
			float dCloseNat = matrix[nat*sz + byDistNat[0]];
			int tokeep = 1;
			for (int i = 1; i < sz; i++) {
				if (matrix[nat*sz + byDistNat[i]] > 1.5 * dCloseNat) {
					break;
				}
				else {
					tokeep++;
				}
			}
			byDistNat.resize(tokeep);			


			int proxy;
			// choose the one that is furthest from line linking ourBase to his.
			float distance = 0; 
			// distance P1P2 for denominator
			const auto & P1 = expansions[nmyStart];
			const auto & P2 = expansions[ti];
			float denom = Distance2D(P1, P2);
			for (int base : byDistNat) {
				const auto & X0 = expansions[base];
				// Equation is straight off of : https://en.wikipedia.org/wiki/Distance_from_a_point_to_a_line#Line_defined_by_two_points
				float d = std::abs((P2.y - P1.y)*X0.x - (P2.x - P1.x)*X0.y + P2.x*P1.y - P2.y*P1.x) / denom;
				if (d > distance) {
					distance = d;
					proxy = base;
				}
			}
			proxyBases.push_back(proxy);
			
		}


	}

	const Units FindFriendliesInRange(const Point2D& start, float radius) {
		auto radiuss = pow(radius, 2);
		Units units = Observation()->GetUnits(Unit::Alliance::Self, [start, radiuss](const Unit& unit) {
			return DistanceSquared2D(unit.pos, start) < radiuss; });
		return units;
	}

	const Units FindEnemiesInRange(const Point2D& start, float radius) {
		Units units = Observation()->GetUnits(Unit::Alliance::Enemy, [start, radius](const Unit& unit) {
			return !unit.is_flying && DistanceSquared2D(unit.pos, start) < pow(radius + unit.radius,2); });
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

	static bool IsMineral(const sc2::UNIT_TYPEID type) {
		switch (type) {
			case  UNIT_TYPEID::NEUTRAL_MINERALFIELD:case  UNIT_TYPEID::NEUTRAL_MINERALFIELD750:case  UNIT_TYPEID::NEUTRAL_RICHMINERALFIELD:
			case  UNIT_TYPEID::NEUTRAL_RICHMINERALFIELD750:case  UNIT_TYPEID::NEUTRAL_PURIFIERMINERALFIELD:case  UNIT_TYPEID::NEUTRAL_PURIFIERMINERALFIELD750:
			case  UNIT_TYPEID::NEUTRAL_PURIFIERRICHMINERALFIELD:case  UNIT_TYPEID::NEUTRAL_PURIFIERRICHMINERALFIELD750:case  UNIT_TYPEID::NEUTRAL_LABMINERALFIELD:
			case  UNIT_TYPEID::NEUTRAL_LABMINERALFIELD750:case  UNIT_TYPEID::NEUTRAL_BATTLESTATIONMINERALFIELD:case  UNIT_TYPEID::NEUTRAL_BATTLESTATIONMINERALFIELD750:
			return true;
		}
		return false;
	}

	static bool IsWorkerType(const sc2::UNIT_TYPEID type) 
	{
		switch (type)
		{
		case sc2::UNIT_TYPEID::TERRAN_SCV:
		case sc2::UNIT_TYPEID::TERRAN_MULE: 
		case sc2::UNIT_TYPEID::PROTOSS_PROBE:
		case sc2::UNIT_TYPEID::ZERG_DRONE: 
		case sc2::UNIT_TYPEID::ZERG_DRONEBURROWED: return true;
		default: return false;
		}
	}

	static bool IsCommandStructure(const sc2::UNIT_TYPEID type) {
		switch (type) {
		case UNIT_TYPEID::TERRAN_COMMANDCENTER:
		case UNIT_TYPEID::TERRAN_COMMANDCENTERFLYING:
		case UNIT_TYPEID::TERRAN_ORBITALCOMMAND:		
		case UNIT_TYPEID::TERRAN_PLANETARYFORTRESS:
		case UNIT_TYPEID::TERRAN_ORBITALCOMMANDFLYING:
		case UNIT_TYPEID::PROTOSS_NEXUS:
		case UNIT_TYPEID::ZERG_HATCHERY:
		case UNIT_TYPEID::ZERG_LAIR:
		case UNIT_TYPEID::ZERG_HIVE: return true;
		default: return false;
		}
	}

	static bool IsBuilding(const sc2::UNIT_TYPEID type) 
	{
		if (IsCommandStructure(type)) {
			return true;
		}
		switch (type)
		{
			//Terran
		case sc2::UNIT_TYPEID::TERRAN_ARMORY:
		case sc2::UNIT_TYPEID::TERRAN_BARRACKS:
		case sc2::UNIT_TYPEID::TERRAN_BARRACKSFLYING:
		case sc2::UNIT_TYPEID::TERRAN_BARRACKSREACTOR:
		case sc2::UNIT_TYPEID::TERRAN_BARRACKSTECHLAB:
		case sc2::UNIT_TYPEID::TERRAN_BUNKER:
		case sc2::UNIT_TYPEID::TERRAN_COMMANDCENTER:		
		case sc2::UNIT_TYPEID::TERRAN_ENGINEERINGBAY:
		case sc2::UNIT_TYPEID::TERRAN_FACTORY:
		case sc2::UNIT_TYPEID::TERRAN_FACTORYFLYING:
		case sc2::UNIT_TYPEID::TERRAN_FACTORYREACTOR:
		case sc2::UNIT_TYPEID::TERRAN_FACTORYTECHLAB:
		case sc2::UNIT_TYPEID::TERRAN_FUSIONCORE:
		case sc2::UNIT_TYPEID::TERRAN_GHOSTACADEMY:
		case sc2::UNIT_TYPEID::TERRAN_MISSILETURRET:		
		case sc2::UNIT_TYPEID::TERRAN_PLANETARYFORTRESS:
		case sc2::UNIT_TYPEID::TERRAN_REFINERY:
		case sc2::UNIT_TYPEID::TERRAN_SENSORTOWER:
		case sc2::UNIT_TYPEID::TERRAN_STARPORT:
		case sc2::UNIT_TYPEID::TERRAN_STARPORTFLYING:
		case sc2::UNIT_TYPEID::TERRAN_STARPORTREACTOR:
		case sc2::UNIT_TYPEID::TERRAN_STARPORTTECHLAB:
		case sc2::UNIT_TYPEID::TERRAN_SUPPLYDEPOT:
		case sc2::UNIT_TYPEID::TERRAN_SUPPLYDEPOTLOWERED:
		case sc2::UNIT_TYPEID::TERRAN_REACTOR:
		case sc2::UNIT_TYPEID::TERRAN_TECHLAB:

			// Zerg
		case sc2::UNIT_TYPEID::ZERG_BANELINGNEST:
		case sc2::UNIT_TYPEID::ZERG_CREEPTUMOR:
		case sc2::UNIT_TYPEID::ZERG_CREEPTUMORBURROWED:
		case sc2::UNIT_TYPEID::ZERG_CREEPTUMORQUEEN:
		case sc2::UNIT_TYPEID::ZERG_EVOLUTIONCHAMBER:
		case sc2::UNIT_TYPEID::ZERG_EXTRACTOR:
		case sc2::UNIT_TYPEID::ZERG_GREATERSPIRE:
		case sc2::UNIT_TYPEID::ZERG_HYDRALISKDEN:
		case sc2::UNIT_TYPEID::ZERG_INFESTATIONPIT:
		case sc2::UNIT_TYPEID::ZERG_LURKERDENMP:
		case sc2::UNIT_TYPEID::ZERG_NYDUSCANAL:
		case sc2::UNIT_TYPEID::ZERG_NYDUSNETWORK:
		case sc2::UNIT_TYPEID::ZERG_SPAWNINGPOOL:
		case sc2::UNIT_TYPEID::ZERG_SPINECRAWLER:
		case sc2::UNIT_TYPEID::ZERG_SPINECRAWLERUPROOTED:
		case sc2::UNIT_TYPEID::ZERG_SPIRE:
		case sc2::UNIT_TYPEID::ZERG_SPORECRAWLER:
		case sc2::UNIT_TYPEID::ZERG_SPORECRAWLERUPROOTED:
		case sc2::UNIT_TYPEID::ZERG_ULTRALISKCAVERN:

			// Protoss
		case sc2::UNIT_TYPEID::PROTOSS_ASSIMILATOR:
		case sc2::UNIT_TYPEID::PROTOSS_CYBERNETICSCORE:
		case sc2::UNIT_TYPEID::PROTOSS_DARKSHRINE:
		case sc2::UNIT_TYPEID::PROTOSS_FLEETBEACON:
		case sc2::UNIT_TYPEID::PROTOSS_FORGE:
		case sc2::UNIT_TYPEID::PROTOSS_GATEWAY:
		case sc2::UNIT_TYPEID::PROTOSS_PHOTONCANNON:
		case sc2::UNIT_TYPEID::PROTOSS_PYLON:
		case sc2::UNIT_TYPEID::PROTOSS_PYLONOVERCHARGED:
		case sc2::UNIT_TYPEID::PROTOSS_ROBOTICSBAY:
		case sc2::UNIT_TYPEID::PROTOSS_ROBOTICSFACILITY:
		case sc2::UNIT_TYPEID::PROTOSS_STARGATE:
		case sc2::UNIT_TYPEID::PROTOSS_TEMPLARARCHIVE:
		case sc2::UNIT_TYPEID::PROTOSS_TWILIGHTCOUNCIL:
		case sc2::UNIT_TYPEID::PROTOSS_WARPGATE:
			return true;
		}
		return false;
	}

	static bool IsArmyUnitType(const sc2::UNIT_TYPEID type) 
	{
		if (IsWorkerType(type)) { return false; }
		if (type == sc2::UNIT_TYPEID::ZERG_OVERLORD) { return false; } //Excluded here the overlord transport etc to count them as army unit
		if (IsBuilding(type)) { return false; }
		if (type == sc2::UNIT_TYPEID::ZERG_EGG) { return false; }
		if (type == sc2::UNIT_TYPEID::ZERG_LARVA) { return false; }

		return true;
	}

	const Unit* FindNearestMineralPatch(const Point2D& start) {		
		return FindNearestUnit(start, Observation()->GetUnits(Unit::Alliance::Neutral, [](const auto & u) { return IsMineral(u.unit_type); }));
	}

	/* used to requeue existing orders taken from unit->orders */
	void sendUnitCommand(const Unit * unit, const sc2::UnitOrder & order, bool queue = false) {
		if (order.target_unit_tag != 0) {
			Actions()->UnitCommand(unit, order.ability_id, Observation()->GetUnit(order.target_unit_tag), queue);
		}
		else {
			Actions()->UnitCommand(unit, order.ability_id, order.target_pos, queue);
		}
	}
};
