#include "stdafx.h"
#include "Camera.h"

namespace
{
	const float kYawRadians = 0.785398163f;   // 45도
	const float kPitchRadians = 0.959931086f; // 55도 (더 위에서 내려다보는 아이소메트릭 느낌)
}

const float Camera::kMinZoom = 0.25f;
const float Camera::kMaxZoom = 4.0f;

Camera::Camera(float viewWidth, float viewHeight, float zoom)
	: m_ViewWidth(viewWidth)
	, m_ViewHeight(viewHeight)
	, m_Zoom(zoom)
{
}

Mat4 Camera::GetViewProjection() const
{
	float halfWidth = m_ViewWidth * 0.5f / m_Zoom;
	float halfHeight = m_ViewHeight * 0.5f / m_Zoom;

	// 이 월드는 Z가 높이(위쪽) 축이므로, 수직축을 기준으로 도는 yaw는
	// RotateZ여야 한다. RotateY를 쓰면 yaw가 높이 축을 함께 섞어버려서
	// 이동 축마다 화면에 비대칭으로 투영되는(탑뷰가 삐뚤어지는) 문제가 생긴다.
	Mat4 view = Mat4::RotateX(kPitchRadians) * Mat4::RotateZ(kYawRadians) * Mat4::Translate(-m_FocusX, -m_FocusY, -m_FocusZ);
	Mat4 projection = Mat4::Ortho(-halfWidth, halfWidth, -halfHeight, halfHeight, -1000.f, 1000.f);

	return projection * view;
}

void Camera::SetFocus(float x, float y, float z)
{
	m_FocusX = x;
	m_FocusY = y;
	m_FocusZ = z;
}

void Camera::AdjustZoom(float factor)
{
	m_Zoom *= factor;

	if (m_Zoom < kMinZoom)
	{
		m_Zoom = kMinZoom;
	}
	else if (m_Zoom > kMaxZoom)
	{
		m_Zoom = kMaxZoom;
	}
}
