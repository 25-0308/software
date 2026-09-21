#include "stdafx.h"
#include "ChatWindow.h"

#include "Renderer.h"

namespace
{
	const float kLifetimeSeconds = 3.f; // 채팅창에 나타난 뒤 이 시간이 지나면 사라진다
	const float kFadeSeconds = 0.5f;    // 사라지기 직전 이 시간 동안 서서히 투명해진다

	const size_t kMaxLines = 8;         // 한꺼번에 보이는 최대 줄 수(넘으면 오래된 것부터 지움)
	const int kFontPixelHeight = 16;
	const int kMaxLineWidth = 420;      // 한 줄 최대 너비(픽셀). 넘으면 줄바꿈한다

	const float kScreenWidth = 800.f;
	const float kScreenHeight = 600.f;
	const float kLeftMargin = 12.f;
	const float kBottomMargin = 12.f;
	const float kLineGap = 2.f;         // 줄 사이 간격(픽셀)
	const float kBackgroundAlpha = 0.45f;

	// 메시지 종류별 글자 색.
	void GetKindColor(GameLog::Kind kind, float& r, float& g, float& b)
	{
		switch (kind)
		{
		case GameLog::Kind::Dialogue: r = 1.0f;  g = 0.93f; b = 0.62f; break; // NPC 대사: 따뜻한 노랑
		case GameLog::Kind::Combat:   r = 0.95f; g = 0.95f; b = 0.95f; break; // 내 공격: 흰색
		case GameLog::Kind::Damage:   r = 1.0f;  g = 0.48f; b = 0.42f; break; // 받은 피해: 붉은색
		case GameLog::Kind::Reward:   r = 0.98f; g = 0.86f; b = 0.35f; break; // 경험치/레벨업: 금색
		default:                      r = 0.82f; g = 0.92f; b = 1.0f;  break; // 안내: 옅은 하늘색
		}
	}
}

ChatWindow::ChatWindow()
	: m_Rasterizer(kFontPixelHeight)
{
}

ChatWindow::~ChatWindow()
{
	for (Line& line : m_Lines)
	{
		TextRasterizer::Destroy(line.texture);
	}
}

void ChatWindow::AddLine(const GameLog::Message& message)
{
	Line line;
	line.texture = m_Rasterizer.Create(message.text, kMaxLineWidth);
	if (line.texture.id == 0)
	{
		return; // 글자를 그리지 못했으면 건너뛴다.
	}

	GetKindColor(message.kind, line.r, line.g, line.b);
	m_Lines.push_back(line);

	while (m_Lines.size() > kMaxLines)
	{
		TextRasterizer::Destroy(m_Lines.front().texture);
		m_Lines.erase(m_Lines.begin());
	}
}

void ChatWindow::Update(float deltaSeconds)
{
	GameLog::Message message;
	while (GameLog::Pop(message))
	{
		AddLine(message);
	}

	for (Line& line : m_Lines)
	{
		line.age += deltaSeconds;
	}

	// 시간이 다 된 줄을 지운다(글자 텍스처도 함께 해제).
	for (size_t i = 0; i < m_Lines.size();)
	{
		if (m_Lines[i].age >= kLifetimeSeconds)
		{
			TextRasterizer::Destroy(m_Lines[i].texture);
			m_Lines.erase(m_Lines.begin() + i);
		}
		else
		{
			++i;
		}
	}
}

void ChatWindow::Draw(Renderer& renderer)
{
	if (m_Lines.empty())
	{
		return;
	}

	Mat4 uiProjection = Mat4::Ortho(0.f, kScreenWidth, 0.f, kScreenHeight, -1.f, 1.f);

	// 가장 최근 줄이 맨 아래에 오고, 오래된 줄이 그 위로 쌓인다.
	float y = kBottomMargin;
	for (size_t i = m_Lines.size(); i-- > 0;)
	{
		const Line& line = m_Lines[i];

		float remaining = kLifetimeSeconds - line.age;
		float fade = (remaining < kFadeSeconds) ? remaining / kFadeSeconds : 1.f;
		if (fade < 0.f)
		{
			fade = 0.f;
		}

		// 글자 텍스처와 배경을 같은 사각형(텍스처 크기 그대로, 정수 픽셀 위치)에 겹쳐 그린다.
		float width = (float)line.texture.width;
		float height = (float)line.texture.height;
		Mat4 model = Mat4::Translate(kLeftMargin + width * 0.5f, y + height * 0.5f, 0.f) * Mat4::Scale(width, height, 1.f);
		Mat4 mvp = uiProjection * model;

		renderer.DrawObject(mvp, 0.f, 0.f, 0.f, kBackgroundAlpha * fade);
		renderer.DrawTexture(line.texture.id, mvp, line.r, line.g, line.b, fade);

		y += height + kLineGap;
	}
}
