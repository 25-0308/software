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

	// 화면 이미지를 좌우(horizontal) / 상하(vertical)로 뒤집는다. 투영 뒤의 NDC x, y 부호만
	// 바꾸므로 카메라가 보는 방향은 그대로다 — 보이는 면, 깊이 순서(앞뒤), 컬링 판정은 그대로이고
	// 그려진 그림만 뒤집힌다. 둘 다 켜면 그림이 180도 돌아간 것과 같다(월드의 위쪽이 화면 아래로 향함).
	void SetFlip(bool horizontal, bool vertical);
	bool IsFlippedHorizontally() const { return m_FlipHorizontal; }
	bool IsFlippedVertically() const { return m_FlipVertical; }

	// 지면을 회전시키는 yaw(라디안). 미니맵처럼 "카메라와 같은 방향으로 놓인 지도"를 그릴 때 쓴다.
	float GetYawRadians() const;

private:
	float m_ViewWidth;
	float m_ViewHeight;
	float m_Zoom;

	bool m_FlipHorizontal = false;
	bool m_FlipVertical = false;

	float m_FocusX = 0.f;
	float m_FocusY = 0.f;
	float m_FocusZ = 0.f;

	static const float kMinZoom;
	static const float kMaxZoom;
};
