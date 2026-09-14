#version 330

in vec2 v_UV;
layout(location=0) out vec4 FragColor;

uniform sampler2D u_Scene;
uniform vec2 u_TexelSize;
uniform vec2 u_Direction;

void main()
{
	// 9-tap separable Gaussian; weights sum to 1.0. Called once for the
	// horizontal pass and once for the vertical pass (see u_Direction).
	float weights[5] = float[](0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);

	vec3 result = texture(u_Scene, v_UV).rgb * weights[0];

	for (int i = 1; i < 5; ++i)
	{
		vec2 offset = u_Direction * u_TexelSize * float(i);
		result += texture(u_Scene, v_UV + offset).rgb * weights[i];
		result += texture(u_Scene, v_UV - offset).rgb * weights[i];
	}

	FragColor = vec4(result, 1.0);
}
