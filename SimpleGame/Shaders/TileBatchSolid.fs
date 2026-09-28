#version 330

// 지형(잔디/돌바닥/흙길) 배치. 색은 LevelBuilder가 정점마다 구워 둔 값(주변 지형과 부드럽게 섞이고
// 물가는 모래색)을 쓰고, 여기서는 재질별 잔무늬와 천천히 흘러가는 구름 그림자만 더한다.
in vec3 v_WorldPos;
in vec2 v_Extra; // y: 재질 번호(0 잔디, 1 돌바닥, 2 흙길, 9 무늬 없음 — 꽃·풀 같은 장식)
in vec4 v_Color;
layout(location=0) out vec4 FragColor;

uniform float u_Time;

float Hash(vec2 p)
{
	return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

// 격자점마다의 난수를 부드럽게 보간한 값 노이즈(0~1).
float Noise(vec2 p)
{
	vec2 cell = floor(p);
	vec2 f = fract(p);
	vec2 u = f * f * (3.0 - 2.0 * f);

	float a = Hash(cell);
	float b = Hash(cell + vec2(1.0, 0.0));
	float c = Hash(cell + vec2(0.0, 1.0));
	float d = Hash(cell + vec2(1.0, 1.0));

	return mix(mix(a, b, u.x), mix(c, d, u.x), u.y);
}

// 바람에 천천히 흘러가는 구름 그림자(0 = 맑음, 1 = 가장 짙은 그늘). TileBatchWater.fs와 같은 식이라
// 땅과 물 위의 그늘이 끊기지 않고 이어진다.
float CloudShadow(vec2 p, float time)
{
	vec2 drift = vec2(0.55, 0.3) * time;
	float n = Noise((p + drift) * 0.07) * 0.65 + Noise((p + drift * 1.4) * 0.16) * 0.35;
	return smoothstep(0.5, 0.72, n);
}

void main()
{
	vec3 color = v_Color.rgb;
	vec2 p = v_WorldPos.xy;
	float material = v_Extra.y;

	if (material < 0.5)
	{
		// 잔디: 크기가 다른 얼룩 두 겹으로 풀밭의 결을 낸다.
		float mottle = Noise(p * 1.8) * 0.6 + Noise(p * 4.7) * 0.4;
		color *= 0.9 + 0.2 * mottle;
	}
	else if (material < 1.5)
	{
		// 돌바닥: 한 칸을 2x2 판석으로 나눠서, 판석마다 밝기를 조금씩 달리하고 사이 줄눈을 어둡게.
		vec2 slab = p * 2.0;
		vec2 inSlab = fract(slab);
		float edge = min(min(inSlab.x, 1.0 - inSlab.x), min(inSlab.y, 1.0 - inSlab.y));
		float groove = 1.0 - smoothstep(0.02, 0.08, edge);
		float tone = Hash(floor(slab));
		color *= (0.9 + 0.16 * tone) * (1.0 - 0.3 * groove);
	}
	else if (material < 2.5)
	{
		// 흙길: 굵은 얼룩 + 드문드문 밝은 자갈 알갱이.
		color *= 0.88 + 0.22 * Noise(p * 2.2);
		float grit = Noise(p * 7.0);
		color *= 1.0 + 0.25 * smoothstep(0.78, 0.9, grit);
	}

	color *= 1.0 - 0.2 * CloudShadow(p, u_Time);
	FragColor = vec4(color, v_Color.a);
}
