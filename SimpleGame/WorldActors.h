#pragma once

#include <vector>

#include "Actor.h"
#include "Renderer.h"

// 청크 하나(예: 8x8칸)의 정적 바닥 타일들을 정점 색상 메시 하나로 구워서 드로우콜 1번에
// 그리는 배치. 예전엔 타일 하나하나가 독립된 액터(1칸=드로우콜 1번)였는데, 32x32=1024칸
// 전체가 화면에 걸리면 그것만으로 드로우콜 1024번이 나가는 게 성능 분석에서 가장 큰
// 병목으로 확인되어 이 방식으로 바꿨다. 물 타일은 시간 기반 물결 셰이더가 필요해서
// 일반 지형과는 별도의 배치로 나눈다(청크당 최대 2개: 지형 1개 + 물 1개). 섬 둘레의 바다도
// 같은 물 배치 하나로 그린다.
class TileBatchActor : public Actor
{
public:
	explicit TileBatchActor(bool isWater);
	~TileBatchActor() override;

	// vertices는 Renderer::kTileBatchFloatsPerVertex(9: 월드좌표3 + extra2 + 색4) 단위로 채워진
	// 정점 목록이고, 위치는 이미 월드 좌표로 구워져 있다(LevelBuilder가 채움).
	// 액터를 만든 직후 딱 한 번만 부른다.
	void Build(Renderer& renderer, const std::vector<float>& vertices);

	void OnRender(const RenderContext& ctx) override;

private:
	bool m_IsWater;
	Renderer* m_Renderer = nullptr; // 소멸자에서 GPU 버퍼를 정리하려고 Build 시점에 기억해 둠
	Renderer::TileBatchHandle m_Batch;
};

enum class TreeKind
{
	Broadleaf, // 둥근 활엽수(올리브·참나무 느낌): 원기둥 줄기 + 여러 겹의 둥근 수관
	Cypress,   // 사이프러스: 지중해 풍경을 상징하는, 가늘고 높이 솟은 짙은 초록 나무
};

// 나무: 원기둥 줄기 + 카메라를 향해 세운 타원 수관 여러 겹(바람에 살짝 흔들림).
// 줄기만 막는다. 색(SetColor)은 수관 색이다.
class TreeActor : public Actor
{
public:
	TreeActor(float x, float y, float size, float r, float g, float b, TreeKind kind);

	void OnRender(const RenderContext& ctx) override;
	void OnRenderShadow(const RenderContext& ctx) override;
	float GetCollisionRadius() const override { return GetSize() * 0.1f; } // 줄기 반지름(최대 0.08*size)을 덮음
	bool CastsShadow() const override { return true; }

private:
	TreeKind m_Kind;
};

// 건물: 흰 회벽 + 테라코타 박공지붕의 그리스 마을 집(돌 기단, 푸른 문, 불 켜진 창과 덧창).
// 발자국은 한 변이 size인 정사각형이고, 그만큼 이동을 막는다.
class BuildingActor : public Actor
{
public:
	BuildingActor(float x, float y, float size);

	void OnRender(const RenderContext& ctx) override;
	void OnRenderShadow(const RenderContext& ctx) override;
	bool BlocksCircle(float x, float y, float moverRadius) const override;
	bool CastsShadow() const override { return true; }
};

enum class ItemKind
{
	Herb,     // 약초: 제자리에서 떠다니는 작은 잎 덤불
	Offering, // 잃어버린 제물(퀘스트 아이템): 금빛 항아리(암포라)
};

// 획득 가능한 아이템: 제자리에서 위아래로 떠다니고, 바닥엔 제 빛깔의 빛이 은은하게 번진다.
class ItemActor : public Actor
{
public:
	ItemActor(float x, float y, float size, float r, float g, float b, ItemKind kind, int interactId);

	void OnRender(const RenderContext& ctx) override;

private:
	ItemKind m_Kind;
};

// 횃불: 바닥에서 세운 횃대 + 쇠 받침 + 일렁이는 불꽃(불 셰이더) + 바닥에 번지는 불빛.
// 보통 건물의 자식으로 붙인다(위치는 부모 기준 로컬 좌표, localZ는 바닥에서 불꽃까지의 높이).
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

// 공중을 천천히 떠다니며 반짝이는 빛 알갱이(반딧불·꽃가루 느낌) — 신화 속 세계의 은은한 마법
// 분위기를 더하는 순수 시각 효과. 알갱이마다 위치는 번호로 정해지는 고정 기준점 + 시간에 따른
// 흔들림이라 따로 상태를 들고 있지 않는다. 경계가 없어 항상 그려지지만(개수가 적고 렌더 큐로 한 번에
// 그려져 싸다), Overlay 레이어라 모든 오브젝트 위에 겹쳐 그려진다.
class AmbientMotesActor : public Actor
{
public:
	// 맵 중심 기준 ±halfWidth, ±halfHeight 범위에 count개를 흩뿌린다.
	AmbientMotesActor(float halfWidth, float halfHeight, int count);

	void OnRender(const RenderContext& ctx) override;

private:
	float m_HalfWidth;
	float m_HalfHeight;
	int m_Count;
};
