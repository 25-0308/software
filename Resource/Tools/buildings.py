# -*- coding: utf-8 -*-
# 건축 키트 + 생활도구 — 2 m 격자에 맞춰 레고처럼 이어 붙이는 조각들.
#  - 단위 1 m, 벽 한 칸 = 폭 2 m · 높이 2.4 m · 두께 0.25 m, 벽 앞면 = -Y, 기준점 = 바닥 가운데
#  - 모서리 기둥(Wall_Corner)은 벽 두 칸이 만나는 꼭짓점에 세움
#  - 재질은 팔레트 이름 그대로(회벽 Plaster, 기와 RoofTile, 대리석 Marble …)
import bpy
import math
import random
from mathutils import Vector, Matrix
from rescommon import (MeshBuilder, Mt, V, FrameMatrix, ResetScene, GetCollection, PaletteMaterial, ExportGlb, SaveBlend,
	WriteJson, ResPath, Log)

OUT = "Buildings"
WALL_W, WALL_H, WALL_T = 2.0, 2.4, 0.25

KIT = []


def Piece(name, category, desc, size, snap="바닥 가운데"):
	def Deco(fn):
		KIT.append(dict(name=name, category=category, desc=desc, size=size, snap=snap, fn=fn))
		return fn
	return Deco


# ───────────────────────── 집 ─────────────────────────

def _WallBase(mb, x0, x1, z0, z1, plinth=True):
	"""x0~x1, z0~z1 구간의 회벽 덩어리(아래 0.3 m는 돌 기단)."""
	if x1 - x0 < 1e-4 or z1 - z0 < 1e-4:
		return
	if plinth and z0 < 0.3:
		mb.Box(((x0 + x1) / 2, 0, min(z1, 0.3) / 2), (x1 - x0, WALL_T + 0.04, min(z1, 0.3)), "StoneGray")
		z0 = 0.3
	if z1 > z0:
		mb.Box(((x0 + x1) / 2, 0, (z0 + z1) / 2), (x1 - x0, WALL_T, z1 - z0), "Plaster")


def _WallTop(mb):
	mb.Box((0, 0, WALL_H - 0.06), (WALL_W, WALL_T + 0.05, 0.12), "Plaster")


@Piece("Wall_Plain", "House", "회벽 한 칸(돌 기단 + 윗단 몰딩)", (2.0, 0.25, 2.4))
def _(mb):
	_WallBase(mb, -1, 1, 0, WALL_H)
	_WallTop(mb)


@Piece("Wall_Door", "House", "문이 난 벽(나무 문틀 + 푸른 문)", (2.0, 0.25, 2.4))
def _(mb):
	w, h = 0.9, 1.95
	_WallBase(mb, -1, -w / 2, 0, WALL_H)
	_WallBase(mb, w / 2, 1, 0, WALL_H)
	_WallBase(mb, -w / 2, w / 2, h, WALL_H, plinth=False)
	_WallTop(mb)
	for s in (1, -1):
		mb.Box((s * (w / 2 + 0.05), -0.02, h / 2), (0.1, WALL_T + 0.06, h), "WoodDark")
	mb.Box((0, -0.02, h + 0.06), (w + 0.2, WALL_T + 0.08, 0.12), "WoodDark")
	mb.Box((0, 0.04, h / 2), (w, 0.06, h), "BlueDoor")
	for z in (0.5, 1.4):
		mb.Box((0, -0.005, z), (w - 0.1, 0.03, 0.06), "BlueShutter")
	mb.Ico((0.32, -0.02, 1.0), 0.035, "Bronze", subdiv=1)
	mb.Box((0, -0.25, 0.05), (1.1, 0.3, 0.1), "StoneGray")


def _Window(mb, lit=False, cx=0.0, wz=1.3, w=0.6, h=0.65):
	_WallBase(mb, -1, cx - w / 2, 0, WALL_H)
	_WallBase(mb, cx + w / 2, 1, 0, WALL_H)
	_WallBase(mb, cx - w / 2, cx + w / 2, 0, wz - h / 2)
	_WallBase(mb, cx - w / 2, cx + w / 2, wz + h / 2, WALL_H, plinth=False)
	mb.Box((cx, 0.05, wz), (w, 0.04, h), "WindowLit" if lit else "Shadow")
	mb.Box((cx, -0.04, wz - h / 2 - 0.03), (w + 0.18, WALL_T + 0.1, 0.06), "Limestone")
	mb.Box((cx, -0.03, wz + h / 2 + 0.04), (w + 0.12, WALL_T + 0.06, 0.08), "WoodDark")
	for s in (1, -1):
		mb.Box((cx + s * (w / 2 + w / 4 + 0.02), -0.15, wz), (w / 2, 0.04, h), "BlueShutter")
		for k in range(3):
			mb.Box((cx + s * (w / 2 + w / 4 + 0.02), -0.175, wz - h / 3 + k * h / 3), (w / 2 - 0.06, 0.012, 0.03), "BlueDoor")


@Piece("Wall_Window", "House", "덧창 달린 창문 벽", (2.0, 0.25, 2.4))
def _(mb):
	_Window(mb)
	_WallTop(mb)


@Piece("Wall_WindowLit", "House", "불 켜진 창문 벽(창이 빛남)", (2.0, 0.25, 2.4))
def _(mb):
	_Window(mb, lit=True)
	_WallTop(mb)


@Piece("Wall_Half", "House", "마당 낮은 담(1.2 m)", (2.0, 0.3, 1.2))
def _(mb):
	_WallBase(mb, -1, 1, 0, 1.15)
	mb.Box((0, 0, 1.17), (2.0, 0.34, 0.08), "Limestone")


@Piece("Wall_Corner", "House", "모서리 기둥(벽 두 칸이 만나는 곳)", (0.35, 0.35, 2.45), snap="바닥 가운데 = 벽 끝 꼭짓점")
def _(mb):
	mb.Box((0, 0, 0.15), (0.4, 0.4, 0.3), "StoneGray")
	mb.Box((0, 0, 1.35), (0.34, 0.34, 2.1), "Plaster")
	mb.Box((0, 0, 2.42), (0.38, 0.38, 0.06), "Plaster")


@Piece("Floor_Stone", "House", "돌 바닥판 2×2", (2.0, 2.0, 0.12))
def _(mb):
	rnd = random.Random(3)
	for i in range(2):
		for j in range(2):
			h = 0.1 + rnd.uniform(-0.01, 0.01)
			mb.Box((-0.5 + i, -0.5 + j, h / 2), (0.97, 0.97, h), "Limestone" if (i + j) % 2 == 0 else "StoneGray")


ROOF_SPAN = 4.0        # 지붕이 덮는 집 깊이(Y)
ROOF_PITCH = 22.0
ROOF_OVER = 0.35


def _RoofSlope(mb, side, x0, x1):
	half = ROOF_SPAN / 2 + ROOF_OVER
	rise = (ROOF_SPAN / 2) * math.tan(math.radians(ROOF_PITCH))
	ang = math.radians(ROOF_PITCH)
	L = half / math.cos(ang)
	cy = side * half / 2
	cz = WALL_H + rise - (half / 2) * math.tan(ang) + 0.05
	M = Matrix.Translation(V((x0 + x1) / 2, cy, cz)) @ Matrix.Rotation(-side * ang, 4, 'X')
	mb.Box((0, 0, 0), (x1 - x0, L, 0.1), "RoofTile", M)
	n = int(round((x1 - x0) / 0.33))
	for k in range(n):
		x = x0 + (k + 0.5) * (x1 - x0) / n - (x0 + x1) / 2
		mb.Cyl(M @ V(x, L / 2, 0.065), M @ V(x, -L / 2, 0.065), 0.045, 0.045, 5, "RoofTileDark", up=(0, 0, 1))
	return rise


@Piece("Roof_Gable", "House", "박공지붕 한 칸(용마루가 X 방향, 깊이 4 m 집용, 기와 골)", (2.0, 4.7, 1.0), snap="바닥 가운데(형상은 벽 높이 2.4 m 위에 미리 올려 둠 — 집 가운데 줄에 그대로 둠)")
def _(mb):
	for side in (1, -1):
		rise = _RoofSlope(mb, side, -1.0, 1.0)
	mb.Cyl((-1.0, 0, WALL_H + rise + 0.1), (1.0, 0, WALL_H + rise + 0.1), 0.08, 0.08, 6, "RoofTileDark", up=(0, 0, 1))


@Piece("Roof_GableEnd", "House", "박공벽(지붕 양 끝 삼각 벽)", (4.5, 0.25, 0.95), snap="바닥 가운데(벽 높이 2.4 m 위로 미리 올려 둠 — 옆벽 줄의 가운데에 90도 돌려 둠)")
def _(mb):
	rise = (ROOF_SPAN / 2) * math.tan(math.radians(ROOF_PITCH))
	half = ROOF_SPAN / 2 + WALL_T / 2
	M = FrameMatrix(V(0, 0, WALL_H), V(1, 0, 0), V(0, 0, 1), V(0, 1, 0))
	mb.Prism([(-half, 0), (half, 0), (0, rise + 0.02)], WALL_T, "Plaster", M)
	mb.Ico((0, -0.14, WALL_H + rise * 0.45), 0.09, "TemplePaint", subdiv=1)


@Piece("Roof_Flat", "House", "평지붕 2×2(난간) — 섬마을 지붕", (2.0, 2.0, 0.45), snap="바닥 가운데(벽 높이 2.4 m 위로 미리 올려 둠)")
def _(mb):
	mb.Box((0, 0, WALL_H + 0.08), (2.1, 2.1, 0.16), "Plaster")
	for s in (1, -1):
		mb.Box((s * 1.0, 0, WALL_H + 0.3), (0.12, 2.1, 0.3), "Plaster")
		mb.Box((0, s * 1.0, WALL_H + 0.3), (2.1, 0.12, 0.3), "Plaster")


@Piece("Stairs_Outside", "House", "벽에 붙인 바깥 돌계단(2.4 m 올라감)", (1.0, 3.0, 2.4))
def _(mb):
	n = 12
	for k in range(n):
		mb.Box((0, -1.5 + (k + 0.5) * 3.0 / n, (k + 1) * WALL_H / n / 2), (1.0, 3.0 / n, (k + 1) * WALL_H / n), "Plaster")
	slope = math.atan2(WALL_H, 3.0)
	M = Matrix.Translation(V(0.47, 0.0, WALL_H / 2 + 0.12)) @ Matrix.Rotation(slope, 4, 'X')
	mb.Box((0, 0, 0), (0.08, math.hypot(3.0, WALL_H), 0.12), "Plaster", M)


@Piece("Awning", "House", "줄무늬 천 차양 2 m(문 위·가게 앞)", (2.0, 1.2, 0.5), snap="벽 앞면, 높이 2.1")
def _(mb):
	rows = []
	for (y, z) in ((0.0, 2.2), (-0.6, 2.05), (-1.15, 1.85)):
		rows.append([V(-1.0 + 2.0 * i / 8, y, z + 0.03 * math.sin(i * 1.3)) for i in range(9)])
	start = mb.FaceCount()
	mb.Sheet(rows, 0.02, "Canvas")
	mb.Paint(start, lambda c: "CanvasStripe" if int((c.x + 1.0) / 0.25) % 2 == 0 else None)
	for s in (1, -1):
		mb.Cyl((s * 0.98, -1.15, 0.0), (s * 0.98, -1.15, 1.86), 0.03, 0.03, 6, "Wood")


@Piece("Pergola", "House", "포도 덩굴 그늘막 2×2(나무 기둥 + 덩굴)", (2.0, 2.0, 2.5))
def _(mb):
	for sx in (1, -1):
		for sy in (1, -1):
			mb.Cyl((sx * 0.9, sy * 0.9, 0), (sx * 0.9, sy * 0.9, 2.3), 0.06, 0.05, 6, "Wood")
		mb.Box((sx * 0.9, 0, 2.35), (0.1, 2.2, 0.1), "WoodDark")
	for k in range(5):
		mb.Box((0, -0.9 + k * 0.45, 2.45), (2.2, 0.07, 0.07), "Wood")
	rnd = random.Random(5)
	for k in range(16):
		p = V(rnd.uniform(-0.9, 0.9), rnd.uniform(-0.9, 0.9), 2.55)
		mb.Ico(p, rnd.uniform(0.18, 0.28), "Leaf" if k % 3 else "LeafDark", subdiv=1, jitter=0.25, seed=k, scale=(1, 1, 0.5))
	for k in range(4):
		p = V(rnd.uniform(-0.8, 0.8), rnd.uniform(-0.8, 0.8), 2.38)
		for g in range(4):
			mb.Ico(p + V(0.03 * (g % 2), 0.0, -0.05 * (g // 2)), 0.035, "Grape", subdiv=1)


@Piece("Fence_Wood", "House", "나무 울타리 2 m", (2.0, 0.1, 1.0))
def _(mb):
	for x in (-0.95, 0.0, 0.95):
		mb.Cyl((x, 0, 0), (x, 0, 1.0), 0.05, 0.045, 6, "Wood")
	for z in (0.4, 0.8):
		mb.Box((0, 0, z), (2.0, 0.05, 0.1), "WoodLight")


@Piece("Wall_DryStone", "House", "막돌 담(밭·목장 경계) 2 m", (2.0, 0.6, 0.9))
def _(mb):
	rnd = random.Random(7)
	for row in range(3):
		x = -1.0
		while x < 1.0:
			w = rnd.uniform(0.28, 0.45)
			w = min(w, 1.0 - x)
			mb.Ico((x + w / 2, rnd.uniform(-0.04, 0.04), 0.15 + row * 0.28), 1.0, "StoneGray" if rnd.random() > 0.3 else "Rock",
				subdiv=1, jitter=0.15, seed=row * 50 + int(x * 10), scale=(w / 2, 0.26 - row * 0.03, 0.15))
			x += w


@Piece("Gate_Wood", "House", "나무 대문(문틀 포함) 2 m", (2.0, 0.3, 2.3))
def _(mb):
	for s in (1, -1):
		mb.Box((s * 0.95, 0, 1.15), (0.2, 0.3, 2.3), "StoneGray")
		mb.Box((s * 0.43, 0, 0.95), (0.84, 0.06, 1.7), "Wood")
		for z in (0.4, 1.5):
			mb.Box((s * 0.43, -0.04, z), (0.8, 0.04, 0.1), "WoodDark")
	mb.Box((0, 0, 2.2), (2.1, 0.34, 0.2), "Limestone")


# ───────────────────────── 신전 ─────────────────────────

def _Fluted(mb, z0, z1, r0, r1, n=20, role="Marble"):
	rings = []
	for (z, r) in ((z0, r0), (z1, r1)):
		ring = []
		for k in range(n):
			a = 2 * math.pi * k / n
			rr = r * (1.0 if k % 2 == 0 else 0.93)
			ring.append(V(math.cos(a) * rr, math.sin(a) * rr, z))
		rings.append(ring)
	mid = []
	for k in range(n):
		a = 2 * math.pi * k / n
		r = (r0 + r1) / 2 * 1.02 * (1.0 if k % 2 == 0 else 0.93)
		mid.append(V(math.cos(a) * r, math.sin(a) * r, (z0 + z1) / 2))
	mb.Loft([rings[0], mid, rings[1]], role)


COLUMN_H = 3.6


@Piece("Temple_Step", "Temple", "신전 기단 블록 2×2(대리석, 0.3 m)", (2.0, 2.0, 0.3))
def _(mb):
	mb.Box((0, 0, 0.15), (2.0, 2.0, 0.3), "Marble")
	mb.Box((0, -0.99, 0.15), (2.0, 0.02, 0.26), "MarbleShade")


@Piece("Column_Doric", "Temple", "도리스식 기둥(홈 파인 기둥 + 단순한 주두) 3.6 m", (0.8, 0.8, 3.6))
def _(mb):
	_Fluted(mb, 0, COLUMN_H - 0.32, 0.32, 0.26)
	mb.Lathe([(0.0, COLUMN_H - 0.33), (0.27, COLUMN_H - 0.33), (0.38, COLUMN_H - 0.2), (0.38, COLUMN_H - 0.14), (0.0, COLUMN_H - 0.14)], 16, "Marble")
	mb.Box((0, 0, COLUMN_H - 0.07), (0.8, 0.8, 0.14), "Marble")


@Piece("Column_Ionic", "Temple", "이오니아식 기둥(소용돌이 주두 + 받침) 3.6 m", (0.9, 0.7, 3.6))
def _(mb):
	mb.Lathe([(0.0, 0.0), (0.38, 0.0), (0.38, 0.08), (0.32, 0.12), (0.34, 0.2), (0.0, 0.2)], 16, "Marble")
	_Fluted(mb, 0.2, COLUMN_H - 0.3, 0.29, 0.25, n=24)
	mb.Box((0, 0, COLUMN_H - 0.22), (0.78, 0.5, 0.14), "Marble")
	for s in (1, -1):
		mb.Cyl((s * 0.36, -0.27, COLUMN_H - 0.26), (s * 0.36, 0.27, COLUMN_H - 0.26), 0.12, 0.12, 10, "Marble", up=(0, 0, 1))
		mb.Cyl((s * 0.36, -0.275, COLUMN_H - 0.26), (s * 0.36, -0.28, COLUMN_H - 0.26), 0.05, 0.05, 8, "TempleBlue", up=(0, 0, 1))
	mb.Box((0, 0, COLUMN_H - 0.06), (0.86, 0.6, 0.12), "Marble")


@Piece("Column_Corinthian", "Temple", "코린트식 기둥(로마 신전, 잎 장식 주두) 3.6 m", (0.8, 0.8, 3.6))
def _(mb):
	mb.Lathe([(0.0, 0.0), (0.4, 0.0), (0.4, 0.1), (0.33, 0.14), (0.35, 0.22), (0.0, 0.22)], 16, "Marble")
	_Fluted(mb, 0.22, COLUMN_H - 0.5, 0.28, 0.25, n=24)
	mb.Lathe([(0.0, COLUMN_H - 0.5), (0.25, COLUMN_H - 0.5), (0.3, COLUMN_H - 0.25), (0.36, COLUMN_H - 0.14), (0.0, COLUMN_H - 0.14)], 12, "Marble")
	for k in range(8):
		a = 2 * math.pi * k / 8
		p = V(math.cos(a) * 0.27, math.sin(a) * 0.27, COLUMN_H - 0.38)
		mb.Ellipsoid(p, (0.06, 0.06, 0.13), 5, 3, "MarbleShade")
	mb.Box((0, 0, COLUMN_H - 0.07), (0.82, 0.82, 0.14), "Marble")


@Piece("Column_Broken", "Temple", "부러진 기둥 + 굴러떨어진 기둥 토막", (2.0, 1.4, 1.7))
def _(mb):
	_Fluted(mb, 0, 1.55, 0.32, 0.30)
	rnd = random.Random(2)
	for k in range(7):
		a = 2 * math.pi * k / 7
		mb.Cyl((math.cos(a) * 0.2, math.sin(a) * 0.2, 1.55), (math.cos(a) * 0.18, math.sin(a) * 0.18, 1.55 + rnd.uniform(0.03, 0.18)), 0.1, 0.05, 4, "Marble")
	sub = MeshBuilder()
	_Fluted(sub, 0, 0.8, 0.3, 0.29)
	mb.Merge(sub, Mt((1.0, 0.3, 0.3), (90, 0, 20)))
	sub2 = MeshBuilder()
	_Fluted(sub2, 0, 0.5, 0.29, 0.28)
	mb.Merge(sub2, Mt((0.6, -0.55, 0.28), (85, 0, -50)))


@Piece("Entablature", "Temple", "엔타블러처 2 m(아키트레이브 + 트리글리프 프리즈 + 코니스)", (2.0, 0.9, 1.15), snap="기둥 윗면 높이 0")
def _(mb):
	z = 0.0
	mb.Box((0, 0, z + 0.25), (2.0, 0.7, 0.5), "Marble")
	mb.Box((0, -0.36, z + 0.47), (2.0, 0.04, 0.05), "TemplePaint")
	mb.Box((0, 0, z + 0.78), (2.0, 0.66, 0.56), "Marble")
	for k in range(4):
		x = -0.75 + k * 0.5
		mb.Box((x, -0.34, z + 0.78), (0.2, 0.04, 0.5), "TempleBlue")
		for g in (-0.05, 0.05):
			mb.Box((x + g, -0.365, z + 0.78), (0.025, 0.02, 0.46), "Shadow")
	mb.Box((0, -0.04, z + 1.1), (2.0, 0.86, 0.12), "Marble")
	mb.Box((0, -0.47, z + 1.05), (2.0, 0.03, 0.04), "TemplePaint")


@Piece("Pediment", "Temple", "박공(삼각 페디먼트) 6 m — 신전 정면 위", (6.4, 0.9, 1.3), snap="엔타블러처 윗면")
def _(mb):
	M = FrameMatrix(V(0, 0, 0), V(1, 0, 0), V(0, 0, 1), V(0, 1, 0))
	mb.Prism([(-3.2, 0), (3.2, 0), (0, 1.25)], 0.5, "Marble", M)
	mb.Prism([(-2.8, 0.1), (2.8, 0.1), (0, 1.05)], 0.06, "TempleBlue", FrameMatrix(V(0, -0.26, 0), V(1, 0, 0), V(0, 0, 1), V(0, 1, 0)))
	ang = math.atan2(1.25, 3.2)
	L = math.hypot(3.2, 1.25)
	for s in (1, -1):
		Mr = Matrix.Translation(V(s * 1.6, -0.05, 0.66)) @ Matrix.Rotation(s * ang, 4, 'Y')
		mb.Box((0, 0, 0), (L + 0.2, 0.9, 0.14), "Marble", Mr)
	for (x, z, r) in ((-1.4, 0.4, 0.14), (0.0, 0.62, 0.2), (1.4, 0.4, 0.14)):
		mb.Ellipsoid((x, -0.3, z), (r * 0.6, 0.06, r), 6, 4, "Marble")
	mb.Ico((0, -0.1, 1.38), 0.1, "Gold", subdiv=1)


@Piece("Temple_Roof", "Temple", "신전 지붕 한 칸 2 m(폭 6.4 m, 대리석 기와)", (2.0, 6.6, 1.3), snap="엔타블러처 윗면")
def _(mb):
	ang = math.atan2(1.25, 3.2)
	L = math.hypot(3.2, 1.25) + 0.15
	for s in (1, -1):
		M = Matrix.Translation(V(0, s * 1.6, 0.66)) @ Matrix.Rotation(-s * ang, 4, 'X')
		mb.Box((0, 0, 0), (2.0, L, 0.14), "MarbleShade", M)
		for k in range(6):
			x = -0.83 + k * 0.333
			mb.Cyl(M @ V(x, L / 2, 0.09), M @ V(x, -L / 2, 0.09), 0.05, 0.05, 5, "Marble", up=(0, 0, 1))
	mb.Cyl((-1.0, 0, 1.3), (1.0, 0, 1.3), 0.09, 0.09, 6, "Marble", up=(0, 0, 1))


@Piece("Temple_Wall", "Temple", "신전 내실(켈라) 벽 2 m — 대리석 마름돌", (2.0, 0.5, 3.6))
def _(mb):
	for row in range(6):
		off = 0.5 if row % 2 else 0.0
		x = -1.0
		k = 0
		while x < 1.0 - 1e-6:
			w = min(1.0 - (off if k == 0 else 0), 1.0 - x)
			mb.Box((x + w / 2, 0, row * 0.6 + 0.3), (w - 0.02, 0.5, 0.58), "Marble" if (row + k) % 3 else "MarbleShade")
			x += w
			k += 1


@Piece("Temple_Door", "Temple", "청동 신전 문(문틀 포함) 2 m", (2.0, 0.5, 3.6))
def _(mb):
	for s in (1, -1):
		mb.Box((s * 0.85, 0, 1.8), (0.3, 0.5, 3.6), "Marble")
	mb.Box((0, 0, 3.3), (2.0, 0.5, 0.6), "Marble")
	mb.Box((0, -0.3, 3.03), (1.6, 0.12, 0.1), "TemplePaint")
	for s in (1, -1):
		mb.Box((s * 0.35, 0.05, 1.5), (0.68, 0.08, 3.0), "Bronze")
		for z in (0.6, 1.5, 2.4):
			mb.Ico((s * 0.35, -0.0, z), 0.05, "Gold", subdiv=1)


@Piece("Roman_Stairs", "Temple", "로마식 정면 계단 2 m 폭(1.5 m 올라감)", (2.0, 2.4, 1.5))
def _(mb):
	n = 8
	for k in range(n):
		mb.Box((0, -1.2 + (k + 0.5) * 2.4 / n, (k + 1) * 1.5 / n / 2), (2.0, 2.4 / n, (k + 1) * 1.5 / n), "Marble" if k % 2 else "MarbleShade")


@Piece("Inscription_Gold", "Temple", "금빛 비문 띠(로마 신전 정면 'IVNO')", (4.0, 0.1, 0.5), snap="엔타블러처 앞면")
def _(mb):
	mb.Box((0, 0, 0.25), (4.0, 0.06, 0.5), "TemplePaint")
	letters = {"I": [(0, 0, 0, 1)], "V": [(-0.5, 1, 0, 0), (0, 0, 0.5, 1)], "N": [(-0.5, 0, -0.5, 1), (-0.5, 1, 0.5, 0), (0.5, 0, 0.5, 1)],
		"O": [(-0.5, 0, -0.5, 1), (0.5, 0, 0.5, 1), (-0.5, 0, 0.5, 0), (-0.5, 1, 0.5, 1)]}
	x = -0.9
	for ch in "IVNO":
		for (x0, y0, x1, y1) in letters[ch]:
			p0 = V(x + x0 * 0.18, -0.04, 0.08 + y0 * 0.34)
			p1 = V(x + x1 * 0.18, -0.04, 0.08 + y1 * 0.34)
			mb.Cyl(p0, p1, 0.028, 0.028, 4, "Gold", up=(0, 1, 0))
		x += 0.6


@Piece("Altar", "Temple", "돌 제단 + 청동 불씨 그릇", (1.4, 1.0, 1.3))
def _(mb):
	mb.Box((0, 0, 0.05), (1.5, 1.1, 0.1), "StoneGray")
	mb.Box((0, 0, 0.5), (1.2, 0.8, 0.8), "Limestone")
	mb.Box((0, 0, 0.94), (1.36, 0.96, 0.1), "Limestone")
	mb.Box((0, -0.41, 0.6), (0.9, 0.02, 0.18), "TemplePaint")
	for s in (1, -1):
		mb.Cyl((s * 0.62, 0.0, 0.99), (s * 0.62, 0.0, 1.06), 0.12, 0.12, 8, "Limestone", up=(0, 1, 0))
	mb.Lathe([(0, 0.99), (0.08, 0.99), (0.06, 1.05), (0.26, 1.16), (0.24, 1.18), (0.0, 1.12)], 10, "Bronze")
	mb.Lathe([(0, 1.12), (0.18, 1.14), (0.1, 1.28), (0.0, 1.38)], 7, "Fire")


@Piece("Tripod", "Temple", "청동 삼발이 솥(델포이의 신탁 삼발이)", (0.8, 0.8, 1.4))
def _(mb):
	for k in range(3):
		a = 2 * math.pi * k / 3
		mb.Cyl((math.cos(a) * 0.36, math.sin(a) * 0.36, 0), (math.cos(a) * 0.22, math.sin(a) * 0.22, 1.0), 0.03, 0.025, 5, "Bronze")
		mb.Cyl((math.cos(a) * 0.3, math.sin(a) * 0.3, 1.25), (math.cos(a) * 0.3, math.sin(a) * 0.3, 1.42), 0.06, 0.06, 8, "Bronze", up=(math.cos(a + 1.57), math.sin(a + 1.57), 0))
	mb.Lathe([(0, 0.95), (0.2, 0.98), (0.32, 1.12), (0.34, 1.26), (0.3, 1.27), (0.0, 1.2)], 12, "Bronze")


@Piece("Statue", "Temple", "받침대 위 대리석 신상(일반 서 있는 신)", (1.0, 1.0, 3.2))
def _(mb):
	mb.Box((0, 0, 0.45), (1.0, 1.0, 0.9), "Limestone")
	mb.Box((0, 0, 0.93), (1.1, 1.1, 0.08), "Limestone")
	mb.Lathe([(0, 0.97), (0.26, 0.97), (0.3, 1.5), (0.24, 1.9), (0.18, 2.3), (0.0, 2.32)], 10, "Marble")
	mb.Ellipsoid((0, -0.02, 2.22), (0.26, 0.17, 0.2), 8, 4, "Marble")
	mb.Ellipsoid((0, -0.01, 2.62), (0.11, 0.13, 0.15), 8, 5, "Marble")
	mb.Cyl((0, 0, 2.3), (0, 0, 2.5), 0.06, 0.06, 6, "Marble")
	mb.Cyl((0.28, -0.05, 2.25), (0.36, -0.3, 1.85), 0.06, 0.05, 6, "Marble")
	mb.Cyl((-0.28, -0.0, 2.25), (-0.36, -0.1, 2.8), 0.06, 0.05, 6, "Marble")
	mb.Cyl((-0.38, -0.12, 1.0), (-0.38, -0.12, 3.15), 0.035, 0.035, 6, "Marble")


@Piece("Statue_Fallen", "Temple", "목이 부러져 쓰러진 신상 + 깨진 받침대(아고라의 유피테르)", (3.4, 1.6, 1.0))
def _(mb):
	mb.Box((-1.2, 0.2, 0.3), (1.0, 1.0, 0.6), "Limestone")
	mb.Box((-1.0, 0.0, 0.66), (0.5, 0.6, 0.15), "Limestone", Mt((0, 0, 0), (0, 12, 15)))
	body = MeshBuilder()
	body.Lathe([(0, 0.0), (0.26, 0.0), (0.3, 0.6), (0.24, 1.0), (0.2, 1.35), (0.0, 1.37)], 10, "Marble")
	body.Ellipsoid((0, -0.02, 1.27), (0.26, 0.17, 0.2), 8, 4, "Marble")
	mb.Merge(body, Mt((-0.2, -0.15, 0.24), (0, -90, 10)))
	mb.Ellipsoid((1.55, -0.45, 0.14), (0.12, 0.13, 0.15), 8, 5, "Marble")
	mb.Ellipsoid((1.55, -0.45, 0.25), (0.1, 0.1, 0.06), 6, 3, "HairWhite")


@Piece("Stele", "Temple", "글씨 새긴 비석(봉인 석판·경계석)", (0.6, 0.25, 1.5))
def _(mb):
	mb.Box((0, 0, 0.1), (0.7, 0.35, 0.2), "StoneGray")
	mb.Frustum((0, 0, 0.2), (0.56, 0.2), (0, 0, 1.3), (0.5, 0.18), "Limestone")
	mb.Prism([(-0.27, 0), (0.27, 0), (0, 0.18)], 0.2, "Limestone", FrameMatrix(V(0, 0, 1.3), V(1, 0, 0), V(0, 0, 1), V(0, 1, 0)))
	for k in range(7):
		mb.Box((0, -0.1, 1.15 - k * 0.12), (0.36 - (k % 3) * 0.05, 0.02, 0.025), "StoneDark")


@Piece("Herm", "Temple", "헤르마 기둥(머리 달린 경계 기둥 — 길가·경계석의 신)", (0.4, 0.4, 1.7))
def _(mb):
	mb.Box((0, 0, 0.06), (0.5, 0.5, 0.12), "StoneGray")
	mb.Frustum((0, 0, 0.12), (0.3, 0.28), (0, 0, 1.3), (0.34, 0.3), "Limestone")
	mb.Ellipsoid((0, 0, 1.47), (0.13, 0.15, 0.18), 8, 5, "Limestone")
	mb.Ellipsoid((0, -0.06, 1.38), (0.11, 0.1, 0.12), 6, 4, "StoneGray")
	for s in (1, -1):
		mb.Box((s * 0.2, 0, 1.15), (0.08, 0.12, 0.08), "Limestone")


@Piece("Brazier", "Temple", "청동 화로대(불이 타는 그릇)", (0.6, 0.6, 1.2))
def _(mb):
	mb.Cyl((0, 0, 0), (0, 0, 0.9), 0.05, 0.04, 6, "Bronze")
	for k in range(3):
		a = 2 * math.pi * k / 3
		mb.Cyl((0, 0, 0.05), (math.cos(a) * 0.3, math.sin(a) * 0.3, 0.0), 0.03, 0.03, 5, "Bronze")
	mb.Lathe([(0, 0.85), (0.08, 0.85), (0.3, 0.98), (0.28, 1.0), (0.0, 0.94)], 10, "Bronze")
	mb.Lathe([(0, 0.95), (0.22, 0.97), (0.12, 1.12), (0.0, 1.25)], 7, "Fire")


# ───────────────────────── 시장·생활도구 ─────────────────────────

def _Pot(mb, profile, n=12, band=None, role="Clay", M=None, handles=None):
	mb.Lathe(profile, n, role, M)
	if band:
		z0, z1, r = band
		mb.Lathe([(0, z0), (r, z0), (r, z1), (0, z1)], n, "BlackFigure", M)
	if handles:
		for s in (1, -1):
			mb.Tube([V(s * p[0], 0, p[1]) for p in handles], [0.018] * len(handles), 5, role, M=M)


@Piece("Amphora", "Goods", "암포라(포도주·기름 항아리, 흑화 띠)", (0.4, 0.4, 0.75))
def _(mb):
	_Pot(mb, [(0, 0.0), (0.05, 0.0), (0.04, 0.06), (0.17, 0.25), (0.2, 0.4), (0.16, 0.55), (0.07, 0.62), (0.06, 0.72), (0.08, 0.74), (0.0, 0.72)],
		band=(0.33, 0.45, 0.205), handles=[(0.07, 0.68), (0.17, 0.66), (0.16, 0.55)])


@Piece("Pithos", "Goods", "큰 저장 항아리 피토스(곡식·기름)", (0.9, 0.9, 1.3))
def _(mb):
	_Pot(mb, [(0, 0.0), (0.2, 0.0), (0.38, 0.3), (0.44, 0.65), (0.38, 1.05), (0.26, 1.2), (0.3, 1.26), (0.0, 1.22)], n=14, role="Clay")
	for z in (0.45, 0.75, 1.0):
		mb.Lathe([(0, z), (0.45 if z < 0.9 else 0.37, z), (0.45 if z < 0.9 else 0.37, z + 0.04), (0, z + 0.04)], 14, "ClayDark")


@Piece("Hydria", "Goods", "물동이 히드리아(손잡이 셋)", (0.4, 0.4, 0.5))
def _(mb):
	_Pot(mb, [(0, 0.0), (0.08, 0.0), (0.18, 0.15), (0.19, 0.28), (0.12, 0.38), (0.06, 0.42), (0.09, 0.48), (0.0, 0.46)],
		band=(0.2, 0.3, 0.195), handles=[(0.17, 0.2), (0.24, 0.27), (0.18, 0.33)])
	mb.Tube([V(0, 0.08, 0.42), V(0, 0.16, 0.42), V(0, 0.18, 0.32)], [0.016] * 3, 5, "Clay")


@Piece("Krater", "Goods", "포도주 섞는 큰 그릇 크라테르", (0.6, 0.6, 0.55))
def _(mb):
	_Pot(mb, [(0, 0.0), (0.12, 0.0), (0.08, 0.08), (0.2, 0.2), (0.26, 0.4), (0.3, 0.52), (0.27, 0.53), (0.0, 0.45)], n=14,
		band=(0.3, 0.42, 0.27), handles=[(0.24, 0.42), (0.34, 0.44), (0.3, 0.5)])
	mb.Lathe([(0, 0.46), (0.24, 0.47), (0.0, 0.47)], 14, "Wine")


@Piece("Kylix", "Goods", "넓은 술잔 킬릭스", (0.3, 0.3, 0.1))
def _(mb):
	_Pot(mb, [(0, 0.0), (0.05, 0.0), (0.015, 0.03), (0.02, 0.05), (0.13, 0.09), (0.12, 0.095), (0.0, 0.07)], band=(0.06, 0.08, 0.1))


@Piece("Oil_Lamp", "Goods", "토기 기름 등잔(심지 불)", (0.15, 0.1, 0.08))
def _(mb):
	mb.Ellipsoid((0, 0, 0.025), (0.06, 0.045, 0.025), 8, 4, "Clay")
	mb.Cyl((0, -0.04, 0.03), (0, -0.08, 0.03), 0.015, 0.012, 6, "Clay")
	mb.Lathe([(0.0, 0.0), (0.012, 0.01), (0.0, 0.04)], 6, "Fire", Mt((0, -0.08, 0.04)))


@Piece("Lamp_Stand", "Goods", "청동 등잔대(키 큰 촛대)", (0.4, 0.4, 1.4))
def _(mb):
	for k in range(3):
		a = 2 * math.pi * k / 3
		mb.Cyl((0, 0, 0.12), (math.cos(a) * 0.18, math.sin(a) * 0.18, 0.0), 0.02, 0.02, 5, "Bronze")
	mb.Cyl((0, 0, 0.0), (0, 0, 1.3), 0.02, 0.018, 6, "Bronze")
	mb.Lathe([(0, 1.28), (0.1, 1.3), (0.11, 1.33), (0.0, 1.32)], 8, "Bronze")
	mb.Lathe([(0.0, 0.0), (0.02, 0.02), (0.0, 0.08)], 6, "Fire", Mt((0, 0, 1.32)))


@Piece("Loom", "Goods", "추를 단 직조기(세로 베틀)", (1.4, 0.6, 1.9))
def _(mb):
	for s in (1, -1):
		mb.Cyl((s * 0.65, 0.2, 0), (s * 0.62, 0.0, 1.85), 0.04, 0.035, 6, "Wood")
	mb.Cyl((-0.7, 0.0, 1.8), (0.7, 0.0, 1.8), 0.05, 0.05, 8, "WoodDark", up=(0, 0, 1))
	mb.Cyl((-0.66, 0.06, 1.0), (0.66, 0.06, 1.0), 0.025, 0.025, 6, "Wood", up=(0, 0, 1))
	mb.Box((0, 0.02, 1.4), (1.15, 0.015, 0.7), "Canvas")
	mb.Box((0, 0.0, 1.62), (1.15, 0.018, 0.1), "CanvasStripe")
	for k in range(12):
		x = -0.55 + k * 0.1
		mb.Box((x, 0.05, 0.75), (0.004, 0.004, 1.1), "Rope")
		mb.Ellipsoid((x, 0.06, 0.18), (0.035, 0.035, 0.04), 6, 3, "Clay")


@Piece("Table", "Goods", "나무 식탁", (1.4, 0.8, 0.75))
def _(mb):
	mb.Box((0, 0, 0.72), (1.4, 0.8, 0.06), "Wood")
	for sx in (1, -1):
		for sy in (1, -1):
			mb.Box((sx * 0.6, sy * 0.32, 0.35), (0.07, 0.07, 0.7), "WoodDark")


@Piece("Table_Marble", "Goods", "대리석 회의 식탁(올림포스)", (2.4, 1.2, 0.8))
def _(mb):
	mb.Box((0, 0, 0.75), (2.4, 1.2, 0.1), "Marble")
	for s in (1, -1):
		mb.Frustum((s * 0.9, 0, 0), (0.3, 0.8), (s * 0.9, 0, 0.7), (0.2, 0.6), "MarbleShade")


@Piece("Chair_Klismos", "Goods", "등받이가 휜 의자 클리스모스", (0.5, 0.55, 0.9))
def _(mb):
	mb.Box((0, 0, 0.45), (0.48, 0.48, 0.05), "Wood")
	for sx in (1, -1):
		mb.Tube([V(sx * 0.2, -0.2, 0.43), V(sx * 0.22, -0.26, 0.2), V(sx * 0.2, -0.32, 0.0)], [0.025] * 3, 5, "WoodDark")
		mb.Tube([V(sx * 0.2, 0.2, 0.43), V(sx * 0.22, 0.27, 0.2), V(sx * 0.2, 0.33, 0.0)], [0.025] * 3, 5, "WoodDark")
		mb.Tube([V(sx * 0.2, 0.22, 0.45), V(sx * 0.2, 0.27, 0.7), V(sx * 0.2, 0.33, 0.85)], [0.022] * 3, 5, "WoodDark")
	mb.Box((0, 0, 0), (0.46, 0.06, 0.12), "Wood", Mt((0, 0.31, 0.8), (-25, 0, 0)))


@Piece("Stool", "Goods", "등 없는 의자(접이식 디프로스)", (0.45, 0.4, 0.45))
def _(mb):
	mb.Box((0, 0, 0.42), (0.45, 0.38, 0.05), "Leather")
	for s in (1, -1):
		mb.Cyl((-0.18, s * 0.17, 0.4), (0.18, -s * 0.17, 0.0), 0.022, 0.022, 5, "WoodDark")
		mb.Cyl((0.18, s * 0.17, 0.4), (-0.18, -s * 0.17, 0.0), 0.022, 0.022, 5, "WoodDark")


@Piece("Kline", "Goods", "기대 눕는 연회용 침상 클리네", (2.0, 0.85, 0.85))
def _(mb):
	mb.Box((0, 0, 0.45), (2.0, 0.85, 0.12), "Wood")
	for sx in (1, -1):
		for sy in (1, -1):
			mb.Lathe([(0, 0), (0.05, 0), (0.035, 0.2), (0.05, 0.3), (0.0, 0.4)], 6, "WoodDark", Mt((sx * 0.92, sy * 0.36, 0)))
	mb.Box((0, 0, 0.56), (1.95, 0.8, 0.12), "Purple")
	mb.Ellipsoid((-0.8, 0.0, 0.7), (0.15, 0.3, 0.1), 6, 3, "Ochre")
	mb.Box((-1.0, 0, 0.7), (0.06, 0.85, 0.4), "Wood")


@Piece("Chest", "Goods", "나무 궤짝(청동 장식)", (0.8, 0.5, 0.55))
def _(mb):
	mb.Box((0, 0, 0.25), (0.8, 0.5, 0.5), "Wood")
	mb.Box((0, 0, 0.53), (0.84, 0.54, 0.06), "WoodDark")
	for x in (-0.3, 0.3):
		mb.Box((x, 0, 0.28), (0.05, 0.52, 0.52), "Bronze")
	mb.Box((0, -0.26, 0.42), (0.08, 0.02, 0.1), "Gold")


@Piece("Crate", "Goods", "짐 상자", (0.6, 0.6, 0.6))
def _(mb):
	mb.Box((0, 0, 0.3), (0.6, 0.6, 0.6), "WoodLight")
	for s in (1, -1):
		mb.Box((0, s * 0.305, 0.3), (0.62, 0.02, 0.08), "Wood")
		mb.Box((s * 0.305, 0, 0.3), (0.02, 0.62, 0.08), "Wood")
	mb.Box((0, 0, 0.605), (0.62, 0.08, 0.02), "Wood")


@Piece("Basket", "Goods", "엮은 바구니(과일)", (0.5, 0.5, 0.4))
def _(mb):
	mb.Lathe([(0, 0.0), (0.18, 0.0), (0.25, 0.25), (0.23, 0.26), (0.0, 0.2)], 12, "Rope")
	for k in range(6):
		a = k * 1.1
		mb.Ico((math.cos(a) * 0.1, math.sin(a) * 0.1, 0.26), 0.06, ("AppleGold", "Pomegranate", "Grape", "LeafLight")[k % 4] if k % 4 != 0 else "Pomegranate", subdiv=1)


@Piece("Sack", "Goods", "곡식 자루", (0.45, 0.4, 0.6))
def _(mb):
	mb.Ico((0, 0, 0.26), 1.0, "Canvas", subdiv=2, jitter=0.06, seed=3, scale=(0.22, 0.2, 0.27))
	mb.Cyl((0, 0, 0.48), (0, 0, 0.58), 0.06, 0.09, 8, "Canvas")
	mb.Cyl((0, 0, 0.5), (0, 0, 0.52), 0.07, 0.07, 8, "Rope")


@Piece("Well", "Goods", "돌 우물(도르래 + 두레박)", (1.4, 1.4, 2.2))
def _(mb):
	mb.Lathe([(0.55, 0.0), (0.7, 0.0), (0.7, 0.8), (0.55, 0.8)], 14, "StoneGray")
	mb.Lathe([(0, 0.6), (0.56, 0.6), (0.0, 0.6)], 14, "WaterDeep")
	for s in (1, -1):
		mb.Box((s * 0.62, 0, 1.35), (0.12, 0.12, 1.1), "Wood")
	mb.Cyl((-0.68, 0, 1.85), (0.68, 0, 1.85), 0.05, 0.05, 8, "WoodDark", up=(0, 0, 1))
	mb.Cyl((0, 0, 1.82), (0, 0, 1.2), 0.008, 0.008, 4, "Rope")
	mb.Lathe([(0, 1.0), (0.1, 1.0), (0.12, 1.2), (0.0, 1.2)], 8, "Wood")


@Piece("Quern", "Goods", "맷돌(곡식 가는 돌)", (0.7, 0.7, 0.5))
def _(mb):
	mb.Lathe([(0, 0.0), (0.35, 0.0), (0.35, 0.22), (0.0, 0.25)], 12, "StoneGray")
	mb.Lathe([(0, 0.25), (0.3, 0.25), (0.3, 0.38), (0.06, 0.4), (0.0, 0.38)], 12, "StoneDark")
	mb.Cyl((0.22, 0, 0.38), (0.22, 0, 0.55), 0.025, 0.025, 6, "Wood")


@Piece("Anvil", "Goods", "모루(받침 나무 위)", (0.7, 0.4, 0.75))
def _(mb):
	mb.Cyl((0, 0, 0.0), (0, 0, 0.45), 0.2, 0.18, 8, "WoodDark")
	mb.Box((0, 0, 0.55), (0.42, 0.18, 0.2), "Iron")
	mb.Box((0, 0, 0.68), (0.5, 0.2, 0.06), "Iron")
	mb.Cyl((0.25, 0, 0.68), (0.38, 0, 0.68), 0.06, 0.01, 6, "Iron", up=(0, 0, 1))


@Piece("Forge", "Goods", "대장간 화덕(풀무 + 불)", (1.4, 1.0, 1.6))
def _(mb):
	mb.Box((0, 0, 0.4), (1.3, 0.9, 0.8), "StoneDark")
	mb.Box((0, 0, 0.82), (1.1, 0.7, 0.06), "Ash_Pile")
	for k in range(5):
		mb.Ico((-0.3 + k * 0.15, (k % 2) * 0.1 - 0.05, 0.87), 0.06, "Ember", subdiv=1, seed=k)
	mb.Lathe([(0, 0.85), (0.25, 0.86), (0.1, 1.05), (0.0, 1.2)], 7, "Fire", Mt((0, 0, 0)))
	mb.Frustum((0, 0.25, 0.85), (0.7, 0.4), (0, 0.3, 1.6), (0.3, 0.25), "StoneDark")
	mb.Ellipsoid((0.85, 0.0, 0.5), (0.18, 0.28, 0.12), 8, 4, "Leather")
	mb.Cyl((0.85, -0.28, 0.5), (0.65, -0.1, 0.65), 0.03, 0.02, 6, "Wood")


@Piece("Campfire", "Goods", "모닥불(돌 두르기 + 장작 + 불꽃)", (1.0, 1.0, 0.7))
def _(mb):
	for k in range(9):
		a = 2 * math.pi * k / 9
		mb.Ico((math.cos(a) * 0.4, math.sin(a) * 0.4, 0.06), 0.11, "Rock" if k % 2 else "StoneGray", subdiv=1, jitter=0.2, seed=k, scale=(1, 1, 0.7))
	for k in range(4):
		a = k * math.pi / 4
		mb.Cyl((math.cos(a) * 0.32, math.sin(a) * 0.32, 0.05), (-math.cos(a) * 0.2, -math.sin(a) * 0.2, 0.18), 0.045, 0.04, 6, "Trunk")
	mb.Lathe([(0, 0.08), (0.22, 0.1), (0.15, 0.3), (0.06, 0.5), (0.0, 0.62)], 7, "Fire")
	mb.Lathe([(0, 0.1), (0.12, 0.12), (0.05, 0.35), (0.0, 0.45)], 6, "GlowGold")


@Piece("Torch_Wall", "Goods", "벽 횃불 걸이(쇠 고리 + 불꽃)", (0.2, 0.45, 0.8), snap="벽 앞면, 높이 1.6")
def _(mb):
	mb.Box((0, 0.0, 0.0), (0.1, 0.03, 0.2), "Iron")
	mb.Cyl((0, -0.03, 0.0), (0, -0.25, 0.25), 0.012, 0.012, 5, "Iron")
	mb.Cyl((0, -0.2, 0.0), (0, -0.3, 0.45), 0.03, 0.04, 6, "Wood")
	mb.Lathe([(0.0, 0.0), (0.05, 0.03), (0.04, 0.12), (0.0, 0.25)], 7, "Fire", Mt((0, -0.31, 0.45)))


@Piece("Bench", "Goods", "돌 벤치", (1.6, 0.5, 0.48))
def _(mb):
	mb.Box((0, 0, 0.43), (1.6, 0.5, 0.1), "Limestone")
	for s in (1, -1):
		mb.Box((s * 0.6, 0, 0.19), (0.2, 0.42, 0.38), "StoneGray")


@Piece("Ladder", "Goods", "나무 사다리 2.5 m", (0.5, 0.15, 2.5))
def _(mb):
	for s in (1, -1):
		mb.Box((s * 0.22, 0.0, 1.25), (0.05, 0.06, 2.5), "Wood")
	for k in range(8):
		mb.Cyl((-0.22, 0, 0.25 + k * 0.3), (0.22, 0, 0.25 + k * 0.3), 0.02, 0.02, 5, "WoodLight", up=(0, 0, 1))


@Piece("Shelf_Pottery", "Goods", "그릇 선반", (1.2, 0.4, 1.6))
def _(mb):
	for s in (1, -1):
		mb.Box((s * 0.58, 0, 0.8), (0.05, 0.38, 1.6), "Wood")
	for z in (0.4, 0.9, 1.4):
		mb.Box((0, 0, z), (1.2, 0.38, 0.04), "Wood")
	rnd = random.Random(4)
	for z in (0.42, 0.92, 1.42):
		x = -0.45
		while x < 0.45:
			h = rnd.uniform(0.12, 0.25)
			r = rnd.uniform(0.05, 0.08)
			mb.Lathe([(0, 0), (r * 0.6, 0), (r, h * 0.5), (r * 0.5, h), (0, h * 0.95)], 8, "Clay" if rnd.random() > 0.4 else "ClayDark", Mt((x, 0, z)))
			x += r * 2 + 0.05


@Piece("Cart", "Goods", "두 바퀴 손수레", (1.4, 2.0, 1.0))
def _(mb):
	mb.Box((0, 0, 0.6), (1.1, 1.3, 0.06), "Wood")
	for s in (1, -1):
		mb.Box((s * 0.55, 0, 0.75), (0.04, 1.3, 0.3), "WoodLight")
		mb.Cyl((s * 0.6, 0.1, 0.45), (s * 0.68, 0.1, 0.45), 0.45, 0.45, 12, "WoodDark", up=(0, 0, 1))
		mb.Cyl((s * 0.5, -0.65, 0.6), (s * 0.3, -1.6, 0.5), 0.03, 0.03, 5, "Wood")
	mb.Cyl((-0.7, 0.1, 0.45), (0.7, 0.1, 0.45), 0.03, 0.03, 6, "Iron", up=(0, 0, 1))


@Piece("Boat_Small", "Goods", "작은 고깃배(노 둘)", (1.4, 4.0, 0.9))
def _(mb):
	rings = []
	for (y, w, d) in ((-2.0, 0.02, 0.25), (-1.5, 0.45, 0.45), (-0.5, 0.68, 0.55), (0.5, 0.68, 0.55), (1.5, 0.5, 0.48), (2.0, 0.08, 0.35)):
		ring = [V(-w, y, 0.75), V(-w * 0.8, y, 0.75 - d * 0.6), V(0, y, 0.75 - d), V(w * 0.8, y, 0.75 - d * 0.6), V(w, y, 0.75)]
		rings.append(ring)
	mb.Loft(rings, "Wood", closed=False, cap0=False, cap1=False)
	inner = [[V(p.x * 0.88, p.y * 0.97, p.z + (0.04 if i in (0, 4) else 0.05)) for i, p in enumerate(r)] for r in rings]
	mb.Loft(inner, "WoodDark", closed=False, cap0=False, cap1=False)
	for (y, w) in ((-0.3, 0.62), (0.7, 0.6)):
		mb.Box((0, y, 0.55), (w * 2, 0.2, 0.05), "WoodLight")
	for s in (1, -1):
		mb.Cyl((s * 0.6, 0.2, 0.8), (s * 1.4, 0.8, 0.35), 0.025, 0.025, 5, "WoodLight")
	mb.Box((0, -2.0, 0.85), (0.06, 0.1, 0.3), "TemplePaint")
	for s in (1, -1):
		mb.Ellipsoid((s * 0.42, -1.55, 0.6), (0.01, 0.07, 0.05), 5, 3, "EyeWhite")


@Piece("Net_Rack", "Goods", "그물 말리는 틀", (2.0, 0.4, 1.6))
def _(mb):
	for s in (1, -1):
		mb.Cyl((s * 0.95, 0, 0), (s * 0.95, 0, 1.6), 0.04, 0.035, 6, "Wood")
	mb.Cyl((-1.0, 0, 1.55), (1.0, 0, 1.55), 0.03, 0.03, 6, "Wood", up=(0, 0, 1))
	rows = []
	for k in range(5):
		z = 1.5 - k * 0.3
		rows.append([V(-0.9 + i * 0.225, 0.04 * math.sin(i + k), z - 0.05 * math.sin(i * 0.7) ** 2) for i in range(9)])
	mb.Sheet(rows, 0.01, "Rope")
	for i in range(4):
		mb.Ico((-0.6 + i * 0.4, 0.0, 0.25), 0.04, "Cream", subdiv=1)


@Piece("Bread_Oven", "Goods", "돔 화덕(빵 굽는 흙 화덕)", (1.4, 1.4, 1.2))
def _(mb):
	mb.Box((0, 0, 0.3), (1.4, 1.4, 0.6), "StoneGray")
	mb.Ellipsoid((0, 0, 0.6), (0.6, 0.6, 0.55), 12, 6, "Clay")
	mb.Box((0, -0.55, 0.78), (0.32, 0.12, 0.28), "Shadow")
	mb.Ellipsoid((0, -0.52, 0.7), (0.12, 0.04, 0.04), 6, 3, "Ember")


@Piece("Scroll_Rack", "Goods", "두루마리 칸 선반(기록 보관소)", (1.2, 0.4, 1.6))
def _(mb):
	mb.Box((0, 0.18, 0.8), (1.2, 0.04, 1.6), "WoodDark")
	for s in (1, -1):
		mb.Box((s * 0.58, 0, 0.8), (0.04, 0.4, 1.6), "Wood")
	for row in range(4):
		mb.Box((0, 0, 0.05 + row * 0.42), (1.2, 0.4, 0.04), "Wood")
		for k in range(8):
			for j in range(2):
				mb.Cyl((-0.48 + k * 0.137, -0.18, 0.12 + row * 0.42 + j * 0.07), (-0.48 + k * 0.137, 0.15, 0.12 + row * 0.42 + j * 0.07), 0.03, 0.03, 6, "Parchment", up=(0, 0, 1))


@Piece("Signpost", "Goods", "이정표(방향판 셋)", (1.0, 0.3, 2.2))
def _(mb):
	mb.Cyl((0, 0, 0), (0, 0, 2.1), 0.06, 0.05, 6, "Wood")
	for k, (z, ang) in enumerate(((1.85, 20), (1.55, -35), (1.25, 140))):
		M = Mt((0, 0, z), (0, 0, ang))
		mb.Prism([(0.05, -0.1), (0.6, -0.1), (0.72, 0.0), (0.6, 0.1), (0.05, 0.1)], 0.04, "WoodLight", M @ FrameMatrix(V(0, 0, 0), V(1, 0, 0), V(0, 0, 1), V(0, 1, 0)))


@Piece("Stall", "Goods", "시장 좌판(천 차양 + 과일·항아리)", (2.0, 1.2, 2.3))
def _(mb):
	mb.Box((0, 0, 0.8), (1.9, 0.9, 0.08), "Wood")
	mb.Box((0, -0.43, 0.5), (1.9, 0.04, 0.6), "WoodDark")
	for sx in (1, -1):
		for sy in (1, -1):
			mb.Cyl((sx * 0.9, sy * 0.42, 0), (sx * 0.9, sy * 0.42, 2.0 + (0.25 if sy > 0 else 0)), 0.04, 0.035, 6, "Wood")
	rows = [[V(-1.05 + 2.1 * i / 6, y, z) for i in range(7)] for (y, z) in ((0.55, 2.3), (0.0, 2.2), (-0.6, 2.0))]
	start = mb.FaceCount()
	mb.Sheet(rows, 0.02, "Canvas")
	mb.Paint(start, lambda c: "CanvasStripe" if int((c.x + 1.05) / 0.35) % 2 == 0 else None)
	rnd = random.Random(9)
	for k in range(10):
		mb.Ico((rnd.uniform(-0.8, 0.8), rnd.uniform(-0.3, 0.3), 0.88), 0.06, ("AppleGold", "Pomegranate", "Grape", "LeafLight", "Clay")[k % 5], subdiv=1)
	_Pot(mb, [(0, 0.0), (0.04, 0.0), (0.09, 0.12), (0.06, 0.24), (0.07, 0.27), (0.0, 0.26)], role="Clay", M=Mt((0.7, 0.2, 0.84)))


@Piece("Tent", "Goods", "진영 천막(올림포스 정상 두 진영)", (3.0, 3.0, 2.6))
def _(mb):
	mb.Lathe([(0.0, 0.0), (1.5, 0.0), (1.45, 0.1), (1.2, 1.4), (0.0, 2.5)], 10, "Canvas")
	mb.Prism([(-0.4, 0.0), (0.4, 0.0), (0.0, 1.3)], 0.02, "Shadow", FrameMatrix(V(0, -1.41, 0.02), V(1, 0, 0), V(0, 0.3, 1).normalized(), V(0, 1, -0.3).normalized()))
	mb.Cyl((0, 0, 2.4), (0, 0, 3.0), 0.03, 0.03, 5, "Wood")
	mb.Prism([(0, 0), (0.5, 0.12), (0, 0.25)], 0.01, "Crest", FrameMatrix(V(0, 0, 2.72), V(1, 0, 0), V(0, 0, 1), V(0, 1, 0)))


@Piece("Banner", "Goods", "세로 깃발(진영 표시 — 천 색은 CanvasStripe)", (0.8, 0.4, 3.2))
def _(mb):
	mb.Cyl((0, 0, 0), (0, 0, 3.2), 0.04, 0.035, 6, "WoodDark")
	mb.Cyl((-0.4, 0, 3.0), (0.4, 0, 3.0), 0.025, 0.025, 5, "Wood", up=(0, 0, 1))
	rows = [[V(-0.35 + 0.7 * i / 4, 0.03 * math.sin(i + z * 3), z) for i in range(5)] for z in (2.98, 2.5, 2.0, 1.6)]
	mb.Sheet(rows, 0.015, "CanvasStripe")
	mb.Ico((0, -0.03, 2.4), 0.12, "Gold", subdiv=1, scale=(1, 0.2, 1))


def BuildPiece(entry, coll):
	mb = MeshBuilder()
	entry["fn"](mb)
	obj = mb.Build(entry["name"], coll, PaletteMaterial)
	obj["category"] = entry["category"]
	obj["desc"] = entry["desc"]
	return obj, mb


# ───────────────────────── 조립 예시(프리팹) ─────────────────────────

def _House(w, d, door=True, roof="Gable", lit=False):
	"""w·d: 벽 칸 수(2 m 단위). 앞면(-Y)에 문, 옆면에 창."""
	items = []
	W, D = w * 2.0, d * 2.0
	for i in range(w):
		x = -W / 2 + 1 + i * 2
		front = "Wall_Door" if (door and i == w // 2) else ("Wall_WindowLit" if lit else "Wall_Window")
		items.append((front, (x, -D / 2, 0), 0))
		items.append(("Wall_Plain" if i % 2 == 0 else "Wall_Window", (x, D / 2, 0), 180))
	for j in range(d):
		y = -D / 2 + 1 + j * 2
		items.append(("Wall_Window" if j % 2 else "Wall_Plain", (W / 2, y, 0), 90))
		items.append(("Wall_Plain", (-W / 2, y, 0), -90))
	for sx in (1, -1):
		for sy in (1, -1):
			items.append(("Wall_Corner", (sx * W / 2, sy * D / 2, 0), 0))
	if roof == "Gable" and d == 2:
		for i in range(w):
			items.append(("Roof_Gable", (-W / 2 + 1 + i * 2, 0, 0), 0))
		for sx in (1, -1):
			items.append(("Roof_GableEnd", (sx * W / 2, 0, 0), 90))
	else:
		for i in range(w):
			for j in range(d):
				items.append(("Roof_Flat", (-W / 2 + 1 + i * 2, -D / 2 + 1 + j * 2, 0), 0))
	for i in range(w):
		for j in range(d):
			items.append(("Floor_Stone", (-W / 2 + 1 + i * 2, -D / 2 + 1 + j * 2, -0.1), 0))
	return items


def _Temple():
	items = []
	cols = [-3.0, -1.0, 1.0, 3.0]
	for i in range(-2, 2):
		for j in range(-3, 3):
			items.append(("Temple_Step", (1.0 + i * 2, 1.0 + j * 2, 0), 0))
	for x in cols:
		items.append(("Column_Doric", (x, -5.0 + 0.6, 0.3), 0))
		items.append(("Column_Doric", (x, 5.0 - 0.6, 0.3), 0))
	for y in (-2.4, -0.4, 1.6):
		for x in (-3.0, 3.0):
			items.append(("Column_Doric", (x, y, 0.3), 0))
	for j in range(-2, 2):
		for x in (-1.75, 1.75):
			items.append(("Temple_Wall", (x, 1.0 + j * 2 + 0.6, 0.3), 90))
	items.append(("Temple_Door", (0, -2.4 + 1.0, 0.3), 0))
	items.append(("Temple_Wall", (0, 3.4, 0.3), 0))
	for side in (-1, 1):
		for i in range(4):
			items.append(("Entablature", (-3.0 + i * 2 + 0.0, side * (5.0 - 0.6), 0.3 + COLUMN_H), 0 if side < 0 else 180))
		for j in range(4):
			items.append(("Entablature", (side * 3.0, -3.4 + 0.6 + j * 2 + 0.8, 0.3 + COLUMN_H), 90 * side))
	for side in (-1, 1):
		items.append(("Pediment", (0, side * (5.0 - 0.6), 0.3 + COLUMN_H + 1.15), 0 if side < 0 else 180))
	for j in range(4):
		items.append(("Temple_Roof", (0, -3.0 + j * 2, 0.3 + COLUMN_H + 1.15), 90))
	items.append(("Altar", (0, -7.0, 0), 0))
	items.append(("Tripod", (1.6, -6.4, 0), 0))
	return items


PREFABS = {
	"House_Small": dict(desc="작은 집 4×4 m(문·창·박공지붕)", items=_House(2, 2)),
	"House_Long": dict(desc="긴 집 6×4 m(불 켜진 창)", items=_House(3, 2, lit=True)),
	"House_Island": dict(desc="섬마을 평지붕 집 4×4 m", items=_House(2, 2, roof="Flat")),
	"Temple_Doric": dict(desc="도리스식 신전 8×10 m(앞뒤 기둥 넷·옆 기둥·켈라·제단)", items=_Temple()),
	"Market_Corner": dict(desc="시장 한 귀퉁이(좌판 둘·항아리·바구니)", items=[
		("Stall", (-1.2, 0, 0), 0), ("Stall", (1.2, 0.3, 0), -12), ("Amphora", (-0.2, -1.0, 0), 0), ("Amphora", (0.1, -1.2, 0), 30),
		("Pithos", (2.6, -0.8, 0), 0), ("Basket", (-2.4, -0.9, 0), 0), ("Crate", (0.2, 1.2, 0), 15), ("Sack", (-0.4, 1.3, 0), 0)]),
}


def Build():
	ResetScene()
	root = GetCollection("Buildings")
	cats = {}
	objs = {}
	catalog = []
	for entry in KIT:
		coll = cats.get(entry["category"]) or GetCollection("Kit_" + entry["category"], root)
		cats[entry["category"]] = coll
		obj, mb = BuildPiece(entry, coll)
		objs[entry["name"]] = obj
		catalog.append(dict(name=entry["name"], category=entry["category"], desc=entry["desc"], size=entry["size"],
			pivot=entry["snap"], verts=len(mb.verts), faces=len(mb.faces), materials=list(mb.roles)))
	# 미리보기용으로 카테고리별 줄 세우기(내보낸 .glb에서는 각 조각이 원점 기준)
	ExportGlb(ResPath(OUT, "Buildings_Kit.glb"), [o for o in objs.values() if o["category"] in ("House", "Temple")], animations=False)
	ExportGlb(ResPath(OUT, "Goods_Kit.glb"), [o for o in objs.values() if o["category"] == "Goods"], animations=False)
	prefabColl = GetCollection("Prefabs", root)
	prefabOut = {}
	for pname, pf in PREFABS.items():
		pcoll = GetCollection("Prefab_" + pname, prefabColl)
		inst = []
		for (piece, loc, rot) in pf["items"]:
			o = objs[piece].copy()
			o.location = loc
			o.rotation_euler = (0, 0, math.radians(rot))
			pcoll.objects.link(o)
			inst.append(o)
		ExportGlb(ResPath(OUT, "Prefabs", pname + ".glb"), inst, animations=False)
		prefabOut[pname] = dict(desc=pf["desc"], file="Prefabs/%s.glb" % pname,
			items=[dict(piece=p, position=list(l), yawDegrees=r) for (p, l, r) in pf["items"]])
		pcoll.hide_render = True
		for o in inst:
			o.hide_set(True)
	WriteJson(ResPath(OUT, "Buildings.json"), dict(grid=dict(unit=1.0, wallWidth=WALL_W, wallHeight=WALL_H, wallThickness=WALL_T,
		roofSpan=ROOF_SPAN, columnHeight=COLUMN_H, forward="-Y(블렌더) / +Z(glTF)"), pieces=catalog, prefabs=prefabOut))
	SaveBlend(ResPath(OUT, "Buildings.blend"))
	return objs
