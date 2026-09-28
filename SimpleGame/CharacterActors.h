#pragma once

#include "Actor.h"

// 경험치/레벨에 따라 성장하는 플레이어 능력치.
struct PlayerStats
{
	int level = 1;
	int xp = 0;
	int xpToNext = 100;
	int attackPower = 10;
	int maxHp = 30;
	int hp = 30;
};

// 이동하는 캐릭터(플레이어/NPC/짐승)를 충돌 판정할 때 쓰는 근사 반지름.
const float kMoverRadius = 0.32f;

// 피격 시 몸이 번쩍이는 색: 짐승이 맞으면 흰색, 플레이어가 맞으면 붉은색.
enum class FlashKind
{
	White,
	Red,
};

// 캐릭터 체형: 사람(두 발)과 네 발 짐승(사슴/늑대).
enum class BodyShape
{
	Human,
	Deer,
	Wolf,
};

// 사람 캐릭터의 겉모습 중 옷 색(SetColor) 말고 나머지 특징.
struct HumanLook
{
	float hairR = 0.20f, hairG = 0.13f, hairB = 0.07f; // 머리카락 색
	bool robe = false;   // 발목까지 내려오는 긴 옷(다리를 덮음) — 장로
	bool beard = false;  // 흰 수염 — 장로
	bool staff = false;  // 지팡이 — 장로
	bool helmet = false; // 청동 투구 + 붉은 깃털 장식 — 플레이어
	bool spear = false;  // 창 — 플레이어
};

// 상자·원기둥(Shapes.h)으로 조립해서 그리는 캐릭터의 공통 기반.
//  - 사람: 다리(맨다리+샌들) → 팔(+손에 든 창/지팡이) → 몸통(옷)+허리띠 → 머리+머리카락/투구 순.
//    이 렌더러는 깊이버퍼 없이 나중에 그린 게 위라서, 겹치는 자리에선 항상 몸통·머리가 우선 보인다.
//  - 네 발 짐승: 다리 4개를 먼저 깔고, 몸통·머리·귀·꼬리·뿔은 바라보는 방향에 따라 앞뒤가 바뀌므로
//    파츠마다 카메라에서 먼 정도를 재서 먼 것부터 그린다.
// 걷기/공격 모션과 피격 플래시(모든 파츠가 같이 번쩍임)도 여기서 처리한다.
class CharacterActor : public Actor
{
public:
	CharacterActor(ActorType type, BodyShape shape);

	void OnUpdate(const UpdateContext& ctx) override;
	void OnRender(const RenderContext& ctx) override;
	void OnRenderShadow(const RenderContext& ctx) override;
	bool CastsShadow() const override { return true; }

protected:
	// 하위 클래스 생성자가 SetSize 뒤에 호출: 몸·머리·팔다리·손에 든 창(공격 때 앞으로 뻗는 것 포함)과
	// 발밑 그림자를 감싸는 경계 구를 크기에 맞춰 잡는다.
	void FitBoundsToSize();

	void SetWalking(bool walking) { m_Walking = walking; }

	// 0~1이면 공격 모션(사람: 오른팔 휘두르기, 늑대: 몸을 앞으로 내밀어 물기) 진행률, 음수면 공격 중이 아님.
	void SetAttackProgress(float progress) { m_AttackProgress = progress; }

	void TriggerFlash(FlashKind kind, float duration);

	void SetLook(const HumanLook& look) { m_Look = look; }

private:
	BodyShape m_Shape;
	HumanLook m_Look;
	bool m_Walking = false;
	float m_AttackProgress = -1.f;

	FlashKind m_FlashKind = FlashKind::White;
	float m_FlashTimer = 0.f;
	float m_FlashDuration = 0.f;
};

// 장로/마을 사람. 제자리에 서 있고 상호작용 식별자만 가진다.
class NpcActor : public CharacterActor
{
public:
	NpcActor(float x, float y, float size, float r, float g, float b, const HumanLook& look, int interactId, const char* name);
};

// 플레이어: 입력에 따른 이동/충돌, 공격 모션, 능력치(경험치/레벨업/체력)를 가진다.
// 겉모습은 청동 투구와 창을 든 그리스 전사.
class PlayerActor : public CharacterActor
{
public:
	PlayerActor(float x, float y);

	const PlayerStats& GetStats() const { return m_Stats; }

	// 사망 시 되돌아갈 지점.
	void SetSpawnPoint(float x, float y) { m_SpawnX = x; m_SpawnY = y; }

	// 이번 프레임의 이동 입력(정규화 전, 각 축 -1~1).
	void SetMoveInput(float x, float y) { m_MoveX = x; m_MoveY = y; }

	void OnUpdate(const UpdateContext& ctx) override;

	void StartAttack();
	void FaceToward(float x, float y);

	void GrantXP(int amount);
	void TakeDamage(int amount);

private:
	void Respawn();

	PlayerStats m_Stats;
	float m_SpawnX = 0.f;
	float m_SpawnY = 0.f;
	float m_MoveX = 0.f;
	float m_MoveY = 0.f;
	float m_AttackTimer = 0.f;
};

// 야생 짐승. 비공격형(사슴)은 anchor 주변을 배회만 하고, 공격형(늑대)은 플레이어가
// 감지 범위에 들어오면 추적하다가 사거리에 닿으면 주기적으로 문다. 배회든 추적이든 실제로 걸어서
// 움직이므로(속도 제한 + 물/건물/나무/다른 캐릭터와의 충돌 검사) 순간이동하거나 장애물을 통과하지 않는다.
class AnimalActor : public CharacterActor
{
public:
	AnimalActor(float anchorX, float anchorY, float size, float r, float g, float b,
		float phase, int hp, bool isAggressive, const char* name);

	void OnUpdate(const UpdateContext& ctx) override;

	// 피해를 입힌다. 쓰러졌으면 true(액터는 파괴 예약됨).
	bool TakeHit(int damage);

	// 플레이어를 추적/공격하는 몬스터(늑대)인지, 배회만 하는 짐승(사슴)인지.
	bool IsAggressive() const { return m_IsAggressive; }

private:
	// 둘 다 이번 프레임에 실제로 움직였으면 true(걷기 모션을 켤지 정하는 데 씀).
	bool Wander(const UpdateContext& ctx);
	bool UpdateMonsterAI(const UpdateContext& ctx);

	// (targetX, targetY)를 향해 speed로 걷는다. 목표에서 stopDistance 안이면 멈춘다. 축별로 따로
	// 충돌을 검사해서 벽·물가에 붙으면 미끄러지듯 이동한다. 움직였으면 true.
	bool MoveToward(const UpdateContext& ctx, float targetX, float targetY, float speed, float stopDistance);

	// (fromX, fromY)에서 (toX, toY)로 한 걸음 옮겨도 되는지(물·장애물·다른 캐릭터와 겹침 검사).
	bool CanStepTo(const UpdateContext& ctx, float fromX, float fromY, float toX, float toY) const;

	float m_AnchorX;
	float m_AnchorY;
	float m_Phase;
	int m_Hp;
	bool m_IsAggressive;
	bool m_IsChasing = false;
	bool m_IsReturning = false; // 추적을 포기하고 anchor로 돌아가는 중(이 동안은 플레이어를 새로 감지하지 않음)
	float m_AttackCooldown = 0.f;
	float m_LungeTimer = 0.f;   // 무는 동작(몸을 앞으로 내밀었다 돌아옴)의 남은 시간
};
