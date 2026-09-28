#include "stdafx.h"
#include "WorldActors.h"

#include <cmath>

#include "Shapes.h"

namespace
{
	const float kHalfPi = 1.5707963f;
	const float kQuarterPi = 0.7853982f;

	// 화면 가로 방향 단위 벡터의 한 성분(월드 (1,-1)/√2 = 현재 카메라에서 화면 왼쪽). 카메라를 향해 세운
	// 도형(수관, 아이템)을 화면 좌우로 벌려 놓을 때 쓴다 — 이 방향으로 옮기면 깊이(앞뒤)는 그대로다.
	const float kScreenAxis = 0.7071f;

	float Fract(float value)
	{
		return value - floorf(value);
	}

	// 정수 번호마다 고정된 0~1 난수(같은 번호면 항상 같은 값).
	float IndexRandom(int index, float salt)
	{
		return Fract(sinf((float)index * 12.9898f + salt * 78.233f) * 43758.5453f);
	}
}

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
		ctx.renderer.DrawTileBatchSolid(m_Batch, ctx.viewProjection, ctx.time);
	}
}

// ---------------------------------------------------------------- TreeActor

TreeActor::TreeActor(float x, float y, float size, float r, float g, float b, TreeKind kind)
	: Actor(ActorType::Prop)
	, m_Kind(kind)
{
	SetPosition(x, y, 0.f);
	SetSize(size);
	SetColor(r, g, b);

	// 줄기·수관(사이프러스는 약 1.75*size까지 솟음)·발밑 그림자까지 감싼다.
	SetBoundingSphere(size * 1.2f, size * 0.7f);
}

void TreeActor::OnRender(const RenderContext& ctx)
{
	float x = GetWorldX();
	float y = GetWorldY();
	float z = GetWorldZ();
	float size = GetSize();
	float sway = sinf(ctx.time * 0.6f + x * 2.1f) * size * 0.03f; // 산들바람에 흔들리는 느낌

	Shapes::Rgb wood = { 0.36f, 0.24f, 0.14f };
	Shapes::Rgb leaf = { GetR(), GetG(), GetB() };
	Shapes::Rgb shadowLeaf = Shapes::Shade(leaf, 0.6f);
	Shapes::Rgb lightLeaf = { leaf.r * 1.25f + 0.05f, leaf.g * 1.2f + 0.05f, leaf.b * 1.1f + 0.02f };

	if (m_Kind == TreeKind::Cypress)
	{
		// 짧게 드러난 줄기 + 가늘고 높은 짙은 수관(뒤쪽 어두운 윤곽 → 본체 → 빛 받는 쪽 세로 하이라이트).
		Shapes::DrawCylinder(ctx, x, y, z, size * 0.06f, size * 0.3f, wood, Shapes::Shade(wood, 1.25f));

		float lean = sway * 0.5f;
		Shapes::DrawUprightEllipse(ctx, x + lean, y, z + size * 0.98f, size * 0.5f, size * 1.5f, shadowLeaf);
		Shapes::DrawUprightEllipse(ctx, x + lean, y, z + size * 1.0f, size * 0.42f, size * 1.4f, leaf);

		float shift = size * 0.07f * kScreenAxis;
		Shapes::DrawUprightEllipse(ctx, x + lean - shift, y + shift, z + size * 1.12f, size * 0.18f, size * 0.9f, lightLeaf);
		return;
	}

	// 활엽수 줄기: 원기둥(반지름 0.08*size, 높이 0.55*size). 윗면 원판은 옆면보다 밝게.
	Shapes::DrawCylinder(ctx, x, y, z, size * 0.08f, size * 0.55f, wood, Shapes::Shade(wood, 1.25f));

	// 수관: 뒤쪽 어두운 덩어리(그늘·윤곽) → 화면 좌우 두 덩어리 → 위 덩어리 순으로 겹쳐 뭉게뭉게한
	// 실루엣을 만들고, 빛이 오는 화면 오른쪽 위(월드 (-1,+1) 방향)에 밝은 하이라이트를 얹는다.
	float spread = size * 0.2f * kScreenAxis;
	Shapes::DrawUprightEllipse(ctx, x + sway * 0.6f, y, z + size * 0.86f, size * 1.16f, size * 0.96f, shadowLeaf);
	Shapes::DrawUprightEllipse(ctx, x + sway + spread, y - spread, z + size * 0.8f, size * 0.7f, size * 0.62f, leaf);
	Shapes::DrawUprightEllipse(ctx, x + sway - spread, y + spread, z + size * 0.82f, size * 0.7f, size * 0.64f, leaf);
	Shapes::DrawUprightEllipse(ctx, x + sway, y, z + size * 1.02f, size * 0.66f, size * 0.52f, leaf);

	float shift = size * 0.16f * kScreenAxis;
	Shapes::DrawUprightEllipse(ctx, x + sway * 1.3f - shift, y + shift, z + size * 1.06f, size * 0.42f, size * 0.32f, lightLeaf);
}

// 수관 크기에 맞춘 둥근 그림자(사이프러스는 가늘어서 작게).
void TreeActor::OnRenderShadow(const RenderContext& ctx)
{
	float radius = (m_Kind == TreeKind::Cypress) ? GetSize() * 0.55f : GetSize() * 1.0f;
	Mat4 model = Mat4::Translate(GetWorldX(), GetWorldY(), 0.001f) * Mat4::Scale(radius, radius, 1.f);
	ctx.renderer.DrawShadow(ctx.viewProjection * model, 0.4f);
}

// ------------------------------------------------------------ BuildingActor

BuildingActor::BuildingActor(float x, float y, float size)
	: Actor(ActorType::Building)
{
	SetPosition(x, y, 0.f);
	SetSize(size);

	// 벽+지붕(용마루 약 1.07*size)·넓게 깔리는 그림자(가로 0.85*size)·자식 횃불이 들어갈 만큼 넉넉히.
	SetBoundingSphere(size * 1.2f, size * 0.5f);
}

void BuildingActor::OnRender(const RenderContext& ctx)
{
	float x = GetWorldX();
	float y = GetWorldY();
	float z = GetWorldZ();
	float size = GetSize();
	float half = size * 0.5f;

	// 흰 회벽 + 테라코타 기와 지붕 + 푸른 문·덧창: 지중해(그리스) 마을 집.
	Shapes::Rgb stone = { 0.56f, 0.53f, 0.49f };
	Shapes::Rgb wall = { 0.90f, 0.86f, 0.78f };
	Shapes::Rgb roof = { 0.72f, 0.33f, 0.19f };
	Shapes::Rgb door = { 0.16f, 0.33f, 0.56f };
	Shapes::Rgb doorFrame = Shapes::Shade(wall, 0.7f);
	Shapes::Rgb window = { 1.3f, 1.05f, 0.5f }; // 1을 넘는 밝기라 블룸으로 은은히 빛난다(불 켜진 창)
	Shapes::Rgb shutter = { 0.20f, 0.40f, 0.62f };

	float plinthHeight = size * 0.07f;
	float wallBase = z + plinthHeight;
	float wallTop = wallBase + size * 0.68f;
	float ridgeZ = wallTop + size * 0.32f;
	float overhang = size * 0.08f;

	// 1) 돌 기단(벽보다 살짝 넓고 낮게) → 2) 벽. 벽을 기단 위에서 시작해야 기단 앞 테두리가 벽에
	//    덮이지 않고 보인다. +y면은 화면 오른쪽(빛 받는 면), +x면은 화면 왼쪽(그늘 면)이다.
	Shapes::DrawSolidBox(ctx, x, y, z, size * 1.08f, size * 1.08f, plinthHeight, stone);
	Shapes::DrawBox(ctx, x, y, wallBase, size, size, wallTop - wallBase, wall, Shapes::Shade(wall, 0.92f), Shapes::Shade(wall, 0.66f));

	// 3) +y면: 문틀 + 푸른 문, 옆에 작은 창.
	Shapes::DrawPanelOnFaceY(ctx, x - size * 0.12f, y + half, wallBase + size * 0.28f, size * 0.3f, size * 0.56f, doorFrame);
	Shapes::DrawPanelOnFaceY(ctx, x - size * 0.12f, y + half, wallBase + size * 0.26f, size * 0.24f, size * 0.52f, door);
	Shapes::DrawPanelOnFaceY(ctx, x - size * 0.37f, y + half, wallBase + size * 0.42f, size * 0.14f, size * 0.16f, window);

	// 4) +x면: 불 켜진 창 + 양쪽 푸른 덧창.
	Shapes::DrawPanelOnFaceX(ctx, x + half, y, wallBase + size * 0.38f, size * 0.24f, size * 0.24f, window);
	Shapes::DrawPanelOnFaceX(ctx, x + half, y - size * 0.19f, wallBase + size * 0.38f, size * 0.1f, size * 0.28f, shutter);
	Shapes::DrawPanelOnFaceX(ctx, x + half, y + size * 0.19f, wallBase + size * 0.38f, size * 0.1f, size * 0.28f, shutter);

	// 5) 박공지붕: 용마루가 y축 방향이라 +y면(문 쪽) 위에 삼각형 박공벽이 생긴다. 박공벽 → 뒤(-x)
	//    지붕면 → 앞(+x) 지붕면 순으로 그려서, 처마가 박공벽 가장자리와 뒷면을 자연스럽게 덮게 한다.
	//    뒷면은 빛이 오는 화면 오른쪽을 향해 밝고, 앞면은 화면 왼쪽을 향해 어둡다.
	float eaveBack = x - half - overhang;
	float eaveFront = x + half + overhang;
	float roofNear = y + half + overhang;
	float roofFar = y - half - overhang;

	Shapes::DrawTriangle(ctx, { x - half, y + half, wallTop }, { x + half, y + half, wallTop }, { x, y + half, ridgeZ },
		Shapes::Shade(wall, 0.92f));
	Shapes::DrawQuad(ctx, { eaveBack, roofFar, wallTop }, { eaveBack, roofNear, wallTop }, { x, roofNear, ridgeZ }, { x, roofFar, ridgeZ },
		roof);
	Shapes::DrawQuad(ctx, { eaveFront, roofFar, wallTop }, { eaveFront, roofNear, wallTop }, { x, roofNear, ridgeZ }, { x, roofFar, ridgeZ },
		Shapes::Shade(roof, 0.74f));

	// 기와 줄: 앞 지붕면에 용마루에서 처마로 흘러내리는 어두운 줄(기와 이음새) 몇 개.
	Shapes::Rgb tileSeam = Shapes::Shade(roof, 0.52f);
	float seamHalfWidth = size * 0.012f;
	const int kSeamCount = 5;
	for (int i = 1; i <= kSeamCount; ++i)
	{
		float seamY = roofFar + (roofNear - roofFar) * (float)i / (float)(kSeamCount + 1);
		Shapes::DrawQuad(ctx, { eaveFront, seamY - seamHalfWidth, wallTop }, { eaveFront, seamY + seamHalfWidth, wallTop },
			{ x, seamY + seamHalfWidth, ridgeZ }, { x, seamY - seamHalfWidth, ridgeZ }, tileSeam);
	}

	// 용마루: 지붕 꼭대기를 따라 놓인 가는 들보.
	Shapes::DrawSolidBox(ctx, x, y, ridgeZ - size * 0.02f, size * 0.07f, roofNear - roofFar, size * 0.04f, Shapes::Shade(roof, 0.85f));
}

// 발자국보다 넓은 그림자를 깔아서, 집이 바닥 위에 떠 보이지 않고 붙어 보이게 한다.
void BuildingActor::OnRenderShadow(const RenderContext& ctx)
{
	Mat4 model = Mat4::Translate(GetWorldX(), GetWorldY(), 0.001f) * Mat4::Scale(GetSize() * 1.7f, GetSize() * 1.4f, 1.f);
	ctx.renderer.DrawShadow(ctx.viewProjection * model, 0.5f);
}

// 눈에 보이는 발자국(한 변 size인 정사각형)과 반지름 moverRadius인 원의 겹침 판정.
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

ItemActor::ItemActor(float x, float y, float size, float r, float g, float b, ItemKind kind, int interactId)
	: Actor(ActorType::Item)
	, m_Kind(kind)
{
	SetPosition(x, y, 0.f);
	SetSize(size);
	SetColor(r, g, b);
	SetInteractId(interactId);

	// 위아래로 떠다니는 폭과 바닥에 번지는 빛(반지름 1.2*size)까지 감싼다.
	SetBoundingSphere(size * 1.3f, size * 0.5f);
}

void ItemActor::OnRender(const RenderContext& ctx)
{
	float x = GetWorldX();
	float y = GetWorldY();
	float z = GetWorldZ();
	float size = GetSize();
	float bob = sinf(ctx.time * 3.f + x * 4.f) * size * 0.12f;
	float pulse = 0.75f + 0.25f * sinf(ctx.time * 2.5f + y * 3.f);
	Shapes::Rgb color = { GetR(), GetG(), GetB() };

	// 바닥에 은은히 번지는 제 빛깔의 빛: 멀리서도 "여기 뭔가 있다"가 보이게 한다.
	Mat4 glowModel = Mat4::Translate(x, y, z + 0.003f) * Mat4::Scale(size * 2.4f, size * 2.4f, 1.f);
	ctx.renderer.DrawSoftDisc(ctx.viewProjection * glowModel, color.r, color.g, color.b, 0.28f * pulse);

	// 모두 카메라를 향해 세운 타원이라 어느 쪽에서 봐도 같은 모양이다.
	float baseZ = z + size * 0.25f + bob;
	if (m_Kind == ItemKind::Offering)
	{
		// 금빛 암포라: 불룩한 몸통(어두운 테두리 + 본체) → 좁은 목 → 넓은 주둥이 테.
		Shapes::DrawUprightEllipse(ctx, x, y, baseZ + size * 0.3f, size * 0.56f, size * 0.6f, Shapes::Shade(color, 0.8f));
		Shapes::DrawUprightEllipse(ctx, x, y, baseZ + size * 0.3f, size * 0.46f, size * 0.52f, color);
		Shapes::DrawUprightEllipse(ctx, x, y, baseZ + size * 0.66f, size * 0.18f, size * 0.24f, Shapes::Shade(color, 0.85f));
		Shapes::DrawUprightEllipse(ctx, x, y, baseZ + size * 0.8f, size * 0.32f, size * 0.09f, color);
	}
	else
	{
		// 약초: 화면 좌우로 벌어진 잎 두 장 → 가운데 큰 잎 → 밝은 새순.
		float side = size * 0.14f * kScreenAxis;
		Shapes::Rgb darkLeaf = Shapes::Shade(color, 0.75f);
		Shapes::DrawUprightEllipse(ctx, x + side, y - side, baseZ + size * 0.3f, size * 0.32f, size * 0.5f, darkLeaf);
		Shapes::DrawUprightEllipse(ctx, x - side, y + side, baseZ + size * 0.3f, size * 0.32f, size * 0.5f, darkLeaf);
		Shapes::DrawUprightEllipse(ctx, x, y, baseZ + size * 0.42f, size * 0.3f, size * 0.56f, color);
		Shapes::DrawUprightEllipse(ctx, x, y, baseZ + size * 0.5f, size * 0.12f, size * 0.2f, Shapes::Shade(color, 1.3f));
	}
}

// ---------------------------------------------------------------- FireActor

FireActor::FireActor(float localX, float localY, float localZ, float size)
	: Actor(ActorType::Fire)
{
	SetPosition(localX, localY, localZ);
	SetSize(size);

	// 불꽃(원점 주변)부터 바닥(localZ 아래)의 횃대·불빛 웅덩이(반지름 0.8*size)까지 감싸도록,
	// 중심을 불꽃과 바닥의 중간에 둔다.
	SetBoundingSphere(localZ * 0.5f + size * 1.7f, -localZ * 0.5f);
}

void FireActor::OnRender(const RenderContext& ctx)
{
	float x = GetWorldX();
	float y = GetWorldY();
	float z = GetWorldZ();
	float size = GetSize();
	float groundZ = z - GetZ(); // 부모(건물)가 서 있는 바닥 높이
	float flicker = 0.85f + 0.15f * sinf(ctx.time * 9.f + x * 1.3f);

	// 1) 바닥에 번지는 따뜻한 불빛.
	Mat4 poolModel = Mat4::Translate(x, y, groundZ + 0.003f) * Mat4::Scale(size * 3.2f, size * 3.2f, 1.f);
	ctx.renderer.DrawSoftDisc(ctx.viewProjection * poolModel, 1.f, 0.62f, 0.28f, 0.22f * flicker);

	// 2) 횃대: 바닥에서 불꽃 아래까지 세운 나무 기둥 + 위의 쇠 받침(예전엔 불꽃만 허공에 떠 있었다).
	float cupBase = z - size * 0.3f;
	Shapes::Rgb pole = { 0.30f, 0.20f, 0.12f };
	Shapes::Rgb iron = { 0.24f, 0.22f, 0.21f };
	Shapes::DrawSolidBox(ctx, x, y, groundZ, size * 0.12f, size * 0.12f, cupBase - groundZ, pole);
	Shapes::DrawSolidBox(ctx, x, y, cupBase, size * 0.34f, size * 0.34f, size * 0.12f, iron);

	// 3) 불꽃: 카메라를 향해 세운 세로로 긴 사각형에 불 셰이더(위로 갈수록 좁아지며 일렁인다).
	Mat4 flameModel = Mat4::Translate(x, y, z + size * 0.15f) * Mat4::RotateZ(-kQuarterPi) * Mat4::RotateX(kHalfPi)
		* Mat4::Scale(size * 0.75f, size * 1.1f, 1.f);
	float phase = x * 1.3f + y * 0.7f;
	ctx.renderer.DrawFire(ctx.viewProjection * flameModel, ctx.time, phase);
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

// -------------------------------------------------------- AmbientMotesActor

AmbientMotesActor::AmbientMotesActor(float halfWidth, float halfHeight, int count)
	: Actor(ActorType::Effect, RenderLayer::Overlay)
	, m_HalfWidth(halfWidth)
	, m_HalfHeight(halfHeight)
	, m_Count(count)
{
	// 경계를 지정하지 않는다(맵 전체에 흩어져 있어 항상 그린다).
}

void AmbientMotesActor::OnRender(const RenderContext& ctx)
{
	const float kMoteSize = 0.14f;
	float time = ctx.time;

	for (int i = 0; i < m_Count; ++i)
	{
		float fi = (float)i;
		float speed = IndexRandom(i, 3.f);

		// 은은하게 켜졌다 꺼지는 밝기. 거의 꺼진 알갱이는 그리지 않는다.
		float pulse = 0.5f + 0.5f * sinf(time * (1.1f + speed) + fi * 3.1f);
		float alpha = pulse * pulse * 0.85f;
		if (alpha < 0.03f)
		{
			continue;
		}

		// 고정 기준점 주변을 천천히 맴돌며 위아래로 살짝 오르내린다.
		float x = (IndexRandom(i, 1.f) * 2.f - 1.f) * m_HalfWidth + sinf(time * 0.21f + fi * 1.7f) * 0.8f;
		float y = (IndexRandom(i, 2.f) * 2.f - 1.f) * m_HalfHeight + cosf(time * 0.17f + fi * 2.3f) * 0.8f;
		float z = 0.35f + speed * 1.1f + sinf(time * 0.8f + fi) * 0.15f;

		// 카메라를 향해 세운 부드러운 원. 밝기가 1을 넘어서 블룸으로 빛난다.
		Mat4 model = Mat4::Translate(x, y, z) * Mat4::RotateZ(-kQuarterPi) * Mat4::RotateX(kHalfPi)
			* Mat4::Scale(kMoteSize, kMoteSize, 1.f);
		ctx.renderer.DrawSoftDisc(ctx.viewProjection * model, 1.8f, 1.5f, 0.7f, alpha);
	}
}
