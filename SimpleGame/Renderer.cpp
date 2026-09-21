#include "stdafx.h"
#include "Renderer.h"
#include "ShaderUtil.h"

Renderer::Renderer(int windowSizeX, int windowSizeY)
{
	Initialize(windowSizeX, windowSizeY);
}

Renderer::~Renderer()
{
	if (m_SolidRectShader != 0)
	{
		glDeleteProgram(m_SolidRectShader);
	}
	if (m_ShadowShader != 0)
	{
		glDeleteProgram(m_ShadowShader);
	}
	if (m_WaterShader != 0)
	{
		glDeleteProgram(m_WaterShader);
	}
	if (m_FireShader != 0)
	{
		glDeleteProgram(m_FireShader);
	}
	if (m_TextShader != 0)
	{
		glDeleteProgram(m_TextShader);
	}
	if (m_VBORect != 0)
	{
		glDeleteBuffers(1, &m_VBORect);
	}
	if (m_VBODynamic != 0)
	{
		glDeleteBuffers(1, &m_VBODynamic);
	}
}

void Renderer::Initialize(int windowSizeX, int windowSizeY)
{
	glViewport(0, 0, windowSizeX, windowSizeY);

	// 반투명 그림자/불꽃을 표현하기 위해 알파 블렌딩을 켠다.
	// 불투명 오브젝트는 알파가 항상 1이라 기존 렌더링에는 영향이 없다.
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	m_SolidRectShader = ShaderUtil::CompileShaderProgram("./Shaders/SolidRect.vs", "./Shaders/SolidRect.fs");
	m_ShadowShader = ShaderUtil::CompileShaderProgram("./Shaders/Shadow.vs", "./Shaders/Shadow.fs");
	m_WaterShader = ShaderUtil::CompileShaderProgram("./Shaders/Water.vs", "./Shaders/Water.fs");
	m_FireShader = ShaderUtil::CompileShaderProgram("./Shaders/Shadow.vs", "./Shaders/Fire.fs");
	m_TextShader = ShaderUtil::CompileShaderProgram("./Shaders/Text.vs", "./Shaders/Text.fs");

	CreateVertexBufferObjects();

	if (m_SolidRectShader > 0 && m_ShadowShader > 0 && m_WaterShader > 0 && m_FireShader > 0 && m_TextShader > 0 && m_VBORect > 0)
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

	// 내용은 DrawTriangles를 부를 때마다 채운다.
	glGenBuffers(1, &m_VBODynamic);
}

void Renderer::BindQuadPositionAttribute(GLuint shader)
{
	glBindBuffer(GL_ARRAY_BUFFER, m_VBORect);

	int attribPosition = glGetAttribLocation(shader, "a_Position");
	glEnableVertexAttribArray(attribPosition);
	glVertexAttribPointer(attribPosition, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 3, 0);
}

void Renderer::DrawObject(const Mat4& mvp, float r, float g, float b, float a)
{
	glUseProgram(m_SolidRectShader);

	glUniformMatrix4fv(glGetUniformLocation(m_SolidRectShader, "u_MVP"), 1, GL_FALSE, mvp.m);
	glUniform4f(glGetUniformLocation(m_SolidRectShader, "u_Color"), r, g, b, a);

	BindQuadPositionAttribute(m_SolidRectShader);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	glDisableVertexAttribArray(glGetAttribLocation(m_SolidRectShader, "a_Position"));
}

void Renderer::DrawShadow(const Mat4& mvp, float opacity)
{
	glUseProgram(m_ShadowShader);

	glUniformMatrix4fv(glGetUniformLocation(m_ShadowShader, "u_MVP"), 1, GL_FALSE, mvp.m);
	glUniform1f(glGetUniformLocation(m_ShadowShader, "u_Opacity"), opacity);

	BindQuadPositionAttribute(m_ShadowShader);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	glDisableVertexAttribArray(glGetAttribLocation(m_ShadowShader, "a_Position"));
}

void Renderer::DrawWater(const Mat4& mvp, float r, float g, float b, float a, float time, float phaseOffset)
{
	glUseProgram(m_WaterShader);

	glUniformMatrix4fv(glGetUniformLocation(m_WaterShader, "u_MVP"), 1, GL_FALSE, mvp.m);
	glUniform4f(glGetUniformLocation(m_WaterShader, "u_Color"), r, g, b, a);
	glUniform1f(glGetUniformLocation(m_WaterShader, "u_Time"), time);
	glUniform1f(glGetUniformLocation(m_WaterShader, "u_PhaseOffset"), phaseOffset);

	BindQuadPositionAttribute(m_WaterShader);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	glDisableVertexAttribArray(glGetAttribLocation(m_WaterShader, "a_Position"));
}

void Renderer::DrawFire(const Mat4& mvp, float time, float phaseOffset)
{
	glUseProgram(m_FireShader);

	glUniformMatrix4fv(glGetUniformLocation(m_FireShader, "u_MVP"), 1, GL_FALSE, mvp.m);
	glUniform1f(glGetUniformLocation(m_FireShader, "u_Time"), time);
	glUniform1f(glGetUniformLocation(m_FireShader, "u_PhaseOffset"), phaseOffset);

	BindQuadPositionAttribute(m_FireShader);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	glDisableVertexAttribArray(glGetAttribLocation(m_FireShader, "a_Position"));
}

MeshHandle Renderer::CreateMesh(const MeshData& meshData)
{
	MeshHandle handle;
	handle.vertexCount = (int)(meshData.vertices.size() / 3);
	handle.primitiveType = meshData.primitiveType;

	glGenBuffers(1, &handle.vbo);
	glBindBuffer(GL_ARRAY_BUFFER, handle.vbo);
	glBufferData(GL_ARRAY_BUFFER, meshData.vertices.size() * sizeof(float), meshData.vertices.data(), GL_STATIC_DRAW);

	return handle;
}

void Renderer::DestroyMesh(MeshHandle& mesh)
{
	if (mesh.vbo != 0)
	{
		glDeleteBuffers(1, &mesh.vbo);
		mesh.vbo = 0;
	}
}

void Renderer::DrawMesh(const MeshHandle& mesh, const Mat4& mvp, float r, float g, float b, float a)
{
	glUseProgram(m_SolidRectShader);

	glUniformMatrix4fv(glGetUniformLocation(m_SolidRectShader, "u_MVP"), 1, GL_FALSE, mvp.m);
	glUniform4f(glGetUniformLocation(m_SolidRectShader, "u_Color"), r, g, b, a);

	glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
	int attribPosition = glGetAttribLocation(m_SolidRectShader, "a_Position");
	glEnableVertexAttribArray(attribPosition);
	glVertexAttribPointer(attribPosition, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 3, 0);

	glDrawArrays(mesh.primitiveType, 0, mesh.vertexCount);

	glDisableVertexAttribArray(attribPosition);
}

void Renderer::DrawTriangles(const float* vertices, int vertexCount, const Mat4& mvp, float r, float g, float b, float a)
{
	if (vertices == nullptr || vertexCount <= 0)
	{
		return;
	}

	glUseProgram(m_SolidRectShader);

	glUniformMatrix4fv(glGetUniformLocation(m_SolidRectShader, "u_MVP"), 1, GL_FALSE, mvp.m);
	glUniform4f(glGetUniformLocation(m_SolidRectShader, "u_Color"), r, g, b, a);

	// 임시 버퍼를 이번 호출의 정점으로 통째로 다시 채운다(GL_STREAM_DRAW: 한 번 쓰고 한 번 그림).
	glBindBuffer(GL_ARRAY_BUFFER, m_VBODynamic);
	glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 3 * vertexCount, vertices, GL_STREAM_DRAW);

	int attribPosition = glGetAttribLocation(m_SolidRectShader, "a_Position");
	glEnableVertexAttribArray(attribPosition);
	glVertexAttribPointer(attribPosition, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 3, 0);

	glDrawArrays(GL_TRIANGLES, 0, vertexCount);

	glDisableVertexAttribArray(attribPosition);
}

void Renderer::DrawTexture(GLuint texture, const Mat4& mvp, float r, float g, float b, float a)
{
	if (texture == 0)
	{
		return;
	}

	glUseProgram(m_TextShader);

	glUniformMatrix4fv(glGetUniformLocation(m_TextShader, "u_MVP"), 1, GL_FALSE, mvp.m);
	glUniform4f(glGetUniformLocation(m_TextShader, "u_Color"), r, g, b, a);

	// 후처리 단계가 활성 텍스처 유닛을 바꿔둘 수 있으므로 0번 유닛을 명시적으로 고른다.
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, texture);
	glUniform1i(glGetUniformLocation(m_TextShader, "u_Texture"), 0);

	BindQuadPositionAttribute(m_TextShader);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	glDisableVertexAttribArray(glGetAttribLocation(m_TextShader, "a_Position"));
}
