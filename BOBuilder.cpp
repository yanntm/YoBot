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

		return bo;
	}

}