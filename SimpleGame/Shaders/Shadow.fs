#version 330

in vec2 v_Local;
layout(location=0) out vec4 FragColor;

uniform float u_Opacity;

void main()
{
	// 사각형 중심에서 멀어질수록 부드럽게 사라지는 원형 그림자(블롭 섀도우).
	float dist = length(v_Local) * 2.0;
	float falloff = 1.0 - smoothstep(0.4, 1.0, dist);
	FragColor = vec4(0.0, 0.0, 0.0, falloff * u_Opacity);
}
