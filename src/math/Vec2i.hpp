#pragma once

#include <cmath>
#include <stdexcept>

namespace math
{
	class Vec2i
	{
	public:
		int x = 0;
		int y = 0;

		constexpr Vec2i() = default;
		constexpr Vec2i(int xValue, int yValue) : x(xValue), y(yValue) {}

		constexpr Vec2i Clone() const
		{
			return Vec2i(x, y);
		}

		constexpr Vec2i Add(const Vec2i &other) const
		{
			return Vec2i(x + other.x, y + other.y);
		}

		constexpr Vec2i operator+(const Vec2i &other) const
		{
			return Vec2i(x + other.x, y + other.y);
		}

		Vec2i &operator+=(const Vec2i &other)
		{
			x += other.x;
			y += other.y;
			return *this;
		}

		constexpr Vec2i operator-(const Vec2i &other) const
		{
			return Vec2i(x - other.x, y - other.y);
		}

		Vec2i &operator-=(const Vec2i &other)
		{
			x -= other.x;
			y -= other.y;
			return *this;
		}

		constexpr Vec2i operator*(int factor) const
		{
			return Vec2i(x * factor, y * factor);
		}

		Vec2i &operator*=(const Vec2i &other)
		{
			x *= other.x;
			y *= other.y;
			return *this;
		}

		constexpr Vec2i operator/(int factor) const
		{
			if (factor == 0)
			{
				throw std::runtime_error("Cannot divide by zero");
			}
			return Vec2i(x / factor, y / factor);
		}

		Vec2i &operator/=(const Vec2i &other)
		{
			x /= other.x;
			y /= other.y;
			return *this;
		}

		constexpr Vec2i Subtract(const Vec2i &other) const
		{
			return Vec2i(x - other.x, y - other.y);
		}

		constexpr Vec2i Multiply(int factor) const
		{
			return Vec2i(x * factor, y * factor);
		}

		constexpr Vec2i Divide(int factor) const
		{
			if (factor == 0)
			{
				throw std::runtime_error("Cannot divide by zero");
			}
			return Vec2i(x / factor, y / factor);
		}

		constexpr Vec2i Negate() const
		{
			return Vec2i(-x, -y);
		}

		float Length() const
		{
			return std::sqrt(static_cast<float>(x * x + y * y));
		}

		float GetRadian() const
		{
			return std::atan2(static_cast<float>(y), static_cast<float>(x));
		}

		constexpr int LengthSquared() const
		{
			return x * x + y * y;
		}

		Vec2i Normalize() const
		{
			float len = Length();
			if (len == 0.0f)
			{
				return Vec2i(0, 0);
			}
			return Vec2i(static_cast<int>(x / len), static_cast<int>(y / len));
		}

		Vec2i ClampLength(int max)
		{
			if (Length() > max)
				return Normalize() * max;
			else
				return Vec2i(x, y);
		}

		constexpr bool Equals(const Vec2i &other) const
		{
			return x == other.x && y == other.y;
		}

		static constexpr int Dot(const Vec2i &a, const Vec2i &b)
		{
			return a.x * b.x + a.y * b.y;
		}

		static constexpr int Cross(const Vec2i &a, const Vec2i &b)
		{
			return a.x * b.y - a.y * b.x;
		}

		static float Distance(const Vec2i &a, const Vec2i &b)
		{
			int dx = a.x - b.x;
			int dy = a.y - b.y;
			return std::sqrt(static_cast<float>(dx * dx + dy * dy));
		}

		static constexpr int DistanceSquared(const Vec2i &a, const Vec2i &b)
		{
			int dx = a.x - b.x;
			int dy = a.y - b.y;
			return dx * dx + dy * dy;
		}
	};

}