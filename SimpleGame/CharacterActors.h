#pragma once

#include "Actor.h"
#include "Faith.h"

struct PatronDef;

// 경험치/레벨에 따라 성장하는 플레이어 능력치. 맵을 옮겨도 통째로 이어받는다(SetStats).
struct PlayerStats
{
	int level = 1;
	int xp = 0;
	int xpToNext = 100;
	int attackPower = 10;
	int maxHp = 30;
	int hp = 30;

	// 수호신의 축복으로 붙는 상시 능력치(PatronPowers.h의 표).
	int defense = 0;          // 받는 피해에서 빼는 값(최소 1은 받는다)
	float critChance = 0.f;   // 일반 공격의 치명타 확률(피해 1.5배)
	int lifeOnKill = 0;       // 적을 쓰러뜨릴 때 회복하는 체력
	float cooldownScale = 1.f; // 공격·상호작용·기본 스킬 쿨타임 배율

	// 레벨업마다 기본 성장(공격력 +3, 최대 체력 +8)에 더 얹히는 몫(수호신마다 다름).
	int growthAttack = 0;
	int growthMaxHp = 0;
	int growthDefense = 0;
	int growthLifeOnKill = 0;
};

// 이동하는 캐릭터(플레이어/NPC/짐승)를 충돌 판정할 때 쓰는 근사 반지름.
const float kMoverRadius = 0.32f;

// 피격 시 몸이 번쩍이는 색: 짐승이 맞으면 흰색, 플레이어가 맞으면 붉은색.
enum class FlashKind
{
	White,
	Red,
};

// 캐릭터 체형: 사람(두 발), 네 발 짐승(사슴/늑대/소), 뱀(그림자 퓌톤).
enum class BodyShape
{
	Human,
	Deer,
	Wolf,
	Serpent,
	Cow,
};

// 사람 캐릭터의 겉모습 중 옷 색(SetColor) 말고 나머지 특징. 신·이야기 인물은 이 조합으로 구분한다
// (예: 제우스 = 흰 수염 + 금관 + 긴 옷, 헤르메스 = 날개 모자 + 전령 지팡이, 노파 = 두건 + 금빛 사과).
struct HumanLook
{
	float hairR = 0.20f, hairG = 0.13f, hairB = 0.07f;    // 머리카락 색
	float beardR = 0.92f, beardG = 0.91f, beardB = 0.87f; // 수염 색
	bool robe = false;      // 발목까지 내려오는 긴 옷(다리를 덮음)
	bool beard = false;     // 수염
	bool longHair = false;  // 얼굴 양옆으로 내려와 등 뒤로 늘어진 긴 머리
	bool staff = false;     // 지팡이(오른손)
	bool helmet = false;    // 청동 투구 + 붉은 깃털 장식
	bool spear = false;     // 창(오른손)
	bool trident = false;   // 삼지창(오른손, 포세이돈)
	bool caduceus = false;  // 금빛 머리가 달린 전령의 지팡이(오른손, 헤르메스)
	bool hammer = false;    // 늘어뜨려 든 대장장이 망치(오른손, 헤파이스토스)
	bool wingedHat = false; // 챙 넓은 날개 모자(헤르메스)
	bool hood = false;      // 머리를 덮는 두건과 등 뒤로 늘어진 자락(노파) — 색은 hoodR/G/B
	float hoodR = 0.3f, hoodG = 0.22f, hoodB = 0.18f;
	bool wreath = false;    // 머리에 두른 관(월계관·금관·담쟁이관) — 색은 wreathR/G/B
	float wreathR = 0.9f, wreathG = 0.75f, wreathB = 0.3f;
	bool apple = false;     // 왼손에 든 금빛 사과(노파 — 복선 F-02)
	bool wineCup = false;   // 왼손의 금 술잔(디오니소스)
	bool horns = false;     // 염소 뿔(판)
	bool goatLegs = false;  // 털 난 염소 다리와 발굽(판)
	bool bronzeBody = false; // 살 대신 청동(헤파이스토스가 만든 청동 전사)
};

// 상자·원기둥·타원(Shapes.h)으로 조립해서 그리는 캐릭터의 공통 기반.
//  - 사람: 다리 → 팔(+손에 든 물건) → 몸통(옷)+허리띠 → 머리+머리카락/투구/관 순.
//    이 렌더러는 깊이버퍼 없이 나중에 그린 게 위라서, 겹치는 자리에선 항상 몸통·머리가 우선 보인다.
//  - 네 발 짐승: 다리 4개를 먼저 깔고, 몸통·머리·귀·꼬리·뿔은 바라보는 방향에 따라 앞뒤가 바뀌므로
//    파츠마다 카메라에서 먼 정도를 재서 먼 것부터 그린다.
//  - 뱀: 마디마다 카메라에서 먼 것부터 그린 둥근 마디들이 바라보는 방향 뒤쪽으로 물결치며 이어진다.
// 걷기/공격 모션, 피격 플래시(모든 파츠가 같이 번쩍임), 전체 불투명도(사라져 가는 신)도 여기서 처리한다.
class CharacterActor : public Actor
{
public:
	CharacterActor(ActorType type, BodyShape shape);

	void OnUpdate(const UpdateContext& ctx) override;
	void OnRender(const RenderContext& ctx) override;
	void OnRenderShadow(const RenderContext& ctx) override;
	bool CastsShadow() const override { return true; }

protected:
	// 하위 클래스 생성자가 SetSize 뒤에 호출: 몸·머리·팔다리·손에 든 물건(공격 때 앞으로 뻗는 것 포함)과
	// 발밑 그림자를 감싸는 경계 구를 크기·체형에 맞춰 잡는다.
	void FitBoundsToSize();

	void SetWalking(bool walking) { m_Walking = walking; }

	// 0~1이면 공격 모션(사람: 오른팔 휘두르기, 늑대·뱀: 몸을 앞으로 내밀어 물기) 진행률, 음수면 공격 중이 아님.
	void SetAttackProgress(float progress) { m_AttackProgress = progress; }

	void TriggerFlash(FlashKind kind, float duration);

	void SetLook(const HumanLook& look) { m_Look = look; }

	// 캐릭터 전체의 불투명도(1 = 또렷, 작을수록 반투명). 사라져 가는 신 등.
	void SetOpacity(float opacity) { m_Opacity = opacity; }

	// 그림자 괴물처럼 눈이 빛나는지(어둠 속에서도 보이는 보랏빛 눈).
	void SetGlowingEyes(bool glowing) { m_GlowingEyes = glowing; }

	// (targetX, targetY)를 향해 speed로 걷는다. 목표에서 stopDistance 안이면 멈춘다. 축별로 따로 충돌을 검사해서
	// 벽·물가에 붙으면 미끄러지듯 이동하고, 움직였으면 그쪽을 바라본다. 움직였으면 true(짐승·마을 사람이 같이 쓴다).
	bool MoveToward(const UpdateContext& ctx, float targetX, float targetY, float speed, float stopDistance);

	// (fromX, fromY)에서 (toX, toY)로 한 걸음 옮겨도 되는지(물·장애물·다른 캐릭터와 겹침 검사).
	bool CanStepTo(const UpdateContext& ctx, float fromX, float fromY, float toX, float toY) const;

	// 다른 캐릭터와 이 거리보다 가까워지는 걸음은 막는다(몸이 큰 괴물은 넓게).
	virtual float PersonalSpace() const;

private:
	BodyShape m_Shape;
	HumanLook m_Look;
	bool m_Walking = false;
	float m_AttackProgress = -1.f;
	float m_Opacity = 1.f;
	bool m_GlowingEyes = false;

	FlashKind m_FlashKind = FlashKind::White;
	float m_FlashTimer = 0.f;
	float m_FlashDuration = 0.f;
};

// NPC가 지금 하고 있는 일. 거닐기는 NPC 스스로, 다른 NPC·플레이어와의 교류는 NpcSociety가 정한다.
enum class NpcActivity
{
	Idle,    // 제자리에 서서 둘러본다
	Stroll,  // 자기 자리 둘레를 거닌다
	Chat,    // 다른 NPC와 수다(같은 신·호의)
	Argue,   // 다른 NPC와 말다툼(경계)
	Scuffle, // 다른 NPC와 주먹다짐(적대 + 다혈질)
	Brawl,   // 플레이어에게 시비를 걸어 주먹다짐 중(목숨은 걸지 않는다)
	Follow,  // 동료처럼 누군가를 따라다닌다
};

// 플레이어와의 주먹다짐이 어떻게 끝났는지. NpcSociety가 한 번 읽어 가며 대사·보상을 처리한다.
enum class BrawlOutcome
{
	None,
	Yielded, // 플레이어에게 맞아 항복했다
	GaveUp,  // 플레이어가 멀리 달아났거나 시간이 다 돼 그만뒀다
};

// NpcSociety가 NPC마다 적어 두는 기록. NPC 자신은 쿨타임을 줄이기만 한다.
struct NpcMemory
{
	float socialCooldown = 0.f;   // 다른 NPC와 다시 어울리기까지 남은 시간
	float reactionCooldown = 0.f; // 플레이어에게 다시 반응하기까지 남은 시간
	bool giftGiven = false;       // 같은 신을 모시는 플레이어에게 선물을 줬다(이 지역에 있는 동안 한 번)
	bool humbled = false;         // 주먹다짐에서 졌다(다시는 시비를 걸지 않는다)
};

// 이야기 인물·마을 사람. 성격(SetTraits — 믿는 신·돌아다니는 반경·다혈질·교류)에 따라 제자리를 지키거나 자기 자리
// 둘레를 거닐고, NpcSociety가 짝지어 주면 다른 NPC와 수다·말다툼·주먹다짐을 하거나 플레이어에게 시비를 건다.
// 동료처럼 누군가를 따라다닐 수도 있다(SetFollowTarget — 그동안은 다른 일을 하지 않는다).
class NpcActor : public CharacterActor
{
public:
	NpcActor(float x, float y, float size, float r, float g, float b, const HumanLook& look, int interactId, const char* name);

	// target을 따라다니게 한다(nullptr이면 제자리). 너무 멀어지면(맵을 막 옮겼거나 막혔을 때) 곁으로 순간이동한다.
	// target은 같은 씬의 액터여야 한다(씬이 지워질 때 함께 사라지므로 따로 해제할 필요 없음).
	void SetFollowTarget(const Actor* target) { m_FollowTarget = target; }

	// 신도를 잃고 반쯤 사라진 신처럼 평소엔 흐리게(baseOpacity) 그린다. Flare()를 부르면(누군가 이름을 불러 주면)
	// 잠깐 또렷해졌다가 천천히 다시 흐려진다.
	void SetFaded(float baseOpacity);
	void Flare();

	// 모니터(WorldMonitor)용 상태: 누군가를 따라다니는 중인지(동료), 흐려진 신인지, 이름이 불려 잠깐 또렷한 중인지.
	bool IsFollowing() const { return m_FollowTarget != nullptr; }
	bool IsFaded() const { return m_BaseOpacity < 1.f; }
	bool IsFlaring() const { return m_FlareTimer > 0.f; }

	// ---- 성격과 자율 행동 ----
	// 믿는 신·돌아다니는 반경·다혈질·교류 여부(Faith.h). 돌아다니는 기준점은 부르는 순간의 자리다.
	void SetTraits(const NpcTraits& traits);
	void SetFaith(Faith faith) { m_Traits.faith = faith; }
	const NpcTraits& GetTraits() const { return m_Traits; }
	NpcActivity GetActivity() const { return m_Activity; }

	// 다른 NPC나 플레이어와 새로 어울릴 수 있는 상태인지(교류하는 마을 사람이고, 따라다니거나 다른 일을 하는 중이 아님).
	bool IsFree() const;

	// 다른 NPC와 마주 서서 seconds 동안 수다(Chat)·말다툼(Argue)·주먹다짐(Scuffle)을 한다. 끝나면 다시 거닌다.
	void BeginEncounter(NpcActor& partner, NpcActivity kind, float seconds);

	// 플레이어에게 시비를 걸어 주먹다짐을 시작한다(쫓아와서 주먹을 휘두름 — 플레이어 체력은 1 밑으로 내려가지 않음).
	// stamina대 맞으면 항복하고, 플레이어가 멀리 달아나거나 seconds가 지나면 그만둔다.
	void BeginBrawl(float seconds, int stamina);
	bool IsBrawling() const { return m_Activity == NpcActivity::Brawl; }

	// 주먹다짐 중에 플레이어에게 한 대 맞았다. 기운이 다해 항복했으면 true.
	bool TakeBrawlHit();

	// 지난번에 읽은 뒤로 주먹다짐이 끝났으면 그 결과를 한 번 돌려준다.
	BrawlOutcome ConsumeBrawlOutcome();

	void FaceToward(float x, float y);

	NpcMemory& Memory() { return m_Memory; }
	const NpcMemory& Memory() const { return m_Memory; }

	void OnUpdate(const UpdateContext& ctx) override;

private:
	void Follow(const UpdateContext& ctx);
	void UpdateRoutine(const UpdateContext& ctx);
	void EndActivity(float idleSeconds);
	bool PickStrollTarget(const UpdateContext& ctx, float awayFromX, float awayFromY, bool away);
	float NextRandom(); // 0~1, NPC마다 따로 굴러가는 난수

	const Actor* m_FollowTarget = nullptr;
	float m_BaseOpacity = 1.f;
	float m_FlareTimer = 0.f;

	NpcTraits m_Traits;
	float m_AnchorX = 0.f;
	float m_AnchorY = 0.f;
	NpcActivity m_Activity = NpcActivity::Idle;
	float m_ActivityTimer = 0.f;
	float m_StrollX = 0.f;
	float m_StrollY = 0.f;
	float m_StuckTimer = 0.f;
	NpcActor* m_Partner = nullptr; // 같은 씬의 NPC(씬과 함께 사라짐)
	float m_SwingTimer = 0.f;      // 팔을 휘두르는 모션의 남은 시간
	float m_PunchCooldown = 0.f;
	int m_BrawlStamina = 0;
	BrawlOutcome m_BrawlOutcome = BrawlOutcome::None;
	unsigned int m_RandomState = 1u;
	NpcMemory m_Memory;
};

// 플레이어: 입력에 따른 이동/충돌, 공격 모션, 능력치(경험치/레벨업/체력)를 가진다.
// 겉모습은 청동 투구와 창을 든 그리스 전사.
class PlayerActor : public CharacterActor
{
public:
	PlayerActor(float x, float y);

	const PlayerStats& GetStats() const { return m_Stats; }

	// 맵을 옮길 때 이전 맵의 능력치를 그대로 이어받는다.
	void SetStats(const PlayerStats& stats) { m_Stats = stats; }

	// 사망 시 되돌아갈 지점.
	void SetSpawnPoint(float x, float y) { m_SpawnX = x; m_SpawnY = y; }

	// 이번 프레임의 이동 입력(정규화 전, 각 축 -1~1).
	void SetMoveInput(float x, float y) { m_MoveX = x; m_MoveY = y; }

	void OnUpdate(const UpdateContext& ctx) override;

	void StartAttack();
	void FaceToward(float x, float y);

	void GrantXP(int amount);

	// 피해를 입는다(방어만큼 줄고, 최소 1). nonLethal이면 체력이 1 밑으로 내려가지 않는다(마을 사람과의 주먹다짐).
	void TakeDamage(int amount, bool nonLethal = false);

	// 체력 회복(최대 체력을 넘지 않음).
	void Heal(int amount);

	// ---- 수호신의 축복(StoryDirector가 PATRON에 따라 적용 — PatronPowers.h의 표) ----
	// 축복을 받는 순간 한 번: 상시 능력치(공격력·최대 체력·방어·치명타·처치 회복·쿨타임)와 레벨업 성장 몫을 더한다.
	void ApplyPatronPassive(const PatronDef& def);
	void SetSpeedScale(float scale) { m_SpeedScale = scale; }       // 이동 속도 배율(헤르메스)
	void SetDetectScale(float scale) { m_DetectScale = scale; }     // 괴물이 알아채는 거리 배율(아프로디테)

	// 괴물이 알아채는 거리 배율(축복 × 지금 걸린 버프).
	float GetDetectScale() const;

	// 공격·상호작용·기본 스킬 쿨타임 배율(축복 × 지금 걸린 버프).
	float GetCooldownScale() const;

	// 일정 시간 동안 이동 속도·쿨타임·들킴 배율을 바꾼다(헤르메스의 궁극기 「신속의 날개」).
	void StartBuff(float seconds, float speedScale, float cooldownScale, float detectScale);
	float GetBuffRemaining() const { return m_BuffTimer; }

	// 각성의 기운: 발밑에 수호신 색의 빛이 숨 쉬듯 번진다(이야기 속 주인공의 각성 뒤 — 궁극기가 열린다).
	void SetAura(bool enabled, float r, float g, float b);

	void OnRenderShadow(const RenderContext& ctx) override;

	// 지난번에 물은 뒤로 쓰러진 적이 있으면 true를 한 번 돌려준다(게임 쪽이 이야기에 "쓰러짐"을 알리는 데 씀).
	bool ConsumeDeath()
	{
		bool died = m_JustDied;
		m_JustDied = false;
		return died;
	}

private:
	void Respawn();

	PlayerStats m_Stats;
	float m_SpawnX = 0.f;
	float m_SpawnY = 0.f;
	float m_MoveX = 0.f;
	float m_MoveY = 0.f;
	float m_AttackTimer = 0.f;
	float m_SpeedScale = 1.f;
	float m_DetectScale = 1.f;
	bool m_JustDied = false;

	float m_BuffTimer = 0.f;
	float m_BuffSpeedScale = 1.f;
	float m_BuffCooldownScale = 1.f;
	float m_BuffDetectScale = 1.f;

	bool m_Aura = false;
	float m_AuraR = 1.f;
	float m_AuraG = 1.f;
	float m_AuraB = 1.f;
};

// 짐승·괴물의 종류. 종류마다 크기·색·체력·AI 수치·처치 경험치가 정해져 있다(CharacterActors.cpp의 표).
enum class AnimalKind
{
	Deer,         // 사슴: 배회만 한다
	Wolf,         // 늑대: 감지 반경 안에 들어온 플레이어를 쫓아와 문다
	ShadowWolf,   // 그림자 늑대: 신탁의 균열에서 나온 검보랏빛 늑대(약하지만 멀리서 알아채고 끈질기다)
	ShadowPython, // 그림자 퓌톤: 균열에서 기어나온 거대한 그림자 뱀(프롤로그의 보스)
	BronzeWarrior, // 청동 전사: 헤파이스토스가 내전 양쪽 진영에 팔아넘긴 청동 자동인형 병사(MQ_04)
	KronosEcho,    // 크로노스의 잔향: 타르타로스에서 깨어난 옛 왕의 거대한 메아리(MQ_05 최종 보스)
	WhiteCow,      // 하얀 암소: 헤라의 저주로 암소가 된 이오(SQ_AEGEAN_02). 공격하지 않고, 말을 걸 수 있다
};

// 짐승·괴물의 한글 이름(채팅창 로그·모니터용). object는 목적격 조사("을/를")까지 붙인 꼴.
struct AnimalNames
{
	const char* name;
	const char* object;
};

AnimalNames GetAnimalNames(AnimalKind kind);

// 야생 짐승·괴물. 비공격형(사슴)은 anchor 주변을 배회만 하고, 공격형은 플레이어가 감지 범위에 들어오면
// 추적하다가 사거리에 닿으면 주기적으로 문다. 배회든 추적이든 실제로 걸어서 움직이므로(속도 제한 +
// 물/건물/나무/다른 캐릭터와의 충돌 검사) 순간이동하거나 장애물을 통과하지 않는다.
class AnimalActor : public CharacterActor
{
public:
	AnimalActor(AnimalKind kind, float anchorX, float anchorY, float phase);

	void OnUpdate(const UpdateContext& ctx) override;
	void OnRender(const RenderContext& ctx) override;
	void OnRenderShadow(const RenderContext& ctx) override;

	// ---- 수호신 스킬이 거는 상태(머리 위에 작은 표시가 돈다) ----
	// 기절: 움직이지도 물지도 못한다. 홀림: 쫓지도 물지도 않고 헤매며, 그동안 받는 피해 1.5배(게임 쪽이 계산).
	// 겁: 쫓지도 물지도 않고 플레이어에게서 달아난다. 같은 상태가 다시 걸리면 남은 시간이 긴 쪽을 따른다.
	void Stun(float seconds);
	void Charm(float seconds);
	void Frighten(float seconds);
	// 쫓던 플레이어를 놓친다(제자리로 걸어 돌아간다 — 헤르메스의 날개로 모습을 감췄을 때).
	void LoseTrack();

	bool IsStunned() const { return m_StunTimer > 0.f; }
	bool IsCharmed() const { return m_CharmTimer > 0.f; }
	bool IsFrightened() const { return m_FearTimer > 0.f; }

	// 피해를 입힌다. 쓰러졌으면 true(액터는 파괴 예약됨).
	bool TakeHit(int damage);

	// 플레이어를 추적/공격하는 괴물인지, 배회만 하는 짐승(사슴)인지.
	bool IsAggressive() const;

	AnimalKind GetKind() const { return m_Kind; }

	// 쓰러뜨렸을 때 주는 경험치.
	int GetKillXP() const;

	// 모니터(WorldMonitor)용 상태: 체력, 플레이어를 쫓는 중인지, 추적을 포기하고 제자리로 돌아가는 중인지.
	int GetHp() const { return m_Hp; }
	int GetMaxHp() const;
	bool IsChasing() const { return m_IsChasing; }
	bool IsReturning() const { return m_IsReturning; }

protected:
	float PersonalSpace() const override;

private:
	// 모두 이번 프레임에 실제로 움직였으면 true(걷기 모션을 켤지 정하는 데 씀).
	bool Wander(const UpdateContext& ctx);
	bool UpdateMonsterAI(const UpdateContext& ctx);
	bool Flee(const UpdateContext& ctx);

	AnimalKind m_Kind;
	float m_AnchorX;
	float m_AnchorY;
	float m_Phase;
	int m_Hp;
	bool m_IsChasing = false;
	bool m_IsReturning = false; // 추적을 포기하고 anchor로 돌아가는 중(이 동안은 플레이어를 새로 감지하지 않음)
	float m_AttackCooldown = 0.f;
	float m_LungeTimer = 0.f;   // 무는 동작(몸을 앞으로 내밀었다 돌아옴)의 남은 시간
	float m_StunTimer = 0.f;
	float m_CharmTimer = 0.f;
	float m_FearTimer = 0.f;
};
