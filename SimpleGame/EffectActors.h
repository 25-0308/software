#pragma once

#include "Actor.h"

// 수호신 스킬이 터질 때 잠깐 떴다 사라지는 시각 효과의 모양.
enum class SkillEffectStyle
{
	Lightning, // 하늘에서 내리꽂는 들쭉날쭉한 벼락 + 바닥의 섬광(제우스) — 모든 것 위에 그린다
	Wave,      // 바닥을 따라 퍼져 나가는 물결 고리 두 겹(포세이돈)
	Shadow,    // 바닥에서 번져 나가는 보랏빛 어둠과 안쪽 고리(하데스)
	Charm,     // 퍼져 나가는 분홍 고리와 떠오르는 빛 방울(아프로디테)
	Gust,      // 지나간 길을 따라 남는 바람 자국(헤르메스의 돌진)
};

// 스킬 효과 하나. 충돌·상호작용 없이 정해진 시간 동안 그려지다가 스스로 사라진다(Destroy 예약).
// 효과는 (x, y)를 중심으로 radius까지 퍼지고, Gust만 (x, y)에서 (toX, toY)까지 이어진 자국이다.
class SkillEffectActor : public Actor
{
public:
	SkillEffectActor(SkillEffectStyle style, float x, float y, float radius, float duration,
		float r, float g, float b, float toX = 0.f, float toY = 0.f);

	void OnUpdate(const UpdateContext& ctx) override;
	void OnRender(const RenderContext& ctx) override;

private:
	void RenderLightning(const RenderContext& ctx, float fade);
	void RenderRing(const RenderContext& ctx, float radius, float width, float r, float g, float b, float a);

	SkillEffectStyle m_Style;
	float m_Radius;
	float m_Duration;
	float m_Age = 0.f;
	float m_ToX;
	float m_ToY;
	unsigned int m_Seed; // 벼락의 꺾임 모양을 정하는 씨앗(자리마다 다름)
};
