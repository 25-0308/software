#include "stdafx.h"
#include "PostProcess.h"
#include "ShaderUtil.h"

#include <iostream>

namespace
{
	// 밝기 버퍼에 적용할 가로+세로 블러 쌍의 횟수. 클수록 더 넓고 부드러운
	// 발광 효과가 나오지만 드로우 콜이 늘어난다.
	const int kBlurPasses = 5;
}

PostProcess::PostProcess(int width, int height)
	: m_Width(width)
	, m_Height(height)
	, m_BlurWidth(width / 2)
	, m_BlurHeight(height / 2)
{
	m_Scene = CreateColorFramebuffer(m_Width, m_Height);
	// 블룸 버퍼는 절반 해상도로 처리한다: 비용이 싸고, 어차피 블러가 다운
	// 샘플링 흔적을 가려준다.
	m_Bright = CreateColorFramebuffer(m_BlurWidth, m_BlurHeight);
	m_PingPong[0] = CreateColorFramebuffer(m_BlurWidth, m_BlurHeight);
	m_PingPong[1] = CreateColorFramebuffer(m_BlurWidth, m_BlurHeight);
	m_Graded = CreateColorFramebuffer(m_Width, m_Height);

	CreateFullscreenQuad();

	m_BrightExtractShader = ShaderUtil::CompileShaderProgram("./Shaders/PostProcess.vs", "./Shaders/BrightExtract.fs");
	m_BlurShader = ShaderUtil::CompileShaderProgram("./Shaders/PostProcess.vs", "./Shaders/Blur.fs");
	m_CompositeShader = ShaderUtil::CompileShaderProgram("./Shaders/PostProcess.vs", "./Shaders/Composite.fs");
	m_GradeShader = ShaderUtil::CompileShaderProgram("./Shaders/PostProcess.vs", "./Shaders/Grade.fs");
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
	if (m_GradeShader != 0)
	{
		glDeleteProgram(m_GradeShader);
	}
	if (m_QuadVBO != 0)
	{
		glDeleteBuffers(1, &m_QuadVBO);
	}

	DestroyFramebuffer(m_Scene);
	DestroyFramebuffer(m_Bright);
	DestroyFramebuffer(m_PingPong[0]);
	DestroyFramebuffer(m_PingPong[1]);
	DestroyFramebuffer(m_Graded);
}

PostProcess::FrameBuffer PostProcess::CreateColorFramebuffer(int width, int height)
{
	FrameBuffer result;

	glGenTextures(1, &result.colorTexture);
	glBindTexture(GL_TEXTURE_2D, result.colorTexture);
	// 조명/블룸 값이 1.0을 넘어도 클리핑되지 않도록 RGBA16F 사용.
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
		std::cout << "PostProcess 프레임버퍼 생성에 실패했습니다.\n";
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

void PostProcess::EndCaptureAndPresent(float exposure, float vignetteStrength, float bloomThreshold, float bloomIntensity, float time)
{
	// 1) 임계값보다 밝은 픽셀만 절반 해상도 버퍼로 추출.
	glBindFramebuffer(GL_FRAMEBUFFER, m_Bright.fbo);
	glViewport(0, 0, m_BlurWidth, m_BlurHeight);
	glUseProgram(m_BrightExtractShader);
	glUniform1f(glGetUniformLocation(m_BrightExtractShader, "u_Threshold"), bloomThreshold);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, m_Scene.colorTexture);
	glUniform1i(glGetUniformLocation(m_BrightExtractShader, "u_Scene"), 0);
	DrawFullscreenQuad(m_BrightExtractShader);

	// 2) 밝기 버퍼에 핑퐁 방식으로 분리형 가우시안 블러 적용.
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

	// 3) 원본 씬 + 블러된 블룸을 합성하고, 톤매핑 + 비네트를 적용해 중간
	//    버퍼(m_Graded)에 기록한다. 화면에 바로 쓰지 않는 이유는 마지막
	//    색보정/필름 그레인 단계에서 이 결과를 다시 읽어야 하기 때문.
	glBindFramebuffer(GL_FRAMEBUFFER, m_Graded.fbo);
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

	// 4) 마지막 단계: 색보정 + 필름 그레인을 적용해 실제 화면에 출력.
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, m_Width, m_Height);
	glClear(GL_COLOR_BUFFER_BIT);

	glUseProgram(m_GradeShader);
	glUniform1f(glGetUniformLocation(m_GradeShader, "u_Time"), time);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, m_Graded.colorTexture);
	glUniform1i(glGetUniformLocation(m_GradeShader, "u_Scene"), 0);

	DrawFullscreenQuad(m_GradeShader);

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, 0);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, 0);
}
