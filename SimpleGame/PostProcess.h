#pragma once

#include "Dependencies\glew.h"

// Renders the scene into an off-screen HDR (RGBA16F) buffer, extracts and
// blurs the bright areas (bloom), then resolves everything to the screen
// through a final pass that applies tone mapping and a vignette. Bloom is
// what makes a self-lit object (e.g. a light source) read as glowing rather
// than just a flat bright color.
class PostProcess
{
public:
	PostProcess(int width, int height);
	~PostProcess();

	// Call before drawing the scene: redirects rendering into the HDR buffer.
	void BeginCapture();

	// Call after the scene has been drawn: extracts+blurs bright areas
	// (bloomThreshold = minimum luminance to bloom, bloomIntensity = how much
	// of the blurred glow gets added back in) and presents the result to the
	// screen with tone mapping (exposure) and a vignette (vignetteStrength).
	void EndCaptureAndPresent(float exposure, float vignetteStrength, float bloomThreshold, float bloomIntensity);

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

	GLuint m_QuadVBO = 0;
	GLuint m_BrightExtractShader = 0;
	GLuint m_BlurShader = 0;
	GLuint m_CompositeShader = 0;
};
