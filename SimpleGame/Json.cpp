#include "stdafx.h"
#include "Json.h"

#include <cstdlib>
#include <cstring>

namespace
{
	const JsonValue& NullValue()
	{
		static const JsonValue s_Null;
		return s_Null;
	}

	const std::string& EmptyString()
	{
		static const std::string s_Empty;
		return s_Empty;
	}

	// 유니코드 코드포인트를 UTF-8 바이트로 덧붙인다.
	void AppendUtf8(std::string& out, unsigned int codepoint)
	{
		if (codepoint < 0x80)
		{
			out.push_back((char)codepoint);
		}
		else if (codepoint < 0x800)
		{
			out.push_back((char)(0xC0 | (codepoint >> 6)));
			out.push_back((char)(0x80 | (codepoint & 0x3F)));
		}
		else if (codepoint < 0x10000)
		{
			out.push_back((char)(0xE0 | (codepoint >> 12)));
			out.push_back((char)(0x80 | ((codepoint >> 6) & 0x3F)));
			out.push_back((char)(0x80 | (codepoint & 0x3F)));
		}
		else
		{
			out.push_back((char)(0xF0 | (codepoint >> 18)));
			out.push_back((char)(0x80 | ((codepoint >> 12) & 0x3F)));
			out.push_back((char)(0x80 | ((codepoint >> 6) & 0x3F)));
			out.push_back((char)(0x80 | (codepoint & 0x3F)));
		}
	}
}

// 재귀 하강 파서. JsonValue의 내부를 채워야 해서 friend로 둔다.
class JsonParser
{
public:
	JsonParser(const char* text, size_t length)
		: m_Text(text)
		, m_End(text + length)
		, m_Cursor(text)
	{
	}

	bool ParseDocument(JsonValue& out, std::string& outError)
	{
		SkipSpace();
		if (!ParseValue(out, 0))
		{
			outError = m_Error + " (" + std::to_string((long long)(m_Cursor - m_Text)) + "번째 바이트)";
			return false;
		}
		return true;
	}

private:
	static const int kMaxDepth = 256;

	void SkipSpace()
	{
		while (m_Cursor < m_End && (*m_Cursor == ' ' || *m_Cursor == '\t' || *m_Cursor == '\n' || *m_Cursor == '\r'))
		{
			++m_Cursor;
		}
	}

	bool Fail(const char* reason)
	{
		m_Error = reason;
		return false;
	}

	bool Expect(const char* word)
	{
		size_t length = strlen(word);
		if ((size_t)(m_End - m_Cursor) < length || strncmp(m_Cursor, word, length) != 0)
		{
			return Fail("알 수 없는 낱말");
		}
		m_Cursor += length;
		return true;
	}

	bool ParseValue(JsonValue& out, int depth)
	{
		if (depth > kMaxDepth)
		{
			return Fail("너무 깊게 중첩됨");
		}
		if (m_Cursor >= m_End)
		{
			return Fail("값이 와야 할 곳에서 끝남");
		}

		char c = *m_Cursor;
		if (c == '{')
		{
			return ParseObject(out, depth);
		}
		if (c == '[')
		{
			return ParseArray(out, depth);
		}
		if (c == '"')
		{
			out.m_Type = JsonValue::Type::String;
			return ParseString(out.m_String);
		}
		if (c == 't')
		{
			out.m_Type = JsonValue::Type::Bool;
			out.m_Bool = true;
			return Expect("true");
		}
		if (c == 'f')
		{
			out.m_Type = JsonValue::Type::Bool;
			out.m_Bool = false;
			return Expect("false");
		}
		if (c == 'n')
		{
			out.m_Type = JsonValue::Type::Null;
			return Expect("null");
		}
		return ParseNumber(out);
	}

	bool ParseNumber(JsonValue& out)
	{
		// strtod는 널 종료 문자열을 읽으므로, 숫자 부분만 잘라 임시 버퍼에 옮겨 읽는다.
		const char* start = m_Cursor;
		while (m_Cursor < m_End && (strchr("+-0123456789.eE", *m_Cursor) != nullptr))
		{
			++m_Cursor;
		}
		if (m_Cursor == start)
		{
			return Fail("값이 아님");
		}

		char buffer[64];
		size_t length = (size_t)(m_Cursor - start);
		if (length >= sizeof(buffer))
		{
			return Fail("숫자가 너무 김");
		}
		memcpy(buffer, start, length);
		buffer[length] = '\0';

		out.m_Type = JsonValue::Type::Number;
		out.m_Number = strtod(buffer, nullptr);
		return true;
	}

	bool ParseHex4(unsigned int& out)
	{
		if (m_End - m_Cursor < 4)
		{
			return Fail("\\u 뒤가 짧음");
		}

		out = 0;
		for (int i = 0; i < 4; ++i)
		{
			char c = *m_Cursor++;
			out <<= 4;
			if (c >= '0' && c <= '9') out |= (unsigned int)(c - '0');
			else if (c >= 'a' && c <= 'f') out |= (unsigned int)(c - 'a' + 10);
			else if (c >= 'A' && c <= 'F') out |= (unsigned int)(c - 'A' + 10);
			else return Fail("\\u 뒤가 16진수가 아님");
		}
		return true;
	}

	bool ParseString(std::string& out)
	{
		++m_Cursor; // 여는 따옴표
		while (m_Cursor < m_End)
		{
			char c = *m_Cursor++;
			if (c == '"')
			{
				return true;
			}
			if (c != '\\')
			{
				out.push_back(c);
				continue;
			}

			if (m_Cursor >= m_End)
			{
				break;
			}

			char escape = *m_Cursor++;
			switch (escape)
			{
			case '"': out.push_back('"'); break;
			case '\\': out.push_back('\\'); break;
			case '/': out.push_back('/'); break;
			case 'b': out.push_back('\b'); break;
			case 'f': out.push_back('\f'); break;
			case 'n': out.push_back('\n'); break;
			case 'r': out.push_back('\r'); break;
			case 't': out.push_back('\t'); break;
			case 'u':
				{
					unsigned int codepoint = 0;
					if (!ParseHex4(codepoint))
					{
						return false;
					}

					// 대리 쌍(이모지 등).
					if (codepoint >= 0xD800 && codepoint <= 0xDBFF && m_End - m_Cursor >= 6 && m_Cursor[0] == '\\' && m_Cursor[1] == 'u')
					{
						m_Cursor += 2;
						unsigned int low = 0;
						if (!ParseHex4(low))
						{
							return false;
						}
						codepoint = 0x10000 + ((codepoint - 0xD800) << 10) + (low - 0xDC00);
					}
					AppendUtf8(out, codepoint);
				}
				break;
			default:
				return Fail("알 수 없는 이스케이프");
			}
		}
		return Fail("문자열이 닫히지 않음");
	}

	bool ParseArray(JsonValue& out, int depth)
	{
		out.m_Type = JsonValue::Type::Array;
		++m_Cursor; // [
		SkipSpace();
		if (m_Cursor < m_End && *m_Cursor == ']')
		{
			++m_Cursor;
			return true;
		}

		for (;;)
		{
			out.m_Items.emplace_back();
			SkipSpace();
			if (!ParseValue(out.m_Items.back(), depth + 1))
			{
				return false;
			}

			SkipSpace();
			if (m_Cursor >= m_End)
			{
				return Fail("배열이 닫히지 않음");
			}
			if (*m_Cursor == ',')
			{
				++m_Cursor;
				continue;
			}
			if (*m_Cursor == ']')
			{
				++m_Cursor;
				return true;
			}
			return Fail("배열에 쉼표나 ]가 와야 함");
		}
	}

	bool ParseObject(JsonValue& out, int depth)
	{
		out.m_Type = JsonValue::Type::Object;
		++m_Cursor; // {
		SkipSpace();
		if (m_Cursor < m_End && *m_Cursor == '}')
		{
			++m_Cursor;
			return true;
		}

		for (;;)
		{
			SkipSpace();
			if (m_Cursor >= m_End || *m_Cursor != '"')
			{
				return Fail("객체의 키가 와야 함");
			}

			std::string key;
			if (!ParseString(key))
			{
				return false;
			}

			SkipSpace();
			if (m_Cursor >= m_End || *m_Cursor != ':')
			{
				return Fail("키 뒤에 :가 와야 함");
			}
			++m_Cursor;
			SkipSpace();

			JsonValue& value = out.m_Members[key];
			if (!ParseValue(value, depth + 1))
			{
				return false;
			}

			SkipSpace();
			if (m_Cursor >= m_End)
			{
				return Fail("객체가 닫히지 않음");
			}
			if (*m_Cursor == ',')
			{
				++m_Cursor;
				continue;
			}
			if (*m_Cursor == '}')
			{
				++m_Cursor;
				return true;
			}
			return Fail("객체에 쉼표나 }가 와야 함");
		}
	}

	const char* m_Text;
	const char* m_End;
	const char* m_Cursor;
	std::string m_Error;
};

const std::string& JsonValue::AsString() const
{
	return (m_Type == Type::String) ? m_String : EmptyString();
}

const JsonValue& JsonValue::At(size_t index) const
{
	return (m_Type == Type::Array && index < m_Items.size()) ? m_Items[index] : NullValue();
}

const JsonValue& JsonValue::operator[](const char* key) const
{
	if (m_Type != Type::Object)
	{
		return NullValue();
	}

	std::map<std::string, JsonValue>::const_iterator found = m_Members.find(key);
	return (found != m_Members.end()) ? found->second : NullValue();
}

bool JsonValue::Has(const char* key) const
{
	return m_Type == Type::Object && m_Members.find(key) != m_Members.end();
}

bool JsonValue::Parse(const char* text, size_t length, JsonValue& outValue, std::string& outError)
{
	outValue = JsonValue();
	JsonParser parser(text, length);
	return parser.ParseDocument(outValue, outError);
}
