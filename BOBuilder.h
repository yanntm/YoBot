#pragma once

#include "BuildOrder.h"

namespace suboo {

	class BOBuilder {
		std::vector<BuildGoal> goals;
		BuildOrder makeBOFromGoal();
		
	public:
		void addGoal(const BuildGoal & goal) { goals.emplace_back(goal); }
		static bool enforcePrereqBySwap(BuildOrder & bo);
		static BuildOrder enforcePrereq(const BuildOrder & bo);
		BuildOrder computeBO();	
		static BuildOrder improveBO(const BuildOrder & bo, int depth);
	};
	// true if it is realizable + edit times of build items and final state
	bool timeBO(BuildOrder & bo);
	
}