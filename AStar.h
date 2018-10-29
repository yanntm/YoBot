#pragma once

#include <vector>
#include "MapTopology.h"
#include "Heap.h"

namespace sc2 {
	bool operator< (const sc2::Point2DI &a, const sc2::Point2DI &b) {
		return a.x + a.y < b.x + b.y;
	}
}

namespace BWTA {
	

	std::vector<sc2::Point2DI> AstarSearchPath(sc2::Point2DI start, sc2::Point2DI end, const sc2::GameInfo & info)
	{
		using std::min;
		using std::max;

		Heap<sc2::Point2DI, int> openTiles(true);
		std::map<sc2::Point2DI, int> gmap;
		std::map<sc2::Point2DI, sc2::Point2DI> parent;
		std::set<sc2::Point2DI> closedTiles;
		openTiles.push(std::make_pair(start, 0));
		gmap[start] = 0;
		parent[start] = start;
		while (!openTiles.empty())
		{
			sc2::Point2DI p = openTiles.top().first;
			if (p == end)
			{
				std::vector<sc2::Point2DI> reverse_path;
				while (p != parent[p])
				{
					reverse_path.push_back(p);
					p = parent[p];
				}
				reverse_path.push_back(start);
				std::vector<sc2::Point2DI> path;
				for (int i = reverse_path.size() - 1; i >= 0; i--)
					path.push_back(reverse_path[i]);
				return path;
			}
			int fvalue = openTiles.top().second;
			int gvalue = gmap[p];
			openTiles.pop();
			closedTiles.insert(p);
			int minx = max(p.x - 1, 0);
			int maxx = min(p.x + 1, info.width - 1);
			int miny = max(p.y - 1, 0);
			int maxy = min(p.y + 1, info.height - 1);
			for (int x = minx; x <= maxx; x++)
				for (int y = miny; y <= maxy; y++)
				{
					if (!sc2util::Pathable(info, sc2::Point2DI( x,y ))) 
						continue;
					if (p.x != x && p.y != y 
						&& !sc2util::Pathable(info, sc2::Point2DI(p.x,y))
						&& !sc2util::Pathable(info, sc2::Point2DI(x,p.y)))
						continue;
					sc2::Point2DI t(x, y);
					if (closedTiles.find(t) != closedTiles.end()) 
						continue;

					int g = gvalue + 10;
					if (x != p.x && y != p.y) g += 4;
					int dx = abs(x - end.x);
					int dy = abs(y - end.y);
					int h = abs(dx - dy) * 10 + min(dx, dy) * 14;
					int f = g + h;
					if (gmap.find(t) == gmap.end() || g < gmap.find(t)->second)	{
						gmap[t] = g;
						openTiles.set(t, f);
						parent[t] = p;
					}
				}
		}
		std::vector<sc2::Point2DI> nopath;
		return nopath;
	}
	std::vector<sc2::Point2DI> AstarSearchPath(sc2::Point2D start, sc2::Point2D end, const sc2::GameInfo & info) {
		return AstarSearchPath(sc2::Point2DI((int)start.x, (int)start.y), sc2::Point2DI((int)end.x, (int)end.y), info);
	}
}
/*
With code adapted from bwapi-bwta-map-analyzer
Standalone BWTA based Brood War map
Thank you.

The MIT License(MIT)

Copyright(c) 2014 krasi0

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files(the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions :

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/