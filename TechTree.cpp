#include "BuildOrder.h"
namespace suboo {
TechTree::TechTree() :
	initial({ UnitInstance( (UnitId)84),
 UnitInstance( (UnitId)84),
 UnitInstance( (UnitId)84),
 UnitInstance( (UnitId)84),
 UnitInstance( (UnitId)84),
 UnitInstance( (UnitId)84),
 UnitInstance( (UnitId)84),
 UnitInstance( (UnitId)84),
 UnitInstance( (UnitId)84),
 UnitInstance( (UnitId)84),
 UnitInstance( (UnitId)84),
 UnitInstance( (UnitId)84),
 UnitInstance( (UnitId)59),
	}) {
  units = {
{	(UnitId)4,  // ID
	"Colossus", // name
	300, // gold
	200, // gas
	-6,  // food
	(UnitId)71,  // builder unit  
	(UnitId)70, // tech requirement 
	1200, // build time
	0,   // travel time
	Unit::BUSY  // producer behavior
},
{	(UnitId)10,  // ID
	"Mothership", // name
	400, // gold
	400, // gas
	-8,  // food
	(UnitId)59,  // builder unit  
	(UnitId)0, // tech requirement 
	2560, // build time
	0,   // travel time
	Unit::BUSY  // producer behavior
},
{	(UnitId)59,  // ID
	"Nexus", // name
	400, // gold
	0, // gas
	15,  // food
	(UnitId)84,  // builder unit  
	(UnitId)0,  // tech requirement  
	1600, // build time
	10,   // travel time
	Unit::TRAVEL  // probe behavior
},
{	(UnitId)60,  // ID
	"Pylon", // name
	100, // gold
	0, // gas
	8,  // food
	(UnitId)84,  // builder unit  
	(UnitId)0,  // tech requirement  
	400, // build time
	4,   // travel time
	Unit::TRAVEL  // probe behavior
},
{	(UnitId)61,  // ID
	"Assimilator", // name
	75, // gold
	0, // gas
	0,  // food
	(UnitId)84,  // builder unit  
	(UnitId)0,  // tech requirement  
	480, // build time
	4,   // travel time
	Unit::TRAVEL  // probe behavior
},
{	(UnitId)62,  // ID
	"Gateway", // name
	150, // gold
	0, // gas
	0,  // food
	(UnitId)84,  // builder unit  
	(UnitId)59,  // tech requirement  
	1040, // build time
	4,   // travel time
	Unit::TRAVEL  // probe behavior
},
{	(UnitId)63,  // ID
	"Forge", // name
	150, // gold
	0, // gas
	0,  // food
	(UnitId)84,  // builder unit  
	(UnitId)59,  // tech requirement  
	720, // build time
	4,   // travel time
	Unit::TRAVEL  // probe behavior
},
{	(UnitId)64,  // ID
	"FleetBeacon", // name
	300, // gold
	200, // gas
	0,  // food
	(UnitId)84,  // builder unit  
	(UnitId)67,  // tech requirement  
	960, // build time
	4,   // travel time
	Unit::TRAVEL  // probe behavior
},
{	(UnitId)65,  // ID
	"TwilightCouncil", // name
	150, // gold
	100, // gas
	0,  // food
	(UnitId)84,  // builder unit  
	(UnitId)72,  // tech requirement  
	800, // build time
	4,   // travel time
	Unit::TRAVEL  // probe behavior
},
{	(UnitId)66,  // ID
	"PhotonCannon", // name
	150, // gold
	0, // gas
	0,  // food
	(UnitId)84,  // builder unit  
	(UnitId)63,  // tech requirement  
	640, // build time
	4,   // travel time
	Unit::TRAVEL  // probe behavior
},
{	(UnitId)67,  // ID
	"Stargate", // name
	150, // gold
	150, // gas
	0,  // food
	(UnitId)84,  // builder unit  
	(UnitId)72,  // tech requirement  
	960, // build time
	4,   // travel time
	Unit::TRAVEL  // probe behavior
},
{	(UnitId)68,  // ID
	"TemplarArchive", // name
	150, // gold
	200, // gas
	0,  // food
	(UnitId)84,  // builder unit  
	(UnitId)65,  // tech requirement  
	800, // build time
	4,   // travel time
	Unit::TRAVEL  // probe behavior
},
{	(UnitId)69,  // ID
	"DarkShrine", // name
	150, // gold
	150, // gas
	0,  // food
	(UnitId)84,  // builder unit  
	(UnitId)65,  // tech requirement  
	1600, // build time
	4,   // travel time
	Unit::TRAVEL  // probe behavior
},
{	(UnitId)70,  // ID
	"RoboticsBay", // name
	150, // gold
	150, // gas
	0,  // food
	(UnitId)84,  // builder unit  
	(UnitId)71,  // tech requirement  
	1040, // build time
	4,   // travel time
	Unit::TRAVEL  // probe behavior
},
{	(UnitId)71,  // ID
	"RoboticsFacility", // name
	200, // gold
	100, // gas
	0,  // food
	(UnitId)84,  // builder unit  
	(UnitId)72,  // tech requirement  
	1040, // build time
	4,   // travel time
	Unit::TRAVEL  // probe behavior
},
{	(UnitId)72,  // ID
	"CyberneticsCore", // name
	150, // gold
	0, // gas
	0,  // food
	(UnitId)84,  // builder unit  
	(UnitId)62,  // tech requirement  
	800, // build time
	4,   // travel time
	Unit::TRAVEL  // probe behavior
},
{	(UnitId)73,  // ID
	"Zealot", // name
	100, // gold
	0, // gas
	-2,  // food
	(UnitId)62,  // builder unit  
	(UnitId)0, // tech requirement 
	608, // build time
	0,   // travel time
	Unit::BUSY  // producer behavior
},
{	(UnitId)74,  // ID
	"Stalker", // name
	125, // gold
	50, // gas
	-2,  // food
	(UnitId)62,  // builder unit  
	(UnitId)72, // tech requirement 
	672, // build time
	0,   // travel time
	Unit::BUSY  // producer behavior
},
{	(UnitId)75,  // ID
	"HighTemplar", // name
	50, // gold
	150, // gas
	-2,  // food
	(UnitId)62,  // builder unit  
	(UnitId)68, // tech requirement 
	880, // build time
	0,   // travel time
	Unit::BUSY  // producer behavior
},
{	(UnitId)76,  // ID
	"DarkTemplar", // name
	125, // gold
	125, // gas
	-2,  // food
	(UnitId)62,  // builder unit  
	(UnitId)69, // tech requirement 
	880, // build time
	0,   // travel time
	Unit::BUSY  // producer behavior
},
{	(UnitId)77,  // ID
	"Sentry", // name
	50, // gold
	100, // gas
	-2,  // food
	(UnitId)62,  // builder unit  
	(UnitId)72, // tech requirement 
	592, // build time
	0,   // travel time
	Unit::BUSY  // producer behavior
},
{	(UnitId)78,  // ID
	"Phoenix", // name
	150, // gold
	100, // gas
	-2,  // food
	(UnitId)67,  // builder unit  
	(UnitId)0, // tech requirement 
	560, // build time
	0,   // travel time
	Unit::BUSY  // producer behavior
},
{	(UnitId)79,  // ID
	"Carrier", // name
	350, // gold
	250, // gas
	-6,  // food
	(UnitId)67,  // builder unit  
	(UnitId)64, // tech requirement 
	1440, // build time
	0,   // travel time
	Unit::BUSY  // producer behavior
},
{	(UnitId)80,  // ID
	"VoidRay", // name
	250, // gold
	150, // gas
	-4,  // food
	(UnitId)67,  // builder unit  
	(UnitId)0, // tech requirement 
	960, // build time
	0,   // travel time
	Unit::BUSY  // producer behavior
},
{	(UnitId)81,  // ID
	"WarpPrism", // name
	200, // gold
	0, // gas
	-2,  // food
	(UnitId)71,  // builder unit  
	(UnitId)0, // tech requirement 
	800, // build time
	0,   // travel time
	Unit::BUSY  // producer behavior
},
{	(UnitId)82,  // ID
	"Observer", // name
	25, // gold
	75, // gas
	-1,  // food
	(UnitId)71,  // builder unit  
	(UnitId)0, // tech requirement 
	480, // build time
	0,   // travel time
	Unit::BUSY  // producer behavior
},
{	(UnitId)83,  // ID
	"Immortal", // name
	250, // gold
	100, // gas
	-4,  // food
	(UnitId)71,  // builder unit  
	(UnitId)0, // tech requirement 
	880, // build time
	0,   // travel time
	Unit::BUSY  // producer behavior
},
{	(UnitId)84,  // ID
	"Probe", // name
	50, // gold
	0, // gas
	-1,  // food
	(UnitId)59,  // builder unit  
	(UnitId)0, // tech requirement 
	272, // build time
	0,   // travel time
	Unit::BUSY  // producer behavior
},
{	(UnitId)133,  // ID
	"WarpGate", // name
	150, // gold
	0, // gas
	0,  // food
	(UnitId)62,  // builder unit  
	(UnitId)0,  // tech requirement  
	160, // build time
	4,   // travel time
	Unit::TRAVEL  // probe behavior
},
{	(UnitId)136,  // ID
	"WarpPrismPhasing", // name
	200, // gold
	0, // gas
	-2,  // food
	(UnitId)81,  // builder unit  
	(UnitId)0, // tech requirement 
	24, // build time
	0,   // travel time
	Unit::BUSY  // producer behavior
},
{	(UnitId)311,  // ID
	"Adept", // name
	100, // gold
	25, // gas
	-2,  // food
	(UnitId)62,  // builder unit  
	(UnitId)72, // tech requirement 
	608, // build time
	0,   // travel time
	Unit::BUSY  // producer behavior
},
{	(UnitId)495,  // ID
	"Oracle", // name
	150, // gold
	150, // gas
	-3,  // food
	(UnitId)67,  // builder unit  
	(UnitId)0, // tech requirement 
	832, // build time
	0,   // travel time
	Unit::BUSY  // producer behavior
},
{	(UnitId)496,  // ID
	"Tempest", // name
	250, // gold
	175, // gas
	-5,  // food
	(UnitId)67,  // builder unit  
	(UnitId)64, // tech requirement 
	960, // build time
	0,   // travel time
	Unit::BUSY  // producer behavior
},
{	(UnitId)694,  // ID
	"Disruptor", // name
	150, // gold
	150, // gas
	-3,  // food
	(UnitId)71,  // builder unit  
	(UnitId)70, // tech requirement 
	800, // build time
	0,   // travel time
	Unit::BUSY  // producer behavior
},
{	(UnitId)732,  // ID
	"OracleStasisTrap", // name
	0, // gold
	0, // gas
	0,  // food
	(UnitId)495,  // builder unit  
	(UnitId)0, // tech requirement 
	80, // build time
	0,   // travel time
	Unit::BUSY  // producer behavior
},
{	(UnitId)1910,  // ID
	"ShieldBattery", // name
	100, // gold
	0, // gas
	0,  // food
	(UnitId)84,  // builder unit  
	(UnitId)72,  // tech requirement  
	640, // build time
	4,   // travel time
	Unit::TRAVEL  // probe behavior
},
{	(UnitId)1911,  // ID
	"ObserverSiegeMode", // name
	25, // gold
	75, // gas
	-1,  // food
	(UnitId)82,  // builder unit  
	(UnitId)0, // tech requirement 
	12, // build time
	0,   // travel time
	Unit::BUSY  // producer behavior
},
  }; // end units
} //end ctor 
} //end ns 
