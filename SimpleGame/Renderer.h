#pragma once

#include "Dependencies\glew.h"
#include "Math3D.h"
#include "Mesh.h"

class Renderer
{
public:
	Renderer(int windowSizeX, int windowSizeY);
	~Renderer();

	bool IsInitialized();

	// 단색 오브젝트(캐릭터 파츠, 건물 파츠, 소품 등)를 그림
	void DrawObject(const Mat4& mvp, float r, float g, float b, float a);

	// 오브젝트 발밑의 부드러운 원형 그림자(블롭 섀도우)를 그림
	void DrawShadow(const Mat4& mvp, float opacity);

	// 시간에 따라 표면이 일렁이는 물 타일을 그림
	void DrawWater(const Mat4& mvp, float r, float g, float b, float a, float time, float phaseOffset);

	// 시간에 따라 일렁이는 횃불/모닥불을 그림
	void DrawFire(const Mat4& mvp, float time, float phaseOffset);

	// 절차적으로 만든 메시(원, 타원 등)를 GPU에 올려 핸들로 반환한다.
	MeshHandle CreateMesh(const MeshData& meshData);
	void DestroyMesh(MeshHandle& mesh);
	void DrawMesh(const MeshHandle& mesh, const Mat4& mvp, float r, float g, float b, float a);

	// 매번 CPU에서 만든 삼각형 목록(x,y,z 반복)을 그대로 올려서 단색으로 한 번에 그린다.
	// 메시 핸들을 만들고 지울 필요가 없어서, 프레임마다 바뀌는 점/조각을 색깔별로 묶어
	// 드로우 콜 한 번으로 그릴 때 쓴다(예: 미니맵 표시).
	void DrawTriangles(const float* vertices, int vertexCount, const Mat4& mvp, float r, float g, float b, float a);

	// 단위 사각형에 텍스처를 입혀 그린다. 텍스처의 알파 채널이 커버리지(글자가 칠해진 정도)이고,
	// 색은 (r,g,b), 전체 불투명도는 a로 정한다 — 글자 텍스처(TextRasterizer) 전용.
	void DrawTexture(GLuint texture, const Mat4& mvp, float r, float g, float b, float a);

private:
	void Initialize(int windowSizeX, int windowSizeY);
	void CreateVertexBufferObjects();
	void BindQuadPositionAttribute(GLuint shader);

	bool m_Initialized = false;

	GLuint m_VBORect = 0;
	GLuint m_VBODynamic = 0; // DrawTriangles가 매번 내용을 덮어쓰는 임시 정점 버퍼
	GLuint m_SolidRectShader = 0;
	GLuint m_ShadowShader = 0;
	GLuint m_WaterShader = 0;
	GLuint m_FireShader = 0;
	GLuint m_TextShader = 0;
};
