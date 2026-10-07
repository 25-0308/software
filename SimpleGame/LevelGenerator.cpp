#include "stdafx.h"
#include "LevelGenerator.h"

#include <cmath>
#include <queue>
#include <utility>
#include <vector>

namespace
{
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
}

void Terrain::Fill(TileMap& tileMap, TileType type)
{
	for (int gy = 0; gy < tileMap.GetHeight(); ++gy)
	{
		for (int gx = 0; gx < tileMap.GetWidth(); ++gx)
		{
			tileMap.SetTile(gx, gy, type);
		}
	}
}

void Terrain::PaintDisk(TileMap& tileMap, float centerX, float centerY, float radius, TileType type)
{
	PaintWhere(tileMap, [centerX, centerY, radius](float wx, float wy)
	{
		float dx = wx - centerX;
		float dy = wy - centerY;
		return dx * dx + dy * dy <= radius * radius;
	}, type);
}

void Terrain::PaintLine(TileMap& tileMap, float x0, float y0, float x1, float y1, float halfWidth, TileType type)
{
	float segmentX = x1 - x0;
	float segmentY = y1 - y0;
	float lengthSq = segmentX * segmentX + segmentY * segmentY;

	PaintWhere(tileMap, [=](float wx, float wy)
	{
		// 타일 중심에서 선분까지의 최단 거리(선분 위 가장 가까운 점까지).
		float t = 0.f;
		if (lengthSq > 0.000001f)
		{
			t = ((wx - x0) * segmentX + (wy - y0) * segmentY) / lengthSq;
			if (t < 0.f) t = 0.f;
			if (t > 1.f) t = 1.f;
		}

		float dx = wx - (x0 + segmentX * t);
		float dy = wy - (y0 + segmentY * t);
		return dx * dx + dy * dy <= halfWidth * halfWidth;
	}, type);
}

void Terrain::PaintWhere(TileMap& tileMap, const std::function<bool(float, float)>& predicate, TileType type)
{
	for (int gy = 0; gy < tileMap.GetHeight(); ++gy)
	{
		for (int gx = 0; gx < tileMap.GetWidth(); ++gx)
		{
			if (predicate(tileMap.GetWorldX(gx), tileMap.GetWorldY(gy)))
			{
				tileMap.SetTile(gx, gy, type);
			}
		}
	}
}

// 시작 지점에서 도달하지 못하는 통행 가능 칸이 있으면, 그 칸에서 시작 지점 쪽으로 직선으로 길을 뚫어
// 도달 가능한 영역에 닿을 때까지 이어 붙인다. 더 이상 고립된 칸이 없을 때까지 반복한다(매 반복마다
// 다시 검증하므로 항상 정확하다).
void Terrain::EnsureFullyConnected(TileMap& tileMap, float startWorldX, float startWorldY)
{
	int width = tileMap.GetWidth();
	int height = tileMap.GetHeight();
	int startX = tileMap.GetGridX(startWorldX);
	int startY = tileMap.GetGridY(startWorldY);

	if (startX < 0 || startX >= width || startY < 0 || startY >= height)
	{
		return;
	}

	// 시작 칸 자체가 막혀 있으면 길로 바꿔서라도 출발점을 보장한다.
	if (!tileMap.IsWalkable(startX, startY))
	{
		tileMap.SetTile(startX, startY, TileType::Path);
	}

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

float Terrain::Hash(int x, int y, int seed)
{
	unsigned int h = (unsigned int)x * 374761393u + (unsigned int)y * 668265263u + (unsigned int)seed * 2246822519u;
	h = (h ^ (h >> 13)) * 1274126177u;
	h ^= h >> 16;
	return (float)(h & 0xFFFFFFu) / 16777215.f;
}

float Terrain::Noise(float x, float y, int seed)
{
	int x0 = (int)floorf(x);
	int y0 = (int)floorf(y);
	float fx = x - (float)x0;
	float fy = y - (float)y0;
	float ux = fx * fx * (3.f - 2.f * fx);
	float uy = fy * fy * (3.f - 2.f * fy);

	float a = Hash(x0, y0, seed);
	float b = Hash(x0 + 1, y0, seed);
	float c = Hash(x0, y0 + 1, seed);
	float d = Hash(x0 + 1, y0 + 1, seed);

	float bottom = a + (b - a) * ux;
	float top = c + (d - c) * ux;
	return bottom + (top - bottom) * uy;
}
