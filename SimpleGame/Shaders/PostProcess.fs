#version 330

in vec2 v_UV;
layout(location=0) out vec4 FragColor;

uniform sampler2D u_Scene;
uniform float u_Exposure;
uniform float u_VignetteStrength;

void main()
{
	vec3 hdrColor = texture(u_Scene, v_UV).rgb;

	// Exposure tone mapping so bright highlights roll off instead of clipping to white.
	vec3 mapped = vec3(1.0) - exp(-hdrColor * u_Exposure);

	// Gamma-correct back to display space.
	mapped = pow(mapped, vec3(1.0 / 2.2));

	// Vignette: darken toward the screen edges based on distance from center.
	vec2 centered = v_UV - vec2(0.5);
	float vignette = 1.0 - u_VignetteStrength * dot(centered, centered) * 2.0;
	mapped *= clamp(vignette, 0.0, 1.0);

	FragColor = vec4(mapped, 1.0);
}
