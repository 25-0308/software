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
			std::cout << filename << " file loading failed.. \n";
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
			fprintf(stderr, "Error creating shader type %d\n", shaderType);
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
			fprintf(stderr, "Error compiling shader type %d: '%s'\n", shaderType, infoLog);
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
		fprintf(stderr, "Error creating shader program\n");
	}

	std::string vs, fs;

	if (!ReadFile(filenameVS, &vs))
	{
		printf("Error compiling vertex shader\n");
		return 0;
	}

	if (!ReadFile(filenameFS, &fs))
	{
		printf("Error compiling fragment shader\n");
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
		std::cout << filenameVS << ", " << filenameFS << " Error linking shader program\n" << errorLog;
		return 0;
	}

	glValidateProgram(shaderProgram);
	glGetProgramiv(shaderProgram, GL_VALIDATE_STATUS, &success);
	if (!success)
	{
		glGetProgramInfoLog(shaderProgram, sizeof(errorLog), NULL, errorLog);
		std::cout << filenameVS << ", " << filenameFS << " Error validating shader program\n" << errorLog;
		return 0;
	}

	glUseProgram(shaderProgram);
	std::cout << filenameVS << ", " << filenameFS << " Shader compiling is done.\n";

	return shaderProgram;
}
