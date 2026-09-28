#version 330

// Renderer의 렌더 큐(배치)용 정점 셰이더. 정점마다 CPU가 이미 MVP까지 곱해 둔 클립 좌표를 받으므로
// 여기서는 그대로 넘기기만 한다 — 그래서 월드 오브젝트든 화면 고정 UI든(행렬이 달라도) 한 버퍼에
// 모아 드로우 콜 한 번으로 그릴 수 있다.
// 위치를 0번 자리로 고정한 건 다른 셰이더들과 같은 규칙(셰이더가 달라도 VAO 설정을 재사용).
layout(location = 0) in vec4 a_Position; // 클립 좌표 (x,y,z,w)
layout(location = 1) in vec2 a_Local;    // 단위 사각형 내부 좌표(-0.5~0.5), 부드러운 원 모양에만 씀
layout(location = 2) in vec4 a_Color;
layout(location = 3) in float a_Shape;   // 0 = 단색, 1 = 가장자리로 갈수록 투명해지는 원

out vec2 v_Local;
out vec4 v_Color;
out float v_Shape;

void main()
{
	v_Local = a_Local;
	v_Color = a_Color;
	v_Shape = a_Shape;
	gl_Position = a_Position;
}
