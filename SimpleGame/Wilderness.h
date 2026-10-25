#pragma once

#include <map>
#include <random>
#include <string>
#include <vector>

class PlayerActor;
class SceneGraph;
class TileMap;

// 마을과 마을 사이 황야의 몬스터. 마을마다 그 마을 둘레(가장 가까운 마을이 그 마을인 황야)에 주민 수와 같은 수
// (WorldLayout::kResidentsPerVillage = 15)의 몬스터가 산다 — 주민과 몬스터는 1:1. 종류는 마을마다 정해져 있다
// (WorldLayout의 wildKinds: 델포이 = 늑대·멧돼지, 테베 = 광란의 마이나스·멧돼지, 에게해 = 바다뱀·산적 등).
// 마을 정사각형에서 조금 떨어진 걸을 수 있는 땅에 서로 떨어지게 흩어 놓고, 쓰러지면 잠시 뒤 플레이어 눈에 띄지 않는
// 먼 곳에 다시 나타나 수를 맞춘다. 몬스터 자신의 AI(감지·추적·포기·귀환)는 AnimalActor 그대로다.
class Wilderness
{
public:
	Wilderness();

	// 월드를 막 지은 뒤 한 번: 마을마다 몬스터가 설 수 있는 자리를 모으고 처음 무리를 풀어 놓는다.
	void Populate(SceneGraph& scene, const TileMap& tileMap, unsigned int seed);

	// 매 프레임(세상이 멈춘 동안엔 부르지 않음): 몇 초마다 마을별로 살아 있는 수를 세어, 모자라면 다시 채운다.
	void Update(float deltaSeconds, SceneGraph& scene, const PlayerActor& player);

	// 지금 그 마을 둘레에 살아 있는 황야 몬스터 수(마지막으로 센 값 — 모니터용).
	int GetAliveCount(const std::string& locId) const;

private:
	struct Range
	{
		std::vector<std::pair<float, float>> spots; // 몬스터가 설 수 있는 자리(월드 좌표)
		int alive = 0;
		float respawnTimer = 0.f;                   // 모자란 동안 다음 한 마리가 나타나기까지
	};

	// 그 마을의 다음 몬스터 하나(종류는 마을의 wildKinds — 앞의 것일수록 자주)를 (x, y)에 만든다.
	void SpawnOne(SceneGraph& scene, int placeIndex, float x, float y);

	std::map<int, Range> m_Ranges; // WorldLayout의 지역 번호 → 그 마을의 황야
	std::mt19937 m_Rng;
	float m_CountTimer = 0.f;
};
