#include "stdafx.h"
#include "CharacterActors.h"

#include <algorithm>
#include <cmath>
#include <string>

#include "GameLog.h"
#include "PatronPowers.h"
#include "Renderer.h"
#include "SceneGraph.h"
#include "Shapes.h"
#include "TileMap.h"

namespace
{
	// 공격 모션 재생 시간(초).
	const float kAttackAnimDuration = 0.25f;

	// 괴물이 물 때 몸을 앞으로 내밀었다 돌아오는 시간(초).
	const float kLungeDuration = 0.3f;

	// 피격 플래시 지속 시간(초).
	const float kAnimalHitFlashDuration = 0.15f;
	const float kPlayerHitFlashDuration = 0.2f;

	const float kPlayerSpeed = 4.f;  // 초당 이동 거리(월드 단위)
	const float kWanderSpeed = 1.2f; // 짐승이 배회 목표점을 따라 걷는 속도(추적을 포기하고 돌아갈 때도)
	const float kMonsterHomeRadius = 2.f; // 포기하고 돌아가다가 anchor에서 이 안으로 들어오면 다시 경계 태세.

	// 동료(따라다니는 NPC).
	const float kFollowDistance = 1.7f;  // 이보다 가까우면 멈춰 선다(상호작용 사거리 1.4보다 조금 밖 — 늘 조준 링이 붙지 않게)
	const float kFollowSpeed = 5.6f;     // 플레이어(4.0, 헤르메스의 축복이면 5.2)보다 빨라야 뒤처지지 않는다
	const float kFollowTeleport = 7.f;   // 이보다 멀어지면(막혔거나 맵을 막 옮김) 곁으로 순간이동

	// 흐려진 신이 이름을 불린 뒤: 이 시간 동안 또렷하게 있다가, 다음 시간 동안 천천히 다시 흐려진다.
	const float kFlareHoldSeconds = 8.f;
	const float kFlareFadeSeconds = 6.f;

	// 마을 사람의 자율 행동.
	const float kStrollSpeed = 1.1f;        // 거니는 속도(플레이어 4.0보다 훨씬 느긋하게)
	const float kIdleMinSeconds = 2.f;      // 한자리에 서서 둘러보는 시간(이 범위에서 고름)
	const float kIdleMaxSeconds = 6.f;
	const float kSwingSeconds = 0.3f;       // 주먹을 휘두르는 모션 시간
	const float kBrawlSpeed = 2.8f;         // 시비를 건 마을 사람이 쫓아오는 속도(플레이어보다 느림)
	const float kBrawlStopDistance = 0.85f; // 이만큼 붙으면 멈춰 서서 주먹을 휘두른다
	const float kBrawlReach = 1.15f;        // 주먹이 닿는 거리
	const float kBrawlPunchCooldown = 1.4f;
	const int kBrawlPunchDamage = 2;
	const float kBrawlGiveUpDistance = 9.f; // 플레이어가 이보다 멀어지면 그만둔다

	// 스킬로 걸린 상태 표시(머리 위를 도는 작은 빛) 색.
	const Shapes::Rgb kStunMarkColor = { 1.5f, 1.3f, 0.5f };  // 기절: 노란 별빛
	const Shapes::Rgb kCharmMarkColor = { 1.6f, 0.6f, 1.0f }; // 홀림: 분홍빛
	const Shapes::Rgb kFearMarkColor = { 0.8f, 0.5f, 1.6f };  // 겁: 보랏빛

	// 화면 가로 방향(월드 (1,-1)/√2). 카메라를 향해 세운 도형을 좌우로 펼칠 때 쓴다.
	const float kScreenAxisX = 0.7071f;
	const float kScreenAxisY = -0.7071f;

	// 사람 파츠 색(옷 색과 무관하게 고정).
	const Shapes::Rgb kSkinColor = { 0.92f, 0.76f, 0.60f };
	const Shapes::Rgb kSandalColor = { 0.36f, 0.22f, 0.12f };
	const Shapes::Rgb kBeltColor = { 0.38f, 0.24f, 0.12f };
	const Shapes::Rgb kBronzeColor = { 0.86f, 0.60f, 0.26f };  // 투구·창끝
	const Shapes::Rgb kCrestColor = { 0.86f, 0.13f, 0.10f };   // 투구 깃털 장식
	const Shapes::Rgb kShaftColor = { 0.46f, 0.31f, 0.16f };   // 창 자루
	const Shapes::Rgb kStaffColor = { 0.40f, 0.26f, 0.13f };   // 지팡이
	const Shapes::Rgb kTridentColor = { 0.36f, 0.62f, 0.58f }; // 바닷빛 청동 삼지창
	const Shapes::Rgb kGoldColor = { 1.05f, 0.82f, 0.32f };    // 금(1을 살짝 넘어 블룸으로 은은히)
	const Shapes::Rgb kIronColor = { 0.26f, 0.25f, 0.25f };
	const Shapes::Rgb kFurLegColor = { 0.44f, 0.31f, 0.19f };  // 판의 염소 다리
	const Shapes::Rgb kHoofColor = { 0.12f, 0.1f, 0.08f };
	const Shapes::Rgb kHornColor = { 0.82f, 0.76f, 0.62f };
	const Shapes::Rgb kHatColor = { 0.58f, 0.45f, 0.27f };     // 헤르메스의 챙 넓은 모자
	const Shapes::Rgb kWingColor = { 0.96f, 0.96f, 0.98f };

	// 짐승 파츠 중 털 색과 무관한 것.
	const Shapes::Rgb kAntlerColor = { 0.86f, 0.78f, 0.62f };
	const Shapes::Rgb kDeerTailColor = { 0.95f, 0.92f, 0.86f };
	const Shapes::Rgb kEyeGlowColor = { 1.6f, 0.6f, 2.0f };   // 그림자 괴물의 보랏빛 눈(블룸으로 빛남)

	// 짐승·괴물 종류별 수치. AnimalKind 순서와 같아야 한다.
	struct AnimalSpec
	{
		const char* name;      // 머리 위 이름표(영문)
		BodyShape shape;
		float size;
		float r, g, b;
		int hp;
		bool aggressive;
		bool shadow;           // 그림자 괴물(보랏빛 기운과 빛나는 눈)
		float detectRadius;    // 이 안에 들어오면 추적을 시작한다
		float giveUpRadius;    // 플레이어가 이보다 멀어지면 추적을 포기한다(히스테리시스)
		float leashRadius;     // 자기 anchor에서 이보다 멀어지면 추적을 포기하고 돌아간다
		float chaseSpeed;      // 플레이어(4.0)보다 느려서 도망칠 여지가 있다
		float attackRadius;    // 이 안에 있어야 실제로 문다
		int damage;
		float attackCooldown;  // 공격 간 최소 간격(초)
		int xp;                // 쓰러뜨리면 주는 경험치
	};

	const AnimalSpec kAnimalSpecs[] =
	{
		{ "Deer", BodyShape::Deer, 0.9f, 0.52f, 0.36f, 0.20f, 20, false, false, 0.f, 0.f, 0.f, 0.f, 0.f, 0, 0.f, 25 },
		{ "Wolf", BodyShape::Wolf, 1.0f, 0.36f, 0.36f, 0.38f, 30, true, false, 4.5f, 7.f, 6.f, 2.3f, 1.1f, 6, 1.1f, 35 },
		{ "Shade", BodyShape::Wolf, 0.9f, 0.16f, 0.10f, 0.22f, 18, true, true, 7.f, 12.f, 14.f, 2.1f, 1.1f, 3, 1.3f, 20 },
		{ "Shadow Python", BodyShape::Serpent, 1.6f, 0.15f, 0.09f, 0.21f, 45, true, true, 9.f, 30.f, 40.f, 1.9f, 1.6f, 4, 1.4f, 60 },
		{ "Bronze Warrior", BodyShape::Human, 1.0f, 0.55f, 0.36f, 0.17f, 34, true, false, 6.f, 11.f, 10.f, 2.2f, 1.2f, 6, 1.3f, 40 },
		{ "Kronos", BodyShape::Human, 2.1f, 0.17f, 0.13f, 0.22f, 150, true, true, 12.f, 40.f, 40.f, 1.6f, 2.2f, 9, 1.7f, 150 },
		{ "White Cow", BodyShape::Cow, 1.05f, 0.95f, 0.94f, 0.9f, 50, false, false, 0.f, 0.f, 0.f, 0.f, 0.f, 0, 0.f, 0 },
	};

	const AnimalSpec& GetSpec(AnimalKind kind)
	{
		return kAnimalSpecs[(int)kind];
	}

	// 한 프레임 그리기에 필요한 캐릭터 상태(체형별 그리기 함수에 넘긴다).
	struct Pose
	{
		float x, y, z;
		float size;
		float time;
		float forwardX, forwardY; // 바라보는 방향(단위 벡터)
		float rightX, rightY;     // forward를 90도 돌린 옆 방향
		float breathe;            // 숨쉬기 상하 움직임
		float walkBob;            // 걷는 동안의 상하 들썩임
		float legSwing;           // 걷는 동안 다리가 앞뒤로 흔들리는 거리(걷지 않으면 0)
		float attack;             // 공격 모션 0~1~0(sin 곡선으로 뻗었다 돌아옴, 공격 중이 아니면 0)
		float flash;              // 피격 플래시 세기 0~1
		FlashKind flashKind;
		float alpha;              // 전체 불투명도
		bool glowEyes;            // 빛나는 눈(그림자 괴물)
	};

	// 피격 플래시를 파츠 색에 입힌다. 모든 파츠에 같이 입혀서 캐릭터 전체가 번쩍이게 한다.
	Shapes::Rgb Flashed(const Pose& pose, const Shapes::Rgb& color)
	{
		if (pose.flash <= 0.f)
		{
			return color;
		}

		Shapes::Rgb result = color;
		if (pose.flashKind == FlashKind::White)
		{
			result.r += (1.f - result.r) * pose.flash;
			result.g += (1.f - result.g) * pose.flash;
			result.b += (1.f - result.b) * pose.flash;
		}
		else
		{
			result.r += (1.f - result.r) * pose.flash;
			result.g -= result.g * pose.flash * 0.7f;
			result.b -= result.b * pose.flash * 0.7f;
		}
		return result;
	}

	// 캐릭터 중심에서 앞으로 forward, 옆으로 side만큼 옮긴 월드 좌표.
	void Offset(const Pose& pose, float forward, float side, float& outX, float& outY)
	{
		outX = pose.x + pose.forwardX * forward + pose.rightX * side;
		outY = pose.y + pose.forwardY * forward + pose.rightY * side;
	}

	// 한 가지 색의 상자 파츠(플래시·불투명도 적용, 위 1.0 / +y면 0.82 / +x면 0.62 명암).
	void DrawPart(const RenderContext& ctx, const Pose& pose, float x, float y, float baseZ,
		float width, float depth, float height, const Shapes::Rgb& color)
	{
		Shapes::DrawSolidBox(ctx, x, y, baseZ, width, depth, height, Flashed(pose, color), pose.alpha);
	}

	void DrawCylinderPart(const RenderContext& ctx, const Pose& pose, float x, float y, float baseZ, float radius, float height,
		const Shapes::Rgb& side, const Shapes::Rgb& top)
	{
		Shapes::DrawCylinder(ctx, x, y, baseZ, radius, height, Flashed(pose, side), Flashed(pose, top), pose.alpha);
	}

	void DrawEllipsePart(const RenderContext& ctx, const Pose& pose, float x, float y, float centerZ, float width, float height,
		const Shapes::Rgb& color)
	{
		Shapes::DrawUprightEllipse(ctx, x, y, centerZ, width, height, Flashed(pose, color), pose.alpha);
	}

	// 바라보는 방향이 카메라 쪽인지(앞으로 한 걸음 가면 카메라에 가까워지는지 — 클립 z가 작아지는지).
	bool IsFacingCamera(const RenderContext& ctx, const Pose& pose)
	{
		const float* m = ctx.viewProjection.m;
		return m[2] * pose.forwardX + m[6] * pose.forwardY < 0.f;
	}

	// 상자 파츠 목록을 모아 두었다가 카메라에서 먼 것부터 그린다(네 발 짐승처럼 바라보는 방향에 따라
	// 파츠의 앞뒤가 바뀌어서 순서를 고정할 수 없을 때). 먼 정도는 SceneGraph의 정렬과 같은 기준(클립 z).
	class PartList
	{
	public:
		void Add(float x, float y, float baseZ, float width, float depth, float height, const Shapes::Rgb& color)
		{
			if (m_Count >= kMaxParts)
			{
				return;
			}

			BoxPart& part = m_Parts[m_Count++];
			part.x = x;
			part.y = y;
			part.baseZ = baseZ;
			part.width = width;
			part.depth = depth;
			part.height = height;
			part.color = color;
			part.distance = 0.f;
		}

		void DrawSorted(const RenderContext& ctx, const Pose& pose)
		{
			const float* m = ctx.viewProjection.m;
			for (int i = 0; i < m_Count; ++i)
			{
				BoxPart& part = m_Parts[i];
				float centerZ = part.baseZ + part.height * 0.5f;
				part.distance = m[2] * part.x + m[6] * part.y + m[10] * centerZ + m[14];
			}

			std::stable_sort(m_Parts, m_Parts + m_Count, [](const BoxPart& lhs, const BoxPart& rhs)
			{
				return lhs.distance > rhs.distance;
			});

			for (int i = 0; i < m_Count; ++i)
			{
				const BoxPart& part = m_Parts[i];
				DrawPart(ctx, pose, part.x, part.y, part.baseZ, part.width, part.depth, part.height, part.color);
			}
		}

	private:
		struct BoxPart
		{
			float x, y, baseZ;
			float width, depth, height;
			Shapes::Rgb color;
			float distance; // 카메라에서 먼 정도(클수록 먼저 그림)
		};

		static const int kMaxParts = 16;
		BoxPart m_Parts[kMaxParts];
		int m_Count = 0;
	};

	// 사람: 다리 → 팔(+손에 든 물건) → 몸통 → 머리 순. 이 렌더러는 실제 깊이버퍼가 없어서(그리기
	// 순서로 깊이를 흉내) 겹친 픽셀은 나중에 그린 것이 보이므로, 팔다리를 먼저 깔고 몸통·머리를 맨
	// 나중에 그려서 겹치는 자리에서는 항상 몸통·머리가 우선적으로 보이게(팔다리가 가려지게) 한다.
	void DrawHuman(const RenderContext& ctx, const Pose& pose, const Shapes::Rgb& clothes, const HumanLook& look)
	{
		float s = pose.size;
		float z = pose.z;
		float lift = pose.breathe + pose.walkBob; // 몸통·머리가 같이 들썩이는 양
		bool facesCamera = IsFacingCamera(ctx, pose);

		// 살 색: 보통은 살색, 청동 전사는 몸 전체가 청동.
		const Shapes::Rgb& skin = look.bronzeBody ? kBronzeColor : kSkinColor;

		Shapes::Rgb hair = { look.hairR, look.hairG, look.hairB };
		Shapes::Rgb hood = { look.hoodR, look.hoodG, look.hoodB };
		Shapes::Rgb beard = { look.beardR, look.beardG, look.beardB };
		Shapes::Rgb wreath = { look.wreathR, look.wreathG, look.wreathB };

		// 등 뒤로 늘어진 긴 머리(또는 두건 자락): 얼굴이 카메라를 향하면 몸·머리 뒤에 가려져 얼굴 양옆으로만
		// 보이도록 팔다리와 함께 먼저 그리고, 등지면 뒤통수와 등을 덮도록 맨 나중에 그린다.
		bool hasBackHair = look.longHair || look.hood;
		Shapes::Rgb backHairColor = look.hood ? hood : hair;
		float backHairX, backHairY;
		Offset(pose, -s * 0.12f, 0.f, backHairX, backHairY);

		if (hasBackHair && facesCamera)
		{
			DrawPart(ctx, pose, backHairX, backHairY, z + s * 0.5f + lift, s * 0.46f, s * 0.46f, s * 0.55f, backHairColor);
		}

		// 다리 2개: 옆으로 벌리고, 걷는 동안 앞뒤로 서로 반대로 흔들림. 사람은 맨다리+샌들, 판은 털 난 염소 다리+발굽.
		for (int side = -1; side <= 1; side += 2)
		{
			float legX, legY;
			Offset(pose, -pose.legSwing * (float)side, s * 0.15f * (float)side, legX, legY);

			if (look.goatLegs)
			{
				DrawPart(ctx, pose, legX, legY, z, s * 0.2f, s * 0.2f, s * 0.34f, kFurLegColor);
				DrawPart(ctx, pose, legX, legY, z, s * 0.17f, s * 0.21f, s * 0.06f, kHoofColor);
			}
			else
			{
				DrawPart(ctx, pose, legX, legY, z, s * 0.16f, s * 0.16f, s * 0.3f, skin);
				DrawPart(ctx, pose, legX, legY, z, s * 0.19f, s * 0.19f, s * 0.05f, kSandalColor);
			}
		}

		// 팔 2개: 걷는 동안은 다리와 반대 위상으로 흔들리고, 공격 중엔 오른팔이 앞으로 크게 뻗는다.
		float armSwing = -pose.legSwing;
		float reach = pose.attack * s * 0.6f;
		float armBaseZ = z + s * 0.4f;

		float leftArmX, leftArmY, rightArmX, rightArmY;
		Offset(pose, armSwing, -s * 0.42f, leftArmX, leftArmY);
		Offset(pose, -armSwing + reach, s * 0.42f, rightArmX, rightArmY);
		DrawPart(ctx, pose, leftArmX, leftArmY, armBaseZ, s * 0.14f, s * 0.14f, s * 0.3f, skin);
		DrawPart(ctx, pose, rightArmX, rightArmY, armBaseZ, s * 0.14f, s * 0.14f, s * 0.3f, skin);

		// 오른손에 든 물건: 팔과 같이 움직여서, 공격하면 창이 앞으로 찔러 나간다.
		if (look.spear)
		{
			DrawPart(ctx, pose, rightArmX, rightArmY, z + s * 0.08f, s * 0.05f, s * 0.05f, s * 1.25f, kShaftColor);
			DrawPart(ctx, pose, rightArmX, rightArmY, z + s * 1.33f, s * 0.09f, s * 0.09f, s * 0.14f, kBronzeColor);
		}
		if (look.staff)
		{
			DrawPart(ctx, pose, rightArmX, rightArmY, z, s * 0.06f, s * 0.06f, s * 1.02f, kStaffColor);
			DrawPart(ctx, pose, rightArmX, rightArmY, z + s * 1.02f, s * 0.11f, s * 0.11f, s * 0.08f, Shapes::Shade(kStaffColor, 1.25f));
		}
		if (look.trident)
		{
			// 자루 → 가로대 → 가운데 날 → 양쪽 날(옆 방향으로 벌림).
			DrawPart(ctx, pose, rightArmX, rightArmY, z + s * 0.05f, s * 0.055f, s * 0.055f, s * 1.3f, kTridentColor);
			DrawPart(ctx, pose, rightArmX, rightArmY, z + s * 1.3f, s * 0.24f, s * 0.24f, s * 0.04f, kTridentColor);
			DrawPart(ctx, pose, rightArmX, rightArmY, z + s * 1.34f, s * 0.05f, s * 0.05f, s * 0.24f, Shapes::Shade(kTridentColor, 1.2f));

			for (int side = -1; side <= 1; side += 2)
			{
				float prongX = rightArmX + pose.rightX * s * 0.1f * (float)side;
				float prongY = rightArmY + pose.rightY * s * 0.1f * (float)side;
				DrawPart(ctx, pose, prongX, prongY, z + s * 1.34f, s * 0.04f, s * 0.04f, s * 0.17f, Shapes::Shade(kTridentColor, 1.1f));
			}
		}
		if (look.caduceus)
		{
			// 전령의 지팡이: 가는 자루 + 금빛 머리와 작은 날개.
			DrawPart(ctx, pose, rightArmX, rightArmY, z + s * 0.1f, s * 0.045f, s * 0.045f, s * 1.0f, Shapes::Shade(kGoldColor, 0.7f));
			DrawEllipsePart(ctx, pose, rightArmX, rightArmY, z + s * 1.14f, s * 0.12f, s * 0.12f, kGoldColor);
			DrawEllipsePart(ctx, pose, rightArmX + kScreenAxisX * s * 0.1f, rightArmY + kScreenAxisY * s * 0.1f, z + s * 1.1f,
				s * 0.14f, s * 0.06f, kWingColor);
			DrawEllipsePart(ctx, pose, rightArmX - kScreenAxisX * s * 0.1f, rightArmY - kScreenAxisY * s * 0.1f, z + s * 1.1f,
				s * 0.14f, s * 0.06f, kWingColor);
		}
		if (look.hammer)
		{
			// 손에 늘어뜨려 든 망치: 손에서 아래로 내려온 자루 끝에 쇠 머리.
			DrawPart(ctx, pose, rightArmX, rightArmY, z + s * 0.2f, s * 0.05f, s * 0.05f, s * 0.45f, kShaftColor);
			DrawPart(ctx, pose, rightArmX, rightArmY, z + s * 0.12f, s * 0.22f, s * 0.12f, s * 0.12f, kIronColor);
		}

		// 왼손에 든 물건: 금빛 사과(노파), 금 술잔(디오니소스).
		if (look.apple)
		{
			DrawEllipsePart(ctx, pose, leftArmX, leftArmY, armBaseZ + s * 0.02f, s * 0.13f, s * 0.12f, Shapes::Rgb{ 1.3f, 1.02f, 0.3f });
			DrawPart(ctx, pose, leftArmX, leftArmY, armBaseZ + s * 0.07f, s * 0.015f, s * 0.015f, s * 0.04f, kShaftColor);
		}
		if (look.wineCup)
		{
			DrawCylinderPart(ctx, pose, leftArmX, leftArmY, armBaseZ, s * 0.06f, s * 0.08f, kGoldColor, Shapes::Rgb{ 0.42f, 0.08f, 0.2f });
		}

		// 몸통(옷): 긴 옷이면 발목까지 내려와 다리를 덮는다(샌들만 살짝 보임). 그 위에 허리띠.
		float bodyTop = z + s * 0.85f + lift;
		float bodyBottom = look.robe ? z + s * 0.03f : z + s * 0.15f + lift;
		DrawPart(ctx, pose, pose.x, pose.y, bodyBottom, s * 0.55f, s * 0.35f, bodyTop - bodyBottom, clothes);
		DrawPart(ctx, pose, pose.x, pose.y, z + s * 0.44f + lift, s * 0.57f, s * 0.37f, s * 0.06f, kBeltColor);

		// 머리: 낮은 원기둥(정수리가 둥글게 보임) + 윗부분을 조금 더 넓게 덮는 머리카락/투구/두건.
		// 수염은 얼굴 쪽에 붙어서, 얼굴이 카메라를 등지면 머리 뒤에 숨도록 머리보다 먼저, 카메라를 향하면
		// 머리 앞에 보이도록 머리 다음에 그린다.
		float headBase = z + s * 0.825f + lift;
		float beardX, beardY;
		Offset(pose, s * 0.13f, 0.f, beardX, beardY);

		if (look.beard && !facesCamera)
		{
			DrawPart(ctx, pose, beardX, beardY, headBase - s * 0.07f, s * 0.2f, s * 0.2f, s * 0.16f, beard);
		}

		DrawCylinderPart(ctx, pose, pose.x, pose.y, headBase, s * 0.2f, s * 0.25f, skin, Shapes::Shade(skin, 1.1f));

		if (look.hood)
		{
			DrawCylinderPart(ctx, pose, pose.x, pose.y, headBase + s * 0.1f, s * 0.235f, s * 0.19f, hood, Shapes::Shade(hood, 1.12f));
		}
		else
		{
			Shapes::Rgb cap = look.helmet ? kBronzeColor : hair;
			DrawCylinderPart(ctx, pose, pose.x, pose.y, headBase + s * 0.13f, s * 0.215f, s * 0.13f, cap, Shapes::Shade(cap, 1.15f));
		}

		if (look.helmet)
		{
			// 투구 꼭대기의 붉은 깃털 장식. 상자는 회전하지 않으므로 바라보는 방향에 가까운 축으로 길게 놓는다.
			bool alongX = fabsf(pose.forwardX) > fabsf(pose.forwardY);
			float crestWidth = alongX ? s * 0.26f : s * 0.07f;
			float crestDepth = alongX ? s * 0.07f : s * 0.26f;
			DrawPart(ctx, pose, pose.x, pose.y, headBase + s * 0.26f, crestWidth, crestDepth, s * 0.12f, kCrestColor);
		}
		else if (look.wingedHat)
		{
			// 챙 넓은 모자 → 모자 몸통 → 양옆의 작은 흰 날개.
			DrawCylinderPart(ctx, pose, pose.x, pose.y, headBase + s * 0.22f, s * 0.33f, s * 0.03f, kHatColor, Shapes::Shade(kHatColor, 1.1f));
			DrawCylinderPart(ctx, pose, pose.x, pose.y, headBase + s * 0.25f, s * 0.19f, s * 0.09f, kHatColor, Shapes::Shade(kHatColor, 1.15f));
			DrawEllipsePart(ctx, pose, pose.x + kScreenAxisX * s * 0.27f, pose.y + kScreenAxisY * s * 0.27f, headBase + s * 0.32f,
				s * 0.16f, s * 0.09f, kWingColor);
			DrawEllipsePart(ctx, pose, pose.x - kScreenAxisX * s * 0.27f, pose.y - kScreenAxisY * s * 0.27f, headBase + s * 0.32f,
				s * 0.16f, s * 0.09f, kWingColor);
		}
		else if (look.wreath && !look.hood)
		{
			// 머리에 두른 관(금관·월계관·담쟁이관).
			DrawCylinderPart(ctx, pose, pose.x, pose.y, headBase + s * 0.17f, s * 0.228f, s * 0.045f, wreath, Shapes::Shade(wreath, 1.15f));
		}

		if (look.horns)
		{
			for (int side = -1; side <= 1; side += 2)
			{
				float hornX, hornY;
				Offset(pose, -s * 0.02f, s * 0.12f * (float)side, hornX, hornY);
				DrawPart(ctx, pose, hornX, hornY, headBase + s * 0.24f, s * 0.06f, s * 0.06f, s * 0.13f, kHornColor);
			}
		}

		if (look.beard && facesCamera)
		{
			DrawPart(ctx, pose, beardX, beardY, headBase - s * 0.07f, s * 0.2f, s * 0.2f, s * 0.16f, beard);
		}

		if (hasBackHair && !facesCamera)
		{
			DrawPart(ctx, pose, backHairX, backHairY, z + s * 0.5f + lift, s * 0.46f, s * 0.46f, s * 0.55f, backHairColor);
		}

		// 빛나는 눈(크로노스의 잔향 같은 그림자 괴물): 얼굴이 카메라를 향할 때만 보인다.
		if (pose.glowEyes && facesCamera)
		{
			float eyeX, eyeY;
			Offset(pose, s * 0.2f, 0.f, eyeX, eyeY);
			for (int side = -1; side <= 1; side += 2)
			{
				Shapes::DrawUprightEllipse(ctx, eyeX + kScreenAxisX * s * 0.07f * (float)side, eyeY + kScreenAxisY * s * 0.07f * (float)side,
					headBase + s * 0.13f, s * 0.06f, s * 0.04f, kEyeGlowColor, pose.alpha);
			}
		}
	}

	// 네 발 짐승(사슴/늑대/소). 다리 4개를 먼저 깔고(먼 다리부터), 몸통·목·머리·귀·꼬리·뿔은 파츠마다 깊이를
	// 재서 먼 것부터 그린다. 상자는 회전하지 않으므로 몸통을 앞(가슴)·뒤(엉덩이) 두 덩어리로 바라보는
	// 방향을 따라 나란히 놓아서, 어느 방향을 봐도 몸이 그 방향으로 길쭉해 보이게 한다.
	void DrawQuadruped(const RenderContext& ctx, const Pose& pose, const Shapes::Rgb& fur, BodyShape shape)
	{
		float s = pose.size;
		float z = pose.z;
		bool isWolf = (shape == BodyShape::Wolf);
		bool isCow = (shape == BodyShape::Cow);

		// 체형: 늑대는 낮고 다리가 굵으며, 사슴은 다리가 길고 가늘며 목이 솟아 있고, 소는 몸통이 넓고 묵직하다.
		float legHeight = isWolf ? s * 0.3f : (isCow ? s * 0.32f : s * 0.4f);
		float legWidth = isWolf ? s * 0.09f : (isCow ? s * 0.11f : s * 0.07f);
		float halfSpan = s * 0.24f; // 앞다리~뒷다리 거리의 절반
		float legSide = s * 0.1f;
		float bodyBase = z + legHeight + pose.walkBob + pose.breathe;
		float bodyHeight = isCow ? s * 0.32f : s * 0.26f;
		float bodyWidth = isCow ? s * 0.36f : s * 0.3f;
		float lunge = pose.attack * s * 0.18f; // 물 때 몸을 앞으로 내미는 거리

		Shapes::Rgb legColor = Shapes::Shade(fur, 0.72f);
		Shapes::Rgb headColor = Shapes::Shade(fur, 1.1f);
		Shapes::Rgb darkColor = Shapes::Shade(fur, 0.7f);

		// 1) 다리 4개: 대각선 쌍(앞왼+뒤오 / 앞오+뒤왼)이 같이 움직이는 걸음.
		const float legForward[4] = { halfSpan, halfSpan, -halfSpan, -halfSpan };
		const float legSideSign[4] = { -1.f, 1.f, -1.f, 1.f };
		const float legPhase[4] = { 1.f, -1.f, -1.f, 1.f };

		PartList legs;
		for (int i = 0; i < 4; ++i)
		{
			float legX, legY;
			Offset(pose, legForward[i] + pose.legSwing * legPhase[i], legSide * legSideSign[i], legX, legY);
			legs.Add(legX, legY, z, legWidth, legWidth, legHeight + s * 0.03f, legColor);
		}
		legs.DrawSorted(ctx, pose);

		// 2) 몸통 두 덩어리 + 체형별 머리·장식.
		PartList body;
		float px, py;

		Offset(pose, s * 0.13f + lunge * 0.5f, 0.f, px, py);
		body.Add(px, py, bodyBase, bodyWidth, bodyWidth, bodyHeight, fur);
		Offset(pose, -s * 0.13f, 0.f, px, py);
		body.Add(px, py, bodyBase, bodyWidth, bodyWidth, bodyHeight * 0.94f, fur);

		float headBase = isWolf ? bodyBase + s * 0.1f : (isCow ? bodyBase + s * 0.12f : bodyBase + s * 0.32f);

		if (isCow)
		{
			// 몸통 앞쪽에 낮게 내민 넓은 머리 + 연분홍 주둥이 + 짧게 휜 뿔 두 개 + 옆으로 난 귀 + 끝에 털 뭉치가 달린 가는 꼬리.
			Offset(pose, s * 0.38f, 0.f, px, py);
			body.Add(px, py, headBase, s * 0.22f, s * 0.2f, s * 0.2f, headColor);
			Offset(pose, s * 0.5f, 0.f, px, py);
			body.Add(px, py, headBase + s * 0.01f, s * 0.13f, s * 0.13f, s * 0.1f, Shapes::Rgb{ 0.92f, 0.72f, 0.7f });

			for (int side = -1; side <= 1; side += 2)
			{
				Offset(pose, s * 0.36f, s * 0.1f * (float)side, px, py);
				body.Add(px, py, headBase + s * 0.18f, s * 0.05f, s * 0.05f, s * 0.08f, kHornColor);
				Offset(pose, s * 0.33f, s * 0.15f * (float)side, px, py);
				body.Add(px, py, headBase + s * 0.12f, s * 0.07f, s * 0.07f, s * 0.04f, darkColor);
			}

			Offset(pose, -s * 0.33f, 0.f, px, py);
			body.Add(px, py, bodyBase - s * 0.06f, s * 0.04f, s * 0.04f, s * 0.26f, fur);
			body.Add(px, py, bodyBase - s * 0.1f, s * 0.07f, s * 0.07f, s * 0.06f, darkColor);
		}
		else if (isWolf)
		{
			// 몸통 앞쪽 위의 머리 + 앞으로 튀어나온 주둥이 + 뾰족한 귀 두 개 + 뒤로 치켜든 꼬리.
			Offset(pose, s * 0.36f + lunge, 0.f, px, py);
			body.Add(px, py, headBase, s * 0.22f, s * 0.22f, s * 0.2f, headColor);
			Offset(pose, s * 0.5f + lunge, 0.f, px, py);
			body.Add(px, py, headBase + s * 0.02f, s * 0.12f, s * 0.12f, s * 0.09f, darkColor);

			for (int side = -1; side <= 1; side += 2)
			{
				Offset(pose, s * 0.32f + lunge, s * 0.07f * (float)side, px, py);
				body.Add(px, py, headBase + s * 0.2f, s * 0.06f, s * 0.06f, s * 0.1f, darkColor);
			}

			Offset(pose, -s * 0.38f, 0.f, px, py);
			body.Add(px, py, bodyBase + s * 0.1f, s * 0.1f, s * 0.1f, s * 0.14f, darkColor);
		}
		else
		{
			// 앞으로 솟은 목 + 머리 + 주둥이 + 가지 친 뿔 두 개 + 흰 꼬리.
			Offset(pose, s * 0.25f, 0.f, px, py);
			body.Add(px, py, bodyBase + s * 0.14f, s * 0.12f, s * 0.12f, s * 0.24f, fur);

			Offset(pose, s * 0.33f, 0.f, px, py);
			body.Add(px, py, headBase, s * 0.17f, s * 0.17f, s * 0.15f, headColor);
			Offset(pose, s * 0.44f, 0.f, px, py);
			body.Add(px, py, headBase + s * 0.01f, s * 0.09f, s * 0.09f, s * 0.08f, darkColor);

			for (int side = -1; side <= 1; side += 2)
			{
				// 곧게 선 뿔 줄기 + 바깥으로 한 번 갈라진 가지.
				Offset(pose, s * 0.31f, s * 0.06f * (float)side, px, py);
				body.Add(px, py, headBase + s * 0.14f, s * 0.035f, s * 0.035f, s * 0.24f, kAntlerColor);
				Offset(pose, s * 0.31f, s * 0.12f * (float)side, px, py);
				body.Add(px, py, headBase + s * 0.26f, s * 0.035f, s * 0.035f, s * 0.1f, kAntlerColor);
			}

			Offset(pose, -s * 0.3f, 0.f, px, py);
			body.Add(px, py, bodyBase + s * 0.14f, s * 0.08f, s * 0.08f, s * 0.09f, kDeerTailColor);
		}

		body.DrawSorted(ctx, pose);

		// 그림자 괴물의 빛나는 눈: 얼굴이 카메라를 향할 때만 보인다.
		if (pose.glowEyes && isWolf && IsFacingCamera(ctx, pose))
		{
			float eyeX, eyeY;
			Offset(pose, s * 0.47f + lunge, 0.f, eyeX, eyeY);
			for (int side = -1; side <= 1; side += 2)
			{
				Shapes::DrawUprightEllipse(ctx, eyeX + kScreenAxisX * s * 0.055f * (float)side, eyeY + kScreenAxisY * s * 0.055f * (float)side,
					headBase + s * 0.14f, s * 0.05f, s * 0.035f, kEyeGlowColor, pose.alpha);
			}
		}
	}

	// 뱀(그림자 퓌톤): 치켜든 머리 뒤로 둥근 마디들이 바라보는 반대쪽으로 이어지며 좌우로 물결친다.
	// 마디는 카메라를 향해 세운 타원이고, 서로 겹치므로 마디마다 깊이를 재서 먼 것부터 그린다.
	void DrawSerpent(const RenderContext& ctx, const Pose& pose, const Shapes::Rgb& skin)
	{
		struct Segment
		{
			float x, y, z;
			float width, height;
			Shapes::Rgb color;
			float distance;
		};

		const int kBodySegments = 9;
		Segment segments[kBodySegments + 2];
		int count = 0;

		float s = pose.size;
		float lunge = pose.attack * s * 0.35f;

		for (int i = 0; i < kBodySegments; ++i)
		{
			float t = (float)i / (float)(kBodySegments - 1); // 0 = 목 쪽, 1 = 꼬리 끝
			float back = s * 0.12f + (float)i * s * 0.21f;
			float wave = sinf(pose.time * 5.f - (float)i * 0.8f) * s * 0.16f * (0.3f + t);
			float thickness = s * (0.4f - 0.28f * t);

			Segment& segment = segments[count++];
			Offset(pose, -back, wave, segment.x, segment.y);
			segment.z = pose.z + thickness * 0.5f + pose.breathe;
			segment.width = thickness * 1.15f;
			segment.height = thickness;
			segment.color = Shapes::Shade(skin, (i % 2 == 0) ? 1.f : 0.85f);
			segment.distance = 0.f;
		}

		// 치켜든 목 + 머리(물 때 앞으로 내민다).
		Segment& neck = segments[count++];
		Offset(pose, s * 0.12f + lunge * 0.5f, 0.f, neck.x, neck.y);
		neck.z = pose.z + s * 0.32f;
		neck.width = s * 0.32f;
		neck.height = s * 0.42f;
		neck.color = skin;
		neck.distance = 0.f;

		Segment& head = segments[count++];
		Offset(pose, s * 0.3f + lunge, 0.f, head.x, head.y);
		head.z = pose.z + s * 0.55f;
		head.width = s * 0.46f;
		head.height = s * 0.34f;
		head.color = Shapes::Shade(skin, 1.25f);
		head.distance = 0.f;

		const float* m = ctx.viewProjection.m;
		for (int i = 0; i < count; ++i)
		{
			segments[i].distance = m[2] * segments[i].x + m[6] * segments[i].y + m[10] * segments[i].z + m[14];
		}

		std::stable_sort(segments, segments + count, [](const Segment& lhs, const Segment& rhs)
		{
			return lhs.distance > rhs.distance;
		});

		for (int i = 0; i < count; ++i)
		{
			const Segment& segment = segments[i];
			DrawEllipsePart(ctx, pose, segment.x, segment.y, segment.z, segment.width, segment.height, segment.color);
		}

		// 빛나는 눈: 얼굴이 카메라를 향할 때만 보인다.
		if (pose.glowEyes && IsFacingCamera(ctx, pose))
		{
			float eyeX, eyeY;
			Offset(pose, s * 0.36f + lunge, 0.f, eyeX, eyeY);
			for (int side = -1; side <= 1; side += 2)
			{
				Shapes::DrawUprightEllipse(ctx, eyeX + kScreenAxisX * s * 0.09f * (float)side, eyeY + kScreenAxisY * s * 0.09f * (float)side,
					pose.z + s * 0.6f, s * 0.07f, s * 0.05f, kEyeGlowColor, pose.alpha);
			}
		}
	}
}

// ----------------------------------------------------------- CharacterActor

CharacterActor::CharacterActor(ActorType type, BodyShape shape)
	: Actor(type)
	, m_Shape(shape)
{
}

void CharacterActor::FitBoundsToSize()
{
	float size = GetSize();

	if (m_Shape == BodyShape::Serpent)
	{
		// 뱀은 머리에서 꼬리 끝까지 약 2*size라, 바라보는 방향이 어디든 감싸도록 넓게 잡는다.
		SetBoundingSphere(size * 2.3f, size * 0.3f);
		return;
	}

	// 가장 높은 것은 창끝·삼지창(약 1.6*size), 가장 멀리 뻗는 것은 공격 때 팔·창(옆으로 약 1.0*size)과
	// 무는 늑대의 주둥이(앞으로 약 0.75*size), 그림자는 반지름 약 0.53*size다. 중심을 0.6*size 높이에 두고
	// 반지름 1.3*size면 전부 들어온다.
	SetBoundingSphere(size * 1.3f, size * 0.6f);
}

void CharacterActor::TriggerFlash(FlashKind kind, float duration)
{
	m_FlashKind = kind;
	m_FlashTimer = duration;
	m_FlashDuration = duration;
}

void CharacterActor::OnUpdate(const UpdateContext& ctx)
{
	if (m_FlashTimer > 0.f)
	{
		m_FlashTimer -= ctx.deltaSeconds;
		if (m_FlashTimer < 0.f)
		{
			m_FlashTimer = 0.f;
		}
	}
}

void CharacterActor::OnRender(const RenderContext& ctx)
{
	float time = ctx.time;
	float facing = GetFacing();

	Pose pose;
	pose.x = GetWorldX();
	pose.y = GetWorldY();
	pose.z = GetWorldZ();
	pose.size = GetSize();
	pose.time = time;

	// facing 기준 정면/측면 단위 벡터. 팔다리·머리를 이 축으로 배치해서 캐릭터가
	// 이동/공격 방향으로 실제로 돌아보는 것처럼 보이게 한다.
	pose.forwardX = cosf(facing);
	pose.forwardY = sinf(facing);
	pose.rightX = -pose.forwardY;
	pose.rightY = pose.forwardX;

	pose.breathe = sinf(time * 4.f + pose.x * 3.1f) * 0.03f; // 개체마다 위상이 다른 미세한 숨쉬기
	pose.walkBob = m_Walking ? fabsf(sinf(time * 8.f)) * 0.05f : 0.f;
	pose.legSwing = m_Walking ? sinf(time * 8.f) * 0.15f : 0.f;
	pose.attack = (m_AttackProgress >= 0.f) ? sinf(m_AttackProgress * 3.14159265f) : 0.f;
	pose.flash = (m_FlashTimer > 0.f && m_FlashDuration > 0.f) ? m_FlashTimer / m_FlashDuration : 0.f;
	pose.flashKind = m_FlashKind;
	pose.alpha = m_Opacity;
	pose.glowEyes = m_GlowingEyes;

	Shapes::Rgb bodyColor = { GetR(), GetG(), GetB() };

	switch (m_Shape)
	{
	case BodyShape::Human:   DrawHuman(ctx, pose, bodyColor, m_Look); break;
	case BodyShape::Deer:    DrawQuadruped(ctx, pose, bodyColor, m_Shape); break;
	case BodyShape::Wolf:    DrawQuadruped(ctx, pose, bodyColor, m_Shape); break;
	case BodyShape::Cow:     DrawQuadruped(ctx, pose, bodyColor, m_Shape); break;
	case BodyShape::Serpent: DrawSerpent(ctx, pose, bodyColor); break;
	}
}

// 발밑 그림자. 사람은 둥글게, 네 발 짐승은 바라보는 방향으로 길쭉하게, 뱀은 뒤로 길게 늘어진 몸을 따라 깐다.
void CharacterActor::OnRenderShadow(const RenderContext& ctx)
{
	float size = GetSize();
	float facing = GetFacing();
	float x = GetWorldX();
	float y = GetWorldY();
	Mat4 model;

	if (m_Shape == BodyShape::Human)
	{
		model = Mat4::Translate(x, y, 0.001f) * Mat4::Scale(size * 0.8f, size * 0.8f, 1.f);
	}
	else if (m_Shape == BodyShape::Serpent)
	{
		float backX = x - cosf(facing) * size * 0.8f;
		float backY = y - sinf(facing) * size * 0.8f;
		model = Mat4::Translate(backX, backY, 0.001f) * Mat4::RotateZ(facing) * Mat4::Scale(size * 2.3f, size * 0.6f, 1.f);
	}
	else
	{
		model = Mat4::Translate(x, y, 0.001f) * Mat4::RotateZ(facing) * Mat4::Scale(size * 1.05f, size * 0.5f, 1.f);
	}

	ctx.renderer.DrawShadow(ctx.viewProjection * model, 0.45f * m_Opacity);
}

// 걸어서 이동한다. 예전엔 짐승만 쓰던 것을 마을 사람도 같이 쓰도록 여기로 옮겼다.
bool CharacterActor::MoveToward(const UpdateContext& ctx, float targetX, float targetY, float speed, float stopDistance)
{
	float x = GetX();
	float y = GetY();
	float dx = targetX - x;
	float dy = targetY - y;
	float distance = sqrtf(dx * dx + dy * dy);

	if (distance <= stopDistance)
	{
		return false;
	}

	// 목표를 지나쳐 앞뒤로 떨지 않도록, 남은 거리보다 멀리는 가지 않는다.
	float step = speed * ctx.deltaSeconds;
	if (step > distance - stopDistance)
	{
		step = distance - stopDistance;
	}

	float newX = x + dx / distance * step;
	if (CanStepTo(ctx, x, y, newX, y))
	{
		x = newX;
	}

	float newY = y + dy / distance * step;
	if (CanStepTo(ctx, x, y, x, newY))
	{
		y = newY;
	}

	float movedX = x - GetX();
	float movedY = y - GetY();
	if (movedX * movedX + movedY * movedY < 0.0000001f)
	{
		return false; // 사방이 막혀서 못 움직임
	}

	SetFacing(atan2f(movedY, movedX));
	SetPosition(x, y, GetZ());
	return true;
}

bool CharacterActor::CanStepTo(const UpdateContext& ctx, float fromX, float fromY, float toX, float toY) const
{
	if (!ctx.tileMap.IsWorldPositionWalkable(toX, toY) || ctx.scene.IsBlocked(toX, toY, kMoverRadius))
	{
		return false;
	}

	// 다른 캐릭터(플레이어/짐승/NPC)와 몸이 겹치게 되는 걸음은 막는다 — 예전엔 늑대 여러 마리가 한 점에
	// 겹쳐 한 마리처럼 보이고 플레이어 몸속으로 파고들었다. 이미 겹친 상태에서 멀어지는 걸음은 허용해서,
	// 겹친 채로 둘 다 꼼짝 못 하게 되는 일은 없게 한다.
	const Actor* self = this;
	Actor* other = ctx.scene.FindNearest(toX, toY, PersonalSpace(), [self](const Actor& actor)
	{
		ActorType type = actor.GetType();
		return &actor != self && (type == ActorType::Animal || type == ActorType::Player || type == ActorType::NPC);
	});

	if (other == nullptr)
	{
		return true;
	}

	float otherX = other->GetWorldX();
	float otherY = other->GetWorldY();
	float before = (fromX - otherX) * (fromX - otherX) + (fromY - otherY) * (fromY - otherY);
	float after = (toX - otherX) * (toX - otherX) + (toY - otherY) * (toY - otherY);

	return after >= before;
}

float CharacterActor::PersonalSpace() const
{
	return kMoverRadius * 2.f;
}

// ---------------------------------------------------------------- NpcActor

NpcActor::NpcActor(float x, float y, float size, float r, float g, float b, const HumanLook& look, int interactId, const char* name)
	: CharacterActor(ActorType::NPC, BodyShape::Human)
{
	SetPosition(x, y, 0.f);
	SetSize(size);
	SetColor(r, g, b);
	SetLook(look);
	SetInteractId(interactId);
	SetName(name);
	FitBoundsToSize();
}

void NpcActor::SetFaded(float baseOpacity)
{
	m_BaseOpacity = baseOpacity;
	SetOpacity(baseOpacity);
}

void NpcActor::Flare()
{
	m_FlareTimer = kFlareHoldSeconds + kFlareFadeSeconds;
}

void NpcActor::OnUpdate(const UpdateContext& ctx)
{
	CharacterActor::OnUpdate(ctx);

	// 흐려진 신: 이름이 불리면(Flare) 잠깐 또렷해졌다가 천천히 다시 흐려진다.
	if (m_BaseOpacity < 1.f)
	{
		if (m_FlareTimer > 0.f)
		{
			m_FlareTimer -= ctx.deltaSeconds;
			if (m_FlareTimer < 0.f)
			{
				m_FlareTimer = 0.f;
			}
		}

		float flare = (m_FlareTimer >= kFlareFadeSeconds) ? 1.f : m_FlareTimer / kFlareFadeSeconds;
		SetOpacity(m_BaseOpacity + (0.92f - m_BaseOpacity) * flare);
	}

	// NpcSociety가 적어 둔 쿨타임은 NPC 자신이 줄인다.
	m_Memory.socialCooldown = (std::max)(0.f, m_Memory.socialCooldown - ctx.deltaSeconds);
	m_Memory.reactionCooldown = (std::max)(0.f, m_Memory.reactionCooldown - ctx.deltaSeconds);

	// 주먹을 휘두르는 모션(말다툼의 삿대질·주먹다짐).
	if (m_SwingTimer > 0.f)
	{
		m_SwingTimer = (std::max)(0.f, m_SwingTimer - ctx.deltaSeconds);
	}
	SetAttackProgress(m_SwingTimer > 0.f ? 1.f - m_SwingTimer / kSwingSeconds : -1.f);

	if (m_FollowTarget != nullptr && IsVisible())
	{
		m_Activity = NpcActivity::Follow;
		m_Partner = nullptr;
		Follow(ctx);
		return;
	}

	UpdateRoutine(ctx);
}

void NpcActor::SetTraits(const NpcTraits& traits)
{
	m_Traits = traits;
	m_AnchorX = GetX();
	m_AnchorY = GetY();

	// 자리마다 다른 난수 씨앗 — 사람마다 걸음과 쉬는 시간이 엇갈려서 다 같이 한꺼번에 움직이지 않는다.
	unsigned int seed = (unsigned int)((m_AnchorX + 100.f) * 7919.f) ^ (unsigned int)((m_AnchorY + 100.f) * 104729.f);
	m_RandomState = (seed != 0u) ? seed : 1u;
	m_ActivityTimer = 0.5f + kIdleMinSeconds * NextRandom();
}

bool NpcActor::IsFree() const
{
	return m_Traits.social && m_FollowTarget == nullptr && IsVisible()
		&& (m_Activity == NpcActivity::Idle || m_Activity == NpcActivity::Stroll);
}

void NpcActor::BeginEncounter(NpcActor& partner, NpcActivity kind, float seconds)
{
	m_Activity = kind;
	m_ActivityTimer = seconds;
	m_Partner = &partner;
	m_StuckTimer = 0.f;
	SetWalking(false);
	FaceToward(partner.GetWorldX(), partner.GetWorldY());
}

void NpcActor::BeginBrawl(float seconds, int stamina)
{
	m_Activity = NpcActivity::Brawl;
	m_ActivityTimer = seconds;
	m_Partner = nullptr;
	m_BrawlStamina = stamina;
	m_PunchCooldown = 0.8f;
	m_BrawlOutcome = BrawlOutcome::None;
}

bool NpcActor::TakeBrawlHit()
{
	TriggerFlash(FlashKind::White, kAnimalHitFlashDuration);

	if (m_Activity != NpcActivity::Brawl)
	{
		return false;
	}

	--m_BrawlStamina;
	if (m_BrawlStamina > 0)
	{
		return false;
	}

	m_BrawlOutcome = BrawlOutcome::Yielded;
	EndActivity(kIdleMaxSeconds);
	return true;
}

BrawlOutcome NpcActor::ConsumeBrawlOutcome()
{
	BrawlOutcome outcome = m_BrawlOutcome;
	m_BrawlOutcome = BrawlOutcome::None;
	return outcome;
}

void NpcActor::FaceToward(float x, float y)
{
	float dx = x - GetWorldX();
	float dy = y - GetWorldY();
	if (dx * dx + dy * dy > 0.0001f)
	{
		SetFacing(atan2f(dy, dx));
	}
}

// 스스로 하는 일: 서서 둘러보기 → 자기 자리 둘레 거닐기 → 다시 쉬기. NpcSociety가 짝지어 준 수다·말다툼·
// 주먹다짐은 정해진 시간 동안 상대를 마주 보고 하다가 끝나면 다시 거닌다. 플레이어와의 주먹다짐은 쫓아가며
// 주먹을 휘두르다가, 플레이어가 멀리 달아나거나 시간이 다 되면 그만둔다.
void NpcActor::UpdateRoutine(const UpdateContext& ctx)
{
	float dt = ctx.deltaSeconds;

	switch (m_Activity)
	{
	case NpcActivity::Chat:
	case NpcActivity::Argue:
	case NpcActivity::Scuffle:
		{
			SetWalking(false);

			if (m_Partner == nullptr || !m_Partner->IsVisible())
			{
				EndActivity(kIdleMinSeconds);
				break;
			}

			FaceToward(m_Partner->GetWorldX(), m_Partner->GetWorldY());

			// 주먹다짐은 자주 주먹을 휘두르고 맞은 쪽이 번쩍인다(목숨은 걸지 않는 실랑이라 체력은 없다).
			// 말다툼은 가끔 삿대질만 한다.
			float swingsPerSecond = (m_Activity == NpcActivity::Scuffle) ? 1.8f : ((m_Activity == NpcActivity::Argue) ? 0.5f : 0.f);
			if (m_SwingTimer <= 0.f && NextRandom() < swingsPerSecond * dt)
			{
				m_SwingTimer = kSwingSeconds;
				if (m_Activity == NpcActivity::Scuffle)
				{
					m_Partner->TriggerFlash(FlashKind::White, 0.12f);
				}
			}

			m_ActivityTimer -= dt;
			if (m_ActivityTimer <= 0.f)
			{
				// 다투고 나면 서로 등을 돌려 떨어져 걷고, 수다를 떨고 나면 그 자리에서 잠깐 쉰다.
				NpcActor* partner = m_Partner;
				bool parted = (m_Activity != NpcActivity::Chat);
				EndActivity(kIdleMinSeconds);
				if (parted && m_Traits.wanderRadius > 0.f && PickStrollTarget(ctx, partner->GetWorldX(), partner->GetWorldY(), true))
				{
					m_Activity = NpcActivity::Stroll;
				}
			}
		}
		break;

	case NpcActivity::Brawl:
		{
			m_ActivityTimer -= dt;

			PlayerActor* player = ctx.scene.GetPlayer();
			if (player == nullptr)
			{
				m_BrawlOutcome = BrawlOutcome::GaveUp;
				EndActivity(kIdleMaxSeconds);
				break;
			}

			float playerX = player->GetWorldX();
			float playerY = player->GetWorldY();
			float dx = playerX - GetX();
			float dy = playerY - GetY();
			float distance = sqrtf(dx * dx + dy * dy);

			if (distance > kBrawlGiveUpDistance || m_ActivityTimer <= 0.f)
			{
				m_BrawlOutcome = BrawlOutcome::GaveUp;
				EndActivity(kIdleMaxSeconds);
				break;
			}

			bool moved = MoveToward(ctx, playerX, playerY, kBrawlSpeed, kBrawlStopDistance);
			SetWalking(moved);
			FaceToward(playerX, playerY);

			m_PunchCooldown = (std::max)(0.f, m_PunchCooldown - dt);
			if (distance <= kBrawlReach && m_PunchCooldown <= 0.f)
			{
				m_SwingTimer = kSwingSeconds;
				m_PunchCooldown = kBrawlPunchCooldown;
				player->TakeDamage(kBrawlPunchDamage, true);
			}
		}
		break;

	case NpcActivity::Stroll:
		{
			float dx = m_StrollX - GetX();
			float dy = m_StrollY - GetY();
			if (dx * dx + dy * dy <= 0.15f * 0.15f)
			{
				EndActivity(kIdleMinSeconds + (kIdleMaxSeconds - kIdleMinSeconds) * NextRandom());
				break;
			}

			bool moved = MoveToward(ctx, m_StrollX, m_StrollY, kStrollSpeed, 0.1f);
			SetWalking(moved);

			// 누가 길을 막고 서 있거나 담에 걸려 못 가면 잠깐 서 있다가 다른 데로 간다.
			m_StuckTimer = moved ? 0.f : m_StuckTimer + dt;
			if (m_StuckTimer > 1.2f)
			{
				EndActivity(1.f + 2.f * NextRandom());
			}
		}
		break;

	case NpcActivity::Follow:
		// 따라다니다 풀려났으면 그 자리에서 쉰다.
		EndActivity(kIdleMinSeconds);
		break;

	case NpcActivity::Idle:
	default:
		{
			SetWalking(false);

			m_ActivityTimer -= dt;
			if (m_ActivityTimer > 0.f)
			{
				break;
			}

			if (m_Traits.wanderRadius > 0.f && PickStrollTarget(ctx, 0.f, 0.f, false))
			{
				m_Activity = NpcActivity::Stroll;
				m_StuckTimer = 0.f;
				break;
			}

			// 제자리를 지키는 마을 사람도 가끔 고개를 돌려 둘러본다(이야기 인물·신은 정해 둔 방향을 지킨다).
			if (m_Traits.social)
			{
				SetFacing(GetFacing() + (NextRandom() - 0.5f) * 1.6f);
			}
			m_ActivityTimer = kIdleMinSeconds + (kIdleMaxSeconds - kIdleMinSeconds) * NextRandom();
		}
		break;
	}
}

void NpcActor::EndActivity(float idleSeconds)
{
	m_Activity = NpcActivity::Idle;
	m_ActivityTimer = idleSeconds;
	m_Partner = nullptr;
	m_StuckTimer = 0.f;
	SetWalking(false);
}

// 거닐 곳을 고른다. away면 지금 자리에서 (awayFromX, awayFromY)의 반대쪽으로(다툰 상대에게서 멀어지게), 아니면
// 자기 자리(anchor) 둘레 아무 데나. 걸을 수 있는 곳을 몇 번 찾아보고 못 찾으면 false.
bool NpcActor::PickStrollTarget(const UpdateContext& ctx, float awayFromX, float awayFromY, bool away)
{
	float radius = m_Traits.wanderRadius;

	for (int attempt = 0; attempt < 6; ++attempt)
	{
		float x;
		float y;

		if (away)
		{
			float dx = GetX() - awayFromX;
			float dy = GetY() - awayFromY;
			float length = sqrtf(dx * dx + dy * dy);
			if (length < 0.01f)
			{
				dx = 1.f;
				dy = 0.f;
				length = 1.f;
			}

			float angle = atan2f(dy, dx) + (NextRandom() - 0.5f) * 1.2f;
			float step = 1.2f + NextRandom();
			x = GetX() + cosf(angle) * step;
			y = GetY() + sinf(angle) * step;

			// 자기 자리에서 너무 멀어지지 않게 안쪽으로 당긴다.
			float fromAnchorX = x - m_AnchorX;
			float fromAnchorY = y - m_AnchorY;
			float fromAnchor = sqrtf(fromAnchorX * fromAnchorX + fromAnchorY * fromAnchorY);
			float limit = radius + 1.f;
			if (fromAnchor > limit)
			{
				x = m_AnchorX + fromAnchorX / fromAnchor * limit;
				y = m_AnchorY + fromAnchorY / fromAnchor * limit;
			}
		}
		else
		{
			float angle = NextRandom() * 6.2831853f;
			float distance = radius * (0.3f + 0.7f * NextRandom());
			x = m_AnchorX + cosf(angle) * distance;
			y = m_AnchorY + sinf(angle) * distance;
		}

		if (ctx.tileMap.IsWorldPositionWalkable(x, y) && !ctx.scene.IsBlocked(x, y, kMoverRadius))
		{
			m_StrollX = x;
			m_StrollY = y;
			return true;
		}
	}

	return false;
}

float NpcActor::NextRandom()
{
	// xorshift32 — NPC마다 따로 굴러가는 가벼운 난수.
	unsigned int state = m_RandomState;
	state ^= state << 13;
	state ^= state >> 17;
	state ^= state << 5;
	m_RandomState = (state != 0u) ? state : 0x9E3779B9u;
	return (float)(m_RandomState & 0xFFFFFFu) / 16777216.f;
}

void NpcActor::Follow(const UpdateContext& ctx)
{
	float targetX = m_FollowTarget->GetWorldX();
	float targetY = m_FollowTarget->GetWorldY();
	float x = GetX();
	float y = GetY();
	float dx = targetX - x;
	float dy = targetY - y;
	float distance = sqrtf(dx * dx + dy * dy);

	// 너무 멀어졌으면(막 나타났거나 막혀서 뒤처졌으면) 따라가는 대상의 등 뒤로 순간이동한다.
	if (distance > kFollowTeleport)
	{
		float facing = m_FollowTarget->GetFacing();
		float newX = targetX - cosf(facing) * 1.2f;
		float newY = targetY - sinf(facing) * 1.2f;
		if (!ctx.tileMap.IsWorldPositionWalkable(newX, newY))
		{
			newX = targetX;
			newY = targetY;
		}

		SetPosition(newX, newY, GetZ());
		SetFacing(facing);
		SetWalking(false);
		return;
	}

	if (distance <= kFollowDistance)
	{
		SetWalking(false);
		return;
	}

	float step = kFollowSpeed * ctx.deltaSeconds;
	if (step > distance - kFollowDistance)
	{
		step = distance - kFollowDistance;
	}

	float newX = x + dx / distance * step;
	if (ctx.tileMap.IsWorldPositionWalkable(newX, y) && !ctx.scene.IsBlocked(newX, y, kMoverRadius))
	{
		x = newX;
	}

	float newY = y + dy / distance * step;
	if (ctx.tileMap.IsWorldPositionWalkable(x, newY) && !ctx.scene.IsBlocked(x, newY, kMoverRadius))
	{
		y = newY;
	}

	bool moved = (x != GetX() || y != GetY());
	SetWalking(moved);
	SetFacing(atan2f(dy, dx));
	SetPosition(x, y, GetZ());
}

// ------------------------------------------------------------- PlayerActor

PlayerActor::PlayerActor(float x, float y)
	: CharacterActor(ActorType::Player, BodyShape::Human)
{
	SetPosition(x, y, 0.f);
	SetSize(0.8f);
	SetColor(0.9f, 0.85f, 0.8f); // 흰 튜닉
	SetName("Player");
	SetSpawnPoint(x, y);

	// 청동 투구(붉은 깃털 장식) + 창: 한눈에 "그리스 전사"로 보이고, 마을 사람들 사이에서도 바로 찾을 수 있다.
	HumanLook look;
	look.helmet = true;
	look.spear = true;
	SetLook(look);

	FitBoundsToSize();
}

void PlayerActor::OnUpdate(const UpdateContext& ctx)
{
	CharacterActor::OnUpdate(ctx);

	float moveX = m_MoveX;
	float moveY = m_MoveY;
	bool isMoving = (moveX != 0.f || moveY != 0.f);
	SetWalking(isMoving);

	if (isMoving)
	{
		float length = sqrtf(moveX * moveX + moveY * moveY);
		moveX /= length;
		moveY /= length;
		SetFacing(atan2f(moveY, moveX));

		float buffSpeed = (m_BuffTimer > 0.f) ? m_BuffSpeedScale : 1.f;
		float step = kPlayerSpeed * m_SpeedScale * buffSpeed * ctx.deltaSeconds;
		float x = GetX();
		float y = GetY();

		// 축별로 따로 검사해서, 물가/건물/나무 벽에 붙어 미끄러지듯 이동하되
		// 그 위로는 올라갈 수 없게 한다 (호수·바위·건물·나무 = 접근 불가 영역).
		float newX = x + moveX * step;
		if (ctx.tileMap.IsWorldPositionWalkable(newX, y) && !ctx.scene.IsBlocked(newX, y, kMoverRadius))
		{
			x = newX;
		}

		float newY = y + moveY * step;
		if (ctx.tileMap.IsWorldPositionWalkable(x, newY) && !ctx.scene.IsBlocked(x, newY, kMoverRadius))
		{
			y = newY;
		}

		// 맵 경계 안쪽으로 클램프(맵 반너비보다 살짝 안쪽).
		float halfX = ctx.tileMap.GetWidth() * 0.5f - 0.5f;
		float halfY = ctx.tileMap.GetHeight() * 0.5f - 0.5f;
		if (x < -halfX) x = -halfX;
		if (x > halfX) x = halfX;
		if (y < -halfY) y = -halfY;
		if (y > halfY) y = halfY;

		SetPosition(x, y, GetZ());
	}

	if (m_AttackTimer > 0.f)
	{
		m_AttackTimer -= ctx.deltaSeconds;
		if (m_AttackTimer < 0.f)
		{
			m_AttackTimer = 0.f;
		}
	}
	SetAttackProgress(m_AttackTimer > 0.f ? 1.f - m_AttackTimer / kAttackAnimDuration : -1.f);

	if (m_BuffTimer > 0.f)
	{
		m_BuffTimer -= ctx.deltaSeconds;
		if (m_BuffTimer <= 0.f)
		{
			m_BuffTimer = 0.f;
			GameLog::Add(GameLog::Kind::Info, "[신속의 날개] 날개의 힘이 가라앉았다.");
		}
	}
}

// 각성의 기운: 보통 그림자 위에 수호신 색의 빛이 숨 쉬듯 번진다(밝기가 1을 넘어 블룸으로 은은하게 빛남).
// 신속의 날개(버프)가 걸려 있는 동안엔 발밑에 하늘빛 바람이 더 빠르게 맥박친다.
void PlayerActor::OnRenderShadow(const RenderContext& ctx)
{
	CharacterActor::OnRenderShadow(ctx);

	float x = GetWorldX();
	float y = GetWorldY();
	float size = GetSize();

	if (m_Aura)
	{
		float pulse = 0.75f + 0.25f * sinf(ctx.time * 2.4f);
		Mat4 model = Mat4::Translate(x, y, 0.002f) * Mat4::Scale(size * 2.2f * pulse, size * 2.2f * pulse, 1.f);
		ctx.renderer.DrawSoftDisc(ctx.viewProjection * model, m_AuraR * 1.4f, m_AuraG * 1.4f, m_AuraB * 1.4f, 0.38f);
	}

	if (m_BuffTimer > 0.f)
	{
		float pulse = 0.7f + 0.3f * sinf(ctx.time * 9.f);
		Mat4 model = Mat4::Translate(x, y, 0.003f) * Mat4::Scale(size * 1.7f * pulse, size * 1.7f * pulse, 1.f);
		ctx.renderer.DrawSoftDisc(ctx.viewProjection * model, 0.8f, 1.3f, 1.6f, 0.35f);
	}
}

void PlayerActor::StartAttack()
{
	m_AttackTimer = kAttackAnimDuration;
}

void PlayerActor::FaceToward(float x, float y)
{
	SetFacing(atan2f(y - GetWorldY(), x - GetWorldX()));
}

void PlayerActor::GrantXP(int amount)
{
	m_Stats.xp += amount;
	GameLog::Add(GameLog::Kind::Reward, "[경험치 획득] +" + std::to_string(amount)
		+ " (현재 " + std::to_string(m_Stats.xp) + "/" + std::to_string(m_Stats.xpToNext) + ")");

	while (m_Stats.xp >= m_Stats.xpToNext)
	{
		m_Stats.xp -= m_Stats.xpToNext;
		m_Stats.level += 1;
		m_Stats.xpToNext = (int)(m_Stats.xpToNext * 1.5f);

		// 기본 성장 + 수호신마다 더 얹히는 몫(제우스는 공격력, 포세이돈은 체력, 하데스는 처치 회복 등).
		m_Stats.attackPower += 3 + m_Stats.growthAttack;
		m_Stats.maxHp += 8 + m_Stats.growthMaxHp;
		m_Stats.defense += m_Stats.growthDefense;
		m_Stats.lifeOnKill += m_Stats.growthLifeOnKill;
		m_Stats.hp = m_Stats.maxHp;

		std::string message = "[레벨업!] Lv." + std::to_string(m_Stats.level)
			+ " (공격력 " + std::to_string(m_Stats.attackPower)
			+ ", 최대체력 " + std::to_string(m_Stats.maxHp);
		if (m_Stats.defense > 0)
		{
			message += ", 방어 " + std::to_string(m_Stats.defense);
		}
		if (m_Stats.lifeOnKill > 0)
		{
			message += ", 처치 회복 " + std::to_string(m_Stats.lifeOnKill);
		}
		GameLog::Add(GameLog::Kind::Reward, message + ")");
	}
}

// 피해를 적용한다: 방어만큼 줄이고(최소 1), 붉은 피격 플래시, 쓰러지면 리스폰. 마을 사람과의 주먹다짐(nonLethal)은
// 체력이 1 밑으로 내려가지 않는다 — 목숨까지 걸 싸움은 아니다.
void PlayerActor::TakeDamage(int amount, bool nonLethal)
{
	int taken = amount - m_Stats.defense;
	if (taken < 1)
	{
		taken = 1;
	}
	if (nonLethal && m_Stats.hp - taken < 1)
	{
		taken = (m_Stats.hp > 1) ? m_Stats.hp - 1 : 0;
	}

	TriggerFlash(FlashKind::Red, kPlayerHitFlashDuration);
	if (taken <= 0)
	{
		GameLog::Add(GameLog::Kind::Damage, "[주먹다짐] 비틀거리며 겨우 버틴다. 맞서 쓰러뜨리거나 멀리 피하자.");
		return;
	}

	m_Stats.hp -= taken;

	int shownHp = (m_Stats.hp > 0) ? m_Stats.hp : 0;
	GameLog::Add(GameLog::Kind::Damage, std::string(nonLethal ? "[주먹다짐] " : "[피격!] ") + std::to_string(taken)
		+ " 피해를 입었다 (체력 " + std::to_string(shownHp) + "/" + std::to_string(m_Stats.maxHp) + ")");

	if (m_Stats.hp <= 0)
	{
		Respawn();
	}
}

void PlayerActor::ApplyPatronPassive(const PatronDef& def)
{
	m_Stats.attackPower += def.attack;
	m_Stats.maxHp += def.maxHp;
	m_Stats.hp += def.maxHp;
	m_Stats.defense += def.defense;
	m_Stats.critChance += def.critChance;
	m_Stats.lifeOnKill += def.lifeOnKill;
	m_Stats.cooldownScale *= def.cooldownScale;
	m_Stats.growthAttack += def.growthAttack;
	m_Stats.growthMaxHp += def.growthMaxHp;
	m_Stats.growthDefense += def.growthDefense;
	m_Stats.growthLifeOnKill += def.growthLifeOnKill;
}

float PlayerActor::GetDetectScale() const
{
	return m_DetectScale * ((m_BuffTimer > 0.f) ? m_BuffDetectScale : 1.f);
}

float PlayerActor::GetCooldownScale() const
{
	return m_Stats.cooldownScale * ((m_BuffTimer > 0.f) ? m_BuffCooldownScale : 1.f);
}

void PlayerActor::StartBuff(float seconds, float speedScale, float cooldownScale, float detectScale)
{
	m_BuffTimer = seconds;
	m_BuffSpeedScale = speedScale;
	m_BuffCooldownScale = cooldownScale;
	m_BuffDetectScale = detectScale;
}

void PlayerActor::SetAura(bool enabled, float r, float g, float b)
{
	m_Aura = enabled;
	m_AuraR = r;
	m_AuraG = g;
	m_AuraB = b;
}

void PlayerActor::Heal(int amount)
{
	m_Stats.hp += amount;
	if (m_Stats.hp > m_Stats.maxHp)
	{
		m_Stats.hp = m_Stats.maxHp;
	}
}

// 체력이 0 이하로 떨어져 쓰러졌을 때: 이 지역의 시작 지점으로 되돌리고 체력을 채워 다시
// 시작한다(세이브/로드가 없는 프로토타입이라 페널티 없는 리스폰으로 단순하게 처리).
void PlayerActor::Respawn()
{
	GameLog::Add(GameLog::Kind::Damage, "[쓰러졌다...] 정신을 차려 보니 처음 자리로 돌아와 있다.");
	SetPosition(m_SpawnX, m_SpawnY, GetZ());
	m_Stats.hp = m_Stats.maxHp;
	m_JustDied = true;
}

// ------------------------------------------------------------- AnimalActor

AnimalActor::AnimalActor(AnimalKind kind, float anchorX, float anchorY, float phase)
	: CharacterActor(ActorType::Animal, GetSpec(kind).shape)
	, m_Kind(kind)
	, m_AnchorX(anchorX)
	, m_AnchorY(anchorY)
	, m_Phase(phase)
	, m_Hp(GetSpec(kind).hp)
{
	const AnimalSpec& spec = GetSpec(kind);

	SetPosition(anchorX, anchorY, 0.f);
	SetSize(spec.size);

	// 개체마다 털 색을 살짝 흔들어 단조로움을 줄인다(phase로 정해지므로 같은 개체는 늘 같은 색).
	float tint = 0.94f + 0.12f * (0.5f + 0.5f * sinf(phase * 12.9898f));
	SetColor(spec.r * tint, spec.g * tint, spec.b * tint);
	SetName(spec.name);
	SetGlowingEyes(spec.shadow);

	// 사람 모양 괴물의 겉모습: 청동 전사는 온몸이 청동인 창병, 크로노스의 잔향은 반쯤 비치는 거대한 옛 왕.
	if (kind == AnimalKind::BronzeWarrior)
	{
		HumanLook look;
		look.bronzeBody = true;
		look.helmet = true;
		look.spear = true;
		SetLook(look);
	}
	else if (kind == AnimalKind::KronosEcho)
	{
		HumanLook look;
		look.hairR = 0.32f; look.hairG = 0.3f; look.hairB = 0.36f;
		look.beardR = 0.36f; look.beardG = 0.34f; look.beardB = 0.4f;
		look.beard = true;
		look.longHair = true;
		look.robe = true;
		look.staff = true;
		look.wreath = true; look.wreathR = 0.22f; look.wreathG = 0.18f; look.wreathB = 0.26f; // 녹슨 옛 왕관
		SetLook(look);
		SetOpacity(0.82f);
	}

	FitBoundsToSize();
}

bool AnimalActor::IsAggressive() const
{
	return GetSpec(m_Kind).aggressive;
}

int AnimalActor::GetKillXP() const
{
	return GetSpec(m_Kind).xp;
}

int AnimalActor::GetMaxHp() const
{
	return GetSpec(m_Kind).hp;
}

AnimalNames GetAnimalNames(AnimalKind kind)
{
	switch (kind)
	{
	case AnimalKind::Deer: return { "사슴", "사슴을" };
	case AnimalKind::Wolf: return { "늑대", "늑대를" };
	case AnimalKind::ShadowWolf: return { "그림자 늑대", "그림자 늑대를" };
	case AnimalKind::ShadowPython: return { "그림자 퓌톤", "그림자 퓌톤을" };
	case AnimalKind::BronzeWarrior: return { "청동 전사", "청동 전사를" };
	case AnimalKind::KronosEcho: return { "크로노스의 잔향", "크로노스의 잔향을" };
	case AnimalKind::WhiteCow: return { "하얀 암소", "하얀 암소를" };
	}
	return { "짐승", "짐승을" };
}

void AnimalActor::OnUpdate(const UpdateContext& ctx)
{
	// 아직 나타나지 않은(이야기 존재 조건이 거짓인) 괴물은 움직이지도 공격하지도 않는다.
	if (!IsVisible())
	{
		return;
	}

	CharacterActor::OnUpdate(ctx);

	m_StunTimer = (std::max)(0.f, m_StunTimer - ctx.deltaSeconds);
	m_CharmTimer = (std::max)(0.f, m_CharmTimer - ctx.deltaSeconds);
	m_FearTimer = (std::max)(0.f, m_FearTimer - ctx.deltaSeconds);

	// 실제로 움직였을 때만 걷는 모션을 켠다(예전엔 제자리에 서 있어도 늘 다리를 흔들었다).
	// 스킬에 걸린 동안: 기절이면 꼼짝 못 하고, 홀리면 헤매고, 겁먹으면 달아난다(셋 다 물지 않는다).
	bool moved = false;
	if (m_StunTimer > 0.f)
	{
		moved = false;
	}
	else if (m_CharmTimer > 0.f)
	{
		m_IsChasing = false;
		moved = Wander(ctx);
	}
	else if (m_FearTimer > 0.f)
	{
		m_IsChasing = false;
		moved = Flee(ctx);
	}
	else
	{
		moved = IsAggressive() ? UpdateMonsterAI(ctx) : Wander(ctx);
	}
	SetWalking(moved);

	if (m_AttackCooldown > 0.f)
	{
		m_AttackCooldown -= ctx.deltaSeconds;
		if (m_AttackCooldown < 0.f)
		{
			m_AttackCooldown = 0.f;
		}
	}

	if (m_LungeTimer > 0.f)
	{
		m_LungeTimer -= ctx.deltaSeconds;
		if (m_LungeTimer < 0.f)
		{
			m_LungeTimer = 0.f;
		}
	}
	SetAttackProgress(m_LungeTimer > 0.f ? 1.f - m_LungeTimer / kLungeDuration : -1.f);
}

// 그림자 괴물은 검은 그림자 대신 발밑에 보랏빛 어둠이 번진다(어디서 왔는지 한눈에 보이게).
void AnimalActor::OnRenderShadow(const RenderContext& ctx)
{
	if (!GetSpec(m_Kind).shadow)
	{
		CharacterActor::OnRenderShadow(ctx);
		return;
	}

	float size = GetSize() * ((m_Kind == AnimalKind::ShadowPython) ? 2.6f : 1.5f);
	float pulse = 0.8f + 0.2f * sinf(ctx.time * 3.f + m_Phase);
	Mat4 model = Mat4::Translate(GetWorldX(), GetWorldY(), 0.001f) * Mat4::Scale(size, size * 0.8f, 1.f);
	ctx.renderer.DrawSoftDisc(ctx.viewProjection * model, 0.2f, 0.05f, 0.3f, 0.6f * pulse);
}

bool AnimalActor::TakeHit(int damage)
{
	m_Hp -= damage;
	TriggerFlash(FlashKind::White, kAnimalHitFlashDuration);

	if (m_Hp <= 0)
	{
		Destroy();
		return true;
	}

	return false;
}

// 배회: anchor 주변을 천천히 도는 목표점을 향해 걸어간다. 예전엔 위치를 이 목표점으로 곧바로 덮어써서,
// 늑대가 추적을 포기하는 순간 원래 자리로 순간이동하고 사슴은 나무·건물·물을 그냥 통과했다. 이제는
// 속도 제한과 충돌 검사를 거쳐 실제로 걸어가므로, 멀리 쫓아갔던 늑대도 걸어서 제자리로 돌아간다.
bool AnimalActor::Wander(const UpdateContext& ctx)
{
	float targetX = m_AnchorX + sinf(ctx.time * 0.4f + m_Phase) * 1.5f;
	float targetY = m_AnchorY + cosf(ctx.time * 0.25f + m_Phase) * 1.0f;

	return MoveToward(ctx, targetX, targetY, kWanderSpeed, 0.05f);
}

// 큰 몸(뱀)은 다른 캐릭터와 그만큼 넓게 떨어진다.
float AnimalActor::PersonalSpace() const
{
	return kMoverRadius * 2.f + ((m_Kind == AnimalKind::ShadowPython) ? 0.4f : 0.f);
}

void AnimalActor::Stun(float seconds)
{
	m_StunTimer = (std::max)(m_StunTimer, seconds);
}

void AnimalActor::Charm(float seconds)
{
	m_CharmTimer = (std::max)(m_CharmTimer, seconds);
	m_IsChasing = false;
}

void AnimalActor::Frighten(float seconds)
{
	m_FearTimer = (std::max)(m_FearTimer, seconds);
	m_IsChasing = false;
}

void AnimalActor::LoseTrack()
{
	if (m_IsChasing)
	{
		m_IsChasing = false;
		m_IsReturning = true;
	}
}

// 겁: 플레이어에게서 반대쪽으로 달아난다(자기 자리에서 너무 멀어지지 않게 목줄 반경 안에서).
bool AnimalActor::Flee(const UpdateContext& ctx)
{
	PlayerActor* player = ctx.scene.GetPlayer();
	if (player == nullptr)
	{
		return Wander(ctx);
	}

	float dx = GetX() - player->GetWorldX();
	float dy = GetY() - player->GetWorldY();
	float length = sqrtf(dx * dx + dy * dy);
	if (length < 0.01f)
	{
		dx = 1.f;
		dy = 0.f;
		length = 1.f;
	}

	const AnimalSpec& spec = GetSpec(m_Kind);
	float speed = (spec.chaseSpeed > 0.f) ? spec.chaseSpeed : kWanderSpeed * 1.5f;
	return MoveToward(ctx, GetX() + dx / length * 3.f, GetY() + dy / length * 3.f, speed, 0.05f);
}

// 스킬로 걸린 상태는 머리 위를 도는 작은 빛 셋으로 보여 준다(기절 노랑·홀림 분홍·겁 보라).
void AnimalActor::OnRender(const RenderContext& ctx)
{
	CharacterActor::OnRender(ctx);

	const Shapes::Rgb* mark = nullptr;
	if (m_StunTimer > 0.f)
	{
		mark = &kStunMarkColor;
	}
	else if (m_CharmTimer > 0.f)
	{
		mark = &kCharmMarkColor;
	}
	else if (m_FearTimer > 0.f)
	{
		mark = &kFearMarkColor;
	}

	if (mark == nullptr)
	{
		return;
	}

	float size = GetSize();
	float headZ = GetWorldZ() + size * ((m_Kind == AnimalKind::ShadowPython) ? 1.3f : 1.45f);
	float orbit = size * 0.35f;
	for (int i = 0; i < 3; ++i)
	{
		float angle = ctx.time * 4.f + (float)i * 2.0944f;
		float x = GetWorldX() + cosf(angle) * orbit;
		float y = GetWorldY() + sinf(angle) * orbit;
		Shapes::DrawUprightEllipse(ctx, x, y, headZ + 0.05f * sinf(angle * 2.f), 0.11f, 0.11f, *mark, 0.9f);
	}
}

// 공격형 괴물의 추적/공격 AI. 감지 반경 안에 플레이어가 들어오면 추적을 시작하고, 공격 사거리에 닿으면
// 쿨다운마다 문다. 플레이어가 너무 멀어지거나(giveUpRadius) 자기 anchor에서 너무 멀어지면(leashRadius)
// 추적을 포기하고, anchor 근처(kMonsterHomeRadius)로 걸어 돌아갈 때까지는 플레이어를 무시한다 —
// 영역 경계에서 추적/포기를 매 프레임 번갈아 하며 제자리에서 떠는 것을 막는다. 감지 반경은 플레이어의
// 축복(아프로디테의 변장)에 따라 줄어든다.
bool AnimalActor::UpdateMonsterAI(const UpdateContext& ctx)
{
	const AnimalSpec& spec = GetSpec(m_Kind);

	PlayerActor* player = ctx.scene.GetPlayer();
	if (player == nullptr)
	{
		return Wander(ctx);
	}

	float x = GetX();
	float y = GetY();
	float dxPlayer = player->GetWorldX() - x;
	float dyPlayer = player->GetWorldY() - y;
	float distToPlayerSq = dxPlayer * dxPlayer + dyPlayer * dyPlayer;

	float dxAnchor = x - m_AnchorX;
	float dyAnchor = y - m_AnchorY;
	float distFromAnchorSq = dxAnchor * dxAnchor + dyAnchor * dyAnchor;

	float detectRadius = spec.detectRadius * player->GetDetectScale();

	if (m_IsReturning)
	{
		if (distFromAnchorSq <= kMonsterHomeRadius * kMonsterHomeRadius)
		{
			m_IsReturning = false;
		}
	}
	else if (!m_IsChasing && distToPlayerSq <= detectRadius * detectRadius)
	{
		m_IsChasing = true;
	}
	else if (m_IsChasing && (distToPlayerSq > spec.giveUpRadius * spec.giveUpRadius ||
		distFromAnchorSq > spec.leashRadius * spec.leashRadius))
	{
		m_IsChasing = false;
		m_IsReturning = true;
	}

	if (!m_IsChasing)
	{
		// 배회 목표점은 anchor 근처라서, 돌아가는 중이면 자연스럽게 걸어서 제자리로 간다.
		return Wander(ctx);
	}

	// 바짝 붙었을 때 제자리에서 떨지 않도록 공격 사거리보다 조금 안쪽에서 멈춘다.
	bool moved = MoveToward(ctx, player->GetWorldX(), player->GetWorldY(), spec.chaseSpeed, spec.attackRadius * 0.6f);

	// 멈춰 있어도 플레이어를 노려본다.
	SetFacing(atan2f(dyPlayer, dxPlayer));

	if (distToPlayerSq <= spec.attackRadius * spec.attackRadius && m_AttackCooldown <= 0.f)
	{
		player->TakeDamage(spec.damage);
		m_AttackCooldown = spec.attackCooldown;
		m_LungeTimer = kLungeDuration;
	}

	return moved;
}
