#version 330

layout(location=0) out vec4 FragColor;

in vec2 v_UV;

uniform sampler2D u_Texture;
uniform vec4 u_Color;

void main()
{
	// 텍스처의 알파 채널이 글자가 칠해진 정도(안티앨리어싱된 커버리지)다. 색은 유니폼으로
	// 정하고, 커버리지만큼만 불투명하게 그린다.
	float coverage = texture(u_Texture, v_UV).a;
	FragColor = vec4(u_Color.rgb, u_Color.a * coverage);
}
