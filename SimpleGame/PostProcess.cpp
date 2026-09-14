#include "stdafx.h"
#include "PostProcess.h"
#include "ShaderUtil.h"

#include <iostream>

PostProcess::PostProcess(int width, int height)
	: m_Width(width)
	, m_Height(height)
{
	CreateFramebuffer(width, height);
	CreateFullscreenQuad();
	m_Shader = ShaderUtil::CompileShaderProgram("./Shaders/PostProcess.vs", "./Shaders/PostProcess.fs");
}

PostProcess::~PostProcess()
{
	if (m_Shader != 0)
	{
		glDeleteProgram(m_Shader);
	}
	if (m_QuadVBO != 0)
	{
		glDeleteBuffers(1, &m_QuadVBO);
	}
	if (m_ColorTexture != 0)
	{
		glDeleteTextures(1, &m_ColorTexture);
	}
	if (m_FBO != 0)
	{
		glDeleteFramebuffers(1, &m_FBO);
	}
}

void PostProcess::CreateFramebuffer(int width, int height)
{
	glGenTextures(1, &m_ColorTexture);
	glBindTexture(GL_TEXTURE_2D, m_ColorTexture);
	// RGBA16F so lighting can exceed 1.0 before tone mapping instead of clipping.
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glBindTexture(GL_TEXTURE_2D, 0);

	glGenFramebuffers(1, &m_FBO);
	glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_ColorTexture, 0);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		std::cout << "PostProcess framebuffer is not complete.\n";
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void PostProcess::CreateFullscreenQuad()
{
	// x, y, u, v
	float quad[] =
	{
		-1.f, -1.f, 0.f, 0.f,
		 1.f, -1.f, 1.f, 0.f,
		 1.f,  1.f, 1.f, 1.f,

		-1.f, -1.f, 0.f, 0.f,
		 1.f,  1.f, 1.f, 1.f,
		-1.f,  1.f, 0.f, 1.f,
	};

	glGenBuffers(1, &m_QuadVBO);
	glBindBuffer(GL_ARRAY_BUFFER, m_QuadVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
}

void PostProcess::BeginCapture()
{
	glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);
	glViewport(0, 0, m_Width, m_Height);
}

void PostProcess::EndCaptureAndPresent(float exposure, float vignetteStrength)
{
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, m_Width, m_Height);
	glClear(GL_COLOR_BUFFER_BIT);

	glUseProgram(m_Shader);
	glUniform1f(glGetUniformLocation(m_Shader, "u_Exposure"), exposure);
	glUniform1f(glGetUniformLocation(m_Shader, "u_VignetteStrength"), vignetteStrength);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, m_ColorTexture);
	glUniform1i(glGetUniformLocation(m_Shader, "u_Scene"), 0);

	glBindBuffer(GL_ARRAY_BUFFER, m_QuadVBO);

	int positionAttrib = glGetAttribLocation(m_Shader, "a_Position");
	int uvAttrib = glGetAttribLocation(m_Shader, "a_UV");

	glEnableVertexAttribArray(positionAttrib);
	glVertexAttribPointer(positionAttrib, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, (void*)0);

	glEnableVertexAttribArray(uvAttrib);
	glVertexAttribPointer(uvAttrib, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, (void*)(sizeof(float) * 2));

	glDrawArrays(GL_TRIANGLES, 0, 6);

	glDisableVertexAttribArray(positionAttrib);
	glDisableVertexAttribArray(uvAttrib);

	glBindTexture(GL_TEXTURE_2D, 0);
}
