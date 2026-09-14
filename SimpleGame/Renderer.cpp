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
	if (m_VBORect != 0)
	{
		glDeleteBuffers(1, &m_VBORect);
	}
}

void Renderer::Initialize(int windowSizeX, int windowSizeY)
{
	glViewport(0, 0, windowSizeX, windowSizeY);

	//Load shaders
	m_SolidRectShader = ShaderUtil::CompileShaderProgram("./Shaders/SolidRect.vs", "./Shaders/SolidRect.fs");

	//Create VBOs
	CreateVertexBufferObjects();

	if (m_SolidRectShader > 0 && m_VBORect > 0)
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
	// Unit quad centered on the origin; world position/size/orientation are
	// applied entirely through the MVP matrix passed to DrawObject().
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

void Renderer::DrawObject(const Mat4& mvp, float r, float g, float b, float a)
{
	glUseProgram(m_SolidRectShader);

	glUniformMatrix4fv(glGetUniformLocation(m_SolidRectShader, "u_MVP"), 1, GL_FALSE, mvp.m);
	glUniform4f(glGetUniformLocation(m_SolidRectShader, "u_Color"), r, g, b, a);

	int attribPosition = glGetAttribLocation(m_SolidRectShader, "a_Position");
	glEnableVertexAttribArray(attribPosition);
	glBindBuffer(GL_ARRAY_BUFFER, m_VBORect);
	glVertexAttribPointer(attribPosition, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 3, 0);

	glDrawArrays(GL_TRIANGLES, 0, 6);

	glDisableVertexAttribArray(attribPosition);
}
