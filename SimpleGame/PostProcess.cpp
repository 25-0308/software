#include "stdafx.h"
#include "PostProcess.h"
#include "ShaderUtil.h"

#include <iostream>

namespace
{
	// Number of horizontal+vertical blur pairs applied to the bright buffer;
	// higher = wider, softer glow but more draw calls.
	const int kBlurPasses = 5;
}

PostProcess::PostProcess(int width, int height)
	: m_Width(width)
	, m_Height(height)
	, m_BlurWidth(width / 2)
	, m_BlurHeight(height / 2)
{
	m_Scene = CreateColorFramebuffer(m_Width, m_Height);
	// Bloom buffers run at half resolution: cheaper, and the blur softens the
	// downsampling artifacts anyway.
	m_Bright = CreateColorFramebuffer(m_BlurWidth, m_BlurHeight);
	m_PingPong[0] = CreateColorFramebuffer(m_BlurWidth, m_BlurHeight);
	m_PingPong[1] = CreateColorFramebuffer(m_BlurWidth, m_BlurHeight);

	CreateFullscreenQuad();

	m_BrightExtractShader = ShaderUtil::CompileShaderProgram("./Shaders/PostProcess.vs", "./Shaders/BrightExtract.fs");
	m_BlurShader = ShaderUtil::CompileShaderProgram("./Shaders/PostProcess.vs", "./Shaders/Blur.fs");
	m_CompositeShader = ShaderUtil::CompileShaderProgram("./Shaders/PostProcess.vs", "./Shaders/Composite.fs");
}

PostProcess::~PostProcess()
{
	if (m_BrightExtractShader != 0)
	{
		glDeleteProgram(m_BrightExtractShader);
	}
	if (m_BlurShader != 0)
	{
		glDeleteProgram(m_BlurShader);
	}
	if (m_CompositeShader != 0)
	{
		glDeleteProgram(m_CompositeShader);
	}
	if (m_QuadVBO != 0)
	{
		glDeleteBuffers(1, &m_QuadVBO);
	}

	DestroyFramebuffer(m_Scene);
	DestroyFramebuffer(m_Bright);
	DestroyFramebuffer(m_PingPong[0]);
	DestroyFramebuffer(m_PingPong[1]);
}

PostProcess::FrameBuffer PostProcess::CreateColorFramebuffer(int width, int height)
{
	FrameBuffer result;

	glGenTextures(1, &result.colorTexture);
	glBindTexture(GL_TEXTURE_2D, result.colorTexture);
	// RGBA16F so lighting/bloom can exceed 1.0 before tone mapping instead of clipping.
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glBindTexture(GL_TEXTURE_2D, 0);

	glGenFramebuffers(1, &result.fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, result.fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, result.colorTexture, 0);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		std::cout << "PostProcess framebuffer is not complete.\n";
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	return result;
}

void PostProcess::DestroyFramebuffer(FrameBuffer& target)
{
	if (target.colorTexture != 0)
	{
		glDeleteTextures(1, &target.colorTexture);
		target.colorTexture = 0;
	}
	if (target.fbo != 0)
	{
		glDeleteFramebuffers(1, &target.fbo);
		target.fbo = 0;
	}
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

void PostProcess::DrawFullscreenQuad(GLuint shader)
{
	glBindBuffer(GL_ARRAY_BUFFER, m_QuadVBO);

	int positionAttrib = glGetAttribLocation(shader, "a_Position");
	int uvAttrib = glGetAttribLocation(shader, "a_UV");

	glEnableVertexAttribArray(positionAttrib);
	glVertexAttribPointer(positionAttrib, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, (void*)0);

	glEnableVertexAttribArray(uvAttrib);
	glVertexAttribPointer(uvAttrib, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, (void*)(sizeof(float) * 2));

	glDrawArrays(GL_TRIANGLES, 0, 6);

	glDisableVertexAttribArray(positionAttrib);
	glDisableVertexAttribArray(uvAttrib);
}

void PostProcess::BeginCapture()
{
	glBindFramebuffer(GL_FRAMEBUFFER, m_Scene.fbo);
	glViewport(0, 0, m_Width, m_Height);
}

void PostProcess::EndCaptureAndPresent(float exposure, float vignetteStrength, float bloomThreshold, float bloomIntensity)
{
	// 1) Extract pixels brighter than the threshold into a half-res buffer.
	glBindFramebuffer(GL_FRAMEBUFFER, m_Bright.fbo);
	glViewport(0, 0, m_BlurWidth, m_BlurHeight);
	glUseProgram(m_BrightExtractShader);
	glUniform1f(glGetUniformLocation(m_BrightExtractShader, "u_Threshold"), bloomThreshold);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, m_Scene.colorTexture);
	glUniform1i(glGetUniformLocation(m_BrightExtractShader, "u_Scene"), 0);
	DrawFullscreenQuad(m_BrightExtractShader);

	// 2) Ping-pong separable Gaussian blur on the bright buffer.
	GLuint sourceTexture = m_Bright.colorTexture;
	bool horizontal = true;
	glUseProgram(m_BlurShader);
	for (int i = 0; i < kBlurPasses * 2; ++i)
	{
		FrameBuffer& target = m_PingPong[horizontal ? 0 : 1];
		glBindFramebuffer(GL_FRAMEBUFFER, target.fbo);
		glViewport(0, 0, m_BlurWidth, m_BlurHeight);

		glUniform2f(glGetUniformLocation(m_BlurShader, "u_TexelSize"), 1.f / m_BlurWidth, 1.f / m_BlurHeight);
		glUniform2f(glGetUniformLocation(m_BlurShader, "u_Direction"), horizontal ? 1.f : 0.f, horizontal ? 0.f : 1.f);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, sourceTexture);
		glUniform1i(glGetUniformLocation(m_BlurShader, "u_Scene"), 0);

		DrawFullscreenQuad(m_BlurShader);

		sourceTexture = target.colorTexture;
		horizontal = !horizontal;
	}

	// 3) Composite the sharp scene with the blurred bloom, then tone map + vignette to the screen.
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, m_Width, m_Height);
	glClear(GL_COLOR_BUFFER_BIT);

	glUseProgram(m_CompositeShader);
	glUniform1f(glGetUniformLocation(m_CompositeShader, "u_Exposure"), exposure);
	glUniform1f(glGetUniformLocation(m_CompositeShader, "u_VignetteStrength"), vignetteStrength);
	glUniform1f(glGetUniformLocation(m_CompositeShader, "u_BloomIntensity"), bloomIntensity);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, m_Scene.colorTexture);
	glUniform1i(glGetUniformLocation(m_CompositeShader, "u_Scene"), 0);

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, sourceTexture);
	glUniform1i(glGetUniformLocation(m_CompositeShader, "u_Bloom"), 1);

	DrawFullscreenQuad(m_CompositeShader);

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, 0);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, 0);
}
