#version 330

// 위치를 0번 자리로 고정해서, 셰이더가 달라도 같은 VAO 설정(위치 0 = 정점 버퍼)을
// 그대로 재사용할 수 있게 한다(매 드로우콜마다 glGetAttribLocation을 다시 조회할 필요가 없어짐).
layout(location = 0) in vec3 a_Position;
uniform mat4 u_MVP;

void main()
{
	gl_Position = u_MVP * vec4(a_Position, 1.0);
}
