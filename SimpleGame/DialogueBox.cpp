#include "stdafx.h"
#include "DialogueBox.h"

#include <cmath>

#include "Math3D.h"
#include "Renderer.h"
#include "StoryState.h"
#include "UiDraw.h"

namespace
{
	// 대화창 패널(화면 아래 가운데, 800x600 화면 좌표).
	const float kPanelLeft = 80.f;
	const float kPanelRight = 720.f;
	const float kPanelBottom = 14.f;
	const float kPanelTop = 146.f;

	const float kTextLeft = 100.f;       // 본문 글자 왼쪽 끝
	const int kTextWrapWidth = 600;      // 본문 줄바꿈 폭(픽셀)
	const int kChoiceWrapWidth = 590;    // 선택지 줄바꿈 폭(픽셀)
	const float kFadeInSeconds = 0.15f;  // 대사가 바뀔 때 글자가 떠오르는 시간

	// 선택지 효과 중에 locId로 가는 지역 이동(TRAVEL 지역ID)이 있는지. 효과 문자열은 파서가 앞뒤 공백을 떼어 둔다.
	bool TravelsTo(const DialogueChoice& choice, const std::string& locId)
	{
		if (locId.empty())
		{
			return false;
		}

		const std::string kTravel = "TRAVEL";
		for (const std::string& effect : choice.effects)
		{
			if (effect.compare(0, kTravel.size(), kTravel) != 0)
			{
				continue;
			}

			size_t begin = effect.find_first_not_of(" \t", kTravel.size());
			if (begin != std::string::npos && effect.substr(begin) == locId)
			{
				return true;
			}
		}
		return false;
	}
}

DialogueBox::DialogueBox()
	: m_BodyText(17)
	, m_NameText(16, true)
	, m_ChoiceText(16)
	, m_HintText(13)
{
}

DialogueBox::~DialogueBox()
{
	ReleaseTextures();
	TextRasterizer::Destroy(m_HintTexture);
}

void DialogueBox::ReleaseTextures()
{
	TextRasterizer::Destroy(m_LineTexture);
	TextRasterizer::Destroy(m_NameTexture);

	for (TextTexture& texture : m_ChoiceTextures)
	{
		TextRasterizer::Destroy(texture);
	}
	m_ChoiceTextures.clear();
	m_ChoiceHighlighted.clear();
}

void DialogueBox::Start(const DialogueVariant& variant, const DialogueDatabase& database, const StoryState& story)
{
	ReleaseTextures();

	m_Lines.clear();
	m_SpeakerNames.clear();
	m_Choices.clear();
	m_VisibleChoices.clear();
	m_FinishedEffects.clear();
	m_Effects = variant.effects;
	m_HasFinished = false;
	m_ShowingChoices = false;
	m_LineIndex = 0;

	for (const DialogueLine& source : variant.lines)
	{
		DialogueLine line;
		line.speaker = source.speaker;
		line.text = story.Interpolate(source.text);
		m_Lines.push_back(line);
		m_SpeakerNames.push_back(database.GetSpeakerName(source.speaker));
	}

	for (const DialogueChoice& source : variant.choices)
	{
		DialogueChoice choice = source;
		choice.text = story.Interpolate(source.text);
		m_Choices.push_back(choice);
	}

	// 대사도 선택지도 없으면 효과만 있는 대화: 창을 띄우지 않고 곧바로 끝낸다.
	if (m_Lines.empty() && m_Choices.empty())
	{
		Finish(-1);
		return;
	}

	m_Active = true;

	if (!m_Lines.empty())
	{
		ShowLine(0);
	}
	else
	{
		ShowChoices(story);
	}
}

void DialogueBox::ShowLine(size_t index)
{
	TextRasterizer::Destroy(m_LineTexture);
	TextRasterizer::Destroy(m_NameTexture);

	m_LineIndex = index;
	m_LineAge = 0.f;

	const DialogueLine& line = m_Lines[index];
	const std::string& speakerName = m_SpeakerNames[index];

	m_LineIsNarration = speakerName.empty();
	m_LineIsPlayer = (line.speaker == "PLAYER");

	m_LineTexture = m_BodyText.Create(line.text, kTextWrapWidth);
	if (!speakerName.empty())
	{
		m_NameTexture = m_NameText.Create(speakerName, 300);
	}
}

void DialogueBox::ShowChoices(const StoryState& story)
{
	m_VisibleChoices.clear();
	for (size_t i = 0; i < m_Choices.size(); ++i)
	{
		if (m_Choices[i].condition.empty() || story.Evaluate(m_Choices[i].condition))
		{
			m_VisibleChoices.push_back((int)i);
		}
	}

	// 조건 때문에 고를 수 있는 게 하나도 없으면 선택지 없이 끝낸다.
	if (m_VisibleChoices.empty())
	{
		Finish(-1);
		return;
	}

	for (size_t i = 0; i < m_VisibleChoices.size(); ++i)
	{
		const DialogueChoice& choice = m_Choices[m_VisibleChoices[i]];
		bool highlighted = TravelsTo(choice, m_ObjectiveRegion);

		std::string label = std::to_string(i + 1) + ". " + choice.text;
		if (highlighted)
		{
			label += "   ◆ 목표";
		}

		m_ChoiceTextures.push_back(m_ChoiceText.Create(label, kChoiceWrapWidth));
		m_ChoiceHighlighted.push_back(highlighted);
	}

	m_ShowingChoices = true;
}

void DialogueBox::Advance(const StoryState& story)
{
	if (!m_Active || m_ShowingChoices)
	{
		return;
	}

	if (m_LineIndex + 1 < m_Lines.size())
	{
		ShowLine(m_LineIndex + 1);
	}
	else if (!m_Choices.empty())
	{
		ShowChoices(story);
	}
	else
	{
		Finish(-1);
	}
}

void DialogueBox::Choose(int number)
{
	if (!m_Active || !m_ShowingChoices)
	{
		return;
	}

	int index = number - 1;
	if (index < 0 || index >= (int)m_VisibleChoices.size())
	{
		return;
	}

	Finish(m_VisibleChoices[index]);
}

void DialogueBox::Finish(int chosenIndex)
{
	// 효과 순서: 변형의 do 효과 → 고른 선택지의 효과(선택지에 GOTO가 있으면 do로 바꾼 값을 보고 갈라진다).
	m_FinishedEffects = m_Effects;
	if (chosenIndex >= 0 && chosenIndex < (int)m_Choices.size())
	{
		const std::vector<std::string>& choiceEffects = m_Choices[chosenIndex].effects;
		m_FinishedEffects.insert(m_FinishedEffects.end(), choiceEffects.begin(), choiceEffects.end());
	}

	m_HasFinished = true;
	m_Active = false;
	m_ShowingChoices = false;
	ReleaseTextures();
}

bool DialogueBox::TakeFinishedEffects(std::vector<std::string>& outEffects)
{
	if (!m_HasFinished)
	{
		return false;
	}

	outEffects = m_FinishedEffects;
	m_FinishedEffects.clear();
	m_HasFinished = false;
	return true;
}

void DialogueBox::Update(float deltaSeconds)
{
	m_LineAge += deltaSeconds;
	m_Time += deltaSeconds;
}

void DialogueBox::Draw(Renderer& renderer)
{
	if (!m_Active)
	{
		return;
	}

	Mat4 ui = UiDraw::ScreenProjection();
	float fade = (m_LineAge < kFadeInSeconds) ? m_LineAge / kFadeInSeconds : 1.f;

	// 패널: 금빛 테두리 + 어두운 바탕.
	float panelWidth = kPanelRight - kPanelLeft;
	float panelHeight = kPanelTop - kPanelBottom;
	UiDraw::Rect(renderer, ui, kPanelLeft - 2.f, kPanelBottom - 2.f, panelWidth + 4.f, panelHeight + 4.f, 0.72f, 0.58f, 0.3f, 0.9f);
	UiDraw::Rect(renderer, ui, kPanelLeft, kPanelBottom, panelWidth, panelHeight, 0.04f, 0.04f, 0.07f, 0.92f);

	// 화자 이름표: 패널 윗변에 반쯤 걸친 작은 판(지문이면 없음).
	if (m_NameTexture.id != 0)
	{
		float plateWidth = (float)m_NameTexture.width + 12.f;
		float plateHeight = (float)m_NameTexture.height + 4.f;
		float plateLeft = kPanelLeft + 14.f;
		float plateBottom = kPanelTop - plateHeight * 0.5f;

		UiDraw::Rect(renderer, ui, plateLeft - 2.f, plateBottom - 2.f, plateWidth + 4.f, plateHeight + 4.f, 0.72f, 0.58f, 0.3f, 0.95f);
		UiDraw::Rect(renderer, ui, plateLeft, plateBottom, plateWidth, plateHeight, 0.13f, 0.1f, 0.06f, 0.98f);
		UiDraw::TextTopLeft(renderer, ui, m_NameTexture, plateLeft + 6.f, plateBottom + plateHeight - 2.f, 1.f, 0.86f, 0.5f, 1.f);
	}

	// 본문: 지문은 차분한 청회색, 플레이어 대사는 옅은 하늘색, 인물 대사는 따뜻한 흰색.
	float r = 0.97f, g = 0.95f, b = 0.9f;
	if (m_LineIsNarration)
	{
		r = 0.78f; g = 0.82f; b = 0.9f;
	}
	else if (m_LineIsPlayer)
	{
		r = 0.72f; g = 0.9f; b = 1.f;
	}
	UiDraw::TextTopLeft(renderer, ui, m_LineTexture, kTextLeft, kPanelTop - 18.f, r, g, b, fade);

	if (m_ShowingChoices)
	{
		// 선택지: 패널 위로 위에서부터 1, 2, 3... 순서로 쌓는다(아래쪽 줄부터 계산해서 올라간다).
		float bottom = kPanelTop + 12.f;
		for (int i = (int)m_ChoiceTextures.size() - 1; i >= 0; --i)
		{
			const TextTexture& texture = m_ChoiceTextures[i];
			float rowHeight = (float)texture.height + 4.f;
			float rowWidth = (float)texture.width + 18.f;
			float rowLeft = kPanelLeft + 20.f;

			// 목표 지역으로 가는 선택지는 바탕·띠·글자를 금빛으로 강조한다.
			bool highlighted = ((size_t)i < m_ChoiceHighlighted.size()) && m_ChoiceHighlighted[i];
			if (highlighted)
			{
				UiDraw::Rect(renderer, ui, rowLeft, bottom, rowWidth, rowHeight, 0.17f, 0.12f, 0.04f, 0.95f);
				UiDraw::Rect(renderer, ui, rowLeft, bottom, 5.f, rowHeight, 1.f, 0.8f, 0.3f, 1.f);
				UiDraw::TextTopLeft(renderer, ui, texture, rowLeft + 10.f, bottom + rowHeight - 2.f, 1.f, 0.86f, 0.45f, 1.f);
			}
			else
			{
				UiDraw::Rect(renderer, ui, rowLeft, bottom, rowWidth, rowHeight, 0.06f, 0.05f, 0.09f, 0.93f);
				UiDraw::Rect(renderer, ui, rowLeft, bottom, 3.f, rowHeight, 0.85f, 0.68f, 0.32f, 1.f);
				UiDraw::TextTopLeft(renderer, ui, texture, rowLeft + 10.f, bottom + rowHeight - 2.f, 0.98f, 0.92f, 0.76f, 1.f);
			}

			bottom += rowHeight + 4.f;
		}
	}
	else
	{
		// 넘김 표시: 패널 오른쪽 아래에서 천천히 깜빡인다.
		if (m_HintTexture.id == 0)
		{
			m_HintTexture = m_HintText.Create("E ▶", 120);
		}

		float blink = 0.55f + 0.45f * sinf(m_Time * 4.f);
		UiDraw::TextTopLeft(renderer, ui, m_HintTexture, kPanelRight - 14.f - (float)m_HintTexture.width,
			kPanelBottom + 4.f + (float)m_HintTexture.height, 0.95f, 0.85f, 0.55f, blink);
	}
}
