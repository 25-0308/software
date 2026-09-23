#version 330

in vec2 v_Local;
in vec4 v_Color;
in vec2 v_TileCenter;
layout(location=0) out vec4 FragColor;

uniform float u_Time;

void main()
{
	// Water.fs와 동일한 공식. 예전엔 CPU가 타일마다 phase = x*1.7 + y*2.3을 계산해서
	// 유니폼으로 넘겼지만(타일 1개 = 드로우콜 1개일 때나 가능한 방식), 지금은 청크 전체를
	// 한 번에 그리므로 정점이 자기 타일의 월드 중심(v_TileCenter)을 들고 있어서
	// 셰이더 안에서 타일별로 다시 계산한다 — 타일마다 물결 위상이 다른 시각 효과는 그대로다.
	float phase = v_TileCenter.x * 1.7 + v_TileCenter.y * 2.3;
	float wave = sin((v_Local.x + v_Local.y) * 6.0 + u_Time * 2.0 + phase) * 0.06;
	float sparkle = pow(max(sin(v_Local.x * 20.0 + u_Time * 3.0 + phase), 0.0), 8.0) * 0.15;

	vec3 color = v_Color.rgb + vec3(wave) + vec3(sparkle);
	FragColor = vec4(color, v_Color.a);
}
