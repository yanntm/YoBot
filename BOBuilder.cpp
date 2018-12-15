#include "BOBuilder.h"


namespace suboo {

	BuildOrder BOBuilder::computeBO()
	{
		BuildOrder bo;
		auto & tech = TechTree::getTechTree();

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

		// enforce prerequisites
		GameState state = tech.getInitial();
		BuildOrder bopre;
		for (auto & bi : bo.getItems()) {
			if (bi.getAction() == BUILD) {
				auto target = bi.getTarget();
				auto & unit = tech.getUnitById(target);
				// unit prereq
				for (UnitId prereq = unit.prereq; (int)prereq != 0 ; prereq = tech.getUnitById(prereq).prereq ) {
					if (!state.hasUnit(prereq)) {
						bopre.addItemFront(prereq);
						state.addUnit(prereq);
					}
				}
				// builder and prereq
				for (UnitId prereq = unit.builder; (int)prereq != 0; prereq = tech.getUnitById(prereq).builder) {
					if (!state.hasUnit(prereq)) {
						bopre.addItemFront(prereq);
						state.addUnit(prereq);
					}
				}
				

				bopre.addItem(target);
				state.addUnit(target);
			}
		}
		return bopre;
	}

}