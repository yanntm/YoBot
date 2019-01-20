#include "ArmyStrategy.h"
#include "UnitTypes.h"
#include <unordered_set>


using namespace std;
using namespace sc2;
using namespace sc2util;

bool CombatStrategy::updateRoster(const sc2::Units & current)
{
	bool rosterChange = false;
	unordered_set<Tag> now;
	// create new workers
	for (auto u : current) {
		if (u->is_alive) {
			auto it = soldierStates.find(u->tag);
			if (it == soldierStates.end()) {
				soldierStates.insert({ u->tag,  Regrouping  });				
				rosterChange = true;
			}
			now.insert(u->tag);
		}
	}
	// erase irrelevant ones
	for (auto it = begin(soldierStates); it != end(soldierStates);)
	{
		const Tag & tag = it->first;
		if (now.find(tag) == now.end())
		{
			it = soldierStates.erase(it);			
			rosterChange = true;
		}
		else
			++it;
	}
	return rosterChange;
}

void CombatStrategy::initialize(const sc2::ObservationInterface * obs)
{
	for (const auto u : obs->GetUnits(Unit::Alliance::Self, [](const auto & u) { return u.is_alive &&  IsArmyUnitType(u.unit_type); })) {
		soldierStates[u->tag] = CombatState::Regrouping;
	}
}

void CombatStrategy::OnStep(const sc2::ObservationInterface * obs, sc2::YoAction * actions)
{
	auto all = obs->GetUnits(Unit::Alliance::Self, [](const auto & u) { return u.is_alive &&  IsArmyUnitType(u.unit_type); });
	updateRoster(all);
}

