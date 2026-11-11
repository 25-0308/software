#version 330

// 3D 모델의 면 색. 도형으로 그리던 것(Shapes.h — 윗면 1.0 / +y면 0.82 / +x면 0.62)과 같은 빛이 들도록, 법선의
// 축 성분 제곱을 가중치로 면마다의 밝기를 섞는다(축과 나란한 면은 도형과 똑같고, 기운 면은 그 사이로 부드럽게).
// 아래·뒤쪽(-z, -x, -y)은 고정 카메라에선 잘 안 보이지만 어둡게. 발광 재질(빛나는 눈·불씨 등)은 1을 넘는 밝기로
// 더해 블룸으로 빛난다.
in vec3 v_Normal;
in vec3 v_Color;
in float v_Emissive;
flat in int v_Role;
layout(location = 0) out vec4 FragColor;

uniform vec4 u_Tint;           // rgb: 전체 색에 곱함, a: 불투명도
uniform vec4 u_Flash;          // rgb: 피격 플래시 색, a: 세기
uniform vec4 u_RoleColors[9];  // 역할마다 바꿔 칠할 색(a > 0이면 바꿈)

void main()
{
	vec3 base = v_Color;
	if (v_Role > 0 && v_Role < 9 && u_RoleColors[v_Role].a > 0.0)
	{
		// 재질이 면마다 단색이라 역할 하나 안의 정점은 모두 같은 색 — 그 색을 통째로 바꾼다.
		base = u_RoleColors[v_Role].rgb;
	}

	vec3 n = normalize(v_Normal);
	if (!gl_FrontFacing)
	{
		n = -n; // 양면 재질의 뒷면
	}

	vec3 n2 = n * n;
	float shade = n2.z * (n.z > 0.0 ? 1.0 : 0.45)
		+ n2.y * (n.y > 0.0 ? 0.82 : 0.62)
		+ n2.x * (n.x > 0.0 ? 0.62 : 0.55);

	vec3 color = base * shade * u_Tint.rgb + base * v_Emissive;
	color = mix(color, u_Flash.rgb, u_Flash.a);
	FragColor = vec4(color, u_Tint.a);
}
