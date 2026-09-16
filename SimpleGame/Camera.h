#pragma once

#include "Math3D.h"

// 고정 쿼터뷰(디메트릭) 카메라: 월드를 yaw 45도 / pitch 55도로 회전시킨 뒤
// 직교 투영하여 전형적인 2.5D 아이소메트릭 느낌을 만든다.
class Camera
{
public:
	Camera(float viewWidth, float viewHeight, float zoom);

	Mat4 GetViewProjection() const;

	// 현재 줌 배율에 `factor`를 곱한다 (>1이면 확대, <1이면 축소), 적정 범위로
	// 클램프됨. 줌의 중심은 현재 포커스 지점(SetFocus 참고)이며 화면 중앙이다.
	void AdjustZoom(float factor);

	// 카메라가 중심으로 삼는 월드 좌표를 설정한다. 매 프레임 플레이어 위치로
	// 호출하면 카메라가 플레이어를 따라간다.
	void SetFocus(float x, float y, float z);

private:
	float m_ViewWidth;
	float m_ViewHeight;
	float m_Zoom;

	float m_FocusX = 0.f;
	float m_FocusY = 0.f;
	float m_FocusZ = 0.f;

	static const float kMinZoom;
	static const float kMaxZoom;
};
