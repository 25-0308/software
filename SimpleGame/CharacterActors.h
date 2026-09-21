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

// 몸통(사각)+머리(원)+팔다리(사각)로 조립해서 그리는 캐릭터의 공통 기반.
// 사람(플레이어/NPC)은 머리·팔이 피부색, 다리는 바지색으로 통일하고, 짐승은
// 몸통 색에서 파생된 털 색 톤을 쓴다. 걷기/공격 모션과 피격 플래시도 여기서 처리한다.
class CharacterActor : public Actor
{
public:
	CharacterActor(ActorType type, bool isHumanoid);

	void OnUpdate(const UpdateContext& ctx) override;
	void OnRender(const RenderContext& ctx) override;
	bool CastsShadow() const override { return true; }

protected:
	// 하위 클래스 생성자가 SetSize 뒤에 호출: 몸통·머리·팔다리(공격 때 앞으로 뻗는 팔 포함)와
	// 발밑 그림자를 감싸는 경계 구를 크기에 맞춰 잡는다.
	void FitBoundsToSize();

	void SetWalking(bool walking) { m_Walking = walking; }

	// 0~1이면 공격 모션 진행률, 음수면 공격 중이 아님.
	void SetAttackProgress(float progress) { m_AttackProgress = progress; }

	void TriggerFlash(FlashKind kind, float duration);

private:
	bool m_IsHumanoid;
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
	NpcActor(float x, float y, float size, float r, float g, float b, int interactId, const char* name);
};

// 플레이어: 입력에 따른 이동/충돌, 공격 모션, 능력치(경험치/레벨업/체력)를 가진다.
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
// 감지 범위에 들어오면 추적하다가 사거리에 닿으면 주기적으로 공격한다.
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
	void Wander(float time);
	void UpdateMonsterAI(const UpdateContext& ctx);

	float m_AnchorX;
	float m_AnchorY;
	float m_Phase;
	int m_Hp;
	bool m_IsAggressive;
	bool m_IsChasing = false;
	float m_AttackCooldown = 0.f;
};
