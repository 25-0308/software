#include "stdafx.h"
#include "Renderer.h"
#include "ShaderUtil.h"

#include <iostream>

namespace
{
	// 원점 중심의 단위 사각형(삼각형 2개). m_VBORect와 같은 정점 순서.
	const float kUnitQuad[6][2] =
	{
		{ -0.5f, -0.5f }, { -0.5f, 0.5f }, { 0.5f, 0.5f },
		{ -0.5f, -0.5f }, { 0.5f, 0.5f }, { 0.5f, -0.5f },
	};

	// 배치 정점의 모양 값(Shaders/ColorBatch.fs 참고).
	const float kShapeSolid = 0.f;
	const float kShapeSoftDisc = 1.f;

	// vertices(x,y,z 반복)의 index번째 정점을 out 뒤에 덧붙인다.
	void AppendPoint(std::vector<float>& out, const float* vertices, size_t index)
	{
		out.push_back(vertices[index * 3 + 0]);
		out.push_back(vertices[index * 3 + 1]);
		out.push_back(vertices[index * 3 + 2]);
	}
}

Renderer::Renderer(int windowSizeX, int windowSizeY)
{
	Initialize(windowSizeX, windowSizeY);
}

Renderer::~Renderer()
{
	if (m_ColorBatch.program != 0) glDeleteProgram(m_ColorBatch.program);
	if (m_Fire.program != 0) glDeleteProgram(m_Fire.program);
	if (m_Text.program != 0) glDeleteProgram(m_Text.program);
	if (m_TileBatchSolid.program != 0) glDeleteProgram(m_TileBatchSolid.program);
	if (m_TileBatchWater.program != 0) glDeleteProgram(m_TileBatchWater.program);

	if (m_VaoRect != 0) glDeleteVertexArrays(1, &m_VaoRect);
	if (m_VaoBatch != 0) glDeleteVertexArrays(1, &m_VaoBatch);
	if (m_VBORect != 0) glDeleteBuffers(1, &m_VBORect);
	if (m_VBOBatch != 0) glDeleteBuffers(1, &m_VBOBatch);
}

ShaderProgramInfo Renderer::CompileAndCache(const char* filenameVS, const char* filenameFS)
{
	ShaderProgramInfo info;
	info.program = ShaderUtil::CompileShaderProgram(filenameVS, filenameFS);

	if (info.program == 0)
	{
		return info;
	}

	// 셰이더마다 실제로 쓰는 유니폼만 있고 나머지는 없지만, 없는 이름을 조회해도 -1이 돌아올
	// 뿐 에러가 나지 않으므로 그냥 전부 조회해서 캐시해 둔다 — 해당 없는 필드는 -1로 남고,
	// glUniform*(-1, ...)은 조용히 무시되므로 Draw* 쪽에서 굳이 분기할 필요가 없다.
	info.uniformMVP = glGetUniformLocation(info.program, "u_MVP");
	info.uniformColor = glGetUniformLocation(info.program, "u_Color");
	info.uniformTime = glGetUniformLocation(info.program, "u_Time");
	info.uniformPhaseOffset = glGetUniformLocation(info.program, "u_PhaseOffset");
	info.uniformOpacity = glGetUniformLocation(info.program, "u_Opacity");
	info.uniformTexture = glGetUniformLocation(info.program, "u_Texture");
	return info;
}

void Renderer::Initialize(int windowSizeX, int windowSizeY)
{
	glViewport(0, 0, windowSizeX, windowSizeY);

	// 반투명 그림자/불꽃을 표현하기 위해 알파 블렌딩을 켠다.
	// 불투명 오브젝트는 알파가 항상 1이라 기존 렌더링에는 영향이 없다.
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	m_ColorBatch = CompileAndCache("./Shaders/ColorBatch.vs", "./Shaders/ColorBatch.fs");
	m_Fire = CompileAndCache("./Shaders/Shadow.vs", "./Shaders/Fire.fs");
	m_Text = CompileAndCache("./Shaders/Text.vs", "./Shaders/Text.fs");
	m_TileBatchSolid = CompileAndCache("./Shaders/TileBatch.vs", "./Shaders/TileBatchSolid.fs");
	m_TileBatchWater = CompileAndCache("./Shaders/TileBatch.vs", "./Shaders/TileBatchWater.fs");

	CreateVertexBufferObjects();

	if (m_ColorBatch.program > 0 && m_Fire.program > 0 && m_Text.program > 0
		&& m_TileBatchSolid.program > 0 && m_TileBatchWater.program > 0 && m_VBORect > 0 && m_VBOBatch > 0)
	{
		m_Initialized = true;
	}
}

bool Renderer::IsInitialized()
{
	return m_Initialized;
}

void Renderer::CreateVertexBufferObjects()
{
	// 원점을 중심으로 한 단위 사각형. 월드 위치/크기/방향은 각 Draw* 함수에
	// 넘기는 MVP 행렬로만 적용된다.
	float rect[]
		=
	{
		-0.5f, -0.5f, 0.f,  -0.5f, 0.5f, 0.f,  0.5f, 0.5f, 0.f, //Triangle1
		-0.5f, -0.5f, 0.f,   0.5f, 0.5f, 0.f,  0.5f, -0.5f, 0.f, //Triangle2
	};

	glGenBuffers(1, &m_VBORect);
	glBindBuffer(GL_ARRAY_BUFFER, m_VBORect);
	glBufferData(GL_ARRAY_BUFFER, sizeof(rect), rect, GL_STATIC_DRAW);

	glGenVertexArrays(1, &m_VaoRect);
	glBindVertexArray(m_VaoRect);
	glBindBuffer(GL_ARRAY_BUFFER, m_VBORect);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 3, 0);

	// 배치 버퍼: 내용은 Flush 때마다 통째로 다시 채우지만(glBufferData), 버퍼 오브젝트 자체와 그
	// 정점 속성 설정은 한 번만 하면 된다 — 내용을 다시 채워도 설정해 둔 속성 바인딩은 그대로 유효하다.
	GLsizei stride = (GLsizei)(sizeof(float) * kBatchFloatsPerVertex);

	glGenBuffers(1, &m_VBOBatch);
	glGenVertexArrays(1, &m_VaoBatch);
	glBindVertexArray(m_VaoBatch);
	glBindBuffer(GL_ARRAY_BUFFER, m_VBOBatch);
	glEnableVertexAttribArray(0); // a_Position: 클립 좌표 (x,y,z,w)
	glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, stride, (void*)0);
	glEnableVertexAttribArray(1); // a_Local: 사각형 내부 로컬 좌표
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (void*)(sizeof(float) * 4));
	glEnableVertexAttribArray(2); // a_Color
	glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, stride, (void*)(sizeof(float) * 6));
	glEnableVertexAttribArray(3); // a_Shape
	glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, stride, (void*)(sizeof(float) * 10));

	glBindVertexArray(0);
}

void Renderer::QueueVertex(const Mat4& mvp, float x, float y, float z, float localX, float localY,
	float r, float g, float b, float a, float shape)
{
	const float* m = mvp.m;

	size_t start = m_BatchVertices.size();
	m_BatchVertices.resize(start + kBatchFloatsPerVertex);
	float* out = &m_BatchVertices[start];

	// 셰이더가 하던 u_MVP * 위치를 여기서 미리 한다(열 우선 저장: m[열*4+행]). w까지 넘겨서
	// 원근 투영이 들어와도 GPU가 그대로 나눗셈을 할 수 있게 한다(지금은 전부 직교라 w = 1).
	out[0] = m[0] * x + m[4] * y + m[8] * z + m[12];
	out[1] = m[1] * x + m[5] * y + m[9] * z + m[13];
	out[2] = m[2] * x + m[6] * y + m[10] * z + m[14];
	out[3] = m[3] * x + m[7] * y + m[11] * z + m[15];
	out[4] = localX;
	out[5] = localY;
	out[6] = r;
	out[7] = g;
	out[8] = b;
	out[9] = a;
	out[10] = shape;
}

void Renderer::QueueUnitQuad(const Mat4& mvp, float r, float g, float b, float a, float shape)
{
	for (int i = 0; i < 6; ++i)
	{
		float lx = kUnitQuad[i][0];
		float ly = kUnitQuad[i][1];
		QueueVertex(mvp, lx, ly, 0.f, lx, ly, r, g, b, a, shape);
	}
}

void Renderer::DrawObject(const Mat4& mvp, float r, float g, float b, float a)
{
	QueueUnitQuad(mvp, r, g, b, a, kShapeSolid);
}

void Renderer::DrawSoftDisc(const Mat4& mvp, float r, float g, float b, float a)
{
	QueueUnitQuad(mvp, r, g, b, a, kShapeSoftDisc);
}

void Renderer::DrawShadow(const Mat4& mvp, float opacity)
{
	DrawSoftDisc(mvp, 0.f, 0.f, 0.f, opacity);
}

MeshHandle Renderer::CreateMesh(const MeshData& meshData)
{
	MeshHandle handle;
	size_t vertexCount = meshData.vertices.size() / 3;
	const float* vertices = meshData.vertices.data();

	if (meshData.primitiveType == GL_TRIANGLE_FAN)
	{
		// 팬(0, i, i+1)을 삼각형 목록으로 푼다 — 배치 버퍼는 삼각형 목록 하나로 이어 그리기 때문.
		for (size_t i = 1; i + 1 < vertexCount; ++i)
		{
			AppendPoint(handle.triangles, vertices, 0);
			AppendPoint(handle.triangles, vertices, i);
			AppendPoint(handle.triangles, vertices, i + 1);
		}
	}
	else if (meshData.primitiveType == GL_TRIANGLES)
	{
		handle.triangles.assign(vertices, vertices + vertexCount * 3);
	}
	else
	{
		std::cout << "[렌더러] 지원하지 않는 메시 형식(" << meshData.primitiveType << ")이라 그리지 않습니다.\n";
	}

	return handle;
}

void Renderer::DestroyMesh(MeshHandle& mesh)
{
	mesh.triangles.clear();
	mesh.triangles.shrink_to_fit();
}

void Renderer::DrawMesh(const MeshHandle& mesh, const Mat4& mvp, float r, float g, float b, float a)
{
	size_t vertexCount = mesh.triangles.size() / 3;

	for (size_t i = 0; i < vertexCount; ++i)
	{
		const float* v = &mesh.triangles[i * 3];
		QueueVertex(mvp, v[0], v[1], v[2], 0.f, 0.f, r, g, b, a, kShapeSolid);
	}
}

void Renderer::DrawTriangles(const float* vertices, int vertexCount, const Mat4& mvp, float r, float g, float b, float a)
{
	if (vertices == nullptr || vertexCount <= 0)
	{
		return;
	}

	for (int i = 0; i < vertexCount; ++i)
	{
		const float* v = &vertices[i * 3];
		QueueVertex(mvp, v[0], v[1], v[2], 0.f, 0.f, r, g, b, a, kShapeSolid);
	}
}

void Renderer::Flush()
{
	if (m_BatchVertices.empty())
	{
		return;
	}

	int vertexCount = (int)(m_BatchVertices.size() / kBatchFloatsPerVertex);

	glUseProgram(m_ColorBatch.program);
	glBindVertexArray(m_VaoBatch);

	// 버퍼를 이번에 모은 정점으로 통째로 다시 채운다(GL_STREAM_DRAW: 한 번 쓰고 한 번 그림).
	glBindBuffer(GL_ARRAY_BUFFER, m_VBOBatch);
	glBufferData(GL_ARRAY_BUFFER, sizeof(float) * m_BatchVertices.size(), m_BatchVertices.data(), GL_STREAM_DRAW);

	glDrawArrays(GL_TRIANGLES, 0, vertexCount);

	m_BatchVertices.clear();
}

void Renderer::DrawFire(const Mat4& mvp, float time, float phaseOffset)
{
	// 먼저 모아 둔 도형을 그려야 이 불꽃보다 앞서 그려야 할 것들이 불꽃을 덮지 않는다.
	Flush();

	glUseProgram(m_Fire.program);
	glUniformMatrix4fv(m_Fire.uniformMVP, 1, GL_FALSE, mvp.m);
	glUniform1f(m_Fire.uniformTime, time);
	glUniform1f(m_Fire.uniformPhaseOffset, phaseOffset);

	glBindVertexArray(m_VaoRect);
	glDrawArrays(GL_TRIANGLES, 0, 6);
}

void Renderer::DrawTexture(GLuint texture, const Mat4& mvp, float r, float g, float b, float a)
{
	if (texture == 0)
	{
		return;
	}

	Flush();

	glUseProgram(m_Text.program);
	glUniformMatrix4fv(m_Text.uniformMVP, 1, GL_FALSE, mvp.m);
	glUniform4f(m_Text.uniformColor, r, g, b, a);

	// 후처리 단계가 활성 텍스처 유닛을 바꿔둘 수 있으므로 0번 유닛을 명시적으로 고른다.
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, texture);
	glUniform1i(m_Text.uniformTexture, 0);

	glBindVertexArray(m_VaoRect);
	glDrawArrays(GL_TRIANGLES, 0, 6);
}

Renderer::TileBatchHandle Renderer::CreateTileBatch(const float* vertices, int vertexCount)
{
	TileBatchHandle batch;
	batch.vertexCount = vertexCount;

	if (vertices == nullptr || vertexCount <= 0)
	{
		return batch;
	}

	glGenBuffers(1, &batch.vbo);
	glBindBuffer(GL_ARRAY_BUFFER, batch.vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(float) * kTileBatchFloatsPerVertex * vertexCount, vertices, GL_STATIC_DRAW);

	glGenVertexArrays(1, &batch.vao);
	glBindVertexArray(batch.vao);
	glBindBuffer(GL_ARRAY_BUFFER, batch.vbo);

	GLsizei stride = (GLsizei)(sizeof(float) * kTileBatchFloatsPerVertex);
	glEnableVertexAttribArray(0); // a_Position (x,y,z)
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
	glEnableVertexAttribArray(1); // a_Extra (재질 번호 / 물가 정도)
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (void*)(sizeof(float) * 3));
	glEnableVertexAttribArray(2); // a_Color (r,g,b,a)
	glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, stride, (void*)(sizeof(float) * 5));

	glBindVertexArray(0);
	return batch;
}

void Renderer::DestroyTileBatch(TileBatchHandle& batch)
{
	if (batch.vao != 0)
	{
		glDeleteVertexArrays(1, &batch.vao);
		batch.vao = 0;
	}
	if (batch.vbo != 0)
	{
		glDeleteBuffers(1, &batch.vbo);
		batch.vbo = 0;
	}
	batch.vertexCount = 0;
}

void Renderer::DrawTileBatchSolid(const TileBatchHandle& batch, const Mat4& viewProjection, float time)
{
	if (batch.vao == 0 || batch.vertexCount <= 0)
	{
		return;
	}

	Flush();

	glUseProgram(m_TileBatchSolid.program);
	glUniformMatrix4fv(m_TileBatchSolid.uniformMVP, 1, GL_FALSE, viewProjection.m);
	glUniform1f(m_TileBatchSolid.uniformTime, time);

	glBindVertexArray(batch.vao);
	glDrawArrays(GL_TRIANGLES, 0, batch.vertexCount);
}

void Renderer::DrawTileBatchWater(const TileBatchHandle& batch, const Mat4& viewProjection, float time)
{
	if (batch.vao == 0 || batch.vertexCount <= 0)
	{
		return;
	}

	Flush();

	glUseProgram(m_TileBatchWater.program);
	glUniformMatrix4fv(m_TileBatchWater.uniformMVP, 1, GL_FALSE, viewProjection.m);
	glUniform1f(m_TileBatchWater.uniformTime, time);

	glBindVertexArray(batch.vao);
	glDrawArrays(GL_TRIANGLES, 0, batch.vertexCount);
}
