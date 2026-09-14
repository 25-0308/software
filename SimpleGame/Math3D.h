#pragma once

#include <cmath>

// Minimal column-major 4x4 matrix, matching OpenGL's expected memory layout
// (m[col*4+row]) so Mat4::m can be passed directly to glUniformMatrix4fv.
struct Mat4
{
	float m[16] = { 0 };

	static Mat4 Identity()
	{
		Mat4 r;
		r.m[0] = 1.f; r.m[5] = 1.f; r.m[10] = 1.f; r.m[15] = 1.f;
		return r;
	}

	static Mat4 Translate(float x, float y, float z)
	{
		Mat4 r = Identity();
		r.m[12] = x; r.m[13] = y; r.m[14] = z;
		return r;
	}

	static Mat4 Scale(float sx, float sy, float sz)
	{
		Mat4 r = Identity();
		r.m[0] = sx; r.m[5] = sy; r.m[10] = sz;
		return r;
	}

	static Mat4 RotateX(float radians)
	{
		Mat4 r = Identity();
		float c = cosf(radians), s = sinf(radians);
		r.m[5] = c; r.m[6] = s; r.m[9] = -s; r.m[10] = c;
		return r;
	}

	static Mat4 RotateY(float radians)
	{
		Mat4 r = Identity();
		float c = cosf(radians), s = sinf(radians);
		r.m[0] = c; r.m[2] = -s; r.m[8] = s; r.m[10] = c;
		return r;
	}

	static Mat4 Ortho(float left, float right, float bottom, float top, float nearZ, float farZ)
	{
		Mat4 r = Identity();
		r.m[0] = 2.f / (right - left);
		r.m[5] = 2.f / (top - bottom);
		r.m[10] = -2.f / (farZ - nearZ);
		r.m[12] = -(right + left) / (right - left);
		r.m[13] = -(top + bottom) / (top - bottom);
		r.m[14] = -(farZ + nearZ) / (farZ - nearZ);
		return r;
	}
};

// a * b : applies b first, then a (standard column-major composition).
inline Mat4 operator*(const Mat4& a, const Mat4& b)
{
	Mat4 r;
	for (int col = 0; col < 4; ++col)
	{
		for (int row = 0; row < 4; ++row)
		{
			float sum = 0.f;
			for (int k = 0; k < 4; ++k)
			{
				sum += a.m[k * 4 + row] * b.m[col * 4 + k];
			}
			r.m[col * 4 + row] = sum;
		}
	}
	return r;
}
