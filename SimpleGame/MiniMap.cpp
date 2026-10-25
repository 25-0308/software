#include "stdafx.h"
#include "MiniMap.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "Camera.h"
#include "CharacterActors.h"
#include "Renderer.h"
#include "SceneGraph.h"
#include "TileMap.h"

namespace
{
	// 화면 크기: HUD와 같은 800x600 픽셀 좌표계(원점은 왼쪽 아래).
	const float kScreenWidth = 800.f;
	const float kScreenHeight = 600.f;

	const float kDefaultHalfSize = 64.f; // 기본 미니맵 패널 한 변 128픽셀
	const float kDefaultWindow = 30.f;   // 기본 미니맵이 보여 주는 플레이어 둘레(반 변, 월드 칸 — 가로세로 60칸)
	const float kPanelMargin = 12.f;     // 기본 미니맵과 화면 가장자리의 간격
	const float kMapPadding = 4.f;       // 패널 안쪽 여백

	// 표시 크기(한 변의 절반, 픽셀). 큰 지도에선 표시 크기 배율(m_MarkerScale)을 곱한다.
	const float kTreeHalf = 1.1f;
	const float kBuildingMinHalf = 1.2f;  // 건물은 크기에 비례하되(신전은 크게) 기둥 같은 작은 것도 이만큼은 보이게
	const float kBuildingScale = 0.6f;    // 건물 크기(월드) x 배율(픽셀/월드) x 이 값 = 반너비
	const float kVillagerHalf = 1.5f;
	const float kStoryCharacterHalf = 2.3f;
	const float kHerbHalf = 1.3f;
	const float kStoryItemHalf = 2.2f;
	const float kDeerHalf = 1.9f;
	const float kWolfHalf = 2.1f;
	const float kShadowHalf = 2.4f;
	const float kSignpostHalf = 2.4f;
	const float kObjectiveHalf = 3.4f;
	const float kPlayerHalf = 3.0f;

	// 목표 둘레로 퍼져 나가는 고리: kPingSeconds마다 반지름 start → end로 커지며 옅어진다.
	const float kPingSeconds = 1.4f;
	const float kPingStartRadius = 3.f;
	const float kPingEndRadius = 10.f;
	const float kPingThickness = 1.4f;
	const int kPingSegments = 20;

	void AppendVertex(std::vector<float>& out, float x, float y)
	{
		out.push_back(x);
		out.push_back(y);
		out.push_back(0.f);
	}

	void AppendTriangle(std::vector<float>& out, float x0, float y0, float x1, float y1, float x2, float y2)
	{
		AppendVertex(out, x0, y0);
		AppendVertex(out, x1, y1);
		AppendVertex(out, x2, y2);
	}

	// 네 꼭짓점을 차례로 이은 사각형을 삼각형 두 개로 넣는다.
	void AppendQuad(std::vector<float>& out, float x0, float y0, float x1, float y1,
		float x2, float y2, float x3, float y3)
	{
		AppendTriangle(out, x0, y0, x1, y1, x2, y2);
		AppendTriangle(out, x0, y0, x2, y2, x3, y3);
	}

	// 화면에 축이 맞는 정사각형 점.
	void AppendSquare(std::vector<float>& out, float centerX, float centerY, float half)
	{
		AppendQuad(out, centerX - half, centerY - half, centerX + half, centerY - half,
			centerX + half, centerY + half, centerX - half, centerY + half);
	}

	// 45도 돌린 정사각형(마름모) 점 — 본 화면의 목표 표시와 같은 모양.
	void AppendDiamond(std::vector<float>& out, float centerX, float centerY, float half)
	{
		AppendQuad(out, centerX, centerY - half, centerX + half, centerY,
			centerX, centerY + half, centerX - half, centerY);
	}

	// 가운데가 빈 고리(segments개의 사다리꼴로 나눠 근사).
	void AppendRing(std::vector<float>& out, float centerX, float centerY, float innerRadius, float outerRadius, int segments)
	{
		const float kTwoPi = 6.2831853f;

		for (int i = 0; i < segments; ++i)
		{
			float angle0 = kTwoPi * (float)i / (float)segments;
			float angle1 = kTwoPi * (float)(i + 1) / (float)segments;
			float cos0 = cosf(angle0), sin0 = sinf(angle0);
			float cos1 = cosf(angle1), sin1 = sinf(angle1);

			AppendQuad(out, centerX + cos0 * innerRadius, centerY + sin0 * innerRadius,
				centerX + cos0 * outerRadius, centerY + sin0 * outerRadius,
				centerX + cos1 * outerRadius, centerY + sin1 * outerRadius,
				centerX + cos1 * innerRadius, centerY + sin1 * innerRadius);
		}
	}

	// (dirX, dirY)(길이 1) 쪽을 가리키는, 뒤가 오목한 화살촉. half는 몸통 반 크기.
	void AppendArrow(std::vector<float>& out, float centerX, float centerY, float dirX, float dirY, float half)
	{
		float sideX = -dirY;
		float sideY = dirX;

		float tipX = centerX + dirX * half * 1.5f;
		float tipY = centerY + dirY * half * 1.5f;
		float leftX = centerX - dirX * half * 0.9f + sideX * half * 1.1f;
		float leftY = centerY - dirY * half * 0.9f + sideY * half * 1.1f;
		float notchX = centerX - dirX * half * 0.4f;
		float notchY = centerY - dirY * half * 0.4f;
		float rightX = centerX - dirX * half * 0.9f - sideX * half * 1.1f;
		float rightY = centerY - dirY * half * 0.9f - sideY * half * 1.1f;

		AppendTriangle(out, tipX, tipY, leftX, leftY, notchX, notchY);
		AppendTriangle(out, tipX, tipY, notchX, notchY, rightX, rightY);
	}

	void DrawList(Renderer& renderer, const Mat4& uiProjection, const std::vector<float>& vertices,
		float r, float g, float b, float a = 1.f)
	{
		if (vertices.empty())
		{
			return;
		}

		renderer.DrawTriangles(vertices.data(), (int)(vertices.size() / 3), uiProjection, r, g, b, a);
	}

	void DrawList(Renderer& renderer, const Mat4& uiProjection, const std::vector<float>& vertices,
		const MapColors::Rgb& color, float a = 1.f)
	{
		DrawList(renderer, uiProjection, vertices, color.r, color.g, color.b, a);
	}

	bool IsStoryId(const Actor& actor, const char* storyId)
	{
		return actor.GetStoryId() != nullptr && strcmp(actor.GetStoryId(), storyId) == 0;
	}

	// 이야기 인물(STORY.md의 CHR_ 접두사: 신·피티아·노파 등)인지, 이름 없는 마을 사람(NPC_)인지.
	bool IsStoryCharacter(const Actor& actor)
	{
		return actor.GetStoryId() != nullptr && strncmp(actor.GetStoryId(), "CHR_", 4) == 0;
	}
}

MiniMap::MiniMap()
	: MiniMap(kScreenWidth - kPanelMargin - kDefaultHalfSize, kScreenHeight - kPanelMargin - kDefaultHalfSize, kDefaultHalfSize, 1.f,
		kDefaultWindow)
{
}

MiniMap::MiniMap(float centerX, float centerY, float halfSize, float markerScale, float windowHalf)
	: m_PanelCenterX(centerX)
	, m_PanelCenterY(centerY)
	, m_PanelHalfSize(halfSize)
	, m_MarkerScale(markerScale)
	, m_WindowHalf(windowHalf)
{
}

void MiniMap::MapTransform::Apply(float worldX, float worldY, float& screenX, float& screenY) const
{
	// 창 중심을 원점으로 → 카메라의 지면 회전(yaw)과 같은 회전 → 좌우/상하 반전이 곱해진 배율.
	float localX = worldX - originX;
	float localY = worldY - originY;
	float rotatedX = cosYaw * localX - sinYaw * localY;
	float rotatedY = sinYaw * localX + cosYaw * localY;

	screenX = centerX + scaleX * rotatedX;
	screenY = centerY + scaleY * rotatedY;
}

void MiniMap::MapTransform::ApplyDirection(float worldX, float worldY, float& screenX, float& screenY) const
{
	float rotatedX = cosYaw * worldX - sinYaw * worldY;
	float rotatedY = sinYaw * worldX + cosYaw * worldY;

	screenX = scaleX * rotatedX;
	screenY = scaleY * rotatedY;
}

bool MiniMap::InWindow(float x, float y) const
{
	if (m_WindowHalf <= 0.f)
	{
		return true;
	}

	return fabsf(x - m_Transform.originX) <= m_WindowHalf && fabsf(y - m_Transform.originY) <= m_WindowHalf;
}

void MiniMap::CollectStaticActors(SceneGraph& scene)
{
	m_TreeSpots.clear();
	m_BuildingSpots.clear();

	// 나무와 건물(집·신전·기둥·좌판 등)은 움직이지 않으니 자리만 한 번 모아 둔다. 이정표는 매 프레임 따로 눈에 띄게 그린다.
	scene.ForEach([this](Actor& actor)
	{
		if (actor.IsPendingDestroy())
		{
			return;
		}

		if (actor.GetType() == ActorType::Prop)
		{
			m_TreeSpots.push_back(actor.GetWorldX());
			m_TreeSpots.push_back(actor.GetWorldY());
		}
		else if (actor.GetType() == ActorType::Building && !IsStoryId(actor, "OBJ_SIGNPOST"))
		{
			m_BuildingSpots.push_back(actor.GetWorldX());
			m_BuildingSpots.push_back(actor.GetWorldY());
			m_BuildingSpots.push_back(actor.GetSize());
		}
	});

	m_ActorsCollected = true;
}

void MiniMap::BuildTerrain(const TileMap& tileMap, int minGridX, int minGridY, int maxGridX, int maxGridY)
{
	m_Grass.clear();
	m_Water.clear();
	m_Stone.clear();
	m_Path.clear();
	m_Rock.clear();
	m_Trees.clear();
	m_Buildings.clear();

	float halfWidth = tileMap.GetWidth() * 0.5f;
	float halfHeight = tileMap.GetHeight() * 0.5f;

	// 풀밭: 보여 줄 범위 전체를 덮는 회전된 사각형 하나. 다른 지형은 이 위에 덧그린다.
	float left = minGridX - halfWidth;
	float right = maxGridX + 1 - halfWidth;
	float bottom = minGridY - halfHeight;
	float top = maxGridY + 1 - halfHeight;
	float x0, y0, x1, y1, x2, y2, x3, y3;
	m_Transform.Apply(left, bottom, x0, y0);
	m_Transform.Apply(right, bottom, x1, y1);
	m_Transform.Apply(right, top, x2, y2);
	m_Transform.Apply(left, top, x3, y3);
	AppendQuad(m_Grass, x0, y0, x1, y1, x2, y2, x3, y3);

	// 줄마다 같은 종류가 이어진 칸을 사각형 하나로 묶는다(런 길이). 이웃 칸이 같은 정수식에서 같은 꼭짓점을 얻어 틈이 없다.
	for (int gy = minGridY; gy <= maxGridY; ++gy)
	{
		int gx = minGridX;
		while (gx <= maxGridX)
		{
			TileType tile = tileMap.GetTile(gx, gy);
			int runEnd = gx;
			while (runEnd + 1 <= maxGridX && tileMap.GetTile(runEnd + 1, gy) == tile)
			{
				++runEnd;
			}

			std::vector<float>* target = nullptr;
			if (tile == TileType::Water) target = &m_Water;
			else if (tile == TileType::Stone) target = &m_Stone;
			else if (tile == TileType::Path) target = &m_Path;
			else if (tile == TileType::Rock) target = &m_Rock;

			if (target != nullptr)
			{
				float runLeft = gx - halfWidth;
				float runRight = runEnd + 1 - halfWidth;
				float runBottom = gy - halfHeight;
				float runTop = gy + 1 - halfHeight;

				m_Transform.Apply(runLeft, runBottom, x0, y0);
				m_Transform.Apply(runRight, runBottom, x1, y1);
				m_Transform.Apply(runRight, runTop, x2, y2);
				m_Transform.Apply(runLeft, runTop, x3, y3);
				AppendQuad(*target, x0, y0, x1, y1, x2, y2, x3, y3);
			}

			gx = runEnd + 1;
		}
	}

	// 나무와 건물. 건물은 크기에 비례해서 그려서 신전처럼 큰 건축물이 지도에서도 크게 보이게 한다.
	float pixelsPerUnit = fabsf(m_Transform.scaleX);
	float treeHalf = kTreeHalf * m_MarkerScale;
	float buildingMinHalf = kBuildingMinHalf * m_MarkerScale;

	for (size_t i = 0; i + 1 < m_TreeSpots.size(); i += 2)
	{
		if (!InWindow(m_TreeSpots[i], m_TreeSpots[i + 1]))
		{
			continue;
		}

		float screenX, screenY;
		m_Transform.Apply(m_TreeSpots[i], m_TreeSpots[i + 1], screenX, screenY);
		AppendSquare(m_Trees, screenX, screenY, treeHalf);
	}

	for (size_t i = 0; i + 2 < m_BuildingSpots.size(); i += 3)
	{
		if (!InWindow(m_BuildingSpots[i], m_BuildingSpots[i + 1]))
		{
			continue;
		}

		float half = (std::max)(m_BuildingSpots[i + 2] * pixelsPerUnit * kBuildingScale, buildingMinHalf);
		float screenX, screenY;
		m_Transform.Apply(m_BuildingSpots[i], m_BuildingSpots[i + 1], screenX, screenY);
		AppendSquare(m_Buildings, screenX, screenY, half);
	}
}

void MiniMap::Draw(Renderer& renderer, SceneGraph& scene, const TileMap& tileMap, const Camera& camera,
	const std::vector<Spot>& objectives, float time)
{
	float yaw = camera.GetYawRadians();
	bool flipH = camera.IsFlippedHorizontally();
	bool flipV = camera.IsFlippedVertically();
	float c = cosf(yaw);
	float s = sinf(yaw);

	if (!m_ActorsCollected)
	{
		CollectStaticActors(scene);
	}

	PlayerActor* player = scene.GetPlayer();

	if (m_WindowHalf > 0.f)
	{
		// 플레이어 둘레의 창: 창(정사각형)을 돌린 마름모가 패널 안쪽에 딱 맞는 배율로, 매 프레임 창 안의 칸만 만든다.
		float extent = m_WindowHalf * (fabsf(c) + fabsf(s));
		float scale = (m_PanelHalfSize - kMapPadding) / extent;

		m_Transform.originX = (player != nullptr) ? player->GetWorldX() : 0.f;
		m_Transform.originY = (player != nullptr) ? player->GetWorldY() : 0.f;
		m_Transform.cosYaw = c;
		m_Transform.sinYaw = s;
		m_Transform.scaleX = flipH ? -scale : scale;
		m_Transform.scaleY = flipV ? -scale : scale;
		m_Transform.centerX = m_PanelCenterX;
		m_Transform.centerY = m_PanelCenterY;

		int minGridX = (std::max)(0, tileMap.GetGridX(m_Transform.originX - m_WindowHalf + 0.5f));
		int maxGridX = (std::min)(tileMap.GetWidth() - 1, tileMap.GetGridX(m_Transform.originX + m_WindowHalf - 0.5f));
		int minGridY = (std::max)(0, tileMap.GetGridY(m_Transform.originY - m_WindowHalf + 0.5f));
		int maxGridY = (std::min)(tileMap.GetHeight() - 1, tileMap.GetGridY(m_Transform.originY + m_WindowHalf - 0.5f));
		BuildTerrain(tileMap, minGridX, minGridY, maxGridX, maxGridY);
	}
	else if (!m_StaticBuilt || yaw != m_BuiltYaw || flipH != m_BuiltFlipH || flipV != m_BuiltFlipV)
	{
		// 맵 전체: 카메라 방향이 처음이거나 바뀌었을 때(또는 맵이 바뀌어 Invalidate됐을 때)만 다시 만든다.
		float width = (float)tileMap.GetWidth();
		float height = (float)tileMap.GetHeight();

		// 회전한 지도가 차지하는 반너비/반높이 중 큰 쪽이 패널 안쪽에 딱 맞도록 배율을 정한다.
		float extentX = 0.5f * (fabsf(c) * width + fabsf(s) * height);
		float extentY = 0.5f * (fabsf(s) * width + fabsf(c) * height);
		float extent = (extentX > extentY) ? extentX : extentY;
		float scale = (m_PanelHalfSize - kMapPadding) / extent;

		m_Transform.originX = 0.f;
		m_Transform.originY = 0.f;
		m_Transform.cosYaw = c;
		m_Transform.sinYaw = s;
		m_Transform.scaleX = flipH ? -scale : scale;
		m_Transform.scaleY = flipV ? -scale : scale;
		m_Transform.centerX = m_PanelCenterX;
		m_Transform.centerY = m_PanelCenterY;

		BuildTerrain(tileMap, 0, 0, tileMap.GetWidth() - 1, tileMap.GetHeight() - 1);

		m_StaticBuilt = true;
		m_BuiltYaw = yaw;
		m_BuiltFlipH = flipH;
		m_BuiltFlipV = flipV;
	}

	Mat4 uiProjection = Mat4::Ortho(0.f, kScreenWidth, 0.f, kScreenHeight, -1.f, 1.f);

	// 패널: 어두운 테두리 + 바다색 배경(본 화면처럼 섬 둘레가 바다로 보이게).
	float centerX = m_Transform.centerX;
	float centerY = m_Transform.centerY;

	Mat4 border = Mat4::Translate(centerX, centerY, 0.f) * Mat4::Scale((m_PanelHalfSize + 2.f) * 2.f, (m_PanelHalfSize + 2.f) * 2.f, 1.f);
	renderer.DrawObject(uiProjection * border, 0.02f, 0.02f, 0.03f, 0.9f);

	Mat4 background = Mat4::Translate(centerX, centerY, 0.f) * Mat4::Scale(m_PanelHalfSize * 2.f, m_PanelHalfSize * 2.f, 1.f);
	renderer.DrawObject(uiProjection * background, 0.07f, 0.2f, 0.34f, 0.9f);

	// 지형과 나무/건물.
	DrawList(renderer, uiProjection, m_Grass, 0.16f, 0.30f, 0.15f);
	DrawList(renderer, uiProjection, m_Water, 0.15f, 0.35f, 0.65f);
	DrawList(renderer, uiProjection, m_Stone, 0.58f, 0.56f, 0.52f);
	DrawList(renderer, uiProjection, m_Path, 0.50f, 0.38f, 0.24f);
	DrawList(renderer, uiProjection, m_Rock, 0.36f, 0.34f, 0.33f);
	DrawList(renderer, uiProjection, m_Trees, 0.05f, 0.16f, 0.07f);
	DrawList(renderer, uiProjection, m_Buildings, 0.78f, 0.52f, 0.30f);

	// 움직이거나 나타났다 사라지는 것들은 매 프레임 씬에서 모아서 종류별로 한 번에 그린다.
	m_Villagers.clear();
	m_StoryCharacters.clear();
	m_Herbs.clear();
	m_StoryItems.clear();
	m_Deer.clear();
	m_Wolves.clear();
	m_Shadows.clear();
	m_Signposts.clear();
	m_ObjectivePings.clear();
	m_ObjectiveOutlines.clear();
	m_Objectives.clear();
	m_PlayerOutline.clear();
	m_Player.clear();

	float markerScale = m_MarkerScale;
	scene.ForEach([this, markerScale](Actor& actor)
	{
		// 이야기 진행 조건으로 숨은 인물·괴물·물건은 지도에도 나오지 않는다. 창 밖(플레이어 둘레 밖)도 그리지 않는다.
		if (actor.IsPendingDestroy() || !actor.IsVisible())
		{
			return;
		}

		ActorType type = actor.GetType();
		if (type != ActorType::NPC && type != ActorType::Item && type != ActorType::Animal && type != ActorType::Building)
		{
			return;
		}

		if (!InWindow(actor.GetWorldX(), actor.GetWorldY()))
		{
			return;
		}

		float screenX, screenY;
		switch (type)
		{
		case ActorType::NPC:
			m_Transform.Apply(actor.GetWorldX(), actor.GetWorldY(), screenX, screenY);
			if (IsStoryCharacter(actor))
			{
				AppendSquare(m_StoryCharacters, screenX, screenY, kStoryCharacterHalf * markerScale);
			}
			else
			{
				AppendSquare(m_Villagers, screenX, screenY, kVillagerHalf * markerScale);
			}
			break;

		case ActorType::Item:
			m_Transform.Apply(actor.GetWorldX(), actor.GetWorldY(), screenX, screenY);
			if (actor.GetInteractId() == kInteractLoot)
			{
				AppendSquare(m_Herbs, screenX, screenY, kHerbHalf * markerScale);
			}
			else
			{
				AppendSquare(m_StoryItems, screenX, screenY, kStoryItemHalf * markerScale);
			}
			break;

		case ActorType::Animal:
			{
				const AnimalActor& animal = static_cast<const AnimalActor&>(actor);
				m_Transform.Apply(actor.GetWorldX(), actor.GetWorldY(), screenX, screenY);

				AnimalKind kind = animal.GetKind();
				if (kind == AnimalKind::ShadowWolf || kind == AnimalKind::ShadowPython || kind == AnimalKind::KronosEcho)
				{
					AppendSquare(m_Shadows, screenX, screenY, kShadowHalf * markerScale);
				}
				else if (animal.IsAggressive())
				{
					AppendSquare(m_Wolves, screenX, screenY, kWolfHalf * markerScale);
				}
				else
				{
					AppendSquare(m_Deer, screenX, screenY, kDeerHalf * markerScale);
				}
			}
			break;

		case ActorType::Building:
			if (IsStoryId(actor, "OBJ_SIGNPOST"))
			{
				m_Transform.Apply(actor.GetWorldX(), actor.GetWorldY(), screenX, screenY);
				AppendDiamond(m_Signposts, screenX, screenY, kSignpostHalf * markerScale);
			}
			break;

		default:
			break;
		}
	});

	// 목표: 금빛 고리가 퍼져 나가며 옅어지고(어디 있는지 한눈에 들어오게), 그 위에 어두운 테두리의 금빛 마름모가
	// 숨 쉬듯 커졌다 작아진다.
	float pulse = 0.5f + 0.5f * sinf(time * 4.f);
	float ping = fmodf(time, kPingSeconds) / kPingSeconds; // 0 → 1
	for (const Spot& spot : objectives)
	{
		// 창 밖의 목표는 그쪽 방향의 창 가장자리에 붙여 보여 준다(어느 쪽으로 가야 하는지는 보이게).
		float spotX = spot.x;
		float spotY = spot.y;
		if (m_WindowHalf > 0.f)
		{
			float limit = m_WindowHalf - 1.f;
			float dx = spotX - m_Transform.originX;
			float dy = spotY - m_Transform.originY;
			float reach = (std::max)(fabsf(dx), fabsf(dy));
			if (reach > limit)
			{
				spotX = m_Transform.originX + dx * (limit / reach);
				spotY = m_Transform.originY + dy * (limit / reach);
			}
		}

		float screenX, screenY;
		m_Transform.Apply(spotX, spotY, screenX, screenY);

		float radius = (kPingStartRadius + (kPingEndRadius - kPingStartRadius) * ping) * m_MarkerScale;
		AppendRing(m_ObjectivePings, screenX, screenY, radius, radius + kPingThickness * m_MarkerScale, kPingSegments);

		float half = kObjectiveHalf * m_MarkerScale * (0.8f + 0.35f * pulse);
		AppendDiamond(m_ObjectiveOutlines, screenX, screenY, half + 1.2f * m_MarkerScale);
		AppendDiamond(m_Objectives, screenX, screenY, half);
	}

	// 플레이어: 바라보는 쪽(화면에서 걷는 방향)을 가리키는 흰 화살촉.
	if (player != nullptr)
	{
		float screenX, screenY;
		m_Transform.Apply(player->GetWorldX(), player->GetWorldY(), screenX, screenY);

		float dirX, dirY;
		m_Transform.ApplyDirection(cosf(player->GetFacing()), sinf(player->GetFacing()), dirX, dirY);

		float length = sqrtf(dirX * dirX + dirY * dirY);
		if (length > 0.0001f)
		{
			dirX /= length;
			dirY /= length;
		}
		else
		{
			dirX = 0.f;
			dirY = 1.f;
		}

		float half = kPlayerHalf * m_MarkerScale;
		AppendArrow(m_PlayerOutline, screenX, screenY, dirX, dirY, half + 1.f * m_MarkerScale);
		AppendArrow(m_Player, screenX, screenY, dirX, dirY, half);
	}

	DrawList(renderer, uiProjection, m_Villagers, MapColors::kVillager);
	DrawList(renderer, uiProjection, m_StoryCharacters, MapColors::kStoryCharacter);
	DrawList(renderer, uiProjection, m_Herbs, MapColors::kHerb);
	DrawList(renderer, uiProjection, m_StoryItems, MapColors::kStoryItem);
	DrawList(renderer, uiProjection, m_Deer, MapColors::kDeer);
	DrawList(renderer, uiProjection, m_Wolves, MapColors::kWolf);
	DrawList(renderer, uiProjection, m_Shadows, MapColors::kShadow);
	DrawList(renderer, uiProjection, m_Signposts, MapColors::kSignpost);
	DrawList(renderer, uiProjection, m_ObjectivePings, MapColors::kObjective, 0.9f * (1.f - ping));
	DrawList(renderer, uiProjection, m_ObjectiveOutlines, 0.1f, 0.06f, 0.0f);        // 목표 테두리
	DrawList(renderer, uiProjection, m_Objectives, MapColors::kObjective);
	DrawList(renderer, uiProjection, m_PlayerOutline, 0.05f, 0.05f, 0.07f);          // 플레이어 테두리
	DrawList(renderer, uiProjection, m_Player, MapColors::kPlayer);                  // 맨 마지막이라 위에 보임
}
