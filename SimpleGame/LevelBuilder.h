#pragma once

#include <random>
#include <vector>

#include "SceneGraph.h"
#include "TileMap.h"

class AnimalActor;
class NpcActor;
class PlayerActor;
class Renderer;
class RingActor;
enum class AnimalKind;
struct HumanLook;

// Actor::GetInteractId()가 가지는 상호작용 식별자. 0이면 상호작용 불가.
const int kInteractStory = 1;  // 이야기 대상(인물·조사할 물건·이정표): Data/Story.txt의 "on talk <스토리ID>" 대화를 연다
const int kInteractPickup = 2; // 줍는 이야기 아이템: "on pickup <스토리ID>" 대화가 열리면 주운 것으로 치고 사라진다
const int kInteractLoot = 3;   // 약초: 주우면 경험치

// 레벨을 채운 뒤 게임 코드가 계속 들고 있어야 하는 액터들.
struct LevelActors
{
	PlayerActor* player = nullptr;
	RingActor* attackRing = nullptr;   // 공격 사거리 안 가장 가까운 짐승 발밑의 붉은 링
	RingActor* interactRing = nullptr; // 상호작용 사거리 안 가장 가까운 대상 발밑의 하늘색 링
};

// 월드 좌표의 한 점.
struct Spot
{
	float x, y;
};

// ---- 바닥 ----

// 타일맵을 보고 바닥 액터들을 씬에 생성한다. 타일은 8x8칸씩 청크로 나뉘고, 청크마다 정점 색상 메시 하나
// (지형용, 물이 있으면 물용까지 최대 2개)로 구워서 드로우콜을 크게 줄인다(GPU 버퍼를 만들어야 해서 renderer가
// 필요함). 지형 색은 모서리마다 주변 지형과 섞이고 물가엔 모래가 깔리며, 꽃·풀 포기·자갈 장식도 같은 버퍼에
// 구워 넣는다(seed가 같으면 같은 무늬). 섬(맵) 둘레엔 바다를 한 겹 두른다. 화면 밖 청크는 뷰 컬링 때 통째로 건너뛴다.
void SpawnTileActors(SceneGraph& scene, Renderer& renderer, const TileMap& tileMap, int seed);

// ---- 자리 찾기 ----

// (x, y)가 무언가를 세워도 되는 열린 땅인지: 플레이어가 갈 수 있는 맵 안쪽이고, 그 자리와 사방 clearance
// 거리의 네 점이 모두 걸을 수 있는 타일이어야 한다(물가·바위에 바짝 붙은 자리도 피함).
bool IsOpenGround(const TileMap& tileMap, float x, float y, float clearance);

// (x, y)가 열린 땅이면 그대로, 아니면 가까운 곳부터 동심원으로 넓혀 가며 찾은 열린 땅을 돌려준다.
Spot FindOpenGround(const TileMap& tileMap, float x, float y, float clearance);

bool IsNearAny(const std::vector<Spot>& spots, float x, float y, float distance);

// ---- 어느 지역에서나 쓰는 배치 ----

// 숲: count그루를 잔디 타일 위, avoid 자리들과 avoidDistance 이상 떨어진 열린 땅에 심는다(서로도 1.3 이상).
// cypressRatio 비율은 지중해식 사이프러스, 나머지는 짙은 초록~올리브빛 활엽수. 심은 자리를 돌려준다.
std::vector<Spot> ScatterTrees(SceneGraph& scene, const TileMap& tileMap, std::mt19937& rng, int count,
	const std::vector<Spot>& avoid, float avoidDistance, float cypressRatio);

// 바위 타일 중 걸을 수 있는 칸과 맞닿은 가장자리를 따라 바위 덩어리를 늘어놓아 산·언덕이 솟아 보이게 한다.
// snowy면 윗면이 눈으로 덮인다(올림포스).
void ScatterRocks(SceneGraph& scene, const TileMap& tileMap, std::mt19937& rng, float density, bool snowy);

// 약초(획득하면 경험치) 하나를 (x, y) 근처의 열린 땅에 생성한다.
void SpawnHerb(SceneGraph& scene, const TileMap& tileMap, float x, float y);

// 짐승·괴물 하나를 (x, y) 근처의 열린 땅에 생성한다.
AnimalActor* SpawnAnimal(SceneGraph& scene, const TileMap& tileMap, AnimalKind kind, float x, float y, float phase);

// 이야기 인물 하나를 (x, y)에 생성한다(storyId·존재 조건은 STORY.md/Data/Story.txt 규칙 그대로).
// 처음엔 카메라 쪽을 바라보게 세운다(필요하면 생성 후 SetFacing으로 바꾼다).
NpcActor* SpawnStoryNpc(SceneGraph& scene, float x, float y, float size, float r, float g, float b, const HumanLook& look,
	const char* storyId, const char* nameTag, const char* presence);

// 플레이어 + 발밑 위치 마커 + 조준 링 두 개를 (x, y)에 생성한다.
LevelActors SpawnPlayerAndMarkers(SceneGraph& scene, float x, float y);

// 섬 위를 떠다니는 빛 알갱이(분위기용 시각 효과).
void SpawnAmbience(SceneGraph& scene, const TileMap& tileMap);
