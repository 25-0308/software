#include "stdafx.h"
#include "StoryState.h"

#include <cstdlib>
#include <cstring>

namespace
{
	std::string Trim(const std::string& text)
	{
		size_t begin = text.find_first_not_of(" \t");
		if (begin == std::string::npos)
		{
			return std::string();
		}

		size_t end = text.find_last_not_of(" \t");
		return text.substr(begin, end - begin + 1);
	}

	// text를 delimiter(여러 글자도 가능)로 나눈다.
	std::vector<std::string> Split(const std::string& text, const std::string& delimiter)
	{
		std::vector<std::string> parts;
		size_t start = 0;

		while (true)
		{
			size_t found = text.find(delimiter, start);
			if (found == std::string::npos)
			{
				parts.push_back(text.substr(start));
				break;
			}

			parts.push_back(text.substr(start, found - start));
			start = found + delimiter.size();
		}

		return parts;
	}

	// 부호 있는 정수 문자열이면 true와 그 값을 돌려준다.
	bool ParseInt(const std::string& text, int& outValue)
	{
		if (text.empty())
		{
			return false;
		}

		size_t i = 0;
		if (text[0] == '-' || text[0] == '+')
		{
			if (text.size() == 1)
			{
				return false;
			}
			i = 1;
		}

		for (; i < text.size(); ++i)
		{
			if (text[i] < '0' || text[i] > '9')
			{
				return false;
			}
		}

		outValue = std::atoi(text.c_str());
		return true;
	}

	bool IsTruthy(const std::string& value)
	{
		return !value.empty() && value != "0" && value != "false";
	}

	int Clamp(int value, int low, int high)
	{
		if (value < low) return low;
		if (value > high) return high;
		return value;
	}
}

StoryState::StoryState()
{
	Reset();
}

void StoryState::Reset()
{
	m_Values.clear();

	// STORY.md 8장의 기본값. 나머지(PATRON, MQ4_SIDE, 각종 bool)는 "없음 = 빈 값 = 거짓"으로 시작한다.
	m_Values["PATRON_TRUST"] = "50";
	m_Values["HUMAN_FAITH"] = "50";
	m_Values["ERIS_CLUE"] = "0";

	if (!m_Steps.empty())
	{
		m_Values["STORY_STEP"] = m_Steps.front();
	}
}

const std::string& StoryState::Get(const std::string& name) const
{
	std::map<std::string, std::string>::const_iterator found = m_Values.find(name);
	return (found != m_Values.end()) ? found->second : m_Empty;
}

int StoryState::GetInt(const std::string& name) const
{
	int value = 0;
	if (!ParseInt(Get(name), value))
	{
		return 0;
	}
	return value;
}

bool StoryState::IsTrue(const std::string& name) const
{
	return IsTruthy(Get(name));
}

void StoryState::Set(const std::string& name, const std::string& value)
{
	m_Values[name] = value;
}

void StoryState::Add(const std::string& name, int delta)
{
	int value = GetInt(name) + delta;

	if (name == "PATRON_TRUST" || name == "HUMAN_FAITH")
	{
		value = Clamp(value, 0, 100);
	}
	else if (name == "ERIS_CLUE")
	{
		value = Clamp(value, 0, 5);
	}

	m_Values[name] = std::to_string(value);
}

void StoryState::ClearSteps()
{
	m_StepOrder.clear();
	m_Steps.clear();
}

void StoryState::RegisterStep(const std::string& step)
{
	if (m_StepOrder.find(step) != m_StepOrder.end())
	{
		return;
	}

	m_StepOrder[step] = (int)m_Steps.size();
	m_Steps.push_back(step);
}

bool StoryState::IsKnownStep(const std::string& step) const
{
	return m_StepOrder.find(step) != m_StepOrder.end();
}

bool StoryState::Evaluate(const std::string& condition) const
{
	std::string trimmed = Trim(condition);
	if (trimmed.empty())
	{
		return true;
	}

	// "A && B || C"는 (A && B) || C — 대안(||) 중 하나라도 그 안의 항(&&)이 모두 참이면 참.
	std::vector<std::string> alternatives = Split(trimmed, "||");
	for (const std::string& alternative : alternatives)
	{
		bool allTrue = true;

		std::vector<std::string> terms = Split(alternative, "&&");
		for (const std::string& term : terms)
		{
			if (!EvaluateTerm(Trim(term)))
			{
				allTrue = false;
				break;
			}
		}

		if (allTrue)
		{
			return true;
		}
	}

	return false;
}

bool StoryState::EvaluateTerm(const std::string& term) const
{
	if (term.empty())
	{
		return true;
	}

	// 두 글자 연산자를 먼저 찾아야 ">="를 ">"로 잘못 자르지 않는다.
	const char* kOperators[] = { ">=", "<=", "==", "!=", ">", "<" };
	for (const char* op : kOperators)
	{
		size_t found = term.find(op);
		if (found != std::string::npos && found > 0)
		{
			std::string name = Trim(term.substr(0, found));
			std::string literal = Trim(term.substr(found + strlen(op)));
			return Compare(name, op, literal);
		}
	}

	if (term[0] == '!')
	{
		return !IsTrue(Trim(term.substr(1)));
	}

	return IsTrue(term);
}

bool StoryState::Compare(const std::string& name, const std::string& op, const std::string& literal) const
{
	const std::string& value = Get(name);

	// true/false와 비교하면 참거짓으로 맞춘다("1"과 "true"를 같은 참으로).
	if (literal == "true" || literal == "false")
	{
		bool lhs = IsTruthy(value);
		bool rhs = (literal == "true");

		if (op == "==") return lhs == rhs;
		if (op == "!=") return lhs != rhs;
		return false;
	}

	// 진행 단계끼리는 등록 순서로, 둘 다 정수면 수로 비교한다(값이 비어 있는 정수 변수는 0으로 친다).
	int lhsNumber = 0;
	int rhsNumber = 0;
	bool numeric = false;

	std::map<std::string, int>::const_iterator lhsStep = m_StepOrder.find(value);
	std::map<std::string, int>::const_iterator rhsStep = m_StepOrder.find(literal);
	if (rhsStep != m_StepOrder.end() && (lhsStep != m_StepOrder.end() || value.empty()))
	{
		lhsNumber = (lhsStep != m_StepOrder.end()) ? lhsStep->second : -1;
		rhsNumber = rhsStep->second;
		numeric = true;
	}
	else if (ParseInt(value.empty() ? std::string("0") : value, lhsNumber) && ParseInt(literal, rhsNumber))
	{
		numeric = true;
	}

	if (numeric)
	{
		if (op == ">=") return lhsNumber >= rhsNumber;
		if (op == "<=") return lhsNumber <= rhsNumber;
		if (op == ">") return lhsNumber > rhsNumber;
		if (op == "<") return lhsNumber < rhsNumber;
		if (op == "==") return lhsNumber == rhsNumber;
		if (op == "!=") return lhsNumber != rhsNumber;
		return false;
	}

	if (op == "==") return value == literal;
	if (op == "!=") return value != literal;
	return false;
}

std::string StoryState::Interpolate(const std::string& text) const
{
	std::string result;
	size_t position = 0;

	while (position < text.size())
	{
		size_t open = text.find('{', position);
		if (open == std::string::npos)
		{
			result.append(text, position, std::string::npos);
			break;
		}

		size_t close = text.find('}', open + 1);
		if (close == std::string::npos)
		{
			result.append(text, position, std::string::npos);
			break;
		}

		result.append(text, position, open - position);
		result += Get(Trim(text.substr(open + 1, close - open - 1)));
		position = close + 1;
	}

	return result;
}
