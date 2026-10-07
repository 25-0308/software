#pragma once

#include <functional>

#include "TileMap.h"

// 맵 지형(타일)을 칠하는 도구 모음. 지역(맵)마다 어떤 모양으로 칠할지는 WorldMaps.cpp가 정하고,
// 여기에는 어느 지역에서나 쓰는 붓만 둔다(원·선·조건 칠하기, 연결성 보장, 결정적 노이즈).
// 좌표는 모두 월드 좌표(타일 중심 기준)다.
namespace Terrain
{
	void Fill(TileMap& tileMap, TileType type);

	// 타일 중심이 (centerX, centerY)에서 radius 안에 드는 칸을 type으로 칠한다.
	void PaintDisk(TileMap& tileMap, float centerX, float centerY, float radius, TileType type);

	// 선분 (x0,y0)-(x1,y1)에서 halfWidth 안에 드는 칸을 type으로 칠한다(길·강·골목).
	void PaintLine(TileMap& tileMap, float x0, float y0, float x1, float y1, float halfWidth, TileType type);

	// 타일 중심의 월드 좌표를 받아 true를 돌려주는 칸을 type으로 칠한다(산자락 띠, 구불구불한 해안선 등 자유 모양).
	void PaintWhere(TileMap& tileMap, const std::function<bool(float, float)>& predicate, TileType type);

	// 지나갈 수 있는 칸이 모두 (startX, startY)에서 걸어서 닿을 수 있도록, 고립된 칸이 있으면 그 칸에서
	// 시작 지점 쪽으로 길(Path)을 뚫는다. 지형을 손으로 그리다 실수로 막힌 곳이 생겨도 안전하게 해 준다.
	void EnsureFullyConnected(TileMap& tileMap, float startX, float startY);

	// 정수 격자점마다 고정된 0~1 난수(같은 입력이면 항상 같은 값).
	float Hash(int x, int y, int seed);

	// 격자점 난수를 부드럽게 보간한 값 노이즈(0~1). 경계를 자연스럽게 흔들 때 쓴다.
	float Noise(float x, float y, int seed);
}
