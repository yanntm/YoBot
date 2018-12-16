#pragma once

#include "BuildOrder.h"

namespace suboo {

	class BOBuilder {
		std::vector<BuildGoal> goals;
		BuildOrder makeBOFromGoal();
		
	public:
		void addGoal(const BuildGoal & goal) { goals.emplace_back(goal); }
		static BuildOrder enforcePrereq(const BuildOrder & bo);
		BuildOrder computeBO();	
		BuildOrder improveBO(const BuildOrder & bo);
	};
	// true if it is realizable + edit times of build items and final state
	bool timeBO(BuildOrder & bo);
	
}