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
	for (auto u : current) {
		auto it = workerStates.find(u->tag);
		if (it == workerStates.end()) {
			if (IsCarryingMinerals(*u)) {
				workerStates.insert({ u->tag, WorkerState()});
			}
		}
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

HarvesterStrategy::HarvesterStrategy(const sc2::Unit * nexus, const sc2::Units & minerals)
	:nexus(nexus),minerals(minerals)
{
	for (auto targetMineral : minerals) {
		const sc2::Point2D magicSpot = calcMagicSpot(targetMineral);
		magicSpots.insert_or_assign(targetMineral->tag, magicSpot);
	}
}
