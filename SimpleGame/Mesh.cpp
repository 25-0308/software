#include "stdafx.h"
#include "Mesh.h"

#include <cmath>

MeshData MeshGen::GenerateQuad()
{
	MeshData mesh;
	mesh.primitiveType = GL_TRIANGLES;
	mesh.vertices =
	{
		-0.5f, -0.5f, 0.f,  -0.5f, 0.5f, 0.f,  0.5f, 0.5f, 0.f,
		-0.5f, -0.5f, 0.f,   0.5f, 0.5f, 0.f,  0.5f, -0.5f, 0.f,
	};
	return mesh;
}

MeshData MeshGen::GenerateEllipse(float radiusX, float radiusY, int segments)
{
	MeshData mesh;
	mesh.primitiveType = GL_TRIANGLE_FAN;

	// 팬의 첫 정점은 중심.
	mesh.vertices.push_back(0.f);
	mesh.vertices.push_back(0.f);
	mesh.vertices.push_back(0.f);

	for (int i = 0; i <= segments; ++i)
	{
		float t = (float)i / (float)segments * 6.2831853f;
		mesh.vertices.push_back(cosf(t) * radiusX);
		mesh.vertices.push_back(sinf(t) * radiusY);
		mesh.vertices.push_back(0.f);
	}

	return mesh;
}

MeshData MeshGen::GenerateCircle(int segments)
{
	return GenerateEllipse(0.5f, 0.5f, segments);
}
