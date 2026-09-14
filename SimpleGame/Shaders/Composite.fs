#version 330

in vec2 v_UV;
layout(location=0) out vec4 FragColor;

uniform sampler2D u_Scene;
uniform sampler2D u_Bloom;
uniform float u_Exposure;
uniform float u_VignetteStrength;
uniform float u_BloomIntensity;

void main()
{
	vec3 hdrColor = texture(u_Scene, v_UV).rgb;
	vec3 bloomColor = texture(u_Bloom, v_UV).rgb;

	hdrColor += bloomColor * u_BloomIntensity;

	// 노출 톤매핑: 밝은 부분(블룸 포함)이 그냥 하얗게 날아가지 않고 부드럽게 눌린다.
	vec3 mapped = vec3(1.0) - exp(-hdrColor * u_Exposure);

	// 디스플레이 공간으로 감마 보정.
	mapped = pow(mapped, vec3(1.0 / 2.2));

	// 비네트: 화면 중심에서 멀어질수록 어두워짐.
	vec2 centered = v_UV - vec2(0.5);
	float vignette = 1.0 - u_VignetteStrength * dot(centered, centered) * 2.0;
	mapped *= clamp(vignette, 0.0, 1.0);

	FragColor = vec4(mapped, 1.0);
}
