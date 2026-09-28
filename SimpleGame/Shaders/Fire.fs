#version 330

in vec2 v_Local;
layout(location=0) out vec4 FragColor;

uniform float u_Time;
uniform float u_PhaseOffset;

float Hash(float n)
{
	return fract(sin(n) * 43758.5453);
}

void main()
{
	// 카메라를 향해 세운 사각형(로컬 y = 위쪽)에 그린다. 위로 갈수록 가로를 조이고 좌우로 살랑이게
	// 해서, 둥근 빛덩이가 아니라 위로 타오르는 불꽃 모양을 만든다.
	vec2 q = v_Local;
	float height01 = q.y + 0.5; // 0(아래) ~ 1(위)
	q.x -= sin(u_Time * 7.0 + u_PhaseOffset + q.y * 6.0) * 0.07 * height01;
	q.x *= 1.0 + height01 * 1.2;
	q.y *= 0.9;

	float dist = length(q) * 2.0;
	float falloff = 1.0 - smoothstep(0.3, 1.0, dist);

	// 살짝 떨리는 밝기로 불꽃이 일렁이는 느낌을 낸다.
	float flicker = 0.85 + 0.15 * sin(u_Time * 12.0 + u_PhaseOffset)
		+ 0.1 * Hash(floor(u_Time * 20.0 + u_PhaseOffset));

	vec3 core = vec3(1.0, 0.85, 0.3);
	vec3 outer = vec3(1.0, 0.35, 0.05);
	vec3 color = mix(outer, core, falloff) * flicker;

	FragColor = vec4(color, falloff);
}
