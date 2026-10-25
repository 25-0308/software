# -*- coding: utf-8 -*-
# 자연환경 — 이어 붙는(타일링) 땅·물 재질 텍스처와 로우폴리 나무·풀·바위.
#  - 텍스처: FFT로 거른 주기 노이즈라 가장자리가 저절로 이어짐(512×512, 색 + 노멀맵)
#  - 땅 타일: 2×2 m 판, UV 0~1 = 텍스처 한 장(타일마다 반복)
#  - 식물·바위: 팔레트 단색 면(텍스처 없음), 기준점 = 바닥 가운데
import bpy
import math
import os
import random
import numpy as np
from mathutils import Vector, Matrix
from rescommon import (MeshBuilder, Mt, V, ResetScene, GetCollection, PaletteMaterial, ExportGlb, SaveBlend, WriteJson, ResPath,
	Log, RES_DIR)
import palette

OUT = "Nature"
TEX = 512


# ───────────────────────── 텍스처 ─────────────────────────

def _Noise(beta, seed, n=TEX):
	"""1/f^beta 스펙트럼의 주기(이어 붙는) 노이즈, 0~1."""
	rng = np.random.default_rng(seed)
	white = rng.standard_normal((n, n))
	fx = np.fft.fftfreq(n)
	f = np.sqrt(fx[None, :] ** 2 + fx[:, None] ** 2)
	f[0, 0] = 1.0
	img = np.real(np.fft.ifft2(np.fft.fft2(white) / f ** beta))
	img -= img.min()
	return img / max(img.max(), 1e-9)


def _Voronoi(count, seed, n=TEX):
	"""주기 경계의 보로노이: (가장 가까운 거리, 두 번째 거리, 칸 번호)."""
	rng = np.random.default_rng(seed)
	pts = rng.random((count, 2))
	ys, xs = np.mgrid[0:n, 0:n] / n
	d1 = np.full((n, n), 9.0)
	d2 = np.full((n, n), 9.0)
	cell = np.zeros((n, n), dtype=np.int32)
	for i, (px, py) in enumerate(pts):
		dx = np.abs(xs - px)
		dx = np.minimum(dx, 1 - dx)
		dy = np.abs(ys - py)
		dy = np.minimum(dy, 1 - dy)
		d = np.sqrt(dx * dx + dy * dy)
		closer = d < d1
		d2 = np.where(closer, d1, np.minimum(d2, d))
		cell = np.where(closer, i, cell)
		d1 = np.where(closer, d, d1)
	return d1, d2, cell


def _Col(name):
	return np.array(palette.Hex2Rgb(palette.PALETTE[name][0]), dtype=np.float64)


def _Mix(a, b, t):
	return a[None, None, :] * (1 - t[..., None]) + b[None, None, :] * t[..., None]


def _Save(rgb, name, folder):
	n = rgb.shape[0]
	rgba = np.ones((n, n, 4), dtype=np.float32)
	rgba[..., :3] = np.clip(rgb, 0, 1)
	img = bpy.data.images.new(name, n, n, alpha=False)
	img.colorspace_settings.name = 'sRGB' if not name.endswith("_N") else 'Non-Color'
	img.pixels.foreach_set(rgba.ravel())
	path = ResPath(OUT, folder, name + ".png")
	img.filepath_raw = path
	img.file_format = 'PNG'
	img.save()
	return img


def _NormalMap(h, strength):
	dx = (np.roll(h, -1, axis=1) - np.roll(h, 1, axis=1)) * strength
	dy = (np.roll(h, -1, axis=0) - np.roll(h, 1, axis=0)) * strength
	nrm = np.stack([-dx, -dy, np.ones_like(h)], axis=-1)
	nrm /= np.linalg.norm(nrm, axis=-1, keepdims=True)
	return nrm * 0.5 + 0.5


def MakeTextures():
	out = {}

	def Emit(key, rgb, height, strength, desc):
		_Save(rgb, "T_" + key, "Textures")
		_Save(_NormalMap(height, strength), "T_%s_N" % key, "Textures")
		out[key] = desc

	# 풀밭: 큰 얼룩 + 잔 풀잎 반점
	big, mid, fine = _Noise(2.2, 1), _Noise(1.4, 2), _Noise(0.6, 3)
	t = np.clip(big * 0.8 + mid * 0.4 - 0.3, 0, 1)
	rgb = _Mix(_Col("GrassDark"), _Col("Grass"), t)
	rgb = rgb * 0.75 + _Mix(_Col("Grass"), _Col("LeafLight"), np.clip(fine * 1.6 - 0.55, 0, 1)) * 0.25
	dry = np.clip((mid - 0.62) * 4, 0, 1)
	rgb = rgb * (1 - dry[..., None] * 0.45) + _Col("GrassDry")[None, None, :] * dry[..., None] * 0.45
	Emit("Grass", rgb, fine * 0.6 + mid * 0.4, 6.0, "풀밭(짙은 얼룩·마른 풀빛)")

	# 마른 풀
	rgb = _Mix(_Col("GrassDry"), _Col("Grass"), np.clip(big * 0.9 - 0.25, 0, 0.6))
	rgb *= (0.85 + 0.3 * fine)[..., None]
	Emit("GrassDry", rgb, fine, 5.0, "마른 풀밭(여름 언덕·산기슭)")

	# 흙길: 얼룩 + 자갈 알갱이
	n1, n2 = _Noise(1.8, 11), _Noise(0.5, 12)
	rgb = _Mix(_Col("Mud"), _Col("Dirt"), np.clip(n1 * 1.3 - 0.1, 0, 1))
	d1, d2, cell = _Voronoi(120, 13)
	pebble = (d1 < 0.009 + 0.006 * (cell % 3) / 2).astype(np.float64)
	shade = (cell % 7) / 7.0
	rgb = rgb * (1 - pebble[..., None]) + _Mix(_Col("StoneGray"), _Col("Limestone"), shade) * pebble[..., None]
	rgb *= (0.9 + 0.2 * n2)[..., None]
	Emit("Dirt", rgb, n1 * 0.5 + pebble * 0.6 + n2 * 0.2, 7.0, "흙길(자갈 알갱이)")

	# 돌바닥: 불규칙 판석 + 줄눈
	d1, d2, cell = _Voronoi(28, 21)
	grout = np.clip((d2 - d1) / 0.012, 0, 1)
	tone = ((cell * 37) % 11) / 11.0
	n3 = _Noise(1.2, 22)
	rgb = _Mix(_Col("StoneGray"), _Col("Limestone"), tone * 0.8 + 0.1)
	rgb *= (0.88 + 0.22 * n3)[..., None]
	rgb = rgb * grout[..., None] + _Col("StoneDark")[None, None, :] * (1 - grout[..., None])
	Emit("StonePaving", rgb, grout * 0.8 + n3 * 0.2, 9.0, "판석 돌바닥(광장·신전 경내)")

	# 모래: 잔물결
	n4, n5 = _Noise(1.6, 31), _Noise(0.4, 32)
	ys, xs = np.mgrid[0:TEX, 0:TEX] / TEX
	ripple = 0.5 + 0.5 * np.sin((xs * 14 + ys * 4 + n4 * 2.0) * 2 * math.pi)
	rgb = _Mix(_Col("SandBeach"), _Col("Sand"), n4 * 0.6) * (0.92 + 0.08 * ripple + 0.06 * n5)[..., None]
	Emit("Sand", rgb, ripple * 0.5 + n5 * 0.3, 4.0, "모래사장(바람 물결)")

	# 바위: 큰 명암 + 가는 틈
	n6, n7 = _Noise(2.0, 41), _Noise(0.9, 42)
	d1, d2, _c = _Voronoi(18, 43)
	crack = np.clip((d2 - d1) / 0.006, 0, 1)
	rgb = _Mix(_Col("RockDark"), _Col("Rock"), np.clip(n6 * 1.2 - 0.1, 0, 1)) * (0.85 + 0.25 * n7)[..., None]
	rgb = rgb * (0.55 + 0.45 * crack[..., None])
	Emit("Rock", rgb, n6 * 0.6 + n7 * 0.3 + crack * 0.3, 8.0, "바위 표면(절벽·산)")

	# 눈
	n8 = _Noise(1.5, 51)
	rgb = _Mix(_Col("Snow"), _Col("Snow") * 0.86 + np.array([0.0, 0.02, 0.06]), n8 * 0.7)
	Emit("Snow", rgb, n8, 3.0, "눈(올림포스 산자락)")

	# 물: 깊이 얼룩 + 반짝이는 물결 무늬
	w1, w2 = _Noise(2.0, 61), _Noise(1.2, 62)
	warp = np.sin((w2 * 6 + xs * 3) * math.pi) * np.sin((w1 * 5 + ys * 3) * math.pi)
	caustic = np.clip(1 - np.abs(warp) * 6, 0, 1) ** 2
	rgb = _Mix(_Col("WaterDeep"), _Col("WaterShallow"), np.clip(w1 * 1.2 - 0.15, 0, 1))
	rgb = rgb + caustic[..., None] * 0.25
	Emit("Water", rgb, w1 * 0.4 + w2 * 0.6, 6.0, "물(깊이 얼룩·물결 빛무늬) — 셰이더에서 UV를 흘리면 흐르는 물")

	# 대리석: 흰 바탕 + 엷은 결
	m1, m2 = _Noise(1.8, 71), _Noise(1.0, 72)
	vein = np.clip(1 - np.abs(np.sin((xs * 2 + ys * 3 + m1 * 3) * math.pi)) * 12, 0, 1)
	rgb = _Mix(_Col("Marble"), _Col("MarbleShade"), m2 * 0.6) * (1 - vein[..., None] * 0.18)
	Emit("Marble", rgb, m2 * 0.3 + vein * 0.2, 2.0, "대리석(신전·신상)")

	# 회벽
	p1, p2 = _Noise(1.6, 81), _Noise(0.5, 82)
	rgb = _Mix(_Col("Plaster"), _Col("Limestone"), np.clip(p1 * 0.6 - 0.15, 0, 1) * 0.5) * (0.96 + 0.06 * p2)[..., None]
	Emit("Plaster", rgb, p1 * 0.4 + p2 * 0.4, 3.0, "회벽(집 벽)")
	Log("텍스처 %d종" % len(out))
	return out


def TextureMaterial(key, roughness=0.9, alpha=1.0):
	name = "M_" + key
	mat = bpy.data.materials.get(name) or bpy.data.materials.new(name)
	mat.use_nodes = True
	nt = mat.node_tree
	bsdf = nt.nodes.get("Principled BSDF")
	tex = nt.nodes.new("ShaderNodeTexImage")
	tex.image = bpy.data.images.load(ResPath(OUT, "Textures", "T_%s.png" % key), check_existing=True)
	ntex = nt.nodes.new("ShaderNodeTexImage")
	ntex.image = bpy.data.images.load(ResPath(OUT, "Textures", "T_%s_N.png" % key), check_existing=True)
	ntex.image.colorspace_settings.name = 'Non-Color'
	nmap = nt.nodes.new("ShaderNodeNormalMap")
	nt.links.new(tex.outputs["Color"], bsdf.inputs["Base Color"])
	nt.links.new(ntex.outputs["Color"], nmap.inputs["Color"])
	nt.links.new(nmap.outputs["Normal"], bsdf.inputs["Normal"])
	bsdf.inputs["Roughness"].default_value = roughness
	if alpha < 1.0:
		bsdf.inputs["Alpha"].default_value = alpha
		mat.surface_render_method = 'BLENDED'
	return mat


def _Tile(name, key, coll, size=2.0, repeat=1.0, z=0.0, roughness=0.9, alpha=1.0, segments=4):
	verts, faces, uvs = [], [], []
	for j in range(segments + 1):
		for i in range(segments + 1):
			verts.append((-size / 2 + size * i / segments, -size / 2 + size * j / segments, z))
	for j in range(segments):
		for i in range(segments):
			a = j * (segments + 1) + i
			faces.append((a, a + 1, a + segments + 2, a + segments + 1))
	me = bpy.data.meshes.new(name)
	me.from_pydata(verts, [], faces)
	uv = me.uv_layers.new(name="UVMap")
	for poly in me.polygons:
		for li in poly.loop_indices:
			x, y, _ = verts[me.loops[li].vertex_index]
			uv.data[li].uv = ((x / size + 0.5) * repeat, (y / size + 0.5) * repeat)
	me.materials.append(TextureMaterial(key, roughness, alpha))
	obj = bpy.data.objects.new(name, me)
	coll.objects.link(obj)
	return obj


TILES = [
	("Tile_Grass", "Grass", 2.0, 1.0, 0.0, 0.95, 1.0, "풀밭 2×2 m"),
	("Tile_GrassDry", "GrassDry", 2.0, 1.0, 0.0, 0.95, 1.0, "마른 풀밭 2×2 m"),
	("Tile_Dirt", "Dirt", 2.0, 1.0, 0.0, 0.95, 1.0, "흙길 2×2 m"),
	("Tile_StonePaving", "StonePaving", 2.0, 1.0, 0.0, 0.8, 1.0, "판석 돌바닥 2×2 m"),
	("Tile_Sand", "Sand", 2.0, 1.0, 0.0, 0.95, 1.0, "모래 2×2 m"),
	("Tile_Rock", "Rock", 2.0, 1.0, 0.0, 0.9, 1.0, "바위 바닥 2×2 m"),
	("Tile_Snow", "Snow", 2.0, 1.0, 0.0, 0.6, 1.0, "눈 2×2 m"),
	("Tile_Marble", "Marble", 2.0, 1.0, 0.0, 0.4, 1.0, "대리석 바닥 2×2 m"),
	("Water_Tile", "Water", 2.0, 1.0, -0.05, 0.05, 0.82, "물 2×2 m(반투명, 땅보다 5 cm 낮음)"),
	("Water_Large", "Water", 20.0, 10.0, -0.05, 0.05, 0.82, "넓은 물 20×20 m(바다·호수, 텍스처 10번 반복)"),
]


# ───────────────────────── 나무 ─────────────────────────

def _Canopy(mb, center, count, spread, radius, roles, seed, flat=1.0):
	rnd = random.Random(seed)
	for k in range(count):
		off = V(rnd.uniform(-1, 1) * spread[0], rnd.uniform(-1, 1) * spread[1], rnd.uniform(-1, 1) * spread[2])
		r = radius * rnd.uniform(0.75, 1.15)
		mb.Ico(V(center) + off, r, roles[k % len(roles)], subdiv=1, jitter=0.22, seed=seed * 31 + k, scale=(1, 1, flat))


def _Trunk(mb, base, top, r0, r1, wobble, seed, role="Trunk", segs=5):
	rnd = random.Random(seed)
	base, top = V(base), V(top)
	pts = []
	for i in range(segs + 1):
		t = i / segs
		p = base.lerp(top, t)
		if 0 < i < segs:
			p += V(rnd.uniform(-1, 1) * wobble, rnd.uniform(-1, 1) * wobble, 0)
		pts.append(p)
	mb.Tube(pts, [r0 + (r1 - r0) * i / segs for i in range(segs + 1)], 6, role)
	return pts


def Tree_Olive(mb, seed):
	rnd = random.Random(seed)
	base = V(0, 0, -0.1)
	pts = _Trunk(mb, base, (0.1, 0.05, 1.3), 0.2, 0.13, 0.12, seed, "TrunkOlive", 4)
	mb.Ico((0, 0, 0.05), 0.28, "TrunkOlive", subdiv=1, jitter=0.25, seed=seed, scale=(1, 1, 0.5))
	tips = []
	for k in range(3):
		a = 2 * math.pi * k / 3 + rnd.uniform(-0.3, 0.3)
		tip = pts[-1] + V(math.cos(a) * 0.9, math.sin(a) * 0.9, rnd.uniform(0.6, 1.0))
		_Trunk(mb, pts[-1], tip, 0.11, 0.05, 0.1, seed + k + 1, "TrunkOlive", 3)
		tips.append(tip)
	for k, tip in enumerate(tips):
		_Canopy(mb, tip + V(0, 0, 0.25), 6, (0.55, 0.55, 0.25), 0.5, ["LeafOlive", "LeafOliveDark", "LeafOlive"], seed * 7 + k, flat=0.7)
	_Canopy(mb, pts[-1] + V(0, 0, 1.0), 4, (0.4, 0.4, 0.2), 0.55, ["LeafOliveDark", "LeafOlive"], seed * 11, flat=0.7)


def Tree_Cypress(mb, seed):
	rnd = random.Random(seed)
	mb.Cyl((0, 0, -0.1), (0, 0, 0.8), 0.12, 0.1, 6, "Trunk")
	h = rnd.uniform(5.2, 6.4)
	prof = [(0.0, 0.5), (0.5, 0.7), (0.72, 1.6), (0.78, 2.6), (0.66, 3.6), (0.46, h - 1.2), (0.2, h - 0.4), (0.0, h)]
	sub = MeshBuilder()
	sub.Lathe(prof, 9, "Cypress")
	for i, v in enumerate(sub.verts):
		if 0.6 < v.z < h - 0.3:
			k = 1 + random.Random(seed * 100 + i).uniform(-0.12, 0.12)
			sub.verts[i] = V(v.x * k, v.y * k, v.z)
	start = mb.FaceCount()
	mb.Merge(sub)
	mb.Paint(start, lambda c: "Pine" if (c.x - c.y) > 0.25 else None)


def Tree_Oak(mb, seed):
	rnd = random.Random(seed)
	pts = _Trunk(mb, (0, 0, -0.1), (0.1, 0, 2.0), 0.26, 0.17, 0.08, seed, "Trunk", 4)
	for k in range(4):
		a = 2 * math.pi * k / 4 + rnd.uniform(-0.3, 0.3)
		start = pts[2 + k % 2]
		tip = start + V(math.cos(a) * 1.2, math.sin(a) * 1.2, 1.2)
		_Trunk(mb, start, tip, 0.12, 0.05, 0.1, seed + 9 + k, "Trunk", 3)
	_Canopy(mb, (0.1, 0, 3.4), 14, (1.5, 1.5, 0.8), 0.9, ["Leaf", "LeafDark", "Leaf", "LeafLight"], seed, flat=0.85)


def Tree_Pine(mb, seed):
	rnd = random.Random(seed)
	lean = V(rnd.uniform(-0.4, 0.4), rnd.uniform(-0.4, 0.4), 0)
	pts = _Trunk(mb, (0, 0, -0.1), V(0, 0, 5.2) + lean, 0.24, 0.14, 0.12, seed, "TrunkDark", 5)
	top = pts[-1]
	for k in range(5):
		a = 2 * math.pi * k / 5
		_Trunk(mb, top - V(0, 0, 0.4), top + V(math.cos(a) * 1.2, math.sin(a) * 1.2, 0.5), 0.09, 0.04, 0.05, seed + k, "TrunkDark", 2)
	_Canopy(mb, top + V(0, 0, 0.7), 12, (1.8, 1.8, 0.2), 0.9, ["Pine", "Cypress", "Pine"], seed, flat=0.4)


def Tree_Laurel(mb, seed):
	_Trunk(mb, (0, 0, -0.1), (0, 0.05, 1.0), 0.1, 0.07, 0.05, seed, "Trunk", 3)
	_Canopy(mb, (0, 0, 1.9), 10, (0.7, 0.7, 0.8), 0.6, ["Laurel", "LeafDark", "Laurel"], seed, flat=1.0)


def Tree_Dead(mb, seed):
	rnd = random.Random(seed)
	pts = _Trunk(mb, (0, 0, -0.1), (0.15, 0.0, 2.4), 0.2, 0.09, 0.12, seed, "DeadWood", 5)
	for k in range(5):
		a = 2 * math.pi * k / 5 + rnd.uniform(-0.4, 0.4)
		start = pts[2 + k % 3]
		tip = start + V(math.cos(a) * rnd.uniform(0.7, 1.2), math.sin(a) * rnd.uniform(0.7, 1.2), rnd.uniform(0.4, 1.0))
		mid = _Trunk(mb, start, tip, 0.07, 0.02, 0.08, seed + 20 + k, "DeadWood", 3)
		twig = tip + V(rnd.uniform(-0.4, 0.4), rnd.uniform(-0.4, 0.4), 0.4)
		_Trunk(mb, mid[1], twig, 0.035, 0.0, 0.05, seed + 40 + k, "DeadWood", 2)


# ───────────────────────── 덤불·풀·꽃 ─────────────────────────

def Bush_Round(mb, seed):
	_Canopy(mb, (0, 0, 0.45), 7, (0.45, 0.45, 0.2), 0.45, ["Leaf", "LeafDark", "LeafLight"], seed, flat=0.8)


def Bush_Oleander(mb, seed):
	_Canopy(mb, (0, 0, 0.7), 8, (0.5, 0.5, 0.35), 0.45, ["LeafDark", "Leaf"], seed, flat=1.0)
	rnd = random.Random(seed)
	for k in range(18):
		a = rnd.uniform(0, 2 * math.pi)
		e = rnd.uniform(0.0, 1.2)
		p = V(math.cos(a) * math.cos(e) * 0.75, math.sin(a) * math.cos(e) * 0.75, 0.7 + math.sin(e) * 0.65)
		mb.Ico(p, 0.07, "FlowerPink" if k % 4 else "FlowerWhite", subdiv=1, seed=k)


def _Blades(mb, center, count, height, role, seed, spread=0.12):
	rnd = random.Random(seed)
	c = V(center)
	for k in range(count):
		a = rnd.uniform(0, 2 * math.pi)
		base = c + V(math.cos(a) * rnd.uniform(0, spread), math.sin(a) * rnd.uniform(0, spread), 0)
		lean = V(math.cos(a), math.sin(a), 0) * rnd.uniform(0.05, 0.25) * height
		h = height * rnd.uniform(0.6, 1.1)
		w = 0.03
		side = V(-math.sin(a), math.cos(a), 0) * w
		tip = base + lean + V(0, 0, h)
		mb.Add([base - side, base + side, tip, base + side * 0.2 + V(0, 0, h * 0.5) + lean * 0.5 + V(math.cos(a), math.sin(a), 0) * 0.015],
			[(0, 1, 2), (0, 3, 1), (1, 3, 2), (0, 2, 3)], role)


def Grass_Tuft(mb, seed):
	_Blades(mb, (0, 0, 0), 14, 0.35, "Grass", seed)
	_Blades(mb, (0, 0, 0), 6, 0.28, "GrassDark", seed + 1)


def Grass_TuftDry(mb, seed):
	_Blades(mb, (0, 0, 0), 14, 0.38, "GrassDry", seed)
	_Blades(mb, (0, 0, 0), 5, 0.3, "Reed", seed + 1)


def Grass_Patch(mb, seed):
	rnd = random.Random(seed)
	for k in range(7):
		c = (rnd.uniform(-0.6, 0.6), rnd.uniform(-0.6, 0.6), 0)
		_Blades(mb, c, 10, rnd.uniform(0.2, 0.35), "Grass" if k % 3 else "GrassDark", seed * 10 + k)


def Reeds(mb, seed):
	rnd = random.Random(seed)
	for k in range(12):
		base = V(rnd.uniform(-0.3, 0.3), rnd.uniform(-0.3, 0.3), 0)
		top = base + V(rnd.uniform(-0.15, 0.15), rnd.uniform(-0.15, 0.15), rnd.uniform(1.0, 1.5))
		mb.Cyl(base, top, 0.012, 0.004, 4, "Reed")
		if k % 3 == 0:
			mb.Cyl(top - (top - base) * 0.18, top - (top - base) * 0.02, 0.02, 0.018, 5, "Brown")


def _Flowers(mb, seed, petal, center, count=7, height=0.3):
	rnd = random.Random(seed)
	for k in range(count):
		base = V(rnd.uniform(-0.25, 0.25), rnd.uniform(-0.25, 0.25), 0)
		h = height * rnd.uniform(0.7, 1.2)
		top = base + V(rnd.uniform(-0.04, 0.04), rnd.uniform(-0.04, 0.04), h)
		mb.Cyl(base, top, 0.008, 0.006, 4, "GrassDark")
		for p in range(5):
			a = 2 * math.pi * p / 5
			mb.Ellipsoid(top + V(math.cos(a) * 0.025, math.sin(a) * 0.025, 0), (0.022, 0.022, 0.006), 5, 2, petal)
		mb.Ico(top + V(0, 0, 0.006), 0.012, center, subdiv=0)
	_Blades(mb, (0, 0, 0), 8, 0.18, "Grass", seed + 99, spread=0.2)


def Flowers_Poppy(mb, seed):
	_Flowers(mb, seed, "Poppy", "BlackFigure")


def Flowers_Anemone(mb, seed):
	_Flowers(mb, seed, "FlowerLilac", "HairBlack")


def Flowers_Daisy(mb, seed):
	_Flowers(mb, seed, "FlowerWhite", "FlowerYellow", count=9, height=0.22)


def Flowers_Yellow(mb, seed):
	_Flowers(mb, seed, "FlowerYellow", "Ochre", count=8, height=0.25)


# ───────────────────────── 바위 ─────────────────────────

def _Rock(mb, size, seed, role="Rock", snow=False, moss=False, flat=0.7):
	rnd = random.Random(seed)
	sub = MeshBuilder()
	sub.Ico((0, 0, size * flat * 0.6), 1.0, role, subdiv=1, jitter=0.0, seed=seed, scale=(size, size * rnd.uniform(0.7, 1.0), size * flat))
	for i, v in enumerate(sub.verts):
		k = 1.0 + random.Random(seed * 1000 + i).uniform(-0.18, 0.18)
		sub.verts[i] = V(v.x * k, v.y * k, max(v.z * k, -0.05))
	start = mb.FaceCount()
	mb.Merge(sub)
	top = size * flat * 1.3

	def Paint(c):
		if snow and c.z > top * 0.75:
			return "Snow"
		if moss and c.z > top * 0.7 and c.x + c.y < 0:
			return "Moss"
		if c.z < size * 0.25:
			return "RockDark"
		return None
	mb.Paint(start, Paint)


def Rock_Small(mb, seed):
	_Rock(mb, 0.3, seed)


def Rock_Medium(mb, seed):
	_Rock(mb, 0.7, seed)


def Rock_Large(mb, seed):
	_Rock(mb, 1.4, seed, flat=0.8)


def Rock_Snowy(mb, seed):
	_Rock(mb, 1.1, seed, snow=True, flat=0.8)


def Rock_Mossy(mb, seed):
	_Rock(mb, 0.8, seed, moss=True)


def Rock_Ash(mb, seed):
	_Rock(mb, 0.8, seed, role="RockAsh")


def Cliff(mb, seed):
	rnd = random.Random(seed)
	for k in range(5):
		x = -1.2 + k * 0.6 + rnd.uniform(-0.15, 0.15)
		h = rnd.uniform(2.0, 3.2)
		sub = MeshBuilder()
		_Rock(sub, 0.6, seed * 10 + k, flat=h / 0.6 * 0.5)
		mb.Merge(sub, Mt((x, rnd.uniform(-0.2, 0.2), 0)))


def Pebbles(mb, seed):
	rnd = random.Random(seed)
	for k in range(9):
		p = V(rnd.uniform(-0.5, 0.5), rnd.uniform(-0.5, 0.5), 0.02)
		mb.Ico(p, rnd.uniform(0.04, 0.09), ("StoneGray", "Rock", "Limestone")[k % 3], subdiv=1, jitter=0.2, seed=k, scale=(1, 0.8, 0.5))


def Stump(mb, seed):
	mb.Cyl((0, 0, -0.05), (0, 0, 0.4), 0.3, 0.26, 8, "Trunk")
	mb.Cyl((0, 0, 0.4), (0, 0, 0.405), 0.24, 0.24, 8, "WoodLight")
	for k in range(4):
		a = k * math.pi / 2 + 0.4
		mb.Cyl((0, 0, 0.05), (math.cos(a) * 0.45, math.sin(a) * 0.45, -0.02), 0.1, 0.04, 5, "Trunk")


def WoodLog(mb, seed):
	mb.Cyl((-1.0, 0, 0.2), (1.0, 0.1, 0.2), 0.2, 0.18, 8, "Trunk", up=(0, 0, 1))
	for x, y in ((-1.0, 0.0), (1.0, 0.1)):
		mb.Cyl((x, y, 0.2), (x + (0.005 if x > 0 else -0.005), y, 0.2), 0.17, 0.17, 8, "WoodLight", up=(0, 0, 1))
	mb.Cyl((0.2, 0.05, 0.3), (0.35, -0.3, 0.55), 0.05, 0.02, 5, "Trunk")


def Mushrooms(mb, seed):
	rnd = random.Random(seed)
	for k in range(5):
		p = V(rnd.uniform(-0.15, 0.15), rnd.uniform(-0.15, 0.15), 0)
		h = rnd.uniform(0.06, 0.14)
		mb.Cyl(p, p + V(0, 0, h), 0.015, 0.012, 5, "Mushroom")
		mb.Lathe([(0, h - 0.01), (0.05, h - 0.005), (0.04, h + 0.02), (0, h + 0.035)], 7, "Crimson" if k % 2 else "Brown", Mt(p))


NATURE = [
	# 이름, 분류, 함수, 변형 시드, 설명
	("Tree_Olive", "Tree", Tree_Olive, (1, 2, 3), "올리브나무(뒤틀린 줄기·은빛 잎) ~3.5 m"),
	("Tree_Cypress", "Tree", Tree_Cypress, (1, 2), "사이프러스(가늘고 높은 짙은 초록) ~6 m"),
	("Tree_Oak", "Tree", Tree_Oak, (1, 2), "활엽수(떡갈나무) ~5 m"),
	("Tree_Pine", "Tree", Tree_Pine, (1, 2), "우산소나무(높은 줄기·넓적한 수관) ~7 m"),
	("Tree_Laurel", "Tree", Tree_Laurel, (1,), "월계수(델포이 카스탈리아 샘가) ~3 m"),
	("Tree_Dead", "Tree", Tree_Dead, (1, 2), "마른 나무(타이나론·타르타로스)"),
	("Bush_Round", "Plant", Bush_Round, (1, 2), "둥근 덤불"),
	("Bush_Oleander", "Plant", Bush_Oleander, (1,), "협죽도 덤불(분홍 꽃)"),
	("Grass_Tuft", "Plant", Grass_Tuft, (1, 2), "풀 포기"),
	("Grass_TuftDry", "Plant", Grass_TuftDry, (1,), "마른 풀 포기"),
	("Grass_Patch", "Plant", Grass_Patch, (1,), "풀 무더기 1.2 m"),
	("Reeds", "Plant", Reeds, (1,), "물가 갈대"),
	("Flowers_Poppy", "Plant", Flowers_Poppy, (1,), "양귀비 무리(빨강)"),
	("Flowers_Anemone", "Plant", Flowers_Anemone, (1,), "아네모네 무리(연보라)"),
	("Flowers_Daisy", "Plant", Flowers_Daisy, (1,), "들국화 무리(흰색)"),
	("Flowers_Yellow", "Plant", Flowers_Yellow, (1,), "노란 들꽃 무리"),
	("Rock_Small", "Rock", Rock_Small, (1, 2, 3), "작은 돌"),
	("Rock_Medium", "Rock", Rock_Medium, (1, 2), "바위"),
	("Rock_Large", "Rock", Rock_Large, (1, 2), "큰 바위"),
	("Rock_Snowy", "Rock", Rock_Snowy, (1,), "눈 덮인 바위(올림포스)"),
	("Rock_Mossy", "Rock", Rock_Mossy, (1,), "이끼 낀 바위"),
	("Rock_Ash", "Rock", Rock_Ash, (1,), "잿빛 바위(타이나론·저승)"),
	("Cliff", "Rock", Cliff, (1, 2), "절벽 덩어리 ~3 m(이어 세워 산자락)"),
	("Pebbles", "Rock", Pebbles, (1,), "자갈 무리"),
	("Stump", "Wood", Stump, (1,), "그루터기"),
	("Log", "Wood", WoodLog, (1,), "쓰러진 통나무"),
	("Mushrooms", "Wood", Mushrooms, (1,), "버섯 무리"),
]


def Build():
	ResetScene()
	root = GetCollection("Nature")
	textures = MakeTextures()
	tileColl = GetCollection("Ground_Tiles", root)
	tiles = []
	for (name, key, size, rep, z, rough, alpha, desc) in TILES:
		tiles.append(_Tile(name, key, tileColl, size, rep, z, rough, alpha, segments=10 if size > 5 else 4))
	# 텍스처를 한 번 더 넣지 않도록 .gltf로 쓰고 Textures/의 그림을 그대로 가리킴
	ExportGlb(ResPath(OUT, "Ground_Tiles.gltf"), tiles, animations=False, separate=True, textureDir="Textures")
	objs = []
	catalog = []
	for (name, cat, fn, seeds, desc) in NATURE:
		coll = GetCollection("Nature_" + cat, root)
		for i, s in enumerate(seeds):
			mb = MeshBuilder()
			fn(mb, s)
			vname = name if len(seeds) == 1 else "%s_%s" % (name, "ABC"[i])
			obj = mb.Build(vname, coll, PaletteMaterial)
			obj["category"] = cat
			obj["desc"] = desc
			objs.append(obj)
			catalog.append(dict(name=vname, category=cat, desc=desc, verts=len(mb.verts), faces=len(mb.faces), materials=list(mb.roles)))
	ExportGlb(ResPath(OUT, "Nature_Kit.glb"), objs, animations=False)
	WriteJson(ResPath(OUT, "Nature.json"), dict(
		textures={k: dict(color="Textures/T_%s.png" % k, normal="Textures/T_%s_N.png" % k, desc=d, size=TEX, tileable=True) for k, d in textures.items()},
		tiles=[dict(name=t[0], texture=t[1], size=t[2], uvRepeat=t[3], height=t[4], roughness=t[5], alpha=t[6], desc=t[7]) for t in TILES],
		props=catalog))
	SaveBlend(ResPath(OUT, "Nature.blend"))
	return tiles, objs
