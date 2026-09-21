#pragma once

#include "LevelGenerator.h"
#include "SceneGraph.h"
#include "TileMap.h"

class PlayerActor;
class RingActor;

// Actor::GetInteractId()가 가지는 상호작용 식별자. 0이면 상호작용 불가.
const int kInteractElder = 1;
const int kInteractQuestItem = 2;
const int kInteractVillager = 3;
const int kInteractLoot = 4;

// 레벨을 채운 뒤 게임 코드가 계속 들고 있어야 하는 액터들.
struct LevelActors
{
	PlayerActor* player = nullptr;
	RingActor* attackRing = nullptr;   // 공격 사거리 안 가장 가까운 짐승 발밑의 붉은 링
	RingActor* interactRing = nullptr; // 상호작용 사거리 안 가장 가까운 대상 발밑의 하늘색 링
};

// 타일맵을 보고 바닥 타일 액터들을 씬에 생성한다. 타일은 8x8칸씩 그룹 노드(청크) 밑에 묶여서,
// 화면 밖 청크는 뷰 컬링 때 통째로 건너뛴다.
void SpawnTileActors(SceneGraph& scene, const TileMap& tileMap);

// 마을/호수 배치(layout)를 기준으로 나무·건물·NPC·아이템·짐승·플레이어 액터를 씬에
// 생성하고, 이후 게임 코드가 필요로 하는 액터 포인터들을 돌려준다.
LevelActors SpawnLevelActors(SceneGraph& scene, const LevelLayout& layout);
