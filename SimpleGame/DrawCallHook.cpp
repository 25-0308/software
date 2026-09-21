#include "stdafx.h"
#include "DrawCallHook.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <chrono>
#include <climits>
#include <cstdio>
#include <cstring>
#include <iostream>

#include "Dependencies\glew.h"

namespace
{
	// 콘솔에 집계 결과를 출력하는 간격(초). 0이면 매 프레임 출력한다 — 다만 이 게임은
	// 프레임 제한이 없어서 초당 수백~수천 줄이 쏟아지고 콘솔 출력 자체가 프레임을
	// 떨어뜨리므로, 기본은 1초마다 한 번 출력한다.
	const double kReportIntervalSeconds = 1.0;

	// 후킹할 OpenGL 함수들의 원형 (APIENTRY = __stdcall).
	typedef void (APIENTRY *DrawArraysFn)(GLenum mode, GLint first, GLsizei count);
	typedef void (APIENTRY *DrawElementsFn)(GLenum mode, GLsizei count, GLenum type, const GLvoid* indices);

	DrawArraysFn g_OriginalDrawArrays = nullptr;
	DrawElementsFn g_OriginalDrawElements = nullptr;

	bool g_Installed = false;

	unsigned int g_FrameDrawCalls = 0; // 이번 프레임에서 지금까지 센 드로우 콜 수

	using Clock = std::chrono::steady_clock;

	unsigned long long g_FrameIndex = 0;
	Clock::time_point g_IntervalStart;
	unsigned int g_IntervalFrames = 0;
	unsigned long long g_IntervalDrawCalls = 0;
	unsigned int g_IntervalMin = UINT_MAX;
	unsigned int g_IntervalMax = 0;

	// ---- 후크 함수: 원래 드로우 콜 대신 여기로 점프해 온다 ----

	void APIENTRY HookedDrawArrays(GLenum mode, GLint first, GLsizei count)
	{
		++g_FrameDrawCalls;
		g_OriginalDrawArrays(mode, first, count);
	}

	void APIENTRY HookedDrawElements(GLenum mode, GLsizei count, GLenum type, const GLvoid* indices)
	{
		++g_FrameDrawCalls;
		g_OriginalDrawElements(mode, count, type, indices);
	}

	// module의 임포트 테이블에서 dllName!functionName 항목을 찾아, 그 주소 칸(IAT 슬롯)을
	// hook 주소로 바꾼다. 찾아서 바꿨으면 true, 바꾸기 전의 주소를 *original에 담아 준다.
	bool PatchImport(HMODULE module, const char* dllName, const char* functionName, void* hook, void** original)
	{
		BYTE* base = reinterpret_cast<BYTE*>(module);
		IMAGE_DOS_HEADER* dosHeader = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
		IMAGE_NT_HEADERS* ntHeaders = reinterpret_cast<IMAGE_NT_HEADERS*>(base + dosHeader->e_lfanew);

		IMAGE_DATA_DIRECTORY importDirectory = ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
		if (importDirectory.VirtualAddress == 0)
		{
			return false;
		}

		IMAGE_IMPORT_DESCRIPTOR* descriptor = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base + importDirectory.VirtualAddress);
		for (; descriptor->Name != 0; ++descriptor)
		{
			const char* importedDll = reinterpret_cast<const char*>(base + descriptor->Name);
			if (_stricmp(importedDll, dllName) != 0 || descriptor->OriginalFirstThunk == 0)
			{
				continue;
			}

			// nameThunk는 "무슨 함수인지"(이름 테이블), addressThunk는 "그 함수의 실제
			// 주소가 적힌 칸"(IAT)이며, 둘은 같은 순서로 나란히 놓여 있다.
			IMAGE_THUNK_DATA* nameThunk = reinterpret_cast<IMAGE_THUNK_DATA*>(base + descriptor->OriginalFirstThunk);
			IMAGE_THUNK_DATA* addressThunk = reinterpret_cast<IMAGE_THUNK_DATA*>(base + descriptor->FirstThunk);

			for (; nameThunk->u1.AddressOfData != 0; ++nameThunk, ++addressThunk)
			{
				if (IMAGE_SNAP_BY_ORDINAL(nameThunk->u1.Ordinal))
				{
					continue; // 이름이 아니라 번호로 임포트된 함수는 건너뜀.
				}

				IMAGE_IMPORT_BY_NAME* importByName = reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(base + nameThunk->u1.AddressOfData);
				if (strcmp(reinterpret_cast<const char*>(importByName->Name), functionName) != 0)
				{
					continue;
				}

				void** slot = reinterpret_cast<void**>(&addressThunk->u1.Function);

				DWORD oldProtect = 0;
				if (!VirtualProtect(slot, sizeof(void*), PAGE_READWRITE, &oldProtect))
				{
					return false;
				}

				*original = *slot;
				*slot = hook;

				VirtualProtect(slot, sizeof(void*), oldProtect, &oldProtect);
				return true;
			}
		}

		return false;
	}
}

bool DrawCallHook::Install()
{
	if (g_Installed)
	{
		return true;
	}

	HMODULE exe = GetModuleHandle(NULL);

	if (!PatchImport(exe, "opengl32.dll", "glDrawArrays",
		reinterpret_cast<void*>(&HookedDrawArrays), reinterpret_cast<void**>(&g_OriginalDrawArrays)))
	{
		std::cout << "[드로우콜] 후킹 실패: 실행 파일에서 opengl32.dll의 glDrawArrays 임포트를 찾지 못했다 (드로우 콜을 세지 않는다)\n";
		return false;
	}

	// glDrawElements는 지금은 안 쓰지만, 나중에 쓰게 되면 자동으로 세지도록 있을 때만 건다.
	PatchImport(exe, "opengl32.dll", "glDrawElements",
		reinterpret_cast<void*>(&HookedDrawElements), reinterpret_cast<void**>(&g_OriginalDrawElements));

	g_Installed = true;
	g_IntervalStart = Clock::now();

	std::cout << "[드로우콜] 후킹 완료: glDrawArrays 호출 때마다 카운트한다 ("
		<< kReportIntervalSeconds << "초마다 프레임당 드로우 콜 수를 출력)\n";
	return true;
}

void DrawCallHook::Uninstall()
{
	if (!g_Installed)
	{
		return;
	}

	HMODULE exe = GetModuleHandle(NULL);
	void* replaced = nullptr;

	PatchImport(exe, "opengl32.dll", "glDrawArrays", reinterpret_cast<void*>(g_OriginalDrawArrays), &replaced);
	if (g_OriginalDrawElements != nullptr)
	{
		PatchImport(exe, "opengl32.dll", "glDrawElements", reinterpret_cast<void*>(g_OriginalDrawElements), &replaced);
	}

	g_Installed = false;
}

void DrawCallHook::BeginFrame()
{
	g_FrameDrawCalls = 0;
}

void DrawCallHook::EndFrame(const char* note)
{
	if (!g_Installed)
	{
		return;
	}

	++g_FrameIndex;
	unsigned int drawCalls = g_FrameDrawCalls;

	++g_IntervalFrames;
	g_IntervalDrawCalls += drawCalls;
	if (drawCalls < g_IntervalMin) g_IntervalMin = drawCalls;
	if (drawCalls > g_IntervalMax) g_IntervalMax = drawCalls;

	Clock::time_point now = Clock::now();
	double elapsed = std::chrono::duration<double>(now - g_IntervalStart).count();
	if (elapsed < kReportIntervalSeconds)
	{
		return;
	}

	double averageDrawCalls = (double)g_IntervalDrawCalls / (double)g_IntervalFrames;
	double fps = (elapsed > 0.0) ? (double)g_IntervalFrames / elapsed : 0.0;

	char line[384];
	snprintf(line, sizeof(line),
		"[드로우콜] 프레임 #%llu: %u회 (최근 %.1f초 평균 %.1f회/프레임, 최소 %u, 최대 %u, %.0f FPS)%s%s\n",
		g_FrameIndex, drawCalls, elapsed, averageDrawCalls, g_IntervalMin, g_IntervalMax, fps,
		note ? " | " : "", note ? note : "");
	std::cout << line;

	g_IntervalStart = now;
	g_IntervalFrames = 0;
	g_IntervalDrawCalls = 0;
	g_IntervalMin = UINT_MAX;
	g_IntervalMax = 0;
}
