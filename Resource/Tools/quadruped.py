# -*- coding: utf-8 -*-
# 네 발 짐승 빌드: 몸틀마다 뼈대 → 부위 → 애니메이션 → 부위 라이브러리 → 종(프리셋)별 .glb → 카탈로그
import bpy
from rescommon import (ResetScene, GetCollection, BuildArmature, SkinObject, MakeMaterial, PaletteMaterial, ExportGlb,
	SaveBlend, WriteJson, ResPath, Log, FPS)
import quadruped_parts as QP
import quadruped_anims as QA

OUT = "Creatures/Quadruped"

# disposition: hostile(위험 — 먼저 덤빔) / neutral(중립 — 건드리면 반격) / passive(순함 — 달아남)
SPECIES = [
	dict(id="WOLF", name="늑대", frame="Canid", scale=1.0, disposition="hostile",
		parts=["Body_Lean", "Legs", "Head_Wolf", "Ears_Pointed", "Tail_Bushy"],
		colors=dict(Fur="WolfGray", FurDark="WolfDark", FurLight="WolfLight", Eye="IrisGold", Nose="Nose"),
		gameKind="Wolf", note="회색 늑대 — 무리 지어 덤비는 포식자"),
	dict(id="SHADOW_WOLF", name="그림자 늑대", frame="Canid", scale=0.95, disposition="hostile",
		parts=["Body_Lean", "Legs", "Head_Wolf", "Ears_Pointed", "Tail_Bushy"],
		colors=dict(Fur="ShadeBody", FurDark="ShadeDark", FurLight="ShadeLight", Eye="GlowPurple", Nose="ShadeDark"),
		gameKind="ShadowWolf", note="신탁의 균열에서 나온 그림자(Shade) — 보랏빛 눈"),
	dict(id="LION", name="사자", frame="Canid", scale=1.25, disposition="hostile",
		parts=["Body_Muscular", "Legs", "Head_Lion", "Mane", "Ears_Round", "Tail_Tuft"],
		colors=dict(Fur="LionTawny", FurDark="LionMane", FurLight="LionLight", Mane="LionMane", Eye="IrisGold", Nose="Nose"),
		gameKind=None, note="갈기 사자(네메아의 사자 같은 위험한 짐승)"),
	dict(id="BOAR", name="멧돼지", frame="Heavy", scale=0.62, disposition="neutral",
		parts=["Body_Boar", "Legs", "Head_Boar", "Ears_Small", "Tail_Thin"],
		colors=dict(Fur="BoarBrown", FurDark="BoarBristle", FurLight="BoarSnout", Nose="Nose", Hoof="Hoof"),
		gameKind=None, note="건드리면 엄니로 들이받는 멧돼지"),
	dict(id="BULL", name="황소", frame="Heavy", scale=1.12, disposition="neutral",
		parts=["Body_Bovine", "Legs", "Head_Bull", "Horns_Bull", "Ears_Side", "Tail_Tuft"],
		colors=dict(Fur="BullBlack", FurDark="BullBlack", FurLight="BullMuzzle", Mane="BullBlack", Horn="Horn", Nose="Nose"),
		gameKind=None, note="검은 황소 — 화나면 머리를 낮춰 돌진"),
	dict(id="RAM", name="숫양", frame="Cervid", scale=0.8, disposition="neutral",
		parts=["Body_Wool", "Legs", "Head_Sheep", "Horns_Ram", "Ears_Droopy", "Tail_Short"],
		colors=dict(Fur="Wool", FurDark="SheepFace", FurLight="WoolDark", Horn="HornDark", Nose="Nose", Hoof="Hoof"),
		gameKind=None, note="말린 뿔의 숫양 — 다가오면 들이받음"),
	dict(id="DEER", name="사슴", frame="Cervid", scale=1.0, disposition="passive",
		parts=["Body_Sleek", "Legs", "Head_Deer", "Ears_Long", "Horns_Antlers", "Tail_Short"],
		colors=dict(Fur="DeerTan", FurDark="DeerDark", FurLight="DeerBelly", Horn="Antler", Nose="Nose", Hoof="Hoof"),
		gameKind="Deer", note="수사슴 — 뿔(Horns_Antlers)을 빼면 암사슴"),
	dict(id="SHEEP", name="양", frame="Cervid", scale=0.75, disposition="passive",
		parts=["Body_Wool", "Legs", "Head_Sheep", "Ears_Droopy", "Tail_Short"],
		colors=dict(Fur="Wool", FurDark="SheepFace", FurLight="WoolDark", Nose="Nose", Hoof="Hoof"),
		gameKind=None, note="양치기의 양"),
	dict(id="CHR_IO", name="하얀 암소(이오)", frame="Heavy", scale=0.95, disposition="passive",
		parts=["Body_Bovine", "Legs", "Head_Bull", "Horns_Short", "Ears_Side", "Tail_Tuft"],
		colors=dict(Fur="CowWhite", FurDark="CowWhite", FurLight="CowPink", Mane="WoolDark", Horn="Horn", Nose="CowPink", Eye="AnimalEye"),
		gameKind="WhiteCow", note="말을 걸 수 있는 하얀 암소(SQ_AEGEAN_02)"),
]


def RoleMaterial(role):
	if role in QP.ROLE_DEFAULTS:
		return MakeMaterial(role, QP.ROLE_DEFAULTS[role])
	return PaletteMaterial(role)


def BuildFrame(frame):
	ResetScene()
	root = GetCollection("Quadruped_" + frame)
	arm = BuildArmature(frame + "_Rig", QP.SkeletonSpecs(QP.FRAMES[frame]), root)
	partsColl = GetCollection(frame + "_Parts", root)
	objs = {}
	parts = QP.AllParts(frame)
	for part in parts:
		coll = GetCollection("%s_Slot_%s" % (frame, part.slot), partsColl)
		obj = part.mb.Build(part.name, coll, RoleMaterial)
		SkinObject(obj, part.mb, arm, part.bones or ["head"], power=part.power, factors=part.factors)
		obj["slot"] = part.slot
		obj["desc"] = part.desc
		objs[part.name] = obj
	clips = QA.BakeAll(arm, frame)
	Log("%s: 부위 %d개, 애니메이션 %d개" % (frame, len(objs), len(clips)))
	return arm, parts, objs, clips


def SpeciesObjects(sp, arm, objs, coll):
	colors = dict(QP.ROLE_DEFAULTS)
	colors.update(sp["colors"])
	out = []
	for short in sp["parts"]:
		src = objs["%s_%s" % (sp["frame"], short)]
		o = src.copy()
		o.data = src.data.copy()
		o.name = "%s.%s" % (sp["id"], short)
		coll.objects.link(o)
		for i, m in enumerate(o.data.materials):
			role = m.name.split(".")[0]
			if role in QP.ROLE_DEFAULTS:
				o.data.materials[i] = MakeMaterial("%s.%s" % (sp["id"], role), colors[role])
		o.parent = arm
		o.modifiers["Armature"].object = arm
		out.append(o)
	return out


def ClipInfo(frame, clips):
	g = QA.GAITS[frame]
	out = []
	for (name, frames, fn, loop, markers) in clips:
		c = dict(name=name, frames=frames, fps=FPS, duration=round(frames / FPS, 4), loop=loop,
			markers={k: round(v * frames / FPS, 4) for k, v in markers.items()})
		if name == "Walk":
			c["speed"] = round(QA.GaitSpeed(g["walk"]), 3)
		if name == "Run":
			c["speed"] = round(QA.GaitSpeed(g["run"]), 3)
		if name.startswith("Turn_"):
			c["rootMotion"] = {"yawDegrees": 90 if name.endswith("L90") else -90}
		if name == "Jump":
			c["rootMotion"] = {"heightPeak": QA.JUMP_HEIGHT}
		out.append(c)
	return out


def Build():
	catalog = dict(frames={}, roles=QP.ROLE_DEFAULTS)
	speciesOut = []
	for frame in QP.FRAMES:
		arm, parts, objs, clips = BuildFrame(frame)
		ExportGlb(ResPath(OUT, "%s_Parts.glb" % frame), [arm] + list(objs.values()))
		catalog["frames"][frame] = dict(rig=arm.name, bones=[b.name for b in arm.data.bones], clips=ClipInfo(frame, clips),
			attackStyle=QA.GAITS[frame]["attack"],
			parts=[dict(name=p.name, slot=p.slot, desc=p.desc, roles=list(p.mb.roles), verts=len(p.mb.verts), faces=len(p.mb.faces)) for p in parts])
		coll = GetCollection("_SpeciesExport")
		for sp in SPECIES:
			if sp["frame"] != frame:
				continue
			sobjs = SpeciesObjects(sp, arm, objs, coll)
			arm.scale = (sp["scale"],) * 3
			ExportGlb(ResPath(OUT, "Species", sp["id"] + ".glb"), [arm] + sobjs)
			arm.scale = (1, 1, 1)
			for o in sobjs:
				me = o.data
				bpy.data.objects.remove(o, do_unlink=True)
				bpy.data.meshes.remove(me)
			colors = dict(QP.ROLE_DEFAULTS)
			colors.update(sp["colors"])
			speciesOut.append(dict(sp, colors=colors, file="Species/%s.glb" % sp["id"]))
		bpy.data.collections.remove(coll)
		SaveBlend(ResPath(OUT, "Quadruped_%s.blend" % frame))
	WriteJson(ResPath(OUT, "Quadruped_Parts.json"), catalog)
	WriteJson(ResPath(OUT, "Quadruped_Species.json"), dict(species=speciesOut))
