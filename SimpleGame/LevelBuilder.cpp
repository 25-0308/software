#include "stdafx.h"
#include "LevelBuilder.h"

#include <cmath>
#include <random>
#include <vector>

#include "CharacterActors.h"
#include "Renderer.h"
#include "WorldActors.h"

namespace
{
	// 타일 하나(중심 cx,cy, 높이 z)를 배치 정점 목록에 사각형(삼각형 2개, 6정점)으로 추가한다.
	// Renderer::kTileBatchFloatsPerVertex(월드좌표3+로컬좌표2+색4=9) 형식에 맞춘다. 정점 순서와
	// 로컬(-0.5~0.5) 오프셋은 Renderer가 예전에 쓰던 단위 사각형과 동일하다.
	void AppendTileQuad(std::vector<float>& out, float cx, float cy, float z, float r, float g, float b, float a)
	{
		const float offsets[6][2] =
		{
			{ -0.5f, -0.5f }, { -0.5f, 0.5f }, { 0.5f, 0.5f },
			{ -0.5f, -0.5f }, { 0.5f, 0.5f }, { 0.5f, -0.5f },
		};

		for (int i = 0; i < 6; ++i)
		{
			float lx = offsets[i][0];
			float ly = offsets[i][1];
			out.push_back(cx + lx);
			out.push_back(cy + ly);
			out.push_back(z);
			out.push_back(lx);
			out.push_back(ly);
			out.push_back(r);
			out.push_back(g);
			out.push_back(b);
			out.push_back(a);
		}
	}

	// 약초(획득 아이템) 하나를 (x, y)에 생성한다.
	void SpawnHerb(SceneGraph& scene, float x, float y)
	{
		scene.Spawn<ItemActor>(x, y, 0.4f, 0.4f, 1.6f, 0.5f, kInteractLoot);
	}

	// 마을 사람 NPC 하나를 (x, y)에 생성한다.
	void SpawnVillager(SceneGraph& scene, float x, float y, float r, float g, float b)
	{
		scene.Spawn<NpcActor>(x, y, 0.9f, r, g, b, kInteractVillager, "Villager");
	}

	// 건물(큐브) 하나와, 그 건물에 붙어 다니는 횃불(자식 액터)을 생성한다. 횃불 위치는 건물 기준
	// 로컬 좌표라 건물을 옮기면 같이 따라간다. 카메라 쪽(+y면, 화면 왼쪽 면) 문 옆 바닥 근처에 둔다 —
	// 반대편에 두면 큐브에 가려서 불꽃만 허공에 떠 보인다.
	void SpawnBuildingWithTorch(SceneGraph& scene, float x, float y)
	{
		BuildingActor* building = scene.Spawn<BuildingActor>(x, y, 1.4f);
		building->AddChild(std::unique_ptr<Actor>(new FireActor(0.42f, 0.85f, 0.8f, 0.5f)));
	}
}

void SpawnTileActors(SceneGraph& scene, Renderer& renderer, const TileMap& tileMap)
{
	// 타일을 8x8칸씩 청크로 나누고, 청크마다 정점 색상 메시 하나(지형용, 물이 있으면 물용까지
	// 최대 2개)로 구워서 드로우콜 1~2번으로 그린다. 예전엔 타일 하나하나가 독립된 액터라 청크
	// 하나(최대 64칸)를 그리는 데 드로우콜이 최대 64번 나갔는데, 32x32=1024칸 전체가 화면에
	// 걸리면 그것만으로 드로우콜 1024번이 나가는 게 성능 분석에서 가장 큰 병목으로 확인되어
	// 이 방식으로 바꿨다. 배치 액터의 위치/경계 구는 청크 범위를 그대로 써서, 화면 밖 청크는
	// 여전히 검사 한 번으로 통째로 건너뛴다(뷰 컬링 — 이전과 동일한 이점).
	const int kChunkSize = 8;
	const float kTileZ = -0.5f;

	int width = tileMap.GetWidth();
	int height = tileMap.GetHeight();

	for (int chunkY = 0; chunkY < height; chunkY += kChunkSize)
	{
		for (int chunkX = 0; chunkX < width; chunkX += kChunkSize)
		{
			int endX = (chunkX + kChunkSize < width) ? chunkX + kChunkSize : width;
			int endY = (chunkY + kChunkSize < height) ? chunkY + kChunkSize : height;

			std::vector<float> solidVertices;
			std::vector<float> waterVertices;

			for (int gy = chunkY; gy < endY; ++gy)
			{
				for (int gx = chunkX; gx < endX; ++gx)
				{
					float wx = tileMap.GetWorldX(gx);
					float wy = tileMap.GetWorldY(gy);

					float r, g, b;
					bool isWater = false;
					switch (tileMap.GetTile(gx, gy))
					{
					case TileType::Stone: r = 0.55f; g = 0.53f; b = 0.50f; break;
					case TileType::Water: r = 0.15f; g = 0.35f; b = 0.55f; isWater = true; break;
					case TileType::Path:  r = 0.45f; g = 0.35f; b = 0.22f; break;
					default:              r = 0.18f; g = 0.32f; b = 0.16f; break; // 잔디
					}

					AppendTileQuad(isWater ? waterVertices : solidVertices, wx, wy, kTileZ, r, g, b, 1.f);
				}
			}

			// 청크 중심과, 중심에서 청크 모서리까지의 반지름(대각선의 절반 + 약간의 여유).
			float centerX = (tileMap.GetWorldX(chunkX) + tileMap.GetWorldX(endX - 1)) * 0.5f;
			float centerY = (tileMap.GetWorldY(chunkY) + tileMap.GetWorldY(endY - 1)) * 0.5f;
			float tilesWide = (float)(endX - chunkX);
			float tilesHigh = (float)(endY - chunkY);
			float chunkRadius = sqrtf(tilesWide * tilesWide + tilesHigh * tilesHigh) * 0.5f + 0.1f;

			// 배치 액터의 정점은 이미 월드 좌표로 구워져 있어서(AppendTileQuad) 그리기 자체엔
			// 위치가 안 쓰이지만, 뷰 컬링은 액터의 월드 위치+경계 구로 판정하므로 여기서
			// 청크 위치를 정확히 지정해 둬야 한다.
			if (!solidVertices.empty())
			{
				TileBatchActor* solidBatch = scene.Spawn<TileBatchActor>(false);
				solidBatch->SetPosition(centerX, centerY, kTileZ);
				solidBatch->SetBoundingSphere(chunkRadius);
				solidBatch->Build(renderer, solidVertices);
			}

			if (!waterVertices.empty())
			{
				TileBatchActor* waterBatch = scene.Spawn<TileBatchActor>(true);
				waterBatch->SetPosition(centerX, centerY, kTileZ);
				waterBatch->SetBoundingSphere(chunkRadius);
				waterBatch->Build(renderer, waterVertices);
			}
		}
	}
}

LevelActors SpawnLevelActors(SceneGraph& scene, const LevelLayout& layout)
{
	float vx = layout.villageCenterX;
	float vy = layout.villageCenterY;
	float lx = layout.lakeCenterX;
	float ly = layout.lakeCenterY;

	// 숲: 마을/호수 중심에서 충분히 떨어진 곳에 무작위로 나무 36그루를 배치.
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
		scene.Spawn<TreeActor>(tx, ty, 1.6f, 0.08f + tint, 0.22f + tint, 0.10f + tint * 0.5f);
		++treesPlaced;
	}

	// 건물 2채와 그 옆의 횃불.
	SpawnBuildingWithTorch(scene, vx - 2.0f, vy + 2.0f);
	SpawnBuildingWithTorch(scene, vx + 2.0f, vy + 2.0f);

	// 마을 사람: 장로(퀘스트 시작점, 눈에 띄도록 빛남) + 마을 사람 7명.
	scene.Spawn<NpcActor>(vx, vy + 1.5f, 1.f, 3.0f, 2.6f, 0.8f, kInteractElder, "Elder");
	SpawnVillager(scene, vx - 1.6f, vy - 1.6f, 0.7f, 0.5f, 0.3f);
	SpawnVillager(scene, vx + 1.6f, vy - 1.6f, 0.5f, 0.4f, 0.7f);
	SpawnVillager(scene, vx, vy - 2.2f, 0.8f, 0.3f, 0.3f);
	SpawnVillager(scene, vx - 2.0f, vy + 0.3f, 0.6f, 0.55f, 0.35f);
	SpawnVillager(scene, vx + 2.0f, vy + 0.3f, 0.45f, 0.5f, 0.6f);
	SpawnVillager(scene, vx - 1.0f, vy + 1.2f, 0.65f, 0.4f, 0.5f);
	SpawnVillager(scene, vx + 1.0f, vy + 1.2f, 0.5f, 0.6f, 0.4f);

	// 퀘스트 아이템: 호수 옆에서 빛나는 잃어버린 제물.
	scene.Spawn<ItemActor>(lx + 0.3f, ly, 0.6f, 3.5f, 3.0f, 0.9f, kInteractQuestItem);

	// 일반 획득 아이템(약초): 경험치용, 넓은 숲 곳곳에 8개 흩어져 있음.
	SpawnHerb(scene, vx + 4.5f, vy + 3.f);
	SpawnHerb(scene, vx - 4.5f, vy - 3.f);
	SpawnHerb(scene, vx - 3.f, vy + 4.5f);
	SpawnHerb(scene, vx + 10.f, vy + 6.f);
	SpawnHerb(scene, vx - 10.f, vy - 6.f);
	SpawnHerb(scene, vx + 7.f, vy - 9.f);
	SpawnHerb(scene, vx - 8.f, vy + 9.f);
	SpawnHerb(scene, vx + 1.f, vy - 11.f);

	// 야생 짐승: 사슴 4마리(배회만 함) + 늑대 3마리(플레이어를 추적/공격하는 몬스터).
	// 인자: anchor 좌표, 크기, 색, 배회 위상, 체력, 공격형 여부, 이름.
	scene.Spawn<AnimalActor>(vx - 6.f, vy + 3.f, 0.9f, 0.45f, 0.32f, 0.18f, 0.f, 20, false, "Deer");
	scene.Spawn<AnimalActor>(vx + 6.f, vy - 3.5f, 0.9f, 0.5f, 0.36f, 0.2f, 2.1f, 20, false, "Deer");
	scene.Spawn<AnimalActor>(vx + 10.f, vy + 7.f, 0.9f, 0.48f, 0.34f, 0.19f, 1.3f, 20, false, "Deer");
	scene.Spawn<AnimalActor>(vx - 9.f, vy - 7.f, 0.9f, 0.42f, 0.3f, 0.17f, 3.4f, 20, false, "Deer");
	scene.Spawn<AnimalActor>(vx - 3.f, vy + 6.f, 1.0f, 0.3f, 0.3f, 0.32f, 4.2f, 30, true, "Wolf");
	scene.Spawn<AnimalActor>(vx + 8.f, vy - 6.f, 1.0f, 0.28f, 0.28f, 0.3f, 5.6f, 30, true, "Wolf");
	scene.Spawn<AnimalActor>(vx - 2.f, vy - 10.f, 1.0f, 0.32f, 0.32f, 0.34f, 0.7f, 30, true, "Wolf");

	LevelActors result;

	// 플레이어는 마을 중심 근처에서 시작한다(사망 시에도 여기로 되돌아옴).
	result.player = scene.Spawn<PlayerActor>(vx, vy - 0.5f);
	scene.SetPlayer(result.player);

	// 플레이어 발밑 위치 마커: 플레이어의 자식이라 이동을 자동으로 따라다닌다.
	RingStyle markerStyle = { 1.0f, 0.92f, 0.6f, 0.8f, 0.1f, 0.14f, 0.08f, 2.5f };
	result.player->AddChild(std::unique_ptr<Actor>(new RingActor(markerStyle, true)));

	// 조준 링: 공격/상호작용 사거리 안의 대상 발밑에 뜬다. 대상은 매 프레임 게임이 지정.
	RingStyle attackStyle = { 1.0f, 0.2f, 0.15f, 0.75f, 0.15f, 0.3f, 0.2f, 6.f };
	result.attackRing = scene.Spawn<RingActor>(attackStyle, false);

	RingStyle interactStyle = { 0.3f, 0.9f, 1.0f, 0.85f, 0.15f, 0.28f, 0.18f, 6.f };
	result.interactRing = scene.Spawn<RingActor>(interactStyle, false);

	return result;
}
