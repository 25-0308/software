#include "stdafx.h"
#include "WorldLayout.h"

#include <cmath>
#include <cstring>

#include "CharacterActors.h"

namespace
{
	// 마을끼리의 간격: 이웃 마을 중심 사이 90칸(플레이어 초당 4칸 → 약 22초, 마을 어귀에서 어귀까지는 그보다 짧다).
	const float kSpacing = 90.f;

	// 길이 굽는 정도(중간 점을 길과 수직으로 미는 거리). 길마다 번갈아 양쪽으로.
	const float kRoadBend = 6.f;

	// 길잡이: 꺾임점에 이만큼 다가가면 그다음 꺾임점을 가리킨다.
	const float kWaypointReach = 2.5f;

	// 지역 표. 성역·무대는 주민·황야 몬스터가 없다.
	//  - 분위기의 수치: 다혈질 비율, 집 둘레를 거니는 반경, 어울리는 정도, 빛 알갱이 색.
	const WorldPlace kPlaces[] =
	{
		{
			"LOC_DELPHI", PlaceKind::Village, 0.f, -1.5f * kSpacing, 48, 10.6f, 9.3f,
			Faith::Apollo, "아폴론",
			{ "pilgrimage", "경건한 순례지", "신탁을 기다리는 순례객과 월계관 장수로 들뜬 성지. 사람들은 예의 바르고 말씨가 부드럽다.",
				0.1f, 2.5f, 1.f, 1.8f, 1.5f, 0.7f },
			"NPC_DELPHI_VILLAGER", "Villager", { Faith::Hermes, Faith::Dionysus }, 2,
			{ AnimalKind::Wolf, AnimalKind::Boar }, 2,
			{ { "LOC_WEST", 12.4f, 9.4f, 23.5f, 9.4f }, { "LOC_THEBES", 10.6f, 11.6f, 10.6f, 23.5f } }, 2,
		},
		{
			"LOC_THEBES", PlaceKind::Village, 0.f, -0.5f * kSpacing, 48, 2.6f, 2.6f,
			Faith::Dionysus, "디오니소스",
			{ "festival", "흥청망청 축제", "포도주와 북소리가 끊이지 않는 도시. 흥겹지만, 흥이 언제 광기로 번질지 모른다.",
				0.35f, 3.5f, 1.5f, 1.6f, 0.6f, 1.3f },
			"NPC_THEBES_CITIZEN", "Theban", { Faith::Apollo, Faith::Pan }, 2,
			{ AnimalKind::Maenad, AnimalKind::Boar }, 2,
			{ { "LOC_DELPHI", 0.5f, -5.6f, 0.5f, -23.5f }, { "LOC_OLYMPUS_FOOT", -5.6f, 0.5f, -23.5f, 0.5f },
				{ "LOC_ATHENS", 0.5f, 5.6f, 0.5f, 23.5f } }, 3,
		},
		{
			"LOC_OLYMPUS_FOOT", PlaceKind::Village, -kSpacing, -0.5f * kSpacing, 48, 10.4f, 9.0f,
			Faith::Zeus, "제우스",
			{ "authority", "권위와 불안", "왕의 산 그림자 아래 사는 목동들. 번개 이야기엔 목소리를 낮추고, 낯선 신의 신도에겐 깐깐하다.",
				0.35f, 2.f, 0.8f, 1.3f, 1.5f, 1.9f },
			"NPC_SHEPHERD", "Shepherd", { Faith::Pan, Faith::Hephaestus }, 2,
			{ AnimalKind::Wolf, AnimalKind::Boar, AnimalKind::BronzeWarrior }, 3,
			{ { "LOC_THEBES", 12.2f, 9.4f, 23.5f, 9.4f }, { "LOC_OLYMPUS", 3.5f, -11.0f, 3.5f, -23.5f } }, 2,
		},
		{
			"LOC_OLYMPUS", PlaceKind::Sanctuary, -kSpacing, -1.5f * kSpacing, 32, 9.6f, 9.6f,
			Faith::None, "올림포스 열두 신",
			{ "feud", "폭풍 전의 정적", "구름 위 신들의 집. 가족이 둘로 갈라서기 직전처럼 조용하다.",
				0.f, 0.f, 0.f, 1.6f, 1.5f, 1.2f },
			nullptr, nullptr, { Faith::None, Faith::None }, 0,
			{ AnimalKind::Wolf, AnimalKind::Wolf, AnimalKind::Wolf }, 0,
			{ { "LOC_OLYMPUS_FOOT", 9.6f, 11.2f, 9.6f, 15.5f } }, 1,
		},
		{
			"LOC_ATHENS", PlaceKind::Village, 0.f, 0.5f * kSpacing, 48, 11.0f, 3.4f,
			Faith::Athena, "아테나",
			{ "debate", "논쟁과 회의", "광장마다 토론이 끊이지 않는 도시. 말이 많은 만큼 말다툼도 많고, 신을 의심하는 목소리도 크다.",
				0.3f, 3.f, 1.4f, 1.2f, 1.5f, 1.0f },
			"NPC_ATHENS_CITIZEN", "Citizen", { Faith::Jupiter, Faith::None }, 2,
			{ AnimalKind::Bandit, AnimalKind::Wolf }, 2,
			{ { "LOC_THEBES", 2.0f, -3.6f, 2.0f, -23.5f }, { "LOC_AEGEAN", -2.0f, -0.8f, -23.5f, -0.8f },
				{ "LOC_TAENARON", 12.6f, 3.0f, 23.5f, 3.0f } }, 3,
		},
		{
			"LOC_AEGEAN", PlaceKind::Village, -kSpacing, 0.5f * kSpacing, 48, 9.6f, 9.4f,
			Faith::Poseidon, "포세이돈",
			{ "seafaring", "거친 뱃사람 기질", "파도처럼 성미 급한 어부들의 섬. 웃음소리도 크고 주먹도 빠르다.",
				0.5f, 2.5f, 1.1f, 0.6f, 1.6f, 1.7f },
			"NPC_FISHER", "Fisher", { Faith::Aphrodite, Faith::Hermes }, 2,
			{ AnimalKind::SeaSerpent, AnimalKind::Bandit }, 2,
			{ { "LOC_ATHENS", 11.8f, 9.8f, 23.5f, 9.8f }, { "LOC_CYPRUS", 9.8f, 11.8f, 9.8f, 23.5f } }, 2,
		},
		{
			"LOC_CYPRUS", PlaceKind::Village, -kSpacing, 1.5f * kSpacing, 48, 10.0f, 3.2f,
			Faith::Aphrodite, "아프로디테",
			{ "revelry", "향락과 소문", "연회와 장미, 속삭이는 소문의 섬. 다툼보다 수다가 많고, 수다가 곧 다툼의 씨앗이다.",
				0.1f, 3.f, 1.6f, 1.9f, 0.9f, 1.3f },
			"NPC_PAPHOS_SERVANT", "Servant", { Faith::Ares, Faith::Hermes }, 2,
			{ AnimalKind::SeaSerpent, AnimalKind::Boar }, 2,
			{ { "LOC_AEGEAN", 8.6f, -2.4f, 8.6f, -23.5f } }, 1,
		},
		{
			"LOC_TAENARON", PlaceKind::Village, kSpacing, 0.5f * kSpacing, 48, 10.0f, 8.6f,
			Faith::Hades, "하데스",
			{ "solemn", "침묵과 체념", "죽지 못한 자들이 사는 곶. 아무도 큰 소리를 내지 않고, 이웃끼리도 말이 거의 없다.",
				0.f, 1.f, 0.3f, 0.9f, 0.8f, 1.6f },
			"NPC_UNDYING", "Undying", { Faith::None, Faith::None }, 0,
			{ AnimalKind::ShadowWolf, AnimalKind::Bandit }, 2,
			{ { "LOC_ATHENS", -9.5f, 4.0f, -23.5f, 4.0f }, { "LOC_TARTARUS_RIM", 2.5f, -5.5f, 2.5f, -23.5f } }, 2,
		},
		{
			"LOC_TARTARUS_RIM", PlaceKind::Sanctuary, kSpacing, -0.5f * kSpacing, 32, 9.6f, 9.6f,
			Faith::None, "티탄의 잔재",
			{ "abyss", "심연의 공포", "세상의 바닥. 심연 가장자리의 균열이 숨을 쉰다. 여기선 아무도 농담하지 않는다.",
				0.f, 0.f, 0.f, 1.2f, 0.5f, 1.8f },
			nullptr, nullptr, { Faith::None, Faith::None }, 0,
			{ AnimalKind::Wolf, AnimalKind::Wolf, AnimalKind::Wolf }, 0,
			{ { "LOC_TAENARON", 9.6f, 11.2f, 9.6f, 15.5f } }, 1,
		},
		{
			"LOC_WEST", PlaceKind::Village, kSpacing, -1.5f * kSpacing, 48, 10.0f, 9.0f,
			Faith::Jupiter, "유피테르(로마식 숭배)",
			{ "order", "질서와 규율", "로마식 새 신앙의 기세가 오른 항구. 거리는 줄을 맞춰 반듯하고, 다른 신의 신도에겐 차갑다.",
				0.4f, 1.8f, 0.9f, 1.8f, 1.0f, 0.6f },
			"NPC_WEST_CITIZEN", "Citizen", { Faith::Zeus, Faith::Ares }, 2,
			{ AnimalKind::Bandit, AnimalKind::Wolf }, 2,
			{ { "LOC_DELPHI", -6.0f, 2.5f, -23.5f, 2.5f } }, 1,
		},
		{
			"LOC_KRONOS_BELLY", PlaceKind::Stage, 0.f, 0.f, 32, -6.2f, 6.2f,
			Faith::None, "—",
			{ "dread", "숨 막히는 어둠", "아주 오래전, 이 안에서 아이들이 울었다.", 0.f, 0.f, 0.f, 0.8f, 0.7f, 1.0f },
			nullptr, nullptr, { Faith::None, Faith::None }, 0,
			{ AnimalKind::Wolf, AnimalKind::Wolf, AnimalKind::Wolf }, 0,
			{}, 0,
		},
	};

	const int kPlaceCount = (int)(sizeof(kPlaces) / sizeof(kPlaces[0]));

	float DistanceSq(float ax, float ay, float bx, float by)
	{
		float dx = ax - bx;
		float dy = ay - by;
		return dx * dx + dy * dy;
	}

	// 점 (x, y)에서 선분 (a, b)까지 거리의 제곱.
	float SegmentDistanceSq(float x, float y, const WorldPoint& a, const WorldPoint& b)
	{
		float abX = b.x - a.x;
		float abY = b.y - a.y;
		float lengthSq = abX * abX + abY * abY;
		float t = (lengthSq > 0.f) ? ((x - a.x) * abX + (y - a.y) * abY) / lengthSq : 0.f;
		t = (t < 0.f) ? 0.f : ((t > 1.f) ? 1.f : t);
		return DistanceSq(x, y, a.x + abX * t, a.y + abY * t);
	}

	const WorldExit* FindExit(const WorldPlace& from, const char* toLocId)
	{
		for (int i = 0; i < from.exitCount; ++i)
		{
			if (strcmp(from.exits[i].toLocId, toLocId) == 0)
			{
				return &from.exits[i];
			}
		}
		return nullptr;
	}

	std::vector<WorldRoad> BuildRoads()
	{
		std::vector<WorldRoad> roads;

		for (int i = 0; i < kPlaceCount; ++i)
		{
			const WorldPlace& a = kPlaces[i];
			for (int e = 0; e < a.exitCount; ++e)
			{
				const WorldPlace* b = WorldLayout::Find(a.exits[e].toLocId);
				if (b == nullptr || b <= &a)
				{
					continue; // 길은 한 번만(표에서 뒤에 나오는 쪽이 앞의 것과 짝지을 때)
				}

				const WorldExit* fromA = &a.exits[e];
				const WorldExit* fromB = FindExit(*b, a.locId);
				if (fromB == nullptr)
				{
					continue;
				}

				WorldRoad road;
				road.a = &a;
				road.b = b;
				road.points[0] = WorldLayout::ToWorld(a, fromA->innerX, fromA->innerY);
				road.points[1] = WorldLayout::ToWorld(a, fromA->borderX, fromA->borderY);
				road.points[3] = WorldLayout::ToWorld(*b, fromB->borderX, fromB->borderY);
				road.points[4] = WorldLayout::ToWorld(*b, fromB->innerX, fromB->innerY);

				// 중간 점은 길과 수직으로 살짝 밀어 길이 자로 그은 듯 곧지 않게 한다(길마다 번갈아 반대쪽).
				float dx = road.points[3].x - road.points[1].x;
				float dy = road.points[3].y - road.points[1].y;
				float length = sqrtf(dx * dx + dy * dy);
				float side = (roads.size() % 2 == 0) ? 1.f : -1.f;
				road.points[2].x = (road.points[1].x + road.points[3].x) * 0.5f;
				road.points[2].y = (road.points[1].y + road.points[3].y) * 0.5f;
				if (length > 0.f)
				{
					road.points[2].x += -dy / length * kRoadBend * side;
					road.points[2].y += dx / length * kRoadBend * side;
				}

				roads.push_back(road);
			}
		}

		return roads;
	}

	// 지역 그래프에서 from → target까지 걸리는 길 수(BFS). 닿지 않으면 큰 수.
	int HopsTo(const WorldPlace* from, const WorldPlace* target)
	{
		const std::vector<WorldRoad>& roads = WorldLayout::GetRoads();
		std::vector<int> hops(kPlaceCount, 1 << 20);
		std::vector<const WorldPlace*> queue;

		int targetIndex = (int)(target - kPlaces);
		hops[targetIndex] = 0;
		queue.push_back(target);

		for (size_t head = 0; head < queue.size(); ++head)
		{
			const WorldPlace* place = queue[head];
			int here = hops[place - kPlaces];

			for (const WorldRoad& road : roads)
			{
				const WorldPlace* other = (road.a == place) ? road.b : ((road.b == place) ? road.a : nullptr);
				if (other != nullptr && hops[other - kPlaces] > here + 1)
				{
					hops[other - kPlaces] = here + 1;
					queue.push_back(other);
				}
			}
		}

		return hops[from - kPlaces];
	}

	// 꺾은선 points(count개, 진행 방향 순)에서 (x, y)가 가장 가까운 구간 다음의 꺾임점. 그 점에 거의 닿았으면 그다음 점.
	WorldPoint NextOnPolyline(const WorldPoint* points, int count, float x, float y)
	{
		int nearest = 0;
		float nearestSq = 0.f;
		for (int i = 0; i + 1 < count; ++i)
		{
			float distanceSq = SegmentDistanceSq(x, y, points[i], points[i + 1]);
			if (i == 0 || distanceSq < nearestSq)
			{
				nearest = i;
				nearestSq = distanceSq;
			}
		}

		int next = nearest + 1;
		if (next + 1 < count && DistanceSq(x, y, points[next].x, points[next].y) < kWaypointReach * kWaypointReach)
		{
			++next;
		}
		return points[next];
	}
}

int WorldLayout::GetPlaceCount()
{
	return kPlaceCount;
}

const WorldPlace& WorldLayout::GetPlace(int index)
{
	return kPlaces[index];
}

const WorldPlace* WorldLayout::Find(const std::string& locId)
{
	for (const WorldPlace& place : kPlaces)
	{
		if (locId == place.locId)
		{
			return &place;
		}
	}
	return nullptr;
}

bool WorldLayout::IsStage(const std::string& locId)
{
	const WorldPlace* place = Find(locId);
	return place != nullptr && place->kind == PlaceKind::Stage;
}

const WorldPlace* WorldLayout::Containing(float x, float y)
{
	for (const WorldPlace& place : kPlaces)
	{
		if (place.kind == PlaceKind::Stage)
		{
			continue;
		}

		float half = place.size * 0.5f;
		if (fabsf(x - place.centerX) <= half && fabsf(y - place.centerY) <= half)
		{
			return &place;
		}
	}
	return nullptr;
}

const WorldPlace* WorldLayout::NearestVillage(float x, float y)
{
	const WorldPlace* nearest = nullptr;
	float nearestSq = 0.f;

	for (const WorldPlace& place : kPlaces)
	{
		if (place.kind != PlaceKind::Village)
		{
			continue;
		}

		float distanceSq = DistanceSq(x, y, place.centerX, place.centerY);
		if (nearest == nullptr || distanceSq < nearestSq)
		{
			nearest = &place;
			nearestSq = distanceSq;
		}
	}
	return nearest;
}

WorldPoint WorldLayout::ToWorld(const WorldPlace& place, float localX, float localY)
{
	WorldPoint point = { place.centerX + localX, place.centerY + localY };
	return point;
}

WorldPoint WorldLayout::ArrivalPoint(const WorldPlace& place)
{
	return ToWorld(place, place.arrivalX, place.arrivalY);
}

const std::vector<WorldRoad>& WorldLayout::GetRoads()
{
	static const std::vector<WorldRoad> s_Roads = BuildRoads();
	return s_Roads;
}

bool WorldLayout::NextWaypoint(float x, float y, const std::string& targetLocId, float& outX, float& outY)
{
	const WorldPlace* target = Find(targetLocId);
	if (target == nullptr || target->kind == PlaceKind::Stage)
	{
		return false;
	}

	const WorldPlace* here = Containing(x, y);
	if (here == target)
	{
		return false;
	}

	const std::vector<WorldRoad>& roads = GetRoads();
	const WorldRoad* chosen = nullptr;
	bool forward = true; // true면 a → b 방향

	if (here != nullptr)
	{
		// 지역 안: 목표에 한 걸음 더 가까운 이웃으로 가는 길.
		int bestHops = 1 << 20;
		for (const WorldRoad& road : roads)
		{
			const WorldPlace* other = (road.a == here) ? road.b : ((road.b == here) ? road.a : nullptr);
			if (other == nullptr)
			{
				continue;
			}

			int hops = HopsTo(other, target);
			if (hops < bestHops)
			{
				bestHops = hops;
				chosen = &road;
				forward = (road.a == here);
			}
		}
	}
	else
	{
		// 황야: 가장 가까운 길을 골라, 그 길의 두 끝 중 목표에 더 가까운 쪽으로.
		float nearestSq = 0.f;
		for (const WorldRoad& road : roads)
		{
			for (int i = 0; i + 1 < 5; ++i)
			{
				float distanceSq = SegmentDistanceSq(x, y, road.points[i], road.points[i + 1]);
				if (chosen == nullptr || distanceSq < nearestSq)
				{
					chosen = &road;
					nearestSq = distanceSq;
				}
			}
		}

		if (chosen != nullptr)
		{
			int hopsA = HopsTo(chosen->a, target);
			int hopsB = HopsTo(chosen->b, target);
			forward = (hopsB < hopsA) || (hopsB == hopsA &&
				DistanceSq(x, y, chosen->b->centerX, chosen->b->centerY) < DistanceSq(x, y, chosen->a->centerX, chosen->a->centerY));
		}
	}

	if (chosen == nullptr)
	{
		// 길이 없으면(그럴 일은 없음) 목표 지역 어귀로 곧장.
		WorldPoint arrival = ArrivalPoint(*target);
		outX = arrival.x;
		outY = arrival.y;
		return true;
	}

	WorldPoint points[5];
	for (int i = 0; i < 5; ++i)
	{
		points[i] = forward ? chosen->points[i] : chosen->points[4 - i];
	}

	WorldPoint next = NextOnPolyline(points, 5, x, y);
	outX = next.x;
	outY = next.y;
	return true;
}

const char* WorldLayout::DirectionName(const WorldPlace& from, const WorldPlace& to)
{
	// 화면 기준: 북 = -(x+y), 동 = y-x.
	float dx = to.centerX - from.centerX;
	float dy = to.centerY - from.centerY;
	float north = -(dx + dy);
	float east = dy - dx;

	if (fabsf(north) >= fabsf(east) * 2.f)
	{
		return (north > 0.f) ? "북쪽" : "남쪽";
	}
	if (fabsf(east) >= fabsf(north) * 2.f)
	{
		return (east > 0.f) ? "동쪽" : "서쪽";
	}
	if (north > 0.f)
	{
		return (east > 0.f) ? "북동쪽" : "북서쪽";
	}
	return (east > 0.f) ? "남동쪽" : "남서쪽";
}
