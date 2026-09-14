#version 330

in vec3 a_Position;
uniform mat4 u_MVP;

out vec2 v_Local;

void main()
{
	v_Local = a_Position.xy; // -0.5 ~ 0.5 범위의 사각형 내부 로컬 좌표
	gl_Position = u_MVP * vec4(a_Position, 1.0);
}
