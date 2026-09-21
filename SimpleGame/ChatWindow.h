#pragma once

#include <vector>

#include "GameLog.h"
#include "TextRasterizer.h"

class Renderer;

// 화면 왼쪽 아래에 뜨는 채팅창. GameLog에 쌓인 메시지(NPC 대사, 전투/보상/알림)를 매 프레임
// 꺼내서 한 줄(길면 여러 줄)씩 글자 텍스처로 만들어 아래에서부터 쌓아 보여주고, 각 메시지는
// 생긴 지 일정 시간(3초)이 지나면 사라진다. 사라지기 직전엔 서서히 투명해진다.
// 후처리 이후 HUD와 같은 화면 고정 좌표계(800x600)로 그린다.
class ChatWindow
{
public:
	ChatWindow();
	~ChatWindow();

	ChatWindow(const ChatWindow&) = delete;
	ChatWindow& operator=(const ChatWindow&) = delete;

	// 새 메시지를 받아 줄로 만들고, 시간이 지난 줄을 지운다. OpenGL 컨텍스트가 현재 상태여야 한다
	// (글자 텍스처를 만들고 지우기 때문).
	void Update(float deltaSeconds);

	void Draw(Renderer& renderer);

private:
	struct Line
	{
		TextTexture texture;
		float age = 0.f; // 채팅창에 나타난 뒤 지난 시간(초)
		float r = 1.f, g = 1.f, b = 1.f;
	};

	void AddLine(const GameLog::Message& message);

	TextRasterizer m_Rasterizer;
	std::vector<Line> m_Lines; // 오래된 줄이 앞
};
