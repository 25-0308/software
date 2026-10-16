#include "stdafx.h"
#include "SkillBar.h"

#include <cmath>

#include "Renderer.h"
#include "StoryState.h"
#include "UiDraw.h"

namespace
{
	// 칸 위의 스킬 이름(최대 약 96픽셀)이 서로 겹치거나 화면 밖으로 나가지 않게 칸 사이를 넉넉히 둔다.
	const float kSlotSize = 54.f;
	const float kSlotGap = 46.f;
	const float kRight = 767.f;   // 오른쪽 칸(R)의 오른쪽 끝
	const float kScreenRightLimit = 796.f;
	const float kBottom = 16.f;   // 칸의 아래쪽 끝
	const float kBorder = 3.f;
	const int kNameWrapWidth = 100;
}

SkillBar::SkillBar()
	: m_KeyText(15, true)
	, m_NameText(12)
	, m_CountText(18, true)
{
}

SkillBar::~SkillBar()
{
	ReleaseTexts();
	for (std::pair<const int, TextTexture>& entry : m_Seconds)
	{
		TextRasterizer::Destroy(entry.second);
	}
}

void SkillBar::ReleaseTexts()
{
	for (int i = 0; i < (int)SkillSlot::Count; ++i)
	{
		TextRasterizer::Destroy(m_Keys[i]);
		TextRasterizer::Destroy(m_Names[i]);
	}
	TextRasterizer::Destroy(m_Locked);
	m_TextsFor.clear();
}

const TextTexture& SkillBar::SecondsText(int seconds)
{
	std::map<int, TextTexture>::iterator found = m_Seconds.find(seconds);
	if (found != m_Seconds.end())
	{
		return found->second;
	}

	TextTexture texture = m_CountText.Create(std::to_string(seconds), 60);
	return m_Seconds.emplace(seconds, texture).first->second;
}

void SkillBar::Draw(Renderer& renderer, const StoryState& story, const float remaining[], const float total[], float time)
{
	if (!story.IsTrue("BLESSING_GRANTED"))
	{
		return;
	}

	const std::string& patron = story.Get("PATRON");
	const PatronDef* def = PatronPowers::Find(patron);
	if (def == nullptr)
	{
		return;
	}

	// 수호신이 정해지면(또는 바뀌면) 키 이름·스킬 이름 글자를 만든다.
	if (m_TextsFor != patron)
	{
		ReleaseTexts();
		for (int i = 0; i < (int)SkillSlot::Count; ++i)
		{
			SkillSlot slot = (SkillSlot)i;
			m_Keys[i] = m_KeyText.Create(PatronPowers::KeyName(slot), 40);
			m_Names[i] = m_NameText.Create(PatronPowers::Skill(*def, slot).name, kNameWrapWidth);
		}
		m_Locked = m_NameText.Create("각성 후", 80);
		m_TextsFor = patron;
	}

	Mat4 ui = UiDraw::ScreenProjection();
	bool awakened = story.IsTrue("AWAKENED");
	float pulse = 0.65f + 0.35f * sinf(time * 3.f);

	for (int i = 0; i < (int)SkillSlot::Count; ++i)
	{
		SkillSlot slot = (SkillSlot)i;
		float left = kRight - kSlotSize - (float)((int)SkillSlot::Count - 1 - i) * (kSlotSize + kSlotGap);
		bool locked = (slot == SkillSlot::Ultimate && !awakened);
		bool ready = !locked && remaining[i] <= 0.f;

		// 테두리: 쓸 수 있으면 수호신 색으로 숨 쉬듯, 쿨타임 중엔 어둡게, 잠겼으면 회색.
		float borderR = locked ? 0.35f : (ready ? def->colorR * pulse : 0.4f);
		float borderG = locked ? 0.35f : (ready ? def->colorG * pulse : 0.36f);
		float borderB = locked ? 0.38f : (ready ? def->colorB * pulse : 0.3f);
		UiDraw::Rect(renderer, ui, left - kBorder, kBottom - kBorder, kSlotSize + kBorder * 2.f, kSlotSize + kBorder * 2.f,
			borderR, borderG, borderB, 0.95f);
		UiDraw::Rect(renderer, ui, left, kBottom, kSlotSize, kSlotSize, 0.08f, 0.07f, 0.1f, 0.92f);

		// 칸 가운데의 수호신 색 문양(마름모) — 잠겼으면 흐리게.
		float glyphAlpha = locked ? 0.25f : 0.85f;
		UiDraw::Diamond(renderer, ui, left + kSlotSize * 0.5f, kBottom + kSlotSize * 0.5f, (slot == SkillSlot::Ultimate) ? 16.f : 12.f,
			def->colorR, def->colorG, def->colorB, glyphAlpha);

		// 쿨타임: 남은 만큼 위에서부터 어둡게 덮는다.
		if (!locked && remaining[i] > 0.f && total[i] > 0.f)
		{
			float fraction = remaining[i] / total[i];
			if (fraction > 1.f)
			{
				fraction = 1.f;
			}
			float coverHeight = kSlotSize * fraction;
			UiDraw::Rect(renderer, ui, left, kBottom + kSlotSize - coverHeight, kSlotSize, coverHeight, 0.f, 0.f, 0.f, 0.62f);

			const TextTexture& count = SecondsText((int)ceilf(remaining[i]));
			UiDraw::TextTopLeft(renderer, ui, count, floorf(left + (kSlotSize - (float)count.width) * 0.5f),
				floorf(kBottom + (kSlotSize + (float)count.height) * 0.5f), 1.f, 1.f, 1.f, 1.f);
		}

		// 키 이름(왼쪽 위 구석)과 스킬 이름(칸 위). 궁극기가 잠겼으면 이름 대신 "각성 후".
		UiDraw::TextTopLeft(renderer, ui, m_Keys[i], left + 3.f, kBottom + kSlotSize - 1.f, 0.98f, 0.92f, 0.7f, 1.f);

		const TextTexture& label = locked ? m_Locked : m_Names[i];
		float labelLeft = floorf(left + (kSlotSize - (float)label.width) * 0.5f);
		if (labelLeft + (float)label.width > kScreenRightLimit)
		{
			labelLeft = floorf(kScreenRightLimit - (float)label.width);
		}
		float labelTop = kBottom + kSlotSize + kBorder + 2.f + (float)label.height;
		UiDraw::Rect(renderer, ui, labelLeft - 2.f, labelTop - (float)label.height, (float)label.width + 4.f, (float)label.height,
			0.f, 0.f, 0.f, 0.45f);
		UiDraw::TextTopLeft(renderer, ui, label, labelLeft, labelTop, locked ? 0.6f : 1.f, locked ? 0.6f : 0.95f, locked ? 0.62f : 0.85f, 1.f);
	}
}
