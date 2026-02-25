#pragma once

#include <cmath>
#include <stdexcept>

namespace math
{
	class Vec2f
	{
	public:
		float x = 0.0f;
		float y = 0.0f;

		constexpr Vec2f() = default;
		constexpr Vec2f(float xValue, float yValue) : x(xValue), y(yValue) {}

		constexpr Vec2f Clone() const
		{
			return Vec2f(x, y);
		}

		Vec2f Add(const Vec2f &other) const
		{
			return Vec2f(x + other.x, y + other.y);
		}

		Vec2f operator+(const Vec2f &other) const
		{
			return Vec2f(x + other.x, y + other.y);
		}

		Vec2f &operator+=(const Vec2f &other)
		{
			x += other.x;
			y += other.y;
			return *this;
		}

		Vec2f operator-(const Vec2f &other) const
		{
			return Vec2f(x - other.x, y - other.y);
		}

		Vec2f &operator-=(const Vec2f &other)
		{
			x -= other.x;
			y -= other.y;
			return *this;
		}

		Vec2f operator*(float factor) const
		{
			return Vec2f(x * factor, y * factor);
		}

		Vec2f &operator*=(const Vec2f &other)
		{
			x *= other.x;
			y *= other.y;
			return *this;
		}

		Vec2f operator/(float factor) const
		{
			if (factor == 0.0f)
			{
				throw std::runtime_error("Cannot divide by zero");
			}
			return Vec2f(x / factor, y / factor);
		}

		Vec2f &operator/=(const Vec2f &other)
		{
			x /= other.x;
			y /= other.y;
			return *this;
		}


		Vec2f Subtract(const Vec2f &other) const
		{
			return Vec2f(x - other.x, y - other.y);
		}

		constexpr Vec2f Multiply(float factor) const
		{
			return Vec2f(x * factor, y * factor);
		}

		Vec2f Divide(float factor) const
		{
			if (factor == 0.0f)
			{
				throw std::runtime_error("Cannot divide by zero");
			}
			return Vec2f(x / factor, y / factor);
		}

		constexpr Vec2f Negate() const
		{
			return Vec2f(-x, -y);
		}

		float Length() const
		{
			return std::sqrt(x * x + y * y);
		}

		float GetRadian() const
		{
			return std::atan2(y,x);
		}

		constexpr float LengthSquared() const
		{
			return x * x + y * y;
		}

		Vec2f Normalize() const
		{
			float len = Length();
			if (len == 0.0f)
			{
				return Vec2f(0.0f, 0.0f);
			}
			return Vec2f(x / len, y / len);
		}

		Vec2f ClampLength(float max){
			if(Length() > max)
				return Normalize() * max;
			else
				return Vec2f(x,y);
		}

		constexpr bool Equals(const Vec2f &other) const
		{
			return x == other.x && y == other.y;
		}

		static constexpr float Dot(const Vec2f &a, const Vec2f &b)
		{
			return a.x * b.x + a.y * b.y;
		}

		static constexpr float Cross(const Vec2f &a, const Vec2f &b)
		{
			return a.x * b.y - a.y * b.x;
		}

		static float Distance(const Vec2f &a, const Vec2f &b)
		{
			float dx = a.x - b.x;
			float dy = a.y - b.y;
			return std::sqrt(dx * dx + dy * dy);
		}

		static constexpr float DistanceSquared(const Vec2f &a, const Vec2f &b)
		{
			float dx = a.x - b.x;
			float dy = a.y - b.y;
			return dx * dx + dy * dy;
		}
	};

} // namespace math
