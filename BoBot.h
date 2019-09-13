#pragma once

#include "YoAgent.h"
#include "BuildOrder.h"
#include "DistUtil.h"

namespace suboo {

	using namespace sc2;
	using namespace sc2util;

	class BoBot : public YoAgent {
		BuildOrder bo;
		int curstep = 0;
	public:
		void setBO(const BuildOrder & bo) { this->bo = bo; }

		void OnGameStart() {
			YoAgent::initialize();
		}

		void OnYoStep() {
			if (curstep >= bo.getItems().size()) {
				OnGameEnd();
			}
			auto bi = bo.getItems().at(curstep);

			auto harvesters = manageHarvesters();

			if (bi.getAction() == BUILD) {
				auto & unit = TechTree::getTechTree().getUnitById(bi.getTarget());
				if (minerals >= unit.mineral_cost && gas >= unit.vespene_cost) {
					if (unit.builder == UnitId::PROTOSS_PROBE) {
						// it is a building a priori

						// TODO : 3 should be real footprint, 20 is an approx distance about the size of a base
						auto places = placer.Placements(Observation()->GetGameInfo(), map.getPosition(map.ally, map.main), 3, 20);
						if (places.empty()) {
							throw "no space to put building !";
						}
						auto & p = places[0];
						auto builder = findProbeNear(p);
						harvesters.erase(builder->tag);

						// do it
						Actions()->UnitCommand(builder,	unit.build_ability,  bi.getTarget(),	p);
					}
					else if (-unit.food_provided <= Observation()->GetFoodCap() - Observation()->GetFoodUsed()) {

					}
				}

			}
			else {
				// TODO : harvest...
			}
		}

		~BoBot() {}

		std::unordered_set<Tag> manageHarvesters() {
			std::unordered_set<Tag> harvesters;
			for (auto & p : Observation()->GetUnits(sc2::Unit::Alliance::Self, [&](const auto & u) { return u.unit_type == UNIT_TYPEID::PROTOSS_PROBE; })) {
				bool busy = false;

				if (IsCarryingVespene(*p)) {
					// or probes that return gas become free on frame they return it
					//YoBot::busy(p->tag);
					if (p->orders.empty()) {
						Actions()->UnitCommand(p, ABILITY_ID::HARVEST_RETURN);
					}
					continue;
				}
				if (p->engaged_target_tag != 0) {
					auto target = Observation()->GetUnit(p->engaged_target_tag);
					if (target != nullptr && (target->vespene_contents != 0 || target->unit_type == UNIT_TYPEID::PROTOSS_ASSIMILATOR))
						continue;
				}
				for (auto o : p->orders) {
					if (o.ability_id != ABILITY_ID::MOVE && o.ability_id != ABILITY_ID::HARVEST_GATHER && o.ability_id != ABILITY_ID::HARVEST_RETURN) {
						busy = true;
						break;
					}
					if (o.ability_id == ABILITY_ID::HARVEST_GATHER) {
						auto target = Observation()->GetUnit(o.target_unit_tag);
						if (target != nullptr && (target->vespene_contents != 0 || target->unit_type == UNIT_TYPEID::PROTOSS_ASSIMILATOR)) {
							busy = true;
							break;
						}
					}
				}
				if (!busy)
					harvesters.insert(p->tag);
			}
			return harvesters;
		}		

		const sc2::Unit *  findProbeNear (const Point2D & pos) {
			Units probes = Observation()->GetUnits(sc2::Unit::Alliance::Self, [&](const auto & u) { return u.unit_type == UNIT_TYPEID::PROTOSS_PROBE; });
			if (any_of(probes.begin(), probes.end(), [&](const auto & p)
			{
				return any_of(p->orders.begin(), p->orders.end(), [&](const auto & o)
				{ return o.ability_id == tobuild; });
			})) {
				return false;
			}
			if (probes.empty()) {
				return false;
			}
			sortByDistanceTo(probes, pos);
			const sc2::Unit * builder = nullptr;
			for (const auto & p : probes) {
				if (YoActions()->isBusy(p->tag) || orderBusy(p)) {
					continue;
				}
				else {
					builder = p;
					break;
				}
			}
			if (builder == nullptr) {
				return false;
			}
			return builder;
		}

		bool orderBusy(const Unit * bob) {
			return std::any_of(bob->orders.begin(), bob->orders.end(), [](const auto & o) {
				return (o.ability_id != ABILITY_ID::PATROL && o.ability_id != ABILITY_ID::MOVE && o.ability_id != ABILITY_ID::HARVEST_GATHER);
			});
		}

	};
}