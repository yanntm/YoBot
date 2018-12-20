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

	void addPreReq(std::vector<BuildItem> & pre, GameState & state, std::unordered_set<UnitId> & seen, UnitId target, const TechTree & tech) {
		auto & unit = tech.getUnitById(target);
		if ((int)unit.prereq != 0) {
			if (!state.hasFinishedUnit(unit.prereq)) {
				addPreReq(pre, state, seen, unit.prereq, tech);
			}
		}
		if ((int)unit.builder != 0) {
			if (!state.hasFreeUnit(unit.builder)) {
				addPreReq(pre, state, seen, unit.builder, tech);
			}
		}
		// food
		auto soup = state.getAvailableSupply();
		if (unit.food_provided < 0 && soup < -unit.food_provided) {
			pre.push_back(UnitId::PROTOSS_PYLON);
			state.addUnit(UnitId::PROTOSS_PYLON);
		}
		// vespene
		if (unit.vespene_cost > 0 && !state.hasFinishedUnit(UnitId::PROTOSS_ASSIMILATOR)) {
			pre.push_back(UnitId::PROTOSS_ASSIMILATOR);
			state.addUnit(UnitId::PROTOSS_ASSIMILATOR);
			pre.push_back(TRANSFER_VESPENE);
		}		
		pre.push_back(unit.type);
		state.addUnit(unit.type);
		seen.insert(unit.type);
	}
	bool BOBuilder::enforcePrereqBySwap(BuildOrder & bo) {
		auto & tech = TechTree::getTechTree();
		// enforce prerequisites
		GameState state = tech.getInitial();
		BuildOrder bopre;
		std::unordered_set<UnitId> seen;
		for (auto & u : state.getUnits()) {
			seen.insert(u.type);
		}
		
		for (int i = 0; i < bo.getItems().size() && i >= 0; i++) {
			auto & bi = bo.getItems()[i];
			if (bi.getAction() == BUILD) {
				auto target = bi.getTarget();
				auto & unit = tech.getUnitById(target);
				// unit prereq				
				std::vector<BuildItem> pre;
				addPreReq(pre, state, seen, unit.type, tech);

				for (auto & id : pre) {
					int j,e;
					for (j = i, e = bo.getItems().size(); j < e; j++) {
						if (bo.getItems()[j] == BuildItem(id)) {
							bo.removeItem(j);
							if (i == j) {
								i--;
							}
							break;
						}
					}
					if (j == e) {
						return false;
					}
					bopre.addItem(id);
					if (id == UnitId::PROTOSS_ASSIMILATOR) {
						auto bbi = BuildItem(TRANSFER_VESPENE);
						for (j = std::max(0,i), e = bo.getItems().size(); j < e; j++) {
							if (bo.getItems()[j] ==bbi) {
								bo.removeItem(j);
								if (i == j) {
									i--;
								}
								break;
							}
						}
						if (j == e) {
							return false;
						}
						bopre.addItem(bbi.getAction());
					}
				}				
			}
			else {
				bopre.addItem(bi.getAction());
			}			
		}
		bo = bopre;
		return true;
	}

	BuildOrder BOBuilder::enforcePrereq(const BuildOrder & bo) {
		auto & tech = TechTree::getTechTree();
		// enforce prerequisites
		GameState state = tech.getInitial();
		BuildOrder bopre;
		std::unordered_set<UnitId> seen;
		for (auto & u : state.getUnits()) {
			seen.insert(u.type);
		}
		int vesp=0;
		for (auto & bi : bo.getItems()) {
			if (bi.getAction() == BUILD) {
				auto target = bi.getTarget();
				auto & unit = tech.getUnitById(target);
				// unit prereq				
				std::vector<BuildItem> pre;
				addPreReq(pre, state, seen, unit.type, tech);
				
				for (auto & id : pre) {
					bopre.addItem(id);					
				}							
			}
			else {				
				if (state.countUnit(UnitId::PROTOSS_ASSIMILATOR) >= vesp) {
					bopre.addItem(bi.getAction());
					vesp++;
				}
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
		optimizers.emplace_back(new NoWaitShifter());
		optimizers.emplace_back(new AddVespeneGatherer());
		optimizers.emplace_back(new AddMineralGatherer());
		optimizers.emplace_back(new AddMineralGathererStack());
		optimizers.emplace_back(new AddProduction());
		optimizers.emplace_back(new LeftShifter());
		//
		BuildOrder best = bo;
		int gain = 0;
		do {
			gain = 0;
			for (auto & p : optimizers) {
				int optgain = 0;
				// nested loop mostly does not help, most rules already try many positions it's redundant
				//do {
					optgain = 0;
					auto res = p->improve(best);
					if (res.first > 0) {
						gain += res.first;
						optgain += res.first;
						best = res.second;
						std::cout << "Improved results using " << p->getName() << " by " << res.first << " s. Current best timing :" << best.getFinal().getTimeStamp() << "s" << std::endl;
						best.print(std::cout);
					}
					else {
						std::cout << "No improvement of results using " << p->getName() << ". Current best timing :" << best.getFinal().getTimeStamp() << "s" << std::endl;
					}
				//} while (optgain >0);
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

			bi.clearTimes();
			// std::cout << "On :"; bi.print(std::cout); std::cout << std::endl;
			if (bi.getAction() == BUILD) {
				auto & u = tech.getUnitById(bi.getTarget());
				std::pair<int, int> waited;
				if (!gs.waitForResources(u.mineral_cost, u.vespene_cost, & waited)) {
					std::cout << "Insufficient resources collection in state \n";
					gs.print(std::cout);
					return false;
				}
				else {
					bi.timeMin = waited.first; 
					bi.timeVesp = waited.second;
				}
				if ((int)u.prereq != 0 && !gs.hasFinishedUnit(u.prereq)) {
					int cur = gs.getTimeStamp();
					if (!gs.waitforUnitCompletion(u.prereq)) {
						std::cout << "Insufficient requirements missing tech req :" << tech.getUnitById(u.prereq).name << std::endl;
						gs.print(std::cout);
						return false;
					}
					bi.timePre = gs.getTimeStamp() - cur;
				}
				if (u.builder != UnitId::INVALID) {
					if (!gs.hasFreeUnit(u.builder)) {
						int cur = gs.getTimeStamp();
						if (!gs.waitforUnitFree(u.builder)) {
							std::cout << "Insufficient requirements missing builder :" << tech.getUnitById(u.builder).name << std::endl;
							gs.print(std::cout);
							return false;
						}
						bi.timeFree = gs.getTimeStamp() - cur;
					}
					if (u.effect == u.TRAVEL) {
						gs.assignFreeUnit(u.builder, UnitInstance::BUSY, u.travel_time);
					}
					else if (u.effect == u.BUSY) {
						gs.assignFreeUnit(u.builder, UnitInstance::BUSY, u.production_time);
					}
				}

				
				if (u.food_provided < 0 && gs.getAvailableSupply() < -u.food_provided) {
					int cur = gs.getTimeStamp();					
					if (!gs.waitforFreeSupply(-u.food_provided)) {
						//std::cout << "Insufficient food missing pylons." << std::endl;
						//gs.print(std::cout);
						return false;
					}
					bi.timeFood = gs.getTimeStamp() - cur;
				}
				gs.getMinerals() -= u.mineral_cost;
				gs.getVespene() -= u.vespene_cost;
				gs.addUnit(UnitInstance(u.type, UnitInstance::BUILDING, TechTree::getTechTree().getUnitById(u.type).production_time));
			}
			else if (bi.getAction() == TRANSFER_MINERALS) {

			}
			else if (bi.getAction() == TRANSFER_VESPENE) {
				bi.timeFree = 0;
				auto prereq = UnitId::PROTOSS_ASSIMILATOR;
				if (!gs.hasFreeUnit(prereq)) {
					int cur = gs.getTimeStamp();		
					if (!gs.waitforUnitCompletion(prereq)) {
						std::cout << "No assimilator in state \n";
						gs.print(std::cout);
						return false;
					}
					bi.timeFree = gs.getTimeStamp() - cur;
				}
				int gas = 0;
				int soongas = 0;

				int vcount =0;
				for (auto & u : gs.getUnits()) {
					if (u.state == UnitInstance::MINING_VESPENE) {
						vcount++;
					}
					else if (u.type == UnitId::PROTOSS_ASSIMILATOR && u.state == UnitInstance::FREE) {
						gas++;
					}
					else if (u.type == UnitId::PROTOSS_ASSIMILATOR) {
						soongas++;
					}
				}
				if (vcount >= 3 * (gas+soongas)) {
					std::cout << "Gas over saturated skipping \n";					
					//gs.print(std::cout);
					return false;					
				}
				if (vcount >= 3 * gas) {
					int cur = gs.getTimeStamp();
					if (!gs.waitforUnitCompletion(prereq)) {
						std::cout << "No assimilator in state \n";
						gs.print(std::cout);
						return false;
					}
					bi.timeFree += gs.getTimeStamp() - cur;
				}
				if (!gs.assignProbe(UnitInstance::MINING_VESPENE)) {
					std::cout << "No probe available for mining \n";
					gs.print(std::cout);
					return false;
				}
			}
			bi.setTime(gs.getTimeStamp());
		}
		// finalize build : free all units
		if (!gs.waitforAllUnitFree()) {
			std::cout << "Could not free all units \n";
			gs.print(std::cout);
			return false;
		}
		bo.getFinal() = gs;
		return true;
	}

}