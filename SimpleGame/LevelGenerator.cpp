#include "stdafx.h"
#include "LevelGenerator.h"

#include <cmath>
#include <queue>
#include <random>
#include <utility>
#include <vector>

namespace
{
	void CarveDisk(TileMap& tileMap, float centerX, float centerY, float radius, TileType type)
	{
		int width = tileMap.GetWidth();
		int height = tileMap.GetHeight();

		for (int gy = 0; gy < height; ++gy)
		{
			for (int gx = 0; gx < width; ++gx)
			{
				float wx = tileMap.GetWorldX(gx);
				float wy = tileMap.GetWorldY(gy);
				float dx = wx - centerX;
				float dy = wy - centerY;

				if (dx * dx + dy * dy <= radius * radius)
				{
					tileMap.SetTile(gx, gy, type);
				}
			}
		}
	}

	// startX,startY에서 4방향으로 이동 가능한 타일만 따라가며 도달 가능한
	// 칸을 표시한다 (BFS).
	std::vector<bool> FloodFillReachable(const TileMap& tileMap, int startX, int startY)
	{
		int width = tileMap.GetWidth();
		int height = tileMap.GetHeight();
		std::vector<bool> reached(width * height, false);

		if (!tileMap.IsWalkable(startX, startY))
		{
			return reached;
		}

		std::queue<std::pair<int, int>> frontier;
		frontier.push(std::make_pair(startX, startY));
		reached[startY * width + startX] = true;

		const int dx[4] = { 1, -1, 0, 0 };
		const int dy[4] = { 0, 0, 1, -1 };

		while (!frontier.empty())
		{
			std::pair<int, int> current = frontier.front();
			frontier.pop();

			for (int dir = 0; dir < 4; ++dir)
			{
				int nx = current.first + dx[dir];
				int ny = current.second + dy[dir];

				if (nx < 0 || nx >= width || ny < 0 || ny >= height)
				{
					continue;
				}
				if (reached[ny * width + nx] || !tileMap.IsWalkable(nx, ny))
				{
					continue;
				}

				reached[ny * width + nx] = true;
				frontier.push(std::make_pair(nx, ny));
			}
		}

		return reached;
	}

	// startX,startY에서 도달하지 못하는 통행 가능 칸이 있으면, 그 칸에서
	// 도달 가능한 영역까지 직선으로 길을 뚫어 연결한다. 더 이상 고립된 칸이
	// 없을 때까지 반복한다 (매 반복마다 다시 검증하므로 항상 정확하다).
	void EnsureFullyConnected(TileMap& tileMap, int startX, int startY)
	{
		int width = tileMap.GetWidth();
		int height = tileMap.GetHeight();

		for (int guard = 0; guard < width * height; ++guard)
		{
			std::vector<bool> reached = FloodFillReachable(tileMap, startX, startY);

			int isolatedX = -1;
			int isolatedY = -1;
			for (int gy = 0; gy < height && isolatedX < 0; ++gy)
			{
				for (int gx = 0; gx < width; ++gx)
				{
					if (tileMap.IsWalkable(gx, gy) && !reached[gy * width + gx])
					{
						isolatedX = gx;
						isolatedY = gy;
						break;
					}
				}
			}

			if (isolatedX < 0)
			{
				return; // 고립된 칸이 없음 - 완료.
			}

			float x = (float)isolatedX;
			float y = (float)isolatedY;
			float dx = (float)startX - x;
			float dy = (float)startY - y;
			float dist = sqrtf(dx * dx + dy * dy);
			if (dist < 0.001f)
			{
				return;
			}
			dx /= dist;
			dy /= dist;

			for (int step = 0; step < width + height; ++step)
			{
				int gx = (int)roundf(x);
				int gy = (int)roundf(y);

				if (gx >= 0 && gx < width && gy >= 0 && gy < height)
				{
					if (!tileMap.IsWalkable(gx, gy))
					{
						tileMap.SetTile(gx, gy, TileType::Path);
					}
					if (reached[gy * width + gx])
					{
						break; // 도달 가능한 영역에 닿았으니 이 고립 지점은 해결됨.
					}
				}

				x += dx;
				y += dy;
			}
			// 다음 반복에서 flood fill을 다시 돌려 결과를 검증한다.
		}
	}
}

LevelLayout GenerateVillageLevel(TileMap& tileMap)
{
	std::mt19937 rng(std::random_device{}());

	int width = tileMap.GetWidth();
	int height = tileMap.GetHeight();

	for (int gy = 0; gy < height; ++gy)
	{
		for (int gx = 0; gx < width; ++gx)
		{
			tileMap.SetTile(gx, gy, TileType::Grass);
		}
	}

	std::uniform_real_distribution<float> villagePosDist(-3.f, 3.f);
	std::uniform_real_distribution<float> villageRadiusDist(2.2f, 2.6f);
	std::uniform_real_distribution<float> angleDist(0.f, 6.2831853f);
	std::uniform_real_distribution<float> lakeDistDist(6.f, 10.f); // 맵이 커진 만큼 호수를 마을에서 더 떨어뜨려 배치.
	std::uniform_real_distribution<float> lakeRadiusDist(1.8f, 2.6f);

	LevelLayout layout;
	layout.villageCenterX = villagePosDist(rng);
	layout.villageCenterY = villagePosDist(rng);
	float villageRadius = villageRadiusDist(rng);

	float angle = angleDist(rng);
	float lakeDist = lakeDistDist(rng);
	layout.lakeCenterX = layout.villageCenterX + cosf(angle) * lakeDist;
	layout.lakeCenterY = layout.villageCenterY + sinf(angle) * lakeDist;
	float lakeRadius = lakeRadiusDist(rng);

	// 맵 경계 안쪽으로 클램프.
	float halfExtent = width * 0.5f - 1.5f;
	if (layout.lakeCenterX < -halfExtent) layout.lakeCenterX = -halfExtent;
	if (layout.lakeCenterX > halfExtent) layout.lakeCenterX = halfExtent;
	if (layout.lakeCenterY < -halfExtent) layout.lakeCenterY = -halfExtent;
	if (layout.lakeCenterY > halfExtent) layout.lakeCenterY = halfExtent;

	CarveDisk(tileMap, layout.lakeCenterX, layout.lakeCenterY, lakeRadius, TileType::Water);
	// 호수가 마을 자리를 침범했을 수 있으니 마을을 나중에 다시 그려서
	// 항상 마을이 우선하도록 한다 (겹침 방지를 정확히 계산하는 대신
	// 이렇게 처리하는 편이 훨씬 단순하고 견고하다).
	CarveDisk(tileMap, layout.villageCenterX, layout.villageCenterY, villageRadius, TileType::Stone);

	int startX = tileMap.GetGridX(layout.villageCenterX);
	int startY = tileMap.GetGridY(layout.villageCenterY);
	EnsureFullyConnected(tileMap, startX, startY);

	return layout;
}
