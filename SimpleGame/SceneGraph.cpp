#include "stdafx.h"
#include "SceneGraph.h"

#include <algorithm>
#include <cmath>

namespace
{
	// 뷰 볼륨에 걸치는 액터만 레이어별로 모은다. 경계 계층 덕분에, 자신과 자손 전체가 화면 밖인
	// 노드는 자손을 하나하나 보지 않고 통째로 건너뛴다(예: 화면 밖 타일 청크 64칸을 검사 1번으로).
	void CollectVisible(Actor& actor, const ViewVolume& view, bool culling, SceneRenderStats& stats,
		std::vector<Actor*>& ground, std::vector<Actor*>& decal, std::vector<Actor*>& objects)
	{
		if (!actor.IsVisible() || actor.IsPendingDestroy())
		{
			return;
		}

		// 루트(부모 없음)는 검사하지 않는다. 자식이 없는 노드는 아래의 "자기 경계" 검사와
		// 같은 구라서 서브트리 검사를 건너뛴다.
		if (culling && actor.GetParent() != nullptr && !actor.GetChildren().empty()
			&& !view.Intersects(actor.GetSubtreeBounds()))
		{
			stats.culledActors += actor.GetSubtreeRenderableCount();
			return;
		}

		if (actor.IsRenderable())
		{
			if (culling && !view.Intersects(actor.GetWorldBounds()))
			{
				++stats.culledActors; // 자신만 화면 밖. 자손은 각자 검사한다.
			}
			else
			{
				switch (actor.GetLayer())
				{
				case RenderLayer::Ground: ground.push_back(&actor); break;
				case RenderLayer::Decal:  decal.push_back(&actor); break;
				case RenderLayer::Object: objects.push_back(&actor); break;
				}
				++stats.renderedActors;
			}
		}

		for (const std::unique_ptr<Actor>& child : actor.GetChildren())
		{
			CollectVisible(*child, view, culling, stats, ground, decal, objects);
		}
	}

	void VisitAll(Actor& actor, const std::function<void(Actor&)>& visitor)
	{
		for (const std::unique_ptr<Actor>& child : actor.GetChildren())
		{
			visitor(*child);
			VisitAll(*child, visitor);
		}
	}

	// 서브트리 경계 구가 (x,y)를 중심으로 한 반지름 reach의 원에 닿을 수 있는지(수평 거리 기준).
	// 경계가 없는 서브트리는 판단할 수 없으므로 항상 true. 액터의 원점과 충돌 원은 자기
	// 경계 구 안에 들어 있어야 이 가지치기가 안전하다.
	bool SubtreeMayReach(const BoundingSphere& bounds, float x, float y, float reach)
	{
		if (!bounds.IsValid())
		{
			return true;
		}

		float dx = bounds.x - x;
		float dy = bounds.y - y;
		float limit = bounds.radius + reach;

		return dx * dx + dy * dy <= limit * limit;
	}

	void FindNearestRecursive(const Actor& actor, float x, float y, const std::function<bool(const Actor&)>& filter,
		float& bestDistSq, Actor*& best)
	{
		for (const std::unique_ptr<Actor>& child : actor.GetChildren())
		{
			if (child->IsPendingDestroy())
			{
				continue;
			}

			// 지금까지 찾은 가장 가까운 거리보다 멀리 있는 서브트리는 들어가지 않는다.
			if (!SubtreeMayReach(child->GetSubtreeBounds(), x, y, sqrtf(bestDistSq)))
			{
				continue;
			}

			if (filter(*child))
			{
				float dx = child->GetWorldX() - x;
				float dy = child->GetWorldY() - y;
				float distSq = dx * dx + dy * dy;

				if (distSq < bestDistSq)
				{
					bestDistSq = distSq;
					best = child.get();
				}
			}

			FindNearestRecursive(*child, x, y, filter, bestDistSq, best);
		}
	}

	bool IsBlockedRecursive(const Actor& actor, float x, float y, float moverRadius)
	{
		for (const std::unique_ptr<Actor>& child : actor.GetChildren())
		{
			if (child->IsPendingDestroy())
			{
				continue;
			}

			// 충돌 원은 경계 구 안에 있으므로, 이동체(반지름 moverRadius)가 닿을 수 없는 서브트리는 건너뛴다.
			if (!SubtreeMayReach(child->GetSubtreeBounds(), x, y, moverRadius))
			{
				continue;
			}

			if (child->BlocksCircle(x, y, moverRadius))
			{
				return true;
			}

			if (IsBlockedRecursive(*child, x, y, moverRadius))
			{
				return true;
			}
		}

		return false;
	}
}

SceneGraph::SceneGraph()
	: m_Root(new Actor(ActorType::Group))
{
}

void SceneGraph::Update(float deltaSeconds, float time, const TileMap& tileMap)
{
	UpdateContext ctx = { deltaSeconds, time, *this, tileMap };

	m_Root->Update(ctx);
	m_Root->RemoveDestroyedChildren();
}

void SceneGraph::Render(const RenderContext& ctx)
{
	ViewVolume view(ctx.viewProjection);

	SceneRenderStats stats;
	stats.totalActors = m_Root->GetSubtreeRenderableCount();

	std::vector<Actor*> ground;
	std::vector<Actor*> decal;
	std::vector<Actor*> objects;
	CollectVisible(*m_Root, view, m_CullingEnabled, stats, ground, decal, objects);

	m_LastStats = stats;

	for (Actor* actor : ground)
	{
		actor->OnRender(ctx);
	}
	for (Actor* actor : decal)
	{
		actor->OnRender(ctx);
	}

	// 이 씬은 2.5D이고 실제 3D 깊이버퍼가 아니므로, 월드 깊이(y + z) 기준으로
	// 정렬한 뒤 뒤에서 앞 순서로 그린다 (페인터 알고리즘). 컬링 후에 정렬하므로
	// 화면 밖 액터는 정렬 비용도 들지 않는다.
	std::stable_sort(objects.begin(), objects.end(), [](const Actor* lhs, const Actor* rhs)
	{
		return (lhs->GetWorldY() + lhs->GetWorldZ()) < (rhs->GetWorldY() + rhs->GetWorldZ());
	});

	// 그림자를 먼저 그려 캐릭터/건물 발밑에 깔리도록 한다.
	for (Actor* actor : objects)
	{
		if (actor->CastsShadow())
		{
			actor->OnRenderShadow(ctx);
		}
	}

	for (Actor* actor : objects)
	{
		actor->OnRender(ctx);
	}
}

void SceneGraph::ForEach(const std::function<void(Actor&)>& visitor)
{
	VisitAll(*m_Root, visitor);
}

Actor* SceneGraph::FindNearest(float x, float y, float maxRadius, const std::function<bool(const Actor&)>& filter) const
{
	float bestDistSq = maxRadius * maxRadius;
	Actor* best = nullptr;

	FindNearestRecursive(*m_Root, x, y, filter, bestDistSq, best);
	return best;
}

bool SceneGraph::IsBlocked(float x, float y, float moverRadius) const
{
	return IsBlockedRecursive(*m_Root, x, y, moverRadius);
}
