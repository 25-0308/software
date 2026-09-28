#version 330

// 호수·바다 배치. 색(깊은 물 ~ 얕은 물)은 LevelBuilder가 정점마다 구워 두고, 여기서는 월드 좌표 기준
// 물결·반짝임·물가 거품·구름 그림자를 더한다. 예전엔 타일마다 물결 위상이 달라서 타일 경계마다
// 물결이 끊겨 격자가 보였는데, 이제는 월드 좌표로 계산해서 호수와 바다 전체가 이어진 수면으로 보인다.
in vec3 v_WorldPos;
in vec2 v_Extra; // x: 물가 정도(0 = 깊은 물, 땅에 닿은 모서리일수록 커짐, 최대 약 0.75)
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

// 구름 그림자(TileBatchSolid.fs와 같은 식).
float CloudShadow(vec2 p, float time)
{
	vec2 drift = vec2(0.55, 0.3) * time;
	float n = Noise((p + drift) * 0.07) * 0.65 + Noise((p + drift * 1.4) * 0.16) * 0.35;
	return smoothstep(0.5, 0.72, n);
}

void main()
{
	vec2 p = v_WorldPos.xy;
	float shore = v_Extra.x;

	// 방향이 다른 물결 두 개 + 느린 노이즈를 겹쳐서 규칙적인 줄무늬처럼 보이지 않게 한다.
	float wave = sin(dot(p, vec2(1.3, 0.9)) * 2.2 + u_Time * 1.6) * 0.5
		+ sin(dot(p, vec2(-0.7, 1.4)) * 3.1 - u_Time * 2.1) * 0.3
		+ (Noise(p * 1.5 + vec2(u_Time * 0.25, 0.0)) - 0.5) * 0.4;
	vec3 color = v_Color.rgb * (1.0 + wave * 0.1);

	// 반짝임: 움직이는 노이즈의 봉우리에서만 짧게 번쩍인다(밝기가 1을 넘어서 블룸으로 빛남).
	float glint = Noise(p * 3.7 + vec2(u_Time * 0.6, -u_Time * 0.4));
	color += vec3(pow(max(glint - 0.78, 0.0) * 4.5, 3.0));

	// 물가 거품: 물가에 가까울수록 출렁이는 흰 띠.
	float foamLine = shore + 0.07 * sin(u_Time * 1.8 + dot(p, vec2(2.1, 1.7))) + (Noise(p * 5.0) - 0.5) * 0.12;
	float foam = smoothstep(0.33, 0.45, foamLine);
	color = mix(color, vec3(0.9, 0.95, 0.97), foam * 0.65);

	color *= 1.0 - 0.2 * CloudShadow(p, u_Time);
	FragColor = vec4(color, v_Color.a);
}
