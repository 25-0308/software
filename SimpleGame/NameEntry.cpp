#include "stdafx.h"
#include "NameEntry.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <imm.h>
#include <algorithm>
#include <cmath>

#include "PlayerProfile.h"
#include "Renderer.h"
#include "UiDraw.h"

#pragma comment(lib, "imm32.lib")

namespace
{
	// 패널(화면 가운데, UI 좌표).
	const float kPanelWidth = 480.f;
	const float kPanelHeight = 260.f;
	const float kBoxWidth = 400.f;
	const float kBoxHeight = 44.f;
	const float kBoxBottomOffset = 86.f; // 패널 아래쪽에서 입력 칸 아래쪽까지
	const float kTextPadding = 6.f;      // TextRasterizer가 글자 둘레에 두는 여백(UI 좌표)
	const float kErrorSeconds = 2.5f;

	NameEntryScreen* s_Active = nullptr;

	// 글자 텍스처에서 여백을 뺀, 글자가 실제로 차지하는 폭.
	float Advance(const TextTexture& texture)
	{
		return (texture.id != 0) ? (std::max)(0.f, (float)texture.width - kTextPadding * 2.f) : 0.f;
	}

	std::wstring ReadComposition(HIMC imc, DWORD index)
	{
		LONG bytes = ImmGetCompositionStringW(imc, index, nullptr, 0);
		if (bytes <= 0)
		{
			return std::wstring();
		}

		std::wstring text((size_t)bytes / sizeof(wchar_t), L'\0');
		ImmGetCompositionStringW(imc, index, &text[0], (DWORD)bytes);
		return text;
	}

	// 이름을 정하는 동안만 게임 창에 끼워 두는 창 프로시저. 입력기 메시지와 글자는 이 화면이 받고, 나머지는 freeglut으로.
	LRESULT CALLBACK NameEntryWindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
	{
		NameEntryScreen* screen = s_Active;
		if (screen == nullptr)
		{
			return DefWindowProcW(window, message, wParam, lParam);
		}

		WNDPROC oldProc = (WNDPROC)screen->GetOldWindowProc();
		if (message == WM_IME_SETCONTEXT)
		{
			// 시스템의 조합 창은 띄우지 않는다(조합 중인 글자는 입력 칸에 직접 그림).
			lParam &= ~ISC_SHOWUICOMPOSITIONWINDOW;
		}
		else if (screen->HandleMessage(message, (unsigned long long)wParam, (long long)lParam))
		{
			return 0;
		}
		return CallWindowProcW(oldProc, window, message, wParam, lParam);
	}
}

NameEntryScreen::NameEntryScreen()
	: m_TitleText(24, true)
	, m_BodyText(15)
	, m_InputText(20)
	, m_SmallText(13)
{
}

NameEntryScreen::~NameEntryScreen()
{
	if (m_Active)
	{
		End();
	}
	ReleaseTexts();
}

void NameEntryScreen::Begin(void* window, const std::string& prefill)
{
	m_Window = window;
	m_Text = PlayerProfile::Sanitize(PlayerProfile::FromUtf8(prefill));
	m_Composition.clear();
	m_HasResult = false;
	m_Error.clear();
	m_ErrorTimer = 0.f;
	m_Time = 0.f;
	m_TextsDirty = true;
	m_Active = true;

	// 창을 못 얻었으면 입력을 받을 수 없으니 지난번 이름(없으면 이름 없이)으로 곧바로 넘어간다.
	HWND hwnd = (HWND)window;
	if (hwnd == nullptr)
	{
		m_Result = PlayerProfile::ToUtf8(m_Text);
		m_HasResult = true;
		return;
	}

	// 입력기를 켜 두고(게임 중엔 꺼 둠) 창 프로시저를 가로챈다.
	ImmAssociateContextEx(hwnd, nullptr, IACE_DEFAULT);
	s_Active = this;
	m_OldWindowProc = (void*)SetWindowLongPtrW(hwnd, GWLP_WNDPROC, (LONG_PTR)NameEntryWindowProc);
}

void NameEntryScreen::End()
{
	HWND hwnd = (HWND)m_Window;
	if (hwnd != nullptr && m_OldWindowProc != nullptr)
	{
		SetWindowLongPtrW(hwnd, GWLP_WNDPROC, (LONG_PTR)m_OldWindowProc);
		DisableIme(hwnd);
	}
	m_OldWindowProc = nullptr;
	s_Active = nullptr;
	m_Active = false;
	ReleaseTexts();
}

void NameEntryScreen::DisableIme(void* window)
{
	if (window != nullptr)
	{
		ImmAssociateContextEx((HWND)window, nullptr, 0);
	}
}

bool NameEntryScreen::TakeResult(std::string& outName)
{
	if (!m_HasResult)
	{
		return false;
	}

	outName = m_Result;
	m_HasResult = false;
	End();
	return true;
}

bool NameEntryScreen::HandleMessage(unsigned int message, unsigned long long wParam, long long lParam)
{
	HWND hwnd = (HWND)m_Window;

	switch (message)
	{
	case WM_IME_STARTCOMPOSITION:
		return true;

	case WM_IME_ENDCOMPOSITION:
		m_Composition.clear();
		m_TextsDirty = true;
		return true;

	case WM_IME_COMPOSITION:
		{
			// 한 메시지에 "앞 글자 완성 + 다음 글자 조합 시작"이 함께 올 수 있어서 둘 다 본다.
			HIMC imc = ImmGetContext(hwnd);
			if (imc != nullptr)
			{
				if (lParam & GCS_RESULTSTR)
				{
					AppendText(ReadComposition(imc, GCS_RESULTSTR));
				}

				if (lParam & GCS_COMPSTR)
				{
					m_Composition = ReadComposition(imc, GCS_COMPSTR);
				}
				else if (lParam & GCS_RESULTSTR)
				{
					m_Composition.clear();
				}
				ImmReleaseContext(hwnd, imc);
			}
			m_TextsDirty = true;
			return true;
		}

	case WM_IME_CHAR:
		return true; // 완성된 글자는 위에서 이미 받았다

	case WM_CHAR:
		{
			wchar_t c = (wchar_t)wParam;
			if (c == L'\b')
			{
				Backspace();
			}
			else if (c == L'\r')
			{
				Confirm();
			}
			else if (c >= 32)
			{
				AppendText(std::wstring(1, c));
			}
			return true;
		}

	default:
		return false;
	}
}

void NameEntryScreen::AppendText(const std::wstring& text)
{
	for (wchar_t c : text)
	{
		if (!PlayerProfile::IsAllowedCharacter(c))
		{
			ShowError("한글·영문·숫자와 공백·_·- 만 쓸 수 있어요.");
			continue;
		}
		if (c == L' ' && (m_Text.empty() || m_Text.back() == L' '))
		{
			continue;
		}
		if ((int)m_Text.size() >= PlayerProfile::kMaxNicknameLength)
		{
			ShowError("이름은 12자까지 쓸 수 있어요.");
			break;
		}
		m_Text.push_back(c);
	}
	m_TextsDirty = true;
}

void NameEntryScreen::Backspace()
{
	if (!m_Text.empty())
	{
		m_Text.pop_back();
		m_TextsDirty = true;
	}
}

void NameEntryScreen::Confirm()
{
	// 조합 중인 글자가 남아 있으면 그것까지 넣고 입력기의 조합은 버린다(같은 글자가 한 번 더 들어오지 않게).
	if (!m_Composition.empty())
	{
		AppendText(m_Composition);
		m_Composition.clear();

		HWND hwnd = (HWND)m_Window;
		HIMC imc = ImmGetContext(hwnd);
		if (imc != nullptr)
		{
			ImmNotifyIME(imc, NI_COMPOSITIONSTR, CPS_CANCEL, 0);
			ImmReleaseContext(hwnd, imc);
		}
	}

	std::wstring name = PlayerProfile::Sanitize(m_Text);
	if (name.empty())
	{
		ShowError("이름을 한 글자 이상 입력해 주세요.");
		return;
	}

	m_Result = PlayerProfile::ToUtf8(name);
	m_HasResult = true;
}

void NameEntryScreen::ShowError(const char* utf8)
{
	m_Error = utf8;
	m_ErrorTimer = kErrorSeconds;
	m_TextsDirty = true;
}

void NameEntryScreen::Update(float deltaSeconds)
{
	m_Time += deltaSeconds;

	if (m_ErrorTimer > 0.f)
	{
		m_ErrorTimer -= deltaSeconds;
		if (m_ErrorTimer <= 0.f)
		{
			m_Error.clear();
			m_TextsDirty = true;
		}
	}
}

void NameEntryScreen::ReleaseTexts()
{
	TextRasterizer::Destroy(m_Title);
	TextRasterizer::Destroy(m_Prompt);
	TextRasterizer::Destroy(m_Hint);
	TextRasterizer::Destroy(m_Placeholder);
	TextRasterizer::Destroy(m_InputTexture);
	TextRasterizer::Destroy(m_CompositionTexture);
	TextRasterizer::Destroy(m_CountTexture);
	TextRasterizer::Destroy(m_ErrorTexture);
	m_StaticTextsBuilt = false;
	m_TextsDirty = true;
}

void NameEntryScreen::BuildTexts()
{
	if (!m_StaticTextsBuilt)
	{
		m_Title = m_TitleText.Create("당신의 이름은?", 400);
		m_Prompt = m_BodyText.Create("델포이 사람들이 당신을 부를 이름을 정해 주세요.", 420);
		m_Hint = m_SmallText.Create("한글·영문 모두 가능 · 최대 12자 · 한/영 키로 전환 · Enter 확정", 440);
		m_Placeholder = m_InputText.Create("이름 입력", 300);
		m_StaticTextsBuilt = true;
	}

	if (!m_TextsDirty)
	{
		return;
	}
	m_TextsDirty = false;

	TextRasterizer::Destroy(m_InputTexture);
	TextRasterizer::Destroy(m_CompositionTexture);
	TextRasterizer::Destroy(m_CountTexture);
	TextRasterizer::Destroy(m_ErrorTexture);

	if (!m_Text.empty())
	{
		m_InputTexture = m_InputText.Create(PlayerProfile::ToUtf8(m_Text), 600);
	}
	if (!m_Composition.empty())
	{
		m_CompositionTexture = m_InputText.Create(PlayerProfile::ToUtf8(m_Composition), 300);
	}

	int count = (int)m_Text.size() + (int)m_Composition.size();
	m_CountTexture = m_SmallText.Create(std::to_string(count) + "/" + std::to_string(PlayerProfile::kMaxNicknameLength), 80);

	if (!m_Error.empty())
	{
		m_ErrorTexture = m_SmallText.Create(m_Error, 420);
	}
}

void NameEntryScreen::Draw(Renderer& renderer)
{
	if (!m_Active)
	{
		return;
	}

	BuildTexts();

	Mat4 ui = UiDraw::ScreenProjection();
	float centerX = UiDraw::kScreenWidth * 0.5f;
	float panelLeft = floorf(centerX - kPanelWidth * 0.5f);
	float panelBottom = floorf(UiDraw::kScreenHeight * 0.5f - kPanelHeight * 0.5f);

	// 화면을 어둡게 덮고 가운데에 금빛 테두리 패널(대화창과 같은 색).
	UiDraw::Rect(renderer, ui, 0.f, 0.f, UiDraw::kScreenWidth, UiDraw::kScreenHeight, 0.f, 0.01f, 0.03f, 0.62f);
	UiDraw::Rect(renderer, ui, panelLeft - 2.f, panelBottom - 2.f, kPanelWidth + 4.f, kPanelHeight + 4.f, 0.72f, 0.58f, 0.3f, 0.95f);
	UiDraw::Rect(renderer, ui, panelLeft, panelBottom, kPanelWidth, kPanelHeight, 0.04f, 0.04f, 0.07f, 0.96f);

	// 제목 + 금빛 줄 + 안내 문구.
	float titleTop = panelBottom + kPanelHeight - 18.f;
	UiDraw::TextTopLeft(renderer, ui, m_Title, floorf(centerX - (float)m_Title.width * 0.5f), titleTop, 1.f, 0.88f, 0.55f, 1.f);
	float lineY = titleTop - (float)m_Title.height - 6.f;
	UiDraw::Rect(renderer, ui, floorf(centerX - 120.f), lineY, 240.f, 2.f, 0.72f, 0.58f, 0.3f, 0.8f);
	UiDraw::TextTopLeft(renderer, ui, m_Prompt, floorf(centerX - (float)m_Prompt.width * 0.5f), lineY - 8.f, 0.82f, 0.85f, 0.92f, 1.f);

	// 입력 칸: 밝은 테두리가 숨 쉬듯 빛난다.
	float boxLeft = floorf(centerX - kBoxWidth * 0.5f);
	float boxBottom = panelBottom + kBoxBottomOffset;
	float glow = 0.75f + 0.25f * sinf(m_Time * 3.f);
	UiDraw::Rect(renderer, ui, boxLeft - 2.f, boxBottom - 2.f, kBoxWidth + 4.f, kBoxHeight + 4.f, 1.f * glow, 0.82f * glow, 0.4f * glow, 1.f);
	UiDraw::Rect(renderer, ui, boxLeft, boxBottom, kBoxWidth, kBoxHeight, 0.1f, 0.09f, 0.12f, 1.f);

	float textLeft = boxLeft + 14.f;
	float centerY = boxBottom + kBoxHeight * 0.5f;
	float inputWidth = Advance(m_InputTexture);
	float compositionWidth = Advance(m_CompositionTexture);

	if (m_InputTexture.id == 0 && m_CompositionTexture.id == 0)
	{
		UiDraw::TextTopLeft(renderer, ui, m_Placeholder, textLeft - kTextPadding, floorf(centerY + (float)m_Placeholder.height * 0.5f),
			0.55f, 0.55f, 0.6f, 0.9f);
	}
	if (m_InputTexture.id != 0)
	{
		UiDraw::TextTopLeft(renderer, ui, m_InputTexture, textLeft - kTextPadding, floorf(centerY + (float)m_InputTexture.height * 0.5f),
			1.f, 0.96f, 0.88f, 1.f);
	}
	if (m_CompositionTexture.id != 0)
	{
		// 조합 중인 글자는 밑줄로 구분한다.
		float compositionLeft = textLeft + inputWidth;
		UiDraw::TextTopLeft(renderer, ui, m_CompositionTexture, compositionLeft - kTextPadding,
			floorf(centerY + (float)m_CompositionTexture.height * 0.5f), 1.f, 0.9f, 0.6f, 1.f);
		UiDraw::Rect(renderer, ui, compositionLeft, boxBottom + 8.f, compositionWidth, 2.f, 1.f, 0.85f, 0.45f, 1.f);
	}

	// 글자 넣는 자리(깜빡임).
	if (fmodf(m_Time, 1.f) < 0.6f)
	{
		UiDraw::Rect(renderer, ui, textLeft + inputWidth + compositionWidth + 1.f, boxBottom + 10.f, 2.f, kBoxHeight - 20.f,
			1.f, 0.92f, 0.7f, 1.f);
	}

	// 글자 수(입력 칸 오른쪽 안쪽).
	UiDraw::TextTopLeft(renderer, ui, m_CountTexture, boxLeft + kBoxWidth - 8.f - (float)m_CountTexture.width,
		floorf(centerY + (float)m_CountTexture.height * 0.5f), 0.6f, 0.62f, 0.68f, 1.f);

	// 잘못 넣었을 때의 안내(빨강) — 입력 칸 바로 아래.
	if (m_ErrorTexture.id != 0)
	{
		float alpha = (std::min)(1.f, m_ErrorTimer / 0.4f);
		UiDraw::TextTopLeft(renderer, ui, m_ErrorTexture, floorf(centerX - (float)m_ErrorTexture.width * 0.5f), boxBottom - 6.f,
			1.f, 0.45f, 0.4f, alpha);
	}

	// 조작 안내.
	UiDraw::TextTopLeft(renderer, ui, m_Hint, floorf(centerX - (float)m_Hint.width * 0.5f), panelBottom + 14.f + (float)m_Hint.height,
		0.7f, 0.72f, 0.78f, 0.9f);
}
