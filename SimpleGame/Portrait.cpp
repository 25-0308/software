#include "stdafx.h"
#include "Portrait.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <vector>

// GDI+는 OLE 헤더(objidl.h)가 필요해서 이 파일에선 WIN32_LEAN_AND_MEAN을 쓰지 않는다. GDI+ 헤더 안의 min/max는
// windows.h 매크로를 쓰는데, NOMINMAX로 매크로가 없을 때도 컴파일되도록 std 판을 Gdiplus 이름공간에 보여 준다.
// (이 파일에선 매크로와 부딪히지 않게 std::min/max를 직접 부르지 않는다.)
#include <Windows.h>
#include <objidl.h>
namespace Gdiplus
{
	using std::min;
	using std::max;
}
#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")

#include "Dependencies\glew.h"

namespace
{
	const float kPi = 3.14159265f;

	// 텍스처 해상도: 화면에 그리는 크기의 두 배(창을 키워도 또렷하게). 그림 좌표는 흉상 기준 크기(168x208)로 쓰고 텍스처에 맞춰 늘려 그린다.
	const int kTexWidth = PortraitLibrary::kWidth * 2;
	const int kTexHeight = PortraitLibrary::kHeight * 2;

	// 그림 좌표계(모든 그리기 함수가 쓰는 흉상 기준 크기). 텍스처에 맞춰 늘려 그린다.
	const float kPaintWidth = 168.f;
	const float kPaintHeight = 208.f;

	// ------------------------------------------------------------------ 색

	struct Rgb
	{
		float r, g, b;
	};

	const Rgb kWhite = { 1.f, 1.f, 1.f };
	const Rgb kBlack = { 0.f, 0.f, 0.f };

	Rgb Mul(const Rgb& c, float k)
	{
		return { c.r * k, c.g * k, c.b * k };
	}

	Rgb Mix(const Rgb& a, const Rgb& b, float t)
	{
		return { a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t };
	}

	BYTE ToByte(float value)
	{
		float clamped = (value < 0.f) ? 0.f : ((value > 1.f) ? 1.f : value);
		return (BYTE)(clamped * 255.f + 0.5f);
	}

	Gdiplus::Color ToColor(const Rgb& c, float alpha)
	{
		return Gdiplus::Color(ToByte(alpha), ToByte(c.r), ToByte(c.g), ToByte(c.b));
	}

	// ------------------------------------------------------------------ 모양(SVG 경로처럼 M/L/C/Z + 타원)

	class Shape
	{
	public:
		Shape& M(float x, float y) { Add('M', x, y); return *this; }
		Shape& L(float x, float y) { Add('L', x, y); return *this; }
		Shape& C(float x1, float y1, float x2, float y2, float x, float y) { Add('C', x1, y1, x2, y2, x, y); return *this; }
		Shape& Z() { Add('Z'); return *this; }
		Shape& E(float cx, float cy, float rx, float ry) { Add('E', cx, cy, rx, ry); return *this; }

		void Build(Gdiplus::GraphicsPath& path) const
		{
			Gdiplus::PointF current(0.f, 0.f);
			for (const Command& command : m_Commands)
			{
				const float* v = command.v;
				switch (command.type)
				{
				case 'M':
					path.StartFigure();
					current = Gdiplus::PointF(v[0], v[1]);
					break;
				case 'L':
					path.AddLine(current, Gdiplus::PointF(v[0], v[1]));
					current = Gdiplus::PointF(v[0], v[1]);
					break;
				case 'C':
					path.AddBezier(current, Gdiplus::PointF(v[0], v[1]), Gdiplus::PointF(v[2], v[3]), Gdiplus::PointF(v[4], v[5]));
					current = Gdiplus::PointF(v[4], v[5]);
					break;
				case 'Z':
					path.CloseFigure();
					break;
				case 'E':
					path.StartFigure();
					path.AddEllipse(v[0] - v[2], v[1] - v[3], v[2] * 2.f, v[3] * 2.f);
					break;
				}
			}
		}

	private:
		struct Command
		{
			char type;
			float v[6];
		};

		void Add(char type, float a = 0.f, float b = 0.f, float c = 0.f, float d = 0.f, float e = 0.f, float f = 0.f)
		{
			Command command = { type, { a, b, c, d, e, f } };
			m_Commands.push_back(command);
		}

		std::vector<Command> m_Commands;
	};

	// 잎사귀 하나: (cx, cy) 중심, angle 방향(라디안)으로 len 길이, wid 폭.
	Shape Leaf(float cx, float cy, float angle, float len, float wid)
	{
		float dx = cosf(angle);
		float dy = sinf(angle);
		float nx = -dy;
		float ny = dx;
		float bx = cx - dx * len * 0.5f;
		float by = cy - dy * len * 0.5f;
		float tx = cx + dx * len * 0.5f;
		float ty = cy + dy * len * 0.5f;

		Shape leaf;
		leaf.M(bx, by)
			.C(bx + dx * len * 0.3f + nx * wid, by + dy * len * 0.3f + ny * wid, tx - dx * len * 0.3f + nx * wid, ty - dy * len * 0.3f + ny * wid, tx, ty)
			.C(tx - dx * len * 0.3f - nx * wid, ty - dy * len * 0.3f - ny * wid, bx + dx * len * 0.3f - nx * wid, by + dy * len * 0.3f - ny * wid, bx, by)
			.Z();
		return leaf;
	}

	Shape RectShape(float x, float y, float w, float h)
	{
		Shape rect;
		rect.M(x, y).L(x + w, y).L(x + w, y + h).L(x, y + h).Z();
		return rect;
	}

	// 얼굴 윤곽(모든 흉상이 같은 자리 — 눈은 y 96, 왼눈 x 72, 오른눈 x 96).
	Shape Face()
	{
		Shape face;
		face.M(56.f, 88.f).C(56.f, 64.f, 69.f, 57.f, 84.f, 57.f).C(99.f, 57.f, 112.f, 64.f, 112.f, 88.f)
			.C(112.f, 108.f, 104.f, 126.f, 84.f, 133.f).C(64.f, 126.f, 56.f, 108.f, 56.f, 88.f).Z();
		return face;
	}

	const float kEyeY = 96.f;
	const float kEyeL = 72.f;
	const float kEyeR = 96.f;

	// ------------------------------------------------------------------ 붓(GDI+ 감싸기)

	class Canvas
	{
	public:
		explicit Canvas(Gdiplus::Graphics& graphics) : m_G(graphics) {}

		void Fill(const Shape& shape, const Rgb& color, float alpha = 1.f)
		{
			Gdiplus::GraphicsPath path(Gdiplus::FillModeWinding);
			shape.Build(path);
			Gdiplus::SolidBrush brush(ToColor(color, alpha));
			m_G.FillPath(&brush, &path);
		}

		// (x0, y0)의 c0에서 (x1, y1)의 c1로 번지는 선형 그러데이션. 두 점 바깥은 끝 색 그대로(캔버스의 pad와 같게 —
		// GDI+ 붓은 바깥을 되풀이하므로 양쪽으로 두 배씩 늘린 붓에 끝 색 구간을 넣는다).
		void FillLinear(const Shape& shape, float x0, float y0, const Rgb& c0, float a0, float x1, float y1, const Rgb& c1, float a1)
		{
			Gdiplus::GraphicsPath path(Gdiplus::FillModeWinding);
			shape.Build(path);

			if (x0 == x1 && y0 == y1)
			{
				y1 += 0.01f;
			}

			const float kExtend = 2.f;
			float dx = x1 - x0;
			float dy = y1 - y0;
			Gdiplus::LinearGradientBrush brush(Gdiplus::PointF(x0 - dx * kExtend, y0 - dy * kExtend), Gdiplus::PointF(x1 + dx * kExtend, y1 + dy * kExtend),
				ToColor(c0, a0), ToColor(c1, a1));

			Gdiplus::Color colors[4] = { ToColor(c0, a0), ToColor(c0, a0), ToColor(c1, a1), ToColor(c1, a1) };
			Gdiplus::REAL positions[4] = { 0.f, kExtend / (1.f + 2.f * kExtend), (1.f + kExtend) / (1.f + 2.f * kExtend), 1.f };
			brush.SetInterpolationColors(colors, positions, 4);
			brush.SetWrapMode(Gdiplus::WrapModeTileFlipXY);
			m_G.FillPath(&brush, &path);
		}

		// 가운데 color(alpha)에서 가장자리 투명으로 번지는 타원.
		void Glow(float cx, float cy, float rx, float ry, const Rgb& color, float alpha)
		{
			Gdiplus::GraphicsPath path;
			path.AddEllipse(cx - rx, cy - ry, rx * 2.f, ry * 2.f);

			Gdiplus::PathGradientBrush brush(&path);
			brush.SetCenterPoint(Gdiplus::PointF(cx, cy));
			brush.SetCenterColor(ToColor(color, alpha));
			Gdiplus::Color surround = ToColor(color, 0.f);
			INT count = 1;
			brush.SetSurroundColors(&surround, &count);
			m_G.FillPath(&brush, &path);
		}

		void Stroke(const Shape& shape, const Rgb& color, float width, float alpha = 1.f)
		{
			Gdiplus::GraphicsPath path(Gdiplus::FillModeWinding);
			shape.Build(path);
			Gdiplus::Pen pen(ToColor(color, alpha), width);
			pen.SetLineCap(Gdiplus::LineCapRound, Gdiplus::LineCapRound, Gdiplus::DashCapRound);
			pen.SetLineJoin(Gdiplus::LineJoinRound);
			m_G.DrawPath(&pen, &path);
		}

		// shape 안쪽에만 그린다(얼굴 그늘·주름 등).
		template <typename F>
		void WithClip(const Shape& shape, F draw)
		{
			Gdiplus::GraphicsPath path(Gdiplus::FillModeWinding);
			shape.Build(path);
			Gdiplus::GraphicsState state = m_G.Save();
			m_G.SetClip(&path, Gdiplus::CombineModeIntersect);
			draw();
			m_G.Restore(state);
		}

		// (ox, oy)를 중심으로 scale배 하고 (dx, dy)만큼 옮겨서 그린다(세 자매·티탄처럼 흉상을 옮기고 키울 때).
		template <typename F>
		void WithTransform(float dx, float dy, float ox, float oy, float scale, F draw)
		{
			Gdiplus::GraphicsState state = m_G.Save();
			m_G.TranslateTransform(dx + ox, dy + oy);
			m_G.ScaleTransform(scale, scale);
			m_G.TranslateTransform(-ox, -oy);
			draw();
			m_G.Restore(state);
		}

	private:
		Gdiplus::Graphics& m_G;
	};

	// 결정적인 난수(인물 ID마다 같은 무늬 — 배경의 흩뿌린 무늬·돌 갈라짐).
	class Rng
	{
	public:
		explicit Rng(const std::string& text)
		{
			unsigned int hash = 2166136261u;
			for (char ch : text)
			{
				hash ^= (unsigned char)ch;
				hash *= 16777619u;
			}
			m_State = (hash != 0u) ? hash : 1u;
		}

		float Next()
		{
			m_State ^= m_State << 13;
			m_State ^= m_State >> 17;
			m_State ^= m_State << 5;
			return (float)(m_State & 0xFFFFu) / 65535.f;
		}

	private:
		unsigned int m_State;
	};

	// ------------------------------------------------------------------ 생김새 표

	enum class Motif { None, Lightning, Waves, Smoke, Petals, Wind, Peacock, Olive, Embers, Sun, Vines, Sparks, Leaves, Threads, Fire, Discord, Ash, Seeds, Rock, Sea, Drops, Road, Abyss, Crack };
	enum class HairStyle { None, Short, Long, Curly, Wild };
	enum class Beard { None, Short, Full, Long, Goatee };
	enum class EyeStyle { Normal, Tired, Narrow, Glow, Closed };
	enum class Expression { Neutral, Smile, Grin, Smirk, Frown, Stern, Angry, Sad, Tired };
	enum class Headgear { None, Crown, DarkCrown, Polos, Wreath, Helmet, HelmetGuards, Hood, Petasos, Horns };
	enum class WreathKind { Laurel, Rose, Ivy, Pomegranate, Gold };
	enum class Prop { None, Scepter, Trident, Caduceus, Hammer, Spear, Apple, Cup, Pomegranate, Scroll, Chains, Spindle, Rod, Shears, Staff, Ember };
	enum class Special { None, Cow, Trio, Titan, Voices };

	struct PortraitSpec
	{
		Rgb bgTop = { 0.3f, 0.3f, 0.35f };
		Rgb bgBottom = { 0.08f, 0.08f, 0.1f };
		Rgb glow = { 0.9f, 0.8f, 0.6f };
		Motif motif = Motif::None;

		Rgb skin = { 0.93f, 0.76f, 0.62f };
		Rgb hair = { 0.2f, 0.13f, 0.07f };
		HairStyle hairStyle = HairStyle::Short;
		Beard beard = Beard::None;
		Rgb beardColor = { 0.f, 0.f, 0.f };
		bool hasBeardColor = false; // 아니면 머리 색
		Rgb brow = { 0.f, 0.f, 0.f };
		bool hasBrow = false;       // 아니면 머리 색 × 0.8

		Rgb eyeColor = { 0.35f, 0.25f, 0.15f };
		EyeStyle eyeStyle = EyeStyle::Normal;
		Expression expr = Expression::Neutral;
		Rgb lip = { 0.6f, 0.3f, 0.28f };

		Rgb robe = { 0.8f, 0.78f, 0.72f };
		Rgb trim = { 0.85f, 0.68f, 0.3f };
		bool hasDrape = false;
		Rgb drape = { 0.f, 0.f, 0.f };
		bool bare = false; // 옷 없이 맨어깨(티탄)

		Headgear headgear = Headgear::None;
		WreathKind wreath = WreathKind::Laurel;
		Rgb hood = { 0.5f, 0.4f, 0.3f };
		Rgb veil = { 0.5f, 0.3f, 0.6f };

		Prop prop = Prop::None;
		float old = 0.f; // 주름(0~1)
		bool feminine = false;
		bool blush = false;
		bool soot = false;
		bool gaunt = false;
		bool stone = false;
		bool faded = false;
		bool pointedEars = false;
		bool coversForehead = false; // 이마가 가려져 이마 주름을 그리지 않음
		Special special = Special::None;

		Rgb BrowColor() const { return hasBrow ? brow : Mul(hair, 0.8f); }
		Rgb BeardColor() const { return hasBeardColor ? beardColor : hair; }
	};

	// 운명의 세 자매(혼자일 때와 세 자매 초상화에 같이 쓴다).
	PortraitSpec MoiraBase()
	{
		PortraitSpec p;
		p.feminine = true;
		p.headgear = Headgear::Hood;
		p.hood = { 0.88f, 0.86f, 0.82f };
		p.robe = { 0.88f, 0.86f, 0.8f };
		p.bgTop = { 0.16f, 0.13f, 0.24f };
		p.bgBottom = { 0.03f, 0.03f, 0.07f };
		p.glow = { 1.f, 0.85f, 0.45f };
		p.motif = Motif::Threads;
		p.coversForehead = true;
		return p;
	}

	PortraitSpec Clotho()
	{
		PortraitSpec p = MoiraBase();
		p.hair = { 0.16f, 0.1f, 0.06f };
		p.expr = Expression::Smile;
		p.prop = Prop::Spindle;
		p.eyeColor = { 0.3f, 0.3f, 0.45f };
		return p;
	}

	PortraitSpec Lachesis()
	{
		PortraitSpec p = MoiraBase();
		p.hair = { 0.4f, 0.3f, 0.2f };
		p.prop = Prop::Rod;
		p.old = 0.35f;
		p.eyeColor = { 0.35f, 0.3f, 0.25f };
		return p;
	}

	PortraitSpec Atropos()
	{
		PortraitSpec p = MoiraBase();
		p.hair = { 0.8f, 0.78f, 0.74f };
		p.expr = Expression::Stern;
		p.eyeStyle = EyeStyle::Narrow;
		p.prop = Prop::Shears;
		p.old = 0.9f;
		p.skin = { 0.88f, 0.74f, 0.64f };
		p.eyeColor = { 0.4f, 0.4f, 0.45f };
		return p;
	}

	PortraitSpec Pythia()
	{
		PortraitSpec p;
		p.hair = { 0.16f, 0.1f, 0.07f };
		p.hairStyle = HairStyle::Long;
		p.headgear = Headgear::Wreath;
		p.wreath = WreathKind::Laurel;
		p.robe = { 0.93f, 0.9f, 0.97f };
		p.trim = { 0.85f, 0.68f, 0.3f };
		p.feminine = true;
		p.expr = Expression::Smile;
		p.eyeColor = { 0.3f, 0.2f, 0.12f };
		p.blush = true;
		p.bgTop = { 0.44f, 0.36f, 0.26f };
		p.bgBottom = { 0.12f, 0.09f, 0.07f };
		p.glow = { 1.f, 0.85f, 0.6f };
		p.motif = Motif::Smoke;
		return p;
	}

	PortraitSpec Io()
	{
		PortraitSpec p;
		p.bgTop = { 0.5f, 0.6f, 0.72f };
		p.bgBottom = { 0.14f, 0.2f, 0.3f };
		p.glow = { 1.f, 0.95f, 0.85f };
		p.motif = Motif::Sea;
		p.special = Special::Cow;
		return p;
	}

	// 화자 ID의 생김새. 표에 없으면 false. 색은 게임 속 모습(WorldMaps.cpp의 *Look과 옷 색)을 따른다.
	bool FindSpec(const std::string& id, PortraitSpec& p)
	{
		p = PortraitSpec();

		if (id == "CHR_ZEUS")
		{
			p.bgTop = { 0.14f, 0.18f, 0.36f }; p.bgBottom = { 0.04f, 0.05f, 0.13f }; p.glow = { 1.f, 0.88f, 0.5f }; p.motif = Motif::Lightning;
			p.hair = { 0.95f, 0.95f, 0.95f }; p.hairStyle = HairStyle::Curly; p.beard = Beard::Full;
			p.brow = { 0.85f, 0.85f, 0.85f }; p.hasBrow = true;
			p.eyeColor = { 0.35f, 0.5f, 0.7f }; p.expr = Expression::Stern;
			p.robe = { 1.f, 0.98f, 0.92f }; p.trim = { 0.9f, 0.72f, 0.3f }; p.drape = { 0.86f, 0.66f, 0.26f }; p.hasDrape = true;
			p.headgear = Headgear::Crown; p.prop = Prop::Scepter; p.old = 0.45f;
			return true;
		}
		if (id == "CHR_POSEIDON")
		{
			p.bgTop = { 0.06f, 0.3f, 0.38f }; p.bgBottom = { 0.02f, 0.09f, 0.15f }; p.glow = { 0.5f, 0.95f, 0.95f }; p.motif = Motif::Waves;
			p.skin = { 0.84f, 0.64f, 0.5f }; p.hair = { 0.5f, 0.62f, 0.6f }; p.hairStyle = HairStyle::Wild; p.beard = Beard::Full;
			p.eyeColor = { 0.2f, 0.55f, 0.55f }; p.expr = Expression::Frown;
			p.robe = { 0.2f, 0.5f, 0.55f }; p.trim = { 0.55f, 0.9f, 0.85f };
			p.prop = Prop::Trident; p.old = 0.35f;
			return true;
		}
		if (id == "CHR_HADES")
		{
			p.bgTop = { 0.14f, 0.08f, 0.18f }; p.bgBottom = { 0.03f, 0.02f, 0.05f }; p.glow = { 0.6f, 0.4f, 0.9f }; p.motif = Motif::Smoke;
			p.skin = { 0.84f, 0.78f, 0.76f }; p.hair = { 0.08f, 0.07f, 0.08f }; p.beard = Beard::Short;
			p.eyeColor = { 0.45f, 0.3f, 0.6f }; p.eyeStyle = EyeStyle::Tired; p.expr = Expression::Tired;
			p.robe = { 0.16f, 0.12f, 0.2f }; p.trim = { 0.55f, 0.4f, 0.75f };
			p.headgear = Headgear::DarkCrown; p.prop = Prop::Scroll; p.old = 0.2f;
			return true;
		}
		if (id == "CHR_APHRODITE")
		{
			p.bgTop = { 0.6f, 0.26f, 0.36f }; p.bgBottom = { 0.2f, 0.06f, 0.12f }; p.glow = { 1.f, 0.75f, 0.75f }; p.motif = Motif::Petals;
			p.skin = { 0.96f, 0.82f, 0.74f }; p.hair = { 0.95f, 0.78f, 0.42f }; p.hairStyle = HairStyle::Long;
			p.feminine = true; p.blush = true; p.eyeColor = { 0.3f, 0.6f, 0.45f }; p.expr = Expression::Smirk; p.lip = { 0.85f, 0.3f, 0.38f };
			p.robe = { 0.95f, 0.6f, 0.7f }; p.trim = { 0.95f, 0.8f, 0.4f };
			p.headgear = Headgear::Wreath; p.wreath = WreathKind::Rose;
			return true;
		}
		if (id == "CHR_HERMES")
		{
			p.bgTop = { 0.32f, 0.52f, 0.78f }; p.bgBottom = { 0.1f, 0.18f, 0.34f }; p.glow = { 0.95f, 0.98f, 1.f }; p.motif = Motif::Wind;
			p.hair = { 0.45f, 0.3f, 0.16f }; p.hairStyle = HairStyle::Curly;
			p.eyeColor = { 0.45f, 0.3f, 0.15f }; p.expr = Expression::Grin;
			p.robe = { 0.55f, 0.72f, 0.92f }; p.trim = { 0.95f, 0.85f, 0.5f };
			p.headgear = Headgear::Petasos; p.prop = Prop::Caduceus;
			return true;
		}
		if (id == "CHR_HERA")
		{
			p.bgTop = { 0.32f, 0.15f, 0.38f }; p.bgBottom = { 0.09f, 0.04f, 0.12f }; p.glow = { 1.f, 0.85f, 0.5f }; p.motif = Motif::Peacock;
			p.hair = { 0.14f, 0.09f, 0.07f }; p.hairStyle = HairStyle::Long;
			p.feminine = true; p.eyeColor = { 0.3f, 0.2f, 0.35f }; p.expr = Expression::Stern; p.lip = { 0.7f, 0.25f, 0.3f };
			p.robe = { 0.52f, 0.28f, 0.62f }; p.trim = { 0.95f, 0.78f, 0.35f };
			p.headgear = Headgear::Polos; p.veil = { 0.62f, 0.42f, 0.72f };
			return true;
		}
		if (id == "CHR_ATHENA")
		{
			p.bgTop = { 0.26f, 0.32f, 0.38f }; p.bgBottom = { 0.07f, 0.09f, 0.12f }; p.glow = { 0.85f, 0.9f, 0.95f }; p.motif = Motif::Olive;
			p.hair = { 0.3f, 0.2f, 0.12f }; p.hairStyle = HairStyle::Long;
			p.feminine = true; p.eyeColor = { 0.45f, 0.55f, 0.65f };
			p.robe = { 0.55f, 0.6f, 0.7f }; p.trim = { 0.9f, 0.75f, 0.4f };
			p.headgear = Headgear::Helmet; p.prop = Prop::Spear; p.coversForehead = true;
			return true;
		}
		if (id == "CHR_ARES")
		{
			p.bgTop = { 0.42f, 0.08f, 0.06f }; p.bgBottom = { 0.1f, 0.02f, 0.02f }; p.glow = { 1.f, 0.5f, 0.25f }; p.motif = Motif::Embers;
			p.skin = { 0.86f, 0.64f, 0.5f }; p.hair = { 0.15f, 0.08f, 0.05f };
			p.eyeColor = { 0.4f, 0.2f, 0.1f }; p.expr = Expression::Angry;
			p.robe = { 0.6f, 0.15f, 0.12f }; p.trim = { 0.9f, 0.65f, 0.3f };
			p.headgear = Headgear::HelmetGuards; p.prop = Prop::Spear; p.coversForehead = true;
			return true;
		}
		if (id == "CHR_DIONYSUS")
		{
			p.bgTop = { 0.36f, 0.1f, 0.3f }; p.bgBottom = { 0.1f, 0.03f, 0.1f }; p.glow = { 0.95f, 0.6f, 0.85f }; p.motif = Motif::Vines;
			p.hair = { 0.2f, 0.12f, 0.18f }; p.hairStyle = HairStyle::Long;
			p.eyeColor = { 0.4f, 0.2f, 0.35f }; p.eyeStyle = EyeStyle::Tired; p.expr = Expression::Smile; p.blush = true;
			p.robe = { 0.5f, 0.2f, 0.45f }; p.trim = { 0.95f, 0.8f, 0.4f };
			p.headgear = Headgear::Wreath; p.wreath = WreathKind::Ivy; p.prop = Prop::Cup;
			return true;
		}
		if (id == "CHR_HEPHAESTUS")
		{
			p.bgTop = { 0.38f, 0.16f, 0.06f }; p.bgBottom = { 0.08f, 0.04f, 0.02f }; p.glow = { 1.f, 0.55f, 0.2f }; p.motif = Motif::Sparks;
			p.skin = { 0.76f, 0.56f, 0.44f }; p.hair = { 0.22f, 0.14f, 0.08f }; p.hairStyle = HairStyle::Wild; p.beard = Beard::Full;
			p.eyeColor = { 0.3f, 0.2f, 0.12f }; p.expr = Expression::Frown;
			p.robe = { 0.42f, 0.29f, 0.18f }; p.trim = { 0.3f, 0.2f, 0.12f };
			p.soot = true; p.prop = Prop::Hammer; p.old = 0.3f;
			return true;
		}
		if (id == "CHR_PAN")
		{
			p.bgTop = { 0.2f, 0.32f, 0.14f }; p.bgBottom = { 0.05f, 0.1f, 0.04f }; p.glow = { 0.85f, 0.95f, 0.6f }; p.motif = Motif::Leaves;
			p.skin = { 0.84f, 0.64f, 0.48f }; p.hair = { 0.4f, 0.28f, 0.16f }; p.hairStyle = HairStyle::Wild;
			p.beard = Beard::Goatee; p.beardColor = { 0.42f, 0.3f, 0.18f }; p.hasBeardColor = true;
			p.eyeColor = { 0.55f, 0.45f, 0.15f }; p.expr = Expression::Grin;
			p.robe = { 0.45f, 0.32f, 0.2f }; p.trim = { 0.3f, 0.22f, 0.12f };
			p.headgear = Headgear::Horns; p.pointedEars = true; p.old = 0.25f;
			return true;
		}
		if (id == "CHR_PYTHIA")
		{
			p = Pythia();
			return true;
		}
		if (id == "CHR_PYTHIA_VOICES")
		{
			// 세 겹의 목소리(봉인된 모이라이의 목소리에 잠긴 피티아): 보랏빛으로 빛나는 눈과 어긋난 얼굴 둘.
			p = Pythia();
			p.eyeStyle = EyeStyle::Glow; p.eyeColor = { 0.75f, 0.45f, 1.f }; p.expr = Expression::Neutral; p.blush = false;
			p.bgTop = { 0.2f, 0.1f, 0.3f }; p.bgBottom = { 0.04f, 0.02f, 0.08f }; p.glow = { 0.7f, 0.4f, 1.f }; p.motif = Motif::Crack;
			p.special = Special::Voices;
			return true;
		}
		if (id == "CHR_ERIS")
		{
			// 떠돌이 점술가 노파(= 에리스의 위장): 두건·주름·금빛 사과(복선 F-02).
			p.bgTop = { 0.22f, 0.13f, 0.08f }; p.bgBottom = { 0.05f, 0.03f, 0.02f }; p.glow = { 1.f, 0.6f, 0.3f }; p.motif = Motif::Fire;
			p.skin = { 0.86f, 0.7f, 0.6f }; p.hair = { 0.72f, 0.7f, 0.66f };
			p.headgear = Headgear::Hood; p.hood = { 0.34f, 0.2f, 0.17f };
			p.robe = { 0.3f, 0.24f, 0.22f }; p.trim = { 0.4f, 0.3f, 0.25f };
			p.feminine = true; p.eyeColor = { 0.75f, 0.55f, 0.15f }; p.eyeStyle = EyeStyle::Narrow; p.expr = Expression::Smirk;
			p.prop = Prop::Apple; p.old = 1.f; p.coversForehead = true;
			return true;
		}
		if (id == "CHR_ERIS_TRUE")
		{
			p.bgTop = { 0.32f, 0.04f, 0.07f }; p.bgBottom = { 0.05f, 0.f, 0.02f }; p.glow = { 1.f, 0.75f, 0.35f }; p.motif = Motif::Discord;
			p.skin = { 0.92f, 0.8f, 0.74f }; p.hair = { 0.38f, 0.06f, 0.08f }; p.hairStyle = HairStyle::Long;
			p.feminine = true; p.eyeColor = { 0.95f, 0.7f, 0.2f }; p.eyeStyle = EyeStyle::Narrow; p.expr = Expression::Smirk; p.lip = { 0.6f, 0.12f, 0.16f };
			p.robe = { 0.12f, 0.06f, 0.08f }; p.trim = { 0.9f, 0.7f, 0.3f };
			p.headgear = Headgear::Wreath; p.wreath = WreathKind::Gold; p.prop = Prop::Apple;
			return true;
		}
		if (id == "CHR_HEARTH_GOD")
		{
			// 이름을 잊은 옛 화로의 신: 잿빛으로 바랜 그림.
			p.bgTop = { 0.24f, 0.22f, 0.22f }; p.bgBottom = { 0.08f, 0.07f, 0.07f }; p.glow = { 0.6f, 0.58f, 0.55f }; p.motif = Motif::Ash;
			p.skin = { 0.82f, 0.74f, 0.68f }; p.hair = { 0.6f, 0.58f, 0.55f }; p.beard = Beard::Full;
			p.eyeColor = { 0.4f, 0.38f, 0.36f }; p.eyeStyle = EyeStyle::Tired; p.expr = Expression::Sad;
			p.robe = { 0.62f, 0.45f, 0.32f }; p.trim = { 0.5f, 0.4f, 0.3f };
			p.old = 0.7f; p.faded = true;
			return true;
		}
		if (id == "CHR_HEARTH_GOD_NAMED")
		{
			// 이름을 되찾은 에스카로스: 두 손의 불씨.
			p.bgTop = { 0.42f, 0.2f, 0.08f }; p.bgBottom = { 0.1f, 0.04f, 0.02f }; p.glow = { 1.f, 0.65f, 0.3f }; p.motif = Motif::Embers;
			p.skin = { 0.9f, 0.72f, 0.58f }; p.hair = { 0.6f, 0.58f, 0.55f }; p.beard = Beard::Full;
			p.eyeColor = { 0.55f, 0.35f, 0.15f }; p.expr = Expression::Smile;
			p.robe = { 0.62f, 0.45f, 0.32f }; p.trim = { 0.95f, 0.65f, 0.3f };
			p.prop = Prop::Ember; p.old = 0.6f;
			return true;
		}
		if (id == "CHR_APOLLO")
		{
			// 20년째 실직한 신탁의 주인: 풀린 눈과 술잔.
			p.bgTop = { 0.52f, 0.36f, 0.16f }; p.bgBottom = { 0.15f, 0.09f, 0.05f }; p.glow = { 1.f, 0.85f, 0.45f }; p.motif = Motif::Sun;
			p.hair = { 0.95f, 0.8f, 0.38f }; p.hairStyle = HairStyle::Curly;
			p.eyeColor = { 0.3f, 0.5f, 0.7f }; p.eyeStyle = EyeStyle::Tired; p.expr = Expression::Sad; p.blush = true;
			p.robe = { 0.95f, 0.9f, 0.8f }; p.trim = { 0.95f, 0.78f, 0.35f };
			p.headgear = Headgear::Wreath; p.wreath = WreathKind::Laurel; p.prop = Prop::Cup;
			return true;
		}
		if (id == "CHR_PERSEPHONE")
		{
			p.bgTop = { 0.22f, 0.08f, 0.15f }; p.bgBottom = { 0.05f, 0.02f, 0.05f }; p.glow = { 0.95f, 0.4f, 0.5f }; p.motif = Motif::Seeds;
			p.skin = { 0.94f, 0.82f, 0.8f }; p.hair = { 0.1f, 0.06f, 0.08f }; p.hairStyle = HairStyle::Long;
			p.feminine = true; p.eyeColor = { 0.35f, 0.15f, 0.25f }; p.lip = { 0.7f, 0.18f, 0.25f };
			p.robe = { 0.42f, 0.12f, 0.2f }; p.trim = { 0.85f, 0.55f, 0.4f };
			p.headgear = Headgear::Wreath; p.wreath = WreathKind::Pomegranate; p.prop = Prop::Pomegranate;
			return true;
		}
		if (id == "CHR_PROMETHEUS")
		{
			// 사슬에 묶인 티탄: 맨어깨와 사슬, 뒤로 바위와 불빛.
			p.bgTop = { 0.28f, 0.22f, 0.2f }; p.bgBottom = { 0.08f, 0.06f, 0.05f }; p.glow = { 1.f, 0.5f, 0.2f }; p.motif = Motif::Rock;
			p.skin = { 0.8f, 0.6f, 0.48f }; p.hair = { 0.42f, 0.38f, 0.34f }; p.hairStyle = HairStyle::Wild;
			p.beard = Beard::Full; p.beardColor = { 0.46f, 0.42f, 0.38f }; p.hasBeardColor = true;
			p.eyeColor = { 0.6f, 0.35f, 0.15f }; p.expr = Expression::Stern;
			p.bare = true; p.prop = Prop::Chains; p.old = 0.6f;
			return true;
		}
		if (id == "CHR_MOIRAI")
		{
			p = MoiraBase();
			p.special = Special::Trio;
			return true;
		}
		if (id == "CHR_CLOTHO")
		{
			p = Clotho();
			return true;
		}
		if (id == "CHR_LACHESIS")
		{
			p = Lachesis();
			return true;
		}
		if (id == "CHR_ATROPOS")
		{
			p = Atropos();
			return true;
		}
		if (id == "CHR_IO" || id == "CHR_IO_NAMED")
		{
			// 헤라의 저주로 암소가 된 이오 — 이름을 되찾은 뒤에도 같은 모습(유머 없음, 눈물 한 방울).
			p = Io();
			return true;
		}
		if (id == "CHR_TANTALUS")
		{
			// 닿지 않는 물과 열매: 여윈 볼, 머리 위의 열매 가지.
			p.bgTop = { 0.14f, 0.22f, 0.26f }; p.bgBottom = { 0.03f, 0.06f, 0.08f }; p.glow = { 0.6f, 0.85f, 0.95f }; p.motif = Motif::Drops;
			p.skin = { 0.82f, 0.68f, 0.58f }; p.hair = { 0.5f, 0.46f, 0.42f }; p.hairStyle = HairStyle::Wild;
			p.beard = Beard::Long; p.beardColor = { 0.52f, 0.48f, 0.44f }; p.hasBeardColor = true;
			p.eyeColor = { 0.3f, 0.35f, 0.4f }; p.eyeStyle = EyeStyle::Tired; p.expr = Expression::Sad;
			p.robe = { 0.5f, 0.42f, 0.36f }; p.trim = { 0.4f, 0.34f, 0.3f };
			p.gaunt = true; p.old = 0.8f;
			return true;
		}
		if (id == "CHR_TERMINUS")
		{
			p.bgTop = { 0.46f, 0.4f, 0.32f }; p.bgBottom = { 0.14f, 0.12f, 0.09f }; p.glow = { 0.95f, 0.9f, 0.75f }; p.motif = Motif::Road;
			p.skin = { 0.86f, 0.72f, 0.6f }; p.hair = { 0.55f, 0.54f, 0.5f };
			p.beard = Beard::Full; p.beardColor = { 0.58f, 0.57f, 0.53f }; p.hasBeardColor = true;
			p.eyeColor = { 0.4f, 0.4f, 0.35f }; p.expr = Expression::Sad;
			p.robe = { 0.58f, 0.56f, 0.52f }; p.trim = { 0.75f, 0.6f, 0.35f };
			p.prop = Prop::Staff; p.old = 0.6f;
			return true;
		}
		if (id == "CHR_KRONOS_ECHO")
		{
			// 크로노스의 잔향: 갈라진 돌빛 거인, 보랏빛 눈.
			p.bgTop = { 0.16f, 0.06f, 0.22f }; p.bgBottom = { 0.02f, 0.f, 0.04f }; p.glow = { 0.7f, 0.35f, 1.f }; p.motif = Motif::Abyss;
			p.skin = { 0.52f, 0.5f, 0.58f }; p.hair = { 0.3f, 0.28f, 0.34f }; p.hairStyle = HairStyle::Wild;
			p.beard = Beard::Long; p.beardColor = { 0.32f, 0.3f, 0.36f }; p.hasBeardColor = true;
			p.eyeColor = { 0.8f, 0.5f, 1.f }; p.eyeStyle = EyeStyle::Glow; p.expr = Expression::Angry;
			p.robe = { 0.2f, 0.16f, 0.24f }; p.trim = { 0.5f, 0.35f, 0.65f };
			p.headgear = Headgear::DarkCrown; p.stone = true; p.special = Special::Titan; p.old = 0.8f;
			return true;
		}

		return false;
	}

	// ------------------------------------------------------------------ 그리기

	void DrawMotif(Canvas& c, const PortraitSpec& p, Rng& rng)
	{
		const Rgb& g = p.glow;

		switch (p.motif)
		{
		case Motif::None:
			break;

		case Motif::Lightning:
			{
				const float bolts[3][3] = { { 22.f, 20.f, 1.f }, { 146.f, 34.f, 1.f }, { 30.f, 120.f, 0.7f } };
				for (const auto& bolt : bolts)
				{
					float x = bolt[0], y = bolt[1], s = bolt[2];
					c.Stroke(Shape().M(x, y).L(x - 6.f * s, y + 18.f * s).L(x + 2.f * s, y + 18.f * s).L(x - 5.f * s, y + 38.f * s), Mix(g, kWhite, 0.4f), 2.2f, 0.35f);
				}
			}
			break;

		case Motif::Waves:
			for (int i = 0; i < 5; ++i)
			{
				float y = 150.f + (float)i * 13.f;
				Shape wave;
				wave.M(-4.f, y);
				for (float x = -4.f; x < 172.f; x += 24.f)
				{
					wave.C(x + 6.f, y - 6.f, x + 18.f, y - 6.f, x + 24.f, y);
				}
				c.Stroke(wave, g, 1.6f, 0.25f - (float)i * 0.03f);
			}
			break;

		case Motif::Smoke:
			for (int i = 0; i < 7; ++i)
			{
				float x = rng.Next() * 168.f;
				float y = 20.f + rng.Next() * 170.f;
				float rx = 20.f + rng.Next() * 20.f;
				float ry = 14.f + rng.Next() * 14.f;
				c.Glow(x, y, rx, ry, g, 0.18f);
			}
			break;

		case Motif::Petals:
			for (int i = 0; i < 12; ++i)
			{
				float x = rng.Next() * 168.f;
				float y = rng.Next() * 190.f;
				float a = rng.Next() * 6.28f;
				c.Fill(Leaf(x, y, a, 9.f, 3.5f), Mix(g, kWhite, 0.3f), 0.35f);
			}
			break;

		case Motif::Wind:
			for (int i = 0; i < 6; ++i)
			{
				float y = 24.f + (float)i * 30.f + rng.Next() * 10.f;
				float x = rng.Next() * 60.f - 20.f;
				c.Stroke(Shape().M(x, y).C(x + 30.f, y - 8.f, x + 70.f, y + 6.f, x + 110.f, y - 4.f), kWhite, 1.4f, 0.18f);
			}
			break;

		case Motif::Peacock:
			for (int i = 0; i < 7; ++i)
			{
				float side = rng.Next();
				float x = (side < 0.5f) ? rng.Next() * 40.f : 128.f + rng.Next() * 40.f;
				float y = 20.f + rng.Next() * 170.f;
				c.Glow(x, y, 10.f, 13.f, { 0.2f, 0.6f, 0.55f }, 0.35f);
				c.Fill(Shape().E(x, y, 3.5f, 4.5f), { 0.15f, 0.25f, 0.65f }, 0.45f);
				c.Fill(Shape().E(x, y, 1.6f, 2.f), { 0.95f, 0.8f, 0.3f }, 0.5f);
			}
			break;

		case Motif::Olive:
			{
				const float sprigs[4][3] = { { 18.f, 40.f, -0.5f }, { 150.f, 60.f, 3.6f }, { 20.f, 150.f, -0.9f }, { 152.f, 150.f, 4.0f } };
				for (const auto& sprig : sprigs)
				{
					float x = sprig[0], y = sprig[1], a = sprig[2];
					c.Stroke(Shape().M(x, y).L(x + cosf(a) * 30.f, y + sinf(a) * 30.f), { 0.45f, 0.5f, 0.3f }, 1.2f, 0.3f);
					for (int k = 0; k < 4; ++k)
					{
						float t = 0.2f + (float)k * 0.22f;
						c.Fill(Leaf(x + cosf(a) * 30.f * t, y + sinf(a) * 30.f * t, a + ((k % 2) ? 0.7f : -0.7f), 10.f, 2.4f), { 0.55f, 0.62f, 0.38f }, 0.3f);
					}
				}
			}
			break;

		case Motif::Embers:
			for (int i = 0; i < 18; ++i)
			{
				float x = rng.Next() * 168.f;
				float y = 40.f + rng.Next() * 168.f;
				c.Glow(x, y, 3.5f, 3.5f, { 1.f, 0.55f, 0.2f }, 0.6f);
			}
			break;

		case Motif::Sun:
			for (int i = 0; i < 9; ++i)
			{
				float a = kPi * (0.15f + 0.7f * (float)i / 8.f);
				float x = 84.f - cosf(a) * 240.f;
				float y = -20.f + sinf(a) * 240.f;
				float nx = sinf(a) * 9.f;
				float ny = cosf(a) * 9.f;
				c.Fill(Shape().M(84.f, -20.f).L(x - nx, y + ny).L(x + nx, y - ny).Z(), g, 0.08f);
			}
			break;

		case Motif::Vines:
			for (int side = 0; side < 2; ++side)
			{
				float x = side ? 160.f : 8.f;
				c.Stroke(Shape().M(x, -4.f).C(x + (side ? -16.f : 16.f), 50.f, x + (side ? 10.f : -10.f), 110.f, x, 212.f), { 0.3f, 0.45f, 0.22f }, 2.f, 0.4f);
				for (int k = 0; k < 6; ++k)
				{
					float y = 14.f + (float)k * 34.f;
					c.Fill(Leaf(x + (side ? -6.f : 6.f), y, side ? 3.6f : -0.5f, 13.f, 5.f), { 0.32f, 0.52f, 0.24f }, 0.4f);
					if (k % 2)
					{
						for (int q = 0; q < 5; ++q)
						{
							c.Fill(Shape().E(x + (side ? -10.f : 10.f) + (float)(q % 2) * 3.f, y + 10.f + (float)q * 2.6f, 2.4f, 2.4f), { 0.45f, 0.15f, 0.4f }, 0.5f);
						}
					}
				}
			}
			break;

		case Motif::Sparks:
			for (int i = 0; i < 16; ++i)
			{
				float x = rng.Next() * 168.f;
				float y = 60.f + rng.Next() * 148.f;
				float a = -1.2f - rng.Next() * 0.7f;
				c.Stroke(Shape().M(x, y).L(x + cosf(a) * 7.f, y + sinf(a) * 7.f), { 1.f, 0.7f, 0.3f }, 1.2f, 0.6f);
			}
			break;

		case Motif::Leaves:
			for (int i = 0; i < 14; ++i)
			{
				float x = rng.Next() * 168.f;
				float y = rng.Next() * 200.f;
				float a = rng.Next() * 6.28f;
				c.Fill(Leaf(x, y, a, 12.f, 4.5f), Mix(g, { 0.3f, 0.5f, 0.2f }, 0.5f), 0.3f);
			}
			break;

		case Motif::Threads:
			for (int i = 0; i < 6; ++i)
			{
				float y = 20.f + (float)i * 34.f;
				float c1 = y - 20.f + rng.Next() * 40.f;
				float c2 = y - 20.f + rng.Next() * 40.f;
				c.Stroke(Shape().M(-4.f, y).C(50.f, c1, 110.f, c2, 172.f, y + 10.f), { 1.f, 0.85f, 0.4f }, 0.9f, 0.35f);
			}
			break;

		case Motif::Fire:
			c.Glow(84.f, 230.f, 110.f, 80.f, { 1.f, 0.5f, 0.15f }, 0.45f);
			for (int i = 0; i < 10; ++i)
			{
				float x = rng.Next() * 168.f;
				float y = 120.f + rng.Next() * 90.f;
				c.Glow(x, y, 2.5f, 2.5f, { 1.f, 0.6f, 0.2f }, 0.7f);
			}
			break;

		case Motif::Discord:
			for (int i = 0; i < 5; ++i)
			{
				float x = rng.Next() * 168.f;
				float y = rng.Next() * 60.f;
				Shape crack;
				crack.M(x, y);
				for (int k = 0; k < 5; ++k)
				{
					x += rng.Next() * 16.f - 8.f;
					y += 8.f + rng.Next() * 10.f;
					crack.L(x, y);
				}
				c.Stroke(crack, { 0.9f, 0.2f, 0.2f }, 1.1f, 0.35f);
			}
			c.Glow(84.f, 200.f, 60.f, 30.f, { 1.f, 0.8f, 0.3f }, 0.18f);
			break;

		case Motif::Ash:
			for (int i = 0; i < 30; ++i)
			{
				float x = rng.Next() * 168.f;
				float y = rng.Next() * 208.f;
				float rx = 1.f + rng.Next();
				float ry = 1.f + rng.Next();
				c.Fill(Shape().E(x, y, rx, ry), { 0.7f, 0.7f, 0.7f }, 0.25f);
			}
			break;

		case Motif::Seeds:
			for (int i = 0; i < 16; ++i)
			{
				float x = rng.Next() * 168.f;
				float y = rng.Next() * 208.f;
				c.Fill(Shape().E(x, y, 1.8f, 2.3f), { 0.85f, 0.15f, 0.22f }, 0.45f);
			}
			break;

		case Motif::Rock:
			c.Fill(Shape().M(-4.f, 140.f).L(30.f, 120.f).L(46.f, 150.f).L(40.f, 212.f).L(-4.f, 212.f).Z(), { 0.15f, 0.12f, 0.12f }, 0.8f);
			c.Fill(Shape().M(172.f, 130.f).L(140.f, 118.f).L(126.f, 160.f).L(136.f, 212.f).L(172.f, 212.f).Z(), { 0.15f, 0.12f, 0.12f }, 0.8f);
			c.Glow(84.f, 220.f, 100.f, 60.f, { 1.f, 0.45f, 0.15f }, 0.35f);
			break;

		case Motif::Sea:
			c.Fill(RectShape(0.f, 120.f, 168.f, 88.f), { 0.2f, 0.36f, 0.5f }, 0.5f);
			for (int i = 0; i < 4; ++i)
			{
				float y = (float)i * 18.f;
				c.Stroke(Shape().M(0.f, 132.f + y).C(40.f, 128.f + y, 120.f, 138.f + y, 168.f, 130.f + y), kWhite, 1.f, 0.2f);
			}
			break;

		case Motif::Drops:
			{
				// 닿지 않는 열매 가지(왼쪽 위)와 물방울.
				c.Stroke(Shape().M(-4.f, 14.f).C(30.f, 8.f, 52.f, 18.f, 70.f, 12.f), { 0.35f, 0.25f, 0.15f }, 2.2f, 0.8f);
				const float fruits[3][2] = { { 18.f, 22.f }, { 40.f, 24.f }, { 60.f, 20.f } };
				for (const auto& fruit : fruits)
				{
					float x = fruit[0], y = fruit[1];
					c.Fill(Leaf(x + 4.f, y - 6.f, -0.6f, 10.f, 3.f), { 0.35f, 0.55f, 0.3f }, 0.8f);
					c.Fill(Shape().E(x, y + 6.f, 4.5f, 5.f), { 0.8f, 0.3f, 0.2f }, 0.85f);
				}
				for (int i = 0; i < 8; ++i)
				{
					float x = rng.Next() * 168.f;
					float y = 60.f + rng.Next() * 140.f;
					c.Fill(Shape().M(x, y - 5.f).C(x + 3.f, y, x + 3.f, y + 3.f, x, y + 3.f).C(x - 3.f, y + 3.f, x - 3.f, y, x, y - 5.f).Z(), { 0.6f, 0.8f, 0.95f }, 0.35f);
				}
			}
			break;

		case Motif::Road:
			c.Fill(Shape().M(40.f, 208.f).L(76.f, 150.f).L(92.f, 150.f).L(128.f, 208.f).Z(), { 0.55f, 0.48f, 0.38f }, 0.35f);
			for (int i = 0; i < 6; ++i)
			{
				float x = 10.f + rng.Next() * 148.f;
				float y = 170.f + rng.Next() * 36.f;
				c.Fill(Shape().E(x, y, 5.f, 3.f), { 0.6f, 0.58f, 0.54f }, 0.35f);
			}
			break;

		case Motif::Abyss:
			for (int i = 0; i < 4; ++i)
			{
				c.Stroke(Shape().E(84.f, 150.f, 30.f + (float)i * 22.f, 12.f + (float)i * 9.f), g, 1.4f, 0.25f);
			}
			c.Glow(84.f, 160.f, 70.f, 30.f, kBlack, 0.5f);
			break;

		case Motif::Crack:
			{
				float x = 10.f;
				float y = 30.f;
				Shape crack;
				crack.M(x, y);
				for (int k = 0; k < 9; ++k)
				{
					x += 17.f;
					y += (k % 2) ? -12.f : 14.f;
					crack.L(x, y);
				}
				c.Glow(84.f, 34.f, 90.f, 22.f, g, 0.3f);
				c.Stroke(crack, Mix(g, kWhite, 0.5f), 1.6f, 0.7f);
			}
			break;
		}
	}

	void DrawBackground(Canvas& c, const PortraitSpec& p, Rng& rng)
	{
		c.FillLinear(RectShape(0.f, 0.f, 168.f, 208.f), 0.f, 0.f, p.bgTop, 1.f, 0.f, 208.f, p.bgBottom, 1.f);
		c.Glow(20.f, 8.f, 96.f, 76.f, kWhite, 0.07f); // 왼쪽 위에서 드는 빛
		c.Glow(84.f, 96.f, 84.f, 96.f, p.glow, 0.5f);

		// 흐릿한 빛망울(황금각으로 흩어 늘 같은 자리)
		for (int i = 0; i < 7; ++i)
		{
			float a = (float)i * 2.39996f;
			float r = 44.f + (float)(i % 3) * 20.f;
			float size = 8.f + (float)(i % 3) * 5.f;
			c.Glow(84.f + cosf(a) * r * 1.1f, 92.f + sinf(a) * r, size, size, Mix(p.glow, kWhite, 0.3f), 0.13f);
		}

		DrawMotif(c, p, rng);
	}

	// 네 가장자리를 살짝 어둡게(액자 안쪽 그늘).
	void DrawVignette(Canvas& c)
	{
		c.FillLinear(RectShape(0.f, 0.f, 22.f, 208.f), 0.f, 0.f, kBlack, 0.45f, 22.f, 0.f, kBlack, 0.f);
		c.FillLinear(RectShape(146.f, 0.f, 22.f, 208.f), 168.f, 0.f, kBlack, 0.45f, 146.f, 0.f, kBlack, 0.f);
		c.FillLinear(RectShape(0.f, 0.f, 168.f, 18.f), 0.f, 0.f, kBlack, 0.45f, 0.f, 18.f, kBlack, 0.f);
		c.FillLinear(RectShape(0.f, 190.f, 168.f, 18.f), 0.f, 208.f, kBlack, 0.45f, 0.f, 190.f, kBlack, 0.f);
	}

	// 안쪽 금선 두 줄과 네 귀퉁이의 마름모(액자).
	void DrawFrame(Canvas& c)
	{
		const Rgb gold = { 0.92f, 0.76f, 0.42f };
		c.Stroke(Shape().M(3.5f, 3.5f).L(164.5f, 3.5f).L(164.5f, 204.5f).L(3.5f, 204.5f).Z(), gold, 0.9f, 0.55f);
		c.Stroke(Shape().M(6.5f, 6.5f).L(161.5f, 6.5f).L(161.5f, 201.5f).L(6.5f, 201.5f).Z(), gold, 0.5f, 0.28f);

		const float corners[4][2] = { { 3.5f, 3.5f }, { 164.5f, 3.5f }, { 164.5f, 204.5f }, { 3.5f, 204.5f } };
		for (const auto& corner : corners)
		{
			float x = corner[0], y = corner[1];
			c.Fill(Shape().M(x, y - 3.4f).L(x + 3.4f, y).L(x, y + 3.4f).L(x - 3.4f, y).Z(), gold, 0.85f);
		}
	}

	void DrawEyes(Canvas& c, const PortraitSpec& p)
	{
		const Rgb lidColor = { 0.14f, 0.09f, 0.07f };
		const float centers[2] = { kEyeL, kEyeR };

		for (float cx : centers)
		{
			float y = kEyeY;
			float out = (cx < 84.f) ? -1.f : 1.f;

			if (p.eyeStyle == EyeStyle::Closed)
			{
				c.Stroke(Shape().M(cx - 6.f, y).C(cx - 3.f, y + 2.5f, cx + 3.f, y + 2.5f, cx + 6.f, y), lidColor, 1.4f);
				continue;
			}

			float ry = (p.eyeStyle == EyeStyle::Narrow) ? 2.3f : 3.4f;
			Shape white = Shape().E(cx, y, 6.2f, ry);
			if (p.eyeStyle == EyeStyle::Glow)
			{
				c.Glow(cx, y, 13.f, 9.f, p.eyeColor, 0.85f);
				c.Fill(white, Mix(p.eyeColor, kWhite, 0.6f));
				c.Glow(cx, y, 4.f, 2.4f, kWhite, 0.8f);
			}
			else
			{
				// 흰자(윗꺼풀 그늘로 위가 어둡다) 안에 홍채(가장자리가 진하고 아래가 밝다)·동공·눈빛 두 점.
				c.FillLinear(white, 0.f, y - ry, { 0.78f, 0.75f, 0.73f }, 1.f, 0.f, y + ry, { 0.97f, 0.96f, 0.93f }, 1.f);
				c.WithClip(white, [&]()
				{
					float ix = cx + 0.3f;
					float iy = y - 0.3f;
					c.Fill(Shape().E(ix, iy, 3.2f, 3.2f), Mul(p.eyeColor, 0.5f));
					c.Fill(Shape().E(ix, iy, 2.6f, 2.6f), p.eyeColor);
					c.Glow(ix, iy + 1.f, 2.6f, 2.2f, Mix(p.eyeColor, kWhite, 0.45f), 0.7f);
					c.Fill(Shape().E(ix, iy, 1.3f, 1.3f), { 0.04f, 0.03f, 0.03f });
					c.Glow(cx, y - ry, 7.f, 2.4f, kBlack, 0.35f);
					c.Fill(Shape().E(ix - 1.1f, iy - 1.2f, 0.85f, 0.85f), kWhite, 0.95f);
					c.Fill(Shape().E(ix + 1.2f, iy + 1.f, 0.45f, 0.45f), kWhite, 0.6f);
				});
			}

			// 쌍꺼풀 주름
			c.Stroke(Shape().M(cx - 5.6f, y - 3.4f).C(cx - 2.5f, y - 6.2f, cx + 3.f, y - 6.2f, cx + 6.2f, y - 3.f), Mul(p.skin, 0.6f), 0.8f, 0.55f);

			if (p.eyeStyle == EyeStyle::Tired)
			{
				// 반쯤 감긴 눈꺼풀.
				c.Fill(Shape().M(cx - 6.8f, y + 0.2f).C(cx - 4.f, y - 4.2f, cx + 4.f, y - 4.2f, cx + 6.8f, y + 0.2f)
					.C(cx + 4.f, y - 0.6f, cx - 4.f, y - 0.6f, cx - 6.8f, y + 0.2f).Z(), Mul(p.skin, 0.88f));
				c.Stroke(Shape().M(cx - 6.8f, y + 0.2f).C(cx - 4.f, y - 0.6f, cx + 4.f, y - 0.6f, cx + 6.8f, y + 0.2f), lidColor, 1.5f);
			}
			else
			{
				c.Stroke(Shape().M(cx - 6.8f, y + 0.3f).C(cx - 4.f, y - 4.f, cx + 4.f, y - 4.f, cx + 6.8f, y - 0.4f), lidColor, 1.7f);
			}

			// 아래꺼풀과 바깥 눈꼬리 속눈썹
			c.Stroke(Shape().M(cx - 5.f, y + ry - 0.6f).C(cx - 2.f, y + ry + 0.8f, cx + 2.f, y + ry + 0.8f, cx + 5.6f, y + ry - 1.f), Mul(p.skin, 0.62f), 0.7f, 0.5f);
			c.Stroke(Shape().M(cx + out * 6.6f, y - 0.4f).L(cx + out * (p.feminine ? 9.f : 8.f), y - (p.feminine ? 2.8f : 1.6f)), lidColor, p.feminine ? 1.1f : 0.9f);
			if (p.feminine)
			{
				c.Stroke(Shape().M(cx + out * 5.2f, y - 2.1f).L(cx + out * 7.2f, y - 4.f), lidColor, 0.9f);
			}
		}
	}

	void DrawNose(Canvas& c, const PortraitSpec& p)
	{
		const Rgb& s = p.skin;
		c.Glow(85.5f, 111.f, 6.f, 2.6f, Mul(s, 0.55f), 0.5f);                                                     // 코 밑 그늘
		c.Stroke(Shape().M(85.4f, 96.5f).C(85.6f, 101.f, 87.2f, 105.f, 88.f, 108.f), Mul(s, 0.7f), 1.f, 0.6f);      // 그늘진 콧등 옆
		c.Stroke(Shape().M(83.2f, 98.f).C(83.f, 101.f, 82.9f, 103.5f, 83.1f, 105.5f), Mix(s, kWhite, 0.45f), 1.6f, 0.3f); // 콧등 하이라이트
		c.Stroke(Shape().M(80.2f, 108.2f).C(79.4f, 109.8f, 80.4f, 111.f, 82.f, 111.f), Mul(s, 0.62f), 0.9f, 0.8f);  // 콧방울
		c.Stroke(Shape().M(88.6f, 108.2f).C(89.4f, 109.8f, 88.4f, 111.f, 86.8f, 111.f), Mul(s, 0.6f), 0.9f, 0.8f);
		c.Fill(Shape().E(82.3f, 110.3f, 1.2f, 0.7f), Mul(s, 0.42f), 0.8f);                                        // 콧구멍
		c.Fill(Shape().E(86.5f, 110.3f, 1.2f, 0.7f), Mul(s, 0.42f), 0.8f);
		c.Glow(83.4f, 106.8f, 2.4f, 1.8f, kWhite, 0.35f);                                                         // 코끝
	}

	void DrawBrows(Canvas& c, const PortraitSpec& p)
	{
		float lift = 0.f;
		switch (p.expr)
		{
		case Expression::Angry: lift = 2.6f; break;
		case Expression::Stern: lift = 1.6f; break;
		case Expression::Frown: lift = 1.6f; break;
		case Expression::Sad: lift = -2.4f; break;
		case Expression::Tired: lift = -0.6f; break;
		default: break;
		}

		float width = p.feminine ? 1.8f : 2.6f;
		float raise = (p.expr == Expression::Smirk) ? -2.f : 0.f; // 비웃을 땐 오른쪽 눈썹만 올라간다
		Rgb color = p.BrowColor();

		c.Stroke(Shape().M(64.f, 88.f).C(68.f, 84.6f, 74.f, 84.6f + lift * 0.5f, 79.5f, 87.f + lift), color, width);
		c.Stroke(Shape().M(88.5f, 87.f + lift + raise).C(94.f, 84.6f + lift * 0.5f + raise, 100.f, 84.6f + raise, 104.f, 88.f + raise), color, width);
	}

	void DrawWrinkles(Canvas& c, const PortraitSpec& p)
	{
		if (p.old <= 0.f)
		{
			return;
		}

		Rgb color = Mul(p.skin, 0.62f);
		float a = p.old;
		c.Stroke(Shape().M(74.f, 113.f).C(72.f, 117.f, 72.f, 121.f, 74.f, 125.f), color, 1.f, a);
		c.Stroke(Shape().M(94.f, 113.f).C(96.f, 117.f, 96.f, 121.f, 94.f, 125.f), color, 1.f, a);

		const float centers[2] = { kEyeL, kEyeR };
		for (float cx : centers)
		{
			float out = (cx < 84.f) ? -1.f : 1.f;
			c.Stroke(Shape().M(cx - 5.f, kEyeY + 4.5f).C(cx - 2.f, kEyeY + 6.f, cx + 2.f, kEyeY + 6.f, cx + 5.f, kEyeY + 4.5f), color, 0.8f, a * 0.8f);
			c.Stroke(Shape().M(cx + out * 8.f, kEyeY - 1.f).L(cx + out * 11.f, kEyeY - 2.5f), color, 0.8f, a);
			c.Stroke(Shape().M(cx + out * 8.f, kEyeY + 1.5f).L(cx + out * 11.f, kEyeY + 2.5f), color, 0.8f, a);
		}

		if (!p.coversForehead)
		{
			c.Stroke(Shape().M(70.f, 77.f).C(78.f, 75.f, 90.f, 75.f, 98.f, 77.f), color, 0.9f, a * 0.8f);
			c.Stroke(Shape().M(72.f, 81.f).C(80.f, 79.5f, 88.f, 79.5f, 96.f, 81.f), color, 0.9f, a * 0.6f);
		}
	}

	// 입의 두 선: 윗입술 아랫선(top)과 아랫입술 윗선(bottom) — 다문 입이면 같다. 베지어 네 점 {x0,y0,x1,y1,x2,y2,x3,y3}.
	struct MouthCurve
	{
		float top[8];
		float bottom[8];
	};

	MouthCurve MouthCurves(Expression expr)
	{
		MouthCurve m;
		float bottom[8] = { 77.5f, 120.f, 81.f, 121.f, 87.f, 121.f, 90.5f, 120.f };
		switch (expr)
		{
		case Expression::Smile:
			{
				const float v[8] = { 76.5f, 118.6f, 80.f, 123.f, 88.f, 123.f, 91.5f, 118.6f };
				std::copy(v, v + 8, bottom);
			}
			break;
		case Expression::Grin:
			{
				const float top[8] = { 76.f, 118.f, 80.f, 120.f, 88.f, 120.f, 92.f, 118.f };
				const float v[8] = { 76.f, 118.f, 80.f, 125.5f, 88.f, 125.5f, 92.f, 118.f };
				std::copy(top, top + 8, m.top);
				std::copy(v, v + 8, m.bottom);
				return m;
			}
		case Expression::Smirk:
			{
				const float v[8] = { 77.5f, 120.4f, 81.f, 121.f, 87.f, 120.4f, 91.5f, 117.4f };
				std::copy(v, v + 8, bottom);
			}
			break;
		case Expression::Frown:
		case Expression::Stern:
		case Expression::Angry:
			{
				const float v[8] = { 77.5f, 121.2f, 81.f, 118.8f, 87.f, 118.8f, 90.5f, 121.2f };
				std::copy(v, v + 8, bottom);
			}
			break;
		case Expression::Sad:
			{
				const float v[8] = { 78.f, 121.8f, 81.f, 119.6f, 87.f, 119.6f, 90.f, 121.8f };
				std::copy(v, v + 8, bottom);
			}
			break;
		default:
			break;
		}

		std::copy(bottom, bottom + 8, m.top);
		std::copy(bottom, bottom + 8, m.bottom);
		return m;
	}

	void DrawMouth(Canvas& c, const PortraitSpec& p)
	{
		MouthCurve curve = MouthCurves(p.expr);
		const float* t = curve.top;
		const float* b = curve.bottom;
		const Rgb& lip = p.lip;
		float lipAlpha = p.feminine ? 0.9f : 0.32f;
		float midY = (b[3] + b[5]) * 0.5f;

		// 인중·입꼬리·아랫입술 밑 그늘
		c.Glow(84.f, 115.5f, 2.2f, 2.6f, Mul(p.skin, 0.78f), 0.35f);
		c.Glow(b[0] - 0.5f, b[1] + 0.5f, 2.4f, 2.f, Mul(p.skin, 0.55f), 0.35f);
		c.Glow(b[6] + 0.5f, b[7] + 0.5f, 2.4f, 2.f, Mul(p.skin, 0.55f), 0.35f);
		c.Glow(84.f, midY + 5.4f, 4.2f, 1.5f, Mul(p.skin, 0.6f), 0.35f);

		if (p.expr == Expression::Grin)
		{
			c.Fill(Shape().M(b[0], b[1]).C(b[2], b[3], b[4], b[5], b[6], b[7]).C(t[4], t[5], t[2], t[3], t[0], t[1]).Z(), { 0.98f, 0.95f, 0.9f });
		}

		Shape upper;
		upper.M(t[0], t[1]).C(t[2], t[3] - 3.f, t[4], t[5] - 3.f, t[6], t[7]).C(t[4], t[5], t[2], t[3], t[0], t[1]).Z();
		c.FillLinear(upper, 0.f, t[3] - 3.f, Mul(lip, 0.78f), lipAlpha, 0.f, t[3], lip, lipAlpha);

		Shape lower;
		lower.M(b[0], b[1]).C(b[2], b[3], b[4], b[5], b[6], b[7]).C(b[4], b[5] + 4.4f, b[2], b[3] + 4.4f, b[0], b[1]).Z();
		c.FillLinear(lower, 0.f, b[3], lip, lipAlpha, 0.f, b[3] + 4.f, Mul(lip, 0.72f), lipAlpha);

		c.Glow(84.f, midY + 2.2f, 3.f, 1.f, kWhite, p.feminine ? 0.4f : 0.15f);
		c.Stroke(Shape().M(b[0], b[1]).C(b[2], b[3], b[4], b[5], b[6], b[7]), Mul(lip, 0.5f), 1.1f);
		if (p.expr == Expression::Grin)
		{
			c.Stroke(Shape().M(t[0], t[1]).C(t[2], t[3], t[4], t[5], t[6], t[7]), Mul(lip, 0.5f), 0.9f);
		}
	}

	void DrawBeard(Canvas& c, const PortraitSpec& p)
	{
		if (p.beard == Beard::None)
		{
			return;
		}

		Rgb color = p.BeardColor();
		Rgb dark = Mul(color, 0.7f);

		Shape beard;
		float bottom = 158.f;
		switch (p.beard)
		{
		case Beard::Full:
			beard.M(57.f, 98.f).C(58.f, 124.f, 70.f, 148.f, 84.f, 156.f).C(98.f, 148.f, 110.f, 124.f, 111.f, 98.f)
				.C(106.f, 110.f, 100.f, 121.f, 92.f, 122.f).C(88.f, 119.5f, 80.f, 119.5f, 76.f, 122.f).C(68.f, 121.f, 62.f, 110.f, 57.f, 98.f).Z();
			bottom = 154.f;
			break;
		case Beard::Long:
			beard.M(57.f, 98.f).C(56.f, 132.f, 66.f, 164.f, 84.f, 180.f).C(102.f, 164.f, 112.f, 132.f, 111.f, 98.f)
				.C(106.f, 110.f, 100.f, 121.f, 92.f, 122.f).C(88.f, 119.5f, 80.f, 119.5f, 76.f, 122.f).C(68.f, 121.f, 62.f, 110.f, 57.f, 98.f).Z();
			bottom = 176.f;
			break;
		case Beard::Short:
			beard.M(58.f, 102.f).C(60.f, 122.f, 70.f, 136.f, 84.f, 139.f).C(98.f, 136.f, 108.f, 122.f, 110.f, 102.f)
				.C(104.f, 114.f, 98.f, 121.f, 92.f, 122.f).C(88.f, 119.5f, 80.f, 119.5f, 76.f, 122.f).C(70.f, 121.f, 64.f, 114.f, 58.f, 102.f).Z();
			bottom = 138.f;
			break;
		case Beard::Goatee:
			beard.M(76.f, 124.f).C(78.f, 140.f, 82.f, 150.f, 84.f, 160.f).C(86.f, 150.f, 90.f, 140.f, 92.f, 124.f).C(88.f, 127.f, 80.f, 127.f, 76.f, 124.f).Z();
			bottom = 158.f;
			break;
		default:
			break;
		}

		c.FillLinear(beard, 0.f, 100.f, color, 1.f, 0.f, 170.f, dark, 1.f);
		c.WithClip(beard, [&]()
		{
			// 수염 결(밝은 결 하나에 어두운 결 둘)과 빛 받는 쪽·배경빛 가장자리
			for (int i = 0; i < 15; ++i)
			{
				float x = 58.f + (float)i * 3.7f;
				Rgb strand = (i % 3 == 0) ? Mix(color, kWhite, 0.3f) : Mul(color, 0.6f);
				c.Stroke(Shape().M(x, 104.f).C(x + (84.f - x) * 0.1f, bottom - 30.f, x + (84.f - x) * 0.3f, bottom - 12.f, 84.f + (x - 84.f) * 0.55f, bottom + 2.f), strand, 0.9f, 0.55f);
			}
			c.Glow(70.f, 130.f, 10.f, 16.f, Mix(color, kWhite, 0.3f), 0.25f);
			c.Glow(108.f, 126.f, 6.f, 22.f, p.glow, 0.3f);
		});

		// 콧수염
		Shape mustache;
		mustache.M(73.f, 117.5f).C(77.f, 113.f, 83.f, 114.f, 84.f, 116.2f).C(85.f, 114.f, 91.f, 113.f, 95.f, 117.5f)
			.C(92.f, 120.f, 88.f, 119.f, 84.f, 118.6f).C(80.f, 119.f, 76.f, 120.f, 73.f, 117.5f).Z();
		c.FillLinear(mustache, 0.f, 113.f, color, 1.f, 0.f, 120.f, dark, 1.f);
		c.Stroke(Shape().M(75.f, 116.2f).C(78.f, 114.4f, 81.f, 114.6f, 83.f, 115.8f), Mix(color, kWhite, 0.35f), 0.7f, 0.6f);
	}

	void DrawHairFront(Canvas& c, const PortraitSpec& p)
	{
		Rgb color = p.hair;
		Rgb dark = Mul(color, 0.72f);

		if (p.hairStyle == HairStyle::None || p.headgear == Headgear::HelmetGuards)
		{
			return;
		}

		if (p.headgear == Headgear::Hood)
		{
			// 두건 아래로 보이는 앞머리 몇 가닥.
			c.FillLinear(Shape().M(64.f, 76.f).C(70.f, 68.f, 98.f, 68.f, 104.f, 76.f).C(96.f, 74.f, 90.f, 76.f, 84.f, 80.f).C(78.f, 76.f, 72.f, 74.f, 64.f, 76.f).Z(),
				0.f, 70.f, Mix(color, kWhite, 0.15f), 1.f, 0.f, 80.f, dark, 1.f);
			return;
		}

		Shape cap;
		cap.M(54.f, 94.f).C(50.f, 40.f, 118.f, 40.f, 114.f, 94.f).C(112.f, 82.f, 108.f, 74.f, 100.f, 70.f)
			.C(92.f, 75.f, 76.f, 75.f, 68.f, 70.f).C(60.f, 74.f, 56.f, 82.f, 54.f, 94.f).Z();
		c.FillLinear(cap, 0.f, 52.f, Mix(color, kWhite, 0.12f), 1.f, 0.f, 94.f, dark, 1.f);
		c.WithClip(cap, [&]()
		{
			// 정수리에서 이마로 내려오는 결, 왼쪽 위의 윤기, 오른쪽 가장자리의 배경빛
			for (int i = 0; i < 12; ++i)
			{
				float t = (float)i / 11.f;
				float x = 58.f + t * 52.f;
				Rgb strand = (i % 3 == 1) ? Mix(color, kWhite, 0.3f) : Mul(color, 0.62f);
				c.Stroke(Shape().M(84.f + (x - 84.f) * 0.2f, 46.f).C(84.f + (x - 84.f) * 0.6f, 52.f, x, 60.f, x + ((x < 84.f) ? -2.f : 2.f), 82.f), strand, 1.f, 0.5f);
			}
			c.Stroke(Shape().M(64.f, 62.f).C(70.f, 54.f, 82.f, 51.f, 94.f, 53.f), Mix(color, kWhite, 0.5f), 3.f, 0.3f);
			c.Glow(117.f, 74.f, 7.f, 22.f, p.glow, 0.35f);
		});

		if (p.hairStyle == HairStyle::Curly)
		{
			for (int i = 0; i < 11; ++i)
			{
				float a = kPi * (1.02f + 0.96f * (float)i / 10.f);
				float x = 84.f + cosf(a) * 30.f;
				float y = 74.f + sinf(a) * 19.f;
				c.Fill(Shape().E(x, y, 6.f, 5.5f), color);
				c.Glow(x - 1.5f, y - 1.6f, 3.f, 2.6f, Mix(color, kWhite, 0.45f), 0.55f);
				c.Stroke(Shape().M(x - 3.f, y + 1.f).C(x - 1.f, y - 3.f, x + 2.f, y - 3.f, x + 3.f, y), dark, 0.9f, 0.7f);
				c.Stroke(Shape().E(x, y, 6.f, 5.5f), Mul(color, 0.6f), 0.6f, 0.5f);
			}
		}

		if (p.hairStyle == HairStyle::Wild)
		{
			Shape spikes;
			spikes.M(52.f, 92.f);
			for (int i = 0; i <= 12; ++i)
			{
				float a = kPi * (1.f + (float)i / 12.f);
				float r = (i % 2) ? 40.f : 30.f;
				spikes.L(84.f + cosf(a) * r, 82.f + sinf(a) * r * 0.9f);
			}
			spikes.L(116.f, 92.f).C(108.f, 76.f, 96.f, 70.f, 84.f, 72.f).C(72.f, 70.f, 60.f, 76.f, 52.f, 92.f).Z();
			c.FillLinear(spikes, 0.f, 44.f, Mix(color, kWhite, 0.15f), 1.f, 0.f, 92.f, dark, 1.f);
			for (int i = 1; i < 12; i += 2)
			{
				float a = kPi * (1.f + (float)i / 12.f);
				c.Stroke(Shape().M(84.f + cosf(a) * 38.f, 82.f + sinf(a) * 34.f).L(84.f + cosf(a) * 24.f, 82.f + sinf(a) * 20.f), Mul(color, 0.6f), 0.8f, 0.6f);
			}
			c.Stroke(Shape().M(62.f, 60.f).C(70.f, 52.f, 82.f, 50.f, 92.f, 52.f), Mix(color, kWhite, 0.5f), 2.4f, 0.3f);
		}

		if (p.hairStyle == HairStyle::Long || p.hairStyle == HairStyle::Short)
		{
			// 옆머리
			c.Fill(Shape().M(54.f, 90.f).C(52.f, 100.f, 54.f, 112.f, 58.f, 120.f).C(57.f, 108.f, 57.f, 98.f, 58.f, 90.f).Z(), color);
			c.Fill(Shape().M(114.f, 90.f).C(116.f, 100.f, 114.f, 112.f, 110.f, 120.f).C(111.f, 108.f, 111.f, 98.f, 110.f, 90.f).Z(), dark);
		}
	}

	void DrawWreath(Canvas& c, const PortraitSpec& p)
	{
		Rgb leaf = { 0.34f, 0.56f, 0.24f };
		if (p.wreath == WreathKind::Gold) leaf = { 0.95f, 0.75f, 0.3f };
		if (p.wreath == WreathKind::Pomegranate) leaf = { 0.22f, 0.38f, 0.2f };
		if (p.wreath == WreathKind::Ivy) leaf = { 0.26f, 0.5f, 0.22f };
		Rgb dark = Mul(leaf, 0.7f);

		const int kLeaves = 13;
		for (int i = 0; i < kLeaves; ++i)
		{
			float a = kPi * (1.06f + 0.88f * (float)i / (float)(kLeaves - 1));
			float x = 84.f + cosf(a) * 31.f;
			float y = 79.f + sinf(a) * 13.f;
			float tangent = a + kPi * 0.5f;
			float len = (p.wreath == WreathKind::Ivy) ? 10.f : 9.f;
			float wid = (p.wreath == WreathKind::Ivy) ? 4.4f : 2.8f;
			c.Fill(Leaf(x, y - 2.f, tangent - 0.5f, len, wid), (i % 2) ? leaf : dark);
			c.Fill(Leaf(x, y + 2.f, tangent + 0.5f, len, wid), (i % 2) ? dark : leaf);
		}

		const float spots[3][2] = { { 60.f, 74.f }, { 84.f, 66.f }, { 108.f, 74.f } };
		if (p.wreath == WreathKind::Rose)
		{
			for (const auto& spot : spots)
			{
				float x = spot[0], y = spot[1];
				c.Fill(Shape().E(x, y, 4.5f, 4.2f), { 1.f, 0.5f, 0.62f });
				c.Stroke(Shape().M(x - 2.f, y).C(x - 1.f, y - 2.5f, x + 2.f, y - 2.f, x + 1.5f, y + 1.f), { 0.75f, 0.25f, 0.4f }, 0.9f);
			}
		}
		if (p.wreath == WreathKind::Pomegranate)
		{
			const float fruits[3][2] = { { 62.f, 74.f }, { 84.f, 66.f }, { 106.f, 74.f } };
			for (const auto& fruit : fruits)
			{
				c.Fill(Shape().E(fruit[0], fruit[1], 3.6f, 3.6f), { 0.78f, 0.14f, 0.2f });
			}
		}
		if (p.wreath == WreathKind::Ivy)
		{
			for (int side = -1; side <= 1; side += 2)
			{
				for (int q = 0; q < 6; ++q)
				{
					c.Fill(Shape().E(84.f + (float)side * (32.f + (float)(q % 2) * 3.f), 82.f + (float)q * 3.f, 2.6f, 2.6f), { 0.42f, 0.14f, 0.38f });
				}
			}
		}
		if (p.wreath == WreathKind::Gold)
		{
			c.Fill(Shape().E(84.f, 66.f, 3.f, 3.f), { 0.6f, 0.1f, 0.12f });
		}
	}

	void DrawHeadgear(Canvas& c, const PortraitSpec& p)
	{
		const Rgb gold = { 1.f, 0.84f, 0.38f };
		const Rgb goldDark = { 0.7f, 0.5f, 0.14f };

		switch (p.headgear)
		{
		case Headgear::None:
			break;

		case Headgear::Crown:
		case Headgear::DarkCrown:
			{
				bool dark = (p.headgear == Headgear::DarkCrown);
				Rgb top = dark ? Rgb{ 0.36f, 0.33f, 0.42f } : gold;
				Rgb bottom = dark ? Rgb{ 0.12f, 0.1f, 0.15f } : goldDark;
				const float xs[5] = { 62.f, 73.f, 84.f, 95.f, 106.f };
				const float darkHeights[5] = { 9.f, 12.f, 16.f, 12.f, 9.f };
				const float goldHeights[5] = { 7.f, 9.f, 13.f, 9.f, 7.f };
				for (int i = 0; i < 5; ++i)
				{
					float x = xs[i];
					float t = (x - 84.f) / 27.f;
					float y = 76.f - 4.5f * (1.f - t * t);
					float h = dark ? darkHeights[i] : goldHeights[i];
					float half = dark ? 2.6f : 4.2f;
					c.FillLinear(Shape().M(x - half, y + 1.f).L(x, y - h).L(x + half, y + 1.f).Z(), 0.f, y - h, top, 1.f, 0.f, y + 1.f, bottom, 1.f);
				}
				c.FillLinear(Shape().M(57.f, 76.f).C(70.f, 70.f, 98.f, 70.f, 111.f, 76.f).L(111.f, 82.f).C(98.f, 76.f, 70.f, 76.f, 57.f, 82.f).Z(),
					0.f, 71.f, top, 1.f, 0.f, 82.f, bottom, 1.f);
				c.Fill(Shape().E(84.f, 77.4f, 2.3f, 2.3f), dark ? Rgb{ 0.65f, 0.3f, 0.95f } : Rgb{ 0.3f, 0.55f, 0.95f });
			}
			break;

		case Headgear::Polos:
			{
				// 헤라의 높은 관(폴로스).
				c.FillLinear(Shape().M(60.f, 76.f).L(62.f, 58.f).C(72.f, 53.f, 96.f, 53.f, 106.f, 58.f).L(108.f, 76.f).C(98.f, 71.f, 70.f, 71.f, 60.f, 76.f).Z(),
					0.f, 54.f, gold, 1.f, 0.f, 76.f, goldDark, 1.f);
				for (int i = 0; i < 5; ++i)
				{
					c.Fill(Shape().E(68.f + (float)i * 8.f, 64.f, 1.8f, 1.8f), { 0.35f, 0.2f, 0.55f });
				}
				c.Stroke(Shape().M(61.f, 70.f).C(72.f, 66.f, 96.f, 66.f, 107.f, 70.f), { 0.85f, 0.65f, 0.25f }, 1.f);
			}
			break;

		case Headgear::Wreath:
			DrawWreath(c, p);
			break;

		case Headgear::Helmet:
		case Headgear::HelmetGuards:
			{
				const Rgb bronze = { 0.88f, 0.64f, 0.32f };
				const Rgb bronzeDark = { 0.5f, 0.32f, 0.14f };

				// 붉은 말총 볏
				c.FillLinear(Shape().M(70.f, 50.f).C(64.f, 14.f, 104.f, 14.f, 98.f, 50.f).C(94.f, 42.f, 74.f, 42.f, 70.f, 50.f).Z(),
					0.f, 18.f, { 0.85f, 0.2f, 0.15f }, 1.f, 0.f, 50.f, { 0.45f, 0.06f, 0.05f }, 1.f);
				for (int i = 0; i < 6; ++i)
				{
					float x = 72.f + (float)i * 5.f;
					c.Stroke(Shape().M(x, 46.f).C(x, 34.f, 76.f + (float)i * 3.4f, 24.f, 84.f, 20.f), { 0.4f, 0.05f, 0.04f }, 0.8f, 0.6f);
				}

				bool guarded = (p.headgear == Headgear::HelmetGuards);
				float domeBottom = guarded ? 90.f : 80.f;
				c.FillLinear(Shape().M(50.f, domeBottom).C(44.f, 38.f, 124.f, 38.f, 118.f, domeBottom).C(106.f, domeBottom - 6.f, 96.f, domeBottom - 8.f, 84.f, domeBottom - 8.f)
					.C(72.f, domeBottom - 8.f, 62.f, domeBottom - 6.f, 50.f, domeBottom).Z(), 60.f, 40.f, bronze, 1.f, 110.f, 90.f, bronzeDark, 1.f);
				c.Stroke(Shape().M(52.f, domeBottom - 3.f).C(64.f, domeBottom - 10.f, 104.f, domeBottom - 10.f, 116.f, domeBottom - 3.f), { 1.f, 0.85f, 0.5f }, 1.2f, 0.7f);
				c.Glow(70.f, 54.f, 10.f, 6.f, kWhite, 0.35f);

				if (guarded)
				{
					// 뺨 가리개와 코 가리개(코린트식 투구)
					c.FillLinear(Shape().M(50.f, 90.f).C(56.f, 86.f, 62.f, 85.f, 66.f, 86.f).C(66.f, 100.f, 68.f, 116.f, 70.f, 132.f)
						.C(62.f, 132.f, 55.f, 128.f, 51.f, 122.f).C(50.f, 112.f, 50.f, 100.f, 50.f, 90.f).Z(), 50.f, 0.f, bronze, 1.f, 70.f, 0.f, bronzeDark, 1.f);
					c.FillLinear(Shape().M(118.f, 90.f).C(112.f, 86.f, 106.f, 85.f, 102.f, 86.f).C(102.f, 100.f, 100.f, 116.f, 98.f, 132.f)
						.C(106.f, 132.f, 113.f, 128.f, 117.f, 122.f).C(118.f, 112.f, 118.f, 100.f, 118.f, 90.f).Z(), 98.f, 0.f, bronze, 1.f, 118.f, 0.f, bronzeDark, 1.f);
					c.Fill(Shape().M(82.f, 82.f).L(86.f, 82.f).L(85.6f, 106.f).C(85.f, 108.f, 83.f, 108.f, 82.4f, 106.f).Z(), bronzeDark);
				}
			}
			break;

		case Headgear::Hood:
			{
				// 두건 앞자락: 얼굴을 감싸고 어깨까지 망토처럼 흘러내린다.
				c.FillLinear(Shape().M(14.f, 212.f).C(24.f, 150.f, 34.f, 44.f, 84.f, 40.f).C(134.f, 44.f, 144.f, 150.f, 154.f, 212.f).L(116.f, 212.f)
					.C(118.f, 150.f, 114.f, 78.f, 84.f, 70.f).C(54.f, 78.f, 50.f, 150.f, 52.f, 212.f).Z(),
					30.f, 0.f, Mix(p.hood, kWhite, 0.1f), 1.f, 140.f, 0.f, Mul(p.hood, 0.6f), 1.f);
				c.Stroke(Shape().M(52.f, 210.f).C(50.f, 150.f, 54.f, 78.f, 84.f, 70.f).C(114.f, 78.f, 118.f, 150.f, 116.f, 210.f), Mul(p.hood, 0.45f), 1.6f, 0.8f);
				const float folds[2][2] = { { 40.f, 30.f }, { 132.f, 140.f } };
				for (const auto& fold : folds)
				{
					float x0 = fold[0], x1 = fold[1];
					c.Stroke(Shape().M(x0, 120.f).C(x0 - 2.f, 150.f, x1, 180.f, x1, 210.f), Mul(p.hood, 0.6f), 1.2f, 0.6f);
				}
			}
			break;

		case Headgear::Petasos:
			{
				// 헤르메스의 날개 달린 챙 모자.
				const Rgb hat = { 0.62f, 0.5f, 0.32f };
				c.FillLinear(Shape().E(84.f, 70.f, 46.f, 8.5f), 0.f, 62.f, Mix(hat, kWhite, 0.15f), 1.f, 0.f, 78.f, Mul(hat, 0.7f), 1.f);
				c.FillLinear(Shape().M(62.f, 70.f).C(62.f, 42.f, 106.f, 42.f, 106.f, 70.f).C(98.f, 67.f, 70.f, 67.f, 62.f, 70.f).Z(), 0.f, 44.f, hat, 1.f, 0.f, 70.f, Mul(hat, 0.75f), 1.f);
				for (int side = -1; side <= 1; side += 2)
				{
					for (int k = 0; k < 3; ++k)
					{
						float a = (side < 0) ? kPi + 0.5f + (float)k * 0.32f : -0.5f - (float)k * 0.32f;
						float x = 84.f + (float)side * 24.f + cosf(a) * 9.f;
						float y = 56.f + sinf(a) * 9.f;
						Shape feather = Leaf(x, y, a, 20.f - (float)k * 3.f, 4.f);
						c.Fill(feather, { 0.97f, 0.97f, 0.95f });
						c.Stroke(feather, { 0.6f, 0.62f, 0.68f }, 0.8f);
					}
				}
			}
			break;

		case Headgear::Horns:
			for (int side = -1; side <= 1; side += 2)
			{
				float s = (float)side;
				auto X = [s](float x) { return 84.f + s * (x - 84.f); };
				c.FillLinear(Shape().M(X(68.f), 72.f).C(X(62.f), 56.f, X(52.f), 44.f, X(40.f), 46.f).C(X(48.f), 50.f, X(54.f), 60.f, X(56.f), 76.f).Z(),
					0.f, 44.f, { 0.9f, 0.84f, 0.7f }, 1.f, 0.f, 76.f, { 0.52f, 0.44f, 0.34f }, 1.f);
				for (int k = 0; k < 3; ++k)
				{
					float kk = (float)k * 4.f;
					c.Stroke(Shape().M(X(58.f - kk), 62.f - kk).L(X(63.f - kk), 60.f - kk), { 0.45f, 0.38f, 0.3f }, 0.8f, 0.7f);
				}
			}
			break;
		}
	}

	// 무언가를 쥔 손(주먹 + 엄지).
	void Hand(Canvas& c, const PortraitSpec& p, float x, float y)
	{
		c.FillLinear(Shape().E(x, y, 10.f, 7.f), 0.f, y - 7.f, p.skin, 1.f, 0.f, y + 7.f, Mul(p.skin, 0.75f), 1.f);
		for (int k = 0; k < 3; ++k)
		{
			float lx = x - 5.f + (float)k * 4.f;
			c.Stroke(Shape().M(lx, y - 2.f).L(lx, y + 3.f), Mul(p.skin, 0.6f), 0.7f, 0.7f);
		}
		c.Fill(Shape().E(x + 7.f, y - 3.5f, 3.6f, 2.8f), Mul(p.skin, 0.95f));
	}

	void DrawProp(Canvas& c, const PortraitSpec& p)
	{
		const Rgb gold = { 1.f, 0.84f, 0.38f };
		const Rgb goldDark = { 0.7f, 0.5f, 0.14f };

		switch (p.prop)
		{
		case Prop::None:
			break;

		case Prop::Scepter:
			c.Stroke(Shape().M(148.f, 212.f).L(146.f, 66.f), goldDark, 4.4f);
			c.Stroke(Shape().M(147.f, 212.f).L(145.f, 66.f), gold, 2.f);
			c.Glow(146.f, 58.f, 14.f, 14.f, gold, 0.5f);
			c.FillLinear(Shape().E(146.f, 58.f, 6.5f, 6.5f), 0.f, 52.f, Mix(gold, kWhite, 0.4f), 1.f, 0.f, 64.f, goldDark, 1.f);
			Hand(c, p, 147.f, 176.f);
			break;

		case Prop::Trident:
			{
				const Rgb metal = { 0.62f, 0.82f, 0.8f };
				const Rgb metalDark = { 0.25f, 0.42f, 0.42f };
				c.Stroke(Shape().M(144.f, 212.f).L(144.f, 72.f), metalDark, 4.f);
				c.Stroke(Shape().M(143.4f, 212.f).L(143.4f, 72.f), metal, 1.6f);
				c.Stroke(Shape().M(130.f, 72.f).C(132.f, 80.f, 156.f, 80.f, 158.f, 72.f), metal, 3.f);
				const float tines[3][2] = { { 130.f, 52.f }, { 144.f, 44.f }, { 158.f, 52.f } };
				for (const auto& tine : tines)
				{
					float x = tine[0], top = tine[1];
					c.Stroke(Shape().M(x, (x == 144.f) ? 78.f : 72.f).L(x, top + 6.f), metal, 2.6f);
					c.Fill(Shape().M(x - 3.6f, top + 8.f).L(x, top).L(x + 3.6f, top + 8.f).Z(), Mix(metal, kWhite, 0.3f));
				}
				Hand(c, p, 144.f, 180.f);
			}
			break;

		case Prop::Caduceus:
			{
				c.Stroke(Shape().M(146.f, 212.f).L(146.f, 60.f), goldDark, 3.6f);
				c.Stroke(Shape().M(145.5f, 212.f).L(145.5f, 60.f), gold, 1.4f);
				const float phases[2] = { 0.f, kPi };
				for (float phase : phases)
				{
					Shape snake;
					snake.M(146.f + sinf(phase) * 7.f, 150.f);
					for (float y = 150.f; y >= 76.f; y -= 4.f)
					{
						snake.L(146.f + sinf(phase + (150.f - y) * 0.12f) * 7.f, y);
					}
					c.Stroke(snake, { 0.3f, 0.55f, 0.3f }, 2.2f);
				}
				const float wings[2] = { kPi + 0.4f, -0.4f };
				for (float a : wings)
				{
					c.Fill(Leaf(146.f + cosf(a) * 9.f, 64.f + sinf(a) * 5.f, a, 16.f, 3.6f), { 0.97f, 0.97f, 0.95f });
				}
				c.Fill(Shape().E(146.f, 57.f, 3.6f, 3.6f), gold);
				Hand(c, p, 146.f, 182.f);
			}
			break;

		case Prop::Hammer:
			{
				c.Stroke(Shape().M(118.f, 212.f).L(150.f, 124.f), { 0.45f, 0.3f, 0.18f }, 5.f);
				const float cx = 152.f, cy = 118.f, a = -1.22f, hw = 15.f, hh = 6.5f;
				float dx = cosf(a), dy = sinf(a), nx = -dy, ny = dx;
				const float corners[4][2] = { { -hw, -hh }, { hw, -hh }, { hw, hh }, { -hw, hh } };
				float px[4], py[4];
				for (int i = 0; i < 4; ++i)
				{
					px[i] = cx + nx * corners[i][0] + dx * corners[i][1];
					py[i] = cy + ny * corners[i][0] + dy * corners[i][1];
				}
				c.FillLinear(Shape().M(px[0], py[0]).L(px[1], py[1]).L(px[2], py[2]).L(px[3], py[3]).Z(),
					140.f, 104.f, { 0.55f, 0.55f, 0.58f }, 1.f, 164.f, 132.f, { 0.2f, 0.2f, 0.23f }, 1.f);
				Hand(c, p, 128.f, 190.f);
			}
			break;

		case Prop::Spear:
			c.Stroke(Shape().M(146.f, 212.f).L(146.f, 40.f), { 0.5f, 0.36f, 0.22f }, 3.2f);
			c.FillLinear(Shape().M(146.f, 12.f).C(153.f, 24.f, 153.f, 34.f, 146.f, 44.f).C(139.f, 34.f, 139.f, 24.f, 146.f, 12.f).Z(),
				140.f, 0.f, { 0.95f, 0.75f, 0.4f }, 1.f, 152.f, 0.f, { 0.55f, 0.36f, 0.14f }, 1.f);
			Hand(c, p, 146.f, 178.f);
			break;

		case Prop::Apple:
			// 금빛 사과(복선 F-02).
			Hand(c, p, 44.f, 194.f);
			c.Glow(44.f, 180.f, 20.f, 20.f, gold, 0.4f);
			c.FillLinear(Shape().M(44.f, 172.f).C(54.f, 168.f, 56.f, 182.f, 50.f, 189.f).C(47.f, 191.f, 41.f, 191.f, 38.f, 189.f).C(32.f, 182.f, 34.f, 168.f, 44.f, 172.f).Z(),
				36.f, 170.f, Mix(gold, kWhite, 0.3f), 1.f, 54.f, 190.f, goldDark, 1.f);
			c.Stroke(Shape().M(44.f, 173.f).C(44.f, 170.f, 45.f, 168.f, 46.f, 166.f), { 0.4f, 0.28f, 0.12f }, 1.2f);
			c.Fill(Leaf(49.f, 167.f, -0.3f, 7.f, 2.2f), { 0.35f, 0.55f, 0.25f });
			c.Fill(Shape().E(40.f, 177.f, 2.f, 3.f), kWhite, 0.5f);
			break;

		case Prop::Cup:
			Hand(c, p, 46.f, 196.f);
			c.FillLinear(Shape().M(26.f, 178.f).C(30.f, 189.f, 62.f, 189.f, 66.f, 178.f).Z(), 0.f, 178.f, gold, 1.f, 0.f, 188.f, goldDark, 1.f);
			c.Fill(Shape().E(46.f, 178.f, 20.f, 3.2f), Mix(gold, kWhite, 0.25f));
			c.Fill(Shape().E(46.f, 178.f, 17.f, 2.1f), { 0.45f, 0.06f, 0.12f });
			c.Fill(Shape().M(44.f, 187.f).L(48.f, 187.f).L(48.5f, 195.f).L(43.5f, 195.f).Z(), goldDark);
			break;

		case Prop::Pomegranate:
			Hand(c, p, 46.f, 196.f);
			c.FillLinear(Shape().E(46.f, 182.f, 10.5f, 10.f), 36.f, 172.f, { 0.88f, 0.2f, 0.24f }, 1.f, 56.f, 192.f, { 0.42f, 0.04f, 0.08f }, 1.f);
			c.Fill(Shape().M(42.f, 173.f).L(43.f, 168.f).L(46.f, 171.f).L(49.f, 168.f).L(50.f, 173.f).Z(), { 0.55f, 0.08f, 0.12f });
			c.Fill(Shape().E(49.f, 184.f, 4.5f, 5.5f), { 0.3f, 0.03f, 0.06f });
			{
				const float seeds[4][2] = { { 48.f, 182.f }, { 50.5f, 183.5f }, { 48.4f, 186.f }, { 50.6f, 186.6f } };
				for (const auto& seed : seeds)
				{
					c.Fill(Shape().E(seed[0], seed[1], 1.2f, 1.3f), { 1.f, 0.35f, 0.4f });
				}
			}
			break;

		case Prop::Scroll:
			// 하데스의 서류(모든 일을 서류 양식으로 처리한다).
			Hand(c, p, 46.f, 198.f);
			c.Fill(Shape().M(24.f, 178.f).L(66.f, 172.f).L(68.f, 190.f).L(26.f, 196.f).Z(), { 0.9f, 0.84f, 0.68f });
			for (int k = 0; k < 4; ++k)
			{
				float y = (float)k * 3.6f;
				c.Stroke(Shape().M(30.f, 181.f + y).L(60.f - (float)(k % 2) * 8.f, 176.5f + y), { 0.45f, 0.38f, 0.3f }, 0.7f, 0.7f);
			}
			c.Fill(Shape().E(25.f, 187.f, 3.2f, 9.5f), { 0.8f, 0.72f, 0.55f });
			c.Fill(Shape().E(67.f, 181.f, 3.2f, 9.5f), { 0.8f, 0.72f, 0.55f });
			c.Stroke(Shape().M(40.f, 192.f).L(50.f, 190.6f), { 0.6f, 0.1f, 0.12f }, 2.2f); // 붉은 인장 끈
			break;

		case Prop::Chains:
			{
				const Rgb metal = { 0.55f, 0.55f, 0.58f };
				for (int i = 0; i < 18; ++i)
				{
					float t = (float)i / 17.f;
					float x = -2.f + t * 172.f;
					float y = 172.f + sinf(t * kPi) * 14.f;
					if (i % 2)
					{
						c.Stroke(Shape().E(x, y, 1.6f, 3.4f), metal, 1.6f);
					}
					else
					{
						c.Stroke(Shape().E(x, y, 4.6f, 2.6f), metal, 1.8f);
					}
				}
			}
			break;

		case Prop::Spindle:
			// 클로토의 물렛가락과 금실.
			c.Stroke(Shape().M(40.f, 206.f).L(40.f, 160.f), { 0.55f, 0.4f, 0.24f }, 2.4f);
			c.Fill(Shape().E(40.f, 190.f, 7.f, 2.8f), { 0.6f, 0.45f, 0.28f });
			c.Glow(40.f, 166.f, 10.f, 10.f, { 1.f, 0.85f, 0.4f }, 0.5f);
			c.Stroke(Shape().M(40.f, 164.f).C(70.f, 150.f, 110.f, 190.f, 172.f, 170.f), { 1.f, 0.85f, 0.4f }, 1.1f, 0.9f);
			Hand(c, p, 40.f, 178.f);
			break;

		case Prop::Rod:
			// 라케시스의 실을 재는 자.
			c.Stroke(Shape().M(20.f, 206.f).L(70.f, 160.f), { 0.95f, 0.78f, 0.36f }, 2.8f);
			for (int k = 1; k < 6; ++k)
			{
				float x = 20.f + (float)k * 8.3f;
				float y = 206.f - (float)k * 7.7f;
				c.Stroke(Shape().M(x, y).L(x + 3.f, y + 3.f), { 0.5f, 0.36f, 0.12f }, 0.8f);
			}
			Hand(c, p, 46.f, 184.f);
			break;

		case Prop::Shears:
			{
				// 아트로포스의 가위와 잘린 금실.
				const Rgb steel = { 0.82f, 0.84f, 0.88f };
				c.Fill(Leaf(46.f, 176.f, -0.9f, 30.f, 3.4f), steel);
				c.Fill(Leaf(46.f, 176.f, -2.24f, 30.f, 3.4f), Mul(steel, 0.85f));
				c.Stroke(Shape().E(36.f, 192.f, 4.5f, 3.5f), { 0.5f, 0.5f, 0.55f }, 1.6f);
				c.Stroke(Shape().E(56.f, 192.f, 4.5f, 3.5f), { 0.5f, 0.5f, 0.55f }, 1.6f);
				c.Stroke(Shape().M(20.f, 160.f).L(40.f, 166.f), { 1.f, 0.85f, 0.4f }, 1.f, 0.9f);
				c.Stroke(Shape().M(54.f, 166.f).L(80.f, 158.f), { 1.f, 0.85f, 0.4f }, 1.f, 0.4f);
			}
			break;

		case Prop::Staff:
			c.Stroke(Shape().M(30.f, 212.f).C(28.f, 170.f, 32.f, 120.f, 28.f, 70.f), { 0.42f, 0.3f, 0.18f }, 4.2f);
			c.Fill(Shape().E(28.f, 66.f, 5.f, 6.f), { 0.36f, 0.25f, 0.15f });
			Hand(c, p, 30.f, 168.f);
			break;

		case Prop::Ember:
			// 되살아난 화덕의 불씨를 두 손에.
			c.Glow(84.f, 196.f, 30.f, 22.f, { 1.f, 0.55f, 0.2f }, 0.8f);
			c.Fill(Shape().M(84.f, 176.f).C(92.f, 186.f, 92.f, 194.f, 84.f, 198.f).C(76.f, 194.f, 76.f, 186.f, 84.f, 176.f).Z(), { 1.f, 0.75f, 0.3f });
			c.Fill(Shape().M(84.f, 184.f).C(88.f, 190.f, 88.f, 194.f, 84.f, 196.f).C(80.f, 194.f, 80.f, 190.f, 84.f, 184.f).Z(), { 1.f, 0.95f, 0.7f });
			Hand(c, p, 70.f, 202.f);
			Hand(c, p, 98.f, 202.f);
			break;
		}
	}

	// 등 뒤로 늘어진 긴 머리(얼굴·목·옷보다 먼저 그린다): 머리결과 오른쪽 가장자리의 배경빛.
	void DrawHairBack(Canvas& c, const PortraitSpec& p)
	{
		Shape back;
		back.M(52.f, 96.f).C(44.f, 36.f, 124.f, 36.f, 116.f, 96.f).C(120.f, 124.f, 126.f, 150.f, 132.f, 180.f).C(120.f, 186.f, 108.f, 180.f, 104.f, 172.f)
			.L(64.f, 172.f).C(60.f, 180.f, 48.f, 186.f, 36.f, 180.f).C(42.f, 150.f, 48.f, 124.f, 52.f, 96.f).Z();
		c.FillLinear(back, 0.f, 60.f, p.hair, 1.f, 0.f, 186.f, Mul(p.hair, 0.72f), 1.f);
		c.WithClip(back, [&]()
		{
			for (int i = 0; i < 16; ++i)
			{
				float t = (float)i / 15.f;
				float x = 38.f + t * 92.f;
				float sway = sinf((float)i * 1.7f) * 4.f;
				Rgb strand = (i % 3 == 0) ? Mix(p.hair, kWhite, 0.28f) : Mul(p.hair, 0.6f);
				c.Stroke(Shape().M(84.f + (x - 84.f) * 0.3f, 52.f).C(x - (84.f - x) * 0.3f, 70.f, x, 130.f, x + sway, 184.f), strand, 1.1f, 0.45f);
			}
			c.Glow(124.f, 130.f, 8.f, 52.f, p.glow, 0.3f);
		});
	}

	void DrawBust(Canvas& c, const PortraitSpec& p, Rng& rng)
	{
		const Rgb& skin = p.skin;
		const Rgb gold = { 1.f, 0.84f, 0.38f };
		Rgb shadow = Mix(Mul(skin, 0.55f), p.bgBottom, 0.25f); // 그늘엔 배경빛이 살짝 섞인다

		// ---- 뒤쪽: 두건 안쪽 그늘, 베일, 긴 머리
		if (p.headgear == Headgear::Hood)
		{
			c.Fill(Shape().E(84.f, 102.f, 41.f, 58.f), Mul(p.hood, 0.35f));
		}
		if (p.headgear == Headgear::Polos)
		{
			for (int side = -1; side <= 1; side += 2)
			{
				float s = (float)side;
				c.Fill(Shape().M(84.f + s * 22.f, 60.f).C(84.f + s * 48.f, 86.f, 84.f + s * 52.f, 150.f, 84.f + s * 62.f, 200.f).L(84.f + s * 36.f, 204.f)
					.C(84.f + s * 34.f, 150.f, 84.f + s * 30.f, 100.f, 84.f + s * 20.f, 70.f).Z(), p.veil, 0.55f);
				c.Stroke(Shape().M(84.f + s * 26.f, 76.f).C(84.f + s * 40.f, 110.f, 84.f + s * 44.f, 160.f, 84.f + s * 50.f, 200.f), Mix(p.veil, kWhite, 0.3f), 1.f, 0.4f);
			}
		}
		if (p.hairStyle == HairStyle::Long)
		{
			DrawHairBack(c, p);
		}

		// ---- 어깨·옷(빛 받는 왼쪽 어깨, 그늘진 오른쪽, 목이 옷깃에 드리운 그늘)
		Rgb robeColor = p.bare ? skin : p.robe;
		Shape robe;
		robe.M(-2.f, 210.f).L(-2.f, 190.f).C(8.f, 172.f, 34.f, 160.f, 64.f, 154.f).C(72.f, 162.f, 96.f, 162.f, 104.f, 154.f)
			.C(134.f, 160.f, 160.f, 172.f, 170.f, 190.f).L(170.f, 210.f).Z();
		c.FillLinear(robe, 0.f, 150.f, Mix(robeColor, kWhite, 0.08f), 1.f, 0.f, 210.f, Mul(robeColor, 0.62f), 1.f);
		c.WithClip(robe, [&]()
		{
			c.Glow(30.f, 176.f, 36.f, 20.f, Mix(robeColor, kWhite, 0.35f), 0.45f);
			c.Glow(150.f, 198.f, 40.f, 26.f, Mul(robeColor, 0.45f), 0.45f);
			c.Glow(84.f, 156.f, 26.f, 7.f, kBlack, 0.35f);
		});
		if (!p.bare)
		{
			// 옷 주름(그늘 + 바로 옆의 밝은 결)
			const float folds[4][2] = { { 40.f, 30.f }, { 60.f, 56.f }, { 118.f, 126.f }, { 138.f, 150.f } };
			for (const auto& fold : folds)
			{
				float x0 = fold[0], x1 = fold[1];
				c.Stroke(Shape().M(x0, 166.f).C(x0, 180.f, x1, 196.f, x1, 210.f), Mul(robeColor, 0.66f), 1.3f, 0.55f);
				c.Stroke(Shape().M(x0 + 2.f, 166.f).C(x0 + 2.f, 180.f, x1 + 2.f, 196.f, x1 + 2.f, 210.f), Mix(robeColor, kWhite, 0.35f), 0.8f, 0.35f);
			}
			if (p.hasDrape)
			{
				c.FillLinear(Shape().M(112.f, 156.f).C(96.f, 176.f, 66.f, 196.f, 40.f, 210.f).L(76.f, 210.f).C(96.f, 196.f, 116.f, 180.f, 132.f, 162.f).Z(),
					0.f, 156.f, p.drape, 1.f, 0.f, 210.f, Mul(p.drape, 0.7f), 1.f);
				c.Stroke(Shape().M(112.f, 156.f).C(96.f, 176.f, 66.f, 196.f, 40.f, 210.f), Mix(p.drape, kWhite, 0.45f), 1.2f, 0.6f);
				c.Stroke(Shape().M(122.f, 160.f).C(106.f, 180.f, 84.f, 196.f, 60.f, 210.f), Mul(p.drape, 0.6f), 1.f, 0.5f);
			}

			// 옷깃의 장식 띠: 그늘·띠·윤기 줄과 금 구슬
			Shape neckline;
			neckline.M(64.f, 154.f).C(72.f, 164.f, 96.f, 164.f, 104.f, 154.f);
			c.Stroke(neckline, Mul(p.trim, 0.55f), 4.2f);
			c.Stroke(neckline, p.trim, 3.f);
			c.Stroke(Shape().M(66.f, 154.6f).C(73.f, 163.f, 95.f, 163.f, 102.f, 154.6f), Mix(p.trim, kWhite, 0.45f), 0.7f, 0.8f);
			for (int i = 0; i < 7; ++i)
			{
				float t = ((float)i + 0.5f) / 7.f;
				float u = 1.f - t;
				float x = u * u * u * 64.f + 3.f * u * u * t * 72.f + 3.f * u * t * t * 96.f + t * t * t * 104.f;
				float y = u * u * u * 154.f + 3.f * u * u * t * 164.f + 3.f * u * t * t * 164.f + t * t * t * 154.f;
				c.Fill(Shape().E(x, y + 3.2f, 0.9f, 0.9f), gold, 0.85f);
			}

			// 어깨의 브로치(피불라)
			c.Glow(62.f, 160.f, 6.f, 6.f, gold, 0.4f);
			c.FillLinear(Shape().E(62.f, 160.f, 3.6f, 3.6f), 59.f, 157.f, Mix(gold, kWhite, 0.4f), 1.f, 65.f, 163.f, Mul(gold, 0.6f), 1.f);
			c.Fill(Shape().E(62.f, 160.f, 1.6f, 1.6f), Mix(p.trim, p.glow, 0.5f));
			c.Fill(Shape().E(61.4f, 159.3f, 0.6f, 0.6f), kWhite, 0.9f);
		}
		else
		{
			// 맨어깨(티탄): 쇄골과 가슴 그늘
			c.Stroke(Shape().M(62.f, 166.f).C(70.f, 170.f, 78.f, 170.f, 82.f, 168.f), Mul(skin, 0.7f), 1.3f, 0.8f);
			c.Stroke(Shape().M(106.f, 166.f).C(98.f, 170.f, 90.f, 170.f, 86.f, 168.f), Mul(skin, 0.7f), 1.3f, 0.8f);
			c.Glow(84.f, 200.f, 40.f, 16.f, Mul(skin, 0.6f), 0.5f);
		}

		// ---- 목(턱 그늘, 그늘진 오른쪽, 빛 받는 왼쪽 아래)
		Shape neck;
		neck.M(74.f, 118.f).L(94.f, 118.f).L(95.f, 150.f).C(90.f, 155.f, 78.f, 155.f, 73.f, 150.f).Z();
		c.FillLinear(neck, 74.f, 0.f, Mul(skin, 0.88f), 1.f, 95.f, 0.f, Mul(skin, 0.72f), 1.f);
		c.WithClip(neck, [&]()
		{
			c.Glow(84.f, 128.f, 16.f, 12.f, shadow, 0.7f);
			c.Glow(96.f, 140.f, 5.f, 18.f, Mul(skin, 0.55f), 0.4f);
			c.Glow(76.f, 146.f, 4.f, 8.f, Mix(skin, kWhite, 0.3f), 0.3f);
		});

		// ---- 귀
		if (p.headgear != Headgear::Hood && p.headgear != Headgear::HelmetGuards)
		{
			if (p.pointedEars)
			{
				c.Fill(Shape().M(58.f, 90.f).C(50.f, 84.f, 44.f, 78.f, 40.f, 76.f).C(44.f, 88.f, 50.f, 100.f, 58.f, 104.f).Z(), Mul(skin, 0.92f));
				c.Fill(Shape().M(110.f, 90.f).C(118.f, 84.f, 124.f, 78.f, 128.f, 76.f).C(124.f, 88.f, 118.f, 100.f, 110.f, 104.f).Z(), Mul(skin, 0.86f));
				c.Stroke(Shape().M(54.f, 90.f).C(50.f, 88.f, 47.f, 85.f, 45.f, 82.f), Mul(skin, 0.62f), 0.9f, 0.7f);
				c.Stroke(Shape().M(114.f, 90.f).C(118.f, 88.f, 121.f, 85.f, 123.f, 82.f), Mul(skin, 0.6f), 0.9f, 0.7f);
			}
			else
			{
				c.Fill(Shape().E(56.f, 97.f, 4.5f, 8.5f), Mul(skin, 0.9f));
				c.Fill(Shape().E(112.f, 97.f, 4.5f, 8.5f), Mul(skin, 0.82f));
				c.Stroke(Shape().M(55.f, 91.5f).C(53.f, 95.f, 53.5f, 100.f, 55.6f, 102.5f), Mul(skin, 0.62f), 0.9f, 0.8f);
				c.Stroke(Shape().M(113.f, 91.5f).C(115.f, 95.f, 114.5f, 100.f, 112.4f, 102.5f), Mul(skin, 0.58f), 0.9f, 0.8f);
			}
		}

		// ---- 얼굴(빛은 왼쪽 위에서): 그늘과 하이라이트를 얼굴 안에만 번지게 해서 둥글게
		Shape face = Face();
		c.FillLinear(face, 60.f, 0.f, skin, 1.f, 112.f, 0.f, Mul(skin, 0.86f), 1.f);
		c.WithClip(face, [&]()
		{
			c.Glow(84.f, 57.f, 36.f, 13.f, shadow, 0.6f);                  // 머리카락이 드리운 이마 그늘
			c.Glow(118.f, 100.f, 18.f, 46.f, shadow, 0.6f);                // 그늘진 오른쪽 볼
			c.Glow(51.f, 100.f, 7.f, 40.f, shadow, 0.3f);                  // 왼쪽 가장자리
			c.Glow(101.f, 117.f, 10.f, 7.f, shadow, 0.25f);                // 광대 아래 그늘
			c.Glow(58.f, 86.f, 6.f, 10.f, shadow, 0.35f);                  // 관자놀이
			c.Glow(110.f, 86.f, 6.f, 10.f, shadow, 0.4f);
			c.Glow(84.f, 140.f, 34.f, 14.f, shadow, 0.45f);                // 턱 그늘
			c.Glow(72.f, 91.5f, 9.f, 4.6f, Mul(skin, 0.72f), 0.35f);       // 눈두덩
			c.Glow(96.f, 91.5f, 9.f, 4.6f, Mul(skin, 0.66f), 0.45f);
			c.Glow(67.f, 105.f, 9.f, 6.f, Mix(skin, kWhite, 0.4f), 0.35f); // 광대 하이라이트
			c.Glow(80.f, 73.f, 15.f, 6.f, Mix(skin, kWhite, 0.35f), 0.3f); // 이마 하이라이트
			c.Glow(116.f, 92.f, 5.f, 30.f, p.glow, 0.4f);                  // 배경빛이 비치는 가장자리
			if (p.blush)
			{
				c.Glow(69.f, 109.f, 7.f, 4.5f, { 0.95f, 0.45f, 0.45f }, 0.4f);
				c.Glow(99.f, 109.f, 7.f, 4.5f, { 0.95f, 0.45f, 0.45f }, 0.35f);
			}
			if (p.gaunt)
			{
				c.Glow(66.f, 113.f, 8.f, 10.f, Mul(skin, 0.5f), 0.55f);
				c.Glow(102.f, 113.f, 8.f, 10.f, Mul(skin, 0.5f), 0.6f);
			}
			if (p.soot)
			{
				c.Glow(70.f, 78.f, 10.f, 5.f, { 0.15f, 0.12f, 0.1f }, 0.35f);
				c.Glow(101.f, 112.f, 6.f, 4.f, { 0.15f, 0.12f, 0.1f }, 0.35f);
			}
			if (p.stone)
			{
				// 돌처럼 갈라진 얼굴(크로노스의 잔향)
				for (int i = 0; i < 6; ++i)
				{
					float x = 60.f + rng.Next() * 48.f;
					float y = 60.f + rng.Next() * 60.f;
					Shape crack;
					crack.M(x, y);
					for (int k = 0; k < 4; ++k)
					{
						x += rng.Next() * 10.f - 5.f;
						y += 4.f + rng.Next() * 6.f;
						crack.L(x, y);
					}
					c.Stroke(crack, Mul(skin, 0.45f), 0.9f, 0.8f);
				}
			}
		});

		DrawEyes(c, p);
		DrawBrows(c, p);
		DrawNose(c, p);
		DrawWrinkles(c, p);
		DrawMouth(c, p);
		DrawBeard(c, p);
		DrawHairFront(c, p);
		DrawHeadgear(c, p);
		DrawProp(c, p);
	}

	// 헤라의 저주로 암소가 된 이오.
	void DrawCow(Canvas& c, const PortraitSpec& p)
	{
		const Rgb white = { 0.96f, 0.95f, 0.9f };
		const Rgb shade = { 0.76f, 0.74f, 0.7f };

		c.FillLinear(Shape().M(56.f, 150.f).C(50.f, 180.f, 42.f, 200.f, 36.f, 212.f).L(132.f, 212.f).C(126.f, 200.f, 118.f, 180.f, 112.f, 150.f).Z(), 0.f, 150.f, white, 1.f, 0.f, 212.f, shade, 1.f);

		for (int side = -1; side <= 1; side += 2)
		{
			float s = (float)side;
			auto X = [s](float x) { return 84.f + s * (x - 84.f); };
			float earAngle = (side < 0) ? kPi + 0.25f : -0.25f;
			c.Fill(Leaf(X(44.f), 88.f, earAngle, 32.f, 8.5f), white);
			c.Fill(Leaf(X(46.f), 88.5f, earAngle, 22.f, 4.f), { 0.92f, 0.7f, 0.72f });
			c.FillLinear(Shape().M(X(64.f), 68.f).C(X(58.f), 58.f, X(56.f), 50.f, X(59.f), 44.f).C(X(63.f), 52.f, X(68.f), 58.f, X(71.f), 66.f).Z(),
				0.f, 44.f, { 0.95f, 0.9f, 0.78f }, 1.f, 0.f, 68.f, { 0.7f, 0.62f, 0.5f }, 1.f);
		}

		Shape head;
		head.M(54.f, 84.f).C(52.f, 58.f, 116.f, 58.f, 114.f, 84.f).C(112.f, 110.f, 106.f, 128.f, 104.f, 146.f).C(100.f, 164.f, 68.f, 164.f, 64.f, 146.f)
			.C(62.f, 128.f, 56.f, 110.f, 54.f, 84.f).Z();
		c.FillLinear(head, 60.f, 0.f, white, 1.f, 114.f, 0.f, shade, 1.f);
		c.WithClip(head, [&]()
		{
			// 그늘진 오른쪽, 빛 받는 이마, 배경빛 가장자리, 주둥이 위 그늘
			c.Glow(120.f, 112.f, 16.f, 50.f, Mul(shade, 0.8f), 0.5f);
			c.Glow(70.f, 84.f, 14.f, 10.f, kWhite, 0.4f);
			c.Glow(117.f, 100.f, 5.f, 34.f, p.glow, 0.35f);
			c.Glow(84.f, 132.f, 20.f, 8.f, Mul(shade, 0.85f), 0.35f);
		});
		c.Fill(Shape().M(70.f, 66.f).C(76.f, 74.f, 92.f, 74.f, 98.f, 66.f).C(92.f, 80.f, 76.f, 80.f, 70.f, 66.f).Z(), { 0.9f, 0.88f, 0.82f }); // 이마 털
		c.FillLinear(Shape().E(84.f, 148.f, 22.f, 15.f), 0.f, 134.f, { 0.96f, 0.78f, 0.76f }, 1.f, 0.f, 163.f, { 0.8f, 0.58f, 0.58f }, 1.f);
		c.Glow(80.f, 143.f, 8.f, 4.f, kWhite, 0.35f);
		c.Fill(Shape().E(76.f, 150.f, 3.6f, 2.6f), { 0.55f, 0.3f, 0.32f });
		c.Fill(Shape().E(92.f, 150.f, 3.6f, 2.6f), { 0.55f, 0.3f, 0.32f });
		c.Stroke(Shape().M(78.f, 157.f).C(82.f, 159.f, 86.f, 159.f, 90.f, 157.f), { 0.6f, 0.38f, 0.38f }, 1.2f);

		const float eyes[2] = { 69.f, 99.f };
		for (float cx : eyes)
		{
			c.Fill(Shape().E(cx, 100.f, 6.4f, 6.8f), { 0.12f, 0.08f, 0.08f });
			c.Fill(Shape().E(cx - 1.6f, 98.f, 1.8f, 1.8f), kWhite, 0.9f);
			c.Fill(Shape().E(cx + 2.f, 102.4f, 0.8f, 0.8f), kWhite, 0.5f);
			float out = (cx < 84.f) ? -1.f : 1.f;
			for (int k = 0; k < 3; ++k)
			{
				float kk = (float)k;
				c.Stroke(Shape().M(cx + out * (2.f + kk * 2.f), 93.6f + kk * 0.3f).L(cx + out * (3.5f + kk * 2.6f), 90.5f + kk * 0.6f), { 0.15f, 0.1f, 0.1f }, 0.9f);
			}
			c.Stroke(Shape().M(cx - 7.f, 89.f - out * 1.5f).C(cx - 3.f, 87.f, cx + 3.f, 87.f, cx + 7.f, 89.f + out * 1.5f), { 0.62f, 0.6f, 0.56f }, 1.4f, 0.7f);
		}

		c.Fill(Shape().M(101.f, 108.f).C(103.f, 113.f, 103.f, 116.f, 101.f, 117.f).C(99.f, 116.f, 99.f, 113.f, 101.f, 108.f).Z(), { 0.7f, 0.85f, 1.f }, 0.85f); // 눈물
	}

	void PaintPortrait(Canvas& c, const std::string& id, const PortraitSpec& p)
	{
		Rng rng(id);

		DrawBackground(c, p, rng);

		if (p.special == Special::Cow)
		{
			DrawCow(c, p);
		}
		else if (p.special == Special::Trio)
		{
			// 운명의 세 자매: 양옆에 클로토·라케시스, 가운데 앞에 아트로포스.
			const PortraitSpec sisters[3] = { Clotho(), Lachesis(), Atropos() };
			const float placements[3][2] = { { -50.f, 30.f }, { 50.f, 30.f }, { 0.f, 22.f } };
			for (int i = 0; i < 3; ++i)
			{
				PortraitSpec sister = sisters[i];
				sister.prop = Prop::None;
				c.WithTransform(placements[i][0], placements[i][1], 84.f, 104.f, 0.78f, [&]() { DrawBust(c, sister, rng); });
			}
		}
		else if (p.special == Special::Titan)
		{
			c.WithTransform(0.f, 6.f, 84.f, 104.f, 1.24f, [&]() { DrawBust(c, p, rng); });
		}
		else
		{
			if (p.special == Special::Voices)
			{
				// 세 겹의 목소리: 그녀 뒤로 어긋난 얼굴 둘이 겹쳐 보인다.
				const float offsets[2] = { -12.f, 12.f };
				for (float dx : offsets)
				{
					c.WithTransform(dx, -3.f, 84.f, 96.f, 1.f, [&]()
					{
						c.Fill(Face(), p.eyeColor, 0.32f);
						c.Glow(84.f, 96.f, 22.f, 8.f, p.eyeColor, 0.35f);
					});
				}
			}
			DrawBust(c, p, rng);
		}

		if (p.faded)
		{
			// 잊혀 가는 신: 잿빛 안개를 덮어 바랜 그림으로.
			c.Fill(RectShape(0.f, 0.f, 168.f, 208.f), { 0.5f, 0.5f, 0.52f }, 0.5f);
		}

		DrawVignette(c);
		DrawFrame(c);
	}

	// ------------------------------------------------------------------ 파일·텍스처

	std::wstring OverridePath(const std::string& speakerId)
	{
		return L"./Portraits/" + std::wstring(speakerId.begin(), speakerId.end()) + L".png";
	}

	bool FileExists(const std::wstring& path)
	{
		DWORD attributes = GetFileAttributesW(path.c_str());
		return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
	}

	// 그림 파일을 초상화 크기에 꽉 차게(비율을 지키고 넘치는 쪽은 잘라) 그린다.
	bool DrawImageFile(Gdiplus::Graphics& graphics, const std::wstring& path)
	{
		Gdiplus::Bitmap source(path.c_str());
		if (source.GetLastStatus() != Gdiplus::Ok || source.GetWidth() == 0 || source.GetHeight() == 0)
		{
			return false;
		}

		float sourceWidth = (float)source.GetWidth();
		float sourceHeight = (float)source.GetHeight();
		float scaleX = (float)kTexWidth / sourceWidth;
		float scaleY = (float)kTexHeight / sourceHeight;
		float scale = (scaleX > scaleY) ? scaleX : scaleY;
		float width = sourceWidth * scale;
		float height = sourceHeight * scale;

		graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
		graphics.DrawImage(&source, Gdiplus::RectF(((float)kTexWidth - width) * 0.5f, ((float)kTexHeight - height) * 0.5f, width, height));
		return true;
	}

	// 필름 그레인: 픽셀마다 정해진 작은 밝기 흔들림(-5~5, 0~255 기준). 그림이 매끈한 벡터처럼 보이지 않고 칠한 그림처럼 보이게.
	float GrainNoise(int x, int y)
	{
		unsigned int hash = ((unsigned int)x * 73856093u) ^ ((unsigned int)y * 19349663u);
		hash ^= hash >> 13;
		hash *= 1274126177u;
		hash ^= hash >> 16;
		return ((float)(hash & 0xFFu) / 255.f * 2.f - 1.f) * 5.f;
	}

	unsigned char AddGrain(unsigned char value, float grain)
	{
		float result = (float)value + grain + 0.5f;
		return (unsigned char)((result < 0.f) ? 0.f : ((result > 255.f) ? 255.f : result));
	}

	// GDI+ 비트맵(BGRA)을 RGBA로 옮겨 OpenGL 텍스처로 올린다. grain이면 필름 그레인을 얹는다(그린 초상화만 — 그림 파일은 그대로).
	TextTexture UploadBitmap(Gdiplus::Bitmap& bitmap, bool grain)
	{
		TextTexture texture;

		Gdiplus::Rect rect(0, 0, kTexWidth, kTexHeight);
		Gdiplus::BitmapData data;
		if (bitmap.LockBits(&rect, Gdiplus::ImageLockModeRead, PixelFormat32bppARGB, &data) != Gdiplus::Ok)
		{
			return texture;
		}

		std::vector<unsigned char> pixels((size_t)kTexWidth * (size_t)kTexHeight * 4);
		for (int y = 0; y < kTexHeight; ++y)
		{
			const unsigned char* row = static_cast<const unsigned char*>(data.Scan0) + (std::ptrdiff_t)y * data.Stride;
			unsigned char* out = pixels.data() + (size_t)y * (size_t)kTexWidth * 4;
			for (int x = 0; x < kTexWidth; ++x)
			{
				float noise = grain ? GrainNoise(x, y) : 0.f;
				out[x * 4 + 0] = AddGrain(row[x * 4 + 2], noise);
				out[x * 4 + 1] = AddGrain(row[x * 4 + 1], noise);
				out[x * 4 + 2] = AddGrain(row[x * 4 + 0], noise);
				out[x * 4 + 3] = row[x * 4 + 3];
			}
		}
		bitmap.UnlockBits(&data);

		GLuint id = 0;
		glGenTextures(1, &id);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, id);
		glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, kTexWidth, kTexHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glBindTexture(GL_TEXTURE_2D, 0);

		// 크기는 화면에 그리는 크기(텍스처는 그 두 배 해상도).
		texture.id = id;
		texture.width = PortraitLibrary::kWidth;
		texture.height = PortraitLibrary::kHeight;
		return texture;
	}
}

PortraitLibrary::PortraitLibrary()
{
}

PortraitLibrary::~PortraitLibrary()
{
	for (std::pair<const std::string, TextTexture>& entry : m_Textures)
	{
		TextRasterizer::Destroy(entry.second);
	}
	m_Textures.clear();

	if (m_GdiplusStarted)
	{
		Gdiplus::GdiplusShutdown((ULONG_PTR)m_GdiplusToken);
	}
}

bool PortraitLibrary::StartGdiplus()
{
	if (m_GdiplusStarted)
	{
		return true;
	}
	if (m_GdiplusFailed)
	{
		return false;
	}

	Gdiplus::GdiplusStartupInput input;
	ULONG_PTR token = 0;
	if (Gdiplus::GdiplusStartup(&token, &input, nullptr) != Gdiplus::Ok)
	{
		std::cout << "[초상화] GDI+를 시작하지 못했습니다. 초상화 없이 대화합니다.\n";
		m_GdiplusFailed = true;
		return false;
	}

	m_GdiplusToken = (std::uintptr_t)token;
	m_GdiplusStarted = true;
	return true;
}

bool PortraitLibrary::Has(const std::string& speakerId)
{
	if (speakerId.empty())
	{
		return false;
	}

	std::map<std::string, bool>::iterator found = m_Has.find(speakerId);
	if (found != m_Has.end())
	{
		return found->second;
	}

	PortraitSpec spec;
	bool has = FindSpec(speakerId, spec) || FileExists(OverridePath(speakerId));
	m_Has[speakerId] = has;
	return has;
}

const TextTexture* PortraitLibrary::Get(const std::string& speakerId)
{
	std::map<std::string, TextTexture>::iterator found = m_Textures.find(speakerId);
	if (found != m_Textures.end())
	{
		return (found->second.id != 0) ? &found->second : nullptr;
	}

	TextTexture texture;
	if (Has(speakerId) && StartGdiplus())
	{
		Gdiplus::Bitmap bitmap(kTexWidth, kTexHeight, PixelFormat32bppARGB);
		bool drawn = false;
		bool painted = false; // 그림 파일이 아니라 표대로 그렸는지(그레인을 얹는다)

		{
			// Graphics는 비트맵 픽셀을 읽기(LockBits) 전에 닫는다.
			Gdiplus::Graphics graphics(&bitmap);
			graphics.Clear(Gdiplus::Color(255, 0, 0, 0));

			// 그림 파일이 있으면 그것을, 없으면 표대로 그린다.
			std::wstring overridePath = OverridePath(speakerId);
			if (FileExists(overridePath))
			{
				drawn = DrawImageFile(graphics, overridePath);
				std::cout << "[초상화] " << speakerId << (drawn ? ": ./Portraits의 그림을 씁니다.\n" : ": ./Portraits의 그림을 읽지 못해 기본 초상화를 그립니다.\n");
			}

			PortraitSpec spec;
			if (!drawn && FindSpec(speakerId, spec))
			{
				graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
				graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);
				graphics.SetCompositingQuality(Gdiplus::CompositingQualityHighQuality);
				graphics.ScaleTransform((float)kTexWidth / kPaintWidth, (float)kTexHeight / kPaintHeight);

				Canvas canvas(graphics);
				PaintPortrait(canvas, speakerId, spec);
				drawn = true;
				painted = true;
			}

			graphics.Flush(Gdiplus::FlushIntentionSync);
		}

		if (drawn)
		{
			texture = UploadBitmap(bitmap, painted);
		}
	}

	TextTexture& stored = m_Textures[speakerId];
	stored = texture;
	return (stored.id != 0) ? &stored : nullptr;
}
