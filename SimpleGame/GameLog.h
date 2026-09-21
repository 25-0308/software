#pragma once

#include <string>

// 게임 중에 생기는 메시지(NPC 대화, 전투/보상/알림 로그)를 모아두는 대기열.
// 액터와 게임 코드는 화면에 어떻게 그려지는지 몰라도 여기에 넣기만 하면 되고, 화면 왼쪽 아래
// 채팅창(ChatWindow)이 매 프레임 꺼내서 보여준다. 콘솔에는 출력하지 않는다.
namespace GameLog
{
	// 메시지 종류. 채팅창에서 색으로 구분해서 보여준다.
	enum class Kind
	{
		Dialogue, // NPC 대화
		Info,     // 안내/알림(상호작용, 조작 안내 등)
		Combat,   // 내가 하는 공격
		Damage,   // 내가 입은 피해
		Reward,   // 경험치/레벨업 같은 보상
	};

	struct Message
	{
		Kind kind = Kind::Info;
		std::string text; // UTF-8
	};

	// 메시지를 대기열 뒤에 넣는다. text는 UTF-8이어야 한다.
	void Add(Kind kind, const std::string& text);

	// 가장 오래 기다린 메시지를 꺼낸다. 대기 중인 게 없으면 false.
	bool Pop(Message& out);
}
