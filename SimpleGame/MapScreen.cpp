#include "stdafx.h"
#include "MapScreen.h"

#include <cmath>

#include "Renderer.h"
#include "StoryDirector.h"
#include "UiDraw.h"

namespace
{
	// 큰 지도(화면 왼쪽): 미니맵과 같은 코드를 크게 놓는다. 점·마름모 같은 표시는 지도만큼 키우지 않고 조금만 키운다.
	const float kMapCenterX = 292.f;
	const float kMapCenterY = 290.f;
	const float kMapHalfSize = 248.f;
	const float kMapMarkerScale = 1.9f;

	// 지역 이름(큰 지도 위).
	const float kTitleLeft = 44.f;
	const float kTitleTop = 594.f;

	// 오른쪽 칸(현재 목표·지역 약도·범례).
	const float kSidebarLeft = 560.f;
	const float kSidebarTop = 594.f;
	const float kSidebarWidth = 228.f;
	const int kSidebarWrapWidth = 228;
	const float kSectionGap = 14.f;
	const float kAtlasHeight = 150.f;
	const float kLegendColumnWidth = 114.f;
	const float kLegendRowHeight = 19.f;

	// 지역 약도에서 각 지역의 자리(0~1, 왼쪽 아래가 (0,0), 위가 북쪽). 실제 그리스 지리를 대충 따른다.
	// 데이터 파일에 선언된(region 줄) 지역 중 여기에 자리가 있는 것만 그린다 — 지역을 새로 지으면 여기에 자리를 더한다.
	struct AtlasPlace
	{
		const char* locId;
		float x, y;
	};

	const AtlasPlace kAtlasPlaces[] =
	{
		{ "LOC_OLYMPUS_FOOT", 0.45f, 0.88f }, // 테살리아(북쪽)
		{ "LOC_DELPHI", 0.2f, 0.5f },         // 그리스 중부 서쪽, 파르나소스 산기슭
		{ "LOC_ATHENS", 0.7f, 0.24f },        // 남동쪽 아티카
	};

	bool FindAtlasPlace(const std::string& locId, float& outX, float& outY)
	{
		for (const AtlasPlace& place : kAtlasPlaces)
		{
			if (locId == place.locId)
			{
				outX = place.x;
				outY = place.y;
				return true;
			}
		}
		return false;
	}

	// 범례 한 칸: 미니맵·큰 지도와 같은 색·모양의 표시 + 이름.
	enum class LegendShape
	{
		Square,
		Diamond,
		Arrow,
	};

	struct LegendEntry
	{
		const char* label;
		MapColors::Rgb color;
		LegendShape shape;
	};

	const LegendEntry kLegend[] =
	{
		{ "나", MapColors::kPlayer, LegendShape::Arrow },
		{ "지금 목표", MapColors::kObjective, LegendShape::Diamond },
		{ "이야기 인물", MapColors::kStoryCharacter, LegendShape::Square },
		{ "주민", MapColors::kVillager, LegendShape::Square },
		{ "이정표", MapColors::kSignpost, LegendShape::Diamond },
		{ "이야기 물건", MapColors::kStoryItem, LegendShape::Square },
		{ "약초", MapColors::kHerb, LegendShape::Square },
		{ "사슴", MapColors::kDeer, LegendShape::Square },
		{ "늑대", MapColors::kWolf, LegendShape::Square },
		{ "그림자 괴물", MapColors::kShadow, LegendShape::Square },
	};

	// 위(dirY = 1)나 아래(dirY = -1)를 가리키는, 뒤가 오목한 화살촉(미니맵의 플레이어 표시와 같은 모양).
	void DrawArrow(Renderer& renderer, const Mat4& ui, float centerX, float centerY, float dirY, float half,
		float r, float g, float b, float a)
	{
		float tipY = centerY + dirY * half * 1.5f;
		float backY = centerY - dirY * half * 0.9f;
		float notchY = centerY - dirY * half * 0.4f;

		UiDraw::Triangle(renderer, ui, centerX, tipY, centerX - half * 1.1f, backY, centerX, notchY, r, g, b, a);
		UiDraw::Triangle(renderer, ui, centerX, tipY, centerX, notchY, centerX + half * 1.1f, backY, r, g, b, a);
	}

	void DrawLegendShape(Renderer& renderer, const Mat4& ui, LegendShape shape, float centerX, float centerY,
		const MapColors::Rgb& color)
	{
		switch (shape)
		{
		case LegendShape::Square:
			UiDraw::Rect(renderer, ui, centerX - 4.f, centerY - 4.f, 8.f, 8.f, color.r, color.g, color.b, 1.f);
			break;

		case LegendShape::Diamond:
			UiDraw::Diamond(renderer, ui, centerX, centerY, 6.f, 0.1f, 0.06f, 0.f, 1.f);
			UiDraw::Diamond(renderer, ui, centerX, centerY, 4.8f, color.r, color.g, color.b, 1.f);
			break;

		case LegendShape::Arrow:
			DrawArrow(renderer, ui, centerX, centerY - 1.f, 1.f, 4.6f, 0.05f, 0.05f, 0.07f, 1.f);
			DrawArrow(renderer, ui, centerX, centerY - 1.f, 1.f, 3.6f, color.r, color.g, color.b, 1.f);
			break;
		}
	}

	// 점선: kDash만큼 긋고 kGap만큼 비우기를 되풀이한다.
	void DrawDashedLine(Renderer& renderer, const Mat4& ui, float x0, float y0, float x1, float y1,
		float r, float g, float b, float a)
	{
		const float kDash = 6.f;
		const float kGap = 4.f;
		const float kThickness = 2.f;

		float dx = x1 - x0;
		float dy = y1 - y0;
		float length = sqrtf(dx * dx + dy * dy);
		if (length <= 0.f)
		{
			return;
		}

		float dirX = dx / length;
		float dirY = dy / length;

		for (float start = 0.f; start < length; start += kDash + kGap)
		{
			float end = (start + kDash < length) ? start + kDash : length;
			UiDraw::Line(renderer, ui, x0 + dirX * start, y0 + dirY * start, x0 + dirX * end, y0 + dirY * end, kThickness,
				r, g, b, a);
		}
	}
}

MapScreen::MapScreen()
	: m_Map(kMapCenterX, kMapCenterY, kMapHalfSize, kMapMarkerScale)
	, m_TitleText(21, true)
	, m_HeaderText(14, true)
	, m_BodyText(15)
	, m_SmallText(13)
{
}

MapScreen::~MapScreen()
{
	ReleaseDynamicTexts();

	TextRasterizer::Destroy(m_ObjectiveHeader);
	TextRasterizer::Destroy(m_AtlasHeader);
	TextRasterizer::Destroy(m_LegendHeader);
	TextRasterizer::Destroy(m_CloseHint);
	TextRasterizer::Destroy(m_MiniMapHint);

	for (TextTexture& label : m_LegendLabels)
	{
		TextRasterizer::Destroy(label);
	}
}

void MapScreen::CreateStaticTexts()
{
	if (m_StaticTextsCreated)
	{
		return;
	}
	m_StaticTextsCreated = true;

	m_ObjectiveHeader = m_HeaderText.Create("현재 목표", kSidebarWrapWidth);
	m_AtlasHeader = m_HeaderText.Create("지역", kSidebarWrapWidth);
	m_LegendHeader = m_HeaderText.Create("범례", kSidebarWrapWidth);
	m_CloseHint = m_SmallText.Create("M · Esc  닫기", 200);
	m_MiniMapHint = m_SmallText.Create("M 지도", 100);

	for (const LegendEntry& entry : kLegend)
	{
		m_LegendLabels.push_back(m_SmallText.Create(entry.label, (int)kLegendColumnWidth));
	}
}

void MapScreen::ReleaseDynamicTexts()
{
	TextRasterizer::Destroy(m_Title);
	TextRasterizer::Destroy(m_Objective);
	TextRasterizer::Destroy(m_ObjectiveHint);

	for (AtlasNode& node : m_AtlasNodes)
	{
		TextRasterizer::Destroy(node.label);
	}
	m_AtlasNodes.clear();
}

void MapScreen::Open(const StoryDirector& director)
{
	CreateStaticTexts();
	ReleaseDynamicTexts();

	const StoryState& story = director.GetStory();
	const std::string& here = story.Get("MAP");

	m_Title = m_TitleText.Create(director.GetRegionName(here), 500);

	const std::string& objective = director.GetObjectiveText();
	m_Objective = m_BodyText.Create(objective.empty() ? std::string("지금은 따로 할 일이 없다.") : objective, kSidebarWrapWidth);

	std::string travelRegion = director.GetTravelRegion();
	if (!travelRegion.empty())
	{
		m_ObjectiveHint = m_SmallText.Create("▶ 목적지: " + director.GetRegionShortName(travelRegion) + " (이정표로 이동)",
			kSidebarWrapWidth);
	}

	// 지역 약도: 데이터 파일에 선언된 지역 중 약도 자리가 있는 것. 잠긴 지역도 이름은 보여준다(이정표에도 길이 적혀 있다).
	std::string objectiveRegion = director.GetObjectiveRegion();
	for (const StoryRegion& region : director.GetRegions())
	{
		AtlasNode node;
		if (!FindAtlasPlace(region.id, node.x, node.y))
		{
			continue;
		}

		node.locId = region.id;
		node.current = (region.id == here);
		node.unlocked = node.current || story.IsTrue("UNLOCK_" + region.id);
		node.objective = (region.id == objectiveRegion);

		std::string label = director.GetRegionShortName(region.id);
		if (!node.unlocked)
		{
			label += " (잠김)";
		}
		if (node.objective)
		{
			label += " ◆ 목표";
		}

		node.label = m_SmallText.Create(label, (int)kSidebarWidth);
		m_AtlasNodes.push_back(node);
	}

	m_Open = true;
}

void MapScreen::Close()
{
	m_Open = false;
	ReleaseDynamicTexts();
}

void MapScreen::Draw(Renderer& renderer, SceneGraph& scene, const TileMap& tileMap, const Camera& camera,
	const StoryDirector& director, float time)
{
	if (!m_Open)
	{
		return;
	}

	Mat4 ui = UiDraw::ScreenProjection();

	// 화면 전체를 어둡게 덮어 멈춘 게임 화면과 구분한다.
	UiDraw::Rect(renderer, ui, 0.f, 0.f, UiDraw::kScreenWidth, UiDraw::kScreenHeight, 0.f, 0.01f, 0.03f, 0.66f);

	// 왼쪽: 지역 이름 + 큰 지도.
	UiDraw::TextTopLeft(renderer, ui, m_Title, kTitleLeft, kTitleTop, 1.f, 0.86f, 0.5f, 1.f);
	m_Map.Draw(renderer, scene, tileMap, camera, director.GetObjectiveSpots(), time);

	// 오른쪽 칸: 현재 목표(+ 목표가 다른 지역이면 목적지).
	float top = DrawHeader(renderer, ui, m_ObjectiveHeader, kSidebarLeft, kSidebarTop);
	UiDraw::TextTopLeft(renderer, ui, m_Objective, kSidebarLeft, top, 1.f, 0.93f, 0.72f, 1.f);
	top -= (float)m_Objective.height;
	UiDraw::TextTopLeft(renderer, ui, m_ObjectiveHint, kSidebarLeft, top, 0.62f, 0.86f, 1.f, 1.f);
	top -= (float)m_ObjectiveHint.height;

	// 지역 약도.
	top -= kSectionGap;
	top = DrawHeader(renderer, ui, m_AtlasHeader, kSidebarLeft, top);
	DrawAtlas(renderer, ui, kSidebarLeft, top - kAtlasHeight, kSidebarWidth, kAtlasHeight, time);
	top -= kAtlasHeight;

	// 범례.
	top -= kSectionGap;
	top = DrawHeader(renderer, ui, m_LegendHeader, kSidebarLeft, top);
	DrawLegend(renderer, ui, kSidebarLeft, top);

	// 닫기 안내(오른쪽 아래).
	UiDraw::TextTopLeft(renderer, ui, m_CloseHint, UiDraw::kScreenWidth - 12.f - (float)m_CloseHint.width,
		12.f + (float)m_CloseHint.height, 0.85f, 0.85f, 0.85f, 0.9f);
}

void MapScreen::DrawMiniMapHint(Renderer& renderer, const MiniMap& miniMap)
{
	CreateStaticTexts();

	// 미니맵 패널의 왼쪽 아래 구석. 글자 텍스처의 여백(왼쪽 6, 위아래 3픽셀)만큼 바깥으로 당겨서, 글자가
	// 구석에 붙고 지도 마름모의 왼쪽 아래 변에는 닿지 않게 한다.
	Mat4 ui = UiDraw::ScreenProjection();
	float left = miniMap.GetPanelCenterX() - miniMap.GetPanelHalfSize() - 3.f;
	float bottom = miniMap.GetPanelCenterY() - miniMap.GetPanelHalfSize() - 2.f;

	UiDraw::TextTopLeft(renderer, ui, m_MiniMapHint, floorf(left), floorf(bottom + (float)m_MiniMapHint.height),
		0.85f, 0.92f, 1.f, 0.85f);
}

float MapScreen::DrawHeader(Renderer& renderer, const Mat4& ui, const TextTexture& header, float left, float top)
{
	UiDraw::TextTopLeft(renderer, ui, header, left, top, 1.f, 0.82f, 0.45f, 1.f);

	float bottom = top - (float)header.height;
	UiDraw::Rect(renderer, ui, left + 4.f, bottom, kSidebarWidth - 8.f, 1.f, 0.9f, 0.72f, 0.32f, 0.7f);
	return bottom - 4.f;
}

void MapScreen::DrawAtlas(Renderer& renderer, const Mat4& ui, float left, float bottom, float width, float height, float time)
{
	// 바탕: 금빛 테두리 + 짙은 바다색 판.
	UiDraw::Rect(renderer, ui, left - 1.f, bottom - 1.f, width + 2.f, height + 2.f, 0.62f, 0.5f, 0.28f, 0.85f);
	UiDraw::Rect(renderer, ui, left, bottom, width, height, 0.05f, 0.13f, 0.23f, 0.96f);

	// 지역을 잇는 길(점선): 이정표로 어느 지역에서나 갈 수 있는 지역끼리 오가므로 모든 쌍을 잇고,
	// 둘 다 갈 수 있으면 밝게, 아니면 흐리게 그린다.
	for (size_t i = 0; i < m_AtlasNodes.size(); ++i)
	{
		for (size_t j = i + 1; j < m_AtlasNodes.size(); ++j)
		{
			const AtlasNode& from = m_AtlasNodes[i];
			const AtlasNode& to = m_AtlasNodes[j];
			bool open = from.unlocked && to.unlocked;

			DrawDashedLine(renderer, ui, left + from.x * width, bottom + from.y * height, left + to.x * width,
				bottom + to.y * height, 0.8f, 0.75f, 0.6f, open ? 0.75f : 0.22f);
		}
	}

	float pulse = 0.5f + 0.5f * sinf(time * 4.f);
	for (const AtlasNode& node : m_AtlasNodes)
	{
		float x = floorf(left + node.x * width);
		float y = floorf(bottom + node.y * height);

		// 목표가 있는 지역: 뒤에서 숨 쉬는 금빛 마름모.
		if (node.objective)
		{
			const MapColors::Rgb& gold = MapColors::kObjective;
			UiDraw::Diamond(renderer, ui, x, y, 9.f + 2.5f * pulse, gold.r, gold.g, gold.b, 0.35f + 0.25f * pulse);
		}

		// 지역 점: 목표가 있으면 금빛, 갈 수 있으면 하늘색, 잠겼으면 회색.
		MapColors::Rgb color = { 0.45f, 0.45f, 0.47f };
		if (node.objective)
		{
			color = MapColors::kObjective;
		}
		else if (node.unlocked)
		{
			color = MapColors::kSignpost;
		}
		UiDraw::Diamond(renderer, ui, x, y, 6.5f, 0.05f, 0.04f, 0.02f, 1.f);
		UiDraw::Diamond(renderer, ui, x, y, 5.f, color.r, color.g, color.b, 1.f);

		// 지금 있는 지역: 위에서 내려 가리키는 흰 화살촉(범례의 "나"와 같은 모양).
		if (node.current)
		{
			const MapColors::Rgb& white = MapColors::kPlayer;
			DrawArrow(renderer, ui, x, y + 14.f, -1.f, 4.6f, 0.05f, 0.05f, 0.07f, 1.f);
			DrawArrow(renderer, ui, x, y + 14.f, -1.f, 3.6f, white.r, white.g, white.b, 1.f);
		}

		// 이름: 점 아래 가운데(약도 밖으로 나가지 않게 좌우를 맞춘다).
		float labelWidth = (float)node.label.width;
		float labelLeft = x - labelWidth * 0.5f;
		if (labelLeft + labelWidth > left + width - 2.f)
		{
			labelLeft = left + width - 2.f - labelWidth;
		}
		if (labelLeft < left + 2.f)
		{
			labelLeft = left + 2.f;
		}

		float r = 0.78f, g = 0.84f, b = 0.9f;
		if (node.objective)
		{
			r = 1.f; g = 0.86f; b = 0.45f;
		}
		else if (node.current)
		{
			r = 1.f; g = 1.f; b = 1.f;
		}
		else if (!node.unlocked)
		{
			r = 0.55f; g = 0.55f; b = 0.58f;
		}
		UiDraw::TextTopLeft(renderer, ui, node.label, floorf(labelLeft), y - 8.f, r, g, b, 1.f);
	}
}

void MapScreen::DrawLegend(Renderer& renderer, const Mat4& ui, float left, float top)
{
	for (size_t i = 0; i < m_LegendLabels.size(); ++i)
	{
		const LegendEntry& entry = kLegend[i];
		float columnLeft = left + (float)(i % 2) * kLegendColumnWidth;
		float rowCenter = top - kLegendRowHeight * (float)(i / 2) - kLegendRowHeight * 0.5f;

		DrawLegendShape(renderer, ui, entry.shape, columnLeft + 8.f, rowCenter, entry.color);

		const TextTexture& label = m_LegendLabels[i];
		UiDraw::TextTopLeft(renderer, ui, label, columnLeft + 14.f, floorf(rowCenter + (float)label.height * 0.5f),
			0.88f, 0.88f, 0.85f, 1.f);
	}
}
