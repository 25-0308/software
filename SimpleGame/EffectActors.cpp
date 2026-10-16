#include "stdafx.h"
#include "EffectActors.h"

#include <cmath>
#include <vector>

#include "Renderer.h"
#include "Shapes.h"

namespace
{
	const float kPi = 3.14159265f;

	// 화면 가로 방향(월드 (1,-1)/√2). 카메라를 향해 세운 벼락을 좌우로 꺾을 때 쓴다.
	const float kScreenAxisX = 0.7071f;
	const float kScreenAxisY = -0.7071f;

	const float kBoltTop = 7.f;      // 벼락이 시작하는 높이
	const int kBoltSegments = 7;

	RenderLayer LayerFor(SkillEffectStyle style)
	{
		// 벼락은 나무·건물·인물 위로 내리꽂혀야 하고, 나머지는 바닥에 퍼지는 표시라 인물이 그 위를 덮는다.
		return (style == SkillEffectStyle::Lightning) ? RenderLayer::Overlay : RenderLayer::Decal;
	}

	// 씨앗과 번호로 정해지는 -1~1 값(같은 효과는 매 프레임 같은 모양).
	float SeededSigned(unsigned int seed, int index)
	{
		unsigned int value = seed ^ (unsigned int)(index * 2654435761u);
		value ^= value >> 15;
		value *= 2246822519u;
		value ^= value >> 13;
		return (float)(value & 0xFFFFu) / 32767.5f - 1.f;
	}
}

SkillEffectActor::SkillEffectActor(SkillEffectStyle style, float x, float y, float radius, float duration,
	float r, float g, float b, float toX, float toY)
	: Actor(ActorType::Effect, LayerFor(style))
	, m_Style(style)
	, m_Radius(radius)
	, m_Duration((duration > 0.01f) ? duration : 0.01f)
	, m_ToX(toX)
	, m_ToY(toY)
{
	SetPosition(x, y, 0.f);
	SetColor(r, g, b);
	m_Seed = (unsigned int)((x + 100.f) * 4099.f) ^ (unsigned int)((y + 100.f) * 6151.f);

	if (style == SkillEffectStyle::Lightning)
	{
		SetBoundingSphere(kBoltTop * 0.6f + radius, kBoltTop * 0.5f);
	}
	else if (style == SkillEffectStyle::Gust)
	{
		// 자국의 가운데를 중심으로 길이의 절반 + 여유.
		float dx = toX - x;
		float dy = toY - y;
		SetBoundingSphere(sqrtf(dx * dx + dy * dy) + radius + 0.5f, 0.f);
	}
	else
	{
		SetBoundingSphere(radius + 0.6f, 0.3f);
	}
}

void SkillEffectActor::OnUpdate(const UpdateContext& ctx)
{
	m_Age += ctx.deltaSeconds;
	if (m_Age >= m_Duration)
	{
		Destroy();
	}
}

void SkillEffectActor::OnRender(const RenderContext& ctx)
{
	float t = m_Age / m_Duration;      // 0 → 1
	float fade = 1.f - t;
	float x = GetWorldX();
	float y = GetWorldY();
	float r = GetR();
	float g = GetG();
	float b = GetB();

	switch (m_Style)
	{
	case SkillEffectStyle::Lightning:
		RenderLightning(ctx, fade);
		break;

	case SkillEffectStyle::Wave:
		{
			// 바깥 고리가 먼저 퍼지고 안쪽 고리가 뒤따른다. 퍼질수록 옅어진다.
			float outer = m_Radius * (0.25f + 0.75f * t);
			float inner = m_Radius * (0.1f + 0.75f * t * t);
			RenderRing(ctx, outer, 0.32f, r, g, b, 0.75f * fade);
			RenderRing(ctx, inner, 0.18f, r * 1.3f, g * 1.3f, b * 1.3f, 0.6f * fade);
			Mat4 foam = Mat4::Translate(x, y, 0.02f) * Mat4::Scale(outer * 2.f, outer * 2.f, 1.f);
			ctx.renderer.DrawSoftDisc(ctx.viewProjection * foam, r, g, b, 0.25f * fade);
		}
		break;

	case SkillEffectStyle::Shadow:
		{
			float spread = m_Radius * (0.3f + 0.7f * sqrtf(t));
			Mat4 dark = Mat4::Translate(x, y, 0.02f) * Mat4::Scale(spread * 2.2f, spread * 2.2f, 1.f);
			ctx.renderer.DrawSoftDisc(ctx.viewProjection * dark, 0.08f, 0.02f, 0.12f, 0.7f * fade);
			RenderRing(ctx, spread, 0.22f, r, g, b, 0.7f * fade);
			RenderRing(ctx, spread * 0.55f, 0.12f, r * 1.4f, g * 1.4f, b * 1.4f, 0.5f * fade);
		}
		break;

	case SkillEffectStyle::Charm:
		{
			float spread = m_Radius * (0.2f + 0.8f * t);
			RenderRing(ctx, spread, 0.2f, r, g, b, 0.7f * fade);

			// 떠오르는 빛 방울: 고리 둘레에서 위로 천천히 오르며 사라진다.
			Shapes::Rgb sparkle = { r * 1.5f, g * 1.5f, b * 1.5f };
			for (int i = 0; i < 8; ++i)
			{
				float angle = (float)i * (2.f * kPi / 8.f) + SeededSigned(m_Seed, i) * 0.4f;
				float around = spread * (0.5f + 0.4f * (0.5f + 0.5f * SeededSigned(m_Seed, i + 20)));
				float sx = x + cosf(angle) * around;
				float sy = y + sinf(angle) * around;
				Shapes::DrawUprightEllipse(ctx, sx, sy, 0.4f + 1.6f * t, 0.14f, 0.14f, sparkle, 0.8f * fade);
			}
		}
		break;

	case SkillEffectStyle::Gust:
		{
			// 지나온 길을 따라 납작한 바람 자국이 이어지고, 앞쪽부터 먼저 사라진다.
			const int kMarks = 8;
			for (int i = 0; i < kMarks; ++i)
			{
				float k = (float)i / (float)(kMarks - 1);
				float alpha = fade * (0.35f + 0.45f * k);
				float mx = x + (m_ToX - x) * k;
				float my = y + (m_ToY - y) * k;
				float size = m_Radius * (0.6f + 0.4f * k);
				Mat4 mark = Mat4::Translate(mx, my, 0.02f) * Mat4::Scale(size, size, 1.f);
				ctx.renderer.DrawSoftDisc(ctx.viewProjection * mark, r, g, b, alpha);
			}
		}
		break;
	}
}

// 하늘에서 바닥까지 들쭉날쭉 꺾인 벼락. 화면 가로 방향으로 꺾고 폭을 줘서 늘 카메라를 향해 보인다.
// 바깥 빛(넓고 옅게) → 심지(가늘고 밝게, 1을 넘어 블룸으로 빛남) 순서로 그리고, 바닥엔 섬광이 번진다.
void SkillEffectActor::RenderLightning(const RenderContext& ctx, float fade)
{
	float x = GetWorldX();
	float y = GetWorldY();

	Shapes::Point3 points[kBoltSegments + 1];
	for (int i = 0; i <= kBoltSegments; ++i)
	{
		float k = (float)i / (float)kBoltSegments;
		float jag = (i == 0 || i == kBoltSegments) ? 0.f : SeededSigned(m_Seed, i) * 0.45f;
		points[i].x = x + kScreenAxisX * jag;
		points[i].y = y + kScreenAxisY * jag;
		points[i].z = kBoltTop * (1.f - k);
	}

	const float widths[2] = { 0.22f, 0.07f };
	const Shapes::Rgb colors[2] = { { 0.7f, 0.75f, 1.6f }, { 2.2f, 2.1f, 2.6f } };
	const float alphas[2] = { 0.45f, 1.f };

	for (int layer = 0; layer < 2; ++layer)
	{
		float half = widths[layer] * 0.5f;
		for (int i = 0; i < kBoltSegments; ++i)
		{
			const Shapes::Point3& top = points[i];
			const Shapes::Point3& bottom = points[i + 1];
			Shapes::Point3 p0 = { top.x - kScreenAxisX * half, top.y - kScreenAxisY * half, top.z };
			Shapes::Point3 p1 = { top.x + kScreenAxisX * half, top.y + kScreenAxisY * half, top.z };
			Shapes::Point3 p2 = { bottom.x + kScreenAxisX * half, bottom.y + kScreenAxisY * half, bottom.z };
			Shapes::Point3 p3 = { bottom.x - kScreenAxisX * half, bottom.y - kScreenAxisY * half, bottom.z };
			Shapes::DrawQuad(ctx, p0, p1, p2, p3, colors[layer], alphas[layer] * fade);
		}
	}

	float flash = 1.f + m_Radius;
	Mat4 ground = Mat4::Translate(x, y, 0.02f) * Mat4::Scale(flash * 2.f, flash * 2.f, 1.f);
	ctx.renderer.DrawSoftDisc(ctx.viewProjection * ground, 1.6f, 1.5f, 1.9f, 0.55f * fade);
}

// 바닥에 누운 고리(반지름 radius, 두께 width). 32조각의 사다리꼴을 삼각형 목록으로 만들어 한 번에 넘긴다.
void SkillEffectActor::RenderRing(const RenderContext& ctx, float radius, float width, float r, float g, float b, float a)
{
	if (radius <= 0.01f || a <= 0.f)
	{
		return;
	}

	const int kSegments = 32;
	float inner = (radius - width * 0.5f > 0.f) ? radius - width * 0.5f : 0.f;
	float outer = radius + width * 0.5f;
	float x = GetWorldX();
	float y = GetWorldY();

	std::vector<float> vertices;
	vertices.reserve(kSegments * 6 * 3);
	for (int i = 0; i < kSegments; ++i)
	{
		float a0 = (float)i * (2.f * kPi / (float)kSegments);
		float a1 = (float)(i + 1) * (2.f * kPi / (float)kSegments);
		float c0 = cosf(a0);
		float s0 = sinf(a0);
		float c1 = cosf(a1);
		float s1 = sinf(a1);

		const float quad[6][2] =
		{
			{ c0 * inner, s0 * inner }, { c0 * outer, s0 * outer }, { c1 * outer, s1 * outer },
			{ c0 * inner, s0 * inner }, { c1 * outer, s1 * outer }, { c1 * inner, s1 * inner },
		};
		for (int v = 0; v < 6; ++v)
		{
			vertices.push_back(x + quad[v][0]);
			vertices.push_back(y + quad[v][1]);
			vertices.push_back(0.03f);
		}
	}

	ctx.renderer.DrawTriangles(vertices.data(), (int)(vertices.size() / 3), ctx.viewProjection, r, g, b, a);
}
