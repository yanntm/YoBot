#include "Boo.h"
#include "BOBuilder.h"
#include "UnitTypes.h"
#include <iostream>
#include <sstream>

namespace suboo {

	std::pair<int, BuildOrder> findBest(const BuildOrder & base, std::vector<BuildOrder> & candidates, std::vector<std::string> candDesc) {
		int best = base.getFinal().getTimeStamp();
		int bestmin = base.getFinal().getMinerals();
		int bestwait = 0;
		for (auto & bi : base.getItems()) {
			bestwait += bi.totalWait();
		}
		int basewait = bestwait;
		int bestindex = -1;
		int index = 0;
		for (auto & bo : candidates) {
			if (timeBO(bo)) {
				auto total = bo.getFinal().getTimeStamp();
				auto bomin = bo.getFinal().getMinerals();
				int totalwait = 0;
				for (auto & bi : bo.getItems()) {
					totalwait += bi.totalWait();
				}
				std::cout << candDesc[index] << " feasible in " << total;
				if (total < best || (total <= best && (bomin > 1 + bestmin || ::abs(bomin - bestmin) < 1 && totalwait < bestwait))) {
					bestindex = index;
					bestmin = bomin;
					best = total;
					bestwait = totalwait;
					std::cout << " mins=" << bestmin;
					std::cout << " totalwait=" << totalwait;
					std::cout << " (new best)";
				}
				std::cout << std::endl;
			}
			else {
				std::cout << candDesc[index] << " unfeasible." << std::endl;
			}
			index++;
		}

		auto delta = (base.getFinal().getTimeStamp() - best);
		auto deltamin = bestmin - base.getFinal().getMinerals();
		auto deltawait = bestwait - basewait;

		return { delta > 0 ? delta : deltamin > 0 ? deltamin : deltawait, bestindex == -1 ? BuildOrder() : candidates[bestindex] };
	}

	std::pair<int, BuildOrder> Randomizer::improve(const BuildOrder & base) {
		BuildOrder best = base;
		for (int i = 0; i < 1000; i++) {
			auto copy = best;
			auto & deq = copy.getItems();
			std::random_shuffle(deq.begin(), deq.end());
			BOBuilder::enforcePrereqBySwap(copy);
			if (timeBO(copy)) {
				auto timefin = copy.getFinal().getTimeStamp();
				auto timebest = best.getFinal().getTimeStamp();
				if (timefin < timebest || timefin == timebest && (best.getFinal().getMinerals() - copy.getFinal().getMinerals()) >0) {
					best = copy;
				}
			}			
		}
		auto delta = (base.getFinal().getTimeStamp() - best.getFinal().getTimeStamp());
		auto deltamin = (base.getFinal().getMinerals() - best.getFinal().getMinerals());
		return { delta > 0 ? delta : deltamin , best };
	}

	std::pair<int, BuildOrder> LeftShifter::improve(const BuildOrder & base)
	{
		auto & tech = TechTree::getTechTree();
		std::vector<BuildOrder> candidates;
		std::vector<std::string > candindexes;
		candidates.reserve(20);
		// find any adjacent events with same timing
		auto & items = base.getItems();
		GameState current = tech.getInitial();
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
						if (uj.prereq != ui.type && (uj.builder != ui.type || uj.builder == UnitId::PROTOSS_PROBE || current.hasFinishedUnit(uj.builder) )) {
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

						if (!BOBuilder::enforcePrereqBySwap(bo)) {
							std::cout << "problem with swap prereq rule " << std::endl;
						}
						candidates.emplace_back(bo);
						candindexes.emplace_back(sstr.str());
					}
					if (!(j < e - 1 && items[j + 1] == items[j])) {
						break;
					}
				}
				else {
					//skip++;
				}
			}
			if (bi.getAction() == BUILD) {
				current.addUnit(bi.getTarget());
			}
			i += skip;
		}
		return findBest(base, candidates,candindexes);
	}



	std::pair<int, BuildOrder> AddVespeneGatherer::improve(const BuildOrder & base)
	{
		int nexi = base.getFinal().countUnit(UnitId::PROTOSS_NEXUS);
		int ass = base.getFinal().countUnit(UnitId::PROTOSS_ASSIMILATOR);
		if (base.getFinal().getVespene() < 20 && 2 *nexi > ass) {
			BuildOrder candidate = base;
			candidate.addItemFront(TRANSFER_VESPENE);			
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
		std::vector<BuildOrder> candidates;
		candidates.reserve(base.getItems().size());
		std::vector<std::string> candnames;
		
		bool needpylon = false;
		if (base.getFinal().getAvailableSupply() == 0) {
			needpylon = true;
		}
		auto & tech = TechTree::getTechTree();
		int available = tech.getInitial().getAvailableSupply();
		if (base.getFinal().getMinerals() < 20 && base.getFinal().probesToSaturation() > 0) {
			for (int i = 0, e = base.getItems().size(); i < e; i++) {
				if (available >= 1) {
					BuildOrder candidate = base;

					candidate.insertItem(UnitId::PROTOSS_PROBE, i);
					if (needpylon) {
						if (i <= 3) {
							candidate.insertItem(UnitId::PROTOSS_PYLON, 0);
						}
						else {
							candidate.insertItem(UnitId::PROTOSS_PYLON, i - 3);
						}
					}
					if (!BOBuilder::enforcePrereqBySwap(candidate)) {
						std::cout << "problem with swap prereq rule " << std::endl;
					}
					candidates.emplace_back(candidate);
					candnames.emplace_back("Add Probe at index " + std::to_string(i));
					// find the next pylons and bring them forward one
					for (int j = i + 2; j < e; j++) {
						auto & act = candidate.getItems()[j];
						if (act.getAction() == BUILD && act.getTarget() == UnitId::PROTOSS_PYLON) {
							candidate.swapItems(j, j - 1);
							candidates.emplace_back(candidate);
							candnames.emplace_back("Add Probe at index " + std::to_string(i) + " and shift pylon at index " + std::to_string(j));							
						}
					}

				}
				auto & bi = base.getItems()[i];
				available += tech.getUnitById(bi.getTarget()).food_provided;
			}			
		}
		return findBest(base,candidates,candnames);	
	}

	std::pair<int, BuildOrder> AddProduction::improve(const BuildOrder & base)
	{
		auto & tech = TechTree::getTechTree();
		std::unordered_map<UnitId, int> used;
		for (auto & bi : base.getItems()) {
			if (bi.getAction() == BUILD) {
				auto & u = tech.getUnitById(bi.getTarget());
				if (u.builder != UnitId::INVALID && !sc2util::IsWorkerType(u.builder)) {
					auto it = used.find(u.builder);
					if (it == used.end()) {
						used[u.builder] = 1;
					}
					else {
						it->second++;
					}
				}
			}
		}
		std::vector<BuildOrder> candidates;
		candidates.reserve(base.getItems().size());
		std::vector<std::string> candnames;
		for (auto & pair : used) {
			if (pair.second > base.getFinal().countUnit(pair.first)) {
				auto & builder = tech.getUnitById(pair.first);
				// try to stutter
				int index = 0;
				bool ok = false;
				for (auto & bi : base.getItems()) {
					if (ok || bi.getAction() == BUILD && bi.getTarget() == pair.first) {
						auto candidate = base;
						candidate.insertItem(pair.first, index);
						candidates.emplace_back(candidate);
						candnames.push_back("Add production " + builder.name + " at step " + std::to_string(index));
						ok = true;
					}
					index++;
				}
			}
		}
		return findBest(base, candidates, candnames);
	}

	std::pair<int, BuildOrder> NoWaitShifter::improve(const BuildOrder & base)		
	{
		auto & tech = TechTree::getTechTree();
		std::vector<BuildOrder> candidates;
		std::vector<std::string > candindexes;
		candidates.reserve(20);
		// find any event with zero wait : left shift it
		auto & items = base.getItems();
		GameState current = tech.getInitial();
		for (int i = 1, e = items.size(); i < e; i++) {
			auto & bi = items[i];
			if (bi == items[i - 1]) {
				continue;
			}
			if (bi.totalWait() == 0) {
				BuildOrder bo = base;
				std::stringstream sstr;
				sstr << "swapping (" << i-1 << " :"; bo.getItems()[i-1].print(sstr);
				sstr << ") and (" << i << " :"; bo.getItems()[i].print(sstr);
				sstr << ")";
				bo.swapItems(i, i-1);

				candidates.emplace_back(bo);
				candindexes.emplace_back(sstr.str());
			}
		}
		return findBest(base, candidates, candindexes);
	}
	std::pair<int, BuildOrder> AddMineralGathererStack::improve(const BuildOrder & base)
	{
		auto copy = base;
		int i = 0, tobuild = base.getFinal().probesToSaturation();
		if (tobuild == 0) {
			return { 0, BuildOrder()};
		}
		for (; i < tobuild && i < 3; i++) {
			copy.addItemFront(UnitId::PROTOSS_PROBE);
		}
		copy = BOBuilder::enforcePrereq(copy);
		copy = BOBuilder::improveBO(copy);
		std::vector<BuildOrder> candidates;
		candidates.emplace_back(copy);
		return findBest(base, candidates, {"optimizedSaturation"});
	}
	std::pair<int, BuildOrder> AddProductionForceful::improve(const BuildOrder & base)
	{
		auto & tech = TechTree::getTechTree();
		auto copy = base;
		std::map<UnitId, int> waitFor;
		for (auto & bi : base.getItems()) {
			if (bi.getAction() == BUILD) {
				if (bi.timeFree > 0) {
					auto & unit = tech.getUnitById(bi.getTarget());
					if (unit.builder != UnitId::INVALID) {
						auto it = waitFor.find(unit.builder);
						if (it == waitFor.end()) {
							waitFor[unit.builder] = bi.timeFree;
						}
						else {
							it->second += bi.timeFree;
						}
					}
				}
			}
		}
		std::vector<BuildOrder> candidates;
		for (auto & pair : waitFor) {
			if (pair.second >= 30) {
				copy.addItem(pair.first);
				copy = BOBuilder::enforcePrereq(copy);
				copy = BOBuilder::improveBO(copy);
				candidates.emplace_back(copy);
			}
		}
		return findBest(base, candidates, { "forcefulProduction" });
	}
}
