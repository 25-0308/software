#pragma once

#include <string>

class PlayerActor;
class Renderer;
class RingActor;
class SceneGraph;
class TileMap;

// 한 지역(맵)을 지은 결과. 타일맵·씬의 소유권은 부른 쪽(SimpleGame)으로 넘어간다(다른 지역으로 갈 때 지운다).
struct LoadedMap
{
	std::string locId;                 // 실제로 지은 지역 ID(모르는 ID를 받으면 LOC_DELPHI)
	TileMap* tileMap = nullptr;
	SceneGraph* scene = nullptr;
	PlayerActor* player = nullptr;
	RingActor* attackRing = nullptr;
	RingActor* interactRing = nullptr;
};

// 이야기 속 지역(STORY.md 4장의 LOC_*)을 맵으로 짓는다. 지역마다 지형(산·강·바다·언덕)과 건축물·인물·
// 괴물 배치를 손으로 정해 두고, 나무·바위·장식 같은 세부만 시드로 흩뿌린다. 인물·괴물·물건의 등장은
// 이야기 진행 조건(Actor::SetPresence)으로 정하므로, 맵은 한 번 지으면 이야기가 진행돼도 다시 짓지 않는다.
//
// 지금 지을 수 있는 지역: LOC_DELPHI(델포이 · 파르나소스 산기슭 — 시작 지역, MQ_00/MQ_01),
// LOC_OLYMPUS_FOOT(올림포스 산기슭 · 테살리아 — MQ_02 신들의 대회의), LOC_ATHENS(아테네 — MQ_02 조사).
namespace WorldMaps
{
	// sessionSeed가 같으면(한 번의 실행 동안) 같은 지역은 언제 다시 와도 똑같은 모습이다.
	// arrivedByTravel이면 이정표 앞에서, 아니면(새 게임) 그 지역의 시작 지점에서 플레이어가 시작한다.
	LoadedMap Build(const std::string& locId, Renderer& renderer, unsigned int sessionSeed, bool arrivedByTravel);

	bool IsKnownLocation(const std::string& locId);
}
