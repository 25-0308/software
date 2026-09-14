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
	if (m_VBORect != 0)
	{
		glDeleteBuffers(1, &m_VBORect);
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

	CreateVertexBufferObjects();

	if (m_SolidRectShader > 0 && m_ShadowShader > 0 && m_WaterShader > 0 && m_FireShader > 0 && m_VBORect > 0)
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
