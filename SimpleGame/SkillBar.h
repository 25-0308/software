#pragma once

#include <map>
#include <string>

#include "PatronPowers.h"
#include "TextRasterizer.h"

class Renderer;
class StoryState;

// 화면 오른쪽 아래의 스킬 칸 두 개: Q(기본 스킬)와 R(궁극기). 칸마다 키 이름, 스킬 이름, 쿨타임이 남은 만큼 위에서부터
// 어둡게 덮는 막과 남은 초를 보여 주고, 쓸 수 있으면 수호신 색 테두리가 숨 쉬듯 빛난다. 궁극기는 각성 전엔 잠겨
// 있다("각성 후"). 축복을 받기 전엔 그리지 않는다. HUD처럼 후처리 이후 800x600 화면 좌표계로 그린다.
class SkillBar
{
public:
	SkillBar();
	~SkillBar();

	SkillBar(const SkillBar&) = delete;
	SkillBar& operator=(const SkillBar&) = delete;

	// remaining/total은 칸마다 남은 쿨타임과 그 쿨타임의 전체 길이(초) — SkillSlot 순서.
	void Draw(Renderer& renderer, const StoryState& story, const float remaining[], const float total[], float time);

private:
	void ReleaseTexts();
	const TextTexture& SecondsText(int seconds);

	TextRasterizer m_KeyText;
	TextRasterizer m_NameText;
	TextRasterizer m_CountText;

	std::string m_TextsFor; // 지금 글자를 만든 수호신(바뀌면 다시 만든다)
	TextTexture m_Keys[(int)SkillSlot::Count];
	TextTexture m_Names[(int)SkillSlot::Count];
	TextTexture m_Locked;
	std::map<int, TextTexture> m_Seconds; // 남은 초 숫자(한 번 만든 것은 계속 쓴다)
};
