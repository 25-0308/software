#version 330

// 단위 사각형 + 내부 로컬 좌표를 넘기는 정점 셰이더. 지금은 횃불 불꽃(Fire.fs)이 쓴다(그림자는
// 렌더 큐의 ColorBatch 셰이더로 옮겨 갔지만, 파일 이름은 처음 쓰던 용도를 따라 그대로 둠).
layout(location = 0) in vec3 a_Position; // 다른 셰이더와 위치를 통일해 VAO를 공유(ColorBatch.vs 주석 참고)
uniform mat4 u_MVP;

out vec2 v_Local;

void main()
{
	v_Local = a_Position.xy; // -0.5 ~ 0.5 범위의 사각형 내부 로컬 좌표
	gl_Position = u_MVP * vec4(a_Position, 1.0);
}
