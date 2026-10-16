#version 330

layout(location=0) out vec4 FragColor;

in vec2 v_UV;

uniform sampler2D u_Texture;
uniform vec4 u_Color;

void main()
{
	// 텍스처 색 × 유니폼 색. 글자 텍스처는 색이 흰색이고 알파가 글자가 칠해진 정도(안티앨리어싱된
	// 커버리지)라서 유니폼 색 그대로 커버리지만큼 그려지고, 초상화처럼 색이 있는 그림은 유니폼 색을
	// 곱한 만큼(듣는 쪽 초상화는 어둡게) 그려진다.
	vec4 texel = texture(u_Texture, v_UV);
	FragColor = vec4(u_Color.rgb * texel.rgb, u_Color.a * texel.a);
}
