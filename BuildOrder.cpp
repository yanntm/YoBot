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
	//	static std::mutex m;
		static const TechTree singletonTech;
	//	std::unique_lock<std::mutex> l(m);
		return singletonTech;
	}
	void BuildOrder::print(std::ostream & out)
	{
		int step = 1;
		for (auto & bi : items) {
			out << step++ << ":";
			bi.print(out);
			out << std::endl;
		}
		out << "Reached state at " << final.getTimeStamp() / 60 << "m" << final.getTimeStamp() % 60 << "s"<< std::endl;
		final.print(out);
	}
	void BuildItem::print(std::ostream & out) const
	{
		if (time != 0) {
			out << time / 60 << ":" << time % 60 << " (" << time << " s)";
		}
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
		else if (action == WAIT_GOAL) {
			out << "Wait for Goal";
		}
		out << " waited : ";
		if (timeMin > 0) out << " min: " << timeMin;
		if (timeVesp > 0) out << " vesp: " << timeVesp;
		if (timePre > 0) out << " pre: " << timePre;
		if (timeFree > 0) out << " free: " << timeFree; 
		if (timeFood > 0) out << " food: " << timeFood;		
	}
	int GameState::probesToSaturation() const
	{
		int nexi = countUnit(UnitId::PROTOSS_NEXUS);
		int ass = countUnit(UnitId::PROTOSS_ASSIMILATOR);
		int prob = countUnit(UnitId::PROTOSS_PROBE);
		return (20 * nexi + 3 * ass) - prob;
	}
	bool GameState::hasFreeUnit(UnitId unit) const
	{
		return std::any_of(freeUnits.begin(), freeUnits.end(), [unit](auto & u) {
			return (u.type == unit && u.state == u.FREE) 
				|| (sc2util::IsWorkerType(unit) && u.state == u.MINING_MINERALS); 
		});
	}
	bool GameState::hasFinishedUnit(UnitId unit) const
	{
		return std::any_of(busyUnits.begin(), busyUnits.end(), [unit](auto & u) {
			return (u.type == unit && u.state != u.BUILDING)
				;
		})
			||
			std::any_of(freeUnits.begin(), freeUnits.end(), [unit](auto & u) {
			return (u.type == unit && u.state != u.BUILDING)
				;
		})
		;
	}
	void GameState::addUnit(UnitId unit)
	{
		addUnit(UnitInstance(unit));		
	}
	void GameState::addUnit(const UnitInstance & unit)
	{
		if (unit.state == UnitInstance::MINING_MINERALS || (unit.state == UnitInstance::FREE && sc2util::IsWorkerType(unit.type) )) {
			freeUnits.push_back(unit);
			mps = -1.0f;
		}
		else if (unit.state == UnitInstance::FREE) {
			if (!sc2util::IsBuilding(unit.type)) {
				attackUnits.push_back(unit);
			}
			else {
				freeUnits.push_back(unit);
			}
		}
		else
		{
			busyUnits.push_back(unit);
			if (unit.state == UnitInstance::MINING_VESPENE) {
				vps = -1.0f;
			}
		}
		if (TechTree::getTechTree().getUnitById(unit.type).food_provided != 0) {
			supply = -1;
		}
	}
	float GameState::getMineralsPerSecond() const
	{
		if (mps == -1.0) {
			int nexi = 0;
			int active = 0;
			for (auto & u : freeUnits) {
				if (u.type == UnitId::PROTOSS_PROBE && u.state == u.MINING_MINERALS) {
					active++;					
				}
				else if (u.type == UnitId::PROTOSS_NEXUS &&  u.state != UnitInstance::BUILDING) {
					nexi++;
				}
			}
			for (auto & u : busyUnits) {
				if (u.type == UnitId::PROTOSS_NEXUS &&  u.state != UnitInstance::BUILDING) {
					nexi++;
				}
			}

			int realactive = std::min(20 * nexi, active);
			// 115 frames : 0,9739130435
			mps = (0.896 * realactive); // as measured for a RT average time of 125 frames
			// 138 (3workers) : 0,8115942029			
		}

		return mps;
	}
	float GameState::getVespenePerSecond() const
	{
		if (vps == -1.0) {
			vps = 0;
			for (auto & u : busyUnits) {
				if (u.type == UnitId::PROTOSS_PROBE && u.state == u.MINING_VESPENE) {
					vps += 0.649; // 138 (3workers) : 0,6492753623
				}
			}
		}
		return vps;
	}
	inline int getFood(const UnitInstance & u, const TechTree & tech) {
		auto & unit = tech.getUnitById(u.type);
		if (unit.food_provided < 0) {
			return unit.food_provided;
		}
		else if (u.state != u.BUILDING) {
				return unit.food_provided;
		}
		return 0;
	}
	int GameState::getAvailableSupply() const
	{
		if (supply == -1) {
			auto & tech = TechTree::getTechTree();
			int sum = 0;
			
			for (auto & u : freeUnits) {
				sum += getFood(u,tech);
			}
			for (auto & u : attackUnits) {
				sum += getFood(u, tech);
			}
			for (auto & u : busyUnits) {
				sum += getFood(u, tech);
			}
			supply = sum;
		} 

		return supply;
	}
	int GameState::getUsedSupply() const
	{
		auto & tech = TechTree::getTechTree();
		int sum = 0;
		for (auto & u : freeUnits) {
			auto f = getFood(u, tech);
			if (f < 0) {
				sum -= f;
			}
		}
		for (auto & u : attackUnits) {
			auto f = getFood(u, tech);
			if (f < 0) {
				sum -= f;
			}
		}
		for (auto & u : busyUnits) {
			auto f = getFood(u, tech);
			if (f < 0) {
				sum -= f;
			}
		}
		return sum;
	}
	int GameState::getMaxSupply() const
	{
		auto & tech = TechTree::getTechTree();
		int sum = 0;
		for (auto & u : freeUnits) {
			auto f = getFood(u,tech);
			if (f > 0) {
				sum += f;
			}
		}
		for (auto & u : busyUnits) {
			auto f = getFood(u,tech);
			if (f > 0) {
				sum += f;
			}
		}

		return sum;
	}
	void GameState::stepForward(int secs)
	{
		for (int i = 0; i < secs; i++) {
			bool changed = false;
			for (auto it = busyUnits.begin(); it != busyUnits.end() ; /*NOP*/) {
				auto & u = *it;
				bool erased = false;
				if (u.time_to_free > 0) {
					u.time_to_free--;
					if (u.time_to_free == 0) {
						if (!sc2util::IsWorkerType(u.type)) {
							u.state = u.FREE;	
							auto & unit = TechTree::getTechTree().getUnitById(u.type);
							addUnit(u);
							erased = true;
						}
						else {
							// default for workers is to mine
							u.state = u.MINING_MINERALS;
							addUnit(u);
							erased = true;
						}						
						changed = true;
					}
				}
				if (erased) {
					it = busyUnits.erase(it);
				}
				else {
					++it;
				}
			}
			if (changed) {
				mps = -1.0f;
				vps = -1.0f;
				supply = -1;
			}
			minerals += getMineralsPerSecond() ;
			vespene += getVespenePerSecond() ;
			timestamp += 1;
		}
		
	}
	bool GameState::waitForResources(int mins, int vesp, std::pair<int,int> * waited)
	{
		if (mins > minerals && getMineralsPerSecond() > 0) {
			int secs = (mins - minerals) / mps;
			if ( ((mins - minerals) / mps) - secs > 0) {
				secs += 1;
			}
			stepForward(secs);
			if (waited != nullptr) {
				waited->first = secs;
			}
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
			if (waited != nullptr) {
				waited->second = secs;
			}
			// std::cout << "Waited for vespene for " << secs << "s." << std::endl;
		}
		else if (vesp > vespene) {
			return false;
		}
		return true;
	}
	bool GameState::waitforUnitCompletion(UnitId id)
	{
		auto it = std::find_if(busyUnits.begin(), busyUnits.end(), [id](auto & u) {return u.type == id && u.state == u.BUILDING; });
		if (it == busyUnits.end()) {
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
		auto it = std::find_if(freeUnits.begin(), freeUnits.end(), [id](auto & u) {return u.type == id && u.state == u.FREE || (sc2util::IsWorkerType(id) && u.state == u.MINING_MINERALS); });
		if (it != freeUnits.end()) {
			return true;
		}
		int index = 0;
		int best = -1;
		for (auto & u : busyUnits) {
			if (u.type == id && u.state != u.FREE && u.time_to_free != 0) {
				if (best == -1 || busyUnits[best].time_to_free > u.time_to_free) {
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
			stepForward(busyUnits[best].time_to_free);
			return true;
		}
	}
	bool GameState::waitforAllUnitFree()
	{
		int index = 0;
		int best = -1;
		for (auto & u : busyUnits) {
			if (u.state != u.FREE && u.time_to_free != 0) {
				if (best == -1 || busyUnits[best].time_to_free < u.time_to_free) {
					best = index;
				}
			}
			index++;
		}
		if (best == -1) {
			return true;
		}
		else {
			// std::cout << "Waited for all to be free for " << units[best].time_to_free << "s." << std::endl;
			stepForward(busyUnits[best].time_to_free);
			return true;
		}		
	}
	bool GameState::waitforFreeSupply(int needed)
	{
		int cur = getAvailableSupply();
		if (cur >= needed) {
			return true;
		}
		auto it = std::find_if(busyUnits.begin(), busyUnits.end(), [](auto & u) {return u.type == UnitId::PROTOSS_PYLON || u.type == UnitId::PROTOSS_NEXUS && u.state != u.FREE ; });
		if (it == busyUnits.end()) {
			return false;
		}
		int index = 0;
		int best = -1;
		for (auto & u : busyUnits) {
			if (u.state == u.BUILDING && (u.type == UnitId::PROTOSS_PYLON || u.type == UnitId::PROTOSS_NEXUS)) {
				if (best == -1 || busyUnits[best].time_to_free > u.time_to_free) {
					best = index;
				}
			}
			index++;
		}
		if (best == -1) {
			return false;
		}
		else {
			//std::cout << "Waited for " << TechTree::getTechTree().getUnitById(units[best].type).name << " to be provide food for " << units[best].time_to_free << "s." << std::endl;
			stepForward(busyUnits[best].time_to_free);
			return true;
		}
		
	}
	bool GameState::assignProbe(UnitInstance::UnitState state)
	{
		int done = 0;
		for (auto it = freeUnits.begin(); it != freeUnits.end(); ) { 
			bool erase = false;
			auto & u = *it;
			if (sc2util::IsWorkerType(u.type) && ( u.state == u.FREE || u.state == u.MINING_MINERALS) ) {
				u.state = UnitInstance::MINING_VESPENE;
				vps = -1;
				mps = -1;
				busyUnits.push_back(u);
				erase = true;
				done++;
			}
			if (erase) {
				it = freeUnits.erase(it);
			}
			else {
				++it;
			}
			if (done >= 3)
				break;
		}
		return done>=3;
	}
	bool GameState::assignFreeUnit(UnitId type, UnitInstance::UnitState state, int time)
	{
		for (auto it = freeUnits.begin(); it != freeUnits.end(); ++it) {
			auto & u = *it;
			if (type == u.type && u.state == u.FREE) {
				u.state = state;
				u.time_to_free = time;
				busyUnits.push_back(u);
				it = freeUnits.erase(it);
				return true;
			}
		}
		return false;
	}
	void GameState::print(std::ostream & out) const
	{
		auto & tech = TechTree::getTechTree();
		std::unordered_map<UnitId, std::unordered_map<UnitInstance::UnitState, int> > perUnitPerState;
		for (auto & u : freeUnits) {
			auto it = perUnitPerState.find(u.type);
			if (it == perUnitPerState.end()) {
				perUnitPerState[u.type] = { {u.state,1} };
			}
			else {
				auto & perState = it->second;
				perState[u.state] += 1;
			}
		}
		for (auto & u : busyUnits) {
			auto it = perUnitPerState.find(u.type);
			if (it == perUnitPerState.end()) {
				perUnitPerState[u.type] = { {u.state,1} };
			}
			else {
				auto & perState = it->second;
				perState[u.state] += 1;
			}
		}
		for (auto & u : attackUnits) {
			auto it = perUnitPerState.find(u.type);
			if (it == perUnitPerState.end()) {
				perUnitPerState[u.type] = { {u.state,1} };
			}
			else {
				auto & perState = it->second;
				perState[u.state] += 1;
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
		out << "bank : minerals = " << minerals << "("<< getMineralsPerSecond() * 60 <<"/min)" << " vespene = " << vespene << "(" << getVespenePerSecond() *60 << "/min)"<< " supply : " << getAvailableSupply() << ":" << getUsedSupply() << "/" << getMaxSupply() <<  std::endl;
	}
	int GameState::countUnit(UnitId unit) const
	{
		return std::count_if(freeUnits.begin(), freeUnits.end(), [unit](auto & u) {return u.type == unit; })
			+ std::count_if(busyUnits.begin(), busyUnits.end(), [unit](auto & u) {return u.type == unit; })
			+ std::count_if(attackUnits.begin(), attackUnits.end(), [unit](auto & u) {return u.type == unit; });
	}
	int GameState::countFreeUnit(UnitId unit) const
	{
		return std::count_if(freeUnits.begin(), freeUnits.end(), [unit](auto & u) {return u.type == unit && u.state == UnitInstance::FREE; });
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