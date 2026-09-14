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
	float dist = length(v_Local) * 2.0;
	float falloff = 1.0 - smoothstep(0.3, 1.0, dist);

	// 살짝 떨리는 밝기로 불꽃이 일렁이는 느낌을 낸다.
	float flicker = 0.85 + 0.15 * sin(u_Time * 12.0 + u_PhaseOffset)
		+ 0.1 * Hash(floor(u_Time * 20.0 + u_PhaseOffset));

	vec3 core = vec3(1.0, 0.85, 0.3);
	vec3 outer = vec3(1.0, 0.35, 0.05);
	vec3 color = mix(outer, core, falloff) * flicker;

	FragColor = vec4(color, falloff);
}
