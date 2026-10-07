#include "stdafx.h"
#include "CharacterActors.h"

#include <algorithm>
#include <cmath>
#include <string>

#include "GameLog.h"
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
				DrawPart(ctx, pose, legX, legY, z, s * 0.16f, s * 0.16f, s * 0.3f, kSkinColor);
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
		DrawPart(ctx, pose, leftArmX, leftArmY, armBaseZ, s * 0.14f, s * 0.14f, s * 0.3f, kSkinColor);
		DrawPart(ctx, pose, rightArmX, rightArmY, armBaseZ, s * 0.14f, s * 0.14f, s * 0.3f, kSkinColor);

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

		DrawCylinderPart(ctx, pose, pose.x, pose.y, headBase, s * 0.2f, s * 0.25f, kSkinColor, Shapes::Shade(kSkinColor, 1.1f));

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
	}

	// 네 발 짐승(사슴/늑대). 다리 4개를 먼저 깔고(먼 다리부터), 몸통·목·머리·귀·꼬리·뿔은 파츠마다 깊이를
	// 재서 먼 것부터 그린다. 상자는 회전하지 않으므로 몸통을 앞(가슴)·뒤(엉덩이) 두 덩어리로 바라보는
	// 방향을 따라 나란히 놓아서, 어느 방향을 봐도 몸이 그 방향으로 길쭉해 보이게 한다.
	void DrawQuadruped(const RenderContext& ctx, const Pose& pose, const Shapes::Rgb& fur, bool isWolf)
	{
		float s = pose.size;
		float z = pose.z;

		// 체형: 늑대는 낮고 다리가 굵으며, 사슴은 다리가 길고 가늘며 목이 솟아 있다.
		float legHeight = isWolf ? s * 0.3f : s * 0.4f;
		float legWidth = isWolf ? s * 0.09f : s * 0.07f;
		float halfSpan = s * 0.24f; // 앞다리~뒷다리 거리의 절반
		float legSide = s * 0.1f;
		float bodyBase = z + legHeight + pose.walkBob + pose.breathe;
		float bodyHeight = s * 0.26f;
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
		body.Add(px, py, bodyBase, s * 0.3f, s * 0.3f, bodyHeight, fur);
		Offset(pose, -s * 0.13f, 0.f, px, py);
		body.Add(px, py, bodyBase, s * 0.3f, s * 0.3f, bodyHeight * 0.94f, fur);

		float headBase = isWolf ? bodyBase + s * 0.1f : bodyBase + s * 0.32f;

		if (isWolf)
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
	case BodyShape::Deer:    DrawQuadruped(ctx, pose, bodyColor, false); break;
	case BodyShape::Wolf:    DrawQuadruped(ctx, pose, bodyColor, true); break;
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

	if (m_FollowTarget != nullptr && IsVisible())
	{
		Follow(ctx);
	}
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

		float step = kPlayerSpeed * m_SpeedScale * ctx.deltaSeconds;
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
		m_Stats.attackPower += 3;
		m_Stats.maxHp += 8;
		m_Stats.hp = m_Stats.maxHp;

		GameLog::Add(GameLog::Kind::Reward, "[레벨업!] Lv." + std::to_string(m_Stats.level)
			+ " (공격력 " + std::to_string(m_Stats.attackPower)
			+ ", 최대체력 " + std::to_string(m_Stats.maxHp) + ")");
	}
}

// 몬스터의 공격을 적용한다: 데미지, 붉은 피격 플래시, 사망 시 리스폰.
void PlayerActor::TakeDamage(int amount)
{
	m_Stats.hp -= amount;
	TriggerFlash(FlashKind::Red, kPlayerHitFlashDuration);

	int shownHp = (m_Stats.hp > 0) ? m_Stats.hp : 0;
	GameLog::Add(GameLog::Kind::Damage, "[피격!] " + std::to_string(amount)
		+ " 피해를 입었다 (체력 " + std::to_string(shownHp) + "/" + std::to_string(m_Stats.maxHp) + ")");

	if (m_Stats.hp <= 0)
	{
		Respawn();
	}
}

void PlayerActor::Heal(int amount)
{
	m_Stats.hp += amount;
	if (m_Stats.hp > m_Stats.maxHp)
	{
		m_Stats.hp = m_Stats.maxHp;
	}
}

void PlayerActor::AddMaxHp(int amount)
{
	m_Stats.maxHp += amount;
	m_Stats.hp += amount;
}

// 체력이 0 이하로 떨어져 쓰러졌을 때: 이 지역의 시작 지점으로 되돌리고 체력을 채워 다시
// 시작한다(세이브/로드가 없는 프로토타입이라 페널티 없는 리스폰으로 단순하게 처리).
void PlayerActor::Respawn()
{
	GameLog::Add(GameLog::Kind::Damage, "[쓰러졌다...] 정신을 차려 보니 처음 자리로 돌아와 있다.");
	SetPosition(m_SpawnX, m_SpawnY, GetZ());
	m_Stats.hp = m_Stats.maxHp;
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

void AnimalActor::OnUpdate(const UpdateContext& ctx)
{
	// 아직 나타나지 않은(이야기 존재 조건이 거짓인) 괴물은 움직이지도 공격하지도 않는다.
	if (!IsVisible())
	{
		return;
	}

	CharacterActor::OnUpdate(ctx);

	// 실제로 움직였을 때만 걷는 모션을 켠다(예전엔 제자리에 서 있어도 늘 다리를 흔들었다).
	bool moved = IsAggressive() ? UpdateMonsterAI(ctx) : Wander(ctx);
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

bool AnimalActor::MoveToward(const UpdateContext& ctx, float targetX, float targetY, float speed, float stopDistance)
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

bool AnimalActor::CanStepTo(const UpdateContext& ctx, float fromX, float fromY, float toX, float toY) const
{
	if (!ctx.tileMap.IsWorldPositionWalkable(toX, toY) || ctx.scene.IsBlocked(toX, toY, kMoverRadius))
	{
		return false;
	}

	// 다른 캐릭터(플레이어/짐승/NPC)와 몸이 겹치게 되는 걸음은 막는다 — 예전엔 늑대 여러 마리가 한 점에
	// 겹쳐 한 마리처럼 보이고 플레이어 몸속으로 파고들었다. 이미 겹친 상태에서 멀어지는 걸음은 허용해서,
	// 겹친 채로 둘 다 꼼짝 못 하게 되는 일은 없게 한다. 큰 몸(뱀)은 그만큼 넓게 잡는다.
	const Actor* self = this;
	float personalSpace = kMoverRadius * 2.f + ((m_Kind == AnimalKind::ShadowPython) ? 0.4f : 0.f);
	Actor* other = ctx.scene.FindNearest(toX, toY, personalSpace, [self](const Actor& actor)
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
