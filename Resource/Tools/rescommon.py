# -*- coding: utf-8 -*-
# 리소스 생성 공용 도구 — Blender 5.2 백그라운드에서 실행됨.
#  - 좌표: 블렌더 기준 Z 위, 캐릭터·짐승의 앞은 -Y, 1 블렌더 단위 = 1 m
#  - MeshBuilder: 로우폴리 도형(상자·로프트·관·회전체·타원체·각기둥)을 모아 메시 하나로 만듦(면마다 재질 역할)
#  - 리그: 뼈 생성, 거리 기반 스킨 가중치, IK/회전 표적으로 자세를 잡아 FK 키프레임으로 굽는 애니메이터
#  - 내보내기: .blend 원본 + .glb(glTF 2.0, Y 위)
import bpy
import bmesh
import math
import os
import json
import random
from contextlib import contextmanager
from mathutils import Vector, Matrix, Quaternion, Euler
from mathutils.bvhtree import BVHTree

import palette

RES_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FPS = 30


def V(*a):
	if len(a) == 1:
		return Vector(a[0])
	return Vector(a)


def Log(msg):
	print("[리소스] " + msg, flush=True)


def ResPath(*parts):
	p = os.path.join(RES_DIR, *parts)
	os.makedirs(os.path.dirname(p), exist_ok=True)
	return p


# ───────────────────────── 씬 / 컬렉션 ─────────────────────────

def ResetScene():
	bpy.ops.wm.read_factory_settings(use_empty=True)
	scene = bpy.context.scene
	scene.render.fps = FPS
	scene.unit_settings.system = 'METRIC'
	return scene


def GetCollection(name, parent=None):
	coll = bpy.data.collections.get(name)
	if coll is None:
		coll = bpy.data.collections.new(name)
		(parent or bpy.context.scene.collection).children.link(coll)
	return coll


# ───────────────────────── 재질 ─────────────────────────

def MakeMaterial(name, colorName, alpha=1.0):
	"""팔레트 색 하나짜리 재질. 이름이 같으면 색만 다시 칠함."""
	hexv, metal, rough, emit = palette.PALETTE[colorName]
	mat = bpy.data.materials.get(name)
	if mat is None:
		mat = bpy.data.materials.new(name)
	lin = palette.LinearColor(colorName)
	mat.diffuse_color = (lin[0], lin[1], lin[2], alpha)
	mat.metallic = metal
	mat.roughness = rough
	if not mat.node_tree:
		mat.use_nodes = True
	bsdf = mat.node_tree.nodes.get("Principled BSDF")
	bsdf.inputs["Base Color"].default_value = lin
	bsdf.inputs["Metallic"].default_value = metal
	bsdf.inputs["Roughness"].default_value = rough
	if emit > 0:
		bsdf.inputs["Emission Color"].default_value = lin
		bsdf.inputs["Emission Strength"].default_value = emit
	else:
		bsdf.inputs["Emission Strength"].default_value = 0.0
	if alpha < 1.0:
		bsdf.inputs["Alpha"].default_value = alpha
		mat.surface_render_method = 'BLENDED'
	mat["paletteColor"] = colorName
	return mat


def PaletteMaterial(colorName):
	return MakeMaterial(colorName, colorName)


# ───────────────────────── 메시 빌더 ─────────────────────────

def Mt(loc=(0, 0, 0), rot=(0, 0, 0), scale=(1, 1, 1)):
	"""위치·회전(도, XYZ)·크기로 4x4 변환."""
	return Matrix.LocRotScale(Vector(loc), Euler([math.radians(a) for a in rot], 'XYZ'), Vector(scale))


def FrameMatrix(origin, xAxis, yAxis, zAxis):
	m = Matrix.Identity(4)
	for i in range(3):
		m[i][0] = xAxis[i]
		m[i][1] = yAxis[i]
		m[i][2] = zAxis[i]
		m[i][3] = origin[i]
	return m


def Ellipse(center, rx, ry, n, z=None, phase=-math.pi / 2, square=2.0, zFn=None):
	"""수평 타원 링 (첫 꼭짓점이 앞(-Y) 한가운데). square > 2면 네모에 가까운 초타원."""
	c = Vector(center)
	pts = []
	for k in range(n):
		a = phase + 2 * math.pi * k / n
		ca, sa = math.cos(a), math.sin(a)
		e = 2.0 / square
		x = rx * math.copysign(abs(ca) ** e, ca)
		y = ry * math.copysign(abs(sa) ** e, sa)
		zz = c.z if z is None else z
		if zFn:
			zz += zFn(a, x, y)
		pts.append(Vector((c.x + x, c.y + y, zz)))
	return pts


class MeshBuilder:
	def __init__(self):
		self.verts = []
		self.faces = []
		self.faceRole = []
		self.roles = []
		self.vertBone = []
		self._bone = None
		self.M = None

	# ── 기본 ──
	def RoleIndex(self, role):
		if role not in self.roles:
			self.roles.append(role)
		return self.roles.index(role)

	@contextmanager
	def Rigid(self, bone):
		"""이 블록에서 만든 꼭짓점은 뼈 하나에 100% 붙음."""
		old = self._bone
		self._bone = bone
		try:
			yield
		finally:
			self._bone = old

	@contextmanager
	def Transform(self, M):
		"""이 블록에서 만든 도형에 변환을 곱함(겹치면 바깥 것이 나중에 적용)."""
		old = self.M
		self.M = M if old is None else old @ M
		try:
			yield
		finally:
			self.M = old

	def Add(self, verts, faces, role, M=None):
		base = len(self.verts)
		for p in verts:
			p = Vector(p)
			if M is not None:
				p = M @ p
			if self.M is not None:
				p = self.M @ p
			self.verts.append(p)
			self.vertBone.append(self._bone)
		ri = self.RoleIndex(role)
		for f in faces:
			self.faces.append([base + i for i in f])
			self.faceRole.append(ri)
		return list(range(base, len(self.verts)))

	def FaceCount(self):
		return len(self.faces)

	def Paint(self, faceStart, fn):
		"""faceStart 이후 면 중 fn(면 중심)이 역할을 돌려주는 면의 역할을 바꿈."""
		for fi in range(faceStart, len(self.faces)):
			c = Vector((0, 0, 0))
			for vi in self.faces[fi]:
				c += self.verts[vi]
			c /= len(self.faces[fi])
			r = fn(c)
			if r:
				self.faceRole[fi] = self.RoleIndex(r)

	def Mirrored(self):
		"""X를 뒤집은 복사본(뼈 태그 .L↔.R)."""
		m = MeshBuilder()
		m.roles = list(self.roles)
		for p, b in zip(self.verts, self.vertBone):
			m.verts.append(Vector((-p.x, p.y, p.z)))
			m.vertBone.append(SwapSide(b))
		m.faces = [list(reversed(f)) for f in self.faces]
		m.faceRole = list(self.faceRole)
		return m

	def Merge(self, other, M=None):
		base = len(self.verts)
		for p, b in zip(other.verts, other.vertBone):
			p = Vector(p)
			if M is not None:
				p = M @ p
			if self.M is not None:
				p = self.M @ p
			self.verts.append(p)
			self.vertBone.append(b if b is not None else self._bone)
		for f, r in zip(other.faces, other.faceRole):
			self.faces.append([base + i for i in f])
			self.faceRole.append(self.RoleIndex(other.roles[r]))

	# ── 도형 ──
	def Box(self, center, size, role, M=None):
		cx, cy, cz = center
		sx, sy, sz = size[0] / 2, size[1] / 2, size[2] / 2
		v = [(cx + x * sx, cy + y * sy, cz + z * sz) for z in (-1, 1) for y in (-1, 1) for x in (-1, 1)]
		f = [(0, 1, 3, 2), (4, 6, 7, 5), (0, 4, 5, 1), (2, 3, 7, 6), (0, 2, 6, 4), (1, 5, 7, 3)]
		return self.Add(v, f, role, M)

	def Frustum(self, c0, s0, c1, s1, role, M=None):
		"""아래 사각형(c0, s0=(sx,sy))에서 위 사각형(c1, s1)으로 가는 사각뿔대."""
		r0 = [(c0[0] + x * s0[0] / 2, c0[1] + y * s0[1] / 2, c0[2]) for (x, y) in ((-1, -1), (1, -1), (1, 1), (-1, 1))]
		r1 = [(c1[0] + x * s1[0] / 2, c1[1] + y * s1[1] / 2, c1[2]) for (x, y) in ((-1, -1), (1, -1), (1, 1), (-1, 1))]
		return self.Loft([r0, r1], role, M=M)

	def Loft(self, rings, role, cap0=True, cap1=True, M=None, closed=True):
		"""링(점 목록)들을 차례로 이음. 점이 하나인 링은 꼭짓점(뿔 끝)."""
		verts = []
		idx = []
		for ring in rings:
			start = len(verts)
			verts.extend(ring)
			idx.append(list(range(start, start + len(ring))))
		faces = []
		for i in range(len(rings) - 1):
			a, b = idx[i], idx[i + 1]
			if len(a) == 1 and len(b) == 1:
				continue
			if len(a) == 1:
				n = len(b)
				for k in range(n if closed else n - 1):
					faces.append((a[0], b[(k + 1) % n], b[k]))
			elif len(b) == 1:
				n = len(a)
				for k in range(n if closed else n - 1):
					faces.append((a[k], a[(k + 1) % n], b[0]))
			else:
				n = len(a)
				for k in range(n if closed else n - 1):
					faces.append((a[k], a[(k + 1) % n], b[(k + 1) % n], b[k]))
		if cap0 and len(idx[0]) > 2:
			faces.append(tuple(reversed(idx[0])))
		if cap1 and len(idx[-1]) > 2:
			faces.append(tuple(idx[-1]))
		return self.Add(verts, faces, role, M)

	def Tube(self, path, radii, n, role, up=(0, 0, 1), cap0=True, cap1=True, M=None, phase=0.0, offsets=None):
		"""점 경로를 따라 가는 관. radii 원소는 반지름 또는 (가로, 세로) 타원."""
		path = [Vector(p) for p in path]
		rings = Ring_Frames(path, up)
		out = []
		for i, (p, nrm, bin_) in enumerate(rings):
			r = radii[i]
			rx, ry = (r, r) if not isinstance(r, (tuple, list)) else r
			if rx <= 0 and ry <= 0:
				out.append([p])
				continue
			ring = []
			for k in range(n):
				a = phase + 2 * math.pi * k / n
				q = p + nrm * (math.cos(a) * rx) + bin_ * (math.sin(a) * ry)
				if offsets is not None:
					q = q + Vector(offsets[i])
				ring.append(q)
			out.append(ring)
		return self.Loft(out, role, cap0, cap1, M)

	def Cyl(self, p0, p1, r0, r1, n, role, up=(0, 0, 1), M=None, phase=0.0):
		return self.Tube([p0, p1], [r0, r1], n, role, up=up, M=M, phase=phase)

	def Lathe(self, profile, n, role, M=None, phase=0.0, sx=1.0, sy=1.0):
		"""(반지름, 높이) 단면을 Z축으로 돌린 회전체. 반지름 0은 꼭짓점."""
		rings = []
		for (r, z) in profile:
			if r <= 0:
				rings.append([Vector((0, 0, z))])
			else:
				rings.append([Vector((math.cos(phase + 2 * math.pi * k / n) * r * sx,
					math.sin(phase + 2 * math.pi * k / n) * r * sy, z)) for k in range(n)])
		return self.Loft(rings, role, M=M)

	def Ellipsoid(self, center, radii, seg, rings, role, M=None):
		rx, ry, rz = radii
		prof = []
		for i in range(rings + 1):
			t = math.pi * i / rings
			prof.append((math.sin(t), -math.cos(t)))
		c = Vector(center)
		rr = []
		for (s, z) in prof:
			if s < 1e-6:
				rr.append([c + Vector((0, 0, z * rz))])
			else:
				rr.append([c + Vector((math.cos(-math.pi / 2 + 2 * math.pi * k / seg) * s * rx,
					math.sin(-math.pi / 2 + 2 * math.pi * k / seg) * s * ry, z * rz)) for k in range(seg)])
		return self.Loft(rr, role, M=M)

	def Ico(self, center, radius, role, subdiv=1, jitter=0.0, seed=0, scale=(1, 1, 1), M=None):
		bm = bmesh.new()
		bmesh.ops.create_icosphere(bm, subdivisions=subdiv, radius=1.0)
		rnd = random.Random(seed)
		verts = []
		for v in bm.verts:
			p = v.co.copy()
			if jitter > 0:
				p *= 1.0 + rnd.uniform(-jitter, jitter)
			verts.append(Vector(center) + Vector((p.x * radius * scale[0], p.y * radius * scale[1], p.z * radius * scale[2])))
		faces = [tuple(v.index for v in f.verts) for f in bm.faces]
		bm.free()
		return self.Add(verts, faces, role, M)

	def Prism(self, poly, depth, role, M=None):
		"""XY 평면 다각형을 Z 방향으로 depth만큼 두께를 줌(가운데 정렬)."""
		n = len(poly)
		bottom = [(x, y, -depth / 2) for (x, y) in poly]
		top = [(x, y, depth / 2) for (x, y) in poly]
		faces = [tuple(reversed(range(n))), tuple(range(n, 2 * n))]
		for k in range(n):
			faces.append((k, (k + 1) % n, n + (k + 1) % n, n + k))
		return self.Add(bottom + top, faces, role, M)

	def Sheet(self, grid, thickness, role, M=None):
		"""점 격자(행마다 같은 개수)를 두께 있는 판으로 — 망토·두건·앞치마·잎·날개."""
		rows, cols = len(grid), len(grid[0])
		g = [[Vector(p) for p in row] for row in grid]
		verts = []
		for i in range(rows):
			for j in range(cols):
				du = g[i][min(j + 1, cols - 1)] - g[i][max(j - 1, 0)]
				dv = g[min(i + 1, rows - 1)][j] - g[max(i - 1, 0)][j]
				n = du.cross(dv)
				n = n.normalized() if n.length > 1e-9 else Vector((0, 0, 1))
				verts.append(g[i][j] + n * thickness / 2)
		for i in range(rows):
			for j in range(cols):
				du = g[i][min(j + 1, cols - 1)] - g[i][max(j - 1, 0)]
				dv = g[min(i + 1, rows - 1)][j] - g[max(i - 1, 0)][j]
				n = du.cross(dv)
				n = n.normalized() if n.length > 1e-9 else Vector((0, 0, 1))
				verts.append(g[i][j] - n * thickness / 2)
		off = rows * cols

		def Id(i, j, back=False):
			return i * cols + j + (off if back else 0)

		faces = []
		for i in range(rows - 1):
			for j in range(cols - 1):
				faces.append((Id(i, j), Id(i, j + 1), Id(i + 1, j + 1), Id(i + 1, j)))
				faces.append((Id(i, j, True), Id(i + 1, j, True), Id(i + 1, j + 1, True), Id(i, j + 1, True)))
		for j in range(cols - 1):
			faces.append((Id(0, j), Id(0, j, True), Id(0, j + 1, True), Id(0, j + 1)))
			faces.append((Id(rows - 1, j), Id(rows - 1, j + 1), Id(rows - 1, j + 1, True), Id(rows - 1, j, True)))
		for i in range(rows - 1):
			faces.append((Id(i, 0), Id(i + 1, 0), Id(i + 1, 0, True), Id(i, 0, True)))
			faces.append((Id(i, cols - 1), Id(i, cols - 1, True), Id(i + 1, cols - 1, True), Id(i + 1, cols - 1)))
		return self.Add(verts, faces, role, M)

	def Wedge(self, center, size, role, M=None):
		"""앞(-Y)으로 경사진 쐐기(삼각 단면, X 방향 두께)."""
		cx, cy, cz = center
		sx, sy, sz = size[0] / 2, size[1] / 2, size[2] / 2
		v = [(cx - sx, cy + sy, cz - sz), (cx + sx, cy + sy, cz - sz), (cx + sx, cy - sy, cz - sz), (cx - sx, cy - sy, cz - sz),
			(cx - sx, cy + sy, cz + sz), (cx + sx, cy + sy, cz + sz)]
		f = [(0, 3, 2, 1), (0, 1, 5, 4), (1, 2, 5), (0, 4, 3), (3, 4, 5, 2)]
		return self.Add(v, f, role, M)

	# ── 결과물 ──
	def Build(self, name, collection, materials, smooth=False):
		"""materials: 역할 이름 → bpy 재질."""
		me = bpy.data.meshes.new(name)
		me.from_pydata([tuple(v) for v in self.verts], [], [tuple(f) for f in self.faces])
		me.update()
		for role in self.roles:
			me.materials.append(materials[role] if isinstance(materials, dict) else materials(role))
		me.polygons.foreach_set("material_index", self.faceRole)
		me.polygons.foreach_set("use_smooth", [smooth] * len(self.faces))
		bm = bmesh.new()
		bm.from_mesh(me)
		bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
		bm.to_mesh(me)
		bm.free()
		me.update()
		obj = bpy.data.objects.new(name, me)
		collection.objects.link(obj)
		return obj


def SwapSide(tag):
	if isinstance(tag, str):
		if tag.endswith(".L"):
			return tag[:-2] + ".R"
		if tag.endswith(".R"):
			return tag[:-2] + ".L"
		return tag
	if isinstance(tag, dict):
		return {SwapSide(k): v for k, v in tag.items()}
	return tag


def Rect(c, u, v, wu, wv):
	"""중심 c, 축 u·v, 폭 wu·wv의 사각 링(4점)."""
	c, u, v = Vector(c), Vector(u), Vector(v)
	return [c - u * wu / 2 - v * wv / 2, c + u * wu / 2 - v * wv / 2, c + u * wu / 2 + v * wv / 2, c - u * wu / 2 + v * wv / 2]


def Ring_Frames(path, up):
	"""경로 점마다 (점, 법선, 종법선) — 평행 이동 프레임."""
	n = len(path)
	tangents = []
	for i in range(n):
		if i == 0:
			t = path[1] - path[0]
		elif i == n - 1:
			t = path[-1] - path[-2]
		else:
			t = (path[i + 1] - path[i]).normalized() + (path[i] - path[i - 1]).normalized()
		tangents.append(t.normalized())
	upv = Vector(up)
	if abs(tangents[0].dot(upv)) > 0.95:
		upv = Vector((0, -1, 0)) if abs(tangents[0].y) < 0.95 else Vector((1, 0, 0))
	nrm = (upv - tangents[0] * upv.dot(tangents[0])).normalized()
	out = []
	for i in range(n):
		if i > 0:
			nrm = (nrm - tangents[i] * nrm.dot(tangents[i]))
			if nrm.length < 1e-6:
				nrm = tangents[i].orthogonal()
			nrm.normalize()
		b = tangents[i].cross(nrm).normalized()
		out.append((path[i], nrm, b))
	return out


def SurfacePoint(bvh, origin, direction, fallback):
	hit = bvh.ray_cast(Vector(origin), Vector(direction).normalized())
	if hit[0] is None:
		return Vector(fallback)
	return hit[0]


def BvhFromBuilder(mb):
	return BVHTree.FromPolygons([tuple(v) for v in mb.verts], mb.faces)


# ───────────────────────── 뼈대 ─────────────────────────

class BoneSpec:
	def __init__(self, name, head, tail, parent=None, deform=True, rollAxis=None):
		self.name = name
		self.head = Vector(head)
		self.tail = Vector(tail)
		self.parent = parent
		self.deform = deform
		self.rollAxis = rollAxis


def MirrorSpecs(specs):
	"""'.L' 뼈를 X 반전한 '.R' 뼈를 더함."""
	out = list(specs)
	for s in specs:
		if s.name.endswith(".L"):
			parent = s.parent[:-2] + ".R" if s.parent and s.parent.endswith(".L") else s.parent
			out.append(BoneSpec(s.name[:-2] + ".R", (-s.head.x, s.head.y, s.head.z), (-s.tail.x, s.tail.y, s.tail.z),
				parent, s.deform, s.rollAxis))
	return out


def BuildArmature(name, specs, collection):
	data = bpy.data.armatures.new(name)
	obj = bpy.data.objects.new(name, data)
	collection.objects.link(obj)
	data.display_type = 'STICK'
	obj.show_in_front = True
	bpy.context.view_layer.objects.active = obj
	bpy.ops.object.mode_set(mode='EDIT')
	for s in specs:
		eb = data.edit_bones.new(s.name)
		eb.head = s.head
		eb.tail = s.tail
		d = (s.tail - s.head).normalized()
		axis = s.rollAxis
		if axis is None:
			axis = (0, 0, 1) if abs(d.y) > 0.8 else (0, -1, 0)
		eb.align_roll(Vector(axis))
		eb.use_deform = s.deform
	for s in specs:
		if s.parent:
			data.edit_bones[s.name].parent = data.edit_bones[s.parent]
	bpy.ops.object.mode_set(mode='OBJECT')
	for pb in obj.pose.bones:
		pb.rotation_mode = 'QUATERNION'
	return obj


def BoneSegments(arm):
	return {b.name: (b.head_local.copy(), b.tail_local.copy()) for b in arm.data.bones}


def BoneMatrix(arm, name):
	return arm.data.bones[name].matrix_local.copy()


def _SegDist(p, a, b):
	ab = b - a
	t = max(0.0, min(1.0, (p - a).dot(ab) / max(ab.length_squared, 1e-12)))
	return (p - (a + ab * t)).length


def SkinObject(obj, mb, arm, candidates, power=5.0, maxInfluence=2, weightFn=None, factors=None):
	"""거리 기반 스킨 가중치. 꼭짓점에 Rigid 뼈가 있으면 그것만, 아니면 후보 뼈 중 가까운 것들을 섞음."""
	segs = BoneSegments(arm)
	groups = {}

	def Group(name):
		if name not in groups:
			groups[name] = obj.vertex_groups.new(name=name)
		return groups[name]

	for i, p in enumerate(mb.verts):
		tag = mb.vertBone[i]
		if isinstance(tag, str):
			Group(tag).add([i], 1.0, 'REPLACE')
			continue
		if isinstance(tag, dict):
			ws = tag
		elif weightFn is not None:
			ws = weightFn(p)
		else:
			ws = None
		if ws is None:
			scored = []
			for b in candidates:
				h, t = segs[b]
				d = max(_SegDist(p, h, t), 0.004)
				f = factors.get(b, 1.0) if factors else 1.0
				scored.append((f / d ** power, b))
			scored.sort(reverse=True)
			scored = scored[:maxInfluence]
			total = sum(s for s, _ in scored)
			ws = {b: s / total for s, b in scored}
		total = sum(ws.values())
		for b, w in ws.items():
			w = w / total
			if w > 0.02:
				Group(b).add([i], w, 'REPLACE')
	obj.parent = arm
	mod = obj.modifiers.new("Armature", 'ARMATURE')
	mod.object = arm
	return obj


def RigidParent(obj, arm, bone):
	obj.vertex_groups.new(name=bone).add(list(range(len(obj.data.vertices))), 1.0, 'REPLACE')
	obj.parent = arm
	mod = obj.modifiers.new("Armature", 'ARMATURE')
	mod.object = arm
	return obj


# ───────────────────────── 애니메이션 ─────────────────────────

def Smooth(t):
	t = max(0.0, min(1.0, t))
	return t * t * (3 - 2 * t)


def Lerp(a, b, t):
	return a + (b - a) * t


def QWorld(pitch=0.0, yaw=0.0, roll=0.0):
	"""월드 축 기준 회전(도, 오른손 법칙).
	pitch=+X축: 위로 선 뼈는 앞(-Y)으로 숙고, 아래로 뻗은 뼈는 뒤(+Y)로 감.
	roll=+Y축: 위로 선 뼈가 캐릭터 왼쪽(+X)으로 기욺.
	yaw=+Z축: 캐릭터가 왼쪽으로 돎."""
	return (Quaternion((0, 0, 1), math.radians(yaw)) @ Quaternion((0, 1, 0), math.radians(roll))
		@ Quaternion((1, 0, 0), math.radians(pitch)))


class Animator:
	"""IK 표적(빈 오브젝트)과 FK 회전으로 자세를 잡고, 평가된 결과를 뼈 로컬 키프레임으로 굽는다.
	limbs: [{name, ikBone, poleDir, rotBones:[...]}]
	  - ikBone: 체인 끝 뼈(정강이·아래팔). 그 꼬리가 표적 위치로 감(체인 2마디).
	  - rotBones: 월드 회전을 직접 정하는 뼈(발·발가락·손 등) — Copy Rotation 표적."""

	def __init__(self, arm, limbs, deformOnly=True):
		self.arm = arm
		self.limbs = {l["name"]: l for l in limbs}
		self.rest = {b.name: b.matrix_local.copy() for b in arm.data.bones}
		self.restQ = {n: m.to_quaternion() for n, m in self.rest.items()}
		self.bones = [b.name for b in arm.data.bones if (b.use_deform or not deformOnly)]
		self.empties = []
		self.ik = {}
		self.pole = {}
		self.rot = {}
		coll = GetCollection("_AnimControls")
		for l in limbs:
			pb = arm.pose.bones[l["ikBone"]]
			tgt = self._Empty("IK_" + l["name"], coll)
			pole = self._Empty("POLE_" + l["name"], coll)
			c = pb.constraints.new('IK')
			c.target = tgt
			c.pole_target = pole
			c.chain_count = 2
			c.use_tail = True
			c.use_stretch = False
			l["constraint"] = c
			self.ik[l["name"]] = tgt
			self.pole[l["name"]] = pole
			for rb in l.get("rotBones", []):
				e = self._Empty("ROT_" + rb, coll)
				cr = arm.pose.bones[rb].constraints.new('COPY_ROTATION')
				cr.target = e
				cr.owner_space = 'WORLD'
				cr.target_space = 'WORLD'
				self.rot[rb] = e
		self.Reset()
		for l in limbs:
			self._DetectPoleAngle(l)
		self.Reset()

	def _Empty(self, name, coll):
		e = bpy.data.objects.new(name, None)
		e.empty_display_size = 0.05
		coll.objects.link(e)
		self.empties.append(e)
		return e

	# ── 기본 ──
	def RestHead(self, bone):
		return self.arm.data.bones[bone].head_local.copy()

	def RestTail(self, bone):
		return self.arm.data.bones[bone].tail_local.copy()

	def Reset(self):
		for pb in self.arm.pose.bones:
			pb.location = (0, 0, 0)
			pb.rotation_quaternion = (1, 0, 0, 0)
			pb.scale = (1, 1, 1)
		for name, l in self.limbs.items():
			b = self.arm.data.bones[l["ikBone"]]
			self.ik[name].location = b.tail_local
			mid = b.head_local
			self.pole[name].location = mid + Vector(l["poleDir"]) * 0.5
			l["constraint"].influence = 1.0
		for rb, e in self.rot.items():
			e.rotation_mode = 'QUATERNION'
			e.rotation_quaternion = self.restQ[rb]
			e.location = self.rest[rb].translation

	def _DetectPoleAngle(self, l):
		c = l["constraint"]
		b = self.arm.data.bones[l["ikBone"]]
		root = b.parent.head_local
		tip = b.tail_local
		self.ik[l["name"]].location = root + (tip - root) * 0.7
		poleDir = Vector(l["poleDir"]).normalized()
		best, bestAng = -9, 0
		for ang in (0, 90, -90, 180):
			c.pole_angle = math.radians(ang)
			bpy.context.view_layer.update()
			ev = self.arm.evaluated_get(bpy.context.evaluated_depsgraph_get())
			mid = ev.pose.bones[l["ikBone"]].head
			d = (mid - (root + tip) * 0.5)
			score = d.normalized().dot(poleDir) if d.length > 1e-5 else -1
			if score > best:
				best, bestAng = score, ang
		c.pole_angle = math.radians(bestAng)
		l["poleAngle"] = bestAng

	# ── 자세 지정 ──
	def Rot(self, bone, q):
		"""뼈를 월드(아마추어) 축 기준 회전 q만큼 돌림(부모 회전 위에 쌓임)."""
		r = self.restQ[bone]
		self.arm.pose.bones[bone].rotation_quaternion = r.inverted() @ q @ r

	def RotE(self, bone, pitch=0.0, yaw=0.0, roll=0.0):
		self.Rot(bone, QWorld(pitch, yaw, roll))

	def Move(self, bone, delta):
		"""뼈를 부모 기준(쉬는 자세의 축)으로 delta만큼 옮김."""
		self.arm.pose.bones[bone].location = self.restQ[bone].inverted() @ Vector(delta)

	def Ik(self, limb, pos, pole=None):
		self.ik[limb].location = Vector(pos)
		if pole is not None:
			self.pole[limb].location = Vector(pole)

	def Orient(self, bone, q):
		"""이 뼈의 월드 회전을 '쉬는 자세에서 q만큼 돌린 것'으로 고정."""
		self.rot[bone].rotation_quaternion = q @ self.restQ[bone]

	def IkInfluence(self, limb, w):
		self.limbs[limb]["constraint"].influence = w

	# ── 굽기 ──
	def _Capture(self):
		bpy.context.view_layer.update()
		dg = bpy.context.evaluated_depsgraph_get()
		ev = self.arm.evaluated_get(dg)
		mats = {b: ev.pose.bones[b].matrix.copy() for b in self.bones}
		out = {}
		for b in self.bones:
			bone = self.arm.data.bones[b]
			M = mats[b]
			if bone.parent:
				pm = mats.get(bone.parent.name)
				if pm is None:
					pm = ev.pose.bones[bone.parent.name].matrix.copy()
				relRest = bone.parent.matrix_local.inverted() @ bone.matrix_local
				local = relRest.inverted() @ pm.inverted() @ M
			else:
				local = bone.matrix_local.inverted() @ M
			loc, rot, _ = local.decompose()
			out[b] = (loc, rot)
		return out

	def Bake(self, name, frames, fn, loop=True, markers=None):
		"""fn(t, anim)으로 t=0..1 자세를 잡아 frames+1개 키를 굽는다(루프면 마지막 키 = 처음 자세)."""
		samples = []
		for i in range(frames + 1):
			t = i / frames if frames > 0 else 0.0
			self.Reset()
			fn(t, self)
			samples.append(self._Capture())
		self.Reset()
		# 쿼터니언 부호 연속성
		for b in self.bones:
			prev = None
			for s in samples:
				loc, q = s[b]
				if prev is not None and prev.dot(q) < 0:
					q = -q
					s[b] = (loc, q)
				prev = q
		arm = self.arm
		if arm.animation_data is None:
			arm.animation_data_create()
		action = bpy.data.actions.new(name)
		action.use_fake_user = True
		arm.animation_data.action = action
		n = len(samples)
		for b in self.bones:
			path = 'pose.bones["%s"]' % b
			for idx in range(3):
				fc = action.fcurve_ensure_for_datablock(arm, path + ".location", index=idx, group_name=b)
				fc.keyframe_points.add(n)
				co = []
				for i, s in enumerate(samples):
					co += [i, s[b][0][idx]]
				fc.keyframe_points.foreach_set("co", co)
				for kp in fc.keyframe_points:
					kp.interpolation = 'LINEAR'
				fc.update()
			for idx in range(4):
				fc = action.fcurve_ensure_for_datablock(arm, path + ".rotation_quaternion", index=idx, group_name=b)
				fc.keyframe_points.add(n)
				co = []
				for i, s in enumerate(samples):
					co += [i, s[b][1][idx]]
				fc.keyframe_points.foreach_set("co", co)
				for kp in fc.keyframe_points:
					kp.interpolation = 'LINEAR'
				fc.update()
		action.use_frame_range = True
		action.frame_start = 0
		action.frame_end = max(frames, 1)
		action.use_cyclic = loop
		for mname, mt in (markers or {}).items():
			m = action.pose_markers.new(mname)
			m.frame = int(round(mt * frames))
		slot = arm.animation_data.action_slot
		arm.animation_data.action = None
		track = arm.animation_data.nla_tracks.new()
		track.name = name
		strip = track.strips.new(name, 0, action)
		strip.action_slot = slot
		track.mute = True
		return action

	def Cleanup(self):
		for l in self.limbs.values():
			pb = self.arm.pose.bones[l["ikBone"]]
			pb.constraints.remove(l["constraint"])
		for rb in self.rot:
			pb = self.arm.pose.bones[rb]
			for c in list(pb.constraints):
				pb.constraints.remove(c)
		for e in self.empties:
			bpy.data.objects.remove(e, do_unlink=True)
		coll = bpy.data.collections.get("_AnimControls")
		if coll:
			bpy.data.collections.remove(coll)
		for pb in self.arm.pose.bones:
			pb.location = (0, 0, 0)
			pb.rotation_quaternion = (1, 0, 0, 0)
			pb.scale = (1, 1, 1)


# ───────────────────────── 내보내기 ─────────────────────────

def SelectOnly(objs):
	for o in bpy.context.view_layer.objects:
		o.select_set(False)
	for o in objs:
		o.hide_set(False)
		o.select_set(True)
	if objs:
		bpy.context.view_layer.objects.active = objs[0]


def ExportGlb(path, objs, animations=True, separate=False, textureDir=""):
	"""separate=True면 .gltf + .bin으로 쓰고 텍스처는 textureDir(이 파일 기준 상대 경로)의 그림을 가리킴."""
	SelectOnly(objs)
	bpy.ops.export_scene.gltf(
		filepath=path,
		export_format='GLTF_SEPARATE' if separate else 'GLB',
		export_texture_dir=textureDir,
		use_selection=True,
		export_apply=False,
		export_yup=True,
		export_animations=animations,
		export_animation_mode='NLA_TRACKS',
		export_force_sampling=True,
		export_def_bones=True,
		export_optimize_animation_size=False,
		export_anim_slide_to_zero=False,
		export_skins=True,
		export_vertex_color='NONE',
		export_extras=True,
	)
	Log("내보냄: " + os.path.relpath(path, RES_DIR))


def SaveBlend(path):
	bpy.ops.wm.save_as_mainfile(filepath=path, compress=True)
	# 블렌더가 남기는 백업 파일은 지움
	if os.path.exists(path + "1"):
		os.remove(path + "1")
	Log("저장함: " + os.path.relpath(path, RES_DIR))


def WriteJson(path, data):
	with open(path, "w", encoding="utf-8", newline="\r\n") as f:
		json.dump(data, f, ensure_ascii=False, indent="\t")
	Log("기록함: " + os.path.relpath(path, RES_DIR))
