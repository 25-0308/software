#version 330

in vec2 v_UV;
layout(location=0) out vec4 FragColor;

uniform sampler2D u_Scene;
uniform float u_Threshold;

void main()
{
	vec3 color = texture(u_Scene, v_UV).rgb;
	float luminance = dot(color, vec3(0.2126, 0.7152, 0.0722));

	if (luminance > u_Threshold)
	{
		FragColor = vec4(color, 1.0);
	}
	else
	{
		FragColor = vec4(0.0, 0.0, 0.0, 1.0);
	}
}
