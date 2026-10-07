#pragma once

#include <map>
#include <set>
#include <string>
#include <vector>

class StoryState;

// 대화가 저절로 열리는 계기. 대화 블록의 "on <계기> <대상ID>" 줄로 정한다.
enum class DialogueTrigger
{
	None,    // 다른 대화의 GOTO로만 열린다
	Talk,    // 인물·물건에 말을 걸거나 조사했을 때(E)
	Pickup,  // 이야기 아이템을 주웠을 때(E) — 대화가 열리면 아이템은 사라진다
	Trigger, // 트리거 영역(제단 앞, 동굴 입구 등)에 들어섰을 때
	Kill,    // 그 ID의 괴물을 쓰러뜨렸을 때
	Enter,   // 그 지역(맵)에 들어섰을 때
};

struct DialogueLine
{
	std::string speaker; // 화자 ID(CHR_PYTHIA 등). "narr"는 이름 없는 지문, "PLAYER"는 플레이어
	std::string text;
};

struct DialogueChoice
{
	std::string condition;            // 비어 있지 않으면 이 조건이 참일 때만 선택지가 보인다
	std::string text;                 // "[진지] ..." 처럼 말투 꼬리표를 포함한 표시 문구
	std::vector<std::string> effects; // 고르면 적용할 효과들(SET/ADD/STEP/GOTO/...)
};

// 대화 블록 안의 한 "변형". 같은 블록이 여러 번 열리면 변형을 차례로 돌려 가며 보여준다(or 줄로 구분).
struct DialogueVariant
{
	std::vector<DialogueLine> lines;
	std::vector<DialogueChoice> choices;
	std::vector<std::string> effects; // do 줄: 대화가 끝날 때(선택지가 있으면 선택지 효과보다 먼저) 적용
};

struct DialogueBlock
{
	std::string id;
	DialogueTrigger trigger = DialogueTrigger::None;
	std::string target;                   // on 줄의 대상 ID
	std::vector<std::string> conditions;  // if 줄들 — 모두 참이어야 열린다
	bool once = false;                    // 한 번 본 뒤로는 다시 열리지 않는다
	std::vector<DialogueVariant> variants;
	int sourceLine = 0;                   // 데이터 파일에서 블록이 시작한 줄(오류 안내용)
};

// 진행 단계 선언(step 줄): 순서·화면 위 목표 문구·목표가 있는 지역·목표 표시 대상.
struct StoryStep
{
	std::string id;
	std::string region;                        // 목표가 있는 지역 ID(LOC_*). 비어 있으면 지역을 따지지 않는다
	std::vector<std::string> targets;          // 목표 표시(머리 위 금빛 마름모)를 띄울 스토리 ID들
	std::vector<std::string> targetConditions; // targets와 같은 순서. 비어 있지 않으면 그 조건이 참일 때만 표시("ID?조건")
	std::string objective;                     // 미니맵 아래에 보여줄 목표 문구({변수} 사용 가능)
	int sourceLine = 0;                        // 데이터 파일에서 선언한 줄(오류 안내용)
};

// 지역 선언(region 줄): 지역 ID별 표시 이름과, 들어설 때 함께 뜨는 헤르메스의 한마디.
struct StoryRegion
{
	std::string id;
	std::string name;
	std::string tip;
};

// 동료의 혼잣말(bark 줄): 동료가 곁에 있을 때 가끔 채팅창에 뜬다.
struct StoryBark
{
	std::string speaker;
	std::string condition;
	std::string text;
};

// Data/Story.txt를 읽어 대화·진행 단계·지역·혼잣말을 들고 있는 데이터베이스. 파일 형식은 Data/Story.txt의
// 머리말에 정리해 두었다. 읽다가 틀린 줄을 만나면 콘솔에 "[이야기 데이터] 줄 N: ..." 형식으로 알리고 그 줄만
// 건너뛴다(게임은 계속 돈다).
class DialogueDatabase
{
public:
	bool Load(const std::string& path);

	const DialogueBlock* FindById(const std::string& id) const;

	// trigger + target으로 열리는 블록 중 조건이 참이고(once면 아직 안 본) 파일에서 가장 먼저 나온 것.
	const DialogueBlock* FindTriggered(DialogueTrigger trigger, const std::string& target, const StoryState& story,
		const std::set<std::string>& seenOnce) const;

	// 이 trigger + target으로 열리는 블록이 하나라도 있는지(조건과 무관).
	bool HasTrigger(DialogueTrigger trigger, const std::string& target) const;

	bool PassesConditions(const DialogueBlock& block, const StoryState& story) const;

	// 화자 ID의 표시 이름(name 줄). 등록 안 됐으면 ID 그대로, "narr"처럼 이름 없이 등록했으면 빈 문자열.
	std::string GetSpeakerName(const std::string& speakerId) const;

	const std::vector<StoryStep>& GetSteps() const { return m_Steps; }
	const StoryStep* FindStep(const std::string& id) const;
	const std::vector<StoryRegion>& GetRegions() const { return m_Regions; }
	const StoryRegion* FindRegion(const std::string& id) const;
	const std::vector<StoryBark>& GetBarks() const { return m_Barks; }

private:
	void ParseLine(const std::string& line, int lineNumber);
	void Warn(int lineNumber, const std::string& message);

	// 다 읽은 뒤 GOTO/STEP 효과가 없는 대화·단계를 가리키지 않는지, step의 지역이 region으로 선언됐는지
	// 확인한다(오타 잡기).
	void Validate();

	std::vector<DialogueBlock> m_Blocks;
	std::map<std::string, size_t> m_BlockIndex;
	std::map<std::string, std::string> m_SpeakerNames;
	std::vector<StoryStep> m_Steps;
	std::vector<StoryRegion> m_Regions;
	std::vector<StoryBark> m_Barks;
	int m_ErrorCount = 0;
};
