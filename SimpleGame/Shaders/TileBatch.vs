#version 330

// 청크 하나(예: 8x8칸)의 타일, 또는 섬 둘레의 바다를 정점 색상 메시 하나로 구워서 한 번의
// 드로우콜로 그리기 위한 정점 형식. a_Position은 미리 월드 좌표로 구워 두므로(오브젝트별 모델
// 행렬 없이) u_MVP에 카메라의 view-projection만 넣으면 된다.
layout(location = 0) in vec3 a_Position; // 월드 좌표
layout(location = 1) in vec2 a_Extra;    // 지형: y = 재질 번호 / 물: x = 물가 정도 (각 .fs 참고)
layout(location = 2) in vec4 a_Color;

uniform mat4 u_MVP;

out vec3 v_WorldPos;
out vec2 v_Extra;
out vec4 v_Color;

void main()
{
	v_WorldPos = a_Position;
	v_Extra = a_Extra;
	v_Color = a_Color;
	gl_Position = u_MVP * vec4(a_Position, 1.0);
}
