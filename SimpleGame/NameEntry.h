#pragma once

#include <string>

#include "TextRasterizer.h"

class Renderer;

// 게임을 시작할 때 플레이어의 닉네임을 정하는 화면(한글·영문 모두). 월드를 다 지은 뒤 그 위를 어둡게 덮고 가운데
// 입력 칸을 띄우며, 정하는 동안 게임은 멈춰 있다(SimpleGame.cpp — 첫 대화도 그 뒤에서 기다림).
//
// 한글 입력: freeglut의 키보드 콜백은 글자를 1바이트(unsigned char)로만 넘겨서 한글이 깨지므로, 입력하는 동안만 게임 창의
// 창 프로시저를 가로채(서브클래싱) 글자(WM_CHAR, UTF-16)와 입력기(IME)의 조합 중인 글자·완성된 글자(WM_IME_COMPOSITION)를
// 직접 받는다. 조합 중인 글자는 입력 칸에 밑줄로 미리 보여 주고, 시스템의 조합 창은 띄우지 않는다. 다 정하면 창 프로시저를
// 되돌리고 게임 창의 입력기를 끈다 — 게임 중엔 한/영 상태와 상관없이 WASD가 영문으로 들어오게.
class NameEntryScreen
{
public:
	NameEntryScreen();
	~NameEntryScreen();

	NameEntryScreen(const NameEntryScreen&) = delete;
	NameEntryScreen& operator=(const NameEntryScreen&) = delete;

	// window는 게임 창(HWND). prefill(UTF-8)은 입력 칸에 미리 채울 이름(지난번 이름).
	void Begin(void* window, const std::string& prefill);
	bool IsActive() const { return m_Active; }

	// 이름을 확정했으면 한 번만 true와 이름(UTF-8)을 돌려주고 화면을 닫는다.
	bool TakeResult(std::string& outName);

	void Update(float deltaSeconds);
	void Draw(Renderer& renderer);

	// 게임 창의 입력기를 끈다(이름 입력이 없을 때 — 명령줄 --name으로 건너뛴 경우 등).
	static void DisableIme(void* window);

	// 가로챈 창 프로시저가 부른다(NameEntry.cpp). 이 화면이 처리했으면 true(그 메시지는 freeglut으로 넘기지 않음).
	// 인자는 창 메시지·WPARAM·LPARAM 그대로이고, 헤더에 windows.h를 끌어들이지 않으려고 정수 형으로 받는다.
	bool HandleMessage(unsigned int message, unsigned long long wParam, long long lParam);
	void* GetOldWindowProc() const { return m_OldWindowProc; }

private:
	void End();
	void AppendText(const std::wstring& text);
	void Backspace();
	void Confirm();
	void ShowError(const char* utf8);
	void ReleaseTexts();
	void BuildTexts();

	bool m_Active = false;
	void* m_Window = nullptr;
	void* m_OldWindowProc = nullptr;

	std::wstring m_Text;        // 확정된 글자
	std::wstring m_Composition; // 입력기가 조합 중인 글자(아직 확정 전)
	std::string m_Result;
	bool m_HasResult = false;

	std::string m_Error;
	float m_ErrorTimer = 0.f;
	float m_Time = 0.f;

	TextRasterizer m_TitleText;
	TextRasterizer m_BodyText;
	TextRasterizer m_InputText;
	TextRasterizer m_SmallText;

	bool m_TextsDirty = true;
	bool m_StaticTextsBuilt = false;
	TextTexture m_Title;
	TextTexture m_Prompt;
	TextTexture m_Hint;
	TextTexture m_Placeholder;
	TextTexture m_InputTexture;
	TextTexture m_CompositionTexture;
	TextTexture m_CountTexture;
	TextTexture m_ErrorTexture;
};
