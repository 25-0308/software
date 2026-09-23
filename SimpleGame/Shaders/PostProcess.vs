#version 330

// 위치를 고정해서 VAO를 한 번만 설정하고 재사용한다(SolidRect.vs 주석 참고).
layout(location = 0) in vec2 a_Position;
layout(location = 1) in vec2 a_UV;

out vec2 v_UV;

void main()
{
	v_UV = a_UV;
	gl_Position = vec4(a_Position, 0.0, 1.0);
}
