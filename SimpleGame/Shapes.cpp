#include "stdafx.h"
#include "Shapes.h"

#include "Renderer.h"

namespace
{
	const float kHalfPi = 1.5707963f;
	const float kQuarterPi = 0.7853982f;
}

// 단위 사각형(XY 평면, 크기 1x1)을 회전시켜 세운 면으로 쓴다:
//  - RotateX(90도): 로컬 y축이 월드 z축이 되어 XZ 평면(폭=x, 높이=z)의 면이 된다.
//  - RotateY(90도): 로컬 x축이 월드 -z축이 되어 YZ 평면(폭=y, 높이=z)의 면이 된다.
// 모델 행렬은 오른쪽부터 적용되므로 Scale → Rotate → Translate 순서다.

void Shapes::DrawBox(const RenderContext& ctx, float x, float y, float baseZ, float width, float depth, float height,
	const Rgb& top, const Rgb& faceY, const Rgb& faceX)
{
	const Mat4& vp = ctx.viewProjection;

	// 위: 수평 사각형.
	Mat4 topModel = Mat4::Translate(x, y, baseZ + height) * Mat4::Scale(width, depth, 1.f);
	ctx.renderer.DrawObject(vp * topModel, top.r, top.g, top.b, 1.f);

	// +y면: XZ 평면의 세운 사각형(화면 왼쪽 면).
	Mat4 faceYModel = Mat4::Translate(x, y + depth * 0.5f, baseZ + height * 0.5f)
		* Mat4::RotateX(kHalfPi) * Mat4::Scale(width, height, 1.f);
	ctx.renderer.DrawObject(vp * faceYModel, faceY.r, faceY.g, faceY.b, 1.f);

	// +x면: YZ 평면의 세운 사각형(화면 오른쪽 면). 회전 후 로컬 x가 z(높이), 로컬 y가 y(깊이)가 된다.
	Mat4 faceXModel = Mat4::Translate(x + width * 0.5f, y, baseZ + height * 0.5f)
		* Mat4::RotateY(kHalfPi) * Mat4::Scale(height, depth, 1.f);
	ctx.renderer.DrawObject(vp * faceXModel, faceX.r, faceX.g, faceX.b, 1.f);
}

void Shapes::DrawPanelOnFaceY(const RenderContext& ctx, float centerX, float faceY, float centerZ,
	float width, float height, const Rgb& color)
{
	Mat4 model = Mat4::Translate(centerX, faceY, centerZ) * Mat4::RotateX(kHalfPi) * Mat4::Scale(width, height, 1.f);
	ctx.renderer.DrawObject(ctx.viewProjection * model, color.r, color.g, color.b, 1.f);
}

void Shapes::DrawPanelOnFaceX(const RenderContext& ctx, float faceX, float centerY, float centerZ,
	float width, float height, const Rgb& color)
{
	Mat4 model = Mat4::Translate(faceX, centerY, centerZ) * Mat4::RotateY(kHalfPi) * Mat4::Scale(height, width, 1.f);
	ctx.renderer.DrawObject(ctx.viewProjection * model, color.r, color.g, color.b, 1.f);
}

void Shapes::DrawCylinder(const RenderContext& ctx, float x, float y, float baseZ, float radius, float height,
	const Rgb& side, const Rgb& top)
{
	const Mat4& vp = ctx.viewProjection;
	float diameter = radius * 2.f;

	// 바닥 원판: 옆면 아래로 둥글게 삐져나와서 밑동이 타원으로 보이게 한다.
	Rgb bottom = Shade(side, 0.6f);
	Mat4 bottomModel = Mat4::Translate(x, y, baseZ) * Mat4::Scale(diameter, diameter, 1.f);
	ctx.renderer.DrawMesh(ctx.circleMesh, vp * bottomModel, bottom.r, bottom.g, bottom.b, 1.f);

	// 옆면: 카메라가 월드 (+1,+1) 방향에서 보므로, 그에 수직인 (1,-1) 방향으로 폭을 가진 세운 사각형.
	// 원기둥은 어느 방향에서 봐도 옆 실루엣이 폭 = 지름인 직사각형이라 이 한 장으로 충분하다.
	Mat4 sideModel = Mat4::Translate(x, y, baseZ + height * 0.5f)
		* Mat4::RotateZ(-kQuarterPi) * Mat4::RotateX(kHalfPi) * Mat4::Scale(diameter, height, 1.f);
	ctx.renderer.DrawObject(vp * sideModel, side.r, side.g, side.b, 1.f);

	// 윗면 원판.
	Mat4 topModel = Mat4::Translate(x, y, baseZ + height) * Mat4::Scale(diameter, diameter, 1.f);
	ctx.renderer.DrawMesh(ctx.circleMesh, vp * topModel, top.r, top.g, top.b, 1.f);
}

void Shapes::DrawUprightEllipse(const RenderContext& ctx, float x, float y, float centerZ,
	float width, float height, const Rgb& color)
{
	// 원 메시(반지름 0.5)를 가로 width, 세로 height로 늘려 타원을 만들고, 카메라를 향해 세운다.
	Mat4 model = Mat4::Translate(x, y, centerZ)
		* Mat4::RotateZ(-kQuarterPi) * Mat4::RotateX(kHalfPi) * Mat4::Scale(width, height, 1.f);
	ctx.renderer.DrawMesh(ctx.circleMesh, ctx.viewProjection * model, color.r, color.g, color.b, 1.f);
}
