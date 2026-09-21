#include "stdafx.h"
#include "Hud.h"

#include "Renderer.h"
#include "Dependencies\freeglut.h"

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
	Mat4 uiProjection = Mat4::Ortho(0.f, 800.f, 0.f, 600.f, -1.f, 1.f);

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

// 화면 위치는 월드 좌표를 직접 NDC로 계산해서 glRasterPos에 넘긴다(레거시
// 모델뷰/프로젝션 행렬은 이 프로젝트 어디서도 건드리지 않아 항등행렬 상태이므로,
// NDC 좌표를 그대로 써도 정확히 맞는다).
void Hud::DrawNameTags(SceneGraph& scene, const Mat4& viewProjection)
{
	glUseProgram(0);
	glColor3f(1.f, 1.f, 1.f);

	scene.ForEach([&viewProjection](Actor& actor)
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

		float ndcX, ndcY;
		TransformToNDC(viewProjection, actor.GetWorldX(), actor.GetWorldY(), actor.GetWorldZ() + actor.GetSize() * 1.3f, ndcX, ndcY);

		if (ndcX < -1.1f || ndcX > 1.1f || ndcY < -1.1f || ndcY > 1.1f)
		{
			return; // 화면 밖이면 그리지 않음.
		}

		int textWidth = 0;
		for (const char* c = name; *c != '\0'; ++c)
		{
			textWidth += glutBitmapWidth(GLUT_BITMAP_HELVETICA_10, *c);
		}

		// 텍스트 폭의 절반만큼 왼쪽으로 밀어서 가운데 정렬한다
		// (픽셀→NDC 변환: 창 폭 800의 절반이 NDC 1.0에 대응).
		float centeredNdcX = ndcX - (float)textWidth / 800.f;

		glRasterPos2f(centeredNdcX, ndcY);
		for (const char* c = name; *c != '\0'; ++c)
		{
			glutBitmapCharacter(GLUT_BITMAP_HELVETICA_10, *c);
		}
	});
}
