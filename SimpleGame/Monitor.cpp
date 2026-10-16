#include "stdafx.h"

// winsock2.h는 windows.h보다 먼저 들어와야 해서(windows.h가 옛 winsock.h를 끌어들이면 정의가 겹친다) 맨 위에 둔다.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <shellapi.h>

#include "Monitor.h"

#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <random>
#include <sstream>

#include "CharacterActors.h"
#include "Faith.h"
#include "LevelBuilder.h"
#include "SceneGraph.h"
#include "StoryDirector.h"

#pragma comment(lib, "Ws2_32.lib")

namespace
{
	const int kFirstPort = 8765;
	const int kPortAttempts = 10;              // 8765~8774 중 처음으로 열리는 포트를 쓴다
	const float kCaptureInterval = 0.5f;       // 지금 지역을 집계하는 간격(초)
	const size_t kMaxEvents = 300;             // 기억해 두는 최근 이벤트 수
	const size_t kMaxConnections = 16;         // 동시에 붙잡고 있는 브라우저 접속 수
	const size_t kMaxRequestBytes = 16 * 1024; // 이보다 긴 요청은 받지 않는다
	const float kConnectionTimeout = 5.f;      // 접속 하나가 이보다 오래 걸리면 닫는다(초)
	const char* kIndexPath = "./Monitor/index.html";

	const char* kSchema = "world-monitor/1";
	const char* kWorldId = "simplegame-twilight-oracle";
	const char* kWorldName = "황혼의 신탁";

	// JSON 문자열(따옴표 포함)로 덧붙인다. UTF-8 바이트는 그대로 두고 따옴표·역슬래시·제어 문자만 이스케이프한다.
	void AppendJsonString(std::string& out, const std::string& text)
	{
		out += '"';
		for (char ch : text)
		{
			unsigned char c = (unsigned char)ch;
			switch (c)
			{
			case '"': out += "\\\""; break;
			case '\\': out += "\\\\"; break;
			case '\n': out += "\\n"; break;
			case '\r': out += "\\r"; break;
			case '\t': out += "\\t"; break;
			default:
				if (c < 0x20)
				{
					char buffer[8];
					snprintf(buffer, sizeof(buffer), "\\u%04x", (unsigned int)c);
					out += buffer;
				}
				else
				{
					out += ch;
				}
				break;
			}
		}
		out += '"';
	}

	// 쉼표를 알아서 넣어 주는 작은 JSON 작성기. 객체 안에서는 key를, 배열 안에서는 nullptr를 넘긴다.
	class JsonWriter
	{
	public:
		const std::string& Text() const { return m_Text; }

		void BeginObject(const char* key) { Key(key); m_Text += '{'; m_First.push_back(true); }
		void EndObject() { m_Text += '}'; m_First.pop_back(); }
		void BeginArray(const char* key) { Key(key); m_Text += '['; m_First.push_back(true); }
		void EndArray() { m_Text += ']'; m_First.pop_back(); }

		void String(const char* key, const std::string& value)
		{
			Key(key);
			AppendJsonString(m_Text, value);
		}

		void Number(const char* key, double value, int decimals = 2)
		{
			Key(key);
			if (!std::isfinite(value))
			{
				m_Text += '0';
				return;
			}

			char buffer[64];
			snprintf(buffer, sizeof(buffer), "%.*f", decimals, value);
			m_Text += buffer;
		}

		void Int(const char* key, long long value)
		{
			Key(key);
			m_Text += std::to_string(value);
		}

		void Bool(const char* key, bool value)
		{
			Key(key);
			m_Text += value ? "true" : "false";
		}

		void Null(const char* key)
		{
			Key(key);
			m_Text += "null";
		}

	private:
		void Key(const char* key)
		{
			if (!m_First.empty())
			{
				if (!m_First.back())
				{
					m_Text += ',';
				}
				m_First.back() = false;
			}

			if (key != nullptr)
			{
				AppendJsonString(m_Text, key);
				m_Text += ':';
			}
		}

		std::string m_Text;
		std::vector<bool> m_First; // 열린 객체·배열마다 "아직 첫 항목인지"
	};

	const char* AnimalKindKey(AnimalKind kind)
	{
		switch (kind)
		{
		case AnimalKind::Deer: return "deer";
		case AnimalKind::Wolf: return "wolf";
		case AnimalKind::ShadowWolf: return "shadowWolf";
		case AnimalKind::ShadowPython: return "shadowPython";
		case AnimalKind::BronzeWarrior: return "bronzeWarrior";
		case AnimalKind::KronosEcho: return "kronosEcho";
		case AnimalKind::WhiteCow: return "whiteCow";
		}
		return "unknown";
	}

	// NPC가 지금 하는 일(NpcActivity)의 JSON 키·한글 이름.
	const char* ActivityKey(NpcActivity activity)
	{
		switch (activity)
		{
		case NpcActivity::Idle: return "idle";
		case NpcActivity::Stroll: return "stroll";
		case NpcActivity::Chat: return "chat";
		case NpcActivity::Argue: return "argue";
		case NpcActivity::Scuffle: return "scuffle";
		case NpcActivity::Brawl: return "brawl";
		case NpcActivity::Follow: return "follow";
		}
		return "idle";
	}

	const char* ActivityLabel(NpcActivity activity)
	{
		switch (activity)
		{
		case NpcActivity::Idle: return "제자리";
		case NpcActivity::Stroll: return "거니는 중";
		case NpcActivity::Chat: return "수다 중";
		case NpcActivity::Argue: return "말다툼 중";
		case NpcActivity::Scuffle: return "주먹다짐(이웃과)";
		case NpcActivity::Brawl: return "플레이어와 주먹다짐";
		case NpcActivity::Follow: return "동행 중";
		}
		return "제자리";
	}

	const char* LogEventType(GameLog::Kind kind)
	{
		switch (kind)
		{
		case GameLog::Kind::Dialogue: return "log.dialogue";
		case GameLog::Kind::Info: return "log.info";
		case GameLog::Kind::Combat: return "log.combat";
		case GameLog::Kind::Damage: return "log.damage";
		case GameLog::Kind::Reward: return "log.reward";
		}
		return "log.info";
	}

	bool ReadWholeFile(const char* path, std::string& out)
	{
		std::ifstream file(path, std::ios::binary);
		if (!file.is_open())
		{
			return false;
		}

		std::ostringstream contents;
		contents << file.rdbuf();
		out = contents.str();
		return true;
	}

	// 응답 한 통(상태 줄 + 헤더 + 본문). 한 번 보내고 닫으므로 Connection: close.
	std::string HttpResponse(int status, const char* reason, const char* contentType, const std::string& body)
	{
		std::string response = "HTTP/1.1 " + std::to_string(status) + " " + reason + "\r\n";
		if (contentType != nullptr)
		{
			response += std::string("Content-Type: ") + contentType + "\r\n";
		}
		response += "Content-Length: " + std::to_string(body.size()) + "\r\n";
		response += "Cache-Control: no-store\r\n";

		// 파일로 직접 연 모니터 화면(다른 출처)에서도 받을 수 있게 한다. 읽기 전용이고 이 PC 안에서만 열린 서버다.
		response += "Access-Control-Allow-Origin: *\r\n";
		response += "Access-Control-Allow-Methods: GET, OPTIONS\r\n";
		response += "Access-Control-Allow-Private-Network: true\r\n";
		response += "Connection: close\r\n\r\n";
		response += body;
		return response;
	}
}

WorldMonitor::WorldMonitor()
{
	// 실행마다 다른 수: 브라우저가 게임이 다시 켜진 걸 알아채고 이벤트 목록을 새로 받게 한다.
	m_Instance = std::random_device{}();
}

WorldMonitor::~WorldMonitor()
{
	for (Connection& connection : m_Connections)
	{
		closesocket((SOCKET)connection.socket);
	}
	m_Connections.clear();

	if (m_Running)
	{
		closesocket((SOCKET)m_ListenSocket);
		m_Running = false;
	}

	if (m_WinsockStarted)
	{
		WSACleanup();
	}
}

bool WorldMonitor::Start()
{
	WSADATA data;
	if (WSAStartup(MAKEWORD(2, 2), &data) != 0)
	{
		std::cout << "[모니터] 네트워크(Winsock)를 시작하지 못해 모니터를 끕니다.\n";
		return false;
	}
	m_WinsockStarted = true;

	for (int attempt = 0; attempt < kPortAttempts; ++attempt)
	{
		int port = kFirstPort + attempt;

		SOCKET listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (listener == INVALID_SOCKET)
		{
			break;
		}

		// 127.0.0.1에만 묶어서 이 PC 안에서만 접속할 수 있게 한다(다른 PC에서는 보이지 않는다).
		sockaddr_in address = {};
		address.sin_family = AF_INET;
		address.sin_port = htons((u_short)port);
		address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

		if (bind(listener, (const sockaddr*)&address, (int)sizeof(address)) == 0 && listen(listener, SOMAXCONN) == 0)
		{
			// 논블로킹: 기다리는 접속이 없으면 accept가 바로 돌아와서 게임 루프를 막지 않는다.
			u_long nonBlocking = 1;
			ioctlsocket(listener, FIONBIO, &nonBlocking);

			m_ListenSocket = (std::uintptr_t)listener;
			m_Port = port;
			m_Running = true;

			std::cout << "[모니터] " << GetUrl() << " 에서 월드 상태를 볼 수 있습니다(F2로 브라우저 열기)\n";
			AddEvent("monitor.start", "모니터 시작: " + GetUrl());
			return true;
		}

		closesocket(listener);
	}

	std::cout << "[모니터] " << kFirstPort << "~" << (kFirstPort + kPortAttempts - 1)
		<< "번 포트를 열지 못해 모니터 서버를 끕니다(다른 프로그램이 쓰는 중일 수 있음).\n";
	return false;
}

std::string WorldMonitor::GetUrl() const
{
	return m_Running ? "http://127.0.0.1:" + std::to_string(m_Port) + "/" : std::string();
}

void WorldMonitor::Update(float deltaSeconds, SceneGraph& scene, const StoryDirector& director)
{
	m_Time += deltaSeconds;

	// FPS는 게임 시간(한 프레임 최대 0.1초로 잘림)이 아니라 실제 시각으로 잰다.
	std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
	if (!m_FpsStarted)
	{
		m_FpsStart = now;
		m_FpsStarted = true;
		m_FpsFrames = 0;
	}

	++m_FpsFrames;
	float elapsed = std::chrono::duration<float>(now - m_FpsStart).count();
	if (elapsed >= 1.f)
	{
		m_Fps = (float)m_FpsFrames / elapsed;
		m_FpsStart = now;
		m_FpsFrames = 0;
	}

	TrackStoryChanges(director);

	m_CaptureTimer -= deltaSeconds;
	if (m_CaptureTimer <= 0.f)
	{
		Capture(scene, director);
		m_CaptureTimer = kCaptureInterval;
	}

	if (m_Running)
	{
		PollNetwork(deltaSeconds, scene, director);
	}

	// 시작할 때 자동으로 열기: 기다리는 동안 이전 실행의 모니터 탭이 다시 붙어 왔으면 그 탭을 그대로 쓴다.
	if (m_AutoOpenPending && m_Time >= m_AutoOpenAt)
	{
		m_AutoOpenPending = false;
		if (m_StateRequested)
		{
			GameLog::Add(GameLog::Kind::Info, "[모니터] 이미 열려 있던 모니터 화면이 다시 연결됐다.");
		}
		else
		{
			OpenInBrowser();
		}
	}
}

void WorldMonitor::RequestAutoOpen(float delaySeconds)
{
	if (!m_Running)
	{
		return;
	}

	m_AutoOpenPending = true;
	m_AutoOpenAt = m_Time + delaySeconds;
}

void WorldMonitor::Capture(SceneGraph& scene, const StoryDirector& director)
{
	const std::string& map = director.GetStory().Get("MAP");
	if (map.empty())
	{
		return;
	}

	VillageStatus& village = m_Villages[map];
	village.captured = true;
	village.capturedAt = m_Time;
	village.npcs.clear();
	village.mobs.clear();
	village.herbs = 0;
	village.storyItems = 0;

	PlayerActor* player = scene.GetPlayer();
	if (player != nullptr)
	{
		const PlayerStats& stats = player->GetStats();
		m_PlayerX = player->GetWorldX();
		m_PlayerY = player->GetWorldY();
		m_PlayerLevel = stats.level;
		m_PlayerHp = stats.hp;
		m_PlayerMaxHp = stats.maxHp;
		m_PlayerXp = stats.xp;
		m_PlayerXpToNext = stats.xpToNext;
		m_PlayerAttack = stats.attackPower;
		m_PlayerDefense = stats.defense;
		m_PlayerCrit = stats.critChance;
		m_PlayerLifeOnKill = stats.lifeOnKill;
		m_PlayerCooldownScale = player->GetCooldownScale();
		m_PlayerBuff = player->GetBuffRemaining();
	}

	float playerX = m_PlayerX;
	float playerY = m_PlayerY;

	scene.ForEach([&](Actor& actor)
	{
		if (actor.IsPendingDestroy())
		{
			return;
		}

		const char* storyId = actor.GetStoryId();
		std::string id = (storyId != nullptr) ? std::string(storyId) : std::string();
		float x = actor.GetWorldX();
		float y = actor.GetWorldY();
		float dx = x - playerX;
		float dy = y - playerY;
		float distance = sqrtf(dx * dx + dy * dy);

		switch (actor.GetType())
		{
		case ActorType::NPC:
			{
				const NpcActor& npc = static_cast<const NpcActor&>(actor);

				NpcStatus status;
				status.id = id;
				status.tag = (actor.GetName() != nullptr) ? actor.GetName() : "";
				status.name = id.empty() ? status.tag : director.GetSpeakerName(id);
				status.x = x;
				status.y = y;
				status.distance = distance;
				status.present = actor.IsVisible();
				status.companion = npc.IsFollowing();
				status.faded = npc.IsFaded();
				status.flaring = npc.IsFlaring();
				status.faith = (int)npc.GetTraits().faith;
				status.activity = (int)npc.GetActivity();
				status.hotheaded = npc.GetTraits().hotheaded;
				status.social = npc.GetTraits().social;
				status.humbled = npc.Memory().humbled;
				village.npcs.push_back(status);
			}
			break;

		case ActorType::Animal:
			{
				const AnimalActor& animal = static_cast<const AnimalActor&>(actor);

				MobStatus status;
				status.id = id;
				status.kind = (int)animal.GetKind();
				status.hp = animal.GetHp();
				status.maxHp = animal.GetMaxHp();
				status.x = x;
				status.y = y;
				status.distance = distance;
				status.present = actor.IsVisible();
				status.hostile = animal.IsAggressive();
				status.chasing = animal.IsChasing();
				status.returning = animal.IsReturning();
				status.stunned = animal.IsStunned();
				status.charmed = animal.IsCharmed();
				status.frightened = animal.IsFrightened();
				village.mobs.push_back(status);
			}
			break;

		case ActorType::Item:
			if (actor.IsVisible())
			{
				if (actor.GetInteractId() == kInteractLoot)
				{
					++village.herbs;
				}
				else if (actor.GetInteractId() == kInteractPickup)
				{
					++village.storyItems;
				}
			}
			break;

		default:
			break;
		}
	});
}

void WorldMonitor::RecordLog(GameLog::Kind kind, const std::string& text)
{
	AddEvent(LogEventType(kind), text);
}

void WorldMonitor::SetSkillCooldowns(const float remaining[], const float total[])
{
	for (int i = 0; i < (int)SkillSlot::Count; ++i)
	{
		m_SkillRemaining[i] = remaining[i];
		m_SkillTotal[i] = total[i];
	}
}

void WorldMonitor::RecordKill(
const std::string& locId)
{
	if (!locId.empty())
	{
		++m_Villages[locId].kills;
	}
}

void WorldMonitor::OpenInBrowser()
{
	if (!m_Running)
	{
		GameLog::Add(GameLog::Kind::Info, "[모니터] 모니터 서버가 꺼져 있어 열 수 없다(콘솔 안내 참고).");
		return;
	}

	// 주소는 ASCII뿐이라 글자를 그대로 넓혀도 된다.
	std::string url = GetUrl();
	std::wstring wideUrl(url.begin(), url.end());
	ShellExecuteW(nullptr, L"open", wideUrl.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
	GameLog::Add(GameLog::Kind::Info, "[모니터] 브라우저에서 " + url + " 을 열었다.");
}

void WorldMonitor::AddEvent(const std::string& type, const std::string& text)
{
	Event event;
	event.seq = m_NextSeq++;
	event.time = m_Time;
	event.type = type;
	event.map = m_CurrentMap;
	event.text = text;
	m_Events.push_back(event);

	while (m_Events.size() > kMaxEvents)
	{
		m_Events.pop_front();
	}
}

// 지역이 바뀌거나 진행 단계가 넘어가면 이벤트로 남긴다(채팅창에 따로 안 뜨는 변화).
void WorldMonitor::TrackStoryChanges(const StoryDirector& director)
{
	const StoryState& story = director.GetStory();

	const std::string& map = story.Get("MAP");
	if (map != m_CurrentMap)
	{
		m_CurrentMap = map;
		if (!map.empty())
		{
			AddEvent("world.travel", "지역 도착: " + director.GetRegionName(map));
		}
	}

	const std::string& step = story.Get("STORY_STEP");
	if (step != m_LastStep)
	{
		m_LastStep = step;

		const std::string& objective = director.GetObjectiveText();
		AddEvent("story.step", objective.empty() ? "진행 단계: " + step : "진행 단계: " + step + " — " + objective);
	}
}

void WorldMonitor::PollNetwork(float deltaSeconds, SceneGraph& scene, const StoryDirector& director)
{
	SOCKET listener = (SOCKET)m_ListenSocket;

	// 1) 새 접속을 받는다(논블로킹이라 기다리는 접속이 없으면 바로 INVALID_SOCKET이 돌아온다).
	for (;;)
	{
		SOCKET client = accept(listener, nullptr, nullptr);
		if (client == INVALID_SOCKET)
		{
			break;
		}

		if (m_Connections.size() >= kMaxConnections)
		{
			closesocket(client);
			continue;
		}

		u_long nonBlocking = 1;
		ioctlsocket(client, FIONBIO, &nonBlocking);

		Connection connection;
		connection.socket = (std::uintptr_t)client;
		m_Connections.push_back(connection);
	}

	// 2) 접속마다 한 걸음씩: 요청 읽기 → 응답 보내기 → 상대가 닫기를 기다리기.
	for (size_t i = 0; i < m_Connections.size(); )
	{
		if (ServiceConnection(m_Connections[i], deltaSeconds, scene, director))
		{
			closesocket((SOCKET)m_Connections[i].socket);
			m_Connections.erase(m_Connections.begin() + (std::ptrdiff_t)i);
		}
		else
		{
			++i;
		}
	}
}

bool WorldMonitor::ServiceConnection(Connection& connection, float deltaSeconds, SceneGraph& scene, const StoryDirector& director)
{
	SOCKET client = (SOCKET)connection.socket;
	char buffer[4096];

	connection.age += deltaSeconds;
	if (connection.age > kConnectionTimeout)
	{
		return true;
	}

	if (connection.phase == Connection::Phase::Reading)
	{
		for (;;)
		{
			int received = recv(client, buffer, (int)sizeof(buffer), 0);
			if (received > 0)
			{
				connection.request.append(buffer, (size_t)received);
				if (connection.request.size() > kMaxRequestBytes)
				{
					return true;
				}
				continue;
			}

			if (received == 0 || WSAGetLastError() != WSAEWOULDBLOCK)
			{
				return true; // 상대가 닫았거나 오류
			}
			break; // 지금은 더 받을 게 없음
		}

		// 요청 헤더가 끝날 때(빈 줄)까지 기다린다. GET만 받으므로 본문은 없다.
		if (connection.request.find("\r\n\r\n") == std::string::npos)
		{
			return false;
		}

		connection.response = BuildResponse(connection.request, scene, director);
		connection.phase = Connection::Phase::Sending;
	}

	if (connection.phase == Connection::Phase::Sending)
	{
		while (connection.sent < connection.response.size())
		{
			int sent = send(client, connection.response.data() + connection.sent, (int)(connection.response.size() - connection.sent), 0);
			if (sent > 0)
			{
				connection.sent += (size_t)sent;
				continue;
			}

			if (sent == SOCKET_ERROR && WSAGetLastError() == WSAEWOULDBLOCK)
			{
				return false; // 보낼 자리가 날 때까지 다음 프레임에
			}
			return true;
		}

		// 다 보냈으면 "더 보낼 것 없음"만 알리고, 상대가 닫을 때까지 남은 입력을 읽어 버린다(읽지 않은 입력을 둔 채
		// 바로 닫으면 연결이 강제로 끊겨 응답 끝이 잘릴 수 있다).
		shutdown(client, SD_SEND);
		connection.phase = Connection::Phase::Draining;
	}

	for (;;)
	{
		int received = recv(client, buffer, (int)sizeof(buffer), 0);
		if (received > 0)
		{
			continue;
		}

		if (received == SOCKET_ERROR && WSAGetLastError() == WSAEWOULDBLOCK)
		{
			return false;
		}
		return true; // 상대가 닫았거나 오류
	}
}

std::string WorldMonitor::BuildResponse(const std::string& request, SceneGraph& scene, const StoryDirector& director)
{
	// 요청 줄: "GET /api/state?since=12 HTTP/1.1"
	std::string line = request.substr(0, request.find("\r\n"));
	size_t firstSpace = line.find(' ');
	size_t secondSpace = (firstSpace == std::string::npos) ? std::string::npos : line.find(' ', firstSpace + 1);
	if (secondSpace == std::string::npos)
	{
		return HttpResponse(400, "Bad Request", "text/plain; charset=utf-8", "잘못된 요청입니다.");
	}

	std::string method = line.substr(0, firstSpace);
	std::string target = line.substr(firstSpace + 1, secondSpace - firstSpace - 1);
	std::string path = target;
	std::string query;

	size_t question = target.find('?');
	if (question != std::string::npos)
	{
		path = target.substr(0, question);
		query = target.substr(question + 1);
	}

	if (method == "OPTIONS")
	{
		return HttpResponse(204, "No Content", nullptr, std::string());
	}
	if (method != "GET")
	{
		return HttpResponse(405, "Method Not Allowed", "text/plain; charset=utf-8", "GET 요청만 받습니다.");
	}

	if (path == "/" || path == "/index.html")
	{
		// 요청마다 파일을 새로 읽는다(화면을 고친 뒤 게임을 다시 켜지 않고 새로고침만 해도 반영되게).
		std::string page;
		if (!ReadWholeFile(kIndexPath, page))
		{
			return HttpResponse(404, "Not Found", "text/plain; charset=utf-8",
				std::string("모니터 화면 파일(") + kIndexPath + ")을 찾지 못했습니다. 게임을 SimpleGame 프로젝트 폴더(작업 폴더)에서 실행했는지 확인하세요.");
		}
		return HttpResponse(200, "OK", "text/html; charset=utf-8", page);
	}

	if (path == "/api/state")
	{
		unsigned long long since = 0;
		size_t found = query.find("since=");
		if (found != std::string::npos)
		{
			since = strtoull(query.c_str() + found + 6, nullptr, 10);
		}

		// 묻는 순간의 최신 상태로 지금 지역을 다시 집계해서 보낸다.
		m_StateRequested = true;
		Capture(scene, director);
		return HttpResponse(200, "OK", "application/json; charset=utf-8", BuildStateJson(director, since));
	}

	if (path == "/favicon.ico")
	{
		return HttpResponse(204, "No Content", nullptr, std::string());
	}

	return HttpResponse(404, "Not Found", "application/json; charset=utf-8", "{\"error\":\"not found\"}");
}

std::string WorldMonitor::BuildStateJson(const StoryDirector& director, unsigned long long sinceSeq) const
{
	const StoryState& story = director.GetStory();
	const std::string& here = story.Get("MAP");
	std::string objectiveRegion = director.GetObjectiveRegion();

	JsonWriter json;
	json.BeginObject(nullptr);
	json.String("schema", kSchema);

	json.BeginObject("world");
	json.String("id", kWorldId);
	json.String("name", kWorldName);
	json.Int("instance", (long long)m_Instance);
	json.Number("time", m_Time, 1);
	json.Number("fps", m_Fps, 1);
	json.String("url", GetUrl());
	json.EndObject();

	json.BeginObject("player");
	json.String("map", here);
	json.String("mapName", director.GetRegionName(here));
	json.Number("x", m_PlayerX);
	json.Number("y", m_PlayerY);
	json.Int("level", m_PlayerLevel);
	json.Int("hp", m_PlayerHp);
	json.Int("maxHp", m_PlayerMaxHp);
	json.Int("xp", m_PlayerXp);
	json.Int("xpToNext", m_PlayerXpToNext);
	json.Int("attack", m_PlayerAttack);
	json.String("patron", story.Get("PATRON"));
	json.Bool("blessing", story.IsTrue("BLESSING_GRANTED"));

	// 수호신의 상시 능력치와 스킬(PatronPowers.h의 표). 축복을 받기 전이면 이름·설명은 빈 문자열.
	const PatronDef* patronDef = story.IsTrue("BLESSING_GRANTED") ? PatronPowers::Find(story.Get("PATRON")) : nullptr;
	bool awakened = story.IsTrue("AWAKENED");
	json.String("patronName", (patronDef != nullptr) ? patronDef->godName : "");
	json.String("blessingName", (patronDef != nullptr) ? patronDef->blessingName : "");
	json.String("passive", (patronDef != nullptr) ? patronDef->passiveText : "");
	json.Int("defense", m_PlayerDefense);
	json.Number("critChance", m_PlayerCrit);
	json.Int("lifeOnKill", m_PlayerLifeOnKill);
	json.Number("cooldownScale", m_PlayerCooldownScale);
	json.Number("speedScale", (patronDef != nullptr) ? patronDef->speedScale : 1.f);
	json.Number("buffRemaining", m_PlayerBuff, 1);
	json.Bool("awakened", awakened);
	json.BeginArray("skills");
	if (patronDef != nullptr)
	{
		for (int i = 0; i < (int)SkillSlot::Count; ++i)
		{
			SkillSlot slot = (SkillSlot)i;
			const SkillDef& skill = PatronPowers::Skill(*patronDef, slot);
			bool ultimate = (slot == SkillSlot::Ultimate);

			json.BeginObject(nullptr);
			json.String("slot", ultimate ? "ultimate" : "basic");
			json.String("key", PatronPowers::KeyName(slot));
			json.String("name", skill.name);
			json.String("summary", skill.summary);
			// 쿨타임 길이: 마지막으로 쓴 때의 실제 길이(헤르메스의 날개로 짧아졌을 수 있음), 아직 안 썼으면 지금 쓰면 걸릴 길이.
			float configured = ultimate ? PatronPowers::kUltimateCooldown : skill.cooldown * m_PlayerCooldownScale;
			json.Number("cooldown", (m_SkillTotal[i] > 0.f) ? m_SkillTotal[i] : configured, 1);
			json.Number("remaining", m_SkillRemaining[i], 1);
			json.Bool("unlocked", !ultimate || awakened);
			json.EndObject();
		}
	}
	json.EndArray();
	json.EndObject();

	json.BeginObject("story");
	json.String("step", story.Get("STORY_STEP"));
	json.String("objective", director.GetObjectiveText());
	json.String("objectiveRegion", objectiveRegion);
	json.String("travelRegion", director.GetTravelRegion());
	json.Bool("dialogueActive", director.IsDialogueActive());
	json.BeginObject("variables");
	for (const std::pair<const std::string, std::string>& entry : story.GetValues())
	{
		json.String(entry.first.c_str(), entry.second);
	}
	json.EndObject();
	json.EndObject();

	// 플레이어의 신앙 = 고른 수호신(축복을 받기 전이면 없음). NPC의 태도·관계도에 쓴다.
	Faith playerFaith = story.IsTrue("BLESSING_GRANTED") ? FaithInfo::FromPatron(story.Get("PATRON")) : Faith::None;

	// ---- 마을(지역)별 ----
	int totalVillages = 0;
	int totalVisited = 0;
	int totalNpcs = 0;
	int totalNpcsPresent = 0;
	int totalMobs = 0;
	int totalMobsPresent = 0;
	int totalMobsHostile = 0;
	int totalMobsChasing = 0;
	int totalKills = 0;

	json.BeginArray("villages");
	for (const StoryRegion& region : director.GetRegions())
	{
		std::map<std::string, VillageStatus>::const_iterator found = m_Villages.find(region.id);
		const VillageStatus* village = (found != m_Villages.end() && found->second.captured) ? &found->second : nullptr;
		int kills = (found != m_Villages.end()) ? found->second.kills : 0;
		bool live = (region.id == here);
		bool visited = (village != nullptr);
		bool unlocked = live || story.IsTrue("UNLOCK_" + region.id);

		// 크로노스의 배 속은 마을이 아니라 이야기 속 무대라(이정표로 갈 수 없음), 지금 그 안에 있거나 다녀온 기록이 있을 때만 보낸다.
		if (region.id == "LOC_KRONOS_BELLY" && !live && !visited)
		{
			continue;
		}
		++totalVillages;

		// 수부터 센다. 동료(피티아)는 플레이어를 따라 지금 지역에 있으므로 다른 지역의 기록에선 뺀다.
		int npcCount = 0;
		int npcPresent = 0;
		int mobCount = 0;
		int mobPresent = 0;
		int mobHostile = 0;
		int mobChasing = 0;
		std::map<std::string, int> mobsByKind;

		if (village != nullptr)
		{
			for (const NpcStatus& npc : village->npcs)
			{
				if (!live && npc.companion)
				{
					continue;
				}

				++npcCount;
				if (npc.present)
				{
					++npcPresent;
				}
			}

			for (const MobStatus& mob : village->mobs)
			{
				++mobCount;
				if (!mob.present)
				{
					continue;
				}

				++mobPresent;
				++mobsByKind[AnimalKindKey((AnimalKind)mob.kind)];
				if (mob.hostile)
				{
					++mobHostile;
				}
				if (live && mob.chasing)
				{
					++mobChasing;
				}
			}
		}

		// 마을 전체 상태: 쫓기는 중이면 위험, 적대 몹이 있으면 경계, 아니면 평온(안 가 본 곳은 미방문).
		const char* status = "calm";
		const char* statusLabel = "평온";
		if (!visited)
		{
			status = "unvisited";
			statusLabel = "미방문";
		}
		else if (mobChasing > 0)
		{
			status = "danger";
			statusLabel = "위험(추적 중인 몹)";
		}
		else if (mobHostile > 0)
		{
			status = "alert";
			statusLabel = "경계(적대 몹 있음)";
		}

		if (visited)
		{
			++totalVisited;
		}
		totalNpcs += npcCount;
		totalNpcsPresent += npcPresent;
		totalMobs += mobCount;
		totalMobsPresent += mobPresent;
		totalMobsHostile += mobHostile;
		totalMobsChasing += mobChasing;
		totalKills += kills;

		json.BeginObject(nullptr);
		json.String("id", region.id);
		json.String("name", region.name);
		json.String("shortName", director.GetRegionShortName(region.id));
		json.Bool("live", live);
		json.Bool("visited", visited);
		json.Bool("unlocked", unlocked);
		json.Bool("objectiveHere", !objectiveRegion.empty() && objectiveRegion == region.id);
		if (visited)
		{
			json.Number("capturedAt", village->capturedAt, 1);
			json.Number("age", m_Time - village->capturedAt, 1);
		}
		else
		{
			json.Null("capturedAt");
			json.Null("age");
		}
		json.String("status", status);
		json.String("statusLabel", statusLabel);
		json.Int("npcCount", npcCount);
		json.Int("npcPresent", npcPresent);
		json.Int("mobCount", mobCount);
		json.Int("mobPresent", mobPresent);
		json.Int("mobHostile", mobHostile);
		json.Int("mobChasing", mobChasing);
		json.BeginObject("mobsByKind");
		for (const std::pair<const std::string, int>& entry : mobsByKind)
		{
			json.Int(entry.first.c_str(), entry.second);
		}
		json.EndObject();
		json.Int("kills", kills);
		json.Int("herbs", visited ? village->herbs : 0);
		json.Int("storyItems", visited ? village->storyItems : 0);

		json.BeginArray("npcs");
		if (village != nullptr)
		{
			for (const NpcStatus& npc : village->npcs)
			{
				if (!live && npc.companion)
				{
					continue;
				}

				const char* state = "idle";
				const char* stateLabel = "제자리";
				if (!npc.present)
				{
					state = "hidden";
					stateLabel = "숨음(이야기 조건)";
				}
				else if (npc.companion)
				{
					state = "following";
					stateLabel = "동행 중";
				}
				else if (npc.faded)
				{
					state = npc.flaring ? "flaring" : "faded";
					stateLabel = npc.flaring ? "잠깐 선명함" : "흐려짐";
				}

				json.BeginObject(nullptr);
				json.String("id", npc.id);
				json.String("name", npc.name);
				json.String("tag", npc.tag);
				json.Number("x", npc.x);
				json.Number("y", npc.y);
				if (live)
				{
					json.Number("distance", npc.distance, 1);
				}
				else
				{
					json.Null("distance");
				}
				json.Bool("present", npc.present);
				json.Bool("companion", npc.companion);
				json.Bool("faded", npc.faded);
				json.Bool("flaring", npc.flaring);
				json.Bool("objective", npc.present && director.IsObjectiveTarget(npc.id, region.id));
				json.Bool("talkable", live && npc.present && director.CanTalkTo(npc.id));
				json.String("state", state);
				json.String("stateLabel", stateLabel);

				// 믿는 신과 플레이어의 수호신을 대하는 태도(수호신을 고르기 전이면 unknown), 지금 하는 일.
				Faith faith = (Faith)npc.faith;
				NpcActivity activity = (NpcActivity)npc.activity;
				json.String("faith", FaithInfo::Key(faith));
				json.String("faithName", FaithInfo::GodName(faith));
				if (playerFaith != Faith::None)
				{
					int relation = FaithInfo::Relation(faith, playerFaith);
					json.String("attitude", FaithInfo::RelationKey(relation));
					json.String("attitudeLabel", FaithInfo::RelationLabel(relation));
				}
				else
				{
					json.String("attitude", "unknown");
					json.String("attitudeLabel", "수호신을 고르기 전");
				}
				json.String("activity", live ? ActivityKey(activity) : "idle");
				json.String("activityLabel", live ? ActivityLabel(activity) : "기록");
				json.Bool("hotheaded", npc.hotheaded);
				json.Bool("social", npc.social);
				json.Bool("humbled", npc.humbled);
				json.EndObject();
			}
		}
		json.EndArray();

		json.BeginArray("mobs");
		if (village != nullptr)
		{
			for (const MobStatus& mob : village->mobs)
			{
				const char* state = "roaming";
				const char* stateLabel = "배회 중";
				if (!mob.present)
				{
					state = "hidden";
					stateLabel = "숨음(이야기 조건)";
				}
				else if (live && mob.stunned)
				{
					state = "stunned";
					stateLabel = "기절";
				}
				else if (live && mob.charmed)
				{
					state = "charmed";
					stateLabel = "홀림";
				}
				else if (live && mob.frightened)
				{
					state = "frightened";
					stateLabel = "겁에 질림";
				}
				else if (live && mob.chasing)
				{
					state = "chasing";
					stateLabel = "추적 중";
				}
				else if (live && mob.returning)
				{
					state = "returning";
					stateLabel = "돌아가는 중";
				}

				AnimalKind kind = (AnimalKind)mob.kind;

				json.BeginObject(nullptr);
				json.String("id", mob.id);
				json.String("kind", AnimalKindKey(kind));
				json.String("name", GetAnimalNames(kind).name);
				json.Int("hp", mob.hp);
				json.Int("maxHp", mob.maxHp);
				json.Number("x", mob.x);
				json.Number("y", mob.y);
				if (live)
				{
					json.Number("distance", mob.distance, 1);
				}
				else
				{
					json.Null("distance");
				}
				json.Bool("present", mob.present);
				json.Bool("hostile", mob.hostile);
				json.Bool("chasing", live && mob.chasing);
				json.Bool("returning", live && mob.returning);
				json.Bool("stunned", live && mob.stunned);
				json.Bool("charmed", live && mob.charmed);
				json.Bool("frightened", live && mob.frightened);
				json.Bool("objective", mob.present && director.IsObjectiveTarget(mob.id, region.id));
				json.String("state", state);
				json.String("stateLabel", stateLabel);
				json.EndObject();
			}
		}
		json.EndArray();

		json.EndObject();
	}
	json.EndArray();

	json.BeginObject("totals");
	json.Int("villages", totalVillages);
	json.Int("visited", totalVisited);
	json.Int("npcs", totalNpcs);
	json.Int("npcsPresent", totalNpcsPresent);
	json.Int("mobs", totalMobs);
	json.Int("mobsPresent", totalMobsPresent);
	json.Int("mobsHostile", totalMobsHostile);
	json.Int("mobsChasing", totalMobsChasing);
	json.Int("kills", totalKills);
	json.EndObject();

	// ---- 신앙과 신들의 관계(관계도) ----
	// 신도 수는 기록된 마을 전부의 NPC(다른 지역 기록의 동료는 지금 지역과 겹치므로 뺀다).
	int faithCounts[(int)Faith::Count] = {};
	for (const std::pair<const std::string, VillageStatus>& entry : m_Villages)
	{
		if (!entry.second.captured)
		{
			continue;
		}

		bool live = (entry.first == here);
		for (const NpcStatus& npc : entry.second.npcs)
		{
			if (!live && npc.companion)
			{
				continue;
			}
			if (npc.faith >= 0 && npc.faith < (int)Faith::Count)
			{
				++faithCounts[npc.faith];
			}
		}
	}

	json.BeginObject("faiths");
	json.String("playerFaith", FaithInfo::Key(playerFaith));
	json.BeginArray("list");
	for (int i = (int)Faith::Zeus; i < (int)Faith::Count; ++i)
	{
		Faith faith = (Faith)i;
		json.BeginObject(nullptr);
		json.String("key", FaithInfo::Key(faith));
		json.String("name", FaithInfo::GodName(faith));
		json.Bool("patron", PatronPowers::Find(FaithInfo::Key(faith)) != nullptr);
		json.Int("npcs", faithCounts[i]);
		json.EndObject();
	}
	json.EndArray();
	json.Int("noFaithNpcs", faithCounts[(int)Faith::None]);
	json.BeginArray("relations");
	for (int a = (int)Faith::Zeus; a < (int)Faith::Count; ++a)
	{
		for (int b = a + 1; b < (int)Faith::Count; ++b)
		{
			int relation = FaithInfo::Relation((Faith)a, (Faith)b);
			if (relation == 0)
			{
				continue;
			}

			json.BeginObject(nullptr);
			json.String("a", FaithInfo::Key((Faith)a));
			json.String("b", FaithInfo::Key((Faith)b));
			json.Int("value", relation);
			json.String("key", FaithInfo::RelationKey(relation));
			json.String("label", FaithInfo::RelationLabel(relation));
			json.EndObject();
		}
	}
	json.EndArray();
	json.EndObject();

	// ---- 이벤트(since보다 새 것만) ----
	json.BeginArray("events");
	for (const Event& event : m_Events)
	{
		if (event.seq <= sinceSeq)
		{
			continue;
		}

		json.BeginObject(nullptr);
		json.Int("seq", (long long)event.seq);
		json.Number("time", event.time, 1);
		json.String("type", event.type);
		json.String("map", event.map);
		json.String("text", event.text);
		json.EndObject();
	}
	json.EndArray();
	json.Int("lastSeq", (long long)(m_NextSeq - 1));

	json.EndObject();
	return json.Text();
}
