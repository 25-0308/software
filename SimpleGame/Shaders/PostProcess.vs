#version 330

in vec2 a_Position;
in vec2 a_UV;

out vec2 v_UV;

void main()
{
	v_UV = a_UV;
	gl_Position = vec4(a_Position, 0.0, 1.0);
}
