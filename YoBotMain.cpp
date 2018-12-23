#include <iostream>
#include "sc2api/sc2_api.h"
#include "sc2lib/sc2_lib.h"
#include "sc2utils/sc2_manage_process.h"
#include "sc2utils/sc2_arg_parser.h"
#include "LadderInterface.h"

//#define _GENTECHTREE 1
//#define _BOSEARCH 1
#define _SELFPLAY 1
//#define _RELEASE 1
//#undef _RELEASE

#ifdef _GENTECHTREE 
#include "BuildOrder.h"
#include "TechBot.h"
#include <regex>


int main(int argc, char* argv[])
{
	sc2::Coordinator coordinator;
	if (!coordinator.LoadSettings(argc, argv))
	{
		std::cout << "Unable to find or parse settings." << std::endl;
		return 1;
	}
	auto path = coordinator.GetExePath();
	std::smatch matched;
	std::regex reg(R"(\bBase\d+\b)");
	std::string version = "100000";
	if (std::regex_search(path, matched, reg)) {
		version = "";
		for (size_t i = 4; i < matched.size(); i++) {
			version += matched[i];
		}
	}
	std::cout << "Version :" << version << std::endl;
	suboo::TechBot bot (version);
		
	coordinator.SetStepSize(1);
	coordinator.SetRealtime(false);
	coordinator.SetMultithreaded(true);

	coordinator.SetParticipants({
		CreateParticipant(sc2::Race::Protoss, &bot)
		,CreateComputer(sc2::Race::Protoss, sc2::Difficulty::CheatInsane)

		});
	// Start the game.
	coordinator.LaunchStarcraft();
	
	auto map = "D:\\games\\StarCraft II\\Maps\\CeruleanFallLE.SC2Map";
//	auto map = "D:\\games\\StarCraft II\\Maps\\DarknessSanctuaryLE.SC2Map";
	
	if (coordinator.StartGame(map)) {
		// Step forward the game simulation.
		for (int i = 0 ; i < 10 ; i++)
			coordinator.Update();
		coordinator.LeaveGame();
	}
	else {
		std::cout << "There was a problem loading the map : " << map << std::endl;
	}
}
#endif

#ifdef _SELFPLAY
#include "YoBot.h"

class Human : public sc2::Agent {

};


int main(int argc, char* argv[])
{
	YoBot bot;
	YoBot bot2;
	Human h;
	sc2::Coordinator coordinator;
	if (!coordinator.LoadSettings(argc, argv))
	{
		std::cout << "Unable to find or parse settings." << std::endl;
		return 1;
	}
	coordinator.SetStepSize(1);
	coordinator.SetRealtime(false);
	coordinator.SetMultithreaded(true);
	coordinator.SetParticipants({
		/*CreateParticipant(sc2::Race::Zerg, &h),*/ CreateParticipant(sc2::Race::Protoss, &bot)//,CreateParticipant(sc2::Race::Protoss, &bot2)
		//sc2::PlayerSetup(sc2::PlayerType::Observer,Util::GetRaceFromString(enemyRaceString)),
		,CreateComputer(sc2::Race::Random, sc2::Difficulty::VeryHard)
		
	});
	coordinator.LaunchStarcraft();	// Start the game.

	//auto map = "G:\\Program Files (x86)\\StarCraft II\\Maps\\AcidPlantLE.SC2Map"; // "AcidPlant LE";  //"Interloper LE""16-Bit LE"
	// auto map = "G:\\Program Files (x86)\\StarCraft II\\Maps\\Redshift.SC2Map";
	// auto map = "G:\\Program Files (x86)\\StarCraft II\\Maps\\DarknessSanctuary.SC2Map";
	//auto map = "G:\\Program Files (x86)\\StarCraft II\\Maps\\16BitLE.SC2Map";
	//auto map = "G:\\Program Files (x86)\\StarCraft II\\Maps\\InterloperLE.SC2Map";
	//auto map = "D:\\games\\StarCraft II\\Maps\\LostAndFoundLE.SC2Map";
	// auto map = "D:\\games\\StarCraft II\\Maps\\ParaSiteLE.SC2Map";
	//auto map = "D:\\games\\StarCraft II\\Maps\\AutomatonLE.SC2Map";
	auto map = "D:\\games\\StarCraft II\\Maps\\CeruleanFallLE.SC2Map";
	//coordinator.StartGame("C:/Program Files (x86)/StarCraft II/Maps/InterloperLE.SC2Map");
	//coordinator.StartGame();
	// coordinator.StartGame("16-Bit LE");
	if (coordinator.StartGame(map)) {
		// Step forward the game simulation.
		while (coordinator.Update())
		{
		}
	}
	else {
		std::cout << "There was a problem loading the map : " << map << std::endl;
	}
}
#endif


#ifdef _BOSEARCH

#include "BuildOrder.h"
#include "BOBuilder.h"

using namespace sc2;
using namespace suboo;

int main(int argc, char* argv[])
{
	BOBuilder builder;

	BuildGoal goal(0); // ASAP
	goal.addUnit(UnitId::PROTOSS_IMMORTAL, 1);
	//goal.addUnit(UnitId::PROTOSS_DISRUPTOR, 1);
	goal.addUnit(UnitId::PROTOSS_CARRIER, 1);
	//goal.addUnit(UnitId::PROTOSS_GATEWAY, 3);
	//goal.addUnit(UnitId::PROTOSS_STARGATE, 3);
	goal.addUnit(UnitId::PROTOSS_OBSERVER, 1);
	goal.addUnit(UnitId::PROTOSS_PHOENIX, 3);
	goal.addUnit(UnitId::PROTOSS_ZEALOT, 10); //goal.addUnit(UnitId::PROTOSS_NEXUS, 1);

	builder.addGoal(goal);
	goal.print(std::cout);

	BuildOrder bo = builder.computeBO();
	std::cout << "Initial realizable :" << std::endl;
	bo.print(std::cout);
	auto boopt = builder.improveBO(bo, 2);

	std::cout << "Final realizable :" << std::endl;
	boopt.print(std::cout);
	auto boopt2 = builder.improveBO(boopt, 5);
	std::cout << "Final realizable :" << std::endl;
	boopt2.print(std::cout);

	if (true) {
		BuildOrder bo;
		bo.addItem(UnitId::PROTOSS_PROBE);
		bo.addItem(UnitId::PROTOSS_PYLON);
		bo.addItem(UnitId::PROTOSS_PROBE);
		bo.addItem(UnitId::PROTOSS_PROBE);
		bo.addItem(UnitId::PROTOSS_GATEWAY);
		bo.addItem(UnitId::PROTOSS_PROBE);
		bo.addItem(UnitId::PROTOSS_GATEWAY);
		bo.addItem(UnitId::PROTOSS_CYBERNETICSCORE);
		bo.addItem(UnitId::PROTOSS_ZEALOT);
		bo.addItem(UnitId::PROTOSS_PROBE);
		bo.addItem(UnitId::PROTOSS_ASSIMILATOR);
		bo.addItem(TRANSFER_VESPENE);
		bo.addItem(UnitId::PROTOSS_PYLON);
		bo.addItem(UnitId::PROTOSS_PROBE);
		bo.addItem(UnitId::PROTOSS_ZEALOT);
		bo.addItem(UnitId::PROTOSS_PROBE);
		bo.addItem(UnitId::PROTOSS_ZEALOT);
		bo.addItem(UnitId::PROTOSS_PROBE);
		bo.addItem(UnitId::PROTOSS_ZEALOT);
		bo.addItem(UnitId::PROTOSS_ZEALOT);
		bo.addItem(UnitId::PROTOSS_PYLON);
		bo.addItem(UnitId::PROTOSS_ZEALOT);
		bo.addItem(UnitId::PROTOSS_PYLON);
		bo.addItem(UnitId::PROTOSS_PROBE);
		bo.addItem(UnitId::PROTOSS_ZEALOT);
		bo.addItem(UnitId::PROTOSS_STARGATE);
		bo.addItem(UnitId::PROTOSS_PYLON);
		bo.addItem(UnitId::PROTOSS_ZEALOT);
		bo.addItem(UnitId::PROTOSS_ZEALOT);
		bo.addItem(UnitId::PROTOSS_PHOENIX);
		bo.addItem(UnitId::PROTOSS_ASSIMILATOR);
		bo.addItem(TRANSFER_VESPENE);
		bo.addItem(UnitId::PROTOSS_ZEALOT);
		bo.addItem(UnitId::PROTOSS_ZEALOT);
		bo.addItem(UnitId::PROTOSS_PYLON);
		bo.addItem(UnitId::PROTOSS_PHOENIX);
		bo.addItem(UnitId::PROTOSS_ZEALOT);
		bo.addItem(UnitId::PROTOSS_ZEALOT);
		bo.addItem(UnitId::PROTOSS_PHOENIX);
		bo.addItem(UnitId::PROTOSS_ZEALOT);
		bo.addItem(UnitId::PROTOSS_ROBOTICSFACILITY);
		bo.addItem(UnitId::PROTOSS_ZEALOT);
		bo.addItem(UnitId::PROTOSS_FLEETBEACON);
		bo.addItem(UnitId::PROTOSS_PYLON);
		bo.addItem(UnitId::PROTOSS_OBSERVER);
		bo.addItem(UnitId::PROTOSS_IMMORTAL);
		bo.addItem(UnitId::PROTOSS_CARRIER);
		if (!timeBO(bo)) {
			auto boe = BOBuilder::enforcePrereq(bo);
			timeBO(boe);
			std::cout << "Patch was necessary :" << std::endl;
			bo = boe;
		}
		std::cout << "build by voxel :" << std::endl;
		bo.print(std::cout);

		auto boopt2 = builder.improveBO(bo, 3);

		std::cout << "Final realizable :" << std::endl;
		boopt2.print(std::cout);
	}

	std::string s;
	std::cin >> s;

	return 0;
}


#endif

#ifdef _RELEASE

#include "YoBot.h"

//*************************************************************************************************
int main(int argc, char* argv[]) 
{

	
	RunBot(argc, argv, new YoBot(), sc2::Race::Protoss);

	return 0;
}
#endif
