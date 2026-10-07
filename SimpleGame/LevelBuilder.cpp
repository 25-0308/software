#include "stdafx.h"
#include "LevelBuilder.h"

#include <cmath>
#include <memory>
#include <vector>

#include "CharacterActors.h"
#include "LandmarkActors.h"
#include "LevelGenerator.h"
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
	const Color kStoneColor = { 0.66f, 0.63f, 0.57f };      // 마을·신전 돌바닥(밝은 석회암)
	const Color kPathColor = { 0.52f, 0.41f, 0.27f };
	const Color kRockColor = { 0.46f, 0.43f, 0.39f };       // 바위 산·절벽(지나갈 수 없음)
	const Color kSandColor = { 0.80f, 0.72f, 0.52f };       // 호수·바다 물가의 모래
	const Color kShallowWaterColor = { 0.20f, 0.52f, 0.58f };
	const Color kDeepWaterColor = { 0.08f, 0.24f, 0.40f };
	const Color kOceanColor = { 0.05f, 0.17f, 0.32f };      // 섬에서 멀리 떨어진 먼바다

	// TileBatchSolid.fs가 재질별 잔무늬를 고르는 번호(지형 정점의 extra.y).
	const float kMaterialGrass = 0.f;
	const float kMaterialStone = 1.f;
	const float kMaterialPath = 2.f;
	const float kMaterialRock = 3.f;
	const float kMaterialPlain = 9.f; // 무늬 없이 색 그대로(꽃·풀 같은 장식)

	// 바닥 높이. 캐릭터·건물·그림자가 서 있는 z = 0과 같아야 한다 — 예전엔 -0.5에 그려서, 이 카메라
	// 각도에선 캐릭터가 실제로 서 있는 타일보다 반 칸쯤 어긋난 자리에 서 있는 것처럼 보였다(물가 충돌이
	// 눈에 보이는 물가와 안 맞던 원인). 바닥은 항상 가장 먼저 그리므로 그리기 순서엔 영향이 없다.
	const float kGroundZ = 0.f;
	const float kDecorationZ = 0.002f; // 바닥에 그려 넣는 꽃·풀 장식(바닥보다 아주 살짝 위)

	const int kChunkSize = 8;

	// 타일 한 칸 안의 장식 배치용 난수. salt를 바꾸면 같은 칸에서 서로 다른 난수가 나온다.
	float TileRandom(int gx, int gy, int seed, int salt)
	{
		return Terrain::Hash(gx + salt * 1013, gy - salt * 719, seed + salt * 7);
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
		int rock = 0;
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
				case TileType::Rock:  ++counts.rock; break;
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
		case TileType::Rock:  return kRockColor;
		default:              return kGrassColor;
		}
	}

	float LandMaterial(TileType type)
	{
		switch (type)
		{
		case TileType::Stone: return kMaterialStone;
		case TileType::Path:  return kMaterialPath;
		case TileType::Rock:  return kMaterialRock;
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
		int land = counts.grass + counts.stone + counts.path + counts.rock;
		if (land > 0)
		{
			float inv = 1.f / (float)land;
			Color average =
			{
				(kGrassColor.r * counts.grass + kStoneColor.r * counts.stone + kPathColor.r * counts.path + kRockColor.r * counts.rock) * inv,
				(kGrassColor.g * counts.grass + kStoneColor.g * counts.stone + kPathColor.g * counts.path + kRockColor.g * counts.rock) * inv,
				(kGrassColor.b * counts.grass + kStoneColor.b * counts.stone + kPathColor.b * counts.path + kRockColor.b * counts.rock) * inv,
			};
			color = Mix(ownColor, average, 0.45f);
		}

		// 넓은 밝기 얼룩 + 잔디에 섞이는 마른 풀빛. 둘 다 모서리 위치만의 함수라 이웃 타일과 이어진다.
		float patch = Terrain::Noise((float)cx * 0.18f, (float)cy * 0.18f, seed);
		if (own == TileType::Grass)
		{
			float dry = Terrain::Noise((float)cx * 0.33f + 11.f, (float)cy * 0.33f + 7.f, seed + 1);
			color = Mix(color, kDryGrassColor, dry * dry * 0.6f);
		}
		color = Scale(color, 0.86f + 0.26f * patch);

		// 물(호수·바다)에 닿은 모서리는 모래색 → 물가를 따라 모래사장이 생긴다(바위 절벽은 그대로 둔다).
		if (counts.water > 0 && own != TileType::Rock)
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
		else if (type == TileType::Path || type == TileType::Rock)
		{
			// 자갈 0~3개(바위 산 위엔 좀 더 큰 돌 부스러기).
			float sizeScale = (type == TileType::Rock) ? 1.6f : 1.f;
			int pebbleCount = (int)(TileRandom(gx, gy, seed, 40) * 3.999f);
			for (int pebble = 0; pebble < pebbleCount; ++pebble)
			{
				float pebbleX = centerX + (TileRandom(gx, gy, seed, 41 + pebble) - 0.5f) * 0.75f;
				float pebbleY = centerY + (TileRandom(gx, gy, seed, 44 + pebble) - 0.5f) * 0.75f;
				Color pebbleColor = Scale(kStoneColor, 0.65f + 0.3f * TileRandom(gx, gy, seed, 47 + pebble));
				float radius = (0.035f + 0.025f * TileRandom(gx, gy, seed, 51 + pebble)) * sizeScale;
				AppendDiamond(out, pebbleX, pebbleY, radius, pebbleColor);
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

	float DistanceSq(float ax, float ay, float bx, float by)
	{
		float dx = ax - bx;
		float dy = ay - by;
		return dx * dx + dy * dy;
	}
}

void SpawnTileActors(SceneGraph& scene, Renderer& renderer, const TileMap& tileMap, int seed)
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
							float ripple = 0.95f + 0.1f * Terrain::Noise((float)cornerX[c] * 0.4f, (float)cornerY[c] * 0.4f, seed + 2);
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

std::vector<Spot> ScatterTrees(SceneGraph& scene, const TileMap& tileMap, std::mt19937& rng, int count,
	const std::vector<Spot>& avoid, float avoidDistance, float cypressRatio)
{
	float mapHalfX = tileMap.GetWidth() * 0.5f;
	float mapHalfY = tileMap.GetHeight() * 0.5f;

	std::uniform_real_distribution<float> treeX(-(mapHalfX - 1.f), mapHalfX - 1.f);
	std::uniform_real_distribution<float> treeY(-(mapHalfY - 1.f), mapHalfY - 1.f);
	std::uniform_real_distribution<float> unit(0.f, 1.f);

	// 잎 색: 활엽수는 짙은 초록 ~ 올리브(은빛 도는 녹색) 사이, 사이프러스는 짙은 초록 — 지중해 숲 느낌.
	const Color kDeepLeaf = { 0.10f, 0.28f, 0.11f };
	const Color kOliveLeaf = { 0.30f, 0.37f, 0.21f };
	const Color kCypressLeaf = { 0.07f, 0.21f, 0.11f };

	std::vector<Spot> trees;
	int guard = 0;
	while ((int)trees.size() < count && guard < count * 80)
	{
		++guard;
		float tx = treeX(rng);
		float ty = treeY(rng);

		if (tileMap.GetTile(tileMap.GetGridX(tx), tileMap.GetGridY(ty)) != TileType::Grass || !IsOpenGround(tileMap, tx, ty, 0.7f))
		{
			continue;
		}

		if (IsNearAny(avoid, tx, ty, avoidDistance) || IsNearAny(trees, tx, ty, 1.3f))
		{
			continue;
		}

		float tint = unit(rng) * 0.1f - 0.05f; // 그루마다 색조를 살짝 흔들어 단조로움을 줄임
		if (unit(rng) < cypressRatio)
		{
			scene.Spawn<TreeActor>(tx, ty, 1.3f + unit(rng) * 0.4f,
				kCypressLeaf.r + tint * 0.5f, kCypressLeaf.g + tint, kCypressLeaf.b + tint * 0.5f, TreeKind::Cypress);
		}
		else
		{
			Color leaf = Mix(kDeepLeaf, kOliveLeaf, unit(rng) * 0.8f);
			scene.Spawn<TreeActor>(tx, ty, 1.4f + unit(rng) * 0.4f,
				leaf.r + tint, leaf.g + tint, leaf.b + tint * 0.5f, TreeKind::Broadleaf);
		}

		trees.push_back(Spot{ tx, ty });
	}

	return trees;
}

void ScatterRocks(SceneGraph& scene, const TileMap& tileMap, std::mt19937& rng, float density, bool snowy)
{
	std::uniform_real_distribution<float> unit(0.f, 1.f);
	int width = tileMap.GetWidth();
	int height = tileMap.GetHeight();

	for (int gy = 0; gy < height; ++gy)
	{
		for (int gx = 0; gx < width; ++gx)
		{
			if (tileMap.GetTile(gx, gy) != TileType::Rock)
			{
				continue;
			}

			// 걸을 수 있는 칸과 맞닿은 가장자리엔 촘촘히, 안쪽엔 드문드문 놓는다(안쪽은 바위 바닥색만으로도 산처럼 보임).
			bool edge = tileMap.IsWalkable(gx + 1, gy) || tileMap.IsWalkable(gx - 1, gy)
				|| tileMap.IsWalkable(gx, gy + 1) || tileMap.IsWalkable(gx, gy - 1);
			float chance = edge ? density : density * 0.25f;
			if (unit(rng) >= chance)
			{
				continue;
			}

			float x = tileMap.GetWorldX(gx) + (unit(rng) - 0.5f) * 0.5f;
			float y = tileMap.GetWorldY(gy) + (unit(rng) - 0.5f) * 0.5f;
			float size = 0.9f + unit(rng) * 0.6f;
			scene.Spawn<RockActor>(x, y, size, gx * 131 + gy * 17, snowy);
		}
	}
}

void SpawnHerb(SceneGraph& scene, const TileMap& tileMap, float x, float y)
{
	Spot spot = FindOpenGround(tileMap, x, y, 0.4f);
	scene.Spawn<ItemActor>(spot.x, spot.y, 0.4f, 0.4f, 1.6f, 0.5f, ItemKind::Herb, kInteractLoot);
}

AnimalActor* SpawnAnimal(SceneGraph& scene, const TileMap& tileMap, AnimalKind kind, float x, float y, float phase)
{
	Spot spot = FindOpenGround(tileMap, x, y, 1.f);
	return scene.Spawn<AnimalActor>(kind, spot.x, spot.y, phase);
}

NpcActor* SpawnStoryNpc(SceneGraph& scene, float x, float y, float size, float r, float g, float b, const HumanLook& look,
	const char* storyId, const char* nameTag, const char* presence)
{
	NpcActor* npc = scene.Spawn<NpcActor>(x, y, size, r, g, b, look, kInteractStory, nameTag);
	npc->SetStoryId(storyId);
	npc->SetPresence(presence);
	npc->SetFacing(0.7853982f); // 기본은 카메라 쪽(+x,+y)을 바라보게 해서 얼굴이 보이도록
	return npc;
}

LevelActors SpawnPlayerAndMarkers(SceneGraph& scene, float x, float y)
{
	LevelActors result;

	// 플레이어는 지역의 시작 지점(또는 도착한 이정표 앞)에서 시작한다(쓰러지면 여기로 되돌아옴).
	result.player = scene.Spawn<PlayerActor>(x, y);
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

void SpawnAmbience(SceneGraph& scene, const TileMap& tileMap)
{
	scene.Spawn<AmbientMotesActor>(tileMap.GetWidth() * 0.5f, tileMap.GetHeight() * 0.5f, 70);
}
