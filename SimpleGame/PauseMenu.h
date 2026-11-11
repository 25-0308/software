#pragma once

#include "TextRasterizer.h"

class Renderer;

// 메뉴에서 고른 일. 게임 쪽(SimpleGame.cpp)이 받아서 처리한다.
enum class PauseAction
{
	None,
	Resume,  // 메뉴를 닫고 이어서 한다
	Restart, // 처음부터 다시 시작(진행은 저장되지 않음)
	Quit,    // 게임을 끝낸다
};

// Esc로 여는 일시 정지 메뉴: 계속하기 / 다시 시작 / 게임 종료. 다시 시작과 종료는 진행이 저장되지 않으므로 한 번 더
// 묻는다. 열려 있는 동안 게임은 멈추고(SimpleGame.cpp), 모든 UI 위에 화면 고정 좌표계(UiDraw)로 그린다.
// 조작: ↑↓·W/S로 고르고 Enter·Space·E로 정함, 숫자키로 바로 고름, 마우스로 가리키고 누름, Esc는 뒤로(첫 화면이면 닫기).
class PauseMenu
{
public:
	PauseMenu();
	~PauseMenu();

	PauseMenu(const PauseMenu&) = delete;
	PauseMenu& operator=(const PauseMenu&) = delete;

	void Open();
	void Close();
	bool IsOpen() const { return m_Open; }

	// 일반 키(소문자로 맞춘 것)·특수 키(GLUT_KEY_UP 등)·마우스(UI 좌표, click이 false면 가리키기만).
	PauseAction HandleKey(unsigned char key);
	PauseAction HandleSpecialKey(int key);
	PauseAction HandleMouse(float uiX, float uiY, bool click);

	void Update(float deltaSeconds);
	void Draw(Renderer& renderer);

private:
	enum class Page
	{
		Main,
		ConfirmRestart,
		ConfirmQuit,
	};

	int GetItemCount() const;
	PauseAction Activate(int index);
	void Move(int step);
	void SetPage(Page page);
	void ReleaseTexts();
	void BuildTexts();

	// 항목 index의 화면 자리(UI 좌표, 왼쪽 아래 + 크기).
	void GetItemRect(int index, float& left, float& bottom, float& width, float& height) const;

	static const int kMaxItems = 3;

	bool m_Open = false;
	Page m_Page = Page::Main;
	int m_Selected = 0;
	float m_Time = 0.f;

	TextRasterizer m_TitleText;
	TextRasterizer m_ItemText;
	TextRasterizer m_SmallText;

	bool m_TextsBuilt = false;
	TextTexture m_Title;
	TextTexture m_Message;
	TextTexture m_Hint;
	TextTexture m_Items[kMaxItems];
};
