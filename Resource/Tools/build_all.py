# -*- coding: utf-8 -*-
# 리소스 전체 빌드 실행기.
# 사용: blender -b --factory-startup --python Resource/Tools/build_all.py -- [humanoid] [quadruped] [buildings] [nature] [animgraph] [previews]
# 인자가 없으면 전부 만듦(previews 포함, 몇 분 걸림).
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import humanoid
import quadruped
import buildings
import nature
import animgraph
import previews

STEPS = {"humanoid": humanoid.Build, "quadruped": quadruped.Build, "buildings": buildings.Build, "nature": nature.Build,
	"animgraph": animgraph.Build, "previews": previews.Build}

args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
for name in (args or list(STEPS)):
	print("[리소스] ── %s 빌드 시작" % name, flush=True)
	STEPS[name]()
print("[리소스] 완료", flush=True)
