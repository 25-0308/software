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

	m_BrightExtract = CompileAndCache("./Shaders/PostProcess.vs", "./Shaders/BrightExtract.fs");
	m_Blur = CompileAndCache("./Shaders/PostProcess.vs", "./Shaders/Blur.fs");
	m_Composite = CompileAndCache("./Shaders/PostProcess.vs", "./Shaders/Composite.fs");
	m_Grade = CompileAndCache("./Shaders/PostProcess.vs", "./Shaders/Grade.fs");
}

PostProcess::~PostProcess()
{
	if (m_BrightExtract.program != 0) glDeleteProgram(m_BrightExtract.program);
	if (m_Blur.program != 0) glDeleteProgram(m_Blur.program);
	if (m_Composite.program != 0) glDeleteProgram(m_Composite.program);
	if (m_Grade.program != 0) glDeleteProgram(m_Grade.program);

	if (m_QuadVAO != 0)
	{
		glDeleteVertexArrays(1, &m_QuadVAO);
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

PostProcess::PassShader PostProcess::CompileAndCache(const char* filenameVS, const char* filenameFS)
{
	PassShader shader;
	shader.program = ShaderUtil::CompileShaderProgram(filenameVS, filenameFS);

	if (shader.program == 0)
	{
		return shader;
	}

	// 이 네 패스가 쓰는 유니폼을 합친 목록. 없는 이름은 -1로 남고 glUniform*(-1,...)은
	// 조용히 무시되므로, 패스마다 실제로 있는 것만 골라 조회할 필요가 없다(Renderer.cpp의
	// ShaderProgramInfo와 같은 방식).
	shader.uniformScene = glGetUniformLocation(shader.program, "u_Scene");
	shader.uniformBloom = glGetUniformLocation(shader.program, "u_Bloom");
	shader.uniformThreshold = glGetUniformLocation(shader.program, "u_Threshold");
	shader.uniformTexelSize = glGetUniformLocation(shader.program, "u_TexelSize");
	shader.uniformDirection = glGetUniformLocation(shader.program, "u_Direction");
	shader.uniformExposure = glGetUniformLocation(shader.program, "u_Exposure");
	shader.uniformVignetteStrength = glGetUniformLocation(shader.program, "u_VignetteStrength");
	shader.uniformBloomIntensity = glGetUniformLocation(shader.program, "u_BloomIntensity");
	shader.uniformTime = glGetUniformLocation(shader.program, "u_Time");
	return shader;
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

	// 네 패스 모두 a_Position=0, a_UV=1로 위치를 고정해 뒀으므로(Shaders/PostProcess.vs),
	// VAO 하나를 만들어 두고 패스마다(glUseProgram만 바뀔 뿐) 그대로 재사용한다.
	glGenVertexArrays(1, &m_QuadVAO);
	glBindVertexArray(m_QuadVAO);
	glBindBuffer(GL_ARRAY_BUFFER, m_QuadVBO);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, (void*)0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, (void*)(sizeof(float) * 2));
	glBindVertexArray(0);
}

void PostProcess::DrawFullscreenQuad()
{
	glBindVertexArray(m_QuadVAO);
	glDrawArrays(GL_TRIANGLES, 0, 6);
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
	glUseProgram(m_BrightExtract.program);
	glUniform1f(m_BrightExtract.uniformThreshold, bloomThreshold);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, m_Scene.colorTexture);
	glUniform1i(m_BrightExtract.uniformScene, 0);
	DrawFullscreenQuad();

	// 2) 밝기 버퍼에 핑퐁 방식으로 분리형 가우시안 블러 적용.
	GLuint sourceTexture = m_Bright.colorTexture;
	bool horizontal = true;
	glUseProgram(m_Blur.program);
	for (int i = 0; i < kBlurPasses * 2; ++i)
	{
		FrameBuffer& target = m_PingPong[horizontal ? 0 : 1];
		glBindFramebuffer(GL_FRAMEBUFFER, target.fbo);
		glViewport(0, 0, m_BlurWidth, m_BlurHeight);

		glUniform2f(m_Blur.uniformTexelSize, 1.f / m_BlurWidth, 1.f / m_BlurHeight);
		glUniform2f(m_Blur.uniformDirection, horizontal ? 1.f : 0.f, horizontal ? 0.f : 1.f);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, sourceTexture);
		glUniform1i(m_Blur.uniformScene, 0);

		// glUseProgram은 루프 밖에서 이미 한 번 해뒀다(패스 내내 같은 셰이더).
		DrawFullscreenQuad();

		sourceTexture = target.colorTexture;
		horizontal = !horizontal;
	}

	// 3) 원본 씬 + 블러된 블룸을 합성하고, 톤매핑 + 비네트를 적용해 중간
	//    버퍼(m_Graded)에 기록한다. 화면에 바로 쓰지 않는 이유는 마지막
	//    색보정/필름 그레인 단계에서 이 결과를 다시 읽어야 하기 때문.
	glBindFramebuffer(GL_FRAMEBUFFER, m_Graded.fbo);
	glViewport(0, 0, m_Width, m_Height);
	glClear(GL_COLOR_BUFFER_BIT);

	glUseProgram(m_Composite.program);
	glUniform1f(m_Composite.uniformExposure, exposure);
	glUniform1f(m_Composite.uniformVignetteStrength, vignetteStrength);
	glUniform1f(m_Composite.uniformBloomIntensity, bloomIntensity);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, m_Scene.colorTexture);
	glUniform1i(m_Composite.uniformScene, 0);

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, sourceTexture);
	glUniform1i(m_Composite.uniformBloom, 1);

	DrawFullscreenQuad();

	// 4) 마지막 단계: 색보정 + 필름 그레인을 적용해 실제 화면에 출력.
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, m_Width, m_Height);
	glClear(GL_COLOR_BUFFER_BIT);

	glUseProgram(m_Grade.program);
	glUniform1f(m_Grade.uniformTime, time);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, m_Graded.colorTexture);
	glUniform1i(m_Grade.uniformScene, 0);

	DrawFullscreenQuad();

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, 0);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, 0);
}
