#include "ImageUtil.h"

namespace sc2util {

	bool isSet(const sc2::GameInfo & info, const sc2::ImageData & grid, const sc2::Point2D & point)
	{
		sc2::Point2DI pointI((int)point.x, (int)point.y);
		return isSet(info, grid, pointI);
	}

	bool isSet(const sc2::GameInfo & info, const sc2::ImageData & grid, const sc2::Point2DI & pointI)
	{
		if (pointI.x < 0 || pointI.x >= info.width || pointI.y < 0 || pointI.y >= info.width)
		{
			return false;
		}
		unsigned char encodedPlacement = grid.data[pointI.x + ((info.height - 1) - pointI.y) * info.width];
		bool decodedPlacement = encodedPlacement == 255 ? false : true;
		return decodedPlacement;
	}

	bool set(sc2::GameInfo & info, sc2::ImageData & grid, const sc2::Point2D & point, bool b)
	{
		sc2::Point2DI pointI((int)point.x, (int)point.y);
		return set(info, grid, pointI, b);
	}

	bool set(sc2::GameInfo & info, sc2::ImageData & grid, const sc2::Point2DI & pointI, bool b)
	{
		if (pointI.x < 0 || pointI.x >= info.width || pointI.y < 0 || pointI.y >= info.width)
		{
			return false;
		}
		unsigned char encodedPlacement = b ? 0 : 255;
		auto index = pointI.x + ((info.height - 1) - pointI.y) * info.width;
		grid.data[index] = encodedPlacement;
		return true;
	}

	bool setBuildingAt(sc2::GameInfo & info, sc2::ImageData & grid, const sc2::Point2D & pos, int footprint, bool b)
	{
		using namespace sc2;
		float fx = pos.x - (float)footprint / 2.0f;
		float fy = pos.y - (float)footprint / 2.0f;

		Point2D point(fx, fy);
		auto inzone = sc2::Point2DI((int)point.x, (int)point.y);
		for (int y = 0; y < footprint; y++) {
			for (int x = 0; x < footprint; x++) {
				set(info, grid, Point2DI(inzone.x + x, inzone.y + y), !b);
			}
		}
		return true;
	}
}