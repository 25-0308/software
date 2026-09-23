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
	template <typename T, typename... Args>
	T* Spawn(Args&&... args)
	{
		std::unique_ptr<T> actor(new T(std::forward<Args>(args)...));
		T* raw = actor.get();
		m_Root->AddChild(std::move(actor));
		return raw;
	}

	// 플레이어 액터를 등록해두면 몬스터 AI 등이 씬을 통해 찾을 수 있다.
	void SetPlayer(PlayerActor* player) { m_Player = player; }
	PlayerActor* GetPlayer() const { return m_Player; }

	// 트리 전체를 갱신하고, 파괴 예약된 액터를 프레임 끝에 정리한다.
	void Update(float deltaSeconds, float time, const TileMap& tileMap);

	// 뷰 컬링으로 화면 밖 액터를 거른 뒤, Ground → Decal → (그림자 → Object 깊이 정렬)
	// 순서로 그린다.
	void Render(const RenderContext& ctx);

	// 뷰 컬링 켜기/끄기(끄면 보이든 안 보이든 전부 그린다 — 최적화 효과 비교용).
	void SetCullingEnabled(bool enabled) { m_CullingEnabled = enabled; }
	bool IsCullingEnabled() const { return m_CullingEnabled; }

	const SceneRenderStats& GetLastRenderStats() const { return m_LastStats; }

	// 트리의 모든 액터(루트 제외)를 깊이 우선으로 방문한다.
	void ForEach(const std::function<void(Actor&)>& visitor);

	// (x,y)에서 maxRadius 안에 있고 filter를 통과하는 가장 가까운 액터. 없으면 nullptr.
	Actor* FindNearest(float x, float y, float maxRadius, const std::function<bool(const Actor&)>& filter) const;

	// (x,y)에 반지름 moverRadius의 원이 놓이면 충돌 반경이 있는 액터와 겹치는지.
	bool IsBlocked(float x, float y, float moverRadius) const;

private:
	std::unique_ptr<Actor> m_Root;
	PlayerActor* m_Player = nullptr;

	bool m_CullingEnabled = true;
	SceneRenderStats m_LastStats;

	// Render()가 매 프레임 쓰는 작업용 목록들. 함수 지역 변수로 두면 프레임마다 새로
	// 할당되고(성능 분석에서 확인된 문제 중 하나), 특히 ground는 타일 배치로 바꾸기 전엔
	// 최대 1024개까지 채워졌었다. 멤버로 옮기고 매 프레임 clear()만 해서(용량은 유지) 이
	// 재할당을 없앤다.
	std::vector<Actor*> m_RenderGround;
	std::vector<Actor*> m_RenderDecal;
	std::vector<Actor*> m_RenderObjects;
};
