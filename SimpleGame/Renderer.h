#pragma once

#include "Dependencies\glew.h"
#include "Math3D.h"

class Renderer
{
public:
	Renderer(int windowSizeX, int windowSizeY);
	~Renderer();

	bool IsInitialized();
	void DrawObject(const Mat4& mvp, float r, float g, float b, float a);

private:
	void Initialize(int windowSizeX, int windowSizeY);
	void CreateVertexBufferObjects();

	bool m_Initialized = false;

	GLuint m_VBORect = 0;
	GLuint m_SolidRectShader = 0;
};

