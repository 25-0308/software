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

private:
	void Initialize(int windowSizeX, int windowSizeY);
	void CreateVertexBufferObjects();
	void BindQuadPositionAttribute(GLuint shader);

	bool m_Initialized = false;

	GLuint m_VBORect = 0;
	GLuint m_SolidRectShader = 0;
	GLuint m_ShadowShader = 0;
	GLuint m_WaterShader = 0;
	GLuint m_FireShader = 0;
};
