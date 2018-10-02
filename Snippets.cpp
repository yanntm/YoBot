
/**File hosting various functions that were deemed useful at some point then discarded */

// Deal with flying idea : supposed to go for cannons vs flying
// never did work

/*
// 0 = nominal, 1=going to, 2=pylon ok, 3=cannon ok, 4=razed
int flystate = 0;
const Unit * flying = nullptr;

// call logic :onStep
if (flying != nullptr && minerals >= 1000) {
	dealWithFlying();
	if (flystate < 1)
		minerals -= 550;
	else if (flystate < 2)
		minerals -= 450;
	else if (flystate < 3)
		minerals -= 300;
}

// actual automaton
void dealWithFlying() {
if (flying != nullptr && bob != nullptr && (minerals >= 600 || flystate ==2)) {
float pylon = 0;
float cannon = 0;
float forge = 0;
const auto & fpos = flying->pos;
Observation()->GetUnits(Unit::Alliance::Self, [&pylon,&cannon,&forge,fpos](const Unit &u) {
if (u.unit_type != UNIT_TYPEID::PROTOSS_PYLON  && u.unit_type != UNIT_TYPEID::PROTOSS_PHOTONCANNON && u.unit_type != UNIT_TYPEID::PROTOSS_FORGE) {
return false;
}
if (Distance2D(fpos, u.pos) > 15.0f) { return false; }
if (u.unit_type == UNIT_TYPEID::PROTOSS_PYLON)  pylon = u.build_progress;
if (u.unit_type == UNIT_TYPEID::PROTOSS_PHOTONCANNON)  cannon = u.build_progress;
if (u.unit_type == UNIT_TYPEID::PROTOSS_FORGE)  forge = u.build_progress;
return true;
}
);

if (flystate == 0 && pylon == 0) {
Actions()->UnitCommand(bob, ABILITY_ID::SMART, flying->pos);
Actions()->UnitCommand(bob, ABILITY_ID::BUILD_PYLON, flying->pos + Point2D(-2, -2));
//Actions()->UnitCommand(bob, ABILITY_ID::PATROL, flying->pos,true);
}
else if (flystate == 0 && pylon > 0) {
flystate = 1;
}
else if (flystate == 1 && pylon == 1.0f && forge == 0) {
Actions()->UnitCommand(bob, ABILITY_ID::BUILD_FORGE, flying->pos + Point2D(-4, 0));
Actions()->UnitCommand(bob, ABILITY_ID::PATROL, flying->pos, true);
}
else if (flystate == 1 && forge > 0) {
flystate = 2;
}
else if (flystate == 2 && forge==1.0f && cannon == 0) {
Actions()->UnitCommand(bob, ABILITY_ID::BUILD_PHOTONCANNON, flying->pos + Point2D(2, 0));
Actions()->UnitCommand(bob, ABILITY_ID::BUILD_PHOTONCANNON, flying->pos + Point2D(-2,0),true);
Actions()->UnitCommand(bob, ABILITY_ID::BUILD_PHOTONCANNON, flying->pos + Point2D(0, -2), true);
Actions()->UnitCommand(bob, ABILITY_ID::BUILD_PHOTONCANNON, flying->pos + Point2D(0, 2), true);
Actions()->UnitCommand(bob, ABILITY_ID::PATROL, flying->pos, true);
}
else if (flystate == 2 && cannon >= 4.0f) {
flystate = 3;
}
}
}

// regularly done in estimateEnemy strength ?
// still WIP
if (false &&  flying == nullptr && u.second->unit_type == UNIT_TYPEID::TERRAN_COMMANDCENTERFLYING) {
flying = u.second;
dealWithFlying();
int freemin = minerals;
for (const Unit * gw : Observation()->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::PROTOSS_GATEWAY))) {
if (freemin < 400 && ! gw->orders.empty()) {
freemin += 100;
Actions()->UnitCommand(gw, ABILITY_ID::CANCEL_LAST);
}
}
}

*/