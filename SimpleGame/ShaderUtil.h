#pragma once

#include "Dependencies\glew.h"

namespace ShaderUtil
{
	// Compiles and links a vertex/fragment shader pair loaded from files.
	// Returns 0 (an invalid program) on any failure.
	GLuint CompileShaderProgram(const char* filenameVS, const char* filenameFS);
}
