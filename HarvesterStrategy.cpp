#include <unordered_set>
#include <iostream>
#include "HarvesterStrategy.h"
#include "MapTopology.h"
#include "DistUtil.h"
#include "UnitTypes.h"

using namespace std;
using namespace sc2;



sc2::Point2D HarvesterStrategy::calcMagicSpot(const sc2::Unit* mineral, const sc2::Unit* nexus) {
	auto minpos = mineral->pos;
	auto pos1 = minpos + Point3D(-.25f, 0,0);
	auto pos2 = minpos + Point3D(+.25f, 0,0);
	if (DistanceSquared2D(nexus->pos, minpos) > DistanceSquared2D(nexus->pos, pos1)) {
		minpos = pos1;
	}
	if (DistanceSquared2D(nexus->pos, minpos) > DistanceSquared2D(nexus->pos, pos2)) {
		minpos = pos2;
	}
	
	// a vector from mineral to nexus
	auto vec = nexus->pos - minpos;
	// normalize
	vec /= Distance2D(Point2D(0, 0), vec);
	// add to mineral position
	return minpos + vec * 0.6f;
}

sc2::Point2D HarvesterStrategy::calcNexusMagicSpot(const sc2::Unit* mineral, const sc2::Unit* nexus, const sc2::GameInfo & info) {
	auto minpos = mineral->pos;
	auto pos1 = minpos + Point3D(-.25f, 0, 0);
	auto pos2 = minpos + Point3D(+.25f, 0, 0);
	if (DistanceSquared2D(nexus->pos, minpos) > DistanceSquared2D(nexus->pos, pos1)) {
		minpos = pos1;
	}
	if (DistanceSquared2D(nexus->pos, minpos) > DistanceSquared2D(nexus->pos, pos2)) {
		minpos = pos2;
	}

	// a vector from nexus to mineral
	auto vec = minpos - nexus->pos ;
	// normalize
	vec /= Distance2D(Point2D(0, 0), vec);
	for (int i = 0; i < 20; i++) {
		auto totry = nexus->pos + vec * (1.0f + 0.1f * i);
		if (sc2util::Pathable(info, totry)) {
			return totry;
		}
	}
	return nexus->pos;
}

void HarvesterStrategy::updateRoster(const sc2::Units & current)
{
	bool rosterChange = false;
	unordered_set<Tag> now;
	// create new workers
	for (auto u : current) {
		if (u->is_alive) {
			auto it = workerStates.find(u->tag);
			if (it == workerStates.end()) {
				if (IsCarryingMinerals(*u)) {
					workerStates.insert({ u->tag, { ReturningMineral, Entering, Distinct , true } });
				}
				else {
					workerStates.insert({ u->tag,{ MovingToMineral, Entering, Distinct , false } });
				}
				rosterChange = true;
			}
			now.insert(u->tag);
		}
	}
	// erase irrelevant ones
	for (auto it = begin(workerAssignedMinerals); it != end(workerAssignedMinerals);)
	{
		const Tag & tag = it->first;
		if (now.find(tag) == now.end())
		{
			workerStates.erase(tag);
			it = workerAssignedMinerals.erase(it);
			rosterChange = true;
		}
		else
			++it;
	}
	if (!current.empty()) {
		// compute overlapping probes
		float radius = (*current.begin())->radius  * 2;
		radius *= radius; // square it for speed
		for (auto i = current.begin(), _end = current.end(); i != _end ; ++i) {
			bool overlap = false;
			for (auto j = i; j != _end; ++j) {
				if (DistanceSquared2D((*i)->pos, (*j)->pos) < radius) {
					overlap = true;
					workerStates[(*j)->tag].overlap = Overlapping;
				}				
			}
			workerStates[(*i)->tag].overlap = overlap ? Overlapping : Distinct;
		}
	}

	for (auto it = minerals.begin() ; it != minerals.end(); ) {
		if ((*it)->mineral_contents <= 5) {
			int index = it - minerals.begin(); 
			it = minerals.erase(it);			
			for (auto & jt : workerAssignedMinerals) {
				if (jt.second == index) {
					jt.second = -1;
				}
				else if (jt.second > index) {
					jt.second--;
				}
			}
			rosterChange = true;
		}
		else {
			++it;
		}
	}

	if (rosterChange && ! minerals.empty()) {
		assignTargets(current);
	}
}


int HarvesterStrategy::getIdealHarvesters()
{
	if (nexus == nullptr || ! nexus->is_alive)
		return 0;
	auto pos = nexus->pos;
	std::function<int(const Unit *)> func = [pos](const Unit *u) {
		float d = Distance2D(u->pos, pos);
		if (d < 7.0f) return 2;
		if (d <= 9.0f) return 3;
		return 1;
	};
	int ideal = 0;
	for (auto m : minerals) {
		ideal += func(m);
	}
	return ideal;
}

int HarvesterStrategy::getCurrentHarvesters()
{
	return workerAssignedMinerals.size();
}

void HarvesterStrategy::initialize(const sc2::Unit * nexus, const sc2::Units & minerals, const sc2::ObservationInterface * obs)
{
	*this = HarvesterStrategy();
	this->nexus = nexus;
	this->minerals=minerals;
	sort(this->minerals.begin(), this->minerals.end(), [nexus](auto &a, auto &b) { return DistanceSquared2D(a->pos, nexus->pos) < DistanceSquared2D(b->pos, nexus->pos); });
	updateRoster(Units());
	for (auto targetMineral : this->minerals) {
		const sc2::Point2D magicSpot = calcMagicSpot(targetMineral,nexus);
		magicSpots.insert_or_assign(targetMineral->tag, magicSpot);
		auto magicNexus = calcNexusMagicSpot(targetMineral,nexus,obs->GetGameInfo());
		magicNexusSpots.insert_or_assign(targetMineral->tag, magicNexus);
	}
	allminerals = obs->GetUnits(Unit::Alliance::Neutral, [](const auto & u) { return sc2util::IsMineral(u.unit_type); });
}

void HarvesterStrategy::OnStep(const sc2::Units & probes, ActionInterface * actions, bool inDanger)
{
#ifdef DEBUG
	frame++;	
#endif
	if (!nexus->is_alive || nexus == nullptr) {
		nexus = nullptr;
		return;
	}
	if (probes.empty()) {
		return;
	}
	updateRoster(probes);

	if (minerals.empty()) {
		allminerals.erase(
			remove_if(allminerals.begin(), allminerals.end(), [](auto u) { return u->mineral_contents <= 5; }),
			allminerals.end()
		);
		auto min = sc2util::FindNearestUnit(nexus->pos, allminerals, 10000.0);
		// only deal with inactive probes
		for (auto p : probes) {
			if (p->orders.empty()) {				
				actions->UnitCommand(p, ABILITY_ID::SMART, min);
			}
		}
		return;
	}

	if (inDanger) {
		// only deal with inactive probes
		for (auto p : probes) {
			if (p->orders.empty()) {
				auto targetMineral = minerals[workerAssignedMinerals[p->tag]];
				actions->UnitCommand(p, ABILITY_ID::SMART, targetMineral);
			}
		}
		return;
	}
	for (auto p : probes) {
		auto & e = workerStates[p->tag];
		auto carrying = IsCarryingMinerals(*p);
		if (! carrying && e.had_mineral) {			
			e.harvest = MovingToMineral;
			e.move = Entering;
#ifdef DEBUG
		totalMined++;
		auto it = lastReturnedFrame.find(p->tag);
		if (it != lastReturnedFrame.end()) {			
			auto old = it->second;
			auto length = frame - old;
			roundtrips.push_back(length);
			lastTripTime[p->tag] = length;
		}
		lastReturnedFrame[p->tag] = frame;
#endif
		}
		else if (carrying && !e.had_mineral) {
			e.harvest = ReturningMineral;
			e.move = Entering;			
		}
		else if (!carrying) {
			float gatherDist = 1.7f;
			if (Distance2D(p->pos, minerals[workerAssignedMinerals[p->tag]]->pos) < gatherDist
				|| Distance2D(p->pos, magicSpots[minerals[workerAssignedMinerals[p->tag]]->tag]) < gatherDist
				) {
				e.harvest = GatheringMineral;
			}
		}
		e.had_mineral = carrying;
	}

	for (auto p : probes) {
		auto & e = workerStates[p->tag];
		if (e.harvest == MovingToMineral) {
			auto min = minerals[workerAssignedMinerals[p->tag]];
			auto mp = magicSpots[min->tag];
			
			if (e.move == Entering) {
				actions->UnitCommand(p, ABILITY_ID::SMART, mp);
				e.move = Accelerating;
			}
			else if (e.move == Accelerating) {				
				actions->UnitCommand(p, ABILITY_ID::SMART, mp);
				e.move = Accelerated;
			}
			else if (e.move == Accelerated) {
				// keep clicking
				actions->UnitCommand(p, ABILITY_ID::SMART, min);
				e.move = Coasting;
			}
			else if (e.move == Coasting) {
				float brakeDist = 2.2;
				if (Distance2D(p->pos, min->pos) < brakeDist
					|| Distance2D(p->pos,mp) < brakeDist
					) {
					e.move = Approaching;
					actions->UnitCommand(p, ABILITY_ID::SMART, mp);
				}
			}
			else if (e.move == Approaching) {
				actions->UnitCommand(p, ABILITY_ID::SMART, mp);
			}
		}
		else if (e.harvest == ReturningMineral) {
			if (e.move == Entering) {
				actions->UnitCommand(p, ABILITY_ID::SMART, nexus);
				e.move = Accelerating;
			}
		}
		else {
			//GatheringMineral, //actively mining the crystal with ability
			auto targetMineral = minerals[workerAssignedMinerals[p->tag]];
			if (p->orders.empty()) {
				actions->UnitCommand(p, ABILITY_ID::SMART, targetMineral);
			}
			else	if (p->orders[0].ability_id == sc2::ABILITY_ID::HARVEST_GATHER)
			{
				//keep worker unit at its mineral field
				if (p->orders[0].target_unit_tag != targetMineral->tag) //stay on assigned field
				{
					actions->UnitCommand(p, ABILITY_ID::SMART, targetMineral);
				}
			}
		}
	}
}



void HarvesterStrategy::assignTargets(const Units & workers)
{
	auto pos = nexus->pos;
	std::function<int(const Unit *)> func = [pos](const Unit *u) {
		float d = Distance2D(u->pos, pos);
		if (d < 7.0f) return 2;
		if (d <= 9.0f) return 3;
		return (int)d / 5;
	};

	std::vector<int> targets = allocateTargets(workers, minerals, func	, workerAssignedMinerals);
	for (int att = 0, e = targets.size(); att < e; att++) {
		if (targets[att] != -1) {
			workerAssignedMinerals[workers[att]->tag] = targets[att];			
		}
	}
}

std::vector<int> HarvesterStrategy::allocateTargets(const Units & probes, const Units & mins, std::function<int(const Unit *)> & toAlloc, std::unordered_map<Tag, int> current) {
	std::unordered_map<Tag, int> targetIndexes;
	for (int i = 0, e = mins.size(); i < e; i++) {
		targetIndexes.insert_or_assign(mins[i]->tag, i);
	}
	std::unordered_map<Tag, int> unitIndexes;
	for (int i = 0, e = probes.size(); i < e; i++) {
		targetIndexes.insert_or_assign(probes[i]->tag, i);
	}
	std::vector<int> targets;
	targets.resize(probes.size(), -1);

	std::vector<std::vector<int>> attackers;
	attackers.resize(mins.size());

	//std::remove_if(mins.begin(), mins.end(), [npos](const Unit * u) { return  Distance2D(u->pos, npos) > 6.0f;  });
	std::vector<int> freeAgents;
	std::deque<int> freeMins;

	bool overSat = getIdealHarvesters() < getCurrentHarvesters();

	if (! current.empty()) {
		int i = 0;
		for (const auto & u : probes) {
			auto it = current.find(u->tag);
			if (it != current.end() && it->second != -1) {
				int ind = it->second;
				targets[i] = ind;
				attackers[ind].push_back(i);
			}
			else {
				targets[i] = -1;
				freeAgents.push_back(i);
			}
			i++;
		}
		for (int i = 0, e = mins.size(); i < e; i++) {
			auto sz = attackers[i].size();
			int goodValue = toAlloc(mins[i]);
			if (sz < goodValue) {
				freeMins.push_back(i);
			}
		}
		for (int i = 0, e = mins.size(); i < e; i++) {
			auto sz = attackers[i].size();
			int goodValue = toAlloc(mins[i]);
			if (sz > goodValue && (! overSat || ! freeMins.empty())) {
				auto start = mins[i]->pos;
				std::sort(attackers[i].begin(), attackers[i].end(), [start, probes](int a, int b) { return DistanceSquared2D(start, probes[a]->pos) < DistanceSquared2D(start, probes[b]->pos); });

				for (int j = goodValue, e = attackers[i].size(); j < e; j++) {
					freeAgents.push_back(attackers[i][j]);
					targets[attackers[i][j]] = -1;
				}
				attackers[i].resize(goodValue);

			}			 
		}
	}
	else {
		for (int i = 0, e = probes.size(); i < e; i++) {
			freeAgents.push_back(i);
		}
		for (int i = 0, e = mins.size(); i < e; i++) {
			freeMins.push_back(i);
		}
	}

	


	if (!freeAgents.empty()) {
		Point2D cogp = probes[freeAgents.front()]->pos;
		int div = 1;
		for (auto it = ++freeAgents.begin(); it != freeAgents.end(); ++it) {
			cogp += probes[*it]->pos;
			div++;
		}
		cogp /= div;

		std::sort(freeAgents.begin(), freeAgents.end(), [cogp, probes](int a, int b) { return DistanceSquared2D(cogp, probes[a]->pos) < DistanceSquared2D(cogp, probes[b]->pos); });
	}
	while (!freeMins.empty() && !freeAgents.empty()) {
		int choice = freeAgents.back();
		freeAgents.pop_back();
		auto start = probes[choice]->pos;
		//std::sort(freeMins.begin(), freeMins.end(), [start, mins](int a, int b) { return DistanceSquared2D(start, mins[a]->pos) > DistanceSquared2D(start, mins[b]->pos); });

		stable_sort(freeMins.begin(), freeMins.end(), [attackers](auto a, auto b) { return attackers[a].size() < attackers[b].size(); });
		int mineral = freeMins.front();
		freeMins.pop_front();


		attackers[mineral].push_back(choice);
		targets[choice] = mineral;
		if (attackers[mineral].size() < toAlloc(mins[mineral])) {
			freeMins.push_back(mineral);
		}

	}
	while (freeMins.empty() && !freeAgents.empty()) {
		int choice = freeAgents.back();
		freeAgents.pop_back();
		// assign to far minerals first
		// these have high index
		int mineral = mins.size() - 1;
		for (; mineral >= 0; mineral--) {
			if (mineral==0 || attackers[mineral].size() <= attackers[mineral - 1].size()) {
				targets[choice] = mineral;
				attackers[mineral].push_back(choice);
				break;
			}
		}		
	}

	return targets;

}


#ifdef DEBUG
void HarvesterStrategy::PrintDebug(sc2::DebugInterface * debug, const sc2::ObservationInterface * obs)
{
	double avg = 0;
	if (!roundtrips.empty()) {
		double avg2 = 0;
		for (auto l : roundtrips) { avg2 += l; }
		avg2 /= roundtrips.size();
		size_t sz=0;
		for (auto l : roundtrips) { if (l <= 2 * avg2) { avg += l; sz++; } }
		avg /= sz;
	}
	debug->DebugTextOut("mins " + std::to_string(totalMined) + " in " + to_string(frame) + " avg : " + to_string(avg));
	debug->DebugTextOut(to_string(getCurrentHarvesters()) + "/" + to_string(getIdealHarvesters()), nexus->pos + Point3D(0,.2f,0.5f));
	if (frame == 3000) {
		std::cout << "Harvesting stats :" << std::to_string(totalMined) + " in " + to_string(frame) + " avg : " + to_string(avg);
	}
	for (auto & e : lastTripTime) {
		auto u = obs->GetUnit(e.first);
		if (u != nullptr)
			debug->DebugTextOut(std::to_string(e.second), u->pos + Point3D(0, 0, .2), Colors::Green);
	}
	unordered_map<Tag, int> perMin;
	for (auto &e : workerAssignedMinerals) {
		auto p = obs->GetUnit(e.first);
		if (p == nullptr) {
			continue;
		}
		if (e.second != -1 && e.second < minerals.size()) {
			auto m = minerals[e.second];
			auto it = perMin.find(m->tag);
			if (it != perMin.end()) {
				it->second++;
			}
			else {
				perMin[m->tag] = 1;
			}
			auto & out = m->pos;
			debug->DebugLineOut(p->pos, Point3D(out.x, out.y, out.z + 0.1f), Colors::Red);
		}
	}
	auto pos = nexus->pos;
	std::function<int(const Unit *)> func = [pos](const Unit *u) {
		float d = Distance2D(u->pos, pos);
		if (d < 7.0f) return 2;
		if (d <= 9.0f) return 3;
		return (int)d / 5;
	};

	int ind = 0;
	for (auto m : minerals) {
		auto & out = m->pos;
		debug->DebugTextOut(to_string(ind), Point3D(out.x, out.y + 0.2, out.z + 0.1f));
		auto ideal = func(m);
		debug->DebugTextOut(to_string(perMin[m->tag])+"/"+to_string(ideal), Point3D(out.x, out.y - 0.2, out.z + 0.1f));		
		ind++;
	}
	ind = 0;
	for (auto mp : magicSpots) {
		auto & p = mp.second;
		auto  m = obs->GetUnit(mp.first);
		if (m != nullptr)
			debug->DebugSphereOut(Point3D(p.x, p.y, m->pos.z + 0.1f), 0.1, Colors::Green);
		ind++;
	}
	ind = 0;
	for (auto mp : magicNexusSpots) {
		auto & p = mp.second;
		auto  m = obs->GetUnit(mp.first);
		if (m != nullptr)
			debug->DebugSphereOut(Point3D(p.x, p.y, m->pos.z + 0.1f), 0.1, Colors::Green);
		ind++;
	}
}
#endif