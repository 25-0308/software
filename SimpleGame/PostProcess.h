#pragma once

#include "Dependencies\glew.h"

// 씬을 오프스크린 HDR(RGBA16F) 버퍼로 렌더링한 뒤, 밝은 영역을 추출/블러해서
// 블룸 효과를 만들고, 톤매핑+비네트로 합성한 다음, 마지막으로 색보정+필름
// 그레인 단계를 거쳐 화면에 출력한다. 블룸 덕분에 스스로 빛나는 오브젝트(예:
// 빛나는 NPC, 퀘스트 아이템)가 밋밋한 밝은 색이 아니라 실제로 빛나는 것처럼
// 보인다.
class PostProcess
{
public:
	PostProcess(int width, int height);
	~PostProcess();

	// 씬을 그리기 전에 호출: 렌더링 대상을 HDR 버퍼로 돌린다.
	void BeginCapture();

	// 씬을 다 그린 후 호출: 밝은 영역을 추출/블러(블룸)하고, 톤매핑(exposure)
	// + 비네트(vignetteStrength)로 합성한 뒤, 색보정/필름 그레인(time) 단계를
	// 거쳐 화면에 출력한다.
	void EndCaptureAndPresent(float exposure, float vignetteStrength, float bloomThreshold, float bloomIntensity, float time);

private:
	struct FrameBuffer
	{
		GLuint fbo = 0;
		GLuint colorTexture = 0;
	};

	// 풀스크린 패스 셰이더 하나 + 그 유니폼 위치들을 캐시해 둔 것. Renderer.h의
	// ShaderProgramInfo와 같은 이유(매 프레임 13번 도는 패스에서 glGetUniformLocation을
	// 반복 조회하지 않기 위함)로 존재한다.
	struct PassShader
	{
		GLuint program = 0;
		GLint uniformScene = -1;
		GLint uniformBloom = -1;
		GLint uniformThreshold = -1;
		GLint uniformTexelSize = -1;
		GLint uniformDirection = -1;
		GLint uniformExposure = -1;
		GLint uniformVignetteStrength = -1;
		GLint uniformBloomIntensity = -1;
		GLint uniformTime = -1;
	};

	FrameBuffer CreateColorFramebuffer(int width, int height);
	void DestroyFramebuffer(FrameBuffer& target);
	void CreateFullscreenQuad();
	PassShader CompileAndCache(const char* filenameVS, const char* filenameFS);

	// 어떤 프로그램을 쓸지는 호출하는 쪽이 이미 glUseProgram + 유니폼 설정까지 끝낸
	// 뒤라고 가정하고, 이 함수는 공용 VAO를 바인드해서 그리기만 한다.
	void DrawFullscreenQuad();

	int m_Width;
	int m_Height;
	int m_BlurWidth;
	int m_BlurHeight;

	FrameBuffer m_Scene;
	FrameBuffer m_Bright;
	FrameBuffer m_PingPong[2];
	FrameBuffer m_Graded; // 톤매핑+비네트 합성 결과 (마지막 색보정 단계의 입력)

	GLuint m_QuadVBO = 0;
	GLuint m_QuadVAO = 0; // a_Position=0, a_UV=1로 한 번만 설정해 두고 재사용(Shaders/PostProcess.vs 참고)

	PassShader m_BrightExtract;
	PassShader m_Blur;
	PassShader m_Composite;
	PassShader m_Grade;
};
