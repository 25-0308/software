#pragma once

#include <map>
#include <string>
#include <vector>

// 이야기 진행 상태: STORY.md 8장의 플래그·변수(PATRON, PATRON_TRUST, HUMAN_FAITH, ERIS_CLUE ...)와,
// 그 밖에 진행 순서를 추적하는 구현용 변수(STORY_STEP, MAP, UNLOCK_<지역> 등 — Data/Story.txt 머리말 참고)를
// 이름 → 값(문자열) 표로 들고 있다. 대화 데이터의 조건식(if ...)과 효과(SET/ADD ...)가 전부 이 표를 읽고 쓴다.
//
// 값은 모두 문자열로 저장한다: 정수는 "50", 참거짓은 "true"/"false", 열거형은 "ZEUS" 같은 이름.
// 없는 변수는 빈 문자열(거짓, 0)로 취급한다.
class StoryState
{
public:
	StoryState();

	// 새 게임: 모든 변수를 지우고 STORY.md 8장 기본값(PATRON_TRUST 50, HUMAN_FAITH 50, ERIS_CLUE 0)을 넣은 뒤,
	// STORY_STEP을 첫 번째로 등록된 단계로 맞춘다.
	void Reset();

	const std::string& Get(const std::string& name) const;
	int GetInt(const std::string& name) const;

	// 비어 있지 않고 "0"/"false"가 아니면 참.
	bool IsTrue(const std::string& name) const;

	void Set(const std::string& name, const std::string& value);

	// 정수 변수에 delta를 더한다. STORY.md 8장의 범위가 정해진 변수는 그 범위로 자른다
	// (PATRON_TRUST·HUMAN_FAITH 0~100, ERIS_CLUE 0~5).
	void Add(const std::string& name, int delta);

	// ---- 진행 단계(STORY_STEP) ----
	// 대화 데이터의 step 선언 순서를 그대로 등록한다. 조건식에서 단계끼리 크기를 비교하면
	// (예: STORY_STEP >= MQ_00.CAVE) 이 순서로 비교한다.
	void ClearSteps();
	void RegisterStep(const std::string& step);
	bool IsKnownStep(const std::string& step) const;

	// 조건식을 평가한다. 형식: 항을 &&(그리고)·||(또는)로 잇는다(괄호 없음, &&가 먼저 묶임).
	// 항: NAME(참이면) / !NAME(거짓이면) / NAME 연산자 값(==, !=, >=, <=, >, <).
	// 값이 정수면 수로, 진행 단계 이름이면 단계 순서로, 그 밖엔 글자로(==, != 만) 비교한다.
	// 빈 조건식은 참.
	bool Evaluate(const std::string& condition) const;

	// 문장 속 {변수이름}을 그 값으로 바꾼다(예: "월계수 가지 ({LAUREL}/3)").
	std::string Interpolate(const std::string& text) const;

private:
	bool EvaluateTerm(const std::string& term) const;
	bool Compare(const std::string& name, const std::string& op, const std::string& literal) const;

	std::map<std::string, std::string> m_Values;
	std::map<std::string, int> m_StepOrder;
	std::vector<std::string> m_Steps;
	std::string m_Empty;
};
