#include "stdafx.h"
#include "ShaderUtil.h"

#include <cstring>
#include <fstream>
#include <iostream>
#include <string>

namespace
{
	bool ReadFile(const char* filename, std::string* target)
	{
		std::ifstream file(filename);
		if (file.fail())
		{
			std::cout << filename << " 파일을 불러오지 못했습니다.\n";
			file.close();
			return false;
		}
		std::string line;
		while (getline(file, line))
		{
			target->append(line.c_str());
			target->append("\n");
		}
		return true;
	}

	void AddShader(GLuint shaderProgram, const char* shaderText, GLenum shaderType)
	{
		GLuint shaderObj = glCreateShader(shaderType);

		if (shaderObj == 0)
		{
			fprintf(stderr, "셰이더 타입 %d 생성 실패\n", shaderType);
		}

		const GLchar* p[1];
		p[0] = shaderText;
		GLint lengths[1];
		lengths[0] = (GLint)strlen(shaderText);

		glShaderSource(shaderObj, 1, p, lengths);
		glCompileShader(shaderObj);

		GLint success = 0;
		glGetShaderiv(shaderObj, GL_COMPILE_STATUS, &success);
		if (!success)
		{
			GLchar infoLog[1024];
			glGetShaderInfoLog(shaderObj, sizeof(infoLog), NULL, infoLog);
			fprintf(stderr, "셰이더 타입 %d 컴파일 오류: '%s'\n", shaderType, infoLog);
			printf("%s \n", shaderText);
		}

		glAttachShader(shaderProgram, shaderObj);
	}
}

GLuint ShaderUtil::CompileShaderProgram(const char* filenameVS, const char* filenameFS)
{
	GLuint shaderProgram = glCreateProgram();

	if (shaderProgram == 0)
	{
		fprintf(stderr, "셰이더 프로그램 생성 실패\n");
	}

	std::string vs, fs;

	if (!ReadFile(filenameVS, &vs))
	{
		printf("버텍스 셰이더 컴파일 실패\n");
		return 0;
	}

	if (!ReadFile(filenameFS, &fs))
	{
		printf("프래그먼트 셰이더 컴파일 실패\n");
		return 0;
	}

	AddShader(shaderProgram, vs.c_str(), GL_VERTEX_SHADER);
	AddShader(shaderProgram, fs.c_str(), GL_FRAGMENT_SHADER);

	GLint success = 0;
	GLchar errorLog[1024] = { 0 };

	glLinkProgram(shaderProgram);
	glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
	if (success == 0)
	{
		glGetProgramInfoLog(shaderProgram, sizeof(errorLog), NULL, errorLog);
		std::cout << filenameVS << ", " << filenameFS << " 셰이더 프로그램 링크 오류\n" << errorLog;
		return 0;
	}

	glValidateProgram(shaderProgram);
	glGetProgramiv(shaderProgram, GL_VALIDATE_STATUS, &success);
	if (!success)
	{
		glGetProgramInfoLog(shaderProgram, sizeof(errorLog), NULL, errorLog);
		std::cout << filenameVS << ", " << filenameFS << " 셰이더 프로그램 검증 오류\n" << errorLog;
		return 0;
	}

	glUseProgram(shaderProgram);
	std::cout << filenameVS << ", " << filenameFS << " 셰이더 컴파일 완료.\n";

	return shaderProgram;
}
