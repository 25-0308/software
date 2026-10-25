# -*- coding: utf-8 -*-
# 네 발 짐승 — 몸틀(Frame) 셋과 갈아 끼우는 부위.
#  Canid  : 개과·고양이과처럼 발가락으로 걷는 포식자(늑대·사자) — 낮고 긴 몸, 깊은 가슴
#  Cervid : 사슴·양처럼 가는 다리로 발굽을 디디는 초식동물 — 높은 어깨, 위로 솟은 목
#  Heavy  : 멧돼지·소처럼 짧고 굵은 다리와 무거운 몸통
# 같은 몸틀 안에서는 머리·꼬리·귀·뿔·몸통을 자유롭게 바꿔 끼울 수 있음(뼈 이름은 세 몸틀이 모두 같음).
# 크기는 종(프리셋)마다 뼈대 전체 배율로 정함(멧돼지 = Heavy × 0.62 등).
import math
from mathutils import Vector, Matrix
from rescommon import MeshBuilder, BoneSpec, MirrorSpecs, FrameMatrix, Mt, V

ROLE_DEFAULTS = {
	"Fur": "WolfGray", "FurDark": "WolfDark", "FurLight": "WolfLight", "Mane": "LionMane", "Eye": "AnimalEye",
	"Nose": "Nose", "Horn": "Horn", "Hoof": "Hoof",
}


def _F(**k):
	return {n: V(p) for n, p in k.items()}


FRAMES = {
	"Canid": _F(
		pelvisH=(0, 0.40, 0.70), pelvisT=(0, 0.20, 0.72), spineT=(0, -0.05, 0.73), chestT=(0, -0.30, 0.72),
		neckH=(0, -0.30, 0.74), neckT=(0, -0.45, 0.86), headT=(0, -0.71, 0.82), jawH=(0, -0.53, 0.80), jawT=(0, -0.69, 0.775),
		earH=(0.048, -0.47, 0.925), earT=(0.06, -0.46, 1.01),
		tail0=(0, 0.42, 0.70), tail1=(0, 0.55, 0.62), tail2=(0, 0.66, 0.50), tail3=(0, 0.72, 0.36),
		scapH=(0.08, -0.22, 0.77), shoulder=(0.10, -0.32, 0.52), elbow=(0.10, -0.24, 0.34), wrist=(0.10, -0.28, 0.10),
		fetF=(0.10, -0.31, 0.035), toeF=(0.10, -0.37, 0.015),
		hipJ=(0.09, 0.36, 0.62), stifle=(0.10, 0.26, 0.40), hock=(0.10, 0.42, 0.20), fetH=(0.10, 0.38, 0.035), toeH=(0.10, 0.32, 0.015)),
	"Cervid": _F(
		pelvisH=(0, 0.42, 0.98), pelvisT=(0, 0.22, 1.00), spineT=(0, -0.05, 1.00), chestT=(0, -0.32, 0.98),
		neckH=(0, -0.32, 1.02), neckT=(0, -0.50, 1.32), headT=(0, -0.76, 1.24), jawH=(0, -0.58, 1.25), jawT=(0, -0.74, 1.20),
		earH=(0.05, -0.50, 1.39), earT=(0.15, -0.48, 1.45),
		tail0=(0, 0.45, 0.99), tail1=(0, 0.52, 0.94), tail2=(0, 0.555, 0.88), tail3=(0, 0.565, 0.83),
		scapH=(0.08, -0.25, 1.03), shoulder=(0.10, -0.33, 0.74), elbow=(0.10, -0.25, 0.58), wrist=(0.10, -0.29, 0.30),
		fetF=(0.10, -0.31, 0.07), toeF=(0.10, -0.36, 0.015),
		hipJ=(0.09, 0.40, 0.88), stifle=(0.10, 0.28, 0.62), hock=(0.10, 0.46, 0.38), fetH=(0.10, 0.42, 0.07), toeH=(0.10, 0.37, 0.015)),
	"Heavy": _F(
		pelvisH=(0, 0.58, 0.88), pelvisT=(0, 0.30, 0.90), spineT=(0, -0.05, 0.94), chestT=(0, -0.40, 0.94),
		neckH=(0, -0.40, 0.86), neckT=(0, -0.64, 0.78), headT=(0, -1.00, 0.54), jawH=(0, -0.74, 0.62), jawT=(0, -0.97, 0.50),
		earH=(0.11, -0.66, 0.82), earT=(0.24, -0.64, 0.80),
		tail0=(0, 0.62, 0.88), tail1=(0, 0.70, 0.72), tail2=(0, 0.72, 0.52), tail3=(0, 0.72, 0.34),
		scapH=(0.14, -0.32, 0.89), shoulder=(0.16, -0.42, 0.60), elbow=(0.16, -0.34, 0.44), wrist=(0.16, -0.37, 0.22),
		fetF=(0.16, -0.38, 0.07), toeF=(0.16, -0.44, 0.015),
		hipJ=(0.15, 0.52, 0.78), stifle=(0.16, 0.40, 0.52), hock=(0.16, 0.55, 0.32), fetH=(0.16, 0.52, 0.07), toeH=(0.16, 0.46, 0.015)),
}

LEG_NAMES = {"FL": ("upperarm", "forearm", "frontfoot", "fronttoe"), "HL": ("thigh", "shin", "hindfoot", "hindtoe")}


def SkeletonSpecs(F):
	s = [
		BoneSpec("root", (0, 0, 0), (0, -0.25, 0)),
		BoneSpec("pelvis", F["pelvisH"], F["pelvisT"], "root"),
		BoneSpec("spine", F["pelvisT"], F["spineT"], "pelvis"),
		BoneSpec("chest", F["spineT"], F["chestT"], "spine"),
		BoneSpec("neck", F["neckH"], F["neckT"], "chest"),
		BoneSpec("head", F["neckT"], F["headT"], "neck"),
		BoneSpec("jaw", F["jawH"], F["jawT"], "head"),
		BoneSpec("ear.L", F["earH"], F["earT"], "head"),
		BoneSpec("tail1", F["tail0"], F["tail1"], "pelvis"),
		BoneSpec("tail2", F["tail1"], F["tail2"], "tail1"),
		BoneSpec("tail3", F["tail2"], F["tail3"], "tail2"),
		BoneSpec("scapula.L", F["scapH"], F["shoulder"], "chest"),
		BoneSpec("upperarm.L", F["shoulder"], F["elbow"], "scapula.L"),
		BoneSpec("forearm.L", F["elbow"], F["wrist"], "upperarm.L"),
		BoneSpec("frontfoot.L", F["wrist"], F["fetF"], "forearm.L"),
		BoneSpec("fronttoe.L", F["fetF"], F["toeF"], "frontfoot.L"),
		BoneSpec("thigh.L", F["hipJ"], F["stifle"], "pelvis"),
		BoneSpec("shin.L", F["stifle"], F["hock"], "thigh.L"),
		BoneSpec("hindfoot.L", F["hock"], F["fetH"], "shin.L"),
		BoneSpec("hindtoe.L", F["fetH"], F["toeH"], "hindfoot.L"),
	]
	return MirrorSpecs(s)


BODY_BONES = ["pelvis", "spine", "chest", "neck", "head", "tail1", "scapula.L", "scapula.R", "thigh.L", "thigh.R"]
LEG_BONES = ["chest", "pelvis", "scapula.L", "scapula.R", "upperarm.L", "upperarm.R", "forearm.L", "forearm.R",
	"frontfoot.L", "frontfoot.R", "fronttoe.L", "fronttoe.R", "thigh.L", "thigh.R", "shin.L", "shin.R",
	"hindfoot.L", "hindfoot.R", "hindtoe.L", "hindtoe.R"]
TAIL_BONES = ["pelvis", "tail1", "tail2", "tail3"]


class QPart:
	def __init__(self, name, slot, frame, mb, bones=None, power=5.0, desc="", factors=None):
		self.name = name
		self.slot = slot
		self.frame = frame
		self.mb = mb
		self.bones = bones
		self.power = power
		self.desc = desc
		self.factors = factors
		self.weightFn = None


def RingXZ(y, zc, rx, rU, rD, n=10, x0=0.0):
	pts = []
	for k in range(n):
		a = -math.pi / 2 + 2 * math.pi * k / n
		s = math.sin(a)
		z = zc + (rU if s > 0 else rD) * s
		pts.append(V(x0 + rx * math.cos(a), y, z))
	return pts


def _BodyLoft(mb, rings, role="Fur", belly=0.55, back=0.7, n=10):
	start = mb.FaceCount()
	mb.Loft([RingXZ(*r, n=n) for r in rings], role)
	centers = [(r[0], r[1], r[3], r[4]) for r in rings]

	def Zc(y):
		for i in range(len(centers) - 1):
			a, b = centers[i], centers[i + 1]
			if min(a[0], b[0]) <= y <= max(a[0], b[0]):
				t = (y - a[0]) / (b[0] - a[0]) if b[0] != a[0] else 0
				return a[1] + (b[1] - a[1]) * t, a[2] + (b[2] - a[2]) * t, a[3] + (b[3] - a[3]) * t
		c = centers[0] if y > centers[0][0] else centers[-1]
		return c[1], c[2], c[3]

	def PaintFn(c):
		zc, rU, rD = Zc(c.y)
		if belly and c.z < zc - rD * belly:
			return "FurLight"
		if back and c.z > zc + rU * back:
			return "FurDark"
		return None
	mb.Paint(start, PaintFn)


def _HeadFrame(F):
	o = F["neckT"]
	f = (F["headT"] - o).normalized()
	z = V(0, 0, 1)
	up = (z - f * z.dot(f)).normalized()
	return FrameMatrix(o, f.cross(up), f, up)


def _HeadLoft(mb, rings, role, M, n=8):
	"""머리 로컬: (앞 거리 d, 가로 반지름, 위 반지름, 아래 반지름, 높이 보정)"""
	rr = []
	for (d, rx, rU, rD, dz) in rings:
		if rx <= 0:
			rr.append([V(0, d, dz)])
			continue
		ring = []
		for k in range(n):
			a = -math.pi / 2 + 2 * math.pi * k / n
			s = math.sin(a)
			ring.append(V(rx * math.cos(a), d, dz + (rU if s > 0 else rD) * s))
		rr.append(ring)
	start = mb.FaceCount()
	mb.Loft(rr, role, M=M)
	return start


def _Eyes(mb, M, d, x, z, size=0.018):
	for s in (1, -1):
		mb.Ellipsoid((s * x, d, z), (size * 0.5, size * 0.7, size * 0.6), 6, 4, "Eye", M)


# ───────────────────────── 몸통 ─────────────────────────

BODY_RINGS = {
	("Canid", "Lean"): [(0.50, 0.70, 0.05, 0.04, 0.05), (0.45, 0.70, 0.10, 0.07, 0.11), (0.33, 0.705, 0.12, 0.08, 0.13),
		(0.16, 0.715, 0.105, 0.075, 0.10), (-0.04, 0.72, 0.12, 0.08, 0.15), (-0.20, 0.715, 0.13, 0.085, 0.20),
		(-0.32, 0.73, 0.12, 0.085, 0.18), (-0.40, 0.77, 0.09, 0.085, 0.11), (-0.46, 0.84, 0.075, 0.07, 0.08), (-0.50, 0.88, 0.065, 0.06, 0.065)],
	("Canid", "Muscular"): [(0.50, 0.70, 0.06, 0.05, 0.05), (0.45, 0.70, 0.115, 0.08, 0.12), (0.33, 0.705, 0.135, 0.09, 0.14),
		(0.16, 0.715, 0.12, 0.08, 0.11), (-0.04, 0.72, 0.135, 0.09, 0.16), (-0.20, 0.715, 0.15, 0.095, 0.21),
		(-0.32, 0.73, 0.14, 0.095, 0.19), (-0.40, 0.77, 0.11, 0.09, 0.12), (-0.46, 0.84, 0.09, 0.075, 0.085), (-0.50, 0.88, 0.075, 0.065, 0.07)],
	("Cervid", "Sleek"): [(0.52, 0.99, 0.05, 0.04, 0.05), (0.46, 0.99, 0.11, 0.07, 0.12), (0.34, 0.99, 0.13, 0.08, 0.15),
		(0.15, 0.995, 0.125, 0.08, 0.16), (-0.05, 1.0, 0.13, 0.085, 0.18), (-0.22, 0.995, 0.13, 0.09, 0.20),
		(-0.33, 1.0, 0.115, 0.09, 0.17), (-0.40, 1.07, 0.08, 0.08, 0.11), (-0.45, 1.17, 0.065, 0.065, 0.07),
		(-0.49, 1.27, 0.055, 0.055, 0.055), (-0.52, 1.33, 0.05, 0.05, 0.05)],
	("Cervid", "Wool"): [(0.56, 0.98, 0.06, 0.05, 0.06), (0.50, 0.98, 0.16, 0.12, 0.16), (0.36, 0.985, 0.19, 0.15, 0.20),
		(0.15, 0.99, 0.20, 0.16, 0.21), (-0.05, 1.0, 0.20, 0.16, 0.22), (-0.22, 1.0, 0.19, 0.16, 0.22),
		(-0.34, 1.02, 0.16, 0.14, 0.18), (-0.42, 1.09, 0.11, 0.10, 0.12), (-0.47, 1.18, 0.08, 0.075, 0.08),
		(-0.50, 1.26, 0.06, 0.06, 0.06), (-0.52, 1.32, 0.05, 0.05, 0.05)],
	("Heavy", "Boar"): [(0.70, 0.84, 0.06, 0.05, 0.06), (0.64, 0.85, 0.17, 0.11, 0.20), (0.48, 0.86, 0.21, 0.13, 0.26),
		(0.20, 0.88, 0.23, 0.15, 0.29), (-0.08, 0.91, 0.24, 0.18, 0.31), (-0.30, 0.93, 0.24, 0.20, 0.32),
		(-0.46, 0.91, 0.22, 0.19, 0.29), (-0.58, 0.84, 0.18, 0.15, 0.22), (-0.66, 0.78, 0.14, 0.12, 0.15)],
	("Heavy", "Bovine"): [(0.70, 0.86, 0.06, 0.05, 0.06), (0.64, 0.87, 0.18, 0.10, 0.20), (0.48, 0.88, 0.23, 0.11, 0.27),
		(0.20, 0.89, 0.25, 0.13, 0.31), (-0.08, 0.90, 0.26, 0.15, 0.33), (-0.30, 0.92, 0.25, 0.20, 0.33),
		(-0.46, 0.89, 0.21, 0.18, 0.32), (-0.58, 0.82, 0.16, 0.13, 0.27), (-0.66, 0.76, 0.13, 0.11, 0.20)],
}
# 늑대·사자는 조금 더 다부지게(가로 반지름 +)
for _k in (("Canid", "Lean"), ("Canid", "Muscular")):
	BODY_RINGS[_k] = [(y, zc, rx + 0.015 * (1 if rx > 0.07 else 0.4), rU, rD) for (y, zc, rx, rU, rD) in BODY_RINGS[_k]]


def PartBody(frame, kind):
	mb = MeshBuilder()
	rings = BODY_RINGS[(frame, kind)]
	desc = ""
	if kind == "Wool":
		_BodyLoft(mb, rings, "Fur", belly=0, back=0, n=12)
		k = 0
		for (y, zc, rx, rU, rD) in rings[1:7]:
			for a in range(0, 360, 40):
				r = math.radians(a + (k % 2) * 20)
				s = math.sin(r)
				p = V(rx * math.cos(r) * 0.95, y, zc + (rU if s > 0 else rD) * s * 0.95)
				if p.z < zc - rD * 0.75:
					continue
				mb.Ico(p, 0.06, "Fur", subdiv=1, jitter=0.2, seed=k)
				k += 1
		desc = "양털 몸통(덩어리 털)"
	elif kind == "Boar":
		_BodyLoft(mb, rings, "Fur", belly=0, back=0.75)
		ys = [r[0] for r in rings[1:-1]]
		for i, y in enumerate(ys):
			r = rings[1 + i]
			ridge = r[1] + r[3]
			h = 0.03 + 0.03 * (i / max(1, len(ys) - 1))
			mb.Prism([(-0.11, 0), (0.11, 0), (0.02, h)], 0.06, "FurDark", FrameMatrix(V(0, y, ridge - 0.02), V(0, 1, 0), V(0, 0, 1), V(1, 0, 0)))
		desc = "멧돼지 몸통(등의 뻣뻣한 갈기)"
	elif kind == "Bovine":
		_BodyLoft(mb, rings, "Fur", belly=0.0, back=0)
		mb.Ellipsoid((0, -0.32, 1.06), (0.17, 0.2, 0.1), 8, 4, "Fur")
		mb.Prism([(-0.62, 0.71), (-0.48, 0.56), (-0.38, 0.60), (-0.56, 0.76)], 0.07, "Fur", FrameMatrix(V(0, 0, 0), V(0, 1, 0), V(0, 0, 1), V(1, 0, 0)))
		desc = "소 몸통(어깨 혹과 목 밑 군살)"
	elif kind == "Muscular":
		_BodyLoft(mb, rings, "Fur", belly=0.55, back=0)
		desc = "굵은 포식자 몸통(사자)"
	else:
		_BodyLoft(mb, rings, "Fur", belly=0.55, back=0.7)
		desc = {"Lean": "날렵한 늑대 몸통(밝은 배·어두운 등)", "Sleek": "사슴 몸통(흰 배)"}[kind]
	return QPart("%s_Body_%s" % (frame, kind), "Body", frame, mb, BODY_BONES, power=4.0, desc=desc,
		factors={"thigh.L": 0.5, "thigh.R": 0.5, "scapula.L": 0.6, "scapula.R": 0.6, "head": 0.5})


# ───────────────────────── 다리 ─────────────────────────

LEG_STYLE = {
	"Canid": dict(front=[0.08, 0.066, 0.048, 0.036, 0.03, 0.03], hind=[0.105, 0.072, 0.04, 0.03, 0.03], foot="paw"),
	"Cervid": dict(front=[0.065, 0.055, 0.038, 0.025, 0.021, 0.024], hind=[0.09, 0.06, 0.03, 0.021, 0.024], foot="hoof"),
	"Heavy": dict(front=[0.13, 0.11, 0.075, 0.058, 0.05, 0.05], hind=[0.16, 0.12, 0.07, 0.055, 0.05], foot="hoofStout"),
}


def PartLegs(frame):
	F = FRAMES[frame]
	st = LEG_STYLE[frame]
	left = MeshBuilder()
	front = [F["scapH"] + V(0, 0, -0.04), F["shoulder"], F["elbow"], F["wrist"], F["fetF"], F["toeF"]]
	left.Tube(front[:5], st["front"][:5], 7, "Fur")
	hind = [F["hipJ"] + V(0, 0.02, 0.04), F["stifle"], F["hock"], F["fetH"], F["toeH"]]
	left.Tube(hind[:4], st["hind"][:4], 7, "Fur")
	left.Ellipsoid(F["hipJ"].lerp(F["stifle"], 0.4) + V(0.01, 0.03, 0), (st["hind"][0] * 0.9, st["hind"][0] * 1.3, st["hind"][0] * 1.6), 7, 4, "Fur")
	left.Ellipsoid(F["shoulder"].lerp(F["elbow"], 0.35) + V(0.005, 0, 0), (st["front"][0] * 0.8, st["front"][0] * 1.1, st["front"][0] * 1.6), 7, 4, "Fur")
	for (fet, toe, tb) in ((F["fetF"], F["toeF"], "fronttoe.L"), (F["fetH"], F["toeH"], "hindtoe.L")):
		with left.Rigid(tb):
			if st["foot"] == "paw":
				left.Frustum(V(fet.x, fet.y - 0.025, 0.0), (0.06, 0.09), V(fet.x, fet.y - 0.02, 0.05), (0.05, 0.06), "Fur")
				for k in (-1, 0, 1):
					left.Ellipsoid(V(fet.x + k * 0.017, toe.y - 0.005, 0.014), (0.01, 0.014, 0.012), 5, 3, "Nose")
			else:
				w = 0.035 if st["foot"] == "hoof" else 0.05
				for s in (1, -1):
					left.Frustum(V(fet.x + s * w * 0.55, fet.y - 0.03, 0.0), (w, w * 1.6), V(fet.x + s * w * 0.5, fet.y - 0.015, 0.06), (w * 0.85, w * 1.1), "Hoof")
	mb = MeshBuilder()
	mb.Merge(left)
	mb.Merge(left.Mirrored())
	desc = {"Canid": "발가락으로 디디는 네 다리 + 발바닥 패드", "Cervid": "가는 네 다리 + 갈라진 발굽", "Heavy": "굵은 네 다리 + 넓은 발굽"}[frame]
	return QPart("%s_Legs" % frame, "Legs", frame, mb, LEG_BONES, power=6.0, desc=desc,
		factors={"chest": 0.4, "pelvis": 0.4, "scapula.L": 0.7, "scapula.R": 0.7})


# ───────────────────────── 머리 ─────────────────────────

def PartHead(frame, kind):
	F = FRAMES[frame]
	M = _HeadFrame(F)
	L = (F["headT"] - F["neckT"]).length
	mb = MeshBuilder()
	with mb.Rigid("head"):
		if kind == "Wolf":
			start = _HeadLoft(mb, [(-0.03, 0.05, 0.05, 0.05, 0.0), (0.03, 0.08, 0.075, 0.06, 0.0), (0.10, 0.075, 0.065, 0.05, -0.005),
				(0.15, 0.045, 0.042, 0.035, -0.025), (0.22, 0.036, 0.032, 0.028, -0.032), (0.265, 0.03, 0.024, 0.022, -0.035)], "Fur", M)
			mb.Paint(start, lambda c: "FurLight" if (M.inverted() @ c).z < -0.03 else None)
			mb.Box((0, 0.27, -0.025), (0.034, 0.02, 0.024), "Nose", M)
			_Eyes(mb, M, 0.105, 0.05, 0.03, 0.02)
			desc = "늑대 머리(길쭉한 주둥이)"
		elif kind == "Lion":
			start = _HeadLoft(mb, [(-0.03, 0.06, 0.06, 0.06, 0.0), (0.03, 0.10, 0.09, 0.08, 0.0), (0.11, 0.095, 0.08, 0.07, -0.01),
				(0.17, 0.065, 0.055, 0.055, -0.025), (0.22, 0.055, 0.045, 0.045, -0.035), (0.245, 0.045, 0.035, 0.035, -0.038)], "Fur", M)
			mb.Paint(start, lambda c: "FurLight" if (M.inverted() @ c).z < -0.04 else None)
			mb.Box((0, 0.25, -0.02), (0.05, 0.02, 0.03), "Nose", M)
			_Eyes(mb, M, 0.12, 0.055, 0.035, 0.022)
			desc = "사자 머리(짧고 넓은 주둥이)"
		elif kind == "Deer":
			start = _HeadLoft(mb, [(-0.03, 0.045, 0.05, 0.045, 0.0), (0.04, 0.065, 0.06, 0.05, 0.0), (0.12, 0.05, 0.045, 0.04, -0.02),
				(0.20, 0.035, 0.03, 0.03, -0.035), (0.27, 0.03, 0.025, 0.025, -0.04), (0.29, 0.024, 0.02, 0.02, -0.042)], "Fur", M)
			mb.Paint(start, lambda c: "FurLight" if (M.inverted() @ c).z < -0.045 else None)
			mb.Box((0, 0.29, -0.035), (0.04, 0.02, 0.026), "Nose", M)
			_Eyes(mb, M, 0.07, 0.058, 0.025, 0.024)
			desc = "사슴 머리(큰 눈·가는 주둥이)"
		elif kind == "Sheep":
			start = _HeadLoft(mb, [(-0.03, 0.05, 0.05, 0.05, 0.0), (0.04, 0.065, 0.06, 0.055, 0.0), (0.12, 0.055, 0.055, 0.045, -0.015),
				(0.19, 0.042, 0.045, 0.038, -0.03), (0.22, 0.035, 0.035, 0.03, -0.035)], "FurDark", M)
			mb.Ico(V(0, 0.0, 0.06) + V(0, 0, 0), 0.07, "Fur", subdiv=1, jitter=0.15, seed=2, M=M)
			mb.Box((0, 0.225, -0.03), (0.04, 0.012, 0.02), "Nose", M)
			_Eyes(mb, M, 0.07, 0.058, 0.03, 0.02)
			desc = "양 머리(검은 얼굴·이마 털뭉치)"
		elif kind == "Boar":
			start = _HeadLoft(mb, [(-0.03, 0.12, 0.15, 0.12, 0.0), (0.06, 0.14, 0.14, 0.13, -0.01), (0.18, 0.11, 0.10, 0.10, -0.03),
				(0.30, 0.075, 0.07, 0.07, -0.05), (0.40, 0.06, 0.055, 0.055, -0.06)], "Fur", M)
			mb.Cyl((0, 0.40, -0.06), (0, 0.43, -0.06), 0.06, 0.06, 10, "FurLight", up=(0, 0, 1), M=M)
			for s in (1, -1):
				mb.Ellipsoid((s * 0.022, 0.432, -0.06), (0.01, 0.004, 0.014), 5, 3, "Nose", M)
				mb.Tube([V(s * 0.06, 0.30, -0.09), V(s * 0.085, 0.34, -0.04), V(s * 0.08, 0.32, 0.01)], [0.016, 0.012, 0.0], 5, "Teeth", M=M)
			_Eyes(mb, M, 0.14, 0.105, 0.06, 0.022)
			desc = "멧돼지 머리(원반 코·엄니)"
		elif kind == "Bull":
			start = _HeadLoft(mb, [(-0.03, 0.12, 0.13, 0.12, 0.0), (0.06, 0.14, 0.13, 0.12, 0.0), (0.18, 0.12, 0.10, 0.10, -0.02),
				(0.30, 0.10, 0.08, 0.09, -0.04), (0.40, 0.10, 0.07, 0.08, -0.05), (0.44, 0.085, 0.055, 0.065, -0.05)], "Fur", M)
			mb.Paint(start, lambda c: "FurLight" if (M.inverted() @ c).y > 0.34 else None)
			for s in (1, -1):
				mb.Ellipsoid((s * 0.035, 0.445, -0.04), (0.014, 0.006, 0.018), 5, 3, "Nose", M)
			_Eyes(mb, M, 0.11, 0.12, 0.055, 0.026)
			mb.Ico(V(0, 0.02, 0.12), 0.06, "FurDark", subdiv=1, jitter=0.2, seed=6, M=M)
			desc = "소 머리(넓은 얼굴·밝은 주둥이)"
	# 아래턱(jaw 뼈) — 포식자는 이빨과 혀까지
	if kind in ("Wolf", "Lion"):
		jw = MeshBuilder()
		ln = 0.25 if kind == "Wolf" else 0.235
		with jw.Rigid("jaw"):
			jw.Loft([[M @ p for p in ring] for ring in (
				[V(-0.045, 0.06, -0.055), V(0.045, 0.06, -0.055), V(0.04, 0.06, -0.085), V(-0.04, 0.06, -0.085)],
				[V(-0.03, ln - 0.03, -0.05), V(0.03, ln - 0.03, -0.05), V(0.025, ln - 0.03, -0.068), V(-0.025, ln - 0.03, -0.068)])], "FurLight")
			for s in (1, -1):
				jw.Cyl(M @ V(s * 0.022, ln - 0.06, -0.045), M @ V(s * 0.022, ln - 0.06, -0.075), 0.007, 0.0, 4, "Teeth")
				mb.Cyl(M @ V(s * 0.022, ln - 0.045, -0.04), M @ V(s * 0.022, ln - 0.045, -0.072), 0.007, 0.0, 4, "Teeth")
			jw.Box(M @ V(0, ln * 0.55, -0.052), (0.05, 0.12, 0.006), "Tongue")
		mb.Merge(jw)
	else:
		jw = MeshBuilder()
		with jw.Rigid("jaw"):
			s = 2.2 if frame == "Heavy" else 1.0
			jw.Loft([[M @ V(p.x * s, p.y, p.z * s) for p in ring] for ring in (
				[V(-0.03, L * 0.35, -0.04), V(0.03, L * 0.35, -0.04), V(0.025, L * 0.35, -0.065), V(-0.025, L * 0.35, -0.065)],
				[V(-0.022, L * 0.9, -0.045), V(0.022, L * 0.9, -0.045), V(0.018, L * 0.9, -0.06), V(-0.018, L * 0.9, -0.06)])], "Fur")
		mb.Merge(jw)
	return QPart("%s_Head_%s" % (frame, kind), "Head", frame, mb, desc=desc)


def PartEars(frame, kind):
	F = FRAMES[frame]
	left = MeshBuilder()
	base, tip = F["earH"], F["earT"]
	d = (tip - base)
	with left.Rigid("ear.L"):
		if kind == "Pointed":
			left.Tube([base, base + d * 0.5, tip + d * 0.05], [(0.03, 0.012), (0.022, 0.01), (0, 0)], 4, "Fur", up=(0, 1, 0), phase=math.pi / 4)
			desc = "쫑긋 선 세모 귀"
		elif kind == "Round":
			left.Ellipsoid(base + d * 0.4, (0.03, 0.012, 0.03), 7, 4, "Fur")
			desc = "둥근 귀(사자)"
		elif kind == "Long":
			left.Tube([base, base + d * 0.45, base + d * 0.95, base + d * 1.25], [(0.02, 0.02), (0.016, 0.038), (0.012, 0.03), (0, 0)], 6, "Fur", up=(0, 1, 0))
			desc = "옆으로 펼친 큰 귀(사슴)"
		elif kind == "Droopy":
			left.Ellipsoid(base + V(0.04, 0.0, -0.035), (0.045, 0.012, 0.02), 7, 4, "FurDark")
			desc = "축 처진 귀(양)"
		elif kind == "Small":
			left.Tube([base, base + d * 0.6 + V(0, 0, 0.04), tip + V(0, 0, 0.06)], [(0.03, 0.014), (0.025, 0.01), (0, 0)], 4, "FurDark", up=(0, 1, 0))
			desc = "뾰족한 작은 귀(멧돼지)"
		elif kind == "Side":
			left.Tube([base, base + d * 0.5, base + d * 1.0], [(0.02, 0.03), (0.016, 0.045), (0, 0)], 6, "Fur", up=(0, 1, 0))
			desc = "옆으로 난 귀(소)"
	mb = MeshBuilder()
	mb.Merge(left)
	mb.Merge(left.Mirrored())
	return QPart("%s_Ears_%s" % (frame, kind), "Ears", frame, mb, desc=desc)


def PartHorns(frame, kind):
	F = FRAMES[frame]
	M = _HeadFrame(F)
	left = MeshBuilder()
	with left.Rigid("head"):
		if kind == "Antlers":
			base = V(0.035, 0.03, 0.06)
			main = [base, base + V(0.04, -0.02, 0.10), base + V(0.09, 0.0, 0.20), base + V(0.12, 0.05, 0.30), base + V(0.13, 0.08, 0.38)]
			left.Tube([M @ p for p in main], [0.016, 0.014, 0.012, 0.009, 0.0], 5, "Horn")
			for (i, off) in ((1, V(0.02, 0.10, 0.06)), (2, V(0.0, 0.10, 0.08)), (3, V(0.05, 0.06, 0.08))):
				p = main[i]
				left.Tube([M @ p, M @ (p + off * 0.5), M @ (p + off)], [0.01, 0.008, 0.0], 4, "Horn")
			desc = "가지 친 수사슴 뿔"
		elif kind == "Ram":
			path = []
			for i in range(14):
				t = i / 13
				a = t * math.pi * 1.6
				r = 0.075 * (1 - 0.45 * t)
				path.append(V(0.055 + 0.03 * t + r * 0.3 * math.sin(a), 0.02 - r * math.sin(a), 0.06 + r * math.cos(a) - 0.04 * t))
			left.Tube([M @ p for p in path], [0.032 * (1 - 0.7 * i / 13) for i in range(14)], 6, "Horn")
			desc = "말린 숫양 뿔"
		elif kind == "Bull":
			path = [V(0.09, 0.03, 0.10), V(0.19, 0.0, 0.12), V(0.29, 0.03, 0.18), V(0.33, 0.10, 0.27), V(0.32, 0.15, 0.32)]
			left.Tube([M @ p for p in path], [0.045, 0.036, 0.026, 0.014, 0.0], 6, "Horn")
			desc = "앞으로 휜 황소 뿔"
		elif kind == "Short":
			path = [V(0.10, 0.03, 0.10), V(0.15, 0.03, 0.13), V(0.17, 0.06, 0.17)]
			left.Tube([M @ p for p in path], [0.025, 0.018, 0.0], 6, "Horn")
			desc = "짧은 암소 뿔"
	mb = MeshBuilder()
	mb.Merge(left)
	mb.Merge(left.Mirrored())
	return QPart("%s_Horns_%s" % (frame, kind), "Horns", frame, mb, desc=desc)


def PartMane(frame):
	F = FRAMES[frame]
	mb = MeshBuilder()
	c0 = F["neckT"] + V(0, 0.02, -0.01)
	k = 0
	for ring, (dy, rad, n) in enumerate(((0.0, 0.13, 12), (0.07, 0.15, 12), (0.15, 0.14, 11), (0.23, 0.11, 9))):
		for i in range(n):
			a = 2 * math.pi * i / n
			p = c0 + V(math.cos(a) * rad, dy, math.sin(a) * rad * 1.1)
			mb.Ico(p, 0.065 - ring * 0.006, "Mane", subdiv=1, jitter=0.22, seed=k)
			k += 1
	return QPart("%s_Mane" % frame, "Mane", frame, mb, ["head", "neck", "chest"], power=5.0, desc="사자 갈기")


def PartTail(frame, kind):
	F = FRAMES[frame]
	mb = MeshBuilder()
	pts = [F["tail0"] + V(0, -0.03, 0), F["tail1"], F["tail2"], F["tail3"]]
	if kind == "Bushy":
		mb.Tube(pts + [F["tail3"] + (F["tail3"] - F["tail2"]) * 0.3], [0.03, 0.05, 0.055, 0.045, 0.0], 7, "Fur")
		desc = "덥수룩한 꼬리"
	elif kind == "Tuft":
		mb.Tube(pts, [0.022, 0.018, 0.014, 0.012], 6, "Fur")
		with mb.Rigid("tail3"):
			mb.Ico(F["tail3"], 0.045, "Mane", subdiv=1, jitter=0.2, seed=1)
		desc = "끝에 털 술이 달린 가는 꼬리(사자·소)"
	elif kind == "Short":
		mb.Tube(pts[:3], [0.04, 0.045, 0.03], 6, "Fur")
		with mb.Rigid("tail2"):
			mb.Ellipsoid(F["tail2"] + V(0, 0.012, -0.02), (0.035, 0.012, 0.05), 6, 3, "FurLight")
		desc = "짧은 꼬리(밑이 흰)"
	elif kind == "Thin":
		mb.Tube(pts, [0.016, 0.012, 0.009, 0.004], 5, "Fur")
		with mb.Rigid("tail3"):
			mb.Ico(F["tail3"], 0.02, "FurDark", subdiv=1, seed=2)
		desc = "가는 꼬리(멧돼지)"
	return QPart("%s_Tail_%s" % (frame, kind), "Tail", frame, mb, TAIL_BONES, power=6.0, desc=desc)


PART_TABLE = {
	"Canid": dict(Body=["Lean", "Muscular"], Head=["Wolf", "Lion"], Ears=["Pointed", "Round"], Horns=[], Tail=["Bushy", "Tuft"], Mane=True),
	"Cervid": dict(Body=["Sleek", "Wool"], Head=["Deer", "Sheep"], Ears=["Long", "Droopy"], Horns=["Antlers", "Ram"], Tail=["Short"], Mane=False),
	"Heavy": dict(Body=["Boar", "Bovine"], Head=["Boar", "Bull"], Ears=["Small", "Side"], Horns=["Bull", "Short"], Tail=["Thin", "Tuft"], Mane=False),
}


def AllParts(frame):
	t = PART_TABLE[frame]
	parts = [PartLegs(frame)]
	parts += [PartBody(frame, k) for k in t["Body"]]
	parts += [PartHead(frame, k) for k in t["Head"]]
	parts += [PartEars(frame, k) for k in t["Ears"]]
	parts += [PartHorns(frame, k) for k in t["Horns"]]
	parts += [PartTail(frame, k) for k in t["Tail"]]
	if t["Mane"]:
		parts.append(PartMane(frame))
	return parts
