#pragma once

#include <chrono>
#include <cstdint>
#include <deque>
#include <map>
#include <string>
#include <vector>

#include "GameLog.h"
#include "PatronPowers.h"

class SceneGraph;
class StoryDirector;

// 월드 모니터: 게임 안의 상태를 같은 PC의 브라우저에서 볼 수 있게 내보낸다 — NPC 하나하나의 상태, 마을(지역)별
// 전체 상태, 몹·NPC 수, 플레이어, 이야기 진행, 최근 이벤트(채팅창 로그·진행 단계·지역 이동).
//
//  - 게임이 127.0.0.1(이 PC 안에서만 접속 가능)의 8765번부터 빈 포트를 찾아 아주 작은 HTTP 서버를 연다.
//      GET /            → 모니터 화면(작업 폴더 기준 ./Monitor/index.html을 요청마다 읽어 보냄 — 고치고 새로고침하면 바로 반영)
//      GET /api/state   → 지금 상태 JSON(아래 형식). ?since=N이면 이벤트는 번호가 N보다 큰 것만 보낸다.
//    읽기 전용이다(게임을 바꾸는 요청은 없음). 다른 출처에서 열어도(파일로 직접 연 index.html 등) 받을 수 있게 CORS를 연다.
//  - 소켓은 전부 논블로킹이고, 게임 루프가 매 프레임 Update에서 한 번씩 돌아본다. 스레드가 없으므로 씬·이야기 상태를
//    잠금 없이 그대로 읽는다(브라우저가 1초에 1~4번 묻는 정도라 비용은 작다 — 프로파일러 "모니터" 구간).
//  - 지역은 모두 하나로 이어진 월드 위에 함께 살아 있으므로 모든 마을을 실시간으로 집계한다(인물·몬스터·물건은 지은
//    지역 소속(Actor::GetHomeRegion), 황야의 몬스터는 가장 가까운 마을 소속). 이야기 무대(크로노스의 배 속)에 들어가
//    있는 동안엔 무대가 실시간이고 월드의 마을들은 들어가기 직전의 기록이다.
//  - 마을마다 숭배하는 신·분위기(WorldLayout.h), 주민 수(집이 있는 NPC)·집 수, 둘레 황야의 몬스터 수(주민과 1:1)를 함께 보낸다.
//
// JSON 형식(schema "world-monitor/1"). 나중에 다른 사람의 게임 월드와 합칠 때도 같은 형식으로 주고받을 수 있게,
// 월드 식별(world.id/instance)과 기계용 값(state 등 영문 키) + 사람용 값(stateLabel 등 한글)을 함께 담는다.
// {
//   "schema": "world-monitor/1",
//   "world":   { id, name, instance(실행마다 다른 수), time(게임 시각 초), fps, url },
//   "player":  { name(닉네임 — 정하기 전이면 빈 문자열), map, mapName, x, y, level, hp, maxHp, xp, xpToNext, attack, patron, patronName, blessing, blessingName,
//                passive, defense, critChance, lifeOnKill, cooldownScale, speedScale, buffRemaining, awakened,
//                skills: [ { slot(basic|ultimate), key(Q|R), name, summary, cooldown, remaining, unlocked } ] },
//   "story":   { step, objective, objectiveRegion, travelRegion, dialogueActive, variables{이름: 값} },
//   "villages": [ { id, name, shortName, kind(village|sanctuary|stage), live, visited, unlocked, objectiveHere, here, capturedAt, age,
//                   worship(신앙 키 — 성역은 NONE), worshipName, mood, moodLabel, moodText, x, y(월드 중심),
//                   status(calm|alert|danger|unvisited), statusLabel,
//                   npcCount, npcPresent, mobCount, mobPresent, mobHostile, mobChasing, mobsByKind{종류: 수},
//                   residents, residentQuota, houses, wildMonsters, wildQuota(주민과 1:1),
//                   kills, herbs, storyItems,
//                   npcs: [ { id, name, tag, x, y, distance(실시간만, 아니면 null), present, companion, faded, flaring,
//                             objective, talkable, state(idle|following|away|faded|flaring|hidden), stateLabel,
//                             faith(ZEUS…JUPITER|NONE), faithName, attitude(same|ally|friendly|neutral|wary|hostile|unknown — 플레이어의
//                             수호신을 대하는 태도), attitudeLabel, activity(idle|stroll|chat|argue|scuffle|brawl|follow),
//                             activityLabel, hotheaded, social, humbled, resident, house(집 번호, 없으면 0) } ],
//                   mobs: [ { id, kind(deer|wolf|shadowWolf|shadowPython|bronzeWarrior|kronosEcho|whiteCow|boar|bandit|seaSerpent|maenad),
//                             name, hp, maxHp, x, y, wild(황야의 몬스터인지),
//                             distance, present, hostile, chasing, returning, stunned, charmed, frightened, objective,
//                             state(roaming|chasing|returning|stunned|charmed|frightened|hidden), stateLabel } ] } ],
//   "totals":  { villages, visited, npcs, npcsPresent, mobs, mobsPresent, mobsHostile, mobsChasing, kills, residents, houses, wildMonsters },
//   "faiths":  { playerFaith, list: [ { key, name, patron(수호신으로 고를 수 있는지), npcs(기록된 신도 수) } ], noFaithNpcs,
//                relations: [ { a, b, value(-2~+2), key, label } ] (0이 아닌 관계만) },
//   "events":  [ { seq, time, type(log.dialogue|log.info|log.combat|log.damage|log.reward|story.step|world.travel|monitor.start),
//                  map, text } ],
//   "lastSeq": 가장 최근 이벤트 번호
// }
class WorldMonitor
{
public:
	WorldMonitor();
	~WorldMonitor();

	WorldMonitor(const WorldMonitor&) = delete;
	WorldMonitor& operator=(const WorldMonitor&) = delete;

	// 서버를 연다. 포트를 못 열어도 게임은 그대로 돈다(모니터만 꺼진 채로, 집계·이벤트 기록은 계속).
	bool Start();

	bool IsRunning() const { return m_Running; }
	std::string GetUrl() const;

	// 매 프레임 게임 갱신 뒤에: FPS 집계, 마을 집계(0.5초마다), 진행 단계·지역 변화 이벤트, 브라우저 요청 처리.
	// world는 하나로 이어진 월드, stage는 지금 들어가 있는 이야기 무대(없으면 nullptr — 그동안 월드는 멈춰 있다).
	void Update(float deltaSeconds, SceneGraph& world, SceneGraph* stage, const StoryDirector& director);

	// 월드의 모든 마을(무대에 들어가 있으면 무대도)을 바로 집계한다. 무대로 들어가기 직전에도 불러서 월드의 마지막
	// 모습을 남긴다.
	void Capture(SceneGraph& world, const StoryDirector& director);

	// 수호신 스킬(Q·R)의 남은 쿨타임과 그 쿨타임의 전체 길이(초, SkillSlot 순서). 매 프레임 Update 전에 넘긴다.
	void SetSkillCooldowns(const float remaining[], const float total[]);

	// 채팅창 로그(GameLog 리스너에서)와 몹 처치(지역별 처치 수)를 기록한다.
	void RecordLog(GameLog::Kind kind, const std::string& text);
	void RecordKill(const std::string& locId);

	// 기본 브라우저로 모니터 화면을 연다(F2).
	void OpenInBrowser();

	// 게임을 켤 때: delaySeconds 뒤에 모니터 화면을 브라우저로 연다. 그 사이에 이미 열려 있던 모니터 화면(이전 실행에서
	// 열어 둔 탭)이 다시 붙어 오면(/api/state 요청) 새 탭을 또 열지 않는다 — 게임을 켤 때마다 탭이 쌓이지 않게.
	void RequestAutoOpen(float delaySeconds);

private:
	// 집계한 NPC 하나(그때의 모습).
	struct NpcStatus
	{
		std::string id;   // 스토리 ID(CHR_/NPC_), 없으면 빈 문자열
		std::string name; // 한글 이름(데이터 파일 name 줄)
		std::string tag;  // 머리 위 영문 이름표
		float x = 0.f;
		float y = 0.f;
		float distance = 0.f; // 집계할 때 플레이어와의 거리
		bool present = true;  // 이야기 조건으로 지금 세상에 있는지
		bool companion = false;
		bool faded = false;
		bool flaring = false;
		int faith = 0;    // Faith(믿는 신)
		int activity = 0; // NpcActivity(지금 하는 일)
		bool hotheaded = false;
		bool social = false;
		bool humbled = false; // 플레이어와의 주먹다짐에서 졌다
		int house = 0;        // 사는 집 번호(마을 주민), 0이면 집 없음
	};

	// 집계한 짐승·괴물 하나.
	struct MobStatus
	{
		std::string id; // 이야기 속 괴물이면 스토리 ID(MON_)
		int kind = 0;   // AnimalKind
		int hp = 0;
		int maxHp = 0;
		float x = 0.f;
		float y = 0.f;
		float distance = 0.f;
		bool present = true;
		bool hostile = false;
		bool chasing = false;
		bool returning = false;
		bool stunned = false;
		bool charmed = false;
		bool frightened = false;
		bool wild = false; // 황야의 몬스터(마을 주민과 1:1)
	};

	// 마을(지역) 하나의 마지막 집계.
	struct VillageStatus
	{
		bool captured = false;
		float capturedAt = 0.f; // 게임 시각(초)
		std::vector<NpcStatus> npcs;
		std::vector<MobStatus> mobs;
		int herbs = 0;      // 남은 약초
		int storyItems = 0; // 지금 주울 수 있는 이야기 물건
		int kills = 0;      // 이번 실행에서 이 지역(황야 포함)에서 쓰러뜨린 몹 수
		int houses = 0;     // 사람이 사는 집 수
	};

	// 한 씬(월드 또는 무대)의 액터를 지역별로 나눠 담는다. stageId가 비어 있지 않으면 모두 그 무대 소속.
	void CaptureScene(SceneGraph& scene, const std::string& stageId, const StoryDirector& director);

	struct Event
	{
		unsigned long long seq = 0;
		float time = 0.f;
		std::string type;
		std::string map;
		std::string text;
	};

	// 브라우저 접속 하나: 요청을 다 읽으면 응답을 만들어 보내고, 다 보내면 상대가 닫을 때까지 남은 걸 버리며 기다린다.
	struct Connection
	{
		enum class Phase
		{
			Reading,
			Sending,
			Draining,
		};

		std::uintptr_t socket = 0; // SOCKET(헤더에 winsock2.h를 끌어들이지 않으려고 정수로 든다)
		Phase phase = Phase::Reading;
		std::string request;
		std::string response;
		size_t sent = 0;
		float age = 0.f;
	};

	void AddEvent(const std::string& type, const std::string& text);
	void TrackStoryChanges(const StoryDirector& director);

	void PollNetwork(float deltaSeconds, SceneGraph& scene, const StoryDirector& director);

	// 접속 하나를 한 걸음 진행한다. 끝났으면(닫아야 하면) true.
	bool ServiceConnection(Connection& connection, float deltaSeconds, SceneGraph& scene, const StoryDirector& director);

	std::string BuildResponse(const std::string& request, SceneGraph& scene, const StoryDirector& director);
	std::string BuildStateJson(const StoryDirector& director, unsigned long long sinceSeq) const;

	bool m_Running = false;
	bool m_WinsockStarted = false;
	std::uintptr_t m_ListenSocket = 0;
	int m_Port = 0;
	unsigned int m_Instance = 0;
	std::vector<Connection> m_Connections;

	float m_Time = 0.f;
	float m_CaptureTimer = 0.f;
	SceneGraph* m_Stage = nullptr; // 지금 들어가 있는 이야기 무대(Update가 넘겨받음)

	// 시작할 때 자동으로 열기(RequestAutoOpen).
	bool m_AutoOpenPending = false;
	float m_AutoOpenAt = 0.f;
	bool m_StateRequested = false; // 이번 실행에서 /api/state 요청을 한 번이라도 받았는지
	std::map<std::string, VillageStatus> m_Villages;

	// 마지막으로 집계할 때의 플레이어.
	float m_PlayerX = 0.f;
	float m_PlayerY = 0.f;
	int m_PlayerLevel = 0;
	int m_PlayerHp = 0;
	int m_PlayerMaxHp = 0;
	int m_PlayerXp = 0;
	int m_PlayerXpToNext = 0;
	int m_PlayerAttack = 0;
	int m_PlayerDefense = 0;
	float m_PlayerCrit = 0.f;
	int m_PlayerLifeOnKill = 0;
	float m_PlayerCooldownScale = 1.f;
	float m_PlayerBuff = 0.f; // 남은 버프 시간(「신속의 날개」)

	// 수호신 스킬의 쿨타임(SetSkillCooldowns).
	float m_SkillRemaining[(int)SkillSlot::Count] = {};
	float m_SkillTotal[(int)SkillSlot::Count] = {};

	// 이벤트(최근 것만 kMaxEvents개).
	std::deque<Event> m_Events;
	unsigned long long m_NextSeq = 1;
	std::string m_CurrentMap;
	std::string m_LastStep;

	// FPS(실제 시각 기준, 1초마다 갱신).
	std::chrono::steady_clock::time_point m_FpsStart;
	bool m_FpsStarted = false;
	int m_FpsFrames = 0;
	float m_Fps = 0.f;
};
