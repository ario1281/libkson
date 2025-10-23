﻿#include "kson/util/graph_curve.hpp"
#include "kson/note/note_info.hpp"
#include <cmath>
#include <algorithm>

namespace kson
{
	double EvaluateCurve(double a, double b, double x)
	{
		// Curve formula:
		//   f(x) = 2(1-t)tb + t^2
		//   t = (a - sqrt(a^2 + x - 2ax)) / (-1 + 2a)
		//   where 0 <= a, b, x <= 1

		a = std::clamp(a, 0.0, 1.0);
		b = std::clamp(b, 0.0, 1.0);
		x = std::clamp(x, 0.0, 1.0);

		// Handle edge case: a == 0.5 makes denominator zero (fallback to linear)
		if (AlmostEquals(a, 0.5))
		{
			return x;
		}

		const double discriminant = a * a + x - 2.0 * a * x;
		if (discriminant < 0.0)
		{
			// Invalid value (fallback to linear)
			return x;
		}

		const double t = (a - std::sqrt(discriminant)) / (-1.0 + 2.0 * a);
		const double result = 2.0 * (1.0 - t) * t * b + t * t;

		return std::clamp(result, 0.0, 1.0);
	}

	double EvaluateCurve(const GraphCurveValue& curve, double x)
	{
		if (curve.isLinear())
		{
			return x;
		}
		return EvaluateCurve(curve.a, curve.b, x);
	}

	Graph ExpandCurveSegments(const Graph& graph, Pulse subdivisionInterval)
	{
		if (subdivisionInterval <= 0)
		{
			throw std::invalid_argument("subdivisionInterval must be positive");
		}

		if (graph.empty())
		{
			return graph;
		}

		Graph result;

		auto itr = graph.begin();
		const auto endItr = graph.end();

		// Add first point
		result.insert(*itr);

		while (itr != endItr)
		{
			const auto nextItr = std::next(itr);
			if (nextItr == endItr)
			{
				break;
			}

			const auto& [y1, point1] = *itr;
			const auto& [y2, point2] = *nextItr;

			// If curve is linear, no need to expand
			if (point1.curve.isLinear())
			{
				result.insert(*nextItr);
				++itr;
				continue;
			}

			const Pulse segmentLength = y2 - y1;

			for (Pulse ry = subdivisionInterval; ry < segmentLength; ry += subdivisionInterval)
			{
				const double lerpRate = static_cast<double>(ry) / static_cast<double>(segmentLength);
				const double curveValue = EvaluateCurve(point1.curve, lerpRate);

				// Interpolate between point1.vf and point2.v using the curve
				const double interpolatedValue = std::lerp(point1.v.vf, point2.v.v, curveValue);

				// Add intermediate point with linear interpolation (no curve)
				result[y1 + ry] = GraphPoint(GraphValue(interpolatedValue));
			}

			// Add next point
			result.insert(*nextItr);

			++itr;
		}

		return result;
	}

	GraphSection ExpandCurveSegments(const GraphSection& graphSection, RelPulse subdivisionInterval)
	{
		if (subdivisionInterval <= 0)
		{
			throw std::invalid_argument("subdivisionInterval must be positive");
		}

		if (graphSection.v.empty())
		{
			return graphSection;
		}

		GraphSection result;

		auto itr = graphSection.v.begin();
		const auto endItr = graphSection.v.end();

		// Add first point
		result.v.insert(*itr);

		while (itr != endItr)
		{
			const auto nextItr = std::next(itr);
			if (nextItr == endItr)
			{
				break;
			}

			const auto& [ry1, point1] = *itr;
			const auto& [ry2, point2] = *nextItr;

			// If curve is linear, no need to expand
			if (point1.curve.isLinear())
			{
				result.v.insert(*nextItr);
				++itr;
				continue;
			}

			const RelPulse segmentLength = ry2 - ry1;

			for (RelPulse ry = subdivisionInterval; ry < segmentLength; ry += subdivisionInterval)
			{
				const double lerpRate = static_cast<double>(ry) / static_cast<double>(segmentLength);
				const double curveValue = EvaluateCurve(point1.curve, lerpRate);

				// Interpolate between point1.vf and point2.v using the curve
				const double interpolatedValue = std::lerp(point1.v.vf, point2.v.v, curveValue);

				// Add intermediate point with linear interpolation (no curve)
				result.v[ry1 + ry] = GraphPoint(GraphValue(interpolatedValue));
			}

			// Add next point
			result.v.insert(*nextItr);

			++itr;
		}

		return result;
	}

	LaserSection ExpandCurveSegments(const LaserSection& laserSection, RelPulse subdivisionInterval)
	{
		if (subdivisionInterval <= 0)
		{
			throw std::invalid_argument("subdivisionInterval must be positive");
		}

		if (laserSection.v.empty())
		{
			return laserSection;
		}

		LaserSection result;
		result.w = laserSection.w; // Copy the width flag

		auto itr = laserSection.v.begin();
		const auto endItr = laserSection.v.end();

		// Add first point
		result.v.insert(*itr);

		while (itr != endItr)
		{
			const auto nextItr = std::next(itr);
			if (nextItr == endItr)
			{
				break;
			}

			const auto& [ry1, point1] = *itr;
			const auto& [ry2, point2] = *nextItr;

			// If curve is linear, no need to expand
			if (point1.curve.isLinear())
			{
				result.v.insert(*nextItr);
				++itr;
				continue;
			}

			const RelPulse segmentLength = ry2 - ry1;

			for (RelPulse ry = subdivisionInterval; ry < segmentLength; ry += subdivisionInterval)
			{
				const double lerpRate = static_cast<double>(ry) / static_cast<double>(segmentLength);
				const double curveValue = EvaluateCurve(point1.curve, lerpRate);

				// Interpolate between point1.vf and point2.v using the curve
				const double interpolatedValue = std::lerp(point1.v.vf, point2.v.v, curveValue);

				// Add intermediate point with linear interpolation (no curve)
				result.v[ry1 + ry] = GraphPoint(GraphValue(interpolatedValue));
			}

			// Add next point
			result.v.insert(*nextItr);

			++itr;
		}

		return result;
	}
}
