#pragma once

#include <vector>

class Camera;
class Renderer;
class SceneGraph;
class TileMap;

// 화면 오른쪽 위에 작게 뜨는 미니맵. 맵 전체를 카메라와 같은 방향(같은 yaw, 같은 좌우/상하 반전)으로
// 놓고, 위에서 내려다본 모양(세로로 눌리지 않은 마름모)으로 보여준다 — 미니맵에서 위로 가는 것이
// 본 화면에서도 같은 방향으로 가는 것처럼 보이게 하려는 것이다.
//
// 드로우 콜을 아끼려고 타일 1024개를 하나씩 그리지 않는다:
//  - 지형(물/돌바닥/길)과 나무/건물처럼 안 변하는 것은 화면 좌표의 삼각형 목록으로 한 번만 만들어 두고
//    종류별로 한 번에 그린다(카메라 방향이 바뀔 때만 다시 만든다).
//  - NPC/아이템/짐승/플레이어 표시는 매 프레임 색깔별로 모아서 한 번에 그린다.
// 후처리(블룸 등)가 끝난 뒤 HUD처럼 화면 고정 좌표계(800x600)로 그린다.
class MiniMap
{
public:
	void Draw(Renderer& renderer, SceneGraph& scene, const TileMap& tileMap, const Camera& camera);

private:
	// 월드 좌표를 미니맵 화면 좌표(픽셀)로 옮기는 변환. 카메라와 같은 방향으로 놓으려고
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
	};

	void RebuildStatic(SceneGraph& scene, const TileMap& tileMap);

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
	std::vector<float> m_Trees;
	std::vector<float> m_Buildings;

	// 매 프레임 채우는 동적 표시(재사용해서 메모리 할당을 줄인다).
	std::vector<float> m_Villagers;
	std::vector<float> m_Elders;
	std::vector<float> m_Herbs;
	std::vector<float> m_QuestItems;
	std::vector<float> m_Deer;
	std::vector<float> m_Wolves;
	std::vector<float> m_Player;
};
