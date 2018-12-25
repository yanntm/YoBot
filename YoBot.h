#pragma once
#include "sc2api/sc2_interfaces.h"
#include "sc2api/sc2_agent.h"
#include "sc2api/sc2_map_info.h"
#include "sc2lib/sc2_lib.h"

#include "UnitTypes.h"
#include "DistUtil.h"

#include "YoAgent.h"
#include "HarvesterStrategy.h"
#include "AStar.h"
#include <valarray>
#include <unordered_set>
#include <iostream>
#include <utility>

//#include "Strategys.h"
#define DllExport   __declspec( dllexport )  
using namespace sc2;
using namespace sc2util;

class YoBot : public YoAgent {
public:
	MultiHarvesterStrategy harvesting;

	virtual void OnGameStart() final {
		std::cout << "YoBot will kill you!" << std::endl;
		

		YoAgent::initialize();


		const ObservationInterface* observation = Observation();
		frame = 0;
		nexus = nullptr;
		auto list = Observation()->GetUnits(Unit::Alliance::Self, [](auto u) { return IsCommandStructure(u.unit_type); });
		nexus = list.front();


		const Unit* mineral_target = FindNearestMineralPatch(nexus->pos);
		Actions()->UnitCommand(nexus, ABILITY_ID::RALLY_NEXUS, mineral_target);
		Actions()->UnitCommand(nexus, ABILITY_ID::TRAIN_PROBE);

		auto probes = Observation()->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::PROTOSS_PROBE));
		bob = probes.back();
		probes.pop_back();
		
		
		const GameInfo& game_info = Observation()->GetGameInfo();
		
		


		auto playerID = Observation()->GetPlayerID();
		for (const auto & playerInfo : Observation()->GetGameInfo().player_info)
		{
			if (playerInfo.player_id != playerID)
			{
				enemyRace = playerInfo.race_requested;
				break;
			}
		}



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
			//scout = *(++Observation()->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::PROTOSS_PROBE)).begin());			

			target = map.getPosition(MapTopology::enemy, MapTopology::main);
			
			int max = 2 ;
			if (map.hasPockets()) {
				max = 3;
			}
			int rnd = GetRandomInteger(0, max);
			int limit = 1;
			if (enemyRace == Race::Zerg) {
				limit = 0;
			}
			rnd = 0;
			if (rnd <= limit) {
				proxy = map.getPosition(MapTopology::enemy, MapTopology::nat);
			}
			else if (rnd <= 2) {
				proxy = map.getPosition(MapTopology::enemy, MapTopology::proxy); 
			}
			else if (rnd == 3) {
				proxy = map.getPosition(MapTopology::enemy, MapTopology::pocket); 
			}
			//proxy = map.getPosition(MapTopology::ally, MapTopology::nat);
			//proxy = (.67 * target + .33*nexus->pos);
			//proxy = FindFarthestBase(nexus->pos,target);
			//proxy = FindNearestBase(proxy);
		}

		baseRazed = false;

		//if (game_info.)
		//choke = (.2f * target + .8f*nexus->pos);
		Actions()->UnitCommand(bob, ABILITY_ID::SMART, proxy);		

		if (scout != nullptr) 
			probes.erase(std::find(probes.begin(), probes.end(), scout));
		
		harvesting.initialize(nexus, map.resourcesPer[map.FindNearestBaseIndex(nexus->pos)], Observation());
		harvesting.OnStep(probes, Observation(),YoActions(),false);


		placer.reserve(map.getExpansionIndex(MapTopology::ally, MapTopology::main));
		placer.reserve(map.getExpansionIndex(MapTopology::ally, MapTopology::nat));
		placer.reserveCliffSensitive(map.FindNearestBaseIndex(proxy),Observation());
	}

	virtual void OnUnitCreated(const Unit* unit) final {
		if (IsArmyUnitType(unit->unit_type)) {						
			if (unit->unit_type == UNIT_TYPEID::PROTOSS_VOIDRAY) {
				for (auto u : allEnemies()) {
					if (u.second->is_flying) {
						Actions()->UnitCommand(unit, ABILITY_ID::ATTACK, u.second->pos);						
						break;
					}
				}
			}
			else {
				Actions()->UnitCommand(unit, ABILITY_ID::ATTACK, defensePoint(proxy));
			}
		}
		else if (unit->unit_type == UNIT_TYPEID::PROTOSS_NEXUS) {
			const Unit* mineral_target = FindNearestMineralPatch(unit->pos);
			Actions()->UnitCommand(unit, ABILITY_ID::RALLY_NEXUS, mineral_target,true);
			//for (auto p : Observation()->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::PROTOSS_PROBE))) {
			//	Actions()->UnitCommand(unit, ABILITY_ID::HARVEST_GATHER, mineral_target);
			//	busy(p->tag);
			//}
			if (nexus == nullptr) {
				nexus = unit;
				proxy = nexus->pos;
			}
			harvesting.initialize(unit, map.resourcesPer[map.FindNearestBaseIndex(unit->pos)], Observation());
			
			buildingNexus = false;
		} 
		if (IsBuilding(unit->unit_type)) {
			for (auto & gw : Observation()->GetUnits(Unit::Alliance::Self, [](auto & u) { return u.unit_type == UNIT_TYPEID::PROTOSS_GATEWAY || u.unit_type == UNIT_TYPEID::PROTOSS_ROBOTICSFACILITY; })) {
				auto direction = proxy - gw->pos;
				direction /= Distance2D(Point2D(0, 0), direction);
				// point towards open spot
				std::vector < Point2D > pos = { {-2,2},{0,2}, {2,2},
															{-2,0},{0,0}, {2,0},
															{-2,-2},{0,-2}, {2,-2} };
				for (auto & p : pos) {
					p += gw->pos;
				}
				sortByDistanceTo(pos, proxy);
				auto width = Observation()->GetGameInfo().width;
				auto center = defensePoint(map.FindNearestBase(gw->pos));
				if (!Pathable(Observation()->GetGameInfo(), center)) {
					center = Point2D(width / 2, Observation()->GetGameInfo().height / 2);
					while (center.x < width && !Pathable(Observation()->GetGameInfo(), center)) {
						center += Point2D(3.0, 0);
					}
				}
				if (Pathable(Observation()->GetGameInfo(), center)) {
					for (auto & p : pos) {
						if (Pathable(Observation()->GetGameInfo(), p)) {
							auto d = Query()->PathingDistance(p, center);
#ifdef DEBUG
							Debug()->DebugTextOut(std::to_string(d),Point3D(p.x,p.y,gw->pos.z+.1f));
#endif
							if (d > 0.1f) {								
								Actions()->UnitCommand(gw, ABILITY_ID::RALLY_BUILDING, p);
								break;
							}
						}
					}
				}
			}
		}
	}

	// return a position to defend a position from
	Point2D defensePoint(const Point2D & pos) {		
		const Unit* min = FindNearestMineralPatch(pos);
		if (Distance2D(pos, min->pos) < 10.0f) {
			return 2 * pos - min->pos;
		}
		else {
			return pos;
		}
	}

	virtual void OnUnitHasAttacked(const Unit* unit)  {


		if (unit->unit_type == UNIT_TYPEID::PROTOSS_ZEALOT && ! YoActions()->isBusy(unit->tag)) {
			auto targets = FindEnemiesInRange(unit->pos, 15);

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
				if (IsWorkerType(t->unit_type)) {
					drones.push_back(t);
				}
				if (t->unit_type == UNIT_TYPEID::TERRAN_SUPPLYDEPOT || t->unit_type == UNIT_TYPEID::TERRAN_SUPPLYDEPOTLOWERED || t->unit_type == UNIT_TYPEID::PROTOSS_PYLON) {
					pylons.push_back(t);
				}
			}
			
			const Unit * enemy = nullptr;
			if (!drones.empty()) {
				if (rand() % 2 == 0) {
					enemy = chooseClosest(unit, drones);
				}
			}
			if (enemy == nullptr) {
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
			}
			if (enemy != nullptr && enemy->tag != unit->engaged_target_tag) {
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
			if (unit == scout) {
				Actions()->UnitCommand(unit, ABILITY_ID::SMART, map.resourcesPer[map.getExpansionIndex(MapTopology::enemy,MapTopology::main)][5]);
			}
			else {
				OnUnitIdle(unit);
			}

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

	bool evade(const Unit * unit, Point2D goal) {
		
		auto nmies = Observation()->GetUnits(Unit::Alliance::Enemy, [&](const Unit& u) {
			return (IsArmyUnitType(u.unit_type) || isStaticDefense(u.unit_type) || IsWorkerType(u.unit_type)) && Distance2D(unit->pos, u.pos) < std::max(6.0f, 1.5f + getRange(&u, Observation()->GetUnitTypeData())); });
		if (nmies.empty()) {
			return false;
		}
		sortByDistanceTo(nmies, unit->pos);
		if (nmies.size() >= 8) {
			nmies.resize(8);
		}
		// up to two probes and a lot of shields : just do our thing
		if (nmies.size() <= 2 && all_of(nmies.begin(), nmies.end(), [](auto & u) { return IsWorkerType(u->unit_type); }) 
			&& unit->shield >= 9 && Distance2D(unit->pos,goal) > 12.0f) {
			return false;
			auto min = FindNearestUnit(goal, Observation()->GetUnits(Unit::Alliance::Neutral, [](const Unit & u) { return IsMineral(u.unit_type); }));
			// mineral walk
			if (min && min->mineral_contents > 0) {
				Actions()->UnitCommand(unit, ABILITY_ID::HARVEST_GATHER, min);
			} else {
				Actions()->UnitCommand(unit, ABILITY_ID::MOVE, goal);
			}			
			return true;
		}
		// if we are overlapping enemy keep walking
		float delta =2 ;
		float delta2 =  delta / sqrt(2) ;
		std::vector<Point2D> outs = { unit->pos, 
			unit->pos + Point2D(delta,0.0f), unit->pos + Point2D(0.0f,delta) , unit->pos + Point2D(-delta,0.0f) , unit->pos + Point2D(0.0f,-delta) ,
			unit->pos + Point2D(delta2,delta2), unit->pos + Point2D(delta2,-delta2) , unit->pos + Point2D(-delta2,delta2) , unit->pos + Point2D(-delta2,-delta2)
		};
		std::vector <sc2::QueryInterface::PathingQuery> queries;
		for (auto p : outs) {
			queries.push_back({ 0, unit->pos, p });
			for (auto nmy : nmies) {
				queries.push_back({ 0, nmy->pos, p });
			}
		}
		// double distance ?
		for (auto pos : outs) {
			queries.push_back({ 0, unit->pos, pos+ (pos - unit->pos) });
		}
		// half distance ?
		for (auto pos : outs) {
			queries.push_back({ 0, unit->pos, (unit->pos + pos )/2 });
		}

		// send the query and wait for answer
		std::vector<float> distances = Query()->PathingDistance(queries);
		std::vector<float> scores;
		scores.reserve(outs.size());
		// 0 pos is us
		scores.push_back(-1);
		// skip us, it's not  an option
		for (int outid = 1; outid < outs.size(); outid++) {
			// our distance
			int ind = outid * (nmies.size()+1);
			float dtobob = distances[ind];
			// who can beat us to it ?
			float score = 0;
			if (dtobob == 0 || dtobob >= 1.2 * delta) {
				// non pathable
				score = -1;
			} else {
				for (int i = 1; i <= nmies.size(); i++) {
					float rank = nmies.size() - i + 1;
					// from enemy to out
					float dtonmy = distances[ind + i];
					// it can't path is good, further than where we are now is good
					if (dtonmy==0) {						
						score+=rank;
					}
					// greater dist from enemy to point than bob to point ?
					// greater dist from enemy to point than enemy to bob ?
					else if (dtonmy >= dtobob && dtonmy >= distances[i]) {
						score += (dtonmy - distances[i]) / delta * rank;
					//	score += (dtonmy - distances[i]) / delta;
					//	score += (dtonmy - dtobob) / delta ;
					//	score += distances[i] / dtonmy ;
					}
				}
			}
			scores.push_back(score);
		}
		// double score of truly open outs
		for (int outid = 1; outid < outs.size(); outid++) {
			float next = distances[outs.size() * (nmies.size() + 1) + outid];
			if (next != 0 && next < 2.3 * delta) {
				scores[outid] *= 2;
			}
		}
		// neg score of not truly reachable
		for (int outid = 1; outid < outs.size(); outid++) {
			float next = distances[outs.size() * (nmies.size() + 2) + outid];
			int left = outid + 1 == outs.size() ? 1 : outid + 1;
			int right = outid - 1 == 0 ? outs.size() - 1 : outid - 1;

			float leftd = distances[outs.size() * (nmies.size() + 2) + left];
			float rightd = distances[outs.size() * (nmies.size() + 2) + right];
			// allow some small obstacle, e.g. going close to a wall or geyser
			if ((next == 0 || next > 0.7 * delta)
				&& (leftd == 0 || leftd > 0.7 * delta)
				&& (rightd == 0 || rightd > 0.7 * delta)
				) {
				if (scores[outid] > 0)
					scores[outid] *= -1;
			}
		}		
		for (int outid = 1; outid < outs.size(); outid++) {
			float next = Distance2D(goal, outs[outid]);
			if (next >= 20.0f) {
				if (scores[outid] > 0)
					scores[outid] *= .66f;
			}
		}

		int best = 0;
		for (int i = 0; i < scores.size(); i++) {
			if (scores[i] > scores[best]) {
				best = i;
			}
		}

		int nbouts = 0;
		//auto dprox = Distance2D(goal, outs[best]);
		int bestproxout = best;
		
		for (int i = 0; i < scores.size(); i++) {
			if (scores[i] > 0.8 * scores[best]) {
				nbouts ++;
			/*	auto dpout = Distance2D(goal, outs[i]);
				if (dpout < dprox) {
					bestproxout = i;
					dprox = dpout;
				}*/
			}
		}

#ifdef DEBUG
		for (int i = 0; i < outs.size(); i++) {
			auto out = outs[i];
			if (i == best) {
				Debug()->DebugLineOut(unit->pos, Point3D(out.x, out.y, unit->pos.z + 0.1f), Colors::Green);
			}
			else if (i == bestproxout) {
				Debug()->DebugLineOut(unit->pos, Point3D(out.x, out.y, unit->pos.z + 0.1f), Colors::Blue);
			} 
			else {			
				Debug()->DebugLineOut(unit->pos, Point3D(out.x, out.y, unit->pos.z + 0.1f));
			}
			Debug()->DebugTextOut(std::to_string(scores[i]), Point3D(out.x, out.y, unit->pos.z + 0.1f));
		}
		Debug()->SendDebug();		
#endif // DEBUG
		auto closest = Distance2D(unit->pos, (*nmies.begin())->pos);
		if (nbouts <= 2 && ( unit->shield == 0 || closest <= 1.5f) && nexus != nullptr && IsWorkerType(unit->unit_type)) {
			// try to mineral slide our way out 
			Actions()->UnitCommand(unit, ABILITY_ID::HARVEST_GATHER, FindNearestMineralPatch(nexus->pos));
			return true;
		}
		if (nbouts >= 3 && closest >= 1.5f && unit->shield >= 0) {
			Actions()->UnitCommand(unit, ABILITY_ID::MOVE, outs[bestproxout]);
		}
		else {
			Actions()->UnitCommand(unit, ABILITY_ID::MOVE, outs[best]);
		}
		return true;
	}

	virtual void OnUnitAttacked(const Unit* unit) final {
		if ( (unit == bob || unit==scout) && unit->shield <= 5) {
			// evasive action
			evade(bob,proxy);
		} else if (unit->unit_type == UNIT_TYPEID::PROTOSS_PROBE && unit->alliance == Unit::Alliance::Self) {
			if (nexus == nullptr) {
				evade(unit, proxy);
			}
			else {

				auto list = Observation()->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::PROTOSS_PROBE));
				int att = 0;
				for (auto unit : list) {
					for (auto order : unit->orders) {
						if (order.ability_id == ABILITY_ID::ATTACK_ATTACK) {
							att++;
						}
					}
				}

				const auto & tpos = (nexus == nullptr) ? unit->pos : nexus->pos;
				Units close = FindEnemiesInRange(tpos, 6);

				if (att < 2 * close.size() && att <= list.size() *.75f) {
					if (bob != nullptr) list.erase(std::remove(list.begin(), list.end(), bob), list.end());
					if (scout != nullptr) list.erase(std::remove(list.begin(), list.end(), scout), list.end());
					auto targets = FindEnemiesInRange(tpos, 6);

					//auto reaper = FindNearestEnemy(Observation()->GetStartLocation());
					auto reaper = FindWeakestUnit(targets);
					float sum = 0;
					for (const auto & p : list) {
						sum += p->health + p->shield;
					}
					sum /= list.size();

					sortByDistanceTo(list, reaper->pos);
					std::stable_sort(list.begin(), list.end(), [](const Unit * a, const Unit *b) { return a->health + a->shield > b->health + b->shield; });

					// the probes with less than half of average life are pushed to end of selection
					//list.erase(
					std::remove_if(list.begin(), list.end(), [sum](const Unit * p) { return p->health + p->shield <= sum / 2; });
					//	,list.end()); 

					// we want new attackers to be mobilized
					std::remove_if(list.begin(), list.end(), [sum](const Unit * p) { return !p->orders.empty() && p->orders[0].ability_id == ABILITY_ID::ATTACK_ATTACK; });

					if (list.size() > 3)
						list.resize(3);
					if (reaper != nullptr && !list.empty()) {
						if (close.size() == 1 && IsWorkerType(reaper->unit_type)) {
							list.resize(1);
							//Actions()->UnitCommand(nexus, ABILITY_ID::TRAIN_PROBE, true);
						}
						
						for (auto u : list) {
							Actions()->UnitCommand(u, ABILITY_ID::ATTACK_ATTACK, reaper->pos);							
						}
					}
				}
				if (unit->shield == 0 && nexus != nullptr) {
					const auto & nmy = FindNearestUnit(unit->pos, close);
					if (nmy != nullptr) {
						auto v = unit->pos - nmy->pos;
						v /= Distance2D(Point2D(0, 0), v);
						v *= 3.0f;
						Actions()->UnitCommand(unit, ABILITY_ID::SMART, FindNearestMineralPatch(unit->pos + v));
					}
					else {
						Actions()->UnitCommand(unit, ABILITY_ID::SMART, FindNearestMineralPatch(tpos));
					}
				}
			}
		}
		else if (unit->unit_type == UNIT_TYPEID::PROTOSS_ZEALOT) {
			if (unit->shield <= 10 && unit->health <= 30) {
				evade(unit, proxy);
				//std::cout << "ouch run away" << std::endl;
			}
			else {
				Actions()->UnitCommand(unit, ABILITY_ID::ATTACK_ATTACK, unit->pos);
			}

		}
		else if (IsBuilding(unit->unit_type) && unit->build_progress < 1.0f) {
			if (unit->health < 30 || unit->build_progress >= 0.95 && (unit->health + unit->shield) / (unit->health_max + unit->shield_max) < 0.6) {
				Actions()->UnitCommand(unit,ABILITY_ID::CANCEL);
			}
		}
		else {
			for (auto friendly : FindFriendliesInRange(unit->pos, 8.0f)) {
				if (IsArmyUnitType(friendly->unit_type) && friendly->orders.empty() && ! YoActions()->isBusy(friendly->tag)) {
					OnUnitHasAttacked(friendly);
				}
			}
		}

	}

	virtual void OnUnitEnterVision(const Unit* u) final {
		int cur = estimateEnemyStrength();
		YoAgent::OnUnitEnterVision(u);
		int next = estimateEnemyStrength();

		if (target == proxy && u->alliance == Unit::Alliance::Enemy && u->health_max >= 200 && !u->is_flying) {
			auto pottarget = map.FindNearestBase(u->pos);
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
		else {
			auto & home = map.getPosition(map.ally, map.main);
			if (IsCommandStructure(u->unit_type) && Distance2D(home, u->pos) < Distance2D(target, home)) {
				target = u->pos;
			}
		}
		
		if (next >= 5 && cur < 5) {
			for (auto u : Observation()->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::PROTOSS_ZEALOT))) {
				OnUnitIdle(u);
			}
		}
	}

	virtual void OnUnitDestroyed(const Unit* unit) final {		
		YoAgent::OnUnitDestroyed(unit);
		if (bob == unit) {
			bob = nullptr;
			if (scout != nullptr && Distance2D(scout->pos, proxy) < 20.0f) {
				bob = scout;
				scout = nullptr;
				return;
			}
			for (auto probe : Observation()->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::PROTOSS_PROBE))) {
				if (probe->is_alive) {
					bob = probe;
					break;
				}
			}
			auto nmy = Observation()->GetUnits(Unit::Alliance::Enemy, [this] (const Unit & u) { return  Distance2D(u.pos, proxy) < 10.0f; } ); 
			bool move = false;
			int att = 0;
			for (const auto & u : nmy) {
				if (isStaticDefense(u->unit_type)) {
					move = true;
				}
				if (IsArmyUnitType(u->unit_type)) {
					att++;
				}
			}
			int cgw = 0;
			for (auto & gw : Observation()->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::PROTOSS_GATEWAY))) {
				if (gw->is_powered) {
					cgw += 3;
				}
				else {
					cgw += 1;
				}
			}
			if (cgw <= 2) {
				move = true;
			}
			if (move || att >= CountUnitType(UNIT_TYPEID::PROTOSS_ZEALOT) || nmy.size() >= 8) {
				if (nexus != nullptr) {
					proxy = nexus->pos;
				}
				else {
					proxy = map.getPosition(MapTopology::ally, MapTopology::main);
				}
			}
		}
		else if (scout == unit) {
			scout = nullptr;
		}
		else if (nexus == unit) {
			nexus = nullptr;
		}
		else if (unit->unit_type == UNIT_TYPEID::PROTOSS_PROBE && unit->alliance == Unit::Alliance::Self) {
			auto list = Observation()->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::PROTOSS_PROBE));
			list.erase(std::remove(list.begin(), list.end(), bob), list.end());	
			list.erase(std::remove_if(list.begin(), list.end(), [&](auto & p) { return Distance2D(p->pos, unit->pos) > 8.0f; }), list.end());

			auto reaper = FindNearestEnemy(unit->pos);
			if (reaper == nullptr) {
				return;
			}
			sortByDistanceTo(list, reaper->pos);
			if (list.size() > 3)
				list.resize(3);
			if (reaper != nullptr) {
				if (IsWorkerType(reaper->unit_type)) {
					//list.resize(2);
					//Actions()->UnitCommand(nexus, ABILITY_ID::TRAIN_PROBE, true);
				}
				for (auto u : list) {
					Actions()->UnitCommand(u, ABILITY_ID::ATTACK_ATTACK, reaper->pos);
				}
			}
		}
		else if (unit->alliance == Unit::Alliance::Enemy && IsCommandStructure( unit->unit_type) ) {
			if (Distance2D(target, unit->pos) < 20) {
				baseRazed = true;
			}
		}
	}

	const Unit* FindWeakestUnit(const Units & units) {
		float maxhp = std::numeric_limits<float>::max();
		const Unit* targete = nullptr;
		for (const auto& u : units) {
			
			float hp = u->health + u->shield;
			if (hp < maxhp) {
				maxhp = hp;
				targete = u;
			}
			
		}
		return targete;
	}


	const Unit* FindNearestEnemy(const Point2D& start) {
		return FindNearestUnit(start, Observation()->GetUnits(Unit::Alliance::Enemy));		
	}


	std::vector<Point2D> findLocationsForBuilding(Point2D pos, int floorsize, int radius) {
		const GameInfo& game_info = Observation()->GetGameInfo();
		 
		std::vector<Point2D> vec;

		return vec;
	}

	
	int staticd;
	int reapers;
	bool needCannons;
	bool needImmo;
	bool reinforce;
	bool buildingNexus;

	virtual void OnYoStep() final {
		needCannons = true;
		
		frame++;

#ifdef DEBUG
		{

			map.debugMap(Debug(), Observation());

			Debug()->DebugTextOut("CURproxy", proxy + Point3D(0, 0, 2.0f), Colors::Yellow);
			Debug()->DebugTextOut("CURtarget", target + Point3D(0, 0, 2.0f), Colors::Yellow);

			for (const auto & u : Observation()->GetUnits(Unit::Alliance::Self)) {
				if (!u->orders.empty()) {
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
				if (u->weapon_cooldown != 0) {
					Debug()->DebugTextOut("cd" + std::to_string(u->weapon_cooldown), u->pos + Point3D(0, 0, .2), Colors::Green);
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
				float range = getRange(u, Observation()->GetUnitTypeData());
				if (range != 0) {
					Debug()->DebugSphereOut(u->pos + Point3D(0, 0, 0.2f), range, Colors::Red);
				}
			}

			int enemies = estimateEnemyStrength();
			Debug()->DebugTextOut("Current enemy :" + std::to_string(enemies));
			/*
			float * weights = computeWeightMap(Observation()->GetGameInfo(), Observation()->GetUnitTypeData(), {});

			for (auto & gw : Observation()->GetUnits(Unit::Alliance::Self, [](auto & u) { return u.unit_type == UNIT_TYPEID::PROTOSS_GATEWAY || u.unit_type == UNIT_TYPEID::PROTOSS_ROBOTICSFACILITY; })) {
				auto direction = proxy - gw->pos;
				direction /= Distance2D(Point2D(0, 0), direction);
				// point towards open spot
				std::vector < Point2D > pos = { {-2,2},{0,2}, {2,2},
												{-2,0},		  {2,0},
												{-2,-2},{0,-2}, {2,-2} };
				for (auto & p : pos) {
					p += gw->pos;
				}
				sortByDistanceTo(pos, proxy);
				auto width = Observation()->GetGameInfo().width;
				auto center = defensePoint(map.FindNearestBase(gw->pos));
				if (!Pathable(Observation()->GetGameInfo(), center)) {
					center = Point2D(width / 2, Observation()->GetGameInfo().height / 2);
					while (center.x < width && !Pathable(Observation()->GetGameInfo(), center)) {
						center += Point2D(3.0, 0);
					}
				}
				if (Pathable(Observation()->GetGameInfo(), center)) {
					for (auto & p : pos) {
						if (Pathable(Observation()->GetGameInfo(), p)) {
							auto d = Query()->PathingDistance(p, center);
							auto path = AstarSearchPath(p, center, Observation()->GetGameInfo(), weights);
							map.debugPath(path, Debug(), Observation());
							auto color = Colors::Red;
							if (d > 0.1f) {
								color = Colors::Green;
							}
							if (path.empty()) {
								color = Colors::Purple;
							}
#ifdef DEBUG
							Debug()->DebugTextOut(std::to_string(d), Point3D(p.x, p.y, gw->pos.z + .1f), color);
							Debug()->DebugTextOut(std::to_string(path.size()), Point3D(p.x, p.y - .2f, gw->pos.z + .1f), color);
#endif

						}
					}
				}
			}
			if (frame % 50 == 0) {
				placer.debug(Debug(), Observation());
				pokeMap();
			}
			*/
			Debug()->SendDebug();


			//sc2::SleepFor(20);

		}
#endif
		if (gas > 150) {
			needImmo = true;
		}
		//sc2::SleepFor(20);
		auto probs = Observation()->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::PROTOSS_PROBE));
		if (false && frame == 400 && scout == nullptr && probs.size() > 2) {
			scout = *(++Observation()->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::PROTOSS_PROBE)).begin());
			// return minerals
			if (IsCarryingMinerals(*scout)) {
				Actions()->UnitCommand(scout, ABILITY_ID::HARVEST_RETURN);
				Actions()->UnitCommand(scout, ABILITY_ID::SMART, map.getPosition(MapTopology::enemy, MapTopology::main), true);
			}
			else {
				Actions()->UnitCommand(scout, ABILITY_ID::SMART, map.getPosition(MapTopology::enemy, MapTopology::main));
			}
		}
		Units pylons = Observation()->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::PROTOSS_PYLON));
		
		
		if (frame % 50 == 0 && frame > 50) {
			staticd = 0;
			reapers = 0;
			needImmo = false;
			needCannons = false;
			for (auto & e : allEnemies()) {
				if (isStaticDefense(e.second->unit_type)) {
					staticd++;
				}
				if (e.second->unit_type == UNIT_TYPEID::TERRAN_REAPER) {
					reapers++;
				}
			}
			if (reapers >= 2 || staticd >= 2) {
				needCannons = true;
			}
			if (staticd >= 2) {
				needImmo = true;
			}
		}

		bool evading = false;
		if (bob != nullptr && ! YoActions()->isBusy(bob->tag) && probs.size() > 2) {
			if (! orderBusy(bob)) {
				auto d = Distance2D(bob->pos, proxy);
				if (d > 15.0f && d < 25.0f) {

					float * weights = computeWeightMap(Observation()->GetGameInfo(), Observation()->GetUnitTypeData(), allEnemies());
					auto path = AstarSearchPath(bob->pos, map.FindHardPointsInMinerals(map.FindNearestBaseIndex(proxy))[0], Observation()->GetGameInfo(), weights);
#ifdef DEBUG
					map.debugPath(path, Debug(), Observation());
#endif
					delete[] weights;
					//YoActions()->UnitCommand(bob, ABILITY_ID::MOVE, bob->pos);
					for (int i = 1; i < path.size() && i < 5; i++) {
						auto & pi = path[i];
						YoActions()->UnitCommand(bob, ABILITY_ID::MOVE, { (float)pi.x, (float)pi.y }, true);
					}

				}
				else {
					evading = evade(bob, proxy);					
					if (!evading && (!bob->orders.empty() && bob->orders.begin()->ability_id == ABILITY_ID::HARVEST_GATHER)) {
						YoActions()->UnitCommand(bob, ABILITY_ID::MOVE, proxy);						
					}
				}
			}
		}
		if (scout != nullptr && !YoActions()->isBusy(scout->tag) && frame > 400 && probs.size() > 2) {
			auto & tg = map.getPosition(MapTopology::enemy, MapTopology::main);
			if (!orderBusy(scout)) {
				if (!evade(scout, tg)) {
					if (Distance2D(scout->pos, tg) < 15.0f) {
						auto v = map.FindHardPointsInMinerals(map.getExpansionIndex(MapTopology::enemy, MapTopology::main));
						sortByDistanceTo(v, scout->pos);

						if (minerals >= 100 && pylons.size() < 5) {
							for (auto & p : v) {
								if (Query()->Placement(ABILITY_ID::BUILD_PYLON, p) && placer.PlacementB(Observation()->GetGameInfo(),p,2)) {
									
									float * weights = computeWeightMap(Observation()->GetGameInfo(), Observation()->GetUnitTypeData(), allEnemies());
									auto path = AstarSearchPath(scout->pos, map.FindHardPointsInMinerals(map.getExpansionIndex(map.enemy, map.main))[0], Observation()->GetGameInfo(), weights);
#ifdef DEBUG
									map.debugPath(path, Debug(), Observation());
#endif
									delete[] weights;
									Actions()->UnitCommand(scout, ABILITY_ID::MOVE, scout->pos);
									for (auto & pi : path) {
										Actions()->UnitCommand(scout, ABILITY_ID::MOVE, { (float) pi.x, (float)pi.y }, true);
									}
									Actions()->UnitCommand(scout, ABILITY_ID::BUILD_PYLON, p,true);
									minerals -= 100;
									break;
								}
							}
						}
						else {
							if (!v.empty()) {
								if (Distance2D(scout->pos, v[0]) < 3.0f && v.size() > 1) {
									Actions()->UnitCommand(scout, ABILITY_ID::SMART, (v[1] - tg)*1.2 + tg);
								}
								else {
									Actions()->UnitCommand(scout, ABILITY_ID::SMART, (v[0] - tg)*1.2 + tg);
								}
							}
						}
					}
				}
				//else {
				//	Actions()->UnitCommand(scout, ABILITY_ID::MOVE, tg);
				//	busy(scout->tag);
				//}
			}
		}
	


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






		
		if (bob != nullptr && (nexus==nullptr || nexus->shield / nexus->shield_max < 0.9f) ) {
			minerals -= 400;
			if (minerals <= 0) {	
				auto gws = Observation()->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::PROTOSS_GATEWAY));
				// prefer to cancel from production that is less advanced
				sort(gws.begin(), gws.end(),
					[](auto & a, auto & b)
				{
					if (a->orders.empty() && b->orders.empty()) return a->tag < b->tag;
					if (a->orders.empty()) return true;
					if (b->orders.empty()) return false;
					return a->orders.begin()->progress < b->orders.begin()->progress;
				});
				for (const Unit * gw : gws) {
					if (minerals > 0)
						break; 
					if (!gw->orders.empty()) {
						minerals += 100;
						Actions()->UnitCommand(gw, ABILITY_ID::CANCEL_LAST);
					}
				}
			}
		}
		if (harvesting.getCurrentHarvesters() >= harvesting.getIdealHarvesters()) {
			if (! buildingNexus) {
				auto nexi = Observation()->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::PROTOSS_NEXUS));
				if (none_of(nexi.begin(), nexi.end(), [](auto & n) { return n->build_progress < 1.0f; })) {
					minerals -= 400;
					if (minerals > 0 && !probs.empty()) {
						const std::vector<int> & bydist = map.distanceSortedBasesPerPlayer[map.ourBaseStartLocIndex];
						bool doit = false;
						for (int i = 1, e = bydist.size(); i < e; i++) {
							Point2D p = map.expansions[bydist[i]];
							if (Query()->Placement(ABILITY_ID::BUILD_NEXUS, p)) {
								sortByDistanceTo(probs, p);
								buildingNexus = true;
								Actions()->UnitCommand(probs.front(), ABILITY_ID::MOVE, p);
								Actions()->UnitCommand(probs.front(), ABILITY_ID::BUILD_NEXUS, p, true);
								break;
							}
						}
					}
				}
			}
			else {
				if (frame % 20 == 0) {
					auto nexi = Observation()->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::PROTOSS_NEXUS));
					if (none_of(nexi.begin(), nexi.end(), [](auto & n) { return n->build_progress < 1.0f; })) {
						buildingNexus = false;
					}
				}
			}
		}
		std::unordered_set<Tag> harvesters;
		
		
		for (auto & p : probs) {
			if ( (p != bob && p != scout) || probs.size() <= 2) {
				bool busy = false;
				
				if (IsCarryingVespene(*p)) {
					// or probes that return gas become free on frame they return it
					//YoBot::busy(p->tag);
					if (p->orders.empty()) {
						Actions()->UnitCommand(p, ABILITY_ID::HARVEST_RETURN);
					}
					continue;
				}
				if (p->engaged_target_tag != 0) {
					auto target = Observation()->GetUnit(p->engaged_target_tag);
					if (target != nullptr && (target->vespene_contents != 0 || target->unit_type == UNIT_TYPEID::PROTOSS_ASSIMILATOR ))
						continue;
				}
				for (auto o : p->orders) {
					if (o.ability_id != ABILITY_ID::MOVE && o.ability_id != ABILITY_ID::HARVEST_GATHER && o.ability_id != ABILITY_ID::HARVEST_RETURN) {
						busy = true;
						break;
					}
					if (o.ability_id == ABILITY_ID::HARVEST_GATHER) {
						auto target = Observation()->GetUnit(o.target_unit_tag);
						if (target != nullptr && (target->vespene_contents != 0 || target->unit_type == UNIT_TYPEID::PROTOSS_ASSIMILATOR)) {
							busy = true;
							break;
						}
					}
				}
				if (!busy)
					harvesters.insert(p->tag);
			}
		}		

		
		if (minerals >= 100 && Observation()->GetFoodCap() == 15 && pylons.empty() && nexus != nullptr) {
			const Unit* min = FindNearestMineralPatch(nexus->pos);
			auto ptarg = nexus->pos + (nexus->pos - min->pos);
			auto probes = Observation()->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::PROTOSS_PROBE));
			int i = 0;
			for (auto p : probes) {
				if (i++ < 3)
					continue;
				if (p != bob) {
					Actions()->UnitCommand(p, ABILITY_ID::BUILD_PYLON, map.FindHardPointsInMinerals(map.FindNearestBaseIndex(p->pos))[1]);
					harvesters.erase(p->tag);
					minerals -= 100;
					break;
				}
			}
		}

		if (minerals >= 100 && frame % 10 == 0) {
			// let each base have at least one pylon
			for (auto & strat : harvesting.getChildren()) {
				if (strat.getCurrentHarvesters() >= 17) {					
					auto & pos = strat.getPylonPos();
					if (placer.PlacementB(Observation()->GetGameInfo(),pos,2) && Query()->Placement(ABILITY_ID::BUILD_PYLON,pos)) {
						auto  p = FindNearestUnit(pos,probs);
						Actions()->UnitCommand(p, ABILITY_ID::MOVE, pos);
						Actions()->UnitCommand(p, ABILITY_ID::BUILD_PYLON, pos,true);
						harvesters.erase(p->tag);
						minerals -= 100;
					}
				}
			}
		}


		if (needCannons) {
			TryBuildCannons(pylons,harvesters);
		}
		// we get undesirable double commands if we do this each frame
		if (frame % 3 == 0) {
			TryBuildSupplyDepot(pylons, evading, harvesters);
			TryBuildBarracks(pylons, evading, harvesters);			
		}
		TryBuildUnits();

		auto estimated = estimateEnemyStrength();
		if (estimated == 0) {
			criticalZeal = 0;
		} else {
			criticalZeal = std::max(7, Observation()->GetFoodUsed() / 5);
		}
		if (Observation()->GetArmyCount() >= criticalZeal || Observation()->GetFoodUsed() >= 195) {
			const GameInfo& game_info = Observation()->GetGameInfo();			
			auto tg = target;
			if (target == proxy) {
				tg = defensePoint(proxy);
			}

			for (const auto & unit : Observation()->GetUnits(Unit::Alliance::Self, [](auto & u) { return IsArmyUnitType(u.unit_type); })) {
				if (unit->orders.size() == 0 && !YoActions()->isBusy(unit->tag)) {
					if (Distance2D(unit->pos, target) < 15.0f) {
						target = proxy;
						Units list;
						for (auto & p : allEnemies()) {
							auto & u = p.second;
							if (IsBuilding(u->unit_type) && !u->is_flying) {
								list.push_back(u);
							}
						}
						if (list.size() != 0) {
							int targetU = GetRandomInteger(0, list.size() - 1);
							if (!list[targetU]->is_flying) {
								Actions()->UnitCommand(unit, ABILITY_ID::ATTACK_ATTACK, list[targetU]->pos);
								continue;
							}
						}
						if (estimated <= 5) {
							int targetU = GetRandomInteger(0, map.expansions.size() - 1);
							Actions()->UnitCommand(unit, ABILITY_ID::ATTACK_ATTACK, map.expansions[targetU]);
						}
					}
					else {
						if (Distance2D(unit->pos, tg) < 8.0f) {
							OnUnitHasAttacked(unit);
						}
						else {
							Actions()->UnitCommand(unit, ABILITY_ID::ATTACK_ATTACK, tg);
						}						
					}
				}
			}			
		}
		else {
			auto tg = defensePoint(proxy);
			for (const auto & unit : Observation()->GetUnits(Unit::Alliance::Self, [](auto & u) { return IsArmyUnitType(u.unit_type); })) {
				if (unit->orders.size() == 0 && !YoActions()->isBusy(unit->tag)) {
					if (Distance2D(unit->pos, tg) < 8.0f) {
						OnUnitHasAttacked(unit);
					}
					else {
						Actions()->UnitCommand(unit, ABILITY_ID::ATTACK_ATTACK, tg);
					}
				}
			}
		}
		if (nexus == nullptr && estimated <= 5 && bob != nullptr && minerals >= 0 && !YoActions()->isBusy(bob->tag)) {
			proxy = map.getPosition(MapTopology::ally, MapTopology::proxy);
			if (Query()->Placement(ABILITY_ID::BUILD_NEXUS,proxy)) {
				Actions()->UnitCommand(bob, ABILITY_ID::BUILD_NEXUS, proxy,true);
				placer.reserve(map.getExpansionIndex(MapTopology::ally, MapTopology::proxy));
			}
			// basic counting bob as harvester should be enough
//			if (bob->orders.empty() || none_of(bob->orders.begin(), bob->orders.end(), [](const auto & o) { return  o.ability_id == ABILITY_ID::HARVEST_GATHER || o.ability_id == ABILITY_ID::HARVEST_RETURN; })) {
//				Actions()->UnitCommand(bob, ABILITY_ID::HARVEST_GATHER, FindNearestMineralPatch(proxy), true);
//				busy(bob->tag);
//			}
		};
		

		auto ourUnits = Observation()->GetUnits(Unit::Alliance::Self, [](const Unit & u) { return u.weapon_cooldown == 0 && (isStaticDefense(u.unit_type) || !IsBuilding(u.unit_type)); });
		auto list = TryImmediateAttack(ourUnits);
		for (auto index : list) {
			harvesters.erase(ourUnits[index]->tag);
		}
		
		if (nexus != nullptr && frame % 3 == 0) {
			auto probes = Observation()->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::PROTOSS_PROBE));
			Units nexi = Observation()->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::PROTOSS_NEXUS));
			for (auto probe : probes) {
				if (YoActions()->isBusy(probe->tag)) {
					continue;
				}
				auto nexus = harvesting.getNexusFor(probe->tag);
				if (nexus == nullptr && ! nexi.empty()) {
					sortByDistanceTo(nexi, probe->pos);
					nexus = nexi.front();
				}

				if (nexus != nullptr) {
					auto d = Distance2D(nexus->pos, probe->pos);
					if (probe != bob && probe != scout && d > 9.0f) {
						auto min = FindNearestMineralPatch(nexus->pos);
						auto dm = Distance2D(nexus->pos, min->pos);
						if (dm <= 10.0f && !buildOrderBusy(probe)
							&& std::any_of(probe->orders.begin(), probe->orders.end(),
								[](auto & o) { return o.ability_id == ABILITY_ID::ATTACK; })) {
							Actions()->UnitCommand(probe, ABILITY_ID::HARVEST_GATHER, min);
						}
					}
				}
			}
/*			if (bob != nullptr && nexus != nullptr && probes.size() == 1 && minerals < 50 && estimated < 5 && ! isBusy(bob->tag)) {
				if (!IsCarryingMinerals(*bob)) {
					Actions()->UnitCommand(bob, ABILITY_ID::HARVEST_GATHER, min);
				}
				else {
					Actions()->UnitCommand(bob, ABILITY_ID::HARVEST_RETURN);
				}
				busy(bob->tag);
			}
			*/
			auto ass = Observation()->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::PROTOSS_ASSIMILATOR));
			if ( (Observation()->GetArmyCount() >= maxZeal || minerals >= 500 || needCannons) && minerals >= 75 && ass.size() < 2 && nexus != nullptr && ( harvesting.getIdealHarvesters()- harvesting.getCurrentHarvesters() < 3 || minerals >= 500)) {
				
				// one gas only on low pop
				if (Observation()->GetArmyCount() >= 10 || ass.size() == 0) {
					auto g = FindNearestVespeneGeyser(nexus->pos, ass);
					if (!probes.empty() && g != nullptr) {
						auto p = chooseClosest(g, probes);
						harvesters.erase(p->tag);
						auto min = FindNearestMineralPatch(p->pos);
						Actions()->UnitCommand(p, ABILITY_ID::BUILD_ASSIMILATOR, g);
						Actions()->UnitCommand(p, ABILITY_ID::HARVEST_GATHER, min, true);
						minerals -= 75;
					}
				}
			}
			for (const auto & a : ass) {
				if (a->build_progress < 1.0f) {
					continue;
				}
				if (gas < 400 && a->assigned_harvesters < 3 && nexus != nullptr && (harvesting.getCurrentHarvesters() >= 18 || harvesting.getIdealHarvesters() - harvesting.getCurrentHarvesters() < 3 || minerals >=200)) {
					auto probesc = probes;
					probesc.erase(
						remove_if(probesc.begin(), probesc.end(), [this,a](const Unit * u) { return IsCarryingVespene(*u) || IsCarryingMinerals(*u) || u->engaged_target_tag == a->tag || !u->orders.empty() && (*u->orders.begin()).target_unit_tag == a->tag || YoActions()->isBusy(u->tag); })
						, probesc.end());
					
					if (!probesc.empty()) {
						auto p = chooseClosest(a, probesc);
						harvesters.erase(p->tag);
						Actions()->UnitCommand(p, ABILITY_ID::HARVEST_GATHER, a);
					}
				}
				else if (a->assigned_harvesters > 3 || ( a->assigned_harvesters > 0 &&  gas > 400)) {
					auto probesc = probes;
					probesc.erase(
						remove_if(probesc.begin(), probesc.end(), [a](const Unit * u) { 
						return !u->orders.empty() && (*u->orders.begin()).target_unit_tag != a->tag;
						 })
						, probesc.end());
					if (!probesc.empty()) {
						auto min = FindNearestMineralPatch(a->pos);
						auto p = chooseClosest(min, probesc);
						Actions()->UnitCommand(p, ABILITY_ID::HARVEST_GATHER, min);
					}
					
				}
			}
		}

		/*
		Units zeals =  Observation()->GetUnits(Unit::Alliance::Self, [](const Unit & u) { return  u.unit_type == UNIT_TYPEID::PROTOSS_ZEALOT ; });
		if (!zeals.empty()) {
			sortByDistanceTo(zeals, target);
			AllocateAttackers((*zeals.begin())->pos);
		}
		*/
		//if (frame % 100 == 0)
			
		// pokeMap();
		Units har;
		har.reserve(harvesters.size());
		for (auto t : harvesters) {
			auto u = Observation()->GetUnit(t);
			if (u!=nullptr)
				har.push_back(u);
		}
		if (har.empty()) {
			if (bob != nullptr && !YoActions()->isBusy(bob->tag)) {
				har.push_back(bob);
			}
			if (scout != nullptr && !YoActions()->isBusy(scout->tag)) {
				har.push_back(scout);
			}
		}
		auto mainpos = map.getPosition(MapTopology::ally, MapTopology::main);
		auto enemies = Observation()->GetUnits(Unit::Alliance::Enemy, [mainpos](const Unit & e) { return DistanceSquared2D(e.pos, mainpos) < 100.0f; });
		if (nexus != nullptr) {
			if (enemies.size() >= 3) {
				harvesting.OnStep(har, Observation(), YoActions(), true);
			}
			else {
				harvesting.OnStep(har, Observation(), YoActions(), false);
			}
		}
		else {
			if (frame % 3 == 0) {
				for (auto p : har) {
					evade(p, proxy);
				}
			}
		}
#ifdef DEBUG
		harvesting.PrintDebug(Debug(),Observation());
#endif

	}

	void pokeMap() {
		auto base = map.getPosition(MapTopology::enemy, MapTopology::main);
		// build the distance matrix between units
		std::vector<Point3D> points;
		points.reserve(200);
		float delta = 2;
		float delta2 = delta / sqrt(2);
		

		std::vector <sc2::QueryInterface::PathingQuery> queries;

		for (int x = -10; x <= 10; x+=2) {
			for (int y = -10; y <= 10; y+=2) {
				auto pos = base + Point3D(x, y, 0);
				std::vector<Point2D> outs = {
					pos + Point2D(delta,0.0f), pos + Point2D(0.0f,delta) , pos + Point2D(-delta,0.0f) , pos + Point2D(0.0f,-delta) ,
					pos + Point2D(delta2,delta2), pos + Point2D(delta2,-delta2) ,pos + Point2D(-delta2,delta2) , pos + Point2D(-delta2,-delta2)
				};
				for (const auto & p : outs) {
					queries.push_back({ 0, pos, p });
				}
				points.emplace_back(pos);
			}
		}
		
		// send the query and wait for answer
		std::vector<float> distances = Query()->PathingDistance(queries);
#ifdef DEBUG
		//if (debug != nullptr) {
		int i = 0;
		for (const auto & q : queries) {
			if (distances[i]==0.0f) 
				Debug()->DebugLineOut(Point3D(q.start_.x, q.start_.y, base.z +0.5f), Point3D(q.end_.x, q.end_.y, base.z + 0.5f), distances[i] == 0.0f ? Colors::Red : Colors::Green);
			i++;
		}
#endif
		
	}

	void AllocateAttackers(const Point2D & npos) {
		Units inRange = Observation()->GetUnits([npos](const Unit & u) { return  u.alliance != Unit::Alliance::Neutral && !u.is_flying && Distance2D(u.pos, npos) < 20.0f ; });
		
		// indexes go to here
		Units allUnits;
		allUnits.reserve(inRange.size());

		// enemies
		std::vector<int> buildings;
		std::vector<int> CCs;
		std::vector<int> drones;
		std::vector<int> meleeDef;
		std::vector<int> rangeDef;
		std::vector<int> staticD;

		// allies
		std::vector<int> probes;
		std::vector<int> zeals;
		for (const auto & u : inRange) {
			int n = allUnits.size();
			if (u->alliance == Unit::Alliance::Enemy) {			
				if (u->unit_type == UNIT_TYPEID::ZERG_EGG || u->unit_type == UNIT_TYPEID::ZERG_BROODLING || u->unit_type == UNIT_TYPEID::ZERG_LARVA) {
					continue;
				}
				if (isStaticDefense(u->unit_type)) {
					staticD.push_back(n);
				}
				else if (IsCommandStructure(u->unit_type)) {
					CCs.push_back(n);
				}
				else if (IsBuilding(u->unit_type)) {
					buildings.push_back(n);
				}
				else if (IsWorkerType(u->unit_type)) {
					drones.push_back(n);
				}
				else if (IsRangedUnit(u->unit_type)) {
					rangeDef.push_back(n);
				} 
				else {
					// it's a melee unit
					meleeDef.push_back(n);
				}
			}
			else if (u->alliance == Unit::Alliance::Self) {
				if (u->unit_type == UNIT_TYPEID::PROTOSS_ZEALOT) {
					zeals.push_back(n);
				}
				else if (u->unit_type == UNIT_TYPEID::PROTOSS_PROBE) {
					probes.push_back(n);
				}
			}
			allUnits.push_back(u);
		}
		
		// build the distance matrix between units
		std::vector<Point3D> points;
		points.reserve(allUnits.size());
		for (auto u : allUnits) {
			points.push_back(u->pos);
		}
		auto matrix = computeDistanceMatrix(points,Query());
#ifdef DEBUG
		//if (debug != nullptr) {
		auto sz = points.size();
		if (sz >= 4) {
			for (int i = 0; i < sz; i++) {
				for (int j = i + 1; j < sz; j++) {
					auto color = (matrix[i*sz + j] > 100000.0f) ? Colors::Red : Colors::Green;
					Debug()->DebugLineOut(points[i] + Point3D(0, 0, 0.5f), points[j] + Point3D(0, 0, 0.5f), color);
					Debug()->DebugTextOut(std::to_string(matrix[i*sz + j]), (points[i] + Point3D(0, 0, 0.2f) + points[j]) / 2, Colors::Green);
				}
			}
			Debug()->SendDebug();
		}
		//}
		
#endif // DEBUG
		// std::vector<int> targets = allocateTargets(probes, mins, [](const Unit *u) { return  2; });
/*		for (int att = 0, e = targets.size(); att < e; att++) {
			if (targets[att] != -1) {
				Actions()->UnitCommand(probes[att], ABILITY_ID::SMART, mins[targets[att]]);
			}
		}*/


	}



	

	

	std::vector<int> TryImmediateAttack(const Units & zeals) {
		std::vector<int> attacking;
		int index = -1;
		for (const auto & z : zeals) {
			index++;
			// weak drones only do this if close to nexus
			if (z->unit_type == UNIT_TYPEID::PROTOSS_PROBE &&  ( (z->shield + z->health <= 10) || (nexus==nullptr || Distance2D(nexus->pos, z->pos) > 5.0f) || z == bob || z == scout)) {
				continue;
			} 
			if (YoActions()->isBusy(z->tag)) {
				continue;
			}
						
			float attRange = getRange(z, Observation()->GetUnitTypeData());

			auto enemies = FindEnemiesInRange(z->pos, attRange);

			// make sure there is no more than 60 degree turn to attack it
			const float length =attRange;
			sc2::Point3D p1 = z->pos;			
			p1.x += length * std::cos(z->facing);
			p1.y += length * std::sin(z->facing); 
			enemies.erase(
				std::remove_if(enemies.begin(), enemies.end(), [p1, attRange](const auto & u) {  return Distance2D(u->pos, p1) > attRange + u->radius;  })
				, enemies.end());

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
					attacking.push_back(index);
				}
				else {
					auto order = z->orders.front();
					Actions()->UnitCommand(z, ABILITY_ID::ATTACK_ATTACK, t);
					sendUnitCommand(z, order, true);
					attacking.push_back(index);
				}
				
			}
		}
		return attacking;
	}

	// In your bot class.
	virtual void OnUnitIdle(const Unit* unit) final {
		if (unit == scout) {
			
			if (Observation()->GetGameInfo().enemy_start_locations.size() > 1) {
				if (proxy != target && nexus != nullptr) {
					Actions()->UnitCommand(unit, ABILITY_ID::SMART, FindNearestMineralPatch(nexus->pos), false);
					scout = nullptr;
				}
				else {
					int s = 0;
					for (const auto & pos : Observation()->GetGameInfo().enemy_start_locations) {
						auto vis = Observation()->GetVisibility(pos);
						if (vis == sc2::Visibility::Fogged || vis == sc2::Visibility::Visible) {
							s++;
						}
					}

					//scouted++;
					if (s == Observation()->GetGameInfo().enemy_start_locations.size() - 1 && nexus != nullptr) {
						target = Observation()->GetGameInfo().enemy_start_locations[s];
						Actions()->UnitCommand(unit, ABILITY_ID::SMART, FindNearestMineralPatch(nexus->pos), false);
						scout = nullptr;
					}
					else {
						Actions()->UnitCommand(unit, ABILITY_ID::SMART, Observation()->GetGameInfo().enemy_start_locations[s], false);
					}
				}
				return;
			} else {
				evade(unit, map.getPosition(MapTopology::enemy, MapTopology::main));
			}
		}
		
		if (unit == bob && !YoActions()->isBusy(bob->tag)) {
			if (IsCarryingMinerals(*bob) && nexus !=nullptr) {
				Actions()->UnitCommand(bob, ABILITY_ID::HARVEST_RETURN, nexus);
			}
			else {				
				auto v = map.FindHardPointsInMinerals(map.FindNearestBaseIndex(proxy));
				sortByDistanceTo(v, map.getPosition(MapTopology::enemy, MapTopology::main));
				std::reverse(v.begin(), v.end());
				for (auto p : v) {
					if (Pathable(Observation()->GetGameInfo(), p)) {
						Actions()->UnitCommand(unit, ABILITY_ID::PATROL, p);
						break;
					}
				}
			}
			return;
		}
		switch (unit->unit_type.ToType()) {
		case UNIT_TYPEID::PROTOSS_NEXUS: {			

			//if (unit->assigned_harvesters < unit->ideal_harvesters)
			//	Actions()->UnitCommand(unit, ABILITY_ID::TRAIN_PROBE);
			break;
		}
		case UNIT_TYPEID::PROTOSS_GATEWAY: {
			//			if (Observation()->GetFoodCap() >= Observation()->GetFoodUsed() - 2)
			//				Actions()->UnitCommand(unit, ABILITY_ID::TRAIN_ZEALOT);
			break;
		}
		case UNIT_TYPEID::PROTOSS_ADEPT:
		case UNIT_TYPEID::PROTOSS_IMMORTAL: 
		case UNIT_TYPEID::PROTOSS_ZEALOT: {
			if (Observation()->GetArmyCount() >= criticalZeal || Observation()->GetFoodUsed() >= 195 /*&& staticd < 4 * Observation()->GetArmyCount()*/) {
				const GameInfo& game_info = Observation()->GetGameInfo();
				auto tg = target;
				if (target == proxy) {
					tg = defensePoint(proxy);
				}
				if (Distance2D(unit->pos, tg) < 8.0f) {
					OnUnitHasAttacked(unit);
				}
				else {
					Actions()->UnitCommand(unit, ABILITY_ID::ATTACK_ATTACK, tg);
				}
			}
			break;
		}
		case UNIT_TYPEID::PROTOSS_VOIDRAY: {
			bool clearing = false;
			for (auto u : allEnemies()) {
				if (u.second->is_flying) {
					Actions()->UnitCommand(unit, ABILITY_ID::ATTACK, u.second->pos);
					clearing = true;
					break;
				}
			}
			if (!clearing) {
				if (Observation()->GetArmyCount() >= criticalZeal) {
					auto tg = target;
					if (target == proxy) {
						tg = defensePoint(proxy);
					}
					Actions()->UnitCommand(unit, ABILITY_ID::ATTACK_ATTACK, tg);
				}
			}
			break;
		}
		case UNIT_TYPEID::PROTOSS_PROBE: {
			auto n = harvesting.getNexusFor(unit->tag);
			if (n  != nullptr) {
				const Unit* mineral_target = FindNearestMineralPatch(n->pos);
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
	
	const Unit * nexus = nullptr;
	
	
	bool baseRazed ;
	
	long int frame = 0;
	Race enemyRace;

	size_t CountUnitType(UNIT_TYPEID unit_type) {
		return Observation()->GetUnits(Unit::Alliance::Self, IsUnit(unit_type)).size();
	}

	void TryBuildUnits() {
		auto & probs = Observation()->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::PROTOSS_PROBE));
		if (probs.size() < 85) {
			for (auto & nexus : Observation()->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::PROTOSS_NEXUS))) {
				if (nexus != nullptr && nexus->orders.empty() && supplyleft >= 1 && minerals >= 50 && !YoActions()->isBusy(nexus->tag)) {
					auto cur = harvesting.getCurrentHarvesters();
					// 85 probes is plenty
					if (cur < harvesting.getIdealHarvesters()) {
						Actions()->UnitCommand(nexus, ABILITY_ID::TRAIN_PROBE);
						if (nexus->energy >= 50 && supplyleft > 1 && harvesting.getCurrentHarvesters() < 16) {
							Actions()->UnitCommand(nexus, (ABILITY_ID)3755, nexus, true);
						}
						minerals -= 50;
						supplyleft -= 1;
					}
				}
			}
		}
		
		if (frame % 3 == 0) {
			auto ta = Observation()->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::PROTOSS_TWILIGHTCOUNCIL));
			if (!ta.empty()) {
				auto & f = *ta.begin();
				if (f->build_progress == 1.0f && !YoActions()->isBusy(f->tag) && !orderBusy(f) && minerals >= 100 && gas >= 100) {
					Actions()->UnitCommand(f, ABILITY_ID::RESEARCH_CHARGE);
					minerals -= 100;
					gas -= 100;
				}
				chrono(f);
			}

			auto forge = Observation()->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::PROTOSS_FORGE));
			if (!forge.empty()) {
				auto & f = *forge.begin();
				bool b = doUpgrade(f);
				chrono(f);
			}
			auto robos = Observation()->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::PROTOSS_ROBOTICSFACILITY));
			for (auto & ro : robos) {
				chrono(ro);
			}
		}
		chronoBuild(UNIT_TYPEID::PROTOSS_ROBOTICSFACILITY, ABILITY_ID::TRAIN_IMMORTAL, 4, 250, 150);
		chronoBuild(UNIT_TYPEID::PROTOSS_STARGATE, ABILITY_ID::TRAIN_VOIDRAY, 4, 250, 150);
		if (CountUnitType(UNIT_TYPEID::PROTOSS_ZEALOT) <= 25)
			chronoBuild(UNIT_TYPEID::PROTOSS_GATEWAY, ABILITY_ID::TRAIN_ZEALOT, 2, 100, 0);
		
	}

	int weapon_level=0;
	int armor_level=0;
	void OnUpgradeCompleted(UpgradeID upgrade) {
		switch (upgrade.ToType()) {
		case UPGRADE_ID::PROTOSSGROUNDWEAPONSLEVEL1: 
		case UPGRADE_ID::PROTOSSGROUNDWEAPONSLEVEL2: 
		case UPGRADE_ID::PROTOSSGROUNDWEAPONSLEVEL3: {
			weapon_level++;
			break;
		}
		case UPGRADE_ID::PROTOSSGROUNDARMORSLEVEL1:
		case UPGRADE_ID::PROTOSSGROUNDARMORSLEVEL2: 
		case UPGRADE_ID::PROTOSSGROUNDARMORSLEVEL3: {
			armor_level++;
			break;
		}
		default:
			break;
		}
	}

	bool doUpgrade(const Unit * f) {
		
		if (f->build_progress == 1.0f && !YoActions()->isBusy(f->tag) && !orderBusy(f)) {
			AvailableAbilities abilities = Query()->GetAbilitiesForUnit(f);	
			if (abilities.abilities.empty()) {
				return false;
			}
			for (const auto & upg :
				{ ABILITY_ID::RESEARCH_PROTOSSGROUNDWEAPONS
				, ABILITY_ID::RESEARCH_PROTOSSGROUNDARMOR
				}) {
				int todo = weapon_level + (int)ABILITY_ID::RESEARCH_PROTOSSGROUNDWEAPONSLEVEL1;
				if (upg == ABILITY_ID::RESEARCH_PROTOSSGROUNDARMOR) {
					todo = armor_level + (int)ABILITY_ID::RESEARCH_PROTOSSGROUNDARMORLEVEL1;
				}
				for (const auto& ability : abilities.abilities) {
					if (ability.ability_id == upg) {
						Actions()->UnitCommand(f, upg);
						auto upgrades = Observation()->GetUpgradeData();						
						auto & uptodo = std::find_if(upgrades.begin(), upgrades.end(), [&todo](const UpgradeData & u) { return u.ability_id == todo; });
						minerals -= uptodo->mineral_cost;
						gas -= uptodo->vespene_cost;
						return true;
					}
				}
			}
		}
		return false;
	}

	void chrono(const Unit * f) {
		bool isChronoed = sc2util::isChronoed(f);
		if (!isChronoed && !YoActions()->isBusy(f->tag) && !f->orders.empty()) {
			for (auto nex : Observation()->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::PROTOSS_NEXUS))) {
				if (nex->energy >= 50 && ! YoActions()->isBusy(nex->tag)) {
					Actions()->UnitCommand(nex, (ABILITY_ID)3755, f);
					break;
				}
			}
		}
	}

	int chronoBuild(UNIT_TYPEID builder, ABILITY_ID tobuild, int supplyreq, int minsreq, int gasreq) {
		int nbuilt = 0;
		if (supplyleft >= supplyreq && minerals >= minsreq && gas >= gasreq) {
			for (auto & gw : Observation()->GetUnits(Unit::Alliance::Self, IsUnit(builder))) {
				if (gw->build_progress < 1) {
					continue;
				}
				if (!gw->is_powered) {
					continue;
				}
				if (YoActions()->isBusy(gw->tag)) {
					continue;
				}
				if (gw->orders.size() == 0) {
					Actions()->UnitCommand(gw, tobuild);
					nbuilt++;
					supplyleft -= supplyreq;
					minerals -= minsreq;
					gas -= gasreq;
				}
				else {
					chrono(gw);
				}
				if (supplyleft < supplyreq || minerals < minsreq || gas < gasreq) {
					break;
				}
			}
		}
		return nbuilt;
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

	float damageRatio(const Unit * unit) {
		auto ratio = (unit->health + unit->shield) / (unit->health_max + unit->shield_max);

		return ratio / unit->build_progress;
	}


	bool TryBuildCannons(const Units & pylons, std::unordered_set<Tag> & harvesters) {
		auto forges = Observation()->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::PROTOSS_FORGE));
		if (forges.empty() || (*forges.begin())->build_progress < 1) {
			return false;
		}
		auto cann = Observation()->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::PROTOSS_PHOTONCANNON));
		if (false && cann.size() < 2 ) {
			minerals -= 150;
			const Unit * builder;
			// If a unit already is building a supply structure of this type, do nothing.
			// Also get an scv to build the structure.
			auto tobuild = ABILITY_ID::BUILD_PHOTONCANNON;
			Units probes = Observation()->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::PROTOSS_PROBE));
			if (any_of(probes.begin(), probes.end(), [&](const auto & p)
			{
				return any_of(p->orders.begin(), p->orders.end(), [&](const auto & o)
				{ return o.ability_id == tobuild; });
			})) {
				return false;
			}
			if (nexus == nullptr || probes.empty()) {
				return false;
			}
			sortByDistanceTo(probes, nexus->pos);
			builder = *probes.begin();
			harvesters.erase(builder->tag);
			
			auto & points = map.FindHardPointsInMinerals(map.FindNearestBaseIndex(nexus->pos));
			auto p = points[0];
			if (Query()->Placement(tobuild, p) && placer.PlacementB(Observation()->GetGameInfo(),p,2)) {
				Actions()->UnitCommand(builder, tobuild, p);
				return true;
			} 
			p = points[points.size()-1];
			if (Query()->Placement(tobuild, p)) {
				Actions()->UnitCommand(builder, tobuild, p);
				return true;
			}
			p = points[points.size() - 2];
			if (Query()->Placement(tobuild, p)) {
				Actions()->UnitCommand(builder, ABILITY_ID::BUILD_PYLON, p);
				minerals += 50;
				return true;
			}
			minerals += 150;
		}
		return false;
	}

	bool TryBuildSupplyDepot(const Units & pylons,  bool evading, std::unordered_set<Tag> & harvesters) {
		const ObservationInterface* observation = Observation();
		if (minerals < 100) {
			return false;
		}
		auto tobuild = ABILITY_ID::BUILD_PYLON;
		// Try and build a depot. Find a random SCV and give it the order.
		const Unit * unit_to_build=nullptr;
		// If a unit already is building a supply structure of this type, do nothing.
		// Also get an scv to build the structure.
		if (minerals >= 400) {
			Units probes = Observation()->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::PROTOSS_PROBE));
			//auto it = find(probes.begin(), probes.end(), bob);
			//if (it != probes.end()) {
			//	probes.erase(it);
			//}
			if (any_of(probes.begin(), probes.end(), [&](const auto & p)
			{
				return any_of(p->orders.begin(), p->orders.end(), [&](const auto & o)
				{ return o.ability_id == tobuild; });
			})) {
				return false;
			}
			if (nexus == nullptr || probes.empty()) {
				return false;
			}
			sortByDistanceTo(probes, nexus->pos);
			for (auto & p : probes) {
				if (YoActions()->isBusy(p->tag)) {
					continue;
				}
				else {
					unit_to_build = p;
					break;
				}
			}
			if (unit_to_build == nullptr) {
				return false;
			}
			harvesters.erase(unit_to_build->tag);
		}
		else {
			if (bob == nullptr) {
				return false;
			}

			if (orderBusy(bob)) {
				return false;
			}
			unit_to_build = bob;
		}
		
		if (unit_to_build == bob) {
			if ((pylons.size() == 0 || (pylons.size() == 1 && Distance2D(pylons.front()->pos, proxy) > 5))) {
				minerals -= 100;
				auto v = map.FindHardPointsInMinerals(map.FindNearestBaseIndex(proxy));
				sortByDistanceTo(v, map.getPosition(MapTopology::enemy, MapTopology::main));
				for (auto & pos : v) {
					if (placer.PlacementB(observation->GetGameInfo(), pos, 2)) {
						Actions()->UnitCommand(bob,
							ABILITY_ID::BUILD_PYLON,
							pos);
						return true;
					}
				}
			}
		}

		bool needSupport = false;
		int building = 0; 
		for (const auto& unit : pylons) {
			if (unit->build_progress < 1 && damageRatio(unit) < 1) {
				needSupport = true;
			}
			if (unit->build_progress == 1 && unit->shield < unit->shield_max) {
				needSupport = true;
			}
			if (unit->build_progress < 1) {
				building++;
			}
		}

		// If we are not supply capped, don't build a supply depot.
		if (!needSupport) {
			if (observation->GetFoodUsed() < 20 && supplyleft > 2)
				return false;
			if (observation->GetFoodUsed() >= 20 && observation->GetFoodUsed() <= 90 && supplyleft > 6)
				return false;
			if (observation->GetFoodUsed() > 90 && supplyleft > 10)
				return false;
			if (building > 0 && observation->GetFoodUsed() <= 90)
				return false;
			if (building > 1)
				return false;
			if (observation->GetFoodCap() == 200) {
				return false;
			}
		}
		else {
			if (building >= 3 || supplyleft >= 12) {
				return false;
			}
		}
			
		if (orderBusy(unit_to_build)) {
			return false;
		}
		
		float rx = GetRandomScalar();
		float ry = GetRandomScalar();

		if (false && CountUnitType(UNIT_TYPEID::PROTOSS_PYLON) == 0) {
			minerals -= 100;
			Actions()->UnitCommand(unit_to_build,
				ABILITY_ID::BUILD_PYLON,
				Point2D(unit_to_build->pos.x + rx * 5.0f, unit_to_build->pos.y + ry * 5.0f));
		} else {
			Units gws = Observation()->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::PROTOSS_GATEWAY));
			if (gws.size() != 0 || needSupport) {
				for (auto & gw : gws) {
					if (gw->build_progress ==  1 && !gw->is_powered && ((! evading && Distance2D(gw->pos,bob->pos) < 20.0f) || (evading && Distance2D(gw->pos, bob->pos) < 8.0f))) {
						
						for (int i = 0; i < 30; i++) {
							rx = GetRandomScalar();
							ry = GetRandomScalar();
							auto candidate = Point2D(gw->pos.x + rx * 5.0f, gw->pos.y + ry * 5.0f);
							if (placer.PlacementB(Observation()->GetGameInfo(), candidate,2) && Query()->Placement(ABILITY_ID::BUILD_PYLON, candidate)) {
								Actions()->UnitCommand(unit_to_build,
									ABILITY_ID::MOVE,
									candidate);
								Actions()->UnitCommand(unit_to_build,
									ABILITY_ID::BUILD_PYLON,
									candidate,true);
								minerals -= 100;
								return true;
							}
						}
					}
				}

				bool good = false;
				int iter = 0;
				auto candidate = Point2D(proxy.x + rx * (8.0f + gws.size() * 2), proxy.y + ry * (8.0f + gws.size() * 2));
				// see if a nasty spot is available close by
				if (proxy != map.getPosition(MapTopology::ally, MapTopology::main)) {
					std::vector<Point2D> targets = map.FindHardPointsInMinerals(map.FindNearestBaseIndex(bob->pos));
					sortByDistanceTo(targets, bob->pos);
					for (auto & p : targets) {
						if (Distance2D(p, bob->pos) < 18.0f && placer.PlacementB(observation->GetGameInfo(), p,2) && Query()->Placement(ABILITY_ID::BUILD_PYLON, p)) {
							candidate = p;
							good = true;
							break;
						}
					}
				}
				if (evading || needSupport) {					
					if (Distance2D(candidate, bob->pos) > 8.0f) {
						candidate = Point2D(unit_to_build->pos.x + rx * 3.0f, proxy.y + ry * 3.0f);
					}
				}
				while (!good && iter++ < 25) {

					if (placer.PlacementB(Observation()->GetGameInfo(), candidate,2)) {

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

						if (spaces > 3 && Query()->PathingDistance(bob, candidate) < 20) {
							good = true;
						}
					}
					if (!good) {
						rx = GetRandomScalar();
						ry = GetRandomScalar();
						candidate = Point2D(proxy.x + rx * (8.0f + gws.size() * 2), proxy.y + ry * (8.0f + gws.size() * 2));
						if (evading || needSupport) {
							candidate = Point2D(unit_to_build->pos.x + rx * 3.0f, proxy.y + ry * 3.0f);
						}
					}
				}

				if (good) {
					minerals -= 100;
					Actions()->UnitCommand(unit_to_build,
						ABILITY_ID::MOVE,
						candidate);
					Actions()->UnitCommand(unit_to_build,
						ABILITY_ID::BUILD_PYLON,
						candidate, true); 
					//Actions()->UnitCommand(unit_to_build,
					//	ABILITY_ID::BUILD_PYLON, candidate);
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
				while (!good && iter++ < 25) {

					if (placer.PlacementB(Observation()->GetGameInfo(), candidate, 3)) {
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
					}
					if (!good) {
						rx = GetRandomScalar();
						ry = GetRandomScalar();
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

	int criticalZeal =7;
	const int maxZeal = 18;

	bool orderBusy(const Unit * bob) {
		return std::any_of(bob->orders.begin(), bob->orders.end(), [](const auto & o) {
			return (o.ability_id != ABILITY_ID::PATROL && o.ability_id != ABILITY_ID::MOVE && o.ability_id != ABILITY_ID::HARVEST_GATHER);
		});
	}
	bool buildOrderBusy(const Unit * bob) {
		return std::any_of(bob->orders.begin(), bob->orders.end(), [](const auto & o) {
			return (o.ability_id != ABILITY_ID::PATROL && o.ability_id != ABILITY_ID::MOVE && o.ability_id != ABILITY_ID::HARVEST_GATHER && o.ability_id != ABILITY_ID::ATTACK);
		});
	}

	bool TryBuildBarracks(Units & pylons, bool evading , std::unordered_set<Tag> & harvesters) {


		if (pylons.empty() || (pylons.size() == 1 && pylons.front()->build_progress < 1)) {
			return false;
		}

		auto gates = Observation()->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::PROTOSS_GATEWAY));
		int gws = 0;
		for (auto g : gates) {
			if (Distance2D(g->pos, proxy) <= 20.0f) {
				gws++;
			}
		}

		int sgs = CountUnitType(UNIT_TYPEID::PROTOSS_STARGATE);
		int zeals = CountUnitType(UNIT_TYPEID::PROTOSS_ZEALOT);
		ABILITY_ID tobuild = ABILITY_ID::BUILD_GATEWAY;
		if (needCannons && CountUnitType(UNIT_TYPEID::PROTOSS_FORGE) == 0 && gates.size() >= 3) {
			tobuild = ABILITY_ID::BUILD_FORGE;
		} else if (needImmo && CountUnitType(UNIT_TYPEID::PROTOSS_CYBERNETICSCORE) == 0) {
			tobuild = ABILITY_ID::BUILD_CYBERNETICSCORE;
		} else if (needImmo && CountUnitType(UNIT_TYPEID::PROTOSS_TWILIGHTCOUNCIL) == 0) {
			tobuild = ABILITY_ID::BUILD_TWILIGHTCOUNCIL;
		} else if (needImmo && CountUnitType(UNIT_TYPEID::PROTOSS_ROBOTICSFACILITY) == 0) {
			tobuild = ABILITY_ID::BUILD_ROBOTICSFACILITY;
		} else {
			if (gws >= 4 && (zeals < maxZeal && minerals < 500 && gas < 100)) {
				return false;
			}
			if (zeals >= maxZeal && sgs >= 2) {
				return false;
			}
			if ((zeals >= maxZeal || minerals >= 500 || gas >= 100) && nexus != nullptr) {
				// maxZeal (20) zeals is plenty make some VR now
				int cyber = CountUnitType(UNIT_TYPEID::PROTOSS_CYBERNETICSCORE);
				if (gws < 4 && Observation()->GetFoodCap() < 80) {
					tobuild = ABILITY_ID::BUILD_GATEWAY;
				}
				else if (gws < 7 && Observation()->GetFoodCap() >= 80) {
					tobuild = ABILITY_ID::BUILD_GATEWAY;
				}
				else if (CountUnitType(UNIT_TYPEID::PROTOSS_FORGE) == 0 && gws >= 3)
				{
					tobuild = ABILITY_ID::BUILD_FORGE;
				}
				else if (cyber == 0 && zeals >= maxZeal) {
					tobuild = ABILITY_ID::BUILD_CYBERNETICSCORE;
				}
				else if (sgs < 2 && zeals >= maxZeal)
				{
					tobuild = ABILITY_ID::BUILD_STARGATE;
				}
				else
				{
					return false;
				}
			}
		}
		if (tobuild == ABILITY_ID::BUILD_GATEWAY && minerals < 150) {
			return false;
		}
		else if (tobuild == ABILITY_ID::BUILD_CYBERNETICSCORE && minerals < 150) {
			return false;
		}
		else if (tobuild == ABILITY_ID::BUILD_TWILIGHTCOUNCIL && (minerals < 150 || gas < 100) ) {
			return false;
		}

		const Unit * builder;
		// If a unit already is building a supply structure of this type, do nothing.
		// Also get an scv to build the structure.
		if ( (tobuild == ABILITY_ID::BUILD_FORGE || tobuild == ABILITY_ID::BUILD_CYBERNETICSCORE || tobuild == ABILITY_ID::BUILD_TWILIGHTCOUNCIL || minerals >= 400) ) {
			Units probes = Observation()->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::PROTOSS_PROBE));
			auto it = find(probes.begin(), probes.end(), bob);
			if (it != probes.end()) {
				probes.erase(it);
			}
			if (any_of(probes.begin(), probes.end(), [&](const auto & p)
			{
				return any_of(p->orders.begin(), p->orders.end(), [&](const auto & o)
				{ return o.ability_id == tobuild; });
			})) {
				return false;
			}
			if (nexus == nullptr || probes.empty()) {
				return false;
			}
			sortByDistanceTo(probes, nexus->pos);
			builder = *probes.begin();
			harvesters.erase(builder->tag);
		} else {
			if (bob == nullptr) {
				return false;
			}

			if (orderBusy(bob)) {
				return false;
			}
			builder = bob;
		}

		auto info = Observation()->GetGameInfo();
		float maxdist = 15.0f;
		if (FindEnemiesInRange(builder->pos, 8.0f).size() >= 2 || evading) {
			maxdist = 3.0f;
		} else { // try flower positions
			sortByDistanceTo(pylons, builder->pos);
			auto m = pylons.front();
			std::vector<Point2D> cands = { m->pos + sc2::Point2D(2, 0) ,   m->pos + sc2::Point2D(-1, 2),  m->pos + sc2::Point2D(-3, -1), m->pos + sc2::Point2D(0, -3) };			
			std::vector<sc2::QueryInterface::PlacementQuery> queries;
			queries.reserve(4);
			for (int i = 0; i < 4; i++) {
				queries.push_back(sc2::QueryInterface::PlacementQuery(tobuild, cands[i]));
			}
			auto q = Query();
			auto resp = q->Placement(queries);
			for (int i = 0; i < 4; i++) {
				if (resp[i] && placer.PlacementB(info,cands[i],3)) {
					Actions()->UnitCommand(builder,
						tobuild,
						cands[i]);
					return true;
				}
			}

		}

		// try ten positions
		for (int i = 0; i < 25; i++) {
			float rx = GetRandomScalar();
			float ry = GetRandomScalar();		
			Point2D candidate = Point2D(builder->pos.x + rx * maxdist, builder->pos.y + ry * maxdist);
			if (!placer.PlacementB(info, candidate,3)) {
				continue;
			}
			std::vector<sc2::QueryInterface::PlacementQuery> queries;
			queries.reserve(5);
			queries.push_back(sc2::QueryInterface::PlacementQuery(tobuild, candidate));
			queries.push_back(sc2::QueryInterface::PlacementQuery(tobuild, candidate + sc2::Point2D(1, 0)));
			queries.push_back(sc2::QueryInterface::PlacementQuery(tobuild, candidate + sc2::Point2D(0, 1)));
			queries.push_back(sc2::QueryInterface::PlacementQuery(tobuild, candidate + sc2::Point2D(-1, 0)));
			queries.push_back(sc2::QueryInterface::PlacementQuery(tobuild, candidate + sc2::Point2D(0, -1)));

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

			if (spaces > 2) {
				Actions()->UnitCommand(builder,
					tobuild,
					candidate);
				return true;
			}
		}
		return false;
	}


	int estimateEnemyStrength() {
		int str = 0;
		for (auto u : allEnemies()) {
			if (IsArmyUnitType(u.second->unit_type)) {
				str++;
			}				
		}
		return str;
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
			if (distances[i] != 0.0f && distance > distances[i]) {
				distance = distances[i];
				ti = i;
			}
		}
		return pot[ti];
	}

	const Unit* FindNearestMineralPatch(const Point2D& start) {
		auto m = FindNearestUnit(start, Observation()->GetUnits(Unit::Alliance::Neutral, [](const auto & u) { return IsMineral(u.unit_type); }));
		if (m == nullptr) {
			m = FindNearestUnit(start, Observation()->GetUnits(Unit::Alliance::Neutral, [](const auto & u) { return IsMineral(u.unit_type); }), 10000.0);
		}
		return m;
	}

	const Unit* FindNearestVespeneGeyser(const Point2D& start, const Units & ass) {
		// an empty geyser i.e. not occupied by an assimilator, but with gas left in it
		auto geysers = Observation()->GetUnits(Unit::Alliance::Neutral, [ass](const Unit & u) {
			if ( u.vespene_contents==0) {
				return false;
			}			
			for (const auto & a : ass) {
				if (Distance2D(a->pos, u.pos) < 1.0f) {
					return false;
				}
			}
			return true;
		});
		if (!geysers.empty()) {
			return FindNearestUnit(start, geysers);			
		}
		
		return nullptr;
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
