#pragma once

// 프레임을 몇 개의 이름 붙은 구간으로 나눠서 CPU 시간을 재고, 주기적으로 콘솔에 구조화된
// 형태로 보고하는 지속적 성능 프로파일러. 목적은 "지금 당장 뭐가 느린지" 뿐 아니라
// "나중에 콘텐츠가 늘어나면 어디가 먼저 느려질지"까지 한눈에 보이게 하는 것이라, 로그 한
// 줄에 구간별 시간(ms)과 프레임 대비 비중(%)을 전부 담고, 비중이 큰 구간엔 [주의] 표시를
// 붙인다 — 사람이 훑어봐도, 나중에 이 로그를 붙여넣고 AI에게 분석을 맡겨도 바로 알아볼 수
// 있게 하기 위해서다(값 이름을 "갱신"/"씬렌더.바닥"처럼 실제 하는 일 그대로 붙임).
//
// 사용법: 재고 싶은 구간을 중괄호 블록으로 감싸고 그 안에 ScopedTimer를 만들면 된다.
//     { Profiler::ScopedTimer _(Profiler::Section::Update); SceneGraph 갱신... }
// 블록을 벗어나는 순간(return으로 일찍 빠져나가도) 소멸자에서 자동으로 시간을 기록한다.
namespace Profiler
{
	enum class Section
	{
		Update,        // SceneGraph::Update (액터 이동/AI 등)
		RenderGround,  // SceneGraph::Render의 Ground 레이어(타일 배치)
		RenderDecal,   // Decal 레이어(발밑 마커/조준 링)
		RenderShadow,  // 그림자 패스
		RenderObject,  // 캐릭터/건물/나무/아이템 등 Object 레이어
		PostProcess,   // 블룸/톤매핑/색보정 4패스
		Hud,           // 레벨 배지/체력바/경험치바/쿨타임바
		NameTags,      // 머리 위 이름표(레거시 glBitmap 경로)
		MiniMap,       // 오른쪽 위 미니맵
		ChatWindow,    // 왼쪽 아래 채팅창(메시지 텍스처 생성 포함)
		Present,       // glutSwapBuffers (수직동기화 대기가 여기 섞여 들어올 수 있음)
		Count,         // 배열 크기로만 씀 — Section 값으로 쓰지 않음
	};

	// 이 객체가 살아있는 동안 걸린 시간을 재서 해당 Section에 누적한다. 스코프를 벗어나면
	// (return으로 일찍 빠져나가도) 소멸자에서 자동으로 기록되므로 수동으로 시작/끝을 맞출
	// 필요가 없다.
	class ScopedTimer
	{
	public:
		explicit ScopedTimer(Section section);
		~ScopedTimer();

		ScopedTimer(const ScopedTimer&) = delete;
		ScopedTimer& operator=(const ScopedTimer&) = delete;

	private:
		Section m_Section;
		long long m_StartTicks;
	};

	// 프레임 시작(Idle() 맨 앞)에서 한 번 호출: 이번 프레임의 구간별 누적치를 0으로 돌린다.
	void BeginFrame();

	// 프레임 끝(버퍼 스왑 이후)에서 한 번 호출: 프레임 전체 시간을 집계하고, 일정 간격마다
	// (kReportIntervalSeconds) 콘솔에 구간별 분석 결과를 출력한다. note가 있으면 보고 줄
	// 끝에 그대로 덧붙인다(드로우콜/씬 통계 등 이 모듈이 모르는 값을 같이 보여주고 싶을 때).
	void EndFrame(const char* note = nullptr);
}
