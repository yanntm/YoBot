#include "YoAgent.h"
#include "UnitTypes.h"
#include "Pathing.h"
#include "Placement.h"

using namespace sc2;
using namespace sc2util;

void YoAgent::OnStep()
{
	// do the updates
	updateFromObservation(Observation());
	// call onStep
	OnYoStep();
	// handle order queue
	dispatchOrders();
	actions->updateBusy();
}

void YoAgent::OnUnitEnterVision(const Unit * u)
{
	if (u->alliance == Unit::Alliance::Enemy) {
		enemies.insert_or_assign(u->tag, u);
	}
	if (sc2util::IsBuilding(u->unit_type)) {
		auto foot = Observation()->GetAbilityData()[(int)Observation()->GetUnitTypeData()[(int)u->unit_type].ability_id].footprint_radius * 2;
		setBuildingAt(info, u->pos, foot, true); // PAthing
		placer.setBuildingAt(info, u->pos, foot, true);
	}
}

void YoAgent::OnUnitDestroyed(const sc2::Unit * u)
{
	if (u->alliance == Unit::Alliance::Enemy) {
		enemies.erase(u->tag);
	}
	if (sc2util::IsBuilding(u->unit_type)) {
		auto foot = Observation()->GetAbilityData()[(int)Observation()->GetUnitTypeData()[(int)u->unit_type].ability_id].footprint_radius * 2;
		setBuildingAt(info, u->pos, foot, false);
		placer.setBuildingAt(info, u->pos, foot, false);
	}
}

void YoAgent::OnUnitCreated(const sc2::Unit * u)
{
	if (sc2util::IsBuilding(u->unit_type)) {
		auto foot = Observation()->GetAbilityData()[(int)Observation()->GetUnitTypeData()[(int)u->unit_type].ability_id].footprint_radius * 2;
		setBuildingAt(info, u->pos, foot, true);
		placer.setBuildingAt(info, u->pos, foot, true);
	}
}

const YoAgent::UnitsMap & YoAgent::allEnemies() const
{
	return enemies;
}

void YoAgent::updateFromObservation(const sc2::ObservationInterface * obs)
{
	minerals = Observation()->GetMinerals();
	gas = Observation()->GetVespene();
	supplyleft = Observation()->GetFoodCap() - Observation()->GetFoodUsed();

}

void YoAgent::dispatchOrders()
{
	std::unordered_map<sc2::Tag, const Order *> finalorders;
	for (const Order & order : orders) {
		auto it = finalorders.find(order.subject->tag);
		if (it == finalorders.end()) {
			finalorders[order.subject->tag] = &order;
		} else if ( it->second->priority >= order.priority) {
				// replace order
				it->second = &order;
		}
	}
	for (const auto & entry : finalorders) {
		const Order & order = *entry.second;
		if (order.tag == Order::SELF) {
			Actions()->UnitCommand(order.subject, order.ability);
		} else if (order.tag == Order::UNIT) {
			Actions()->UnitCommand(order.subject, order.ability, order.targetU); 
		} else { // it's a POS order
			Actions()->UnitCommand(order.subject, order.ability, order.targetP); 
		}
	}
	orders.clear();
}
