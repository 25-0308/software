#pragma once

#include <vector>

#include "LevelBuilder.h"

class Camera;
class Renderer;
class SceneGraph;
class TileMap;

// 미니맵·큰 지도(MapScreen)의 표시 색. 지도 화면의 범례도 이 색을 그대로 쓴다.
namespace MapColors
{
	struct Rgb
	{
		float r, g, b;
	};

	const Rgb kPlayer = { 1.0f, 1.0f, 1.0f };          // 플레이어: 흰색(바라보는 쪽을 가리키는 화살촉)
	const Rgb kObjective = { 1.0f, 0.78f, 0.25f };     // 지금 목표: 금빛 마름모 + 퍼져 나가는 고리
	const Rgb kStoryCharacter = { 1.0f, 0.85f, 0.2f }; // 이야기 인물(신·피티아 등): 노랑
	const Rgb kVillager = { 0.92f, 0.90f, 0.80f };     // 마을 사람: 옅은 베이지
	const Rgb kSignpost = { 0.55f, 0.85f, 1.0f };      // 이정표(지역 이동): 밝은 하늘색 마름모
	const Rgb kStoryItem = { 0.4f, 0.9f, 1.0f };       // 이야기 물건(월계수 가지·장부 등): 하늘색
	const Rgb kHerb = { 0.45f, 0.95f, 0.55f };         // 약초: 연두
	const Rgb kDeer = { 0.85f, 0.65f, 0.4f };          // 사슴: 갈색
	const Rgb kWolf = { 0.95f, 0.2f, 0.2f };           // 늑대(몬스터): 빨강
	const Rgb kShadow = { 0.7f, 0.3f, 0.95f };         // 균열의 그림자 괴물: 보라
}

// 맵 전체를 위에서 내려다본 지도. 기본은 화면 오른쪽 위에 작게 뜨는 미니맵이고, 패널 위치·크기를 주면 같은
// 코드로 큰 지도(MapScreen, M 키)도 그린다. 맵 전체를 카메라와 같은 방향(같은 yaw, 같은 좌우/상하 반전)으로
// 놓고, 위에서 내려다본 모양(세로로 눌리지 않은 마름모)으로 보여준다 — 지도에서 위로 가는 것이 본 화면에서도
// 같은 방향으로 가는 것처럼 보이게 하려는 것이다(플레이어 화살촉도 화면에서 바라보는 쪽을 가리킨다).
//
// 드로우 콜을 아끼려고 타일 1024개를 하나씩 그리지 않는다:
//  - 지형(물/돌바닥/길/바위)과 나무/건물처럼 안 변하는 것은 화면 좌표의 삼각형 목록으로 한 번만 만들어 두고
//    종류별로 한 번에 그린다(카메라 방향이 바뀌거나 다른 지역으로 옮겨 Invalidate될 때만 다시 만든다).
//  - 인물/아이템/짐승/이정표/목표/플레이어 표시는 매 프레임 색깔별로 모아서 한 번에 그린다.
//    이야기 진행 조건으로 숨은 액터(IsVisible() == false)는 표시하지 않는다.
// 후처리(블룸 등)가 끝난 뒤 HUD처럼 화면 고정 좌표계(800x600)로 그린다.
class MiniMap
{
public:
	// 기본: 화면 오른쪽 위 구석의 128x128 미니맵.
	MiniMap();

	// 패널 가운데(화면 픽셀, 원점은 왼쪽 아래)와 반 변(픽셀), 점·마름모 같은 표시의 크기 배율(큰 지도는 크게).
	MiniMap(float centerX, float centerY, float halfSize, float markerScale);

	// objectives: 지금 목표 대상들의 월드 위치(StoryDirector::GetObjectiveSpots) — 금빛 마름모가 숨 쉬고
	// 그 둘레로 금빛 고리가 퍼져 나간다.
	void Draw(Renderer& renderer, SceneGraph& scene, const TileMap& tileMap, const Camera& camera,
		const std::vector<Spot>& objectives, float time);

	// 다른 지역으로 옮겨 맵이 바뀌었을 때 부른다: 다음 Draw에서 지형·나무·건물을 새 맵으로 다시 만든다.
	void Invalidate() { m_StaticBuilt = false; }

	float GetPanelCenterX() const { return m_PanelCenterX; }
	float GetPanelCenterY() const { return m_PanelCenterY; }
	float GetPanelHalfSize() const { return m_PanelHalfSize; }

private:
	// 월드 좌표를 지도 화면 좌표(픽셀)로 옮기는 변환. 카메라와 같은 방향으로 놓으려고
	// yaw 회전 → 좌우/상하 반전 → 배율 → 패널 중심 이동 순으로 적용한다.
	struct MapTransform
	{
		float cosYaw = 1.f;
		float sinYaw = 0.f;
		float scaleX = 1.f; // 반전이면 음수
		float scaleY = 1.f;
		float centerX = 0.f;
		float centerY = 0.f;

		void Apply(float worldX, float worldY, float& screenX, float& screenY) const;

		// 방향(이동 없이 회전·반전·배율만). 바라보는 방향 화살촉에 쓴다.
		void ApplyDirection(float worldX, float worldY, float& screenX, float& screenY) const;
	};

	void RebuildStatic(SceneGraph& scene, const TileMap& tileMap);

	float m_PanelCenterX;
	float m_PanelCenterY;
	float m_PanelHalfSize;
	float m_MarkerScale;

	// 카메라 방향이 바뀌었는지 보기 위해 마지막으로 정적 지오메트리를 만들 때의 값을 기억한다.
	bool m_StaticBuilt = false;
	float m_BuiltYaw = 0.f;
	bool m_BuiltFlipH = false;
	bool m_BuiltFlipV = false;

	MapTransform m_Transform;

	// 한 번 만들어 두는 정적 지오메트리(화면 좌표 삼각형: x,y,z 반복).
	std::vector<float> m_Grass;
	std::vector<float> m_Water;
	std::vector<float> m_Stone;
	std::vector<float> m_Path;
	std::vector<float> m_Rock;
	std::vector<float> m_Trees;
	std::vector<float> m_Buildings;

	// 매 프레임 채우는 동적 표시(재사용해서 메모리 할당을 줄인다).
	std::vector<float> m_Villagers;
	std::vector<float> m_StoryCharacters;
	std::vector<float> m_Herbs;
	std::vector<float> m_StoryItems;
	std::vector<float> m_Deer;
	std::vector<float> m_Wolves;
	std::vector<float> m_Shadows;
	std::vector<float> m_Signposts;
	std::vector<float> m_ObjectivePings;
	std::vector<float> m_ObjectiveOutlines;
	std::vector<float> m_Objectives;
	std::vector<float> m_PlayerOutline;
	std::vector<float> m_Player;
};
