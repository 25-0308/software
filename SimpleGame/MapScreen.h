#pragma once

#include <string>
#include <vector>

#include "Math3D.h"
#include "MiniMap.h"
#include "TextRasterizer.h"

class Camera;
class Renderer;
class SceneGraph;
class StoryDirector;
class TileMap;

// 큰 지도 화면(M 키로 열고 M·Esc로 닫는다). 화면을 어둡게 덮고 하나로 이어진 월드 전체를 보여준다 — 지형·길·
// 건물·인물·괴물과 지역 이름, 지금 목표(금빛 마름모 + 퍼져 나가는 고리), 플레이어(바라보는 쪽 화살촉). 큰 지도 자체는
// 미니맵(MiniMap)과 같은 코드를 크게 놓아 그린다. 오른쪽 칸엔 지금 목표 문구(목표가 다른 지역이면 목적지),
// 마을 목록(숭배하는 신·분위기, 지금 있는 곳·목표·막힌 성역), 범례가 뜬다. 열려 있는 동안 게임은 멈춘다
// (SimpleGame.cpp — 대화창이 열려 있을 때와 같은 방식). 닫혀 있을 땐 미니맵 구석에 "M 지도" 안내만 작게 띄운다.
// 후처리가 끝난 뒤 HUD처럼 화면 고정 좌표계(800x600)로, 모든 UI의 맨 위에 그린다.
class MapScreen
{
public:
	MapScreen();
	~MapScreen();

	MapScreen(const MapScreen&) = delete;
	MapScreen& operator=(const MapScreen&) = delete;

	bool IsOpen() const { return m_Open; }

	// 연다: 지금 지역 이름·목표·지역 상태로 글자를 새로 만든다(열려 있는 동안은 게임이 멈춰 있어 바뀌지 않는다).
	void Open(const StoryDirector& director);
	void Close();

	// 다른 지역으로 옮겨 맵이 바뀌었을 때 부른다(미니맵과 같이).
	void Invalidate() { m_Map.Invalidate(); }

	void Draw(Renderer& renderer, SceneGraph& scene, const TileMap& tileMap, const Camera& camera,
		const StoryDirector& director, float time);

	// 지도가 닫혀 있을 때: 미니맵 왼쪽 아래 구석(지도 마름모 바깥의 바다)에 "M 지도" 안내를 작게 띄운다.
	void DrawMiniMapHint(Renderer& renderer, const MiniMap& miniMap);

private:
	// 월드의 지역 하나(데이터 파일에 선언된 지역 중 월드에 놓인 것 — 이야기 무대는 빼고).
	struct RegionEntry
	{
		std::string locId;
		float worldX = 0.f;     // 지역 가운데(월드 좌표) — 지도 위 이름표 자리
		float worldY = 0.f;
		bool current = false;   // 지금 서 있는 지역
		bool locked = false;    // 아직 안개가 길을 막고 있는 성역
		bool objective = false; // 지금 목표가 있는 지역
		TextTexture mapLabel;   // 지도 위 이름표(짧은 이름)
		TextTexture listLine;   // 오른쪽 목록 한 줄(이름 · 숭배하는 신 · 분위기)
	};

	void CreateStaticTexts();
	void ReleaseDynamicTexts();

	// 섹션 제목(굵은 글씨 + 금빛 밑줄)을 top에 그리고, 그 아래 다음 내용이 시작할 top을 돌려준다.
	float DrawHeader(Renderer& renderer, const Mat4& ui, const TextTexture& header, float left, float top);
	void DrawRegionLabels(Renderer& renderer, const Mat4& ui, float time);
	float DrawRegionList(Renderer& renderer, const Mat4& ui, float left, float top);
	void DrawLegend(Renderer& renderer, const Mat4& ui, float left, float top);

	bool m_Open = false;
	MiniMap m_Map;

	TextRasterizer m_TitleText;
	TextRasterizer m_HeaderText;
	TextRasterizer m_BodyText;
	TextRasterizer m_SmallText;

	// 한 번 만들어 계속 쓰는 글자(OpenGL 컨텍스트가 있어야 해서 처음 쓸 때 만든다).
	bool m_StaticTextsCreated = false;
	TextTexture m_ObjectiveHeader;
	TextTexture m_AtlasHeader;
	TextTexture m_LegendHeader;
	TextTexture m_CloseHint;
	TextTexture m_MiniMapHint;
	std::vector<TextTexture> m_LegendLabels; // 범례 표(MapScreen.cpp의 kLegend)와 같은 순서

	// 열 때마다 새로 만드는 글자.
	TextTexture m_Title;
	TextTexture m_Objective;
	TextTexture m_ObjectiveHint;
	std::vector<RegionEntry> m_Regions;
};
