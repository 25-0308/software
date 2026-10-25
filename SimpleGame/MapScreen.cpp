#include "stdafx.h"
#include "MapScreen.h"

#include <cmath>

#include "Renderer.h"
#include "StoryDirector.h"
#include "UiDraw.h"
#include "WorldLayout.h"

namespace
{
	// 큰 지도(화면 왼쪽): 미니맵과 같은 코드를 크게 놓아 월드 전체를 보여 준다. 점·마름모 같은 표시는 지도만큼 키우지
	// 않고 조금만 키운다.
	const float kMapCenterX = 292.f;
	const float kMapCenterY = 290.f;
	const float kMapHalfSize = 248.f;
	const float kMapMarkerScale = 1.4f;

	// 제목(큰 지도 위).
	const float kTitleLeft = 44.f;
	const float kTitleTop = 594.f;

	// 오른쪽 칸(현재 목표·마을 목록·범례).
	const float kSidebarLeft = 560.f;
	const float kSidebarTop = 594.f;
	const float kSidebarWidth = 228.f;
	const int kSidebarWrapWidth = 228;
	const float kSectionGap = 12.f;
	const float kLegendColumnWidth = 114.f;
	const float kLegendRowHeight = 19.f;

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
		{ "온순한 짐승", MapColors::kDeer, LegendShape::Square },
		{ "몬스터", MapColors::kWolf, LegendShape::Square },
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
}

MapScreen::MapScreen()
	: m_Map(kMapCenterX, kMapCenterY, kMapHalfSize, kMapMarkerScale, 0.f)
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
	m_AtlasHeader = m_HeaderText.Create("마을 · 숭배하는 신 · 분위기", kSidebarWrapWidth);
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

	for (RegionEntry& entry : m_Regions)
	{
		TextRasterizer::Destroy(entry.mapLabel);
		TextRasterizer::Destroy(entry.listLine);
	}
	m_Regions.clear();
}

void MapScreen::Open(const StoryDirector& director)
{
	CreateStaticTexts();
	ReleaseDynamicTexts();

	const StoryState& story = director.GetStory();
	const std::string& current = director.GetCurrentRegion();
	bool inStage = WorldLayout::IsStage(current);

	std::string place = current.empty() ? std::string("황야 — 마을과 마을 사이") : director.GetRegionName(current);
	m_Title = m_TitleText.Create(inStage ? place : "그리스 세계 — " + place, 500);

	const std::string& objective = director.GetObjectiveText();
	m_Objective = m_BodyText.Create(objective.empty() ? std::string("지금은 따로 할 일이 없다.") : objective, kSidebarWrapWidth);

	std::string travelRegion = director.GetTravelRegion();
	if (!travelRegion.empty())
	{
		m_ObjectiveHint = m_SmallText.Create("▶ 목적지: " + director.GetRegionShortName(travelRegion) + " — 길을 따라 걸어가자",
			kSidebarWrapWidth);
	}

	// 마을 목록과 지도 위 이름표: 데이터 파일에 선언된 지역 중 월드에 놓인 것(무대 안에선 지도가 무대뿐이라 이름표는 없다).
	std::string objectiveRegion = director.GetObjectiveRegion();
	for (const StoryRegion& region : director.GetRegions())
	{
		const WorldPlace* worldPlace = WorldLayout::Find(region.id);
		if (worldPlace == nullptr || worldPlace->kind == PlaceKind::Stage)
		{
			continue;
		}

		RegionEntry entry;
		entry.locId = region.id;
		entry.worldX = worldPlace->centerX;
		entry.worldY = worldPlace->centerY;
		entry.current = (region.id == current);
		entry.locked = (worldPlace->kind == PlaceKind::Sanctuary) && !story.IsTrue("UNLOCK_" + region.id);
		entry.objective = (region.id == objectiveRegion);

		std::string shortName = director.GetRegionShortName(region.id);
		if (!inStage)
		{
			entry.mapLabel = m_SmallText.Create(shortName, 160);
		}

		std::string line = (entry.objective ? "◆ " : "") + shortName + " · " + worldPlace->worshipLabel + " · " + worldPlace->mood.label;
		if (entry.locked)
		{
			line += " (막힘)";
		}
		entry.listLine = m_SmallText.Create(line, kSidebarWrapWidth);
		m_Regions.push_back(entry);
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

	// 왼쪽: 제목 + 큰 지도 + 지역 이름표.
	UiDraw::TextTopLeft(renderer, ui, m_Title, kTitleLeft, kTitleTop, 1.f, 0.86f, 0.5f, 1.f);
	m_Map.Draw(renderer, scene, tileMap, camera, director.GetObjectiveSpots(), time);
	DrawRegionLabels(renderer, ui, time);

	// 오른쪽 칸: 현재 목표(+ 목표가 다른 지역이면 목적지).
	float top = DrawHeader(renderer, ui, m_ObjectiveHeader, kSidebarLeft, kSidebarTop);
	UiDraw::TextTopLeft(renderer, ui, m_Objective, kSidebarLeft, top, 1.f, 0.93f, 0.72f, 1.f);
	top -= (float)m_Objective.height;
	UiDraw::TextTopLeft(renderer, ui, m_ObjectiveHint, kSidebarLeft, top, 0.62f, 0.86f, 1.f, 1.f);
	top -= (float)m_ObjectiveHint.height;

	// 마을 목록.
	top -= kSectionGap;
	top = DrawHeader(renderer, ui, m_AtlasHeader, kSidebarLeft, top);
	top = DrawRegionList(renderer, ui, kSidebarLeft, top);

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

void MapScreen::DrawRegionLabels(Renderer& renderer, const Mat4& ui, float time)
{
	float pulse = 0.5f + 0.5f * sinf(time * 4.f);

	for (const RegionEntry& entry : m_Regions)
	{
		if (entry.mapLabel.id == 0)
		{
			continue;
		}

		float x, y;
		m_Map.WorldToPanel(entry.worldX, entry.worldY, x, y);
		x = floorf(x);
		y = floorf(y);

		// 목표가 있는 지역: 뒤에서 숨 쉬는 금빛 마름모.
		if (entry.objective)
		{
			const MapColors::Rgb& gold = MapColors::kObjective;
			UiDraw::Diamond(renderer, ui, x, y, 9.f + 2.5f * pulse, gold.r, gold.g, gold.b, 0.3f + 0.25f * pulse);
		}

		// 이름표: 어두운 띠 위에, 지금 있는 곳은 흰색·목표는 금빛·막힌 성역은 회색.
		float width = (float)entry.mapLabel.width;
		float height = (float)entry.mapLabel.height;
		float left = floorf(x - width * 0.5f);
		float top = floorf(y + height * 0.5f);
		UiDraw::Rect(renderer, ui, left + 3.f, top - height + 2.f, width - 6.f, height - 4.f, 0.02f, 0.03f, 0.05f, 0.55f);

		float r = 0.86f, g = 0.9f, b = 0.95f;
		if (entry.current)
		{
			r = 1.f; g = 1.f; b = 1.f;
		}
		else if (entry.objective)
		{
			r = 1.f; g = 0.86f; b = 0.45f;
		}
		else if (entry.locked)
		{
			r = 0.6f; g = 0.6f; b = 0.64f;
		}
		UiDraw::TextTopLeft(renderer, ui, entry.mapLabel, left, top, r, g, b, 1.f);
	}
}

float MapScreen::DrawRegionList(Renderer& renderer, const Mat4& ui, float left, float top)
{
	for (const RegionEntry& entry : m_Regions)
	{
		float r = 0.82f, g = 0.86f, b = 0.9f;
		if (entry.current)
		{
			r = 1.f; g = 1.f; b = 1.f;

			// 지금 있는 곳: 줄 앞의 흰 화살촉(범례의 "나"와 같은 모양, 옆을 가리키게 눕히지 않고 아래로).
			DrawArrow(renderer, ui, left - 7.f, top - (float)entry.listLine.height * 0.5f, -1.f, 3.2f, 1.f, 1.f, 1.f, 1.f);
		}
		else if (entry.objective)
		{
			r = 1.f; g = 0.86f; b = 0.45f;
		}
		else if (entry.locked)
		{
			r = 0.55f; g = 0.55f; b = 0.58f;
		}

		UiDraw::TextTopLeft(renderer, ui, entry.listLine, left, top, r, g, b, 1.f);
		top -= (float)entry.listLine.height;
	}
	return top;
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
