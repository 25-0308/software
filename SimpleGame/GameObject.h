#pragma once

// World-space object for the prototype scene: a solid-colored cube-ish quad
// placed at (x, y, z) and drawn at uniform scale `size`.
struct GameObject
{
	float x = 0.f, y = 0.f, z = 0.f;
	float size = 1.f;
	float r = 1.f, g = 1.f, b = 1.f, a = 1.f;
};
