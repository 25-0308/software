# -*- coding: utf-8 -*-
# 인간형 리소스 빌드: 뼈대 → 부위 메시 → 애니메이션 → 부위 라이브러리(.blend/.glb) → 프리셋별 .glb → 카탈로그(.json)
import bpy
from rescommon import (ResetScene, GetCollection, BuildArmature, SkinObject, MakeMaterial, PaletteMaterial, ExportGlb,
	SaveBlend, WriteJson, ResPath, Log, FPS)
import humanoid_parts as HP
import humanoid_anims as HA
import humanoid_presets as PR

OUT = "Characters/Humanoid"


def RoleMaterial(role):
	if role in HP.ROLE_DEFAULTS:
		return MakeMaterial(role, HP.ROLE_DEFAULTS[role])
	return PaletteMaterial(role)


def BuildRig():
	"""뼈대 + 부위 + 애니메이션까지 만든 씬을 돌려줌(프리뷰도 이것을 씀)."""
	ResetScene()
	root = GetCollection("Humanoid")
	arm = BuildArmature("Humanoid_Rig", HP.SkeletonSpecs(), root)
	partsColl = GetCollection("Humanoid_Parts", root)
	objs = {}
	parts = HP.AllParts(arm)
	for part in parts:
		coll = GetCollection("Slot_" + part.slot, partsColl)
		obj = part.mb.Build(part.name, coll, RoleMaterial)
		SkinObject(obj, part.mb, arm, part.bones or ["head"], power=part.power, weightFn=part.weightFn, factors=part.factors)
		obj["slot"] = part.slot
		obj["desc"] = part.desc
		objs[part.name] = obj
	Log("인간형 부위 %d개" % len(objs))
	HA.BakeAll(arm)
	Log("인간형 애니메이션 %d개" % len(HA.CLIPS))
	return arm, parts, objs


def PresetObjects(preset, arm, objs, coll):
	"""프리셋에 쓰는 부위를 복사해 프리셋 색 재질을 입힘."""
	pid = preset["id"]
	names = ["Head_" + preset["body"], "Hands_Bare"] + list(preset["parts"])
	if preset.get("R"):
		names.append("Prop_%s.R" % preset["R"])
	if preset.get("L"):
		names.append("Prop_%s.L" % preset["L"])
	colors = dict(HP.ROLE_DEFAULTS)
	colors.update(preset["colors"])
	out = []
	for n in names:
		src = objs.get(n)
		if src is None:
			raise KeyError("프리셋 %s: 없는 부위 %s" % (pid, n))
		o = src.copy()
		o.data = src.data.copy()
		o.name = "%s.%s" % (pid, n)
		coll.objects.link(o)
		for i, m in enumerate(o.data.materials):
			role = m.name.split(".")[0]
			if role in HP.ROLE_DEFAULTS:
				o.data.materials[i] = MakeMaterial("%s.%s" % (pid, role), colors[role])
		o.parent = arm
		o.modifiers["Armature"].object = arm
		out.append(o)
	return out


def ClipInfo():
	clips = []
	for (name, frames, fn, loop, markers) in HA.CLIPS:
		c = dict(name=name, frames=frames, fps=FPS, duration=round(frames / FPS, 4), loop=loop,
			markers={k: round(v * frames / FPS, 4) for k, v in markers.items()})
		if name == "Walk":
			c["speed"] = round(HA.StanceSpeed(HA.WALK), 3)
		if name == "Run":
			c["speed"] = round(HA.StanceSpeed(HA.RUN), 3)
		if name.startswith("Turn_"):
			c["rootMotion"] = {"yawDegrees": 90 if name.endswith("L90") else -90}
		if name == "Jump":
			c["rootMotion"] = {"heightPeak": HA.JUMP_HEIGHT}
		clips.append(c)
	return clips


def Build():
	arm, parts, objs = BuildRig()
	lib = [arm] + list(objs.values())
	ExportGlb(ResPath(OUT, "Humanoid_Parts.glb"), lib)

	catalog = dict(rig="Humanoid_Rig", height=1.76, forward="-Y(블렌더) / +Z(glTF)", up="+Z(블렌더) / +Y(glTF)",
		bones=[b.name for b in arm.data.bones], roles=HP.ROLE_DEFAULTS, clips=ClipInfo(), parts=[])
	for part in parts:
		catalog["parts"].append(dict(name=part.name, slot=part.slot, body=part.body, desc=part.desc,
			roles=list(part.mb.roles), verts=len(part.mb.verts), faces=len(part.mb.faces)))
	WriteJson(ResPath(OUT, "Humanoid_Parts.json"), catalog)

	presetColl = GetCollection("_PresetExport")
	presetsOut = []
	for preset in PR.PRESETS:
		pobjs = PresetObjects(preset, arm, objs, presetColl)
		arm.scale = (preset["scale"],) * 3
		ExportGlb(ResPath(OUT, "Presets", preset["id"] + ".glb"), [arm] + pobjs)
		arm.scale = (1, 1, 1)
		for o in pobjs:
			me = o.data
			bpy.data.objects.remove(o, do_unlink=True)
			bpy.data.meshes.remove(me)
		colors = dict(HP.ROLE_DEFAULTS)
		colors.update(preset["colors"])
		presetsOut.append(dict(preset, colors=colors, file="Presets/%s.glb" % preset["id"]))
	bpy.data.collections.remove(presetColl)
	WriteJson(ResPath(OUT, "Humanoid_Presets.json"), dict(presets=presetsOut))
	SaveBlend(ResPath(OUT, "Humanoid.blend"))
