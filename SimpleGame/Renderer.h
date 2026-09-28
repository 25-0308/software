#pragma once

#include <vector>

#include "Dependencies\glew.h"
#include "Math3D.h"
#include "Mesh.h"

// 셰이더 하나의 프로그램 + 자주 쓰는 유니폼 위치를 묶어 둔다. 유니폼 위치는 셰이더가
// 링크된 직후 딱 한 번만 glGetUniformLocation으로 조회해서 캐시해 둔다 — 예전엔 Draw*
// 함수가 호출될 때마다(프레임당 수백~수천 번) 매번 문자열로 다시 조회했는데, 이건
// 드라이버 안에서 이름을 해시로 찾는 비용이 매 드로우콜마다 반복되는 것이라 불필요한
// CPU 비용이었다. 위치는 프로그램이 다시 링크되지 않는 한 절대 안 바뀌므로 캐시해도 안전하다.
struct ShaderProgramInfo
{
	GLuint program = 0;
	GLint uniformMVP = -1;
	GLint uniformColor = -1;
	GLint uniformTime = -1;
	GLint uniformPhaseOffset = -1;
	GLint uniformOpacity = -1;
	GLint uniformTexture = -1;
};

// 모든 그리기의 창구.
//
// 렌더 큐(배치): 단색 도형(사각형, 원·타원 메시, 삼각형 목록, 부드러운 원형 그림자)은 호출 즉시 그리지
// 않고, CPU에서 MVP까지 곱한 정점을 배치 버퍼에 차례로 모아 두었다가 Flush 때 드로우 콜 한 번으로
// 그린다. 예전엔 사각형 한 장이 드로우 콜 한 번이라(캐릭터 한 명 15회, 나무 한 그루 5회) 카메라를
// 멀리 빼면 오브젝트만으로 수백 회가 나갔다. 한 드로우 콜 안에서도 삼각형은 버퍼에 들어간 순서대로
// 합성되므로 페인터 알고리즘("나중에 그린 것이 위")은 그대로 유지된다.
//
// 전용 셰이더가 필요한 그리기(불꽃, 글자 텍스처, 타일 배치)는 먼저 모아 둔 것을 Flush한 뒤 그리므로
// 순서가 섞이지 않는다. 렌더러를 거치지 않고 GL을 직접 만지기 전(후처리가 프레임버퍼를 바꿀 때,
// 레거시 이름표, 버퍼 스왑)에는 호출하는 쪽이 Flush()를 먼저 불러야 한다.
class Renderer
{
public:
	Renderer(int windowSizeX, int windowSizeY);
	~Renderer();

	bool IsInitialized();

	// ---- 배치로 모이는 단색 그리기 ----

	// 단위 사각형(-0.5~0.5) 하나를 단색으로(캐릭터·건물 파츠, UI 막대 등).
	void DrawObject(const Mat4& mvp, float r, float g, float b, float a);

	// 가운데는 진하고 가장자리로 갈수록 투명해지는 원(단위 사각형 기준). 불빛 웅덩이, 빛 알갱이 등.
	void DrawSoftDisc(const Mat4& mvp, float r, float g, float b, float a);

	// 오브젝트 발밑의 부드러운 원형 그림자(블롭 섀도우) = 검은 DrawSoftDisc.
	void DrawShadow(const Mat4& mvp, float opacity);

	// 절차적으로 만든 메시(원, 타원 등)를 그리기 좋은 삼각형 목록으로 바꿔 핸들로 돌려준다.
	MeshHandle CreateMesh(const MeshData& meshData);
	void DestroyMesh(MeshHandle& mesh);
	void DrawMesh(const MeshHandle& mesh, const Mat4& mvp, float r, float g, float b, float a);

	// CPU에서 만든 삼각형 목록(x,y,z 반복)을 단색으로(미니맵 표시, 지붕 같은 임의의 다각형 등).
	void DrawTriangles(const float* vertices, int vertexCount, const Mat4& mvp, float r, float g, float b, float a);

	// 지금까지 모아 둔 단색 도형을 드로우 콜 한 번으로 그리고 비운다. 모아 둔 게 없으면 아무것도 안 한다.
	void Flush();

	// ---- 전용 셰이더(그리기 전에 자동으로 Flush) ----

	// 시간에 따라 일렁이는 횃불 불꽃을 그림
	void DrawFire(const Mat4& mvp, float time, float phaseOffset);

	// 단위 사각형에 텍스처를 입혀 그린다. 텍스처의 알파 채널이 커버리지(글자가 칠해진 정도)이고,
	// 색은 (r,g,b), 전체 불투명도는 a로 정한다 — 글자 텍스처(TextRasterizer) 전용.
	void DrawTexture(GLuint texture, const Mat4& mvp, float r, float g, float b, float a);

	// ---- 타일 배치(청크) 전용 ----
	// 정점 형식: 월드 좌표(x,y,z) + extra(2) + 색(r,g,b,a) = 9 floats/정점. extra는 지형이면 y에 재질
	// 번호, 물이면 x에 물가 정도를 담는다(Shaders/TileBatchSolid.fs, TileBatchWater.fs 참고).
	// 정점이 이미 월드 좌표로 구워져 있으므로 오브젝트별 모델 행렬이 필요 없다(mvp = 카메라의
	// view-projection 그대로). VBO/VAO는 TileBatchActor가 CreateTileBatch로 한 번만 만들어
	// 두고 계속 재사용한다 — 청크 하나(최대 64칸)가 드로우콜 1번으로 그려진다.
	struct TileBatchHandle
	{
		GLuint vbo = 0;
		GLuint vao = 0;
		int vertexCount = 0;
	};
	static const int kTileBatchFloatsPerVertex = 9;

	TileBatchHandle CreateTileBatch(const float* vertices, int vertexCount);
	void DestroyTileBatch(TileBatchHandle& batch);
	void DrawTileBatchSolid(const TileBatchHandle& batch, const Mat4& viewProjection, float time);
	void DrawTileBatchWater(const TileBatchHandle& batch, const Mat4& viewProjection, float time);

private:
	void Initialize(int windowSizeX, int windowSizeY);
	void CreateVertexBufferObjects();
	ShaderProgramInfo CompileAndCache(const char* filenameVS, const char* filenameFS);

	// 배치 버퍼에 정점 하나를 덧붙인다: (x,y,z)에 mvp를 곱한 클립 좌표 + 사각형 내부 로컬 좌표 + 색 + 모양
	// (0 = 단색, 1 = 부드러운 원, Shaders/ColorBatch.fs 참고).
	void QueueVertex(const Mat4& mvp, float x, float y, float z, float localX, float localY,
		float r, float g, float b, float a, float shape);

	// 단위 사각형(-0.5~0.5) 하나를 삼각형 2개로 덧붙인다.
	void QueueUnitQuad(const Mat4& mvp, float r, float g, float b, float a, float shape);

	// 배치 정점 하나의 float 개수: 클립 좌표 4 + 로컬 좌표 2 + 색 4 + 모양 1.
	static const int kBatchFloatsPerVertex = 11;

	bool m_Initialized = false;

	// 단위 사각형 VBO/VAO — 전용 셰이더 그리기(DrawFire/DrawTexture)가 쓴다. 모든 셰이더의 a_Position을
	// 위치 0으로 고정해 뒀기 때문에(각 .vs 참고) 셰이더가 바뀌어도 이 VAO를 그대로 재사용한다.
	GLuint m_VBORect = 0;
	GLuint m_VaoRect = 0;

	// 배치 버퍼: Flush 때마다 m_BatchVertices로 통째로 다시 채운다.
	GLuint m_VBOBatch = 0;
	GLuint m_VaoBatch = 0;
	std::vector<float> m_BatchVertices; // 매 Flush 뒤 clear()만 해서 용량은 재사용

	ShaderProgramInfo m_ColorBatch;
	ShaderProgramInfo m_Fire;
	ShaderProgramInfo m_Text;
	ShaderProgramInfo m_TileBatchSolid;
	ShaderProgramInfo m_TileBatchWater;
};
