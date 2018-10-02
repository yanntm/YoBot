#include "YoAgent.h"

void YoAgent::OnStep()
{
	// do the updates
	updateFromObservation(Observation());
	// call onStep
	OnYoStep();
	// handle order queue
	dispatchOrders();
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
