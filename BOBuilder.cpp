#include "BOBuilder.h"
#include "UnitTypes.h"
#include <unordered_set>
#include <iostream>
#include "boo.h"

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
		for (auto & u : state.getUnits()) {
			seen.insert(u.type);
		}
		for (auto & bi : bo.getItems()) {
			if (bi.getAction() == BUILD) {
				auto target = bi.getTarget();
				auto & unit = tech.getUnitById(target);

				// unit prereq				
				std::vector<UnitId> pre;
				for (UnitId prereq = unit.prereq; (int)prereq != 0; prereq = tech.getUnitById(prereq).prereq) {
					if (!state.hasUnit(prereq)) {
						pre.push_back(prereq);
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
						pre.push_back(prereq);
						state.addUnit(prereq);
						seen.insert(prereq);
					}
				}
				// food
				auto soup = state.getAvailableSupply();
				if (unit.food_provided < 0 &&  soup < -unit.food_provided) {
					pre.push_back(UnitId::PROTOSS_PYLON);
					state.addUnit(UnitId::PROTOSS_PYLON);
				}
				// vespene
				if (unit.vespene_cost > 0 && !state.hasUnit(UnitId::PROTOSS_ASSIMILATOR)) {					
					
					pre.push_back(UnitId::PROTOSS_ASSIMILATOR);
					state.addUnit(UnitId::PROTOSS_ASSIMILATOR);
				}

				if (unit.food_provided < 0 && soup < -unit.food_provided) {
					pre.push_back(UnitId::PROTOSS_PYLON);
					state.addUnit(UnitId::PROTOSS_PYLON);
				}


				std::reverse(pre.begin(), pre.end());
				for (auto & id : pre) {
					bopre.addItem(id);
					if (id == UnitId::PROTOSS_ASSIMILATOR) {
						for (int i = 0; i < 3; i++) {
							bopre.addItem(BuildAction::TRANSFER_VESPENE);
						}
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
		timeBO(bo);
		return bo;
	}

	BuildOrder BOBuilder::improveBO(const BuildOrder & bo)
	{
		std::vector<pboo> optimizers;
		optimizers.emplace_back(new LeftShifter());
		optimizers.emplace_back(new AddVespeneGatherer());
		optimizers.emplace_back(new AddMineralGatherer());
		BuildOrder best = bo;
		int gain = 0;
		do {
			gain = 0;
			for (auto & p : optimizers) {
				auto res = p->improve(best);
				if (res.first > 0) {
					gain += res.first;
					best = res.second;
					std::cout << "Improved results using " << p->getName() << " by " << res.first << " s. Current best timing :" << best.getFinal().getTimeStamp() << "s" << std::endl;
				}
			}
		} while (gain > 0);

		return best;
	}

	bool timeBO(BuildOrder & bo) {
		auto & tech = TechTree::getTechTree();
		//bo = addPower(bo);
		// at this point BO should be doable.
		// simulate it for timing
		auto gs = tech.getInitial();
		for (auto & bi : bo.getItems()) {
			// std::cout << "On :"; bi.print(std::cout); std::cout << std::endl;
			if (bi.getAction() == BUILD) {
				auto & u = tech.getUnitById(bi.getTarget());												
				if (!gs.waitForResources(u.mineral_cost, u.vespene_cost)) {
					std::cout << "Insufficient resources collection in state \n";
					gs.print(std::cout);
					return false;
				}
				if ((int)u.prereq != 0 && !gs.hasUnit(u.prereq)) {
					if (!gs.waitforUnitCompletion(u.prereq)) {
						std::cout << "Insufficient requirements missing :" << tech.getUnitById(u.prereq).name << std::endl;
						gs.print(std::cout);
						return false;
					}
				}
				gs.getMinerals() -= u.mineral_cost;
				gs.getVespene() -= u.vespene_cost;
				gs.addUnit(UnitInstance(u.type, UnitInstance::BUILDING, TechTree::getTechTree().getUnitById(u.type).production_time));
			}
			else if (bi.getAction() == TRANSFER_MINERALS) {

			}
			else if (bi.getAction() == TRANSFER_VESPENE) {
				auto prereq = UnitId::PROTOSS_ASSIMILATOR;
				if (!gs.hasUnit(prereq)) {
					if (!gs.waitforUnitCompletion(prereq)) {
						std::cout << "No assimilator in state \n";
						gs.print(std::cout);
						return false;
					}
				}
				if (!gs.assignProbe(UnitInstance::MINING_VESPENE)) {
					std::cout << "No probe available for mining \n";
					gs.print(std::cout);
					return false;
				}				
			}
			bi.setTime(gs.getTimeStamp());
		}
		bo.getFinal() = gs;
		return true;
	}

}