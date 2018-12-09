#include "YoAction.h"
#include <iostream>

namespace sc2 {

	YoAction::YoAction(sc2::ActionInterface * act) :deco(act)
	{
		// three sets of units
		busyUnits.push_back({});
		busyUnits.push_back({});
		busyUnits.push_back({});
	}

	void sc2::YoAction::UnitCommand(const Unit * unit, AbilityID ability, bool queued_command)
	{
		if (queued_command || (!isBusy(unit->tag) && std::none_of(unit->orders.begin(), unit->orders.end(), [&ability](auto & o) { return o.ability_id == ability; }))) {
				deco->UnitCommand(unit, ability, queued_command);
				busy(unit->tag);
		}
		else {
			//std::cout << "filtered" << std::endl;
		}
	}

	void YoAction::UnitCommand(const Unit * unit, AbilityID ability, const sc2::Point2D & point, bool queued_command)
	{
		if (queued_command || 
			(!isBusy(unit->tag) 
				&& std::none_of(unit->orders.begin(), unit->orders.end(), 
					[&](const sc2::UnitOrder & o) { 
				return o.ability_id == ability &&  point.x == o.target_pos.x && point.y == o.target_pos.y; }))) {
				deco->UnitCommand(unit, ability, point, queued_command);
				busy(unit->tag);
			}
			else {
			//std::cout << "filtered2" << std::endl;
			}
	}

	void YoAction::UnitCommand(const Unit * unit, AbilityID ability, const Unit * target, bool queued_command)
	{
		if (queued_command ||  (!isBusy(unit->tag) && std::none_of(unit->orders.begin(), unit->orders.end(), [&](const sc2::UnitOrder & o) { return o.ability_id == ability && target->tag == o.target_unit_tag ; }))) {
			deco->UnitCommand(unit, ability, target, queued_command);
			busy(unit->tag);
		}
		else {
			//std::cout << "filtered3" << std::endl;
		}
	}

	void YoAction::UnitCommand(const Units & units, AbilityID ability, bool queued_move)
	{
		deco->UnitCommand(units, ability, queued_move);
	}

	void YoAction::UnitCommand(const Units & units, AbilityID ability, const Point2D & point, bool queued_command)
	{
		deco->UnitCommand(units, ability, point, queued_command);
	}

	void YoAction::UnitCommand(const Units & units, AbilityID ability, const Unit * target, bool queued_command)
	{
		deco->UnitCommand(units, ability, target, queued_command);
	}

	const std::vector<Tag>& YoAction::Commands() const
	{
		return deco->Commands() ;
	}

	void YoAction::ToggleAutocast(Tag unit_tag, AbilityID ability)
	{
		deco->ToggleAutocast(unit_tag,ability);
	}

	void YoAction::ToggleAutocast(const std::vector<Tag>& unit_tags, AbilityID ability)
	{
		deco->ToggleAutocast(unit_tags, ability);
	}

	void YoAction::SendChat(const std::string & message, ChatChannel channel)
	{
		deco->SendChat(message, channel);
	}




	void YoAction::SendActions()
	{
		deco->SendActions();
	}
}