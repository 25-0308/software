# -*- coding: utf-8 -*-
# 7등신 인간형 — 공용 뼈대와 레고처럼 갈아 끼우는 부위별 메시.
# 키 1.76 m(머리 하나 0.25 m), 앞은 -Y, 발바닥 z = 0.
# 모든 부위는 같은 뼈대에 스킨되어 있어서 슬롯마다 하나씩 골라 붙이면 한 캐릭터가 됨.
# 색은 역할(Role) 재질로 칠함 — 프리셋이 역할마다 팔레트 색을 정함(같은 부위도 캐릭터마다 다른 색).
import math
from mathutils import Vector, Matrix, Quaternion
from rescommon import (MeshBuilder, BoneSpec, MirrorSpecs, Ellipse, Rect, FrameMatrix, Mt, V,
	BvhFromBuilder, SurfacePoint)

# ───────────────────────── 관절 위치(왼쪽 기준) ─────────────────────────
J = {
	"pelvis": V(0, 0, 0.95), "spine": V(0, 0, 1.06), "chest": V(0, 0, 1.22), "chestTop": V(0, 0, 1.43),
	"neck": V(0, 0.005, 1.45), "head": V(0, -0.005, 1.53), "headTop": V(0, -0.005, 1.76),
	"clav": V(0.03, 0.0, 1.42), "clavEnd": V(0.17, 0.01, 1.425),
	"shoulder": V(0.19, 0.01, 1.415), "elbow": V(0.395, 0.035, 1.21), "wrist": V(0.57, 0.0, 1.04),
	"handEnd": V(0.685, 0.0, 0.925),
	"hip": V(0.09, 0.0, 0.92), "knee": V(0.09, -0.015, 0.50), "ankle": V(0.09, 0.02, 0.085),
	"ball": V(0.09, -0.10, 0.02), "toe": V(0.09, -0.165, 0.02),
}

# 손 좌표계(왼손): Y' = 손끝 방향, X' = 손바닥이 보는 쪽, Z' = 엄지 쪽(앞, -Y) = 주먹을 관통하는 축
HAND_Y = (J["handEnd"] - J["wrist"]).normalized()
HAND_Z = V(0, -1, 0)
HAND_X = HAND_Y.cross(HAND_Z).normalized()
GRIP = J["wrist"] + HAND_Y * 0.075 + HAND_X * 0.03

# 역할 기본색(부위 라이브러리에서 보이는 색)
ROLE_DEFAULTS = {
	"Skin": "SkinTan", "Hair": "HairBrown", "Eye": "IrisDark", "Lip": "Lip", "Line": "SkinDark",
	"ClothA": "Linen", "ClothB": "Terracotta", "Trim": "Ochre", "Metal": "Bronze", "Leather": "Leather",
	"Accent": "Laurel", "Accent2": "RoseRed", "Fur": "HairBrown", "Crest": "Crest", "Glow": "GlowPurple",
	"Horn": "Horn", "Feather": "Feather",
}

BODY_M = "M"
BODY_F = "F"


def SkeletonSpecs():
	s = [
		BoneSpec("root", (0, 0, 0), (0, -0.25, 0)),
		BoneSpec("pelvis", J["pelvis"], J["spine"], "root"),
		BoneSpec("spine", J["spine"], J["chest"], "pelvis"),
		BoneSpec("chest", J["chest"], J["chestTop"], "spine"),
		BoneSpec("neck", J["neck"], J["head"], "chest"),
		BoneSpec("head", J["head"], J["headTop"], "neck"),
		BoneSpec("clavicle.L", J["clav"], J["clavEnd"], "chest"),
		BoneSpec("upperarm.L", J["shoulder"], J["elbow"], "clavicle.L"),
		BoneSpec("forearm.L", J["elbow"], J["wrist"], "upperarm.L"),
		BoneSpec("hand.L", J["wrist"], J["handEnd"], "forearm.L", rollAxis=(0, -1, 0)),
		BoneSpec("prop.L", GRIP, GRIP + V(0, -0.1, 0), "hand.L", rollAxis=(0, 0, 1)),
		BoneSpec("thigh.L", J["hip"], J["knee"], "pelvis"),
		BoneSpec("shin.L", J["knee"], J["ankle"], "thigh.L"),
		BoneSpec("foot.L", J["ankle"], J["ball"], "shin.L", rollAxis=(0, 0, 1)),
		BoneSpec("toe.L", J["ball"], J["toe"], "foot.L", rollAxis=(0, 0, 1)),
	]
	return MirrorSpecs(s)


TORSO_BONES = ["pelvis", "spine", "chest", "neck", "clavicle.L", "clavicle.R", "upperarm.L", "upperarm.R"]
ARM_BONES = ["chest", "clavicle.L", "clavicle.R", "upperarm.L", "upperarm.R", "forearm.L", "forearm.R", "hand.L", "hand.R"]
LEG_BONES = ["pelvis", "thigh.L", "thigh.R", "shin.L", "shin.R", "foot.L", "foot.R"]
HIP_BONES = ["spine", "pelvis", "thigh.L", "thigh.R"]
HEAD_BONES = ["head", "neck", "chest"]


class Part:
	def __init__(self, name, slot, mb, bones=None, weightFn=None, factors=None, power=5.0, desc="", body=None):
		self.name = name
		self.slot = slot
		self.mb = mb
		self.bones = bones
		self.weightFn = weightFn
		self.factors = factors
		self.power = power
		self.desc = desc
		self.body = body


# ───────────────────────── 머리 ─────────────────────────
# (높이, 중심 y, 가로 반지름, 앞뒤 반지름)
HEAD_RINGS = [
	(1.43, 0.012, 0.050, 0.050), (1.50, 0.010, 0.052, 0.052), (1.525, -0.012, 0.060, 0.068),
	(1.555, -0.012, 0.072, 0.088), (1.595, -0.006, 0.080, 0.098), (1.640, 0.000, 0.085, 0.102),
	(1.685, 0.004, 0.084, 0.100), (1.725, 0.008, 0.072, 0.086), (1.752, 0.010, 0.045, 0.055),
]
HEAD_TOP = 1.762


def HeadBuilder(body=BODY_M):
	mb = MeshBuilder()
	rings = []
	for (z, cy, rx, ry) in HEAD_RINGS:
		if body == BODY_F:
			rx *= 0.93 if z > 1.58 else 0.86
			ry *= 0.97
		rings.append(Ellipse((0, cy, z), rx, ry, 12, square=2.15))
	rings.append([V(0, 0.010, HEAD_TOP)])
	mb.Loft(rings, "Skin")
	for s in (1, -1):
		with mb.Rigid("head"):
			mb.Ellipsoid((s * (0.083 if body == BODY_M else 0.077), 0.014, 1.625), (0.014, 0.024, 0.034), 6, 4, "Skin")
	return mb


_HEAD_BVH = {}


def HeadSurface(x, z, body=BODY_M):
	"""머리 앞면 (x, z) 지점의 표면 점."""
	if body not in _HEAD_BVH:
		_HEAD_BVH[body] = BvhFromBuilder(HeadBuilder(body))
	return SurfacePoint(_HEAD_BVH[body], (x, -0.6, z), (0, 1, 0), (x, -0.1, z))


def PartHead(body):
	mb = HeadBuilder(body)
	return Part("Head_" + body, "Head", mb, HEAD_BONES, power=6.0,
		desc="머리·목·귀(피부). M은 각진 턱, F는 갸름한 턱", body=body)


# ───────────────────────── 얼굴 ─────────────────────────

def _Brow(mb, s, angle, raise_=0.0, thick=0.008, role="Hair"):
	p = HeadSurface(s * 0.036, 1.664 + raise_)
	M = Matrix.Translation(p + V(0, -0.004, 0)) @ Matrix.Rotation(math.radians(angle), 4, 'Y')
	mb.Box((0, 0, 0), (0.034, 0.012, thick), role, M)


def _Eye(mb, s, iris=True, glow=False, lid=0.0, lash=False, size=1.0):
	p = HeadSurface(s * 0.035, 1.640)
	if glow:
		mb.Box(p + V(0, -0.001, 0), (0.032 * size, 0.012, 0.012), "Glow")
		return
	mb.Box(p + V(0, 0.002, 0), (0.030 * size, 0.012, 0.016 * size), "EyeWhite")
	if iris:
		mb.Box(p + V(s * -0.002, -0.0045, -0.0005), (0.012, 0.004, 0.014 * size), "Eye")
	if lid > 0:
		mb.Box(p + V(0, -0.0045, 0.008 - lid * 0.008), (0.033 * size, 0.008, 0.008 * lid + 0.002), "Skin")
	if lash:
		M = Matrix.Translation(p + V(s * 0.004, -0.004, 0.0085)) @ Matrix.Rotation(math.radians(-s * 8), 4, 'Y')
		mb.Box((0, 0, 0), (0.034 * size, 0.008, 0.004), "Line", M)


def _Nose(mb, wide=1.0, long_=1.0):
	top = HeadSurface(0, 1.648)
	bot = HeadSurface(0, 1.597)
	wt, wb = 0.007 * wide, 0.013 * wide
	A = (top.y + 0.004, 1.650)
	B = (bot.y - 0.026 * long_, 1.600)
	C = (bot.y + 0.004, 1.590)
	verts = [(-wt, A[0], A[1]), (wt, A[0], A[1]), (-wb * 0.6, B[0], B[1]), (wb * 0.6, B[0], B[1]), (-wb, C[0], C[1]), (wb, C[0], C[1])]
	faces = [(0, 1, 3, 2), (2, 3, 5, 4), (0, 2, 4), (1, 5, 3), (0, 4, 5, 1)]
	mb.Add(verts, faces, "Skin")


def _Mouth(mb, style, fem=False):
	p = HeadSurface(0, 1.563)
	c = p + V(0, -0.001, 0)
	if style == "smile":
		mb.Box(c, (0.024, 0.008, 0.006), "Lip")
		for s in (1, -1):
			M = Matrix.Translation(c + V(s * 0.016, 0.002, 0.003)) @ Matrix.Rotation(math.radians(-s * 30), 4, 'Y')
			mb.Box((0, 0, 0), (0.014, 0.008, 0.005), "Lip", M)
	elif style == "frown":
		mb.Box(c, (0.026, 0.008, 0.005), "Lip")
		for s in (1, -1):
			M = Matrix.Translation(c + V(s * 0.016, 0.002, -0.003)) @ Matrix.Rotation(math.radians(s * 30), 4, 'Y')
			mb.Box((0, 0, 0), (0.012, 0.008, 0.005), "Lip", M)
	elif style == "smirk":
		mb.Box(c + V(-0.004, 0, 0), (0.026, 0.008, 0.005), "Lip")
		M = Matrix.Translation(c + V(0.014, 0.002, 0.004)) @ Matrix.Rotation(math.radians(-35), 4, 'Y')
		mb.Box((0, 0, 0), (0.012, 0.008, 0.005), "Lip", M)
	elif style == "flat":
		mb.Box(c, (0.032, 0.008, 0.004), "Lip")
	else:
		mb.Box(c, (0.034, 0.008, 0.006), "Lip")
	if fem:
		mb.Box(c + V(0, -0.002, -0.006), (0.024, 0.008, 0.006), "Lip")


def _Lines(mb, old):
	if not old:
		return
	for dz in (0.02, 0.03):
		p = HeadSurface(0, 1.665 + dz)
		mb.Box(p + V(0, -0.001, 0), (0.05, 0.006, 0.003), "Line")
	for s in (1, -1):
		p = HeadSurface(s * 0.028, 1.58)
		M = Matrix.Translation(p + V(0, -0.002, 0)) @ Matrix.Rotation(math.radians(s * 25), 4, 'Y')
		mb.Box((0, 0, 0), (0.004, 0.006, 0.03), "Line", M)
		p = HeadSurface(s * 0.036, 1.622)
		mb.Box(p + V(0, -0.0015, 0), (0.024, 0.006, 0.003), "Line")


FACE_STYLES = {
	# 이름: (눈썹 각도, 눈썹 높이, 입, 눈꺼풀, 속눈썹, 주름, 빛나는 눈, 설명)
	"Calm":     dict(brow=0, raise_=0, mouth="calm", lid=0, desc="담담한 얼굴"),
	"Stern":    dict(brow=-14, raise_=-0.002, mouth="flat", lid=0.25, desc="엄한 얼굴 — 안쪽이 내려간 눈썹"),
	"Smile":    dict(brow=8, raise_=0.003, mouth="smile", lid=0, desc="웃는 얼굴"),
	"Angry":    dict(brow=-24, raise_=-0.004, mouth="frown", lid=0.2, desc="성난 얼굴"),
	"Weary":    dict(brow=10, raise_=0, mouth="flat", lid=0.6, desc="피곤한 얼굴 — 반쯤 감긴 눈"),
	"Old":      dict(brow=8, raise_=0, mouth="frown", lid=0.5, old=True, desc="늙은 얼굴 — 주름"),
	"Sly":      dict(brow=0, raise_=0, mouth="smirk", lid=0.3, sly=True, desc="능청스러운 얼굴 — 한쪽 눈썹이 올라감"),
	"Fem":      dict(brow=4, raise_=0.002, mouth="calm", lid=0, lash=True, fem=True, thin=True, desc="부드러운 얼굴(속눈썹·입술)"),
	"FemStern": dict(brow=-12, raise_=0, mouth="flat", lid=0.25, lash=True, fem=True, thin=True, desc="단호한 얼굴(속눈썹)"),
	"FemSmile": dict(brow=8, raise_=0.003, mouth="smile", lid=0, lash=True, fem=True, thin=True, desc="미소 짓는 얼굴(속눈썹)"),
	"Glow":     dict(brow=-22, raise_=-0.003, mouth="flat", glow=True, desc="빛나는 눈(그림자·거인·신들린 목소리)"),
}


def PartFace(style):
	o = FACE_STYLES[style]
	mb = MeshBuilder()
	with mb.Rigid("head"):
		for s in (1, -1):
			ang = o["brow"]
			raise_ = o["raise_"]
			if o.get("sly") and s == 1:
				ang, raise_ = 14, 0.008
			_Brow(mb, s, -s * ang, raise_, thick=0.006 if o.get("thin") else 0.008)
			_Eye(mb, s, glow=o.get("glow", False), lid=o.get("lid", 0), lash=o.get("lash", False))
		_Nose(mb, wide=0.85 if o.get("fem") else 1.0, long_=0.9 if o.get("fem") else 1.0)
		_Mouth(mb, o["mouth"], fem=o.get("fem", False))
		_Lines(mb, o.get("old", False))
	return Part("Face_" + style, "Face", mb, desc=o["desc"])


# ───────────────────────── 머리카락 ─────────────────────────

def _HairCap(mb, frontZ=1.668, backZ=1.60, grow=1.0, role="Hair"):
	def zFn(a, x, y):
		return (frontZ - backZ) * max(0.0, -math.sin(a)) ** 1.6
	rings = [
		Ellipse((0, 0.004, backZ), 0.093 * grow, 0.109 * grow, 12, zFn=zFn, square=2.1),
		Ellipse((0, 0.006, 1.705), 0.094 * grow, 0.110 * grow, 12, square=2.1),
		Ellipse((0, 0.010, 1.745), 0.081 * grow, 0.095 * grow, 12, square=2.1),
		Ellipse((0, 0.012, 1.773), 0.049 * grow, 0.059 * grow, 12),
		[V(0, 0.012, 1.783 + (grow - 1) * 0.05)],
	]
	mb.Loft(rings, role)


def _ScalpPoint(az, el, grow=1.0):
	"""머리 위 타원면 점과 바깥 방향. az: 뒤(+Y)에서 0, el: 0 = 귀 높이, 90 = 정수리."""
	a, e = math.radians(az), math.radians(el)
	c = V(0, 0.008, 1.655)
	d = V(math.sin(a) * math.cos(e), math.cos(a) * math.cos(e), math.sin(e))
	p = c + V(d.x * 0.097 * grow, d.y * 0.113 * grow, d.z * 0.122 * grow)
	return p, d


def PartHair(style):
	mb = MeshBuilder()
	desc = ""
	bones = None
	with mb.Rigid("head"):
		if style == "Short":
			_HairCap(mb)
			for s in (1, -1):
				mb.Box((s * 0.088, -0.02, 1.615), (0.018, 0.026, 0.05), "Hair")
			desc = "짧은 머리"
		elif style == "Curly":
			_HairCap(mb)
			k = 0
			for el in (18, 42, 66):
				n = {18: 12, 42: 10, 66: 6}[el]
				for i in range(n):
					az = 360.0 * i / n + (15 if el == 42 else 0)
					if el == 18 and abs(((az + 180) % 360) - 180) > 150:
						continue
					p, d = _ScalpPoint(az, el)
					mb.Ico(p + d * 0.006, 0.03, "Hair", subdiv=1, jitter=0.18, seed=k)
					k += 1
			mb.Ico(_ScalpPoint(0, 88)[0], 0.032, "Hair", subdiv=1, jitter=0.15, seed=99)
			for i, az in enumerate((150, 170, 190, 210)):
				p, d = _ScalpPoint(az, 30)
				mb.Ico(p + d * 0.004, 0.024, "Hair", subdiv=1, jitter=0.15, seed=200 + i)
			desc = "곱슬머리(덩어리 컬)"
		elif style == "Wild":
			_HairCap(mb, frontZ=1.672, backZ=1.56, grow=1.04)
			k = 0
			for el in (8, 30, 52, 74):
				n = 11 if el < 60 else 6
				for i in range(n):
					az = 360.0 * i / n + el
					if abs(((az + 180) % 360) - 180) > 135 and el < 40:
						continue
					p, d = _ScalpPoint(az, el, 1.0)
					tipDir = (d * 0.5 + V(0, 1.0, -0.9 if el < 40 else -0.3)).normalized()
					length = 0.05 + 0.025 * ((k * 7) % 3) / 2
					mb.Cyl(p - d * 0.012, p + tipDir * length + d * 0.01, 0.03, 0.004, 5, "Hair")
					k += 1
			desc = "헝클어진 머리(바닷바람에 뻗친 갈래)"
		elif style == "Bun":
			_HairCap(mb)
			mb.Ellipsoid((0, 0.118, 1.70), (0.048, 0.042, 0.044), 8, 5, "Hair")
			mb.Cyl((0, 0.075, 1.70), (0, 0.09, 1.70), 0.03, 0.03, 8, "Trim", up=(0, 0, 1))
			desc = "올림머리(뒤쪽 쪽머리 + 머리끈)"
	if style in ("Long", "LongFem"):
		with mb.Rigid("head"):
			_HairCap(mb, backZ=1.58)
		path = [V(0, 0.075, 1.67), V(0, 0.105, 1.56), V(0, 0.122, 1.46), V(0, 0.13, 1.36), V(0, 0.13, 1.30)]
		radii = [(0.045, 0.088), (0.045, 0.098), (0.04, 0.105), (0.032, 0.102), (0.02, 0.09)]
		mb.Tube(path, radii, 10, "Hair")
		if style == "LongFem":
			for s in (1, -1):
				lock = [V(s * 0.08, -0.03, 1.64), V(s * 0.092, -0.035, 1.55), V(s * 0.105, -0.045, 1.45), V(s * 0.11, -0.05, 1.38)]
				mb.Tube(lock, [0.022, 0.022, 0.018, 0.008], 6, "Hair")
		desc = "긴 머리(어깨뼈까지)" if style == "Long" else "긴 머리 + 앞으로 내린 옆머리"
		bones = HEAD_BONES
	return Part("Hair_" + style, "Hair", mb, bones, power=6.0, desc=desc)


def PartBeard(style):
	mb = MeshBuilder()

	def zFn(a, x, y):
		return -0.048 * max(0.0, -math.sin(a)) ** 2

	with mb.Rigid("head"):
		if style == "Full":
			rings = [Ellipse((0, -0.012, 1.605), 0.089, 0.101, 12, zFn=zFn),
				Ellipse((0, -0.024, 1.545), 0.083, 0.097, 12),
				Ellipse((0, -0.048, 1.49), 0.062, 0.068, 10 + 2),
				Ellipse((0, -0.068, 1.45), 0.032, 0.032, 12),
				[V(0, -0.075, 1.43)]]
			mb.Loft(rings, "Hair")
			desc = "덥수룩한 수염"
		elif style == "Short":
			rings = [Ellipse((0, -0.012, 1.60), 0.087, 0.100, 12, zFn=zFn),
				Ellipse((0, -0.020, 1.545), 0.080, 0.096, 12),
				Ellipse((0, -0.035, 1.51), 0.05, 0.06, 12),
				[V(0, -0.045, 1.495)]]
			mb.Loft(rings, "Hair")
			desc = "짧게 다듬은 수염"
		elif style == "Long":
			rings = [Ellipse((0, -0.012, 1.605), 0.09, 0.102, 12, zFn=zFn),
				Ellipse((0, -0.026, 1.545), 0.085, 0.098, 12),
				Ellipse((0, -0.055, 1.47), 0.07, 0.07, 12),
				Ellipse((0, -0.08, 1.38), 0.055, 0.05, 12),
				Ellipse((0, -0.09, 1.31), 0.032, 0.03, 12),
				[V(0, -0.092, 1.27)]]
			mb.Loft(rings, "Hair")
			desc = "가슴까지 내려오는 긴 수염"
		elif style == "Goatee":
			rings = [Ellipse((0, -0.07, 1.545), 0.028, 0.022, 8), Ellipse((0, -0.078, 1.50), 0.02, 0.016, 8), [V(0, -0.082, 1.46)]]
			mb.Loft(rings, "Hair")
			desc = "염소수염"
		# 콧수염
		if style != "Goatee":
			for s in (1, -1):
				p = HeadSurface(s * 0.016, 1.578)
				M = Matrix.Translation(p + V(0, -0.006, 0)) @ Matrix.Rotation(math.radians(s * 18), 4, 'Y')
				mb.Box((0, 0, 0), (0.03, 0.016, 0.01), "Hair", M)
	return Part("Beard_" + style, "Beard", mb, desc=desc)


# ───────────────────────── 머리 장식 ─────────────────────────

def _Band(mb, center, rx, ry, z0, z1, thick, n, role, zFnTop=None, zFnBot=None):
	c = V(center)
	ol = Ellipse((c.x, c.y, z0), rx, ry, n, zFn=zFnBot)
	oh = Ellipse((c.x, c.y, z1), rx, ry, n, zFn=zFnTop)
	ih = Ellipse((c.x, c.y, z1), rx - thick, ry - thick, n, zFn=zFnTop)
	il = Ellipse((c.x, c.y, z0), rx - thick, ry - thick, n, zFn=zFnBot)
	mb.Loft([ol, oh, ih, il, ol], role, cap0=False, cap1=False)


def _CrestProfile(y0, y1, zBase, height, steps=8):
	pts = []
	for i in range(steps + 1):
		t = i / steps
		y = y0 + (y1 - y0) * t
		z = zBase + height * math.sin(math.pi * min(1.0, t * 1.15)) ** 0.8
		pts.append((y, z))
	pts.append((y1, zBase - 0.02))
	pts.append((y0 + 0.03, zBase))
	return pts


def _Crest(mb, y0, y1, zBase, height, thick=0.03, role="Crest", tail=0.0):
	prof = _CrestProfile(y0, y1, zBase, height)
	if tail > 0:
		prof.insert(-2, (y1 + 0.02, zBase - tail))
	M = FrameMatrix(V(0, 0, 0), V(0, 1, 0), V(0, 0, 1), V(1, 0, 0))
	mb.Prism(prof, thick, role, M)


def _Helmet(mb, faceplate=False, cheek=0.085, crest=0.10, crestTail=0.0, neck=True):
	def zFn(a, x, y):
		return 0.058 * max(0.0, -math.sin(a)) ** 2
	rings = [Ellipse((0, 0.004, 1.595), 0.103, 0.119, 12, zFn=zFn),
		Ellipse((0, 0.006, 1.70), 0.104, 0.120, 12),
		Ellipse((0, 0.010, 1.748), 0.089, 0.103, 12),
		Ellipse((0, 0.012, 1.778), 0.057, 0.067, 12),
		[V(0, 0.012, 1.792)]]
	mb.Loft(rings, "Metal")
	_Band(mb, (0, 0.004, 0), 0.107, 0.123, 1.638, 1.652, 0.008, 12, "Metal",
		zFnTop=lambda a, x, y: 0.03 * max(0.0, -math.sin(a)) ** 2, zFnBot=lambda a, x, y: 0.03 * max(0.0, -math.sin(a)) ** 2)
	for s in (1, -1):
		M = Matrix.Translation(V(s * 0.09, -0.04, 1.61 - (cheek - 0.085) * 0.5)) @ Matrix.Rotation(math.radians(s * 18), 4, 'Z')
		mb.Box((0, 0, 0), (0.016, 0.075, cheek), "Metal", M)
	mb.Box((0, -0.115, 1.632), (0.012, 0.012, 0.055), "Metal")
	if faceplate:
		mb.Box((0, -0.12, 1.60), (0.17, 0.014, 0.075), "Metal")
		for s in (1, -1):
			mb.Box((s * 0.036, -0.128, 1.635), (0.034, 0.004, 0.013), "Shadow")
	if neck:
		M = Matrix.Translation(V(0, 0.118, 1.595)) @ Matrix.Rotation(math.radians(-25), 4, 'X')
		mb.Box((0, 0, 0), (0.17, 0.02, 0.05), "Metal", M)
	if crest > 0:
		mb.Box((0, 0.01, 1.795), (0.022, 0.16, 0.02), "Metal")
		_Crest(mb, -0.10, 0.17, 1.80, crest, tail=crestTail)


def PartHeadwear(kind):
	mb = MeshBuilder()
	desc = ""
	bones = None
	with mb.Rigid("head"):
		if kind == "Helm_Crested":
			_Helmet(mb, crest=0.10)
			desc = "청동 투구 + 붉은 깃털 장식(주인공·병사)"
		elif kind == "Helm_Athena":
			sub = MeshBuilder()
			_Helmet(sub, faceplate=True, cheek=0.06, crest=0.15)
			pivot = V(0, 0, 1.66)
			M = Matrix.Translation(pivot + V(0, 0.01, 0.075)) @ Matrix.Rotation(math.radians(-16), 4, 'X') @ Matrix.Translation(-pivot)
			mb.Merge(sub, M)
			desc = "이마 위로 밀어 올린 코린트식 투구(높은 깃털)"
		elif kind == "Helm_Ares":
			_Helmet(mb, cheek=0.11, crest=0.17, crestTail=0.16)
			desc = "볼가리개가 큰 전쟁 투구 + 뒤로 늘어진 깃털"
		elif kind == "Crown_Gold":
			_Band(mb, (0, 0.004, 0), 0.097, 0.113, 1.688, 1.71, 0.008, 16, "Gold",
				zFnTop=lambda a, x, y: 0.04 * max(0.0, -math.sin(a)) ** 6)
			mb.Ico(HeadSurface(0, 1.715) + V(0, -0.016, 0.004), 0.011, "Accent2", subdiv=1)
			desc = "앞이 솟은 금관(스테파네)"
		elif kind == "Crown_Spiked":
			_Band(mb, (0, 0.004, 0), 0.098, 0.114, 1.688, 1.715, 0.008, 16, "Metal")
			for i in range(12):
				a = -math.pi / 2 + 2 * math.pi * i / 12
				front = max(0.0, -math.sin(a))
				p = V(0.098 * math.cos(a), 0.004 + 0.114 * math.sin(a), 1.712)
				out = V(math.cos(a), math.sin(a), 0) * 0.012
				mb.Cyl(p, p + V(0, 0, 0.04 + 0.035 * front) + out, 0.011, 0.0, 4, "Metal")
			desc = "가시 돋친 관(하데스·크로노스·포세이돈)"
		elif kind == "Crown_Polos":
			M = Matrix.Translation(V(0, 0.008, 0))
			mb.Lathe([(0, 1.70), (0.083, 1.70), (0.09, 1.75), (0.099, 1.83), (0.104, 1.845), (0.0, 1.845)], 14, "Gold", M)
			_Band(mb, (0, 0.008, 0), 0.096, 0.096, 1.765, 1.785, 0.004, 14, "Accent2")
			desc = "높은 원통관(헤라의 폴로스)"
		elif kind.startswith("Wreath_"):
			_Wreath(mb, kind)
			desc = {"Wreath_Laurel": "월계관", "Wreath_Rose": "장미 화관", "Wreath_IvyGrape": "담쟁이·포도 화관",
				"Wreath_Pomegranate": "석류 화관"}[kind]
		elif kind == "Hat_PetasosWinged":
			M = Matrix.Translation(V(0, 0.006, 0)) @ Matrix.Rotation(math.radians(-6), 4, 'X')
			mb.Lathe([(0, 1.708), (0.185, 1.705), (0.186, 1.717), (0.0, 1.733)], 16, "ClothB", M)
			mb.Lathe([(0, 1.70), (0.099, 1.70), (0.097, 1.75), (0.075, 1.795), (0, 1.81)], 12, "ClothB", M)
			_Band(mb, (0, 0.006, 0), 0.1, 0.1, 1.715, 1.73, 0.004, 12, "Trim")
			for s in (1, -1):
				for k, (ang, ln) in enumerate(((20, 0.11), (45, 0.095), (70, 0.075))):
					base = V(s * 0.1, 0.02, 1.745)
					d = V(s * math.cos(math.radians(ang)) * 0.4, 0.55, math.sin(math.radians(ang)))
					mb.Ellipsoid((0, 0, 0), (0.008, ln / 2, 0.02), 4, 3, "Feather",
						Matrix.Translation(base + d.normalized() * ln / 2) @ _Align(V(0, 1, 0), d))
			desc = "날개 달린 챙 모자(헤르메스의 페타소스)"
		elif kind == "Cap_Pilos":
			mb.Lathe([(0, 1.675), (0.1, 1.675), (0.092, 1.74), (0.055, 1.83), (0, 1.86)], 12, "ClothB", Matrix.Translation(V(0, 0.008, 0)))
			desc = "원뿔 펠트 모자(필로스 — 장인·양치기·뱃사람)"
		elif kind == "Horns_Goat":
			for s in (1, -1):
				path = [V(s * 0.045, -0.025, 1.735), V(s * 0.065, 0.0, 1.795), V(s * 0.095, 0.06, 1.825), V(s * 0.13, 0.115, 1.795), V(s * 0.145, 0.13, 1.735)]
				mb.Tube(path, [0.021, 0.019, 0.015, 0.01, 0.0], 6, "Horn")
			desc = "뒤로 휜 염소 뿔(판)"
		elif kind == "Mask_Party":
			rows = []
			for z in (1.668, 1.645, 1.622):
				row = []
				for i in range(9):
					x = -0.088 + 0.176 * i / 8
					row.append(HeadSurface(x * 0.9, z) + V(0, -0.009, 0))
				rows.append(row)
			mb.Sheet(rows, 0.006, "Gold")
			for s in (1, -1):
				p = HeadSurface(s * 0.035, 1.644)
				mb.Box(p + V(0, -0.0135, 0), (0.026, 0.004, 0.012), "Shadow")
			mb.Cyl(HeadSurface(0.07, 1.668) + V(0, -0.01, 0), HeadSurface(0.08, 1.668) + V(0.03, -0.02, 0.09), 0.006, 0.0, 4, "Feather")
			desc = "가면무도회 반가면(키프로스 연회 손님)"
	if kind in ("Hood", "Veil"):
		bones = HEAD_BONES
		if kind == "Hood":
			levels = [(1.80, 0.03, 0.035, 0.012, 150), (1.776, 0.088, 0.102, 0.012, 150), (1.72, 0.11, 0.125, 0.012, 148),
				(1.64, 0.114, 0.128, 0.016, 146), (1.56, 0.112, 0.124, 0.02, 150), (1.49, 0.13, 0.13, 0.022, 164),
				(1.44, 0.20, 0.145, 0.012, 176), (1.37, 0.235, 0.155, 0.0, 178)]
			desc = "얼굴을 드러낸 두건 + 어깨 망토(노파·모이라이·돌아온 자들)"
			role = "ClothB"
		else:
			levels = [(1.86, 0.05, 0.05, 0.02, 120), (1.83, 0.108, 0.12, 0.016, 118), (1.74, 0.118, 0.13, 0.016, 112),
				(1.63, 0.124, 0.136, 0.022, 108), (1.53, 0.15, 0.14, 0.03, 106), (1.42, 0.21, 0.155, 0.03, 104),
				(1.30, 0.225, 0.165, 0.03, 104), (1.18, 0.23, 0.17, 0.032, 104)]
			desc = "정수리에서 등까지 늘어진 베일(헤라)"
			role = "ClothB"
		grid = []
		for (z, rx, ry, cy, openDeg) in levels:
			row = []
			for j in range(13):
				th = math.radians(-openDeg + 2 * openDeg * j / 12)
				row.append(V(rx * math.sin(th), cy + ry * math.cos(th), z))
			grid.append(row)
		mb.Sheet(grid, 0.012, role)
	return Part(kind, "Headwear", mb, bones, power=6.0, desc=desc)


def _Align(fromV, toV):
	q = V(fromV).normalized().rotation_difference(V(toV).normalized())
	return q.to_matrix().to_4x4()


def _Wreath(mb, kind):
	n = 18
	c = V(0, 0.006, 0)
	pts = []
	for i in range(n + 1):
		a = -math.pi / 2 + 2 * math.pi * i / n
		pts.append(V(c.x + 0.099 * math.cos(a), c.y + 0.115 * math.sin(a), 1.70 + 0.012 * math.sin(a)))
	leafRole = "Accent"
	big = kind == "Wreath_IvyGrape"
	for i in range(n):
		p, q = pts[i], pts[i + 1]
		mb.Cyl(p, q, 0.006, 0.006, 4, "Wood" if kind != "Wreath_Laurel" else "Accent")
		tang = (q - p).normalized()
		outward = V(p.x - c.x, p.y - c.y, 0).normalized()
		# 앞 한가운데에서 뒤로 갈수록 잎이 뒤를 향함
		back = tang if math.sin(-math.pi / 2 + 2 * math.pi * i / n) < 2 else tang
		for side in (1, -1):
			d = (back + V(0, 0, 0.45 * side) + outward * 0.25).normalized()
			L = 0.024 if not big else 0.03
			W = 0.009 if not big else 0.016
			M = Matrix.Translation(p + d * L * 0.5 + outward * 0.006) @ _Align(V(0, 1, 0), d)
			mb.Ellipsoid((0, 0, 0), (W, L * 0.55, 0.004), 4, 2, leafRole, M)
	front = [i for i in range(n) if abs(i - n // 4 * 0) < 99]
	if kind == "Wreath_Rose":
		for k, ang in enumerate((-90, -60, -120, -30, -150)):
			a = math.radians(ang)
			p = V(0.103 * math.cos(a), 0.006 + 0.119 * math.sin(a), 1.70 + 0.012 * math.sin(a) + 0.008)
			mb.Ico(p, 0.016 if k == 0 else 0.013, "Accent2", subdiv=1, jitter=0.1, seed=k)
	elif kind == "Wreath_IvyGrape":
		for s in (1, -1):
			base = V(s * 0.098, -0.03, 1.69)
			for k in range(6):
				off = V(s * 0.012 * (k % 2), -0.008 * (k % 3), -0.016 * (k // 2))
				mb.Ico(base + off, 0.011, "Accent2", subdiv=1)
	elif kind == "Wreath_Pomegranate":
		for k, ang in enumerate((-90, -50, -130)):
			a = math.radians(ang)
			p = V(0.104 * math.cos(a), 0.006 + 0.12 * math.sin(a), 1.71)
			mb.Ellipsoid(p, (0.017, 0.017, 0.016), 7, 4, "Accent2")
			mb.Cyl(p + V(0, 0, 0.014), p + V(0, 0, 0.026), 0.007, 0.004, 5, "Accent2")


# ───────────────────────── 몸통 ─────────────────────────
TORSO = {
	BODY_M: [(1.00, 0.005, 0.135, 0.095), (1.08, 0.005, 0.142, 0.100), (1.18, 0.000, 0.152, 0.104),
		(1.28, -0.006, 0.166, 0.110), (1.36, -0.004, 0.180, 0.104), (1.42, 0.006, 0.188, 0.088),
		(1.465, 0.010, 0.110, 0.068), (1.50, 0.012, 0.060, 0.055)],
	BODY_F: [(1.00, 0.005, 0.120, 0.090), (1.08, 0.004, 0.120, 0.090), (1.18, -0.002, 0.128, 0.096),
		(1.27, -0.012, 0.142, 0.117), (1.34, -0.008, 0.152, 0.110), (1.415, 0.006, 0.168, 0.086),
		(1.46, 0.010, 0.100, 0.064), (1.50, 0.012, 0.055, 0.052)],
}


def TorsoRing(body, z, off=0.0):
	rings = TORSO[body]
	for i in range(len(rings) - 1):
		a, b = rings[i], rings[i + 1]
		if a[0] <= z <= b[0]:
			t = (z - a[0]) / (b[0] - a[0])
			return tuple(a[k] + (b[k] - a[k]) * t for k in range(1, 4))
	r = rings[0] if z < rings[0][0] else rings[-1]
	return r[1], r[2], r[3]


def _Pec(mb, s, y, role):
	"""남자 가슴 근육 — 가슴 곡면을 따라 살짝 비스듬한 넓고 납작한 판(둥근 덩어리는 여자 가슴처럼 보였음)."""
	M = Matrix.Translation(V(s * 0.07, y, 1.338)) @ Matrix.Rotation(math.radians(s * 14), 4, 'Z') @ Matrix.Rotation(math.radians(-8), 4, 'X')
	mb.Frustum((0, 0, -0.034), (0.112, 0.016), (0, -0.004, 0.034), (0.122, 0.026), role, M)


def _TorsoLoft(mb, body, off, role, zMin=None):
	rings = []
	for (z, cy, rx, ry) in TORSO[body]:
		if zMin is not None and z < zMin:
			continue
		o = off if z < 1.49 else off * 0.5
		rings.append(Ellipse((0, cy, z), rx + o, ry + o, 12, square=2.3))
	start = mb.FaceCount()
	mb.Loft(rings, role)
	return start


def PartTorso(kind, body):
	mb = MeshBuilder()
	desc = ""
	if kind == "Bare":
		_TorsoLoft(mb, body, 0.0, "Skin")
		for s in (1, -1):
			_Pec(mb, s, -0.1, "Skin")
		desc = "맨몸 상체"
	elif kind == "Tunic":
		_TorsoLoft(mb, body, 0.01, "ClothA")
		_Band(mb, (0, 0.011, 0), 0.075, 0.068, 1.455, 1.47, 0.01, 12, "Trim")
		desc = "키톤 튜닉 상의(목둘레 장식 띠)"
	elif kind == "Robe":
		_TorsoLoft(mb, body, 0.014, "ClothA")
		_Band(mb, (0, 0.011, 0), 0.078, 0.07, 1.452, 1.47, 0.012, 12, "Trim")
		mb.Box((0, -0.118 if body == BODY_M else -0.122, 1.30), (0.03, 0.01, 0.30), "Trim")
		desc = "긴 예복 상의(가운데 장식 띠)"
	elif kind == "Exomis":
		start = _TorsoLoft(mb, body, 0.01, "ClothA")
		mb.Paint(start, lambda c: "Skin" if c.z > 1.27 + 0.6 * (c.x + 0.19) else None)
		mb.Ellipsoid((0.15, -0.01, 1.47), (0.035, 0.045, 0.025), 6, 4, "ClothA")
		desc = "한쪽 어깨를 드러낸 노동복(엑소미스)"
	elif kind == "Himation":
		start = _TorsoLoft(mb, body, 0.004, "Skin")
		mb.Paint(start, lambda c: "ClothB" if c.z < 1.27 + 0.6 * (c.x + 0.19) else None)
		path = [V(-0.17, -0.10, 1.25), V(-0.05, -0.125, 1.33), V(0.08, -0.11, 1.42), V(0.17, -0.03, 1.49),
			V(0.19, 0.03, 1.48), V(0.10, 0.10, 1.42), V(-0.05, 0.115, 1.33), V(-0.17, 0.10, 1.25)]
		mb.Tube(path, [0.022] * len(path), 6, "ClothB")
		_Pec(mb, -1, -0.103, "Skin")
		desc = "왼어깨에 두른 히마티온(오른 가슴은 맨살)"
	elif kind == "Dress":
		_TorsoLoft(mb, body, 0.01, "ClothA")
		_Band(mb, (0, -0.004, 0), TorsoRing(body, 1.21)[1] + 0.016, TorsoRing(body, 1.21)[2] + 0.016, 1.20, 1.225, 0.01, 12, "Trim")
		for s in (1, -1):
			mb.Ico((s * 0.085, 0.0, 1.47), 0.013, "Gold", subdiv=1)
		_Band(mb, (0, 0.011, 0), 0.072, 0.066, 1.455, 1.468, 0.01, 12, "ClothA")
		desc = "여성 키톤(가슴 아래 띠·어깨 금핀)"
	elif kind == "Cuirass":
		_TorsoLoft(mb, body, 0.022, "Metal")
		for s in (1, -1):
			_Pec(mb, s, -0.121, "Metal")
			for row in range(3):
				mb.Box((s * 0.033, -0.118 + row * 0.002, 1.22 - row * 0.065), (0.05, 0.012, 0.05), "Metal")
			M = Matrix.Translation(V(s * 0.17, 0.0, 1.445)) @ Matrix.Rotation(math.radians(s * -22), 4, 'Y')
			mb.Box((0, 0, 0), (0.12, 0.17, 0.025), "Leather", M)
		_Band(mb, (0, 0.006, 0), 0.165, 0.123, 1.04, 1.075, 0.01, 12, "Metal")
		desc = "청동 근육 흉갑 + 가죽 어깨덮개"
	elif kind == "Aegis":
		_TorsoLoft(mb, body, 0.01, "ClothA")
		rings = []
		for z in (1.24, 1.30, 1.37, 1.425, 1.465):
			cy, rx, ry = TorsoRing(body, z)
			rings.append(Ellipse((0, cy, z), rx + 0.024, ry + 0.024, 12, square=2.3))
		inner = []
		for z in (1.465, 1.24):
			cy, rx, ry = TorsoRing(body, z)
			inner.append(Ellipse((0, cy, z), rx + 0.006, ry + 0.006, 12, square=2.3))
		mb.Loft(rings + inner + [rings[0]], "Gold", cap0=False, cap1=False)
		cy, rx, ry = TorsoRing(body, 1.33)
		mb.Cyl((0, cy - ry - 0.02, 1.33), (0, cy - ry - 0.035, 1.33), 0.034, 0.032, 10, "Crest", up=(0, 0, 1))
		for i in range(12):
			a = -math.pi / 2 + 2 * math.pi * (i + 0.5) / 12
			cy, rx, ry = TorsoRing(body, 1.24)
			p = V((rx + 0.024) * math.cos(a), cy + (ry + 0.024) * math.sin(a), 1.245)
			mb.Cyl(p, p + V(0, 0, -0.045), 0.009, 0.0, 4, "Gold")
		desc = "황금 아이기스(어깨·가슴을 덮는 비늘 갑옷 + 고르곤 메달)"
	name = "Torso_%s_%s" % (kind, body)
	return Part(name, "Torso", mb, TORSO_BONES, power=4.0, desc=desc, body=body)


# ───────────────────────── 팔·손 ─────────────────────────

def _ArmPath(t0=0.0, t1=1.0):
	sh, el, wr = J["shoulder"], J["elbow"], J["wrist"]
	pts = [V(0.13, 0.01, 1.43), sh, sh.lerp(el, 0.5), el, el.lerp(wr, 0.45), wr]
	return pts


def PartArms(kind):
	mb = MeshBuilder()
	left = MeshBuilder()
	path = _ArmPath()
	radii = [0.05, 0.056, 0.047, 0.037, 0.040, 0.030]
	if kind != "LongSleeve":
		left.Tube(path, radii, 8, "Skin")
	desc = "맨팔"
	if kind == "ShortSleeve":
		sh, el = J["shoulder"], J["elbow"]
		left.Tube([V(0.125, 0.01, 1.435), sh, sh.lerp(el, 0.42)], [0.058, 0.068, 0.06], 8, "ClothA")
		desc = "짧은 소매"
	elif kind == "LongSleeve":
		r2 = [r + 0.014 for r in radii]
		r2[-1] = 0.05
		left.Tube(path, r2, 8, "ClothA")
		wr, el = J["wrist"], J["elbow"]
		left.Cyl(el.lerp(wr, 0.86), el.lerp(wr, 1.0), 0.054, 0.054, 8, "Trim")
		left.Tube([el.lerp(wr, 0.95), wr + HAND_Y * 0.01], [0.034, 0.03], 8, "Skin")
		desc = "긴 소매(소맷부리 장식)"
	elif kind == "Bracers":
		el, wr = J["elbow"], J["wrist"]
		left.Tube([el.lerp(wr, 0.3), el.lerp(wr, 0.6), el.lerp(wr, 0.93)], [0.049, 0.047, 0.04], 8, "Leather")
		desc = "가죽 팔뚝 보호대"
	mb.Merge(left)
	mb.Merge(left.Mirrored())
	return Part("Arms_" + kind, "Arms", mb, ARM_BONES, power=5.0, desc=desc)


def HandFrame():
	return FrameMatrix(J["wrist"], HAND_X, HAND_Y, HAND_Z)


def PartHands():
	left = MeshBuilder()
	M = HandFrame()
	with left.Rigid("hand.L"):
		rings = [Rect((0.0, -0.01, 0.0), (1, 0, 0), (0, 0, 1), 0.026, 0.048),
			Rect((0.0, 0.03, 0.002), (1, 0, 0), (0, 0, 1), 0.032, 0.066),
			Rect((0.002, 0.085, 0.0), (1, 0, 0), (0, 0, 1), 0.03, 0.074),
			Rect((0.012, 0.135, 0.0), (1, 0, 0), (0, 0, 1), 0.024, 0.07),
			Rect((0.022, 0.16, 0.0), (1, 0, 0), (0, 0, 1), 0.016, 0.056)]
		left.Loft(rings, "Skin", M=M)
		left.Tube([V(0.004, 0.02, 0.026), V(0.014, 0.05, 0.046), V(0.026, 0.078, 0.05)], [0.013, 0.011, 0.008], 6, "Skin", M=M)
	mb = MeshBuilder()
	mb.Merge(left)
	mb.Merge(left.Mirrored())
	return Part("Hands_Bare", "Hands", mb, desc="손(엄지 + 모은 네 손가락)")


# ───────────────────────── 하체(허리 아래 옷) ─────────────────────────
HIPS = {
	BODY_M: [(1.10, 0.004, 0.138, 0.098), (1.02, 0.006, 0.150, 0.105), (0.94, 0.010, 0.158, 0.110), (0.88, 0.010, 0.152, 0.104),
		(0.83, 0.008, 0.120, 0.080), (0.80, 0.008, 0.060, 0.050)],
	BODY_F: [(1.10, 0.004, 0.124, 0.092), (1.02, 0.008, 0.150, 0.106), (0.94, 0.012, 0.166, 0.114), (0.88, 0.012, 0.160, 0.108),
		(0.83, 0.010, 0.126, 0.082), (0.80, 0.008, 0.060, 0.050)],
}


def _RobeWeights(p):
	side = "L" if p.x > 0 else "R"
	h = max(0.0, min(1.0, (0.96 - p.z) / 0.86))
	lat = max(0.0, min(1.0, abs(p.x) / 0.09))
	legw = h * 0.75 * lat
	w = {"pelvis": max(0.0, 1.0 - legw)}
	if legw > 0:
		if p.z > 0.5:
			w["thigh." + side] = legw
		else:
			k = max(0.0, min(1.0, (0.5 - p.z) / 0.3))
			w["thigh." + side] = legw * (1 - k * 0.6)
			w["shin." + side] = legw * k * 0.6
	return w


def _SkirtWeights(p):
	if p.z > 0.97:
		return {"pelvis": 1.0} if p.z < 1.06 else {"spine": 0.3, "pelvis": 0.7}
	side = "L" if p.x > 0 else "R"
	h = max(0.0, min(1.0, (0.97 - p.z) / 0.3))
	lat = max(0.0, min(1.0, abs(p.x) / 0.1))
	legw = h * 0.85 * (0.35 + 0.65 * lat)
	return {"pelvis": 1 - legw, "thigh." + side: legw}


def _HipsRings(body, extra):
	out = []
	for (z, cy, rx, ry) in extra:
		dx = 0.0 if body == BODY_M else 0.012
		out.append(Ellipse((0, cy, z), rx + dx, ry + dx * 0.5, 12, square=2.3))
	return out


def PartLower(kind, body):
	mb = MeshBuilder()
	wf = _SkirtWeights
	desc = ""
	if kind == "ShortSkirt":
		rings = _HipsRings(body, [(1.10, 0.004, 0.146, 0.104), (1.02, 0.006, 0.158, 0.112), (0.94, 0.010, 0.168, 0.12),
			(0.84, 0.010, 0.182, 0.13), (0.72, 0.010, 0.192, 0.138), (0.64, 0.010, 0.198, 0.142)])
		mb.Loft(rings, "ClothA")
		_Band(mb, (0, 0.01, 0), 0.202 + (0.012 if body == BODY_F else 0), 0.146, 0.64, 0.665, 0.01, 12, "Trim")
		_Band(mb, (0, 0.006, 0), 0.152 + (0.0 if body == BODY_M else 0.004), 0.11, 1.05, 1.075, 0.01, 12, "Leather")
		desc = "허벅지 중간까지 오는 튜닉 아랫단 + 가죽 허리띠"
	elif kind == "LongRobe":
		rings = _HipsRings(body, [(1.10, 0.004, 0.148, 0.105), (1.02, 0.006, 0.160, 0.113), (0.94, 0.010, 0.170, 0.12),
			(0.80, 0.012, 0.188, 0.132), (0.60, 0.014, 0.198, 0.14), (0.35, 0.016, 0.21, 0.152), (0.12, 0.018, 0.226, 0.165),
			(0.05, 0.018, 0.232, 0.17)])
		mb.Loft(rings, "ClothA")
		_Band(mb, (0, 0.018, 0), 0.236 + (0.012 if body == BODY_F else 0), 0.174, 0.05, 0.09, 0.01, 12, "Trim")
		_Band(mb, (0, 0.006, 0), 0.155, 0.112, 1.05, 1.075, 0.01, 12, "Trim")
		wf = _RobeWeights
		desc = "발목까지 오는 긴 옷"
	elif kind == "Pteruges":
		rings = _HipsRings(body, [(1.10, 0.004, 0.146, 0.104), (1.02, 0.006, 0.158, 0.112), (0.94, 0.010, 0.168, 0.12),
			(0.84, 0.010, 0.18, 0.128), (0.74, 0.010, 0.188, 0.134)])
		mb.Loft(rings, "ClothA")
		for i in range(16):
			a = -math.pi / 2 + 2 * math.pi * (i + 0.5) / 16
			p = V(0.188 * math.cos(a), 0.01 + 0.136 * math.sin(a), 0.80)
			M = Matrix.Translation(p) @ Matrix.Rotation(a + math.pi / 2, 4, 'Z')
			mb.Box((0, 0, 0), (0.06, 0.012, 0.17), "Leather", M)
		_Band(mb, (0, 0.006, 0), 0.172, 0.126, 0.875, 0.905, 0.01, 16, "Metal")
		desc = "가죽 띠 치마(프테루게스) + 청동 허리띠"
	elif kind == "Loincloth":
		rings = _HipsRings(body, [(1.06, 0.004, 0.145, 0.102), (0.98, 0.006, 0.158, 0.11), (0.90, 0.010, 0.165, 0.116),
			(0.84, 0.010, 0.14, 0.10), (0.80, 0.01, 0.07, 0.06)])
		mb.Loft(rings, "ClothA")
		mb.Box((0, -0.11, 0.80), (0.1, 0.012, 0.15), "ClothA")
		_Band(mb, (0, 0.006, 0), 0.152, 0.108, 1.02, 1.04, 0.01, 12, "Leather")
		desc = "허리에 감은 천(낡은 옷 — 프로메테우스·탄탈로스)"
	elif kind == "FurHips":
		rings = _HipsRings(body, [(1.06, 0.004, 0.148, 0.104), (0.98, 0.006, 0.162, 0.114), (0.90, 0.010, 0.17, 0.12),
			(0.82, 0.010, 0.15, 0.11), (0.78, 0.01, 0.07, 0.06)])
		mb.Loft(rings, "Fur")
		for i in range(14):
			a = -math.pi / 2 + 2 * math.pi * i / 14
			p = V(0.165 * math.cos(a), 0.01 + 0.116 * math.sin(a), 0.84)
			mb.Ico(p, 0.03, "Fur", subdiv=1, jitter=0.2, seed=i)
		desc = "털 난 허리(판의 염소 하체와 이어짐)"
	return Part("Lower_%s_%s" % (kind, body), "Lower", mb, LEG_BONES + ["spine"], weightFn=wf, desc=desc, body=body)


# ───────────────────────── 다리·발 ─────────────────────────

def PartLegs(kind):
	left = MeshBuilder()
	hip, kn, an = J["hip"], J["knee"], J["ankle"]
	path = [V(0.088, 0.004, 1.0), hip, hip.lerp(kn, 0.5), kn.lerp(hip, 0.12), kn, kn.lerp(an, 0.3), kn.lerp(an, 0.7), an + V(0, 0, 0.015)]
	offs = [(0, 0, 0)] * 5 + [(0, 0.014, 0), (0, 0.006, 0), (0, 0, 0)]
	radii = [0.08, 0.086, 0.072, 0.056, 0.05, 0.055, 0.042, 0.034]
	desc = "맨다리"
	if kind == "Goat":
		radii = [0.09, 0.098, 0.085, 0.06, 0.05, 0.05, 0.036, 0.03]
		left.Tube(path, radii, 8, "Fur", offsets=offs)
		for k in range(5):
			t = 0.15 + k * 0.17
			p = hip.lerp(kn, t) + V(0.0, 0.03 * ((k % 2) * 2 - 1), 0)
			left.Ico(p, 0.06, "Fur", subdiv=1, jitter=0.2, seed=k)
		left.Ico(kn.lerp(an, 0.35) + V(0, 0.03, 0), 0.05, "Fur", subdiv=1, jitter=0.2, seed=9)
		desc = "털 덮인 염소 다리(판) — 발은 Feet_Hooves"
	else:
		left.Tube(path, radii, 8, "Skin", offsets=offs)
	if kind == "Greaves":
		left.Tube([kn.lerp(an, 0.05), kn.lerp(an, 0.3), kn.lerp(an, 0.7), kn.lerp(an, 0.9)],
			[0.06, 0.066, 0.052, 0.045], 8, "Metal", offsets=[(0, 0.004, 0), (0, 0.014, 0), (0, 0.006, 0), (0, 0, 0)])
		desc = "청동 정강이받이"
	mb = MeshBuilder()
	mb.Merge(left)
	mb.Merge(left.Mirrored())
	return Part("Legs_" + kind, "Legs", mb, LEG_BONES, power=5.0, desc=desc)


def _FootShape(mb, role, grow=0.0, zBottom=0.012, heightScale=1.0):
	x = J["ankle"].x
	g = grow
	hs = heightScale
	rings = []
	for (y, w, zt) in ((0.068, 0.068, 0.06), (0.02, 0.074, 0.10), (-0.05, 0.084, 0.066), (-0.10, 0.09, 0.046)):
		rings.append(Rect((x, y, (zBottom + zt * hs) / 2), (1, 0, 0), (0, 0, 1), w + g, zt * hs - zBottom + g))
	with mb.Rigid("foot.L"):
		mb.Loft(rings, role)
	toe = [Rect((x, -0.10, (zBottom + 0.046 * hs) / 2), (1, 0, 0), (0, 0, 1), 0.09 + g, 0.046 * hs - zBottom + g),
		Rect((x, -0.168, (zBottom + 0.03) / 2), (1, 0, 0), (0, 0, 1), 0.078 + g, 0.03 - zBottom + g)]
	with mb.Rigid("toe.L"):
		mb.Loft(toe, role)


def _Sole(mb, role="Leather"):
	x = J["ankle"].x
	with mb.Rigid("foot.L"):
		mb.Box((x, -0.013, 0.007), (0.094, 0.17, 0.014), role)
	with mb.Rigid("toe.L"):
		mb.Box((x, -0.137, 0.007), (0.09, 0.072, 0.014), role)


def PartFeet(kind):
	left = MeshBuilder()
	x = J["ankle"].x
	desc = ""
	if kind == "Bare":
		_FootShape(left, "Skin", zBottom=0.0)
		desc = "맨발"
	elif kind in ("Sandals", "WingedSandals"):
		_FootShape(left, "Skin", zBottom=0.014)
		_Sole(left)
		with left.Rigid("foot.L"):
			left.Box((x, -0.06, 0.052), (0.096, 0.016, 0.012), "Leather")
			left.Box((x, -0.02, 0.07), (0.088, 0.016, 0.012), "Leather")
			left.Cyl((x, 0.02, 0.105), (x, 0.02, 0.12), 0.046, 0.046, 8, "Leather")
		desc = "가죽 샌들"
		if kind == "WingedSandals":
			with left.Rigid("foot.L"):
				for s in (1, -1):
					for k, (ang, ln) in enumerate(((15, 0.09), (40, 0.075), (65, 0.058))):
						base = V(x + s * 0.045, 0.03, 0.11)
						d = V(s * 0.6, 0.65, math.sin(math.radians(ang)) * 1.4).normalized()
						left.Ellipsoid((0, 0, 0), (0.006, ln / 2, 0.016), 4, 3, "Feather",
							Matrix.Translation(base + d * ln / 2) @ _Align(V(0, 1, 0), d))
			desc = "날개 달린 샌들(헤르메스)"
	elif kind == "Boots":
		_FootShape(left, "Leather", grow=0.008, zBottom=0.0, heightScale=1.05)
		kn, an = J["knee"], J["ankle"]
		with left.Rigid("shin.L"):
			left.Tube([an + V(0, 0, 0.02), kn.lerp(an, 0.62)], [0.046, 0.052], 8, "Leather")
			left.Cyl(kn.lerp(an, 0.66), kn.lerp(an, 0.6), 0.058, 0.058, 8, "Trim")
		desc = "가죽 장화(엔드로미데스)"
	elif kind == "Hooves":
		with left.Rigid("foot.L"):
			left.Tube([J["ankle"] + V(0, 0.0, 0.03), V(x, -0.05, 0.04)], [0.036, 0.04], 8, "Fur")
		for s in (1, -1):
			with left.Rigid("toe.L"):
				left.Frustum((x + s * 0.019, -0.10, 0.0), (0.034, 0.08), (x + s * 0.017, -0.085, 0.05), (0.028, 0.05), "Horn")
		desc = "갈라진 염소 발굽(판)"
	mb = MeshBuilder()
	mb.Merge(left)
	mb.Merge(left.Mirrored())
	return Part("Feet_" + kind, "Feet", mb, desc=desc)


# ───────────────────────── 등(망토) ─────────────────────────

def PartBack(kind):
	mb = MeshBuilder()
	if kind == "CapeShort":
		levels = [(1.475, 0.02, 100), (1.40, 0.03, 102), (1.28, 0.034, 106), (1.14, 0.04, 110), (1.00, 0.05, 114)]
		desc = "짧은 망토(클라미스 — 오른 어깨 금핀)"
	else:
		levels = [(1.475, 0.02, 100), (1.38, 0.03, 104), (1.20, 0.04, 108), (0.95, 0.06, 112), (0.65, 0.08, 116), (0.38, 0.1, 120)]
		desc = "발목 가까이 늘어진 긴 망토"
	grid = []
	for (z, gap, openDeg) in levels:
		zz = max(z, 1.0)
		cy, rx, ry = TorsoRing(BODY_M, min(zz, 1.44))
		row = []
		for j in range(11):
			th = math.radians(-openDeg + 2 * openDeg * j / 10)
			rr = 1.0 + (1.0 - z) * 0.25 if z < 1.0 else 1.0
			row.append(V((rx + gap) * math.sin(th) * rr, cy + (ry + gap) * math.cos(th) * (1.0 + max(0.0, 1.0 - z) * 0.4), z))
		grid.append(row)
	mb.Sheet(grid, 0.014, "ClothB")
	with mb.Rigid("chest"):
		mb.Ico((-0.16, -0.07, 1.45), 0.017, "Gold", subdiv=1)
	return Part("Back_" + kind, "Back", mb, ["neck", "chest", "spine", "pelvis", "clavicle.L", "clavicle.R"],
		factors={"pelvis": 0.6}, power=3.0, desc=desc)


# ───────────────────────── 장신구 ─────────────────────────

def PartAccessory(kind):
	mb = MeshBuilder()
	bones = ["chest", "spine", "pelvis"]
	desc = ""
	if kind == "Apron":
		grid = []
		for z in (1.36, 1.26, 1.14, 1.02, 0.90, 0.78, 0.66):
			if z > 1.0:
				cy, rx, ry = TorsoRing(BODY_M, z)
			else:
				cy, rx, ry = 0.01, 0.17, 0.125
			row = []
			for j in range(7):
				x = -0.13 + 0.26 * j / 6
				xr = max(-0.95, min(0.95, x / (rx + 0.03)))
				row.append(V(x, cy - (ry + 0.028) * math.sqrt(1 - xr * xr), z))
			grid.append(row)
		mb.Sheet(grid, 0.012, "Leather")
		for s in (1, -1):
			mb.Tube([V(s * 0.12, -0.10, 1.36), V(s * 0.09, -0.02, 1.47), V(0, 0.07, 1.48), V(-s * 0.09, -0.02, 1.47)][:3], [0.008] * 3, 4, "Leather")
		desc = "대장장이 가죽 앞치마(헤파이스토스)"
		bones = ["chest", "spine", "pelvis", "thigh.L", "thigh.R"]
	elif kind == "Satchel":
		front = [V(-0.17, -0.07, 1.44), V(-0.07, -0.13, 1.33), V(0.05, -0.135, 1.2), V(0.15, -0.12, 1.07)]
		back = [V(-0.17, 0.08, 1.44), V(-0.07, 0.115, 1.33), V(0.05, 0.115, 1.2), V(0.15, 0.10, 1.07)]
		mb.Tube(front, [0.011] * 4, 4, "Leather")
		mb.Tube(back, [0.011] * 4, 4, "Leather")
		mb.Tube([V(-0.17, -0.07, 1.44), V(-0.2, 0.0, 1.46), V(-0.17, 0.08, 1.44)], [0.011] * 3, 4, "Leather")
		mb.Box((0.2, -0.01, 0.98), (0.065, 0.17, 0.15), "Leather")
		mb.Box((0.235, -0.01, 1.02), (0.012, 0.17, 0.08), "Trim")
		desc = "어깨에 멘 편지 가방(헤르메스·상인)"
		bones = ["chest", "spine", "pelvis"]
	elif kind == "Shackles":
		el, wr = J["elbow"], J["wrist"]
		left = MeshBuilder()
		with left.Rigid("forearm.L"):
			left.Cyl(el.lerp(wr, 0.78), el.lerp(wr, 0.95), 0.045, 0.045, 8, "Iron")
			for k in range(3):
				c = el.lerp(wr, 0.86) + V(0.0, 0.02 + k * 0.035, -0.04 - k * 0.012)
				left.Cyl(c + V(-0.012, 0, 0), c + V(0.012, 0, 0), 0.016, 0.016, 6, "Iron", up=(0, 1, 0))
		mb.Merge(left)
		mb.Merge(left.Mirrored())
		desc = "쇠고랑과 끊어진 사슬(프로메테우스)"
		bones = None
	elif kind == "Wraps":
		for z in (1.10, 1.22, 1.34):
			cy, rx, ry = TorsoRing(BODY_M, z)
			_Band(mb, (0, cy, 0), rx + 0.03, ry + 0.03, z - 0.02, z + 0.015, 0.014, 12, "Accent")
		desc = "몸에 감은 천 띠(죽지 못하는 '돌아온 자들')"
	return Part("Acc_" + kind, "Accessory", mb, bones, power=4.0, desc=desc)


# ───────────────────────── 손에 드는 소품 ─────────────────────────
# 소품 좌표: 쥐는 점이 원점, +Y = 주먹을 관통하는 축(창끝 방향), +Z = 위(쉬는 자세 기준).
# 들고 있는 자세(Idle)에서는 +Y가 하늘을 향하고, 찌르기에서는 앞을 향함.

def _Prop_Spear(mb):
	mb.Cyl((0, -0.78, 0), (0, 1.06, 0), 0.014, 0.013, 6, "Wood")
	mb.Cyl((0, -0.78, 0), (0, -0.90, 0), 0.016, 0.003, 6, "Bronze")
	mb.Tube([V(0, 1.03, 0), V(0, 1.10, 0), V(0, 1.24, 0), V(0, 1.33, 0)], [(0.015, 0.015), (0.008, 0.042), (0.006, 0.03), (0, 0)], 4, "Bronze")
	mb.Cyl((0, 1.0, 0), (0, 1.04, 0), 0.019, 0.017, 6, "Bronze")


def _Prop_Trident(mb, metal="Gold", wood="Gold"):
	mb.Cyl((0, -0.8, 0), (0, 1.0, 0), 0.015, 0.015, 6, wood)
	mb.Cyl((-0.095, 1.0, 0), (0.095, 1.0, 0), 0.016, 0.016, 6, metal, up=(0, 1, 0))
	for x, top in ((0, 1.30), (-0.088, 1.23), (0.088, 1.23)):
		mb.Cyl((x, 0.99, 0), (x, top, 0), 0.012, 0.0, 5, metal)
		mb.Cyl((x, top - 0.08, 0), (x - math.copysign(0.025, x if x else 1), top - 0.12, 0), 0.008, 0.0, 4, metal)
	mb.Ico((0, 0.95, 0), 0.03, metal, subdiv=1)


def _Prop_Bident(mb):
	mb.Cyl((0, -0.8, 0), (0, 1.0, 0), 0.015, 0.015, 6, "WoodDark")
	mb.Cyl((-0.06, 1.0, 0), (0.06, 1.0, 0), 0.016, 0.016, 6, "DarkIron", up=(0, 1, 0))
	for x in (-0.055, 0.055):
		mb.Cyl((x, 0.99, 0), (x, 1.25, 0), 0.013, 0.0, 5, "DarkIron")
	mb.Ico((0, 0.96, 0), 0.03, "Pomegranate", subdiv=1)


def _Prop_ScepterEagle(mb):
	mb.Cyl((0, -0.6, 0), (0, 0.72, 0), 0.013, 0.012, 6, "Gold")
	mb.Ico((0, 0.76, 0), 0.034, "Gold", subdiv=1)
	mb.Ellipsoid((0, 0.82, 0), (0.022, 0.03, 0.022), 6, 4, "Gold")
	mb.Cyl((0, 0.84, -0.01), (0, 0.86, -0.04), 0.014, 0.0, 4, "Gold")
	for s in (1, -1):
		mb.Prism([(0, 0), (s * 0.09, 0.05), (s * 0.12, 0.02), (s * 0.08, -0.01)], 0.01, "Gold", Mt((0, 0.8, 0)))
	mb.Cyl((0, -0.6, 0), (0, -0.66, 0), 0.016, 0.006, 6, "Gold")


def _Prop_ScepterLotus(mb):
	mb.Cyl((0, -0.6, 0), (0, 0.7, 0), 0.012, 0.012, 6, "Gold")
	mb.Lathe([(0, 0), (0.035, 0.03), (0.05, 0.08), (0.03, 0.075), (0, 0.04)], 8, "Gold", Mt((0, 0.7, 0), (-90, 0, 0)))
	mb.Ellipsoid((0, 0.80, -0.005), (0.018, 0.022, 0.026), 6, 4, "Peacock")
	mb.Cyl((0, 0.80, 0.02), (0, 0.80, 0.04), 0.008, 0.0, 4, "Gold")


def _Prop_Thunderbolt(mb):
	for s in (1, -1):
		pts = [(0, 0.03 * s), (0.03, 0.12 * s), (-0.025, 0.17 * s), (0.035, 0.27 * s), (-0.01, 0.30 * s), (0.0, 0.40 * s)]
		path = [V(px, py, 0) for (px, py) in pts]
		mb.Tube(path, [0.03, 0.026, 0.024, 0.018, 0.014, 0.0], 5, "Lightning")
		for k in (1, -1):
			mb.Cyl((0, 0.26 * s, 0), (0.07 * k, 0.33 * s, 0.02), 0.012, 0.0, 4, "Lightning")
	mb.Cyl((0, -0.045, 0), (0, 0.045, 0), 0.026, 0.026, 8, "Gold")


def _Prop_Caduceus(mb):
	mb.Cyl((0, -0.26, 0), (0, 0.45, 0), 0.009, 0.009, 6, "Gold")
	mb.Ico((0, 0.47, 0), 0.02, "Gold", subdiv=1)
	for ph in (0, math.pi):
		path = []
		for i in range(13):
			t = i / 12
			a = ph + t * math.pi * 3
			path.append(V(math.cos(a) * 0.028, -0.1 + t * 0.48, math.sin(a) * 0.028))
		mb.Tube(path, [0.007] * 12 + [0.004], 5, "Bronze")
		mb.Ellipsoid(path[-1] + V(0, 0.012, 0), (0.01, 0.016, 0.008), 5, 3, "Bronze")
	for s in (1, -1):
		mb.Prism([(0, 0), (s * 0.07, 0.035), (s * 0.085, 0.01), (s * 0.06, -0.01)], 0.008, "Feather", Mt((0, 0.43, 0)))


def _Prop_Hammer(mb):
	mb.Cyl((0, -0.20, 0), (0, 0.30, 0), 0.016, 0.014, 6, "Wood")
	mb.Box((0, 0.33, 0.015), (0.06, 0.07, 0.16), "Iron")
	mb.Box((0, 0.33, 0.1), (0.066, 0.076, 0.012), "Iron")


def _Prop_Staff(mb):
	path = [V(0, -1.0, 0), V(0.008, -0.6, 0.005), V(-0.006, -0.2, 0), V(0.005, 0.25, 0.006), V(0, 0.6, 0), V(0.03, 0.72, 0.01)]
	mb.Tube(path, [0.016, 0.018, 0.019, 0.019, 0.021, 0.016], 6, "Wood")
	mb.Ico((0.03, 0.74, 0.01), 0.032, "Wood", subdiv=1, jitter=0.2, seed=3)


def _Prop_Thyrsus(mb):
	mb.Cyl((0, -0.95, 0), (0, 0.66, 0), 0.013, 0.013, 6, "WoodLight")
	mb.Ellipsoid((0, 0.73, 0), (0.032, 0.065, 0.032), 7, 5, "Brown")
	for i in range(5):
		a = i * 1.3
		p = V(math.cos(a) * 0.02, 0.62 - i * 0.07, math.sin(a) * 0.02)
		mb.Ellipsoid(p + V(math.cos(a) * 0.02, 0, math.sin(a) * 0.02), (0.022, 0.014, 0.005), 4, 2, "Ivy")


def _Prop_WineCup(mb):
	M = Mt((0, 0.02, 0), (-90, 0, 0))
	mb.Lathe([(0, -0.035), (0.03, -0.035), (0.01, -0.025), (0.009, 0.0), (0.04, 0.02), (0.052, 0.07), (0.048, 0.072), (0.0, 0.06)], 10, "Gold", M)
	mb.Cyl((0, 0.08, 0), (0, 0.082, 0), 0.044, 0.044, 10, "Wine")
	for s in (1, -1):
		path = [V(s * 0.045, 0.075, 0), V(s * 0.075, 0.09, 0), V(s * 0.07, 0.04, 0), V(s * 0.04, 0.03, 0)]
		mb.Tube(path, [0.006] * 4, 4, "Gold")


def _Prop_Apple(mb):
	mb.Ellipsoid((0, 0.02, 0.0), (0.036, 0.034, 0.036), 8, 5, "AppleGold")
	mb.Cyl((0, 0.052, 0), (0, 0.07, 0.004), 0.004, 0.003, 4, "WoodDark")
	mb.Ellipsoid((0.01, 0.066, 0.01), (0.012, 0.004, 0.006), 4, 2, "Laurel")


def _Prop_Mirror(mb):
	mb.Cyl((0, -0.06, 0), (0, 0.09, 0), 0.011, 0.01, 6, "Bronze")
	mb.Cyl((-0.006, 0.17, 0), (0.006, 0.17, 0), 0.072, 0.072, 14, "Bronze", up=(0, 1, 0))
	mb.Cyl((-0.0075, 0.17, 0), (-0.007, 0.17, 0), 0.062, 0.062, 14, "Silver", up=(0, 1, 0))


def _Prop_Lyre(mb):
	mb.Ellipsoid((0, 0.06, 0), (0.08, 0.06, 0.03), 8, 4, "WoodDark")
	for s in (1, -1):
		path = [V(s * 0.05, 0.08, 0), V(s * 0.085, 0.16, 0), V(s * 0.075, 0.26, 0), V(s * 0.1, 0.31, 0)]
		mb.Tube(path, [0.012, 0.012, 0.011, 0.008], 5, "Horn")
	mb.Cyl((-0.09, 0.27, 0), (0.09, 0.27, 0), 0.008, 0.008, 5, "Wood", up=(0, 1, 0))
	for k in range(5):
		x = -0.03 + k * 0.015
		mb.Box((x, 0.17, 0.0), (0.0025, 0.2, 0.0025), "Gold")


def _Prop_Pomegranate(mb):
	mb.Ellipsoid((0, 0.025, 0), (0.04, 0.038, 0.04), 8, 5, "Pomegranate")
	mb.Cyl((0, 0.06, 0), (0, 0.08, 0), 0.012, 0.016, 6, "Pomegranate")


def _Prop_Shield(mb):
	# 앞면(볼록한 쪽)은 소품 -X = 왼팔 바깥쪽, 손은 방패 안쪽 손잡이를 쥠
	M = Mt((-0.07, 0.0, 0.0), (0, -90, 0))
	mb.Lathe([(0.0, 0.0), (0.31, 0.0), (0.345, 0.01), (0.345, 0.028), (0.315, 0.042), (0.16, 0.07), (0.0, 0.078)], 16, "Bronze", M)
	mb.Lathe([(0.0, 0.0), (0.13, 0.0), (0.12, 0.007), (0.0, 0.01)], 12, "BlackFigure", Mt((-0.142, 0.0, 0.0), (0, -90, 0)))
	for s in (1, -1):
		mb.Ellipsoid((-0.153, 0.0, s * 0.04), (0.006, 0.024, 0.024), 8, 4, "Crest")
	mb.Box((-0.045, 0.0, 0.0), (0.05, 0.03, 0.12), "Leather")


def _Prop_Sword(mb):
	mb.Cyl((0, -0.06, 0), (0, 0.055, 0), 0.016, 0.016, 6, "LeatherDark")
	mb.Ico((0, -0.07, 0), 0.022, "Bronze", subdiv=1)
	mb.Box((0, 0.065, 0), (0.1, 0.018, 0.026), "Bronze")
	mb.Tube([V(0, 0.07, 0), V(0, 0.25, 0), V(0, 0.43, 0), V(0, 0.55, 0)], [(0.006, 0.026), (0.006, 0.022), (0.005, 0.03), (0, 0)], 4, "Iron")


def _Prop_Torch(mb):
	mb.Cyl((0, -0.32, 0), (0, 0.36, 0), 0.017, 0.022, 6, "Wood")
	mb.Lathe([(0.0, 0.0), (0.045, 0.0), (0.05, 0.06), (0.0, 0.05)], 8, "Bronze", Mt((0, 0.34, 0), (-90, 0, 0)))
	mb.Lathe([(0.0, 0.0), (0.04, 0.02), (0.035, 0.08), (0.015, 0.15), (0.0, 0.2)], 7, "Fire", Mt((0, 0.38, 0), (-90, 0, 0)))


def _Prop_Spindle(mb):
	mb.Cyl((0, -0.30, 0), (0, 0.36, 0), 0.008, 0.008, 5, "WoodLight")
	mb.Cyl((0, -0.22, 0), (0, -0.20, 0), 0.034, 0.034, 10, "WoodDark")
	mb.Ico((0, 0.36, 0), 0.05, "Wool", subdiv=1, jitter=0.15, seed=4)
	path = [V(0.03, 0.34, 0.02), V(0.07, 0.2, 0.04), V(0.06, 0.0, 0.03), V(0.02, -0.18, 0.0)]
	mb.Tube(path, [0.004] * 4, 4, "GlowGold")


def _Prop_Rod(mb):
	mb.Cyl((0, -0.45, 0), (0, 0.45, 0), 0.011, 0.011, 6, "WoodLight")
	for k in range(9):
		y = -0.4 + k * 0.1
		mb.Cyl((0, y, 0), (0, y + 0.012, 0), 0.0135, 0.0135, 6, "Gold")


def _Prop_Shears(mb):
	for s in (1, -1):
		mb.Prism([(-0.004, 0.06), (0.012, 0.06), (0.008, 0.36), (0.0, 0.38)], 0.006, "Iron", Mt((s * 0.006, 0, 0), (0, 0, s * 3)))
		mb.Tube([V(s * 0.01, 0.06, 0), V(s * 0.04, 0.02, 0), V(s * 0.035, -0.04, 0), V(s * 0.01, -0.03, 0), V(s * 0.008, 0.05, 0)],
			[0.007] * 5, 4, "Iron")
	mb.Cyl((0, 0.06, -0.01), (0, 0.06, 0.01), 0.01, 0.01, 6, "Gold", up=(0, 1, 0))


def _Prop_Harpe(mb):
	mb.Cyl((0, -0.12, 0), (0, 0.16, 0), 0.018, 0.016, 6, "LeatherDark")
	pts = [(0.012, 0.16), (0.016, 0.45), (0.0, 0.62), (-0.06, 0.72), (-0.16, 0.73), (-0.24, 0.66), (-0.17, 0.68), (-0.08, 0.65),
		(-0.03, 0.56), (-0.012, 0.42), (-0.012, 0.16)]
	M = FrameMatrix(V(0, 0, 0), V(0, 0, 1), V(0, 1, 0), V(-1, 0, 0))
	mb.Prism(pts, 0.008, "DarkIron", M)


def _Prop_PanPipes(mb):
	for k in range(7):
		x = -0.045 + k * 0.015
		ln = 0.17 - k * 0.018
		mb.Cyl((x, -0.02, 0), (x, -0.02 + ln, 0), 0.0075, 0.0075, 6, "Reed")
	mb.Box((0, 0.03, 0), (0.115, 0.018, 0.02), "Leather")


def _Prop_Scroll(mb):
	mb.Cyl((0, -0.13, 0), (0, 0.13, 0), 0.026, 0.026, 8, "Parchment")
	for y in (-0.14, 0.14):
		mb.Cyl((0, y - 0.01, 0), (0, y + 0.01, 0), 0.012, 0.012, 6, "Wood")
	mb.Cyl((0, -0.02, 0), (0, 0.02, 0), 0.028, 0.028, 8, "Crest")


def _Prop_Laurel(mb):
	mb.Tube([V(0, -0.1, 0), V(0.01, 0.15, 0.0), V(0.0, 0.32, 0.01)], [0.007, 0.006, 0.003], 4, "WoodDark")
	for k in range(7):
		y = 0.0 + k * 0.045
		s = 1 if k % 2 else -1
		d = V(s * 0.8, 0.6, 0.15)
		mb.Ellipsoid((0, 0, 0), (0.014, 0.03, 0.004), 4, 2, "Laurel", Matrix.Translation(V(s * 0.02, y + 0.02, 0)) @ _Align(V(0, 1, 0), d))


def _Prop_Ember(mb):
	mb.Ico((0, 0.0, 0.0), 0.034, "Ember", subdiv=1, jitter=0.2, seed=8)
	mb.Lathe([(0.0, 0.0), (0.025, 0.02), (0.02, 0.06), (0.0, 0.1)], 6, "Fire", Mt((0, 0.02, 0), (-90, 0, 0)))


PROPS = {
	"Spear": (_Prop_Spear, "청동 창(주인공·병사·아테나)"),
	"Trident": (_Prop_Trident, "황금 삼지창(포세이돈)"),
	"Bident": (_Prop_Bident, "검은 쇠 두 갈래 창(하데스)"),
	"Scepter_Eagle": (_Prop_ScepterEagle, "독수리 홀(제우스)"),
	"Scepter_Lotus": (_Prop_ScepterLotus, "연꽃·뻐꾸기 홀(헤라)"),
	"Thunderbolt": (_Prop_Thunderbolt, "번개 다발(제우스, 빛남)"),
	"Caduceus": (_Prop_Caduceus, "뱀 두 마리 감긴 전령 지팡이(헤르메스)"),
	"Hammer": (_Prop_Hammer, "대장장이 망치(헤파이스토스)"),
	"Staff": (_Prop_Staff, "나무 지팡이(노파·순례객·양치기)"),
	"Thyrsus": (_Prop_Thyrsus, "솔방울 지팡이 튀르소스(디오니소스)"),
	"WineCup": (_Prop_WineCup, "손잡이 둘 달린 금 술잔 칸타로스(디오니소스·아폴론)"),
	"Apple_Golden": (_Prop_Apple, "금빛 사과(에리스, 빛남)"),
	"Mirror": (_Prop_Mirror, "청동 손거울(아프로디테)"),
	"Lyre": (_Prop_Lyre, "리라(아폴론)"),
	"Pomegranate": (_Prop_Pomegranate, "석류(페르세포네)"),
	"Shield_Round": (_Prop_Shield, "둥근 청동 방패 아스피스 — 왼팔용"),
	"Sword_Xiphos": (_Prop_Sword, "잎 모양 칼 크시포스(아레스)"),
	"Torch": (_Prop_Torch, "횃불(프로메테우스, 불꽃 빛남)"),
	"Spindle": (_Prop_Spindle, "물렛가락과 금실(클로토)"),
	"MeasuringRod": (_Prop_Rod, "실 재는 막대(라케시스)"),
	"Shears": (_Prop_Shears, "운명의 가위(아트로포스)"),
	"Harpe": (_Prop_Harpe, "낫 하르페(크로노스)"),
	"PanPipes": (_Prop_PanPipes, "갈대 피리 시링크스(판)"),
	"Scroll": (_Prop_Scroll, "두루마리(하데스의 서류·철학자·기록관)"),
	"Laurel_Branch": (_Prop_Laurel, "월계수 가지(피티아)"),
	"Ember": (_Prop_Ember, "손바닥 위 불씨(화로의 신)"),
}


def PartProp(name, side, gripMatrix):
	fn, desc = PROPS[name]
	sub = MeshBuilder()
	fn(sub)
	mb = MeshBuilder()
	bone = "prop." + side
	with mb.Rigid(bone):
		mb.Merge(sub, gripMatrix)
	return Part("Prop_%s.%s" % (name, side), "Prop" + side, mb, desc=desc)


# ───────────────────────── 전체 목록 ─────────────────────────

def AllParts(armature):
	parts = []
	for b in (BODY_M, BODY_F):
		parts.append(PartHead(b))
	for style in FACE_STYLES:
		parts.append(PartFace(style))
	for style in ("Short", "Curly", "Wild", "Bun", "Long", "LongFem"):
		parts.append(PartHair(style))
	for style in ("Full", "Short", "Long", "Goatee"):
		parts.append(PartBeard(style))
	for kind in ("Helm_Crested", "Helm_Athena", "Helm_Ares", "Crown_Gold", "Crown_Spiked", "Crown_Polos",
		"Wreath_Laurel", "Wreath_Rose", "Wreath_IvyGrape", "Wreath_Pomegranate", "Hat_PetasosWinged", "Cap_Pilos",
		"Hood", "Veil", "Horns_Goat", "Mask_Party"):
		parts.append(PartHeadwear(kind))
	for kind, bodies in (("Bare", (BODY_M,)), ("Tunic", (BODY_M, BODY_F)), ("Robe", (BODY_M, BODY_F)), ("Exomis", (BODY_M,)),
		("Himation", (BODY_M,)), ("Dress", (BODY_F,)), ("Cuirass", (BODY_M,)), ("Aegis", (BODY_F,))):
		for b in bodies:
			parts.append(PartTorso(kind, b))
	for kind in ("Bare", "ShortSleeve", "LongSleeve", "Bracers"):
		parts.append(PartArms(kind))
	parts.append(PartHands())
	for kind, bodies in (("ShortSkirt", (BODY_M, BODY_F)), ("LongRobe", (BODY_M, BODY_F)), ("Pteruges", (BODY_M,)),
		("Loincloth", (BODY_M,)), ("FurHips", (BODY_M,))):
		for b in bodies:
			parts.append(PartLower(kind, b))
	for kind in ("Bare", "Greaves", "Goat"):
		parts.append(PartLegs(kind))
	for kind in ("Bare", "Sandals", "WingedSandals", "Boots", "Hooves"):
		parts.append(PartFeet(kind))
	for kind in ("CapeShort", "CapeLong"):
		parts.append(PartBack(kind))
	for kind in ("Apron", "Satchel", "Shackles", "Wraps"):
		parts.append(PartAccessory(kind))
	gripR = armature.data.bones["prop.R"].matrix_local.copy()
	gripL = armature.data.bones["prop.L"].matrix_local.copy()
	for name in PROPS:
		parts.append(PartProp(name, "R", gripR))
	for name in ("Shield_Round", "Apple_Golden", "Scepter_Eagle", "Lyre", "Thyrsus", "Scroll", "Torch", "WineCup", "Staff"):
		parts.append(PartProp(name, "L", gripL))
	return parts
