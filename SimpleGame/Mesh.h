#pragma once

#include <vector>
#include "Dependencies\glew.h"

// 렌더링용 정점 데이터 (위치만, x,y,z 반복) + 그리기 방식.
struct MeshData
{
	std::vector<float> vertices;
	GLenum primitiveType = GL_TRIANGLES;
};

// Renderer::CreateMesh가 만든 메시. 단색 도형은 전부 Renderer의 배치 버퍼에 모아서 한 번에 그리므로
// (Renderer.h의 렌더 큐 설명 참고), 메시는 GPU 버퍼가 아니라 CPU 쪽 삼각형 목록으로 들고 있다가
// 그릴 때마다 MVP를 곱해 배치에 덧붙인다. 삼각형 팬 같은 형식은 만들 때 삼각형 목록으로 풀어 둔다.
struct MeshHandle
{
	std::vector<float> triangles; // x,y,z 반복, 3개 정점마다 삼각형 하나
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
