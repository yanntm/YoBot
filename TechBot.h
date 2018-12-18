#pragma once

#include "sc2api/sc2_agent.h"
#include "BuildOrder.h"

namespace suboo {

	class TechBot : public sc2::Agent {
		std::string version;
		GameState initial;
		int initmin;
		int initvesp;
	public :
		TechBot(const std::string & version) :version(version) {}
		void OnGameStart();
		void OnStep();
	};

}