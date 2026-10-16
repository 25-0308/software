#include "stdafx.h"
#include "Faith.h"

#include <cstring>

namespace
{
	struct FaithName
	{
		Faith faith;
		const char* key;
		const char* god;
	};

	// Faith 순서와 같아야 한다.
	const FaithName kFaithNames[] =
	{
		{ Faith::None, "NONE", "없음" },
		{ Faith::Zeus, "ZEUS", "제우스" },
		{ Faith::Poseidon, "POSEIDON", "포세이돈" },
		{ Faith::Hades, "HADES", "하데스" },
		{ Faith::Aphrodite, "APHRODITE", "아프로디테" },
		{ Faith::Hermes, "HERMES", "헤르메스" },
		{ Faith::Hera, "HERA", "헤라" },
		{ Faith::Athena, "ATHENA", "아테나" },
		{ Faith::Ares, "ARES", "아레스" },
		{ Faith::Apollo, "APOLLO", "아폴론" },
		{ Faith::Dionysus, "DIONYSUS", "디오니소스" },
		{ Faith::Hephaestus, "HEPHAESTUS", "헤파이스토스" },
		{ Faith::Pan, "PAN", "판" },
		{ Faith::Jupiter, "JUPITER", "유피테르" },
	};

	// 신앙끼리의 관계(서로 같음 — 한 번만 적는다). 적지 않은 짝은 0(중립).
	//  - 제우스: 딸 아테나는 충성(+2), 왕좌를 노리는 헤라·포세이돈·아레스와는 긴장(-1), 자기 이름을 가져간 유피테르 숭배와는 적대(-2)
	//  - 포세이돈: 아테네를 두고 다툰 아테나와 적대(-2), 반란에 기대를 거는 헤라와 같은 처지의 하데스에겐 호의(+1)
	//  - 아프로디테: 황금사과 소동의 헤라·아테나와 적대(-2), 아레스와는 연인(+2), 남편 헤파이스토스와는 서먹(-1)
	//  - 헤르메스: 거의 모두와 잘 지내고(+1), 아들 판과는 각별하다(+2)
	//  - 헤라: 아들 아레스(+2)와 자기가 키운 유피테르 숭배(+2)는 한편
	//  - 아폴론: 질서의 신이라 광기의 디오니소스·피리 시합에서 진 판과 서먹(-1)
	struct RelationEntry
	{
		Faith a;
		Faith b;
		int value;
	};

	const RelationEntry kRelations[] =
	{
		{ Faith::Zeus, Faith::Athena, 2 },
		{ Faith::Zeus, Faith::Apollo, 1 },
		{ Faith::Zeus, Faith::Hermes, 1 },
		{ Faith::Zeus, Faith::Dionysus, 1 },
		{ Faith::Zeus, Faith::Hera, -1 },
		{ Faith::Zeus, Faith::Poseidon, -1 },
		{ Faith::Zeus, Faith::Ares, -1 },
		{ Faith::Zeus, Faith::Jupiter, -2 },

		{ Faith::Poseidon, Faith::Athena, -2 },
		{ Faith::Poseidon, Faith::Hera, 1 },
		{ Faith::Poseidon, Faith::Hades, 1 },
		{ Faith::Poseidon, Faith::Hermes, 1 },

		{ Faith::Hades, Faith::Hermes, 1 },
		{ Faith::Hades, Faith::Apollo, -1 },

		{ Faith::Aphrodite, Faith::Ares, 2 },
		{ Faith::Aphrodite, Faith::Hera, -2 },
		{ Faith::Aphrodite, Faith::Athena, -2 },
		{ Faith::Aphrodite, Faith::Hephaestus, -1 },
		{ Faith::Aphrodite, Faith::Hermes, 1 },
		{ Faith::Aphrodite, Faith::Dionysus, 1 },

		{ Faith::Hermes, Faith::Apollo, 1 },
		{ Faith::Hermes, Faith::Athena, 1 },
		{ Faith::Hermes, Faith::Dionysus, 1 },
		{ Faith::Hermes, Faith::Pan, 2 },

		{ Faith::Hera, Faith::Ares, 2 },
		{ Faith::Hera, Faith::Jupiter, 2 },
		{ Faith::Hera, Faith::Athena, -1 },
		{ Faith::Hera, Faith::Hephaestus, -1 },

		{ Faith::Athena, Faith::Ares, -2 },
		{ Faith::Athena, Faith::Jupiter, -2 }, // 아테나의 도시 한복판에 로마식 신전을 세우고 신앙을 사 간 새 숭배

		{ Faith::Ares, Faith::Hephaestus, -1 },
		{ Faith::Ares, Faith::Jupiter, 1 },

		{ Faith::Apollo, Faith::Dionysus, -1 },
		{ Faith::Apollo, Faith::Pan, -1 },

		{ Faith::Dionysus, Faith::Pan, 2 },
		{ Faith::Dionysus, Faith::Jupiter, 1 },
	};

	struct TraitsEntry
	{
		const char* storyId;
		NpcTraits traits;
	};

	// 마을 사람·이야기 속 사람의 성격. 돌아다니는 반경은 그 사람이 서 있던 자리 기준이다. 줄을 선 영혼·돌아온 자들·
	// 조난자·살아남은 양치기처럼 그 자리에 있는 것 자체가 이야기인 사람은 제자리(0)를 지킨다.
	const TraitsEntry kTraits[] =
	{
		{ "NPC_PILGRIM",         { Faith::Apollo,    2.0f, false, true } },
		{ "NPC_MERCHANT",        { Faith::Hermes,    0.6f, false, true } },
		{ "NPC_DELPHI_VILLAGER", { Faith::Apollo,    2.5f, false, true } },
		{ "NPC_SHEPHERD",        { Faith::Pan,       2.5f, true,  true } },
		{ "NPC_ATHENS_SKEPTIC",  { Faith::None,      0.f,  false, false } },
		{ "NPC_ATHENS_CITIZEN",  { Faith::Athena,    2.5f, true,  true } },
		{ "NPC_PRIEST",          { Faith::Jupiter,   1.2f, false, true } },
		{ "NPC_PHILOSOPHER",     { Faith::None,      1.5f, false, true } },
		{ "NPC_FISHER",          { Faith::Poseidon,  2.0f, true,  true } },
		{ "NPC_SOUL",            { Faith::Hades,     0.f,  false, false } },
		{ "NPC_SOUL_FISHER",     { Faith::Hades,     0.f,  false, false } },
		{ "NPC_SOUL_GRANNY",     { Faith::Hades,     0.f,  false, false } },
		{ "NPC_SOUL_GENERAL",    { Faith::Hades,     0.f,  false, false } },
		{ "NPC_UNDYING_ELDER",   { Faith::Hades,     0.f,  false, false } },
		{ "NPC_UNDYING",         { Faith::Hades,     0.f,  false, false } },
		{ "NPC_GUEST_HERO",      { Faith::Ares,      1.8f, true,  true } },
		{ "NPC_GUEST_NYMPH",     { Faith::Aphrodite, 1.8f, false, true } },
		{ "NPC_GUEST_MERCHANT",  { Faith::Hermes,    1.5f, false, true } },
		{ "NPC_PAPHOS_SERVANT",  { Faith::Aphrodite, 2.0f, false, true } },
		{ "NPC_RECORD_KEEPER",   { Faith::Hera,      0.6f, false, true } },
		{ "NPC_WEST_PRIEST",     { Faith::Jupiter,   0.8f, false, true } },
		{ "NPC_WEST_CITIZEN",    { Faith::Jupiter,   2.5f, true,  true } },
		{ "NPC_SURVIVOR",        { Faith::Zeus,      0.f,  false, false } },
		{ "CHR_PYTHIA",          { Faith::Apollo,    0.f,  false, false } },
		{ "CHR_PERSEPHONE",      { Faith::Hades,     0.f,  false, false } },
	};

	// 신(CHR_)은 자기 자신을 믿는다.
	const TraitsEntry kGods[] =
	{
		{ "CHR_ZEUS",       { Faith::Zeus,       0.f, false, false } },
		{ "CHR_POSEIDON",   { Faith::Poseidon,   0.f, false, false } },
		{ "CHR_HADES",      { Faith::Hades,      0.f, false, false } },
		{ "CHR_APHRODITE",  { Faith::Aphrodite,  0.f, false, false } },
		{ "CHR_HERMES",     { Faith::Hermes,     0.f, false, false } },
		{ "CHR_HERA",       { Faith::Hera,       0.f, false, false } },
		{ "CHR_ATHENA",     { Faith::Athena,     0.f, false, false } },
		{ "CHR_ARES",       { Faith::Ares,       0.f, false, false } },
		{ "CHR_APOLLO",     { Faith::Apollo,     0.f, false, false } },
		{ "CHR_DIONYSUS",   { Faith::Dionysus,   0.f, false, false } },
		{ "CHR_HEPHAESTUS", { Faith::Hephaestus, 0.f, false, false } },
		{ "CHR_PAN",        { Faith::Pan,        0.f, false, false } },
	};
}

Faith FaithInfo::FromPatron(const std::string& patron)
{
	if (patron == "ZEUS") return Faith::Zeus;
	if (patron == "POSEIDON") return Faith::Poseidon;
	if (patron == "HADES") return Faith::Hades;
	if (patron == "APHRODITE") return Faith::Aphrodite;
	if (patron == "HERMES") return Faith::Hermes;
	return Faith::None;
}

const char* FaithInfo::Key(Faith faith)
{
	int index = (int)faith;
	return (index >= 0 && index < (int)Faith::Count) ? kFaithNames[index].key : "NONE";
}

Faith FaithInfo::FromKey(const std::string& key, bool& outKnown)
{
	for (const FaithName& entry : kFaithNames)
	{
		if (key == entry.key)
		{
			outKnown = true;
			return entry.faith;
		}
	}

	outKnown = false;
	return Faith::None;
}

const char* FaithInfo::GodName(Faith faith)
{
	int index = (int)faith;
	return (index >= 0 && index < (int)Faith::Count) ? kFaithNames[index].god : "없음";
}

int FaithInfo::Relation(Faith a, Faith b)
{
	if (a == Faith::None || b == Faith::None)
	{
		return 0;
	}

	if (a == b)
	{
		return kSameFaith;
	}

	for (const RelationEntry& entry : kRelations)
	{
		if ((entry.a == a && entry.b == b) || (entry.a == b && entry.b == a))
		{
			return entry.value;
		}
	}

	return 0;
}

const char* FaithInfo::RelationKey(int relation)
{
	if (relation >= kSameFaith) return "same";
	if (relation >= 2) return "ally";
	if (relation == 1) return "friendly";
	if (relation == 0) return "neutral";
	if (relation == -1) return "wary";
	return "hostile";
}

const char* FaithInfo::RelationLabel(int relation)
{
	if (relation >= kSameFaith) return "같은 신";
	if (relation >= 2) return "동맹";
	if (relation == 1) return "호의";
	if (relation == 0) return "중립";
	if (relation == -1) return "경계";
	return "적대";
}

NpcTraits FaithInfo::TraitsFor(const char* storyId)
{
	if (storyId == nullptr)
	{
		return NpcTraits();
	}

	for (const TraitsEntry& entry : kTraits)
	{
		if (strcmp(entry.storyId, storyId) == 0)
		{
			return entry.traits;
		}
	}

	for (const TraitsEntry& entry : kGods)
	{
		if (strcmp(entry.storyId, storyId) == 0)
		{
			return entry.traits;
		}
	}

	return NpcTraits();
}
