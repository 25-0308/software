#pragma once

#include <map>
#include <random>
#include <set>
#include <string>
#include <vector>

#include "DialogueBox.h"
#include "DialogueData.h"
#include "LevelBuilder.h"
#include "StoryState.h"
#include "TextRasterizer.h"

class Actor;
class GuideArrowActor;
class ObjectiveMarkerActor;
class PlayerActor;
class Renderer;
class SceneGraph;

// 이야기 진행의 지휘자. Data/Story.txt(대화·진행 단계·지역)를 읽어 들고, 게임에서 일어난 일(말 걸기, 줍기,
// 괴물 처치, 트리거 영역 진입, 지역 도착)을 그에 맞는 대화로 바꿔 대화창에 띄우고, 대화가 끝나면 그 효과를
// 적용한다(이야기 변수 바꾸기, 진행 단계 넘기기, 지역 열기, 경험치, 지역 이동 요청, 축복 등).
// 매 프레임: 액터의 존재 조건(Actor::SetPresence) 평가, 트리거 영역 검사, 목표 표시 갱신, 동료 혼잣말.
// 목표 표시: 대상 머리 위 금빛 마름모, 플레이어 발밑의 길잡이 화살표, 미니맵·지도의 금빛 점. 지금 단계의
// 목표가 다른 지역(step의 지역)에 있으면 전부 이 지역의 이정표를 가리키고, 목표 문구 아래에 목적지를 띄우고,
// 이정표 대화의 그 지역 선택지를 강조한다.
// 화면엔 오른쪽 위 미니맵 아래의 "목표", 지역에 들어설 때의 지역 이름 배너, 아래쪽 대화창을 그린다.
//
// 효과 문법(대화 데이터의 do/choice 줄): SET 변수 값 / ADD 변수 수 / STEP 단계 / UNLOCK 지역 / GOTO 대화 /
// TRAVEL 지역 / XP 수 / HEAL / NOTICE 문구 / EVENT 이름 [인자] (BLESSING, AWAKEN, SHAKE 초, FLARE 스토리ID, BANNER 제목|한마디).
class StoryDirector
{
public:
	StoryDirector();
	~StoryDirector();

	StoryDirector(const StoryDirector&) = delete;
	StoryDirector& operator=(const StoryDirector&) = delete;

	// 데이터 파일을 읽고 새 게임 상태(STORY.md 8장 기본값 + 첫 진행 단계)로 만든다.
	bool Initialize(const std::string& dataPath);

	StoryState& GetStory() { return m_Story; }
	const StoryState& GetStory() const { return m_Story; }

	// 맵을 새로 지은 직후: 현재 지역 기록(MAP), 다시 올 수 있게 지역 열기, 목표 표시 생성, 지역 이름 배너,
	// "on enter" 대화. 이전 맵의 씬·플레이어는 이미 지워졌으므로 여기서 새 것으로 바꿔 든다.
	void OnMapLoaded(const std::string& locId, SceneGraph& scene, PlayerActor& player);

	// 게임 쪽 상호작용 → 이야기. 대화가 열렸으면(= 이야기 데이터가 반응했으면) true.
	bool OnTalk(const Actor& actor);
	bool OnPickup(const Actor& actor);
	void OnKill(const Actor& actor);

	// 플레이어가 쓰러졌을 때(제자리로 돌아와 다시 일어선 직후): "on death PLAYER" 대화(최종 보스전의 패배 등).
	void OnPlayerDeath();

	// 매 프레임 게임 갱신 전에 부른다.
	void Update(float deltaSeconds);

	bool IsDialogueActive() const { return m_DialogueBox.IsActive(); }

	// 대화 중의 키 입력(E·Space·Enter = 다음, 1~9 = 선택지).
	void HandleKey(unsigned char key);

	// 지역 이동 요청(TRAVEL 효과)이 있으면 true와 목적지를 돌려주고 요청을 지운다.
	bool TakeTravelRequest(std::string& outLocId);

	// 화면 흔들림(EVENT SHAKE) 오프셋(월드 단위). 카메라 초점에 더한다.
	void GetShakeOffset(float time, float& outX, float& outY) const;

	// 지금 목표 대상들의 월드 위치(미니맵·지도 표시용). 목표가 다른 지역에 있으면 이 지역의 이정표.
	const std::vector<Spot>& GetObjectiveSpots() const { return m_ObjectiveSpots; }

	// 지금 목표 문구({변수}를 채운 것, 없으면 빈 문자열)와 목표가 있는 지역(step에 지역이 없으면 빈 문자열).
	const std::string& GetObjectiveText() const { return m_ObjectiveString; }
	std::string GetObjectiveRegion() const;

	// 목표가 지금 지역이 아닌 다른 지역에 있으면 그 지역 ID, 아니면 빈 문자열(이정표로 가야 하는지).
	std::string GetTravelRegion() const;

	// 데이터 파일에 선언된 지역들(region 줄, 선언 순서)과 표시 이름. 짧은 이름은 가운뎃점 앞부분
	// (예: "올림포스 산기슭 · 테살리아" → "올림포스 산기슭"). 모르는 지역이면 ID 그대로.
	const std::vector<StoryRegion>& GetRegions() const { return m_Database.GetRegions(); }
	std::string GetRegionName(const std::string& locId) const;
	std::string GetRegionShortName(const std::string& locId) const;

	// 모니터(WorldMonitor)용: 스토리 ID의 한글 표시 이름(데이터 파일 name 줄, 없으면 ID 그대로),
	// locId 지역에 있는 그 인물·물건이 지금 목표 대상인지(지금 지역이면 목표 마름모가 뜨는 것과 같은 판정),
	// 지금 말을 걸면 열릴 대화가 있는지.
	std::string GetSpeakerName(const std::string& storyId) const { return m_Database.GetSpeakerName(storyId); }
	bool IsObjectiveTarget(const std::string& storyId, const std::string& locId) const;
	bool CanTalkTo(const std::string& storyId) const;

	// 화면을 seconds 동안 흔든다(EVENT SHAKE와 같음 — 포세이돈의 대지진 같은 스킬도 쓴다).
	void Shake(float seconds);

	// 마을 사람의 사회적 대사(Data/Story.txt의 social 줄 — NpcSociety가 고른다).
	const std::vector<SocialLine>& GetSocialLines() const { return m_Database.GetSocialLines(); }

	// 수호신 축복 중 맵을 새로 지을 때마다 다시 걸어야 하는 것(이동 속도·괴물이 알아채는 거리 배율, 각성의 기운)을
	// 적용한다. 축복을 받기 전이면 모두 기본값으로 되돌린다.
	static void ApplyPatronModifiers(PlayerActor& player, const StoryState& story);

	void Draw(Renderer& renderer);

private:
	bool StartTriggered(DialogueTrigger trigger, const std::string& target);

	// 대화 블록을 연다. 대화창에 이미 다른 대화가 떠 있으면(대화 효과의 GOTO가 이어서 다른 대화를 부른 경우 등)
	// 대기열에 넣었다가 지금 대화가 끝난 뒤에 연다 — 앞 대화를 덮어쓰지 않게.
	void StartBlock(const DialogueBlock& block);
	void ShowBlock(const DialogueBlock& block);

	// 대화창에서 끝난 대화가 있으면 그 효과(do + 고른 선택지)를 적용한다.
	void ApplyFinishedEffects();
	void ApplyEffects(const std::vector<std::string>& effects);
	void ApplyEffect(const std::string& effect);
	void GrantBlessing();
	void Awaken();

	void UpdatePresence();
	void UpdateTriggers();
	void UpdateObjective();
	void UpdateBarks(float deltaSeconds);

	void ShowRegionBanner(const std::string& locId);

	// 화면 위쪽에 5초간 떠오르는 큰 제목 + 한마디(지역 이름 배너와 같은 모양 — 장 제목·엔딩 제목에도 쓴다).
	void ShowBanner(const std::string& title, const std::string& tip);

	DialogueDatabase m_Database;
	StoryState m_Story;
	DialogueBox m_DialogueBox;

	std::set<std::string> m_SeenOnce;           // once 대화 중 이미 본 것
	std::vector<const DialogueBlock*> m_QueuedBlocks; // 지금 대화가 끝나면 이어서 열 대화(m_Database가 소유)
	std::string m_TriggerArmedStep;             // 트리거 영역을 마지막으로 다시 무장시킨 진행 단계
	std::map<std::string, int> m_VariantCursor; // 대화 블록별 다음에 보여줄 변형 번호(or 변형을 돌려 가며 보여줌)
	int m_EffectDepth = 0;                      // GOTO가 GOTO를 부르는 깊이(데이터 실수로 무한 반복되는 것을 막음)

	std::string m_TravelRequest;
	float m_ShakeTimer = 0.f;
	float m_ShakeDuration = 0.f;

	// 현재 맵(OnMapLoaded로 갱신). 맵이 바뀌는 순간 이전 씬은 지워지므로, 그 뒤 OnMapLoaded 전까진 쓰지 않는다.
	SceneGraph* m_Scene = nullptr;
	PlayerActor* m_Player = nullptr;
	ObjectiveMarkerActor* m_Marker = nullptr;
	GuideArrowActor* m_Guide = nullptr; // 플레이어의 자식(맵이 바뀌면 플레이어와 함께 새로 생긴다)
	std::vector<Spot> m_ObjectiveSpots;

	std::mt19937 m_Rng;
	float m_BarkTimer = 30.f;

	// 화면 글자
	TextRasterizer m_ObjectiveText;
	TextRasterizer m_BannerTitleText;
	TextRasterizer m_BannerTipText;
	TextTexture m_ObjectiveTexture;
	std::string m_ObjectiveString; // 지금 텍스처로 만들어 둔 목표 문구(바뀌면 다시 만든다)
	TextTexture m_ObjectiveHintTexture;
	std::string m_ObjectiveHintString; // 목표가 다른 지역에 있을 때 목표 문구 아래에 뜨는 목적지 안내
	TextTexture m_BannerTitleTexture;
	TextTexture m_BannerTipTexture;
	float m_BannerTimer = 0.f;
	float m_Time = 0.f;
};
