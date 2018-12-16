#pragma once

#include "BuildOrder.h"

namespace suboo {

	class BOBuilder {
		std::vector<BuildGoal> goals;
		BuildOrder makeBOFromGoal();
	public:
		void addGoal(const BuildGoal & goal) { goals.emplace_back(goal); }
		
		BuildOrder computeBO();	
		BuildOrder improveBO(const BuildOrder & bo);
	};
	// true if it is realizable + edit times of build items and final state
	bool timeBO(BuildOrder & bo);
	
}