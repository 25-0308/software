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
	struct Color
	{
		float r, g, b;
	};

	Color Mix(const Color& a, const Color& b, float t)
	{
		Color result = { a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t };
		return result;
	}

	Color Scale(const Color& color, float k)
	{
		Color result = { color.r * k, color.g * k, color.b * k };
		return result;
	}

	// 지형 기본색.
	const Color kGrassColor = { 0.21f, 0.37f, 0.17f };
	const Color kDryGrassColor = { 0.42f, 0.43f, 0.21f };   // 햇볕에 마른 풀(잔디에 얼룩덜룩 섞음)
	const Color kStoneColor = { 0.66f, 0.63f, 0.57f };      // 마을 돌바닥(밝은 석회암)
	const Color kPathColor = { 0.52f, 0.41f, 0.27f };
	const Color kSandColor = { 0.80f, 0.72f, 0.52f };       // 호수·바다 물가의 모래
	const Color kShallowWaterColor = { 0.20f, 0.52f, 0.58f };
	const Color kDeepWaterColor = { 0.08f, 0.24f, 0.40f };
	const Color kOceanColor = { 0.05f, 0.17f, 0.32f };      // 섬에서 멀리 떨어진 먼바다

	// TileBatchSolid.fs가 재질별 잔무늬를 고르는 번호(지형 정점의 extra.y).
	const float kMaterialGrass = 0.f;
	const float kMaterialStone = 1.f;
	const float kMaterialPath = 2.f;
	const float kMaterialPlain = 9.f; // 무늬 없이 색 그대로(꽃·풀 같은 장식)

	// 바닥 높이. 캐릭터·건물·그림자가 서 있는 z = 0과 같아야 한다 — 예전엔 -0.5에 그려서, 이 카메라
	// 각도에선 캐릭터가 실제로 서 있는 타일보다 반 칸쯤 어긋난 자리에 서 있는 것처럼 보였다(물가 충돌이
	// 눈에 보이는 물가와 안 맞던 원인). 바닥은 항상 가장 먼저 그리므로 그리기 순서엔 영향이 없다.
	const float kGroundZ = 0.f;
	const float kDecorationZ = 0.002f; // 바닥에 그려 넣는 꽃·풀 장식(바닥보다 아주 살짝 위)

	const int kChunkSize = 8;

	// ---- 결정적 난수/노이즈 (같은 입력이면 항상 같은 값) ----

	float HashToUnit(int x, int y, int seed)
	{
		unsigned int h = (unsigned int)x * 374761393u + (unsigned int)y * 668265263u + (unsigned int)seed * 2246822519u;
		h = (h ^ (h >> 13)) * 1274126177u;
		h ^= h >> 16;
		return (float)(h & 0xFFFFFFu) / 16777215.f;
	}

	// 격자점마다의 난수를 부드럽게 보간한 값 노이즈(0~1).
	float ValueNoise(float x, float y, int seed)
	{
		int x0 = (int)floorf(x);
		int y0 = (int)floorf(y);
		float fx = x - (float)x0;
		float fy = y - (float)y0;
		float ux = fx * fx * (3.f - 2.f * fx);
		float uy = fy * fy * (3.f - 2.f * fy);

		float a = HashToUnit(x0, y0, seed);
		float b = HashToUnit(x0 + 1, y0, seed);
		float c = HashToUnit(x0, y0 + 1, seed);
		float d = HashToUnit(x0 + 1, y0 + 1, seed);

		float bottom = a + (b - a) * ux;
		float top = c + (d - c) * ux;
		return bottom + (top - bottom) * uy;
	}

	// 타일 한 칸 안의 장식 배치용 난수. salt를 바꾸면 같은 칸에서 서로 다른 난수가 나온다.
	float TileRandom(int gx, int gy, int seed, int salt)
	{
		return HashToUnit(gx + salt * 1013, gy - salt * 719, seed + salt * 7);
	}

	// ---- 바닥 정점 ----

	// Renderer::kTileBatchFloatsPerVertex(월드좌표3 + extra2 + 색4 = 9) 형식의 정점 하나.
	void AppendVertex(std::vector<float>& out, float x, float y, float z, float extraX, float extraY, const Color& color)
	{
		out.push_back(x);
		out.push_back(y);
		out.push_back(z);
		out.push_back(extraX);
		out.push_back(extraY);
		out.push_back(color.r);
		out.push_back(color.g);
		out.push_back(color.b);
		out.push_back(1.f);
	}

	// 격자 모서리 (cx, cy) — 타일 (cx-1..cx, cy-1..cy) 네 칸이 만나는 점 — 에 닿은 타일 종류별 개수.
	// 맵 밖은 바다이므로 물로 센다.
	struct CornerTiles
	{
		int water = 0;
		int grass = 0;
		int stone = 0;
		int path = 0;
	};

	CornerTiles CountCornerTiles(const TileMap& tileMap, int cx, int cy)
	{
		CornerTiles counts;

		for (int gy = cy - 1; gy <= cy; ++gy)
		{
			for (int gx = cx - 1; gx <= cx; ++gx)
			{
				if (gx < 0 || gx >= tileMap.GetWidth() || gy < 0 || gy >= tileMap.GetHeight())
				{
					++counts.water;
					continue;
				}

				switch (tileMap.GetTile(gx, gy))
				{
				case TileType::Water: ++counts.water; break;
				case TileType::Stone: ++counts.stone; break;
				case TileType::Path:  ++counts.path; break;
				default:              ++counts.grass; break;
				}
			}
		}

		return counts;
	}

	Color LandBaseColor(TileType type)
	{
		switch (type)
		{
		case TileType::Stone: return kStoneColor;
		case TileType::Path:  return kPathColor;
		default:              return kGrassColor;
		}
	}

	float LandMaterial(TileType type)
	{
		switch (type)
		{
		case TileType::Stone: return kMaterialStone;
		case TileType::Path:  return kMaterialPath;
		default:              return kMaterialGrass;
		}
	}

	// 땅 타일(종류 own)의 모서리 (cx, cy) 색. 예전엔 타일마다 한 가지 색이라 바닥이 모눈종이처럼
	// 보였는데, 이제 모서리마다 색을 따로 정해서 칸 안에서 부드럽게 이어지게 한다.
	Color LandCornerColor(const TileMap& tileMap, int cx, int cy, TileType own, int seed)
	{
		CornerTiles counts = CountCornerTiles(tileMap, cx, cy);
		Color ownColor = LandBaseColor(own);
		Color color = ownColor;

		// 모서리에 닿은 땅 타일 색의 평균과 자기 색을 섞는다: 지형 경계가 부드럽게 번지되, 한 칸짜리
		// 흙길처럼 좁은 지형이 주변 색에 묻혀 사라지지는 않을 만큼만.
		int land = counts.grass + counts.stone + counts.path;
		if (land > 0)
		{
			float inv = 1.f / (float)land;
			Color average =
			{
				(kGrassColor.r * counts.grass + kStoneColor.r * counts.stone + kPathColor.r * counts.path) * inv,
				(kGrassColor.g * counts.grass + kStoneColor.g * counts.stone + kPathColor.g * counts.path) * inv,
				(kGrassColor.b * counts.grass + kStoneColor.b * counts.stone + kPathColor.b * counts.path) * inv,
			};
			color = Mix(ownColor, average, 0.45f);
		}

		// 넓은 밝기 얼룩 + 잔디에 섞이는 마른 풀빛. 둘 다 모서리 위치만의 함수라 이웃 타일과 이어진다.
		float patch = ValueNoise((float)cx * 0.18f, (float)cy * 0.18f, seed);
		if (own == TileType::Grass)
		{
			float dry = ValueNoise((float)cx * 0.33f + 11.f, (float)cy * 0.33f + 7.f, seed + 1);
			color = Mix(color, kDryGrassColor, dry * dry * 0.6f);
		}
		color = Scale(color, 0.86f + 0.26f * patch);

		// 물(호수·바다)에 닿은 모서리는 모래색 → 물가를 따라 모래사장이 생긴다.
		if (counts.water > 0)
		{
			float sandy = (float)counts.water * 0.4f;
			if (sandy > 1.f)
			{
				sandy = 1.f;
			}
			color = Mix(color, kSandColor, sandy);
		}

		return color;
	}

	// 물 모서리의 물가 정도: 모서리에 닿은 네 칸 중 땅의 비율(0 = 사방이 물인 깊은 곳, 물가 직선에선 0.5).
	float WaterShore(const TileMap& tileMap, int cx, int cy)
	{
		CornerTiles counts = CountCornerTiles(tileMap, cx, cy);
		return (float)(4 - counts.water) / 4.f;
	}

	// 물가 정도가 클수록 얕은 물색(청록), 작을수록 깊은 물색.
	Color WaterColor(float shore)
	{
		float shallow = shore * 1.6f;
		if (shallow > 1.f)
		{
			shallow = 1.f;
		}
		return Mix(kDeepWaterColor, kShallowWaterColor, shallow);
	}

	// 타일 한 칸을 삼각형 2개로 덧붙인다. 네 모서리가 각자 색과 extra.x를 가져서 칸 안에서 부드럽게 섞인다.
	// 모서리 순서: 0=(왼,아래) 1=(왼,위) 2=(오,위) 3=(오,아래) — 예전 단위 사각형과 같은 삼각형 분할.
	void AppendTileQuad(std::vector<float>& out, float left, float bottom, const Color corners[4], const float extraX[4], float extraY)
	{
		const float offsets[4][2] = { { 0.f, 0.f }, { 0.f, 1.f }, { 1.f, 1.f }, { 1.f, 0.f } };
		const int order[6] = { 0, 1, 2, 0, 2, 3 };

		for (int i = 0; i < 6; ++i)
		{
			int c = order[i];
			AppendVertex(out, left + offsets[c][0], bottom + offsets[c][1], kGroundZ, extraX[c], extraY, corners[c]);
		}
	}

	// ---- 바닥 장식(꽃·풀 포기·자갈) ----
	// 지형 배치와 같은 버퍼에 납작하게 그려 넣어서 드로우콜이 늘지 않는다. 한 칸 안에만 그려서
	// 이웃 청크의 타일에 덮이지 않는다.

	// 중심 (x, y), 반지름 radius인 마름모.
	void AppendDiamond(std::vector<float>& out, float x, float y, float radius, const Color& color)
	{
		AppendVertex(out, x - radius, y, kDecorationZ, 0.f, kMaterialPlain, color);
		AppendVertex(out, x, y + radius, kDecorationZ, 0.f, kMaterialPlain, color);
		AppendVertex(out, x + radius, y, kDecorationZ, 0.f, kMaterialPlain, color);

		AppendVertex(out, x - radius, y, kDecorationZ, 0.f, kMaterialPlain, color);
		AppendVertex(out, x + radius, y, kDecorationZ, 0.f, kMaterialPlain, color);
		AppendVertex(out, x, y - radius, kDecorationZ, 0.f, kMaterialPlain, color);
	}

	// (x, y)에서 angle 방향으로 뻗은 풀잎 하나(뿌리 쪽은 폭 width, 끝은 뾰족한 삼각형).
	void AppendBlade(std::vector<float>& out, float x, float y, float angle, float length, float width, const Color& color)
	{
		float dirX = cosf(angle);
		float dirY = sinf(angle);
		float sideX = -dirY * width * 0.5f;
		float sideY = dirX * width * 0.5f;

		AppendVertex(out, x - sideX, y - sideY, kDecorationZ, 0.f, kMaterialPlain, color);
		AppendVertex(out, x + sideX, y + sideY, kDecorationZ, 0.f, kMaterialPlain, color);
		AppendVertex(out, x + dirX * length, y + dirY * length, kDecorationZ, 0.f, kMaterialPlain, color);
	}

	void AppendTileDecorations(std::vector<float>& out, TileType type, int gx, int gy, float centerX, float centerY, int seed)
	{
		if (type == TileType::Grass)
		{
			// 풀 포기: 잔디보다 짙은 풀잎 4장을 부챗살로.
			if (TileRandom(gx, gy, seed, 1) < 0.45f)
			{
				float tuftX = centerX + (TileRandom(gx, gy, seed, 2) - 0.5f) * 0.6f;
				float tuftY = centerY + (TileRandom(gx, gy, seed, 3) - 0.5f) * 0.6f;
				Color bladeColor = Scale(kGrassColor, 0.62f + 0.12f * TileRandom(gx, gy, seed, 4));
				float baseAngle = TileRandom(gx, gy, seed, 5) * 6.2831853f;

				for (int blade = 0; blade < 4; ++blade)
				{
					float angle = baseAngle + (float)blade * 1.5707963f + (TileRandom(gx, gy, seed, 6 + blade) - 0.5f) * 0.8f;
					float length = 0.1f + 0.06f * TileRandom(gx, gy, seed, 10 + blade);
					AppendBlade(out, tuftX, tuftY, angle, length, 0.035f, bladeColor);
				}
			}

			// 들꽃 무리: 한 가지 색으로 2~4송이(흰색·노랑·분홍·연보라 중 하나) + 노란 꽃술.
			if (TileRandom(gx, gy, seed, 20) < 0.14f)
			{
				const Color kPetalColors[4] =
				{
					{ 0.90f, 0.89f, 0.84f },
					{ 0.98f, 0.82f, 0.25f },
					{ 0.92f, 0.45f, 0.55f },
					{ 0.66f, 0.55f, 0.88f },
				};
				const Color kCenterColor = { 0.95f, 0.78f, 0.2f };

				Color petal = kPetalColors[(int)(TileRandom(gx, gy, seed, 21) * 3.999f)];
				int flowerCount = 2 + (int)(TileRandom(gx, gy, seed, 22) * 2.999f);

				for (int flower = 0; flower < flowerCount; ++flower)
				{
					float flowerX = centerX + (TileRandom(gx, gy, seed, 23 + flower) - 0.5f) * 0.7f;
					float flowerY = centerY + (TileRandom(gx, gy, seed, 27 + flower) - 0.5f) * 0.7f;
					AppendDiamond(out, flowerX, flowerY, 0.055f, petal);
					AppendDiamond(out, flowerX, flowerY, 0.02f, kCenterColor);
				}
			}
		}
		else if (type == TileType::Path)
		{
			// 자갈 0~3개.
			int pebbleCount = (int)(TileRandom(gx, gy, seed, 40) * 3.999f);
			for (int pebble = 0; pebble < pebbleCount; ++pebble)
			{
				float pebbleX = centerX + (TileRandom(gx, gy, seed, 41 + pebble) - 0.5f) * 0.75f;
				float pebbleY = centerY + (TileRandom(gx, gy, seed, 44 + pebble) - 0.5f) * 0.75f;
				Color pebbleColor = Scale(kStoneColor, 0.7f + 0.3f * TileRandom(gx, gy, seed, 47 + pebble));
				AppendDiamond(out, pebbleX, pebbleY, 0.035f + 0.025f * TileRandom(gx, gy, seed, 51 + pebble), pebbleColor);
			}
		}
	}

	// 섬(맵) 둘레 바다의 사각 테 하나(안쪽 반크기 → 바깥 반크기). 네 변을 사다리꼴 하나씩으로 채운다.
	void AppendWaterRing(std::vector<float>& out, float innerHalfX, float innerHalfY, float outerHalfX, float outerHalfY,
		float innerShore, float outerShore, const Color& innerColor, const Color& outerColor)
	{
		const float signs[4][2] = { { -1.f, -1.f }, { 1.f, -1.f }, { 1.f, 1.f }, { -1.f, 1.f } };

		for (int side = 0; side < 4; ++side)
		{
			int next = (side + 1) % 4;

			// 사다리꼴: 안쪽 두 점(side, next) → 바깥 두 점(next, side).
			float ax = signs[side][0] * innerHalfX;
			float ay = signs[side][1] * innerHalfY;
			float bx = signs[next][0] * innerHalfX;
			float by = signs[next][1] * innerHalfY;
			float cx = signs[next][0] * outerHalfX;
			float cy = signs[next][1] * outerHalfY;
			float dx = signs[side][0] * outerHalfX;
			float dy = signs[side][1] * outerHalfY;

			AppendVertex(out, ax, ay, kGroundZ, innerShore, 0.f, innerColor);
			AppendVertex(out, bx, by, kGroundZ, innerShore, 0.f, innerColor);
			AppendVertex(out, cx, cy, kGroundZ, outerShore, 0.f, outerColor);

			AppendVertex(out, ax, ay, kGroundZ, innerShore, 0.f, innerColor);
			AppendVertex(out, cx, cy, kGroundZ, outerShore, 0.f, outerColor);
			AppendVertex(out, dx, dy, kGroundZ, outerShore, 0.f, outerColor);
		}
	}

	// ---- 배치 자리 찾기 ----

	struct Spot
	{
		float x, y;
	};

	float DistanceSq(float ax, float ay, float bx, float by)
	{
		float dx = ax - bx;
		float dy = ay - by;
		return dx * dx + dy * dy;
	}

	bool IsNearAny(const std::vector<Spot>& spots, float x, float y, float distance)
	{
		for (const Spot& spot : spots)
		{
			if (DistanceSq(spot.x, spot.y, x, y) < distance * distance)
			{
				return true;
			}
		}
		return false;
	}

	// (x, y)가 무언가를 세워도 되는 열린 땅인지: 플레이어가 갈 수 있는 맵 안쪽이고, 그 자리와 사방
	// clearance 거리의 네 점이 모두 걸을 수 있는 타일이어야 한다(물가에 바짝 붙은 자리도 피함).
	bool IsOpenGround(const TileMap& tileMap, float x, float y, float clearance)
	{
		float limitX = tileMap.GetWidth() * 0.5f - 1.f;
		float limitY = tileMap.GetHeight() * 0.5f - 1.f;
		if (fabsf(x) > limitX || fabsf(y) > limitY)
		{
			return false;
		}

		return tileMap.IsWorldPositionWalkable(x, y)
			&& tileMap.IsWorldPositionWalkable(x + clearance, y)
			&& tileMap.IsWorldPositionWalkable(x - clearance, y)
			&& tileMap.IsWorldPositionWalkable(x, y + clearance)
			&& tileMap.IsWorldPositionWalkable(x, y - clearance);
	}

	// (x, y)가 열린 땅이면 그대로, 아니면 가까운 곳부터 동심원으로 넓혀 가며 찾은 열린 땅을 돌려준다.
	// 예전엔 마을 기준 고정 좌표를 그대로 써서 약초·짐승·퀘스트 아이템이 호수 안에 생길 수 있었다.
	Spot FindOpenGround(const TileMap& tileMap, float x, float y, float clearance)
	{
		Spot spot = { x, y };
		if (IsOpenGround(tileMap, x, y, clearance))
		{
			return spot;
		}

		const int kAngleSteps = 16;
		for (float radius = 0.5f; radius <= 12.f; radius += 0.5f)
		{
			for (int i = 0; i < kAngleSteps; ++i)
			{
				float angle = (float)i * (6.2831853f / (float)kAngleSteps);
				float candidateX = x + cosf(angle) * radius;
				float candidateY = y + sinf(angle) * radius;

				if (IsOpenGround(tileMap, candidateX, candidateY, clearance))
				{
					spot.x = candidateX;
					spot.y = candidateY;
					return spot;
				}
			}
		}

		return spot; // 찾지 못하면(현실적으로 없음) 원래 자리 그대로.
	}

	// 마을 사람 머리 색(짙은 갈색·검정·적갈색·희끗한 회색)을 번호로 돌려 가며 고른다.
	HumanLook VillagerLook(int index)
	{
		const float kHairColors[4][3] =
		{
			{ 0.20f, 0.13f, 0.07f },
			{ 0.09f, 0.07f, 0.06f },
			{ 0.42f, 0.24f, 0.11f },
			{ 0.55f, 0.50f, 0.44f },
		};

		const float* hair = kHairColors[index % 4];
		HumanLook look;
		look.hairR = hair[0];
		look.hairG = hair[1];
		look.hairB = hair[2];
		return look;
	}

	// 마을 사람 NPC 하나를 (x, y)에 생성한다.
	void SpawnVillager(SceneGraph& scene, float x, float y, float r, float g, float b, int index)
	{
		scene.Spawn<NpcActor>(x, y, 0.9f, r, g, b, VillagerLook(index), kInteractVillager, "Villager");
	}

	// 집 한 채와, 그 집에 붙어 다니는 횃불(자식 액터)을 생성한다. 횃불 위치는 집 기준 로컬 좌표라
	// 집을 옮기면 같이 따라간다. 카메라 쪽(+y면) 문 옆 바닥에 세운다 — 반대편에 두면 집에 가려진다.
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
	int width = tileMap.GetWidth();
	int height = tileMap.GetHeight();
	float halfWidth = width * 0.5f;
	float halfHeight = height * 0.5f;

	// 맵 배치가 매번 다르므로 꽃·풀 배치와 얼룩 무늬도 실행마다 달라지게 한다.
	int seed = (int)(std::random_device{}() & 0x7FFFFFFFu);

	for (int chunkY = 0; chunkY < height; chunkY += kChunkSize)
	{
		for (int chunkX = 0; chunkX < width; chunkX += kChunkSize)
		{
			int endX = (chunkX + kChunkSize < width) ? chunkX + kChunkSize : width;
			int endY = (chunkY + kChunkSize < height) ? chunkY + kChunkSize : height;

			std::vector<float> solidVertices;
			std::vector<float> decorationVertices;
			std::vector<float> waterVertices;

			for (int gy = chunkY; gy < endY; ++gy)
			{
				for (int gx = chunkX; gx < endX; ++gx)
				{
					TileType type = tileMap.GetTile(gx, gy);
					float left = gx - halfWidth;
					float bottom = gy - halfHeight;

					// 이 칸의 네 모서리(격자 좌표). AppendTileQuad의 모서리 순서와 같다.
					const int cornerX[4] = { gx, gx, gx + 1, gx + 1 };
					const int cornerY[4] = { gy, gy + 1, gy + 1, gy };
					Color colors[4];
					float extraX[4];

					if (type == TileType::Water)
					{
						for (int c = 0; c < 4; ++c)
						{
							extraX[c] = WaterShore(tileMap, cornerX[c], cornerY[c]);
							float ripple = 0.95f + 0.1f * ValueNoise((float)cornerX[c] * 0.4f, (float)cornerY[c] * 0.4f, seed + 2);
							colors[c] = Scale(WaterColor(extraX[c]), ripple);
						}
						AppendTileQuad(waterVertices, left, bottom, colors, extraX, 0.f);
					}
					else
					{
						for (int c = 0; c < 4; ++c)
						{
							extraX[c] = 0.f;
							colors[c] = LandCornerColor(tileMap, cornerX[c], cornerY[c], type, seed);
						}
						AppendTileQuad(solidVertices, left, bottom, colors, extraX, LandMaterial(type));
						AppendTileDecorations(decorationVertices, type, gx, gy, left + 0.5f, bottom + 0.5f, seed);
					}
				}
			}

			// 장식은 바닥 타일 위에 보여야 하므로 같은 버퍼의 뒤쪽에 붙인다(한 드로우콜 안에서도 버퍼 순서대로
			// 합성되므로 나중에 들어간 장식이 위에 그려진다).
			solidVertices.insert(solidVertices.end(), decorationVertices.begin(), decorationVertices.end());

			// 청크 중심과, 중심에서 청크 모서리까지의 반지름(대각선의 절반 + 약간의 여유).
			float centerX = (tileMap.GetWorldX(chunkX) + tileMap.GetWorldX(endX - 1)) * 0.5f;
			float centerY = (tileMap.GetWorldY(chunkY) + tileMap.GetWorldY(endY - 1)) * 0.5f;
			float tilesWide = (float)(endX - chunkX);
			float tilesHigh = (float)(endY - chunkY);
			float chunkRadius = sqrtf(tilesWide * tilesWide + tilesHigh * tilesHigh) * 0.5f + 0.1f;

			// 배치 액터의 정점은 이미 월드 좌표로 구워져 있어서 그리기 자체엔 위치가 안 쓰이지만,
			// 뷰 컬링은 액터의 월드 위치+경계 구로 판정하므로 여기서 청크 위치를 정확히 지정해 둬야 한다.
			if (!solidVertices.empty())
			{
				TileBatchActor* solidBatch = scene.Spawn<TileBatchActor>(false);
				solidBatch->SetPosition(centerX, centerY, kGroundZ);
				solidBatch->SetBoundingSphere(chunkRadius);
				solidBatch->Build(renderer, solidVertices);
			}

			if (!waterVertices.empty())
			{
				TileBatchActor* waterBatch = scene.Spawn<TileBatchActor>(true);
				waterBatch->SetPosition(centerX, centerY, kGroundZ);
				waterBatch->SetBoundingSphere(chunkRadius);
				waterBatch->Build(renderer, waterVertices);
			}
		}
	}

	// 섬 둘레 바다: 맵 가장자리의 얕은 물(물가 거품이 이는 곳, 호수 물가와 같은 색) → 조금 밖에서 깊은
	// 물 → 먼바다. 예전엔 맵 밖이 새까만 허공이었는데, 카메라를 멀리 빼면서 가장자리가 자주 보여서
	// 섬처럼 바다를 두른다. 화면을 가장 멀리 빼도 끝이 보이지 않을 만큼 멀리까지 덮는다.
	const float kShallowBand = 3.f;
	const float kOceanReach = 40.f;

	std::vector<float> oceanVertices;
	AppendWaterRing(oceanVertices, halfWidth, halfHeight, halfWidth + kShallowBand, halfHeight + kShallowBand,
		0.5f, 0.f, WaterColor(0.5f), kDeepWaterColor);
	AppendWaterRing(oceanVertices, halfWidth + kShallowBand, halfHeight + kShallowBand, halfWidth + kOceanReach, halfHeight + kOceanReach,
		0.f, 0.f, kDeepWaterColor, kOceanColor);

	float oceanHalf = ((halfWidth > halfHeight) ? halfWidth : halfHeight) + kOceanReach;
	TileBatchActor* ocean = scene.Spawn<TileBatchActor>(true);
	ocean->SetPosition(0.f, 0.f, kGroundZ);
	ocean->SetBoundingSphere(oceanHalf * 1.415f);
	ocean->Build(renderer, oceanVertices);
}

LevelActors SpawnLevelActors(SceneGraph& scene, const LevelLayout& layout, const TileMap& tileMap)
{
	float vx = layout.villageCenterX;
	float vy = layout.villageCenterY;
	float lx = layout.lakeCenterX;
	float ly = layout.lakeCenterY;
	float mapHalfX = tileMap.GetWidth() * 0.5f;
	float mapHalfY = tileMap.GetHeight() * 0.5f;

	// ---- 1) 아이템·짐승 자리를 먼저 정한다(나무가 그 자리를 피해서 심어지도록) ----

	// 퀘스트 아이템: 호수 가장자리 바로 바깥, 마을을 바라보는 쪽 물가. 예전엔 호수 중심 바로 옆(=물 한가운데)에
	// 생겨서, 호수가 조금만 커도 상호작용 반경이 닿지 않아 메인 퀘스트를 끝낼 수 없는 맵이 절반 넘게 나왔다.
	float toVillageX = vx - lx;
	float toVillageY = vy - ly;
	float toVillageLength = sqrtf(toVillageX * toVillageX + toVillageY * toVillageY);
	if (toVillageLength < 0.001f)
	{
		toVillageX = 1.f;
		toVillageY = 0.f;
		toVillageLength = 1.f;
	}

	float shoreDistance = layout.lakeRadius + 0.9f;
	Spot questSpot = FindOpenGround(tileMap, lx + toVillageX / toVillageLength * shoreDistance,
		ly + toVillageY / toVillageLength * shoreDistance, 0.3f);

	// 약초 8개: 마을 기준 원하는 자리 근처의 열린 땅.
	const float kHerbOffsets[8][2] =
	{
		{ 4.5f, 3.f }, { -4.5f, -3.f }, { -3.f, 4.5f }, { 10.f, 6.f },
		{ -10.f, -6.f }, { 7.f, -9.f }, { -8.f, 9.f }, { 1.f, -11.f },
	};
	std::vector<Spot> herbSpots;
	for (int i = 0; i < 8; ++i)
	{
		herbSpots.push_back(FindOpenGround(tileMap, vx + kHerbOffsets[i][0], vy + kHerbOffsets[i][1], 0.4f));
	}

	// 야생 짐승 7마리의 배회 중심(anchor): 물가에서 한 칸 이상 떨어진 열린 땅.
	struct AnimalSpawn
	{
		float offsetX, offsetY;
		float size;
		float r, g, b;
		float phase;
		int hp;
		bool isAggressive;
		const char* name;
	};
	const AnimalSpawn kAnimals[7] =
	{
		{ -6.f, 3.f, 0.9f, 0.52f, 0.36f, 0.20f, 0.f, 20, false, "Deer" },
		{ 6.f, -3.5f, 0.9f, 0.56f, 0.40f, 0.22f, 2.1f, 20, false, "Deer" },
		{ 10.f, 7.f, 0.9f, 0.54f, 0.38f, 0.21f, 1.3f, 20, false, "Deer" },
		{ -9.f, -7.f, 0.9f, 0.50f, 0.35f, 0.19f, 3.4f, 20, false, "Deer" },
		{ -3.f, 6.f, 1.0f, 0.36f, 0.36f, 0.38f, 4.2f, 30, true, "Wolf" },
		{ 8.f, -6.f, 1.0f, 0.33f, 0.33f, 0.35f, 5.6f, 30, true, "Wolf" },
		{ -2.f, -10.f, 1.0f, 0.38f, 0.37f, 0.40f, 0.7f, 30, true, "Wolf" },
	};
	std::vector<Spot> animalSpots;
	for (int i = 0; i < 7; ++i)
	{
		animalSpots.push_back(FindOpenGround(tileMap, vx + kAnimals[i].offsetX, vy + kAnimals[i].offsetY, 1.f));
	}

	// 나무가 덮으면 안 되는 자리: 아이템, 짐승 배회 중심, 플레이어 시작 지점.
	float spawnX = vx;
	float spawnY = vy - 0.5f;
	std::vector<Spot> reserved = herbSpots;
	reserved.push_back(questSpot);
	reserved.insert(reserved.end(), animalSpots.begin(), animalSpots.end());
	reserved.push_back(Spot{ spawnX, spawnY });

	std::vector<Spot> buildingSpots;
	buildingSpots.push_back(Spot{ vx - 2.0f, vy + 2.0f });
	buildingSpots.push_back(Spot{ vx + 2.0f, vy + 2.0f });

	// ---- 2) 숲: 호수·마을·집·다른 나무·예약된 자리와 떨어진 잔디 위에만 나무 36그루 ----
	// 35%는 지중해식 사이프러스, 나머지는 짙은 초록~올리브빛 활엽수로 섞는다.
	std::mt19937 treeRng(std::random_device{}());
	std::uniform_real_distribution<float> treeX(-(mapHalfX - 1.f), mapHalfX - 1.f);
	std::uniform_real_distribution<float> treeY(-(mapHalfY - 1.f), mapHalfY - 1.f);
	std::uniform_real_distribution<float> unit(0.f, 1.f);

	const Color kDeepLeaf = { 0.10f, 0.28f, 0.11f };
	const Color kOliveLeaf = { 0.30f, 0.37f, 0.21f };
	const Color kCypressLeaf = { 0.07f, 0.21f, 0.11f };

	std::vector<Spot> treeSpots;
	int guard = 0;
	while ((int)treeSpots.size() < 36 && guard < 3000)
	{
		++guard;
		float tx = treeX(treeRng);
		float ty = treeY(treeRng);

		if (tileMap.GetTile(tileMap.GetGridX(tx), tileMap.GetGridY(ty)) != TileType::Grass || !IsOpenGround(tileMap, tx, ty, 0.7f))
		{
			continue;
		}

		float villageClear = layout.villageRadius + 1.5f;
		if (DistanceSq(tx, ty, vx, vy) < villageClear * villageClear)
		{
			continue;
		}

		if (IsNearAny(buildingSpots, tx, ty, 2.f) || IsNearAny(treeSpots, tx, ty, 1.3f) || IsNearAny(reserved, tx, ty, 1.1f))
		{
			continue;
		}

		float tint = unit(treeRng) * 0.1f - 0.05f; // 그루마다 색조를 살짝 흔들어 단조로움을 줄임
		if (unit(treeRng) < 0.35f)
		{
			scene.Spawn<TreeActor>(tx, ty, 1.3f + unit(treeRng) * 0.4f,
				kCypressLeaf.r + tint * 0.5f, kCypressLeaf.g + tint, kCypressLeaf.b + tint * 0.5f, TreeKind::Cypress);
		}
		else
		{
			Color leaf = Mix(kDeepLeaf, kOliveLeaf, unit(treeRng) * 0.8f);
			scene.Spawn<TreeActor>(tx, ty, 1.4f + unit(treeRng) * 0.4f,
				leaf.r + tint, leaf.g + tint, leaf.b + tint * 0.5f, TreeKind::Broadleaf);
		}

		treeSpots.push_back(Spot{ tx, ty });
	}

	// ---- 3) 마을: 집 2채(+횃불), 장로, 마을 사람 7명 ----
	for (const Spot& spot : buildingSpots)
	{
		SpawnBuildingWithTorch(scene, spot.x, spot.y);
	}

	// 장로: 흰 머리·흰 수염·발목까지 내려오는 긴 옷·지팡이. 옷은 퀘스트 시작점으로 눈에 띄도록 금빛으로 빛난다.
	HumanLook elderLook;
	elderLook.hairR = 0.90f;
	elderLook.hairG = 0.90f;
	elderLook.hairB = 0.86f;
	elderLook.robe = true;
	elderLook.beard = true;
	elderLook.staff = true;
	scene.Spawn<NpcActor>(vx, vy + 1.5f, 1.f, 2.0f, 1.75f, 0.7f, elderLook, kInteractElder, "Elder");

	SpawnVillager(scene, vx - 1.6f, vy - 1.6f, 0.72f, 0.46f, 0.26f, 0);
	SpawnVillager(scene, vx + 1.6f, vy - 1.6f, 0.30f, 0.42f, 0.66f, 1);
	SpawnVillager(scene, vx, vy - 2.2f, 0.74f, 0.28f, 0.24f, 2);
	SpawnVillager(scene, vx - 2.0f, vy + 0.3f, 0.82f, 0.76f, 0.58f, 3);
	SpawnVillager(scene, vx + 2.0f, vy + 0.3f, 0.40f, 0.52f, 0.34f, 4);
	SpawnVillager(scene, vx - 1.0f, vy + 1.2f, 0.62f, 0.38f, 0.52f, 5);
	SpawnVillager(scene, vx + 1.0f, vy + 1.2f, 0.78f, 0.62f, 0.30f, 6);

	// ---- 4) 아이템과 짐승(1단계에서 정한 자리) ----

	// 퀘스트 아이템: 호숫가에서 금빛으로 빛나는 잃어버린 제물(암포라).
	scene.Spawn<ItemActor>(questSpot.x, questSpot.y, 0.6f, 2.2f, 1.7f, 0.5f, ItemKind::Offering, kInteractQuestItem);

	// 약초: 경험치용, 넓은 숲 곳곳에 흩어져 있음.
	for (const Spot& spot : herbSpots)
	{
		scene.Spawn<ItemActor>(spot.x, spot.y, 0.4f, 0.4f, 1.6f, 0.5f, ItemKind::Herb, kInteractLoot);
	}

	// 야생 짐승: 사슴 4마리(배회만 함) + 늑대 3마리(플레이어를 추적/공격하는 몬스터).
	for (int i = 0; i < 7; ++i)
	{
		const AnimalSpawn& animal = kAnimals[i];
		scene.Spawn<AnimalActor>(animalSpots[i].x, animalSpots[i].y, animal.size, animal.r, animal.g, animal.b,
			animal.phase, animal.hp, animal.isAggressive, animal.name);
	}

	// ---- 5) 플레이어와 표시들 ----
	LevelActors result;

	// 플레이어는 마을 중심 근처에서 시작한다(사망 시에도 여기로 되돌아옴).
	result.player = scene.Spawn<PlayerActor>(spawnX, spawnY);
	scene.SetPlayer(result.player);

	// 플레이어 발밑 위치 마커: 플레이어의 자식이라 이동을 자동으로 따라다닌다.
	RingStyle markerStyle = { 1.0f, 0.92f, 0.6f, 0.8f, 0.1f, 0.14f, 0.08f, 2.5f };
	result.player->AddChild(std::unique_ptr<Actor>(new RingActor(markerStyle, true)));

	// 조준 링: 공격/상호작용 사거리 안의 대상 발밑에 뜬다. 대상은 매 프레임 게임이 지정.
	RingStyle attackStyle = { 1.0f, 0.2f, 0.15f, 0.75f, 0.15f, 0.3f, 0.2f, 6.f };
	result.attackRing = scene.Spawn<RingActor>(attackStyle, false);

	RingStyle interactStyle = { 0.3f, 0.9f, 1.0f, 0.85f, 0.15f, 0.28f, 0.18f, 6.f };
	result.interactRing = scene.Spawn<RingActor>(interactStyle, false);

	// 섬 위를 떠다니는 빛 알갱이(분위기용 시각 효과).
	scene.Spawn<AmbientMotesActor>(mapHalfX, mapHalfY, 70);

	return result;
}
