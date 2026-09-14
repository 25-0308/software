#version 330

in vec2 v_Local;
layout(location=0) out vec4 FragColor;

uniform vec4 u_Color;
uniform float u_Time;
uniform float u_PhaseOffset;

void main()
{
	// 타일마다 위상이 달라 표면 전체가 동시에 반짝이지 않고 잔물결처럼 보인다.
	float wave = sin((v_Local.x + v_Local.y) * 6.0 + u_Time * 2.0 + u_PhaseOffset) * 0.06;
	float sparkle = pow(max(sin(v_Local.x * 20.0 + u_Time * 3.0 + u_PhaseOffset), 0.0), 8.0) * 0.15;

	vec3 color = u_Color.rgb + vec3(wave) + vec3(sparkle);
	FragColor = vec4(color, u_Color.a);
}
