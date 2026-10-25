#pragma once

#include <string>

class PlayerActor;
class Renderer;
class RingActor;
class SceneGraph;
class TileMap;

// 지은 맵(하나로 이어진 월드, 또는 월드 밖의 이야기 무대). 타일맵·씬의 소유권은 부른 쪽(SimpleGame)으로 넘어간다.
struct LoadedMap
{
	std::string locId;                 // 처음 서는 지역(월드는 LOC_DELPHI, 무대는 그 무대의 ID)
	TileMap* tileMap = nullptr;
	SceneGraph* scene = nullptr;
	PlayerActor* player = nullptr;
	RingActor* attackRing = nullptr;
	RingActor* interactRing = nullptr;
};

// 이야기 속 지역(STORY.md 4장의 LOC_* + 새 마을 LOC_THEBES)을 하나로 이어진 월드 위에 짓는다(배치는 WorldLayout.h).
// 지역마다 지형(산·강·바다·언덕)과 건축물·인물·괴물 배치를 손으로 정해 두고(지역 가운데를 원점으로 한 로컬 좌표),
// 마을은 주민 15명과 그들이 사는 집(한 집에 1~3명)으로 채우고, 나무·바위·장식 같은 세부만 시드로 흩뿌린다. 지역과
// 지역 사이는 황야(들판·숲·산줄기·바다의 모래톱)와 길로 잇는다 — 걸어서 오간다. 인물·괴물·물건의 등장은 이야기 진행
// 조건(Actor::SetPresence)으로 정하므로, 월드는 한 번 지으면 이야기가 진행돼도 다시 짓지 않는다.
//
// 월드의 지역: LOC_DELPHI(델포이 — 시작), LOC_THEBES(테베 — 새 마을), LOC_OLYMPUS_FOOT(올림포스 산기슭), LOC_ATHENS(아테네),
//   LOC_AEGEAN(에게해 군도), LOC_CYPRUS(키프로스), LOC_TAENARON(타이나론 곶), LOC_WEST(서쪽 해안),
//   LOC_OLYMPUS(올림포스 정상 — 성역), LOC_TARTARUS_RIM(타르타로스 경계 — 성역).
// 월드 밖의 무대: LOC_KRONOS_BELLY(크로노스의 배 속 — MQ_05 2페이즈, 이야기의 TRAVEL로만 드나든다).
namespace WorldMaps
{
	// 월드 전체를 짓는다. sessionSeed가 같으면(한 번의 실행 동안) 같은 모습이다. 플레이어는 델포이 시장 길에서 시작한다.
	LoadedMap BuildOverworld(Renderer& renderer, unsigned int sessionSeed);

	// 월드 밖의 이야기 무대(크로노스의 배 속)를 짓는다. 플레이어는 무대의 시작 지점에 선다.
	LoadedMap BuildStage(const std::string& locId, Renderer& renderer, unsigned int sessionSeed);

	bool IsKnownLocation(const std::string& locId);
}
