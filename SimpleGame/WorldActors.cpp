#include "stdafx.h"
#include "WorldActors.h"

#include <cmath>

#include "Shapes.h"

// ------------------------------------------------------------ TileBatchActor

TileBatchActor::TileBatchActor(bool isWater)
	: Actor(ActorType::TileBatch, RenderLayer::Ground)
	, m_IsWater(isWater)
{
}

TileBatchActor::~TileBatchActor()
{
	if (m_Renderer != nullptr)
	{
		m_Renderer->DestroyTileBatch(m_Batch);
	}
}

void TileBatchActor::Build(Renderer& renderer, const std::vector<float>& vertices)
{
	m_Renderer = &renderer;
	int vertexCount = (int)(vertices.size() / Renderer::kTileBatchFloatsPerVertex);
	m_Batch = renderer.CreateTileBatch(vertices.data(), vertexCount);
}

void TileBatchActor::OnRender(const RenderContext& ctx)
{
	// 정점이 이미 월드 좌표로 구워져 있어서(LevelBuilder 참고) 오브젝트별 모델 행렬이 필요
	// 없다 — 카메라의 view-projection만 곱하면 된다.
	if (m_IsWater)
	{
		ctx.renderer.DrawTileBatchWater(m_Batch, ctx.viewProjection, ctx.time);
	}
	else
	{
		ctx.renderer.DrawTileBatchSolid(m_Batch, ctx.viewProjection);
	}
}

// ---------------------------------------------------------------- TreeActor

TreeActor::TreeActor(float x, float y, float size, float r, float g, float b)
	: Actor(ActorType::Prop)
{
	SetPosition(x, y, 0.f);
	SetSize(size);
	SetColor(r, g, b);

	// 몸통·수관·발밑 그림자(가로 0.9*size)까지 감싼다.
	SetBoundingSphere(size * 1.15f, size * 0.5f);
}

void TreeActor::OnRender(const RenderContext& ctx)
{
	float x = GetWorldX();
	float y = GetWorldY();
	float z = GetWorldZ();
	float size = GetSize();
	float sway = sinf(ctx.time * 0.6f + x * 2.1f) * size * 0.03f; // 산들바람에 흔들리는 느낌

	// 줄기: 원기둥(반지름 0.09*size, 높이 0.55*size). 윗면 원판은 옆면보다 밝게.
	Shapes::Rgb wood = { 0.35f, 0.22f, 0.12f };
	Shapes::DrawCylinder(ctx, x, y, z, size * 0.09f, size * 0.55f, wood, Shapes::Shade(wood, 1.25f));

	// 수관: 카메라를 향해 세운 큰 타원(줄기 윗부분을 덮는다).
	Shapes::Rgb leaf = { GetR(), GetG(), GetB() };
	Shapes::DrawUprightEllipse(ctx, x + sway, y, z + size * 0.85f, size * 1.0f, size * 0.85f, leaf);

	// 하이라이트: 빛이 오는 화면 왼쪽 위로 치우친 더 작고 밝은 타원을 겹쳐서 둥근 덩어리처럼 보이게 한다.
	// (화면 왼쪽 = 월드 (-1,+1) 방향)
	Shapes::Rgb light = { leaf.r * 1.2f + 0.05f, leaf.g * 1.15f + 0.05f, leaf.b * 1.1f };
	float shift = size * 0.18f * 0.7071f;
	Shapes::DrawUprightEllipse(ctx, x + sway * 1.4f - shift, y + shift, z + size * 1.0f, size * 0.55f, size * 0.45f, light);
}

// ------------------------------------------------------------ BuildingActor

BuildingActor::BuildingActor(float x, float y, float size)
	: Actor(ActorType::Building)
{
	SetPosition(x, y, 0.f);
	SetSize(size);

	// 큐브(꼭짓점까지 약 0.87*size)·넓게 깔리는 그림자(가로 0.85*size)·자식 횃불이 들어갈 만큼 넉넉히.
	SetBoundingSphere(size * 1.2f, size * 0.5f);
}

void BuildingActor::OnRender(const RenderContext& ctx)
{
	float x = GetWorldX();
	float y = GetWorldY();
	float z = GetWorldZ();
	float size = GetSize();

	Shapes::Rgb wall = { 0.55f, 0.40f, 0.28f };
	Shapes::Rgb roof = { 0.42f, 0.22f, 0.14f };

	// 한 변이 size인 큐브. 위는 지붕색, 앞 두 면은 벽색이며 빛이 오는 왼쪽 면이 오른쪽 면보다 밝다.
	Shapes::DrawBox(ctx, x, y, z, size, size, size, roof, Shapes::Shade(wall, 0.85f), Shapes::Shade(wall, 0.62f));

	// 문(화면 왼쪽 면)과 불 켜진 창문(화면 오른쪽 면): 큐브가 집으로 읽히게 하는 사각 장식.
	// 창문은 밝은 색이라 블룸으로 은은하게 빛난다.
	Shapes::Rgb door = { 0.18f, 0.10f, 0.06f };
	Shapes::DrawPanelOnFaceY(ctx, x - size * 0.12f, y + size * 0.5f, z + size * 0.27f, size * 0.26f, size * 0.54f, door);

	Shapes::Rgb window = { 1.3f, 1.05f, 0.5f };
	Shapes::DrawPanelOnFaceX(ctx, x + size * 0.5f, y, z + size * 0.6f, size * 0.26f, size * 0.26f, window);
}

// 큐브 발자국보다 넓은 그림자를 깔아서, 큐브가 바닥 위에 떠 보이지 않고 붙어 보이게 한다.
void BuildingActor::OnRenderShadow(const RenderContext& ctx)
{
	Mat4 model = Mat4::Translate(GetWorldX(), GetWorldY(), 0.001f) * Mat4::Scale(GetSize() * 1.7f, GetSize() * 1.4f, 1.f);
	ctx.renderer.DrawShadow(ctx.viewProjection * model, 0.5f);
}

// 눈에 보이는 큐브 발자국(한 변 size인 정사각형)과 반지름 moverRadius인 원의 겹침 판정.
// 원 중심에서 정사각형까지의 최단 거리가 moverRadius보다 짧으면 막힌다(중심이 안쪽이면 0).
bool BuildingActor::BlocksCircle(float x, float y, float moverRadius) const
{
	float half = GetSize() * 0.5f;
	float dx = fabsf(x - GetWorldX()) - half;
	float dy = fabsf(y - GetWorldY()) - half;

	if (dx < 0.f) dx = 0.f;
	if (dy < 0.f) dy = 0.f;

	return dx * dx + dy * dy < moverRadius * moverRadius;
}

// ---------------------------------------------------------------- ItemActor

ItemActor::ItemActor(float x, float y, float size, float r, float g, float b, int interactId)
	: Actor(ActorType::Item)
{
	SetPosition(x, y, 0.f);
	SetSize(size);
	SetColor(r, g, b);
	SetInteractId(interactId);

	// 위아래로 떠다니는 폭(±0.15*size)까지 감싼다.
	SetBoundingSphere(size * 0.9f, size * 0.4f);
}

void ItemActor::OnRender(const RenderContext& ctx)
{
	float size = GetSize();
	float bob = sinf(ctx.time * 3.f + GetWorldX() * 4.f) * size * 0.15f;

	Mat4 model = Mat4::Translate(GetWorldX(), GetWorldY(), GetWorldZ() + size * 0.4f + bob)
		* Mat4::Scale(size, size, size);
	ctx.renderer.DrawMesh(ctx.ellipseMesh, ctx.viewProjection * model, GetR(), GetG(), GetB(), GetA());
}

// ---------------------------------------------------------------- FireActor

FireActor::FireActor(float localX, float localY, float localZ, float size)
	: Actor(ActorType::Fire)
{
	SetPosition(localX, localY, localZ);
	SetSize(size);

	SetBoundingSphere(size * 0.9f);
}

void FireActor::OnRender(const RenderContext& ctx)
{
	float size = GetSize();
	Mat4 model = Mat4::Translate(GetWorldX(), GetWorldY(), GetWorldZ()) * Mat4::Scale(size, size, size);
	float phase = GetWorldX() * 1.3f + GetWorldY() * 0.7f;

	ctx.renderer.DrawFire(ctx.viewProjection * model, ctx.time, phase);
}

// ---------------------------------------------------------------- RingActor

RingActor::RingActor(const RingStyle& style, bool followsParent)
	: Actor(ActorType::Marker, RenderLayer::Decal)
	, m_Style(style)
	, m_FollowsParent(followsParent)
{
	// 가장 큰 대상(크기 1.0, 반지름 배율 최대 0.9)의 링을 감싼다.
	SetBoundingSphere(1.2f);
}

void RingActor::SetTarget(const Actor* target)
{
	m_HasTarget = (target != nullptr);

	if (target != nullptr)
	{
		SetPosition(target->GetWorldX(), target->GetWorldY(), 0.f);
		m_TargetSize = target->GetSize();
	}
}

void RingActor::OnRender(const RenderContext& ctx)
{
	float size;

	if (m_FollowsParent)
	{
		if (GetParent() == nullptr)
		{
			return;
		}

		size = GetParent()->GetSize();
	}
	else
	{
		if (!m_HasTarget)
		{
			return;
		}

		size = m_TargetSize;
	}

	// 두 모드 모두 링 자신의 월드 위치에 그린다(마커는 부모를 따라 이동, 조준 링은 SetTarget이 옮겨 둠).
	float x = GetWorldX();
	float y = GetWorldY();

	float pulse = 0.5f + 0.5f * sinf(ctx.time * m_Style.pulseSpeed);
	float radius = size * (m_Style.baseScale + pulse * m_Style.pulseScale);
	float alpha = m_Style.baseAlpha + pulse * m_Style.pulseAlpha;

	Mat4 model = Mat4::Translate(x, y, 0.0006f) * Mat4::Scale(radius, radius * 0.7f, 1.f);
	ctx.renderer.DrawMesh(ctx.ellipseMesh, ctx.viewProjection * model, m_Style.r, m_Style.g, m_Style.b, alpha);
}
