#pragma once

#include <functional>
#include <memory>
#include <utility>
#include <vector>

#include "Actor.h"

class PlayerActor;

// 마지막으로 그린 프레임의 컬링 결과. 그려지는 액터(Group 제외)만 센다.
struct SceneRenderStats
{
	unsigned int totalActors = 0;    // 씬에 있는 전체 수
	unsigned int renderedActors = 0; // 그리기 목록에 들어가 실제로 그려진 수
	unsigned int culledActors = 0;   // 화면 밖이라 컬링으로 제외된 수(서브트리째 걸러진 것 포함)
};

// 화면에 배치된 모든 Actor를 트리(부모-자식 계층)로 소유하고 제어하는 씬 그래프.
// 갱신/그리기/검색/충돌 질의가 전부 여기를 거치므로, 게임 코드는 Actor를 직접
// 들고 다니지 않고 씬에 물어본다.
//
// 씬 그래프의 핵심 용도는 "경계 계층"을 이용한 공간 최적화다: 모든 노드는 자신과 자손을
// 감싸는 경계 구를 캐시하고 있어서,
//  - 그릴 때: 카메라 뷰 볼륨 밖인 노드는 자손 전체를 한 번의 검사로 건너뛴다(뷰 컬링).
//  - 질의할 때: 검색/충돌 반경 밖인 서브트리는 안 들어가고 건너뛴다.
// 그래서 액터를 그룹 노드 밑에 공간적으로 가깝게 묶어둘수록(예: 타일 청크) 효과가 크다.
class SceneGraph
{
public:
	SceneGraph();

	SceneGraph(const SceneGraph&) = delete;
	SceneGraph& operator=(const SceneGraph&) = delete;

	// 루트 바로 아래에 액터를 생성해서 붙이고 원래 타입의 포인터를 돌려준다.
	// 다른 액터의 자식으로 붙이려면 그 액터의 AddChild를 직접 쓴다.
	// 생성 문맥(SetSpawnContext)이 있으면 만든 액터를 그만큼 옮기고 그 지역 소속으로 표시한다.
	template <typename T, typename... Args>
	T* Spawn(Args&&... args)
	{
		std::unique_ptr<T> actor(new T(std::forward<Args>(args)...));
		T* raw = actor.get();

		if (m_SpawnOffsetX != 0.f || m_SpawnOffsetY != 0.f)
		{
			raw->SetPosition(raw->GetX() + m_SpawnOffsetX, raw->GetY() + m_SpawnOffsetY, raw->GetZ());
		}
		if (m_SpawnRegion != nullptr)
		{
			raw->SetHomeRegion(m_SpawnRegion);
		}

		m_Root->AddChild(std::move(actor));
		return raw;
	}

	// 지역 하나를 지을 동안의 생성 문맥: 그동안 Spawn하는 액터는 (offsetX, offsetY)만큼 옮겨지고(지역 로컬 좌표 →
	// 월드 좌표) regionId 소속이 된다. 큰 월드에 지역을 하나씩 앉힐 때, 지역을 짓는 코드는 예전처럼 지역 가운데를
	// 원점으로 한 좌표를 그대로 쓰게 하려는 것이다. regionId는 정적 문자열만. 다 지으면 ClearSpawnContext.
	// 주의: 생성자에서 자기 자리를 따로 기억하는 액터(짐승의 배회 기준점 등)는 생성 뒤에 다시 맞춰야 한다.
	void SetSpawnContext(const char* regionId, float offsetX, float offsetY)
	{
		m_SpawnRegion = regionId;
		m_SpawnOffsetX = offsetX;
		m_SpawnOffsetY = offsetY;
	}
	void ClearSpawnContext() { SetSpawnContext(nullptr, 0.f, 0.f); }
	float GetSpawnOffsetX() const { return m_SpawnOffsetX; }
	float GetSpawnOffsetY() const { return m_SpawnOffsetY; }

	// 루트 바로 아래의 액터 중 shouldGroup이 true인 것(움직이지 않는 나무·건물·바위·바닥 청크 등)을 cellSize 크기의
	// 정사각형 칸마다 그룹 노드 하나로 묶는다. 큰 월드에선 루트 아래에 액터가 수천 개 늘어서 있으면 충돌·검색 질의가
	// 매번 그 전부를 훑게 되는데, 칸으로 묶어 두면 경계 계층 덕분에 멀리 있는 칸은 검사 한 번으로 통째로 건너뛴다.
	// 그룹 노드는 원점에 두므로 옮겨진 액터의 로컬 위치가 그대로 월드 위치다.
	void GroupIntoCells(float cellSize, const std::function<bool(const Actor&)>& shouldGroup);

	// 플레이어 액터를 등록해두면 몬스터 AI 등이 씬을 통해 찾을 수 있다.
	void SetPlayer(PlayerActor* player) { m_Player = player; }
	PlayerActor* GetPlayer() const { return m_Player; }

	// 트리 전체를 갱신하고, 파괴 예약된 액터를 프레임 끝에 정리한다.
	void Update(float deltaSeconds, float time, const TileMap& tileMap);

	// 캐릭터(플레이어·NPC·짐승)를 갱신하는 범위. radius가 0보다 크면 (x, y)에서 그보다 먼 캐릭터는 그 프레임에 멈춰
	// 있는다(AI·걷기·충돌 검사를 하지 않음) — 마을 여러 곳이 한 월드에 함께 있어서, 화면에서 한참 먼 마을의 주민과
	// 황야의 몬스터까지 매 프레임 움직이면 충돌 질의만으로 프레임이 무거워진다. 0이면 모두 갱신한다.
	void SetActiveArea(float x, float y, float radius)
	{
		m_ActiveX = x;
		m_ActiveY = y;
		m_ActiveRadius = radius;
	}

	// 뷰 컬링으로 화면 밖 액터를 거른 뒤, Ground → Decal → (그림자 → Object 깊이 정렬) → Overlay
	// 순서로 그리고, 마지막에 렌더러에 모아 둔 도형을 Flush해서 씬 버퍼에 확실히 그려 둔다.
	void Render(const RenderContext& ctx);

	// 뷰 컬링 켜기/끄기(끄면 보이든 안 보이든 전부 그린다 — 최적화 효과 비교용).
	void SetCullingEnabled(bool enabled) { m_CullingEnabled = enabled; }
	bool IsCullingEnabled() const { return m_CullingEnabled; }

	const SceneRenderStats& GetLastRenderStats() const { return m_LastStats; }

	// 트리의 모든 액터(루트 제외)를 깊이 우선으로 방문한다.
	void ForEach(const std::function<void(Actor&)>& visitor);

	// (x,y)에서 maxRadius 안에 있고 filter를 통과하는 가장 가까운 액터. 없으면 nullptr.
	// 숨은 액터(IsVisible() == false)와 그 자손은 찾지 않는다.
	Actor* FindNearest(float x, float y, float maxRadius, const std::function<bool(const Actor&)>& filter) const;

	// (x,y)에 반지름 moverRadius의 원이 놓이면 충돌 반경이 있는 액터와 겹치는지. 숨은 액터는 막지 않는다.
	bool IsBlocked(float x, float y, float moverRadius) const;

private:
	std::unique_ptr<Actor> m_Root;
	PlayerActor* m_Player = nullptr;

	bool m_CullingEnabled = true;
	SceneRenderStats m_LastStats;

	float m_ActiveX = 0.f;
	float m_ActiveY = 0.f;
	float m_ActiveRadius = 0.f;

	const char* m_SpawnRegion = nullptr;
	float m_SpawnOffsetX = 0.f;
	float m_SpawnOffsetY = 0.f;

	// Render()가 매 프레임 쓰는 작업용 목록들. 함수 지역 변수로 두면 프레임마다 새로
	// 할당되고(성능 분석에서 확인된 문제 중 하나), 특히 ground는 타일 배치로 바꾸기 전엔
	// 최대 1024개까지 채워졌었다. 멤버로 옮기고 매 프레임 clear()만 해서(용량은 유지) 이
	// 재할당을 없앤다.
	std::vector<Actor*> m_RenderGround;
	std::vector<Actor*> m_RenderDecal;
	std::vector<Actor*> m_RenderObjects;
	std::vector<Actor*> m_RenderOverlay;

	// Object 레이어를 깊이 정렬할 때 쓰는 목록. 비교할 때마다 부모 체인을 거슬러 월드 위치를 다시
	// 계산하지 않도록, 액터마다 카메라에서 먼 정도를 한 번만 구해서 같이 담아 둔다.
	struct SortedActor
	{
		Actor* actor;
		float distance; // 카메라에서 먼 정도(클수록 멀어서 먼저 그림)
	};
	std::vector<SortedActor> m_SortedObjects;
};
