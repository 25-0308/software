#include "stdafx.h"
#include "Renderer.h"
#include "ShaderUtil.h"

Renderer::Renderer(int windowSizeX, int windowSizeY)
{
	Initialize(windowSizeX, windowSizeY);
}

Renderer::~Renderer()
{
	if (m_SolidRect.program != 0) glDeleteProgram(m_SolidRect.program);
	if (m_Shadow.program != 0) glDeleteProgram(m_Shadow.program);
	if (m_Fire.program != 0) glDeleteProgram(m_Fire.program);
	if (m_Text.program != 0) glDeleteProgram(m_Text.program);
	if (m_TileBatchSolid.program != 0) glDeleteProgram(m_TileBatchSolid.program);
	if (m_TileBatchWater.program != 0) glDeleteProgram(m_TileBatchWater.program);

	if (m_VaoRect != 0) glDeleteVertexArrays(1, &m_VaoRect);
	if (m_VaoDynamic != 0) glDeleteVertexArrays(1, &m_VaoDynamic);
	if (m_VBORect != 0) glDeleteBuffers(1, &m_VBORect);
	if (m_VBODynamic != 0) glDeleteBuffers(1, &m_VBODynamic);
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

	m_SolidRect = CompileAndCache("./Shaders/SolidRect.vs", "./Shaders/SolidRect.fs");
	m_Shadow = CompileAndCache("./Shaders/Shadow.vs", "./Shaders/Shadow.fs");
	m_Fire = CompileAndCache("./Shaders/Shadow.vs", "./Shaders/Fire.fs");
	m_Text = CompileAndCache("./Shaders/Text.vs", "./Shaders/Text.fs");
	m_TileBatchSolid = CompileAndCache("./Shaders/TileBatch.vs", "./Shaders/TileBatchSolid.fs");
	m_TileBatchWater = CompileAndCache("./Shaders/TileBatch.vs", "./Shaders/TileBatchWater.fs");

	CreateVertexBufferObjects();

	if (m_SolidRect.program > 0 && m_Shadow.program > 0 && m_Fire.program > 0
		&& m_Text.program > 0 && m_TileBatchSolid.program > 0 && m_TileBatchWater.program > 0 && m_VBORect > 0)
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

	// 모든 정점 셰이더가 a_Position을 위치 0으로 고정해 뒀으므로(Shaders/*.vs 참고), 이 VAO
	// 하나를 셰이더가 바뀔 때마다 다시 설정하지 않고 계속 재사용할 수 있다.
	glGenVertexArrays(1, &m_VaoRect);
	glBindVertexArray(m_VaoRect);
	glBindBuffer(GL_ARRAY_BUFFER, m_VBORect);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 3, 0);

	// 내용은 DrawTriangles를 부를 때마다 채우지만(glBufferData), 버퍼 오브젝트 자체와 그
	// 속성 설정은 한 번만 하면 된다 — glBufferData로 내용을 다시 채워도 이미 설정해 둔
	// glVertexAttribPointer 바인딩은 그대로 유효하다.
	glGenBuffers(1, &m_VBODynamic);
	glGenVertexArrays(1, &m_VaoDynamic);
	glBindVertexArray(m_VaoDynamic);
	glBindBuffer(GL_ARRAY_BUFFER, m_VBODynamic);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 3, 0);

	glBindVertexArray(0);
}

void Renderer::DrawObject(const Mat4& mvp, float r, float g, float b, float a)
{
	glUseProgram(m_SolidRect.program);
	glUniformMatrix4fv(m_SolidRect.uniformMVP, 1, GL_FALSE, mvp.m);
	glUniform4f(m_SolidRect.uniformColor, r, g, b, a);

	glBindVertexArray(m_VaoRect);
	glDrawArrays(GL_TRIANGLES, 0, 6);
}

void Renderer::DrawShadow(const Mat4& mvp, float opacity)
{
	glUseProgram(m_Shadow.program);
	glUniformMatrix4fv(m_Shadow.uniformMVP, 1, GL_FALSE, mvp.m);
	glUniform1f(m_Shadow.uniformOpacity, opacity);

	glBindVertexArray(m_VaoRect);
	glDrawArrays(GL_TRIANGLES, 0, 6);
}

void Renderer::DrawFire(const Mat4& mvp, float time, float phaseOffset)
{
	glUseProgram(m_Fire.program);
	glUniformMatrix4fv(m_Fire.uniformMVP, 1, GL_FALSE, mvp.m);
	glUniform1f(m_Fire.uniformTime, time);
	glUniform1f(m_Fire.uniformPhaseOffset, phaseOffset);

	glBindVertexArray(m_VaoRect);
	glDrawArrays(GL_TRIANGLES, 0, 6);
}

MeshHandle Renderer::CreateMesh(const MeshData& meshData)
{
	MeshHandle handle;
	handle.vertexCount = (int)(meshData.vertices.size() / 3);
	handle.primitiveType = meshData.primitiveType;

	glGenBuffers(1, &handle.vbo);
	glBindBuffer(GL_ARRAY_BUFFER, handle.vbo);
	glBufferData(GL_ARRAY_BUFFER, meshData.vertices.size() * sizeof(float), meshData.vertices.data(), GL_STATIC_DRAW);

	// VAO도 메시를 만들 때 한 번만 설정해서 핸들에 담아 둔다 — DrawMesh는 이후로 그리기
	// 직전에 속성을 다시 설정할 필요 없이 이 VAO만 바인드하면 된다.
	glGenVertexArrays(1, &handle.vao);
	glBindVertexArray(handle.vao);
	glBindBuffer(GL_ARRAY_BUFFER, handle.vbo);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 3, 0);
	glBindVertexArray(0);

	return handle;
}

void Renderer::DestroyMesh(MeshHandle& mesh)
{
	if (mesh.vao != 0)
	{
		glDeleteVertexArrays(1, &mesh.vao);
		mesh.vao = 0;
	}
	if (mesh.vbo != 0)
	{
		glDeleteBuffers(1, &mesh.vbo);
		mesh.vbo = 0;
	}
}

void Renderer::DrawMesh(const MeshHandle& mesh, const Mat4& mvp, float r, float g, float b, float a)
{
	glUseProgram(m_SolidRect.program);
	glUniformMatrix4fv(m_SolidRect.uniformMVP, 1, GL_FALSE, mvp.m);
	glUniform4f(m_SolidRect.uniformColor, r, g, b, a);

	glBindVertexArray(mesh.vao);
	glDrawArrays(mesh.primitiveType, 0, mesh.vertexCount);
}

void Renderer::DrawTriangles(const float* vertices, int vertexCount, const Mat4& mvp, float r, float g, float b, float a)
{
	if (vertices == nullptr || vertexCount <= 0)
	{
		return;
	}

	glUseProgram(m_SolidRect.program);
	glUniformMatrix4fv(m_SolidRect.uniformMVP, 1, GL_FALSE, mvp.m);
	glUniform4f(m_SolidRect.uniformColor, r, g, b, a);

	// 임시 버퍼를 이번 호출의 정점으로 통째로 다시 채운다(GL_STREAM_DRAW: 한 번 쓰고 한 번 그림).
	glBindVertexArray(m_VaoDynamic);
	glBindBuffer(GL_ARRAY_BUFFER, m_VBODynamic);
	glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 3 * vertexCount, vertices, GL_STREAM_DRAW);

	glDrawArrays(GL_TRIANGLES, 0, vertexCount);
}

void Renderer::DrawTexture(GLuint texture, const Mat4& mvp, float r, float g, float b, float a)
{
	if (texture == 0)
	{
		return;
	}

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

	GLsizei stride = sizeof(float) * kTileBatchFloatsPerVertex;
	glEnableVertexAttribArray(0); // a_Position (x,y,z)
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
	glEnableVertexAttribArray(1); // a_Local (lx,ly)
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

void Renderer::DrawTileBatchSolid(const TileBatchHandle& batch, const Mat4& viewProjection)
{
	if (batch.vao == 0 || batch.vertexCount <= 0)
	{
		return;
	}

	glUseProgram(m_TileBatchSolid.program);
	glUniformMatrix4fv(m_TileBatchSolid.uniformMVP, 1, GL_FALSE, viewProjection.m);

	glBindVertexArray(batch.vao);
	glDrawArrays(GL_TRIANGLES, 0, batch.vertexCount);
}

void Renderer::DrawTileBatchWater(const TileBatchHandle& batch, const Mat4& viewProjection, float time)
{
	if (batch.vao == 0 || batch.vertexCount <= 0)
	{
		return;
	}

	glUseProgram(m_TileBatchWater.program);
	glUniformMatrix4fv(m_TileBatchWater.uniformMVP, 1, GL_FALSE, viewProjection.m);
	glUniform1f(m_TileBatchWater.uniformTime, time);

	glBindVertexArray(batch.vao);
	glDrawArrays(GL_TRIANGLES, 0, batch.vertexCount);
}
