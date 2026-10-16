#include "stdafx.h"
#include "GameLog.h"

#include <deque>

namespace
{
	// 채팅창이 아직 꺼내가지 못한 메시지. 무한히 쌓이지 않도록 상한을 둔다.
	const size_t kMaxPending = 64;

	std::deque<GameLog::Message> g_Pending;

	std::function<void(GameLog::Kind, const std::string&)> g_Listener;
}

void GameLog::Add(Kind kind, const std::string& text)
{
	Message message;
	message.kind = kind;
	message.text = text;
	g_Pending.push_back(message);

	while (g_Pending.size() > kMaxPending)
	{
		g_Pending.pop_front();
	}

	if (g_Listener)
	{
		g_Listener(kind, text);
	}
}

void GameLog::SetListener(const std::function<void(Kind, const std::string&)>& listener)
{
	g_Listener = listener;
}

bool GameLog::Pop(Message& out)
{
	if (g_Pending.empty())
	{
		return false;
	}

	out = g_Pending.front();
	g_Pending.pop_front();
	return true;
}
