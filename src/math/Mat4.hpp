#pragma once

#include <cmath>
#include <array>
#include <stdexcept>

namespace math
{
	class Mat4
	{
	public:
		// Data stored in column-major order (like OpenGL/WebGPU)
		// m[column][row]
		float m[4][4];

		// Default constructor - creates identity matrix
		constexpr Mat4() : m{
			{1.0f, 0.0f, 0.0f, 0.0f},
			{0.0f, 1.0f, 0.0f, 0.0f},
			{0.0f, 0.0f, 1.0f, 0.0f},
			{0.0f, 0.0f, 0.0f, 1.0f}
		} {}

		// Constructor from array (column-major order)
		Mat4(const float data[16]) : m{}
		{
			for (int col = 0; col < 4; col++)
			{
				for (int row = 0; row < 4; row++)
				{
					m[col][row] = data[col * 4 + row];
				}
			}
		}

		// Constructor from individual values (row-major for readability)
		constexpr Mat4(
			float m00, float m01, float m02, float m03,
			float m10, float m11, float m12, float m13,
			float m20, float m21, float m22, float m23,
			float m30, float m31, float m32, float m33) : m{
			{m00, m10, m20, m30},
			{m01, m11, m21, m31},
			{m02, m12, m22, m32},
			{m03, m13, m23, m33}
		} {}

		// Access element at [row][col]
		constexpr float& At(int row, int col)
		{
			return m[col][row];
		}

		constexpr const float& At(int row, int col) const
		{
			return m[col][row];
		}

		// Get raw pointer to data (for WebGPU/shader upload)
		const float* Data() const
		{
			return &m[0][0];
		}

		// Copy data to array
		void CopyTo(float* dest) const
		{
			for (int col = 0; col < 4; col++)
			{
				for (int row = 0; row < 4; row++)
				{
					dest[col * 4 + row] = m[col][row];
				}
			}
		}

		// Matrix multiplication
		Mat4 operator*(const Mat4& other) const
		{
			Mat4 result;
			for (int row = 0; row < 4; row++)
			{
				for (int col = 0; col < 4; col++)
				{
					result.m[col][row] = 
						m[0][row] * other.m[col][0] +
						m[1][row] * other.m[col][1] +
						m[2][row] * other.m[col][2] +
						m[3][row] * other.m[col][3];
				}
			}
			return result;
		}

		// Matrix-scalar multiplication
		Mat4 operator*(float scalar) const
		{
			Mat4 result;
			for (int col = 0; col < 4; col++)
			{
				for (int row = 0; row < 4; row++)
				{
					result.m[col][row] = m[col][row] * scalar;
				}
			}
			return result;
		}

		// Matrix addition
		Mat4 operator+(const Mat4& other) const
		{
			Mat4 result;
			for (int col = 0; col < 4; col++)
			{
				for (int row = 0; row < 4; row++)
				{
					result.m[col][row] = m[col][row] + other.m[col][row];
				}
			}
			return result;
		}

		// Matrix subtraction
		Mat4 operator-(const Mat4& other) const
		{
			Mat4 result;
			for (int col = 0; col < 4; col++)
			{
				for (int row = 0; row < 4; row++)
				{
					result.m[col][row] = m[col][row] - other.m[col][row];
				}
			}
			return result;
		}

		// Transpose
		Mat4 Transpose() const
		{
			Mat4 result;
			for (int col = 0; col < 4; col++)
			{
				for (int row = 0; row < 4; row++)
				{
					result.m[col][row] = m[row][col];
				}
			}
			return result;
		}

		// Static factory methods for common matrices

		// Identity matrix
		static constexpr Mat4 Identity()
		{
			return Mat4();
		}

		// Orthographic projection (for 2D rendering)
		static Mat4 Ortho(float left, float right, float bottom, float top, float near = -1.0f, float far = 1.0f)
		{
			Mat4 result;
			
			result.m[0][0] = 2.0f / (right - left);
			result.m[1][1] = 2.0f / (top - bottom);
			result.m[2][2] = -2.0f / (far - near);
			
			result.m[3][0] = -(right + left) / (right - left);
			result.m[3][1] = -(top + bottom) / (top - bottom);
			result.m[3][2] = -(far + near) / (far - near);
			
			return result;
		}

		// Perspective projection
		static Mat4 Perspective(float fovY, float aspect, float near, float far)
		{
			Mat4 result;
			const float tanHalfFovy = std::tan(fovY / 2.0f);
			
			result.m[0][0] = 1.0f / (aspect * tanHalfFovy);
			result.m[1][1] = 1.0f / tanHalfFovy;
			result.m[2][2] = -(far + near) / (far - near);
			result.m[2][3] = -1.0f;
			result.m[3][2] = -(2.0f * far * near) / (far - near);
			result.m[3][3] = 0.0f;
			
			return result;
		}

		// Translation matrix
		static Mat4 Translate(float x, float y, float z)
		{
			Mat4 result;
			result.m[3][0] = x;
			result.m[3][1] = y;
			result.m[3][2] = z;
			return result;
		}

		// Scale matrix
		static Mat4 Scale(float x, float y, float z)
		{
			Mat4 result;
			result.m[0][0] = x;
			result.m[1][1] = y;
			result.m[2][2] = z;
			return result;
		}

		// Rotation around X axis
		static Mat4 RotateX(float angle)
		{
			Mat4 result;
			const float c = std::cos(angle);
			const float s = std::sin(angle);
			
			result.m[1][1] = c;
			result.m[2][1] = -s;
			result.m[1][2] = s;
			result.m[2][2] = c;
			
			return result;
		}

		// Rotation around Y axis
		static Mat4 RotateY(float angle)
		{
			Mat4 result;
			const float c = std::cos(angle);
			const float s = std::sin(angle);
			
			result.m[0][0] = c;
			result.m[2][0] = s;
			result.m[0][2] = -s;
			result.m[2][2] = c;
			
			return result;
		}

		// Rotation around Z axis
		static Mat4 RotateZ(float angle)
		{
			Mat4 result;
			const float c = std::cos(angle);
			const float s = std::sin(angle);
			
			result.m[0][0] = c;
			result.m[1][0] = -s;
			result.m[0][1] = s;
			result.m[1][1] = c;
			
			return result;
		}

		// Look-at matrix (for camera)
		static Mat4 LookAt(float eyeX, float eyeY, float eyeZ,
		                   float centerX, float centerY, float centerZ,
		                   float upX, float upY, float upZ)
		{
			// Forward vector (from eye to center)
			float fX = centerX - eyeX;
			float fY = centerY - eyeY;
			float fZ = centerZ - eyeZ;
			const float fLen = std::sqrt(fX * fX + fY * fY + fZ * fZ);
			fX /= fLen; fY /= fLen; fZ /= fLen;
			
			// Right vector (cross product of forward and up)
			float rX = fY * upZ - fZ * upY;
			float rY = fZ * upX - fX * upZ;
			float rZ = fX * upY - fY * upX;
			const float rLen = std::sqrt(rX * rX + rY * rY + rZ * rZ);
			rX /= rLen; rY /= rLen; rZ /= rLen;
			
			// Recalculate up vector
			const float uX = rY * fZ - rZ * fY;
			const float uY = rZ * fX - rX * fZ;
			const float uZ = rX * fY - rY * fX;
			
			Mat4 result;
			result.m[0][0] = rX;
			result.m[1][0] = rY;
			result.m[2][0] = rZ;
			result.m[0][1] = uX;
			result.m[1][1] = uY;
			result.m[2][1] = uZ;
			result.m[0][2] = -fX;
			result.m[1][2] = -fY;
			result.m[2][2] = -fZ;
			result.m[3][0] = -(rX * eyeX + rY * eyeY + rZ * eyeZ);
			result.m[3][1] = -(uX * eyeX + uY * eyeY + uZ * eyeZ);
			result.m[3][2] = fX * eyeX + fY * eyeY + fZ * eyeZ;
			
			return result;
		}
	};

} // namespace math