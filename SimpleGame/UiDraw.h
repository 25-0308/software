#pragma once

#include "Math3D.h"
#include "TextRasterizer.h"

class Renderer;

// 화면 고정 UI(대화창·목표·지역 배너 등)를 그릴 때 여러 곳에서 같이 쓰는 작은 도구들.
// 좌표계는 HUD·채팅창·미니맵이 함께 쓰는 높이 600의 16:9 UI 좌표(약 1067x600, 원점은 왼쪽 아래)다. 게임 화면은
// 1920x1080이라 UI 좌표 1 = 실제 화면 1.8픽셀(kPixelScale) — 글자는 그만큼 큰 글꼴로 그려 또렷하게 둔다(TextRasterizer).
namespace UiDraw
{
	const float kScreenHeight = 600.f;
	const float kScreenWidth = kScreenHeight * 16.f / 9.f;

	// 게임 화면(1920x1080)의 픽셀 수 / UI 좌표. SimpleGame.cpp의 화면 크기를 바꾸면 같이 바꾼다.
	const float kPixelScale = 1080.f / kScreenHeight;

	// UI 화면 좌표계의 투영 행렬.
	Mat4 ScreenProjection();

	// 왼쪽 아래 모서리 (left, bottom)부터 width x height 크기의 단색 사각형.
	void Rect(Renderer& renderer, const Mat4& ui, float left, float bottom, float width, float height,
		float r, float g, float b, float a);

	// 세 점(화면 픽셀)을 잇는 단색 삼각형.
	void Triangle(Renderer& renderer, const Mat4& ui, float x0, float y0, float x1, float y1, float x2, float y2,
		float r, float g, float b, float a);

	// (centerX, centerY) 중심의 마름모(45도 돌린 정사각형). half는 중심에서 꼭짓점까지의 거리.
	void Diamond(Renderer& renderer, const Mat4& ui, float centerX, float centerY, float half,
		float r, float g, float b, float a);

	// 두 점을 잇는 두께 thickness의 선(가늘고 긴 사각형).
	void Line(Renderer& renderer, const Mat4& ui, float x0, float y0, float x1, float y1, float thickness,
		float r, float g, float b, float a);

	// 글자 텍스처를 왼쪽 위 모서리 (left, top)에 맞춰 텍스처 크기 그대로 그린다(정수 픽셀 위치면 또렷하다).
	void TextTopLeft(Renderer& renderer, const Mat4& ui, const TextTexture& texture, float left, float top,
		float r, float g, float b, float a);

	// 그림 텍스처(초상화 등)를 왼쪽 아래 (left, bottom)부터 width x height로 그린다. 텍스처 색에 (r, g, b, a)를 곱한다.
	void Image(Renderer& renderer, const Mat4& ui, const TextTexture& texture, float left, float bottom, float width, float height,
		float r, float g, float b, float a);
}
