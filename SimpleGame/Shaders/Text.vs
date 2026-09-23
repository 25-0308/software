#version 330

layout(location = 0) in vec3 a_Position; // 다른 셰이더와 위치를 통일해 VAO를 공유(SolidRect.vs 주석 참고)
uniform mat4 u_MVP;

out vec2 v_UV;

void main()
{
	// 단위 사각형(-0.5 ~ 0.5)의 로컬 좌표를 텍스처 좌표로 쓴다. 텍스처의 첫 번째 줄이 그림의
	// 맨 위 줄이므로, 사각형 위쪽(y=+0.5)이 v=0이 되도록 세로를 뒤집는다.
	v_UV = vec2(a_Position.x + 0.5, 0.5 - a_Position.y);
	gl_Position = u_MVP * vec4(a_Position, 1.0);
}
