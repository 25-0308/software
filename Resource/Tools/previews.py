# -*- coding: utf-8 -*-
# 미리보기 이미지 묶음(Previews/*.png) — 리소스를 열어 보지 않고도 무엇이 있는지 훑어보는 용도.
import bpy
import math
import os
from rescommon import ResetScene, GetCollection, ResPath, Log
import preview as PV
import humanoid as H
import humanoid_presets as HPR
import quadruped as Q
import buildings as BL
import nature as N


def _Lineup(arm, objs, presets, coll, spacing, action="Pose_Base", frame=0):
	for i, pr in enumerate(presets):
		a2 = arm.copy()
		coll.objects.link(a2)
		a2.hide_render = False
		a2.location.x = (i - (len(presets) - 1) / 2) * spacing
		s = pr["scale"] if pr["scale"] < 1.5 else 1.0
		a2.scale = (s,) * 3
		for o in H.PresetObjects(pr, a2, objs, coll):
			o.hide_render = False
		PV.PlayAction(a2, action, frame)


def _Strip(arm, frames, out, width, height, target, ortho, yaw=90, pitch=10):
	paths = []
	for i, (action, f) in enumerate(frames):
		PV.PlayAction(arm, action, f)
		PV.Camera(target, 30, yaw, pitch, ortho)
		p = out + "_%d.png" % i
		PV.Render(p)
		paths.append(p)
	PV.Stitch(paths, out + ".jpg", min(6, len(paths)))


def Humanoids():
	arm, parts, objs = H.BuildRig()
	for o in objs.values():
		o.hide_render = True
	arm.hide_render = True
	coll = GetCollection("_Preview")
	PV.SetupRender(100, 100, samples=12)
	PV.Ground(400)
	chunks = [HPR.PRESETS[i:i + 14] for i in range(0, len(HPR.PRESETS), 14)]
	for k, chunk in enumerate(chunks):
		for o in list(coll.objects):
			bpy.data.objects.remove(o)
		_Lineup(arm, objs, chunk, coll, 1.0)
		PV.SetupRender(170 * len(chunk), 440, samples=12)
		PV.Camera((0, 0, 0.95), 30, 20, 12, 1.0 * len(chunk))
		PV.Render(ResPath("Previews", "Humanoid_Presets_%d.jpg" % (k + 1)))
	for o in list(coll.objects):
		bpy.data.objects.remove(o)
	pmap = {p["id"]: p for p in HPR.PRESETS}
	for o in H.PresetObjects(pmap["PLAYER"], arm, objs, coll):
		o.hide_render = False
	PV.SetupRender(300, 420, samples=8)
	_Strip(arm, [("Walk", f) for f in (0, 4, 9, 13, 18, 22)] + [("Run", f) for f in (0, 3, 6, 10, 13, 16)],
		ResPath("Previews", "Humanoid_Anim_WalkRun"), 300, 420, (0, 0, 0.9), 2.6)
	_Strip(arm, [("Attack", f) for f in (0, 6, 10, 13, 17, 23)] + [("Jump", f) for f in (0, 7, 12, 18, 27, 31)],
		ResPath("Previews", "Humanoid_Anim_AttackJump"), 300, 420, (0, 0, 1.1), 3.0, yaw=60)
	_Strip(arm, [("Turn_L90", f) for f in (0, 5, 10, 14, 19, 24)], ResPath("Previews", "Humanoid_Anim_Turn"), 300, 420, (0, 0, 0.9), 2.6, yaw=20, pitch=35)


def Quadrupeds():
	for frame in Q.QP.FRAMES:
		arm, parts, objs, clips = Q.BuildFrame(frame)
		for o in objs.values():
			o.hide_render = True
		arm.hide_render = True
		coll = GetCollection("_Preview")
		PV.Ground(400)
		sps = [s for s in Q.SPECIES if s["frame"] == frame]
		for i, sp in enumerate(sps):
			a2 = arm.copy()
			coll.objects.link(a2)
			a2.hide_render = False
			a2.location.x = (i - (len(sps) - 1) / 2) * 2.2
			a2.scale = (sp["scale"],) * 3
			for o in Q.SpeciesObjects(sp, a2, objs, coll):
				o.hide_render = False
			PV.PlayAction(a2, "Pose_Base", 0)
		PV.SetupRender(520 * len(sps), 420, samples=12)
		PV.Camera((0, 0, 0.6), 30, 45, 18, 2.4 * len(sps))
		PV.Render(ResPath("Previews", "Quadruped_%s_Species.jpg" % frame))
		for o in list(coll.objects):
			bpy.data.objects.remove(o)
		show = {"Canid": "WOLF", "Cervid": "DEER", "Heavy": "BULL"}[frame]
		sp = next(s for s in sps if s["id"] == show)
		arm.hide_render = False
		arm.scale = (sp["scale"],) * 3
		for o in Q.SpeciesObjects(sp, arm, objs, coll):
			o.hide_render = False
		PV.SetupRender(360, 300, samples=8)
		w = Q.QA.GAITS[frame]["walk"]["frames"]
		r = Q.QA.GAITS[frame]["run"]["frames"]
		seq = [("Walk", int(w * k / 4)) for k in range(4)] + [("Run", int(r * k / 4)) for k in range(4)] + [("Attack", f) for f in (0, 8, 12, 16)]
		h = 0.5 if frame == "Canid" else 0.7
		_Strip(arm, seq, ResPath("Previews", "Quadruped_%s_Anim" % frame), 360, 300, (0, 0, h * sp["scale"]), 2.6 * sp["scale"] * (1.2 if frame == "Heavy" else 1.0), pitch=15)


def Buildings():
	ResetScene()
	coll = GetCollection("_Preview")
	objs = {}
	for e in BL.KIT:
		o, mb = BL.BuildPiece(e, coll)
		o.hide_render = True
		objs[e["name"]] = o
	x = 0
	for name in BL.PREFABS:
		for (piece, loc, rot) in BL.PREFABS[name]["items"]:
			o = objs[piece].copy()
			coll.objects.link(o)
			o.hide_render = False
			o.location = (loc[0] + x, loc[1], loc[2])
			o.rotation_euler = (0, 0, math.radians(rot))
		x += 11
	PV.SetupRender(1800, 700, samples=12)
	PV.Ground(400)
	PV.Camera((22, 0, 1.5), 80, 35, 35, 52)
	PV.Render(ResPath("Previews", "Buildings_Prefabs.jpg"))
	for o in list(coll.objects):
		if o.hide_render is False:
			bpy.data.objects.remove(o)
	for cat, sp in (("House", 3.2), ("Temple", 3.2), ("Goods", 2.4)):
		entries = [e for e in BL.KIT if e["category"] == cat]
		cols = 7
		shown = []
		for i, e in enumerate(entries):
			o = objs[e["name"]].copy()
			coll.objects.link(o)
			o.hide_render = False
			o.location = ((i % cols) * sp, -(i // cols) * sp, 0)
			shown.append(o)
		rows = (len(entries) + cols - 1) // cols
		PV.SetupRender(1600, 300 * rows + 200, samples=12)
		PV.Camera(((cols - 1) * sp / 2, -(rows - 1) * sp / 2, 0.8), 60, 25, 35, cols * sp * 1.05)
		PV.Render(ResPath("Previews", "Buildings_Kit_%s.jpg" % cat))
		for o in shown:
			bpy.data.objects.remove(o)


def Nature():
	tiles, objs = N.Build()
	for o in tiles:
		o.hide_render = True
	cols = 9
	for i, o in enumerate(objs):
		o.location = ((i % cols) * 3.0 - 12, (i // cols) * 3.5, 0)
	PV.SetupRender(1800, 1100, samples=12)
	PV.Ground(400, (0.5, 0.5, 0.45))
	PV.Camera((0, 2, 0), 80, 20, 38, 34)
	PV.Render(ResPath("Previews", "Nature_Kit.jpg"))
	ResetScene()
	c = GetCollection("T")
	for i, (name, key, size, rep, z, rough, alpha, desc) in enumerate(N.TILES[:9]):
		o = N._Tile(name, key, c, 2.0, 2.0, 0.0, rough, alpha)
		o.location = ((i % 5) * 2.1 - 4.2, -(i // 5) * 2.1, 0.02)
	PV.SetupRender(1500, 640, samples=8)
	PV.Camera((0, -1.05, 0), 40, 0, 70, 11)
	PV.Render(ResPath("Previews", "Nature_Textures.jpg"))


def Build():
	Humanoids()
	Quadrupeds()
	Buildings()
	Nature()
	Log("미리보기 완료")
