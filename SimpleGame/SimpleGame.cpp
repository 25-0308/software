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
#include <string>
#include "Dependencies\glew.h"
#include "Dependencies\freeglut.h"

#include "Camera.h"
#include "ChatWindow.h"
#include "CharacterActors.h"
#include "DrawCallHook.h"
#include "GameLog.h"
#include "Hud.h"
#include "LevelBuilder.h"
#include "MapScreen.h"
#include "Mesh.h"
#include "MeshCache.h"
#include "MiniMap.h"
#include "PostProcess.h"
#include "Profiler.h"
#include "Renderer.h"
#include "SceneGraph.h"
#include "StoryDirector.h"
#include "TileMap.h"
#include "WorldActors.h"
#include "WorldMaps.h"

namespace
{
	// 공격/상호작용 판정 반경. Try*()와 조준 링(RingActor) 대상 지정이 같은 값을
	// 공유해야 "링이 보이면 실제로 닿는다"는 예측이 항상 맞는다.
	const float kAttackRadius = 1.6f;
	const float kInteractRadius = 1.4f;

	// 게임 화면의 기준 해상도(4:3). HUD·미니맵·채팅창도 이 픽셀 좌표계로 그린다. 창 크기가 바뀌면
	// 이 비율을 유지한 채 창 가운데에 맞춰 늘리거나 줄인다(Reshape 참고).
	const int kScreenWidth = 800;
	const int kScreenHeight = 600;

	// 한 프레임에 진행하는 게임 시간의 상한(초). 창을 드래그하는 동안처럼 루프가 잠깐 멈췄다 풀리면
	// 한 프레임의 dt가 수 초가 될 수 있는데, 이동 충돌은 도착 지점만 검사하므로 그 한 걸음에 건물·물을
	// 건너뛸 수 있다. 상한을 두면 그 순간 게임 시간이 잠깐 느려질 뿐 벽을 뚫는 일은 없다.
	const float kMaxDeltaSeconds = 0.1f;

	// 약초를 주웠을 때의 경험치.
	const int kHerbXP = 15;

	// 수호신 축복 중 전투에 붙는 효과(나머지 상시 효과는 StoryDirector::GrantBlessing).
	const float kZeusLightningChance = 0.15f; // 제우스: 공격할 때 벼락이 함께 떨어져 피해 2배가 될 확률
	const float kZeusMisfireChance = 0.03f;   // 제우스: 벼락이 엉뚱한 데 떨어질 확률(피해 그대로, 웃음용)
	const int kHadesHealOnKill = 8;           // 하데스: 적을 쓰러뜨릴 때 회복하는 체력

	// 첫 지역과 이야기 데이터 파일(셰이더처럼 작업 폴더 기준 경로 — 다시 컴파일하지 않고 대사만 고칠 수 있게).
	const char* kFirstLocation = "LOC_DELPHI";
	const char* kStoryDataPath = "./Data/Story.txt";

	const unsigned char kEscapeKey = 27;
}

Renderer *g_Renderer = NULL;
Camera *g_Camera = NULL;
PostProcess *g_PostProcess = NULL;
TileMap *g_TileMap = NULL;
SceneGraph *g_Scene = NULL;
MiniMap *g_MiniMap = NULL;
MapScreen *g_MapScreen = NULL; // M 키로 여는 큰 지도(열려 있는 동안 게임이 멈춘다)
ChatWindow *g_ChatWindow = NULL;

// 이야기 진행(대화·진행 단계·지역 이동). 맵이 바뀌어도 계속 살아 있다.
StoryDirector *g_Director = NULL;

// 씬 그래프가 소유하는 액터들을 게임 코드가 빠르게 참조하기 위한 포인터. 지역을 옮기면 씬과 함께
// 사라지므로 LoadMap이 새 맵의 것으로 바꿔 든다.
PlayerActor *g_Player = NULL;
RingActor *g_AttackRing = NULL;
RingActor *g_InteractRing = NULL;

MeshHandle g_CircleMesh;
MeshHandle g_EllipseMesh; // 발밑 링(위치 마커/조준 링)에 쓰는 납작한 타원 메시

// 이번 실행의 세계 시드. 실행마다 지역의 세부(나무·바위·바닥 장식)가 달라지고, 한 번 실행하는 동안엔
// 같은 지역에 다시 와도 똑같은 모습이다.
unsigned int g_SessionSeed = 0;

bool g_KeyW = false, g_KeyA = false, g_KeyS = false, g_KeyD = false;

// 키마다 지금 눌려 있는지(소문자 기준). 키를 누르고 있으면 운영체제가 같은 키를 반복해서 보내므로,
// 대화를 넘길 때는 반복 입력을 걸러서 "한 번 누름 = 한 줄"이 되게 한다.
bool g_KeyHeld[256] = {};

// 대화를 넘기는 데 쓴 키. 대화가 끝난 뒤에도 그 키를 뗄 때까지는 반복 입력이 공격·상호작용으로
// 이어지지 않게 한다(Space로 마지막 줄을 넘기자마자 허공에 창을 휘두르지 않도록).
bool g_KeyConsumedByDialogue[256] = {};

// 게임 창 핸들. 이 창이 맨 앞(키보드 입력을 받는 창)인지 확인하는 데 쓴다(IsGameWindowFocused).
HWND g_GameWindow = NULL;

std::chrono::steady_clock::time_point g_LastFrameTime;
float g_ElapsedSeconds = 0.f;

// 공격(Space)과 상호작용(E)이 공유하는 쿨타임. 둘 중 하나를 하면 이 시간이 지나기 전에는
// 공격도 상호작용도 다시 할 수 없다(쿨타임 중 입력은 무시).
const float kActionCooldownSeconds = 1.0f;
float g_ActionCooldown = 0.f; // 남은 쿨타임(초). 0이면 행동할 수 있다.

// 사후처리 분위기 조절값: exposure(노출)는 밝은 부분이 눌리는 정도(톤매핑),
// vignetteStrength는 화면 가장자리가 어두워지는 정도, bloomThreshold는
// 발광이 시작되는 최소 밝기, bloomIntensity는 발광의 세기를 결정한다.
float g_Exposure = 1.0f;
float g_VignetteStrength = 0.6f;
float g_BloomThreshold = 0.9f;
float g_BloomIntensity = 0.8f;

namespace
{
	// 짐승·괴물의 한글 이름(채팅창 로그용). object는 목적격 조사("을/를")까지 붙인 꼴.
	struct AnimalNames
	{
		const char* name;
		const char* object;
	};

	AnimalNames GetAnimalNames(AnimalKind kind)
	{
		switch (kind)
		{
		case AnimalKind::Deer: return { "사슴", "사슴을" };
		case AnimalKind::Wolf: return { "늑대", "늑대를" };
		case AnimalKind::ShadowWolf: return { "그림자 늑대", "그림자 늑대를" };
		case AnimalKind::ShadowPython: return { "그림자 퓌톤", "그림자 퓌톤을" };
		}
		return { "짐승", "짐승을" };
	}

	// 0~1 사이의 난수(축복 효과의 확률 판정).
	float RollChance()
	{
		static std::mt19937 rng(std::random_device{}());
		std::uniform_real_distribution<float> unit(0.f, 1.f);
		return unit(rng);
	}

	// 키 코드를 소문자 기준으로 맞춘다(Shift를 누른 채 누르고 뗀 경우에도 같은 키로 보이게).
	unsigned char NormalizeKey(unsigned char key)
	{
		return (key >= 'A' && key <= 'Z') ? (unsigned char)(key - 'A' + 'a') : key;
	}

	// 공격 사거리 안에 있는 가장 가까운 짐승. TryAttack()과 조준 링이 이 함수를
	// 공유해서, 화면에 보이는 조준 표시와 실제 공격 결과가 항상 일치하게 한다.
	AnimalActor* FindNearestAttackTarget()
	{
		Actor* nearest = g_Scene->FindNearest(g_Player->GetWorldX(), g_Player->GetWorldY(), kAttackRadius,
			[](const Actor& actor) { return actor.GetType() == ActorType::Animal; });

		return static_cast<AnimalActor*>(nearest);
	}

	// 상호작용 사거리 안에 있는 상호작용 가능한 가장 가까운 액터.
	Actor* FindNearestInteractable()
	{
		return g_Scene->FindNearest(g_Player->GetWorldX(), g_Player->GetWorldY(), kInteractRadius,
			[](const Actor& actor) { return actor.GetInteractId() != 0; });
	}

	void TryAttack()
	{
		// 쿨타임 중에는 입력을 통째로 무시한다(휘두르는 모션도 재생하지 않는다).
		if (g_ActionCooldown > 0.f)
		{
			return;
		}

		AnimalActor* target = FindNearestAttackTarget();

		// 대상 유무와 상관없이 휘두르는 모션은 재생하고 쿨타임도 시작한다 (허공을
		// 가르더라도 입력에 대한 시각적 반응이 있어야 손맛이 느껴지고, 모션을
		// 연타로 뿌리는 것도 막아야 하므로).
		g_Player->StartAttack();
		g_ActionCooldown = kActionCooldownSeconds;

		if (target == NULL)
		{
			GameLog::Add(GameLog::Kind::Combat, "[허공을 가른다]");
			return;
		}

		// 공격 모션이 대상 쪽을 향하도록 캐릭터 방향을 맞춘다.
		g_Player->FaceToward(target->GetWorldX(), target->GetWorldY());

		AnimalNames names = GetAnimalNames(target->GetKind());
		int damage = g_Player->GetStats().attackPower;

		// 수호신의 축복 중 전투에 붙는 것: 제우스는 가끔 벼락이 함께 떨어져 피해가 두 배가 되고, 아주 가끔은
		// 벼락이 엉뚱한 데 떨어진다(STORY.md 5장 — 제우스의 번개는 가끔 오발한다).
		const StoryState& story = g_Director->GetStory();
		bool blessed = story.IsTrue("BLESSING_GRANTED");
		const std::string& patron = story.Get("PATRON");

		if (blessed && patron == "ZEUS")
		{
			float roll = RollChance();
			if (roll < kZeusLightningChance)
			{
				damage *= 2;
				GameLog::Add(GameLog::Kind::Combat, "[번개의 축복] 하늘에서 벼락이 함께 내리꽂힌다!");
			}
			else if (roll < kZeusLightningChance + kZeusMisfireChance)
			{
				GameLog::Add(GameLog::Kind::Combat, "[번개의 축복] 벼락이… 저 멀리 애꿎은 올리브 나무에 떨어졌다. 어디선가 \"경고 사격이다\"라는 목소리가 들린다.");
			}
		}

		GameLog::Add(GameLog::Kind::Combat, "[공격!] " + std::string(names.name) + "에게 " + std::to_string(damage) + " 피해");

		if (target->TakeHit(damage))
		{
			GameLog::Add(GameLog::Kind::Combat, "[" + std::string(names.object) + " 쓰러뜨렸다]");
			g_Player->GrantXP(target->GetKillXP());

			if (blessed && patron == "HADES")
			{
				g_Player->Heal(kHadesHealOnKill);
				GameLog::Add(GameLog::Kind::Reward, "[망자의 축복] 스러진 생명의 온기가 스며든다. 체력 +" + std::to_string(kHadesHealOnKill));
			}

			// 이야기 속 괴물(그림자 퓌톤 등)이면 Data/Story.txt의 "on kill" 대화가 이어진다.
			g_Director->OnKill(*target);
		}
	}

	// target 액터와 상호작용한다. 무엇을 할지(대사·선택지·진행)는 거의 전부 이야기 데이터(Data/Story.txt)가
	// 정하고, 게임 코드는 상호작용 종류만 나눈다. 아이템 획득처럼 그 액터 하나만 없애야 하는 경우 그 액터만
	// 파괴 예약한다(같은 종류의 다른 아이템은 그대로 남는다).
	void HandleInteract(Actor& target)
	{
		int id = target.GetInteractId();

		if (id == kInteractStory)
		{
			// 인물·조사할 물건·이정표: "on talk <스토리ID>" 대화.
			if (!g_Director->OnTalk(target))
			{
				GameLog::Add(GameLog::Kind::Info, "[지금은 별다른 반응이 없다]");
			}
		}
		else if (id == kInteractPickup)
		{
			// 이야기 아이템: "on pickup <스토리ID>" 대화가 열렸으면(= 지금 필요한 물건이면) 주운 것으로 친다.
			if (g_Director->OnPickup(target))
			{
				target.Destroy();
			}
			else
			{
				GameLog::Add(GameLog::Kind::Info, "[지금은 필요 없어 보인다]");
			}
		}
		else if (id == kInteractLoot)
		{
			GameLog::Add(GameLog::Kind::Info, "[약초를 발견해 챙겼다]");
			target.Destroy();
			g_Player->GrantXP(kHerbXP);
		}
	}

	void TryInteract()
	{
		if (g_ActionCooldown > 0.f)
		{
			return;
		}

		Actor* target = FindNearestInteractable();

		if (target == NULL)
		{
			// 실제로 상호작용이 일어난 게 아니므로 쿨타임을 시작하지 않는다(헛 눌렀다고
			// 곧바로 공격까지 막히지 않게).
			GameLog::Add(GameLog::Kind::Info, "[근처에 상호작용할 대상이 없습니다]");
			return;
		}

		g_ActionCooldown = kActionCooldownSeconds;
		HandleInteract(*target);
	}

	// 뷰 컬링을 켜고 끈다. 끄면 화면 밖 액터까지 전부 그리므로, 드로우 콜 수가 얼마나
	// 달라지는지 콘솔의 드로우 콜 출력으로 바로 비교해볼 수 있다.
	void ToggleCulling()
	{
		bool enabled = !g_Scene->IsCullingEnabled();
		g_Scene->SetCullingEnabled(enabled);

		GameLog::Add(GameLog::Kind::Info, enabled ? "[뷰 컬링] 켬" : "[뷰 컬링] 끔");
	}

	// 게임 창이 지금 키보드 입력을 받는 맨 앞 창인지. 다른 창으로 전환한 동안 뗀 키는 KeyUp이 오지
	// 않아서, 전환 전에 누르고 있던 방향키가 계속 눌린 것으로 남아 캐릭터가 혼자 걸어가던 문제를
	// 막는 데 쓴다. 창 핸들을 못 얻었으면 입력을 막지 않도록 true.
	bool IsGameWindowFocused()
	{
		if (g_GameWindow == NULL)
		{
			return true;
		}

		return GetForegroundWindow() == g_GameWindow;
	}

	// 지역(맵)을 새로 짓고 그곳으로 옮긴다. 이전 지역의 씬·타일맵은 지우고(그 안의 액터 포인터도 전부
	// 무효가 됨), 플레이어 능력치(레벨·경험치·체력)와 수호신 축복은 그대로 이어받는다.
	// arrivedByTravel이면 그 지역의 이정표 앞에, 아니면(새 게임) 그 지역의 시작 지점에 선다.
	void LoadMap(const std::string& locId, bool arrivedByTravel)
	{
		PlayerStats stats;
		bool keepStats = (g_Player != NULL);
		if (keepStats)
		{
			stats = g_Player->GetStats();
		}

		bool culling = (g_Scene != NULL) ? g_Scene->IsCullingEnabled() : true;

		g_Player = NULL;
		g_AttackRing = NULL;
		g_InteractRing = NULL;
		delete g_Scene;
		g_Scene = NULL;
		delete g_TileMap;
		g_TileMap = NULL;

		LoadedMap map = WorldMaps::Build(locId, *g_Renderer, g_SessionSeed, arrivedByTravel);
		g_TileMap = map.tileMap;
		g_Scene = map.scene;
		g_Player = map.player;
		g_AttackRing = map.attackRing;
		g_InteractRing = map.interactRing;
		g_Scene->SetCullingEnabled(culling);

		if (keepStats)
		{
			g_Player->SetStats(stats);
		}

		// 이동 속도·괴물이 알아채는 거리 같은 축복 효과는 플레이어 액터에 붙어 있어서 맵마다 다시 건다.
		const StoryState& story = g_Director->GetStory();
		StoryDirector::ApplyPatronModifiers(*g_Player, story.IsTrue("BLESSING_GRANTED") ? story.Get("PATRON") : std::string());

		g_MiniMap->Invalidate();
		g_MapScreen->Invalidate();
		g_Camera->SetFocus(g_Player->GetWorldX(), g_Player->GetWorldY(), 0.f);

		// 마지막에: 지역 이름 배너, 목표 표시, 이 지역에 들어설 때의 대화("on enter").
		g_Director->OnMapLoaded(map.locId, *g_Scene, *g_Player);

		std::cout << "[지역] " << map.locId << " 로딩 완료\n";
	}
}

void RenderScene(void)
{
	// 이 프레임의 드로우 콜 카운터를 0으로 돌린다 (후킹된 glDrawArrays가 센다).
	DrawCallHook::BeginFrame();

	g_PostProcess->BeginCapture();

	// 지우는 색 = 먼바다 색. 섬 둘레에 바다를 깔아 두었지만, 혹시 그 바깥이 보여도 허공이 아니라
	// 바다처럼 보이게 한다.
	glClearColor(0.05f, 0.16f, 0.30f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	Mat4 viewProjection = g_Camera->GetViewProjection();

	// 화면에 배치된 모든 것(타일/오브젝트/캐릭터/링)은 씬 그래프가 알아서 그린다. 씬 그래프는 끝에서
	// 렌더러의 렌더 큐를 비워서(Flush) 씬 버퍼에 다 그려 둔 상태로 돌아온다.
	RenderContext renderContext = { *g_Renderer, viewProjection, g_ElapsedSeconds, g_CircleMesh, g_EllipseMesh };
	g_Scene->Render(renderContext);

	{
		Profiler::ScopedTimer timer(Profiler::Section::PostProcess);
		g_PostProcess->EndCaptureAndPresent(g_Exposure, g_VignetteStrength, g_BloomThreshold, g_BloomIntensity, g_ElapsedSeconds);
	}

	// HUD/이름표는 후처리(블룸/비네트/그레인)가 이미 끝난 기본 프레임버퍼 위에
	// 그대로 덧그려서, 화면 흔들림/줌/색보정의 영향을 받지 않고 항상 또렷하게
	// 보이게 한다. 렌더 큐에 모인 도형은 구간마다 끝에서 Flush해서, 그 구간의 비용이
	// 그 구간에 잡히고 레거시 GL로 그리는 이름표와 순서가 섞이지 않게 한다.
	{
		// 이름표는 레거시 glRasterPos/glutBitmapCharacter 경로라 드로우콜 카운터엔 안
		// 잡히지만 실제 비용은 있으므로, 여기서 따로 재서 눈에 보이게 한다.
		Profiler::ScopedTimer timer(Profiler::Section::NameTags);
		g_Renderer->Flush();
		Hud::DrawNameTags(*g_Scene, viewProjection);
	}
	{
		Profiler::ScopedTimer timer(Profiler::Section::Hud);
		// 쿨타임이 얼마나 충전됐는지(1이면 바로 행동 가능)를 HUD 막대로 보여준다.
		float actionReadiness = (g_ActionCooldown > 0.f) ? 1.f - g_ActionCooldown / kActionCooldownSeconds : 1.f;
		Hud::DrawStatus(*g_Renderer, g_CircleMesh, g_Player->GetStats(), actionReadiness);
		g_Renderer->Flush();
	}

	{
		// 오른쪽 위 미니맵도 HUD처럼 후처리 이후 화면 고정 좌표계로 그린다. 지금 목표 대상은 금빛 마름모(+ 퍼져
		// 나가는 고리)로 표시하고, 구석에 큰 지도를 여는 키(M)를 적어 둔다.
		Profiler::ScopedTimer timer(Profiler::Section::MiniMap);
		g_MiniMap->Draw(*g_Renderer, *g_Scene, *g_TileMap, *g_Camera, g_Director->GetObjectiveSpots(), g_ElapsedSeconds);
		if (!g_MapScreen->IsOpen())
		{
			g_MapScreen->DrawMiniMapHint(*g_Renderer, *g_MiniMap);
		}
		g_Renderer->Flush();
	}

	// 왼쪽 아래 채팅창(NPC 대사와 전투/보상/알림 로그). 3초가 지나면 각 줄이 사라진다. 대화창이나 큰 지도가
	// 열려 있으면 그리지 않는다(그동안은 채팅창 시간도 멈춰 있어서, 그 전에 뜬 줄을 놓치지 않는다).
	if (!g_Director->IsDialogueActive() && !g_MapScreen->IsOpen())
	{
		Profiler::ScopedTimer timer(Profiler::Section::ChatWindow);
		g_ChatWindow->Draw(*g_Renderer);
		g_Renderer->Flush();
	}

	{
		// 목표(미니맵 아래)·지역 이름 배너·대화창은 모든 UI 위에 그린다.
		Profiler::ScopedTimer timer(Profiler::Section::Story);
		g_Director->Draw(*g_Renderer);
		g_Renderer->Flush();
	}

	if (g_MapScreen->IsOpen())
	{
		// 큰 지도(M)는 화면 전체를 덮으므로 맨 마지막에 그린다. 큰 미니맵이라 미니맵 구간에 함께 잰다.
		Profiler::ScopedTimer timer(Profiler::Section::MiniMap);
		g_MapScreen->Draw(*g_Renderer, *g_Scene, *g_TileMap, *g_Camera, *g_Director, g_ElapsedSeconds);
		g_Renderer->Flush();
	}

	{
		// 수직동기화가 켜져 있으면 다음 화면 갱신 시점까지 여기서 기다리므로, 이 구간이
		// 유난히 크게 나온다고 해서 우리 코드가 그만큼 느린 건 아닐 수 있다(구간 이름에
		// 그 가능성을 남겨 둔 이유).
		Profiler::ScopedTimer timer(Profiler::Section::Present);
		glutSwapBuffers();
	}

	// 이 프레임에서 센 드로우 콜 수와 씬의 컬링 결과를 집계하고, 정해진 간격마다
	// 콘솔에 출력한다. 같은 note를 프로파일러 보고 줄에도 그대로 붙여서, 두 로그를
	// 따로 짜맞추지 않아도 한 줄만 봐도 맥락이 보이게 한다.
	const SceneRenderStats& sceneStats = g_Scene->GetLastRenderStats();
	char note[128];
	snprintf(note, sizeof(note), "씬 액터 %u개 중 %u개 렌더, %u개 컬링 (컬링 %s)",
		sceneStats.totalActors, sceneStats.renderedActors, sceneStats.culledActors,
		g_Scene->IsCullingEnabled() ? "켬" : "끔");
	DrawCallHook::EndFrame(note);
	Profiler::EndFrame(note);
}

void Update(float deltaSeconds)
{
	g_ElapsedSeconds += deltaSeconds;

	if (g_ActionCooldown > 0.f)
	{
		g_ActionCooldown -= deltaSeconds;
		if (g_ActionCooldown < 0.f)
		{
			g_ActionCooldown = 0.f;
		}
	}

	// 이야기 진행: 방금 끝난 대화의 효과 적용, 인물·괴물·물건의 존재 조건, 트리거 영역, 목표 표시.
	// 대화창이나 큰 지도가 열려 있는 동안엔 세상이 멈춘다(이동·짐승 AI·공격 없음) — 대사를 읽거나 지도를 보는
	// 동안 늑대에게 물리지 않게.
	{
		Profiler::ScopedTimer timer(Profiler::Section::Story);
		g_Director->Update(deltaSeconds);
	}

	// 지도를 연 사이에 대화가 시작됐으면(대화를 넘기자마자 M을 눌러 대기 중이던 대화가 이어서 열린 경우 등)
	// 지도를 닫는다 — 대화 중엔 키가 대화로 가서 지도를 닫을 수 없게 되므로.
	bool inDialogue = g_Director->IsDialogueActive();
	if (inDialogue && g_MapScreen->IsOpen())
	{
		g_MapScreen->Close();
	}
	bool paused = inDialogue || g_MapScreen->IsOpen();

	// 좌우(A/D)와 상하(W/S) 이동 입력을 모두 반대로 뒤집는다: A는 월드 +x, D는 월드 -x,
	// W는 월드 -y, S는 월드 +y 방향으로 간다. 카메라 화면을 좌우/상하 반전한 상태에서
	// 키를 눌렀을 때 화면에 보이는 방향과 실제로 맞도록 맞춘 것이다. 원래대로 되돌리려면
	// 각각 false로 바꾼다.
	const bool kInvertHorizontalInput = true;
	const bool kInvertVerticalInput = true;
	float horizontalStep = kInvertHorizontalInput ? -1.f : 1.f;
	float verticalStep = kInvertVerticalInput ? -1.f : 1.f;

	// 다른 창으로 전환해 있는 동안 뗀 키는 KeyUp이 오지 않으므로, 게임 창이 맨 앞이 아니면 방향키를
	// 전부 뗀 것으로 본다(돌아왔을 때 캐릭터가 혼자 걸어가지 않게).
	if (!IsGameWindowFocused())
	{
		g_KeyW = g_KeyA = g_KeyS = g_KeyD = false;
		std::fill(std::begin(g_KeyHeld), std::end(g_KeyHeld), false);
		std::fill(std::begin(g_KeyConsumedByDialogue), std::end(g_KeyConsumedByDialogue), false);
	}

	float moveX = 0.f, moveY = 0.f;
	if (!paused)
	{
		if (g_KeyW) moveY += verticalStep;
		if (g_KeyS) moveY -= verticalStep;
		if (g_KeyA) moveX -= horizontalStep;
		if (g_KeyD) moveX += horizontalStep;
	}
	g_Player->SetMoveInput(moveX, moveY);

	{
		Profiler::ScopedTimer timer(Profiler::Section::Update);

		// 플레이어 이동, 짐승 AI, 애니메이션 타이머 등 모든 액터 갱신은 씬 그래프가 한다.
		if (!paused)
		{
			g_Scene->Update(deltaSeconds, g_ElapsedSeconds, *g_TileMap);
		}

		// 파괴 예약된 액터가 정리된 뒤에, 지금 Space/E를 누르면 뭐가 맞을지/
		// 상호작용될지 미리 보여주는 조준 링의 대상을 갱신한다(대화 중·지도를 보는 중엔 숨긴다).
		if (paused)
		{
			g_AttackRing->SetTarget(NULL);
			g_InteractRing->SetTarget(NULL);
		}
		else
		{
			g_AttackRing->SetTarget(FindNearestAttackTarget());
			g_InteractRing->SetTarget(FindNearestInteractable());
		}
	}

	if (!paused)
	{
		// 채팅 메시지가 몰리면 GDI로 텍스처를 새로 굽는 비용이 튈 수 있어서 따로 잰다.
		Profiler::ScopedTimer timer(Profiler::Section::ChatWindow);
		g_ChatWindow->Update(deltaSeconds);
	}

	// 균열이 열릴 때 같은 순간엔 카메라가 흔들린다(EVENT SHAKE).
	float shakeX, shakeY;
	g_Director->GetShakeOffset(g_ElapsedSeconds, shakeX, shakeY);
	g_Camera->SetFocus(g_Player->GetWorldX() + shakeX, g_Player->GetWorldY() + shakeY, 0.f);
}

void Idle(void)
{
	// 이번 프레임의 프로파일러 구간별 누적치를 0으로 돌린다. Update()/RenderScene() 안의
	// ScopedTimer들이 여기부터 EndFrame까지의 구간을 잰다.
	Profiler::BeginFrame();

	// 이야기가 지역 이동을 요청했으면(이정표 대화의 TRAVEL) 이번 프레임을 시작하기 전에 맵을 바꾼다.
	// 맵을 짓는 시간이 다음 프레임의 dt로 들어가지 않도록 프레임 시각을 새로 잡는다.
	std::string travelTo;
	if (g_Director->TakeTravelRequest(travelTo))
	{
		LoadMap(travelTo, true);
		g_LastFrameTime = std::chrono::steady_clock::now();
	}

	std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
	float deltaSeconds = std::chrono::duration<float>(now - g_LastFrameTime).count();
	g_LastFrameTime = now;

	if (deltaSeconds > kMaxDeltaSeconds)
	{
		deltaSeconds = kMaxDeltaSeconds;
	}

	Update(deltaSeconds);
	RenderScene();
}

// 창 크기가 바뀌면 게임 화면(800x600, 4:3)의 비율을 유지한 채 창 가운데에 맞추고, 남는 곳은 검은 띠로
// 둔다. 예전엔 800x600이 곳곳에 고정돼 있어서 창을 최대화하면 왼쪽 아래 800x600에만 그려졌다.
// HUD·미니맵·채팅창·이름표도 이 영역을 기준으로 그려지므로 같이 커지고 줄어든다.
void Reshape(int width, int height)
{
	if (g_PostProcess == NULL || width <= 0 || height <= 0)
	{
		return;
	}

	float scaleX = (float)width / (float)kScreenWidth;
	float scaleY = (float)height / (float)kScreenHeight;
	float scale = (scaleX < scaleY) ? scaleX : scaleY;

	int viewWidth = (int)(kScreenWidth * scale);
	int viewHeight = (int)(kScreenHeight * scale);
	g_PostProcess->SetPresentViewport((width - viewWidth) / 2, (height - viewHeight) / 2, viewWidth, viewHeight);
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
	key = NormalizeKey(key);
	bool repeat = g_KeyHeld[key];
	g_KeyHeld[key] = true;

	// 이동 키는 대화 중에도 눌림 상태를 기록해 둔다(대화가 끝나는 순간 누르고 있던 방향으로 바로 걷도록).
	switch (key)
	{
	case 'w': g_KeyW = true; return;
	case 'a': g_KeyA = true; return;
	case 's': g_KeyS = true; return;
	case 'd': g_KeyD = true; return;
	default: break;
	}

	// 대화 중엔 E/Space/Enter가 다음 줄, 숫자키가 선택지다. 누르고 있을 때 오는 반복 입력은 무시한다.
	if (g_Director->IsDialogueActive())
	{
		if (!repeat)
		{
			g_Director->HandleKey(key);
		}
		g_KeyConsumedByDialogue[key] = true;
		return;
	}

	// 큰 지도가 열려 있으면 게임이 멈춰 있으므로, 지도를 닫는 키(M·Esc)만 받는다.
	if (g_MapScreen->IsOpen())
	{
		if (!repeat && (key == 'm' || key == kEscapeKey))
		{
			g_MapScreen->Close();
		}
		return;
	}

	// 대화를 넘기던 키를 아직 누르고 있다(그 반복 입력이 공격·상호작용으로 이어지지 않게).
	if (g_KeyConsumedByDialogue[key])
	{
		return;
	}

	switch (key)
	{
	case 'e': TryInteract(); break;
	case 'c': ToggleCulling(); break;
	case ' ': TryAttack(); break;
	case 'm':
		// 누르고 있는 동안의 반복 입력으로 열렸다 닫혔다 하지 않게 처음 누를 때만.
		if (!repeat)
		{
			g_MapScreen->Open(*g_Director);
		}
		break;
	default: break;
	}
}

void KeyUp(unsigned char key, int x, int y)
{
	key = NormalizeKey(key);
	g_KeyHeld[key] = false;
	g_KeyConsumedByDialogue[key] = false;

	switch (key)
	{
	case 'w': g_KeyW = false; break;
	case 'a': g_KeyA = false; break;
	case 's': g_KeyS = false; break;
	case 'd': g_KeyD = false; break;
	default: break;
	}
}

void SpecialKeyInput(int key, int x, int y)
{
}

int main(int argc, char **argv)
{
	// 콘솔 코드페이지를 UTF-8로 맞춰서 한글 로그(std::cout)가 깨지지 않게 한다.
	// (소스 파일이 UTF-8로 저장되어 있으므로, 콘솔도 같은 인코딩으로 맞춰야 한다.)
	SetConsoleOutputCP(CP_UTF8);
	SetConsoleCP(CP_UTF8);

	// 첫 드로우 콜이 일어나기 전에 OpenGL 드로우 콜 후킹을 걸어서, 이후 모든
	// glDrawArrays 호출이 카운터를 거치게 한다.
	DrawCallHook::Install();

	// OpenGL 초기화
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DEPTH | GLUT_DOUBLE | GLUT_RGBA);
	glutInitWindowPosition(0, 0);
	glutInitWindowSize(kScreenWidth, kScreenHeight);
	glutCreateWindow("Game Software Engineering KPU");

	// 방금 만든 창의 OpenGL 컨텍스트가 현재 컨텍스트이므로, 그 DC에서 창 핸들을 얻어 둔다.
	g_GameWindow = WindowFromDC(wglGetCurrentDC());

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
	g_Renderer = new Renderer(kScreenWidth, kScreenHeight);
	if (!g_Renderer->IsInitialized())
	{
		std::cout << "렌더러를 초기화하지 못했습니다.\n";
	}

	// 기본 시야 16x12 월드 단위(예전 8x6의 2배 = 카메라를 두 배 멀리 뺀 것과 같음). 직교 투영이라
	// "거리"는 한 화면에 담기는 범위로 정해진다.
	g_Camera = new Camera(16.f, 12.f, 1.f);

	// 카메라 화면을 좌우·상하로 모두 뒤집는다(그림이 180도 돌아간 상태). HUD는 별도의 화면
	// 좌표계라 뒤집히지 않고, 이름표는 뒤집힌 화면 위치를 따라간다. 되돌리려면 (false, false).
	g_Camera->SetFlip(true, true);

	g_PostProcess = new PostProcess(kScreenWidth, kScreenHeight);

	// 원기둥 원판·세운 타원(머리, 나무 수관, 아이템 등)에 쓸 원형 메시와 발밑 링에 쓸 타원 메시.
	// 처음 실행할 땐 만들어서 ./Cache에 저장하고, 다음 실행부터는 파일에서 그대로 불러온다.
	// 나무 수관·원기둥 원판처럼 큰 원도 매끄럽게 보이도록 32각형으로 만든다.
	MeshData circleMeshData = MeshCache::GetOrCreate("circle_32", []() { return MeshGen::GenerateCircle(32); });
	g_CircleMesh = g_Renderer->CreateMesh(circleMeshData);

	MeshData ellipseMeshData = MeshCache::GetOrCreate("ellipse_16", []() { return MeshGen::GenerateEllipse(0.5f, 0.35f, 16); });
	g_EllipseMesh = g_Renderer->CreateMesh(ellipseMeshData);

	g_MiniMap = new MiniMap();
	g_MapScreen = new MapScreen();
	g_ChatWindow = new ChatWindow();

	// 이야기 데이터(대사·진행 단계·지역)를 읽는다. 파일이 없거나 깨져도 게임은 대화 없이 돌아간다.
	g_Director = new StoryDirector();
	if (!g_Director->Initialize(kStoryDataPath))
	{
		std::cout << "[이야기] " << kStoryDataPath << "를 읽지 못했습니다. 대화 없이 시작합니다.\n";
	}

	// 첫 지역(델포이)을 짓고 그곳에서 시작한다. 이후 지역 이동은 이야기(이정표 대화)가 요청한다.
	g_SessionSeed = std::random_device{}();
	LoadMap(kFirstLocation, false);

	GameLog::Add(GameLog::Kind::Info, "[조작] WASD 이동, E 대화·조사, Space 공격, M 지도, 마우스 휠 확대/축소. 대화는 E/Space로 넘기고 숫자키로 고른다.");
	GameLog::Add(GameLog::Kind::Info, "[길잡이] 발밑의 금빛 화살표와 미니맵의 금빛 점이 지금 목표를 가리킨다.");

	g_LastFrameTime = std::chrono::steady_clock::now();

	glutDisplayFunc(RenderScene);
	glutReshapeFunc(Reshape);
	glutIdleFunc(Idle);
	glutKeyboardFunc(KeyInput);
	glutKeyboardUpFunc(KeyUp);
	glutMouseFunc(MouseInput);
	glutMouseWheelFunc(MouseWheel);
	glutSpecialFunc(SpecialKeyInput);

	glutMainLoop();

	DrawCallHook::Uninstall();

	g_Renderer->DestroyMesh(g_CircleMesh);
	g_Renderer->DestroyMesh(g_EllipseMesh);
	delete g_Director;
	delete g_MapScreen;
	delete g_ChatWindow;
	delete g_MiniMap;
	delete g_Scene;
	delete g_TileMap;
	delete g_PostProcess;
	delete g_Camera;
	delete g_Renderer;

	return 0;
}
