# -*- coding: utf-8 -*-
# 인간형 애니메이션 — IK 표적으로 자세를 잡아 FK 키로 구움(30fps).
# 모든 클립은 제자리(in-place). 회전(Turn_*)은 root 뼈의 yaw, 점프(Jump)는 root 뼈의 높이를 루트 모션으로 가짐.
# 오른손은 소품을 든 자세(지팡이·창이 하늘을 향함)를 기본으로 함 — 빈손이어도 자연스러운 '허리춤에 둔 손'.
import math
from mathutils import Vector, Quaternion
from rescommon import Animator, QWorld, Smooth, Lerp, V, FPS
import humanoid_parts as HP

J = HP.J

LIMBS = [
	dict(name="legL", ikBone="shin.L", poleDir=(0, -1, 0), rotBones=["foot.L", "toe.L"]),
	dict(name="legR", ikBone="shin.R", poleDir=(0, -1, 0), rotBones=["foot.R", "toe.R"]),
	dict(name="armL", ikBone="forearm.L", poleDir=(0.35, 1, -0.4), rotBones=["hand.L"]),
	dict(name="armR", ikBone="forearm.R", poleDir=(-0.35, 1, -0.4), rotBones=["hand.R"]),
]

HANG = {"L": QWorld(roll=45), "R": QWorld(roll=-45)}
HOLD_R = QWorld(pitch=-88) @ HANG["R"]
THRUST_R = QWorld(yaw=-6) @ HANG["R"]
BALL = {"L": V(J["ball"].x, J["ball"].y, J["ball"].z), "R": V(-J["ball"].x, J["ball"].y, J["ball"].z)}
ANKLE_FROM_BALL = V(0, J["ankle"].y - J["ball"].y, J["ankle"].z - J["ball"].z)
SHOULDER = {"L": V(J["shoulder"]), "R": V(-J["shoulder"].x, J["shoulder"].y, J["shoulder"].z)}
WRIST_HANG = {"L": V(0.236, 0.005, 0.893), "R": V(-0.236, 0.005, 0.893)}
WRIST_HOLD_R = V(-0.205, -0.17, 1.02)
# 왼손: 팔꿈치를 살짝 굽혀 든 자세(긴 소품은 비스듬히 서고, 빈손이면 편히 내린 손)
HOLD_L = QWorld(pitch=-58) @ HANG["L"]
WRIST_HOLD_L = V(0.228, -0.11, 0.975)


def CarryArmL(phi, lift=0.0):
	"""왼손 든 자세를 어깨 기준으로 phi(도, 앞으로 +)만큼 흔듦."""
	sh = SHOULDER["L"]
	q = QWorld(pitch=-phi)
	w = sh + q @ (WRIST_HOLD_L - sh) + V(0, 0, lift)
	return w, q @ HOLD_L


class Pose:
	"""한 프레임의 자세. 위치·회전은 root 기준(root-local)으로 받고, root의 yaw·높이를 곱해 월드로 바꿈."""

	def __init__(self, an, yaw=0.0, lift=0.0):
		self.an = an
		self.Q = QWorld(yaw=yaw)
		self.root = V(0, 0, lift)
		if yaw != 0.0:
			an.RotE("root", yaw=yaw)
		if lift != 0.0:
			an.Move("root", (0, 0, lift))
		self.pelvis = V(0, 0, 0)

	def W(self, p):
		return self.root + self.Q @ V(p)

	def Body(self, pelvis=(0, 0, 0), pelvisRot=(0, 0, 0), spine=(0, 0, 0), chest=(0, 0, 0), neck=(0, 0, 0), head=(0, 0, 0)):
		self.pelvis = V(pelvis)
		self.an.Move("pelvis", pelvis)
		self.an.RotE("pelvis", *pelvisRot)
		self.an.RotE("spine", *spine)
		self.an.RotE("chest", *chest)
		self.an.RotE("neck", *neck)
		self.an.RotE("head", *head)

	def _Knee(self, side, ankle, footYaw):
		sx = 1 if side == "L" else -1
		hip = V(sx * 0.09, 0, 0.92) + self.pelvis
		mid = (hip + ankle) * 0.5
		return mid + QWorld(yaw=footYaw) @ V(sx * 0.05, -0.8, 0.1)

	def FootBall(self, side, ball, pitch=0.0, footYaw=0.0, toePitch=0.0):
		"""발 앞꿈치(볼)를 ball에 두고 뒤꿈치를 pitch만큼 듦(도)."""
		q = QWorld(yaw=footYaw) @ QWorld(pitch=pitch)
		ankle = V(ball) + q @ ANKLE_FROM_BALL
		self.an.Ik("leg" + side, self.W(ankle), self.W(self._Knee(side, ankle, footYaw)))
		self.an.Orient("foot." + side, self.Q @ q)
		self.an.Orient("toe." + side, self.Q @ QWorld(yaw=footYaw) @ QWorld(pitch=toePitch))

	def FootAnkle(self, side, ankle, pitch=0.0, footYaw=0.0, toePitch=None):
		"""발목을 ankle에 두고 발을 pitch만큼 기울임(+면 발끝이 아래)."""
		q = QWorld(yaw=footYaw) @ QWorld(pitch=pitch)
		self.an.Ik("leg" + side, self.W(ankle), self.W(self._Knee(side, ankle, footYaw)))
		self.an.Orient("foot." + side, self.Q @ q)
		tp = QWorld(yaw=footYaw) @ QWorld(pitch=pitch if toePitch is None else toePitch)
		self.an.Orient("toe." + side, self.Q @ tp)

	def Hand(self, side, wrist, q, pole=None):
		sx = 1 if side == "L" else -1
		if pole is None:
			pole = SHOULDER[side] + V(sx * 0.3, 0.5, -0.35)
		self.an.Ik("arm" + side, self.W(wrist), self.W(pole))
		self.an.Orient("hand." + side, self.Q @ q)

	def Stand(self, footSpread=0.0):
		for side in ("L", "R"):
			b = V(BALL[side])
			b.x += (footSpread if side == "L" else -footSpread)
			self.FootBall(side, b)


def SwingArm(side, phi, bend=0.0):
	"""매달린 팔을 phi(도, 앞으로 +)만큼 흔든 손목 위치와 손 방향."""
	sh = SHOULDER[side]
	h = WRIST_HANG[side] - sh
	q = QWorld(pitch=-phi)
	w = sh + q @ h
	w += V(0, -bend, bend * 0.6)
	return w, QWorld(pitch=-phi * 0.9 - bend * 60) @ HANG[side]


def IdleArms(p, t=0.0, breathe=0.0):
	w, q = CarryArmL(1.5 * breathe)
	p.Hand("L", w, q)
	p.Hand("R", WRIST_HOLD_R + V(0, 0, breathe * 0.006), HOLD_R)


def PoseIdle(t, an, breatheAmt=1.0):
	p = Pose(an)
	b = math.sin(2 * math.pi * t) * breatheAmt
	sway = math.sin(2 * math.pi * t + 0.6) * breatheAmt
	p.Body(pelvis=(0.006 * sway, 0, -0.008 + 0.004 * b), pelvisRot=(0, 0, -1.2 * sway), spine=(1 + 0.6 * b, 0, 0.6 * sway),
		chest=(-0.8 * b, 0, 0.6 * sway), neck=(0, 0, 0), head=(-1.5, 3 * math.sin(2 * math.pi * t * 1.0 + 1.2) * breatheAmt, 0))
	p.Stand(0.015)
	IdleArms(p, t, b)
	return p


# ── 걸음 주기 ──

def GaitLeg(p, side, u, beta, yF, yB, lift, heelMax, kick=0.0, x=None):
	sx = 1 if side == "L" else -1
	x = sx * 0.09 if x is None else sx * x
	u = u % 1.0
	if u < beta:
		s = u / beta
		by = yF + (yB - yF) * s
		pitch = heelMax * max(0.0, (s - 0.55) / 0.45) ** 1.6
		p.FootBall(side, V(x, by, J["ball"].z), pitch=pitch)
		return
	s = (u - beta) / (1 - beta)
	q0 = QWorld(pitch=heelMax)
	a0 = V(x, yB, J["ball"].z) + q0 @ ANKLE_FROM_BALL
	a1 = V(x, yF, J["ball"].z) + ANKLE_FROM_BALL
	e = Smooth(s)
	ankle = a0.lerp(a1, e)
	ankle.z += lift * math.sin(math.pi * s) ** 1.2
	if kick > 0:
		k = math.sin(math.pi * min(1.0, s * 1.6)) * (1 - s)
		ankle.y += kick * k
		ankle.z += kick * 0.9 * k
	pitch = heelMax * (1 - e) ** 1.5 - 14 * math.sin(math.pi * e) ** 2 * (1 - e)
	p.FootAnkle(side, ankle, pitch=pitch, toePitch=pitch * 0.6)


WALK = dict(frames=27, beta=0.62, yF=-0.37, yB=0.22, lift=0.07, heel=32)
RUN = dict(frames=20, beta=0.36, yF=-0.36, yB=0.30, lift=0.13, heel=42, kick=0.20)


def StanceSpeed(g):
	"""한 주기 동안 발이 땅을 미는 거리 / 시간 = 몸이 나아가는 속도(m/s)."""
	T = g["frames"] / FPS
	return (g["yB"] - g["yF"]) / (g["beta"] * T)


def PoseWalk(t, an):
	p = Pose(an)
	c, s2 = math.cos(2 * math.pi * t), math.sin(2 * math.pi * (t - 0.06))
	p.Body(pelvis=(0, 0, -0.035 - 0.012 * math.cos(4 * math.pi * t)), pelvisRot=(2, -5 * c, -3 * s2),
		spine=(2, 4 * c, 1.5 * s2), chest=(1, 6 * c, 1.0 * s2), neck=(0, -2 * c, 0), head=(-2, -3 * c, 0))
	g = WALK
	GaitLeg(p, "L", t, g["beta"], g["yF"], g["yB"], g["lift"], g["heel"])
	GaitLeg(p, "R", t + 0.5, g["beta"], g["yF"], g["yB"], g["lift"], g["heel"])
	w, q = CarryArmL(-16 * c)
	p.Hand("L", w, q)
	p.Hand("R", WRIST_HOLD_R + V(0, 0.02 * c, 0.01 * math.cos(4 * math.pi * t)), HOLD_R)
	return p


def PoseRun(t, an):
	p = Pose(an)
	g = RUN
	c = math.cos(2 * math.pi * t)
	bob = -0.075 - 0.03 * math.cos(4 * math.pi * (t - g["beta"] / 2))
	p.Body(pelvis=(0, -0.02, bob), pelvisRot=(6, -7 * c, 0), spine=(7, 6 * c, 0), chest=(3, 8 * c, 0), neck=(-4, -3 * c, 0), head=(-6, -4 * c, 0))
	GaitLeg(p, "L", t, g["beta"], g["yF"], g["yB"], g["lift"], g["heel"], kick=g["kick"], x=0.085)
	GaitLeg(p, "R", t + 0.5, g["beta"], g["yF"], g["yB"], g["lift"], g["heel"], kick=g["kick"], x=0.085)
	for side, ph in (("L", 1.0), ("R", -1.0)):
		sx = 1 if side == "L" else -1
		phi = -40 * c * ph
		sh = SHOULDER[side]
		rel = V(sx * 0.05, -0.22, -0.30)
		w = sh + QWorld(pitch=-phi) @ rel
		if side == "R":
			q = QWorld(pitch=-phi * 0.7) @ QWorld(pitch=-55) @ HANG["R"]
		else:
			q = QWorld(pitch=-phi * 0.8) @ QWorld(pitch=-70) @ HANG["L"]
		p.Hand(side, w, q, pole=sh + V(sx * 0.25, 0.6, -0.2))
	return p


# ── 회전 ──

def PoseTurn(t, an, direction):
	"""direction = 1: 왼쪽으로 90도, -1: 오른쪽으로 90도. 먼저 도는 쪽 발이 먼저 디딤."""
	yaw = 90.0 * direction * Smooth(t)
	p = Pose(an, yaw=yaw)
	dip = 0.018 * math.sin(math.pi * t)
	p.Body(pelvis=(0, 0, -0.012 - dip), pelvisRot=(0, 6 * direction * math.sin(math.pi * t), 0),
		spine=(1, 0, 0), chest=(0, 4 * direction * math.sin(math.pi * t), 0), head=(-1.5, 10 * direction * math.sin(math.pi * min(1, t * 1.4)), 0))
	first, second = ("L", "R") if direction > 0 else ("R", "L")
	windows = {first: (0.05, 0.52), second: (0.42, 0.9)}
	final = QWorld(yaw=90.0 * direction)
	for side in ("L", "R"):
		t0, t1 = windows[side]
		b0 = V(BALL[side]) + V(0.015 if side == "L" else -0.015, 0, 0)
		b1 = final @ b0
		s = max(0.0, min(1.0, (t - t0) / (t1 - t0)))
		e = Smooth(s)
		fy = 90.0 * direction * e
		world = (QWorld(yaw=fy) @ b0)
		world.z += 0.07 * math.sin(math.pi * s)
		pitch = 18 * math.sin(math.pi * s)
		# 발은 월드에 붙어 있으므로 root 회전을 빼서 root 기준으로 넘김
		local = p.Q.inverted() @ world
		p.FootBall(side, local, pitch=pitch, footYaw=fy - yaw)
	IdleArms(p, t, 0.0)
	return p


# ── 공격(창 찌르기) ──

def PoseAttack(t, an):
	p = Pose(an)
	k1 = Smooth(t / 0.32)                       # 끌어당김
	k2 = Smooth((t - 0.32) / 0.12)              # 찌름
	k3 = Smooth((t - 0.56) / 0.44)              # 돌아옴
	wind = k1 * (1 - k2)
	thrust = k2 * (1 - k3)
	p.Body(pelvis=(0, 0.03 * wind - 0.07 * thrust, -0.01 - 0.02 * wind - 0.045 * thrust),
		pelvisRot=(0, -10 * wind + 14 * thrust, 0), spine=(2 - 4 * wind + 10 * thrust, -6 * wind + 8 * thrust, 0),
		chest=(0, -10 * wind + 12 * thrust, 0), neck=(0, 6 * wind - 8 * thrust, 0), head=(-2, 6 * wind - 10 * thrust, 0))
	# 왼발이 앞으로 디뎠다가 돌아옴
	step = Smooth((t - 0.22) / 0.2) * (1 - Smooth((t - 0.62) / 0.3))
	stepLift = math.sin(math.pi * max(0.0, min(1.0, (t - 0.22) / 0.2))) * 0.05 + math.sin(math.pi * max(0.0, min(1.0, (t - 0.62) / 0.3))) * 0.04
	bl = V(BALL["L"]) + V(0.03, -0.20 * step, stepLift)
	p.FootBall("L", bl)
	p.FootBall("R", V(BALL["R"]) + V(-0.02, 0.06 * step, 0), pitch=8 * thrust)
	wHold, wBack, wOut = WRIST_HOLD_R, V(-0.17, 0.13, 1.17), V(-0.09, -0.48, 1.21)
	if t < 0.32:
		w = wHold.lerp(wBack, k1)
		q = HOLD_R.slerp(THRUST_R, k1)
	elif t < 0.56:
		w = wBack.lerp(wOut, k2)
		q = THRUST_R
	else:
		w = wOut.lerp(wHold, k3)
		q = THRUST_R.slerp(HOLD_R, k3)
	p.Hand("R", w, q, pole=SHOULDER["R"] + V(-0.45, 0.3, -0.25))
	wl, ql = CarryArmL(-28 * thrust + 10 * wind)
	wl += V(0.04 * thrust, 0, 0.02 * thrust)
	p.Hand("L", wl, ql)
	return p


# ── 점프 ──

def HoldArms(side):
	return (WRIST_HOLD_R, HOLD_R) if side == "R" else CarryArmL(0.0)


def _JumpCrouch(p, depth, armBack, heel=0.0, armUp=0.0, fromHold=0.0):
	p.Body(pelvis=(0, 0.02 * depth / 0.16, -depth), pelvisRot=(depth * 40, 0, 0), spine=(depth * 50 - armUp * 8, 0, 0),
		chest=(depth * 20, 0, 0), head=(-depth * 50 + armUp * 4, 0, 0))
	for side in ("L", "R"):
		p.FootBall(side, V(BALL[side]) + V(0.015 if side == "L" else -0.015, 0, 0), pitch=heel)
		sx = 1 if side == "L" else -1
		phi = -40 * armBack + 120 * armUp
		w, q = SwingArm(side, phi, bend=0.03)
		if fromHold > 0:
			hw, hq = HoldArms(side)
			w = w.lerp(hw, fromHold)
			q = q.slerp(hq, fromHold)
		p.Hand(side, w, q)


def PoseJumpStart(t, an):
	p = Pose(an)
	if t < 0.6:
		k = Smooth(t / 0.6)
		_JumpCrouch(p, 0.16 * k, k, fromHold=1 - k)
	else:
		k = Smooth((t - 0.6) / 0.4)
		_JumpCrouch(p, 0.16 * (1 - k) - 0.03 * k, 1 - k, heel=40 * k, armUp=k * 0.8)
	return p


def _AirPose(p, t, extra=0.0):
	w = math.sin(2 * math.pi * t)
	p.Body(pelvis=(0, 0, 0.0), pelvisRot=(8, 0, 0), spine=(4 + 2 * w, 0, 0), chest=(0, 0, 0), head=(-6, 0, 0))
	for side, ph in (("L", 0.0), ("R", 0.35)):
		sx = 1 if side == "L" else -1
		ankle = V(sx * 0.1, 0.05 + 0.02 * math.sin(2 * math.pi * (t + ph)), 0.32 + extra + 0.02 * w)
		p.FootAnkle(side, ankle, pitch=28)
		wr, q = SwingArm(side, 70 + 8 * w, bend=0.05)
		wr += V(sx * 0.08, 0, 0)
		p.Hand(side, wr, q)


def PoseJumpAir(t, an):
	p = Pose(an)
	_AirPose(p, t)
	return p


def PoseJumpLand(t, an):
	p = Pose(an)
	if t < 0.25:
		k = Smooth(t / 0.25)
		d = 0.17 * k
	else:
		k = Smooth((t - 0.25) / 0.75)
		d = 0.17 * (1 - k)
	p.Body(pelvis=(0, 0.02, -d - 0.008), pelvisRot=(d * 60, 0, 0), spine=(d * 60, 0, 0), chest=(d * 20, 0, 0), head=(-d * 60 - 1.5, 0, 0))
	p.Stand(0.015)
	if t < 0.25:
		for side in ("L", "R"):
			w, q = SwingArm(side, 60 * (1 - k) + 25 * k, bend=0.05)
			p.Hand(side, w, q)
	else:
		for side in ("L", "R"):
			w, q = SwingArm(side, 25 * (1 - k) + 3 * k, bend=0.05 * (1 - k))
			hw, hq = HoldArms(side)
			w = w.lerp(hw, k)
			q = q.slerp(hq, k)
			p.Hand(side, w, q)
	return p


JUMP_HEIGHT = 0.45


def PoseJump(t, an):
	"""한 번에 끝나는 점프(루트 높이 포함): 0~0.25 웅크림, 0.25~0.8 공중, 0.8~1 착지."""
	takeoff, land = 0.25, 0.8
	if t < takeoff:
		return PoseJumpStart(t / takeoff, an)
	if t < land:
		s = (t - takeoff) / (land - takeoff)
		lift = JUMP_HEIGHT * 4 * s * (1 - s)
		p = Pose(an, lift=lift)
		_AirPose(p, s * 0.5, extra=-0.15 * (1 - math.sin(math.pi * s)))
		return p
	return PoseJumpLand((t - land) / (1 - land), an)


CLIPS = [
	# 이름, 프레임 수, 자세 함수, 루프, 마커(0~1)
	("Pose_Base", 1, lambda t, an: PoseIdle(0.0, an, 0.0), False, {}),
	("Idle", 60, PoseIdle, True, {}),
	("Walk", WALK["frames"], PoseWalk, True, {"L_contact": 0.0, "R_contact": 0.5}),
	("Run", RUN["frames"], PoseRun, True, {"L_contact": 0.0, "R_contact": 0.5}),
	("Turn_L90", 24, lambda t, an: PoseTurn(t, an, 1), False, {"L_step": 0.05, "R_step": 0.42}),
	("Turn_R90", 24, lambda t, an: PoseTurn(t, an, -1), False, {"R_step": 0.05, "L_step": 0.42}),
	("Attack", 24, PoseAttack, False, {"hit": 0.42, "recover": 0.56}),
	("Jump_Start", 10, PoseJumpStart, False, {"takeoff": 1.0}),
	("Jump_Air", 18, PoseJumpAir, True, {}),
	("Jump_Land", 12, PoseJumpLand, False, {"impact": 0.25}),
	("Jump", 33, PoseJump, False, {"takeoff": 0.25, "land": 0.8}),
]


def BakeAll(arm):
	an = Animator(arm, LIMBS)
	for (name, frames, fn, loop, markers) in CLIPS:
		an.Bake(name, frames, fn, loop=loop, markers=markers)
	an.Cleanup()
	return an
