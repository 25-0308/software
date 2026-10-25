# -*- coding: utf-8 -*-
# 네 발 짐승 애니메이션 — 다리마다 위상이 다른 걸음 주기를 IK로 디뎌서 굽는다.
#  걷기: 옆 순서 4박자(뒷왼 → 앞왼 → 뒷오른 → 앞오른) — 늑대·사슴·소의 평상 걸음
#  달리기: Canid 회전 갤럽(척추가 크게 접혔다 펴짐) / Cervid 바운드(뒷다리 둘이 거의 함께 차고 앞다리 둘이 함께 내림)
#          / Heavy 무거운 갤럽(몸이 앞뒤로 흔들림)
#  공격: Canid 웅크렸다 달려들며 물기 / Cervid 뒤로 젖혔다 머리로 들이받기 / Heavy 머리를 낮춰 들이받고 쳐올리기
import math
from rescommon import Animator, QWorld, Smooth, V, FPS
import quadruped_parts as QP

LEGS = ("FL", "FR", "HL", "HR")


def Limbs():
	out = []
	for side in ("L", "R"):
		out.append(dict(name="F" + side, ikBone="forearm." + side, poleDir=(0, 1, 0), rotBones=["frontfoot." + side, "fronttoe." + side]))
		out.append(dict(name="H" + side, ikBone="shin." + side, poleDir=(0, -1, 0), rotBones=["hindfoot." + side, "hindtoe." + side]))
	return out


GAITS = {
	"Canid": dict(
		walk=dict(frames=24, beta=0.62, S=0.32, lift=0.07, drop=0.02, ph=dict(HL=0.0, FL=0.25, HR=0.5, FR=0.75), bob=0.012, flex=0.0, rock=1.0, scap=8),
		run=dict(frames=14, beta=0.32, S=0.46, lift=0.13, drop=0.05, ph=dict(HL=0.0, HR=0.1, FR=0.45, FL=0.55), bob=0.035, flex=10.0, rock=4.0, scap=16),
		attack="bite"),
	"Cervid": dict(
		walk=dict(frames=28, beta=0.64, S=0.36, lift=0.09, drop=0.035, ph=dict(HL=0.0, FL=0.25, HR=0.5, FR=0.75), bob=0.015, flex=0.0, rock=1.5, scap=8),
		run=dict(frames=16, beta=0.30, S=0.56, lift=0.2, drop=0.07, ph=dict(HL=0.0, HR=0.04, FL=0.5, FR=0.54), bob=0.07, flex=7.0, rock=9.0, scap=14),
		attack="butt"),
	"Heavy": dict(
		walk=dict(frames=32, beta=0.66, S=0.38, lift=0.08, drop=0.04, ph=dict(HL=0.0, FL=0.25, HR=0.5, FR=0.75), bob=0.015, flex=0.0, rock=1.0, scap=7),
		run=dict(frames=18, beta=0.36, S=0.54, lift=0.14, drop=0.07, ph=dict(HL=0.0, HR=0.12, FL=0.5, FR=0.62), bob=0.045, flex=3.0, rock=5.0, scap=12),
		attack="gore"),
}


class QPose:
	def __init__(self, an, F, yaw=0.0, lift=0.0):
		self.an = an
		self.F = F
		self.Q = QWorld(yaw=yaw)
		self.root = V(0, 0, lift)
		if yaw:
			an.RotE("root", yaw=yaw)
		if lift:
			an.Move("root", (0, 0, lift))
		self.pelvis = V(0, 0, 0)

	def W(self, p):
		return self.root + self.Q @ V(p)

	def Body(self, pelvis=(0, 0, 0), pelvisRot=(0, 0, 0), spine=(0, 0, 0), chest=(0, 0, 0), neck=(0, 0, 0), head=(0, 0, 0),
		jaw=0.0, tail=(0, 0, 0), ears=0.0, scap=(0.0, 0.0)):
		self.pelvis = V(pelvis)
		an = self.an
		an.Move("pelvis", pelvis)
		an.RotE("pelvis", *pelvisRot)
		an.RotE("spine", *spine)
		an.RotE("chest", *chest)
		an.RotE("neck", *neck)
		an.RotE("head", *head)
		an.RotE("jaw", pitch=jaw)
		for i, b in enumerate(("tail1", "tail2", "tail3")):
			an.RotE(b, pitch=tail[0] * (0.6 + 0.2 * i), yaw=tail[1] * (0.5 + 0.35 * i), roll=tail[2])
		an.RotE("ear.L", roll=-ears)
		an.RotE("ear.R", roll=ears)
		an.RotE("scapula.L", pitch=scap[0])
		an.RotE("scapula.R", pitch=scap[1])

	def Rest(self, leg):
		F = self.F
		front = leg[0] == "F"
		c = V(F["fetF"] if front else F["fetH"])
		top = V(F["wrist"] if front else F["hock"])
		hip = V(F["shoulder"] if front else F["hipJ"])
		if leg[1] == "R":
			c.x, top.x, hip.x = -c.x, -top.x, -hip.x
		return c, top, hip

	def Foot(self, leg, contact, pitch=0.0, footYaw=0.0, toePitch=0.0):
		c, top, hip = self.Rest(leg)
		front = leg[0] == "F"
		q = QWorld(yaw=footYaw) @ QWorld(pitch=pitch)
		upper = V(contact) + q @ (top - c)
		mid = (hip + self.pelvis + upper) * 0.5
		pole = mid + QWorld(yaw=footYaw) @ V(0, 0.7 if front else -0.7, 0)
		self.an.Ik(leg, self.W(upper), self.W(pole))
		side = leg[1]
		fb, tb = ("frontfoot." + side, "fronttoe." + side) if front else ("hindfoot." + side, "hindtoe." + side)
		self.an.Orient(fb, self.Q @ q)
		self.an.Orient(tb, self.Q @ QWorld(yaw=footYaw) @ QWorld(pitch=toePitch))

	def Stand(self, spread=0.0):
		for leg in LEGS:
			c, _, _ = self.Rest(leg)
			self.Foot(leg, c)


def GaitLeg(p, leg, u, g):
	u = u % 1.0
	c, _, _ = p.Rest(leg)
	S, beta = g["S"], g["beta"]
	front = leg[0] == "F"
	if u < beta:
		s = u / beta
		pos = V(c.x, c.y - S / 2 + S * s, c.z)
		push = max(0.0, (s - 0.7) / 0.3)
		p.Foot(leg, pos, pitch=(18 if front else -8) * push, toePitch=0.0)
		return
	s = (u - beta) / (1 - beta)
	e = Smooth(s)
	pos = V(c.x, c.y + S / 2 - S * e, c.z + g["lift"] * math.sin(math.pi * s))
	fold = math.sin(math.pi * min(1.0, s * 1.15))
	if front:
		p.Foot(leg, pos, pitch=70 * fold, toePitch=40 * fold)
	else:
		pos.y += 0.04 * fold
		p.Foot(leg, pos, pitch=-20 * fold, toePitch=45 * fold)


def PoseGait(t, an, F, g, frame):
	p = QPose(an, F)
	c2 = math.cos(4 * math.pi * t)
	ph = g["ph"]
	hindPhase = math.sin(2 * math.pi * (t - ph["HL"] - g["beta"] / 2))
	frontPhase = math.sin(2 * math.pi * (t - ph["FL"] - g["beta"] / 2))
	flex = g["flex"] * math.sin(2 * math.pi * (t - ph["HL"] + 0.15))
	rock = g["rock"] * math.sin(2 * math.pi * (t - ph["HL"] + 0.3))
	bob = -g["drop"] - g["bob"] * (0.5 + 0.5 * c2 if g["flex"] == 0 else 0.5 + 0.5 * math.cos(2 * math.pi * (t - ph["HL"] - 0.1)))
	p.Body(pelvis=(0, 0, bob), pelvisRot=(rock + flex * 0.5, 2 * hindPhase if g["flex"] == 0 else 0, 0),
		spine=(-flex * 0.6, 0, 0), chest=(-flex * 0.6, -2 * frontPhase if g["flex"] == 0 else 0, 0),
		neck=(-rock * 0.6 + 3 * c2 * (1 if g["flex"] == 0 else 0), 0, 0), head=(-rock * 0.4 + flex * 0.6, 0, 0),
		tail=(8 + 6 * math.sin(2 * math.pi * t * 2), 10 * math.sin(2 * math.pi * t), 0),
		scap=(g["scap"] * math.cos(2 * math.pi * (t - ph["FL"])), g["scap"] * math.cos(2 * math.pi * (t - ph["FR"]))))
	for leg in LEGS:
		GaitLeg(p, leg, t - ph[leg], g)
	return p


def PoseIdle(t, an, F, amount=1.0):
	p = QPose(an, F)
	b = math.sin(2 * math.pi * t) * amount
	look = math.sin(2 * math.pi * t + 0.8) * amount
	p.Body(pelvis=(0, 0, -0.004 * b), chest=(0.8 * b, 0, 0), neck=(-2 * b, 6 * look, 0), head=(2 * b, 9 * look, 0),
		tail=(4 + 3 * b, 12 * math.sin(4 * math.pi * t) * amount, 0), ears=10 * max(0.0, math.sin(6 * math.pi * t)) * amount, jaw=0.0)
	p.Stand()
	return p


def PoseTurn(t, an, F, direction):
	yaw = 90.0 * direction * Smooth(t)
	p = QPose(an, F, yaw=yaw)
	p.Body(pelvis=(0, 0, -0.01 - 0.02 * math.sin(math.pi * t)), neck=(0, 10 * direction * math.sin(math.pi * min(1.0, t * 1.4)), 0),
		head=(0, 8 * direction * math.sin(math.pi * min(1.0, t * 1.4)), 0), tail=(6, -14 * direction * math.sin(math.pi * t), 0))
	if direction > 0:
		order = (("FL", 0.0, 0.32), ("HR", 0.18, 0.5), ("FR", 0.45, 0.77), ("HL", 0.62, 0.95))
	else:
		order = (("FR", 0.0, 0.32), ("HL", 0.18, 0.5), ("FL", 0.45, 0.77), ("HR", 0.62, 0.95))
	for leg, t0, t1 in order:
		c, _, _ = p.Rest(leg)
		s = max(0.0, min(1.0, (t - t0) / (t1 - t0)))
		e = Smooth(s)
		fy = 90.0 * direction * e
		world = QWorld(yaw=fy) @ c
		world.z += 0.08 * math.sin(math.pi * s)
		local = p.Q.inverted() @ world
		front = leg[0] == "F"
		fold = math.sin(math.pi * s)
		p.Foot(leg, local, pitch=(50 if front else -15) * fold, footYaw=fy - yaw, toePitch=30 * fold)
	return p


def PoseAttack(t, an, F, style):
	p = QPose(an, F)
	if style == "bite":
		k1 = Smooth(t / 0.35)
		k2 = Smooth((t - 0.35) / 0.15)
		k3 = Smooth((t - 0.6) / 0.4)
		crouch = k1 * (1 - k2)
		lunge = k2 * (1 - k3)
		jaw = 32 * Smooth((t - 0.3) / 0.12) * (1 - Smooth((t - 0.47) / 0.06)) + 8 * lunge * (1 - Smooth((t - 0.47) / 0.06))
		p.Body(pelvis=(0, 0.06 * crouch - 0.16 * lunge, -0.07 * crouch + 0.02 * lunge), pelvisRot=(4 * crouch - 6 * lunge, 0, 0),
			chest=(6 * crouch - 4 * lunge, 0, 0), neck=(12 * crouch - 18 * lunge, 0, 0), head=(-6 * crouch + 10 * lunge, 0, 0), jaw=jaw,
			tail=(-10 * crouch + 15 * lunge, 0, 0), ears=-25 * (crouch + lunge))
		for leg in LEGS:
			c, _, _ = p.Rest(leg)
			if leg[0] == "F":
				step = Smooth((t - 0.33) / 0.15) * (1 - Smooth((t - 0.62) / 0.3))
				lift = 0.06 * math.sin(math.pi * max(0.0, min(1.0, (t - 0.33) / 0.15)))
				p.Foot(leg, c + V(0, -0.14 * step, lift), pitch=40 * lift / 0.06)
			else:
				p.Foot(leg, c, pitch=-10 * lunge)
	elif style == "butt":
		k1 = Smooth(t / 0.4)
		k2 = Smooth((t - 0.4) / 0.14)
		k3 = Smooth((t - 0.62) / 0.38)
		rear = k1 * (1 - k2)
		ram = k2 * (1 - k3)
		p.Body(pelvis=(0, 0.08 * rear - 0.14 * ram, -0.02 * rear - 0.04 * ram), pelvisRot=(-4 * rear + 4 * ram, 0, 0),
			chest=(-8 * rear + 6 * ram, 0, 0), neck=(-14 * rear + 34 * ram, 0, 0), head=(-10 * rear + 20 * ram, 0, 0),
			tail=(20 * (rear + ram), 0, 0), ears=-20 * ram)
		for leg in LEGS:
			c, _, _ = p.Rest(leg)
			if leg == "FL":
				lift = 0.07 * math.sin(math.pi * max(0.0, min(1.0, t / 0.4))) * (1 - k2)
				p.Foot(leg, c + V(0, 0.02, lift), pitch=50 * lift / 0.07)
			else:
				p.Foot(leg, c)
	else:
		# 앞발로 땅을 긁고(0~0.3) 머리를 낮춰 달려들어(0.3~0.48) 쳐올림(0.48~0.6)
		scrape = math.sin(math.pi * max(0.0, min(1.0, t / 0.3)))
		k2 = Smooth((t - 0.3) / 0.18)
		k3 = Smooth((t - 0.48) / 0.12)
		k4 = Smooth((t - 0.62) / 0.38)
		low = Smooth(t / 0.3) * (1 - k3)
		toss = k3 * (1 - k4)
		charge = k2 * (1 - k4)
		p.Body(pelvis=(0, -0.14 * charge, -0.04 * low), pelvisRot=(3 * low, 0, 0), chest=(4 * low - 4 * toss, 0, 0),
			neck=(16 * low - 22 * toss, 0, 0), head=(14 * low - 20 * toss, 0, 0), tail=(25 * charge, 0, 0), ears=-15 * low)
		for leg in LEGS:
			c, _, _ = p.Rest(leg)
			if leg == "FR":
				pos = c + V(0, -0.08 + 0.16 * math.sin(math.pi * min(1.0, t / 0.15) * 2) * 0.5 * (t < 0.3), 0.05 * scrape)
				p.Foot(leg, pos, pitch=30 * scrape)
			else:
				p.Foot(leg, c)
	return p


def _Crouch(p, depth, rearUp=0.0, frontUp=0.0, tuck=0.0):
	F = p.F
	p.Body(pelvis=(0, 0.02, -depth), pelvisRot=(-rearUp * 20 + frontUp * 10, 0, 0), chest=(-frontUp * 10, 0, 0),
		neck=(-frontUp * 10 + depth * 30, 0, 0), head=(depth * 20, 0, 0), tail=(10 + 20 * frontUp, 0, 0), ears=-10)
	for leg in LEGS:
		c, _, _ = p.Rest(leg)
		front = leg[0] == "F"
		if front and frontUp > 0:
			lift = 0.25 * frontUp
			p.Foot(leg, c + V(0, 0.05 * frontUp, lift), pitch=70 * frontUp, toePitch=40 * frontUp)
		else:
			p.Foot(leg, c, pitch=(-20 if not front else 0) * rearUp)


def PoseJumpStart(t, an, F):
	p = QPose(an, F)
	if t < 0.55:
		k = Smooth(t / 0.55)
		_Crouch(p, 0.1 * k)
	else:
		k = Smooth((t - 0.55) / 0.45)
		_Crouch(p, 0.1 * (1 - k) - 0.03 * k, rearUp=k, frontUp=k)
	return p


def _AirPose(p, t, stretch=1.0):
	w = math.sin(2 * math.pi * t)
	p.Body(pelvis=(0, 0, 0.0), pelvisRot=(2 * w, 0, 0), chest=(-3 * w, 0, 0), neck=(-6, 0, 0), head=(4, 0, 0),
		tail=(-10 + 6 * w, 0, 0), ears=-20)
	for leg in LEGS:
		c, _, _ = p.Rest(leg)
		if leg[0] == "F":
			p.Foot(leg, c + V(0, 0.08, 0.22 * stretch), pitch=85 * stretch, toePitch=50)
		else:
			p.Foot(leg, c + V(0, 0.14 * stretch, 0.12 * stretch), pitch=-55 * stretch, toePitch=40)


def PoseJumpAir(t, an, F):
	p = QPose(an, F)
	_AirPose(p, t)
	return p


def PoseJumpLand(t, an, F):
	p = QPose(an, F)
	if t < 0.3:
		k = Smooth(t / 0.3)
		d = 0.09 * k
	else:
		k = Smooth((t - 0.3) / 0.7)
		d = 0.09 * (1 - k)
	p.Body(pelvis=(0, 0, -d), pelvisRot=(-d * 60, 0, 0), chest=(d * 50, 0, 0), neck=(d * 80, 0, 0), head=(-d * 40, 0, 0), tail=(8, 0, 0))
	p.Stand()
	return p


JUMP_HEIGHT = 0.5


def PoseJump(t, an, F):
	takeoff, land = 0.25, 0.75
	if t < takeoff:
		return PoseJumpStart(t / takeoff, an, F)
	if t < land:
		s = (t - takeoff) / (land - takeoff)
		p = QPose(an, F, lift=JUMP_HEIGHT * 4 * s * (1 - s))
		_AirPose(p, s * 0.5, stretch=0.5 + 0.5 * math.sin(math.pi * s))
		return p
	return PoseJumpLand((t - land) / (1 - land), an, F)


def Clips(frame):
	F = QP.FRAMES[frame]
	g = GAITS[frame]
	w, r = g["walk"], g["run"]

	def Contact(gait):
		return {"%s_contact" % leg: round(gait["ph"][leg] % 1.0, 4) for leg in ("HL", "FL", "HR", "FR")}
	return [
		("Pose_Base", 1, lambda t, an: PoseIdle(0.0, an, F, 0.0), False, {}),
		("Idle", 75, lambda t, an: PoseIdle(t, an, F), True, {}),
		("Walk", w["frames"], lambda t, an: PoseGait(t, an, F, w, frame), True, Contact(w)),
		("Run", r["frames"], lambda t, an: PoseGait(t, an, F, r, frame), True, Contact(r)),
		("Turn_L90", 30, lambda t, an: PoseTurn(t, an, F, 1), False, {}),
		("Turn_R90", 30, lambda t, an: PoseTurn(t, an, F, -1), False, {}),
		("Attack", 24, lambda t, an: PoseAttack(t, an, F, g["attack"]), False,
			{"hit": {"bite": 0.47, "butt": 0.52, "gore": 0.5}[g["attack"]], "recover": 0.62}),
		("Jump_Start", 10, lambda t, an: PoseJumpStart(t, an, F), False, {"takeoff": 1.0}),
		("Jump_Air", 16, lambda t, an: PoseJumpAir(t, an, F), True, {}),
		("Jump_Land", 12, lambda t, an: PoseJumpLand(t, an, F), False, {"impact": 0.3}),
		("Jump", 30, lambda t, an: PoseJump(t, an, F), False, {"takeoff": 0.25, "land": 0.75}),
	]


def GaitSpeed(g):
	return g["S"] / (g["beta"] * g["frames"] / FPS)


def BakeAll(arm, frame):
	an = Animator(arm, Limbs())
	clips = Clips(frame)
	for (name, frames, fn, loop, markers) in clips:
		an.Bake("%s" % name, frames, fn, loop=loop, markers=markers)
	an.Cleanup()
	return clips
