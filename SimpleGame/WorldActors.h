#pragma once

#include "Actor.h"

// 바닥 타일 한 칸. 물 타일은 시간에 따라 일렁이는 전용 셰이더로 그린다.
class TileActor : public Actor
{
public:
	TileActor(float x, float y, float r, float g, float b, bool isWater);

	void OnRender(const RenderContext& ctx) override;
};

// 나무: 원기둥 줄기 + 세운 타원 수관(큰 타원 + 밝은 하이라이트 타원, 바람에 살짝 흔들림).
// 줄기만 막는다. 색(SetColor)은 수관 색이다.
class TreeActor : public Actor
{
public:
	TreeActor(float x, float y, float size, float r, float g, float b);

	void OnRender(const RenderContext& ctx) override;
	float GetCollisionRadius() const override { return GetSize() * 0.1f; } // 줄기 반지름(0.09*size)에 맞춤
	bool CastsShadow() const override { return true; }
};

// 건물: 한 변이 size인 큐브(위=지붕색, 앞 두 면=벽색 + 문/창문). 큐브 발자국(정사각형)만큼 이동을 막는다.
class BuildingActor : public Actor
{
public:
	BuildingActor(float x, float y, float size);

	void OnRender(const RenderContext& ctx) override;
	void OnRenderShadow(const RenderContext& ctx) override;
	bool BlocksCircle(float x, float y, float moverRadius) const override;
	bool CastsShadow() const override { return true; }
};

// 획득 가능한 아이템: 제자리에서 위아래로 떠다니는 작은 타원.
class ItemActor : public Actor
{
public:
	ItemActor(float x, float y, float size, float r, float g, float b, int interactId);

	void OnRender(const RenderContext& ctx) override;
};

// 횃불/모닥불: 시간 기반으로 일렁이는 불 셰이더. 보통 건물의 자식으로 붙인다
// (위치는 부모 기준 로컬 좌표).
class FireActor : public Actor
{
public:
	FireActor(float localX, float localY, float localZ, float size);

	void OnRender(const RenderContext& ctx) override;
};

// 바닥에 깔리는 반투명 펄스 링(Decal). 두 가지로 쓴다:
//  - 플레이어의 자식으로 붙이면 부모를 따라다니는 위치 마커.
//  - SetTarget으로 대상을 지정하면 그 대상 발밑에 뜨는 조준 링(nullptr이면 숨김).
struct RingStyle
{
	float r, g, b;
	float baseScale;   // 반지름 = 대상 크기 * (baseScale + pulse * pulseScale)
	float pulseScale;
	float baseAlpha;   // 알파 = baseAlpha + pulse * pulseAlpha
	float pulseAlpha;
	float pulseSpeed;
};

class RingActor : public Actor
{
public:
	// followsParent가 true면 부모를 따라다니는 마커(부모 크기 기준),
	// false면 SetTarget으로 지정한 대상 발밑에 뜨는 조준 링이다.
	RingActor(const RingStyle& style, bool followsParent);

	// 조준 링 모드에서 대상을 지정한다. nullptr이면 링을 그리지 않는다. 링은 대상의 자식이
	// 아니라서, 컬링용 경계가 링 자신의 위치를 따라가도록 대상의 현재 위치와 크기를 복사해 둔다
	// (대상 포인터를 저장하지 않으므로 대상이 사라져도 안전하다). 대상이 움직이면 매 프레임
	// 게임 코드가 다시 지정해줘야 한다.
	void SetTarget(const Actor* target);

	void OnRender(const RenderContext& ctx) override;

private:
	RingStyle m_Style;
	bool m_FollowsParent;
	bool m_HasTarget = false;
	float m_TargetSize = 1.f;
};
