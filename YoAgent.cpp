#include "YoAgent.h"

using namespace sc2;

void YoAgent::OnStep()
{
	// do the updates
	updateFromObservation(Observation());
	// call onStep
	OnYoStep();
	// handle order queue
	dispatchOrders();
}

void YoAgent::OnUnitEnterVision(const Unit * u)
{
	if (u->alliance == Unit::Alliance::Enemy) {
		enemies.insert_or_assign(u->tag, u);
	}

}

void YoAgent::OnUnitDestroyed(const sc2::Unit * unit)
{
	if (unit->alliance == Unit::Alliance::Enemy) {
		enemies.erase(unit->tag);
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
	for (const Order & order : orders) {
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
