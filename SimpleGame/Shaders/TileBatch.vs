#version 330

// 청크 하나(예: 8x8칸)의 타일들을 정점 색상을 입힌 메시 하나로 구워서 한 번의
// 드로우콜로 그리기 위한 정점 형식. a_Position은 미리 월드 좌표로 구워 두므로
// (오브젝트별 모델 행렬 없이) u_MVP에 카메라의 view-projection만 곱하면 된다.
layout(location = 0) in vec3 a_Position; // 월드 좌표
layout(location = 1) in vec2 a_Local;    // 타일 내부 로컬 좌표(-0.5~0.5), 물결 계산에만 씀
layout(location = 2) in vec4 a_Color;

uniform mat4 u_MVP;

out vec2 v_Local;
out vec4 v_Color;
out vec2 v_TileCenter; // = a_Position.xy - a_Local (이 정점이 속한 타일의 월드 중심)

void main()
{
	v_Local = a_Local;
	v_Color = a_Color;
	v_TileCenter = a_Position.xy - a_Local;
	gl_Position = u_MVP * vec4(a_Position, 1.0);
}
