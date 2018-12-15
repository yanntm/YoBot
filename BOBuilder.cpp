#include "BOBuilder.h"
#include "UnitTypes.h"
#include <unordered_set>

namespace suboo {

	BuildOrder BOBuilder::makeBOFromGoal() {
		auto & tech = TechTree::getTechTree();
		BuildOrder bo;
		// make the basic thing : list of creation order of each desired units x qty
		for (auto & goal : goals) {
			for (int index = 0; index < tech.size(); index++) {
				int qty = goal.getQty(index);
				while (qty > 0) {
					bo.addItem(tech.getUnitByIndex(index).type);
					qty--;
				}
			}
		}
		return bo;
	}

	static BuildOrder enforcePrereq(const BuildOrder & bo) {
		auto & tech = TechTree::getTechTree();
		// enforce prerequisites
		GameState state = tech.getInitial();
		BuildOrder bopre;
		std::unordered_set<UnitId> seen;
		for (auto & bi : bo.getItems()) {
			if (bi.getAction() == BUILD) {
				auto target = bi.getTarget();
				auto & unit = tech.getUnitById(target);

				// unit prereq
				for (UnitId prereq = unit.prereq; (int)prereq != 0; prereq = tech.getUnitById(prereq).prereq) {
					if (!state.hasUnit(prereq)) {
						bopre.addItemFront(prereq);
						state.addUnit(prereq);
						seen.insert(prereq);
					}
				}
				// builder and prereq				
				for (UnitId prereq = unit.builder; (int)prereq != 0; prereq = tech.getUnitById(prereq).builder) {
					if (seen.find(prereq) != seen.end()) {
						break;
					}
					if (!state.hasUnit(prereq)) {
						bopre.addItemFront(prereq);
						state.addUnit(prereq);
						seen.insert(prereq);
					}
				}


				bopre.addItem(target);
				state.addUnit(target);
			}
		}
		return bopre;
	}

	static BuildOrder addPower(const BuildOrder & bo) {
		auto & tech = TechTree::getTechTree();
		BuildOrder bopre;
		auto state = tech.getInitial();
		for (auto & bi : bo.getItems()) {
			if (bi.getAction() == BUILD) {
				auto target = bi.getTarget();
				auto & unit = tech.getUnitById(target);
				// deal with power				
				if (target != UnitId::PROTOSS_NEXUS
					&& target != UnitId::PROTOSS_PYLON
					&& unit.builder == UnitId::PROTOSS_PROBE
					&& sc2util::IsBuilding(target)
					)
				{
					auto prereq = UnitId::PROTOSS_PYLON;
					if (!state.hasUnit(prereq)) {
						bopre.addItemFront(prereq);
						state.addUnit(prereq);
					}
				}
				bopre.addItem(target);
			}
		}
		return bopre;
	}

	BuildOrder BOBuilder::computeBO()
	{
		BuildOrder bo = makeBOFromGoal();
		bo = enforcePrereq(bo);
		bo = addPower(bo);
		return bo;
	}

}