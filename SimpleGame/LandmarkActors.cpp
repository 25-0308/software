#include "stdafx.h"
#include "LandmarkActors.h"

#include <algorithm>
#include <cmath>

#include "LevelGenerator.h"
#include "Renderer.h"
#include "Shapes.h"

namespace
{
	const float kPi = 3.14159265f;

	// 화면 가로 방향(월드 (1,-1)/√2). 카메라를 향해 세운 도형을 좌우로 펼칠 때 쓴다.
	const float kScreenAxisX = 0.7071f;
	const float kScreenAxisY = -0.7071f;

	const Shapes::Rgb kMarble = { 0.86f, 0.84f, 0.79f };
	const Shapes::Rgb kStone = { 0.68f, 0.65f, 0.59f };
	const Shapes::Rgb kRock = { 0.50f, 0.47f, 0.43f };
	const Shapes::Rgb kWood = { 0.40f, 0.27f, 0.15f };
	const Shapes::Rgb kBronze = { 0.78f, 0.55f, 0.26f };
	const Shapes::Rgb kSoot = { 0.06f, 0.05f, 0.06f };

	// 한 액터 안에서 카메라에서 먼 것부터 그려야 하는 부품(상자 또는 기둥).
	struct Part
	{
		enum Kind
		{
			Box,       // 직육면체(윗면 색 따로)
			Column,    // 받침 + 원기둥 + 머리
			DoorBox,   // +y면에 어두운 문이 난 직육면체(신전 내실)
		};

		Kind kind;
		float x, y, baseZ;
		float width, depth, height;
		Shapes::Rgb color;
		Shapes::Rgb top;
		float distance;
	};

	Part MakeBox(float x, float y, float baseZ, float width, float depth, float height, const Shapes::Rgb& color)
	{
		Part part = { Part::Box, x, y, baseZ, width, depth, height, color, Shapes::Shade(color, 1.08f), 0.f };
		return part;
	}

	// 카메라에서 먼 정도(SceneGraph의 정렬과 같은 기준: view-projection의 셋째 행으로 구한 클립 z).
	float DistanceFromCamera(const RenderContext& ctx, float x, float y, float z)
	{
		const float* m = ctx.viewProjection.m;
		return m[2] * x + m[6] * y + m[10] * z + m[14];
	}

	void DrawColumn(const RenderContext& ctx, float x, float y, float baseZ, float height, const Shapes::Rgb& color)
	{
		const float kCapHeight = 0.09f;
		Shapes::DrawSolidBox(ctx, x, y, baseZ, 0.46f, 0.46f, kCapHeight, color);
		Shapes::DrawCylinder(ctx, x, y, baseZ + kCapHeight, 0.17f, height - kCapHeight * 2.f,
			Shapes::Shade(color, 0.92f), color);
		Shapes::DrawSolidBox(ctx, x, y, baseZ + height - kCapHeight, 0.46f, 0.46f, kCapHeight, Shapes::Shade(color, 1.04f));
	}

	void DrawPartsSorted(const RenderContext& ctx, std::vector<Part>& parts)
	{
		for (Part& part : parts)
		{
			part.distance = DistanceFromCamera(ctx, part.x, part.y, part.baseZ + part.height * 0.5f);
		}

		std::stable_sort(parts.begin(), parts.end(), [](const Part& lhs, const Part& rhs)
		{
			return lhs.distance > rhs.distance;
		});

		for (const Part& part : parts)
		{
			switch (part.kind)
			{
			case Part::Box:
				Shapes::DrawBox(ctx, part.x, part.y, part.baseZ, part.width, part.depth, part.height,
					part.top, Shapes::Shade(part.color, 0.82f), Shapes::Shade(part.color, 0.62f));
				break;

			case Part::Column:
				DrawColumn(ctx, part.x, part.y, part.baseZ, part.height, part.color);
				break;

			case Part::DoorBox:
				Shapes::DrawBox(ctx, part.x, part.y, part.baseZ, part.width, part.depth, part.height,
					part.top, Shapes::Shade(part.color, 0.82f), Shapes::Shade(part.color, 0.62f));
				Shapes::DrawPanelOnFaceY(ctx, part.x, part.y + part.depth * 0.5f + 0.004f, part.baseZ + part.height * 0.38f,
					part.width * 0.24f, part.height * 0.76f, kSoot);
				break;
			}
		}
	}

	// 반지름 radius인 원이 축 정렬 사각형 [minX,maxX]x[minY,maxY]와 겹치는지.
	bool CircleHitsRect(float x, float y, float radius, float minX, float maxX, float minY, float maxY)
	{
		float nearestX = (x < minX) ? minX : ((x > maxX) ? maxX : x);
		float nearestY = (y < minY) ? minY : ((y > maxY) ? maxY : y);
		float dx = x - nearestX;
		float dy = y - nearestY;
		return dx * dx + dy * dy < radius * radius;
	}

	// 카메라를 향해 세운 평면(가로 = 화면 가로축, 세로 = 위) 위의 점.
	Shapes::Point3 Billboard(float centerX, float centerY, float centerZ, float right, float up)
	{
		Shapes::Point3 point = { centerX + kScreenAxisX * right, centerY + kScreenAxisY * right, centerZ + up };
		return point;
	}

	// 길잡이 화살표(GuideArrowActor).
	const float kGuideFirstDart = 1.0f;   // 플레이어 중심에서 첫 화살촉 중심까지(발밑 링 바깥)
	const float kGuideDartSpacing = 0.42f;
	const float kGuideHideDistance = 2.2f; // 목표가 이보다 가까우면 숨긴다
	const float kGuideFadeDistance = 1.2f; // 숨는 거리부터 이만큼 더 멀어질 때까지 서서히 진해진다
	const float kGuideGroundZ = 0.02f;

	// 바닥에 누운 점: (x, y)에서 앞(dirX, dirY) 방향으로 forward, 왼쪽 옆으로 side만큼 간 곳.
	Shapes::Point3 GroundPoint(float x, float y, float dirX, float dirY, float forward, float side)
	{
		Shapes::Point3 point = { x + dirX * forward - dirY * side, y + dirY * forward + dirX * side, kGuideGroundZ };
		return point;
	}

	// 바닥에 누운 화살촉(뒤가 오목한 촉) 하나. 중심이 (x, y)에서 앞으로 forward만큼 간 곳이고, scale로 크기를 키운다.
	void DrawGroundDart(const RenderContext& ctx, float x, float y, float dirX, float dirY, float forward, float scale,
		const Shapes::Rgb& color, float alpha)
	{
		Shapes::Point3 tip = GroundPoint(x, y, dirX, dirY, forward + 0.2f * scale, 0.f);
		Shapes::Point3 left = GroundPoint(x, y, dirX, dirY, forward - 0.16f * scale, 0.27f * scale);
		Shapes::Point3 notch = GroundPoint(x, y, dirX, dirY, forward - 0.05f * scale, 0.f);
		Shapes::Point3 right = GroundPoint(x, y, dirX, dirY, forward - 0.16f * scale, -0.27f * scale);

		Shapes::DrawTriangle(ctx, tip, left, notch, color, alpha);
		Shapes::DrawTriangle(ctx, tip, notch, right, color, alpha);
	}
}

// ------------------------------------------------------------- TempleActor

TempleActor::TempleActor(float x, float y, float width, float depth, float columnHeight, bool roman)
	: Actor(ActorType::Building)
	, m_Width(width)
	, m_Depth(depth)
	, m_ColumnHeight(columnHeight)
	, m_Roman(roman)
{
	SetPosition(x, y, 0.f);
	SetSize((width > depth) ? width : depth);

	// 기단(로마식은 앞 계단까지)·지붕 꼭대기까지 감싼다.
	float halfDiagonal = sqrtf(width * width + depth * depth) * 0.5f + 1.2f;
	float totalHeight = columnHeight + 0.7f + 0.24f + width * 0.25f;
	SetBoundingSphere(sqrtf(halfDiagonal * halfDiagonal + totalHeight * totalHeight * 0.25f), totalHeight * 0.5f);
}

void TempleActor::OnRender(const RenderContext& ctx)
{
	float x = GetWorldX();
	float y = GetWorldY();
	float z = GetWorldZ();
	float halfWidth = m_Width * 0.5f;
	float halfDepth = m_Depth * 0.5f;

	Shapes::Rgb roof = m_Roman ? Shapes::Rgb{ 0.62f, 0.22f, 0.15f } : Shapes::Rgb{ 0.66f, 0.38f, 0.25f };

	// 1) 기단. 그리스식은 낮은 3단, 로마식은 높은 단 + 정면 계단.
	float floorZ = z;
	if (m_Roman)
	{
		const float kPodium = 0.7f;
		Shapes::DrawSolidBox(ctx, x, y, z, m_Width + 0.3f, m_Depth + 0.3f, kPodium, kStone);

		for (int i = 0; i < 3; ++i)
		{
			float stepHeight = kPodium * (float)(3 - i) / 4.f;
			float stepY = y + halfDepth + 0.15f + 0.22f * ((float)i + 0.5f);
			Shapes::DrawSolidBox(ctx, x, stepY, z, m_Width * 0.5f, 0.22f, stepHeight, Shapes::Shade(kStone, 1.04f));
		}
		floorZ = z + kPodium;
	}
	else
	{
		const float kStep = 0.12f;
		for (int i = 0; i < 3; ++i)
		{
			float grow = 0.8f - 0.25f * (float)i;
			Shapes::DrawSolidBox(ctx, x, y, z + kStep * (float)i, m_Width + grow, m_Depth + grow, kStep,
				Shapes::Shade(kStone, 1.f + 0.06f * (float)i));
		}
		floorZ = z + kStep * 3.f;
	}

	// 2) 기둥들과 내실을 카메라에서 먼 것부터(뒷줄 기둥 → 내실 → 앞줄 기둥).
	std::vector<Part> parts;
	const float kInset = 0.22f;
	float frontY = y + halfDepth - kInset;
	float backY = y - halfDepth + kInset;
	float leftX = x - halfWidth + kInset;
	float rightX = x + halfWidth - kInset;
	int columnsAlongX = (int)(m_Width / 0.85f + 0.5f);
	int columnsAlongY = (int)(m_Depth / 0.85f + 0.5f);
	if (columnsAlongX < 4) columnsAlongX = 4;
	if (columnsAlongY < 3) columnsAlongY = 3;

	for (int i = 0; i < columnsAlongX; ++i)
	{
		float t = (float)i / (float)(columnsAlongX - 1);
		float columnX = leftX + (rightX - leftX) * t;

		Part front = { Part::Column, columnX, frontY, floorZ, 0.46f, 0.46f, m_ColumnHeight, kMarble, kMarble, 0.f };
		parts.push_back(front);

		if (!m_Roman)
		{
			Part back = front;
			back.y = backY;
			parts.push_back(back);
		}
	}

	if (!m_Roman)
	{
		for (int j = 1; j < columnsAlongY - 1; ++j)
		{
			float t = (float)j / (float)(columnsAlongY - 1);
			float columnY = backY + (frontY - backY) * t;

			Part left = { Part::Column, leftX, columnY, floorZ, 0.46f, 0.46f, m_ColumnHeight, kMarble, kMarble, 0.f };
			parts.push_back(left);

			Part right = left;
			right.x = rightX;
			parts.push_back(right);
		}
	}

	// 내실: 그리스식은 기둥 안쪽에 작게, 로마식은 앞줄 기둥 뒤를 단 전체로 채운다.
	Part cella = { Part::DoorBox, x, y, floorZ, m_Width - 1.1f, m_Depth - 1.3f, m_ColumnHeight * 0.96f,
		Shapes::Shade(kMarble, 0.93f), Shapes::Shade(kMarble, 0.9f), 0.f };
	if (m_Roman)
	{
		cella.width = m_Width - 0.2f;
		cella.depth = m_Depth - 0.9f;
		cella.y = y - 0.35f;
	}
	parts.push_back(cella);

	DrawPartsSorted(ctx, parts);

	// 3) 들보(기둥 위를 두른 띠). 로마식은 정면에 금빛 비문 띠를 새긴다.
	float beamZ = floorZ + m_ColumnHeight;
	const float kBeamHeight = 0.24f;
	Shapes::DrawSolidBox(ctx, x, y, beamZ, m_Width + 0.15f, m_Depth + 0.15f, kBeamHeight, kMarble);
	if (m_Roman)
	{
		Shapes::Rgb inscription = { 0.9f, 0.72f, 0.3f };
		Shapes::DrawPanelOnFaceY(ctx, x, y + (m_Depth + 0.15f) * 0.5f + 0.004f, beamZ + kBeamHeight * 0.5f,
			m_Width * 0.55f, kBeamHeight * 0.5f, inscription);
	}

	// 4) 박공지붕: 정면(+y) 삼각형 박공 + 안쪽 그늘진 삼각형 → 뒤(-x) 지붕면 → 앞(+x) 지붕면 → 용마루.
	float roofBase = beamZ + kBeamHeight;
	float roofHeight = (m_Width + 0.15f) * 0.18f;
	float ridgeZ = roofBase + roofHeight;
	float gableY = y + halfDepth + 0.085f;
	float eaveBack = x - halfWidth - 0.14f;
	float eaveFront = x + halfWidth + 0.14f;
	float roofNear = y + halfDepth + 0.14f;
	float roofFar = y - halfDepth - 0.14f;

	Shapes::DrawTriangle(ctx, { x - halfWidth - 0.07f, gableY, roofBase }, { x + halfWidth + 0.07f, gableY, roofBase },
		{ x, gableY, ridgeZ }, Shapes::Shade(kMarble, 0.94f));
	Shapes::DrawTriangle(ctx, { x - halfWidth * 0.78f, gableY + 0.003f, roofBase + 0.05f }, { x + halfWidth * 0.78f, gableY + 0.003f, roofBase + 0.05f },
		{ x, gableY + 0.003f, ridgeZ - 0.09f }, Shapes::Shade(kMarble, 0.68f));
	Shapes::DrawQuad(ctx, { eaveBack, roofFar, roofBase }, { eaveBack, roofNear, roofBase }, { x, roofNear, ridgeZ },
		{ x, roofFar, ridgeZ }, roof);
	Shapes::DrawQuad(ctx, { eaveFront, roofFar, roofBase }, { eaveFront, roofNear, roofBase }, { x, roofNear, ridgeZ },
		{ x, roofFar, ridgeZ }, Shapes::Shade(roof, 0.74f));
	Shapes::DrawSolidBox(ctx, x, y, ridgeZ - 0.03f, 0.1f, roofNear - roofFar, 0.06f, Shapes::Shade(roof, 0.85f));
}

void TempleActor::OnRenderShadow(const RenderContext& ctx)
{
	Mat4 model = Mat4::Translate(GetWorldX(), GetWorldY(), 0.001f) * Mat4::Scale(m_Width + 1.6f, m_Depth + 1.6f, 1.f);
	ctx.renderer.DrawShadow(ctx.viewProjection * model, 0.45f);
}

bool TempleActor::BlocksCircle(float x, float y, float moverRadius) const
{
	float halfWidth = m_Width * 0.5f + 0.4f;
	float halfDepth = m_Depth * 0.5f + 0.4f;
	float frontExtra = m_Roman ? 0.7f : 0.f; // 로마식 정면 계단

	return CircleHitsRect(x, y, moverRadius, GetWorldX() - halfWidth, GetWorldX() + halfWidth,
		GetWorldY() - halfDepth, GetWorldY() + halfDepth + frontExtra);
}

// --------------------------------------------------------------- RockActor

RockActor::RockActor(float x, float y, float size, int variant, bool snowy)
	: Actor(ActorType::Rock)
	, m_Variant(variant)
	, m_Snowy(snowy)
{
	SetPosition(x, y, 0.f);
	SetSize(size);
	SetBoundingSphere(size * 1.0f, size * 0.4f);
}

void RockActor::OnRender(const RenderContext& ctx)
{
	float x = GetWorldX();
	float y = GetWorldY();
	float z = GetWorldZ();
	float s = GetSize();

	// 변형 번호마다 고정된 난수로 덩어리 크기·밝기를 조금씩 다르게 한다(같은 바위는 늘 같은 모양).
	float a = Terrain::Hash(m_Variant, 1, 71);
	float b = Terrain::Hash(m_Variant, 2, 71);
	float c = Terrain::Hash(m_Variant, 3, 71);
	float d = Terrain::Hash(m_Variant, 4, 71);

	Shapes::Rgb color = Shapes::Shade(kRock, 0.86f + 0.24f * d);
	Shapes::Rgb snow = { 0.92f, 0.94f, 0.97f };

	std::vector<Part> parts;
	parts.push_back(MakeBox(x, y, z, s * (0.75f + 0.2f * a), s * (0.7f + 0.2f * b), s * (0.55f + 0.4f * c), color));
	parts.push_back(MakeBox(x - s * 0.36f, y + s * 0.22f, z, s * 0.5f, s * 0.45f, s * (0.3f + 0.25f * b), Shapes::Shade(color, 1.05f)));
	parts.push_back(MakeBox(x + s * 0.26f, y - s * 0.3f, z, s * 0.45f, s * 0.5f, s * (0.25f + 0.25f * a), Shapes::Shade(color, 0.95f)));

	if (m_Snowy)
	{
		for (Part& part : parts)
		{
			part.top = snow;
		}
	}

	DrawPartsSorted(ctx, parts);
}

// ------------------------------------------------------- CaveEntranceActor

CaveEntranceActor::CaveEntranceActor(float x, float y, float size)
	: Actor(ActorType::Rock)
{
	SetPosition(x, y, 0.f);
	SetSize(size);
	SetBoundingSphere(size * 1.5f, size * 0.5f);
}

void CaveEntranceActor::OnRender(const RenderContext& ctx)
{
	float x = GetWorldX();
	float y = GetWorldY();
	float z = GetWorldZ();
	float s = GetSize();

	// 바위 덩어리들(뒤의 큰 덩어리, 양옆, 입구가 난 앞 덩어리).
	std::vector<Part> parts;
	parts.push_back(MakeBox(x - s * 0.3f, y - s * 0.3f, z, s * 1.3f, s * 1.0f, s * 1.2f, Shapes::Shade(kRock, 0.92f)));
	parts.push_back(MakeBox(x + s * 0.6f, y - s * 0.05f, z, s * 0.6f, s * 1.0f, s * 0.85f, kRock));
	parts.push_back(MakeBox(x - s * 0.62f, y + s * 0.1f, z, s * 0.55f, s * 0.9f, s * 0.9f, Shapes::Shade(kRock, 1.05f)));
	parts.push_back(MakeBox(x, y + s * 0.1f, z, s * 1.0f, s * 0.8f, s * 0.95f, kRock));
	DrawPartsSorted(ctx, parts);

	// 입구: 앞 덩어리의 +y면에 어두운 구멍 — 아래 사각형 + 위 반원 아치(면에 붙은 삼각형 부채꼴).
	float faceY = y + s * 0.1f + s * 0.4f + 0.004f;
	float halfOpening = s * 0.27f;
	float archBaseZ = z + s * 0.55f;
	Shapes::Rgb darkness = { 0.03f, 0.02f, 0.04f };

	Shapes::DrawPanelOnFaceY(ctx, x, faceY, z + s * 0.275f, halfOpening * 2.f, s * 0.55f, darkness);

	const int kArcSegments = 8;
	for (int i = 0; i < kArcSegments; ++i)
	{
		float t0 = kPi * (float)i / (float)kArcSegments;
		float t1 = kPi * (float)(i + 1) / (float)kArcSegments;
		Shapes::DrawTriangle(ctx, { x, faceY, archBaseZ },
			{ x + cosf(t0) * halfOpening, faceY, archBaseZ + sinf(t0) * halfOpening * 0.9f },
			{ x + cosf(t1) * halfOpening, faceY, archBaseZ + sinf(t1) * halfOpening * 0.9f }, darkness);
	}
}

bool CaveEntranceActor::BlocksCircle(float x, float y, float moverRadius) const
{
	float s = GetSize();
	return CircleHitsRect(x, y, moverRadius, GetWorldX() - s * 0.95f, GetWorldX() + s * 0.9f,
		GetWorldY() - s * 0.8f, GetWorldY() + s * 0.5f);
}

// ------------------------------------------------------------- ColumnActor

ColumnActor::ColumnActor(float x, float y, float height, bool broken)
	: Actor(ActorType::Building)
	, m_Height(height)
	, m_Broken(broken)
{
	SetPosition(x, y, 0.f);
	SetSize(0.5f);
	SetBoundingSphere(height * 0.55f + 0.4f, height * 0.5f);
}

void ColumnActor::OnRender(const RenderContext& ctx)
{
	float x = GetWorldX();
	float y = GetWorldY();
	float z = GetWorldZ();

	if (!m_Broken)
	{
		Shapes::DrawSolidBox(ctx, x, y, z, 0.56f, 0.56f, 0.1f, Shapes::Shade(kMarble, 0.95f));
		DrawColumn(ctx, x, y, z + 0.1f, m_Height - 0.1f, kMarble);
		return;
	}

	// 부러진 기둥: 짧은 기둥 + 위에 남은 들쭉날쭉한 조각 + 곁에 굴러떨어진 기둥 토막.
	float stump = m_Height * 0.42f;
	Shapes::DrawSolidBox(ctx, x, y, z, 0.56f, 0.56f, 0.1f, Shapes::Shade(kMarble, 0.95f));
	Shapes::DrawCylinder(ctx, x, y, z + 0.1f, 0.17f, stump, Shapes::Shade(kMarble, 0.9f), kMarble);
	Shapes::DrawSolidBox(ctx, x - 0.05f, y + 0.04f, z + 0.1f + stump, 0.14f, 0.12f, 0.12f, Shapes::Shade(kMarble, 0.96f));
	Shapes::DrawSolidBox(ctx, x + 0.42f, y + 0.3f, z, 0.5f, 0.3f, 0.3f, Shapes::Shade(kMarble, 0.9f));
}

// ---------------------------------------------------------- FloorDiscActor

FloorDiscActor::FloorDiscActor(float x, float y, float radius, float r, float g, float b)
	: Actor(ActorType::Marker, RenderLayer::Decal)
{
	SetPosition(x, y, 0.f);
	SetSize(radius);
	SetColor(r, g, b);
	SetBoundingSphere(radius + 0.2f);
}

void FloorDiscActor::OnRender(const RenderContext& ctx)
{
	float x = GetWorldX();
	float y = GetWorldY();
	float diameter = GetSize() * 2.f;
	Shapes::Rgb color = { GetR(), GetG(), GetB() };

	// 바깥 테두리 띠 → 바닥 → 안쪽 동심원 띠 → 가운데 문양.
	Shapes::DrawFlatDisc(ctx, x, y, 0.003f, diameter, diameter, Shapes::Shade(color, 0.72f));
	Shapes::DrawFlatDisc(ctx, x, y, 0.003f, diameter * 0.93f, diameter * 0.93f, color);
	Shapes::DrawFlatDisc(ctx, x, y, 0.003f, diameter * 0.55f, diameter * 0.55f, Shapes::Shade(color, 0.85f));
	Shapes::DrawFlatDisc(ctx, x, y, 0.003f, diameter * 0.5f, diameter * 0.5f, Shapes::Shade(color, 1.04f));
	Shapes::DrawFlatDisc(ctx, x, y, 0.003f, diameter * 0.14f, diameter * 0.14f, Shapes::Rgb{ 0.82f, 0.66f, 0.34f });
}

// -------------------------------------------------------------- TableActor

TableActor::TableActor(float x, float y, float width, float depth, float height, float r, float g, float b)
	: Actor(ActorType::Building)
	, m_Width(width)
	, m_Depth(depth)
	, m_Height(height)
{
	SetPosition(x, y, 0.f);
	SetSize((width > depth) ? width : depth);
	SetColor(r, g, b);
	SetBoundingSphere(sqrtf(width * width + depth * depth) * 0.5f + 0.5f, height * 0.5f);
}

void TableActor::OnRender(const RenderContext& ctx)
{
	float x = GetWorldX();
	float y = GetWorldY();
	float z = GetWorldZ();
	Shapes::Rgb color = { GetR(), GetG(), GetB() };
	const float kSlab = 0.12f;

	// 가운데 받침 → 상판.
	Shapes::DrawSolidBox(ctx, x, y, z, m_Width * 0.55f, m_Depth * 0.5f, m_Height - kSlab, Shapes::Shade(color, 0.9f));
	Shapes::DrawSolidBox(ctx, x, y, z + m_Height - kSlab, m_Width, m_Depth, kSlab, color);
}

bool TableActor::BlocksCircle(float x, float y, float moverRadius) const
{
	return CircleHitsRect(x, y, moverRadius, GetWorldX() - m_Width * 0.5f, GetWorldX() + m_Width * 0.5f,
		GetWorldY() - m_Depth * 0.5f, GetWorldY() + m_Depth * 0.5f);
}

// -------------------------------------------------------------- AltarActor

AltarActor::AltarActor(float x, float y)
	: Actor(ActorType::Building)
{
	SetPosition(x, y, 0.f);
	SetSize(1.f);
	SetBoundingSphere(1.4f, 0.5f);
}

void AltarActor::OnRender(const RenderContext& ctx)
{
	float x = GetWorldX();
	float y = GetWorldY();
	float z = GetWorldZ();

	// 삼발이(무녀가 앉아 신탁을 받는 청동 의자): 제단보다 카메라에서 멀어서 먼저 그린다.
	float tripodX = x - 0.8f;
	float tripodY = y - 0.15f;
	for (int i = 0; i < 3; ++i)
	{
		float angle = (float)i * (2.f * kPi / 3.f) + 0.4f;
		Shapes::DrawSolidBox(ctx, tripodX + cosf(angle) * 0.17f, tripodY + sinf(angle) * 0.17f, z, 0.05f, 0.05f, 0.62f,
			Shapes::Shade(kBronze, 0.85f));
	}
	Shapes::DrawCylinder(ctx, tripodX, tripodY, z + 0.58f, 0.24f, 0.08f, kBronze, Shapes::Shade(kBronze, 1.15f));

	// 돌 제단 + 청동 그릇 + 그릇 속 불씨(블룸으로 은은히).
	Shapes::DrawSolidBox(ctx, x, y, z, 0.95f, 0.8f, 0.55f, kStone);
	Shapes::DrawSolidBox(ctx, x, y, z + 0.55f, 1.05f, 0.9f, 0.08f, Shapes::Shade(kStone, 1.06f));
	Shapes::DrawCylinder(ctx, x, y, z + 0.63f, 0.28f, 0.12f, kBronze, Shapes::Shade(kBronze, 0.6f));

	float flicker = 0.75f + 0.25f * sinf(ctx.time * 5.f + x);
	Mat4 ember = Mat4::Translate(x, y, z + 0.77f) * Mat4::Scale(0.5f, 0.5f, 1.f);
	ctx.renderer.DrawSoftDisc(ctx.viewProjection * ember, 1.6f, 0.7f, 0.25f, 0.8f * flicker);
}

// -------------------------------------------------------------- MuralActor

MuralActor::MuralActor(float x, float y)
	: Actor(ActorType::Building)
{
	SetPosition(x, y, 0.f);
	SetSize(1.9f);
	SetBoundingSphere(1.5f, 0.8f);
}

void MuralActor::OnRender(const RenderContext& ctx)
{
	float x = GetWorldX();
	float y = GetWorldY();
	float z = GetWorldZ();

	// 돌판(낮은 받침 위에 세움).
	Shapes::DrawSolidBox(ctx, x, y, z, 2.1f, 0.5f, 0.12f, Shapes::Shade(kStone, 0.9f));
	Shapes::DrawSolidBox(ctx, x, y, z + 0.12f, 1.9f, 0.3f, 1.45f, kStone);

	// 앞면(+y)의 빛바랜 그림: 황토색 바탕 → 크로노스의 큰 검은 실루엣(몸·머리·관) → 강보에 싼 돌 →
	// 그 발치에 늘어선 아이들의 작은 그림자 다섯.
	float face = y + 0.15f + 0.004f;
	Shapes::Rgb ground = { 0.62f, 0.48f, 0.32f };
	Shapes::Rgb ink = { 0.16f, 0.1f, 0.08f };
	Shapes::Rgb gold = { 0.78f, 0.6f, 0.3f };
	Shapes::Rgb cloth = { 0.88f, 0.84f, 0.74f };

	Shapes::DrawPanelOnFaceY(ctx, x, face, z + 0.85f, 1.66f, 1.2f, Shapes::Shade(ground, 0.7f));
	Shapes::DrawPanelOnFaceY(ctx, x, face + 0.001f, z + 0.85f, 1.56f, 1.1f, ground);

	Shapes::DrawPanelOnFaceY(ctx, x - 0.3f, face + 0.002f, z + 0.72f, 0.46f, 0.78f, ink);
	Shapes::DrawPanelOnFaceY(ctx, x - 0.3f, face + 0.002f, z + 1.24f, 0.28f, 0.26f, ink);
	Shapes::DrawPanelOnFaceY(ctx, x - 0.39f, face + 0.003f, z + 1.43f, 0.06f, 0.1f, gold);
	Shapes::DrawPanelOnFaceY(ctx, x - 0.3f, face + 0.003f, z + 1.45f, 0.06f, 0.13f, gold);
	Shapes::DrawPanelOnFaceY(ctx, x - 0.21f, face + 0.003f, z + 1.43f, 0.06f, 0.1f, gold);
	Shapes::DrawPanelOnFaceY(ctx, x - 0.07f, face + 0.002f, z + 1.02f, 0.26f, 0.08f, ink);
	Shapes::DrawPanelOnFaceY(ctx, x - 0.06f, face + 0.003f, z + 1.17f, 0.13f, 0.17f, cloth);

	for (int i = 0; i < 5; ++i)
	{
		float height = 0.22f - 0.02f * (float)i;
		Shapes::DrawPanelOnFaceY(ctx, x + 0.18f + 0.13f * (float)i, face + 0.002f, z + 0.38f + height * 0.5f, 0.07f, height,
			Shapes::Shade(ink, 1.6f));
	}
}

bool MuralActor::BlocksCircle(float x, float y, float moverRadius) const
{
	return CircleHitsRect(x, y, moverRadius, GetWorldX() - 1.05f, GetWorldX() + 1.05f, GetWorldY() - 0.25f, GetWorldY() + 0.25f);
}

// ------------------------------------------------------------- StatueActor

StatueActor::StatueActor(float x, float y, float size, bool fallen)
	: Actor(ActorType::Building)
	, m_Fallen(fallen)
{
	SetPosition(x, y, 0.f);
	SetSize(size);
	SetBoundingSphere(size * 1.4f + 0.6f, size * 0.7f);
}

void StatueActor::OnRender(const RenderContext& ctx)
{
	float x = GetWorldX();
	float y = GetWorldY();
	float z = GetWorldZ();
	float s = GetSize();
	Shapes::Rgb bolt = { 1.1f, 0.88f, 0.35f };

	if (!m_Fallen)
	{
		// 받침대 → 긴 옷을 입은 몸 → 번개를 치켜든 오른팔 → 머리·수염.
		float top = z + 0.75f;
		Shapes::DrawSolidBox(ctx, x, y, z, 1.0f, 1.0f, 0.75f, kStone);
		Shapes::DrawSolidBox(ctx, x, y, top, s * 0.5f, s * 0.36f, s * 0.82f, kMarble);
		Shapes::DrawSolidBox(ctx, x + s * 0.3f, y, top + s * 0.55f, s * 0.12f, s * 0.12f, s * 0.48f, kMarble);
		Shapes::DrawSolidBox(ctx, x + s * 0.3f, y, top + s * 1.03f, s * 0.06f, s * 0.06f, s * 0.22f, bolt);
		Shapes::DrawCylinder(ctx, x, y, top + s * 0.82f, s * 0.17f, s * 0.24f, Shapes::Shade(kMarble, 0.95f), kMarble);
		Shapes::DrawSolidBox(ctx, x, y + s * 0.1f, top + s * 0.74f, s * 0.2f, s * 0.12f, s * 0.16f, Shapes::Shade(kMarble, 0.97f));
		return;
	}

	// 무너진 신상: 윗부분이 깨진 받침대 → 바닥에 누운 몸 → 떨어져 나뒹구는 머리 → 부스러기.
	Shapes::DrawSolidBox(ctx, x, y, z, 1.0f, 1.0f, 0.62f, kStone);
	Shapes::DrawSolidBox(ctx, x - 0.18f, y - 0.12f, z + 0.62f, 0.4f, 0.45f, 0.14f, Shapes::Shade(kStone, 0.95f));
	Shapes::DrawSolidBox(ctx, x + 0.2f, y + 0.15f, z + 0.62f, 0.25f, 0.3f, 0.07f, Shapes::Shade(kStone, 1.05f));

	Shapes::DrawSolidBox(ctx, x + 1.05f, y + 0.25f, z, s * 1.05f, s * 0.4f, s * 0.34f, Shapes::Shade(kMarble, 0.94f));
	Shapes::DrawSolidBox(ctx, x + 1.25f, y - 0.1f, z, s * 0.42f, s * 0.12f, s * 0.12f, kMarble);
	Shapes::DrawSolidBox(ctx, x + 1.52f, y - 0.1f, z + s * 0.04f, s * 0.2f, s * 0.05f, s * 0.05f, Shapes::Shade(bolt, 0.7f));
	Shapes::DrawUprightEllipse(ctx, x + 1.75f, y + 0.95f, z + s * 0.17f, s * 0.36f, s * 0.34f, kMarble);
	Shapes::DrawUprightEllipse(ctx, x + 1.68f, y + 0.95f, z + s * 0.1f, s * 0.22f, s * 0.14f, Shapes::Shade(kMarble, 0.8f));

	Shapes::DrawSolidBox(ctx, x + 0.55f, y + 0.85f, z, 0.18f, 0.14f, 0.1f, Shapes::Shade(kMarble, 0.9f));
	Shapes::DrawSolidBox(ctx, x + 0.75f, y + 1.15f, z, 0.1f, 0.1f, 0.07f, kMarble);
}

bool StatueActor::BlocksCircle(float x, float y, float moverRadius) const
{
	float baseX = GetWorldX();
	float baseY = GetWorldY();

	if (CircleHitsRect(x, y, moverRadius, baseX - 0.5f, baseX + 0.5f, baseY - 0.5f, baseY + 0.5f))
	{
		return true;
	}

	// 무너진 신상은 바닥에 누운 몸도 막는다.
	return m_Fallen && CircleHitsRect(x, y, moverRadius, baseX + 0.45f, baseX + 1.65f, baseY + 0.05f, baseY + 0.45f);
}

// ------------------------------------------------------------- HearthActor

HearthActor::HearthActor(float x, float y)
	: Actor(ActorType::Building)
{
	SetPosition(x, y, 0.f);
	SetSize(1.f);
	SetBoundingSphere(0.9f, 0.3f);
}

void HearthActor::OnRender(const RenderContext& ctx)
{
	float x = GetWorldX();
	float y = GetWorldY();
	float z = GetWorldZ();
	Shapes::Rgb ash = { 0.3f, 0.29f, 0.28f };

	// 뒷벽(그을음 자국) → 왼쪽 벽(-x, 카메라에서 먼 쪽) → 재 더미 → 오른쪽 벽(+x).
	Shapes::DrawSolidBox(ctx, x, y - 0.32f, z, 1.0f, 0.24f, 0.5f, kStone);
	Shapes::DrawPanelOnFaceY(ctx, x, y - 0.2f + 0.004f, z + 0.3f, 0.5f, 0.32f, kSoot, 0.7f);
	Shapes::DrawSolidBox(ctx, x - 0.4f, y, z, 0.22f, 0.62f, 0.42f, Shapes::Shade(kStone, 0.97f));
	Shapes::DrawFlatDisc(ctx, x, y + 0.02f, z + 0.01f, 0.55f, 0.42f, ash);
	Shapes::DrawFlatDisc(ctx, x + 0.05f, y, z + 0.012f, 0.3f, 0.22f, Shapes::Shade(ash, 0.7f));
	Shapes::DrawSolidBox(ctx, x + 0.4f, y, z, 0.22f, 0.62f, 0.42f, Shapes::Shade(kStone, 1.02f));
}

// --------------------------------------------------------------- GlowActor

GlowActor::GlowActor(float x, float y, float z, float size, float r, float g, float b)
	: Actor(ActorType::Effect)
{
	SetPosition(x, y, z);
	SetSize(size);
	SetColor(r, g, b);
	SetBoundingSphere(size);
}

void GlowActor::OnRender(const RenderContext& ctx)
{
	float pulse = 0.65f + 0.35f * sinf(ctx.time * 2.3f + GetWorldX() * 1.7f);
	Mat4 model = Mat4::Translate(GetWorldX(), GetWorldY(), GetWorldZ()) * Mat4::RotateZ(-0.7853982f) * Mat4::RotateX(1.5707963f)
		* Mat4::Scale(GetSize(), GetSize(), 1.f);
	ctx.renderer.DrawSoftDisc(ctx.viewProjection * model, GetR(), GetG(), GetB(), pulse);
}

// ----------------------------------------------------------- SignpostActor

SignpostActor::SignpostActor(float x, float y)
	: Actor(ActorType::Building)
{
	SetPosition(x, y, 0.f);
	SetSize(1.f);
	SetBoundingSphere(1.1f, 0.8f);
}

void SignpostActor::OnRender(const RenderContext& ctx)
{
	float x = GetWorldX();
	float y = GetWorldY();
	float z = GetWorldZ();
	Shapes::Rgb board = { 0.62f, 0.46f, 0.28f };

	// 기둥과 방향판 셋(각자 다른 쪽을 가리킴)을 카메라에서 먼 것부터.
	std::vector<Part> parts;
	parts.push_back(MakeBox(x, y, z, 0.12f, 0.12f, 1.6f, kWood));
	parts.push_back(MakeBox(x + 0.22f, y, z + 1.28f, 0.72f, 0.06f, 0.2f, board));
	parts.push_back(MakeBox(x, y + 0.22f, z + 1.02f, 0.06f, 0.66f, 0.2f, Shapes::Shade(board, 1.05f)));
	parts.push_back(MakeBox(x - 0.18f, y, z + 0.76f, 0.58f, 0.06f, 0.18f, Shapes::Shade(board, 0.95f)));
	parts.push_back(MakeBox(x, y, z + 1.6f, 0.18f, 0.18f, 0.06f, Shapes::Shade(kWood, 1.2f)));
	DrawPartsSorted(ctx, parts);
}

// -------------------------------------------------------------- CrackActor

CrackActor::CrackActor(float x, float y, float length)
	: Actor(ActorType::Marker, RenderLayer::Decal)
{
	SetPosition(x, y, 0.f);
	SetSize(length);
	SetBoundingSphere(length * 0.5f + 0.8f, 0.6f);
}

void CrackActor::OnRender(const RenderContext& ctx)
{
	float x = GetWorldX();
	float y = GetWorldY();
	float length = GetSize();
	float time = ctx.time;

	// 균열은 화면 가로로 길게(월드 (1,-1) 방향) 지그재그로 뻗는다.
	const int kSegments = 9;
	float points[kSegments + 1][2];
	for (int i = 0; i <= kSegments; ++i)
	{
		float t = (float)i / (float)kSegments - 0.5f;
		float along = t * length;
		float jitter = (Terrain::Hash(i, 3, 91) - 0.5f) * 0.55f;
		points[i][0] = x + kScreenAxisX * along + 0.7071f * jitter;
		points[i][1] = y + kScreenAxisY * along + 0.7071f * jitter;
	}

	Shapes::Rgb dark = { 0.04f, 0.02f, 0.06f };
	Shapes::Rgb glow = { 0.85f, 0.35f, 1.3f };
	float pulse = 0.55f + 0.45f * sinf(time * 2.2f);

	for (int pass = 0; pass < 2; ++pass)
	{
		for (int i = 0; i < kSegments; ++i)
		{
			// 가운데가 가장 넓고 양 끝으로 갈수록 가늘어지는 틈. 두 번째 패스는 안쪽에서 새어 나오는 빛.
			float middle = 1.f - fabsf((float)i / (float)(kSegments - 1) - 0.5f) * 1.6f;
			float width = (pass == 0) ? (0.1f + 0.22f * middle) : (0.03f + 0.07f * middle);

			float dx = points[i + 1][0] - points[i][0];
			float dy = points[i + 1][1] - points[i][1];
			float segmentLength = sqrtf(dx * dx + dy * dy);
			if (segmentLength < 0.0001f)
			{
				continue;
			}

			float nx = -dy / segmentLength * width;
			float ny = dx / segmentLength * width;
			Shapes::Rgb color = (pass == 0) ? dark : glow;
			float alpha = (pass == 0) ? 1.f : 0.45f * pulse;

			Shapes::DrawQuad(ctx, { points[i][0] - nx, points[i][1] - ny, 0.004f }, { points[i][0] + nx, points[i][1] + ny, 0.004f },
				{ points[i + 1][0] + nx, points[i + 1][1] + ny, 0.004f }, { points[i + 1][0] - nx, points[i + 1][1] - ny, 0.004f },
				color, alpha);
		}
	}

	// 틈에서 피어오르는 보랏빛 그림자 김: 위로 오르며 흐려지는 부드러운 원들.
	const int kPuffs = 7;
	for (int i = 0; i < kPuffs; ++i)
	{
		float phase = (time * 0.22f + (float)i / (float)kPuffs);
		phase -= floorf(phase);

		const float* anchor = points[1 + (i * 7) % (kSegments - 1)];
		float rise = phase * 1.8f;
		float size = 0.5f + phase * 0.9f;
		float alpha = (1.f - phase) * 0.32f;

		Mat4 model = Mat4::Translate(anchor[0], anchor[1], 0.2f + rise) * Mat4::RotateZ(-0.7853982f) * Mat4::RotateX(1.5707963f)
			* Mat4::Scale(size, size, 1.f);
		ctx.renderer.DrawSoftDisc(ctx.viewProjection * model, 0.22f, 0.08f, 0.32f, alpha);
	}
}

// --------------------------------------------------------------- StallActor

StallActor::StallActor(float x, float y, float r, float g, float b)
	: Actor(ActorType::Building)
{
	SetPosition(x, y, 0.f);
	SetSize(1.3f);
	SetColor(r, g, b);
	SetBoundingSphere(1.2f, 0.7f);
}

void StallActor::OnRender(const RenderContext& ctx)
{
	float x = GetWorldX();
	float y = GetWorldY();
	float z = GetWorldZ();
	Shapes::Rgb cloth = { GetR(), GetG(), GetB() };
	const float kPostHeight = 1.35f;

	// 뒤 기둥 둘 → 판매대 → 물건 → 앞 기둥 둘 → 비스듬한 천 차양(맨 위라 마지막).
	Shapes::DrawSolidBox(ctx, x - 0.6f, y - 0.32f, z, 0.07f, 0.07f, kPostHeight + 0.1f, kWood);
	Shapes::DrawSolidBox(ctx, x + 0.6f, y - 0.32f, z, 0.07f, 0.07f, kPostHeight + 0.1f, kWood);
	Shapes::DrawSolidBox(ctx, x, y, z, 1.3f, 0.7f, 0.55f, Shapes::Shade(kWood, 1.15f));

	Shapes::DrawCylinder(ctx, x - 0.35f, y - 0.1f, z + 0.55f, 0.12f, 0.26f, Shapes::Rgb{ 0.72f, 0.42f, 0.24f }, Shapes::Rgb{ 0.45f, 0.26f, 0.15f });
	Shapes::DrawUprightEllipse(ctx, x + 0.05f, y + 0.05f, z + 0.63f, 0.16f, 0.15f, Shapes::Rgb{ 0.95f, 0.55f, 0.15f });
	Shapes::DrawUprightEllipse(ctx, x + 0.2f, y + 0.08f, z + 0.62f, 0.15f, 0.14f, Shapes::Rgb{ 0.82f, 0.2f, 0.18f });
	Shapes::DrawUprightEllipse(ctx, x + 0.36f, y + 0.02f, z + 0.63f, 0.16f, 0.15f, Shapes::Rgb{ 0.45f, 0.62f, 0.22f });

	Shapes::DrawSolidBox(ctx, x - 0.6f, y + 0.32f, z, 0.07f, 0.07f, kPostHeight - 0.15f, kWood);
	Shapes::DrawSolidBox(ctx, x + 0.6f, y + 0.32f, z, 0.07f, 0.07f, kPostHeight - 0.15f, kWood);

	float backTop = z + kPostHeight + 0.1f;
	float frontTop = z + kPostHeight - 0.15f;
	Shapes::DrawQuad(ctx, { x - 0.72f, y - 0.42f, backTop }, { x - 0.72f, y + 0.46f, frontTop }, { x + 0.72f, y + 0.46f, frontTop },
		{ x + 0.72f, y - 0.42f, backTop }, cloth);
	Shapes::DrawQuad(ctx, { x - 0.2f, y - 0.42f, backTop + 0.002f }, { x - 0.2f, y + 0.46f, frontTop + 0.002f },
		{ x + 0.2f, y + 0.46f, frontTop + 0.002f }, { x + 0.2f, y - 0.42f, backTop + 0.002f }, Shapes::Rgb{ 0.95f, 0.92f, 0.85f });
}

bool StallActor::BlocksCircle(float x, float y, float moverRadius) const
{
	return CircleHitsRect(x, y, moverRadius, GetWorldX() - 0.68f, GetWorldX() + 0.68f, GetWorldY() - 0.38f, GetWorldY() + 0.38f);
}

// --------------------------------------------------------- TriggerZoneActor

TriggerZoneActor::TriggerZoneActor(float x, float y, float radius)
	: Actor(ActorType::Marker, RenderLayer::Decal)
	, m_Radius(radius)
{
	SetPosition(x, y, 0.f);
	SetSize(radius);
	SetBoundingSphere(radius + 0.3f);
}

bool TriggerZoneActor::UpdateInside(bool playerInside)
{
	bool entered = playerInside && !m_PlayerInside;
	m_PlayerInside = playerInside;
	return entered;
}

void TriggerZoneActor::OnRender(const RenderContext& ctx)
{
	if (!m_Highlighted)
	{
		return;
	}

	// 목표 지점: 바닥에 은은하게 숨 쉬는 금빛 웅덩이 + 가운데 밝은 점.
	float pulse = 0.5f + 0.5f * sinf(ctx.time * 3.f);
	float x = GetWorldX();
	float y = GetWorldY();
	float diameter = m_Radius * 2.2f;

	Mat4 pool = Mat4::Translate(x, y, 0.005f) * Mat4::Scale(diameter, diameter, 1.f);
	ctx.renderer.DrawSoftDisc(ctx.viewProjection * pool, 1.4f, 1.1f, 0.45f, 0.18f + 0.14f * pulse);

	Mat4 core = Mat4::Translate(x, y, 0.006f) * Mat4::Scale(diameter * 0.3f, diameter * 0.3f, 1.f);
	ctx.renderer.DrawSoftDisc(ctx.viewProjection * core, 1.6f, 1.3f, 0.6f, 0.35f + 0.25f * pulse);
}

// ---------------------------------------------------- ObjectiveMarkerActor

ObjectiveMarkerActor::ObjectiveMarkerActor()
	: Actor(ActorType::Effect, RenderLayer::Overlay)
{
	// 경계를 두지 않는다(대상이 맵 어디에나 있을 수 있어서 항상 그린다).
}

void ObjectiveMarkerActor::OnRender(const RenderContext& ctx)
{
	Shapes::Rgb outline = { 0.25f, 0.16f, 0.04f };
	Shapes::Rgb gold = { 1.5f, 1.15f, 0.35f };

	for (const Target& target : m_Targets)
	{
		float bob = sinf(ctx.time * 3.f) * 0.09f;
		float cx = target.x;
		float cy = target.y;
		float cz = target.z + bob;

		// 어두운 테두리 마름모 → 금빛 마름모(카메라를 향해 세움). 밝기가 1을 넘어 블룸으로 빛난다.
		for (int pass = 0; pass < 2; ++pass)
		{
			float halfWidth = (pass == 0) ? 0.17f : 0.12f;
			float halfHeight = (pass == 0) ? 0.26f : 0.2f;
			const Shapes::Rgb& color = (pass == 0) ? outline : gold;

			Shapes::Point3 top = Billboard(cx, cy, cz, 0.f, halfHeight);
			Shapes::Point3 bottom = Billboard(cx, cy, cz, 0.f, -halfHeight);
			Shapes::Point3 left = Billboard(cx, cy, cz, -halfWidth, 0.f);
			Shapes::Point3 right = Billboard(cx, cy, cz, halfWidth, 0.f);

			Shapes::DrawTriangle(ctx, top, left, right, color, (pass == 0) ? 0.8f : 1.f);
			Shapes::DrawTriangle(ctx, bottom, right, left, color, (pass == 0) ? 0.8f : 1.f);
		}
	}
}

// --------------------------------------------------------- GuideArrowActor

GuideArrowActor::GuideArrowActor()
	: Actor(ActorType::Marker, RenderLayer::Decal)
{
	// 부모(플레이어) 둘레로 가장 바깥 화살촉의 끝까지 감싼다.
	SetBoundingSphere(kGuideFirstDart + kGuideDartSpacing + 0.5f);
}

void GuideArrowActor::SetTarget(float x, float y)
{
	m_HasTarget = true;
	m_TargetX = x;
	m_TargetY = y;
}

void GuideArrowActor::OnRender(const RenderContext& ctx)
{
	if (!m_HasTarget)
	{
		return;
	}

	float x = GetWorldX();
	float y = GetWorldY();
	float dx = m_TargetX - x;
	float dy = m_TargetY - y;
	float distance = sqrtf(dx * dx + dy * dy);

	// 목표가 코앞이면 머리 위 마름모로 충분하므로, 가까워질수록 흐려지다 사라진다.
	float alpha = (distance - kGuideHideDistance) / kGuideFadeDistance;
	if (alpha <= 0.f)
	{
		return;
	}
	if (alpha > 1.f)
	{
		alpha = 1.f;
	}

	float dirX = dx / distance;
	float dirY = dy / distance;

	Shapes::Rgb outline = { 0.2f, 0.13f, 0.03f };
	Shapes::Rgb gold = { 1.5f, 1.15f, 0.35f };

	// 화살촉 두 개가 안쪽부터 차례로 밝아져서 목표 쪽으로 흘러가는 것처럼 보이게 하고, 전체가 살짝 앞뒤로 숨 쉰다.
	float bob = sinf(ctx.time * 3.f) * 0.05f;
	for (int i = 0; i < 2; ++i)
	{
		float forward = kGuideFirstDart + kGuideDartSpacing * (float)i + bob;
		float wave = 0.5f + 0.5f * sinf(ctx.time * 4.f - (float)i * 1.3f);
		float dartAlpha = alpha * (0.45f + 0.55f * wave);

		// 어두운 테두리(조금 큰 촉)를 먼저 깔고 그 위에 금빛 촉. 금빛은 밝기가 1을 넘어 블룸으로 은은히 빛난다.
		DrawGroundDart(ctx, x, y, dirX, dirY, forward, 1.3f, outline, 0.55f * dartAlpha);
		DrawGroundDart(ctx, x, y, dirX, dirY, forward, 1.f, gold, dartAlpha);
	}
}
