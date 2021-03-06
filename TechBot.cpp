#include "TechBot.h"
#include "sc2api/sc2_action.h"
#include "sc2api/sc2_interfaces.h"
#include "UnitTypes.h"
#include "MapTopology.h"
#include "Placement.h"
#include <iostream>
#include <fstream>


bool isRelevant(const sc2::UnitTypeData & unitdesc, const std::unordered_map<int, int> & abilityToUnit) {
	return unitdesc.available && unitdesc.race == sc2::Race::Protoss
		&& !(unitdesc.name.find("Weapon") != std::string::npos
			|| unitdesc.name.find("MP") != std::string::npos
			|| unitdesc.name.find("SkinPreview") != std::string::npos
			|| unitdesc.name.find("Interceptors") != std::string::npos)
		&& (abilityToUnit.empty() || sc2util::IsBuilding(unitdesc.unit_type_id) || (abilityToUnit.find((int)unitdesc.ability_id) != abilityToUnit.end() || unitdesc.name == "Mothership"));
}

void suboo::TechBot::OnGameStart()
{
	
	auto & types = Observation()->GetUnitTypeData();
	auto & abilities = Observation()->GetAbilityData();

	MapTopology map;
	map.init(Observation(), Query(), Debug());
	BuildingPlacer placer;
	placer.init(Observation(), Query(), &map, Debug());
	auto & info = Observation()->GetGameInfo();

	// inspect and create an initial state
	std::vector<UnitInstance> state;
	for (auto u : Observation()->GetUnits(sc2::Unit::Alliance::Self)) {
		state.emplace_back(UnitInstance(u->unit_type));
	}
	initial = GameState(state,Observation()->GetMinerals(), Observation()->GetVespene());

	// time to create some units
	Debug()->DebugGiveAllResources();
	Debug()->DebugGiveAllTech();
	Debug()->DebugGiveAllUpgrades();
	initmin = Observation()->GetMinerals();
	initvesp = Observation()->GetVespene();
	auto point = map.getPosition(map.ally,map.main);
	for (const sc2::UnitTypeData & unitdesc : types) {
		if (isRelevant(unitdesc, {})) {
			point = point + sc2::Point3D(3, 0, 0);
			while (! placer.PlacementB(info, point,3)) {
				point = point + sc2::Point3D(3, 0, 0);
				if (point.x >= info.width) {
					point = sc2::Point3D(0, point.y + 3, 0);
				}
				if (point.y >= info.height) {
					return;
				}
			}
		
			Debug()->DebugCreateUnit(unitdesc.unit_type_id, point,info.player_info[0].player_id);
			// for power, or we don't get build/tech abilities
			Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::PROTOSS_PYLON, point + sc2::Point3D(0,-3,0), info.player_info[0].player_id);
		}
	}
	Debug()->SendDebug();


}

void suboo::TechBot::OnStep()
{
	if (Observation()->GetGameLoop() == 5) {

		auto & types = Observation()->GetUnitTypeData();
		auto & abilities = Observation()->GetAbilityData();

		std::unordered_map<int, int> abilityToUnit;
		for (auto u : Observation()->GetUnits(sc2::Unit::Alliance::Self)) {
			auto & abilities = Query()->GetAbilitiesForUnit(u,true);
			for (auto & ab : abilities.abilities) {
				abilityToUnit[(int)ab.ability_id] = (int) u->unit_type;
			}
		}

		std::ofstream out("TechTree" + version + ".cpp");

		out << "// This file was generated by TechBot, do not edit. Run TechBot again to regenerate.\n";
		out << "#include \"BuildOrder.h\"\n";
		out << "namespace suboo {\n";

		out << "TechTree::TechTree() :\n";
		out << "	initial({";
		for (auto & u : initial.getFreeUnits()) {
			if (sc2util::IsWorkerType(u.type)) {
				out << " UnitInstance( (UnitId)" << (int)u.type << ", UnitInstance::MINING_MINERALS, 0),\n";
			}
			else {
				// CC, larva
				out << " UnitInstance( (UnitId)" << (int)u.type << "),\n";
			}
		}
		out << "	}) {\n";
		out << "  initial.getMinerals() = " << initmin << ";\n";
		out << "  initial.getVespene() = " << initvesp << ";\n";
		out << "  units = {\n";

		// reindex units
		int index = 0;
		// to compute max
		int maxunitID = 0;
		// associations as pairs
		std::vector<std::pair<int, int> > ind;
		for (const sc2::UnitTypeData & unitdesc : types) {

			if (isRelevant(unitdesc,abilityToUnit)) {
				if ((int)unitdesc.unit_type_id > maxunitID) {
					maxunitID = (int)unitdesc.unit_type_id;
				}
				ind.push_back({ (int)unitdesc.unit_type_id , index});
				if (sc2util::IsBuilding(unitdesc.unit_type_id))
				{	// protoss buildings are all produced by a probe walking there 
					int traveltime = 4; // default estimate is 4 seconds to reach build site
					if (sc2util::IsCommandStructure(unitdesc.unit_type_id)) {
						traveltime = 10; // CC require to move to an expansion, that takes about 10 secs
					}
					auto builderid = abilityToUnit[(int)unitdesc.ability_id];
					if (builderid != 0) {
						auto techreq = unitdesc.tech_requirement;
						if (unitdesc.unit_type_id == UnitId::PROTOSS_GATEWAY) {
							techreq = UnitId::PROTOSS_PYLON;
						}
						out << "{"
							"	" << index++ << ", // index" << std::endl <<
							"	(UnitId)" << unitdesc.unit_type_id << ",  // ID" << std::endl <<
							"	\"" << unitdesc.name << "\", // name" << std::endl <<
							"	" << unitdesc.mineral_cost << ", // gold" << std::endl <<
							"	" << unitdesc.vespene_cost << ", // gas" << std::endl <<
							"	" << unitdesc.food_provided - unitdesc.food_required << ",  // food" << std::endl <<
							"	(UnitId)" << (int)builderid << ",  // builder unit  " << std::endl <<
							"	(UnitId)" << (int)techreq << ",  // tech requirement  " << std::endl <<
							"	" << (int) ( (float) unitdesc.build_time / 22.4) << ", // build time" << std::endl << // 22.4 frames per game second
							"	" << traveltime << ",   // travel time" << std::endl <<
							"	Unit::TRAVEL  // probe behavior" << std::endl <<
							"}," << std::endl;
					}
				}
				else
				{
					// it's a moving unit, produced from a production building 
					auto builderid = abilityToUnit[(int)unitdesc.ability_id];
					// no mothership (only one at atime on map = no ability avail on nexus),
					if (unitdesc.name == "Mothership") {
						builderid = (int) sc2::UNIT_TYPEID::PROTOSS_NEXUS;
					}
					// TODO no archons : need two templar to have the ability
					if (builderid != 0) {
						out << "{"
							"	" << index++ << ", // index" << std::endl <<
							"	(UnitId)" << unitdesc.unit_type_id << ",  // ID" << std::endl <<
							"	\"" << unitdesc.name << "\", // name" << std::endl <<
							"	" << unitdesc.mineral_cost << ", // gold" << std::endl <<
							"	" << unitdesc.vespene_cost << ", // gas" << std::endl <<
							"	" << unitdesc.food_provided - unitdesc.food_required << ",  // food" << std::endl <<
							"	(UnitId)" << (int)builderid << ",  // builder unit  " << std::endl <<
							"	(UnitId)" << (int)unitdesc.tech_requirement << ", // tech requirement " << std::endl <<
							"	" << (int)((float)unitdesc.build_time / 22.4) << ", // build time" << std::endl << // 22.4 frames per game second
							"	" << 0 << ",   // travel time" << std::endl <<
							"	Unit::BUSY  // producer behavior" << std::endl <<
							"}," << std::endl;
					}
				}
			}
		};
		out << "  }; // end units\n";
		// build index from unitID to index
		
		out << "  indexes=std::vector<int>("<< (maxunitID+1) <<",0); \n";		
		for (auto & p : ind) {
			out << "  indexes[" << p.first << "] = " << p.second << ";\n";
		}

		
		out << "} //end ctor \n";
		out << "} //end ns \n";
		out.close();
	}
}
