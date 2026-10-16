#pragma once

#include <random>
#include <string>

class NpcActor;
class PlayerActor;
class SceneGraph;
class StoryDirector;
struct SocialLine;

// 마을 사람들의 사회: 믿는 신(Faith.h)끼리의 관계에 따라 NPC를 짝지어 수다·말다툼·주먹다짐을 시키고, 플레이어의
// 수호신에 따라 반가워하거나(같은 신이면 작은 선물), 텃세를 부리거나, 다혈질이면 시비를 걸어 주먹다짐을 시작한다.
// 대사는 Data/Story.txt의 social 줄에서 고르고(코드를 다시 컴파일하지 않고 고칠 수 있게), 플레이어 곁(8칸 안)에서
// 일어난 일만 채팅창에 띄운다. 주먹다짐은 목숨을 걸지 않는다(플레이어 체력은 1 밑으로 내려가지 않고, 마을 사람은
// 항복한다). 대화창·지도로 세상이 멈춘 동안엔 부르지 않는다.
class NpcSociety
{
public:
	NpcSociety();

	// 지역을 옮길 때(씬이 새로 지어지면) 부른다. 씬의 NPC를 가리키던 것을 잊는다.
	void Reset();

	void Update(float deltaSeconds, SceneGraph& scene, PlayerActor& player, const StoryDirector& director);

	// 플레이어가 주먹다짐 중인 마을 사람을 때렸다(게임의 공격이 부름). 항복 처리는 다음 Update에서 한다.
	void OnPlayerHitBrawler(NpcActor& npc);

	// 지금 플레이어와 주먹다짐 중인 사람이 있는지(모니터·안내용).
	bool IsBrawlActive() const { return m_Brawler != nullptr; }

private:
	void UpdateEncounters(SceneGraph& scene, PlayerActor& player, const StoryDirector& director);
	void UpdateReactions(SceneGraph& scene, PlayerActor& player, const StoryDirector& director);
	void UpdateBrawl(PlayerActor& player, const StoryDirector& director);

	// situation(데이터 파일의 social 줄 상황)에 맞는 대사 하나를 골라 그 사람 이름으로 채팅창에 띄운다.
	// 대사의 {god}은 말하는 사람의 신, {other}는 상대(상대 NPC의 신이나 플레이어의 수호신) 이름으로 바뀐다.
	// 알맞은 대사가 없으면 false.
	bool Say(const NpcActor& speaker, const char* situation, const char* otherGod, const StoryDirector& director);

	float m_ScanTimer = 0.f;     // NPC끼리 짝을 찾는 간격
	float m_LineCooldown = 0.f;  // NPC끼리의 대사가 채팅창을 뒤덮지 않게
	float m_ReactionCooldown = 0.f; // 플레이어에게 하는 반응도 너무 몰리지 않게
	NpcActor* m_Brawler = nullptr;  // 지금 플레이어와 주먹다짐 중인 사람(같은 씬 — Reset으로 잊음)
	std::mt19937 m_Rng;
};
