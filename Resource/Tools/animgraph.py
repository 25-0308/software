# -*- coding: utf-8 -*-
# 애니메이션 조합 정보 — 클립을 어떻게 섞고 넘겨야 하는지(걷기↔달리기, 회전, 이동 중 공격, 점프 단계)를
# 코드 쪽에서 그대로 읽어 쓸 수 있게 JSON으로 적음. 수치는 클립을 만든 코드에서 직접 가져옴.
from rescommon import WriteJson, ResPath
import humanoid
import quadruped
import quadruped_anims as QA
import quadruped_parts as QP

UPPER_BODY_HUMANOID = ["spine", "chest", "neck", "head", "clavicle.L", "clavicle.R", "upperarm.L", "upperarm.R", "forearm.L",
	"forearm.R", "hand.L", "hand.R", "prop.L", "prop.R"]
UPPER_BODY_QUADRUPED = ["neck", "head", "jaw", "ear.L", "ear.R"]


def Graph(rig, clips, contact, upperBody, notes):
	byName = {c["name"]: c for c in clips}
	walk, run = byName["Walk"], byName["Run"]
	return {
		"rig": rig,
		"fps": clips[0]["fps"],
		"clips": byName,
		"parameters": {
			"speed": "지면 위 이동 속도(m/s, 블렌더 단위 기준 — 게임 단위로 바꿀 때는 아래 speedScale 참고)",
			"turnDelta": "목표 방향 - 현재 방향(도, +면 왼쪽)",
			"grounded": "발이 땅에 닿아 있는지",
			"attack": "공격 트리거(한 번)",
			"jump": "점프 트리거(한 번)",
		},
		"states": {
			"Locomotion": {
				"type": "blend1D",
				"parameter": "speed",
				"points": [{"clip": "Idle", "value": 0.0}, {"clip": "Walk", "value": walk["speed"]}, {"clip": "Run", "value": run["speed"]}],
				"syncGroup": "feet",
				"syncMarker": contact,
				"howTo": "Walk·Run은 둘 다 정규화 시간 0에서 같은 발(%s)이 땅을 디딤 — 두 클립의 재생 위치를 같은 정규화 시간으로 맞춘 채 "
					"속도에 따라 가중치만 바꾸면 발이 미끄러지거나 엇갈리지 않음. 재생 속도 = 실제 속도 / 두 점 사이를 보간한 클립 속도"
					"(0.8~1.3배로 제한, 그 밖은 클립 사이 가중치로 흡수)." % contact,
			},
			"Turn": {
				"type": "single",
				"clips": {"left": "Turn_L90", "right": "Turn_R90"},
				"howTo": "서 있을 때(speed < 0.1) |turnDelta| ≥ 45°면 그쪽 회전 클립을 재생 — 재생 속도 = |turnDelta| / 90(0.6~1.4로 제한). "
					"클립의 root yaw가 루트 모션이므로, 끝나면 캐릭터 방향에 실제 회전량을 더하고 root 회전은 버림. "
					"이동 중이거나 45° 미만이면 클립 없이 모델 방향만 초당 최대 540°로 부드럽게 돌림(이때도 하체는 Locomotion 그대로).",
			},
			"Attack": {
				"type": "layered",
				"clip": "Attack",
				"standing": "서 있으면 몸 전체에 재생(0.1초 교차), 끝나면 Locomotion으로 0.2초 교차",
				"moving": "움직이는 중이면 상체 뼈(upperBodyMask)에만 Attack을 덮고 하체는 Locomotion을 계속 재생 — 걷다가/달리다가 공격",
				"hitMarker": "markers.hit 시각에 피해 판정",
			},
			"Jump": {
				"type": "sequence",
				"sequence": ["Jump_Start", "Jump_Air", "Jump_Land"],
				"howTo": "Jump_Start가 끝나면(markers.takeoff) 실제 도약(물리)과 함께 Jump_Air를 반복, grounded가 참이 되는 순간 Jump_Land(0.05초 교차), "
					"Jump_Land의 impact 이후 이동 입력이 있으면 0.15초 교차로 Locomotion. 높이를 물리로 처리하지 않는 경우엔 "
					"root 높이가 들어 있는 한 번짜리 Jump 클립을 대신 씀(rootMotion.heightPeak).",
			},
			"Pose_Base": {
				"type": "pose",
				"clip": "Pose_Base",
				"howTo": "회전·순간이동·컷신 전환처럼 클립 사이를 이을 게 없을 때 0.1~0.15초 동안 섞어 넣는 기준 자세(Idle의 첫 프레임과 같음). "
					"회전 보간 중에는 현재 자세와 이 자세를 섞어서 팔다리가 튀지 않게 함.",
			},
		},
		"transitions": [
			{"from": "Locomotion(Idle)", "to": "Locomotion(Walk)", "when": "speed > 0.1", "blend": 0.2},
			{"from": "Locomotion(Walk)", "to": "Locomotion(Run)", "when": "speed가 Walk·Run 사이", "blend": "가중치 보간(동기화)", "sync": contact},
			{"from": "Locomotion(Run)", "to": "Locomotion(Walk)", "when": "speed 감소", "blend": "가중치 보간(동기화)", "sync": contact},
			{"from": "Locomotion(Walk/Run)", "to": "Locomotion(Idle)", "when": "speed < 0.1", "blend": 0.25},
			{"from": "Locomotion(Idle)", "to": "Turn", "when": "|turnDelta| ≥ 45", "blend": 0.12},
			{"from": "Turn", "to": "Locomotion", "when": "클립 끝 또는 이동 시작", "blend": 0.15},
			{"from": "Any(지상)", "to": "Attack", "when": "attack", "blend": 0.08},
			{"from": "Attack", "to": "Locomotion", "when": "markers.recover 이후 이동 입력 또는 클립 끝", "blend": 0.2},
			{"from": "Any(지상)", "to": "Jump_Start", "when": "jump", "blend": 0.08},
			{"from": "Jump_Start", "to": "Jump_Air", "when": "클립 끝(takeoff)", "blend": 0.05},
			{"from": "Jump_Air", "to": "Jump_Land", "when": "grounded", "blend": 0.05},
			{"from": "Jump_Land", "to": "Locomotion", "when": "markers.impact 이후 이동 입력 또는 클립 끝", "blend": 0.15},
		],
		"upperBodyMask": upperBody,
		"speedScale": "클립 속도(speed)는 이 리그 1배 크기 기준 m/s. 프리셋·종의 scale만큼 곱하고, 게임 단위가 m가 아니면 "
			"(게임 이동 속도 × 1단위의 m) / (클립 speed × scale)로 재생 속도를 정함.",
		"notes": notes,
	}


def Build():
	h = Graph("Humanoid_Rig", humanoid.ClipInfo(), "L_contact", UPPER_BODY_HUMANOID,
		["오른손(prop.R)은 소품을 세워 든 자세가 기본 — 빈손 캐릭터도 같은 클립을 씀",
			"모든 클립은 제자리(in-place). 루트 모션은 Turn_*의 yaw와 Jump의 높이뿐"])
	WriteJson(ResPath("Animations", "AnimGraph_Humanoid.json"), h)
	for frame in QP.FRAMES:
		clips = quadruped.ClipInfo(frame, QA.Clips(frame))
		q = Graph("%s_Rig" % frame, clips, "HL_contact", UPPER_BODY_QUADRUPED,
			["걷기는 4박자(뒷왼→앞왼→뒷오른→앞오른), 달리기는 %s" % {"Canid": "회전 갤럽", "Cervid": "바운드", "Heavy": "무거운 갤럽"}[frame],
				"Walk·Run 모두 뒷왼발(HL)이 정규화 시간 0에 디딤 — 이걸로 동기화",
				"공격은 %s(markers.hit) — 상체 마스크는 목·머리·턱이라 걸으며 물기/들이받기 가능" % {"bite": "달려들며 물기", "butt": "머리로 들이받기", "gore": "머리를 낮춰 들이받고 쳐올리기"}[QA.GAITS[frame]["attack"]],
				"이 몸틀을 쓰는 종: " + ", ".join(s["id"] for s in quadruped.SPECIES if s["frame"] == frame)])
		WriteJson(ResPath("Animations", "AnimGraph_Quadruped_%s.json" % frame), q)
