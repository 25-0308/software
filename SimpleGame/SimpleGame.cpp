/*
Copyright 2022 Lee Taek Hee (Tech University of Korea)

This program is free software: you can redistribute it and/or modify
it under the terms of the What The Hell License. Do it plz.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY.
*/

#include "stdafx.h"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <random>
#include <vector>
#include "Dependencies\glew.h"
#include "Dependencies\freeglut.h"

#include "Camera.h"
#include "GameObject.h"
#include "LevelGenerator.h"
#include "Mesh.h"
#include "MeshCache.h"
#include "PostProcess.h"
#include "Renderer.h"
#include "TileMap.h"

namespace
{
	GameObject MakeObject(float x, float y, float z, float size, float r, float g, float b, float a,
		EntityType type = EntityType::Prop, int interactId = 0, const char* name = nullptr)
	{
		GameObject obj;
		obj.x = x; obj.y = y; obj.z = z; obj.size = size;
		obj.r = r; obj.g = g; obj.b = b; obj.a = a;
		obj.type = type;
		obj.interactId = interactId;
		obj.name = name;
		return obj;
	}

	// HandleInteract()가 분기하는 상호작용 식별자. 0이면 상호작용 불가.
	const int kInteractElder = 1;
	const int kInteractQuestItem = 2;
	const int kInteractVillager = 3;
	const int kInteractLoot = 4;

	// 공격/상호작용 판정 반경. Try*()와 조준 하이라이트 렌더링이 같은 값을
	// 공유해야 "링이 보이면 실제로 닿는다"는 예측이 항상 맞는다.
	const float kAttackRadius = 1.6f;
	const float kInteractRadius = 1.4f;

	// 몬스터(공격형 짐승) AI 파라미터.
	const float kMonsterDetectRadius = 4.5f;  // 이 안에 들어오면 추적을 시작한다.
	const float kMonsterGiveUpRadius = 7.f;   // 플레이어가 이보다 멀어지면 추적을 포기한다(히스테리시스).
	const float kMonsterLeashRadius = 6.f;    // 자기 anchor에서 이보다 멀어지면 추적을 포기하고 돌아간다.
	const float kMonsterAttackRadius = 1.1f;  // 이 안에 있어야 실제로 때린다.
	const float kMonsterChaseSpeed = 2.3f;    // 플레이어(4.0)보다 느려서 도망칠 여지가 있음.
	const float kMonsterAttackCooldown = 1.1f; // 몬스터 공격 간 최소 간격(초).
	const int kMonsterAttackDamage = 6;

	// 플레이어가 몬스터에게 맞았을 때 번쩍이는 히트플래시 지속 시간(초).
	const float kPlayerHitFlashDuration = 0.2f;

	enum class QuestState
	{
		NotStarted,
		ItemRequested,
		ItemCollected,
		Completed,
	};

	// 배회하는 야생 짐승 하나. isAggressive가 false면(사슴) 실제 위치는 매 프레임
	// anchor를 중심으로 한 단순한 진자 운동으로만 갱신된다. isAggressive가
	// true면(늑대) 플레이어가 감지 범위에 들어왔을 때 직선으로 추적하고,
	// 공격 사거리에 닿으면 주기적으로 플레이어를 공격한다 (경로탐색은 아직
	// 없음 — 장애물/물은 피해서 이동하지만 우회 경로를 찾진 않는다).
	struct Animal
	{
		GameObject visual;
		float anchorX = 0.f;
		float anchorY = 0.f;
		float phase = 0.f;
		int hp = 20;
		bool alive = true;

		// 공격당한 직후 몸이 하얗게 번쩍이는 타격감 연출에 쓰는 잔여 시간(초).
		float hitFlashTimer = 0.f;

		bool isAggressive = false; // true면 플레이어를 추적/공격하는 몬스터로 동작.
		bool isChasing = false;    // 현재 추적 중인지(감지/이탈 반경에 히스테리시스를 줌).
		float attackCooldown = 0.f; // 0 이하가 될 때마다 한 번씩 플레이어를 공격.
	};

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
}

Renderer *g_Renderer = NULL;
Camera *g_Camera = NULL;
PostProcess *g_PostProcess = NULL;
TileMap *g_TileMap = NULL;

MeshHandle g_CircleMesh;
MeshHandle g_EllipseMesh; // 나무 수관, 아이템 등 둥글넓적한 파츠에 재사용하는 타원 메시

GameObject g_Player;
bool g_PlayerIsMoving = false;
PlayerStats g_PlayerStats;
float g_PlayerSpawnX = 0.f; // 사망 시 되돌아갈 마을 중심 좌표(BuildLevel에서 설정).
float g_PlayerSpawnY = 0.f;

// 공격 모션 재생 시간(초). 0보다 크면 공격 중이며, DrawCharacter가 이 값으로
// 앞팔을 정면으로 휘두르는 모션을 계산한다.
const float kAttackAnimDuration = 0.25f;
float g_PlayerAttackTimer = 0.f;

// 피격 시 하얗게 번쩍이는 연출이 재생되는 시간(초). Animal::hitFlashTimer가
// 이 값에서부터 줄어든다.
const float kHitFlashDuration = 0.15f;

// 몬스터에게 맞았을 때 플레이어 몸이 붉게 번쩍이는 연출의 잔여 시간(초).
float g_PlayerHitFlashTimer = 0.f;

std::vector<GameObject> g_TileObjects; // 바닥/길/호수 타일. 정적이며 정렬하지 않고 가장 먼저 그린다.
std::vector<GameObject> g_Entities;    // NPC, 건물, 소품, 아이템, 횃불 등
std::vector<Animal> g_Animals;         // 배회하는 야생 짐승 (공격 대상)

QuestState g_QuestState = QuestState::NotStarted;

bool g_KeyW = false, g_KeyA = false, g_KeyS = false, g_KeyD = false;

std::chrono::steady_clock::time_point g_LastFrameTime;
float g_ElapsedSeconds = 0.f;

// 사후처리 분위기 조절값: exposure(노출)는 밝은 부분이 눌리는 정도(톤매핑),
// vignetteStrength는 화면 가장자리가 어두워지는 정도, bloomThreshold는
// 발광이 시작되는 최소 밝기, bloomIntensity는 발광의 세기를 결정한다.
float g_Exposure = 1.0f;
float g_VignetteStrength = 0.6f;
float g_BloomThreshold = 0.9f;
float g_BloomIntensity = 0.8f;

namespace
{
	void GrantXP(int amount)
	{
		g_PlayerStats.xp += amount;
		std::cout << "[경험치 획득] +" << amount << " (현재 " << g_PlayerStats.xp << "/" << g_PlayerStats.xpToNext << ")\n";

		while (g_PlayerStats.xp >= g_PlayerStats.xpToNext)
		{
			g_PlayerStats.xp -= g_PlayerStats.xpToNext;
			g_PlayerStats.level += 1;
			g_PlayerStats.xpToNext = (int)(g_PlayerStats.xpToNext * 1.5f);
			g_PlayerStats.attackPower += 3;
			g_PlayerStats.maxHp += 8;
			g_PlayerStats.hp = g_PlayerStats.maxHp;

			std::cout << "[레벨업!] Lv." << g_PlayerStats.level
				<< " (공격력 " << g_PlayerStats.attackPower
				<< ", 최대체력 " << g_PlayerStats.maxHp << ")\n";
		}
	}

	// 체력이 0 이하로 떨어져 쓰러졌을 때: 마을 중심으로 되돌리고 체력을 채워
	// 다시 시작하게 한다(세이브/로드가 없는 튜토리얼 프로토타입이라 죽음 =
	// 패널티 없는 리스폰으로 단순하게 처리).
	void RespawnPlayer()
	{
		std::cout << "[쓰러졌다...] 마을로 돌아왔다.\n";
		g_Player.x = g_PlayerSpawnX;
		g_Player.y = g_PlayerSpawnY;
		g_PlayerStats.hp = g_PlayerStats.maxHp;
	}

	// 몬스터의 공격을 플레이어에게 적용한다: 데미지, 피격 플래시, 사망 시 리스폰.
	void DamagePlayer(int amount)
	{
		g_PlayerStats.hp -= amount;
		g_PlayerHitFlashTimer = kPlayerHitFlashDuration;
		std::cout << "[피격!] 몬스터에게 " << amount << " 피해를 입었다 (체력 "
			<< g_PlayerStats.hp << "/" << g_PlayerStats.maxHp << ")\n";

		if (g_PlayerStats.hp <= 0)
		{
			RespawnPlayer();
		}
	}

	// 건물/나무처럼 화면에 확실히 "덩어리"로 보이는 오브젝트와 겹치는지 검사한다.
	// 움직이는 주체(플레이어/몬스터)를 반지름 kMoverRadius의 원으로, 장애물을
	// obj.size에 비례한 반지름의 원으로 근사한 원-원 충돌 판정 — 눈에 보이는
	// 벽/나무 몸통을 그대로 통과해버리면 "부딪힐 것 같은데 안 부딪힌다"는
	// 위화감이 생기므로, 시각적 크기와 실제 충돌 범위를 최대한 맞춘다.
	bool IsBlockedByObstacle(float x, float y)
	{
		const float kMoverRadius = 0.32f;

		for (const GameObject& obj : g_Entities)
		{
			float obstacleRadius;
			if (obj.type == EntityType::Building)
			{
				obstacleRadius = obj.size * 0.55f;
			}
			else if (obj.type == EntityType::Prop)
			{
				obstacleRadius = obj.size * 0.2f; // 나무는 몸통(기둥)만 막고, 수관 아래는 지나갈 수 있음
			}
			else
			{
				continue;
			}

			float dx = x - obj.x;
			float dy = y - obj.y;
			float minDist = kMoverRadius + obstacleRadius;

			if (dx * dx + dy * dy < minDist * minDist)
			{
				return true;
			}
		}

		return false;
	}

	// 비공격형 짐승(사슴)의 기본 이동, 그리고 공격형 짐승(늑대)이 추적 중이
	// 아닐 때의 기본 이동: anchor를 중심으로 한 단순한 진자 운동. 실제로
	// 이동한 방향을 바라보게 해서 배회 중에도 살아있는 느낌을 준다.
	void WanderAnimal(Animal& animal)
	{
		float prevX = animal.visual.x;
		float prevY = animal.visual.y;
		animal.visual.x = animal.anchorX + sinf(g_ElapsedSeconds * 0.4f + animal.phase) * 1.5f;
		animal.visual.y = animal.anchorY + cosf(g_ElapsedSeconds * 0.25f + animal.phase) * 1.0f;

		float vx = animal.visual.x - prevX;
		float vy = animal.visual.y - prevY;
		if (vx * vx + vy * vy > 0.000001f)
		{
			animal.visual.facing = atan2f(vy, vx);
		}
	}

	// 공격형 짐승(늑대)의 추적/공격 AI. 감지 반경 안에 플레이어가 들어오면
	// 추적을 시작하고, 공격 사거리에 닿으면 쿨다운마다 플레이어를 공격한다.
	// 플레이어가 너무 멀어지거나(kMonsterGiveUpRadius) 자기 anchor에서 너무
	// 멀어지면(kMonsterLeashRadius) 추적을 포기하고 배회로 돌아간다.
	void UpdateMonsterAI(Animal& animal, float deltaSeconds)
	{
		float dxPlayer = g_Player.x - animal.visual.x;
		float dyPlayer = g_Player.y - animal.visual.y;
		float distToPlayerSq = dxPlayer * dxPlayer + dyPlayer * dyPlayer;

		float dxAnchor = animal.visual.x - animal.anchorX;
		float dyAnchor = animal.visual.y - animal.anchorY;
		float distFromAnchorSq = dxAnchor * dxAnchor + dyAnchor * dyAnchor;

		// 감지 반경과 이탈 반경을 다르게 둬서(히스테리시스) 경계선에서
		// 추적을 시작/포기를 반복하며 떠는 것을 막는다.
		if (!animal.isChasing && distToPlayerSq <= kMonsterDetectRadius * kMonsterDetectRadius)
		{
			animal.isChasing = true;
		}
		else if (animal.isChasing && (distToPlayerSq > kMonsterGiveUpRadius * kMonsterGiveUpRadius ||
			distFromAnchorSq > kMonsterLeashRadius * kMonsterLeashRadius))
		{
			animal.isChasing = false;
		}

		if (!animal.isChasing)
		{
			WanderAnimal(animal);
			return;
		}

		float distToPlayer = sqrtf(distToPlayerSq);
		if (distToPlayer > kMonsterAttackRadius * 0.6f) // 바짝 붙었을 때 제자리에서 미세하게 떨지 않도록 여유를 둠.
		{
			float dirX = dxPlayer / distToPlayer;
			float dirY = dyPlayer / distToPlayer;
			float step = kMonsterChaseSpeed * deltaSeconds;

			float newX = animal.visual.x + dirX * step;
			if (g_TileMap->IsWorldPositionWalkable(newX, animal.visual.y) && !IsBlockedByObstacle(newX, animal.visual.y))
			{
				animal.visual.x = newX;
			}

			float newY = animal.visual.y + dirY * step;
			if (g_TileMap->IsWorldPositionWalkable(animal.visual.x, newY) && !IsBlockedByObstacle(animal.visual.x, newY))
			{
				animal.visual.y = newY;
			}
		}

		animal.visual.facing = atan2f(dyPlayer, dxPlayer);

		if (distToPlayer <= kMonsterAttackRadius && animal.attackCooldown <= 0.f)
		{
			DamagePlayer(kMonsterAttackDamage);
			animal.attackCooldown = kMonsterAttackCooldown;
		}
	}

	// 공격 사거리(kAttackRadius) 안에 있는 살아있는 짐승 중 가장 가까운 것을
	// 찾는다. TryAttack()과 "지금 누굴 때릴지" 조준 하이라이트가 이 함수를
	// 공유해서, 화면에 보이는 조준 표시와 실제 공격 결과가 항상 일치하게 한다.
	Animal* FindNearestAttackTarget()
	{
		float bestDistSq = kAttackRadius * kAttackRadius;
		Animal* best = NULL;

		for (Animal& animal : g_Animals)
		{
			if (!animal.alive)
			{
				continue;
			}

			float dx = animal.visual.x - g_Player.x;
			float dy = animal.visual.y - g_Player.y;
			float distSq = dx * dx + dy * dy;

			if (distSq < bestDistSq)
			{
				bestDistSq = distSq;
				best = &animal;
			}
		}

		return best;
	}

	void TryAttack()
	{
		Animal* target = FindNearestAttackTarget();

		// 대상 유무와 상관없이 휘두르는 모션은 재생한다 (허공을 가르더라도
		// 입력에 대한 시각적 반응이 있어야 손맛이 느껴짐).
		g_PlayerAttackTimer = kAttackAnimDuration;

		if (target == NULL)
		{
			std::cout << "[허공을 가른다]\n";
			return;
		}

		// 공격 모션이 대상 쪽을 향하도록 캐릭터 방향을 맞춘다.
		g_Player.facing = atan2f(target->visual.y - g_Player.y, target->visual.x - g_Player.x);

		target->hp -= g_PlayerStats.attackPower;
		target->hitFlashTimer = kHitFlashDuration;
		std::cout << "[공격!] 짐승에게 " << g_PlayerStats.attackPower << " 피해\n";

		if (target->hp <= 0)
		{
			target->alive = false;
			std::cout << "[짐승을 쓰러뜨렸다]\n";
			GrantXP(30);
		}
	}

	// index 위치의 엔티티와 상호작용한다. 아이템 획득처럼 그 엔티티 하나만
	// 없애야 하는 경우 g_Entities에서 정확히 이 인덱스만 지운다 (동일한
	// interactId를 가진 다른 아이템까지 같이 사라지지 않도록).
	void HandleInteract(int index)
	{
		int id = g_Entities[index].interactId;

		if (id == kInteractElder)
		{
			if (g_QuestState == QuestState::NotStarted)
			{
				std::cout << "[장로] 호수 근처에서 잃어버린 제물을 찾아다오.\n";
				g_QuestState = QuestState::ItemRequested;
			}
			else if (g_QuestState == QuestState::ItemRequested)
			{
				std::cout << "[장로] 아직 제물을 찾지 못했구나. 호수 쪽을 살펴보게.\n";
			}
			else if (g_QuestState == QuestState::ItemCollected)
			{
				std::cout << "[장로] 오, 찾아왔구나! 그대에게 작은 축복을 내리네.\n";
				g_QuestState = QuestState::Completed;
				GrantXP(50);
				// 보상: 플레이어가 블룸이 걸릴 만큼 밝아진다 (작은 시각적 보상).
				g_Player.r = 2.5f; g_Player.g = 2.2f; g_Player.b = 1.6f;
			}
			else
			{
				std::cout << "[장로] 마을을 지켜줘서 고맙네.\n";
			}
		}
		else if (id == kInteractQuestItem)
		{
			if (g_QuestState == QuestState::ItemRequested)
			{
				std::cout << "[잃어버린 제물을 주웠다. 장로에게 가져다주자.]\n";
				g_QuestState = QuestState::ItemCollected;
				g_Entities.erase(g_Entities.begin() + index);
			}
			else
			{
				std::cout << "[이미 조사했다.]\n";
			}
		}
		else if (id == kInteractVillager)
		{
			static const char* lines[] =
			{
				"[마을 사람] 요즘 호수 근처가 뒤숭숭하다더군.",
				"[마을 사람] 장로님과 이야기해보게.",
				"[마을 사람] 숲 속엔 함부로 들어가지 않는 게 좋을걸세.",
				"[마을 사람] 밤이 되면 늑대 울음소리가 들린다네.",
			};
			static int lineIndex = 0;
			std::cout << lines[lineIndex % 4] << "\n";
			++lineIndex;
		}
		else if (id == kInteractLoot)
		{
			std::cout << "[약초를 발견해 챙겼다]\n";
			g_Entities.erase(g_Entities.begin() + index);
			GrantXP(15);
		}
	}

	// 상호작용 사거리(kInteractRadius) 안에 있는 상호작용 가능 엔티티 중 가장
	// 가까운 것의 g_Entities 인덱스를 찾는다(없으면 -1). TryInteract()와 "지금
	// 뭘 상호작용할지" 조준 하이라이트가 이 함수를 공유한다.
	int FindNearestInteractableIndex()
	{
		float bestDistSq = kInteractRadius * kInteractRadius;
		int bestIndex = -1;

		for (size_t i = 0; i < g_Entities.size(); ++i)
		{
			const GameObject& obj = g_Entities[i];
			if (obj.interactId == 0)
			{
				continue;
			}

			float dx = obj.x - g_Player.x;
			float dy = obj.y - g_Player.y;
			float distSq = dx * dx + dy * dy;

			if (distSq < bestDistSq)
			{
				bestDistSq = distSq;
				bestIndex = (int)i;
			}
		}

		return bestIndex;
	}

	void TryInteract()
	{
		int bestIndex = FindNearestInteractableIndex();

		if (bestIndex < 0)
		{
			std::cout << "[근처에 상호작용할 대상이 없습니다]\n";
			return;
		}

		HandleInteract(bestIndex);
	}

	// 캐릭터(플레이어/NPC/짐승)를 몸통+머리(원형 메시)+팔다리 여러 파츠로 그린다.
	// animateWalk가 true면 팔다리가 좌우로 흔들리는 절차적 걷기 애니메이션이 재생되고,
	// attackProgress(0~1)가 0 이상이면 앞팔이 obj.facing 방향으로 휘둘러지는 공격
	// 모션이 걷기 애니메이션 위에 덧씌워진다 (음수면 공격 중이 아님).
	void DrawCharacter(const GameObject& obj, const Mat4& viewProjection, bool animateWalk, float attackProgress)
	{
		float breathe = sinf(g_ElapsedSeconds * 4.f + obj.x * 3.1f) * 0.03f; // 개체마다 위상이 다른 미세한 숨쉬기 움직임
		float legSwing = animateWalk ? sinf(g_ElapsedSeconds * 8.f) * 0.15f : 0.f;
		float walkBob = animateWalk ? fabsf(sinf(g_ElapsedSeconds * 8.f)) * 0.05f : 0.f; // 걷는 동안의 상하 들썩임

		// obj.facing 기준 정면/측면 단위 벡터. 다리·팔을 이 축으로 배치해서
		// 캐릭터가 이동/공격 방향으로 실제로 돌아보는 것처럼 보이게 한다.
		float forwardX = cosf(obj.facing), forwardY = sinf(obj.facing);
		float rightX = -forwardY, rightY = forwardX;

		// 사람(플레이어/NPC)은 옷 색(obj.r/g/b)과 상관없이 머리·팔은 피부색,
		// 다리는 중립 톤 바지색으로 통일한다 — "이게 머리다/팔이다/다리다"가
		// 옷 색조 변형(예전엔 obj.r*0.6 같은 명암 차이뿐)보다 한눈에 더 잘
		// 들어와서 단순하고 직관적으로 읽힌다. 짐승은 지금처럼 몸통 색에서
		// 파생된 톤(밝은 머리/어두운 다리)을 그대로 써서 털 색처럼 보이게 한다.
		bool isHumanoid = (obj.type != EntityType::Animal);
		const float kSkinR = 0.92f, kSkinG = 0.78f, kSkinB = 0.62f;
		const float kPantsR = 0.24f, kPantsG = 0.22f, kPantsB = 0.26f;

		float headR = isHumanoid ? kSkinR : (obj.r * 1.2f + 0.1f);
		float headG = isHumanoid ? kSkinG : (obj.g * 1.2f + 0.1f);
		float headB = isHumanoid ? kSkinB : (obj.b * 1.2f + 0.1f);

		float limbR = isHumanoid ? kSkinR : (obj.r * 0.75f);
		float limbG = isHumanoid ? kSkinG : (obj.g * 0.75f);
		float limbB = isHumanoid ? kSkinB : (obj.b * 0.75f);

		float legR = isHumanoid ? kPantsR : (obj.r * 0.6f);
		float legG = isHumanoid ? kPantsG : (obj.g * 0.6f);
		float legB = isHumanoid ? kPantsB : (obj.b * 0.6f);

		// 몸통
		Mat4 body = Mat4::Translate(obj.x, obj.y, obj.z + obj.size * 0.5f + breathe + walkBob)
			* Mat4::Scale(obj.size * 0.55f, obj.size * 0.35f, obj.size * 0.7f);
		g_Renderer->DrawObject(viewProjection * body, obj.r, obj.g, obj.b, obj.a);

		// 머리 (원형 메시)
		Mat4 head = Mat4::Translate(obj.x, obj.y, obj.z + obj.size * 0.95f + breathe + walkBob)
			* Mat4::Scale(obj.size * 0.4f, obj.size * 0.4f, obj.size * 0.4f);
		g_Renderer->DrawMesh(g_CircleMesh, viewProjection * head, headR, headG, headB, obj.a);

		// 다리 2개: right축으로 벌리고, 걷는 동안 forward축으로 서로 반대로 흔들림.
		Mat4 legLeft = Mat4::Translate(obj.x - rightX * obj.size * 0.15f + forwardX * legSwing, obj.y - rightY * obj.size * 0.15f + forwardY * legSwing, obj.z + obj.size * 0.15f)
			* Mat4::Scale(obj.size * 0.18f, obj.size * 0.18f, obj.size * 0.3f);
		Mat4 legRight = Mat4::Translate(obj.x + rightX * obj.size * 0.15f - forwardX * legSwing, obj.y + rightY * obj.size * 0.15f - forwardY * legSwing, obj.z + obj.size * 0.15f)
			* Mat4::Scale(obj.size * 0.18f, obj.size * 0.18f, obj.size * 0.3f);
		g_Renderer->DrawObject(viewProjection * legLeft, legR, legG, legB, obj.a);
		g_Renderer->DrawObject(viewProjection * legRight, legR, legG, legB, obj.a);

		// 팔 2개: 걷는 동안은 다리와 반대 위상으로 흔들리고, 공격 중에는 오른팔이
		// facing 방향으로 크게 휘둘러진다.
		float armWalkSwing = animateWalk ? -legSwing : 0.f;
		float attackSwing = (attackProgress >= 0.f) ? sinf(attackProgress * 3.14159265f) * obj.size * 0.6f : 0.f;

		Mat4 armLeft = Mat4::Translate(obj.x - rightX * obj.size * 0.42f + forwardX * armWalkSwing, obj.y - rightY * obj.size * 0.42f + forwardY * armWalkSwing, obj.z + obj.size * 0.55f)
			* Mat4::Scale(obj.size * 0.14f, obj.size * 0.14f, obj.size * 0.3f);
		Mat4 armRight = Mat4::Translate(obj.x + rightX * obj.size * 0.42f - forwardX * armWalkSwing + forwardX * attackSwing, obj.y + rightY * obj.size * 0.42f - forwardY * armWalkSwing + forwardY * attackSwing, obj.z + obj.size * 0.55f)
			* Mat4::Scale(obj.size * 0.14f, obj.size * 0.14f, obj.size * 0.3f);
		g_Renderer->DrawObject(viewProjection * armLeft, limbR, limbG, limbB, obj.a);
		g_Renderer->DrawObject(viewProjection * armRight, limbR, limbG, limbB, obj.a);
	}

	// 건물을 벽(나무 재질)+지붕(어두운 갈색 재질) 두 파츠로 그린다.
	void DrawBuilding(const GameObject& obj, const Mat4& viewProjection)
	{
		Mat4 wall = Mat4::Translate(obj.x, obj.y, obj.z + obj.size * 0.45f)
			* Mat4::Scale(obj.size, obj.size * 0.9f, obj.size * 0.9f);
		g_Renderer->DrawObject(viewProjection * wall, 0.55f, 0.40f, 0.28f, 1.f);

		Mat4 roof = Mat4::Translate(obj.x, obj.y, obj.z + obj.size * 0.95f)
			* Mat4::Scale(obj.size * 1.15f, obj.size * 1.15f, obj.size * 0.5f);
		g_Renderer->DrawObject(viewProjection * roof, 0.32f, 0.16f, 0.10f, 1.f);
	}

	// 나무를 몸통(사각 기둥)+둥근 수관(타원 메시 두 겹) 조합으로 그린다.
	// 위쪽 수관은 아래쪽보다 살짝 밝은 톤으로 겹쳐서 입체감을 준다.
	void DrawTree(const GameObject& obj, const Mat4& viewProjection)
	{
		float sway = sinf(g_ElapsedSeconds * 0.6f + obj.x * 2.1f) * obj.size * 0.03f; // 산들바람에 흔들리는 느낌

		Mat4 trunk = Mat4::Translate(obj.x, obj.y, obj.z + obj.size * 0.28f)
			* Mat4::Scale(obj.size * 0.16f, obj.size * 0.16f, obj.size * 0.55f);
		g_Renderer->DrawObject(viewProjection * trunk, 0.35f, 0.22f, 0.12f, 1.f);

		Mat4 canopyLow = Mat4::Translate(obj.x + sway, obj.y, obj.z + obj.size * 0.68f)
			* Mat4::Scale(obj.size * 0.85f, obj.size * 0.85f, obj.size * 0.85f);
		g_Renderer->DrawMesh(g_EllipseMesh, viewProjection * canopyLow, obj.r, obj.g, obj.b, obj.a);

		Mat4 canopyHigh = Mat4::Translate(obj.x + sway * 1.4f, obj.y, obj.z + obj.size * 0.98f)
			* Mat4::Scale(obj.size * 0.55f, obj.size * 0.55f, obj.size * 0.55f);
		g_Renderer->DrawMesh(g_EllipseMesh, viewProjection * canopyHigh, obj.r * 1.2f + 0.05f, obj.g * 1.15f + 0.05f, obj.b * 1.1f, obj.a);
	}

	// 아이템(약초/퀘스트 아이템)을 작은 타원(잎·보석 모양)으로 그리고, 제자리에서
	// 은은하게 위아래로 떠다니게 해서 사각 타일과 구분되는 픽업 느낌을 준다.
	void DrawItem(const GameObject& obj, const Mat4& viewProjection)
	{
		float bob = sinf(g_ElapsedSeconds * 3.f + obj.x * 4.f) * obj.size * 0.15f;

		Mat4 model = Mat4::Translate(obj.x, obj.y, obj.z + obj.size * 0.4f + bob)
			* Mat4::Scale(obj.size, obj.size, obj.size);
		g_Renderer->DrawMesh(g_EllipseMesh, viewProjection * model, obj.r, obj.g, obj.b, obj.a);
	}

	// 플레이어 발밑에 반투명한 타원을 깔고 은은하게 펄스시켜서, 세워진 카메라
	// 각도에서도 플레이어 위치를 한눈에 짚을 수 있게 하는 장식용 마커.
	void DrawPlayerMarker(const GameObject& player, const Mat4& viewProjection)
	{
		float pulse = 0.5f + 0.5f * sinf(g_ElapsedSeconds * 2.5f);
		float radius = player.size * (0.8f + pulse * 0.1f);

		Mat4 model = Mat4::Translate(player.x, player.y, 0.0005f) * Mat4::Scale(radius, radius * 0.7f, 1.f);
		g_Renderer->DrawMesh(g_EllipseMesh, viewProjection * model, 1.0f, 0.92f, 0.6f, 0.14f + pulse * 0.08f);
	}

	// 공격/상호작용 사거리 안에 들어온 대상 발밑에 조준 링을 그려서, 버튼을
	// 누르기 전부터 "지금 뭐가 맞을지/상호작용될지"를 미리 보여준다.
	// FindNearest*() 함수를 Try*()와 그대로 공유하므로, 여기 표시되는 대상과
	// 실제로 Space/E를 눌렀을 때의 결과가 항상 일치한다.
	void DrawTargetHighlights(const Mat4& viewProjection)
	{
		float pulse = 0.5f + 0.5f * sinf(g_ElapsedSeconds * 6.f);

		Animal* attackTarget = FindNearestAttackTarget();
		if (attackTarget != NULL)
		{
			float radius = attackTarget->visual.size * (0.75f + pulse * 0.15f);
			Mat4 model = Mat4::Translate(attackTarget->visual.x, attackTarget->visual.y, 0.0006f)
				* Mat4::Scale(radius, radius * 0.7f, 1.f);
			g_Renderer->DrawMesh(g_EllipseMesh, viewProjection * model, 1.0f, 0.2f, 0.15f, 0.3f + pulse * 0.2f);
		}

		int interactIndex = FindNearestInteractableIndex();
		if (interactIndex >= 0)
		{
			const GameObject& obj = g_Entities[interactIndex];
			float radius = obj.size * (0.85f + pulse * 0.15f);
			Mat4 model = Mat4::Translate(obj.x, obj.y, 0.0006f) * Mat4::Scale(radius, radius * 0.7f, 1.f);
			g_Renderer->DrawMesh(g_EllipseMesh, viewProjection * model, 0.3f, 0.9f, 1.0f, 0.28f + pulse * 0.18f);
		}
	}

	// 플레이어/NPC/짐승 머리 위에 이름표를 그린다. 이 엔진엔 자체 폰트가 없어서
	// freeglut이 내장한 비트맵 폰트(glutBitmapCharacter)를 그대로 쓴다 — 이건
	// 셰이더가 아니라 옛 고정기능 래스터 경로라, 우리 커스텀 셰이더 렌더러와는
	// 별개로 동작한다. 화면 위치는 월드 좌표를 직접 NDC로 계산해서
	// glRasterPos에 넘긴다(레거시 모델뷰/프로젝션 행렬은 이 프로젝트 어디서도
	// 건드리지 않아 항등행렬 상태이므로, NDC 좌표를 그대로 써도 정확히 맞는다).
	// 후처리(블룸 등)에 안 물들도록 HUD와 마찬가지로 EndCaptureAndPresent() 이후
	// 기본 프레임버퍼에 그린다.
	void DrawNameTags(const std::vector<GameObject>& objects, const Mat4& viewProjection)
	{
		glUseProgram(0);
		glColor3f(1.f, 1.f, 1.f);

		for (const GameObject& obj : objects)
		{
			if (obj.name == NULL)
			{
				continue;
			}
			if (obj.type != EntityType::Player && obj.type != EntityType::NPC && obj.type != EntityType::Animal)
			{
				continue;
			}

			float ndcX, ndcY;
			TransformToNDC(viewProjection, obj.x, obj.y, obj.z + obj.size * 1.3f, ndcX, ndcY);

			if (ndcX < -1.1f || ndcX > 1.1f || ndcY < -1.1f || ndcY > 1.1f)
			{
				continue; // 화면 밖이면 그리지 않음.
			}

			int textWidth = 0;
			for (const char* c = obj.name; *c != '\0'; ++c)
			{
				textWidth += glutBitmapWidth(GLUT_BITMAP_HELVETICA_10, *c);
			}

			// 텍스트 폭의 절반만큼 왼쪽으로 밀어서 가운데 정렬한다
			// (픽셀→NDC 변환: 창 폭 800의 절반이 NDC 1.0에 대응).
			float centeredNdcX = ndcX - (float)textWidth / 800.f;

			glRasterPos2f(centeredNdcX, ndcY);
			for (const char* c = obj.name; *c != '\0'; ++c)
			{
				glutBitmapCharacter(GLUT_BITMAP_HELVETICA_10, *c);
			}
		}
	}

	// ---- 화면 고정 HUD (레벨 배지 + 체력바 + 경험치바) ----
	// 아직 폰트/텍스트 렌더링 파이프라인이 없어서, 레벨 숫자는 계산기 표시창처럼
	// 사각형 세그먼트 조각으로 그린다(0~9, 7세그먼트 방식). 나머지는 사각형
	// 막대뿐이라 별도 메시 없이 Renderer::DrawObject만으로 그릴 수 있다.

	// 자릿수 0~9의 on/off 세그먼트 비트마스크(bit0=위, bit1=오른쪽위, bit2=오른쪽아래,
	// bit3=아래, bit4=왼쪽아래, bit5=왼쪽위, bit6=가운데). 표준 7세그먼트 인코딩.
	const unsigned char kDigitSegments[10] = { 0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F };

	void DrawDigit(int digit, float centerX, float centerY, float w, float h, float r, float g, float b, const Mat4& uiProjection)
	{
		if (digit < 0 || digit > 9)
		{
			return;
		}

		unsigned char segs = kDigitSegments[digit];
		float thickness = w * 0.24f;
		float halfW = w * 0.5f;
		float halfH = h * 0.5f;
		float armLength = halfH - thickness * 0.5f;

		struct Segment { unsigned char bit; float x, y, w, h; };
		Segment segments[7] =
		{
			{ 0x01, 0.f,               halfH - thickness * 0.5f, w - thickness, thickness },  // 위
			{ 0x40, 0.f,               0.f,                      w - thickness, thickness },  // 가운데
			{ 0x08, 0.f,              -halfH + thickness * 0.5f, w - thickness, thickness },  // 아래
			{ 0x20, -halfW + thickness * 0.5f,  halfH * 0.5f,    thickness,     armLength },  // 왼쪽위
			{ 0x02,  halfW - thickness * 0.5f,  halfH * 0.5f,    thickness,     armLength },  // 오른쪽위
			{ 0x10, -halfW + thickness * 0.5f, -halfH * 0.5f,    thickness,     armLength },  // 왼쪽아래
			{ 0x04,  halfW - thickness * 0.5f, -halfH * 0.5f,    thickness,     armLength },  // 오른쪽아래
		};

		for (const Segment& seg : segments)
		{
			if ((segs & seg.bit) == 0)
			{
				continue;
			}

			Mat4 model = Mat4::Translate(centerX + seg.x, centerY + seg.y, 0.f) * Mat4::Scale(seg.w, seg.h, 1.f);
			g_Renderer->DrawObject(uiProjection * model, r, g, b, 1.f);
		}
	}

	// 음이 아닌 정수를 여러 자릿수 DrawDigit로 이어 그린다. left는 첫 자릿수의
	// 왼쪽 끝 x좌표.
	void DrawNumber(int value, float left, float centerY, float digitW, float digitH, float gap, float r, float g, float b, const Mat4& uiProjection)
	{
		if (value < 0)
		{
			value = 0;
		}

		char digits[8];
		int digitCount = 0;
		int remaining = value;
		do
		{
			digits[digitCount++] = (char)(remaining % 10);
			remaining /= 10;
		}
		while (remaining > 0 && digitCount < 8);

		// digits[]엔 낮은 자리부터 들어있으므로 뒤에서부터(높은 자리부터) 그린다.
		float x = left + digitW * 0.5f;
		for (int i = digitCount - 1; i >= 0; --i)
		{
			DrawDigit(digits[i], x, centerY, digitW, digitH, r, g, b, uiProjection);
			x += digitW + gap;
		}
	}

	// 왼쪽 정렬로 fraction(0~1)만큼 채워지는 막대: 테두리+배경+채움 3겹.
	void DrawBar(float left, float centerY, float width, float height, float fraction, float r, float g, float b, const Mat4& uiProjection)
	{
		if (fraction < 0.f) fraction = 0.f;
		if (fraction > 1.f) fraction = 1.f;

		Mat4 border = Mat4::Translate(left + width * 0.5f, centerY, 0.f) * Mat4::Scale(width + 4.f, height + 4.f, 1.f);
		g_Renderer->DrawObject(uiProjection * border, 0.05f, 0.05f, 0.06f, 0.85f);

		Mat4 bg = Mat4::Translate(left + width * 0.5f, centerY, 0.f) * Mat4::Scale(width, height, 1.f);
		g_Renderer->DrawObject(uiProjection * bg, 0.16f, 0.16f, 0.2f, 0.9f);

		if (fraction > 0.001f)
		{
			float fillWidth = width * fraction;
			Mat4 fill = Mat4::Translate(left + fillWidth * 0.5f, centerY, 0.f) * Mat4::Scale(fillWidth, height - 4.f, 1.f);
			g_Renderer->DrawObject(uiProjection * fill, r, g, b, 1.f);
		}
	}

	// 화면 왼쪽 위 HUD: 레벨 배지(원+숫자) + 체력바(빨강) + 경험치바(하늘색).
	// 카메라/줌/포스트프로세싱과 무관하게 항상 같은 화면 픽셀 위치에 그리기
	// 위해, 월드 카메라가 아닌 별도의 화면 좌표계 투영(uiProjection)을 쓴다.
	void DrawHUD()
	{
		Mat4 uiProjection = Mat4::Ortho(0.f, 800.f, 0.f, 600.f, -1.f, 1.f);

		const float kBadgeX = 40.f;
		const float kBadgeY = 560.f;
		const float kBarLeft = 74.f;
		const float kBarWidth = 190.f;

		// 레벨 배지: 어두운 원판 위에 숫자.
		Mat4 badge = Mat4::Translate(kBadgeX, kBadgeY, 0.f) * Mat4::Scale(52.f, 52.f, 1.f);
		g_Renderer->DrawMesh(g_CircleMesh, uiProjection * badge, 0.12f, 0.1f, 0.08f, 0.9f);

		if (g_PlayerStats.level < 10)
		{
			DrawDigit(g_PlayerStats.level, kBadgeX, kBadgeY, 20.f, 32.f, 0.95f, 0.85f, 0.35f, uiProjection);
		}
		else
		{
			DrawNumber(g_PlayerStats.level, kBadgeX - 18.f, kBadgeY, 16.f, 30.f, 3.f, 0.95f, 0.85f, 0.35f, uiProjection);
		}

		// 체력바(빨강): 위쪽. 경험치바(하늘색): 그 아래 조금 더 얇게.
		float hpFraction = (g_PlayerStats.maxHp > 0) ? (float)g_PlayerStats.hp / (float)g_PlayerStats.maxHp : 0.f;
		DrawBar(kBarLeft, kBadgeY + 10.f, kBarWidth, 20.f, hpFraction, 0.82f, 0.18f, 0.18f, uiProjection);

		float xpFraction = (g_PlayerStats.xpToNext > 0) ? (float)g_PlayerStats.xp / (float)g_PlayerStats.xpToNext : 0.f;
		DrawBar(kBarLeft, kBadgeY - 14.f, kBarWidth, 12.f, xpFraction, 0.35f, 0.68f, 0.95f, uiProjection);
	}
}

void RenderScene(void)
{
	g_PostProcess->BeginCapture();

	glClearColor(0.05f, 0.05f, 0.08f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	Mat4 viewProjection = g_Camera->GetViewProjection();

	// 바닥 타일: 서로 겹치지 않으므로 깊이 정렬이 필요 없다. 물 타일만 일렁이는
	// 전용 셰이더로 그린다.
	for (const GameObject& tile : g_TileObjects)
	{
		Mat4 model = Mat4::Translate(tile.x, tile.y, tile.z) * Mat4::Scale(tile.size, tile.size, tile.size);
		Mat4 mvp = viewProjection * model;

		if (tile.type == EntityType::Water)
		{
			float phase = tile.x * 1.7f + tile.y * 2.3f;
			g_Renderer->DrawWater(mvp, tile.r, tile.g, tile.b, tile.a, g_ElapsedSeconds, phase);
		}
		else
		{
			g_Renderer->DrawObject(mvp, tile.r, tile.g, tile.b, tile.a);
		}
	}

	// 플레이어 발밑에 옅게 펄스하는 마커. 카메라가 위에서 내려다보는 각도라
	// 자기 위치를 놓치기 쉬운데, 이 마커가 눈에 잘 띄는 기준점이 되어준다.
	DrawPlayerMarker(g_Player, viewProjection);

	// 지금 Space/E를 누르면 뭐가 맞을지/상호작용될지 미리 보여주는 조준 링.
	DrawTargetHighlights(viewProjection);

	// 이 씬은 2.5D이고 실제 3D 깊이버퍼가 아니므로, 매 프레임 월드 깊이
	// (y + z) 기준으로 정렬한 뒤 뒤에서 앞 순서로 그린다 (페인터 알고리즘).
	std::vector<GameObject> drawOrder = g_Entities;
	for (const Animal& animal : g_Animals)
	{
		if (!animal.alive)
		{
			continue;
		}

		// 피격 직후엔 원래 색을 하얀색 쪽으로 밀어서 잠깐 번쩍이게 한다
		// (Animal 자체의 색은 그대로 두고, 그리기용 사본만 밝힌다).
		GameObject flashed = animal.visual;
		if (animal.hitFlashTimer > 0.f)
		{
			float flash = animal.hitFlashTimer / kHitFlashDuration;
			flashed.r += (1.f - flashed.r) * flash;
			flashed.g += (1.f - flashed.g) * flash;
			flashed.b += (1.f - flashed.b) * flash;
		}
		drawOrder.push_back(flashed);
	}

	// 몬스터에게 맞은 직후엔 플레이어 색을 붉은 쪽으로 밀어서 피격을 알린다
	// (동물의 흰색 히트플래시와 구분되는 톤이라 "누가 맞았는지" 헷갈리지 않음).
	GameObject flashedPlayer = g_Player;
	if (g_PlayerHitFlashTimer > 0.f)
	{
		float flash = g_PlayerHitFlashTimer / kPlayerHitFlashDuration;
		flashedPlayer.r += (1.f - flashedPlayer.r) * flash;
		flashedPlayer.g -= flashedPlayer.g * flash * 0.7f;
		flashedPlayer.b -= flashedPlayer.b * flash * 0.7f;
	}
	drawOrder.push_back(flashedPlayer);

	std::sort(drawOrder.begin(), drawOrder.end(), [](const GameObject& lhs, const GameObject& rhs)
	{
		return (lhs.y + lhs.z) < (rhs.y + rhs.z);
	});

	// 그림자를 먼저 그려 캐릭터/건물 발밑에 깔리도록 한다.
	for (const GameObject& obj : drawOrder)
	{
		if (obj.type == EntityType::Player || obj.type == EntityType::NPC ||
			obj.type == EntityType::Animal || obj.type == EntityType::Building ||
			obj.type == EntityType::Prop)
		{
			Mat4 shadowModel = Mat4::Translate(obj.x, obj.y, 0.001f) * Mat4::Scale(obj.size * 0.9f, obj.size * 0.6f, 1.f);
			g_Renderer->DrawShadow(viewProjection * shadowModel, 0.45f);
		}
	}

	for (const GameObject& obj : drawOrder)
	{
		switch (obj.type)
		{
		case EntityType::Player:
			{
				float attackProgress = (g_PlayerAttackTimer > 0.f) ? (1.f - g_PlayerAttackTimer / kAttackAnimDuration) : -1.f;
				DrawCharacter(obj, viewProjection, g_PlayerIsMoving, attackProgress);
			}
			break;
		case EntityType::NPC:
			DrawCharacter(obj, viewProjection, false, -1.f);
			break;
		case EntityType::Animal:
			DrawCharacter(obj, viewProjection, true, -1.f);
			break;
		case EntityType::Building:
			DrawBuilding(obj, viewProjection);
			break;
		case EntityType::Prop:
			DrawTree(obj, viewProjection);
			break;
		case EntityType::Item:
			DrawItem(obj, viewProjection);
			break;
		case EntityType::Fire:
			{
				Mat4 model = Mat4::Translate(obj.x, obj.y, obj.z) * Mat4::Scale(obj.size, obj.size, obj.size);
				float phase = obj.x * 1.3f + obj.y * 0.7f;
				g_Renderer->DrawFire(viewProjection * model, g_ElapsedSeconds, phase);
			}
			break;
		default:
			{
				Mat4 model = Mat4::Translate(obj.x, obj.y, obj.z) * Mat4::Scale(obj.size, obj.size, obj.size);
				g_Renderer->DrawObject(viewProjection * model, obj.r, obj.g, obj.b, obj.a);
			}
			break;
		}
	}

	g_PostProcess->EndCaptureAndPresent(g_Exposure, g_VignetteStrength, g_BloomThreshold, g_BloomIntensity, g_ElapsedSeconds);

	// HUD/이름표는 후처리(블룸/비네트/그레인)가 이미 끝난 기본 프레임버퍼 위에
	// 그대로 덧그려서, 화면 흔들림/줌/색보정의 영향을 받지 않고 항상 또렷하게
	// 보이게 한다.
	DrawNameTags(drawOrder, viewProjection);
	DrawHUD();

	glutSwapBuffers();
}

void Update(float deltaSeconds)
{
	g_ElapsedSeconds += deltaSeconds;

	float moveX = 0.f, moveY = 0.f;
	if (g_KeyW) moveY += 1.f;
	if (g_KeyS) moveY -= 1.f;
	if (g_KeyA) moveX -= 1.f;
	if (g_KeyD) moveX += 1.f;

	g_PlayerIsMoving = (moveX != 0.f || moveY != 0.f);

	if (g_PlayerIsMoving)
	{
		float length = sqrtf(moveX * moveX + moveY * moveY);
		moveX /= length;
		moveY /= length;
		g_Player.facing = atan2f(moveY, moveX);

		const float kPlayerSpeed = 4.f; // 초당 이동 거리(월드 단위)
		float step = kPlayerSpeed * deltaSeconds;

		// 축별로 따로 검사해서, 물가/건물/나무 벽에 붙어 미끄러지듯 이동하되
		// 그 위로는 올라갈 수 없게 한다 (호수·건물·나무 = 접근 불가 영역).
		// 화면에 덩어리로 보이는 것(건물/나무)은 실제로도 막혀야 "부딪힐 것
		// 같으면 부딪힌다"는 예측이 항상 맞는다.
		float newX = g_Player.x + moveX * step;
		if (g_TileMap->IsWorldPositionWalkable(newX, g_Player.y) && !IsBlockedByObstacle(newX, g_Player.y))
		{
			g_Player.x = newX;
		}

		float newY = g_Player.y + moveY * step;
		if (g_TileMap->IsWorldPositionWalkable(g_Player.x, newY) && !IsBlockedByObstacle(g_Player.x, newY))
		{
			g_Player.y = newY;
		}

		const float kWorldHalfExtent = 15.5f; // 32x32 맵(반너비 16)보다 살짝 안쪽으로 클램프.
		if (g_Player.x < -kWorldHalfExtent) g_Player.x = -kWorldHalfExtent;
		if (g_Player.x > kWorldHalfExtent) g_Player.x = kWorldHalfExtent;
		if (g_Player.y < -kWorldHalfExtent) g_Player.y = -kWorldHalfExtent;
		if (g_Player.y > kWorldHalfExtent) g_Player.y = kWorldHalfExtent;
	}

	for (Animal& animal : g_Animals)
	{
		if (!animal.alive)
		{
			continue;
		}

		if (animal.isAggressive)
		{
			UpdateMonsterAI(animal, deltaSeconds);
		}
		else
		{
			WanderAnimal(animal);
		}

		if (animal.attackCooldown > 0.f)
		{
			animal.attackCooldown -= deltaSeconds;
			if (animal.attackCooldown < 0.f)
			{
				animal.attackCooldown = 0.f;
			}
		}

		if (animal.hitFlashTimer > 0.f)
		{
			animal.hitFlashTimer -= deltaSeconds;
			if (animal.hitFlashTimer < 0.f)
			{
				animal.hitFlashTimer = 0.f;
			}
		}
	}

	if (g_PlayerHitFlashTimer > 0.f)
	{
		g_PlayerHitFlashTimer -= deltaSeconds;
		if (g_PlayerHitFlashTimer < 0.f)
		{
			g_PlayerHitFlashTimer = 0.f;
		}
	}

	if (g_PlayerAttackTimer > 0.f)
	{
		g_PlayerAttackTimer -= deltaSeconds;
		if (g_PlayerAttackTimer < 0.f)
		{
			g_PlayerAttackTimer = 0.f;
		}
	}

	g_Camera->SetFocus(g_Player.x, g_Player.y, 0.f);
}

void Idle(void)
{
	std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
	float deltaSeconds = std::chrono::duration<float>(now - g_LastFrameTime).count();
	g_LastFrameTime = now;

	Update(deltaSeconds);
	RenderScene();
}

void MouseInput(int button, int state, int x, int y)
{
}

void MouseWheel(int wheel, int direction, int x, int y)
{
	// direction은 휠을 위로 굴리면 +1(확대), 아래로 굴리면 -1(축소)이다.
	const float kZoomStep = 1.1f;
	g_Camera->AdjustZoom(direction > 0 ? kZoomStep : 1.f / kZoomStep);
}

void KeyInput(unsigned char key, int x, int y)
{
	switch (key)
	{
	case 'w': case 'W': g_KeyW = true; break;
	case 'a': case 'A': g_KeyA = true; break;
	case 's': case 'S': g_KeyS = true; break;
	case 'd': case 'D': g_KeyD = true; break;
	case 'e': case 'E': TryInteract(); break;
	case ' ': TryAttack(); break;
	default: break;
	}
}

void KeyUp(unsigned char key, int x, int y)
{
	switch (key)
	{
	case 'w': case 'W': g_KeyW = false; break;
	case 'a': case 'A': g_KeyA = false; break;
	case 's': case 'S': g_KeyS = false; break;
	case 'd': case 'D': g_KeyD = false; break;
	default: break;
	}
}

void SpecialKeyInput(int key, int x, int y)
{
}

namespace
{
	void GenerateTileVisuals(const TileMap& tileMap)
	{
		for (int gy = 0; gy < tileMap.GetHeight(); ++gy)
		{
			for (int gx = 0; gx < tileMap.GetWidth(); ++gx)
			{
				TileType tile = tileMap.GetTile(gx, gy);
				float wx = tileMap.GetWorldX(gx);
				float wy = tileMap.GetWorldY(gy);

				float r, g, b;
				EntityType tileEntityType = EntityType::Prop;
				switch (tile)
				{
				case TileType::Stone: r = 0.55f; g = 0.53f; b = 0.50f; break;
				case TileType::Water: r = 0.15f; g = 0.35f; b = 0.55f; tileEntityType = EntityType::Water; break;
				case TileType::Path:  r = 0.45f; g = 0.35f; b = 0.22f; break;
				default:              r = 0.18f; g = 0.32f; b = 0.16f; break; // 잔디
				}

				g_TileObjects.push_back(MakeObject(wx, wy, -0.5f, 1.f, r, g, b, 1.f, tileEntityType));
			}
		}
	}

	void BuildLevel(const LevelLayout& layout)
	{
		float vx = layout.villageCenterX;
		float vy = layout.villageCenterY;
		float lx = layout.lakeCenterX;
		float ly = layout.lakeCenterY;

		// 숲: 마을/호수 중심에서 충분히 떨어진 곳에 무작위로 나무를 배치.
		// 맵이 32x32로 넓어진 만큼 범위와 그루 수도 함께 키움(4배 면적에 맞춰
		// 10그루 → 36그루).
		std::mt19937 treeRng(std::random_device{}());
		std::uniform_real_distribution<float> treeOffset(-14.f, 14.f);
		std::uniform_real_distribution<float> treeTint(-0.05f, 0.05f); // 그루마다 색조를 살짝 흔들어 단조로움을 줄임.
		int treesPlaced = 0;
		int guard = 0;
		while (treesPlaced < 36 && guard < 600)
		{
			++guard;
			float tx = treeOffset(treeRng);
			float ty = treeOffset(treeRng);
			float distVillage = sqrtf((tx - vx) * (tx - vx) + (ty - vy) * (ty - vy));
			float distLake = sqrtf((tx - lx) * (tx - lx) + (ty - ly) * (ty - ly));
			if (distVillage < 3.5f || distLake < 3.f)
			{
				continue;
			}
			float tint = treeTint(treeRng);
			g_Entities.push_back(MakeObject(tx, ty, 0.f, 1.6f, 0.08f + tint, 0.22f + tint, 0.10f + tint * 0.5f, 1.f, EntityType::Prop));
			++treesPlaced;
		}

		// 건물 2채와 그 옆의 횃불.
		g_Entities.push_back(MakeObject(vx - 2.0f, vy + 2.0f, 0.f, 1.4f, 1.f, 1.f, 1.f, 1.f, EntityType::Building));
		g_Entities.push_back(MakeObject(vx + 2.0f, vy + 2.0f, 0.f, 1.4f, 1.f, 1.f, 1.f, 1.f, EntityType::Building));
		g_Entities.push_back(MakeObject(vx - 2.0f, vy + 1.2f, 1.0f, 0.5f, 1.f, 1.f, 1.f, 1.f, EntityType::Fire));
		g_Entities.push_back(MakeObject(vx + 2.0f, vy + 1.2f, 1.0f, 0.5f, 1.f, 1.f, 1.f, 1.f, EntityType::Fire));

		// 마을 사람: 장로(퀘스트 시작점, 눈에 띄도록 빛남) + 마을 사람 7명.
		g_Entities.push_back(MakeObject(vx, vy + 1.5f, 0.f, 1.f, 3.0f, 2.6f, 0.8f, 1.f, EntityType::NPC, kInteractElder, "Elder"));
		g_Entities.push_back(MakeObject(vx - 1.6f, vy - 1.6f, 0.f, 0.9f, 0.7f, 0.5f, 0.3f, 1.f, EntityType::NPC, kInteractVillager, "Villager"));
		g_Entities.push_back(MakeObject(vx + 1.6f, vy - 1.6f, 0.f, 0.9f, 0.5f, 0.4f, 0.7f, 1.f, EntityType::NPC, kInteractVillager, "Villager"));
		g_Entities.push_back(MakeObject(vx, vy - 2.2f, 0.f, 0.9f, 0.8f, 0.3f, 0.3f, 1.f, EntityType::NPC, kInteractVillager, "Villager"));
		g_Entities.push_back(MakeObject(vx - 2.0f, vy + 0.3f, 0.f, 0.9f, 0.6f, 0.55f, 0.35f, 1.f, EntityType::NPC, kInteractVillager, "Villager"));
		g_Entities.push_back(MakeObject(vx + 2.0f, vy + 0.3f, 0.f, 0.9f, 0.45f, 0.5f, 0.6f, 1.f, EntityType::NPC, kInteractVillager, "Villager"));
		g_Entities.push_back(MakeObject(vx - 1.0f, vy + 1.2f, 0.f, 0.9f, 0.65f, 0.4f, 0.5f, 1.f, EntityType::NPC, kInteractVillager, "Villager"));
		g_Entities.push_back(MakeObject(vx + 1.0f, vy + 1.2f, 0.f, 0.9f, 0.5f, 0.6f, 0.4f, 1.f, EntityType::NPC, kInteractVillager, "Villager"));

		// 퀘스트 아이템: 호수 옆에서 빛나는 잃어버린 제물.
		g_Entities.push_back(MakeObject(lx + 0.3f, ly, 0.f, 0.6f, 3.5f, 3.0f, 0.9f, 1.f, EntityType::Item, kInteractQuestItem));

		// 일반 획득 아이템(약초): 경험치용, 넓어진 숲 곳곳에 흩어져 있음
		// (맵이 커진 만큼 3개 → 8개로 늘림).
		g_Entities.push_back(MakeObject(vx + 4.5f, vy + 3.f, 0.f, 0.4f, 0.4f, 1.6f, 0.5f, 1.f, EntityType::Item, kInteractLoot));
		g_Entities.push_back(MakeObject(vx - 4.5f, vy - 3.f, 0.f, 0.4f, 0.4f, 1.6f, 0.5f, 1.f, EntityType::Item, kInteractLoot));
		g_Entities.push_back(MakeObject(vx - 3.f, vy + 4.5f, 0.f, 0.4f, 0.4f, 1.6f, 0.5f, 1.f, EntityType::Item, kInteractLoot));
		g_Entities.push_back(MakeObject(vx + 10.f, vy + 6.f, 0.f, 0.4f, 0.4f, 1.6f, 0.5f, 1.f, EntityType::Item, kInteractLoot));
		g_Entities.push_back(MakeObject(vx - 10.f, vy - 6.f, 0.f, 0.4f, 0.4f, 1.6f, 0.5f, 1.f, EntityType::Item, kInteractLoot));
		g_Entities.push_back(MakeObject(vx + 7.f, vy - 9.f, 0.f, 0.4f, 0.4f, 1.6f, 0.5f, 1.f, EntityType::Item, kInteractLoot));
		g_Entities.push_back(MakeObject(vx - 8.f, vy + 9.f, 0.f, 0.4f, 0.4f, 1.6f, 0.5f, 1.f, EntityType::Item, kInteractLoot));
		g_Entities.push_back(MakeObject(vx + 1.f, vy - 11.f, 0.f, 0.4f, 0.4f, 1.6f, 0.5f, 1.f, EntityType::Item, kInteractLoot));

		// 야생 짐승: 맵이 커진 만큼 사슴 2 → 4마리, 늑대(몬스터) 1 → 3마리로 늘려
		// 넓어진 숲 곳곳에 분산 배치. 공격해서 쓰러뜨리면 경험치를 준다.
		Animal deer1;
		deer1.anchorX = vx - 6.f; deer1.anchorY = vy + 3.f; deer1.phase = 0.f;
		deer1.visual = MakeObject(deer1.anchorX, deer1.anchorY, 0.f, 0.9f, 0.45f, 0.32f, 0.18f, 1.f, EntityType::Animal, 0, "Deer");
		g_Animals.push_back(deer1);

		Animal deer2;
		deer2.anchorX = vx + 6.f; deer2.anchorY = vy - 3.5f; deer2.phase = 2.1f;
		deer2.visual = MakeObject(deer2.anchorX, deer2.anchorY, 0.f, 0.9f, 0.5f, 0.36f, 0.2f, 1.f, EntityType::Animal, 0, "Deer");
		g_Animals.push_back(deer2);

		Animal deer3;
		deer3.anchorX = vx + 10.f; deer3.anchorY = vy + 7.f; deer3.phase = 1.3f;
		deer3.visual = MakeObject(deer3.anchorX, deer3.anchorY, 0.f, 0.9f, 0.48f, 0.34f, 0.19f, 1.f, EntityType::Animal, 0, "Deer");
		g_Animals.push_back(deer3);

		Animal deer4;
		deer4.anchorX = vx - 9.f; deer4.anchorY = vy - 7.f; deer4.phase = 3.4f;
		deer4.visual = MakeObject(deer4.anchorX, deer4.anchorY, 0.f, 0.9f, 0.42f, 0.3f, 0.17f, 1.f, EntityType::Animal, 0, "Deer");
		g_Animals.push_back(deer4);

		Animal wolf1;
		wolf1.anchorX = vx - 3.f; wolf1.anchorY = vy + 6.f; wolf1.phase = 4.2f;
		wolf1.hp = 30; // 늑대가 사슴보다 조금 더 강함
		wolf1.isAggressive = true; // 늑대는 몬스터: 플레이어를 추적/공격함. 사슴은 그대로 배회만 함.
		wolf1.visual = MakeObject(wolf1.anchorX, wolf1.anchorY, 0.f, 1.0f, 0.3f, 0.3f, 0.32f, 1.f, EntityType::Animal, 0, "Wolf");
		g_Animals.push_back(wolf1);

		Animal wolf2;
		wolf2.anchorX = vx + 8.f; wolf2.anchorY = vy - 6.f; wolf2.phase = 5.6f;
		wolf2.hp = 30;
		wolf2.isAggressive = true;
		wolf2.visual = MakeObject(wolf2.anchorX, wolf2.anchorY, 0.f, 1.0f, 0.28f, 0.28f, 0.3f, 1.f, EntityType::Animal, 0, "Wolf");
		g_Animals.push_back(wolf2);

		Animal wolf3;
		wolf3.anchorX = vx - 2.f; wolf3.anchorY = vy - 10.f; wolf3.phase = 0.7f;
		wolf3.hp = 30;
		wolf3.isAggressive = true;
		wolf3.visual = MakeObject(wolf3.anchorX, wolf3.anchorY, 0.f, 1.0f, 0.32f, 0.32f, 0.34f, 1.f, EntityType::Animal, 0, "Wolf");
		g_Animals.push_back(wolf3);

		// 플레이어는 마을 중심 근처에서 시작한다. 사망 시 이 지점으로 되돌아온다.
		g_PlayerSpawnX = vx;
		g_PlayerSpawnY = vy - 0.5f;
		g_Player = MakeObject(g_PlayerSpawnX, g_PlayerSpawnY, 0.f, 0.8f, 0.9f, 0.85f, 0.8f, 1.f, EntityType::Player, 0, "Player");
	}
}

int main(int argc, char **argv)
{
	// 콘솔 코드페이지를 UTF-8로 맞춰서 한글 로그(std::cout)가 깨지지 않게 한다.
	// (소스 파일이 UTF-8로 저장되어 있으므로, 콘솔도 같은 인코딩으로 맞춰야 한다.)
	SetConsoleOutputCP(CP_UTF8);
	SetConsoleCP(CP_UTF8);

	// OpenGL 초기화
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DEPTH | GLUT_DOUBLE | GLUT_RGBA);
	glutInitWindowPosition(0, 0);
	glutInitWindowSize(800, 600);
	glutCreateWindow("Game Software Engineering KPU");

	glewInit();
	if (glewIsSupported("GL_VERSION_3_0"))
	{
		std::cout << "GLEW 버전은 3.0입니다.\n";
	}
	else
	{
		std::cout << "GLEW 3.0을 지원하지 않습니다.\n";
	}

	// 렌더러 초기화
	g_Renderer = new Renderer(800, 600);
	if (!g_Renderer->IsInitialized())
	{
		std::cout << "렌더러를 초기화하지 못했습니다.\n";
	}

	g_Camera = new Camera(8.f, 6.f, 1.f);
	g_PostProcess = new PostProcess(800, 600);

	// 캐릭터 머리에 쓸 원형 메시, 나무 수관/아이템에 쓸 타원 메시. 처음 실행할 땐
	// 만들어서 ./Cache에 저장하고, 다음 실행부터는 파일에서 그대로 불러온다.
	MeshData circleMeshData = MeshCache::GetOrCreate("circle_16", []() { return MeshGen::GenerateCircle(16); });
	g_CircleMesh = g_Renderer->CreateMesh(circleMeshData);

	MeshData ellipseMeshData = MeshCache::GetOrCreate("ellipse_16", []() { return MeshGen::GenerateEllipse(0.5f, 0.35f, 16); });
	g_EllipseMesh = g_Renderer->CreateMesh(ellipseMeshData);

	// 가로/세로 2배 = 면적 4배. (32x32)
	const int kMapWidth = 32;
	const int kMapHeight = 32;
	g_TileMap = new TileMap(kMapWidth, kMapHeight);
	LevelLayout layout = GenerateVillageLevel(*g_TileMap);
	GenerateTileVisuals(*g_TileMap);
	BuildLevel(layout);

	std::cout << "[튜토리얼] WASD로 이동, E로 상호작용, Space로 공격. 장로를 찾아가보자.\n";

	g_LastFrameTime = std::chrono::steady_clock::now();

	glutDisplayFunc(RenderScene);
	glutIdleFunc(Idle);
	glutKeyboardFunc(KeyInput);
	glutKeyboardUpFunc(KeyUp);
	glutMouseFunc(MouseInput);
	glutMouseWheelFunc(MouseWheel);
	glutSpecialFunc(SpecialKeyInput);

	glutMainLoop();

	g_Renderer->DestroyMesh(g_CircleMesh);
	g_Renderer->DestroyMesh(g_EllipseMesh);
	delete g_TileMap;
	delete g_PostProcess;
	delete g_Camera;
	delete g_Renderer;

    return 0;
}
