#pragma once

#include <string>
#include <vector>

#include "DialogueData.h"
#include "TextRasterizer.h"

class Renderer;
class StoryState;

// 화면 아래쪽 대화창: 대사를 한 줄씩 넘겨 보여주고(화자 이름표 + 본문), 마지막에 선택지를 띄운다.
// 대사를 다 보거나 선택지를 고르면 대화가 끝나고, 그 대화의 효과(do + 고른 선택지의 효과)를 꺼내 갈 수 있다.
// 효과를 실제로 적용하는 건 StoryDirector다(맵 이동·경험치처럼 게임 쪽 일이 섞여 있어서).
// HUD와 같은 화면 고정 좌표계(800x600)로 후처리 이후에 그린다.
class DialogueBox
{
public:
	DialogueBox();
	~DialogueBox();

	DialogueBox(const DialogueBox&) = delete;
	DialogueBox& operator=(const DialogueBox&) = delete;

	// variant의 대사를 처음부터 보여준다. 대사·선택지 속 {변수}는 지금 story 값으로 채운다.
	// 대사도 선택지도 없는 변형이면 대화창을 띄우지 않고 곧바로 끝난 것으로 친다(효과만 있는 대화).
	void Start(const DialogueVariant& variant, const DialogueDatabase& database, const StoryState& story);

	// 지금 목표가 있는 다른 지역. 그 지역으로 가는 선택지(TRAVEL 효과)에 "◆ 목표"를 붙이고 금빛으로 강조한다
	// (이정표에서 어디로 가야 하는지 보이게). 비우면 강조하지 않는다. 선택지를 띄우기 전에 정해 둔다.
	void SetObjectiveRegion(const std::string& locId) { m_ObjectiveRegion = locId; }

	bool IsActive() const { return m_Active; }

	// 다음 대사로(E·Space·Enter). 마지막 대사에서 선택지가 있으면 선택지를 띄우고, 없으면 대화를 끝낸다.
	void Advance(const StoryState& story);

	// 선택지 고르기(화면에 보이는 번호, 1부터). 선택지가 떠 있을 때만 받는다.
	void Choose(int number);

	// 방금 끝난 대화의 효과를 꺼낸다(끝난 뒤 한 번만 true).
	bool TakeFinishedEffects(std::vector<std::string>& outEffects);

	void Update(float deltaSeconds);
	void Draw(Renderer& renderer);

private:
	void ShowLine(size_t index);
	void ShowChoices(const StoryState& story);
	void Finish(int chosenIndex);
	void ReleaseTextures();

	bool m_Active = false;
	bool m_HasFinished = false;

	std::vector<DialogueLine> m_Lines;     // {변수}를 채운 대사
	std::vector<std::string> m_SpeakerNames; // 대사마다의 화자 표시 이름
	std::vector<DialogueChoice> m_Choices; // 선택지(조건 포함, 문구는 {변수}를 채움)
	std::vector<std::string> m_Effects;    // 변형의 do 효과
	std::vector<int> m_VisibleChoices;     // 지금 보이는 선택지(m_Choices의 인덱스)
	std::vector<bool> m_ChoiceHighlighted; // m_VisibleChoices와 같은 순서: 목표 지역으로 가는 선택지인지
	std::vector<std::string> m_FinishedEffects;
	std::string m_ObjectiveRegion;

	size_t m_LineIndex = 0;
	bool m_ShowingChoices = false;
	float m_LineAge = 0.f; // 지금 대사가 뜬 뒤 지난 시간(살짝 페이드 인)
	float m_Time = 0.f;    // 넘김 표시 깜빡임용

	TextRasterizer m_BodyText;
	TextRasterizer m_NameText;
	TextRasterizer m_ChoiceText;
	TextRasterizer m_HintText;

	TextTexture m_LineTexture;
	TextTexture m_NameTexture;
	TextTexture m_HintTexture;
	std::vector<TextTexture> m_ChoiceTextures;

	// 지금 대사의 종류(글자 색을 다르게): 지문(이름 없음) / 플레이어 / 그 밖의 인물
	bool m_LineIsNarration = false;
	bool m_LineIsPlayer = false;
};
