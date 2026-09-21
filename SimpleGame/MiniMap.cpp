#include "stdafx.h"
#include "MiniMap.h"

#include <cmath>

#include "Camera.h"
#include "CharacterActors.h"
#include "LevelBuilder.h"
#include "Renderer.h"
#include "SceneGraph.h"
#include "TileMap.h"

namespace
{
	// 화면 크기: HUD와 같은 800x600 픽셀 좌표계(원점은 왼쪽 아래).
	const float kScreenWidth = 800.f;
	const float kScreenHeight = 600.f;

	const float kPanelHalfSize = 64.f; // 패널 한 변 128픽셀
	const float kPanelMargin = 12.f;   // 화면 가장자리와의 간격
	const float kMapPadding = 4.f;     // 패널 안쪽 여백

	// 표시 크기(한 변의 절반, 픽셀).
	const float kTreeHalf = 1.1f;
	const float kBuildingHalf = 2.6f;
	const float kVillagerHalf = 1.5f;
	const float kElderHalf = 2.3f;
	const float kHerbHalf = 1.3f;
	const float kQuestItemHalf = 2.4f;
	const float kDeerHalf = 1.9f;
	const float kWolfHalf = 2.1f;
	const float kPlayerHalf = 3.0f;

	void AppendVertex(std::vector<float>& out, float x, float y)
	{
		out.push_back(x);
		out.push_back(y);
		out.push_back(0.f);
	}

	// 네 꼭짓점을 차례로 이은 사각형을 삼각형 두 개로 넣는다.
	void AppendQuad(std::vector<float>& out, float x0, float y0, float x1, float y1,
		float x2, float y2, float x3, float y3)
	{
		AppendVertex(out, x0, y0);
		AppendVertex(out, x1, y1);
		AppendVertex(out, x2, y2);

		AppendVertex(out, x0, y0);
		AppendVertex(out, x2, y2);
		AppendVertex(out, x3, y3);
	}

	// 화면에 축이 맞는 정사각형 점.
	void AppendSquare(std::vector<float>& out, float centerX, float centerY, float half)
	{
		AppendQuad(out, centerX - half, centerY - half, centerX + half, centerY - half,
			centerX + half, centerY + half, centerX - half, centerY + half);
	}

	void DrawList(Renderer& renderer, const Mat4& uiProjection, const std::vector<float>& vertices,
		float r, float g, float b)
	{
		if (vertices.empty())
		{
			return;
		}

		renderer.DrawTriangles(vertices.data(), (int)(vertices.size() / 3), uiProjection, r, g, b, 1.f);
	}
}

void MiniMap::MapTransform::Apply(float worldX, float worldY, float& screenX, float& screenY) const
{
	// 카메라의 지면 회전(yaw)과 같은 회전 → 좌우/상하 반전이 곱해진 배율.
	float rotatedX = cosYaw * worldX - sinYaw * worldY;
	float rotatedY = sinYaw * worldX + cosYaw * worldY;

	screenX = centerX + scaleX * rotatedX;
	screenY = centerY + scaleY * rotatedY;
}

void MiniMap::RebuildStatic(SceneGraph& scene, const TileMap& tileMap)
{
	m_Grass.clear();
	m_Water.clear();
	m_Stone.clear();
	m_Path.clear();
	m_Trees.clear();
	m_Buildings.clear();

	int width = tileMap.GetWidth();
	int height = tileMap.GetHeight();
	float halfWidth = width * 0.5f;
	float halfHeight = height * 0.5f;

	// 풀밭: 맵 전체를 덮는 회전된 사각형 하나. 다른 지형은 이 위에 덧그린다.
	float x0, y0, x1, y1, x2, y2, x3, y3;
	m_Transform.Apply(-halfWidth, -halfHeight, x0, y0);
	m_Transform.Apply(halfWidth, -halfHeight, x1, y1);
	m_Transform.Apply(halfWidth, halfHeight, x2, y2);
	m_Transform.Apply(-halfWidth, halfHeight, x3, y3);
	AppendQuad(m_Grass, x0, y0, x1, y1, x2, y2, x3, y3);

	for (int gy = 0; gy < height; ++gy)
	{
		for (int gx = 0; gx < width; ++gx)
		{
			TileType tile = tileMap.GetTile(gx, gy);

			std::vector<float>* target = nullptr;
			if (tile == TileType::Water) target = &m_Water;
			else if (tile == TileType::Stone) target = &m_Stone;
			else if (tile == TileType::Path) target = &m_Path;

			if (target == nullptr)
			{
				continue; // 풀밭은 위에서 한 번에 그렸다.
			}

			// 타일 (gx, gy)는 월드에서 [gx - halfWidth, gx + 1 - halfWidth] x [...] 구간이다. 이웃 타일이
			// 같은 정수식에서 같은 꼭짓점을 얻도록 해서 타일 사이에 틈이 생기지 않게 한다.
			float left = gx - halfWidth;
			float right = gx + 1 - halfWidth;
			float bottom = gy - halfHeight;
			float top = gy + 1 - halfHeight;

			m_Transform.Apply(left, bottom, x0, y0);
			m_Transform.Apply(right, bottom, x1, y1);
			m_Transform.Apply(right, top, x2, y2);
			m_Transform.Apply(left, top, x3, y3);
			AppendQuad(*target, x0, y0, x1, y1, x2, y2, x3, y3);
		}
	}

	// 나무와 건물도 움직이지 않으니 지금 한 번 모아 둔다.
	scene.ForEach([this](Actor& actor)
	{
		if (actor.IsPendingDestroy())
		{
			return;
		}

		float screenX, screenY;
		if (actor.GetType() == ActorType::Prop)
		{
			m_Transform.Apply(actor.GetWorldX(), actor.GetWorldY(), screenX, screenY);
			AppendSquare(m_Trees, screenX, screenY, kTreeHalf);
		}
		else if (actor.GetType() == ActorType::Building)
		{
			m_Transform.Apply(actor.GetWorldX(), actor.GetWorldY(), screenX, screenY);
			AppendSquare(m_Buildings, screenX, screenY, kBuildingHalf);
		}
	});
}

void MiniMap::Draw(Renderer& renderer, SceneGraph& scene, const TileMap& tileMap, const Camera& camera)
{
	float yaw = camera.GetYawRadians();
	bool flipH = camera.IsFlippedHorizontally();
	bool flipV = camera.IsFlippedVertically();

	// 카메라 방향이 처음이거나 바뀌었을 때만 정적 지오메트리를 다시 만든다.
	if (!m_StaticBuilt || yaw != m_BuiltYaw || flipH != m_BuiltFlipH || flipV != m_BuiltFlipV)
	{
		float width = (float)tileMap.GetWidth();
		float height = (float)tileMap.GetHeight();
		float c = cosf(yaw);
		float s = sinf(yaw);

		// 회전한 지도가 차지하는 반너비/반높이 중 큰 쪽이 패널 안쪽에 딱 맞도록 배율을 정한다.
		float extentX = 0.5f * (fabsf(c) * width + fabsf(s) * height);
		float extentY = 0.5f * (fabsf(s) * width + fabsf(c) * height);
		float extent = (extentX > extentY) ? extentX : extentY;
		float scale = (kPanelHalfSize - kMapPadding) / extent;

		m_Transform.cosYaw = c;
		m_Transform.sinYaw = s;
		m_Transform.scaleX = flipH ? -scale : scale;
		m_Transform.scaleY = flipV ? -scale : scale;
		m_Transform.centerX = kScreenWidth - kPanelMargin - kPanelHalfSize;
		m_Transform.centerY = kScreenHeight - kPanelMargin - kPanelHalfSize;

		RebuildStatic(scene, tileMap);

		m_StaticBuilt = true;
		m_BuiltYaw = yaw;
		m_BuiltFlipH = flipH;
		m_BuiltFlipV = flipV;
	}

	Mat4 uiProjection = Mat4::Ortho(0.f, kScreenWidth, 0.f, kScreenHeight, -1.f, 1.f);

	// 패널: 어두운 테두리 + 반투명 배경.
	float centerX = m_Transform.centerX;
	float centerY = m_Transform.centerY;

	Mat4 border = Mat4::Translate(centerX, centerY, 0.f) * Mat4::Scale((kPanelHalfSize + 2.f) * 2.f, (kPanelHalfSize + 2.f) * 2.f, 1.f);
	renderer.DrawObject(uiProjection * border, 0.02f, 0.02f, 0.03f, 0.9f);

	Mat4 background = Mat4::Translate(centerX, centerY, 0.f) * Mat4::Scale(kPanelHalfSize * 2.f, kPanelHalfSize * 2.f, 1.f);
	renderer.DrawObject(uiProjection * background, 0.08f, 0.09f, 0.11f, 0.85f);

	// 지형과 나무/건물.
	DrawList(renderer, uiProjection, m_Grass, 0.16f, 0.30f, 0.15f);
	DrawList(renderer, uiProjection, m_Water, 0.15f, 0.35f, 0.65f);
	DrawList(renderer, uiProjection, m_Stone, 0.58f, 0.56f, 0.52f);
	DrawList(renderer, uiProjection, m_Path, 0.50f, 0.38f, 0.24f);
	DrawList(renderer, uiProjection, m_Trees, 0.05f, 0.16f, 0.07f);
	DrawList(renderer, uiProjection, m_Buildings, 0.78f, 0.52f, 0.30f);

	// 움직이거나 사라질 수 있는 것들은 매 프레임 씬에서 모아서 종류별로 한 번에 그린다.
	m_Villagers.clear();
	m_Elders.clear();
	m_Herbs.clear();
	m_QuestItems.clear();
	m_Deer.clear();
	m_Wolves.clear();
	m_Player.clear();

	scene.ForEach([this](Actor& actor)
	{
		if (actor.IsPendingDestroy())
		{
			return;
		}

		float screenX, screenY;
		switch (actor.GetType())
		{
		case ActorType::NPC:
			m_Transform.Apply(actor.GetWorldX(), actor.GetWorldY(), screenX, screenY);
			if (actor.GetInteractId() == kInteractElder)
			{
				AppendSquare(m_Elders, screenX, screenY, kElderHalf);
			}
			else
			{
				AppendSquare(m_Villagers, screenX, screenY, kVillagerHalf);
			}
			break;

		case ActorType::Item:
			m_Transform.Apply(actor.GetWorldX(), actor.GetWorldY(), screenX, screenY);
			if (actor.GetInteractId() == kInteractQuestItem)
			{
				AppendSquare(m_QuestItems, screenX, screenY, kQuestItemHalf);
			}
			else
			{
				AppendSquare(m_Herbs, screenX, screenY, kHerbHalf);
			}
			break;

		case ActorType::Animal:
			{
				const AnimalActor& animal = static_cast<const AnimalActor&>(actor);
				m_Transform.Apply(actor.GetWorldX(), actor.GetWorldY(), screenX, screenY);

				if (animal.IsAggressive())
				{
					AppendSquare(m_Wolves, screenX, screenY, kWolfHalf);
				}
				else
				{
					AppendSquare(m_Deer, screenX, screenY, kDeerHalf);
				}
			}
			break;

		default:
			break;
		}
	});

	PlayerActor* player = scene.GetPlayer();
	if (player != nullptr)
	{
		float screenX, screenY;
		m_Transform.Apply(player->GetWorldX(), player->GetWorldY(), screenX, screenY);
		AppendSquare(m_Player, screenX, screenY, kPlayerHalf);
	}

	DrawList(renderer, uiProjection, m_Villagers, 0.92f, 0.90f, 0.80f); // 마을 사람: 옅은 베이지
	DrawList(renderer, uiProjection, m_Elders, 1.0f, 0.85f, 0.2f);      // 장로: 노랑
	DrawList(renderer, uiProjection, m_Herbs, 0.45f, 0.95f, 0.55f);     // 약초: 연두
	DrawList(renderer, uiProjection, m_QuestItems, 0.4f, 0.9f, 1.0f);   // 퀘스트 아이템: 하늘색
	DrawList(renderer, uiProjection, m_Deer, 0.85f, 0.65f, 0.4f);       // 사슴: 갈색
	DrawList(renderer, uiProjection, m_Wolves, 0.95f, 0.2f, 0.2f);      // 늑대(몬스터): 빨강
	DrawList(renderer, uiProjection, m_Player, 1.0f, 1.0f, 1.0f);       // 플레이어: 흰색(맨 마지막이라 위에 보임)
}
