#pragma once

#include <cmath>

#include "Math3D.h"

// 컬링/공간 질의에 쓰는 경계 구. radius가 0 이하면 "경계 없음"(항상 보이는 것으로 취급).
struct BoundingSphere
{
	float x = 0.f, y = 0.f, z = 0.f;
	float radius = 0.f;

	bool IsValid() const { return radius > 0.f; }
};

// 카메라가 보는 영역(뷰 볼륨). 이 엔진의 카메라는 직교 투영이라 절두체가 직육면체이므로,
// 경계 구의 중심을 NDC로 옮겨서 [-1,1] 사각형과 겹치는지만 보면 된다. 뷰 회전은 거리를
// 보존하므로, 월드 반지름 r인 구는 NDC에서 가로 r*scaleX, 세로 r*scaleY인 타원(을 감싸는
// 사각형)이 된다. 판정은 항상 보수적이라서 화면에 조금이라도 걸치는 것을 잘못 걸러내진 않는다.
class ViewVolume
{
public:
	explicit ViewVolume(const Mat4& viewProjection)
		: m_ViewProjection(viewProjection)
	{
		// VP의 위 두 행 = (직교 투영의 축별 스케일) * (단위 길이의 회전 행)이라서,
		// 각 행의 길이가 곧 그 축의 NDC 스케일이다 (열 우선 저장: m[열*4+행]).
		const float* m = viewProjection.m;
		m_ScaleX = sqrtf(m[0] * m[0] + m[4] * m[4] + m[8] * m[8]);
		m_ScaleY = sqrtf(m[1] * m[1] + m[5] * m[5] + m[9] * m[9]);
	}

	// 경계 구가 조금이라도 화면에 걸치면 true. 경계가 없으면 항상 true.
	bool Intersects(const BoundingSphere& sphere) const
	{
		if (!sphere.IsValid())
		{
			return true;
		}

		float ndcX, ndcY;
		TransformToNDC(m_ViewProjection, sphere.x, sphere.y, sphere.z, ndcX, ndcY);

		// 경계 추정 오차로 화면 가장자리에서 오브젝트가 톡 튀어나오지 않도록 여유를 조금 둔다.
		const float kMargin = 0.05f;
		float extentX = 1.f + kMargin + sphere.radius * m_ScaleX;
		float extentY = 1.f + kMargin + sphere.radius * m_ScaleY;

		return fabsf(ndcX) <= extentX && fabsf(ndcY) <= extentY;
	}

private:
	Mat4 m_ViewProjection;
	float m_ScaleX;
	float m_ScaleY;
};
