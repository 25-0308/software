#pragma once

#include "Actor.h"

// 사각형/원/타원/삼각형만으로 입체 도형(큐브, 원기둥, 지붕 등)을 조립해서 그리는 함수 모음.
//
// 카메라가 고정(yaw 45°, pitch 55°)이라 월드의 (+x, +y, +z) 쪽에서 내려다본다. 그래서 물체에서
// 실제로 보이는 면은 위(+z), +y면, +x면 세 개뿐이고, 이 면들만 회전시킨 사각형/원판으로 그리면
// 입체로 보인다. 세 면이 서로 겹치지 않으므로 면끼리의 그리기 순서는 상관없다. 현재 카메라 설정
// (좌우·상하 반전)에서는 +y면이 화면 오른쪽(빛을 받는 밝은 면), +x면이 화면 왼쪽(그늘진 면)에 보인다.
// (오브젝트 사이의 앞뒤 순서는 씬 그래프가 깊이 정렬로 처리한다.)
namespace Shapes
{
	struct Rgb
	{
		float r, g, b;
	};

	struct Point3
	{
		float x, y, z;
	};

	// 색을 k배로 어둡게/밝게. 면마다 밝기를 달리해서(위 > +y면 > +x면) 입체감을 낸다.
	inline Rgb Shade(const Rgb& color, float k)
	{
		Rgb result = { color.r * k, color.g * k, color.b * k };
		return result;
	}

	// 직육면체(큐브). (x, y)는 바닥 중심, baseZ는 바닥 높이. 위/+y면/+x면을 각각 주어진 색으로 그린다.
	void DrawBox(const RenderContext& ctx, float x, float y, float baseZ, float width, float depth, float height,
		const Rgb& top, const Rgb& faceY, const Rgb& faceX);

	// 한 가지 색으로 칠한 직육면체: 위 1.0 / +y면 0.82 / +x면 0.62 배 밝기(캐릭터 파츠 등의 기본 명암비).
	void DrawSolidBox(const RenderContext& ctx, float x, float y, float baseZ, float width, float depth, float height,
		const Rgb& color);

	// 큐브의 +y면 위에 붙이는 사각 장식(문 등). 중심이 (centerX, faceY, centerZ).
	void DrawPanelOnFaceY(const RenderContext& ctx, float centerX, float faceY, float centerZ,
		float width, float height, const Rgb& color);

	// 큐브의 +x면 위에 붙이는 사각 장식(창문 등). 중심이 (faceX, centerY, centerZ).
	void DrawPanelOnFaceX(const RenderContext& ctx, float faceX, float centerY, float centerZ,
		float width, float height, const Rgb& color);

	// 월드 좌표 네 점을 차례로 이은 사각형(지붕면처럼 기울어진 면) / 세 점의 삼각형(박공벽 등).
	void DrawQuad(const RenderContext& ctx, const Point3& p0, const Point3& p1, const Point3& p2, const Point3& p3,
		const Rgb& color);
	void DrawTriangle(const RenderContext& ctx, const Point3& p0, const Point3& p1, const Point3& p2, const Rgb& color);

	// 원기둥. (x, y)는 바닥 중심. 바닥 원판(어둡게) → 옆면(카메라를 향한 세로 사각형) → 윗면 원판 순서로
	// 그려서, 위아래 가장자리가 타원으로 둥글게 보인다.
	void DrawCylinder(const RenderContext& ctx, float x, float y, float baseZ, float radius, float height,
		const Rgb& side, const Rgb& top);

	// 카메라를 향해 세운 타원(타원체의 실루엣). 중심이 (x, y, centerZ)이고 가로 width, 세로 height.
	void DrawUprightEllipse(const RenderContext& ctx, float x, float y, float centerZ,
		float width, float height, const Rgb& color);
}
