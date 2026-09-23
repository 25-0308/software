#pragma once

#include <memory>
#include <vector>

#include "Bounds.h"
#include "Math3D.h"
#include "Mesh.h"

class Renderer;
class SceneGraph;
class TileMap;

enum class ActorType
{
	Group,     // 자식들을 묶기만 하는 빈 노드 (씬 그래프의 루트 등)
	TileBatch, // 청크 하나(예: 8x8칸)의 바닥 타일들을 정점 색상 메시 하나로 구워 그리는 배치
	Prop,      // 나무 같은 장식/장애물
	Building,  // 건물
	Item,      // 획득 가능한 아이템
	Fire,      // 횃불 (렌더링 시 불 셰이더로 분기)
	Marker,    // 발밑 링 같은 바닥 표시
	Player,
	NPC,
	Animal,    // 야생 짐승 (사슴/늑대)
};

// 그리기 순서 그룹. 씬 그래프는 Ground → Decal → Object 순서로 그리고,
// Object만 깊이(y+z) 기준으로 정렬한다.
enum class RenderLayer
{
	Ground, // 바닥 타일: 서로 겹치지 않으므로 정렬 불필요
	Decal,  // 바닥 위 표시(마커/조준 링): 그림자·캐릭터보다 먼저 그림
	Object, // 캐릭터/건물/나무 등: 깊이 정렬 후 그림
};

struct UpdateContext
{
	float deltaSeconds;
	float time;
	SceneGraph& scene;
	const TileMap& tileMap;
};

struct RenderContext
{
	Renderer& renderer;
	const Mat4& viewProjection;
	float time;
	const MeshHandle& circleMesh;
	const MeshHandle& ellipseMesh;
};

// 화면에 배치되는 모든 오브젝트의 기반 클래스. 씬 그래프의 노드이기도 해서
// 부모/자식 계층을 가진다: 로컬 위치(SetPosition)는 부모 기준이고, 월드 위치
// (GetWorldX/Y/Z)는 부모 체인을 거슬러 올라가며 더해서 얻는다(이동만 상속하고
// 회전/스케일은 상속하지 않음). 실제 동작(이동/AI/그리기)은 하위 클래스가
// OnUpdate/OnRender를 재정의해서 구현한다.
class Actor
{
public:
	explicit Actor(ActorType type, RenderLayer layer = RenderLayer::Object);
	virtual ~Actor();

	Actor(const Actor&) = delete;
	Actor& operator=(const Actor&) = delete;

	// ---- 씬 그래프 계층 ----
	Actor* AddChild(std::unique_ptr<Actor> child);
	Actor* GetParent() const { return m_Parent; }
	const std::vector<std::unique_ptr<Actor>>& GetChildren() const { return m_Children; }

	// ---- 트랜스폼 ----
	void SetPosition(float x, float y, float z);
	float GetX() const { return m_X; }
	float GetY() const { return m_Y; }
	float GetZ() const { return m_Z; }
	float GetWorldX() const;
	float GetWorldY() const;
	float GetWorldZ() const;

	// 바라보는 방향(라디안, atan2 규약: 0=+x, 90도=+y).
	void SetFacing(float radians) { m_Facing = radians; }
	float GetFacing() const { return m_Facing; }

	// ---- 속성 ----
	ActorType GetType() const { return m_Type; }
	RenderLayer GetLayer() const { return m_Layer; }

	void SetSize(float size) { m_Size = size; }
	float GetSize() const { return m_Size; }

	void SetColor(float r, float g, float b, float a = 1.f);
	float GetR() const { return m_R; }
	float GetG() const { return m_G; }
	float GetB() const { return m_B; }
	float GetA() const { return m_A; }

	// 머리 위 이름표에 쓰는 표시 이름. 정적 문자열 리터럴만 받는다(소유권 없음).
	// nullptr이면 이름표를 그리지 않는다.
	void SetName(const char* name) { m_Name = name; }
	const char* GetName() const { return m_Name; }

	// 0이면 상호작용 불가, 그 외엔 게임이 분기하는 상호작용 식별자.
	void SetInteractId(int id) { m_InteractId = id; }
	int GetInteractId() const { return m_InteractId; }

	void SetVisible(bool visible) { m_Visible = visible; }
	bool IsVisible() const { return m_Visible; }

	// 화면에 그려지는 액터인지(Group은 자식을 묶기만 하므로 아님).
	bool IsRenderable() const { return m_Type != ActorType::Group; }

	// ---- 경계(컬링/공간 질의용) ----
	// 이 액터 자신을 감싸는 경계 구: 액터 원점 기준이고, 중심은 위로 centerZ만큼 올린 곳.
	// 보이는 모든 파츠(그림자·애니메이션으로 튀어나오는 팔다리 포함)를 감싸도록 넉넉히
	// 잡아야 컬링 때 화면 가장자리에서 튀어 보이지 않는다. 0이면 경계 없음(항상 그려짐).
	void SetBoundingSphere(float radius, float centerZ = 0.f);

	// 자기 자신만의 월드 경계 구.
	BoundingSphere GetWorldBounds() const;

	// 자신과 모든 자손을 한꺼번에 감싸는 월드 경계 구(경계 계층). 그룹 노드 하나를 검사해서
	// 수십~수백 개 자손을 통째로 걸러내는 데 쓴다. 자손이 움직이거나 추가/제거되면 다음
	// 호출 때 다시 계산한다(캐시 + 더티 플래그). 그려지는 자손 중 경계 없는 것이 하나라도
	// 있으면 안전하게 "경계 없음"(radius 0)을 돌려준다.
	BoundingSphere GetSubtreeBounds() const;

	// 자신과 자손 중 그려지는 액터(Group 제외)의 수. 컬링 통계용.
	unsigned int GetSubtreeRenderableCount() const;

	// ---- 수명 ----
	// 즉시 지우지 않고 예약만 한다. 씬 그래프가 프레임 끝에 안전하게 제거한다.
	void Destroy() { m_PendingDestroy = true; }
	bool IsPendingDestroy() const { return m_PendingDestroy; }

	// ---- 매 프레임 ----
	// 자신의 OnUpdate 후 자식들을 순서대로 갱신한다.
	void Update(const UpdateContext& ctx);

	// 파괴 예약된 자식을 (재귀적으로) 실제로 제거한다.
	void RemoveDestroyedChildren();

	virtual void OnUpdate(const UpdateContext& ctx);
	virtual void OnRender(const RenderContext& ctx);
	virtual void OnRenderShadow(const RenderContext& ctx);

	// 이동을 막는 충돌 반경(월드 단위). 0이면 통과 가능. 기본 BlocksCircle이 이 값으로 만든 원을 쓴다.
	virtual float GetCollisionRadius() const { return 0.f; }

	// 반지름 moverRadius의 원이 (x,y)에 놓였을 때 이 액터와 겹쳐 이동이 막히는지. 기본은
	// GetCollisionRadius()로 만든 원과의 원-원 판정이고, 원이 아닌 모양(건물 등)은 재정의한다.
	// 충돌 영역은 액터의 경계 구(SetBoundingSphere) 안에 있어야 씬의 질의 가지치기가 안전하다.
	virtual bool BlocksCircle(float x, float y, float moverRadius) const;

	// true면 씬 그래프의 그림자 패스에서 발밑에 블롭 섀도우를 그린다.
	virtual bool CastsShadow() const { return false; }

private:
	// 조상 방향으로 "서브트리 경계가 낡았다" 표시를 올려 보낸다. 이미 더티인 노드를 만나면
	// 그 위쪽 조상도 이미 더티이므로(더티 노드는 항상 조상도 더티) 거기서 멈춘다.
	void MarkSubtreeDirty();

	// 더티면 자손까지 재귀로 서브트리 경계/개수를 다시 계산한다.
	void RefreshSubtree() const;

	ActorType m_Type;
	RenderLayer m_Layer;

	Actor* m_Parent = nullptr;
	std::vector<std::unique_ptr<Actor>> m_Children;

	float m_X = 0.f, m_Y = 0.f, m_Z = 0.f;
	float m_Facing = -1.5707963f;
	float m_Size = 1.f;
	float m_R = 1.f, m_G = 1.f, m_B = 1.f, m_A = 1.f;
	const char* m_Name = nullptr;
	int m_InteractId = 0;
	bool m_Visible = true;
	bool m_PendingDestroy = false;

	// 자기 자신의 경계 구(로컬).
	float m_BoundRadius = 0.f;
	float m_BoundCenterZ = 0.f;

	// 서브트리 경계 캐시. 이 액터 원점 기준 로컬 좌표라서, 조상이 움직여도 낡아지지 않는다
	// (부모 이동만 상속하므로 자식 구는 자식의 로컬 위치만큼만 옮기면 된다).
	mutable bool m_SubtreeDirty = true;
	mutable bool m_SubtreeUnbounded = false;
	mutable BoundingSphere m_SubtreeLocal;
	mutable unsigned int m_SubtreeRenderable = 0;
};
