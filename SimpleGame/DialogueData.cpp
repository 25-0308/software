#include "stdafx.h"
#include "DialogueData.h"

#include <cstring>
#include <fstream>
#include <iostream>

#include "StoryState.h"

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

	bool StartsWith(const std::string& text, const char* prefix)
	{
		return text.compare(0, strlen(prefix), prefix) == 0;
	}

	// 화자 ID로 쓸 수 있는 글자(영문·숫자·밑줄)로만 이뤄졌는지.
	bool IsIdentifier(const std::string& text)
	{
		if (text.empty())
		{
			return false;
		}

		for (char c : text)
		{
			bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_';
			if (!ok)
			{
				return false;
			}
		}
		return true;
	}

	// 첫 단어(공백 전까지)와 나머지로 나눈다.
	void SplitFirstToken(const std::string& text, std::string& outFirst, std::string& outRest)
	{
		std::string trimmed = Trim(text);
		size_t space = trimmed.find_first_of(" \t");

		if (space == std::string::npos)
		{
			outFirst = trimmed;
			outRest.clear();
			return;
		}

		outFirst = trimmed.substr(0, space);
		outRest = Trim(trimmed.substr(space + 1));
	}

	// "효과; 효과; ..." 를 효과 하나씩으로 나눈다(빈 칸은 버림).
	std::vector<std::string> SplitEffects(const std::string& text)
	{
		std::vector<std::string> effects;
		size_t start = 0;

		while (start <= text.size())
		{
			size_t found = text.find(';', start);
			std::string part = Trim(text.substr(start, (found == std::string::npos) ? std::string::npos : found - start));
			if (!part.empty())
			{
				effects.push_back(part);
			}

			if (found == std::string::npos)
			{
				break;
			}
			start = found + 1;
		}

		return effects;
	}

	// 맨 앞이 '('면 짝이 맞는 ')'까지를 조건으로 떼어 낸다. 괄호가 안 닫혔으면 false.
	bool TakeParenthesizedCondition(std::string& inOutText, std::string& outCondition)
	{
		outCondition.clear();
		if (inOutText.empty() || inOutText[0] != '(')
		{
			return true;
		}

		int depth = 0;
		for (size_t i = 0; i < inOutText.size(); ++i)
		{
			if (inOutText[i] == '(')
			{
				++depth;
			}
			else if (inOutText[i] == ')')
			{
				--depth;
				if (depth == 0)
				{
					outCondition = Trim(inOutText.substr(1, i - 1));
					inOutText = Trim(inOutText.substr(i + 1));
					return true;
				}
			}
		}

		return false;
	}

	bool ParseTrigger(const std::string& word, DialogueTrigger& outTrigger)
	{
		if (word == "talk") { outTrigger = DialogueTrigger::Talk; return true; }
		if (word == "pickup") { outTrigger = DialogueTrigger::Pickup; return true; }
		if (word == "trigger") { outTrigger = DialogueTrigger::Trigger; return true; }
		if (word == "kill") { outTrigger = DialogueTrigger::Kill; return true; }
		if (word == "enter") { outTrigger = DialogueTrigger::Enter; return true; }
		return false;
	}
}

bool DialogueDatabase::Load(const std::string& path)
{
	m_Blocks.clear();
	m_BlockIndex.clear();
	m_SpeakerNames.clear();
	m_Steps.clear();
	m_Regions.clear();
	m_Barks.clear();
	m_ErrorCount = 0;

	std::ifstream file(path, std::ios::binary);
	if (!file.is_open())
	{
		std::cout << "[이야기 데이터] " << path << " 파일을 열지 못했습니다. 대화 없이 진행합니다.\n";
		return false;
	}

	std::string line;
	int lineNumber = 0;
	while (std::getline(file, line))
	{
		++lineNumber;

		if (!line.empty() && line[line.size() - 1] == '\r')
		{
			line.erase(line.size() - 1);
		}

		// UTF-8 BOM(메모장 등이 파일 맨 앞에 붙이는 표시)은 떼어 낸다.
		if (lineNumber == 1 && line.size() >= 3 && (unsigned char)line[0] == 0xEF && (unsigned char)line[1] == 0xBB
			&& (unsigned char)line[2] == 0xBF)
		{
			line.erase(0, 3);
		}

		ParseLine(line, lineNumber);
	}

	Validate();

	std::cout << "[이야기 데이터] " << path << " 읽음: 대화 " << m_Blocks.size() << "개, 진행 단계 " << m_Steps.size()
		<< "개, 지역 " << m_Regions.size() << "개";
	if (m_ErrorCount > 0)
	{
		std::cout << " (문제 " << m_ErrorCount << "개 — 위 안내 참고)";
	}
	std::cout << "\n";

	return true;
}

void DialogueDatabase::Warn(int lineNumber, const std::string& message)
{
	++m_ErrorCount;
	std::cout << "[이야기 데이터] 줄 " << lineNumber << ": " << message << "\n";
}

void DialogueDatabase::ParseLine(const std::string& rawLine, int lineNumber)
{
	std::string line = Trim(rawLine);
	if (line.empty() || line[0] == '#')
	{
		return;
	}

	// ---- 대화 블록 시작: "== 대화ID" ----
	if (StartsWith(line, "=="))
	{
		std::string id = Trim(line.substr(2));
		if (id.empty())
		{
			Warn(lineNumber, "== 뒤에 대화 ID가 없습니다");
			return;
		}

		if (m_BlockIndex.find(id) != m_BlockIndex.end())
		{
			Warn(lineNumber, "대화 ID '" + id + "'가 이미 있습니다(GOTO는 먼저 나온 것을 엽니다)");
		}

		DialogueBlock block;
		block.id = id;
		block.sourceLine = lineNumber;
		block.variants.push_back(DialogueVariant());
		m_Blocks.push_back(block);

		if (m_BlockIndex.find(id) == m_BlockIndex.end())
		{
			m_BlockIndex[id] = m_Blocks.size() - 1;
		}
		return;
	}

	std::string keyword;
	std::string rest;
	SplitFirstToken(line, keyword, rest);

	// ---- 블록 밖에서도 쓰는 선언들 ----
	if (keyword == "name")
	{
		std::string id;
		std::string displayName;
		SplitFirstToken(rest, id, displayName);

		if (id.empty())
		{
			Warn(lineNumber, "name 뒤에 화자 ID가 없습니다");
			return;
		}
		m_SpeakerNames[id] = displayName;
		return;
	}

	if (keyword == "step")
	{
		StoryStep step;
		std::string remainder;
		SplitFirstToken(rest, step.id, remainder);

		if (step.id.empty())
		{
			Warn(lineNumber, "step 뒤에 단계 ID가 없습니다");
			return;
		}

		step.sourceLine = lineNumber;

		// 단계 ID 다음의 LOC_로 시작하는 단어는 목표가 있는 지역이다(그 지역이 아니면 이정표를 가리킨다).
		if (StartsWith(remainder, "LOC_"))
		{
			std::string afterRegion;
			SplitFirstToken(remainder, step.region, afterRegion);
			remainder = afterRegion;
		}

		if (!remainder.empty() && remainder[0] == '@')
		{
			std::string targetsToken;
			std::string objective;
			SplitFirstToken(remainder.substr(1), targetsToken, objective);

			size_t start = 0;
			while (start <= targetsToken.size())
			{
				size_t comma = targetsToken.find(',', start);
				std::string target = Trim(targetsToken.substr(start, (comma == std::string::npos) ? std::string::npos : comma - start));
				if (!target.empty())
				{
					// "ID?조건"이면 그 조건이 참일 때만 목표로 표시한다(예: 아직 면담하지 않은 신만).
					std::string condition;
					size_t question = target.find('?');
					if (question != std::string::npos)
					{
						condition = Trim(target.substr(question + 1));
						target = Trim(target.substr(0, question));
					}

					step.targets.push_back(target);
					step.targetConditions.push_back(condition);
				}

				if (comma == std::string::npos)
				{
					break;
				}
				start = comma + 1;
			}

			remainder = objective;
		}

		step.objective = remainder;
		m_Steps.push_back(step);
		return;
	}

	if (keyword == "region")
	{
		StoryRegion region;
		std::string remainder;
		SplitFirstToken(rest, region.id, remainder);

		size_t bar = remainder.find('|');
		region.name = Trim((bar == std::string::npos) ? remainder : remainder.substr(0, bar));
		region.tip = (bar == std::string::npos) ? std::string() : Trim(remainder.substr(bar + 1));
		m_Regions.push_back(region);
		return;
	}

	if (keyword == "bark")
	{
		StoryBark bark;
		std::string remainder;
		SplitFirstToken(rest, bark.speaker, remainder);

		if (!TakeParenthesizedCondition(remainder, bark.condition))
		{
			Warn(lineNumber, "bark 조건의 괄호가 닫히지 않았습니다");
			return;
		}

		bark.text = remainder;
		if (bark.speaker.empty() || bark.text.empty())
		{
			Warn(lineNumber, "bark는 '화자ID (조건) 대사' 형식이어야 합니다");
			return;
		}
		m_Barks.push_back(bark);
		return;
	}

	if (m_Blocks.empty())
	{
		Warn(lineNumber, "대화 블록(== ID) 밖에 있는 줄이라 무시합니다");
		return;
	}

	DialogueBlock& block = m_Blocks.back();

	// ---- 블록 설정 ----
	if (keyword == "on")
	{
		std::string triggerWord;
		std::string target;
		SplitFirstToken(rest, triggerWord, target);

		if (!ParseTrigger(triggerWord, block.trigger) || target.empty())
		{
			Warn(lineNumber, "on 줄은 'on talk|pickup|trigger|kill|enter 대상ID' 형식이어야 합니다");
			return;
		}
		block.target = target;
		return;
	}

	if (keyword == "if")
	{
		block.conditions.push_back(rest);
		return;
	}

	if (keyword == "once")
	{
		block.once = true;
		return;
	}

	if (keyword == "or")
	{
		block.variants.push_back(DialogueVariant());
		return;
	}

	DialogueVariant& variant = block.variants.back();

	if (keyword == "choice")
	{
		DialogueChoice choice;
		std::string body = rest;

		if (!TakeParenthesizedCondition(body, choice.condition))
		{
			Warn(lineNumber, "선택지 조건의 괄호가 닫히지 않았습니다");
			return;
		}

		size_t arrow = body.find("->");
		if (arrow != std::string::npos)
		{
			choice.text = Trim(body.substr(0, arrow));
			choice.effects = SplitEffects(body.substr(arrow + 2));
		}
		else
		{
			choice.text = body;
		}

		if (choice.text.empty())
		{
			Warn(lineNumber, "선택지 문구가 비어 있습니다");
			return;
		}
		variant.choices.push_back(choice);
		return;
	}

	if (keyword == "do")
	{
		std::vector<std::string> effects = SplitEffects(rest);
		variant.effects.insert(variant.effects.end(), effects.begin(), effects.end());
		return;
	}

	// ---- 대사 줄: "화자ID: 대사" ----
	size_t colon = line.find(':');
	if (colon != std::string::npos)
	{
		std::string speaker = Trim(line.substr(0, colon));
		if (IsIdentifier(speaker))
		{
			DialogueLine dialogueLine;
			dialogueLine.speaker = speaker;
			dialogueLine.text = Trim(line.substr(colon + 1));
			variant.lines.push_back(dialogueLine);
			return;
		}
	}

	Warn(lineNumber, "알 수 없는 줄입니다: " + line);
}

void DialogueDatabase::Validate()
{
	for (const DialogueBlock& block : m_Blocks)
	{
		for (const DialogueVariant& variant : block.variants)
		{
			std::vector<std::string> effects = variant.effects;
			for (const DialogueChoice& choice : variant.choices)
			{
				effects.insert(effects.end(), choice.effects.begin(), choice.effects.end());
			}

			for (const std::string& effect : effects)
			{
				std::string command;
				std::string argument;
				SplitFirstToken(effect, command, argument);

				if (command == "GOTO" && m_BlockIndex.find(argument) == m_BlockIndex.end())
				{
					Warn(block.sourceLine, "대화 '" + block.id + "'의 GOTO 대상 '" + argument + "'가 없습니다");
				}
				else if (command == "STEP" && FindStep(argument) == nullptr)
				{
					Warn(block.sourceLine, "대화 '" + block.id + "'의 STEP '" + argument + "'가 step으로 선언되지 않았습니다");
				}
			}
		}
	}

	for (const StoryStep& step : m_Steps)
	{
		if (!step.region.empty() && FindRegion(step.region) == nullptr)
		{
			Warn(step.sourceLine, "step '" + step.id + "'의 지역 '" + step.region + "'가 region으로 선언되지 않았습니다");
		}
	}
}

const DialogueBlock* DialogueDatabase::FindById(const std::string& id) const
{
	std::map<std::string, size_t>::const_iterator found = m_BlockIndex.find(id);
	return (found != m_BlockIndex.end()) ? &m_Blocks[found->second] : nullptr;
}

const DialogueBlock* DialogueDatabase::FindTriggered(DialogueTrigger trigger, const std::string& target, const StoryState& story,
	const std::set<std::string>& seenOnce) const
{
	for (const DialogueBlock& block : m_Blocks)
	{
		if (block.trigger != trigger || block.target != target)
		{
			continue;
		}

		if (block.once && seenOnce.find(block.id) != seenOnce.end())
		{
			continue;
		}

		if (PassesConditions(block, story))
		{
			return &block;
		}
	}

	return nullptr;
}

bool DialogueDatabase::HasTrigger(DialogueTrigger trigger, const std::string& target) const
{
	for (const DialogueBlock& block : m_Blocks)
	{
		if (block.trigger == trigger && block.target == target)
		{
			return true;
		}
	}
	return false;
}

bool DialogueDatabase::PassesConditions(const DialogueBlock& block, const StoryState& story) const
{
	for (const std::string& condition : block.conditions)
	{
		if (!story.Evaluate(condition))
		{
			return false;
		}
	}
	return true;
}

std::string DialogueDatabase::GetSpeakerName(const std::string& speakerId) const
{
	std::map<std::string, std::string>::const_iterator found = m_SpeakerNames.find(speakerId);
	return (found != m_SpeakerNames.end()) ? found->second : speakerId;
}

const StoryStep* DialogueDatabase::FindStep(const std::string& id) const
{
	for (const StoryStep& step : m_Steps)
	{
		if (step.id == id)
		{
			return &step;
		}
	}
	return nullptr;
}

const StoryRegion* DialogueDatabase::FindRegion(const std::string& id) const
{
	for (const StoryRegion& region : m_Regions)
	{
		if (region.id == id)
		{
			return &region;
		}
	}
	return nullptr;
}
