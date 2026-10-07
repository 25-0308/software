#include "stdafx.h"
#include "MiniMap.h"

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
	: MiniMap(kScreenWidth - kPanelMargin - kDefaultHalfSize, kScreenHeight - kPanelMargin - kDefaultHalfSize, kDefaultHalfSize, 1.f)
{
}

MiniMap::MiniMap(float centerX, float centerY, float halfSize, float markerScale)
	: m_PanelCenterX(centerX)
	, m_PanelCenterY(centerY)
	, m_PanelHalfSize(halfSize)
	, m_MarkerScale(markerScale)
{
}

void MiniMap::MapTransform::Apply(float worldX, float worldY, float& screenX, float& screenY) const
{
	// 카메라의 지면 회전(yaw)과 같은 회전 → 좌우/상하 반전이 곱해진 배율.
	float rotatedX = cosYaw * worldX - sinYaw * worldY;
	float rotatedY = sinYaw * worldX + cosYaw * worldY;

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

void MiniMap::RebuildStatic(SceneGraph& scene, const TileMap& tileMap)
{
	m_Grass.clear();
	m_Water.clear();
	m_Stone.clear();
	m_Path.clear();
	m_Rock.clear();
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
			else if (tile == TileType::Rock) target = &m_Rock;

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

	// 나무와 건물(집·신전·기둥·좌판 등)도 움직이지 않으니 지금 한 번 모아 둔다. 건물은 크기에 비례해서
	// 그려서 신전처럼 큰 건축물이 지도에서도 크게 보이게 한다. 이정표는 매 프레임 따로 눈에 띄게 그린다.
	float pixelsPerUnit = fabsf(m_Transform.scaleX);
	float treeHalf = kTreeHalf * m_MarkerScale;
	float buildingMinHalf = kBuildingMinHalf * m_MarkerScale;

	scene.ForEach([this, pixelsPerUnit, treeHalf, buildingMinHalf](Actor& actor)
	{
		if (actor.IsPendingDestroy())
		{
			return;
		}

		float screenX, screenY;
		if (actor.GetType() == ActorType::Prop)
		{
			m_Transform.Apply(actor.GetWorldX(), actor.GetWorldY(), screenX, screenY);
			AppendSquare(m_Trees, screenX, screenY, treeHalf);
		}
		else if (actor.GetType() == ActorType::Building && !IsStoryId(actor, "OBJ_SIGNPOST"))
		{
			float half = actor.GetSize() * pixelsPerUnit * kBuildingScale;
			if (half < buildingMinHalf)
			{
				half = buildingMinHalf;
			}

			m_Transform.Apply(actor.GetWorldX(), actor.GetWorldY(), screenX, screenY);
			AppendSquare(m_Buildings, screenX, screenY, half);
		}
	});
}

void MiniMap::Draw(Renderer& renderer, SceneGraph& scene, const TileMap& tileMap, const Camera& camera,
	const std::vector<Spot>& objectives, float time)
{
	float yaw = camera.GetYawRadians();
	bool flipH = camera.IsFlippedHorizontally();
	bool flipV = camera.IsFlippedVertically();

	// 카메라 방향이 처음이거나 바뀌었을 때(또는 맵이 바뀌어 Invalidate됐을 때)만 정적 지오메트리를 다시 만든다.
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
		float scale = (m_PanelHalfSize - kMapPadding) / extent;

		m_Transform.cosYaw = c;
		m_Transform.sinYaw = s;
		m_Transform.scaleX = flipH ? -scale : scale;
		m_Transform.scaleY = flipV ? -scale : scale;
		m_Transform.centerX = m_PanelCenterX;
		m_Transform.centerY = m_PanelCenterY;

		RebuildStatic(scene, tileMap);

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
		// 이야기 진행 조건으로 숨은 인물·괴물·물건은 지도에도 나오지 않는다.
		if (actor.IsPendingDestroy() || !actor.IsVisible())
		{
			return;
		}

		float screenX, screenY;
		switch (actor.GetType())
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

				if (animal.GetKind() == AnimalKind::ShadowWolf || animal.GetKind() == AnimalKind::ShadowPython)
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
		float screenX, screenY;
		m_Transform.Apply(spot.x, spot.y, screenX, screenY);

		float radius = (kPingStartRadius + (kPingEndRadius - kPingStartRadius) * ping) * m_MarkerScale;
		AppendRing(m_ObjectivePings, screenX, screenY, radius, radius + kPingThickness * m_MarkerScale, kPingSegments);

		float half = kObjectiveHalf * m_MarkerScale * (0.8f + 0.35f * pulse);
		AppendDiamond(m_ObjectiveOutlines, screenX, screenY, half + 1.2f * m_MarkerScale);
		AppendDiamond(m_Objectives, screenX, screenY, half);
	}

	// 플레이어: 바라보는 쪽(화면에서 걷는 방향)을 가리키는 흰 화살촉.
	PlayerActor* player = scene.GetPlayer();
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
