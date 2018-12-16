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
	bool GameState::hasFreeUnit(UnitId unit) const
	{
		return std::any_of(units.begin(), units.end(), [unit](auto & u) {
			return (u.type == unit && u.state == u.FREE) 
				|| (sc2util::IsWorkerType(unit) && u.state == u.MINING_MINERALS); 
		});
	}
	bool GameState::hasFinishedUnit(UnitId unit) const
	{
		return std::any_of(units.begin(), units.end(), [unit](auto & u) {
			return (u.type == unit && u.state != u.BUILDING)
				;
		});
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
						if (!sc2util::IsWorkerType(u.type)) {
							u.state = u.FREE;
						}
						else {
							// default for workers is to mine
							u.state = u.MINING_MINERALS;
						}
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
			// std::cout << "Waited for minerals for " << secs << "s." << std::endl;
		}
		else if (mins > minerals) {
				return false;			
		}
		if (vesp > vespene && getVespenePerSecond() > 0) {
			int secs = (vesp - vespene) / vps;
			if (((vesp - vespene) / vps) - secs > 0) {
				secs += 1;
			}
			stepForward(secs);
			// std::cout << "Waited for vespene for " << secs << "s." << std::endl;
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
			// std::cout << "Waited for " << TechTree::getTechTree().getUnitById(id).name << " for " << it->time_to_free << "s." << std::endl;
			stepForward(it->time_to_free);
			return true;
		}
	}
	bool GameState::waitforUnitFree(UnitId id)
	{
		auto it = std::find_if(units.begin(), units.end(), [id](auto & u) {return u.type == id && u.state == u.FREE || (sc2util::IsWorkerType(id) && u.state == u.MINING_MINERALS); });
		if (it != units.end()) {
			return true;
		}
		int index = 0;
		int best = -1;
		for (auto & u : units) {
			if (u.type == id && u.state != u.FREE && u.time_to_free != 0) {
				if (best == -1 || units[best].time_to_free > u.time_to_free) {
					best = index;
				}
			}
			index++;
		}
		if (best == -1) {
			return false;
		}
		else {
			// std::cout << "Waited for " << TechTree::getTechTree().getUnitById(id).name << " to be free for " << units[best].time_to_free << "s." << std::endl;
			stepForward(units[best].time_to_free);
			return true;
		}
	}
	bool GameState::waitforFreeSupply(int needed)
	{
		int cur = getAvailableSupply();
		if (cur >= needed) {
			return true;
		}
		auto it = std::find_if(units.begin(), units.end(), [](auto & u) {return u.type == UnitId::PROTOSS_PYLON && u.state != u.FREE ; });
		if (it == units.end()) {
			return false;
		}
		int index = 0;
		int best = -1;
		for (auto & u : units) {
			if (u.state == u.BUILDING && (u.type == UnitId::PROTOSS_PYLON || u.type == UnitId::PROTOSS_NEXUS)) {
				if (best == -1 || units[best].time_to_free > u.time_to_free) {
					best = index;
				}
			}
			index++;
		}
		if (best == -1) {
			return false;
		}
		else {
			// std::cout << "Waited for " << TechTree::getTechTree().getUnitById(id).name << " to be provide food for " << units[best].time_to_free << "s." << std::endl;
			stepForward(units[best].time_to_free);
			return true;
		}
		
	}
	bool GameState::assignProbe(UnitInstance::UnitState state)
	{
		int done = 0;
		for (auto & u : units) {
			if (sc2util::IsWorkerType(u.type) && ( u.state == u.FREE || u.state == u.MINING_MINERALS) ) {
				u.state = UnitInstance::MINING_VESPENE;
				vps = -1;
				mps = -1;
				done++;
				if (done >= 3) 
					break;
			}
		}
		return done>=3;
	}
	bool GameState::assignFreeUnit(UnitId type, UnitInstance::UnitState state, int time)
	{
		for (auto & u : units) {
			if (type == u.type && u.state == u.FREE) {
				u.state = state;
				u.time_to_free = time;
				return true;
			}
		}
		return false;
	}
	void GameState::print(std::ostream & out) const
	{
		auto & tech = TechTree::getTechTree();
		std::unordered_map<UnitId, std::unordered_map<UnitInstance::UnitState, int> > perUnitPerState;
		for (auto & u : units) {
			auto it = perUnitPerState.find(u.type);
			if (it == perUnitPerState.end()) {
				perUnitPerState[u.type] = { {u.state,1} };
			}
			else {
				auto & perState = it->second;
				auto jt = perState.find(u.state);
				if (jt == perState.end()) {
					perState[u.state] = 1;
				}
				else
				{
					jt->second++;
				}
			}
		}
		for (auto it : perUnitPerState) {
			out << tech.getUnitById(it.first).name << " ";
			for (auto jt : it.second) {
				out << jt.second << " x " << to_string(jt.first) << " ";
			}
			out << ",";
		}

		out << std::endl;
		out << "bank : minerals = " << minerals << "("<< getMineralsPerSecond() <<"/s)" << " vespene = " << vespene << "(" << getVespenePerSecond() << "/s)"<< " supply : " << getAvailableSupply() << ":" << getUsedSupply() << "/" << getMaxSupply() <<  std::endl;
	}
	int GameState::countUnit(UnitId unit) const
	{
		return std::count_if(units.begin(), units.end(), [unit](auto & u) {return u.type == unit ; });
	}
	UnitInstance::UnitInstance(UnitId type)
		: type(type), state(FREE), time_to_free(0) 
	{
	}
	void UnitInstance::print(std::ostream & out) const
	{
		out << TechTree::getTechTree().getUnitById(type).name;
		std::string status = to_string(state);
		
		out << "(" << status;
		if (time_to_free > 0)
			out << time_to_free;
		out << ")";
	}
	std::string to_string(const UnitInstance::UnitState &state)
	{
		std::string status;
		switch (state) {
		case UnitInstance::BUILDING:
			status = "B";
			break;
		case UnitInstance::MINING_MINERALS:
			status = "M";
			break;
		case UnitInstance::MINING_VESPENE:
			status = "V";
			break;
		case UnitInstance::BUSY:
			status = "C";
			break;
		case UnitInstance::FREE:
		default:
			status = "F";
			break;
		}
		return status;
	}
}