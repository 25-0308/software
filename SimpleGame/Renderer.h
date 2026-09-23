#pragma once

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

	// 시간에 따라 일렁이는 횃불/모닥불을 그림
	void DrawFire(const Mat4& mvp, float time, float phaseOffset);

	// 절차적으로 만든 메시(원, 타원 등)를 GPU에 올려 핸들로 반환한다. VAO도 이때 한 번만
	// 만들어서 핸들에 같이 담아 두므로, DrawMesh는 그때그때 정점 속성을 다시 설정할 필요가 없다.
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

	// ---- 타일 배치(청크) 전용 ----
	// 정점 형식: 월드 좌표(x,y,z) + 타일 내부 로컬 좌표(lx,ly, -0.5~0.5) + 색(r,g,b,a) = 9 floats/정점.
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
	void DrawTileBatchSolid(const TileBatchHandle& batch, const Mat4& viewProjection);
	void DrawTileBatchWater(const TileBatchHandle& batch, const Mat4& viewProjection, float time);

private:
	void Initialize(int windowSizeX, int windowSizeY);
	void CreateVertexBufferObjects();
	ShaderProgramInfo CompileAndCache(const char* filenameVS, const char* filenameFS);

	bool m_Initialized = false;

	GLuint m_VBORect = 0;
	GLuint m_VBODynamic = 0; // DrawTriangles가 매번 내용을 덮어쓰는 임시 정점 버퍼

	// a_Position을 모든 셰이더에서 위치 0으로 고정해 뒀기 때문에(각 .vs 참고), 이 두 VAO는
	// 셰이더 프로그램이 바뀌어도(glUseProgram) 다시 설정할 필요 없이 그대로 재사용된다.
	GLuint m_VaoRect = 0;    // m_VBORect(단위 사각형)를 가리킴 — DrawObject/DrawShadow/DrawFire/DrawTexture
	GLuint m_VaoDynamic = 0; // m_VBODynamic을 가리킴 — DrawTriangles

	ShaderProgramInfo m_SolidRect;
	ShaderProgramInfo m_Shadow;
	ShaderProgramInfo m_Fire;
	ShaderProgramInfo m_Text;
	ShaderProgramInfo m_TileBatchSolid;
	ShaderProgramInfo m_TileBatchWater;
};
