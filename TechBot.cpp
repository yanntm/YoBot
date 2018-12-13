#include "TechBot.h"
#include "sc2api/sc2_action.h"
#include "sc2api/sc2_interfaces.h"
#include "UnitTypes.h"
#include <iostream>
#include <fstream>


void suboo::TechBot::OnGameStart()
{
	std::ofstream out("TechTree" + version + ".cpp");

	out << "#include \"TechTree.h\"\n";
	out << "namespace suboo {\n";

	out << "TechTree::TechTree() {\n";
	out << "  units = {\n";
	
	for (const sc2::UnitTypeData & unitdesc : Observation()->GetUnitTypeData()) {
		if (unitdesc.available && unitdesc.race == sc2::Race::Protoss) {
			if (sc2util::IsBuilding(unitdesc.unit_type_id))
			{	
				out << "{"
					"	" << unitdesc.unit_type_id << ",  // ID" << std::endl <<
					"	" << unitdesc.name << ", // name" << std::endl <<
					"	" << unitdesc.mineral_cost << ", // gold" << std::endl <<
					"	" << unitdesc.vespene_cost << ", // gas" << std::endl <<
					"	" << unitdesc.food_provided - unitdesc.food_required << ",  // food" << std::endl <<
					"	{},  // prerequisites" << std::endl <<
					"	" << unitdesc.build_time << ", // build time" << std::endl <<
					"	4,   // travel time" << std::endl <<
					"	Unit::TRAVEL  // probe behavior" << std::endl <<
					"}," << std::endl;
			}			
		}
	};


	out << "  }; // end units";
	out << "} //end ns \n";
	out.close();
}

void suboo::TechBot::OnStep()
{
	Actions();
}
