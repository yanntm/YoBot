#pragma once

#include "BuildOrder.h"

namespace suboo {

	class BOBuilder {
		std::vector<BuildGoal> goals;
	public:
		void addGoal(const BuildGoal & goal) { goals.emplace_back(goal); }
		BuildOrder computeBO();
	};
}