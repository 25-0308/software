#include "stdafx.h"
#include "StoryDirector.h"

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>

#include "CharacterActors.h"
#include "GameLog.h"
#include "LandmarkActors.h"
#include "Renderer.h"
#include "SceneGraph.h"
#include "UiDraw.h"

namespace
{
	const float kBannerSeconds = 5.f;  // 지역 이름 배너가 떠 있는 시간
	const float kBannerFadeIn = 0.6f;
	const float kBannerFadeOut = 1.2f;
	const float kBarkRange = 8.f;      // 동료가 이 거리 안에 있어야 혼잣말이 들린다
	const int kMaxEffectDepth = 16;    // GOTO가 GOTO를 부를 수 있는 최대 깊이

	// 목표 표시(오른쪽 위 미니맵 아래).
	const float kObjectiveRight = 788.f;
	const float kObjectiveTop = 452.f;
	const int kObjectiveWrapWidth = 250;
	const int kObjectiveHintWrapWidth = 340; // 목적지 안내는 한 줄로 보이게 조금 더 넓게

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

	// "올림포스 산기슭 · 테살리아" → "올림포스 산기슭"(가운뎃점 앞부분). 가운뎃점이 없으면 그대로.
	std::string ShortRegionName(const std::string& name)
	{
		size_t dot = name.find(" · ");
		return (dot == std::string::npos) ? name : name.substr(0, dot);
	}

	// 목표 마름모를 대상의 어느 높이에 띄울지(인물은 이름표 위, 물건은 그 위쪽).
	float MarkerHeight(const Actor& actor)
	{
		switch (actor.GetType())
		{
		case ActorType::NPC:
		case ActorType::Player:
			return actor.GetSize() * 2.05f;
		case ActorType::Animal:
			return actor.GetSize() * 1.5f + 0.4f;
		case ActorType::Item:
			return 1.2f;
		case ActorType::Marker:
			return 1.0f;
		default:
			return 2.1f;
		}
	}
}

StoryDirector::StoryDirector()
	: m_Rng(std::random_device{}())
	, m_ObjectiveText(15)
	, m_BannerTitleText(26, true)
	, m_BannerTipText(15)
{
}

StoryDirector::~StoryDirector()
{
	TextRasterizer::Destroy(m_ObjectiveTexture);
	TextRasterizer::Destroy(m_ObjectiveHintTexture);
	TextRasterizer::Destroy(m_BannerTitleTexture);
	TextRasterizer::Destroy(m_BannerTipTexture);
}

bool StoryDirector::Initialize(const std::string& dataPath)
{
	bool loaded = m_Database.Load(dataPath);

	// 진행 단계의 순서는 데이터 파일의 step 선언 순서 그대로다(조건식의 STORY_STEP 크기 비교가 이 순서를 따름).
	m_Story.ClearSteps();
	for (const StoryStep& step : m_Database.GetSteps())
	{
		m_Story.RegisterStep(step.id);
	}

	m_Story.Reset();
	m_SeenOnce.clear();
	m_QueuedBlocks.clear();
	m_VariantCursor.clear();
	return loaded;
}

void StoryDirector::OnMapLoaded(const std::string& locId, SceneGraph& scene, PlayerActor& player)
{
	m_Scene = &scene;
	m_Player = &player;

	// 한 번 와 본 지역은 이정표로 언제든 다시 올 수 있다.
	m_Story.Set("MAP", locId);
	m_Story.Set("UNLOCK_" + locId, "true");

	m_Marker = scene.Spawn<ObjectiveMarkerActor>();
	m_ObjectiveSpots.clear();

	// 길잡이 화살표는 플레이어의 자식으로 붙여서 플레이어를 저절로 따라다니게 한다.
	m_Guide = static_cast<GuideArrowActor*>(player.AddChild(std::unique_ptr<Actor>(new GuideArrowActor())));

	// 첫 프레임부터 지금 이야기에 맞는 인물·물건만 보이도록 존재 조건을 바로 한 번 평가한다.
	UpdatePresence();

	ShowRegionBanner(locId);
	StartTriggered(DialogueTrigger::Enter, locId);
}

bool StoryDirector::OnTalk(const Actor& actor)
{
	return (actor.GetStoryId() != nullptr) && StartTriggered(DialogueTrigger::Talk, actor.GetStoryId());
}

bool StoryDirector::OnPickup(const Actor& actor)
{
	return (actor.GetStoryId() != nullptr) && StartTriggered(DialogueTrigger::Pickup, actor.GetStoryId());
}

void StoryDirector::OnKill(const Actor& actor)
{
	if (actor.GetStoryId() != nullptr)
	{
		StartTriggered(DialogueTrigger::Kill, actor.GetStoryId());
	}
}

bool StoryDirector::StartTriggered(DialogueTrigger trigger, const std::string& target)
{
	if (m_DialogueBox.IsActive())
	{
		return false;
	}

	const DialogueBlock* block = m_Database.FindTriggered(trigger, target, m_Story, m_SeenOnce);
	if (block == nullptr)
	{
		return false;
	}

	StartBlock(*block);
	return true;
}

void StoryDirector::StartBlock(const DialogueBlock& block)
{
	if (block.variants.empty())
	{
		return;
	}

	if (block.once)
	{
		m_SeenOnce.insert(block.id);
	}

	// 방금 끝난 대화의 효과가 아직 적용되기 전이면 먼저 적용한다(새 대화를 열면서 그 효과를 지우지 않게).
	ApplyFinishedEffects();

	if (m_DialogueBox.IsActive())
	{
		m_QueuedBlocks.push_back(&block);
		return;
	}

	ShowBlock(block);
}

void StoryDirector::ApplyFinishedEffects()
{
	std::vector<std::string> effects;
	if (m_DialogueBox.TakeFinishedEffects(effects))
	{
		ApplyEffects(effects);
	}
}

void StoryDirector::ShowBlock(const DialogueBlock& block)
{
	// 같은 블록이 여러 번 열리면 변형(or)을 차례로 돌려 가며 보여준다(마을 사람의 잡담 등).
	int& cursor = m_VariantCursor[block.id];
	const DialogueVariant& variant = block.variants[(size_t)cursor % block.variants.size()];
	cursor = (cursor + 1) % (int)block.variants.size();

	// 이정표 대화라면 목표가 있는 지역으로 가는 선택지를 강조한다.
	m_DialogueBox.SetObjectiveRegion(GetTravelRegion());
	m_DialogueBox.Start(variant, m_Database, m_Story);

	// 대사·선택지가 없는 대화(효과만 있는 것)는 대화창 없이 곧바로 끝나므로 효과를 바로 적용한다.
	ApplyFinishedEffects();
}

void StoryDirector::ApplyEffects(const std::vector<std::string>& effects)
{
	if (m_EffectDepth >= kMaxEffectDepth)
	{
		std::cout << "[이야기] GOTO가 너무 깊게 이어집니다(데이터에 순환이 있는지 확인). 효과 적용을 멈춥니다.\n";
		return;
	}

	++m_EffectDepth;
	for (const std::string& effect : effects)
	{
		ApplyEffect(effect);
	}
	--m_EffectDepth;
}

void StoryDirector::ApplyEffect(const std::string& effect)
{
	std::string command;
	std::string argument;
	SplitFirstToken(effect, command, argument);

	if (command == "SET")
	{
		std::string name;
		std::string value;
		SplitFirstToken(argument, name, value);
		m_Story.Set(name, value);
	}
	else if (command == "ADD")
	{
		std::string name;
		std::string amount;
		SplitFirstToken(argument, name, amount);
		m_Story.Add(name, std::atoi(amount.c_str()));
	}
	else if (command == "STEP")
	{
		m_Story.Set("STORY_STEP", argument);
	}
	else if (command == "UNLOCK")
	{
		m_Story.Set("UNLOCK_" + argument, "true");

		const StoryRegion* region = m_Database.FindRegion(argument);
		std::string name = (region != nullptr) ? region->name : argument;
		GameLog::Add(GameLog::Kind::Reward, "[새 지역] " + name + " — 이정표에서 갈 수 있다.");
	}
	else if (command == "GOTO")
	{
		// 이어지는 대화. 그 대화의 조건(if)이 거짓이면 열리지 않는다 — 조건부 분기에도 쓴다.
		const DialogueBlock* block = m_Database.FindById(argument);
		bool alreadySeen = (block != nullptr) && block->once && m_SeenOnce.find(block->id) != m_SeenOnce.end();
		if (block != nullptr && !alreadySeen && m_Database.PassesConditions(*block, m_Story))
		{
			StartBlock(*block);
		}
	}
	else if (command == "TRAVEL")
	{
		m_TravelRequest = argument;
	}
	else if (command == "XP")
	{
		if (m_Player != nullptr)
		{
			m_Player->GrantXP(std::atoi(argument.c_str()));
		}
	}
	else if (command == "HEAL")
	{
		if (m_Player != nullptr)
		{
			m_Player->Heal(m_Player->GetStats().maxHp);
		}
	}
	else if (command == "NOTICE")
	{
		GameLog::Add(GameLog::Kind::Info, m_Story.Interpolate(argument));
	}
	else if (command == "EVENT")
	{
		std::string name;
		std::string rest;
		SplitFirstToken(argument, name, rest);

		if (name == "BLESSING")
		{
			GrantBlessing();
		}
		else if (name == "SHAKE")
		{
			float seconds = rest.empty() ? 1.f : (float)std::atof(rest.c_str());
			m_ShakeTimer = seconds;
			m_ShakeDuration = seconds;
		}
		else if (name == "FLARE")
		{
			// 그 스토리 ID의 흐려진 신을 잠깐 또렷하게(이름을 불러 줬을 때).
			if (m_Scene != nullptr)
			{
				m_Scene->ForEach([&rest](Actor& actor)
				{
					if (actor.GetType() == ActorType::NPC && actor.GetStoryId() != nullptr && rest == actor.GetStoryId())
					{
						static_cast<NpcActor&>(actor).Flare();
					}
				});
			}
		}
		else
		{
			std::cout << "[이야기] 알 수 없는 EVENT: " << argument << "\n";
		}
	}
	else
	{
		std::cout << "[이야기] 알 수 없는 효과: " << effect << "\n";
	}
}

// 수호신의 첫 축복(PATRON에 따라). STORY.md 6장 MQ_01 표의 축복 테마를 따르며, 지금은 상시(패시브)
// 효과만 있다. 액티브 스킬·스킬 트리는 아직 정해지지 않았다(TODO).
void StoryDirector::GrantBlessing()
{
	if (m_Player == nullptr || m_Story.IsTrue("BLESSING_GRANTED"))
	{
		return;
	}

	m_Story.Set("BLESSING_GRANTED", "true");
	const std::string& patron = m_Story.Get("PATRON");

	if (patron == "ZEUS")
	{
		m_Player->AddAttackPower(4);
		GameLog::Add(GameLog::Kind::Reward, "[번개의 축복] 공격력 +4. 공격할 때 가끔 벼락이 함께 떨어진다.");
	}
	else if (patron == "POSEIDON")
	{
		m_Player->AddMaxHp(12);
		GameLog::Add(GameLog::Kind::Reward, "[바다의 축복] 최대 체력 +12. 파도처럼 쉽게 무너지지 않는다.");
	}
	else if (patron == "HADES")
	{
		GameLog::Add(GameLog::Kind::Reward, "[망자의 축복] 적을 쓰러뜨리면 그 생명의 온기가 스며들어 체력이 회복된다.");
	}
	else if (patron == "APHRODITE")
	{
		GameLog::Add(GameLog::Kind::Reward, "[매혹의 축복] 짐승과 괴물이 너를 훨씬 가까이 와서야 알아챈다.");
	}
	else if (patron == "HERMES")
	{
		GameLog::Add(GameLog::Kind::Reward, "[질풍의 축복] 이동 속도 +30%.");
	}

	ApplyPatronModifiers(*m_Player, patron);
}

void StoryDirector::ApplyPatronModifiers(PlayerActor& player, const std::string& patron)
{
	player.SetSpeedScale((patron == "HERMES") ? 1.3f : 1.f);
	player.SetDetectScale((patron == "APHRODITE") ? 0.55f : 1.f);
}

void StoryDirector::Update(float deltaSeconds)
{
	m_Time += deltaSeconds;
	m_DialogueBox.Update(deltaSeconds);

	// 방금 끝난 대화의 효과를 적용한다(대화창은 키 입력으로 끝나고, 효과는 여기서 한 번에 적용).
	ApplyFinishedEffects();

	// 대화창이 비었으면 기다리던 대화를 차례로 연다(효과만 있는 대화는 바로 적용되고 다음 것으로 넘어간다).
	while (!m_DialogueBox.IsActive() && !m_QueuedBlocks.empty())
	{
		const DialogueBlock* next = m_QueuedBlocks.front();
		m_QueuedBlocks.erase(m_QueuedBlocks.begin());
		ShowBlock(*next);
	}

	if (m_ShakeTimer > 0.f)
	{
		m_ShakeTimer -= deltaSeconds;
	}
	if (m_BannerTimer > 0.f)
	{
		m_BannerTimer -= deltaSeconds;
	}

	if (m_Scene == nullptr || m_Player == nullptr)
	{
		return;
	}

	UpdatePresence();

	if (!m_DialogueBox.IsActive())
	{
		UpdateTriggers();
	}

	UpdateObjective();
	UpdateBarks(deltaSeconds);
}

void StoryDirector::HandleKey(unsigned char key)
{
	if (!m_DialogueBox.IsActive())
	{
		return;
	}

	if (key == 'e' || key == 'E' || key == ' ' || key == '\r' || key == '\n')
	{
		m_DialogueBox.Advance(m_Story);
	}
	else if (key >= '1' && key <= '9')
	{
		m_DialogueBox.Choose(key - '0');
	}
}

bool StoryDirector::TakeTravelRequest(std::string& outLocId)
{
	if (m_TravelRequest.empty())
	{
		return false;
	}

	outLocId = m_TravelRequest;
	m_TravelRequest.clear();
	return true;
}

void StoryDirector::GetShakeOffset(float time, float& outX, float& outY) const
{
	outX = 0.f;
	outY = 0.f;

	if (m_ShakeTimer <= 0.f || m_ShakeDuration <= 0.f)
	{
		return;
	}

	float strength = 0.12f * (m_ShakeTimer / m_ShakeDuration);
	outX = sinf(time * 47.f) * strength;
	outY = cosf(time * 39.f) * strength;
}

// 존재 조건이 있는 액터마다 지금 이야기 상태에서 보일지 숨을지 정한다.
void StoryDirector::UpdatePresence()
{
	if (m_Scene == nullptr)
	{
		return;
	}

	const StoryState& story = m_Story;
	m_Scene->ForEach([&story](Actor& actor)
	{
		const char* presence = actor.GetPresence();
		if (presence != nullptr)
		{
			actor.SetVisible(story.Evaluate(presence));
		}
	});
}

// 트리거 영역마다 플레이어가 바깥에서 안으로 막 들어섰는지 보고, 들어섰으면 "on trigger" 대화를 연다.
void StoryDirector::UpdateTriggers()
{
	float playerX = m_Player->GetWorldX();
	float playerY = m_Player->GetWorldY();
	std::string entered;

	// 진행 단계가 바뀌었으면 모든 영역을 "바깥"으로 되돌려서, 이미 안에 서 있는 영역도 새 단계의 대화를 열 수 있게 한다.
	const std::string& step = m_Story.Get("STORY_STEP");
	bool rearm = (step != m_TriggerArmedStep);
	m_TriggerArmedStep = step;

	m_Scene->ForEach([&](Actor& actor)
	{
		if (actor.GetType() != ActorType::Marker || actor.GetStoryId() == nullptr || !actor.IsVisible())
		{
			return;
		}

		TriggerZoneActor* zone = dynamic_cast<TriggerZoneActor*>(&actor);
		if (zone == nullptr)
		{
			return;
		}

		if (rearm)
		{
			zone->ResetInside();
		}

		float dx = playerX - zone->GetWorldX();
		float dy = playerY - zone->GetWorldY();
		bool inside = dx * dx + dy * dy <= zone->GetRadius() * zone->GetRadius();

		if (zone->UpdateInside(inside) && entered.empty())
		{
			entered = actor.GetStoryId();
		}
	});

	// 대화는 순회가 끝난 뒤에 연다(대화 효과가 씬을 건드려도 순회 중이 아니도록).
	if (!entered.empty())
	{
		StartTriggered(DialogueTrigger::Trigger, entered);
	}
}

// 지금 진행 단계의 목표 문구와 목표 대상(머리 위 금빛 마름모, 바닥의 금빛 트리거 영역, 발밑 길잡이 화살표,
// 미니맵 점)을 갱신한다. 목표가 다른 지역에 있으면(step의 지역) 이 지역의 이정표를 가리키고 목표 문구 아래에
// 목적지를 띄운다. 지역을 정하지 않은 단계는 대상이 이 지역에 하나도 없을 때 이정표를 가리킨다.
void StoryDirector::UpdateObjective()
{
	const StoryStep* step = m_Database.FindStep(m_Story.Get("STORY_STEP"));
	std::string travelRegion = GetTravelRegion();

	std::string objective = (step != nullptr) ? m_Story.Interpolate(step->objective) : std::string();
	if (objective != m_ObjectiveString)
	{
		m_ObjectiveString = objective;
		TextRasterizer::Destroy(m_ObjectiveTexture);
		if (!objective.empty())
		{
			m_ObjectiveTexture = m_ObjectiveText.Create(objective, kObjectiveWrapWidth);
		}
	}

	std::string hint;
	if (!objective.empty() && !travelRegion.empty())
	{
		hint = "▶ 목적지: " + GetRegionShortName(travelRegion) + " (이정표로 이동)";
	}
	if (hint != m_ObjectiveHintString)
	{
		m_ObjectiveHintString = hint;
		TextRasterizer::Destroy(m_ObjectiveHintTexture);
		if (!hint.empty())
		{
			m_ObjectiveHintTexture = m_ObjectiveText.Create(hint, kObjectiveHintWrapWidth);
		}
	}

	std::vector<ObjectiveMarkerActor::Target> targets;
	m_ObjectiveSpots.clear();
	bool hasTargets = (step != nullptr) && !step->targets.empty() && travelRegion.empty();
	bool targetExistsHere = false;

	m_Scene->ForEach([&](Actor& actor)
	{
		const char* storyId = actor.GetStoryId();
		bool isTarget = false;

		if (hasTargets && storyId != nullptr && !actor.IsPendingDestroy())
		{
			for (size_t i = 0; i < step->targets.size(); ++i)
			{
				if (step->targets[i] == storyId)
				{
					// 조건이 붙은 대상("ID?조건")은 조건이 참일 때만 표시한다(예: 아직 면담하지 않은 신만).
					const std::string& condition = step->targetConditions[i];
					targetExistsHere = true;
					isTarget = actor.IsVisible() && (condition.empty() || m_Story.Evaluate(condition));
					break;
				}
			}
		}

		TriggerZoneActor* zone = (actor.GetType() == ActorType::Marker) ? dynamic_cast<TriggerZoneActor*>(&actor) : nullptr;
		if (zone != nullptr)
		{
			zone->SetHighlighted(isTarget);
		}

		if (isTarget)
		{
			ObjectiveMarkerActor::Target marker = { actor.GetWorldX(), actor.GetWorldY(), actor.GetWorldZ() + MarkerHeight(actor) };
			targets.push_back(marker);
			m_ObjectiveSpots.push_back(Spot{ actor.GetWorldX(), actor.GetWorldY() });
		}
	});

	bool pointToSignpost = !travelRegion.empty();
	if (hasTargets && !targetExistsHere && step->region.empty())
	{
		pointToSignpost = true;
	}

	if (pointToSignpost)
	{
		m_Scene->ForEach([&](Actor& actor)
		{
			const char* storyId = actor.GetStoryId();
			if (storyId != nullptr && strcmp(storyId, "OBJ_SIGNPOST") == 0 && actor.IsVisible())
			{
				ObjectiveMarkerActor::Target marker = { actor.GetWorldX(), actor.GetWorldY(), actor.GetWorldZ() + MarkerHeight(actor) };
				targets.push_back(marker);
				m_ObjectiveSpots.push_back(Spot{ actor.GetWorldX(), actor.GetWorldY() });
			}
		});
	}

	// 대화 중엔 화면을 어지럽히지 않도록 목표 마름모와 길잡이 화살표를 감춘다.
	bool hideMarkers = m_DialogueBox.IsActive();

	if (m_Marker != nullptr)
	{
		if (hideMarkers)
		{
			targets.clear();
		}
		m_Marker->SetTargets(targets);
	}

	// 길잡이 화살표는 목표 대상 중 플레이어에게 가장 가까운 것을 가리킨다(면담할 신이 여럿일 때 등).
	if (m_Guide != nullptr)
	{
		const Spot* nearest = nullptr;
		float nearestDistanceSq = 0.f;

		for (const Spot& spot : m_ObjectiveSpots)
		{
			float dx = spot.x - m_Player->GetWorldX();
			float dy = spot.y - m_Player->GetWorldY();
			float distanceSq = dx * dx + dy * dy;

			if (nearest == nullptr || distanceSq < nearestDistanceSq)
			{
				nearest = &spot;
				nearestDistanceSq = distanceSq;
			}
		}

		if (nearest != nullptr && !hideMarkers)
		{
			m_Guide->SetTarget(nearest->x, nearest->y);
		}
		else
		{
			m_Guide->ClearTarget();
		}
	}
}

std::string StoryDirector::GetObjectiveRegion() const
{
	const StoryStep* step = m_Database.FindStep(m_Story.Get("STORY_STEP"));
	return (step != nullptr) ? step->region : std::string();
}

std::string StoryDirector::GetTravelRegion() const
{
	std::string region = GetObjectiveRegion();
	if (region.empty() || region == m_Story.Get("MAP"))
	{
		return std::string();
	}
	return region;
}

std::string StoryDirector::GetRegionName(const std::string& locId) const
{
	const StoryRegion* region = m_Database.FindRegion(locId);
	return (region != nullptr) ? region->name : locId;
}

std::string StoryDirector::GetRegionShortName(const std::string& locId) const
{
	return ShortRegionName(GetRegionName(locId));
}

// 동료가 곁에 있고 대화 중이 아니면, 가끔 조건에 맞는 혼잣말 하나를 채팅창에 띄운다.
void StoryDirector::UpdateBarks(float deltaSeconds)
{
	if (m_DialogueBox.IsActive())
	{
		return;
	}

	m_BarkTimer -= deltaSeconds;
	if (m_BarkTimer > 0.f)
	{
		return;
	}

	std::uniform_real_distribution<float> nextDelay(45.f, 75.f);
	m_BarkTimer = nextDelay(m_Rng);

	float playerX = m_Player->GetWorldX();
	float playerY = m_Player->GetWorldY();
	std::vector<const StoryBark*> candidates;

	for (const StoryBark& bark : m_Database.GetBarks())
	{
		if (!bark.condition.empty() && !m_Story.Evaluate(bark.condition))
		{
			continue;
		}

		const std::string& speaker = bark.speaker;
		Actor* nearby = m_Scene->FindNearest(playerX, playerY, kBarkRange, [&speaker](const Actor& actor)
		{
			return actor.GetStoryId() != nullptr && speaker == actor.GetStoryId();
		});

		if (nearby != nullptr)
		{
			candidates.push_back(&bark);
		}
	}

	if (candidates.empty())
	{
		return;
	}

	std::uniform_int_distribution<size_t> pick(0, candidates.size() - 1);
	const StoryBark* bark = candidates[pick(m_Rng)];
	GameLog::Add(GameLog::Kind::Dialogue, "[" + m_Database.GetSpeakerName(bark->speaker) + "] " + m_Story.Interpolate(bark->text));
}

void StoryDirector::ShowRegionBanner(const std::string& locId)
{
	TextRasterizer::Destroy(m_BannerTitleTexture);
	TextRasterizer::Destroy(m_BannerTipTexture);

	const StoryRegion* region = m_Database.FindRegion(locId);
	if (region == nullptr)
	{
		m_BannerTimer = 0.f;
		return;
	}

	m_BannerTitleTexture = m_BannerTitleText.Create(region->name, 700);
	if (!region->tip.empty())
	{
		m_BannerTipTexture = m_BannerTipText.Create(region->tip, 560);
	}
	m_BannerTimer = kBannerSeconds;
}

void StoryDirector::Draw(Renderer& renderer)
{
	Mat4 ui = UiDraw::ScreenProjection();

	// 목표: 오른쪽 위 미니맵 아래, 어두운 판 위에 금빛 테두리 띠 + 문구(+ 목표가 다른 지역이면 그 아래에 하늘색
	// 목적지 안내). 대화 중엔 가린다.
	if (m_ObjectiveTexture.id != 0 && !m_DialogueBox.IsActive())
	{
		float width = (float)m_ObjectiveTexture.width;
		float height = (float)m_ObjectiveTexture.height;
		float hintWidth = (float)m_ObjectiveHintTexture.width;   // 안내가 없으면 0
		float hintHeight = (float)m_ObjectiveHintTexture.height;
		float panelWidth = (width > hintWidth) ? width : hintWidth;
		float panelHeight = height + hintHeight;
		float left = kObjectiveRight - panelWidth - 8.f;

		UiDraw::Rect(renderer, ui, left - 8.f, kObjectiveTop - panelHeight - 4.f, panelWidth + 16.f, panelHeight + 8.f,
			0.03f, 0.03f, 0.05f, 0.75f);
		UiDraw::Rect(renderer, ui, left - 8.f, kObjectiveTop - panelHeight - 4.f, 3.f, panelHeight + 8.f, 0.9f, 0.72f, 0.32f, 1.f);
		UiDraw::TextTopLeft(renderer, ui, m_ObjectiveTexture, left, kObjectiveTop, 1.f, 0.93f, 0.72f, 1.f);
		UiDraw::TextTopLeft(renderer, ui, m_ObjectiveHintTexture, left, kObjectiveTop - height, 0.62f, 0.86f, 1.f, 1.f);
	}

	// 지역 이름 배너: 화면 위쪽에 지역 이름(크게) + 헤르메스의 한마디가 떠올랐다 사라진다.
	if (m_BannerTimer > 0.f && m_BannerTitleTexture.id != 0)
	{
		float elapsed = kBannerSeconds - m_BannerTimer;
		float alpha = 1.f;
		if (elapsed < kBannerFadeIn)
		{
			alpha = elapsed / kBannerFadeIn;
		}
		else if (m_BannerTimer < kBannerFadeOut)
		{
			alpha = m_BannerTimer / kBannerFadeOut;
		}

		float titleWidth = (float)m_BannerTitleTexture.width;
		float titleHeight = (float)m_BannerTitleTexture.height;
		float tipHeight = (m_BannerTipTexture.id != 0) ? (float)m_BannerTipTexture.height : 0.f;
		float titleTop = 440.f;
		float bandBottom = titleTop - titleHeight - tipHeight - 22.f;

		UiDraw::Rect(renderer, ui, 0.f, bandBottom, UiDraw::kScreenWidth, titleTop - bandBottom + 10.f, 0.f, 0.f, 0.f, 0.38f * alpha);
		UiDraw::TextTopLeft(renderer, ui, m_BannerTitleTexture, floorf(400.f - titleWidth * 0.5f), titleTop, 1.f, 0.88f, 0.55f, alpha);
		UiDraw::Rect(renderer, ui, floorf(400.f - titleWidth * 0.5f - 30.f), titleTop - titleHeight - 5.f, titleWidth + 60.f, 2.f,
			0.9f, 0.72f, 0.32f, 0.9f * alpha);

		if (m_BannerTipTexture.id != 0)
		{
			float tipWidth = (float)m_BannerTipTexture.width;
			UiDraw::TextTopLeft(renderer, ui, m_BannerTipTexture, floorf(400.f - tipWidth * 0.5f), titleTop - titleHeight - 10.f,
				0.88f, 0.9f, 0.95f, alpha);
		}
	}

	m_DialogueBox.Draw(renderer);
}
