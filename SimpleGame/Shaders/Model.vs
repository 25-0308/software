#version 330

// Resource 폴더의 3D 모델(glTF — Model.h). 위치·법선은 게임 축(Z 위)으로 이미 돌려 두었고, 스킨 모델은 정점마다
// 뼈 넷의 행렬을 가중치로 섞어 움직인다(스킨 없는 모델은 0번 뼈 = 단위 행렬 하나에 매달려 있다).
layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec3 a_Color;
layout(location = 3) in float a_Emissive;
layout(location = 4) in float a_Role;
layout(location = 5) in vec4 a_Joints;
layout(location = 6) in vec4 a_Weights;

uniform mat4 u_MVP;          // view-projection × 모델
uniform mat4 u_Model;        // 모델(법선을 월드로 돌릴 때)
uniform mat4 u_Joints[40];   // 뼈 행렬(Renderer::kModelMaxJoints)
uniform vec2 u_DepthRemap;    // (모델 원점의 클립 z, 배율) — 모델의 깊이를 제 크기만큼으로 펴서 좁은 깊이 구간을 다 쓴다

out vec3 v_Normal;
out vec3 v_Color;
out float v_Emissive;
flat out int v_Role;

void main()
{
	mat4 skin = a_Weights.x * u_Joints[int(a_Joints.x)]
		+ a_Weights.y * u_Joints[int(a_Joints.y)]
		+ a_Weights.z * u_Joints[int(a_Joints.z)]
		+ a_Weights.w * u_Joints[int(a_Joints.w)];

	vec4 local = skin * vec4(a_Position, 1.0);
	vec3 normal = mat3(skin) * a_Normal;

	v_Normal = mat3(u_Model) * normal;
	v_Color = a_Color;
	v_Emissive = a_Emissive;
	v_Role = int(a_Role + 0.5);
	gl_Position = u_MVP * local;

	// 모델 하나의 깊이를 [-1, 1]로 편다(넘치면 잘리지 않게 붙잡음 — 그 점만 앞뒤가 뭉개짐).
	float depth = (gl_Position.z - u_DepthRemap.x * gl_Position.w) * u_DepthRemap.y;
	gl_Position.z = clamp(depth, -gl_Position.w, gl_Position.w);
}
