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

	// Multiplies the current zoom by `factor` (>1 zooms in, <1 zooms out),
	// clamped to a sane range. Zoom is centered on the world origin, which is
	// also the screen center today; once the camera follows the main
	// character, centering will naturally follow that character instead.
	void AdjustZoom(float factor);

private:
	float m_ViewWidth;
	float m_ViewHeight;
	float m_Zoom;

	static const float kMinZoom;
	static const float kMaxZoom;
};
