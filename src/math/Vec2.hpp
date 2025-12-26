#pragma once

#include <cmath>
#include <stdexcept>

namespace math
{

	class Vec2
	{
	public:
		float x = 0.0f;
		float y = 0.0f;

		constexpr Vec2() = default;
		constexpr Vec2(float xValue, float yValue) : x(xValue), y(yValue) {}

		constexpr Vec2 Clone() const
		{
			return Vec2(x, y);
		}

		Vec2 Add(const Vec2 &other) const
		{
			return Vec2(x + other.x, y + other.y);
		}

		Vec2 operator+(const Vec2 &other) const
		{
			return Vec2(x + other.x, y + other.y);
		}

		Vec2 &operator+=(const Vec2 &other)
		{
			x += other.x;
			y += other.y;
			return *this;
		}

		Vec2 operator-(const Vec2 &other) const
		{
			return Vec2(x - other.x, y - other.y);
		}

		Vec2 &operator-=(const Vec2 &other)
		{
			x -= other.x;
			y -= other.y;
			return *this;
		}

		Vec2 operator*(float factor) const
		{
			return Vec2(x * factor, y * factor);
		}

		Vec2 &operator*=(const Vec2 &other)
		{
			x *= other.x;
			y *= other.y;
			return *this;
		}

		Vec2 operator/(float factor) const
		{
			if (factor == 0.0f)
			{
				throw std::runtime_error("Cannot divide by zero");
			}
			return Vec2(x / factor, y / factor);
		}

		Vec2 &operator/=(const Vec2 &other)
		{
			x /= other.x;
			y /= other.y;
			return *this;
		}


		Vec2 Subtract(const Vec2 &other) const
		{
			return Vec2(x - other.x, y - other.y);
		}

		constexpr Vec2 Multiply(float factor) const
		{
			return Vec2(x * factor, y * factor);
		}

		Vec2 Divide(float factor) const
		{
			if (factor == 0.0f)
			{
				throw std::runtime_error("Cannot divide by zero");
			}
			return Vec2(x / factor, y / factor);
		}

		constexpr Vec2 Negate() const
		{
			return Vec2(-x, -y);
		}

		float Length() const
		{
			return std::sqrt(x * x + y * y);
		}

		constexpr float LengthSquared() const
		{
			return x * x + y * y;
		}

		Vec2 Normalize() const
		{
			float len = Length();
			if (len == 0.0f)
			{
				return Vec2(0.0f, 0.0f);
			}
			return Vec2(x / len, y / len);
		}

		constexpr bool Equals(const Vec2 &other) const
		{
			return x == other.x && y == other.y;
		}

		static constexpr float Dot(const Vec2 &a, const Vec2 &b)
		{
			return a.x * b.x + a.y * b.y;
		}

		static constexpr float Cross(const Vec2 &a, const Vec2 &b)
		{
			return a.x * b.y - a.y * b.x;
		}

		static float Distance(const Vec2 &a, const Vec2 &b)
		{
			float dx = a.x - b.x;
			float dy = a.y - b.y;
			return std::sqrt(dx * dx + dy * dy);
		}

		static constexpr float DistanceSquared(const Vec2 &a, const Vec2 &b)
		{
			float dx = a.x - b.x;
			float dy = a.y - b.y;
			return dx * dx + dy * dy;
		}
	};

} // namespace math
