#include "Boo.h"
#include "BOBuilder.h"
#include <iostream>
#include <sstream>

namespace suboo {

	std::pair<int, BuildOrder> LeftShifter::improve(const BuildOrder & base)
	{
		auto & tech = TechTree::getTechTree();
		std::vector<BuildOrder> candidates;
		std::vector<std::string > candindexes;
		candidates.reserve(20);
		// find any adjacent events with same timing
		auto & items = base.getItems();
		for (int i = 0, e = items.size(); i < e; i++) {
			auto & bi = items[i];
			int skip = 0;
			for (int j = i + 1; j < e; j++) {
				auto & bj = items[j];
				if (! (bj == bi)) {
					bool biprecedesbj = true;
					if (bi.getAction() == BUILD && bj.getAction() == BUILD) {
						auto & ui = tech.getUnitById(bi.getTarget());
						auto & uj = tech.getUnitById(bj.getTarget());
						if (uj.prereq != ui.type && (uj.builder != ui.type || uj.builder == UnitId::PROTOSS_PROBE )) {
							biprecedesbj = false;
						}
					}
					if (bj.getAction() == TRANSFER_VESPENE && bi.getAction() == BUILD && bi.getTarget() == UnitId::PROTOSS_ASSIMILATOR) {

					}
					else {
						if (bi.getAction() == BUILD && bj.getAction() != BUILD) {
							biprecedesbj = false;
						}
					}
					if (bj.getTime() == bi.getTime() || !biprecedesbj) {
						BuildOrder bo = base;
						std::stringstream sstr;
						sstr << "swapping (" << i << " :"; bo.getItems()[i].print(sstr);
						sstr << ") and (" << j << " :"; bo.getItems()[j].print(sstr);
						sstr << ")" ;
						bo.swapItems(i, j);

						candidates.emplace_back(bo);
						candindexes.emplace_back(sstr.str());
					}
					break;					
				}
				else {
					skip++;
				}
			}
			i += skip;
		}
		int best = base.getFinal().getTimeStamp();
		int bestindex = -1;
		int index = 0;
		for (auto & bo : candidates) {
			if (timeBO(bo)) {
				auto total = bo.getFinal().getTimeStamp();
				std::cout << candindexes[index] << " feasible in " << total ;
				if (total < best) {
					bestindex = index;
					best = bo.getFinal().getTimeStamp();
					std::cout << " (new best)" ;
				}
				std::cout << std::endl;
			}
			else {
				std::cout << " unfeasible." << std::endl;
			}
			index++;
		}
		
		return { base.getFinal().getTimeStamp() - best, bestindex == -1 ? BuildOrder() : candidates[bestindex]};
	}



	std::pair<int, BuildOrder> AddVespeneGatherer::improve(const BuildOrder & base)
	{
		int nexi = base.getFinal().countUnit(UnitId::PROTOSS_NEXUS);
		int ass = base.getFinal().countUnit(UnitId::PROTOSS_ASSIMILATOR);
		if (base.getFinal().getVespene() < 20 && 2 *nexi > ass) {
			BuildOrder candidate = base;
			for (int i = 0; i < 3; i++) {
				candidate.addItemFront(TRANSFER_VESPENE);
			}
			candidate.addItemFront(UnitId::PROTOSS_ASSIMILATOR);
			if (timeBO(candidate)) {
				int delta =  base.getFinal().getTimeStamp() - candidate.getFinal().getTimeStamp() ;
				if (delta > 0) {
					return {delta, candidate };
				}
			}
		}
		return std::pair<int, BuildOrder>(0,BuildOrder());
		
	}

	std::pair<int, BuildOrder> AddMineralGatherer::improve(const BuildOrder & base)
	{
		int nexi = base.getFinal().countUnit(UnitId::PROTOSS_NEXUS);
		int ass = base.getFinal().countUnit(UnitId::PROTOSS_ASSIMILATOR);
		int prob = base.getFinal().countUnit(UnitId::PROTOSS_PROBE);
		if (base.getFinal().getMinerals() < 20 && (20 * nexi + 3*ass) > prob) {
			BuildOrder best = base;
			for (int i = 0, e = best.getItems().size(); i < e; i++) {
				BuildOrder candidate = best;				
				candidate.insertItem(UnitId::PROTOSS_PROBE, i);
				if (timeBO(candidate)) {
					int delta = best.getFinal().getTimeStamp() - candidate.getFinal().getTimeStamp();
					if (delta > 0) {
						return { delta, candidate };
					}
				}
			}
		}
		return std::pair<int, BuildOrder>(0, BuildOrder());	
	}

}
