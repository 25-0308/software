#include "stdafx.h"
#include "Hud.h"
#include "UiDraw.h"

#include <cmath>
#include <cstring>
#include <map>
#include <string>

#include "Renderer.h"
#include "TextRasterizer.h"

#include "Actor.h"
#include "CharacterActors.h"
#include "SceneGraph.h"

namespace
{
	// 아직 폰트/텍스트 렌더링 파이프라인이 없어서, 레벨 숫자는 계산기 표시창처럼
	// 사각형 세그먼트 조각으로 그린다(0~9, 7세그먼트 방식). 나머지는 사각형
	// 막대뿐이라 Renderer::DrawObject만으로 그릴 수 있다.

	// 자릿수 0~9의 on/off 세그먼트 비트마스크(bit0=위, bit1=오른쪽위, bit2=오른쪽아래,
	// bit3=아래, bit4=왼쪽아래, bit5=왼쪽위, bit6=가운데). 표준 7세그먼트 인코딩.
	const unsigned char kDigitSegments[10] = { 0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F };

	void DrawDigit(Renderer& renderer, int digit, float centerX, float centerY, float w, float h,
		float r, float g, float b, const Mat4& uiProjection)
	{
		if (digit < 0 || digit > 9)
		{
			return;
		}

		unsigned char segs = kDigitSegments[digit];
		float thickness = w * 0.24f;
		float halfW = w * 0.5f;
		float halfH = h * 0.5f;
		float armLength = halfH - thickness * 0.5f;

		struct Segment { unsigned char bit; float x, y, w, h; };
		Segment segments[7] =
		{
			{ 0x01, 0.f,               halfH - thickness * 0.5f, w - thickness, thickness },  // 위
			{ 0x40, 0.f,               0.f,                      w - thickness, thickness },  // 가운데
			{ 0x08, 0.f,              -halfH + thickness * 0.5f, w - thickness, thickness },  // 아래
			{ 0x20, -halfW + thickness * 0.5f,  halfH * 0.5f,    thickness,     armLength },  // 왼쪽위
			{ 0x02,  halfW - thickness * 0.5f,  halfH * 0.5f,    thickness,     armLength },  // 오른쪽위
			{ 0x10, -halfW + thickness * 0.5f, -halfH * 0.5f,    thickness,     armLength },  // 왼쪽아래
			{ 0x04,  halfW - thickness * 0.5f, -halfH * 0.5f,    thickness,     armLength },  // 오른쪽아래
		};

		for (const Segment& seg : segments)
		{
			if ((segs & seg.bit) == 0)
			{
				continue;
			}

			Mat4 model = Mat4::Translate(centerX + seg.x, centerY + seg.y, 0.f) * Mat4::Scale(seg.w, seg.h, 1.f);
			renderer.DrawObject(uiProjection * model, r, g, b, 1.f);
		}
	}

	// 음이 아닌 정수를 여러 자릿수 DrawDigit로 이어 그린다. left는 첫 자릿수의
	// 왼쪽 끝 x좌표.
	void DrawNumber(Renderer& renderer, int value, float left, float centerY, float digitW, float digitH, float gap,
		float r, float g, float b, const Mat4& uiProjection)
	{
		if (value < 0)
		{
			value = 0;
		}

		char digits[8];
		int digitCount = 0;
		int remaining = value;
		do
		{
			digits[digitCount++] = (char)(remaining % 10);
			remaining /= 10;
		}
		while (remaining > 0 && digitCount < 8);

		// digits[]엔 낮은 자리부터 들어있으므로 뒤에서부터(높은 자리부터) 그린다.
		float x = left + digitW * 0.5f;
		for (int i = digitCount - 1; i >= 0; --i)
		{
			DrawDigit(renderer, digits[i], x, centerY, digitW, digitH, r, g, b, uiProjection);
			x += digitW + gap;
		}
	}

	// 왼쪽 정렬로 fraction(0~1)만큼 채워지는 막대: 테두리+배경+채움 3겹.
	void DrawBar(Renderer& renderer, float left, float centerY, float width, float height, float fraction,
		float r, float g, float b, const Mat4& uiProjection)
	{
		if (fraction < 0.f) fraction = 0.f;
		if (fraction > 1.f) fraction = 1.f;

		Mat4 border = Mat4::Translate(left + width * 0.5f, centerY, 0.f) * Mat4::Scale(width + 4.f, height + 4.f, 1.f);
		renderer.DrawObject(uiProjection * border, 0.05f, 0.05f, 0.06f, 0.85f);

		Mat4 bg = Mat4::Translate(left + width * 0.5f, centerY, 0.f) * Mat4::Scale(width, height, 1.f);
		renderer.DrawObject(uiProjection * bg, 0.16f, 0.16f, 0.2f, 0.9f);

		if (fraction > 0.001f)
		{
			float fillWidth = width * fraction;
			Mat4 fill = Mat4::Translate(left + fillWidth * 0.5f, centerY, 0.f) * Mat4::Scale(fillWidth, height - 4.f, 1.f);
			renderer.DrawObject(uiProjection * fill, r, g, b, 1.f);
		}
	}
}

void Hud::DrawStatus(Renderer& renderer, const MeshHandle& circleMesh, const PlayerStats& stats, float actionReadiness)
{
	Mat4 uiProjection = UiDraw::ScreenProjection();

	const float kBadgeX = 40.f;
	const float kBadgeY = 560.f;
	const float kBarLeft = 74.f;
	const float kBarWidth = 190.f;

	// 레벨 배지: 어두운 원판 위에 숫자.
	Mat4 badge = Mat4::Translate(kBadgeX, kBadgeY, 0.f) * Mat4::Scale(52.f, 52.f, 1.f);
	renderer.DrawMesh(circleMesh, uiProjection * badge, 0.12f, 0.1f, 0.08f, 0.9f);

	if (stats.level < 10)
	{
		DrawDigit(renderer, stats.level, kBadgeX, kBadgeY, 20.f, 32.f, 0.95f, 0.85f, 0.35f, uiProjection);
	}
	else
	{
		DrawNumber(renderer, stats.level, kBadgeX - 18.f, kBadgeY, 16.f, 30.f, 3.f, 0.95f, 0.85f, 0.35f, uiProjection);
	}

	// 체력바(빨강): 위쪽. 경험치바(하늘색): 그 아래 조금 더 얇게.
	float hpFraction = (stats.maxHp > 0) ? (float)stats.hp / (float)stats.maxHp : 0.f;
	DrawBar(renderer, kBarLeft, kBadgeY + 10.f, kBarWidth, 20.f, hpFraction, 0.82f, 0.18f, 0.18f, uiProjection);

	float xpFraction = (stats.xpToNext > 0) ? (float)stats.xp / (float)stats.xpToNext : 0.f;
	DrawBar(renderer, kBarLeft, kBadgeY - 14.f, kBarWidth, 12.f, xpFraction, 0.35f, 0.68f, 0.95f, uiProjection);

	// 행동 쿨타임 막대: 경험치바 아래에 더 짧고 얇게. 쿨타임 중엔 주황색으로 차오르고, 다 차서
	// 바로 행동할 수 있으면 초록색이 된다. 눌러도 반응이 없을 때 "아직 충전 중"임을 알려준다.
	bool isReady = (actionReadiness >= 1.f);
	float readyR = isReady ? 0.5f : 0.95f;
	float readyG = isReady ? 0.9f : 0.7f;
	float readyB = isReady ? 0.5f : 0.25f;
	DrawBar(renderer, kBarLeft, kBadgeY - 32.f, kBarWidth * 0.5f, 8.f, actionReadiness, readyR, readyG, readyB, uiProjection);
}

namespace
{
	// 이름표 글자(UI 좌표 크기 — 실제로는 UiDraw::kPixelScale배로 그림). 이름은 몇십 가지뿐이라 한 번 만든 글자는
	// 끝날 때까지 들고 있다(Hud::Shutdown).
	const int kNameTagFontHeight = 12;
	const int kNameTagWrapWidth = 240;

	TextRasterizer* s_NameTagText = nullptr;
	std::map<std::string, TextTexture> s_NameTagTextures;

	// 액터의 영문 이름표(Actor::GetName — 모델 고르기·모니터가 그대로 씀)에 맞는 한글 이름. 짐승·괴물은 채팅창 로그와
	// 같은 이름(GetAnimalNames), 그 밖은 아래 표. 표에 없으면 영문 그대로.
	const char* KoreanNameTag(const Actor& actor)
	{
		if (actor.GetType() == ActorType::Animal)
		{
			return GetAnimalNames(static_cast<const AnimalActor&>(actor).GetKind()).name;
		}

		struct Entry
		{
			const char* english;
			const char* korean;
		};
		static const Entry kNames[] =
		{
			{ "Player", "플레이어" },
			{ "Pythia", "피티아" },
			{ "Old Woman", "떠돌이 노파" },
			{ "Eris", "에리스" },
			{ "Hermes", "헤르메스" },
			{ "Zeus", "제우스" },
			{ "Poseidon", "포세이돈" },
			{ "Hades", "하데스" },
			{ "Aphrodite", "아프로디테" },
			{ "Hera", "헤라" },
			{ "Athena", "아테나" },
			{ "Ares", "아레스" },
			{ "Dionysus", "디오니소스" },
			{ "Hephaestus", "헤파이스토스" },
			{ "Pan", "판" },
			{ "Apollo", "아폴론" },
			{ "Persephone", "페르세포네" },
			{ "Prometheus", "프로메테우스" },
			{ "Clotho", "클로토" },
			{ "Lachesis", "라케시스" },
			{ "Atropos", "아트로포스" },
			{ "Tantalus", "탄탈로스" },
			{ "Terminus", "경계석의 신" },
			{ "Pilgrim", "순례객" },
			{ "Merchant", "상인" },
			{ "Villager", "주민" },
			{ "Citizen", "시민" },
			{ "Theban", "테베 시민" },
			{ "Shepherd", "양치기" },
			{ "Priest", "사제" },
			{ "High Priest", "사제장" },
			{ "Philosopher", "철학자" },
			{ "Fisher", "어부" },
			{ "Castaway", "조난자" },
			{ "Soul", "영혼" },
			{ "Elder", "돌아온 노인" },
			{ "Undying", "돌아온 자" },
			{ "Guest", "가면 쓴 손님" },
			{ "Servant", "시종" },
			{ "Keeper", "기록 관리인" },
			{ "Survivor", "살아남은 양치기" },
		};

		const char* english = actor.GetName();
		for (const Entry& entry : kNames)
		{
			if (strcmp(entry.english, english) == 0)
			{
				return entry.korean;
			}
		}
		return english;
	}

	const TextTexture& NameTagTexture(const std::string& name)
	{
		std::map<std::string, TextTexture>::iterator found = s_NameTagTextures.find(name);
		if (found != s_NameTagTextures.end())
		{
			return found->second;
		}

		if (s_NameTagText == nullptr)
		{
			s_NameTagText = new TextRasterizer(kNameTagFontHeight, true);
		}
		TextTexture& texture = s_NameTagTextures[name];
		texture = s_NameTagText->Create(name, kNameTagWrapWidth);
		return texture;
	}

	// UI 좌표를 실제 화면 픽셀 경계에 맞춘다(글자가 픽셀 사이에 걸려 흐려지지 않게).
	float SnapToPixel(float ui)
	{
		return floorf(ui * UiDraw::kPixelScale + 0.5f) / UiDraw::kPixelScale;
	}
}

// 이름표는 채팅창과 같은 GDI 글자 텍스처(TextRasterizer)로 그려 한글이 나온다. 머리 위 자리는 월드 좌표를 NDC로
// 옮긴 뒤 UI 좌표(UiDraw — 후처리가 남겨 둔 출력 영역 전체)로 바꾼다.
void Hud::DrawNameTags(Renderer& renderer, SceneGraph& scene, const Mat4& viewProjection)
{
	Mat4 ui = UiDraw::ScreenProjection();
	const float kShadowOffset = 1.f / UiDraw::kPixelScale; // 실제 화면 1픽셀

	scene.ForEach([&renderer, &ui, &viewProjection, kShadowOffset](Actor& actor)
	{
		const char* name = actor.GetName();
		if (name == nullptr || !actor.IsVisible() || actor.IsPendingDestroy())
		{
			return;
		}

		ActorType type = actor.GetType();
		if (type != ActorType::Player && type != ActorType::NPC && type != ActorType::Animal)
		{
			return;
		}

		// 플레이어의 창끝(약 1.47*size)보다 조금 위에 띄운다.
		float ndcX, ndcY;
		TransformToNDC(viewProjection, actor.GetWorldX(), actor.GetWorldY(), actor.GetWorldZ() + actor.GetSize() * 1.55f, ndcX, ndcY);

		if (ndcX < -1.1f || ndcX > 1.1f || ndcY < -1.1f || ndcY > 1.1f)
		{
			return; // 화면 밖이면 그리지 않음.
		}

		const TextTexture& texture = NameTagTexture(KoreanNameTag(actor));
		if (texture.id == 0)
		{
			return;
		}

		// 가운데 정렬, 글자 아래쪽(여백 제외)이 머리 위 자리에 오게.
		float uiX = (ndcX + 1.f) * 0.5f * UiDraw::kScreenWidth;
		float uiY = (ndcY + 1.f) * 0.5f * UiDraw::kScreenHeight;
		float left = SnapToPixel(uiX - (float)texture.width * 0.5f);
		float top = SnapToPixel(uiY + (float)texture.height - 3.f);

		// 1픽셀 오른쪽 아래에 검은 그림자를 먼저 찍어서, 밝은 모래·돌바닥 위에서도 글자가 또렷하게 읽히게 한다.
		UiDraw::TextTopLeft(renderer, ui, texture, left + kShadowOffset, top - kShadowOffset, 0.f, 0.f, 0.f, 0.85f);
		UiDraw::TextTopLeft(renderer, ui, texture, left, top, 1.f, 1.f, 1.f, 1.f);
	});
}

void Hud::Shutdown()
{
	for (std::map<std::string, TextTexture>::value_type& entry : s_NameTagTextures)
	{
		TextRasterizer::Destroy(entry.second);
	}
	s_NameTagTextures.clear();

	delete s_NameTagText;
	s_NameTagText = nullptr;
}
