#version 330

in vec2 v_Local;
in vec4 v_Color;
in float v_Shape;
layout(location=0) out vec4 FragColor;

void main()
{
	float alpha = v_Color.a;

	// 부드러운 원(그림자, 불빛, 빛 알갱이): 사각형 중심에서 멀어질수록 부드럽게 사라진다.
	if (v_Shape > 0.5)
	{
		float dist = length(v_Local) * 2.0;
		alpha *= 1.0 - smoothstep(0.4, 1.0, dist);
	}

	FragColor = vec4(v_Color.rgb, alpha);
}
