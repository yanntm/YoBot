#include <iostream>
#include "sc2api/sc2_api.h"
#include "sc2lib/sc2_lib.h"
#include "sc2utils/sc2_manage_process.h"
#include "sc2utils/sc2_arg_parser.h"

#include "YoBot.h"
#include "LadderInterface.h"

#define TECHTREE 1

#ifdef DEBUG
#ifdef TECHTREE 
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
	
	if (coordinator.StartGame(map)) {
		// Step forward the game simulation.
		coordinator.Update();
		coordinator.Update();
		coordinator.LeaveGame();
	}
	else {
		std::cout << "There was a problem loading the map : " << map << std::endl;
	}
}
#else
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
		,CreateComputer(sc2::Race::Protoss, sc2::Difficulty::CheatInsane)
		
	});
	// Start the game.
	coordinator.LaunchStarcraft();
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
#else 
//*************************************************************************************************
int main(int argc, char* argv[]) 
{

	
	RunBot(argc, argv, new YoBot(), sc2::Race::Protoss);

	return 0;
}
#endif
