#pragma once

#include <string>
#include <vector>

#include "Faith.h"

enum class AnimalKind;

// 하나로 이어진 월드의 배치표. 이야기 지역(STORY.md 4장의 LOC_* + 새 마을 테베)이 큰 월드 한 장 위의 어디에 놓이고,
// 어느 신을 숭배하며 분위기가 어떤지, 어느 쪽으로 길이 나 있는지를 한곳에 모은다. 지역끼리는 걸어서 오가고(이정표로
// 순간이동하지 않음), 이웃 마을 중심 사이는 약 90칸 — 플레이어 걸음(초당 4칸)으로 20초 남짓 걸린다. 마을과 마을 사이의
// 빈 땅(황야)엔 가장 가까운 마을에 속한 몬스터가 마을 주민 수만큼 산다(Wilderness.h).
//
// 지역의 로컬 좌표: 지역마다 예전처럼 원점을 가운데로 한 정사각형 맵(마을 48칸, 성역 32칸)으로 짓고, 월드에서는
// 중심(centerX, centerY)만큼 옮겨 놓는다. 표의 출구·도착 지점은 로컬 좌표다.
// 화면 방향(카메라 반전 설정 기준): 월드 x+y가 작을수록 화면 위(북쪽), y−x가 클수록 화면 오른쪽(동쪽).

// 마을의 분위기 — 숭배하는 신에 맞춘 마을 사람들의 성미. 주민의 다혈질 비율·돌아다니는 반경·어울리는 정도와
// 마을 위를 떠도는 빛 알갱이 색으로 드러난다(모니터에도 이름과 설명을 내보냄).
struct VillageMood
{
	const char* key;        // 영문 키(JSON)
	const char* label;      // 한글 이름
	const char* text;       // 한 줄 설명
	float hotheadedRatio;   // 주민 중 다혈질의 비율(적대 신앙을 보면 시비를 건다)
	float wanderRadius;     // 주민이 자기 집 둘레를 거니는 반경
	float sociability;      // 이웃과 어울리는 정도(NpcSociety의 만남 확률 배율, 1이 보통)
	float moteR, moteG, moteB; // 마을 위를 떠도는 빛 알갱이 색(1을 넘으면 블룸으로 빛남)
};

enum class PlaceKind
{
	Village,   // 주민이 사는 마을(주민 15명, 둘레 황야에 몬스터 15마리)
	Sanctuary, // 사람이 살지 않는 성역(올림포스 정상·타르타로스 경계 — 바위로 둘러싸여 한 길로만 들어가고, 이야기가 열어야 들어갈 수 있다)
	Stage,     // 월드 밖의 이야기 무대(크로노스의 배 속 — 이야기의 TRAVEL로만 드나든다)
};

// 지역에서 이웃 지역으로 나가는 길의 출구. 안쪽 점은 지역 안의 길·광장(지역 안 어디서든 걸어서 닿는 곳), 가장자리 점은
// 지역 정사각형의 테두리. 월드를 지을 때 안쪽 점 → 가장자리 → (황야를 지나) → 이웃의 가장자리 → 이웃의 안쪽 점으로 길을 깐다.
struct WorldExit
{
	const char* toLocId;
	float innerX, innerY;
	float borderX, borderY;
};

struct WorldPlace
{
	const char* locId;
	PlaceKind kind;
	float centerX, centerY;     // 월드 좌표(지역 로컬 원점)
	int size;                   // 지역 정사각형 한 변(타일, 짝수)
	float arrivalX, arrivalY;   // 마을 어귀(로컬): 이 지역에서 쓰러지면 되돌아오는 자리, 이야기로 옮겨질 때 서는 자리
	Faith worship;              // 숭배하는 신(성역은 None — 이름은 worshipLabel)
	const char* worshipLabel;   // 숭배하는 신의 한글 이름("아폴론", "올림포스 열두 신" 등)
	VillageMood mood;
	const char* residentId;     // 마을을 채우는 주민의 스토리 ID(대사는 Data/Story.txt) — 성역·무대는 nullptr
	const char* residentTag;    // 그 주민의 이름표(영문)
	Faith minorities[2];        // 숭배하는 신 말고 주민 일부(약 3할)가 믿는 신
	int minorityCount;
	AnimalKind wildKinds[3];    // 둘레 황야에 사는 몬스터 종류(앞의 것일수록 많다)
	int wildKindCount;
	WorldExit exits[3];
	int exitCount;
};

// 월드 좌표의 한 점.
struct WorldPoint
{
	float x, y;
};

// 이웃한 두 지역을 잇는 길(월드 좌표 꺾은선: a의 안쪽 점 → a의 가장자리 → 중간(살짝 굽음) → b의 가장자리 → b의 안쪽 점).
struct WorldRoad
{
	const WorldPlace* a;
	const WorldPlace* b;
	WorldPoint points[5];
};

namespace WorldLayout
{
	// 월드 타일맵의 크기(칸). 원점이 가운데다.
	const int kWorldWidth = 256;
	const int kWorldHeight = 352;

	// 마을마다 주민 수(= 그 마을 둘레 황야의 몬스터 수 — 1:1), 집 한 채에 사는 최대 인원.
	const int kResidentsPerVillage = 15;
	const int kMaxResidentsPerHouse = 3;

	int GetPlaceCount();
	const WorldPlace& GetPlace(int index);
	const WorldPlace* Find(const std::string& locId);

	// 이야기의 TRAVEL로만 드나드는 무대(월드 밖)인지.
	bool IsStage(const std::string& locId);

	// 월드 좌표가 들어 있는 지역(정사각형 안). 황야면 nullptr. 무대는 월드 밖이라 찾지 않는다.
	const WorldPlace* Containing(float x, float y);

	// 가장 가까운 마을(성역·무대 제외) — 황야의 몬스터가 어느 마을에 속하는지 정할 때.
	const WorldPlace* NearestVillage(float x, float y);

	// 로컬 좌표 → 월드 좌표.
	WorldPoint ToWorld(const WorldPlace& place, float localX, float localY);

	// 지역의 마을 어귀(쓰러지면 되돌아오는 자리)의 월드 좌표.
	WorldPoint ArrivalPoint(const WorldPlace& place);

	// 이웃 지역을 잇는 길 목록(월드를 지을 때 깔고, 길잡이가 따라간다).
	const std::vector<WorldRoad>& GetRoads();

	// (x, y)에서 targetLocId 지역까지 길을 따라갈 때 다음에 향할 곳(월드 좌표). 이미 그 지역 안이면 false.
	// 지역 안에선 다음 지역으로 나가는 출구를, 황야에선 가장 가까운 길을 따라 목표 쪽으로 다음 꺾임점을 가리킨다.
	bool NextWaypoint(float x, float y, const std::string& targetLocId, float& outX, float& outY);

	// 지역 바깥 방향 글: 이웃 지역으로 가는 길이 이 지역에서 어느 쪽(화면 기준 북·남·동·서)으로 나 있는지.
	const char* DirectionName(const WorldPlace& from, const WorldPlace& to);
}
