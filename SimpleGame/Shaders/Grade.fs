#version 330

in vec2 v_UV;
layout(location=0) out vec4 FragColor;

uniform sampler2D u_Scene;
uniform float u_Time;

float Hash(vec2 p)
{
	return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);
}

void main()
{
	vec3 color = texture(u_Scene, v_UV).rgb;

	// 색보정: 어두운 신화적 분위기를 위해 어두운 부분은 살짝 푸르게,
	// 밝은 부분은 살짝 따뜻하게 물들인다.
	vec3 shadowTint = vec3(0.92, 0.95, 1.05);
	vec3 highlightTint = vec3(1.06, 1.0, 0.90);
	float luminance = dot(color, vec3(0.2126, 0.7152, 0.0722));
	color *= mix(shadowTint, highlightTint, clamp(luminance, 0.0, 1.0));

	// 필름 그레인: 미세한 노이즈로 분위기를 더함.
	float grain = (Hash(v_UV * (u_Time + 1.0)) - 0.5) * 0.035;
	color += grain;

	FragColor = vec4(color, 1.0);
}
