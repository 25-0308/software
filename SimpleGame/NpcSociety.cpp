#include "stdafx.h"
#include "NpcSociety.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "CharacterActors.h"
#include "DialogueData.h"
#include "Faith.h"
#include "GameLog.h"
#include "SceneGraph.h"
#include "StoryDirector.h"
#include "StoryState.h"

namespace
{
	const float kScanInterval = 0.6f;          // 짝을 찾고 플레이어에게 반응할지 살피는 간격(초)
	const float kEncounterRadius = 2.4f;       // 이 안에 있는 둘이 어울린다
	const float kEncounterChance = 0.35f;      // 짝을 찾았을 때 실제로 어울릴 확률(지나칠 때마다 붙잡지 않게)
	const float kHearingRadius = 8.f;          // 플레이어가 이 안에 있어야 NPC끼리의 대사가 채팅창에 뜬다
	const float kLineCooldown = 7.f;           // NPC끼리의 대사 사이 최소 간격(채팅창을 뒤덮지 않게)
	const float kSocialCooldownMin = 18.f;     // 한 번 어울린 사람이 다시 어울리기까지
	const float kSocialCooldownMax = 32.f;

	const float kReactionRadius = 2.4f;        // 플레이어가 이만큼 다가오면 반응한다
	const float kReactionGlobalCooldown = 4.f; // 반응끼리의 최소 간격
	const float kReactionCooldownSame = 90.f;  // 같은 사람이 다시 반응하기까지(같은 신·주먹다짐 뒤)
	const float kReactionCooldownOther = 45.f;
	const int kGiftHeal = 6;                   // 같은 신을 모시는 사람이 건네는 선물(체력)

	const float kBrawlSeconds = 20.f;
	const int kBrawlStamina = 3;               // 이만큼 맞으면 항복한다
	const int kBrawlWinXP = 10;

	void ReplaceAll(std::string& text, const std::string& from, const std::string& to)
	{
		size_t position = 0;
		while ((position = text.find(from, position)) != std::string::npos)
		{
			text.replace(position, from.size(), to);
			position += to.size();
		}
	}

	float DistanceSq(const Actor& a, float x, float y)
	{
		float dx = a.GetWorldX() - x;
		float dy = a.GetWorldY() - y;
		return dx * dx + dy * dy;
	}
}

NpcSociety::NpcSociety()
	: m_Rng(std::random_device{}())
{
}

void NpcSociety::Reset()
{
	m_ScanTimer = 0.f;
	m_LineCooldown = 0.f;
	m_ReactionCooldown = 0.f;
	m_Brawler = nullptr;
}

void NpcSociety::Update(float deltaSeconds, SceneGraph& scene, PlayerActor& player, const StoryDirector& director)
{
	m_LineCooldown = (std::max)(0.f, m_LineCooldown - deltaSeconds);
	m_ReactionCooldown = (std::max)(0.f, m_ReactionCooldown - deltaSeconds);

	UpdateBrawl(player, director);

	m_ScanTimer -= deltaSeconds;
	if (m_ScanTimer > 0.f)
	{
		return;
	}
	m_ScanTimer = kScanInterval;

	UpdateEncounters(scene, player, director);
	UpdateReactions(scene, player, director);
}

void NpcSociety::OnPlayerHitBrawler(NpcActor& npc)
{
	npc.TakeBrawlHit();
}

// NPC끼리: 가까이 있는 둘을 짝지어 믿는 신의 관계대로 어울리게 한다.
//  - 같은 신·호의 이상: 수다(chat) / 중립: 가벼운 인사(greet)
//  - 경계(또는 적대인데 둘 다 순한 사람): 말다툼(argue)
//  - 적대 + 한쪽이 다혈질: 주먹다짐(다혈질이 fight로 먼저 덤비고, 상대는 argue로 받아친다)
void NpcSociety::UpdateEncounters(SceneGraph& scene, PlayerActor& player, const StoryDirector& director)
{
	std::vector<NpcActor*> candidates;
	scene.ForEach([&candidates](Actor& actor)
	{
		if (actor.GetType() != ActorType::NPC || actor.IsPendingDestroy())
		{
			return;
		}

		NpcActor& npc = static_cast<NpcActor&>(actor);
		if (npc.IsFree() && npc.Memory().socialCooldown <= 0.f)
		{
			candidates.push_back(&npc);
		}
	});

	std::uniform_real_distribution<float> unit(0.f, 1.f);
	float playerX = player.GetWorldX();
	float playerY = player.GetWorldY();

	for (size_t i = 0; i < candidates.size(); ++i)
	{
		NpcActor* a = candidates[i];
		if (!a->IsFree() || a->Memory().socialCooldown > 0.f)
		{
			continue; // 이번에 이미 다른 사람과 짝지어졌다
		}

		for (size_t j = i + 1; j < candidates.size(); ++j)
		{
			NpcActor* b = candidates[j];
			if (!b->IsFree() || b->Memory().socialCooldown > 0.f)
			{
				continue;
			}

			if (DistanceSq(*a, b->GetWorldX(), b->GetWorldY()) > kEncounterRadius * kEncounterRadius)
			{
				continue;
			}

			if (unit(m_Rng) > kEncounterChance)
			{
				break; // 이번엔 그냥 스쳐 지나간다
			}

			const NpcTraits& traitsA = a->GetTraits();
			const NpcTraits& traitsB = b->GetTraits();
			int relation = FaithInfo::Relation(traitsA.faith, traitsB.faith);
			bool hothead = traitsA.hotheaded || traitsB.hotheaded;

			NpcActivity kind = NpcActivity::Chat;
			float seconds = 4.f;
			const char* situation = "chat";
			if (relation == 0)
			{
				seconds = 2.5f;
				situation = "greet";
			}
			else if (relation <= -2 && hothead)
			{
				kind = NpcActivity::Scuffle;
				seconds = 3.5f;
				situation = "fight";
			}
			else if (relation < 0)
			{
				kind = NpcActivity::Argue;
				situation = "argue";
			}

			a->BeginEncounter(*b, kind, seconds);
			b->BeginEncounter(*a, kind, seconds);

			float cooldown = kSocialCooldownMin + (kSocialCooldownMax - kSocialCooldownMin) * unit(m_Rng);
			a->Memory().socialCooldown = seconds + cooldown;
			b->Memory().socialCooldown = seconds + cooldown + 3.f;

			// 플레이어 곁에서 벌어진 일만 들린다(그래도 너무 잦지 않게).
			bool heard = DistanceSq(*a, playerX, playerY) <= kHearingRadius * kHearingRadius
				|| DistanceSq(*b, playerX, playerY) <= kHearingRadius * kHearingRadius;
			if (heard && m_LineCooldown <= 0.f)
			{
				// 주먹다짐은 다혈질 쪽이 먼저 덤비고(fight) 상대가 받아친다(argue).
				NpcActor* first = a;
				NpcActor* second = b;
				if (kind == NpcActivity::Scuffle && !traitsA.hotheaded)
				{
					first = b;
					second = a;
				}

				const char* secondSituation = (kind == NpcActivity::Scuffle) ? "argue" : situation;
				const char* firstGod = FaithInfo::GodName(first->GetTraits().faith);
				const char* secondGod = FaithInfo::GodName(second->GetTraits().faith);
				if (Say(*first, situation, secondGod, director))
				{
					Say(*second, secondSituation, firstGod, director);
					m_LineCooldown = kLineCooldown;
				}
			}
			break;
		}
	}
}

// 플레이어에게: 수호신을 고른 뒤, 가까이 온 마을 사람 하나가 믿는 신의 관계대로 반응한다.
//  - 같은 신: 반가워하며 처음 한 번은 작은 선물(체력) / 호의: 반가운 인사 / 중립: 무심히 지나침
//  - 경계: 텃세 / 적대: 노골적인 적의, 다혈질이면 시비를 걸어 주먹다짐(한 번 진 사람은 다시 덤비지 않는다)
void NpcSociety::UpdateReactions(SceneGraph& scene, PlayerActor& player, const StoryDirector& director)
{
	const StoryState& story = director.GetStory();
	if (m_ReactionCooldown > 0.f || !story.IsTrue("BLESSING_GRANTED"))
	{
		return;
	}

	Faith patron = FaithInfo::FromPatron(story.Get("PATRON"));
	if (patron == Faith::None)
	{
		return;
	}

	float playerX = player.GetWorldX();
	float playerY = player.GetWorldY();
	Actor* nearest = scene.FindNearest(playerX, playerY, kReactionRadius, [](const Actor& actor)
	{
		if (actor.GetType() != ActorType::NPC)
		{
			return false;
		}

		const NpcActor& npc = static_cast<const NpcActor&>(actor);
		return npc.IsFree() && npc.GetTraits().faith != Faith::None && npc.Memory().reactionCooldown <= 0.f;
	});

	if (nearest == nullptr)
	{
		return;
	}

	NpcActor& npc = static_cast<NpcActor&>(*nearest);
	NpcMemory& memory = npc.Memory();
	int relation = FaithInfo::Relation(npc.GetTraits().faith, patron);
	const char* patronGod = FaithInfo::GodName(patron);

	// 무심한 사람(중립)은 반응하지 않지만, 매번 다시 살피지 않도록 쿨타임만 둔다.
	if (relation == 0 && !memory.humbled)
	{
		memory.reactionCooldown = kReactionCooldownOther;
		return;
	}

	npc.FaceToward(playerX, playerY);
	m_ReactionCooldown = kReactionGlobalCooldown;

	if (memory.humbled)
	{
		Say(npc, "humbled", patronGod, director);
		memory.reactionCooldown = kReactionCooldownSame;
	}
	else if (relation >= FaithInfo::kSameFaith)
	{
		Say(npc, "devoted", patronGod, director);
		if (!memory.giftGiven)
		{
			memory.giftGiven = true;
			player.Heal(kGiftHeal);
			GameLog::Add(GameLog::Kind::Reward, "[신도의 선물] 같은 신을 모시는 이가 건넨 무화과에 기운이 돈다. 체력 +" + std::to_string(kGiftHeal));
		}
		memory.reactionCooldown = kReactionCooldownSame;
	}
	else if (relation > 0)
	{
		Say(npc, "friendly", patronGod, director);
		memory.reactionCooldown = kReactionCooldownOther;
	}
	else if (relation == -1)
	{
		Say(npc, "wary", patronGod, director);
		memory.reactionCooldown = kReactionCooldownOther;
	}
	else if (npc.GetTraits().hotheaded && m_Brawler == nullptr)
	{
		Say(npc, "challenge", patronGod, director);
		npc.BeginBrawl(kBrawlSeconds, kBrawlStamina);
		m_Brawler = &npc;
		GameLog::Add(GameLog::Kind::Info, "[주먹다짐] 시비가 붙었다! Space로 맞서거나 멀리 피하자 — 목숨까지 걸 싸움은 아니다.");
	}
	else
	{
		Say(npc, "hostile", patronGod, director);
		memory.reactionCooldown = kReactionCooldownOther;
	}
}

// 플레이어와의 주먹다짐이 끝났으면 마무리한다: 항복이면 경험치와 함께 다시는 덤비지 않고, 그만뒀으면 한마디 남긴다.
void NpcSociety::UpdateBrawl(PlayerActor& player, const StoryDirector& director)
{
	if (m_Brawler == nullptr)
	{
		return;
	}

	BrawlOutcome outcome = m_Brawler->ConsumeBrawlOutcome();
	if (outcome == BrawlOutcome::None)
	{
		if (!m_Brawler->IsBrawling())
		{
			m_Brawler = nullptr; // 다른 까닭으로 끝났다(숨겨졌다 등)
		}
		return;
	}

	const char* patronGod = FaithInfo::GodName(FaithInfo::FromPatron(director.GetStory().Get("PATRON")));
	NpcMemory& memory = m_Brawler->Memory();

	if (outcome == BrawlOutcome::Yielded)
	{
		Say(*m_Brawler, "yield", patronGod, director);
		memory.humbled = true;
		memory.reactionCooldown = kReactionCooldownSame;
		GameLog::Add(GameLog::Kind::Reward, "[주먹다짐] 상대가 손을 들었다.");
		player.GrantXP(kBrawlWinXP);
	}
	else
	{
		Say(*m_Brawler, "giveup", patronGod, director);
		memory.reactionCooldown = kReactionCooldownSame;
	}

	m_Brawler = nullptr;
}

bool NpcSociety::Say(const NpcActor& speaker, const char* situation, const char* otherGod, const StoryDirector& director)
{
	const std::string faithKey = FaithInfo::Key(speaker.GetTraits().faith);

	std::vector<const SocialLine*> specific;
	std::vector<const SocialLine*> general;
	for (const SocialLine& line : director.GetSocialLines())
	{
		if (line.situation != situation)
		{
			continue;
		}

		if (line.faith == faithKey)
		{
			specific.push_back(&line);
		}
		else if (line.faith == "*")
		{
			general.push_back(&line);
		}
	}

	// 그 신앙만의 대사가 있으면 주로 그것을, 가끔은 누구나 하는 말도 섞는다.
	std::uniform_real_distribution<float> unit(0.f, 1.f);
	bool useSpecific = !specific.empty() && (general.empty() || unit(m_Rng) < 0.7f);
	const std::vector<const SocialLine*>& pool = useSpecific ? specific : general;
	if (pool.empty())
	{
		return false;
	}

	std::uniform_int_distribution<size_t> pick(0, pool.size() - 1);
	std::string text = pool[pick(m_Rng)]->text;
	ReplaceAll(text, "{god}", FaithInfo::GodName(speaker.GetTraits().faith));
	ReplaceAll(text, "{other}", otherGod);

	std::string name = (speaker.GetStoryId() != nullptr) ? director.GetSpeakerName(speaker.GetStoryId()) : std::string();
	if (name.empty())
	{
		name = "누군가";
	}

	GameLog::Add(GameLog::Kind::Dialogue, "[" + name + "] " + text);
	return true;
}
