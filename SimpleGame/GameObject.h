#pragma once

enum class EntityType
{
	Prop,     // 장식용, 상호작용 불가 (나무 등)
	Player,
	NPC,
	Item,
	Animal,   // 야생 짐승
	Building, // 건물 (벽+지붕 다중 파츠로 렌더링됨)
	Water,    // 물 타일 전용 태그 (렌더링 시 물 셰이더로 분기)
	Fire,     // 횃불/모닥불 전용 태그 (렌더링 시 불 셰이더로 분기)
};

// 씬에 배치되는 월드 스페이스 오브젝트. (x, y, z) 위치에 `size` 배율로
// 그려지는 단색 사각형이 기본형이며, `type`에 따라 렌더링 시 캐릭터(몸통+머
// 리+다리)나 건물(벽+지붕) 등 여러 파츠로 조립되어 그려질 수 있다.
// `interactId`는 0이면 상호작용 불가, 그 외 값이면 SimpleGame.cpp의
// HandleInteract()가 분기하는 게임별 상호작용 식별자다.
struct GameObject
{
	float x = 0.f, y = 0.f, z = 0.f;
	float size = 1.f;
	float r = 1.f, g = 1.f, b = 1.f, a = 1.f;
	EntityType type = EntityType::Prop;
	int interactId = 0;
};
