#include <unordered_set>
#include <iostream>
#include "HarvesterStrategy.h"

using namespace std;
using namespace sc2;



sc2::Point2D HarvesterStrategy::calcMagicSpot(const sc2::Unit* mineral) {
	const sc2::Unit* closestTH = nexus;
	if (closestTH == nullptr) return sc2::Point3D(0, 0, 0); //in case there are no TownHalls left (or they're flying :)

															//must give a position right in front of crystal, closest to base //base location??

	float offset = 0.1f, xOffset, yOffset;
	Point2D magicSpot;

	/*float tx = closestTH->pos.x, ty = closestTH->pos.y;
	float mx = mineral->pos.x,   my = mineral->pos.y;

	if (tx > mx)	magicSpot.x = (tx - mx) * 0.1f;
	else			magicSpot.x = mineral->pos.x - offset;
	if (ty > my)	magicSpot.y = mineral->pos.y + offset;
	else			magicSpot.y = mineral->pos.y - offset;*/

	if (closestTH->pos.x < 60.0f) xOffset = 0.1f; else xOffset = -0.1f;
	if (closestTH->pos.y < 60.0f) yOffset = 0.1f; else yOffset = -0.1f;

	return Point2D(mineral->pos.x + xOffset, mineral->pos.y + yOffset); //magicSpot
}

void HarvesterStrategy::updateRoster(const sc2::Units & current)
{
	bool rosterChange = false;
	unordered_set<Tag> now;
	// create new workers
	for (auto u : current) {
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

	if (rosterChange) {
		assignTargets(current);
	}
}


int HarvesterStrategy::getIdealHarvesters()
{
	return minerals.size() * 2 + 1;
}

int HarvesterStrategy::getCurrentHarvesters()
{
	return workerAssignedMinerals.size();
}

void HarvesterStrategy::initialize(const sc2::Unit * nexus, const sc2::Units & minerals)	
{
	this->nexus = nexus;
	this->minerals=minerals;
	for (auto targetMineral : minerals) {
		const sc2::Point2D magicSpot = calcMagicSpot(targetMineral);
		magicSpots.insert_or_assign(targetMineral->tag, magicSpot);
	}
}

void HarvesterStrategy::OnStep(const sc2::Units & probes, ActionInterface * actions)
{
#ifdef DEBUG
	frame++;	
#endif

	updateRoster(probes);
	
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
		}
		lastReturnedFrame[p->tag] = frame;
#endif
		}
		else if (carrying && !e.had_mineral) {
			e.harvest = ReturningMineral;
			e.move = Entering;			
		}
		e.had_mineral = carrying;
	}

	for (auto p : probes) {
		auto & e = workerStates[p->tag];
		if (e.harvest == MovingToMineral) {
			if (e.move == Entering) {
				actions->UnitCommand(p, ABILITY_ID::SMART, minerals[workerAssignedMinerals[p->tag]]);
				e.move = Accelerating;
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

		}
	}
}



void HarvesterStrategy::assignTargets(const Units & workers)
{
	std::vector<int> targets = allocateTargets(workers, minerals, [](const Unit *u) { return  2; });
	for (int att = 0, e = targets.size(); att < e; att++) {
		if (targets[att] != -1) {
			workerAssignedMinerals[workers[att]->tag] = targets[att];			
		}
	}
}

std::vector<int> HarvesterStrategy::allocateTargets(const Units & probes, const Units & mins, int(*toAlloc) (const Unit *), bool keepCurrent) {
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
	std::vector<int> freeMins;

	if (keepCurrent) {
		int i = 0;
		for (const auto & u : probes) {
			if (!u->orders.empty()) {
				const auto & o = u->orders.front();
				if (o.target_unit_tag != 0) {
					auto pu = targetIndexes.find(o.target_unit_tag);
					if (pu == targetIndexes.end()) {
						targets[i] = -1;
					}
					else {
						int ind = pu->second;
						targets[i] = ind;
						attackers[ind].push_back(i);
					}
				}
			}
			i++;
		}
		for (int i = 0, e = mins.size(); i < e; i++) {
			auto sz = attackers[i].size();
			int goodValue = toAlloc(mins[i]);
			if (sz > goodValue) {
				auto start = mins[i]->pos;
				std::sort(attackers[i].begin(), attackers[i].end(), [start, probes](int a, int b) { return DistanceSquared2D(start, probes[a]->pos) < DistanceSquared2D(start, probes[b]->pos); });

				for (int j = goodValue, e = attackers[i].size(); j < e; j++) {
					freeAgents.push_back(attackers[i][j]);
					targets[attackers[i][j]] = -1;
				}
				attackers[i].resize(goodValue);

			}
			else if (sz < goodValue) {
				freeMins.push_back(i);
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
		std::sort(freeMins.begin(), freeMins.end(), [start, mins](int a, int b) { return DistanceSquared2D(start, mins[a]->pos) > DistanceSquared2D(start, mins[b]->pos); });

		int mineral = freeMins.back();
		freeMins.pop_back();


		attackers[mineral].push_back(choice);
		targets[choice] = mineral;
		if (attackers[mineral].size() < toAlloc(mins[mineral])) {
			freeMins.insert(freeMins.begin(), mineral);
		}

	}

	return targets;

}


#ifdef DEBUG
void HarvesterStrategy::PrintDebug(sc2::DebugInterface * debug)
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
	debug->DebugTextOut("Harvested :" + std::to_string(totalMined) + " in " + to_string(frame) + " avg : " + to_string(avg));
	if (frame == 3000) {
		std::cout << "Harvesting stats :" << std::to_string(totalMined) + " in " + to_string(frame) + " avg : " + to_string(avg);
	}
}
#endif