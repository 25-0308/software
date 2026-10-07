#pragma once

#include "Math3D.h"
#include "TextRasterizer.h"

class Renderer;

// 화면 고정 UI(대화창·목표·지역 배너 등)를 그릴 때 여러 곳에서 같이 쓰는 작은 도구들.
// 좌표계는 HUD·채팅창과 같은 800x600 픽셀(원점은 왼쪽 아래)이다.
namespace UiDraw
{
	const float kScreenWidth = 800.f;
	const float kScreenHeight = 600.f;

	// 800x600 화면 좌표계의 투영 행렬.
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
}
