#pragma once

#include <vector>
#include "Dependencies\glew.h"

// 렌더링용 정점 데이터 (위치만, x,y,z 반복) + 그리기 방식.
struct MeshData
{
	std::vector<float> vertices;
	GLenum primitiveType = GL_TRIANGLES;
};

// GPU에 업로드된 메시에 대한 핸들.
struct MeshHandle
{
	GLuint vbo = 0;
	int vertexCount = 0;
	GLenum primitiveType = GL_TRIANGLES;
};

namespace MeshGen
{
	// -0.5~0.5 크기의 사각형 (Renderer 내장 사각형과 동일한 형태).
	MeshData GenerateQuad();

	// 반지름(radiusX, radiusY)의 타원판을 삼각형 팬으로 근사.
	MeshData GenerateEllipse(float radiusX, float radiusY, int segments);

	// 반지름 0.5의 원판 (GenerateEllipse(0.5, 0.5, segments)와 동일).
	MeshData GenerateCircle(int segments);
}
