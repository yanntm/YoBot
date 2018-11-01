/*
File originally under MIT License
Copyright (c) 2016 Hendrik Weideman
https://github.com/hjweide/a-star/

*/

#include <queue>
#include <limits>
#include <cmath>
#include "AStar.h"
#include "YoAgent.h"
#include "UnitTypes.h"

using namespace sc2util;
using namespace std;

float * computeWeightMap(const sc2::GameInfo & info, const sc2::UnitTypes & types, const YoAgent::UnitsMap & allEnemies) {
	const int wsize = info.width * info.height;
	float * weights = new float[wsize];
	for (int x = 0; x < info.width; x++) {
		for (int y = 0; y < info.height; y++) {
			auto w = sc2util::Pathable(info, sc2::Point2DI(x, y)) ? 1.0f : std::numeric_limits<float>::max();
			weights[x + y * info.width] = w;
		}
	}
	
	for (auto & ent : allEnemies) {
		auto & u = ent.second;
		// make enemy units unpathable
		if (! IsBuilding(u->unit_type) && ! u->is_flying) {
			weights[ (int)u->pos.x + (int)u->pos.y * info.width] *= 10;
		}
		// make enemy influenced zone less pathable
		auto r = getRange(u, types);
		if (r < 2.0f) {
			r = 2.0f;
		}
		auto r2 = r * r;
		for (int x = (int)-r; x <= (int)r; x++) {
			for (int y = (int)-r; y <= (int)r; y++) {
				if (x*x + y * y <= r2) {
					auto xp = (int)u->pos.x + x;
					auto yp = (int)u->pos.y + y);
					if (xp >= 0 && xp < info.width && yp >= 0 && yp < info.height) {
						auto & oldw = weights[xp + (yp* info.width)];
						if (oldw < std::numeric_limits<float>::max()) {
							oldw += 3.0f;
						}
					}
				}
			}
		}		
	}
	return weights;
}

std::vector<sc2::Point2DI> AstarSearchPath(sc2::Point2DI start, sc2::Point2DI end, const sc2::GameInfo & info, float * weights)
{	
	const int wsize = info.width * info.height;
	//		bool astar(const float* weights, const int h, const int w,	const int start, const int goal, bool diag_ok,
	//	       int* paths)
	int * paths = new int[wsize];
	memset(paths, 0, wsize * sizeof(int));
	std::vector<sc2::Point2DI> path;
	auto startidx = start.x + start.y * info.width;
	auto endidx = end.x + end.y * info.width;
	if (astar(weights, info.height, info.width, startidx, endidx, true, paths)) {
		auto path_idx = endidx;
		while (path_idx != startidx) {
			path.emplace_back(sc2::Point2DI(path_idx % info.width, path_idx / info.width));
			path_idx = paths[path_idx];
		}		
		std::reverse(path.begin(), path.end());
	} 

	delete[] paths;	

	return path;
}

std::vector<sc2::Point2DI> AstarSearchPath(sc2::Point2D start, sc2::Point2D end, const sc2::GameInfo & info, float * weights) {
	return AstarSearchPath(sc2::Point2DI((int)start.x, (int)start.y), sc2::Point2DI((int)end.x, (int)end.y), info, weights);
}


// represents a single pixel
class Node {
public:
	int idx;     // index in the flattened grid
	float cost;  // cost of traversing this pixel

	Node(int i, float c) : idx(i), cost(c) {}
};

// the top of the priority queue is the greatest element by default,
// but we want the smallest, so flip the sign
bool operator<(const Node &n1, const Node &n2) {
	return n1.cost > n2.cost;
}

bool operator==(const Node &n1, const Node &n2) {
	return n1.idx == n2.idx;
}

// See for various grid heuristics:
// http://theory.stanford.edu/~amitp/GameProgramming/Heuristics.html#S7
// L_\inf norm (diagonal distance)
float linf_norm(int i0, int j0, int i1, int j1) {
	return std::max(std::abs(i0 - i1), std::abs(j0 - j1));
}

// L_1 norm (manhattan distance)
float l1_norm(int i0, int j0, int i1, int j1) {
	return std::abs(i0 - i1) + std::abs(j0 - j1);
}

// weights:        flattened h x w grid of costs
// h, w:           height and width of grid
// start, goal:    index of start/goal in flattened grid
// diag_ok:        if true, allows diagonal moves (8-conn.)
// paths (output): for each node, stores previous node in path
bool astar(
	const float* weights, const int h, const int w,
	const int start, const int goal, bool diag_ok,
	int* paths) {

	const float INF = std::numeric_limits<float>::infinity();

	Node start_node(start, 0.);
	Node goal_node(goal, 0.);

	float* costs = new float[h * w];
	for (int i = 0; i < h * w; ++i)
		costs[i] = INF;
	costs[start] = 0.;

	std::priority_queue<Node> nodes_to_visit;
	nodes_to_visit.push(start_node);

	int* nbrs = new int[8];

	bool solution_found = false;
	while (!nodes_to_visit.empty()) {
		// .top() doesn't actually remove the node
		Node cur = nodes_to_visit.top();

		if (cur == goal_node) {
			solution_found = true;
			break;
		}

		nodes_to_visit.pop();

		int row = cur.idx / w;
		int col = cur.idx % w;
		// check bounds and find up to eight neighbors: top to bottom, left to right
		nbrs[0] = (diag_ok && row > 0 && col > 0) ? cur.idx - w - 1 : -1;
		nbrs[1] = (row > 0) ? cur.idx - w : -1;
		nbrs[2] = (diag_ok && row > 0 && col + 1 < w) ? cur.idx - w + 1 : -1;
		nbrs[3] = (col > 0) ? cur.idx - 1 : -1;
		nbrs[4] = (col + 1 < w) ? cur.idx + 1 : -1;
		nbrs[5] = (diag_ok && row + 1 < h && col > 0) ? cur.idx + w - 1 : -1;
		nbrs[6] = (row + 1 < h) ? cur.idx + w : -1;
		nbrs[7] = (diag_ok && row + 1 < h && col + 1 < w) ? cur.idx + w + 1 : -1;

		float heuristic_cost;
		for (int i = 0; i < 8; ++i) {
			if (nbrs[i] >= 0) {
				// the sum of the cost so far and the cost of this move
				auto wei = weights[nbrs[i]];
				// diagonal moves cost more
				if (i == 0 || i == 2 || i == 5 || i == 7) {
					wei *= sqrt(2);
				}
				float new_cost = costs[cur.idx] + wei;
				if (new_cost < costs[nbrs[i]]) {
					// estimate the cost to the goal based on legal moves
					if (diag_ok) {
						heuristic_cost = linf_norm(nbrs[i] / w, nbrs[i] % w,
							goal / w, goal    % w);
					}
					else {
						heuristic_cost = l1_norm(nbrs[i] / w, nbrs[i] % w,
							goal / w, goal    % w);
					}

					// paths with lower expected cost are explored first
					float priority = new_cost + heuristic_cost;
					nodes_to_visit.push(Node(nbrs[i], priority));

					costs[nbrs[i]] = new_cost;
					paths[nbrs[i]] = cur.idx;
				}
			}
		}
	}

	delete[] costs;
	delete[] nbrs;

	return solution_found;
}