#include "stdafx.h"
#include "Actor.h"

#include <algorithm>

#include "Renderer.h"

Actor::Actor(ActorType type, RenderLayer layer)
	: m_Type(type)
	, m_Layer(layer)
{
}

Actor::~Actor()
{
}

Actor* Actor::AddChild(std::unique_ptr<Actor> child)
{
	child->m_Parent = this;
	m_Children.push_back(std::move(child));

	MarkSubtreeDirty();
	return m_Children.back().get();
}

void Actor::SetPosition(float x, float y, float z)
{
	m_X = x;
	m_Y = y;
	m_Z = z;

	// 내 로컬 위치는 부모가 캐시한 서브트리 경계에 들어 있으므로 부모 쪽이 낡아진다.
	// (내 서브트리 경계는 내 원점 기준이라 그대로 유효하다.)
	if (m_Parent != nullptr)
	{
		m_Parent->MarkSubtreeDirty();
	}
}

float Actor::GetWorldX() const
{
	return m_Parent ? m_Parent->GetWorldX() + m_X : m_X;
}

float Actor::GetWorldY() const
{
	return m_Parent ? m_Parent->GetWorldY() + m_Y : m_Y;
}

float Actor::GetWorldZ() const
{
	return m_Parent ? m_Parent->GetWorldZ() + m_Z : m_Z;
}

void Actor::SetColor(float r, float g, float b, float a)
{
	m_R = r;
	m_G = g;
	m_B = b;
	m_A = a;
}

void Actor::Update(const UpdateContext& ctx)
{
	OnUpdate(ctx);

	// OnUpdate 도중 자식이 추가될 수 있으므로 인덱스로 순회한다.
	for (size_t i = 0; i < m_Children.size(); ++i)
	{
		m_Children[i]->Update(ctx);
	}
}

void Actor::RemoveDestroyedChildren()
{
	size_t countBefore = m_Children.size();

	m_Children.erase(
		std::remove_if(m_Children.begin(), m_Children.end(),
			[](const std::unique_ptr<Actor>& child) { return child->IsPendingDestroy(); }),
		m_Children.end());

	if (m_Children.size() != countBefore)
	{
		MarkSubtreeDirty();
	}

	for (std::unique_ptr<Actor>& child : m_Children)
	{
		child->RemoveDestroyedChildren();
	}
}

void Actor::SetBoundingSphere(float radius, float centerZ)
{
	m_BoundRadius = radius;
	m_BoundCenterZ = centerZ;

	MarkSubtreeDirty();
}

BoundingSphere Actor::GetWorldBounds() const
{
	BoundingSphere bounds;
	bounds.x = GetWorldX();
	bounds.y = GetWorldY();
	bounds.z = GetWorldZ() + m_BoundCenterZ;
	bounds.radius = m_BoundRadius;
	return bounds;
}

void Actor::MarkSubtreeDirty()
{
	for (Actor* node = this; node != nullptr && !node->m_SubtreeDirty; node = node->m_Parent)
	{
		node->m_SubtreeDirty = true;
	}
}

void Actor::RefreshSubtree() const
{
	if (!m_SubtreeDirty)
	{
		return;
	}

	unsigned int renderable = IsRenderable() ? 1u : 0u;
	bool unbounded = IsRenderable() && m_BoundRadius <= 0.f;

	// 1) 자식들을 먼저 갱신하고, 나와 자식들의 구 중심(로컬 좌표)의 평균을 새 구의 중심으로 잡는다.
	float sumX = 0.f, sumY = 0.f, sumZ = 0.f;
	unsigned int sphereCount = 0;

	if (m_BoundRadius > 0.f)
	{
		sumZ += m_BoundCenterZ;
		++sphereCount;
	}

	for (const std::unique_ptr<Actor>& child : m_Children)
	{
		if (child->m_PendingDestroy)
		{
			continue;
		}

		child->RefreshSubtree();
		renderable += child->m_SubtreeRenderable;
		unbounded = unbounded || child->m_SubtreeUnbounded;

		if (child->m_SubtreeLocal.IsValid())
		{
			sumX += child->m_X + child->m_SubtreeLocal.x;
			sumY += child->m_Y + child->m_SubtreeLocal.y;
			sumZ += child->m_Z + child->m_SubtreeLocal.z;
			++sphereCount;
		}
	}

	m_SubtreeLocal = BoundingSphere();

	// 2) 그 중심에서 각 구를 다 덮는 최소 반지름을 구한다. 경계 없는 자손이 있으면 이 서브트리는
	//    경계를 "없음"으로 남겨서(항상 그려짐/항상 검사됨) 잘못 걸러내는 일이 없게 한다.
	if (!unbounded && sphereCount > 0)
	{
		float centerX = sumX / sphereCount;
		float centerY = sumY / sphereCount;
		float centerZ = sumZ / sphereCount;
		float radius = 0.f;

		if (m_BoundRadius > 0.f)
		{
			float dx = centerX;
			float dy = centerY;
			float dz = centerZ - m_BoundCenterZ;
			radius = sqrtf(dx * dx + dy * dy + dz * dz) + m_BoundRadius;
		}

		for (const std::unique_ptr<Actor>& child : m_Children)
		{
			if (child->m_PendingDestroy || !child->m_SubtreeLocal.IsValid())
			{
				continue;
			}

			float dx = child->m_X + child->m_SubtreeLocal.x - centerX;
			float dy = child->m_Y + child->m_SubtreeLocal.y - centerY;
			float dz = child->m_Z + child->m_SubtreeLocal.z - centerZ;
			float reach = sqrtf(dx * dx + dy * dy + dz * dz) + child->m_SubtreeLocal.radius;
			if (reach > radius)
			{
				radius = reach;
			}
		}

		m_SubtreeLocal.x = centerX;
		m_SubtreeLocal.y = centerY;
		m_SubtreeLocal.z = centerZ;
		m_SubtreeLocal.radius = radius;
	}

	m_SubtreeUnbounded = unbounded;
	m_SubtreeRenderable = renderable;
	m_SubtreeDirty = false;
}

BoundingSphere Actor::GetSubtreeBounds() const
{
	RefreshSubtree();

	BoundingSphere bounds = m_SubtreeLocal;
	if (bounds.IsValid())
	{
		bounds.x += GetWorldX();
		bounds.y += GetWorldY();
		bounds.z += GetWorldZ();
	}
	return bounds;
}

unsigned int Actor::GetSubtreeRenderableCount() const
{
	RefreshSubtree();
	return m_SubtreeRenderable;
}

void Actor::OnUpdate(const UpdateContext& ctx)
{
}

void Actor::OnRender(const RenderContext& ctx)
{
}

bool Actor::BlocksCircle(float x, float y, float moverRadius) const
{
	float radius = GetCollisionRadius();
	if (radius <= 0.f)
	{
		return false;
	}

	float dx = x - GetWorldX();
	float dy = y - GetWorldY();
	float minDist = moverRadius + radius;

	return dx * dx + dy * dy < minDist * minDist;
}

// 기본 그림자: 월드 위치 발밑에 크기에 비례한 반투명 블롭.
void Actor::OnRenderShadow(const RenderContext& ctx)
{
	Mat4 model = Mat4::Translate(GetWorldX(), GetWorldY(), 0.001f) * Mat4::Scale(m_Size * 0.9f, m_Size * 0.6f, 1.f);
	ctx.renderer.DrawShadow(ctx.viewProjection * model, 0.45f);
}
