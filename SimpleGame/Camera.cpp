#include "stdafx.h"
#include "Camera.h"

namespace
{
	const float kYawRadians = 0.785398163f;   // 45 degrees
	const float kPitchRadians = 0.523598776f; // 30 degrees
}

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

	Mat4 view = Mat4::RotateX(kPitchRadians) * Mat4::RotateY(kYawRadians);
	Mat4 projection = Mat4::Ortho(-halfWidth, halfWidth, -halfHeight, halfHeight, -1000.f, 1000.f);

	return projection * view;
}
