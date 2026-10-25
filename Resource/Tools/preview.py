# -*- coding: utf-8 -*-
# 미리보기 렌더 — 결과물 검수용(Previews/*.png).
import bpy
import math
import os
import numpy as np
from mathutils import Vector
from rescommon import Log, ResPath, RES_DIR


def SetupRender(width, height, engine='BLENDER_EEVEE', samples=16, transparent=False):
	scene = bpy.context.scene
	scene.render.engine = engine
	scene.render.resolution_x = width
	scene.render.resolution_y = height
	scene.render.resolution_percentage = 100
	scene.render.film_transparent = transparent
	scene.render.image_settings.file_format = 'PNG'
	if engine == 'BLENDER_EEVEE':
		scene.eevee.taa_render_samples = samples
	scene.view_settings.view_transform = 'AgX'
	scene.view_settings.look = 'AgX - Medium High Contrast'
	world = bpy.data.worlds.get("PreviewWorld") or bpy.data.worlds.new("PreviewWorld")
	world.use_nodes = True
	bg = world.node_tree.nodes.get("Background")
	bg.inputs[0].default_value = (0.62, 0.66, 0.72, 1.0)
	bg.inputs[1].default_value = 0.9
	scene.world = world
	sun = bpy.data.objects.get("PreviewSun")
	if sun is None:
		data = bpy.data.lights.new("PreviewSun", 'SUN')
		data.energy = 3.2
		data.angle = math.radians(8)
		data.color = (1.0, 0.95, 0.86)
		sun = bpy.data.objects.new("PreviewSun", data)
		scene.collection.objects.link(sun)
	sun.rotation_euler = (math.radians(50), math.radians(10), math.radians(-35))
	return scene


def Camera(target, distance, yawDeg, pitchDeg, orthoScale):
	scene = bpy.context.scene
	cam = bpy.data.objects.get("PreviewCam")
	if cam is None:
		cam = bpy.data.objects.new("PreviewCam", bpy.data.cameras.new("PreviewCam"))
		scene.collection.objects.link(cam)
	cam.data.type = 'ORTHO'
	cam.data.ortho_scale = orthoScale
	cam.data.clip_end = 500
	yaw, pitch = math.radians(yawDeg), math.radians(pitchDeg)
	d = Vector((math.sin(yaw) * math.cos(pitch), -math.cos(yaw) * math.cos(pitch), math.sin(pitch)))
	cam.location = Vector(target) + d * distance
	cam.rotation_euler = (-d).to_track_quat('-Z', 'Y').to_euler()
	scene.camera = cam
	return cam


def Ground(size=60, color=(0.42, 0.46, 0.36)):
	g = bpy.data.objects.get("PreviewGround")
	if g is None:
		me = bpy.data.meshes.new("PreviewGround")
		s = size / 2
		me.from_pydata([(-s, -s, 0), (s, -s, 0), (s, s, 0), (-s, s, 0)], [], [(0, 1, 2, 3)])
		g = bpy.data.objects.new("PreviewGround", me)
		bpy.context.scene.collection.objects.link(g)
		mat = bpy.data.materials.new("PreviewGroundMat")
		mat.use_nodes = True
		mat.node_tree.nodes["Principled BSDF"].inputs["Base Color"].default_value = (*color, 1)
		mat.node_tree.nodes["Principled BSDF"].inputs["Roughness"].default_value = 1.0
		me.materials.append(mat)
	return g


def Render(path):
	settings = bpy.context.scene.render.image_settings
	if path.lower().endswith(".jpg"):
		settings.file_format = 'JPEG'
		settings.quality = 88
	else:
		settings.file_format = 'PNG'
	bpy.context.scene.render.filepath = path
	bpy.ops.render.render(write_still=True)
	Log("렌더: " + os.path.relpath(path, RES_DIR))


def LoadPixels(path):
	img = bpy.data.images.load(path)
	w, h = img.size
	px = np.array(img.pixels[:], dtype=np.float32).reshape(h, w, 4)
	bpy.data.images.remove(img)
	return px


def SavePixels(px, path):
	h, w = px.shape[:2]
	img = bpy.data.images.new("strip", w, h, alpha=True)
	img.pixels.foreach_set(px.ravel())
	if path.lower().endswith(".jpg"):
		settings = bpy.context.scene.render.image_settings
		settings.file_format = 'JPEG'
		settings.quality = 88
		img.save_render(path, scene=bpy.context.scene)
	else:
		img.filepath_raw = path
		img.file_format = 'PNG'
		img.save()
	bpy.data.images.remove(img)


def Stitch(paths, out, cols):
	tiles = [LoadPixels(p) for p in paths]
	h, w = tiles[0].shape[:2]
	rows = (len(tiles) + cols - 1) // cols
	sheet = np.ones((rows * h, cols * w, 4), dtype=np.float32)
	for i, t in enumerate(tiles):
		r, c = i // cols, i % cols
		y0 = (rows - 1 - r) * h
		sheet[y0:y0 + h, c * w:(c + 1) * w] = t
	SavePixels(sheet, out)
	for p in paths:
		os.remove(p)
	Log("시트: " + os.path.relpath(out, RES_DIR))


def PlayAction(arm, actionName, frame):
	act = bpy.data.actions[actionName]
	if arm.animation_data is None:
		arm.animation_data_create()
	arm.animation_data.action = act
	if act.slots:
		arm.animation_data.action_slot = act.slots[0]
	bpy.context.scene.frame_set(int(frame))
