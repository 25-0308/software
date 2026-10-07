#include "stdafx.h"
#include "WorldMaps.h"

#include <cmath>
#include <memory>
#include <random>
#include <vector>

#include "CharacterActors.h"
#include "LandmarkActors.h"
#include "LevelBuilder.h"
#include "LevelGenerator.h"
#include "SceneGraph.h"
#include "TileMap.h"
#include "WorldActors.h"

namespace
{
	const int kMapSize = 32;
	const float kPi = 3.14159265f;

	// 어느 지역에서나 같은 문자열 → 같은 수(지역마다 다른 시드를 만들 때).
	unsigned int HashString(const std::string& text)
	{
		unsigned int hash = 2166136261u;
		for (char c : text)
		{
			hash ^= (unsigned char)c;
			hash *= 16777619u;
		}
		return hash;
	}

	float AngleDistance(float a, float b)
	{
		float difference = fmodf(fabsf(a - b), 2.f * kPi);
		return (difference > kPi) ? 2.f * kPi - difference : difference;
	}

	// ---- 인물 겉모습(STORY.md 5장 인물 정의를 한눈에 알아볼 특징으로) ----

	HumanLook PythiaLook()
	{
		// 델포이 무녀: 긴 흑발 + 월계관 + 흰 무녀복.
		HumanLook look;
		look.hairR = 0.16f; look.hairG = 0.1f; look.hairB = 0.07f;
		look.longHair = true;
		look.robe = true;
		look.wreath = true; look.wreathR = 0.34f; look.wreathG = 0.56f; look.wreathB = 0.22f;
		return look;
	}

	HumanLook OldWomanLook()
	{
		// 떠돌이 점술가 노파(= 에리스의 위장): 두건 + 긴 옷 + 지팡이 + 왼손의 금빛 사과(복선 F-02).
		HumanLook look;
		look.hairR = 0.72f; look.hairG = 0.7f; look.hairB = 0.66f;
		look.hood = true; look.hoodR = 0.34f; look.hoodG = 0.2f; look.hoodB = 0.17f;
		look.robe = true;
		look.staff = true;
		look.apple = true;
		return look;
	}

	HumanLook ZeusLook()
	{
		// 흰 머리·흰 수염 + 금관 + 홀(지팡이).
		HumanLook look;
		look.hairR = 0.95f; look.hairG = 0.95f; look.hairB = 0.95f;
		look.beard = true;
		look.robe = true;
		look.wreath = true;
		look.staff = true;
		return look;
	}

	HumanLook PoseidonLook()
	{
		// 바닷빛 회색 머리·수염 + 삼지창.
		HumanLook look;
		look.hairR = 0.5f; look.hairG = 0.62f; look.hairB = 0.6f;
		look.beardR = 0.5f; look.beardG = 0.62f; look.beardB = 0.6f;
		look.beard = true;
		look.robe = true;
		look.trident = true;
		return look;
	}

	HumanLook HadesLook()
	{
		// 검은 머리·수염 + 어두운 쇠관.
		HumanLook look;
		look.hairR = 0.08f; look.hairG = 0.07f; look.hairB = 0.08f;
		look.beardR = 0.1f; look.beardG = 0.09f; look.beardB = 0.1f;
		look.beard = true;
		look.robe = true;
		look.wreath = true; look.wreathR = 0.3f; look.wreathG = 0.28f; look.wreathB = 0.34f;
		return look;
	}

	HumanLook AphroditeLook()
	{
		// 금빛 긴 머리 + 장미관.
		HumanLook look;
		look.hairR = 0.95f; look.hairG = 0.78f; look.hairB = 0.42f;
		look.longHair = true;
		look.robe = true;
		look.wreath = true; look.wreathR = 1.f; look.wreathG = 0.55f; look.wreathB = 0.66f;
		return look;
	}

	HumanLook HermesLook()
	{
		// 날개 달린 챙 넓은 모자 + 전령의 지팡이(케뤼케이온). 짧은 튜닉이라 다리가 보인다.
		HumanLook look;
		look.hairR = 0.45f; look.hairG = 0.3f; look.hairB = 0.16f;
		look.wingedHat = true;
		look.caduceus = true;
		return look;
	}

	HumanLook HeraLook()
	{
		HumanLook look;
		look.hairR = 0.14f; look.hairG = 0.09f; look.hairB = 0.07f;
		look.longHair = true;
		look.robe = true;
		look.wreath = true;
		return look;
	}

	HumanLook AthenaLook()
	{
		HumanLook look;
		look.helmet = true;
		look.spear = true;
		look.robe = true;
		return look;
	}

	HumanLook AresLook()
	{
		HumanLook look;
		look.helmet = true;
		look.spear = true;
		return look;
	}

	HumanLook DionysusLook()
	{
		HumanLook look;
		look.hairR = 0.2f; look.hairG = 0.12f; look.hairB = 0.18f;
		look.longHair = true;
		look.robe = true;
		look.wreath = true; look.wreathR = 0.28f; look.wreathG = 0.52f; look.wreathB = 0.22f; // 담쟁이관
		look.wineCup = true;
		return look;
	}

	HumanLook HephaestusLook()
	{
		HumanLook look;
		look.hairR = 0.22f; look.hairG = 0.14f; look.hairB = 0.08f;
		look.beardR = 0.22f; look.beardG = 0.14f; look.beardB = 0.08f;
		look.beard = true;
		look.hammer = true;
		return look;
	}

	HumanLook PanLook()
	{
		HumanLook look;
		look.hairR = 0.4f; look.hairG = 0.28f; look.hairB = 0.16f;
		look.beardR = 0.42f; look.beardG = 0.3f; look.beardB = 0.18f;
		look.beard = true;
		look.horns = true;
		look.goatLegs = true;
		return look;
	}

	HumanLook HearthGodLook()
	{
		HumanLook look;
		look.hairR = 0.6f; look.hairG = 0.58f; look.hairB = 0.55f;
		look.beardR = 0.6f; look.beardG = 0.58f; look.beardB = 0.55f;
		look.beard = true;
		look.robe = true;
		return look;
	}

	// 순례객·상인·마을 사람·시민 같은 보통 사람: 번호로 머리 색을 돌려 가며 고르고, 일부는 긴 옷.
	HumanLook CommonLook(int index, bool robe)
	{
		const float kHairColors[5][3] =
		{
			{ 0.20f, 0.13f, 0.07f },
			{ 0.09f, 0.07f, 0.06f },
			{ 0.42f, 0.24f, 0.11f },
			{ 0.55f, 0.50f, 0.44f },
			{ 0.30f, 0.20f, 0.10f },
		};

		const float* hair = kHairColors[index % 5];
		HumanLook look;
		look.hairR = hair[0];
		look.hairG = hair[1];
		look.hairB = hair[2];
		look.robe = robe;
		look.longHair = (index % 3 == 1);
		return look;
	}

	// ---- 공용 배치 ----

	// 집 한 채(+ 카메라 쪽 문 옆 횃불).
	void SpawnHouse(SceneGraph& scene, float x, float y, float size, bool torch)
	{
		BuildingActor* building = scene.Spawn<BuildingActor>(x, y, size);
		if (torch)
		{
			building->AddChild(std::unique_ptr<Actor>(new FireActor(size * 0.3f, size * 0.61f, 0.8f, 0.5f)));
		}
	}

	// 지역 이동 이정표(+ 조사하면 Data/Story.txt의 OBJ_SIGNPOST 대화).
	void SpawnSignpost(SceneGraph& scene, float x, float y)
	{
		SignpostActor* signpost = scene.Spawn<SignpostActor>(x, y);
		signpost->SetStoryId("OBJ_SIGNPOST");
		signpost->SetInteractId(kInteractStory);
	}

	TriggerZoneActor* SpawnTrigger(SceneGraph& scene, float x, float y, float radius, const char* storyId)
	{
		TriggerZoneActor* zone = scene.Spawn<TriggerZoneActor>(x, y, radius);
		zone->SetStoryId(storyId);
		return zone;
	}

	// 동료 피티아: 동굴로 피신한 뒤(MQ_00.CAVE)부터 플레이어를 따라다닌다(MQ_04에서 잃기 전까지).
	// 어느 지역이든 플레이어 곁에 하나씩 두고, 존재 조건이 참일 때만 나타난다.
	void SpawnCompanions(SceneGraph& scene, PlayerActor& player)
	{
		NpcActor* pythia = SpawnStoryNpc(scene, player.GetWorldX() - 1.2f, player.GetWorldY(), 0.95f, 0.93f, 0.9f, 0.97f,
			PythiaLook(), "CHR_PYTHIA", "Pythia", "STORY_STEP >= MQ_00.CAVE && !PYTHIA_LOST");
		pythia->SetFollowTarget(&player);
	}

	// 회의장 식탁 둘레의 자리: 식탁 중심에서 각도 angleDegrees 방향으로 radius만큼, 식탁을 바라보게.
	NpcActor* SpawnSeated(SceneGraph& scene, float centerX, float centerY, float angleDegrees, float radius, float size,
		float r, float g, float b, const HumanLook& look, const char* storyId, const char* nameTag, const char* presence)
	{
		float angle = angleDegrees * kPi / 180.f;
		float x = centerX + cosf(angle) * radius;
		float y = centerY + sinf(angle) * radius;

		NpcActor* npc = SpawnStoryNpc(scene, x, y, size, r, g, b, look, storyId, nameTag, presence);
		npc->SetFacing(atan2f(centerY - y, centerX - x));
		return npc;
	}

	// 지형·지형지물을 다 놓은 뒤 공통으로 마무리: 바위·나무·플레이어·동료·분위기.
	LevelActors FinishMap(SceneGraph& scene, const TileMap& tileMap, std::mt19937& rng, const std::vector<Spot>& avoid,
		int treeCount, float cypressRatio, float rockDensity, bool snowyRocks, const Spot& start)
	{
		ScatterRocks(scene, tileMap, rng, rockDensity, snowyRocks);
		ScatterTrees(scene, tileMap, rng, treeCount, avoid, 2.2f, cypressRatio);

		LevelActors actors = SpawnPlayerAndMarkers(scene, start.x, start.y);
		SpawnCompanions(scene, *actors.player);
		SpawnAmbience(scene, tileMap);
		return actors;
	}

	// =====================================================================================
	// LOC_DELPHI — 델포이 · 파르나소스 산기슭 (시작 지역: MQ_00 프롤로그, MQ_01 수호신의 선택)
	// 화면 위쪽(월드 x+y가 작은 쪽)에 파르나소스 산, 그 아래 산을 깎은 테라스에 아폴론 신전과 신탁 제단,
	// 제단 앞 광장에서 내려가는 성스러운 길을 따라 시장과 마을, 화면 오른쪽에 카스탈리아 샘과 월계수 숲,
	// 화면 왼쪽에 동굴이 난 바위 언덕과 노파의 모닥불, 맨 아래 길 끝에 이정표.
	// =====================================================================================
	LevelActors BuildDelphi(SceneGraph& scene, TileMap& tileMap, Renderer& renderer, std::mt19937& rng, int seed, bool arrivedByTravel)
	{
		const Spot kStart = { 2.6f, 5.0f };      // 새 게임 시작: 광장 아래 시장 길
		const Spot kArrival = { 10.6f, 9.3f };   // 이정표로 도착
		const Spot kSignpost = { 11.8f, 10.4f };
		const Spot kTemple = { -4.2f, -3.8f };
		const Spot kPlaza = { -1.5f, 1.0f };
		const Spot kAltar = { -3.5f, 0.2f };
		const Spot kMarket = { 5.4f, 4.4f };
		const Spot kSpring = { -9.5f, 2.0f };
		const Spot kCaveHill = { 5.6f, -11.0f };
		const Spot kCave = { 4.6f, -8.5f };
		const Spot kCampfire = { 7.0f, -6.2f };

		// ---- 지형 ----
		Terrain::Fill(tileMap, TileType::Grass);

		// 파르나소스 산: 화면 위쪽을 덮는 바위 산자락. 경계를 노이즈로 구불구불하게.
		Terrain::PaintWhere(tileMap, [seed](float x, float y)
		{
			return x + y < -12.f + Terrain::Noise(x * 0.25f, y * 0.25f, seed) * 3.f - 1.5f;
		}, TileType::Rock);

		// 동굴이 난 바위 언덕(화면 왼쪽).
		Terrain::PaintDisk(tileMap, kCaveHill.x, kCaveHill.y, 2.6f, TileType::Rock);

		// 길: 광장 → 시장 → 이정표(성스러운 길), 광장 → 카스탈리아 샘, 광장 → 동굴 언덕.
		Terrain::PaintLine(tileMap, kPlaza.x + 2.8f, kPlaza.y + 2.2f, 4.0f, 5.0f, 0.7f, TileType::Path);
		Terrain::PaintLine(tileMap, 4.0f, 5.0f, kSignpost.x - 0.6f, kSignpost.y - 0.6f, 0.7f, TileType::Path);
		Terrain::PaintLine(tileMap, kPlaza.x - 3.8f, kPlaza.y + 0.8f, kSpring.x + 2.0f, kSpring.y + 0.3f, 0.55f, TileType::Path);
		Terrain::PaintLine(tileMap, kPlaza.x + 2.5f, kPlaza.y - 3.0f, kCave.x, kCave.y + 1.7f, 0.55f, TileType::Path);

		// 신전 테라스(산을 깎아 만든 돌바닥) + 제단 앞 광장 + 시장 광장.
		Terrain::PaintDisk(tileMap, kTemple.x, kTemple.y, 4.7f, TileType::Stone);
		Terrain::PaintDisk(tileMap, kPlaza.x, kPlaza.y, 4.2f, TileType::Stone);
		Terrain::PaintDisk(tileMap, kMarket.x, kMarket.y, 2.2f, TileType::Stone);

		// 카스탈리아 샘(작은 연못).
		Terrain::PaintDisk(tileMap, kSpring.x, kSpring.y, 1.7f, TileType::Water);

		Spot start = arrivedByTravel ? kArrival : kStart;
		Terrain::EnsureFullyConnected(tileMap, start.x, start.y);
		SpawnTileActors(scene, renderer, tileMap, seed);

		// ---- 신전·제단·균열 ----
		scene.Spawn<TempleActor>(kTemple.x, kTemple.y, 6.f, 4.2f, 1.8f, false);
		scene.Spawn<AltarActor>(kAltar.x, kAltar.y);
		SpawnTrigger(scene, kAltar.x + 0.3f, kAltar.y + 1.2f, 1.f, "TRIGGER_ALTAR");

		CrackActor* crack = scene.Spawn<CrackActor>(kAltar.x + 0.8f, kAltar.y + 1.8f, 3.4f);
		crack->SetPresence("CRACK_OPEN");

		// ---- 동굴과 노파의 모닥불 ----
		scene.Spawn<CaveEntranceActor>(kCave.x, kCave.y, 1.6f);
		SpawnTrigger(scene, kCave.x, kCave.y + 1.5f, 1.1f, "TRIGGER_CAVE");

		FireActor* campfire = scene.Spawn<FireActor>(kCampfire.x, kCampfire.y, 0.f, 0.6f, true);
		campfire->SetPresence("STORY_STEP >= MQ_00.OLDWOMAN");

		// ---- 시장과 마을 ----
		scene.Spawn<StallActor>(4.7f, 3.5f, 0.82f, 0.3f, 0.26f);
		scene.Spawn<StallActor>(6.3f, 5.3f, 0.3f, 0.48f, 0.72f);
		SpawnHouse(scene, 7.2f, 0.8f, 1.4f, true);
		SpawnHouse(scene, 8.9f, 3.6f, 1.3f, false);
		SpawnHouse(scene, 2.0f, 7.8f, 1.4f, true);
		SpawnHouse(scene, -1.2f, 9.0f, 1.3f, false);
		SpawnHouse(scene, 10.4f, -1.6f, 1.4f, true);
		SpawnSignpost(scene, kSignpost.x, kSignpost.y);

		// ---- 인물 ----
		// 피티아(신전): 동굴로 피신하기 전까지 제단 곁 삼발이 옆에 있다(그 뒤로는 동료로 따라다닌다).
		SpawnStoryNpc(scene, -4.9f, 0.9f, 0.95f, 0.93f, 0.9f, 0.97f, PythiaLook(), "CHR_PYTHIA", "Pythia", "STORY_STEP < MQ_00.CAVE");

		// 순례객·상인·마을 사람: 균열이 열리자 모두 도망쳤다가, 신들이 내려온 뒤(MQ_01) 구경하러 돌아온다.
		const char* kCrowd = "!CRACK_OPEN || STORY_STEP >= MQ_01.INTERVIEW";
		SpawnStoryNpc(scene, -6.4f, -0.4f, 0.9f, 0.62f, 0.46f, 0.3f, CommonLook(0, true), "NPC_PILGRIM", "Pilgrim", kCrowd);
		SpawnStoryNpc(scene, 0.2f, -1.0f, 0.9f, 0.4f, 0.5f, 0.66f, CommonLook(1, true), "NPC_PILGRIM", "Pilgrim", kCrowd);
		SpawnStoryNpc(scene, -0.2f, 0.4f, 0.9f, 0.75f, 0.68f, 0.5f, CommonLook(2, true), "NPC_PILGRIM", "Pilgrim", kCrowd);
		SpawnStoryNpc(scene, 3.6f, 3.7f, 0.9f, 0.7f, 0.55f, 0.35f, CommonLook(3, false), "NPC_MERCHANT", "Merchant", kCrowd);
		SpawnStoryNpc(scene, 7.2f, 5.0f, 0.9f, 0.45f, 0.36f, 0.6f, CommonLook(4, false), "NPC_MERCHANT", "Merchant", kCrowd);
		SpawnStoryNpc(scene, 6.0f, 1.8f, 0.9f, 0.55f, 0.62f, 0.4f, CommonLook(5, false), "NPC_DELPHI_VILLAGER", "Villager", kCrowd);
		SpawnStoryNpc(scene, 1.0f, 6.8f, 0.9f, 0.78f, 0.5f, 0.42f, CommonLook(6, false), "NPC_DELPHI_VILLAGER", "Villager", kCrowd);

		// 떠돌이 노파(에리스의 위장): 피티아가 깨어난 뒤 모닥불 곁에 나타나 계속 머문다.
		SpawnStoryNpc(scene, kCampfire.x + 0.8f, kCampfire.y + 0.9f, 0.85f, 0.3f, 0.24f, 0.22f, OldWomanLook(), "CHR_ERIS", "Old Woman",
			"STORY_STEP >= MQ_00.OLDWOMAN");

		// 헤르메스: 프롤로그 끝에 모닥불 근처로 "착지"했다가, MQ_01에선 신전 광장에서 면접 일정을 진행한다.
		SpawnStoryNpc(scene, 5.4f, -4.6f, 1.f, 0.55f, 0.72f, 0.92f, HermesLook(), "CHR_HERMES", "Hermes", "STORY_STEP == MQ_00.HERMES");

		// MQ_01 면접: 다섯 신이 제단 앞 광장에 화면 가로로 늘어선다(수호신을 고르면 모두 떠난다).
		const char* kInterview = "STORY_STEP >= MQ_01.INTERVIEW && STORY_STEP < MQ_01.CHOSEN";
		SpawnStoryNpc(scene, 0.4f, 3.6f, 1.25f, 1.02f, 1.f, 0.94f, ZeusLook(), "CHR_ZEUS", "Zeus", kInterview);
		SpawnStoryNpc(scene, -0.8f, 4.8f, 1.2f, 0.2f, 0.5f, 0.55f, PoseidonLook(), "CHR_POSEIDON", "Poseidon", kInterview);
		SpawnStoryNpc(scene, 1.6f, 2.4f, 1.15f, 0.16f, 0.12f, 0.2f, HadesLook(), "CHR_HADES", "Hades", kInterview);
		SpawnStoryNpc(scene, -2.0f, 6.0f, 1.05f, 0.95f, 0.6f, 0.7f, AphroditeLook(), "CHR_APHRODITE", "Aphrodite", kInterview);
		SpawnStoryNpc(scene, 2.8f, 1.2f, 1.f, 0.55f, 0.72f, 0.92f, HermesLook(), "CHR_HERMES", "Hermes", kInterview);

		// ---- 축제 준비: 카스탈리아 샘가의 월계수 가지 셋 + 샘을 둘러싼 월계수 ----
		const Spot kLaurels[3] = { { -7.4f, 3.8f }, { -11.6f, 3.9f }, { -10.9f, -0.4f } };
		std::vector<Spot> avoid;
		for (const Spot& wanted : kLaurels)
		{
			Spot spot = FindOpenGround(tileMap, wanted.x, wanted.y, 0.3f);
			ItemActor* laurel = scene.Spawn<ItemActor>(spot.x, spot.y, 0.5f, 0.45f, 1.15f, 0.35f, ItemKind::Laurel, kInteractPickup);
			laurel->SetStoryId("ITEM_LAUREL");
			laurel->SetPresence("STORY_STEP == MQ_00.LAUREL");
			avoid.push_back(spot);
		}
		const Spot kLaurelTrees[3] = { { -8.2f, 0.2f }, { -11.8f, 1.6f }, { -10.0f, 4.6f } };
		for (int i = 0; i < 3; ++i)
		{
			scene.Spawn<TreeActor>(kLaurelTrees[i].x, kLaurelTrees[i].y, 1.4f + 0.1f * (float)i, 0.12f, 0.3f, 0.12f, TreeKind::Broadleaf);
			avoid.push_back(kLaurelTrees[i]);
		}

		// ---- 균열의 괴물(균열이 열린 뒤 그림자 퓌톤을 쓰러뜨릴 때까지) ----
		const char* kShadows = "CRACK_OPEN && !PYTHON_SLAIN";
		AnimalActor* python = SpawnAnimal(scene, tileMap, AnimalKind::ShadowPython, kAltar.x + 1.2f, kAltar.y + 2.6f, 0.3f);
		python->SetStoryId("MON_SHADOW_PYTHON");
		python->SetPresence(kShadows);
		SpawnAnimal(scene, tileMap, AnimalKind::ShadowWolf, kAltar.x + 3.0f, kAltar.y + 0.8f, 1.7f)->SetPresence(kShadows);
		SpawnAnimal(scene, tileMap, AnimalKind::ShadowWolf, kAltar.x - 0.6f, kAltar.y + 3.6f, 2.9f)->SetPresence(kShadows);

		// ---- 들짐승과 약초 ----
		SpawnAnimal(scene, tileMap, AnimalKind::Wolf, 11.5f, -9.5f, 4.2f);
		SpawnAnimal(scene, tileMap, AnimalKind::Wolf, -12.5f, 11.0f, 5.6f);
		SpawnAnimal(scene, tileMap, AnimalKind::Deer, 9.0f, -3.0f, 0.f);
		SpawnAnimal(scene, tileMap, AnimalKind::Deer, -9.0f, 9.5f, 2.1f);
		SpawnAnimal(scene, tileMap, AnimalKind::Deer, 13.0f, 2.5f, 1.3f);
		SpawnHerb(scene, tileMap, 9.5f, -6.5f);
		SpawnHerb(scene, tileMap, -12.5f, 7.0f);
		SpawnHerb(scene, tileMap, 7.5f, 10.0f);
		SpawnHerb(scene, tileMap, -5.5f, 11.5f);

		// 나무가 덮으면 안 되는 자리.
		const Spot kKeep[] =
		{
			kStart, kArrival, kSignpost, kCave, kCampfire, kSpring, { 7.2f, 0.8f }, { 8.9f, 3.6f }, { 2.0f, 7.8f },
			{ -1.2f, 9.0f }, { 10.4f, -1.6f }, { -2.0f, 6.0f }, { 1.0f, 6.8f }, { 6.0f, 1.8f }, { 5.4f, -4.6f }, { 2.8f, 1.2f },
		};
		avoid.insert(avoid.end(), std::begin(kKeep), std::end(kKeep));

		return FinishMap(scene, tileMap, rng, avoid, 30, 0.35f, 0.55f, false, start);
	}

	// =====================================================================================
	// LOC_OLYMPUS_FOOT — 올림포스 산기슭 · 테살리아 (중반 공용 허브: MQ_02 신들의 대회의)
	// 화면 위쪽에 눈 덮인 올림포스, 그 아래 언덕 위에 기둥이 둥글게 선 회의장과 크로노스 벽화, 화면 왼쪽에
	// 헤파이스토스의 대장간, 평원을 가로지르는 강(길이 지나는 곳은 여울), 강 건너 아래쪽 끝에 이정표.
	// =====================================================================================
	LevelActors BuildOlympusFoot(SceneGraph& scene, TileMap& tileMap, Renderer& renderer, std::mt19937& rng, int seed)
	{
		const Spot kArrival = { 10.4f, 9.0f };
		const Spot kSignpost = { 11.6f, 10.0f };
		const Spot kHall = { -4.5f, -4.0f };
		const Spot kMural = { 0.4f, -7.6f };
		const Spot kForge = { 7.6f, -6.6f };

		// ---- 지형 ----
		Terrain::Fill(tileMap, TileType::Grass);

		// 올림포스 산: 화면 위쪽을 덮는 높은 바위 산.
		Terrain::PaintWhere(tileMap, [seed](float x, float y)
		{
			return x + y < -11.f + Terrain::Noise(x * 0.22f, y * 0.22f, seed) * 3.f - 1.5f;
		}, TileType::Rock);

		// 길: 이정표 → 여울 → 회의장 언덕, 갈림길 → 대장간.
		Terrain::PaintLine(tileMap, kSignpost.x - 0.8f, kSignpost.y - 0.8f, 3.5f, 3.0f, 0.7f, TileType::Path);
		Terrain::PaintLine(tileMap, 3.5f, 3.0f, kHall.x + 3.2f, kHall.y + 3.2f, 0.7f, TileType::Path);
		Terrain::PaintLine(tileMap, 2.4f, 1.3f, kForge.x - 1.4f, kForge.y + 2.6f, 0.55f, TileType::Path);

		// 강: 평원을 화면 가로로 가로지르며 구불구불 흐른다.
		Terrain::PaintWhere(tileMap, [](float x, float y)
		{
			float across = x + y - 6.5f - 1.6f * sinf((x - y) * 0.28f);
			return fabsf(across) < 1.1f;
		}, TileType::Water);

		// 여울: 길이 강을 건너는 곳(물 위에 흙길을 다시 깐다).
		Terrain::PaintLine(tileMap, 2.0f, 2.0f, 6.2f, 5.8f, 0.75f, TileType::Path);

		// 회의장 언덕: 둥근 돌바닥 터.
		Terrain::PaintDisk(tileMap, kHall.x, kHall.y, 4.3f, TileType::Stone);

		Terrain::EnsureFullyConnected(tileMap, kArrival.x, kArrival.y);
		SpawnTileActors(scene, renderer, tileMap, seed);

		// ---- 신들의 회의장: 바닥 원판 + 둥글게 선 기둥들(입구 쪽은 비우고 둘은 부러짐) + 대리석 식탁 ----
		scene.Spawn<FloorDiscActor>(kHall.x, kHall.y, 3.7f, 0.74f, 0.71f, 0.65f);

		const int kColumns = 12;
		const float kEntranceAngle = kPi * 0.25f; // 카메라 쪽(45도) — 플레이어가 들어오는 곳
		for (int i = 0; i < kColumns; ++i)
		{
			float angle = (float)i * (2.f * kPi / (float)kColumns);
			if (AngleDistance(angle, kEntranceAngle) < 0.3f)
			{
				continue;
			}

			bool broken = (i == 4 || i == 9);
			scene.Spawn<ColumnActor>(kHall.x + cosf(angle) * 3.4f, kHall.y + sinf(angle) * 3.4f, 2.4f, broken);
		}

		scene.Spawn<TableActor>(kHall.x, kHall.y, 2.4f, 1.3f, 0.75f, 0.84f, 0.82f, 0.77f);
		// 회의장 트리거는 기둥 고리 안 전체를 덮는다(기둥 사이 틈으로 들어와도 회의가 시작되게).
		SpawnTrigger(scene, kHall.x, kHall.y, 3.0f, "TRIGGER_COUNCIL");

		// 대회의 참석자(MQ_02.COUNCIL 동안만): 식탁 상석(카메라 반대쪽)에 제우스, 둘레에 가족들.
		const char* kCouncil = "STORY_STEP == MQ_02.COUNCIL";
		SpawnSeated(scene, kHall.x, kHall.y, 225.f, 1.95f, 1.25f, 1.02f, 1.f, 0.94f, ZeusLook(), "CHR_ZEUS", "Zeus", kCouncil);
		SpawnSeated(scene, kHall.x, kHall.y, 190.f, 1.95f, 1.15f, 0.52f, 0.28f, 0.62f, HeraLook(), "CHR_HERA", "Hera", kCouncil);
		SpawnSeated(scene, kHall.x, kHall.y, 260.f, 1.95f, 1.2f, 0.2f, 0.5f, 0.55f, PoseidonLook(), "CHR_POSEIDON", "Poseidon", kCouncil);
		SpawnSeated(scene, kHall.x, kHall.y, 155.f, 1.95f, 1.1f, 0.55f, 0.6f, 0.7f, AthenaLook(), "CHR_ATHENA", "Athena", kCouncil);
		SpawnSeated(scene, kHall.x, kHall.y, 295.f, 1.95f, 1.3f, 0.6f, 0.15f, 0.12f, AresLook(), "CHR_ARES", "Ares", kCouncil);
		SpawnSeated(scene, kHall.x, kHall.y, 125.f, 1.95f, 1.05f, 0.95f, 0.6f, 0.7f, AphroditeLook(), "CHR_APHRODITE", "Aphrodite", kCouncil);
		SpawnSeated(scene, kHall.x, kHall.y, 330.f, 1.95f, 1.05f, 0.5f, 0.2f, 0.45f, DionysusLook(), "CHR_DIONYSUS", "Dionysus", kCouncil);
		SpawnSeated(scene, kHall.x, kHall.y, 95.f, 1.95f, 1.f, 0.55f, 0.72f, 0.92f, HermesLook(), "CHR_HERMES", "Hermes", kCouncil);

		// 크로노스 벽화(복선 F-04: 제우스가 그 앞에 오래 서 있던 흔적).
		MuralActor* mural = scene.Spawn<MuralActor>(kMural.x, kMural.y);
		mural->SetStoryId("OBJ_KRONOS_MURAL");
		mural->SetInteractId(kInteractStory);

		// ---- 헤파이스토스의 대장간 ----
		SpawnHouse(scene, kForge.x, kForge.y, 1.5f, false);
		scene.Spawn<FireActor>(kForge.x + 0.6f, kForge.y + 1.6f, 0.f, 0.6f, true);
		scene.Spawn<TableActor>(kForge.x - 0.6f, kForge.y + 1.7f, 0.55f, 0.35f, 0.5f, 0.3f, 0.29f, 0.29f);
		SpawnStoryNpc(scene, kForge.x - 1.1f, kForge.y + 2.3f, 1.15f, 0.42f, 0.29f, 0.18f, HephaestusLook(), "CHR_HEPHAESTUS", "Hephaestus", nullptr);

		// ---- 들판의 사람들 ----
		SpawnHouse(scene, -9.6f, 1.0f, 1.3f, true);
		SpawnHouse(scene, -11.4f, 3.8f, 1.2f, false);
		SpawnStoryNpc(scene, -8.8f, 6.6f, 0.95f, 0.45f, 0.32f, 0.2f, PanLook(), "CHR_PAN", "Pan", nullptr);
		SpawnStoryNpc(scene, -6.6f, 3.2f, 0.9f, 0.55f, 0.5f, 0.38f, CommonLook(2, false), "NPC_SHEPHERD", "Shepherd", nullptr);
		SpawnStoryNpc(scene, 3.0f, -1.2f, 0.9f, 0.48f, 0.55f, 0.36f, CommonLook(0, false), "NPC_SHEPHERD", "Shepherd", nullptr);
		SpawnSignpost(scene, kSignpost.x, kSignpost.y);

		// ---- 들짐승과 약초 ----
		SpawnAnimal(scene, tileMap, AnimalKind::Wolf, 13.0f, -6.5f, 1.1f);
		SpawnAnimal(scene, tileMap, AnimalKind::Wolf, -3.0f, 12.5f, 3.3f);
		SpawnAnimal(scene, tileMap, AnimalKind::Deer, 6.0f, 10.0f, 0.4f);
		SpawnAnimal(scene, tileMap, AnimalKind::Deer, -6.0f, 11.0f, 2.6f);
		SpawnAnimal(scene, tileMap, AnimalKind::Deer, -12.0f, 7.5f, 4.8f);
		SpawnHerb(scene, tileMap, 12.0f, 2.0f);
		SpawnHerb(scene, tileMap, -12.5f, 10.5f);
		SpawnHerb(scene, tileMap, 4.0f, 12.5f);
		SpawnHerb(scene, tileMap, -1.0f, -9.0f);

		std::vector<Spot> avoid =
		{
			kArrival, kSignpost, kMural, kForge, { kForge.x - 1.1f, kForge.y + 2.3f }, { -9.6f, 1.0f }, { -11.4f, 3.8f },
			{ -8.8f, 6.6f }, { -6.6f, 3.2f }, { 3.0f, -1.2f },
		};

		return FinishMap(scene, tileMap, rng, avoid, 26, 0.45f, 0.6f, true, kArrival);
	}

	// =====================================================================================
	// LOC_ATHENS — 아테네 · 폴리스 연합 (MQ_02: 무너진 신상, 신앙을 잃은 시민들, 잊혀지는 신)
	// 화면 위쪽에 바위 언덕 아크로폴리스와 꼭대기의 신전, 가운데 아고라 광장(목이 부러진 채 누운 제우스 신상),
	// 광장 오른쪽에 새로 세운 로마식 유피테르 신전과 사제단 창고, 화면 왼쪽에 집들 사이로 난 뒷골목(막다른 곳에
	// 식은 화덕), 아래쪽 모서리에 항구로 이어지는 바다, 그 곁에 이정표.
	// =====================================================================================
	LevelActors BuildAthens(SceneGraph& scene, TileMap& tileMap, Renderer& renderer, std::mt19937& rng, int seed)
	{
		const Spot kArrival = { 11.0f, 3.4f };
		const Spot kSignpost = { 12.2f, 4.2f };
		const Spot kAcropolis = { -8.2f, -8.2f };
		const Spot kAgora = { 1.5f, 1.0f };
		const Spot kStatue = { 0.8f, -0.2f };
		const Spot kJupiter = { -3.4f, 3.0f };
		const Spot kStorehouse = { -7.4f, 7.0f };

		// ---- 지형 ----
		Terrain::Fill(tileMap, TileType::Grass);

		// 남쪽 바다(화면 아래쪽 모서리): 항구로 이어지는 만.
		Terrain::PaintWhere(tileMap, [seed](float x, float y)
		{
			return x + y > 18.5f + Terrain::Noise(x * 0.2f, y * 0.2f, seed + 5) * 3.f - 1.5f;
		}, TileType::Water);

		// 아크로폴리스: 바위 언덕 + 꼭대기의 평평한 돌바닥 + 아고라 쪽으로 내려오는 비탈길.
		Terrain::PaintDisk(tileMap, kAcropolis.x, kAcropolis.y, 5.4f, TileType::Rock);
		Terrain::PaintDisk(tileMap, kAcropolis.x, kAcropolis.y, 3.4f, TileType::Stone);
		Terrain::PaintLine(tileMap, kAcropolis.x + 2.2f, kAcropolis.y + 2.2f, kAcropolis.x + 5.6f, kAcropolis.y + 5.6f, 0.75f, TileType::Path);

		// 거리: 아고라 → 이정표, 아고라 → 유피테르 신전 앞 → 창고 문 앞, 뒷골목(집들 사이의 좁은 길).
		// 창고로 가는 길은 신전(정면 계단이 +y)을 막지 않도록 신전 앞(카메라 쪽)으로 돌아간다.
		Terrain::PaintLine(tileMap, kAgora.x + 3.0f, kAgora.y + 2.0f, kSignpost.x - 0.8f, kSignpost.y - 0.4f, 0.7f, TileType::Path);
		Terrain::PaintLine(tileMap, 0.2f, 5.2f, -2.0f, 7.6f, 0.6f, TileType::Path);
		Terrain::PaintLine(tileMap, -2.0f, 7.6f, kStorehouse.x + 0.4f, kStorehouse.y + 2.0f, 0.6f, TileType::Path);
		Terrain::PaintLine(tileMap, 4.6f, -1.6f, 8.6f, -7.6f, 0.55f, TileType::Path);

		// 아고라(광장) + 유피테르 신전 경내.
		Terrain::PaintDisk(tileMap, kAgora.x, kAgora.y, 4.6f, TileType::Stone);
		Terrain::PaintDisk(tileMap, kJupiter.x, kJupiter.y, 3.0f, TileType::Stone);

		Terrain::EnsureFullyConnected(tileMap, kArrival.x, kArrival.y);
		SpawnTileActors(scene, renderer, tileMap, seed);

		// ---- 신전과 신상 ----
		scene.Spawn<TempleActor>(kAcropolis.x - 0.4f, kAcropolis.y - 0.4f, 5.f, 3.2f, 1.6f, false);
		scene.Spawn<TempleActor>(kJupiter.x, kJupiter.y, 3.6f, 3.f, 1.4f, true);

		StatueActor* statue = scene.Spawn<StatueActor>(kStatue.x, kStatue.y, 1.3f, true);
		statue->SetStoryId("OBJ_FALLEN_STATUE");
		statue->SetInteractId(kInteractStory);

		// ---- 사제단 창고와 장부 ----
		SpawnHouse(scene, kStorehouse.x, kStorehouse.y, 1.6f, true);
		Spot ledgerSpot = FindOpenGround(tileMap, kStorehouse.x + 0.4f, kStorehouse.y + 1.5f, 0.3f);
		ItemActor* ledger = scene.Spawn<ItemActor>(ledgerSpot.x, ledgerSpot.y, 0.55f, 0.95f, 0.86f, 0.66f, ItemKind::Scroll, kInteractStory);
		ledger->SetStoryId("OBJ_PRIEST_LEDGER");
		ledger->SetPresence("STORY_STEP == MQ_02.PRIESTS"); // 조사를 마치면 챙겨 가므로 사라진다

		// ---- 뒷골목: 길 양옆의 집들 + 막다른 곳의 식은 화덕과 옛 화로의 신 ----
		SpawnHouse(scene, 7.1f, -2.6f, 1.3f, false);
		SpawnHouse(scene, 4.5f, -4.3f, 1.3f, false);
		SpawnHouse(scene, 8.55f, -4.75f, 1.3f, false);
		SpawnHouse(scene, 6.0f, -6.5f, 1.3f, false);
		scene.Spawn<HearthActor>(9.1f, -8.35f);
		GlowActor* ember = scene.Spawn<GlowActor>(9.1f, -8.3f, 0.12f, 0.3f, 1.6f, 0.6f, 0.2f);
		ember->SetPresence("HEARTH_GOD_NAMED");

		NpcActor* hearthGod = SpawnStoryNpc(scene, 8.4f, -7.2f, 0.95f, 0.62f, 0.45f, 0.32f, HearthGodLook(), "CHR_HEARTH_GOD", "???", nullptr);
		hearthGod->SetFaded(0.3f);

		// ---- 시내의 집들 ----
		SpawnHouse(scene, 7.2f, 5.6f, 1.4f, true);
		SpawnHouse(scene, 9.0f, 1.8f, 1.3f, false);
		SpawnHouse(scene, 5.4f, 7.6f, 1.3f, false);
		SpawnHouse(scene, 1.4f, 8.8f, 1.4f, true);
		SpawnHouse(scene, -1.6f, 10.0f, 1.3f, false);
		SpawnSignpost(scene, kSignpost.x, kSignpost.y);

		// ---- 사람들 ----
		SpawnStoryNpc(scene, 2.6f, 1.6f, 0.9f, 0.72f, 0.62f, 0.45f, CommonLook(1, true), "NPC_ATHENS_SKEPTIC", "Citizen", nullptr);
		SpawnStoryNpc(scene, 4.2f, 3.4f, 0.9f, 0.5f, 0.42f, 0.62f, CommonLook(0, false), "NPC_ATHENS_CITIZEN", "Citizen", nullptr);
		SpawnStoryNpc(scene, 0.4f, 3.8f, 0.9f, 0.66f, 0.3f, 0.26f, CommonLook(3, false), "NPC_ATHENS_CITIZEN", "Citizen", nullptr);
		SpawnStoryNpc(scene, 3.6f, -2.4f, 0.9f, 0.4f, 0.55f, 0.42f, CommonLook(4, true), "NPC_ATHENS_CITIZEN", "Citizen", nullptr);
		SpawnStoryNpc(scene, -1.8f, -1.6f, 0.95f, 0.85f, 0.83f, 0.78f, HearthGodLook(), "NPC_PHILOSOPHER", "Philosopher", nullptr);
		SpawnStoryNpc(scene, -2.4f, 6.4f, 0.9f, 0.92f, 0.9f, 0.86f, CommonLook(2, true), "NPC_PRIEST", "Priest", nullptr);

		// ---- 약초 ----
		SpawnHerb(scene, tileMap, -12.0f, -1.0f);
		SpawnHerb(scene, tileMap, -2.0f, -12.0f);

		std::vector<Spot> avoid =
		{
			kArrival, kSignpost, kJupiter, kStorehouse, ledgerSpot, { 7.1f, -2.6f }, { 4.5f, -4.3f }, { 8.55f, -4.75f },
			{ 6.0f, -6.5f }, { 9.1f, -8.35f }, { 7.2f, 5.6f }, { 9.0f, 1.8f }, { 5.4f, 7.6f }, { 1.4f, 8.8f }, { -1.6f, 10.0f }, { -2.4f, 6.4f },
		};

		return FinishMap(scene, tileMap, rng, avoid, 14, 0.25f, 0.6f, false, kArrival);
	}
}

LoadedMap WorldMaps::Build(const std::string& locId, Renderer& renderer, unsigned int sessionSeed, bool arrivedByTravel)
{
	LoadedMap result;
	result.locId = IsKnownLocation(locId) ? locId : std::string("LOC_DELPHI");
	result.tileMap = new TileMap(kMapSize, kMapSize);
	result.scene = new SceneGraph();

	// 실행마다 다른 세계, 같은 실행 안에선 같은 지역은 늘 같은 모습(다시 와도 같은 마을·숲).
	unsigned int mapSeed = sessionSeed ^ HashString(result.locId);
	std::mt19937 rng(mapSeed);
	int seed = (int)(mapSeed & 0x7FFFFFFFu);

	LevelActors actors;
	if (result.locId == "LOC_OLYMPUS_FOOT")
	{
		actors = BuildOlympusFoot(*result.scene, *result.tileMap, renderer, rng, seed);
	}
	else if (result.locId == "LOC_ATHENS")
	{
		actors = BuildAthens(*result.scene, *result.tileMap, renderer, rng, seed);
	}
	else
	{
		actors = BuildDelphi(*result.scene, *result.tileMap, renderer, rng, seed, arrivedByTravel);
	}

	result.player = actors.player;
	result.attackRing = actors.attackRing;
	result.interactRing = actors.interactRing;
	return result;
}

bool WorldMaps::IsKnownLocation(const std::string& locId)
{
	return locId == "LOC_DELPHI" || locId == "LOC_OLYMPUS_FOOT" || locId == "LOC_ATHENS";
}
