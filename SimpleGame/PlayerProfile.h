#pragma once

#include <string>

// 플레이어의 닉네임(게임을 시작할 때 이름 입력 화면에서 정함 — NameEntry.h). 머리 위 이름표·HUD·대화창 이름표·
// 이야기 변수 PLAYER_NAME·월드 모니터가 쓴다. 마지막으로 쓴 이름은 ./Cache/nickname.txt(UTF-8)에 남겨 두어
// 다음에 켤 때(다시 시작 포함) 입력 칸에 미리 채운다.
namespace PlayerProfile
{
	// 닉네임 글자 수 상한(한글·영문 모두 한 글자를 1로 셈).
	const int kMaxNicknameLength = 12;

	// 아직 정하지 않았으면 빈 문자열.
	const std::string& GetNickname();
	void SetNickname(const std::string& utf8);

	// 화면에 보일 이름: 닉네임, 없으면 "플레이어".
	std::string GetDisplayName();

	// 닉네임에 쓸 수 있는 글자인가: 영문·숫자·공백·_·-, 한글 음절(가~힣)과 한글 자모(ㄱ~ㅣ).
	bool IsAllowedCharacter(wchar_t c);

	// 앞뒤 공백을 지우고, 쓸 수 없는 글자를 빼고, 글자 수를 잘라 다듬는다(명령줄 --name 등 바깥에서 받은 이름).
	std::wstring Sanitize(const std::wstring& name);

	std::string ToUtf8(const std::wstring& text);
	std::wstring FromUtf8(const std::string& utf8);

	// 마지막으로 쓴 닉네임(없거나 못 읽으면 빈 문자열)과 저장.
	std::string LoadLastNickname();
	void SaveLastNickname(const std::string& utf8);
}
