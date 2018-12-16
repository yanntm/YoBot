#include "BuildOrder.h"
#include "UnitTypes.h"
#include <iostream>
#include <mutex>

namespace suboo {
	BuildGoal::BuildGoal(int deadline) : framesToCompletion(deadline), desiredPerUnit(TechTree::getTechTree().size(), 0)
	{
	}
	void BuildGoal::addUnit(UnitId id, int qty)
	{		
		int index = TechTree::getTechTree().getUnitIndex(id);
		desiredPerUnit[index] += qty;
	}

	void BuildGoal::print(std::ostream & out) const
	{
		auto & tech = TechTree::getTechTree();
		out << "Goal time=" << framesToCompletion << std::endl;
		for (int i = 0, e = desiredPerUnit.size(); i < e; i++) {
			int qty = desiredPerUnit[i];
			if (qty > 0) {
				auto & u = tech.getUnitByIndex(i).name;
				out << qty << " x " << u << std::endl;
			}
		}
	}

	
	const TechTree & TechTree::getTechTree()
	{
		static std::mutex m;
		static TechTree singletonTech;
		std::unique_lock<std::mutex> l(m);
		return singletonTech;
	}
	int TechTree::getUnitIndex(UnitId id) const
	{
		return indexes[(int)id];
	}
	const Unit & TechTree::getUnitById(UnitId id) const
	{
		return units[getUnitIndex(id)];
	}
	const Unit & TechTree::getUnitByIndex(int index) const
	{
		return units[index];
	}
	void BuildOrder::print(std::ostream & out)
	{
		int step = 1;
		for (auto & bi : items) {
			out << step++ << ":";
			bi.print(out);
			out << std::endl;
		}
		out << "Reached state at " << final.getTimeStamp()  << std::endl;
		final.print(out);
	}
	void BuildOrder::addItem(UnitId tocreate)
	{		
		items.emplace_back(BuildItem(tocreate));
	}
	void BuildOrder::addItem(BuildAction action)
	{
		items.emplace_back(BuildItem(action));
	}
	void BuildOrder::addItemFront(UnitId tocreate)
	{
		items.push_front(BuildItem(tocreate));
	}
	void BuildItem::print(std::ostream & out) const
	{
		auto & tech = TechTree::getTechTree();
		if (action == BUILD) {
			out << "Build " << tech.getUnitById(target).name;
		}
		else if (action == TRANSFER_VESPENE) {
			out << "Transfer To Vespene" ;
		}
		else if (action == TRANSFER_MINERALS) {
			out << "Transfer To Minerals";
		}
		if (time != 0) {
			out << "'" << time;
		}
	}
	bool GameState::hasUnit(UnitId unit) const
	{
		return std::any_of(units.begin(), units.end(), [unit](auto & u) {return u.type == unit && u.state != u.BUILDING; });
	}
	void GameState::addUnit(UnitId unit)
	{
		addUnit(UnitInstance(unit));		
	}
	void GameState::addUnit(const UnitInstance & unit)
	{
		units.emplace_back(unit);
		vps = -1.0;
		mps = -1.0;
	}
	float GameState::getMineralsPerSecond() const
	{
		if (mps == -1.0) {
			mps = 0;
			for (auto & u : units) {
				if (u.type == UnitId::PROTOSS_PROBE && u.state == u.MINING_MINERALS) {
					mps += 0.625; // arbitrary
				}
			}
		}
		return mps;
	}
	float GameState::getVespenePerSecond() const
	{
		if (vps == -1.0) {
			vps = 0;
			for (auto & u : units) {
				if (u.type == UnitId::PROTOSS_PROBE && u.state == u.MINING_VESPENE) {
					vps += 0.625; // arbitrary
				}
			}
		}
		return vps;
	}
	int GameState::getAvailableSupply() const
	{
		auto & tech = TechTree::getTechTree();
		int sum = 0;
		for (auto & u : units) {
			auto & unit = tech.getUnitById(u.type);
			if (unit.food_provided < 0) {
				sum += unit.food_provided;
			}
			else {
				if (u.state != u.BUILDING) {
					sum += unit.food_provided;
				}
			}
		}

		return sum;
	}
	int GameState::getUsedSupply() const
	{
		auto & tech = TechTree::getTechTree();
		int sum = 0;
		for (auto & u : units) {
			auto & unit = tech.getUnitById(u.type);
			if (unit.food_provided < 0) {
				sum -= unit.food_provided;
			}
		}

		return sum;
	}
	int GameState::getMaxSupply() const
	{
		auto & tech = TechTree::getTechTree();
		int sum = 0;
		for (auto & u : units) {
			auto & unit = tech.getUnitById(u.type);
			if (unit.food_provided > 0 && u.state != u.BUILDING) {
				sum += unit.food_provided;
			}
		}

		return sum;
	}
	void GameState::stepForward(int secs)
	{
		for (int i = 0; i < secs; i++) {
			bool changed = false;
			for (auto & u : units) {
				if (u.time_to_free > 0) {
					u.time_to_free--;
					if (u.time_to_free == 0) {
						u.state = u.FREE;
						changed = true;
					}
				}
			}
			if (changed) {
				mps = -1;
				vps = -1;
			}
			minerals += getMineralsPerSecond() ;
			vespene += getVespenePerSecond() ;
			timestamp += 1;
		}
		
	}
	bool GameState::waitForResources(int mins, int vesp)
	{
		if (mins > minerals && getMineralsPerSecond() > 0) {
			int secs = (mins - minerals) / mps;
			if ( ((mins - minerals) / mps) - secs > 0) {
				secs += 1;
			}
			stepForward(secs);
			std::cout << "Waited for minerals for " << secs << "s." << std::endl;
		}
		else if (mins > minerals) {
				return false;			
		}
		if (vesp > vespene && getVespenePerSecond() > 0) {
			stepForward((vesp - vespene) / vps);
		}
		else if (vesp > vespene) {
			return false;
		}
		return true;
	}
	bool GameState::waitforUnitCompletion(UnitId id)
	{
		auto it = std::find_if(units.begin(), units.end(), [id](auto & u) {return u.type == id && u.state == u.BUILDING; });
		if (it == units.end()) {
			return false;
		}
		else {
			std::cout << "Waited for " << TechTree::getTechTree().getUnitById(id).name << " for " << it->time_to_free << "s." << std::endl;
			stepForward(it->time_to_free);
			return true;
		}
	}
	bool GameState::assignProbe(UnitInstance::UnitState state)
	{
		bool done = false;
		for (auto & u : units) {
			if (sc2util::IsWorkerType(u.type) && u.state == u.FREE) {
				u.state = UnitInstance::MINING_VESPENE;
				vps = -1;
				done = true;
				break;
			}
		}
		if (!done) {
			for (auto & u : units) {
				if (sc2util::IsWorkerType(u.type) && u.state == u.MINING_MINERALS) {
					u.state = UnitInstance::MINING_VESPENE;
					vps = -1;
					mps = -1;
					done = true;
					break;
				}
			}
		}
		return done;
	}
	void GameState::print(std::ostream & out) const
	{
		auto & tech = TechTree::getTechTree();
		for (auto & u : units) {
			u.print(out);
			out << ",";
		}
		out << std::endl;
		out << "bank : minerals = " << minerals << "("<< getMineralsPerSecond() <<"/s)" << " vespene = " << vespene << "(" << getVespenePerSecond() << "/s)"<< " supply : " << getAvailableSupply() << ":" << getUsedSupply() << "/" << getMaxSupply() <<  std::endl;
	}
	UnitInstance::UnitInstance(UnitId type)
		: type(type), state(FREE), time_to_free(0) 
	{
	}
	void UnitInstance::print(std::ostream & out) const
	{
		out << TechTree::getTechTree().getUnitById(type).name;
		std::string status;
		switch (state) {
		case BUILDING :
			status = "B";
			break;
		case MINING_MINERALS :
			status = "M";
			break;
		case MINING_VESPENE :
			status = "V";
			break;
		case FREE:
		default:
			status = "F";
			break;
		}
		out << "(" << status;
		if (time_to_free > 0)
			out << time_to_free;
		out << ")";
	}
}