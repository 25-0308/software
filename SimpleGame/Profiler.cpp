#include "stdafx.h"
#include "Profiler.h"

#include <chrono>
#include <cstdio>
#include <iostream>

namespace
{
	using Clock = std::chrono::steady_clock;

	// 콘솔에 구간별 분석을 출력하는 간격(초). DrawCallHook과 같은 값을 쓰면(둘 다 1초) 두 로그가
	// 거의 같은 시점에 나란히 찍혀서 "이 순간의 드로우콜 수"와 "이 순간 CPU가 뭘 하고 있었는지"를
	// 한눈에 맞춰 볼 수 있다.
	const double kReportIntervalSeconds = 1.0;

	// 구간 평균이 프레임 시간의 이 비율을 넘으면 [주의] 표시를 붙인다 — 지금 당장 문제가
	// 아니더라도, 콘텐츠가 늘어나면 여기부터 느려질 가능성이 크다는 신호.
	const double kHotspotFraction = 0.25;

	const int kSectionCount = (int)Profiler::Section::Count;

	// AI가 로그만 보고도 바로 알아볼 수 있도록, 코드에서 하는 일 그대로 이름을 붙인다.
	const char* kSectionNames[kSectionCount] =
	{
		"갱신",          // Update
		"씬렌더.바닥",   // RenderGround
		"씬렌더.장식",   // RenderDecal
		"씬렌더.그림자", // RenderShadow
		"씬렌더.오브젝트", // RenderObject
		"후처리",        // PostProcess
		"HUD",           // Hud
		"이름표",        // NameTags
		"미니맵",        // MiniMap
		"채팅창",        // ChatWindow
		"화면출력",      // Present
	};

	long long NowNanoseconds()
	{
		return std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now().time_since_epoch()).count();
	}

	// 이번 프레임에 구간별로 누적된 시간(ms). BeginFrame에서 0으로 돌아간다.
	double g_FrameSectionMs[kSectionCount] = { 0.0 };

	long long g_FrameStartTicks = 0;

	// 보고 간격 동안 누적한 값들.
	Clock::time_point g_IntervalStart;
	unsigned int g_IntervalFrames = 0;
	double g_IntervalTotalMs = 0.0;
	double g_IntervalMinMs = 1e9;
	double g_IntervalMaxMs = 0.0;
	double g_IntervalSectionMs[kSectionCount] = { 0.0 };

	bool g_IntervalStartValid = false;
}

Profiler::ScopedTimer::ScopedTimer(Section section)
	: m_Section(section)
	, m_StartTicks(NowNanoseconds())
{
}

Profiler::ScopedTimer::~ScopedTimer()
{
	int index = (int)m_Section;
	if (index < 0 || index >= kSectionCount)
	{
		return;
	}

	double ms = (double)(NowNanoseconds() - m_StartTicks) / 1000000.0;
	g_FrameSectionMs[index] += ms;
}

void Profiler::BeginFrame()
{
	g_FrameStartTicks = NowNanoseconds();

	for (int i = 0; i < kSectionCount; ++i)
	{
		g_FrameSectionMs[i] = 0.0;
	}

	if (!g_IntervalStartValid)
	{
		g_IntervalStart = Clock::now();
		g_IntervalStartValid = true;
	}
}

void Profiler::EndFrame(const char* note)
{
	double frameMs = (double)(NowNanoseconds() - g_FrameStartTicks) / 1000000.0;

	++g_IntervalFrames;
	g_IntervalTotalMs += frameMs;
	if (frameMs < g_IntervalMinMs) g_IntervalMinMs = frameMs;
	if (frameMs > g_IntervalMaxMs) g_IntervalMaxMs = frameMs;
	for (int i = 0; i < kSectionCount; ++i)
	{
		g_IntervalSectionMs[i] += g_FrameSectionMs[i];
	}

	Clock::time_point now = Clock::now();
	double elapsed = std::chrono::duration<double>(now - g_IntervalStart).count();
	if (elapsed < kReportIntervalSeconds || g_IntervalFrames == 0)
	{
		return;
	}

	double avgFrameMs = g_IntervalTotalMs / g_IntervalFrames;
	double fps = (elapsed > 0.0) ? (double)g_IntervalFrames / elapsed : 0.0;

	// 한 줄: [프로파일] 프레임 요약 | 구간=ms(비중%)[주의] ... | 부가정보
	// 사람이 훑어보기도 쉽고, 이 줄을 그대로 복사해서 AI에게 분석을 맡겨도 구간 이름과
	// 숫자가 명확해서 바로 해석할 수 있는 형태로 만들었다.
	// 한글 구간 이름 11개 + [주의] 표시 + note까지 다 붙어도 넉넉하도록 여유 있게 잡는다
	// (모자라면 snprintf가 뒤쪽을 조용히 잘라낼 뿐 메모리 오류는 나지 않지만, 로그가
	// 잘리면 정작 보고 싶은 구간이 안 보일 수 있으니 애초에 넉넉하게).
	char line[1024];
	int written = snprintf(line, sizeof(line),
		"[프로파일] %u프레임 평균 %.2fms(%.0fFPS, 최소 %.2fms/최대 %.2fms) |",
		g_IntervalFrames, avgFrameMs, fps, g_IntervalMinMs, g_IntervalMaxMs);

	for (int i = 0; i < kSectionCount && written > 0 && written < (int)sizeof(line); ++i)
	{
		double sectionAvgMs = g_IntervalSectionMs[i] / g_IntervalFrames;
		double fraction = (avgFrameMs > 0.0) ? sectionAvgMs / avgFrameMs : 0.0;
		bool isHotspot = (fraction >= kHotspotFraction);

		written += snprintf(line + written, sizeof(line) - written, " %s=%.2fms(%.0f%%)%s",
			kSectionNames[i], sectionAvgMs, fraction * 100.0, isHotspot ? "[주의]" : "");
	}

	if (note != nullptr && written > 0 && written < (int)sizeof(line))
	{
		snprintf(line + written, sizeof(line) - written, " | %s", note);
	}

	std::cout << line << "\n";

	g_IntervalStartValid = false; // 다음 BeginFrame에서 새 간격을 시작한다.
	g_IntervalFrames = 0;
	g_IntervalTotalMs = 0.0;
	g_IntervalMinMs = 1e9;
	g_IntervalMaxMs = 0.0;
	for (int i = 0; i < kSectionCount; ++i)
	{
		g_IntervalSectionMs[i] = 0.0;
	}
}
