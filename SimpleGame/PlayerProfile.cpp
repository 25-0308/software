#include "stdafx.h"
#include "PlayerProfile.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <fstream>
#include <iterator>

namespace
{
	const char* kNicknamePath = "./Cache/nickname.txt";

	std::string s_Nickname;
}

const std::string& PlayerProfile::GetNickname()
{
	return s_Nickname;
}

void PlayerProfile::SetNickname(const std::string& utf8)
{
	s_Nickname = utf8;
}

std::string PlayerProfile::GetDisplayName()
{
	return s_Nickname.empty() ? std::string("플레이어") : s_Nickname;
}

bool PlayerProfile::IsAllowedCharacter(wchar_t c)
{
	if ((c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z') || (c >= L'0' && c <= L'9'))
	{
		return true;
	}
	if (c == L' ' || c == L'_' || c == L'-')
	{
		return true;
	}

	// 한글 음절(가~힣)과 호환용 한글 자모(ㄱ~ㅣ — 입력기가 낱자로 끝낸 것).
	return (c >= 0xAC00 && c <= 0xD7A3) || (c >= 0x3131 && c <= 0x318E);
}

std::wstring PlayerProfile::Sanitize(const std::wstring& name)
{
	std::wstring result;
	for (wchar_t c : name)
	{
		if (!IsAllowedCharacter(c))
		{
			continue;
		}
		if (c == L' ' && (result.empty() || result.back() == L' '))
		{
			continue; // 앞 공백·겹친 공백
		}
		if ((int)result.size() >= kMaxNicknameLength)
		{
			break;
		}
		result.push_back(c);
	}

	while (!result.empty() && result.back() == L' ')
	{
		result.pop_back();
	}
	return result;
}

std::string PlayerProfile::ToUtf8(const std::wstring& text)
{
	if (text.empty())
	{
		return std::string();
	}

	int length = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), (int)text.size(), nullptr, 0, nullptr, nullptr);
	if (length <= 0)
	{
		return std::string();
	}

	std::string utf8((size_t)length, '\0');
	WideCharToMultiByte(CP_UTF8, 0, text.c_str(), (int)text.size(), &utf8[0], length, nullptr, nullptr);
	return utf8;
}

std::wstring PlayerProfile::FromUtf8(const std::string& utf8)
{
	if (utf8.empty())
	{
		return std::wstring();
	}

	int length = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), nullptr, 0);
	if (length <= 0)
	{
		return std::wstring();
	}

	std::wstring wide((size_t)length, L'\0');
	MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), &wide[0], length);
	return wide;
}

std::string PlayerProfile::LoadLastNickname()
{
	std::ifstream file(kNicknamePath, std::ios::binary);
	if (!file)
	{
		return std::string();
	}

	std::string text((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
	return ToUtf8(Sanitize(FromUtf8(text)));
}

void PlayerProfile::SaveLastNickname(const std::string& utf8)
{
	CreateDirectoryA("./Cache", nullptr); // 이미 있으면 그대로

	std::ofstream file(kNicknamePath, std::ios::binary | std::ios::trunc);
	if (file)
	{
		file << utf8;
	}
}
