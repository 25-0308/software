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

// 캐릭터 체형: 사람(두 발), 네 발 짐승(사슴/늑대), 뱀(그림자 퓌톤).
enum class BodyShape
{
	Human,
	Deer,
	Wolf,
	Serpent,
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

// 이야기 인물·마을 사람. 제자리에 서 있거나(기본), 동료처럼 누군가를 따라다닌다(SetFollowTarget).
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

	void OnUpdate(const UpdateContext& ctx) override;

private:
	void Follow(const UpdateContext& ctx);

	const Actor* m_FollowTarget = nullptr;
	float m_BaseOpacity = 1.f;
	float m_FlareTimer = 0.f;
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
	void TakeDamage(int amount);

	// 체력 회복(최대 체력을 넘지 않음).
	void Heal(int amount);

	// ---- 수호신의 축복(StoryDirector가 PATRON에 따라 적용) ----
	void AddAttackPower(int amount) { m_Stats.attackPower += amount; }
	void AddMaxHp(int amount);
	void SetSpeedScale(float scale) { m_SpeedScale = scale; }       // 이동 속도 배율(헤르메스)
	void SetDetectScale(float scale) { m_DetectScale = scale; }     // 괴물이 알아채는 거리 배율(아프로디테)
	float GetDetectScale() const { return m_DetectScale; }

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
};

// 짐승·괴물의 종류. 종류마다 크기·색·체력·AI 수치·처치 경험치가 정해져 있다(CharacterActors.cpp의 표).
enum class AnimalKind
{
	Deer,         // 사슴: 배회만 한다
	Wolf,         // 늑대: 감지 반경 안에 들어온 플레이어를 쫓아와 문다
	ShadowWolf,   // 그림자 늑대: 신탁의 균열에서 나온 검보랏빛 늑대(약하지만 멀리서 알아채고 끈질기다)
	ShadowPython, // 그림자 퓌톤: 균열에서 기어나온 거대한 그림자 뱀(프롤로그의 보스)
};

// 야생 짐승·괴물. 비공격형(사슴)은 anchor 주변을 배회만 하고, 공격형은 플레이어가 감지 범위에 들어오면
// 추적하다가 사거리에 닿으면 주기적으로 문다. 배회든 추적이든 실제로 걸어서 움직이므로(속도 제한 +
// 물/건물/나무/다른 캐릭터와의 충돌 검사) 순간이동하거나 장애물을 통과하지 않는다.
class AnimalActor : public CharacterActor
{
public:
	AnimalActor(AnimalKind kind, float anchorX, float anchorY, float phase);

	void OnUpdate(const UpdateContext& ctx) override;
	void OnRenderShadow(const RenderContext& ctx) override;

	// 피해를 입힌다. 쓰러졌으면 true(액터는 파괴 예약됨).
	bool TakeHit(int damage);

	// 플레이어를 추적/공격하는 괴물인지, 배회만 하는 짐승(사슴)인지.
	bool IsAggressive() const;

	AnimalKind GetKind() const { return m_Kind; }

	// 쓰러뜨렸을 때 주는 경험치.
	int GetKillXP() const;

private:
	// 둘 다 이번 프레임에 실제로 움직였으면 true(걷기 모션을 켤지 정하는 데 씀).
	bool Wander(const UpdateContext& ctx);
	bool UpdateMonsterAI(const UpdateContext& ctx);

	// (targetX, targetY)를 향해 speed로 걷는다. 목표에서 stopDistance 안이면 멈춘다. 축별로 따로
	// 충돌을 검사해서 벽·물가에 붙으면 미끄러지듯 이동한다. 움직였으면 true.
	bool MoveToward(const UpdateContext& ctx, float targetX, float targetY, float speed, float stopDistance);

	// (fromX, fromY)에서 (toX, toY)로 한 걸음 옮겨도 되는지(물·장애물·다른 캐릭터와 겹침 검사).
	bool CanStepTo(const UpdateContext& ctx, float fromX, float fromY, float toX, float toY) const;

	AnimalKind m_Kind;
	float m_AnchorX;
	float m_AnchorY;
	float m_Phase;
	int m_Hp;
	bool m_IsChasing = false;
	bool m_IsReturning = false; // 추적을 포기하고 anchor로 돌아가는 중(이 동안은 플레이어를 새로 감지하지 않음)
	float m_AttackCooldown = 0.f;
	float m_LungeTimer = 0.f;   // 무는 동작(몸을 앞으로 내밀었다 돌아옴)의 남은 시간
};
