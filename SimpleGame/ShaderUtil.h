#pragma once

#include "Dependencies\glew.h"

namespace ShaderUtil
{
	// 파일에서 읽은 vertex/fragment 셰이더 쌍을 컴파일하고 링크한다.
	// 실패 시 0(유효하지 않은 프로그램)을 반환한다.
	GLuint CompileShaderProgram(const char* filenameVS, const char* filenameFS);
}
