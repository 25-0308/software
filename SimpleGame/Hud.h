#pragma once

#include "Math3D.h"
#include "Mesh.h"

class Renderer;
class SceneGraph;
struct PlayerStats;

// 화면에 고정되는 UI(HUD, 이름표). 둘 다 후처리(블룸/비네트/그레인)가 끝난 기본
// 프레임버퍼 위에 덧그려서 항상 또렷하게 보이게 한다.
namespace Hud
{
	// 화면 왼쪽 위: 레벨 배지(원+숫자) + 체력바(빨강) + 경험치바(하늘색) + 행동(공격/상호작용)
	// 쿨타임 막대. actionReadiness는 0~1이고, 1이면 바로 행동할 수 있는 상태(막대가 가득 차고
	// 초록색), 쿨타임 중에는 0에서 1까지 차오른다(주황색).
	// 월드 카메라와 별개인 화면 좌표계(800x600 픽셀)로 그린다.
	void DrawStatus(Renderer& renderer, const MeshHandle& circleMesh, const PlayerStats& stats, float actionReadiness);

	// 씬의 플레이어/NPC/짐승 머리 위에 이름을 그린다. 이 엔진엔 자체 폰트가 없어서
	// freeglut 내장 비트맵 폰트를 쓰며, 한글 글리프가 없어 영문 이름만 표시된다.
	void DrawNameTags(SceneGraph& scene, const Mat4& viewProjection);
}
