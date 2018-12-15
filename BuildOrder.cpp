#include "BuildOrder.h"
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

	void BuildGoal::print(std::ostream & out)
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
		static std::mutex m;
		static TechTree singletonTech;
		std::unique_lock<std::mutex> l(m);
		return singletonTech;
	}
	int TechTree::getUnitIndex(UnitId id) const
	{
		return indexes[(int)id];
	}
	const Unit & TechTree::getUnitById(UnitId id) const
	{
		return units[getUnitIndex(id)];
	}
	const Unit & TechTree::getUnitByIndex(int index) const
	{
		return units[index];
	}
	void BuildOrder::print(std::ostream & out)
	{
		int step = 1;
		for (auto & bi : items) {
			out << step++ << ":";
			bi.print(out);
			out << std::endl;
		}
	}
	void BuildOrder::addItem(UnitId tocreate)
	{		
		items.emplace_back(BuildItem(tocreate));
	}
	void BuildOrder::addItemFront(UnitId tocreate)
	{
		items.push_front(BuildItem(tocreate));
	}
	void BuildItem::print(std::ostream & out)
	{
		auto & tech = TechTree::getTechTree();
		out << "Build " << tech.getUnitById(target).name;
	}
	bool GameState::hasUnit(UnitId unit) const
	{
		return std::any_of(units.begin(), units.end(), [unit](auto & u) {return u.type == unit; });
	}
	void GameState::addUnit(UnitId unit)
	{
		units.emplace_back(UnitInstance(unit));
	}
}