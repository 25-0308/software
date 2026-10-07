#include "stdafx.h"
#include "UiDraw.h"

#include <cmath>

#include "Renderer.h"

namespace
{
	// 화면 픽셀 좌표의 사각형(왼쪽 아래 기준)을 단위 사각형으로 그리기 위한 모델 행렬.
	Mat4 RectModel(float left, float bottom, float width, float height)
	{
		return Mat4::Translate(left + width * 0.5f, bottom + height * 0.5f, 0.f) * Mat4::Scale(width, height, 1.f);
	}
}

Mat4 UiDraw::ScreenProjection()
{
	return Mat4::Ortho(0.f, kScreenWidth, 0.f, kScreenHeight, -1.f, 1.f);
}

void UiDraw::Rect(Renderer& renderer, const Mat4& ui, float left, float bottom, float width, float height,
	float r, float g, float b, float a)
{
	renderer.DrawObject(ui * RectModel(left, bottom, width, height), r, g, b, a);
}

void UiDraw::Triangle(Renderer& renderer, const Mat4& ui, float x0, float y0, float x1, float y1, float x2, float y2,
	float r, float g, float b, float a)
{
	const float vertices[9] =
	{
		x0, y0, 0.f,  x1, y1, 0.f,  x2, y2, 0.f,
	};
	renderer.DrawTriangles(vertices, 3, ui, r, g, b, a);
}

void UiDraw::Diamond(Renderer& renderer, const Mat4& ui, float centerX, float centerY, float half,
	float r, float g, float b, float a)
{
	const float vertices[18] =
	{
		centerX, centerY - half, 0.f,  centerX + half, centerY, 0.f,  centerX, centerY + half, 0.f,
		centerX, centerY - half, 0.f,  centerX, centerY + half, 0.f,  centerX - half, centerY, 0.f,
	};
	renderer.DrawTriangles(vertices, 6, ui, r, g, b, a);
}

void UiDraw::Line(Renderer& renderer, const Mat4& ui, float x0, float y0, float x1, float y1, float thickness,
	float r, float g, float b, float a)
{
	float dx = x1 - x0;
	float dy = y1 - y0;
	float length = sqrtf(dx * dx + dy * dy);
	if (length <= 0.f)
	{
		return;
	}

	// 선 방향에 수직인 쪽으로 두께의 절반씩 벌린 사각형.
	float sideX = -dy / length * thickness * 0.5f;
	float sideY = dx / length * thickness * 0.5f;

	const float vertices[18] =
	{
		x0 + sideX, y0 + sideY, 0.f,  x1 + sideX, y1 + sideY, 0.f,  x1 - sideX, y1 - sideY, 0.f,
		x0 + sideX, y0 + sideY, 0.f,  x1 - sideX, y1 - sideY, 0.f,  x0 - sideX, y0 - sideY, 0.f,
	};
	renderer.DrawTriangles(vertices, 6, ui, r, g, b, a);
}

void UiDraw::TextTopLeft(Renderer& renderer, const Mat4& ui, const TextTexture& texture, float left, float top,
	float r, float g, float b, float a)
{
	if (texture.id == 0)
	{
		return;
	}

	float width = (float)texture.width;
	float height = (float)texture.height;
	renderer.DrawTexture(texture.id, ui * RectModel(left, top - height, width, height), r, g, b, a);
}
