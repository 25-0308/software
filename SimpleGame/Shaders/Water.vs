#version 330

in vec3 a_Position;
uniform mat4 u_MVP;

out vec2 v_Local;

void main()
{
	v_Local = a_Position.xy;
	gl_Position = u_MVP * vec4(a_Position, 1.0);
}
