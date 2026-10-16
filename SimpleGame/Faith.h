#pragma once

#include <string>

// 믿는 신(신앙). NPC마다 하나씩 있고, 플레이어의 신앙은 고른 수호신(PATRON)이다. 신앙끼리의 관계가
// NPC끼리의 교류(수다·말다툼·주먹다짐 — NpcSociety)와 NPC가 플레이어를 대하는 태도(반가움·선물·텃세·시비)를 정한다.
// 신들의 관계는 STORY.md 5장의 인물 관계(형제의 경쟁, 황금사과, 아테네를 두고 다툰 포세이돈과 아테나 등)를 따른다.
enum class Faith
{
	None,       // 믿는 신이 없다(신앙을 잃은 시민, 철학자, 이야기와 무관한 사람)
	Zeus,       // 아래 다섯은 플레이어가 수호신으로 고를 수 있는 신
	Poseidon,
	Hades,
	Aphrodite,
	Hermes,
	Hera,
	Athena,
	Ares,
	Apollo,
	Dionysus,
	Hephaestus,
	Pan,
	Jupiter,    // 서쪽에서 퍼지는 로마식 새 숭배(유피테르·유노) — 헤라가 돈을 대어 키운 신흥 신앙
	Count,
};

// NPC 하나의 성격: 믿는 신, 돌아다니는 반경(0이면 제자리), 다혈질(적대 신앙을 보면 시비를 건다),
// 마을 사람처럼 다른 NPC·플레이어와 교류하는지(이야기 인물·신은 교류하지 않고 제자리를 지킨다).
struct NpcTraits
{
	Faith faith = Faith::None;
	float wanderRadius = 0.f;
	bool hotheaded = false;
	bool social = false;
};

namespace FaithInfo
{
	// 같은 신을 모시는 사이의 관계 값. 그 밖의 관계는 -2(적대) ~ +2(동맹).
	const int kSameFaith = 3;

	// "ZEUS" 같은 수호신 값(PATRON) → 신앙. 수호신이 아니면 None.
	Faith FromPatron(const std::string& patron);

	// 데이터 파일·JSON용 영문 키("ZEUS", "JUPITER", 없으면 "NONE")와 그 반대.
	const char* Key(Faith faith);
	Faith FromKey(const std::string& key, bool& outKnown);

	// 신의 한글 이름("제우스"). None이면 "없음".
	const char* GodName(Faith faith);

	// 두 신앙의 관계: 같으면 kSameFaith(+3), 한쪽이 None이면 0(중립), 그 밖엔 관계표(-2~+2).
	int Relation(Faith a, Faith b);

	// 관계 값의 영문 키(JSON)·한글 이름: same/ally/friendly/neutral/wary/hostile — 같은 신/동맹/호의/중립/경계/적대.
	const char* RelationKey(int relation);
	const char* RelationLabel(int relation);

	// 이야기 ID(NPC_FISHER, CHR_ZEUS 등)별 기본 성격. 신(CHR_)은 자기 자신을 믿고 제자리를 지키며, 표에 없는
	// ID는 믿는 신 없이 제자리에 서 있다. 마을 사람은 신앙에 따라 돌아다니며 서로·플레이어와 교류한다.
	NpcTraits TraitsFor(const char* storyId);
}
