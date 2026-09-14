#version 330

in vec2 v_UV;
layout(location=0) out vec4 FragColor;

uniform sampler2D u_Scene;
uniform vec2 u_TexelSize;
uniform vec2 u_Direction;

void main()
{
	// 9탭 분리형 가우시안. 가중치 합은 1.0. 가로 패스와 세로 패스에 한 번씩
	// 호출된다 (u_Direction 참고).
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
