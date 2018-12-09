#pragma once
#include "sc2api/sc2_interfaces.h"
#include <unordered_set>
#include <deque>

namespace sc2 {

	class YoAction :
		public sc2::ActionInterface
	{
		sc2::ActionInterface * deco;
		std::deque<std::unordered_set<Tag> > busyUnits;

		// make sure this unit is left alone for 3 frames
		void busy(Tag tag) {
			busyUnits.back().insert(tag);
		}

		

		
	public:
		void setDeco(sc2::ActionInterface * deco) { this->deco = deco;  }
		bool isBusy(Tag tag) {
			return std::any_of(busyUnits.begin(), busyUnits.end(), [tag](auto & set) { return set.find(tag) != set.end(); });
		}
		void updateBusy() {
			busyUnits.pop_front();
			busyUnits.push_back({});
		}
		YoAction(sc2::ActionInterface * act);
		//! Issues a command to a unit. Self targeting.
		//!< \param unit The unit to send the command to.
		//!< \param ability The ability id of the command.
		void UnitCommand(const Unit* unit, AbilityID ability, bool queued_command = false) ;

		//! Issues a command to a unit. Targets a point.
		//!< \param unit The unit to send the command to.
		//!< \param ability The ability id of the command.
		//!< \param point The 2D world position to target.
		 void UnitCommand(const Unit* unit, AbilityID ability, const Point2D& point, bool queued_command = false) ;

		//! Issues a command to a unit. Targets another unit.
		//!< \param unit The unit to send the command to.
		//!< \param ability The ability id of the command.
		//!< \param target The unit that is a target of the unit getting the command.
		void UnitCommand(const Unit* unit, AbilityID ability, const Unit* target, bool queued_command = false) ;

		//! Issues a command to multiple units (prefer this where possible). Same as UnitCommand(Unit, AbilityID).
		void UnitCommand(const Units& units, AbilityID ability, bool queued_move = false);

		//! Issues a command to multiple units (prefer this where possible). Same as UnitCommand(Unit, AbilityID, Point2D).
		void UnitCommand(const Units& units, AbilityID ability, const Point2D& point, bool queued_command = false) ;

		//! Issues a command to multiple units (prefer this where possible). Same as UnitCommand(Unit, AbilityID, Unit).
		void UnitCommand(const Units& units, AbilityID ability, const Unit* target, bool queued_command = false);

		//! Returns a list of unit tags that have sent commands out in the last call to SendActions. This will be used to determine
		//! if a unit actually has a command when the observation is received.
		//!< \return Array of units that have sent commands.
		 const std::vector<Tag>& Commands() const ;

		//! Enables or disables autocast of an ability on a unit.
		//!< \param unit_tag The unit to toggle the ability on.
		//!< \param ability The ability to be toggled.
		void ToggleAutocast(Tag unit_tag, AbilityID ability) ;
		//! Enables or disables autocast of an ability on a list of units.
		//!< \param unit_tags The units to toggle the ability on.
		//!< \param ability The ability to be toggled.
		void ToggleAutocast(const std::vector<Tag>& unit_tags, AbilityID ability) ;

		//! Sends a message to the game chat.
		//!< \param message Text of message to send.
		//!< \param channel Which players will see the message.
		void SendChat(const std::string& message, ChatChannel channel = ChatChannel::All) ;

		//! This function sends out all batched unit commands. You DO NOT need to call this function in non real time simulations since
		//! it is automatically called when stepping the simulation forward. You only need to call this function in a real time simulation.
		//! For example, if you wanted to move 20 marines to some position on the map you'd want to batch all of those unit commands and
		//! send them at once.
		void SendActions();
		~YoAction() {}
	};

}