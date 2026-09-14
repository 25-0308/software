#pragma once

#include "TileMap.h"

// 무작위로 생성된 레벨에서 마을/호수의 중심 위치. BuildLevel()이 이 값을
// 기준으로 건물/NPC/아이템 등을 배치한다.
struct LevelLayout
{
	float villageCenterX = 0.f;
	float villageCenterY = 0.f;
	float lakeCenterX = 0.f;
	float lakeCenterY = 0.f;
};

// 마을(Stone)과 호수(Water)를 무작위 위치/크기로 배치한다. 호수 때문에 물에
// 막혀 갈 수 없는 영역이 생기면, 마을에서부터 도달 가능한 가장 가까운 칸까지
// 직선으로 길(Path)을 뚫어 맵 전체가 하나로 연결되도록 보장한다.
LevelLayout GenerateVillageLevel(TileMap& tileMap);
