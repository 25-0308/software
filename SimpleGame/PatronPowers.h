#pragma once

#include <string>

// 수호신의 힘: 고른 신(PATRON)에 따라 붙는 상시 능력치(패시브)와 레벨업마다 더 오르는 몫, 그리고 신의 권능을 딴
// 스킬 두 개 — 축복을 받으면 바로 쓰는 기본 스킬(Q)과, 이야기 속 주인공의 각성(MQ_03 진실 공개 직후, Data/Story.txt의
// EVENT AWAKEN) 뒤에 열리는 궁극기(R, 쿨타임 30초). 수치와 설명은 전부 PatronPowers.cpp의 표 한 곳에 모아 둔다.
// 실제 효과(피해·기절·매혹·돌진·버프)는 게임 쪽(SimpleGame.cpp의 UseSkill)이 이 표를 읽어 적용한다.
enum class SkillSlot
{
	Basic = 0,    // Q
	Ultimate = 1, // R — 각성 뒤에만
	Count = 2,
};

struct SkillDef
{
	const char* name;    // 스킬 이름(「」 없이)
	const char* summary; // 한 줄 설명(채팅창·스킬 칸·모니터)
	float cooldown;      // 초
	float range;         // 효과 반경·사거리·돌진 거리(월드 단위)
	float power;         // 피해 = 공격력 × power(0이면 피해 없음)
	float duration;      // 기절·매혹·겁·버프 시간(초)
};

struct PatronDef
{
	const char* patron;       // PATRON 값("ZEUS" 등)
	const char* godName;      // "제우스"
	const char* blessingName; // "번개의 축복"
	float colorR, colorG, colorB; // 스킬 효과·각성의 기운·스킬 칸 테두리 색

	// 축복을 받는 순간 더해지는 상시 능력치.
	int attack;
	int maxHp;
	int defense;          // 받는 피해에서 빼는 값(최소 1은 받는다)
	float critChance;     // 치명타 확률(피해 1.5배)
	int lifeOnKill;       // 적을 쓰러뜨릴 때 회복하는 체력
	float cooldownScale;  // 공격·상호작용·기본 스킬 쿨타임 배율(작을수록 빠름)
	float speedScale;     // 이동 속도 배율
	float detectScale;    // 짐승·괴물이 알아채는 거리 배율

	// 레벨업마다 기본 성장(공격력 +3, 최대 체력 +8)에 더 얹히는 몫.
	int growthAttack;
	int growthMaxHp;
	int growthDefense;
	int growthLifeOnKill;

	const char* passiveText; // 상시 효과 요약(채팅창·모니터)
	SkillDef basic;
	SkillDef ultimate;
};

namespace PatronPowers
{
	// 궁극기의 쿨타임(초). 헤르메스의 쿨타임 감소도 궁극기엔 붙지 않는다.
	const float kUltimateCooldown = 30.f;

	// PATRON 값의 정의. 수호신이 아니면 nullptr.
	const PatronDef* Find(const std::string& patron);

	const SkillDef& Skill(const PatronDef& def, SkillSlot slot);

	// 스킬 칸·모니터용 키 이름("Q"/"R").
	const char* KeyName(SkillSlot slot);
}
