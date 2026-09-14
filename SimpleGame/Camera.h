#pragma once

#include "Math3D.h"

// Fixed quarter-view (dimetric) camera: the world is rotated into a
// 45-degree yaw / 30-degree pitch orientation and projected with an
// orthographic projection, producing the classic 2.5D isometric-style look.
class Camera
{
public:
	Camera(float viewWidth, float viewHeight, float zoom);

	Mat4 GetViewProjection() const;

private:
	float m_ViewWidth;
	float m_ViewHeight;
	float m_Zoom;
};
