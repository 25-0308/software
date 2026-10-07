#pragma once

#include <vector>

#include "Actor.h"

// 지역마다 놓이는 지형지물·건축물·이야기 소품. 전부 Shapes.h의 도형 조합으로 그린다.
// 정면(문·박공·그림)은 카메라 쪽인 +y면(화면 오른쪽, 빛 받는 면)에 둔다.

// 그리스 신전: 3단 기단 위에 대리석 기둥을 두르고 박공지붕을 얹는다(델포이 아폴론 신전, 아크로폴리스).
// roman이면 로마식: 높은 단 + 정면에만 기둥 + 벽으로 둘러싼 내실 + 붉은 지붕 + 정면 비문(아테네의 유피테르 신전).
// 단 전체가 길을 막는다.
class TempleActor : public Actor
{
public:
	TempleActor(float x, float y, float width, float depth, float columnHeight, bool roman);

	void OnRender(const RenderContext& ctx) override;
	void OnRenderShadow(const RenderContext& ctx) override;
	bool BlocksCircle(float x, float y, float moverRadius) const override;
	bool CastsShadow() const override { return true; }

private:
	float m_Width;
	float m_Depth;
	float m_ColumnHeight;
	bool m_Roman;
};

// 바위: 회갈색 상자 몇 개를 겹친 덩어리. 산자락·절벽 가장자리를 따라 늘어놓아 지형이 솟아 보이게 한다.
// snowy면 윗면이 눈으로 덮인다(올림포스). 지나갈 수 없다.
class RockActor : public Actor
{
public:
	RockActor(float x, float y, float size, int variant, bool snowy);

	void OnRender(const RenderContext& ctx) override;
	float GetCollisionRadius() const override { return GetSize() * 0.45f; }
	bool CastsShadow() const override { return true; }

private:
	int m_Variant;
	bool m_Snowy;
};

// 동굴 입구: 바위 덩어리 앞면(+y면)에 어두운 아치형 구멍. 바위는 지나갈 수 없고, 입구 앞에 둔
// 트리거 영역(TriggerZoneActor)으로 "동굴에 들어섰다"를 판정한다.
class CaveEntranceActor : public Actor
{
public:
	CaveEntranceActor(float x, float y, float size);

	void OnRender(const RenderContext& ctx) override;
	bool BlocksCircle(float x, float y, float moverRadius) const override;
	bool CastsShadow() const override { return true; }
};

// 대리석 기둥 하나(원형 회의장·주랑에 줄지어 세움). broken이면 위가 부러진 짧은 기둥. 지나갈 수 없다.
// 기둥을 하나씩 액터로 두는 이유: 기둥 사이에 선 인물과의 앞뒤가 기둥마다 따로 정렬되어야 하기 때문.
class ColumnActor : public Actor
{
public:
	ColumnActor(float x, float y, float height, bool broken);

	void OnRender(const RenderContext& ctx) override;
	float GetCollisionRadius() const override { return 0.26f; }
	bool CastsShadow() const override { return true; }

private:
	float m_Height;
	bool m_Broken;
};

// 바닥에 깔린 둥근 돌바닥(Decal 레이어): 테두리 띠 + 안쪽 동심원 무늬. 원형 회의장 바닥 등.
// 납작한 바닥이라 Object 레이어에 두면 그 위에 선 인물을 덮어 버리므로 바닥 표시처럼 먼저 그린다.
class FloorDiscActor : public Actor
{
public:
	FloorDiscActor(float x, float y, float radius, float r, float g, float b);

	void OnRender(const RenderContext& ctx) override;
};

// 상자 모양 가구(대리석 회의 식탁 등). 지나갈 수 없다.
class TableActor : public Actor
{
public:
	TableActor(float x, float y, float width, float depth, float height, float r, float g, float b);

	void OnRender(const RenderContext& ctx) override;
	bool BlocksCircle(float x, float y, float moverRadius) const override;
	bool CastsShadow() const override { return true; }

private:
	float m_Width;
	float m_Depth;
	float m_Height;
};

// 신탁 제단: 돌 제단 위의 청동 그릇(은은한 불씨) + 곁에 세운 삼발이. 지나갈 수 없다.
class AltarActor : public Actor
{
public:
	AltarActor(float x, float y);

	void OnRender(const RenderContext& ctx) override;
	float GetCollisionRadius() const override { return 0.6f; }
	bool CastsShadow() const override { return true; }
};

// 오래된 벽화가 그려진 돌판(올림포스 산기슭의 크로노스 벽화). 조사할 수 있는 이야기 대상. 지나갈 수 없다.
class MuralActor : public Actor
{
public:
	MuralActor(float x, float y);

	void OnRender(const RenderContext& ctx) override;
	bool BlocksCircle(float x, float y, float moverRadius) const override;
	bool CastsShadow() const override { return true; }
};

// 대리석 신상. fallen이면 받침대만 남고, 신상은 목이 부러진 채 바닥에 누워 있다(아테네의 무너진 제우스 신상).
// 받침대와 누운 신상 둘레가 길을 막는다.
class StatueActor : public Actor
{
public:
	StatueActor(float x, float y, float size, bool fallen);

	void OnRender(const RenderContext& ctx) override;
	bool BlocksCircle(float x, float y, float moverRadius) const override;
	bool CastsShadow() const override { return true; }

private:
	bool m_Fallen;
};

// 돌 화덕: 뒤·양옆을 두른 돌벽 + 재 더미(아테네 뒷골목의 식은 화덕). 지나갈 수 없다.
class HearthActor : public Actor
{
public:
	HearthActor(float x, float y);

	void OnRender(const RenderContext& ctx) override;
	float GetCollisionRadius() const override { return 0.5f; }
};

// 은은하게 숨 쉬듯 빛나는 점(화덕의 불씨, 마법의 빛 등). Object 레이어에서 깊이 정렬된다.
class GlowActor : public Actor
{
public:
	GlowActor(float x, float y, float z, float size, float r, float g, float b);

	void OnRender(const RenderContext& ctx) override;
};

// 이정표: 지역 이동 지점. 조사하면(E) Data/Story.txt의 OBJ_SIGNPOST 대화가 열려 갈 수 있는 지역 목록이 뜬다.
class SignpostActor : public Actor
{
public:
	SignpostActor(float x, float y);

	void OnRender(const RenderContext& ctx) override;
	float GetCollisionRadius() const override { return 0.2f; }
	bool CastsShadow() const override { return true; }
};

// 바닥의 균열(델포이 신탁의 균열): 검게 갈라진 틈 + 그 틈에서 피어오르는 보랏빛 그림자 김. Decal 레이어.
class CrackActor : public Actor
{
public:
	CrackActor(float x, float y, float length);

	void OnRender(const RenderContext& ctx) override;
};

// 시장 좌판: 나무 판매대 + 기둥 넷 + 천 차양(색은 SetColor) + 과일·항아리. 지나갈 수 없다.
class StallActor : public Actor
{
public:
	StallActor(float x, float y, float r, float g, float b);

	void OnRender(const RenderContext& ctx) override;
	bool BlocksCircle(float x, float y, float moverRadius) const override;
	bool CastsShadow() const override { return true; }
};

// 눈에 보이지 않는 이야기 트리거 영역(제단 앞, 동굴 입구, 회의장 등). 플레이어가 바깥에서 안으로 들어서는 순간
// StoryDirector가 "on trigger <스토리ID>" 대화를 연다. 지금 열릴 대화가 있으면(= 목표 지점이면) 바닥에 금빛
// 고리가 은은하게 떠서 어디로 가야 하는지 보여준다. Decal 레이어.
class TriggerZoneActor : public Actor
{
public:
	TriggerZoneActor(float x, float y, float radius);

	float GetRadius() const { return m_Radius; }

	// 플레이어가 지금 안에 있는지를 넘기면, 바깥 → 안으로 막 들어선 순간에만 true.
	bool UpdateInside(bool playerInside);

	// "바깥에 있었다"로 되돌린다. 진행 단계가 바뀌면 StoryDirector가 부른다 — 새 목표가 바로 이 영역인데
	// 플레이어가 이미 그 안에 서 있어도(예: 제단 앞에서 대화를 마친 직후) 다음 프레임에 들어선 것으로 친다.
	void ResetInside() { m_PlayerInside = false; }

	void SetHighlighted(bool highlighted) { m_Highlighted = highlighted; }

	void OnRender(const RenderContext& ctx) override;

private:
	float m_Radius;
	bool m_PlayerInside = false;
	bool m_Highlighted = false;
};

// 목표 표시: 지금 해야 할 일의 대상(인물·물건·장소) 머리 위에서 위아래로 흔들리는 금빛 마름모. Overlay 레이어.
// StoryDirector가 매 프레임 대상 위치를 넘겨준다(대상이 여럿이면 전부).
class ObjectiveMarkerActor : public Actor
{
public:
	ObjectiveMarkerActor();

	struct Target
	{
		float x, y, z; // 마름모를 띄울 높이까지 포함한 월드 좌표
	};

	void SetTargets(const std::vector<Target>& targets) { m_Targets = targets; }

	void OnRender(const RenderContext& ctx) override;

private:
	std::vector<Target> m_Targets;
};

// 길잡이 화살표: 플레이어 발밑 둘레의 바닥에 깔려 지금 목표 쪽을 가리키는 금빛 화살촉 두 개(안쪽에서 바깥쪽으로
// 차례로 밝아져 그쪽으로 흘러가는 것처럼 보인다). 플레이어의 자식이라 플레이어를 저절로 따라다니고, 가리킬 곳은
// StoryDirector가 매 프레임 넘겨준다(목표가 다른 지역이면 이 지역의 이정표). 목표가 코앞이면(머리 위 마름모가
// 바로 보이는 거리) 흐려져 사라진다. 바닥 표시라 Decal 레이어 — 나무·건물·인물이 그 위를 덮는다.
class GuideArrowActor : public Actor
{
public:
	GuideArrowActor();

	// 가리킬 월드 위치. ClearTarget 뒤로는 그리지 않는다(목표가 없거나 대화 중).
	void SetTarget(float x, float y);
	void ClearTarget() { m_HasTarget = false; }

	void OnRender(const RenderContext& ctx) override;

private:
	bool m_HasTarget = false;
	float m_TargetX = 0.f;
	float m_TargetY = 0.f;
};
