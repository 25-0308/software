#include "stdafx.h"
#include "PauseMenu.h"

#include <cmath>

#include "Renderer.h"
#include "UiDraw.h"
#include "Dependencies\freeglut.h"

namespace
{
	// 패널(화면 가운데, UI 좌표).
	const float kPanelWidth = 360.f;
	const float kPanelHeight = 300.f;
	const float kItemWidth = 260.f;
	const float kItemHeight = 40.f;
	const float kItemGap = 12.f;
	const float kItemsTop = 196.f; // 패널 아래쪽에서 첫 항목 윗변까지

	const unsigned char kEscapeKey = 27;
	const unsigned char kEnterKey = 13;

	float PanelLeft()
	{
		return floorf(UiDraw::kScreenWidth * 0.5f - kPanelWidth * 0.5f);
	}

	float PanelBottom()
	{
		return floorf(UiDraw::kScreenHeight * 0.5f - kPanelHeight * 0.5f);
	}
}

PauseMenu::PauseMenu()
	: m_TitleText(24, true)
	, m_ItemText(18)
	, m_SmallText(13)
{
}

PauseMenu::~PauseMenu()
{
	ReleaseTexts();
}

void PauseMenu::Open()
{
	m_Open = true;
	m_Time = 0.f;
	SetPage(Page::Main);
}

void PauseMenu::Close()
{
	m_Open = false;
	ReleaseTexts();
}

int PauseMenu::GetItemCount() const
{
	return (m_Page == Page::Main) ? 3 : 2;
}

void PauseMenu::SetPage(Page page)
{
	m_Page = page;
	m_Selected = (page == Page::Main) ? 0 : 1; // 확인 화면은 "취소"에서 시작(실수로 두 번 눌러도 안전하게)
	ReleaseTexts();
}

void PauseMenu::Move(int step)
{
	int count = GetItemCount();
	m_Selected = (m_Selected + step + count) % count;
}

PauseAction PauseMenu::Activate(int index)
{
	switch (m_Page)
	{
	case Page::Main:
		if (index == 0)
		{
			return PauseAction::Resume;
		}
		SetPage((index == 1) ? Page::ConfirmRestart : Page::ConfirmQuit);
		return PauseAction::None;

	case Page::ConfirmRestart:
		if (index == 0)
		{
			return PauseAction::Restart;
		}
		SetPage(Page::Main);
		m_Selected = 1;
		return PauseAction::None;

	case Page::ConfirmQuit:
		if (index == 0)
		{
			return PauseAction::Quit;
		}
		SetPage(Page::Main);
		m_Selected = 2;
		return PauseAction::None;
	}
	return PauseAction::None;
}

PauseAction PauseMenu::HandleKey(unsigned char key)
{
	if (!m_Open)
	{
		return PauseAction::None;
	}

	// Esc: 확인 화면이면 첫 화면으로, 첫 화면이면 닫고 이어서 한다.
	if (key == kEscapeKey)
	{
		if (m_Page == Page::Main)
		{
			return PauseAction::Resume;
		}
		int back = (m_Page == Page::ConfirmRestart) ? 1 : 2;
		SetPage(Page::Main);
		m_Selected = back;
		return PauseAction::None;
	}

	if (key >= '1' && key < '1' + GetItemCount())
	{
		return Activate(key - '1');
	}

	switch (key)
	{
	case 'w': Move(-1); break;
	case 's': Move(1); break;
	case kEnterKey:
	case ' ':
	case 'e':
		return Activate(m_Selected);
	default: break;
	}
	return PauseAction::None;
}

PauseAction PauseMenu::HandleSpecialKey(int key)
{
	if (!m_Open)
	{
		return PauseAction::None;
	}

	if (key == GLUT_KEY_UP)
	{
		Move(-1);
	}
	else if (key == GLUT_KEY_DOWN)
	{
		Move(1);
	}
	return PauseAction::None;
}

PauseAction PauseMenu::HandleMouse(float uiX, float uiY, bool click)
{
	if (!m_Open)
	{
		return PauseAction::None;
	}

	for (int i = 0; i < GetItemCount(); ++i)
	{
		float left, bottom, width, height;
		GetItemRect(i, left, bottom, width, height);
		if (uiX >= left && uiX <= left + width && uiY >= bottom && uiY <= bottom + height)
		{
			m_Selected = i;
			return click ? Activate(i) : PauseAction::None;
		}
	}
	return PauseAction::None;
}

void PauseMenu::Update(float deltaSeconds)
{
	m_Time += deltaSeconds;
}

void PauseMenu::GetItemRect(int index, float& left, float& bottom, float& width, float& height) const
{
	width = kItemWidth;
	height = kItemHeight;
	left = floorf(UiDraw::kScreenWidth * 0.5f - kItemWidth * 0.5f);

	// 확인 화면은 안내 문구 자리만큼 항목을 아래로 내린다.
	float top = PanelBottom() + kItemsTop - ((m_Page == Page::Main) ? 0.f : 44.f);
	bottom = top - kItemHeight - (float)index * (kItemHeight + kItemGap);
}

void PauseMenu::BuildTexts()
{
	if (m_TextsBuilt)
	{
		return;
	}
	m_TextsBuilt = true;

	const char* items[kMaxItems] = { nullptr, nullptr, nullptr };
	switch (m_Page)
	{
	case Page::Main:
		m_Title = m_TitleText.Create("일시 정지", 300);
		items[0] = "1   계속하기";
		items[1] = "2   다시 시작";
		items[2] = "3   게임 종료";
		break;

	case Page::ConfirmRestart:
		m_Title = m_TitleText.Create("다시 시작", 300);
		m_Message = m_SmallText.Create("처음부터 다시 시작할까요? 지금까지의 진행은 저장되지 않습니다.", 300);
		items[0] = "1   다시 시작";
		items[1] = "2   취소";
		break;

	case Page::ConfirmQuit:
		m_Title = m_TitleText.Create("게임 종료", 300);
		m_Message = m_SmallText.Create("게임을 끝낼까요? 지금까지의 진행은 저장되지 않습니다.", 300);
		items[0] = "1   종료";
		items[1] = "2   취소";
		break;
	}

	for (int i = 0; i < kMaxItems; ++i)
	{
		if (items[i] != nullptr)
		{
			m_Items[i] = m_ItemText.Create(items[i], (int)kItemWidth);
		}
	}

	m_Hint = m_SmallText.Create("↑↓ 고르기 · Enter 확인 · Esc 돌아가기", 340);
}

void PauseMenu::ReleaseTexts()
{
	TextRasterizer::Destroy(m_Title);
	TextRasterizer::Destroy(m_Message);
	TextRasterizer::Destroy(m_Hint);
	for (int i = 0; i < kMaxItems; ++i)
	{
		TextRasterizer::Destroy(m_Items[i]);
	}
	m_TextsBuilt = false;
}

void PauseMenu::Draw(Renderer& renderer)
{
	if (!m_Open)
	{
		return;
	}

	BuildTexts();

	Mat4 ui = UiDraw::ScreenProjection();
	float centerX = UiDraw::kScreenWidth * 0.5f;
	float panelLeft = PanelLeft();
	float panelBottom = PanelBottom();

	// 화면 전체를 어둡게 덮고, 가운데에 금빛 테두리 패널(대화창과 같은 색).
	UiDraw::Rect(renderer, ui, 0.f, 0.f, UiDraw::kScreenWidth, UiDraw::kScreenHeight, 0.f, 0.01f, 0.03f, 0.6f);
	UiDraw::Rect(renderer, ui, panelLeft - 2.f, panelBottom - 2.f, kPanelWidth + 4.f, kPanelHeight + 4.f, 0.72f, 0.58f, 0.3f, 0.95f);
	UiDraw::Rect(renderer, ui, panelLeft, panelBottom, kPanelWidth, kPanelHeight, 0.04f, 0.04f, 0.07f, 0.96f);

	// 제목 + 아래 금빛 줄.
	float titleTop = panelBottom + kPanelHeight - 20.f;
	UiDraw::TextTopLeft(renderer, ui, m_Title, floorf(centerX - (float)m_Title.width * 0.5f), titleTop, 1.f, 0.88f, 0.55f, 1.f);
	float lineY = titleTop - (float)m_Title.height - 6.f;
	UiDraw::Rect(renderer, ui, floorf(centerX - 110.f), lineY, 220.f, 2.f, 0.72f, 0.58f, 0.3f, 0.8f);

	// 확인 화면의 안내 문구.
	if (m_Message.id != 0)
	{
		UiDraw::TextTopLeft(renderer, ui, m_Message, floorf(centerX - (float)m_Message.width * 0.5f), lineY - 10.f,
			0.82f, 0.85f, 0.92f, 1.f);
	}

	// 항목: 고른 것은 금빛 바탕·띠가 숨 쉬듯 빛난다.
	float pulse = 0.75f + 0.25f * sinf(m_Time * 4.f);
	for (int i = 0; i < GetItemCount(); ++i)
	{
		float left, bottom, width, height;
		GetItemRect(i, left, bottom, width, height);

		const TextTexture& text = m_Items[i];
		float textLeft = left + 22.f;
		float textTop = floorf(bottom + height * 0.5f + (float)text.height * 0.5f);

		if (i == m_Selected)
		{
			UiDraw::Rect(renderer, ui, left - 2.f, bottom - 2.f, width + 4.f, height + 4.f, 1.f, 0.8f, 0.3f, 0.9f * pulse);
			UiDraw::Rect(renderer, ui, left, bottom, width, height, 0.2f, 0.14f, 0.05f, 0.98f);
			UiDraw::Rect(renderer, ui, left, bottom, 6.f, height, 1.f, 0.8f, 0.3f, 1.f);
			UiDraw::TextTopLeft(renderer, ui, text, textLeft, textTop, 1.f, 0.9f, 0.55f, 1.f);
		}
		else
		{
			UiDraw::Rect(renderer, ui, left, bottom, width, height, 0.08f, 0.07f, 0.11f, 0.95f);
			UiDraw::Rect(renderer, ui, left, bottom, 3.f, height, 0.6f, 0.48f, 0.26f, 1.f);
			UiDraw::TextTopLeft(renderer, ui, text, textLeft, textTop, 0.9f, 0.88f, 0.82f, 1.f);
		}
	}

	// 아래쪽 조작 안내.
	UiDraw::TextTopLeft(renderer, ui, m_Hint, floorf(centerX - (float)m_Hint.width * 0.5f), panelBottom + 12.f + (float)m_Hint.height,
		0.7f, 0.72f, 0.78f, 0.9f);
}
