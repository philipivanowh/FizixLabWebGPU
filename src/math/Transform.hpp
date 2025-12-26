#pragma once

#include "Vec2.hpp"

#include <cmath>

namespace math
{

	class Transform
	{
	public:
		Transform(const Vec2 &positionValue, float angleRadians)
			: position(positionValue), sinValue(std::sin(angleRadians)), cosValue(std::cos(angleRadians)) {}

		Vec2 Apply(const Vec2 &vec) const
		{
			float x = cosValue * vec.x - sinValue * vec.y + position.x;
			float y = sinValue * vec.x + cosValue * vec.y + position.y;
			return Vec2(x, y);
		}

	private:
		Vec2 position;
		float sinValue = 0.0f;
		float cosValue = 1.0f;
	};

} // namespace math
