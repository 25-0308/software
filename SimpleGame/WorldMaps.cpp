#include "stdafx.h"
#include "WorldMaps.h"

#include <algorithm>
#include <cmath>
#include <cstring>
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
#include "WorldLayout.h"

namespace
{
	const float kPi = 3.14159265f;

	// 지금 짓고 있는 지역과, 그 지역에 지은 살림집(SpawnHouse가 기록 — 주민의 집으로 나눠 준다).
	// 지역 하나를 다 지으면 비운다.
	const WorldPlace* g_BuildPlace = nullptr;
	std::vector<BuildingActor*> g_BuildHouses;

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

	HumanLook ApolloLook()
	{
		// 금빛 머리 + 월계관(다프네의 월계수) + 긴 옷 + 왼손의 술잔(20년째 실직해 술집에 처박혀 있다).
		HumanLook look;
		look.hairR = 0.95f; look.hairG = 0.8f; look.hairB = 0.38f;
		look.robe = true;
		look.wreath = true; look.wreathR = 0.34f; look.wreathG = 0.56f; look.wreathB = 0.22f;
		look.wineCup = true;
		return look;
	}

	HumanLook PersephoneLook()
	{
		// 검은 긴 머리 + 석류빛 화관 + 긴 옷.
		HumanLook look;
		look.hairR = 0.1f; look.hairG = 0.06f; look.hairB = 0.08f;
		look.longHair = true;
		look.robe = true;
		look.wreath = true; look.wreathR = 0.78f; look.wreathG = 0.16f; look.wreathB = 0.2f;
		return look;
	}

	HumanLook PrometheusLook()
	{
		// 헝클어진 잿빛 머리·수염, 옷 없이 사슬에 묶인 티탄.
		HumanLook look;
		look.hairR = 0.42f; look.hairG = 0.38f; look.hairB = 0.34f;
		look.beardR = 0.46f; look.beardG = 0.42f; look.beardB = 0.38f;
		look.beard = true;
		return look;
	}

	HumanLook MoiraLook(int index)
	{
		// 운명의 세 자매: 흰 두건과 긴 옷. 클로토(젊음)·라케시스(중년)·아트로포스(늙음)로 머리 색이 다르다.
		const float kHair[3][3] = { { 0.16f, 0.1f, 0.06f }, { 0.4f, 0.3f, 0.2f }, { 0.78f, 0.76f, 0.72f } };
		HumanLook look;
		look.hairR = kHair[index % 3][0]; look.hairG = kHair[index % 3][1]; look.hairB = kHair[index % 3][2];
		look.robe = true;
		look.hood = true; look.hoodR = 0.86f; look.hoodG = 0.84f; look.hoodB = 0.8f;
		return look;
	}

	HumanLook ErisLook()
	{
		// 정체를 드러낸 불화의 여신: 검붉은 긴 머리 + 검은 옷 + 왼손의 금빛 사과(두건은 벗었다).
		HumanLook look;
		look.hairR = 0.36f; look.hairG = 0.06f; look.hairB = 0.08f;
		look.longHair = true;
		look.robe = true;
		look.apple = true;
		look.wreath = true; look.wreathR = 0.9f; look.wreathG = 0.72f; look.wreathB = 0.28f;
		return look;
	}

	HumanLook TantalusLook()
	{
		HumanLook look;
		look.hairR = 0.5f; look.hairG = 0.46f; look.hairB = 0.42f;
		look.beardR = 0.52f; look.beardG = 0.48f; look.beardB = 0.44f;
		look.beard = true;
		return look;
	}

	HumanLook TerminusLook()
	{
		// 경계석의 하급 신: 돌빛 옷 + 잿빛 수염. 그리스 이름(호로스)과 로마 이름(테르미누스) 사이에서 흔들린다.
		HumanLook look;
		look.hairR = 0.55f; look.hairG = 0.54f; look.hairB = 0.5f;
		look.beardR = 0.58f; look.beardG = 0.57f; look.beardB = 0.53f;
		look.beard = true;
		look.robe = true;
		look.staff = true;
		return look;
	}

	HumanLook RomanPriestLook()
	{
		// 로마식 사제: 머리까지 덮어쓴 흰 토가(두건처럼).
		HumanLook look;
		look.robe = true;
		look.hood = true; look.hoodR = 0.92f; look.hoodG = 0.9f; look.hoodB = 0.86f;
		return look;
	}

	HumanLook UndyingLook(int index)
	{
		// "돌아온 자들": 실이 끊기지 않아 죽지 못하는 망자. 붕대 같은 잿빛 두건과 긴 옷으로 몸을 감쌌다(부패는 천으로 가림 — V-03).
		HumanLook look;
		look.hairR = 0.6f; look.hairG = 0.58f; look.hairB = 0.55f;
		look.robe = true;
		look.hood = true;
		look.hoodR = 0.62f - 0.04f * (float)(index % 3); look.hoodG = 0.58f - 0.04f * (float)(index % 3); look.hoodB = 0.5f;
		look.staff = (index % 2 == 0);
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

	// 집 한 채(+ 카메라 쪽 문 옆 횃불). residential이면 사람이 사는 살림집이라 마을 주민의 집으로 나눠 준다
	// (대장간·창고·정자처럼 일하는 건물은 false).
	BuildingActor* SpawnHouse(SceneGraph& scene, float x, float y, float size, bool torch, bool residential = true)
	{
		BuildingActor* building = scene.Spawn<BuildingActor>(x, y, size);
		if (torch)
		{
			building->AddChild(std::unique_ptr<Actor>(new FireActor(size * 0.3f, size * 0.61f, 0.8f, 0.5f)));
		}

		if (residential)
		{
			g_BuildHouses.push_back(building);
		}
		return building;
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

	// 조사할 수 있는 이야기 대상(비석·벽화·상자 등)으로 만든다.
	void MakeInspectable(Actor* actor, const char* storyId, const char* presence)
	{
		actor->SetStoryId(storyId);
		actor->SetInteractId(kInteractStory);
		actor->SetPresence(presence);
	}

	// 수호신 다섯을 한 자리에 하나씩 둔다(실제로는 PATRON에 맞는 하나만 보인다). presence는 공통 조건(수호신 조건은 덧붙임).
	// 존재 조건 문자열은 수명이 끝까지 가야 해서 정적 문자열 표에서 고른다.
	struct PatronPresence
	{
		const char* zeus;
		const char* poseidon;
		const char* hades;
		const char* aphrodite;
		const char* hermes;
	};

	void SpawnPatrons(SceneGraph& scene, float x, float y, const PatronPresence& presence, const Actor* follow)
	{
		NpcActor* gods[5] =
		{
			SpawnStoryNpc(scene, x, y, 1.25f, 1.02f, 1.f, 0.94f, ZeusLook(), "CHR_ZEUS", "Zeus", presence.zeus),
			SpawnStoryNpc(scene, x, y, 1.2f, 0.2f, 0.5f, 0.55f, PoseidonLook(), "CHR_POSEIDON", "Poseidon", presence.poseidon),
			SpawnStoryNpc(scene, x, y, 1.15f, 0.16f, 0.12f, 0.2f, HadesLook(), "CHR_HADES", "Hades", presence.hades),
			SpawnStoryNpc(scene, x, y, 1.05f, 0.95f, 0.6f, 0.7f, AphroditeLook(), "CHR_APHRODITE", "Aphrodite", presence.aphrodite),
			SpawnStoryNpc(scene, x, y, 1.f, 0.55f, 0.72f, 0.92f, HermesLook(), "CHR_HERMES", "Hermes", presence.hermes),
		};

		if (follow != nullptr)
		{
			for (NpcActor* god : gods)
			{
				god->SetFollowTarget(follow);
			}
		}
	}

	// 무너진 집터의 잔해(내전의 그림자 — V-06): 돌무더기와 아직 꺼지지 않은 불. 존재 조건이 참일 때만.
	void SpawnRuins(SceneGraph& scene, float x, float y, int seed, const char* presence)
	{
		RockActor* rubble = scene.Spawn<RockActor>(x, y, 0.8f, seed % 3, false);
		rubble->SetPresence(presence);
		FireActor* fire = scene.Spawn<FireActor>(x + 0.7f, y + 0.6f, 0.f, 0.45f, true);
		fire->SetPresence(presence);
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

	// 이야기 무대(크로노스의 배 속)를 다 지은 뒤 마무리: 바위·나무·플레이어·동료·분위기. 무대는 월드 밖의 따로 된 맵이다.
	LevelActors FinishStage(SceneGraph& scene, const TileMap& tileMap, std::mt19937& rng, const std::vector<Spot>& avoid,
		int treeCount, float cypressRatio, float rockDensity, bool snowyRocks, const Spot& start)
	{
		ScatterRocks(scene, tileMap, rng, rockDensity, snowyRocks);
		ScatterTrees(scene, tileMap, rng, treeCount, avoid, 2.2f, cypressRatio);

		LevelActors actors = SpawnPlayerAndMarkers(scene, start.x, start.y);
		SpawnCompanions(scene, *actors.player);
		SpawnAmbience(scene, tileMap);
		return actors;
	}

	// ---- 마을 주민과 집 ----

	// 마을 주민으로 치는(집이 있는) 스토리 ID. 순례객·조난자·줄 선 영혼·가면 쓴 손님·살아남은 양치기처럼 잠깐 머무는
	// 사람과 신·이야기 인물(CHR_)은 주민이 아니다(집이 없다).
	bool IsResidentId(const char* storyId)
	{
		static const char* const kResidentIds[] =
		{
			"NPC_MERCHANT", "NPC_DELPHI_VILLAGER", "NPC_SHEPHERD", "NPC_ATHENS_SKEPTIC", "NPC_ATHENS_CITIZEN", "NPC_PRIEST",
			"NPC_PHILOSOPHER", "NPC_FISHER", "NPC_PAPHOS_SERVANT", "NPC_RECORD_KEEPER", "NPC_WEST_PRIEST", "NPC_WEST_CITIZEN",
			"NPC_UNDYING_ELDER", "NPC_UNDYING", "NPC_THEBES_CITIZEN",
		};

		if (storyId == nullptr)
		{
			return false;
		}

		for (const char* id : kResidentIds)
		{
			if (strcmp(id, storyId) == 0)
			{
				return true;
			}
		}
		return false;
	}

	float SegmentDistance(float x, float y, float ax, float ay, float bx, float by)
	{
		float abX = bx - ax;
		float abY = by - ay;
		float lengthSq = abX * abX + abY * abY;
		float t = (lengthSq > 0.f) ? ((x - ax) * abX + (y - ay) * abY) / lengthSq : 0.f;
		t = (std::max)(0.f, (std::min)(1.f, t));

		float dx = x - (ax + abX * t);
		float dy = y - (ay + abY * t);
		return sqrtf(dx * dx + dy * dy);
	}

	// (x, y)(지역 로컬)가 지역 밖으로 나가는 길(출구의 안쪽 점 → 가장자리) 위이거나 그 곁인지.
	bool IsNearExitRoad(const WorldPlace& place, float x, float y, float margin)
	{
		for (int i = 0; i < place.exitCount; ++i)
		{
			const WorldExit& exit = place.exits[i];
			if (SegmentDistance(x, y, exit.innerX, exit.innerY, exit.borderX, exit.borderY) < margin)
			{
				return true;
			}
		}
		return false;
	}

	// 새 집을 앉힐 수 있는 자리(지역 로컬)인지: 집 둘레까지 걸을 수 있는 땅이고, 흙길 위가 아니고, 밖으로 나가는 길·
	// 다른 집·이야기 장소·이미 놓인 것들과 넉넉히 떨어져 있어야 한다.
	bool CanPlaceHouse(SceneGraph& scene, const TileMap& tileMap, const WorldPlace& place, float x, float y,
		const std::vector<Spot>& houseSpots, const std::vector<Spot>& avoid)
	{
		if (!IsOpenGround(tileMap, x, y, 1.6f))
		{
			return false;
		}

		// 흙길 위는 피한다 — 다만 땅 전체가 흙(타이나론 곶의 잿빛 흙)이라 둘레 5x5칸이 모두 흙이면 길이 아니라 맨땅이다.
		int gridX = tileMap.GetGridX(x);
		int gridY = tileMap.GetGridY(y);
		if (tileMap.GetTile(gridX, gridY) == TileType::Path)
		{
			int pathTiles = 0;
			for (int dy = -2; dy <= 2; ++dy)
			{
				for (int dx = -2; dx <= 2; ++dx)
				{
					pathTiles += (tileMap.GetTile(gridX + dx, gridY + dy) == TileType::Path) ? 1 : 0;
				}
			}
			if (pathTiles < 25)
			{
				return false;
			}
		}

		if (IsNearExitRoad(place, x, y, 3.f) || IsNearAny(houseSpots, x, y, 3.6f) || IsNearAny(avoid, x, y, 3.f))
		{
			return false;
		}

		Actor* nearby = scene.FindNearest(x + scene.GetSpawnOffsetX(), y + scene.GetSpawnOffsetY(), 2.8f, [](const Actor& actor)
		{
			ActorType type = actor.GetType();
			return type != ActorType::Group && type != ActorType::TileBatch && type != ActorType::Effect;
		});
		return nearby == nullptr;
	}

	// 마을 주민의 겉모습: 대개 보통 사람(머리 색·긴 옷을 번호로 돌려 가며), 타이나론은 천을 감은 "돌아온 자들".
	HumanLook ResidentLook(const WorldPlace& place, int index, bool robe)
	{
		if (strcmp(place.locId, "LOC_TAENARON") == 0)
		{
			return UndyingLook(index);
		}

		HumanLook look = CommonLook(index, robe);
		if (strcmp(place.locId, "LOC_THEBES") == 0 && index % 3 == 0)
		{
			look.wreath = true; look.wreathR = 0.28f; look.wreathG = 0.52f; look.wreathB = 0.22f; // 축제의 담쟁이관
		}
		else if (strcmp(place.locId, "LOC_CYPRUS") == 0 && index % 3 == 1)
		{
			look.wreath = true; look.wreathR = 1.f; look.wreathG = 0.55f; look.wreathB = 0.66f; // 장미 화관
		}
		return look;
	}

	// 마을을 주민 15명(WorldLayout::kResidentsPerVillage)으로 채운다. 지금 있는 살림집(SpawnHouse로 지은 것)이 8채가
	// 안 되면 마을 둘레의 빈 땅에 집을 더 짓고, 집마다 식구 수(1~3명, 합쳐서 15명)를 정한 뒤, 이미 있는 주민(이야기에
	// 나오는 상인·어부·시민 등)을 가까운 집에 들이고, 남은 자리를 새 주민으로 채운다. 새 주민은 자기 집 문 앞에서
	// 시작해 그 둘레를 거닐고, 대부분 마을이 숭배하는 신을, 일부는 그 마을의 다른 신을 믿으며, 성미(다혈질 비율·
	// 거니는 반경·어울리는 정도)는 마을 분위기를 따른다. presence는 새 주민의 존재 조건(델포이의 축제 군중처럼).
	void FillVillage(SceneGraph& scene, const TileMap& tileMap, std::mt19937& rng, std::vector<Spot>& avoid, const char* presence)
	{
		const WorldPlace& place = *g_BuildPlace;
		const VillageMood& mood = place.mood;
		float offsetX = scene.GetSpawnOffsetX();
		float offsetY = scene.GetSpawnOffsetY();
		std::uniform_real_distribution<float> unit(0.f, 1.f);

		// 1) 이미 있는 주민.
		std::vector<NpcActor*> residents;
		scene.ForEach([&residents, &place](Actor& actor)
		{
			if (actor.GetType() == ActorType::NPC && actor.GetHomeRegion() != nullptr && strcmp(actor.GetHomeRegion(), place.locId) == 0
				&& IsResidentId(actor.GetStoryId()))
			{
				residents.push_back(static_cast<NpcActor*>(&actor));
			}
		});

		// 2) 집이 모자라면 마을 둘레(중심에서 11~19칸)에 더 짓는다.
		const int kTargetHouses = 8;
		std::vector<Spot> houseSpots;
		for (BuildingActor* house : g_BuildHouses)
		{
			houseSpots.push_back(Spot{ house->GetX() - offsetX, house->GetY() - offsetY });
		}

		// 마을 둘레(11~19칸)를 먼저 찾고, 바다·바위가 많아 자리가 모자라면(섬 마을 등) 안쪽부터 가장자리 가까이까지 넓혀 찾는다.
		for (int attempt = 0; attempt < 1600 && (int)g_BuildHouses.size() < kTargetHouses; ++attempt)
		{
			float angle = unit(rng) * 2.f * kPi;
			float radius = (attempt < 800) ? 11.f + unit(rng) * 8.f : 3.f + unit(rng) * 16.5f;
			float x = floorf(cosf(angle) * radius * 2.f) * 0.5f;
			float y = floorf(sinf(angle) * radius * 2.f) * 0.5f;

			if (!CanPlaceHouse(scene, tileMap, place, x, y, houseSpots, avoid))
			{
				continue;
			}

			SpawnHouse(scene, x, y, 1.2f + unit(rng) * 0.25f, g_BuildHouses.size() % 2 == 0);
			houseSpots.push_back(Spot{ x, y });
			avoid.push_back(Spot{ x, y });
		}

		// 3) 집마다 식구 수: 모두 한 명씩에서 시작해, 15명이 될 때까지 아무 집에나 한 명씩 더한다(한 집에 최대 3명).
		int houseCount = (int)g_BuildHouses.size();
		if (houseCount == 0)
		{
			return;
		}

		std::vector<int> capacity(houseCount, 0);
		int total = 0;
		for (int h = 0; h < houseCount && total < WorldLayout::kResidentsPerVillage; ++h)
		{
			capacity[h] = 1;
			++total;
		}
		while (total < WorldLayout::kResidentsPerVillage && total < houseCount * WorldLayout::kMaxResidentsPerHouse)
		{
			int h = (int)(unit(rng) * (float)houseCount) % houseCount;
			if (capacity[h] < WorldLayout::kMaxResidentsPerHouse)
			{
				++capacity[h];
				++total;
			}
		}

		// 집 문 앞(살림집은 +y면에 문이 있다 — 화면 오른쪽의 밝은 면).
		std::vector<Spot> doors;
		for (BuildingActor* house : g_BuildHouses)
		{
			float localX = house->GetX() - offsetX;
			float localY = house->GetY() - offsetY + house->GetSize() * 0.5f + 0.6f;
			doors.push_back(FindOpenGround(tileMap, localX, localY, 0.3f));
		}

		std::vector<int> occupied(houseCount, 0);

		// 4) 이미 있는 주민은 빈자리가 있는 가장 가까운 집에 든다(자기 자리 — 좌판·광장 — 는 그대로 지킨다).
		for (NpcActor* npc : residents)
		{
			int best = -1;
			float bestSq = 0.f;
			for (int h = 0; h < houseCount; ++h)
			{
				if (occupied[h] >= capacity[h])
				{
					continue;
				}

				float dx = g_BuildHouses[h]->GetX() - npc->GetX();
				float dy = g_BuildHouses[h]->GetY() - npc->GetY();
				if (best < 0 || dx * dx + dy * dy < bestSq)
				{
					best = h;
					bestSq = dx * dx + dy * dy;
				}
			}

			NpcTraits traits = npc->GetTraits();
			traits.sociability = mood.sociability;
			npc->SetTraits(traits);

			if (best >= 0)
			{
				npc->SetHome(best + 1, doors[best].x + offsetX, doors[best].y + offsetY);
				++occupied[best];
			}
		}

		// 5) 남은 자리를 새 주민으로 채운다.
		int index = (int)residents.size();
		for (int h = 0; h < houseCount; ++h)
		{
			for (; occupied[h] < capacity[h]; ++occupied[h], ++index)
			{
				float x = doors[h].x + (unit(rng) - 0.5f) * 0.8f;
				float y = doors[h].y + (unit(rng) - 0.5f) * 0.8f;
				if (!tileMap.IsWorldPositionWalkable(x, y))
				{
					x = doors[h].x;
					y = doors[h].y;
				}

				float r = 0.3f + unit(rng) * 0.55f;
				float g = 0.3f + unit(rng) * 0.5f;
				float b = 0.28f + unit(rng) * 0.5f;
				HumanLook look = ResidentLook(place, index, unit(rng) < 0.4f);
				NpcActor* npc = SpawnStoryNpc(scene, x, y, 0.88f + unit(rng) * 0.06f, r, g, b, look, place.residentId, place.residentTag, presence);

				NpcTraits traits;
				traits.faith = place.worship;
				if (place.minorityCount > 0 && unit(rng) < 0.3f)
				{
					traits.faith = place.minorities[(int)(unit(rng) * (float)place.minorityCount) % place.minorityCount];
				}
				traits.wanderRadius = mood.wanderRadius;
				traits.hotheaded = unit(rng) < mood.hotheadedRatio;
				traits.social = true;
				traits.sociability = mood.sociability;
				npc->SetTraits(traits);
				npc->SetHome(h + 1, doors[h].x + offsetX, doors[h].y + offsetY);
				npc->SetFacing(unit(rng) * 2.f * kPi);

				avoid.push_back(Spot{ x, y });
			}

			g_BuildHouses[h]->SetHouse(h + 1, occupied[h]);
			avoid.push_back(doors[h]);
		}
	}

	// 지역 하나(월드에 앉힐 마을·성역)를 다 지은 뒤 마무리: 마을이면 집과 주민을 15명까지 채우고, 밖으로 나가는 길을
	// 비워 둔 채 바위와 나무를 놓는다. 플레이어·동료·바닥·분위기는 월드를 다 지은 뒤 한 번에 한다(BuildOverworld).
	// 나무 수는 32칸 맵 기준이라 지역 넓이에 맞춰 늘린다. residentPresence는 새 주민의 존재 조건.
	void FinishRegion(SceneGraph& scene, const TileMap& tileMap, std::mt19937& rng, std::vector<Spot>& avoid,
		int treeCount, float cypressRatio, float rockDensity, bool snowyRocks, const char* residentPresence = nullptr)
	{
		const WorldPlace& place = *g_BuildPlace;

		// 밖으로 나가는 길 위엔 아무것도 놓지 않는다(나중에 월드가 그 위로 흙길을 깐다).
		for (int i = 0; i < place.exitCount; ++i)
		{
			const WorldExit& exit = place.exits[i];
			float dx = exit.borderX - exit.innerX;
			float dy = exit.borderY - exit.innerY;
			float length = sqrtf(dx * dx + dy * dy);
			for (float t = 0.f; t <= length; t += 1.2f)
			{
				avoid.push_back(Spot{ exit.innerX + dx / length * t, exit.innerY + dy / length * t });
			}
		}

		if (place.kind == PlaceKind::Village)
		{
			FillVillage(scene, tileMap, rng, avoid, residentPresence);
		}

		float area = (float)(place.size * place.size) / 1024.f;
		ScatterRocks(scene, tileMap, rng, rockDensity, snowyRocks);
		ScatterTrees(scene, tileMap, rng, (int)((float)treeCount * area), avoid, 2.2f, cypressRatio);
	}

	// =====================================================================================
	// LOC_DELPHI — 델포이 · 파르나소스 산기슭 (시작 지역: MQ_00 프롤로그, MQ_01 수호신의 선택)
	// 화면 위쪽(월드 x+y가 작은 쪽)에 파르나소스 산, 그 아래 산을 깎은 테라스에 아폴론 신전과 신탁 제단,
	// 제단 앞 광장에서 내려가는 성스러운 길을 따라 시장과 마을, 화면 오른쪽에 카스탈리아 샘과 월계수 숲,
	// 화면 왼쪽에 동굴이 난 바위 언덕과 노파의 모닥불, 맨 아래 길 끝에 이정표.
	// =====================================================================================
	void BuildDelphi(SceneGraph& scene, TileMap& tileMap, std::mt19937& rng, int seed)
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

		Terrain::EnsureFullyConnected(tileMap, kStart.x, kStart.y);

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

		// 떠돌이 노파(에리스의 위장): 피티아가 깨어난 뒤 모닥불 곁에 나타나 내전이 터질 때까지 머문다. MQ_03에서 정체를
		// 의심받으면(ERIS_SUSPECTED) 웃기만 하고 사라진다.
		SpawnStoryNpc(scene, kCampfire.x + 0.8f, kCampfire.y + 0.9f, 0.85f, 0.3f, 0.24f, 0.22f, OldWomanLook(), "CHR_ERIS", "Old Woman",
			"STORY_STEP >= MQ_00.OLDWOMAN && STORY_STEP < MQ_04.DECLARE && !ERIS_SUSPECTED");

		// 헤르메스: 프롤로그 끝에 모닥불 근처로 "착지"했다가, MQ_01에선 신전 광장에서 면접 일정을 진행한다.
		SpawnStoryNpc(scene, 5.4f, -4.6f, 1.f, 0.55f, 0.72f, 0.92f, HermesLook(), "CHR_HERMES", "Hermes", "STORY_STEP == MQ_00.HERMES");

		// RQ_HERMES: 마을 동쪽 끝 집 곁의 헤르메스 은신처(편지 배달을 맡기고, 이중 장부를 들킨다).
		SpawnStoryNpc(scene, 9.0f, -0.4f, 1.f, 0.55f, 0.72f, 0.92f, HermesLook(), "CHR_HERMES", "Hermes",
			"PATRON == HERMES && STORY_STEP >= RQ_HERMES_01.LETTERS && STORY_STEP <= RQ_HERMES_02.CONFRONT");
		Spot ledgerSpot = FindOpenGround(tileMap, 11.8f, -2.6f, 0.3f);
		ItemActor* doubleLedger = scene.Spawn<ItemActor>(ledgerSpot.x, ledgerSpot.y, 0.55f, 0.95f, 0.86f, 0.66f, ItemKind::Scroll, kInteractStory);
		doubleLedger->SetStoryId("OBJ_DOUBLE_LEDGER");
		doubleLedger->SetPresence("STORY_STEP == RQ_HERMES_02.LEDGER");

		// MQ_03 귀향: 마을 술집(시장 아래 집) 앞의 취한 아폴론, 동굴 앞에 꺼내 놓은 봉인 석판 원본, 광장의 헤르메스와 수호신.
		SpawnStoryNpc(scene, 3.0f, 9.0f, 1.05f, 0.95f, 0.9f, 0.8f, ApolloLook(), "CHR_APOLLO", "Apollo",
			"STORY_STEP >= MQ_03.APOLLO && STORY_STEP < MQ_04.DECLARE");
		MakeInspectable(scene.Spawn<SteleActor>(3.3f, -7.3f, 1.1f, 0.5f, 0.48f, 0.45f, true), "OBJ_SEAL_TABLET", "STORY_STEP >= MQ_03.VAULT");
		SpawnStoryNpc(scene, 0.4f, 3.0f, 1.f, 0.55f, 0.72f, 0.92f, HermesLook(), "CHR_HERMES", "Hermes",
			"STORY_STEP == MQ_03.RUMOR || STORY_STEP == MQ_03.PATRON && PATRON == HERMES");
		const PatronPresence kPatronCalls =
		{
			"PATRON == ZEUS && STORY_STEP == MQ_03.PATRON",
			"PATRON == POSEIDON && STORY_STEP == MQ_03.PATRON",
			"PATRON == HADES && STORY_STEP == MQ_03.PATRON",
			"PATRON == APHRODITE && STORY_STEP == MQ_03.PATRON",
			"STORY_STEP < MQ_00.START", // 헤르메스가 수호신이면 위의 광장 헤르메스가 그대로 맡는다(이 자리는 늘 비어 있음)
		};
		SpawnPatrons(scene, -1.2f, 2.2f, kPatronCalls, nullptr);

		// MQ_01 면접: 다섯 신이 제단 앞 광장에 화면 가로로 늘어선다(수호신을 고르면 모두 떠난다).
		const char* kInterview = "STORY_STEP >= MQ_01.INTERVIEW && STORY_STEP <= MQ_01.CHOOSE";
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

		// ---- 약초(들짐승·몬스터는 마을 밖 황야에 산다 — Wilderness.h) ----
		SpawnHerb(scene, tileMap, 9.5f, -6.5f);
		SpawnHerb(scene, tileMap, -12.5f, 7.0f);
		SpawnHerb(scene, tileMap, 7.5f, 10.0f);
		SpawnHerb(scene, tileMap, -5.5f, 11.5f);

		// 나무가 덮으면 안 되는 자리.
		const Spot kKeep[] =
		{
			kStart, kArrival, kSignpost, kCave, kCampfire, kSpring, { 7.2f, 0.8f }, { 8.9f, 3.6f }, { 2.0f, 7.8f },
			{ -1.2f, 9.0f }, { 10.4f, -1.6f }, { -2.0f, 6.0f }, { 1.0f, 6.8f }, { 6.0f, 1.8f }, { 5.4f, -4.6f }, { 2.8f, 1.2f },
			{ 9.0f, -0.4f }, ledgerSpot, { 3.0f, 9.0f }, { 3.3f, -7.3f }, { 0.4f, 3.0f }, { -1.2f, 2.2f },
		};
		avoid.insert(avoid.end(), std::begin(kKeep), std::end(kKeep));

		FinishRegion(scene, tileMap, rng, avoid, 30, 0.35f, 0.55f, false, kCrowd);
	}

	// =====================================================================================
	// LOC_OLYMPUS_FOOT — 올림포스 산기슭 · 테살리아 (중반 공용 허브: MQ_02 신들의 대회의)
	// 화면 위쪽에 눈 덮인 올림포스, 그 아래 언덕 위에 기둥이 둥글게 선 회의장과 크로노스 벽화, 화면 왼쪽에
	// 헤파이스토스의 대장간, 평원을 가로지르는 강(길이 지나는 곳은 여울), 강 건너 아래쪽 끝에 이정표.
	// =====================================================================================
	void BuildOlympusFoot(SceneGraph& scene, TileMap& tileMap, std::mt19937& rng, int seed)
	{
		const Spot kArrival = { 10.4f, 9.0f };
		const Spot kSignpost = { 11.6f, 10.0f };
		const Spot kHall = { -4.5f, -4.0f };
		const Spot kMural = { 0.4f, -7.6f };
		const Spot kForge = { 7.6f, -6.6f };
		const Spot kShrine = { 11.0f, -11.0f };  // 제우스의 옛 사당("왕의 서랍", RQ_ZEUS_02)
		const Spot kPass = { 3.5f, -10.5f };     // 북쪽 산길 초소(RQ_ZEUS_03의 "사고")
		const Spot kBeacon = { -12.2f, 7.6f };   // 들판 언덕의 봉화대(RQ_ZEUS_01)
		const Spot kHamlet = { -8.0f, 3.0f };    // 목동 마을

		// ---- 지형 ----
		Terrain::Fill(tileMap, TileType::Grass);

		// 올림포스 산: 화면 위쪽을 덮는 높은 바위 산.
		Terrain::PaintWhere(tileMap, [seed](float x, float y)
		{
			return x + y < -11.f + Terrain::Noise(x * 0.22f, y * 0.22f, seed) * 3.f - 1.5f;
		}, TileType::Rock);

		// 길: 이정표 → 여울 → 회의장 언덕, 갈림길 → 대장간 → 옛 사당, 벽화 → 북쪽 산길 초소.
		Terrain::PaintLine(tileMap, kSignpost.x - 0.8f, kSignpost.y - 0.8f, 3.5f, 3.0f, 0.7f, TileType::Path);
		Terrain::PaintLine(tileMap, 3.5f, 3.0f, kHall.x + 3.2f, kHall.y + 3.2f, 0.7f, TileType::Path);
		Terrain::PaintLine(tileMap, 2.4f, 1.3f, kForge.x - 1.4f, kForge.y + 2.6f, 0.55f, TileType::Path);
		Terrain::PaintLine(tileMap, kForge.x + 1.0f, kForge.y - 0.6f, kShrine.x - 0.4f, kShrine.y + 1.8f, 0.5f, TileType::Path);
		Terrain::PaintLine(tileMap, kMural.x + 0.6f, kMural.y - 0.4f, kPass.x, kPass.y, 0.5f, TileType::Path);

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
		SpawnHouse(scene, kForge.x, kForge.y, 1.5f, false, false);
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

		// ---- RQ_ZEUS "왕의 곁": 목동 마을의 번개 공연, 들판 언덕의 봉화대, 옛 사당의 "왕의 서랍", 북쪽 산길의 아테나 ----
		SpawnStoryNpc(scene, kHamlet.x + 0.4f, kHamlet.y - 2.2f, 1.25f, 1.02f, 1.f, 0.94f, ZeusLook(), "CHR_ZEUS", "Zeus",
			"PATRON == ZEUS && STORY_STEP >= RQ_ZEUS_01.TOUR && STORY_STEP <= RQ_ZEUS_03.SILENCE");

		MakeInspectable(scene.Spawn<AltarActor>(kBeacon.x, kBeacon.y), "OBJ_BEACON", nullptr);
		FireActor* beaconFire = scene.Spawn<FireActor>(kBeacon.x, kBeacon.y, 0.75f, 0.7f, false);
		beaconFire->SetPresence("BEACON_LIT");

		scene.Spawn<TempleActor>(kShrine.x, kShrine.y, 3.0f, 2.2f, 1.2f, false);
		MakeInspectable(scene.Spawn<TableActor>(kShrine.x, kShrine.y + 1.9f, 0.7f, 0.45f, 0.4f, 0.42f, 0.3f, 0.16f), "OBJ_ZEUS_CHEST", nullptr);

		SpawnTrigger(scene, kPass.x, kPass.y, 1.2f, "TRIGGER_MOUNTAIN_PASS");
		SpawnStoryNpc(scene, kPass.x + 1.1f, kPass.y - 0.9f, 1.1f, 0.55f, 0.6f, 0.7f, AthenaLook(), "CHR_ATHENA", "Athena",
			"PATRON == ZEUS && STORY_STEP == RQ_ZEUS_03.ATHENA");

		// ---- MQ_04 두 번째 전선: 빗나간 번개에 탄 목동 마을(내전이 터진 뒤 — V-06) ----
		const char* kWarRuins = "STORY_STEP >= MQ_04.SIDE";
		SpawnRuins(scene, kHamlet.x - 0.6f, kHamlet.y - 0.8f, 0, kWarRuins);
		SpawnRuins(scene, kHamlet.x - 2.4f, kHamlet.y + 2.0f, 1, kWarRuins);
		SpawnRuins(scene, kHamlet.x + 2.0f, kHamlet.y + 2.6f, 2, kWarRuins);
		SpawnTrigger(scene, kHamlet.x + 0.4f, kHamlet.y + 0.6f, 1.8f, "TRIGGER_FOOT_RUINS");
		SpawnStoryNpc(scene, kHamlet.x - 1.0f, kHamlet.y + 1.2f, 0.9f, 0.4f, 0.36f, 0.3f, CommonLook(4, false), "NPC_SURVIVOR", "Survivor",
			"STORY_STEP >= MQ_04.FRONT2 && STORY_STEP <= MQ_04.FRONT2_HELP");

		// ---- 약초(들짐승·몬스터는 마을 밖 황야에 산다 — Wilderness.h) ----
		SpawnHerb(scene, tileMap, 12.0f, 2.0f);
		SpawnHerb(scene, tileMap, -12.5f, 10.5f);
		SpawnHerb(scene, tileMap, 4.0f, 12.5f);
		SpawnHerb(scene, tileMap, -1.0f, -9.0f);

		std::vector<Spot> avoid =
		{
			kArrival, kSignpost, kMural, kForge, { kForge.x - 1.1f, kForge.y + 2.3f }, { -9.6f, 1.0f }, { -11.4f, 3.8f },
			{ -8.8f, 6.6f }, { -6.6f, 3.2f }, { 3.0f, -1.2f }, kShrine, { kShrine.x, kShrine.y + 1.9f }, kPass, kBeacon, kHamlet,
			{ kHamlet.x + 0.4f, kHamlet.y - 2.2f }, { kPass.x + 1.1f, kPass.y - 0.9f },
		};

		FinishRegion(scene, tileMap, rng, avoid, 26, 0.45f, 0.6f, true);
	}

	// =====================================================================================
	// LOC_ATHENS — 아테네 · 폴리스 연합 (MQ_02: 무너진 신상, 신앙을 잃은 시민들, 잊혀지는 신)
	// 화면 위쪽에 바위 언덕 아크로폴리스와 꼭대기의 신전, 가운데 아고라 광장(목이 부러진 채 누운 제우스 신상),
	// 광장 오른쪽에 새로 세운 로마식 유피테르 신전과 사제단 창고, 화면 왼쪽에 집들 사이로 난 뒷골목(막다른 곳에
	// 식은 화덕), 아래쪽 모서리에 항구로 이어지는 바다, 그 곁에 이정표.
	// =====================================================================================
	void BuildAthens(SceneGraph& scene, TileMap& tileMap, std::mt19937& rng, int seed)
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

		// ---- 신전과 신상 ----
		scene.Spawn<TempleActor>(kAcropolis.x - 0.4f, kAcropolis.y - 0.4f, 5.f, 3.2f, 1.6f, false);
		scene.Spawn<TempleActor>(kJupiter.x, kJupiter.y, 3.6f, 3.f, 1.4f, true);

		StatueActor* statue = scene.Spawn<StatueActor>(kStatue.x, kStatue.y, 1.3f, true);
		statue->SetStoryId("OBJ_FALLEN_STATUE");
		statue->SetInteractId(kInteractStory);

		// ---- 사제단 창고와 장부 ----
		SpawnHouse(scene, kStorehouse.x, kStorehouse.y, 1.6f, true, false);
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
		SpawnStoryNpc(scene, 3.6f, -2.4f, 0.9f, 0.4f, 0.55f, 0.42f, CommonLook(4, true), "NPC_ATHENS_CITIZEN", "Citizen", nullptr)->SetFaith(Faith::Jupiter); // 유피테르 신전으로 갈아탄 시민(아테나를 믿는 이웃과 다툰다)
		SpawnStoryNpc(scene, -1.8f, -1.6f, 0.95f, 0.85f, 0.83f, 0.78f, HearthGodLook(), "NPC_PHILOSOPHER", "Philosopher", nullptr);
		SpawnStoryNpc(scene, -2.4f, 6.4f, 0.9f, 0.92f, 0.9f, 0.86f, CommonLook(2, true), "NPC_PRIEST", "Priest", nullptr);

		// ---- RQ_HERMES "배달 사고": 아크로폴리스의 아테나(편지 받는 신), 신상 받침대 속 헤르메스의 비밀 우편함 ----
		SpawnStoryNpc(scene, kAcropolis.x + 2.6f, kAcropolis.y + 2.6f, 1.1f, 0.55f, 0.6f, 0.7f, AthenaLook(), "CHR_ATHENA", "Athena",
			"PATRON == HERMES && STORY_STEP == RQ_HERMES_01.LETTERS && !LETTER_ATHENA");
		Spot dropSpot = FindOpenGround(tileMap, kStatue.x + 1.3f, kStatue.y + 1.1f, 0.3f);
		ItemActor* deadDrop = scene.Spawn<ItemActor>(dropSpot.x, dropSpot.y, 0.5f, 0.8f, 0.7f, 0.5f, ItemKind::Scroll, kInteractStory);
		deadDrop->SetStoryId("OBJ_DEAD_DROP");
		deadDrop->SetPresence("STORY_STEP == RQ_HERMES_03.DROP");
		SpawnStoryNpc(scene, kAgora.x + 2.6f, kAgora.y - 1.6f, 1.f, 0.55f, 0.72f, 0.92f, HermesLook(), "CHR_HERMES", "Hermes",
			"PATRON == HERMES && STORY_STEP == RQ_HERMES_03.FEAR");

		// ---- SQ_ANY_01 디오니소스의 축제: 대회의가 끝난 뒤 아고라에서 축제를 연다(내전이 터지면 떠난다) ----
		SpawnStoryNpc(scene, kAgora.x + 3.4f, kAgora.y + 0.6f, 1.05f, 0.5f, 0.2f, 0.45f, DionysusLook(), "CHR_DIONYSUS", "Dionysus",
			"MQ02_DONE && STORY_STEP < MQ_04.DECLARE");

		// ---- MQ_04 첫 번째 전선: 해일이 덮친 항구 거리(V-06)와 그 틈에 상륙한 청동 전사들 ----
		const char* kWarRuins = "STORY_STEP >= MQ_04.SIDE";
		SpawnRuins(scene, 8.4f, 6.4f, 0, kWarRuins);
		SpawnRuins(scene, 4.2f, 9.6f, 1, kWarRuins);
		SpawnRuins(scene, 10.2f, -0.2f, 2, kWarRuins);
		SpawnTrigger(scene, 7.6f, 3.8f, 1.6f, "TRIGGER_ATHENS_RUINS");
		const float kBronze[3][2] = { { 6.0f, 8.6f }, { 9.6f, 6.8f }, { 3.4f, 6.8f } };
		for (int i = 0; i < 3; ++i)
		{
			AnimalActor* warrior = SpawnAnimal(scene, tileMap, AnimalKind::BronzeWarrior, kBronze[i][0], kBronze[i][1], 0.7f * (float)i);
			warrior->SetStoryId("MON_BRONZE_ATHENS");
			warrior->SetPresence("STORY_STEP == MQ_04.FRONT1_FIGHT");
		}

		// ---- 약초 ----
		SpawnHerb(scene, tileMap, -12.0f, -1.0f);
		SpawnHerb(scene, tileMap, -2.0f, -12.0f);

		std::vector<Spot> avoid =
		{
			kArrival, kSignpost, kJupiter, kStorehouse, ledgerSpot, { 7.1f, -2.6f }, { 4.5f, -4.3f }, { 8.55f, -4.75f },
			{ 6.0f, -6.5f }, { 9.1f, -8.35f }, { 7.2f, 5.6f }, { 9.0f, 1.8f }, { 5.4f, 7.6f }, { 1.4f, 8.8f }, { -1.6f, 10.0f }, { -2.4f, 6.4f },
			dropSpot, { kAcropolis.x + 2.6f, kAcropolis.y + 2.6f }, { kAgora.x + 3.4f, kAgora.y + 0.6f }, { 8.4f, 6.4f }, { 4.2f, 9.6f },
			{ 10.2f, -0.2f }, { 7.6f, 3.8f },
		};

		FinishRegion(scene, tileMap, rng, avoid, 14, 0.25f, 0.6f, false);
	}

	// =====================================================================================
	// LOC_AEGEAN — 에게해 군도 (RQ_POSEIDON "바다의 원한", SQ_AEGEAN_02 하얀 암소)
	// 바다 위에 섬 셋: 화면 아래쪽 큰 섬(어부 마을과 항구, 위쪽 해변에 난파선), 왼쪽 바위섬(해저 동굴),
	// 위쪽 작은 섬(하얀 암소). 섬끼리는 썰물에 드러나는 모래톱 길로 이어진다.
	// =====================================================================================
	void BuildAegean(SceneGraph& scene, TileMap& tileMap, std::mt19937& rng, int seed)
	{
		const Spot kArrival = { 9.6f, 9.4f };
		const Spot kSignpost = { 10.6f, 10.4f };
		const Spot kMainIsland = { 5.5f, 6.0f };
		const Spot kWestIsland = { -8.5f, -1.0f };
		const Spot kNorthIslet = { -3.0f, -10.0f };
		const Spot kWreck = { 1.2f, 2.4f };
		const Spot kCave = { -9.0f, -1.0f };

		// ---- 지형: 바다 위의 섬들 ----
		Terrain::Fill(tileMap, TileType::Water);
		Terrain::PaintWhere(tileMap, [seed, kMainIsland](float x, float y)
		{
			float dx = x - kMainIsland.x;
			float dy = y - kMainIsland.y;
			return sqrtf(dx * dx + dy * dy) < 8.f + (Terrain::Noise(x * 0.3f, y * 0.3f, seed) - 0.5f) * 2.f;
		}, TileType::Grass);
		Terrain::PaintDisk(tileMap, kWestIsland.x, kWestIsland.y, 4.4f, TileType::Grass);
		Terrain::PaintDisk(tileMap, -10.4f, -3.2f, 2.3f, TileType::Rock); // 동굴이 난 바위 절벽
		Terrain::PaintDisk(tileMap, kNorthIslet.x, kNorthIslet.y, 3.0f, TileType::Grass);
		Terrain::PaintDisk(tileMap, 11.0f, -4.5f, 1.6f, TileType::Rock);  // 바다 위에 솟은 바위

		// 모래톱 길(섬과 섬 사이)과 항구 부두.
		Terrain::PaintLine(tileMap, 1.5f, 3.5f, -5.0f, 0.4f, 0.7f, TileType::Path);
		Terrain::PaintLine(tileMap, 2.6f, 0.0f, -1.6f, -7.4f, 0.7f, TileType::Path);
		Terrain::PaintDisk(tileMap, 9.4f, 9.8f, 2.2f, TileType::Stone);
		Terrain::PaintLine(tileMap, 8.4f, 8.8f, 4.0f, 5.0f, 0.6f, TileType::Path);

		Terrain::EnsureFullyConnected(tileMap, kArrival.x, kArrival.y);

		// ---- 어부 마을과 항구 ----
		SpawnHouse(scene, 6.4f, 9.6f, 1.3f, true);
		SpawnHouse(scene, 3.4f, 8.4f, 1.2f, false);
		SpawnHouse(scene, 8.6f, 5.8f, 1.3f, true);
		SpawnSignpost(scene, kSignpost.x, kSignpost.y);
		SpawnStoryNpc(scene, 5.0f, 7.6f, 0.9f, 0.4f, 0.5f, 0.62f, CommonLook(1, false), "NPC_FISHER", "Fisher", nullptr);
		SpawnStoryNpc(scene, 7.8f, 8.0f, 0.9f, 0.62f, 0.52f, 0.36f, CommonLook(3, false), "NPC_FISHER", "Fisher", nullptr);

		// 포세이돈(RQ_POSEIDON 동안 항구 부두에).
		SpawnStoryNpc(scene, 10.8f, 7.8f, 1.2f, 0.2f, 0.5f, 0.55f, PoseidonLook(), "CHR_POSEIDON", "Poseidon",
			"PATRON == POSEIDON && STORY_STEP >= RQ_POSEIDON_01.HARBOR && STORY_STEP <= RQ_POSEIDON_03.SCHEME");

		// ---- 난파선 해변: 뒤집힌 선체 + 부러진 돛대, 생존자 셋(RQ_POSEIDON_01) ----
		scene.Spawn<TableActor>(kWreck.x, kWreck.y, 2.4f, 0.9f, 0.5f, 0.32f, 0.22f, 0.14f);
		scene.Spawn<TableActor>(kWreck.x + 1.6f, kWreck.y - 0.4f, 0.16f, 0.16f, 1.7f, 0.36f, 0.25f, 0.15f);
		SpawnStoryNpc(scene, 0.4f, 3.8f, 0.9f, 0.7f, 0.64f, 0.5f, CommonLook(0, false), "NPC_CASTAWAY_A", "Castaway",
			"PATRON == POSEIDON && STORY_STEP == RQ_POSEIDON_01.RESCUE && !RESCUED_A");
		SpawnStoryNpc(scene, 1.8f, 1.0f, 0.9f, 0.5f, 0.58f, 0.66f, CommonLook(2, true), "NPC_CASTAWAY_B", "Castaway",
			"PATRON == POSEIDON && STORY_STEP == RQ_POSEIDON_01.RESCUE && !RESCUED_B");
		SpawnStoryNpc(scene, 3.4f, 0.8f, 0.9f, 0.66f, 0.42f, 0.3f, CommonLook(4, false), "NPC_CASTAWAY_C", "Castaway",
			"PATRON == POSEIDON && STORY_STEP == RQ_POSEIDON_01.RESCUE && !RESCUED_C");

		// ---- 「형님 잘못 목록」 두루마리 묶음 넷(섬마다 흩어짐, RQ_POSEIDON_02) ----
		const Spot kScrolls[4] = { { 4.2f, 4.2f }, { 8.2f, 2.6f }, { -7.0f, 1.6f }, { -2.2f, -9.0f } };
		std::vector<Spot> avoid;
		for (const Spot& wanted : kScrolls)
		{
			Spot spot = FindOpenGround(tileMap, wanted.x, wanted.y, 0.3f);
			ItemActor* scroll = scene.Spawn<ItemActor>(spot.x, spot.y, 0.5f, 0.95f, 0.88f, 0.7f, ItemKind::Scroll, kInteractPickup);
			scroll->SetStoryId("ITEM_GRUDGE_SCROLL");
			scroll->SetPresence("STORY_STEP == RQ_POSEIDON_02.SCROLLS");
			avoid.push_back(spot);
		}

		// ---- 서쪽 바위섬의 해저 동굴과 젖은 실 뭉치(RQ_POSEIDON_03) ----
		scene.Spawn<CaveEntranceActor>(kCave.x, kCave.y, 1.5f);
		SpawnTrigger(scene, kCave.x + 0.4f, kCave.y + 1.4f, 1.1f, "TRIGGER_SEA_CAVE");
		Spot threadSpot = FindOpenGround(tileMap, kCave.x + 1.6f, kCave.y + 0.6f, 0.3f);
		ItemActor* thread = scene.Spawn<ItemActor>(threadSpot.x, threadSpot.y, 0.5f, 1.2f, 1.0f, 0.5f, ItemKind::Scroll, kInteractPickup);
		thread->SetStoryId("ITEM_CUT_THREAD");
		thread->SetPresence("STORY_STEP == RQ_POSEIDON_03.THREAD");

		// ---- 위쪽 작은 섬의 하얀 암소(이오, SQ_AEGEAN_02): 말을 걸 수 있고 공격 대상이 아니다 ----
		AnimalActor* io = SpawnAnimal(scene, tileMap, AnimalKind::WhiteCow, kNorthIslet.x, kNorthIslet.y, 1.3f);
		io->SetStoryId("CHR_IO");
		io->SetInteractId(kInteractStory);

		SpawnHerb(scene, tileMap, 2.0f, 9.2f);
		SpawnHerb(scene, tileMap, -9.4f, 1.8f);

		const Spot kKeep[] =
		{
			kArrival, kSignpost, kWreck, { kWreck.x + 1.6f, kWreck.y - 0.4f }, kCave, threadSpot, kNorthIslet, { 6.4f, 9.6f },
			{ 3.4f, 8.4f }, { 8.6f, 5.8f }, { 5.0f, 7.6f }, { 7.8f, 8.0f }, { 10.8f, 7.8f }, { 0.4f, 3.8f }, { 1.8f, 1.0f }, { 3.4f, 0.8f },
		};
		avoid.insert(avoid.end(), std::begin(kKeep), std::end(kKeep));

		FinishRegion(scene, tileMap, rng, avoid, 12, 0.6f, 0.7f, false);
	}

	// =====================================================================================
	// LOC_CYPRUS — 키프로스 · 파포스 (RQ_APHRODITE "가면무도회")
	// 화면 위쪽 언덕 위 아프로디테의 저택, 가운데 기둥이 둥글게 선 연회장(가면 쓴 손님들), 오른쪽에 기록 관리인이
	// 지키는 정자, 왼쪽 장미 정원 끝에 거울의 방, 화면 아래 모서리 바다와 항구, 그 곁의 이정표.
	// =====================================================================================
	void BuildCyprus(SceneGraph& scene, TileMap& tileMap, std::mt19937& rng, int seed)
	{
		const Spot kArrival = { 10.0f, 3.2f };
		const Spot kSignpost = { 11.0f, 4.0f };
		const Spot kHall = { -1.5f, -1.0f };
		const Spot kVilla = { -8.0f, -6.5f };
		const Spot kPavilion = { 6.4f, -4.4f };
		const Spot kMirrorRoom = { -10.0f, 3.5f };

		// ---- 지형 ----
		Terrain::Fill(tileMap, TileType::Grass);
		Terrain::PaintWhere(tileMap, [seed](float x, float y)
		{
			return x + y > 17.f + Terrain::Noise(x * 0.2f, y * 0.2f, seed + 3) * 3.f - 1.5f;
		}, TileType::Water);
		Terrain::PaintWhere(tileMap, [seed](float x, float y)
		{
			return x + y < -17.f + Terrain::Noise(x * 0.25f, y * 0.25f, seed + 9) * 3.f - 1.5f;
		}, TileType::Rock);

		Terrain::PaintDisk(tileMap, kHall.x, kHall.y, 4.4f, TileType::Stone);
		Terrain::PaintDisk(tileMap, kVilla.x, kVilla.y, 3.6f, TileType::Stone);
		Terrain::PaintLine(tileMap, kSignpost.x - 0.8f, kSignpost.y - 0.4f, kHall.x + 3.4f, kHall.y + 2.4f, 0.7f, TileType::Path);
		Terrain::PaintLine(tileMap, kHall.x - 3.0f, kHall.y - 3.0f, kVilla.x + 2.4f, kVilla.y + 2.4f, 0.6f, TileType::Path);
		Terrain::PaintLine(tileMap, kHall.x + 3.6f, kHall.y - 1.6f, kPavilion.x - 0.4f, kPavilion.y + 1.6f, 0.55f, TileType::Path);
		Terrain::PaintLine(tileMap, kHall.x - 3.4f, kHall.y + 2.0f, kMirrorRoom.x + 1.2f, kMirrorRoom.y + 1.4f, 0.55f, TileType::Path);

		Terrain::EnsureFullyConnected(tileMap, kArrival.x, kArrival.y);

		// ---- 연회장: 바닥 원판 + 둥글게 선 기둥(입구는 비움) + 식탁 둘, 한 기둥엔 황금사과 문양 ----
		scene.Spawn<FloorDiscActor>(kHall.x, kHall.y, 3.8f, 0.86f, 0.78f, 0.74f);
		const int kColumns = 10;
		const float kEntranceAngle = kPi * 0.25f;
		const int kMarkedColumn = 6; // 황금사과 문양이 남은 기둥(RQ_APHRODITE_03)
		for (int i = 0; i < kColumns; ++i)
		{
			float angle = (float)i * (2.f * kPi / (float)kColumns);
			if (AngleDistance(angle, kEntranceAngle) < 0.35f)
			{
				continue;
			}

			ColumnActor* column = scene.Spawn<ColumnActor>(kHall.x + cosf(angle) * 3.5f, kHall.y + sinf(angle) * 3.5f, 2.3f, false);
			if (i == kMarkedColumn)
			{
				MakeInspectable(column, "OBJ_APPLE_MARK", nullptr);
			}
		}
		scene.Spawn<TableActor>(kHall.x - 0.6f, kHall.y - 0.8f, 1.8f, 0.9f, 0.7f, 0.9f, 0.82f, 0.78f);
		scene.Spawn<TableActor>(kHall.x + 1.0f, kHall.y + 0.8f, 1.2f, 0.8f, 0.7f, 0.9f, 0.82f, 0.78f);

		// 가면 쓴 손님 셋(소문을 캐낼 상대) + 시종 둘.
		HumanLook heroLook = CommonLook(1, false);
		heroLook.helmet = true;
		heroLook.spear = true;
		HumanLook nymphLook = CommonLook(2, true);
		nymphLook.longHair = true;
		nymphLook.wreath = true; nymphLook.wreathR = 0.95f; nymphLook.wreathG = 0.6f; nymphLook.wreathB = 0.75f;
		SpawnStoryNpc(scene, kHall.x + 1.6f, kHall.y - 1.4f, 1.f, 0.7f, 0.25f, 0.2f, heroLook, "NPC_GUEST_HERO", "Guest", nullptr);
		SpawnStoryNpc(scene, kHall.x - 2.2f, kHall.y + 0.6f, 0.95f, 0.5f, 0.75f, 0.7f, nymphLook, "NPC_GUEST_NYMPH", "Guest", nullptr);
		SpawnStoryNpc(scene, kHall.x - 0.4f, kHall.y + 2.0f, 0.95f, 0.72f, 0.56f, 0.3f, CommonLook(3, true), "NPC_GUEST_MERCHANT", "Guest", nullptr);
		SpawnStoryNpc(scene, kHall.x + 2.4f, kHall.y + 1.2f, 0.9f, 0.92f, 0.9f, 0.86f, CommonLook(0, true), "NPC_PAPHOS_SERVANT", "Servant", nullptr);
		SpawnStoryNpc(scene, kHall.x - 1.8f, kHall.y - 2.4f, 0.9f, 0.92f, 0.9f, 0.86f, CommonLook(4, true), "NPC_PAPHOS_SERVANT", "Servant", nullptr);

		// ---- 아프로디테의 저택(+ 루트 동안 저택 앞의 아프로디테) ----
		scene.Spawn<TempleActor>(kVilla.x, kVilla.y, 4.6f, 3.2f, 1.6f, false);
		SpawnStoryNpc(scene, kVilla.x + 2.2f, kVilla.y + 3.0f, 1.05f, 0.95f, 0.6f, 0.7f, AphroditeLook(), "CHR_APHRODITE", "Aphrodite",
			"PATRON == APHRODITE && STORY_STEP >= RQ_APHRODITE_01.MASK && STORY_STEP <= RQ_APHRODITE_03.TRACE");

		// ---- 기록 관리인이 지키는 정자(헤라·아레스의 비밀 회동 기록, RQ_APHRODITE_02) ----
		SpawnHouse(scene, kPavilion.x, kPavilion.y, 1.5f, true, false);
		SpawnStoryNpc(scene, kPavilion.x - 0.6f, kPavilion.y + 1.6f, 0.95f, 0.36f, 0.34f, 0.4f, CommonLook(1, true), "NPC_RECORD_KEEPER", "Keeper", nullptr);

		// ---- 장미 정원 끝의 거울의 방(RQ_APHRODITE_03: 늙어 가는 얼굴) ----
		scene.Spawn<TempleActor>(kMirrorRoom.x, kMirrorRoom.y, 2.8f, 2.2f, 1.3f, false);
		SpawnTrigger(scene, kMirrorRoom.x + 1.4f, kMirrorRoom.y + 2.0f, 1.0f, "TRIGGER_MIRROR_ROOM");
		SpawnStoryNpc(scene, kMirrorRoom.x + 0.6f, kMirrorRoom.y + 1.6f, 1.05f, 0.95f, 0.6f, 0.7f, AphroditeLook(), "CHR_APHRODITE", "Aphrodite",
			"PATRON == APHRODITE && STORY_STEP == RQ_APHRODITE_03.MIRROR");
		const Spot kRoses[5] = { { -6.4f, 2.4f }, { -7.6f, 5.0f }, { -5.0f, 5.6f }, { -8.8f, 0.6f }, { -4.2f, 3.4f } };
		for (int i = 0; i < 5; ++i)
		{
			scene.Spawn<TreeActor>(kRoses[i].x, kRoses[i].y, 0.8f + 0.08f * (float)i, 0.62f, 0.2f, 0.32f, TreeKind::Broadleaf);
		}

		SpawnSignpost(scene, kSignpost.x, kSignpost.y);
		SpawnHouse(scene, 8.0f, 6.6f, 1.3f, true);
		SpawnHouse(scene, 4.8f, 8.4f, 1.2f, false);
		SpawnStoryNpc(scene, 7.0f, 4.6f, 0.9f, 0.4f, 0.5f, 0.62f, CommonLook(2, false), "NPC_FISHER", "Fisher", nullptr);
		SpawnHerb(scene, tileMap, 9.0f, -9.0f);
		SpawnHerb(scene, tileMap, -12.0f, 8.0f);

		std::vector<Spot> avoid =
		{
			kArrival, kSignpost, kVilla, kPavilion, kMirrorRoom, { kVilla.x + 2.2f, kVilla.y + 3.0f }, { kPavilion.x - 0.6f, kPavilion.y + 1.6f },
			{ kMirrorRoom.x + 1.4f, kMirrorRoom.y + 2.0f }, { 8.0f, 6.6f }, { 4.8f, 8.4f }, { 7.0f, 4.6f },
		};
		avoid.insert(avoid.end(), std::begin(kRoses), std::end(kRoses));

		FinishRegion(scene, tileMap, rng, avoid, 16, 0.55f, 0.6f, false);
	}

	// =====================================================================================
	// LOC_TAENARON — 타이나론 곶 · 저승 입구 (RQ_HADES "끝나지 않는 자들", SQ_TAENARON_02 닿지 않는 물)
	// 화면 위쪽 절벽 아래 저승으로 내려가는 동굴 문, 그 앞 잿빛 광장의 접수 창구와 줄 선 영혼들, 오른쪽의
	// 석류 정원(페르세포네), 곶 끝(왼쪽)의 "돌아온 자들"의 마을, 아래쪽 탄탈로스의 웅덩이, 오른쪽 아래 이정표.
	// =====================================================================================
	void BuildTaenaron(SceneGraph& scene, TileMap& tileMap, std::mt19937& rng, int seed)
	{
		const Spot kArrival = { 10.0f, 8.6f };
		const Spot kSignpost = { 11.0f, 9.6f };
		const Spot kGate = { -3.0f, -6.6f };
		const Spot kDesk = { -1.6f, -3.0f };
		const Spot kGarden = { 7.6f, -5.6f };
		const Spot kVillage = { -9.0f, 4.0f };
		const Spot kPool = { 3.6f, 7.6f };

		// ---- 지형: 잿빛 흙의 곶, 위쪽 절벽, 왼쪽 바다 ----
		Terrain::Fill(tileMap, TileType::Path);
		Terrain::PaintWhere(tileMap, [seed](float x, float y)
		{
			return x + y < -11.f + Terrain::Noise(x * 0.25f, y * 0.25f, seed) * 3.f - 1.5f;
		}, TileType::Rock);
		Terrain::PaintWhere(tileMap, [seed](float x, float y)
		{
			return y - x > 15.f + Terrain::Noise(x * 0.2f, y * 0.2f, seed + 4) * 3.f - 1.5f;
		}, TileType::Water);
		Terrain::PaintDisk(tileMap, kDesk.x, kDesk.y, 4.0f, TileType::Stone);
		Terrain::PaintDisk(tileMap, kGarden.x, kGarden.y, 3.2f, TileType::Grass);
		Terrain::PaintDisk(tileMap, kVillage.x, kVillage.y, 3.4f, TileType::Grass);
		Terrain::PaintDisk(tileMap, kPool.x, kPool.y, 1.3f, TileType::Water);

		Terrain::EnsureFullyConnected(tileMap, kArrival.x, kArrival.y);

		// ---- 저승 문과 접수 창구(+ 루트 동안 창구 뒤의 하데스) ----
		scene.Spawn<CaveEntranceActor>(kGate.x, kGate.y, 2.0f);
		scene.Spawn<TableActor>(kDesk.x, kDesk.y, 1.8f, 0.7f, 0.8f, 0.2f, 0.17f, 0.18f);
		scene.Spawn<SteleActor>(kDesk.x - 2.0f, kDesk.y - 0.6f, 1.3f, 0.34f, 0.32f, 0.34f, false); // 접수 안내문
		SpawnStoryNpc(scene, kDesk.x - 0.6f, kDesk.y - 1.0f, 1.15f, 0.16f, 0.12f, 0.2f, HadesLook(), "CHR_HADES", "Hades",
			"PATRON == HADES && STORY_STEP >= RQ_HADES_01.DESK && STORY_STEP <= RQ_HADES_03.TRUTH");

		// 줄 선 영혼들(늘 있음) + 서류가 뒤섞인 영혼 셋(RQ_HADES_01).
		for (int i = 0; i < 4; ++i)
		{
			NpcActor* soul = SpawnStoryNpc(scene, 1.8f + 0.9f * (float)i, -0.6f + 0.9f * (float)i, 0.9f, 0.7f, 0.72f, 0.78f, CommonLook(i, true),
				"NPC_SOUL", "Soul", nullptr);
			soul->SetFaded(0.45f);
		}
		NpcActor* fisherSoul = SpawnStoryNpc(scene, 0.4f, -1.8f, 0.9f, 0.4f, 0.5f, 0.62f, CommonLook(1, false), "NPC_SOUL_FISHER", "Soul",
			"PATRON == HADES && STORY_STEP == RQ_HADES_01.SORT && !SORTED_FISHER");
		fisherSoul->SetFaded(0.55f);
		NpcActor* grannySoul = SpawnStoryNpc(scene, 1.2f, -3.4f, 0.85f, 0.6f, 0.5f, 0.56f, CommonLook(3, true), "NPC_SOUL_GRANNY", "Soul",
			"PATRON == HADES && STORY_STEP == RQ_HADES_01.SORT && !SORTED_GRANNY");
		grannySoul->SetFaded(0.55f);
		HumanLook generalLook = CommonLook(2, false);
		generalLook.helmet = true;
		NpcActor* generalSoul = SpawnStoryNpc(scene, -0.6f, -0.2f, 0.95f, 0.6f, 0.2f, 0.16f, generalLook, "NPC_SOUL_GENERAL", "Soul",
			"PATRON == HADES && STORY_STEP == RQ_HADES_01.SORT && !SORTED_GENERAL");
		generalSoul->SetFaded(0.55f);

		// ---- 석류 정원과 페르세포네(지상과 저승을 오가는 정보원) ----
		const Spot kPomegranates[4] = { { 9.0f, -7.0f }, { 6.2f, -7.2f }, { 9.4f, -4.4f }, { 6.6f, -3.6f } };
		for (int i = 0; i < 4; ++i)
		{
			scene.Spawn<TreeActor>(kPomegranates[i].x, kPomegranates[i].y, 1.1f, 0.3f, 0.36f, 0.16f, TreeKind::Broadleaf);
		}
		SpawnStoryNpc(scene, kGarden.x - 0.4f, kGarden.y + 0.8f, 1.f, 0.42f, 0.12f, 0.2f, PersephoneLook(), "CHR_PERSEPHONE", "Persephone",
			"STORY_STEP < MQ_04.DECLARE");

		// ---- "돌아온 자들"의 마을(유머 없음 — V-03): 집 셋, 붕대처럼 천을 감은 사람들, 노인 ----
		SpawnHouse(scene, kVillage.x - 1.8f, kVillage.y - 1.6f, 1.3f, false);
		SpawnHouse(scene, kVillage.x + 1.4f, kVillage.y - 2.2f, 1.2f, false);
		SpawnHouse(scene, kVillage.x - 2.4f, kVillage.y + 1.8f, 1.2f, false);
		SpawnTrigger(scene, kVillage.x + 1.6f, kVillage.y + 0.6f, 2.0f, "TRIGGER_UNDYING_VILLAGE");
		SpawnStoryNpc(scene, kVillage.x - 0.2f, kVillage.y + 0.4f, 0.9f, 0.5f, 0.46f, 0.4f, UndyingLook(0), "NPC_UNDYING_ELDER", "Elder", nullptr);
		SpawnStoryNpc(scene, kVillage.x + 0.8f, kVillage.y + 2.2f, 0.9f, 0.46f, 0.44f, 0.4f, UndyingLook(1), "NPC_UNDYING", "Undying", nullptr);
		SpawnStoryNpc(scene, kVillage.x - 1.0f, kVillage.y - 0.2f, 0.9f, 0.44f, 0.42f, 0.38f, UndyingLook(2), "NPC_UNDYING", "Undying", nullptr);

		// ---- 탄탈로스의 웅덩이(SQ_TAENARON_02): 목까지 물에 잠겼으나 마실 수 없는 왕 ----
		SpawnStoryNpc(scene, kPool.x - 0.5f, kPool.y - 0.5f, 0.95f, 0.5f, 0.42f, 0.36f, TantalusLook(), "CHR_TANTALUS", "Tantalus", nullptr);

		SpawnSignpost(scene, kSignpost.x, kSignpost.y);
		SpawnHerb(scene, tileMap, 12.0f, -2.0f);
		SpawnHerb(scene, tileMap, -4.0f, 10.0f);

		std::vector<Spot> avoid =
		{
			kArrival, kSignpost, kGate, kDesk, kGarden, kVillage, kPool, { kDesk.x - 2.0f, kDesk.y - 0.6f }, { 0.4f, -1.8f }, { 1.2f, -3.4f },
			{ -0.6f, -0.2f }, { kVillage.x - 1.8f, kVillage.y - 1.6f }, { kVillage.x + 1.4f, kVillage.y - 2.2f },
			{ kVillage.x - 2.4f, kVillage.y + 1.8f },
		};
		avoid.insert(avoid.end(), std::begin(kPomegranates), std::end(kPomegranates));

		FinishRegion(scene, tileMap, rng, avoid, 6, 0.8f, 0.6f, false);
	}

	// =====================================================================================
	// LOC_WEST — 서쪽 해안 · 라티움 방면 (MQ_03: 사제단의 자금줄, 헤라와의 대면, "유피테르의 아내 유노" 비문,
	// 신전 뒤 비밀 통로, SQ_WEST_01 리브랜딩)
	// 화면 위쪽에 높은 단 위의 로마식 대신전(유피테르·유노), 그 앞 광장, 신전 뒤 바위 언덕의 비밀 통로, 오른쪽 위
	// 길가의 경계석과 하급 신, 아래쪽 로마식 시장과 항구, 오른쪽 아래 이정표.
	// =====================================================================================
	void BuildWest(SceneGraph& scene, TileMap& tileMap, std::mt19937& rng, int seed)
	{
		const Spot kArrival = { 10.0f, 9.0f };
		const Spot kSignpost = { 11.0f, 10.0f };
		const Spot kTemple = { -4.0f, -4.4f };
		const Spot kForum = { -2.0f, -0.8f };
		const Spot kPassage = { -9.6f, -7.0f };
		const Spot kBoundary = { 6.6f, -6.4f };
		const Spot kMarket = { 3.4f, 5.4f };

		// ---- 지형 ----
		Terrain::Fill(tileMap, TileType::Grass);
		Terrain::PaintWhere(tileMap, [seed](float x, float y)
		{
			return x + y > 18.f + Terrain::Noise(x * 0.2f, y * 0.2f, seed + 7) * 3.f - 1.5f;
		}, TileType::Water);
		Terrain::PaintDisk(tileMap, -11.4f, -8.6f, 2.4f, TileType::Rock);
		Terrain::PaintDisk(tileMap, kForum.x, kForum.y, 5.0f, TileType::Stone);
		Terrain::PaintDisk(tileMap, kMarket.x, kMarket.y, 2.6f, TileType::Stone);
		Terrain::PaintDisk(tileMap, 9.0f, 9.6f, 2.2f, TileType::Stone);
		Terrain::PaintLine(tileMap, kSignpost.x - 0.8f, kSignpost.y - 0.8f, kMarket.x + 1.6f, kMarket.y + 1.4f, 0.7f, TileType::Path);
		Terrain::PaintLine(tileMap, kMarket.x - 1.4f, kMarket.y - 1.4f, kForum.x + 3.4f, kForum.y + 3.4f, 0.7f, TileType::Path);
		Terrain::PaintLine(tileMap, kForum.x + 3.0f, kForum.y - 2.4f, kBoundary.x - 0.6f, kBoundary.y + 1.2f, 0.55f, TileType::Path);
		Terrain::PaintLine(tileMap, kForum.x - 4.0f, kForum.y - 2.4f, kPassage.x + 1.0f, kPassage.y + 1.6f, 0.5f, TileType::Path);

		Terrain::EnsureFullyConnected(tileMap, kArrival.x, kArrival.y);

		// ---- 로마식 대신전 + 정면의 금빛 비문(유피테르와 그의 아내 유노) ----
		scene.Spawn<TempleActor>(kTemple.x, kTemple.y, 5.2f, 4.0f, 2.0f, true);
		MakeInspectable(scene.Spawn<SteleActor>(kForum.x + 2.2f, kForum.y - 1.6f, 1.4f, 0.86f, 0.84f, 0.79f, true), "OBJ_JUNO_INSCRIPTION", nullptr);

		// 사제장(늘 있음), 대면 동안 신전 앞의 헤라와 아레스.
		SpawnStoryNpc(scene, kForum.x + 1.6f, kForum.y + 1.4f, 0.95f, 0.92f, 0.9f, 0.86f, RomanPriestLook(), "NPC_WEST_PRIEST", "High Priest", nullptr);
		const char* kConfrontation = "STORY_STEP == MQ_03.HERA || STORY_STEP == MQ_03.INSCRIPTION";
		SpawnStoryNpc(scene, kForum.x - 1.6f, kForum.y - 0.2f, 1.15f, 0.52f, 0.28f, 0.62f, HeraLook(), "CHR_HERA", "Hera", kConfrontation);
		SpawnStoryNpc(scene, kForum.x - 2.8f, kForum.y + 0.6f, 1.3f, 0.6f, 0.15f, 0.12f, AresLook(), "CHR_ARES", "Ares", kConfrontation);

		// ---- 신전 뒤 바위 언덕의 비밀 통로(숨은 단서) ----
		MakeInspectable(scene.Spawn<CaveEntranceActor>(kPassage.x, kPassage.y, 1.1f), "OBJ_SECRET_PASSAGE", nullptr);

		// ---- 길가의 경계석과 이름이 둘인 하급 신(SQ_WEST_01) ----
		scene.Spawn<SteleActor>(kBoundary.x, kBoundary.y, 0.9f, 0.62f, 0.6f, 0.56f, false);
		SpawnStoryNpc(scene, kBoundary.x + 0.9f, kBoundary.y + 0.9f, 0.95f, 0.58f, 0.56f, 0.52f, TerminusLook(), "CHR_TERMINUS", "Terminus", nullptr);

		// ---- 로마식 시장과 시민 ----
		scene.Spawn<StallActor>(kMarket.x - 0.8f, kMarket.y - 0.6f, 0.75f, 0.12f, 0.12f);
		scene.Spawn<StallActor>(kMarket.x + 1.0f, kMarket.y + 0.6f, 0.9f, 0.86f, 0.8f);
		SpawnHouse(scene, 7.4f, 4.6f, 1.4f, true);
		SpawnHouse(scene, 0.6f, 9.0f, 1.3f, true);
		SpawnHouse(scene, 6.4f, 1.0f, 1.3f, false);
		SpawnHouse(scene, -5.0f, 6.0f, 1.3f, false);
		SpawnStoryNpc(scene, kMarket.x + 0.2f, kMarket.y + 1.8f, 0.9f, 0.82f, 0.8f, 0.76f, CommonLook(0, true), "NPC_WEST_CITIZEN", "Citizen", nullptr);
		SpawnStoryNpc(scene, kMarket.x - 2.0f, kMarket.y + 0.6f, 0.9f, 0.6f, 0.18f, 0.18f, CommonLook(3, false), "NPC_WEST_CITIZEN", "Citizen", nullptr);
		SpawnStoryNpc(scene, 8.6f, 7.6f, 0.9f, 0.45f, 0.42f, 0.5f, CommonLook(2, false), "NPC_WEST_CITIZEN", "Citizen", nullptr);
		SpawnSignpost(scene, kSignpost.x, kSignpost.y);

		SpawnHerb(scene, tileMap, -12.0f, 2.0f);
		SpawnHerb(scene, tileMap, 11.0f, -10.0f);

		std::vector<Spot> avoid =
		{
			kArrival, kSignpost, kTemple, kPassage, kBoundary, { kBoundary.x + 0.9f, kBoundary.y + 0.9f }, { kForum.x + 2.2f, kForum.y - 1.6f },
			{ 7.4f, 4.6f }, { 0.6f, 9.0f }, { 6.4f, 1.0f }, { -5.0f, 6.0f }, { 8.6f, 7.6f },
		};

		FinishRegion(scene, tileMap, rng, avoid, 18, 0.7f, 0.6f, false);
	}

	// =====================================================================================
	// LOC_OLYMPUS — 올림포스 정상 · 지하 베틀실 (MQ_04 올림포스 내전, MQ_05 모이라이와 마지막 실)
	// 구름 위 대리석 정상: 화면 위쪽에 왕좌의 홀(큰 신전), 오른쪽 위 제우스 진영과 왼쪽 아래 반란군 진영의 천막,
	// 오른쪽 아래 정상 끝의 균열(피티아의 희생), 왼쪽 위 눈 덮인 바위 속에 숨은 지하 베틀실(바위로 막힌 좁은 길 너머 —
	// 피티아가 길을 연 뒤에야 지나갈 수 있다), 오른쪽 아래 이정표.
	// =====================================================================================
	void BuildOlympus(SceneGraph& scene, TileMap& tileMap, std::mt19937& rng, int seed)
	{
		const Spot kArrival = { 9.6f, 9.6f };
		const Spot kSignpost = { 10.6f, 10.6f };
		const Spot kHall = { -3.6f, -4.0f };
		const Spot kCrack = { 8.4f, 4.6f };
		const Spot kLoomRoom = { -9.6f, -9.6f };
		const Spot kGate = { -6.8f, -6.8f };

		// ---- 지형: 대리석 정상 + 위쪽의 눈 덮인 바위, 그 속에 파 놓은 베틀실과 좁은 길 ----
		Terrain::Fill(tileMap, TileType::Grass);
		Terrain::PaintDisk(tileMap, 0.0f, 0.0f, 9.0f, TileType::Stone);
		Terrain::PaintWhere(tileMap, [seed](float x, float y)
		{
			return x + y < -11.5f + Terrain::Noise(x * 0.25f, y * 0.25f, seed) * 2.f - 1.f;
		}, TileType::Rock);
		Terrain::PaintDisk(tileMap, kLoomRoom.x, kLoomRoom.y, 3.2f, TileType::Stone);
		Terrain::PaintLine(tileMap, -5.0f, -5.0f, kLoomRoom.x + 1.8f, kLoomRoom.y + 1.8f, 0.75f, TileType::Stone);

		Terrain::EnsureFullyConnected(tileMap, kArrival.x, kArrival.y);

		// ---- 왕좌의 홀 ----
		scene.Spawn<TempleActor>(kHall.x, kHall.y, 6.4f, 4.6f, 2.6f, false);
		SpawnTrigger(scene, kHall.x + 3.0f, kHall.y + 3.0f, 2.0f, "TRIGGER_THRONE_HALL");

		// 두 진영의 천막(제우스 진영은 흰·금빛, 반란군은 붉은빛).
		scene.Spawn<StallActor>(5.2f, -5.6f, 0.95f, 0.92f, 0.8f);
		scene.Spawn<StallActor>(7.4f, -3.4f, 0.95f, 0.85f, 0.45f);
		scene.Spawn<StallActor>(-6.0f, 5.6f, 0.72f, 0.12f, 0.12f);
		scene.Spawn<StallActor>(-3.6f, 7.4f, 0.6f, 0.1f, 0.14f);

		// 내전이 선포된 뒤 정상에 모인 신들(피티아의 희생 전까지).
		const char* kWar = "STORY_STEP >= MQ_04.DECLARE && STORY_STEP < MQ_04.SACRIFICE";
		SpawnStoryNpc(scene, 0.6f, -0.6f, 1.25f, 1.02f, 1.f, 0.94f, ZeusLook(), "CHR_ZEUS", "Zeus", kWar);
		SpawnStoryNpc(scene, 1.8f, -1.6f, 1.1f, 0.55f, 0.6f, 0.7f, AthenaLook(), "CHR_ATHENA", "Athena", kWar);
		SpawnStoryNpc(scene, -1.6f, 1.2f, 1.15f, 0.52f, 0.28f, 0.62f, HeraLook(), "CHR_HERA", "Hera", kWar);
		SpawnStoryNpc(scene, -3.0f, 2.2f, 1.3f, 0.6f, 0.15f, 0.12f, AresLook(), "CHR_ARES", "Ares", kWar);
		SpawnStoryNpc(scene, 2.8f, 1.0f, 1.2f, 0.2f, 0.5f, 0.55f, PoseidonLook(), "CHR_POSEIDON", "Poseidon", kWar);
		SpawnStoryNpc(scene, 3.6f, -0.4f, 1.15f, 0.16f, 0.12f, 0.2f, HadesLook(), "CHR_HADES", "Hades", kWar);
		SpawnStoryNpc(scene, 0.4f, 2.6f, 1.05f, 0.95f, 0.6f, 0.7f, AphroditeLook(), "CHR_APHRODITE", "Aphrodite", kWar);
		SpawnStoryNpc(scene, 2.0f, 3.2f, 1.f, 0.55f, 0.72f, 0.92f, HermesLook(), "CHR_HERMES", "Hermes", kWar);

		// 마지막 전선: 왕좌의 홀 앞 청동 전사들(MQ_04.FRONT3).
		const float kBronze[3][2] = { { 3.0f, 4.0f }, { 5.4f, 1.6f }, { 1.0f, 5.6f } };
		for (int i = 0; i < 3; ++i)
		{
			AnimalActor* warrior = SpawnAnimal(scene, tileMap, AnimalKind::BronzeWarrior, kBronze[i][0], kBronze[i][1], 1.1f * (float)i);
			warrior->SetStoryId("MON_BRONZE_SUMMIT");
			warrior->SetPresence("STORY_STEP == MQ_04.FRONT3");
		}

		// ---- 정상 끝의 균열(피티아의 희생 — 유머 금지) ----
		CrackActor* crack = scene.Spawn<CrackActor>(kCrack.x, kCrack.y, 3.0f);
		crack->SetPresence("STORY_STEP >= MQ_04.DECLARE");
		SpawnTrigger(scene, kCrack.x - 1.0f, kCrack.y - 1.0f, 1.3f, "TRIGGER_SUMMIT_CRACK");

		// ---- 지하 베틀실: 좁은 길을 막은 바위(피티아가 길을 열면 사라짐) → 베틀과 잠든 모이라이 ----
		SpawnTrigger(scene, kGate.x + 1.2f, kGate.y + 1.2f, 1.2f, "TRIGGER_LOOM_GATE");
		for (int k = -2; k <= 2; ++k)
		{
			RockActor* boulder = scene.Spawn<RockActor>(kGate.x + 0.5f * (float)k, kGate.y - 0.5f * (float)k, 1.2f, (k + 2) % 3, true);
			boulder->SetPresence("STORY_STEP < MQ_04.LOOM");
		}
		SpawnTrigger(scene, kLoomRoom.x + 1.8f, kLoomRoom.y + 1.8f, 1.0f, "TRIGGER_LOOM_ROOM");
		MakeInspectable(scene.Spawn<LoomActor>(kLoomRoom.x - 0.4f, kLoomRoom.y - 0.4f), "OBJ_LOOM", nullptr);
		const char* kMoirai = "STORY_STEP >= MQ_04.LOOM";
		SpawnStoryNpc(scene, kLoomRoom.x + 0.8f, kLoomRoom.y - 1.4f, 0.95f, 0.88f, 0.86f, 0.8f, MoiraLook(0), "CHR_MOIRAI", "Clotho", kMoirai);
		SpawnStoryNpc(scene, kLoomRoom.x + 1.2f, kLoomRoom.y + 0.2f, 0.95f, 0.84f, 0.82f, 0.76f, MoiraLook(1), "CHR_MOIRAI", "Lachesis", kMoirai);
		SpawnStoryNpc(scene, kLoomRoom.x - 1.4f, kLoomRoom.y + 0.8f, 0.95f, 0.8f, 0.78f, 0.74f, MoiraLook(2), "CHR_MOIRAI", "Atropos", kMoirai);

		SpawnSignpost(scene, kSignpost.x, kSignpost.y);

		std::vector<Spot> avoid =
		{
			kArrival, kSignpost, kHall, kCrack, kLoomRoom, kGate, { 5.2f, -5.6f }, { 7.4f, -3.4f }, { -6.0f, 5.6f }, { -3.6f, 7.4f },
		};

		FinishRegion(scene, tileMap, rng, avoid, 8, 0.9f, 0.7f, true);
	}

	// =====================================================================================
	// LOC_TARTARUS_RIM — 타르타로스 경계 (MQ_05: 가족의 공동전선, 크로노스의 잔향, 에리스, SQ_TARTARUS_01 사슬의 티탄)
	// 잿빛 흙의 벌판, 화면 위쪽을 가득 채운 심연(바위 낭떠러지)과 그 가장자리의 보랏빛 균열들, 가운데 돌바닥에
	// 모인 신들, 오른쪽 위 바위에 사슬로 묶인 프로메테우스, 오른쪽 아래 이정표.
	// =====================================================================================
	void BuildTartarusRim(SceneGraph& scene, TileMap& tileMap, std::mt19937& rng, int seed)
	{
		const Spot kArrival = { 9.6f, 9.6f };
		const Spot kSignpost = { 10.6f, 10.6f };
		const Spot kFront = { 2.0f, 2.0f };
		const Spot kPrometheus = { 9.0f, -6.4f };

		// ---- 지형: 심연(바위)과 잿빛 벌판 ----
		Terrain::Fill(tileMap, TileType::Path);
		Terrain::PaintWhere(tileMap, [seed](float x, float y)
		{
			return x + y < -6.f + Terrain::Noise(x * 0.22f, y * 0.22f, seed) * 4.f - 2.f;
		}, TileType::Rock);
		Terrain::PaintDisk(tileMap, kFront.x, kFront.y, 3.6f, TileType::Stone);
		Terrain::PaintDisk(tileMap, kPrometheus.x - 1.2f, kPrometheus.y - 1.2f, 1.4f, TileType::Rock);

		Terrain::EnsureFullyConnected(tileMap, kArrival.x, kArrival.y);

		// 심연 가장자리의 균열들(늘 보랏빛으로 숨 쉼).
		scene.Spawn<CrackActor>(-2.6f, -0.2f, 2.6f);
		scene.Spawn<CrackActor>(0.4f, -3.2f, 2.2f);
		scene.Spawn<CrackActor>(-4.8f, 2.4f, 2.0f);

		// ---- 가족의 공동전선: 서로 싸우던 신들이 모였다(에리스와의 대화가 끝날 때까지) ----
		const char* kFrontLine = "STORY_STEP >= MQ_05.FRONT && STORY_STEP <= MQ_05.ERIS";
		SpawnStoryNpc(scene, 0.6f, 1.0f, 1.25f, 1.02f, 1.f, 0.94f, ZeusLook(), "CHR_ZEUS", "Zeus", kFrontLine);
		SpawnStoryNpc(scene, 1.0f, 3.6f, 1.15f, 0.52f, 0.28f, 0.62f, HeraLook(), "CHR_HERA", "Hera", kFrontLine);
		SpawnStoryNpc(scene, 3.2f, 0.4f, 1.2f, 0.2f, 0.5f, 0.55f, PoseidonLook(), "CHR_POSEIDON", "Poseidon", kFrontLine);
		SpawnStoryNpc(scene, 4.0f, 2.0f, 1.15f, 0.16f, 0.12f, 0.2f, HadesLook(), "CHR_HADES", "Hades", kFrontLine);
		SpawnStoryNpc(scene, -0.4f, 2.4f, 1.1f, 0.55f, 0.6f, 0.7f, AthenaLook(), "CHR_ATHENA", "Athena", kFrontLine);
		SpawnStoryNpc(scene, -0.2f, 4.0f, 1.3f, 0.6f, 0.15f, 0.12f, AresLook(), "CHR_ARES", "Ares", kFrontLine);
		SpawnStoryNpc(scene, 2.6f, 4.4f, 1.05f, 0.95f, 0.6f, 0.7f, AphroditeLook(), "CHR_APHRODITE", "Aphrodite", kFrontLine);
		SpawnStoryNpc(scene, 4.4f, 3.8f, 1.f, 0.55f, 0.72f, 0.92f, HermesLook(), "CHR_HERMES", "Hermes", kFrontLine);
		SpawnTrigger(scene, 4.2f, 5.4f, 1.6f, "TRIGGER_UNITED_FRONT");

		// ---- 심연에서 깨어나는 크로노스의 잔향(MQ_05.KRONOS) ----
		AnimalActor* kronos = SpawnAnimal(scene, tileMap, AnimalKind::KronosEcho, -1.2f, -1.2f, 0.2f);
		kronos->SetStoryId("MON_KRONOS_ECHO");
		kronos->SetPresence("STORY_STEP == MQ_05.KRONOS");

		// ---- 에리스와 그녀가 부른 불화의 그림자(MQ_05.ERIS_FIGHT → MQ_05.ERIS) ----
		SpawnStoryNpc(scene, -0.6f, 0.2f, 0.95f, 0.12f, 0.08f, 0.1f, ErisLook(), "CHR_ERIS", "Eris",
			"STORY_STEP == MQ_05.ERIS_FIGHT || STORY_STEP == MQ_05.ERIS");
		const float kShades[3][2] = { { 1.2f, -1.4f }, { -2.2f, 1.6f }, { 2.6f, -0.2f } };
		for (int i = 0; i < 3; ++i)
		{
			AnimalActor* shade = SpawnAnimal(scene, tileMap, AnimalKind::ShadowWolf, kShades[i][0], kShades[i][1], 0.9f * (float)i + 0.4f);
			shade->SetStoryId("MON_ERIS_SHADE");
			shade->SetPresence("STORY_STEP == MQ_05.ERIS_FIGHT");
		}

		// ---- 사슬의 티탄 프로메테우스(SQ_TARTARUS_01) ----
		scene.Spawn<RockActor>(kPrometheus.x - 0.9f, kPrometheus.y - 0.9f, 1.8f, 1, false);
		NpcActor* prometheus = SpawnStoryNpc(scene, kPrometheus.x, kPrometheus.y, 1.2f, 0.62f, 0.5f, 0.4f, PrometheusLook(), "CHR_PROMETHEUS",
			"Prometheus", nullptr);
		prometheus->SetFacing(kPi * 0.25f);

		SpawnSignpost(scene, kSignpost.x, kSignpost.y);

		std::vector<Spot> avoid = { kArrival, kSignpost, kFront, kPrometheus };
		FinishRegion(scene, tileMap, rng, avoid, 0, 0.f, 0.8f, false);
	}

	// =====================================================================================
	// LOC_THEBES — 테베 (새 마을: 델포이·올림포스 산기슭·아테네 사이의 갈림길, 디오니소스를 숭배하는 축제의 도시)
	// STORY.md 12장의 SQ_THEBES_01(포도덩굴의 밤)이 "아테네 인근 신규 구역"으로 적어 둔 곳. 그 퀘스트는 아직 없다(TODO).
	// 가운데 네거리 광장(디오니소스 신상·포도주 좌판·축제의 모닥불), 화면 위쪽에 디오니소스 신전, 화면 오른쪽 위에
	// 둥글게 기둥을 두른 노천극장, 화면 왼쪽 아래에 포도밭, 오른쪽 아래 구석에 카드모스의 바위 언덕.
	// 길: 위(북서)로 델포이, 왼쪽(북동)으로 올림포스 산기슭, 아래(남동)로 아테네.
	// =====================================================================================
	void BuildThebes(SceneGraph& scene, TileMap& tileMap, std::mt19937& rng, int seed)
	{
		const Spot kPlaza = { 0.f, 0.f };
		const Spot kSignpost = { 3.6f, 3.6f };
		const Spot kTemple = { -9.5f, -9.5f };
		const Spot kTheater = { 9.5f, -8.5f };
		const Spot kVineyard = { -9.f, 9.f };

		// ---- 지형 ----
		Terrain::Fill(tileMap, TileType::Grass);
		Terrain::PaintDisk(tileMap, 15.5f, 15.5f, 3.6f, TileType::Rock); // 카드모스의 바위 언덕
		Terrain::PaintDisk(tileMap, kPlaza.x, kPlaza.y, 5.6f, TileType::Stone);
		Terrain::PaintDisk(tileMap, kTemple.x, kTemple.y, 4.2f, TileType::Stone);
		Terrain::PaintLine(tileMap, kPlaza.x - 3.4f, kPlaza.y - 3.4f, kTemple.x + 2.4f, kTemple.y + 2.4f, 0.65f, TileType::Path);
		Terrain::PaintLine(tileMap, kPlaza.x + 4.2f, kPlaza.y - 2.6f, kTheater.x - 2.2f, kTheater.y + 2.2f, 0.6f, TileType::Path);
		Terrain::PaintLine(tileMap, kPlaza.x - 3.0f, kPlaza.y + 3.6f, kVineyard.x + 2.6f, kVineyard.y - 2.6f, 0.55f, TileType::Path);
		Terrain::PaintDisk(tileMap, -6.5f, -14.5f, 1.4f, TileType::Water); // 디르케의 샘

		Terrain::EnsureFullyConnected(tileMap, kPlaza.x + 2.6f, kPlaza.y + 2.6f);

		// ---- 광장: 디오니소스 신상, 축제의 모닥불, 포도주 좌판 ----
		scene.Spawn<StatueActor>(kPlaza.x - 0.8f, kPlaza.y - 0.8f, 1.2f, false);
		scene.Spawn<FireActor>(kPlaza.x + 2.4f, kPlaza.y - 2.6f, 0.f, 0.55f, true);
		scene.Spawn<FireActor>(kPlaza.x - 2.8f, kPlaza.y + 2.2f, 0.f, 0.55f, true);
		scene.Spawn<StallActor>(kPlaza.x + 3.2f, kPlaza.y + 0.6f, 0.5f, 0.12f, 0.36f);
		scene.Spawn<StallActor>(kPlaza.x - 1.2f, kPlaza.y + 3.6f, 0.62f, 0.18f, 0.4f);
		SpawnSignpost(scene, kSignpost.x, kSignpost.y);

		// ---- 디오니소스 신전 ----
		scene.Spawn<TempleActor>(kTemple.x, kTemple.y, 5.f, 3.4f, 1.6f, false);
		scene.Spawn<FireActor>(kTemple.x + 3.4f, kTemple.y + 1.0f, 0.f, 0.5f, true);

		// ---- 노천극장: 둥근 무대 + 반달 모양으로 늘어선 기둥(관객석 쪽) ----
		scene.Spawn<FloorDiscActor>(kTheater.x, kTheater.y, 3.0f, 0.78f, 0.74f, 0.68f);
		for (int i = 0; i < 7; ++i)
		{
			float angle = kPi * (1.05f + 0.15f * (float)i);
			scene.Spawn<ColumnActor>(kTheater.x + cosf(angle) * 3.6f, kTheater.y + sinf(angle) * 3.6f, 2.0f, i == 2);
		}

		// ---- 포도밭: 낮은 덩굴 줄(보랏빛 열매가 섞인 짙은 잎) ----
		std::vector<Spot> avoid = { kPlaza, kSignpost, kTemple, kTheater, kVineyard, { kTemple.x + 3.4f, kTemple.y + 1.0f } };
		for (int row = 0; row < 3; ++row)
		{
			for (int column = 0; column < 4; ++column)
			{
				float x = kVineyard.x - 3.f + (float)column * 2.f;
				float y = kVineyard.y - 2.f + (float)row * 2.f;
				scene.Spawn<TreeActor>(x, y, 0.7f, 0.3f, 0.2f, 0.32f, TreeKind::Broadleaf);
				avoid.push_back(Spot{ x, y });
			}
		}

		// ---- 처음부터 있는 살림집 셋(나머지는 FinishRegion이 마을 둘레에 더 짓는다) ----
		SpawnHouse(scene, 6.5f, 4.5f, 1.35f, true);
		SpawnHouse(scene, -4.5f, 6.5f, 1.3f, false);
		SpawnHouse(scene, 6.0f, -4.5f, 1.3f, true);
		avoid.push_back(Spot{ 6.5f, 4.5f });
		avoid.push_back(Spot{ -4.5f, 6.5f });
		avoid.push_back(Spot{ 6.0f, -4.5f });

		SpawnHerb(scene, tileMap, -14.f, 3.f);
		SpawnHerb(scene, tileMap, 12.f, -14.f);

		FinishRegion(scene, tileMap, rng, avoid, 20, 0.5f, 0.55f, false);
	}

	// =====================================================================================
	// LOC_KRONOS_BELLY — 크로노스의 배 속 (MQ_05 2페이즈 「아버지의 배 속」 — V-01, 유머 없음)
	// 어둡고 축축한 굽은 굴. 벽을 따라 그가 수천 년 전 삼켰던 자식들의 이름이 새겨진 돌(손톱자국)이 늘어서 있고,
	// 굴 끝에 빛이 새는 곳이 있다. 이정표는 없다(이야기로만 들어오고 나간다). 수호신이 곁에서 함께 걷는다.
	// =====================================================================================
	LevelActors BuildKronosBelly(SceneGraph& scene, TileMap& tileMap, Renderer& renderer, std::mt19937& rng, int seed)
	{
		const Spot kStart = { -6.2f, 6.2f };
		const Spot kLight = { 6.2f, -6.2f };

		// ---- 지형: 바위 속을 구불구불 판 굴 ----
		Terrain::Fill(tileMap, TileType::Rock);
		Terrain::PaintLine(tileMap, -6.f, 6.f, -2.f, 2.f, 1.4f, TileType::Stone);
		Terrain::PaintLine(tileMap, -2.f, 2.f, 2.f, 4.f, 1.4f, TileType::Stone);
		Terrain::PaintLine(tileMap, 2.f, 4.f, 4.f, -1.f, 1.4f, TileType::Stone);
		Terrain::PaintLine(tileMap, 4.f, -1.f, 6.f, -6.f, 1.4f, TileType::Stone);
		Terrain::PaintDisk(tileMap, kStart.x, kStart.y, 2.0f, TileType::Stone);
		Terrain::PaintDisk(tileMap, kLight.x, kLight.y, 2.0f, TileType::Stone);
		Terrain::PaintDisk(tileMap, 0.8f, 3.4f, 0.6f, TileType::Water); // 바닥에 고인 물

		Terrain::EnsureFullyConnected(tileMap, kStart.x, kStart.y);
		SpawnTileActors(scene, renderer, tileMap, seed);

		// ---- 벽의 이름들(손톱자국으로 새긴): 헤스티아·데메테르·헤라·하데스·포세이돈 ----
		const float kWet[3] = { 0.3f, 0.27f, 0.3f };
		MakeInspectable(scene.Spawn<SteleActor>(-3.9f, 5.3f, 1.2f, kWet[0], kWet[1], kWet[2], false), "OBJ_NAME_HESTIA", nullptr);
		MakeInspectable(scene.Spawn<SteleActor>(-3.5f, 2.1f, 1.2f, kWet[0], kWet[1], kWet[2], false), "OBJ_NAME_DEMETER", nullptr);
		MakeInspectable(scene.Spawn<SteleActor>(-0.45f, 3.9f, 1.2f, kWet[0], kWet[1], kWet[2], false), "OBJ_NAME_HERA", nullptr);
		MakeInspectable(scene.Spawn<SteleActor>(3.8f, 2.1f, 1.2f, kWet[0], kWet[1], kWet[2], false), "OBJ_NAME_HADES", nullptr);
		MakeInspectable(scene.Spawn<SteleActor>(4.1f, -3.9f, 1.2f, kWet[0], kWet[1], kWet[2], false), "OBJ_NAME_POSEIDON", nullptr);

		// ---- 굴 끝의 빛(목구멍 위로 새어 드는 바깥의 빛) ----
		scene.Spawn<GlowActor>(kLight.x, kLight.y, 1.2f, 1.6f, 1.6f, 1.3f, 0.8f);
		SpawnTrigger(scene, kLight.x, kLight.y, 1.4f, "TRIGGER_BELLY_LIGHT");

		std::vector<Spot> avoid = { kStart, kLight };
		LevelActors actors = FinishStage(scene, tileMap, rng, avoid, 0, 0.f, 0.9f, false, kStart);

		// 함께 삼켜진 수호신이 곁에서 걷는다(PATRON에 맞는 하나만 보인다).
		const PatronPresence kCompanion =
		{
			"PATRON == ZEUS",
			"PATRON == POSEIDON",
			"PATRON == HADES",
			"PATRON == APHRODITE",
			"PATRON == HERMES",
		};
		SpawnPatrons(scene, kStart.x + 1.0f, kStart.y - 1.0f, kCompanion, actors.player);
		return actors;
	}

	// =====================================================================================
	// 하나로 이어진 월드: 지역을 월드 위에 앉히고, 그 사이를 황야와 길로 채운다(WorldLayout.h의 배치표).
	// =====================================================================================

	// 새 게임을 시작하는 자리(델포이 로컬 좌표: 광장 아래 시장 길).
	const float kNewGameStartX = 2.6f;
	const float kNewGameStartY = 5.0f;

	// 움직이지 않는 액터를 묶는 칸(그룹 노드)의 한 변.
	const float kCellSize = 24.f;

	// 길의 반폭(흙길 — 바다 위에선 둑길·모래톱 길).
	const float kRoadHalfWidth = 0.85f;

	// 마을 정사각형의 가장자리 이만큼(칸)은 둘레 황야와 섞는다(바다·바위가 자로 그은 듯 끊기지 않게).
	const float kBlendBand = 4.f;

	// 성역을 두르는 바위 띠의 두께(칸). 길이 난 곳만 뚫린다.
	const float kSanctuaryRing = 4.f;

	bool IsPlace(const WorldPlace* place, const char* locId)
	{
		return place != nullptr && strcmp(place->locId, locId) == 0;
	}

	// 지역 하나를 지역 크기의 로컬 타일맵에 짓는다(지형·건축물·인물 — 바닥 배치는 월드를 다 지은 뒤 한 번에).
	void BuildPlace(const WorldPlace& place, SceneGraph& scene, TileMap& tileMap, std::mt19937& rng, int seed)
	{
		if (IsPlace(&place, "LOC_DELPHI"))
		{
			BuildDelphi(scene, tileMap, rng, seed);
		}
		else if (IsPlace(&place, "LOC_THEBES"))
		{
			BuildThebes(scene, tileMap, rng, seed);
		}
		else if (IsPlace(&place, "LOC_OLYMPUS_FOOT"))
		{
			BuildOlympusFoot(scene, tileMap, rng, seed);
		}
		else if (IsPlace(&place, "LOC_ATHENS"))
		{
			BuildAthens(scene, tileMap, rng, seed);
		}
		else if (IsPlace(&place, "LOC_AEGEAN"))
		{
			BuildAegean(scene, tileMap, rng, seed);
		}
		else if (IsPlace(&place, "LOC_CYPRUS"))
		{
			BuildCyprus(scene, tileMap, rng, seed);
		}
		else if (IsPlace(&place, "LOC_TAENARON"))
		{
			BuildTaenaron(scene, tileMap, rng, seed);
		}
		else if (IsPlace(&place, "LOC_WEST"))
		{
			BuildWest(scene, tileMap, rng, seed);
		}
		else if (IsPlace(&place, "LOC_OLYMPUS"))
		{
			BuildOlympus(scene, tileMap, rng, seed);
		}
		else if (IsPlace(&place, "LOC_TARTARUS_RIM"))
		{
			BuildTartarusRim(scene, tileMap, rng, seed);
		}
	}

	// 월드에 놓이는 지역(무대 제외) 중 중심이 가장 가까운 것 — 황야의 땅 모양(바다·산줄기·잿빛 벌판·들판)을 정한다.
	const WorldPlace* NearestPlace(float x, float y)
	{
		const WorldPlace* nearest = nullptr;
		float nearestSq = 0.f;

		for (int i = 0; i < WorldLayout::GetPlaceCount(); ++i)
		{
			const WorldPlace& place = WorldLayout::GetPlace(i);
			if (place.kind == PlaceKind::Stage)
			{
				continue;
			}

			float dx = x - place.centerX;
			float dy = y - place.centerY;
			if (nearest == nullptr || dx * dx + dy * dy < nearestSq)
			{
				nearest = &place;
				nearestSq = dx * dx + dy * dy;
			}
		}
		return nearest;
	}

	float DistanceToRoads(float x, float y)
	{
		float best = 1.0e9f;
		for (const WorldRoad& road : WorldLayout::GetRoads())
		{
			for (int i = 0; i + 1 < 5; ++i)
			{
				float distance = SegmentDistance(x, y, road.points[i].x, road.points[i].y, road.points[i + 1].x, road.points[i + 1].y);
				best = (std::min)(best, distance);
			}
		}
		return best;
	}

	// 황야의 땅: 가장 가까운 지역에 따라
	//  - 에게해·키프로스 쪽, 남동쪽 끝, 델포이와 서쪽 해안 사이의 해협: 바다(길을 따라 이어진 모래톱·작은 섬만 땅)
	//  - 올림포스 정상 쪽: 거의 바위인 산줄기
	//  - 타이나론·타르타로스 쪽: 잿빛 흙의 벌판과 바위
	//  - 그 밖: 풀밭(군데군데 바위 언덕과 연못)
	// 월드 가장자리는 바다라서 맵 밖의 먼바다로 이어진다. 지역 정사각형 안도 일단 칠해 두고, 지역을 앉힐 때 덮는다.
	void PaintWilderness(TileMap& world, int seed)
	{
		float halfWidth = world.GetWidth() * 0.5f;
		float halfHeight = world.GetHeight() * 0.5f;

		for (int gy = 0; gy < world.GetHeight(); ++gy)
		{
			for (int gx = 0; gx < world.GetWidth(); ++gx)
			{
				float x = world.GetWorldX(gx);
				float y = world.GetWorldY(gy);
				const WorldPlace* nearest = NearestPlace(x, y);
				float detail = Terrain::Noise(x * 0.08f, y * 0.08f, seed + 31);
				TileType type = TileType::Grass;

				bool sea = IsPlace(nearest, "LOC_AEGEAN") || IsPlace(nearest, "LOC_CYPRUS") || (y > 95.f && x > -40.f)
					|| (x > 36.f && x < 54.f && y < -95.f);
				if (sea)
				{
					float shore = 3.f + 6.f * Terrain::Noise(x * 0.11f, y * 0.11f, seed + 21);
					type = (DistanceToRoads(x, y) < shore) ? TileType::Grass : TileType::Water;
				}
				else if (IsPlace(nearest, "LOC_OLYMPUS"))
				{
					type = (detail > 0.2f) ? TileType::Rock : TileType::Grass;
				}
				else if (IsPlace(nearest, "LOC_TAENARON") || IsPlace(nearest, "LOC_TARTARUS_RIM"))
				{
					type = (detail > 0.74f) ? TileType::Rock : TileType::Path;
				}
				else if (detail > 0.78f)
				{
					type = TileType::Rock;
				}
				else if (Terrain::Noise(x * 0.1f + 50.f, y * 0.1f, seed + 41) > 0.84f)
				{
					type = TileType::Water;
				}

				float edge = (std::min)(halfWidth - fabsf(x), halfHeight - fabsf(y));
				if (edge < 5.f + 4.f * Terrain::Noise(x * 0.15f, y * 0.15f, seed + 61))
				{
					type = TileType::Water;
				}

				world.SetTile(gx, gy, type);
			}
		}
	}

	// 지역의 로컬 타일맵을 월드의 그 자리에 옮겨 칠한다. 마을 가장자리 띠에선 지역의 바다·바위가 둘레 황야의 땅과
	// 들쭉날쭉 섞이게 한다(지역 안의 걸을 수 있는 땅은 늘 그대로 — 그 위에 놓인 것들이 물에 빠지지 않게).
	void PlaceRegionTiles(TileMap& world, const TileMap& local, const WorldPlace& place, int seed)
	{
		float half = place.size * 0.5f;

		for (int gy = 0; gy < local.GetHeight(); ++gy)
		{
			for (int gx = 0; gx < local.GetWidth(); ++gx)
			{
				float localX = local.GetWorldX(gx);
				float localY = local.GetWorldY(gy);
				float x = localX + place.centerX;
				float y = localY + place.centerY;
				int worldX = world.GetGridX(x);
				int worldY = world.GetGridY(y);
				if (worldX < 0 || worldX >= world.GetWidth() || worldY < 0 || worldY >= world.GetHeight())
				{
					continue;
				}

				TileType tile = local.GetTile(gx, gy);
				float border = half - (std::max)(fabsf(localX), fabsf(localY));
				bool blocked = (tile == TileType::Water || tile == TileType::Rock);

				if (place.kind == PlaceKind::Village && blocked && border < kBlendBand && world.IsWalkable(worldX, worldY))
				{
					float keep = border / kBlendBand;
					if (Terrain::Noise(x * 0.35f, y * 0.35f, seed + 77) > keep)
					{
						continue; // 둘레 황야의 땅을 남긴다
					}
				}

				world.SetTile(worldX, worldY, tile);
			}
		}
	}

	// 성역(올림포스 정상·타르타로스 경계)을 바위 띠로 두른다 — 길이 난 곳만 나중에 뚫린다(PaintRoads).
	void PaintSanctuaryRing(TileMap& world, const WorldPlace& place)
	{
		float half = place.size * 0.5f;
		Terrain::PaintWhere(world, [&place, half](float x, float y)
		{
			float reach = (std::max)(fabsf(x - place.centerX), fabsf(y - place.centerY));
			return reach > half && reach <= half + kSanctuaryRing;
		}, TileType::Rock);
	}

	// 이웃 지역 사이에 길을 깐다(지역 안 출구 → 황야 → 이웃의 출구). 물 위로는 둑길, 바위는 깎아 낸 산길이 된다.
	void PaintRoads(TileMap& world)
	{
		for (const WorldRoad& road : WorldLayout::GetRoads())
		{
			for (int i = 0; i + 1 < 5; ++i)
			{
				Terrain::PaintLine(world, road.points[i].x, road.points[i].y, road.points[i + 1].x, road.points[i + 1].y,
					kRoadHalfWidth, TileType::Path);
			}
		}
	}

	// 시작 자리에서 걸어서 닿지 않는 땅(황야의 노이즈가 만든 외딴 섬·바위에 갇힌 풀밭)을 둘레에 맞춰 바위나 물로 메운다.
	// 지역은 저마다 안쪽이 이어져 있고 길로 서로 이어져 있으므로, 남는 건 아무도 갈 수 없는 황야 조각뿐이다.
	void RemoveIsolatedPockets(TileMap& world, float startX, float startY)
	{
		int width = world.GetWidth();
		int height = world.GetHeight();
		int startGridX = world.GetGridX(startX);
		int startGridY = world.GetGridY(startY);
		if (!world.IsWalkable(startGridX, startGridY))
		{
			return;
		}

		std::vector<char> reached((size_t)(width * height), 0);
		std::vector<int> queue;
		queue.push_back(startGridY * width + startGridX);
		reached[(size_t)(startGridY * width + startGridX)] = 1;

		const int kSteps[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
		for (size_t head = 0; head < queue.size(); ++head)
		{
			int gx = queue[head] % width;
			int gy = queue[head] / width;

			for (const int* step : kSteps)
			{
				int nx = gx + step[0];
				int ny = gy + step[1];
				if (world.IsWalkable(nx, ny) && !reached[(size_t)(ny * width + nx)])
				{
					reached[(size_t)(ny * width + nx)] = 1;
					queue.push_back(ny * width + nx);
				}
			}
		}

		for (int gy = 0; gy < height; ++gy)
		{
			for (int gx = 0; gx < width; ++gx)
			{
				if (!world.IsWalkable(gx, gy) || reached[(size_t)(gy * width + gx)])
				{
					continue;
				}

				bool nearRock = world.GetTile(gx + 1, gy) == TileType::Rock || world.GetTile(gx - 1, gy) == TileType::Rock
					|| world.GetTile(gx, gy + 1) == TileType::Rock || world.GetTile(gx, gy - 1) == TileType::Rock;
				world.SetTile(gx, gy, nearRock ? TileType::Rock : TileType::Water);
			}
		}
	}

	// 황야의 바위(산줄기·바위 언덕 가장자리에 촘촘히, 안쪽엔 드문드문 — 올림포스 쪽은 눈 덮인 바위)와 숲(노이즈로 정한
	// 숲 짙은 곳에 많이, 들판엔 드문드문 — 길 바로 곁과 물가는 비운다).
	void DecorateWilderness(SceneGraph& scene, const TileMap& world, std::mt19937& rng, int seed)
	{
		std::uniform_real_distribution<float> unit(0.f, 1.f);

		for (int gy = 0; gy < world.GetHeight(); ++gy)
		{
			for (int gx = 0; gx < world.GetWidth(); ++gx)
			{
				if (world.GetTile(gx, gy) != TileType::Rock)
				{
					continue;
				}

				float x = world.GetWorldX(gx);
				float y = world.GetWorldY(gy);
				if (WorldLayout::Containing(x, y) != nullptr)
				{
					continue; // 지역 안의 바위는 지역을 지을 때 놓았다
				}

				bool edge = world.IsWalkable(gx + 1, gy) || world.IsWalkable(gx - 1, gy) || world.IsWalkable(gx, gy + 1) || world.IsWalkable(gx, gy - 1);
				if (unit(rng) >= (edge ? 0.42f : 0.025f))
				{
					continue;
				}

				bool snowy = IsPlace(NearestPlace(x, y), "LOC_OLYMPUS");
				scene.Spawn<RockActor>(x + (unit(rng) - 0.5f) * 0.5f, y + (unit(rng) - 0.5f) * 0.5f, 0.9f + unit(rng) * 0.6f,
					gx * 131 + gy * 17, snowy);
			}
		}

		const float kStep = 1.8f;
		float halfWidth = world.GetWidth() * 0.5f - 1.f;
		float halfHeight = world.GetHeight() * 0.5f - 1.f;
		for (float baseY = -halfHeight; baseY < halfHeight; baseY += kStep)
		{
			for (float baseX = -halfWidth; baseX < halfWidth; baseX += kStep)
			{
				float x = baseX + (unit(rng) - 0.5f) * 1.2f;
				float y = baseY + (unit(rng) - 0.5f) * 1.2f;
				int gx = world.GetGridX(x);
				int gy = world.GetGridY(y);
				if (world.GetTile(gx, gy) != TileType::Grass || WorldLayout::Containing(x, y) != nullptr)
				{
					continue;
				}

				float forest = Terrain::Noise(x * 0.045f, y * 0.045f, seed + 51);
				float chance = (std::max)(0.02f, (std::min)(0.8f, (forest - 0.42f) * 2.4f));
				if (unit(rng) >= chance || !IsOpenGround(world, x, y, 0.8f))
				{
					continue;
				}

				bool nearPath = world.GetTile(world.GetGridX(x + 1.2f), gy) == TileType::Path || world.GetTile(world.GetGridX(x - 1.2f), gy) == TileType::Path
					|| world.GetTile(gx, world.GetGridY(y + 1.2f)) == TileType::Path || world.GetTile(gx, world.GetGridY(y - 1.2f)) == TileType::Path;
				if (nearPath)
				{
					continue;
				}

				SpawnRandomTree(scene, rng, x, y, 0.35f);
			}
		}
	}

	// 지역 경계의 안개 장벽(아직 이야기가 열지 않은 성역, 프롤로그 동안의 델포이)과 마을마다 그 신의 빛깔로 떠도는 빛 알갱이.
	void SpawnGatesAndMotes(SceneGraph& scene)
	{
		for (int i = 0; i < WorldLayout::GetPlaceCount(); ++i)
		{
			const WorldPlace& place = WorldLayout::GetPlace(i);
			if (place.kind == PlaceKind::Stage)
			{
				continue;
			}

			float half = place.size * 0.5f;
			RegionGateActor* gate = nullptr;
			if (IsPlace(&place, "LOC_OLYMPUS"))
			{
				gate = scene.Spawn<RegionGateActor>(place.centerX, place.centerY, half,
					"[길이 막혔다] 산길을 구름이 막고 있다 — 올림포스 정상은 아직 오를 때가 아니다(이야기가 진행되면 길이 열린다).");
				gate->SetPresence("!UNLOCK_LOC_OLYMPUS");
			}
			else if (IsPlace(&place, "LOC_TARTARUS_RIM"))
			{
				gate = scene.Spawn<RegionGateActor>(place.centerX, place.centerY, half,
					"[길이 막혔다] 심연에서 올라오는 보랏빛 안개가 내리막길을 막는다 — 아직 내려갈 때가 아니다.");
				gate->SetPresence("!UNLOCK_LOC_TARTARUS_RIM");
			}
			else if (IsPlace(&place, "LOC_DELPHI"))
			{
				gate = scene.Spawn<RegionGateActor>(place.centerX, place.centerY, half,
					"[아직 떠날 수 없다] 오늘은 신탁 축제 날이다 — 델포이를 떠나기 전에 할 일이 남았다.");
				gate->SetPresence("STORY_STEP < MQ_01.INTERVIEW");
			}

			const VillageMood& mood = place.mood;
			int count = (place.kind == PlaceKind::Village) ? 32 : 22;
			scene.Spawn<AmbientMotesActor>(place.centerX, place.centerY, half - 2.f, half - 2.f, count, mood.moteR, mood.moteG, mood.moteB);
		}
	}

	// 움직이지 않는 액터(바닥 청크·나무·건물·바위·물건·불·바닥 표시·불씨)인지 — 월드를 다 지은 뒤 칸으로 묶는다.
	// 캐릭터, 조준 링, 지역 경계의 안개 장벽(지역 전체를 덮어 칸에 넣으면 칸의 경계가 부풀어 오름), 빛 알갱이, 섬 둘레
	// 바다(맵 전체를 덮음)는 루트에 그대로 둔다.
	bool IsStaticActor(const Actor& actor)
	{
		switch (actor.GetType())
		{
		case ActorType::TileBatch:
			return actor.GetWorldBounds().radius < kCellSize;
		case ActorType::Prop:
		case ActorType::Building:
		case ActorType::Rock:
		case ActorType::Item:
		case ActorType::Fire:
			return true;
		case ActorType::Marker:
			return dynamic_cast<const RingActor*>(&actor) == nullptr && dynamic_cast<const RegionGateActor*>(&actor) == nullptr;
		case ActorType::Effect:
			return dynamic_cast<const GlowActor*>(&actor) != nullptr;
		default:
			return false;
		}
	}
}

LoadedMap WorldMaps::BuildOverworld(Renderer& renderer, unsigned int sessionSeed)
{
	LoadedMap result;
	result.locId = "LOC_DELPHI";
	result.tileMap = new TileMap(WorldLayout::kWorldWidth, WorldLayout::kWorldHeight);
	result.scene = new SceneGraph();

	TileMap& world = *result.tileMap;
	SceneGraph& scene = *result.scene;

	unsigned int worldSeed = sessionSeed ^ HashString("LOC_WORLD");
	std::mt19937 worldRng(worldSeed);
	int seed = (int)(worldSeed & 0x7FFFFFFFu);

	PaintWilderness(world, seed);

	// 지역을 하나씩 지어 월드에 앉힌다. 지역을 짓는 코드는 예전처럼 지역 가운데를 원점으로 한 좌표를 쓰고, 씬의 생성
	// 문맥이 액터를 월드 자리로 옮기며 그 지역 소속으로 표시한다. 실행마다 다른 세계, 같은 실행 안에선 지역마다 같은 시드.
	for (int i = 0; i < WorldLayout::GetPlaceCount(); ++i)
	{
		const WorldPlace& place = WorldLayout::GetPlace(i);
		if (place.kind == PlaceKind::Stage)
		{
			continue;
		}

		unsigned int placeSeed = sessionSeed ^ HashString(place.locId);
		std::mt19937 rng(placeSeed);
		TileMap local(place.size, place.size);

		g_BuildPlace = &place;
		g_BuildHouses.clear();
		scene.SetSpawnContext(place.locId, place.centerX, place.centerY);
		BuildPlace(place, scene, local, rng, (int)(placeSeed & 0x7FFFFFFFu));
		scene.ClearSpawnContext();
		g_BuildPlace = nullptr;
		g_BuildHouses.clear();

		PlaceRegionTiles(world, local, place, seed);
		if (place.kind == PlaceKind::Sanctuary)
		{
			PaintSanctuaryRing(world, place);
		}
	}

	PaintRoads(world);

	WorldPoint start = WorldLayout::ToWorld(*WorldLayout::Find("LOC_DELPHI"), kNewGameStartX, kNewGameStartY);
	RemoveIsolatedPockets(world, start.x, start.y);

	SpawnTileActors(scene, renderer, world, seed);
	DecorateWilderness(scene, world, worldRng, seed);
	SpawnGatesAndMotes(scene);

	// 플레이어(델포이 시장 길)와 동료 피티아(존재 조건이 참일 때만 나타나 따라다닌다).
	LevelActors actors = SpawnPlayerAndMarkers(scene, start.x, start.y);
	SpawnCompanions(scene, *actors.player);

	// 움직이지 않는 것들을 칸으로 묶는다 — 충돌·검색·컬링이 멀리 있는 칸을 통째로 건너뛰게.
	scene.GroupIntoCells(kCellSize, IsStaticActor);

	result.player = actors.player;
	result.attackRing = actors.attackRing;
	result.interactRing = actors.interactRing;
	return result;
}

LoadedMap WorldMaps::BuildStage(const std::string& locId, Renderer& renderer, unsigned int sessionSeed)
{
	// 무대는 지금 크로노스의 배 속 하나뿐이다.
	LoadedMap result;
	result.locId = "LOC_KRONOS_BELLY";
	const WorldPlace* place = WorldLayout::Find(result.locId);
	int size = (place != nullptr) ? place->size : 32;
	result.tileMap = new TileMap(size, size);
	result.scene = new SceneGraph();

	unsigned int mapSeed = sessionSeed ^ HashString(result.locId);
	std::mt19937 rng(mapSeed);
	int seed = (int)(mapSeed & 0x7FFFFFFFu);

	LevelActors actors = BuildKronosBelly(*result.scene, *result.tileMap, renderer, rng, seed);
	result.player = actors.player;
	result.attackRing = actors.attackRing;
	result.interactRing = actors.interactRing;
	return result;
}

bool WorldMaps::IsKnownLocation(const std::string& locId)
{
	return WorldLayout::Find(locId) != nullptr;
}
