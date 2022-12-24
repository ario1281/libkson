﻿#pragma once
#include "kson/common/common.hpp"

namespace kson
{
	constexpr std::int32_t kLaserXScale1x = 1;
	constexpr std::int32_t kLaserXScale2x = 2;

	struct LaserSection
	{
		ByRelPulse<GraphValue> v; // Laser points

		std::int32_t w = kLaserXScale1x; // 1-2, sets whether the laser section is 2x-widen or not

		// Returns 2x-widen or not
		bool wide() const
		{
			return w == kLaserXScale2x;
		}
	};

	struct NoteInfo
	{
		BTLane<Interval> bt;
		FXLane<Interval> fx;
		LaserLane<LaserSection> laser;
	};
}
