#include "stdafx.h"
#include "Wilderness.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "CharacterActors.h"
#include "LevelBuilder.h"
#include "SceneGraph.h"
#include "TileMap.h"
#include "WorldLayout.h"

namespace
{
	const float kSpotStep = 2.f;          // 자리를 찾는 격자 간격
	const float kVillageMargin = 6.f;     // 마을 정사각형 테두리에서 이만큼은 떨어진 곳에만(마을 어귀는 안전하게)
	const float kRoadMargin = 2.5f;       // 길 바로 위엔 서지 않는다(길을 걸으면 양옆에서 덤벼 오는 정도)
	const float kSpacing = 7.f;           // 처음 풀어 놓을 때 몬스터끼리 떨어뜨리는 거리
	const float kCountInterval = 3.f;     // 살아 있는 수를 세는 간격(초)
	const float kRespawnSeconds = 30.f;   // 모자랄 때 한 마리가 다시 나타나기까지
	const float kRespawnMinDistance = 26.f; // 플레이어에게서 이보다 먼 곳에만 다시 나타난다(눈앞에서 솟아나지 않게)

	float SegmentDistanceSq(float x, float y, const WorldPoint& a, const WorldPoint& b)
	{
		float abX = b.x - a.x;
		float abY = b.y - a.y;
		float lengthSq = abX * abX + abY * abY;
		float t = (lengthSq > 0.f) ? ((x - a.x) * abX + (y - a.y) * abY) / lengthSq : 0.f;
		t = (std::max)(0.f, (std::min)(1.f, t));

		float dx = x - (a.x + abX * t);
		float dy = y - (a.y + abY * t);
		return dx * dx + dy * dy;
	}

	bool IsNearRoad(float x, float y)
	{
		for (const WorldRoad& road : WorldLayout::GetRoads())
		{
			for (int i = 0; i + 1 < 5; ++i)
			{
				if (SegmentDistanceSq(x, y, road.points[i], road.points[i + 1]) < kRoadMargin * kRoadMargin)
				{
					return true;
				}
			}
		}
		return false;
	}

	// 어느 지역의 정사각형(+ margin) 안인지.
	bool IsInsideAnyPlace(float x, float y, float margin)
	{
		for (int i = 0; i < WorldLayout::GetPlaceCount(); ++i)
		{
			const WorldPlace& place = WorldLayout::GetPlace(i);
			if (place.kind == PlaceKind::Stage)
			{
				continue;
			}

			float half = place.size * 0.5f + margin;
			if (fabsf(x - place.centerX) <= half && fabsf(y - place.centerY) <= half)
			{
				return true;
			}
		}
		return false;
	}

	int PlaceIndex(const WorldPlace* place)
	{
		for (int i = 0; i < WorldLayout::GetPlaceCount(); ++i)
		{
			if (&WorldLayout::GetPlace(i) == place)
			{
				return i;
			}
		}
		return -1;
	}
}

Wilderness::Wilderness()
	: m_Rng(std::random_device{}())
{
}

void Wilderness::Populate(SceneGraph& scene, const TileMap& tileMap, unsigned int seed)
{
	m_Ranges.clear();
	m_Rng.seed(seed);

	// 1) 황야의 걸을 수 있는 땅을 격자로 훑어, 가장 가까운 마을의 자리로 나눈다(마을·성역 정사각형과 그 둘레, 길 위는 뺀다).
	float halfWidth = tileMap.GetWidth() * 0.5f - 2.f;
	float halfHeight = tileMap.GetHeight() * 0.5f - 2.f;
	for (float y = -halfHeight; y <= halfHeight; y += kSpotStep)
	{
		for (float x = -halfWidth; x <= halfWidth; x += kSpotStep)
		{
			if (IsInsideAnyPlace(x, y, kVillageMargin) || !IsOpenGround(tileMap, x, y, 1.f) || IsNearRoad(x, y))
			{
				continue;
			}

			int index = PlaceIndex(WorldLayout::NearestVillage(x, y));
			if (index >= 0)
			{
				m_Ranges[index].spots.push_back(std::make_pair(x, y));
			}
		}
	}

	// 2) 마을마다 주민 수만큼, 서로 떨어진 자리를 골라 풀어 놓는다(자리가 모자라면 간격을 줄여 가며).
	for (std::pair<const int, Range>& entry : m_Ranges)
	{
		Range& range = entry.second;
		std::shuffle(range.spots.begin(), range.spots.end(), m_Rng);

		std::vector<std::pair<float, float>> chosen;
		for (float spacing = kSpacing; spacing >= 1.f && (int)chosen.size() < WorldLayout::kResidentsPerVillage; spacing *= 0.6f)
		{
			for (const std::pair<float, float>& spot : range.spots)
			{
				if ((int)chosen.size() >= WorldLayout::kResidentsPerVillage)
				{
					break;
				}

				bool crowded = false;
				for (const std::pair<float, float>& other : chosen)
				{
					float dx = other.first - spot.first;
					float dy = other.second - spot.second;
					if (dx * dx + dy * dy < spacing * spacing)
					{
						crowded = true;
						break;
					}
				}

				if (!crowded)
				{
					chosen.push_back(spot);
				}
			}
		}

		for (const std::pair<float, float>& spot : chosen)
		{
			SpawnOne(scene, entry.first, spot.first, spot.second);
		}
		range.alive = (int)chosen.size();
	}
}

void Wilderness::SpawnOne(SceneGraph& scene, int placeIndex, float x, float y)
{
	const WorldPlace& place = WorldLayout::GetPlace(placeIndex);
	if (place.wildKindCount <= 0)
	{
		return;
	}

	// 종류: 앞의 것일수록 자주(둘이면 6:4, 셋이면 5:3:2).
	std::uniform_real_distribution<float> unit(0.f, 1.f);
	float roll = unit(m_Rng);
	int kindIndex = 0;
	if (place.wildKindCount == 2)
	{
		kindIndex = (roll < 0.6f) ? 0 : 1;
	}
	else if (place.wildKindCount >= 3)
	{
		kindIndex = (roll < 0.5f) ? 0 : ((roll < 0.8f) ? 1 : 2);
	}

	AnimalActor* monster = scene.Spawn<AnimalActor>(place.wildKinds[kindIndex], x, y, unit(m_Rng) * 6.2831853f);
	monster->SetHomeRegion(place.locId);
	monster->SetWild(true);
	monster->SetFacing(unit(m_Rng) * 6.2831853f);
}

void Wilderness::Update(float deltaSeconds, SceneGraph& scene, const PlayerActor& player)
{
	m_CountTimer -= deltaSeconds;
	if (m_CountTimer > 0.f)
	{
		return;
	}
	m_CountTimer = kCountInterval;

	for (std::pair<const int, Range>& entry : m_Ranges)
	{
		entry.second.alive = 0;
	}

	scene.ForEach([this](Actor& actor)
	{
		if (actor.GetType() != ActorType::Animal || actor.IsPendingDestroy() || !static_cast<AnimalActor&>(actor).IsWild())
		{
			return;
		}

		const WorldPlace* place = (actor.GetHomeRegion() != nullptr) ? WorldLayout::Find(actor.GetHomeRegion()) : nullptr;
		int index = PlaceIndex(place);
		std::map<int, Range>::iterator found = m_Ranges.find(index);
		if (found != m_Ranges.end())
		{
			++found->second.alive;
		}
	});

	float playerX = player.GetWorldX();
	float playerY = player.GetWorldY();
	std::uniform_int_distribution<int> any(0, 1 << 20);

	for (std::pair<const int, Range>& entry : m_Ranges)
	{
		Range& range = entry.second;
		if (range.alive >= WorldLayout::kResidentsPerVillage || range.spots.empty())
		{
			range.respawnTimer = kRespawnSeconds;
			continue;
		}

		range.respawnTimer -= kCountInterval;
		if (range.respawnTimer > 0.f)
		{
			continue;
		}
		range.respawnTimer = kRespawnSeconds;

		// 플레이어 눈에 띄지 않는 먼 자리 하나(몇 번 골라 보고 없으면 다음 기회에).
		for (int attempt = 0; attempt < 12; ++attempt)
		{
			const std::pair<float, float>& spot = range.spots[(size_t)any(m_Rng) % range.spots.size()];
			float dx = spot.first - playerX;
			float dy = spot.second - playerY;
			if (dx * dx + dy * dy >= kRespawnMinDistance * kRespawnMinDistance)
			{
				SpawnOne(scene, entry.first, spot.first, spot.second);
				++range.alive;
				break;
			}
		}
	}
}

int Wilderness::GetAliveCount(const std::string& locId) const
{
	int index = PlaceIndex(WorldLayout::Find(locId));
	std::map<int, Range>::const_iterator found = m_Ranges.find(index);
	return (found != m_Ranges.end()) ? found->second.alive : 0;
}
