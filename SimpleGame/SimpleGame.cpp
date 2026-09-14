/*
Copyright 2022 Lee Taek Hee (Tech University of Korea)

This program is free software: you can redistribute it and/or modify
it under the terms of the What The Hell License. Do it plz.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY.
*/

#include "stdafx.h"
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
		EntityType type = EntityType::Prop, int interactId = 0)
	{
		GameObject obj;
		obj.x = x; obj.y = y; obj.z = z; obj.size = size;
		obj.r = r; obj.g = g; obj.b = b; obj.a = a;
		obj.type = type;
		obj.interactId = interactId;
		return obj;
	}

	// HandleInteract()가 분기하는 상호작용 식별자. 0이면 상호작용 불가.
	const int kInteractElder = 1;
	const int kInteractQuestItem = 2;
	const int kInteractVillager = 3;
	const int kInteractLoot = 4;

	enum class QuestState
	{
		NotStarted,
		ItemRequested,
		ItemCollected,
		Completed,
	};

	// 배회하는 야생 짐승 하나. 실제 위치(visual.x/y)는 매 프레임 anchor를
	// 중심으로 한 단순한 진자 운동으로 갱신된다 (경로탐색은 아직 없음).
	struct Animal
	{
		GameObject visual;
		float anchorX = 0.f;
		float anchorY = 0.f;
		float phase = 0.f;
		int hp = 20;
		bool alive = true;
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

GameObject g_Player;
bool g_PlayerIsMoving = false;
PlayerStats g_PlayerStats;

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

	void TryAttack()
	{
		const float kAttackRadius = 1.6f;
		float bestDistSq = kAttackRadius * kAttackRadius;
		Animal* target = NULL;

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
				target = &animal;
			}
		}

		if (target == NULL)
		{
			std::cout << "[허공을 가른다]\n";
			return;
		}

		target->hp -= g_PlayerStats.attackPower;
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

	void TryInteract()
	{
		const float kInteractRadius = 1.4f;
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

		if (bestIndex < 0)
		{
			std::cout << "[근처에 상호작용할 대상이 없습니다]\n";
			return;
		}

		HandleInteract(bestIndex);
	}

	// 캐릭터(플레이어/NPC/짐승)를 몸통+머리(원형 메시)+다리 여러 파츠로 그린다.
	// animateWalk가 true면 다리가 좌우로 흔들리는 절차적 걷기 애니메이션이 재생된다.
	void DrawCharacter(const GameObject& obj, const Mat4& viewProjection, bool animateWalk)
	{
		float breathe = sinf(g_ElapsedSeconds * 4.f + obj.x * 3.1f) * 0.03f; // 개체마다 위상이 다른 미세한 숨쉬기 움직임
		float legSwing = animateWalk ? sinf(g_ElapsedSeconds * 8.f) * 0.15f : 0.f;

		// 몸통
		Mat4 body = Mat4::Translate(obj.x, obj.y, obj.z + obj.size * 0.5f + breathe)
			* Mat4::Scale(obj.size * 0.55f, obj.size * 0.35f, obj.size * 0.7f);
		g_Renderer->DrawObject(viewProjection * body, obj.r, obj.g, obj.b, obj.a);

		// 머리 (원형 메시, 몸통보다 밝은 톤)
		Mat4 head = Mat4::Translate(obj.x, obj.y, obj.z + obj.size * 0.95f + breathe)
			* Mat4::Scale(obj.size * 0.4f, obj.size * 0.4f, obj.size * 0.4f);
		g_Renderer->DrawMesh(g_CircleMesh, viewProjection * head, obj.r * 1.2f + 0.1f, obj.g * 1.2f + 0.1f, obj.b * 1.2f + 0.1f, obj.a);

		// 다리 2개 (이동 중일 때만 서로 반대로 흔들림)
		Mat4 legLeft = Mat4::Translate(obj.x - obj.size * 0.15f, obj.y + legSwing * 0.2f, obj.z + obj.size * 0.15f)
			* Mat4::Scale(obj.size * 0.18f, obj.size * 0.18f, obj.size * 0.3f);
		Mat4 legRight = Mat4::Translate(obj.x + obj.size * 0.15f, obj.y - legSwing * 0.2f, obj.z + obj.size * 0.15f)
			* Mat4::Scale(obj.size * 0.18f, obj.size * 0.18f, obj.size * 0.3f);
		g_Renderer->DrawObject(viewProjection * legLeft, obj.r * 0.6f, obj.g * 0.6f, obj.b * 0.6f, obj.a);
		g_Renderer->DrawObject(viewProjection * legRight, obj.r * 0.6f, obj.g * 0.6f, obj.b * 0.6f, obj.a);
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

	// 이 씬은 2.5D이고 실제 3D 깊이버퍼가 아니므로, 매 프레임 월드 깊이
	// (y + z) 기준으로 정렬한 뒤 뒤에서 앞 순서로 그린다 (페인터 알고리즘).
	std::vector<GameObject> drawOrder = g_Entities;
	for (const Animal& animal : g_Animals)
	{
		if (animal.alive)
		{
			drawOrder.push_back(animal.visual);
		}
	}
	drawOrder.push_back(g_Player);

	std::sort(drawOrder.begin(), drawOrder.end(), [](const GameObject& lhs, const GameObject& rhs)
	{
		return (lhs.y + lhs.z) < (rhs.y + rhs.z);
	});

	// 그림자를 먼저 그려 캐릭터/건물 발밑에 깔리도록 한다.
	for (const GameObject& obj : drawOrder)
	{
		if (obj.type == EntityType::Player || obj.type == EntityType::NPC ||
			obj.type == EntityType::Animal || obj.type == EntityType::Building)
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
			DrawCharacter(obj, viewProjection, g_PlayerIsMoving);
			break;
		case EntityType::NPC:
			DrawCharacter(obj, viewProjection, false);
			break;
		case EntityType::Animal:
			DrawCharacter(obj, viewProjection, true);
			break;
		case EntityType::Building:
			DrawBuilding(obj, viewProjection);
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

		const float kPlayerSpeed = 4.f; // 초당 이동 거리(월드 단위)
		float step = kPlayerSpeed * deltaSeconds;

		// 축별로 따로 검사해서, 물가에 붙어 미끄러지듯 이동하되 물 위로는
		// 올라갈 수 없게 한다 (호수 = 접근 불가 영역).
		float newX = g_Player.x + moveX * step;
		if (g_TileMap->IsWorldPositionWalkable(newX, g_Player.y))
		{
			g_Player.x = newX;
		}

		float newY = g_Player.y + moveY * step;
		if (g_TileMap->IsWorldPositionWalkable(g_Player.x, newY))
		{
			g_Player.y = newY;
		}

		const float kWorldHalfExtent = 7.5f;
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
		animal.visual.x = animal.anchorX + sinf(g_ElapsedSeconds * 0.4f + animal.phase) * 1.5f;
		animal.visual.y = animal.anchorY + cosf(g_ElapsedSeconds * 0.25f + animal.phase) * 1.0f;
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
		std::mt19937 treeRng(std::random_device{}());
		std::uniform_real_distribution<float> treeOffset(-7.f, 7.f);
		int treesPlaced = 0;
		int guard = 0;
		while (treesPlaced < 10 && guard < 200)
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
			g_Entities.push_back(MakeObject(tx, ty, 0.f, 1.6f, 0.08f, 0.22f, 0.10f, 1.f, EntityType::Prop));
			++treesPlaced;
		}

		// 건물 2채와 그 옆의 횃불.
		g_Entities.push_back(MakeObject(vx - 2.0f, vy + 2.0f, 0.f, 1.4f, 1.f, 1.f, 1.f, 1.f, EntityType::Building));
		g_Entities.push_back(MakeObject(vx + 2.0f, vy + 2.0f, 0.f, 1.4f, 1.f, 1.f, 1.f, 1.f, EntityType::Building));
		g_Entities.push_back(MakeObject(vx - 2.0f, vy + 1.2f, 1.0f, 0.5f, 1.f, 1.f, 1.f, 1.f, EntityType::Fire));
		g_Entities.push_back(MakeObject(vx + 2.0f, vy + 1.2f, 1.0f, 0.5f, 1.f, 1.f, 1.f, 1.f, EntityType::Fire));

		// 마을 사람: 장로(퀘스트 시작점, 눈에 띄도록 빛남) + 마을 사람 7명.
		g_Entities.push_back(MakeObject(vx, vy + 1.5f, 0.f, 1.f, 3.0f, 2.6f, 0.8f, 1.f, EntityType::NPC, kInteractElder));
		g_Entities.push_back(MakeObject(vx - 1.6f, vy - 1.6f, 0.f, 0.9f, 0.7f, 0.5f, 0.3f, 1.f, EntityType::NPC, kInteractVillager));
		g_Entities.push_back(MakeObject(vx + 1.6f, vy - 1.6f, 0.f, 0.9f, 0.5f, 0.4f, 0.7f, 1.f, EntityType::NPC, kInteractVillager));
		g_Entities.push_back(MakeObject(vx, vy - 2.2f, 0.f, 0.9f, 0.8f, 0.3f, 0.3f, 1.f, EntityType::NPC, kInteractVillager));
		g_Entities.push_back(MakeObject(vx - 2.0f, vy + 0.3f, 0.f, 0.9f, 0.6f, 0.55f, 0.35f, 1.f, EntityType::NPC, kInteractVillager));
		g_Entities.push_back(MakeObject(vx + 2.0f, vy + 0.3f, 0.f, 0.9f, 0.45f, 0.5f, 0.6f, 1.f, EntityType::NPC, kInteractVillager));
		g_Entities.push_back(MakeObject(vx - 1.0f, vy + 1.2f, 0.f, 0.9f, 0.65f, 0.4f, 0.5f, 1.f, EntityType::NPC, kInteractVillager));
		g_Entities.push_back(MakeObject(vx + 1.0f, vy + 1.2f, 0.f, 0.9f, 0.5f, 0.6f, 0.4f, 1.f, EntityType::NPC, kInteractVillager));

		// 퀘스트 아이템: 호수 옆에서 빛나는 잃어버린 제물.
		g_Entities.push_back(MakeObject(lx + 0.3f, ly, 0.f, 0.6f, 3.5f, 3.0f, 0.9f, 1.f, EntityType::Item, kInteractQuestItem));

		// 일반 획득 아이템(약초): 경험치용, 숲 곳곳에 흩어져 있음.
		g_Entities.push_back(MakeObject(vx + 4.5f, vy + 3.f, 0.f, 0.4f, 0.4f, 1.6f, 0.5f, 1.f, EntityType::Item, kInteractLoot));
		g_Entities.push_back(MakeObject(vx - 4.5f, vy - 3.f, 0.f, 0.4f, 0.4f, 1.6f, 0.5f, 1.f, EntityType::Item, kInteractLoot));
		g_Entities.push_back(MakeObject(vx - 3.f, vy + 4.5f, 0.f, 0.4f, 0.4f, 1.6f, 0.5f, 1.f, EntityType::Item, kInteractLoot));

		// 야생 짐승 3마리: 공격해서 쓰러뜨리면 경험치를 준다.
		Animal deer1;
		deer1.anchorX = vx - 6.f; deer1.anchorY = vy + 3.f; deer1.phase = 0.f;
		deer1.visual = MakeObject(deer1.anchorX, deer1.anchorY, 0.f, 0.9f, 0.45f, 0.32f, 0.18f, 1.f, EntityType::Animal);
		g_Animals.push_back(deer1);

		Animal deer2;
		deer2.anchorX = vx + 6.f; deer2.anchorY = vy - 3.5f; deer2.phase = 2.1f;
		deer2.visual = MakeObject(deer2.anchorX, deer2.anchorY, 0.f, 0.9f, 0.5f, 0.36f, 0.2f, 1.f, EntityType::Animal);
		g_Animals.push_back(deer2);

		Animal wolf;
		wolf.anchorX = vx - 3.f; wolf.anchorY = vy + 6.f; wolf.phase = 4.2f;
		wolf.hp = 30; // 늑대가 사슴보다 조금 더 강함
		wolf.visual = MakeObject(wolf.anchorX, wolf.anchorY, 0.f, 1.0f, 0.3f, 0.3f, 0.32f, 1.f, EntityType::Animal);
		g_Animals.push_back(wolf);

		// 플레이어는 마을 중심 근처에서 시작한다.
		g_Player = MakeObject(vx, vy - 0.5f, 0.f, 0.8f, 0.9f, 0.85f, 0.8f, 1.f, EntityType::Player);
	}
}

int main(int argc, char **argv)
{
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

	// 캐릭터 머리에 쓸 원형 메시. 처음 실행할 땐 만들어서 ./Cache에 저장하고,
	// 다음 실행부터는 파일에서 그대로 불러온다.
	MeshData circleMeshData = MeshCache::GetOrCreate("circle_16", []() { return MeshGen::GenerateCircle(16); });
	g_CircleMesh = g_Renderer->CreateMesh(circleMeshData);

	const int kMapWidth = 16;
	const int kMapHeight = 16;
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
	delete g_TileMap;
	delete g_PostProcess;
	delete g_Camera;
	delete g_Renderer;

    return 0;
}
