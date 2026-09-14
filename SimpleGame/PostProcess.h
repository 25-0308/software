#pragma once

#include "Dependencies\glew.h"

// Renders the scene into an off-screen HDR (RGBA16F) buffer, then resolves it
// to the screen through a full-screen pass that applies exposure tone mapping
// and a vignette. This is how the "brighter highlights instead of clipping"
// and "darker screen edges" mood effects are produced.
class PostProcess
{
public:
	PostProcess(int width, int height);
	~PostProcess();

	// Call before drawing the scene: redirects rendering into the HDR buffer.
	void BeginCapture();

	// Call after the scene has been drawn: presents the HDR buffer to the
	// screen with tone mapping (controlled by `exposure`) and a vignette
	// (controlled by `vignetteStrength`, 0 = off).
	void EndCaptureAndPresent(float exposure, float vignetteStrength);

private:
	void CreateFramebuffer(int width, int height);
	void CreateFullscreenQuad();

	int m_Width;
	int m_Height;

	GLuint m_FBO = 0;
	GLuint m_ColorTexture = 0;

	GLuint m_QuadVBO = 0;
	GLuint m_Shader = 0;
};
