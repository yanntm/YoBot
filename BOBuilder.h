#pragma once

#include "BuildOrder.h"

namespace suboo {

	class BOBuilder {
		std::vector<BuildGoal> goals;
		BuildOrder makeBOFromGoal();
	public:
		void addGoal(const BuildGoal & goal) { goals.emplace_back(goal); }
		
		BuildOrder computeBO();
	};
}