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

	FrameBuffer CreateColorFramebuffer(int width, int height);
	void DestroyFramebuffer(FrameBuffer& target);
	void CreateFullscreenQuad();
	void DrawFullscreenQuad(GLuint shader);

	int m_Width;
	int m_Height;
	int m_BlurWidth;
	int m_BlurHeight;

	FrameBuffer m_Scene;
	FrameBuffer m_Bright;
	FrameBuffer m_PingPong[2];
	FrameBuffer m_Graded; // 톤매핑+비네트 합성 결과 (마지막 색보정 단계의 입력)

	GLuint m_QuadVBO = 0;
	GLuint m_BrightExtractShader = 0;
	GLuint m_BlurShader = 0;
	GLuint m_CompositeShader = 0;
	GLuint m_GradeShader = 0;
};
