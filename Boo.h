#pragma once

#include "BuildOrder.h"

namespace suboo {

	// a BO Optimizer interface
	class boo {
	public :
		// returns delta seconds gained, 0 means unable to optimize
		// if delta > 0, an improved BuildOrder is returned 
		virtual std::pair<int,BuildOrder> improve(const BuildOrder & base)=0;
		virtual const char* getName() const = 0;
		virtual ~boo() {}
	};
	using pboo = std::unique_ptr<boo> ;

	// a basic "improver" terminal/leaf
	// when meeting adjacent builditems bi0, bi1, suppose bi0 was responsible 
	// for delaying bi1. Shift bi1 left (permute the two events if not prereq contradiction) and measure improvement.
	// macro blocks are built for adjacent equal actions.
	class LeftShifter : public boo {
		std::pair<int, BuildOrder> improve(const BuildOrder & base) ;
		const char* getName() const { return "LeftShifter";  }
	};


	// if a build is gas starved, and the number of bases permits it add a assimilator
	class AddVespeneGatherer : public boo {
		std::pair<int, BuildOrder> improve(const BuildOrder & base);
		const char* getName() const { return "AddVespeneGatherer"; }
	};

	// if a build is mineral starved, and the number of bases permits it add a worker
	class AddMineralGatherer : public boo {
		std::pair<int, BuildOrder> improve(const BuildOrder & base);
		const char* getName() const { return "AddMineralGatherer"; }
	};

	// if a build is using a builder (building type) many times, and the number of bases permits it add a worker
	class AddProduction : public boo {
		std::pair<int, BuildOrder> improve(const BuildOrder & base);
		const char* getName() const { return "AddProduction"; }
	};


}