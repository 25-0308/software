# -*- coding: utf-8 -*-
# 대화창 초상화 — 3D 캐릭터 모델(프리셋)을 흉상으로 렌더한 그림(Portraits/<화자ID>.png).
# 게임(Portrait.cpp)은 작업 폴더의 ./Portraits/<화자ID>.png가 있으면 그린 2D 초상화 대신 그 그림을 씀
# (비율을 지켜 꽉 차게 384×476 텍스처로). 그래서 크기·비율을 그대로 맞춰 렌더함.
import bpy
import math
import os
import numpy as np
from mathutils import Vector
from rescommon import ResPath, Log, GetCollection
import palette
import preview as PV
import humanoid as H
import humanoid_presets as HPR
import quadruped as Q

WIDTH, HEIGHT = 384, 476     # 게임 초상화 텍스처 크기(화면 192×238의 두 배)
VIEW_HEIGHT = 0.74           # 화면 세로가 담는 키(m, 배율 1 기준) — 가슴 위부터 머리 장식까지
CAMERA_YAW = 22.0            # 정면에서 캐릭터 왼쪽으로 살짝 돈 3/4 구도
CAMERA_PITCH = -3.0

# 화자 ID → 배경(위 색, 아래 색)과 테두리빛(림 라이트) 색. 없으면 옷 색에서 만듦.
THEMES = {
	"PLAYER": ("#6E8FB0", "#2B3A4E", "#FFE2B0"),
	"CHR_ZEUS": ("#4A5A8A", "#1A1E36", "#CFE6FF"),
	"CHR_POSEIDON": ("#2F7F8A", "#0E2C3A", "#7FD8D0"),
	"CHR_HADES": ("#4A1E24", "#0E080C", "#C83A3A"),
	"CHR_APHRODITE": ("#D88AA0", "#5A2440", "#FFD0D8"),
	"CHR_HERMES": ("#E0B060", "#6A4420", "#FFF0C0"),
	"CHR_HERA": ("#7A5A9A", "#2A1A3E", "#5FB0C0"),
	"CHR_ATHENA": ("#7A8A5A", "#26301E", "#D8E0B0"),
	"CHR_ARES": ("#8A2A1E", "#200808", "#FF8040"),
	"CHR_DIONYSUS": ("#7A2A4A", "#2A0C1A", "#C8A0F0"),
	"CHR_HEPHAESTUS": ("#B0602A", "#2A140A", "#FFA040"),
	"CHR_PAN": ("#5A7A3A", "#1E2A12", "#C8E090"),
	"CHR_APOLLO": ("#E8B060", "#7A4A2A", "#FFE0A0"),
	"CHR_PERSEPHONE": ("#5A2A3A", "#1A1418", "#E04050"),
	"CHR_ERIS": ("#6A4A2E", "#1E1410", "#FFB060"),
	"CHR_ERIS_TRUE": ("#5A1418", "#120406", "#E8C04A"),
	"CHR_HEARTH_GOD": ("#6E6A66", "#2A2826", "#B0A8A0"),
	"CHR_HEARTH_GOD_NAMED": ("#A85A2A", "#2A1408", "#FF9040"),
	"CHR_PROMETHEUS": ("#6A5A4A", "#1E1814", "#FF8A2A"),
	"CHR_MOIRAI": ("#5A5A6E", "#16161E", "#E8D080"),
	"CHR_CLOTHO": ("#6A6A80", "#1A1A26", "#E8D080"),
	"CHR_LACHESIS": ("#5A5A6E", "#16161E", "#E8D080"),
	"CHR_ATROPOS": ("#3A3A4A", "#0C0C12", "#E8D080"),
	"CHR_TERMINUS": ("#8A8A6A", "#2E2E22", "#F0E8C0"),
	"CHR_KRONOS_ECHO": ("#3A1E5A", "#08040E", "#A060E0"),
	"CHR_PYTHIA": ("#8A7A5A", "#2A2418", "#D8E0A0"),
	"CHR_PYTHIA_VOICES": ("#3A2A5A", "#0E0818", "#A060E0"),
	"CHR_IO": ("#5A8AA0", "#1E3440", "#E0F0FF"),
	"CHR_IO_NAMED": ("#5A8AA0", "#1E3440", "#E0F0FF"),
	"CHR_TANTALUS": ("#4A4A40", "#141410", "#9AB0A0"),
	"NPC_SOUL": ("#4A5260", "#14181E", "#A8B8D0"),
	"NPC_SOUL_GRANNY": ("#4A5260", "#14181E", "#A8B8D0"),
	"NPC_SOUL_FISHER": ("#4A5260", "#14181E", "#A8B8D0"),
	"NPC_SOUL_GENERAL": ("#4A5260", "#14181E", "#A8B8D0"),
	"NPC_UNDYING": ("#5A5448", "#1A1814", "#C8C0A8"),
	"NPC_UNDYING_ELDER": ("#5A5448", "#1A1814", "#C8C0A8"),
	"MON_BRONZE_WARRIOR": ("#6A4A2A", "#1A1008", "#FFD27A"),
}

# 프리셋이 없는 화자 — 비슷한 프리셋을 빌리고 색만 바꿈
ALIASES = {
	"NPC_THEBES_CITIZEN": ("NPC_ATHENS_CITIZEN", dict(ClothB="Wine", Trim="Gold"), ("#7A2A4A", "#2A0C1A", "#C8A0F0")),
}


def _Hex(h):
	return np.array(palette.Hex2Rgb(h), dtype=np.float32)


def _Theme(pid, colors):
	if pid in THEMES:
		top, bottom, rim = THEMES[pid]
		return _Hex(top), _Hex(bottom), _Hex(rim)
	cloth = np.array(palette.Hex2Rgb(palette.PALETTE[colors.get("ClothA", "Linen")][0]), dtype=np.float32)
	gray = np.array([0.42, 0.44, 0.48], dtype=np.float32)
	top = cloth * 0.45 + gray * 0.55
	return top, top * 0.28, np.array([1.0, 0.9, 0.75], dtype=np.float32)


def _Background(top, bottom, rim):
	"""위에서 아래로 짙어지는 배경 + 왼쪽 위에서 드는 빛 + 반대쪽 테두리빛 번짐 + 비네트."""
	ys, xs = np.mgrid[0:HEIGHT, 0:WIDTH].astype(np.float32)
	u = xs / (WIDTH - 1)
	v = 1.0 - ys / (HEIGHT - 1)          # 0 = 아래, 1 = 위(블렌더 이미지는 아래부터)
	t = np.clip(v * 0.9 + 0.05, 0, 1)[..., None]
	bg = bottom[None, None, :] * (1 - t) + top[None, None, :] * t
	light = np.exp(-(((u - 0.22) / 0.5) ** 2 + ((v - 0.85) / 0.45) ** 2))[..., None]
	bg = bg + light * 0.18
	glow = np.exp(-(((u - 0.82) / 0.3) ** 2 + ((v - 0.55) / 0.35) ** 2))[..., None]
	bg = bg * (1 - glow * 0.25) + rim[None, None, :] * glow * 0.12
	d = np.sqrt(((u - 0.5) / 0.62) ** 2 + ((v - 0.52) / 0.62) ** 2)
	bg = bg * np.clip(1.15 - d ** 2 * 0.55, 0.45, 1.0)[..., None]
	return np.clip(bg, 0, 1)


def _Frame(img):
	"""2D 초상화와 같은 안쪽 금선 액자(가장자리에서 6px 안, 두께 2px)."""
	gold = _Hex("#C9A24A")
	m, w = 6, 2
	img[m:m + w, m:WIDTH - m, :3] = gold
	img[HEIGHT - m - w:HEIGHT - m, m:WIDTH - m, :3] = gold
	img[m:HEIGHT - m, m:m + w, :3] = gold
	img[m:HEIGHT - m, WIDTH - m - w:WIDTH - m, :3] = gold
	return img


def _Lights(rimColor):
	scene = bpy.context.scene
	for name in ("PortraitKey", "PortraitFill", "PortraitRim"):
		o = bpy.data.objects.get(name)
		if o:
			bpy.data.objects.remove(o)
	sun = bpy.data.objects.get("PreviewSun")
	if sun:
		bpy.data.objects.remove(sun)

	def Area(name, energy, size, color, loc, target):
		data = bpy.data.lights.new(name, 'AREA')
		data.energy = energy
		data.size = size
		data.color = color
		o = bpy.data.objects.new(name, data)
		scene.collection.objects.link(o)
		o.location = loc
		o.rotation_euler = (Vector(target) - Vector(loc)).to_track_quat('-Z', 'Y').to_euler()
		return o
	return Area, rimColor


def _Setup():
	PV.SetupRender(WIDTH, HEIGHT, samples=48, transparent=True)
	world = bpy.context.scene.world
	world.node_tree.nodes["Background"].inputs[1].default_value = 0.35


def _Shoot(target, scale, rim, path, yaw=CAMERA_YAW, viewHeight=VIEW_HEIGHT):
	"""target(월드)을 가운데에 두고 흉상을 찍어 배경과 합쳐 path에 저장."""
	scene = bpy.context.scene
	cam = bpy.data.objects.get("PortraitCam")
	if cam is None:
		cam = bpy.data.objects.new("PortraitCam", bpy.data.cameras.new("PortraitCam"))
		scene.collection.objects.link(cam)
	cam.data.lens = 85
	cam.data.sensor_fit = 'VERTICAL'
	cam.data.sensor_height = 24
	vfov = 2 * math.atan(12 / 85)
	dist = (viewHeight * scale / 2) / math.tan(vfov / 2)
	y, p = math.radians(yaw), math.radians(CAMERA_PITCH)
	d = Vector((math.sin(y) * math.cos(p), -math.cos(y) * math.cos(p), -math.sin(p)))
	cam.location = Vector(target) + d * dist
	cam.rotation_euler = (-d).to_track_quat('-Z', 'Y').to_euler()
	cam.data.clip_end = dist * 4
	scene.camera = cam

	Area, _ = _Lights(rim)
	t = Vector(target)
	s = scale
	Area("PortraitKey", 160 * s * s, 1.6 * s, (1.0, 0.94, 0.86), t + Vector((1.3, -1.6, 1.2)) * s, t)
	Area("PortraitFill", 40 * s * s, 2.0 * s, (0.75, 0.82, 1.0), t + Vector((-1.6, -1.4, 0.2)) * s, t)
	Area("PortraitRim", 220 * s * s, 1.0 * s, tuple(float(c) for c in rim), t + Vector((-0.9, 1.4, 0.9)) * s, t)

	tmp = path + ".tmp.png"
	PV.Render(tmp)
	return tmp


def _Compose(tmp, path, theme):
	top, bottom, rim = theme
	fg = PV.LoadPixels(tmp)
	os.remove(tmp)
	bg = _Background(top, bottom, rim)
	a = fg[..., 3:4]
	out = np.ones((HEIGHT, WIDTH, 4), dtype=np.float32)
	out[..., :3] = fg[..., :3] * a + bg * (1 - a)
	out = _Frame(out)
	PV.SavePixels(out, path)


def Humanoids(targets):
	arm, parts, objs = H.BuildRig()
	for o in objs.values():
		o.hide_render = True
	coll = GetCollection("_Portrait")
	_Setup()
	presets = {p["id"]: p for p in HPR.PRESETS}
	done = []
	for pid in targets:
		if pid in ALIASES:
			base, recolor, theme = ALIASES[pid]
			pr = dict(presets[base])
			pr["colors"] = dict(pr["colors"], **recolor)
			pr["id"] = pid
			themeArr = tuple(_Hex(c) for c in theme)
		elif pid in presets:
			pr = presets[pid]
			themeArr = None
		else:
			continue
		# 왼손 소품(홀·지팡이 등)은 든 자세에서 얼굴 앞을 가로지르므로 초상화에서는 뺌
		pr = dict(pr, L=None)
		pobjs = H.PresetObjects(pr, arm, objs, coll)
		for o in pobjs:
			o.hide_render = False
		s = pr["scale"]
		arm.scale = (s,) * 3
		PV.PlayAction(arm, "Pose_Base", 0)
		bpy.context.view_layer.update()
		ev = arm.evaluated_get(bpy.context.evaluated_depsgraph_get())
		head = arm.matrix_world @ ev.pose.bones["head"].head
		target = head + Vector((0, 0, 0.05 * s))
		colors = dict(H.HP.ROLE_DEFAULTS)
		colors.update(pr["colors"])
		theme = themeArr or _Theme(pid, colors)
		path = ResPath("Portraits", pid + ".png")
		tmp = _Shoot(target, s, theme[2], path)
		_Compose(tmp, path, theme)
		done.append(pid)
		for o in pobjs:
			me = o.data
			bpy.data.objects.remove(o, do_unlink=True)
			bpy.data.meshes.remove(me)
		arm.scale = (1, 1, 1)
	return done


def Animals(targets):
	done = []
	for sp in Q.SPECIES:
		if sp["id"] not in targets:
			continue
		arm, parts, objs, clips = Q.BuildFrame(sp["frame"])
		for o in objs.values():
			o.hide_render = True
		coll = GetCollection("_Portrait")
		_Setup()
		sobjs = Q.SpeciesObjects(sp, arm, objs, coll)
		for o in sobjs:
			o.hide_render = False
		s = sp["scale"]
		arm.scale = (s,) * 3
		PV.PlayAction(arm, "Pose_Base", 0)
		bpy.context.view_layer.update()
		ev = arm.evaluated_get(bpy.context.evaluated_depsgraph_get())
		hb = ev.pose.bones["head"]
		target = arm.matrix_world @ hb.head.lerp(hb.tail, 0.45) + Vector((0, 0, 0.04 * s))
		colors = dict(Q.QP.ROLE_DEFAULTS)
		colors.update(sp["colors"])
		theme = _Theme(sp["id"], colors)
		path = ResPath("Portraits", sp["id"] + ".png")
		tmp = _Shoot(target, s, theme[2], path, yaw=38.0, viewHeight=0.95)
		_Compose(tmp, path, theme)
		done.append(sp["id"])
	return done


def Speakers():
	"""Data/Story.txt에서 대사를 하는 화자 ID 목록(파일이 없으면 프리셋 전부)."""
	story = os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))), "SimpleGame", "Data", "Story.txt")
	ids = set(p["id"] for p in HPR.PRESETS) | set(ALIASES) | {"CHR_IO"}
	if os.path.exists(story):
		import re
		with open(story, encoding="utf-8") as f:
			for line in f:
				m = re.match(r"\s*((?:CHR|NPC|MON)_[A-Z0-9_]+)\s*:", line)
				if m:
					ids.add(m.group(1))
	return sorted(ids)


def Build():
	ids = Speakers()
	done = Humanoids(ids)
	done += Animals(ids)
	missing = [i for i in ids if i not in done]
	Log("초상화 %d장%s" % (len(done), (" — 모델이 없어 건너뜀: " + ", ".join(missing)) if missing else ""))
