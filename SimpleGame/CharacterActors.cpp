#include "stdafx.h"
#include "CharacterActors.h"

#include <cmath>
#include <string>

#include "GameLog.h"
#include "Renderer.h"
#include "SceneGraph.h"
#include "Shapes.h"
#include "TileMap.h"

namespace
{
	// 공격 모션 재생 시간(초).
	const float kAttackAnimDuration = 0.25f;

	// 피격 플래시 지속 시간(초).
	const float kAnimalHitFlashDuration = 0.15f;
	const float kPlayerHitFlashDuration = 0.2f;

	const float kPlayerSpeed = 4.f; // 초당 이동 거리(월드 단위)

	// 몬스터(공격형 짐승) AI 파라미터.
	const float kMonsterDetectRadius = 4.5f;   // 이 안에 들어오면 추적을 시작한다.
	const float kMonsterGiveUpRadius = 7.f;    // 플레이어가 이보다 멀어지면 추적을 포기한다(히스테리시스).
	const float kMonsterLeashRadius = 6.f;     // 자기 anchor에서 이보다 멀어지면 추적을 포기하고 돌아간다.
	const float kMonsterAttackRadius = 1.1f;   // 이 안에 있어야 실제로 때린다.
	const float kMonsterChaseSpeed = 2.3f;     // 플레이어(4.0)보다 느려서 도망칠 여지가 있음.
	const float kMonsterAttackCooldown = 1.1f; // 몬스터 공격 간 최소 간격(초).
	const int kMonsterAttackDamage = 6;

	// 사람의 머리·팔(피부색)과 다리(바지색).
	const float kSkinR = 0.92f, kSkinG = 0.78f, kSkinB = 0.62f;
	const float kPantsR = 0.24f, kPantsG = 0.22f, kPantsB = 0.26f;
}

// ----------------------------------------------------------- CharacterActor

CharacterActor::CharacterActor(ActorType type, bool isHumanoid)
	: Actor(type)
	, m_IsHumanoid(isHumanoid)
{
}

void CharacterActor::FitBoundsToSize()
{
	// 머리 끝은 위로 약 1.15*size, 공격 때 팔은 옆으로 최대 약 1.0*size 뻗고, 그림자는
	// 가로 0.9*size다. 중심을 0.55*size 높이에 두고 반지름 1.25*size면 전부 들어온다.
	float size = GetSize();
	SetBoundingSphere(size * 1.25f, size * 0.55f);
}

void CharacterActor::TriggerFlash(FlashKind kind, float duration)
{
	m_FlashKind = kind;
	m_FlashTimer = duration;
	m_FlashDuration = duration;
}

void CharacterActor::OnUpdate(const UpdateContext& ctx)
{
	if (m_FlashTimer > 0.f)
	{
		m_FlashTimer -= ctx.deltaSeconds;
		if (m_FlashTimer < 0.f)
		{
			m_FlashTimer = 0.f;
		}
	}
}

void CharacterActor::OnRender(const RenderContext& ctx)
{
	float x = GetWorldX();
	float y = GetWorldY();
	float z = GetWorldZ();
	float size = GetSize();
	float time = ctx.time;
	float facing = GetFacing();

	// 피격 직후엔 원래 색을 플래시 색 쪽으로 밀어서 잠깐 번쩍이게 한다
	// (액터 자신의 색은 그대로 두고 그리기용 색만 바꾼다).
	float r = GetR(), g = GetG(), b = GetB();
	if (m_FlashTimer > 0.f)
	{
		float flash = m_FlashTimer / m_FlashDuration;
		if (m_FlashKind == FlashKind::White)
		{
			r += (1.f - r) * flash;
			g += (1.f - g) * flash;
			b += (1.f - b) * flash;
		}
		else
		{
			r += (1.f - r) * flash;
			g -= g * flash * 0.7f;
			b -= b * flash * 0.7f;
		}
	}

	float breathe = sinf(time * 4.f + x * 3.1f) * 0.03f; // 개체마다 위상이 다른 미세한 숨쉬기 움직임
	float legSwing = m_Walking ? sinf(time * 8.f) * 0.15f : 0.f;
	float walkBob = m_Walking ? fabsf(sinf(time * 8.f)) * 0.05f : 0.f; // 걷는 동안의 상하 들썩임

	// facing 기준 정면/측면 단위 벡터. 팔다리를 이 축으로 배치해서 캐릭터가
	// 이동/공격 방향으로 실제로 돌아보는 것처럼 보이게 한다.
	float forwardX = cosf(facing), forwardY = sinf(facing);
	float rightX = -forwardY, rightY = forwardX;

	// 사람은 옷 색과 무관하게 머리·팔은 피부색, 다리는 바지색으로 통일하고,
	// 짐승은 몸통 색에서 파생된 톤(밝은 머리/어두운 다리)을 쓴다.
	float headR = m_IsHumanoid ? kSkinR : (r * 1.2f + 0.1f);
	float headG = m_IsHumanoid ? kSkinG : (g * 1.2f + 0.1f);
	float headB = m_IsHumanoid ? kSkinB : (b * 1.2f + 0.1f);

	float limbR = m_IsHumanoid ? kSkinR : (r * 0.75f);
	float limbG = m_IsHumanoid ? kSkinG : (g * 0.75f);
	float limbB = m_IsHumanoid ? kSkinB : (b * 0.75f);

	float legR = m_IsHumanoid ? kPantsR : (r * 0.6f);
	float legG = m_IsHumanoid ? kPantsG : (g * 0.6f);
	float legB = m_IsHumanoid ? kPantsB : (b * 0.6f);

	// 예전엔 몸통·팔다리가 전부 납작한 카드 한 장이라 입체감이 없었다. 건물/나무에 쓰던
	// Shapes.h의 상자/원기둥(면마다 밝기를 달리해 카메라 쪽에서 입체로 보이게 하는 기법)를
	// 그대로 재사용해서 실제 부피가 있는 것처럼 보이게 한다. 각 파츠의 중심 높이/두께는
	// 예전 값을 그대로 물려받아(중심 = 아래끝 + 높이/2) 전체 실루엣과 비율은 유지했다.
	Shapes::Rgb bodyColor = { r, g, b };
	Shapes::Rgb headColor = { headR, headG, headB };
	Shapes::Rgb limbColor = { limbR, limbG, limbB };
	Shapes::Rgb legColor = { legR, legG, legB };

	// 이 렌더러는 실제 깊이버퍼가 없어서(그리기 순서로 깊이를 흉내), 겹친 픽셀은 "나중에
	// 그린 것"이 화면에 보인다. 그래서 팔다리를 먼저 그려 깔아 두고, 몸통·머리를 맨 나중에
	// 그려서 겹치는 자리에서는 항상 몸통·머리가 우선적으로 보이도록(팔다리가 가려지도록) 한다.

	// 다리 2개: right축으로 벌리고, 걷는 동안 forward축으로 서로 반대로 흔들림.
	float legBaseZ = z;
	float legLeftX = x - rightX * size * 0.15f + forwardX * legSwing;
	float legLeftY = y - rightY * size * 0.15f + forwardY * legSwing;
	float legRightX = x + rightX * size * 0.15f - forwardX * legSwing;
	float legRightY = y + rightY * size * 0.15f - forwardY * legSwing;
	Shapes::DrawBox(ctx, legLeftX, legLeftY, legBaseZ, size * 0.18f, size * 0.18f, size * 0.3f,
		Shapes::Shade(legColor, 1.0f), Shapes::Shade(legColor, 0.82f), Shapes::Shade(legColor, 0.62f));
	Shapes::DrawBox(ctx, legRightX, legRightY, legBaseZ, size * 0.18f, size * 0.18f, size * 0.3f,
		Shapes::Shade(legColor, 1.0f), Shapes::Shade(legColor, 0.82f), Shapes::Shade(legColor, 0.62f));

	// 팔 2개: 걷는 동안은 다리와 반대 위상으로 흔들리고, 공격 중에는 오른팔이
	// facing 방향으로 크게 휘둘러진다.
	float armWalkSwing = m_Walking ? -legSwing : 0.f;
	float attackSwing = (m_AttackProgress >= 0.f) ? sinf(m_AttackProgress * 3.14159265f) * size * 0.6f : 0.f;
	float armBaseZ = z + size * 0.4f;

	float armLeftX = x - rightX * size * 0.42f + forwardX * armWalkSwing;
	float armLeftY = y - rightY * size * 0.42f + forwardY * armWalkSwing;
	float armRightX = x + rightX * size * 0.42f - forwardX * armWalkSwing + forwardX * attackSwing;
	float armRightY = y + rightY * size * 0.42f - forwardY * armWalkSwing + forwardY * attackSwing;
	Shapes::DrawBox(ctx, armLeftX, armLeftY, armBaseZ, size * 0.14f, size * 0.14f, size * 0.3f,
		Shapes::Shade(limbColor, 1.0f), Shapes::Shade(limbColor, 0.82f), Shapes::Shade(limbColor, 0.62f));
	Shapes::DrawBox(ctx, armRightX, armRightY, armBaseZ, size * 0.14f, size * 0.14f, size * 0.3f,
		Shapes::Shade(limbColor, 1.0f), Shapes::Shade(limbColor, 0.82f), Shapes::Shade(limbColor, 0.62f));

	// 몸통: 위(1.0)>화면 왼쪽 면(0.82)>화면 오른쪽 면(0.62) — 건물 벽과 같은 명암비.
	float bodyCenterZ = z + size * 0.5f + breathe + walkBob;
	Shapes::DrawBox(ctx, x, y, bodyCenterZ - size * 0.35f, size * 0.55f, size * 0.35f, size * 0.7f,
		Shapes::Shade(bodyColor, 1.0f), Shapes::Shade(bodyColor, 0.82f), Shapes::Shade(bodyColor, 0.62f));

	// 머리: 납작한 원반 대신 낮은 원기둥으로 그려서 정수리가 둥글게 보이도록 한다. 맨 마지막에
	// 그려서 팔이 휘둘러질 때도 항상 머리가 가장 우선적으로 보인다. 크기는 예전의 절반
	// (반지름 0.4→0.2, 높이 0.5→0.25)으로 줄이되 중심 높이는 그대로 둬서 몸통 위에 자연스럽게 얹힌다.
	float headCenterZ = z + size * 0.95f + breathe + walkBob;
	Shapes::DrawCylinder(ctx, x, y, headCenterZ - size * 0.125f, size * 0.2f, size * 0.25f,
		headColor, Shapes::Shade(headColor, 1.15f));
}

// ---------------------------------------------------------------- NpcActor

NpcActor::NpcActor(float x, float y, float size, float r, float g, float b, int interactId, const char* name)
	: CharacterActor(ActorType::NPC, true)
{
	SetPosition(x, y, 0.f);
	SetSize(size);
	SetColor(r, g, b);
	SetInteractId(interactId);
	SetName(name);
	FitBoundsToSize();
}

// ------------------------------------------------------------- PlayerActor

PlayerActor::PlayerActor(float x, float y)
	: CharacterActor(ActorType::Player, true)
{
	SetPosition(x, y, 0.f);
	SetSize(0.8f);
	SetColor(0.9f, 0.85f, 0.8f);
	SetName("Player");
	SetSpawnPoint(x, y);
	FitBoundsToSize();
}

void PlayerActor::OnUpdate(const UpdateContext& ctx)
{
	CharacterActor::OnUpdate(ctx);

	float moveX = m_MoveX;
	float moveY = m_MoveY;
	bool isMoving = (moveX != 0.f || moveY != 0.f);
	SetWalking(isMoving);

	if (isMoving)
	{
		float length = sqrtf(moveX * moveX + moveY * moveY);
		moveX /= length;
		moveY /= length;
		SetFacing(atan2f(moveY, moveX));

		float step = kPlayerSpeed * ctx.deltaSeconds;
		float x = GetX();
		float y = GetY();

		// 축별로 따로 검사해서, 물가/건물/나무 벽에 붙어 미끄러지듯 이동하되
		// 그 위로는 올라갈 수 없게 한다 (호수·건물·나무 = 접근 불가 영역).
		float newX = x + moveX * step;
		if (ctx.tileMap.IsWorldPositionWalkable(newX, y) && !ctx.scene.IsBlocked(newX, y, kMoverRadius))
		{
			x = newX;
		}

		float newY = y + moveY * step;
		if (ctx.tileMap.IsWorldPositionWalkable(x, newY) && !ctx.scene.IsBlocked(x, newY, kMoverRadius))
		{
			y = newY;
		}

		// 맵 경계 안쪽으로 클램프(맵 반너비보다 살짝 안쪽).
		float halfX = ctx.tileMap.GetWidth() * 0.5f - 0.5f;
		float halfY = ctx.tileMap.GetHeight() * 0.5f - 0.5f;
		if (x < -halfX) x = -halfX;
		if (x > halfX) x = halfX;
		if (y < -halfY) y = -halfY;
		if (y > halfY) y = halfY;

		SetPosition(x, y, GetZ());
	}

	if (m_AttackTimer > 0.f)
	{
		m_AttackTimer -= ctx.deltaSeconds;
		if (m_AttackTimer < 0.f)
		{
			m_AttackTimer = 0.f;
		}
	}
	SetAttackProgress(m_AttackTimer > 0.f ? 1.f - m_AttackTimer / kAttackAnimDuration : -1.f);
}

void PlayerActor::StartAttack()
{
	m_AttackTimer = kAttackAnimDuration;
}

void PlayerActor::FaceToward(float x, float y)
{
	SetFacing(atan2f(y - GetWorldY(), x - GetWorldX()));
}

void PlayerActor::GrantXP(int amount)
{
	m_Stats.xp += amount;
	GameLog::Add(GameLog::Kind::Reward, "[경험치 획득] +" + std::to_string(amount)
		+ " (현재 " + std::to_string(m_Stats.xp) + "/" + std::to_string(m_Stats.xpToNext) + ")");

	while (m_Stats.xp >= m_Stats.xpToNext)
	{
		m_Stats.xp -= m_Stats.xpToNext;
		m_Stats.level += 1;
		m_Stats.xpToNext = (int)(m_Stats.xpToNext * 1.5f);
		m_Stats.attackPower += 3;
		m_Stats.maxHp += 8;
		m_Stats.hp = m_Stats.maxHp;

		GameLog::Add(GameLog::Kind::Reward, "[레벨업!] Lv." + std::to_string(m_Stats.level)
			+ " (공격력 " + std::to_string(m_Stats.attackPower)
			+ ", 최대체력 " + std::to_string(m_Stats.maxHp) + ")");
	}
}

// 몬스터의 공격을 적용한다: 데미지, 붉은 피격 플래시, 사망 시 리스폰.
void PlayerActor::TakeDamage(int amount)
{
	m_Stats.hp -= amount;
	TriggerFlash(FlashKind::Red, kPlayerHitFlashDuration);

	int shownHp = (m_Stats.hp > 0) ? m_Stats.hp : 0;
	GameLog::Add(GameLog::Kind::Damage, "[피격!] 몬스터에게 " + std::to_string(amount)
		+ " 피해를 입었다 (체력 " + std::to_string(shownHp) + "/" + std::to_string(m_Stats.maxHp) + ")");

	if (m_Stats.hp <= 0)
	{
		Respawn();
	}
}

// 체력이 0 이하로 떨어져 쓰러졌을 때: 마을 중심으로 되돌리고 체력을 채워 다시
// 시작한다(세이브/로드가 없는 프로토타입이라 페널티 없는 리스폰으로 단순하게 처리).
void PlayerActor::Respawn()
{
	GameLog::Add(GameLog::Kind::Damage, "[쓰러졌다...] 마을로 돌아왔다.");
	SetPosition(m_SpawnX, m_SpawnY, GetZ());
	m_Stats.hp = m_Stats.maxHp;
}

// ------------------------------------------------------------- AnimalActor

AnimalActor::AnimalActor(float anchorX, float anchorY, float size, float r, float g, float b,
	float phase, int hp, bool isAggressive, const char* name)
	: CharacterActor(ActorType::Animal, false)
	, m_AnchorX(anchorX)
	, m_AnchorY(anchorY)
	, m_Phase(phase)
	, m_Hp(hp)
	, m_IsAggressive(isAggressive)
{
	SetPosition(anchorX, anchorY, 0.f);
	SetSize(size);
	SetColor(r, g, b);
	SetName(name);
	FitBoundsToSize();
}

void AnimalActor::OnUpdate(const UpdateContext& ctx)
{
	CharacterActor::OnUpdate(ctx);
	SetWalking(true);

	if (m_IsAggressive)
	{
		UpdateMonsterAI(ctx);
	}
	else
	{
		Wander(ctx.time);
	}

	if (m_AttackCooldown > 0.f)
	{
		m_AttackCooldown -= ctx.deltaSeconds;
		if (m_AttackCooldown < 0.f)
		{
			m_AttackCooldown = 0.f;
		}
	}
}

bool AnimalActor::TakeHit(int damage)
{
	m_Hp -= damage;
	TriggerFlash(FlashKind::White, kAnimalHitFlashDuration);

	if (m_Hp <= 0)
	{
		Destroy();
		return true;
	}

	return false;
}

// 기본 이동: anchor를 중심으로 한 단순한 진자 운동. 실제로 이동한 방향을
// 바라보게 해서 배회 중에도 살아있는 느낌을 준다.
void AnimalActor::Wander(float time)
{
	float prevX = GetX();
	float prevY = GetY();
	float x = m_AnchorX + sinf(time * 0.4f + m_Phase) * 1.5f;
	float y = m_AnchorY + cosf(time * 0.25f + m_Phase) * 1.0f;
	SetPosition(x, y, GetZ());

	float vx = x - prevX;
	float vy = y - prevY;
	if (vx * vx + vy * vy > 0.000001f)
	{
		SetFacing(atan2f(vy, vx));
	}
}

// 공격형 짐승(늑대)의 추적/공격 AI. 감지 반경 안에 플레이어가 들어오면 추적을
// 시작하고, 공격 사거리에 닿으면 쿨다운마다 플레이어를 공격한다. 플레이어가
// 너무 멀어지거나(kMonsterGiveUpRadius) 자기 anchor에서 너무 멀어지면
// (kMonsterLeashRadius) 추적을 포기하고 배회로 돌아간다.
void AnimalActor::UpdateMonsterAI(const UpdateContext& ctx)
{
	PlayerActor* player = ctx.scene.GetPlayer();
	if (player == nullptr)
	{
		Wander(ctx.time);
		return;
	}

	float x = GetX();
	float y = GetY();
	float dxPlayer = player->GetWorldX() - x;
	float dyPlayer = player->GetWorldY() - y;
	float distToPlayerSq = dxPlayer * dxPlayer + dyPlayer * dyPlayer;

	float dxAnchor = x - m_AnchorX;
	float dyAnchor = y - m_AnchorY;
	float distFromAnchorSq = dxAnchor * dxAnchor + dyAnchor * dyAnchor;

	// 감지 반경과 이탈 반경을 다르게 둬서(히스테리시스) 경계선에서 추적을
	// 시작/포기하며 떠는 것을 막는다.
	if (!m_IsChasing && distToPlayerSq <= kMonsterDetectRadius * kMonsterDetectRadius)
	{
		m_IsChasing = true;
	}
	else if (m_IsChasing && (distToPlayerSq > kMonsterGiveUpRadius * kMonsterGiveUpRadius ||
		distFromAnchorSq > kMonsterLeashRadius * kMonsterLeashRadius))
	{
		m_IsChasing = false;
	}

	if (!m_IsChasing)
	{
		Wander(ctx.time);
		return;
	}

	float distToPlayer = sqrtf(distToPlayerSq);
	if (distToPlayer > kMonsterAttackRadius * 0.6f) // 바짝 붙었을 때 제자리에서 떨지 않도록 여유를 둠.
	{
		float dirX = dxPlayer / distToPlayer;
		float dirY = dyPlayer / distToPlayer;
		float step = kMonsterChaseSpeed * ctx.deltaSeconds;

		float newX = x + dirX * step;
		if (ctx.tileMap.IsWorldPositionWalkable(newX, y) && !ctx.scene.IsBlocked(newX, y, kMoverRadius))
		{
			x = newX;
		}

		float newY = y + dirY * step;
		if (ctx.tileMap.IsWorldPositionWalkable(x, newY) && !ctx.scene.IsBlocked(x, newY, kMoverRadius))
		{
			y = newY;
		}

		SetPosition(x, y, GetZ());
	}

	SetFacing(atan2f(dyPlayer, dxPlayer));

	if (distToPlayer <= kMonsterAttackRadius && m_AttackCooldown <= 0.f)
	{
		player->TakeDamage(kMonsterAttackDamage);
		m_AttackCooldown = kMonsterAttackCooldown;
	}
}
