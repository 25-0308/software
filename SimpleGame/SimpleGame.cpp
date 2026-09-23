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
#include <chrono>
#include <cmath>
#include <iostream>
#include "Dependencies\glew.h"
#include "Dependencies\freeglut.h"

#include "Camera.h"
#include "ChatWindow.h"
#include "CharacterActors.h"
#include "DrawCallHook.h"
#include "GameLog.h"
#include "Hud.h"
#include "LevelBuilder.h"
#include "LevelGenerator.h"
#include "Mesh.h"
#include "MeshCache.h"
#include "MiniMap.h"
#include "PostProcess.h"
#include "Profiler.h"
#include "Renderer.h"
#include "SceneGraph.h"
#include "TileMap.h"
#include "WorldActors.h"

namespace
{
	enum class QuestState
	{
		NotStarted,
		ItemRequested,
		ItemCollected,
		Completed,
	};

	// 공격/상호작용 판정 반경. Try*()와 조준 링(RingActor) 대상 지정이 같은 값을
	// 공유해야 "링이 보이면 실제로 닿는다"는 예측이 항상 맞는다.
	const float kAttackRadius = 1.6f;
	const float kInteractRadius = 1.4f;
}

Renderer *g_Renderer = NULL;
Camera *g_Camera = NULL;
PostProcess *g_PostProcess = NULL;
TileMap *g_TileMap = NULL;
SceneGraph *g_Scene = NULL;
MiniMap *g_MiniMap = NULL;
ChatWindow *g_ChatWindow = NULL;

// 씬 그래프가 소유하는 액터들을 게임 코드가 빠르게 참조하기 위한 포인터.
PlayerActor *g_Player = NULL;
RingActor *g_AttackRing = NULL;
RingActor *g_InteractRing = NULL;

MeshHandle g_CircleMesh;
MeshHandle g_EllipseMesh; // 나무 수관, 아이템 등 둥글넓적한 파츠에 재사용하는 타원 메시

QuestState g_QuestState = QuestState::NotStarted;

bool g_KeyW = false, g_KeyA = false, g_KeyS = false, g_KeyD = false;

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

		int damage = g_Player->GetStats().attackPower;
		GameLog::Add(GameLog::Kind::Combat, "[공격!] 짐승에게 " + std::to_string(damage) + " 피해");

		if (target->TakeHit(damage))
		{
			GameLog::Add(GameLog::Kind::Combat, "[짐승을 쓰러뜨렸다]");
			g_Player->GrantXP(30);
		}
	}

	// target 액터와 상호작용한다. 아이템 획득처럼 그 액터 하나만 없애야 하는
	// 경우 그 액터만 파괴 예약한다(같은 종류의 다른 아이템은 그대로 남는다).
	void HandleInteract(Actor& target)
	{
		int id = target.GetInteractId();

		if (id == kInteractElder)
		{
			if (g_QuestState == QuestState::NotStarted)
			{
				GameLog::Add(GameLog::Kind::Dialogue, "[장로] 호수 근처에서 잃어버린 제물을 찾아다오.");
				g_QuestState = QuestState::ItemRequested;
			}
			else if (g_QuestState == QuestState::ItemRequested)
			{
				GameLog::Add(GameLog::Kind::Dialogue, "[장로] 아직 제물을 찾지 못했구나. 호수 쪽을 살펴보게.");
			}
			else if (g_QuestState == QuestState::ItemCollected)
			{
				GameLog::Add(GameLog::Kind::Dialogue, "[장로] 오, 찾아왔구나! 그대에게 작은 축복을 내리네.");
				g_QuestState = QuestState::Completed;
				g_Player->GrantXP(50);
				// 보상: 플레이어가 블룸이 걸릴 만큼 밝아진다 (작은 시각적 보상).
				g_Player->SetColor(2.5f, 2.2f, 1.6f);
			}
			else
			{
				GameLog::Add(GameLog::Kind::Dialogue, "[장로] 마을을 지켜줘서 고맙네.");
			}
		}
		else if (id == kInteractQuestItem)
		{
			if (g_QuestState == QuestState::ItemRequested)
			{
				GameLog::Add(GameLog::Kind::Info, "[잃어버린 제물을 주웠다. 장로에게 가져다주자.]");
				g_QuestState = QuestState::ItemCollected;
				target.Destroy();
			}
			else
			{
				GameLog::Add(GameLog::Kind::Info, "[이미 조사했다.]");
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
			GameLog::Add(GameLog::Kind::Dialogue, lines[lineIndex % 4]);
			++lineIndex;
		}
		else if (id == kInteractLoot)
		{
			GameLog::Add(GameLog::Kind::Info, "[약초를 발견해 챙겼다]");
			target.Destroy();
			g_Player->GrantXP(15);
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
}

void RenderScene(void)
{
	// 이 프레임의 드로우 콜 카운터를 0으로 돌린다 (후킹된 glDrawArrays가 센다).
	DrawCallHook::BeginFrame();

	g_PostProcess->BeginCapture();

	glClearColor(0.05f, 0.05f, 0.08f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	Mat4 viewProjection = g_Camera->GetViewProjection();

	// 화면에 배치된 모든 것(타일/오브젝트/캐릭터/링)은 씬 그래프가 알아서 그린다.
	RenderContext renderContext = { *g_Renderer, viewProjection, g_ElapsedSeconds, g_CircleMesh, g_EllipseMesh };
	g_Scene->Render(renderContext);

	{
		Profiler::ScopedTimer timer(Profiler::Section::PostProcess);
		g_PostProcess->EndCaptureAndPresent(g_Exposure, g_VignetteStrength, g_BloomThreshold, g_BloomIntensity, g_ElapsedSeconds);
	}

	// HUD/이름표는 후처리(블룸/비네트/그레인)가 이미 끝난 기본 프레임버퍼 위에
	// 그대로 덧그려서, 화면 흔들림/줌/색보정의 영향을 받지 않고 항상 또렷하게
	// 보이게 한다.
	{
		// 이름표는 레거시 glRasterPos/glutBitmapCharacter 경로라 드로우콜 카운터엔 안
		// 잡히지만 실제 비용은 있으므로, 여기서 따로 재서 눈에 보이게 한다.
		Profiler::ScopedTimer timer(Profiler::Section::NameTags);
		Hud::DrawNameTags(*g_Scene, viewProjection);
	}
	{
		Profiler::ScopedTimer timer(Profiler::Section::Hud);
		// 쿨타임이 얼마나 충전됐는지(1이면 바로 행동 가능)를 HUD 막대로 보여준다.
		float actionReadiness = (g_ActionCooldown > 0.f) ? 1.f - g_ActionCooldown / kActionCooldownSeconds : 1.f;
		Hud::DrawStatus(*g_Renderer, g_CircleMesh, g_Player->GetStats(), actionReadiness);
	}

	{
		// 오른쪽 위 미니맵도 HUD처럼 후처리 이후 화면 고정 좌표계로 그린다.
		Profiler::ScopedTimer timer(Profiler::Section::MiniMap);
		g_MiniMap->Draw(*g_Renderer, *g_Scene, *g_TileMap, *g_Camera);
	}

	{
		// 왼쪽 아래 채팅창(NPC 대사와 전투/보상/알림 로그). 3초가 지나면 각 줄이 사라진다.
		Profiler::ScopedTimer timer(Profiler::Section::ChatWindow);
		g_ChatWindow->Draw(*g_Renderer);
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

	// 좌우(A/D)와 상하(W/S) 이동 입력을 모두 반대로 뒤집는다: A는 월드 +x, D는 월드 -x,
	// W는 월드 -y, S는 월드 +y 방향으로 간다. 카메라 화면을 좌우/상하 반전한 상태에서
	// 키를 눌렀을 때 화면에 보이는 방향과 실제로 맞도록 맞춘 것이다. 원래대로 되돌리려면
	// 각각 false로 바꾼다.
	const bool kInvertHorizontalInput = true;
	const bool kInvertVerticalInput = true;
	float horizontalStep = kInvertHorizontalInput ? -1.f : 1.f;
	float verticalStep = kInvertVerticalInput ? -1.f : 1.f;

	float moveX = 0.f, moveY = 0.f;
	if (g_KeyW) moveY += verticalStep;
	if (g_KeyS) moveY -= verticalStep;
	if (g_KeyA) moveX -= horizontalStep;
	if (g_KeyD) moveX += horizontalStep;
	g_Player->SetMoveInput(moveX, moveY);

	{
		Profiler::ScopedTimer timer(Profiler::Section::Update);

		// 플레이어 이동, 짐승 AI, 애니메이션 타이머 등 모든 액터 갱신은 씬 그래프가 한다.
		g_Scene->Update(deltaSeconds, g_ElapsedSeconds, *g_TileMap);

		// 파괴 예약된 액터가 정리된 뒤에, 지금 Space/E를 누르면 뭐가 맞을지/
		// 상호작용될지 미리 보여주는 조준 링의 대상을 갱신한다.
		g_AttackRing->SetTarget(FindNearestAttackTarget());
		g_InteractRing->SetTarget(FindNearestInteractable());
	}

	{
		// 채팅 메시지가 몰리면 GDI로 텍스처를 새로 굽는 비용이 튈 수 있어서 따로 잰다.
		Profiler::ScopedTimer timer(Profiler::Section::ChatWindow);
		g_ChatWindow->Update(deltaSeconds);
	}

	g_Camera->SetFocus(g_Player->GetWorldX(), g_Player->GetWorldY(), 0.f);
}

void Idle(void)
{
	// 이번 프레임의 프로파일러 구간별 누적치를 0으로 돌린다. Update()/RenderScene() 안의
	// ScopedTimer들이 여기부터 EndFrame까지의 구간을 잰다.
	Profiler::BeginFrame();

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
	case 'c': case 'C': ToggleCulling(); break;
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

	// 카메라 화면을 좌우·상하로 모두 뒤집는다(그림이 180도 돌아간 상태). HUD는 별도의 화면
	// 좌표계라 뒤집히지 않고, 이름표는 뒤집힌 화면 위치를 따라간다. 되돌리려면 (false, false).
	g_Camera->SetFlip(true, true);

	g_PostProcess = new PostProcess(800, 600);

	// 캐릭터 머리에 쓸 원형 메시, 나무 수관/아이템에 쓸 타원 메시. 처음 실행할 땐
	// 만들어서 ./Cache에 저장하고, 다음 실행부터는 파일에서 그대로 불러온다.
	// 나무 수관·원기둥 원판처럼 큰 원도 매끄럽게 보이도록 32각형으로 만든다.
	MeshData circleMeshData = MeshCache::GetOrCreate("circle_32", []() { return MeshGen::GenerateCircle(32); });
	g_CircleMesh = g_Renderer->CreateMesh(circleMeshData);

	MeshData ellipseMeshData = MeshCache::GetOrCreate("ellipse_16", []() { return MeshGen::GenerateEllipse(0.5f, 0.35f, 16); });
	g_EllipseMesh = g_Renderer->CreateMesh(ellipseMeshData);

	// 가로/세로 2배 = 면적 4배. (32x32)
	const int kMapWidth = 32;
	const int kMapHeight = 32;
	g_TileMap = new TileMap(kMapWidth, kMapHeight);
	LevelLayout layout = GenerateVillageLevel(*g_TileMap);

	// 화면에 배치되는 모든 것을 액터로 만들어 씬 그래프에 올린다.
	g_Scene = new SceneGraph();
	SpawnTileActors(*g_Scene, *g_Renderer, *g_TileMap);
	LevelActors level = SpawnLevelActors(*g_Scene, layout);
	g_Player = level.player;
	g_AttackRing = level.attackRing;
	g_InteractRing = level.interactRing;

	g_MiniMap = new MiniMap();

	g_ChatWindow = new ChatWindow();
	GameLog::Add(GameLog::Kind::Info, "[튜토리얼] WASD로 이동, E로 상호작용, Space로 공격. 장로를 찾아가보자.");

	g_LastFrameTime = std::chrono::steady_clock::now();

	glutDisplayFunc(RenderScene);
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
	delete g_ChatWindow;
	delete g_MiniMap;
	delete g_Scene;
	delete g_TileMap;
	delete g_PostProcess;
	delete g_Camera;
	delete g_Renderer;

	return 0;
}
