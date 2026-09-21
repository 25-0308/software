#pragma once

#include "Actor.h"

// 사각형/원/타원만으로 입체 도형(큐브, 원기둥, 타원체)을 조립해서 그리는 함수 모음.
//
// 카메라가 고정(yaw 45°, pitch 55°)이라 월드의 (+x, +y, +z) 쪽에서 내려다본다. 그래서 물체에서
// 실제로 보이는 면은 위(+z), 화면 왼쪽 면(+y), 화면 오른쪽 면(+x) 세 개뿐이고, 이 면들만 회전시킨
// 사각형/원판으로 그리면 입체로 보인다. 세 면이 서로 겹치지 않으므로 면끼리의 그리기 순서는 상관없다.
// (오브젝트 사이의 앞뒤 순서는 씬 그래프가 깊이 정렬로 처리한다.)
namespace Shapes
{
	struct Rgb
	{
		float r, g, b;
	};

	// 색을 k배로 어둡게/밝게. 면마다 밝기를 달리해서(위 > 왼쪽 > 오른쪽) 입체감을 낸다.
	inline Rgb Shade(const Rgb& color, float k)
	{
		Rgb result = { color.r * k, color.g * k, color.b * k };
		return result;
	}

	// 직육면체(큐브). (x, y)는 바닥 중심, baseZ는 바닥 높이. 위/+y면(화면 왼쪽)/+x면(화면 오른쪽)을 그린다.
	void DrawBox(const RenderContext& ctx, float x, float y, float baseZ, float width, float depth, float height,
		const Rgb& top, const Rgb& faceY, const Rgb& faceX);

	// 큐브의 +y면(화면 왼쪽 면) 위에 붙이는 사각 장식(문 등). 중심이 (centerX, faceY, centerZ).
	void DrawPanelOnFaceY(const RenderContext& ctx, float centerX, float faceY, float centerZ,
		float width, float height, const Rgb& color);

	// 큐브의 +x면(화면 오른쪽 면) 위에 붙이는 사각 장식(창문 등). 중심이 (faceX, centerY, centerZ).
	void DrawPanelOnFaceX(const RenderContext& ctx, float faceX, float centerY, float centerZ,
		float width, float height, const Rgb& color);

	// 원기둥. (x, y)는 바닥 중심. 바닥 원판(어둡게) → 옆면(카메라를 향한 세로 사각형) → 윗면 원판 순서로
	// 그려서, 위아래 가장자리가 타원으로 둥글게 보인다.
	void DrawCylinder(const RenderContext& ctx, float x, float y, float baseZ, float radius, float height,
		const Rgb& side, const Rgb& top);

	// 카메라를 향해 세운 타원(타원체의 실루엣). 중심이 (x, y, centerZ)이고 가로 width, 세로 height.
	void DrawUprightEllipse(const RenderContext& ctx, float x, float y, float centerZ,
		float width, float height, const Rgb& color);
}
