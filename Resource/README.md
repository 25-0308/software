# Resource — 그래픽 리소스(메시·애니메이션·재질)

SimpleGame(그리스·로마 신화 오픈월드 RPG)용 로우폴리 3D 리소스입니다. 모든 리소스는 Blender 5.2 파이썬 스크립트(`Tools/`)로 **절차적으로 생성**합니다. 그래서 수치를 고치고 다시 빌드하면 전부 같은 규칙으로 다시 만들어집니다. 게임 코드는 이 폴더를 건드리지 않고, 이 폴더도 게임 코드를 건드리지 않습니다.

미리보기 이미지는 [Previews/](Previews)에 있습니다.

## 폴더 구조

```
Resource/
├─ Characters/Humanoid/        인간형(7등신) — 부위 조립식
│   ├─ Humanoid.blend          원본(뼈대 + 모든 부위 + 애니메이션)
│   ├─ Humanoid_Parts.glb      부위 라이브러리(뼈대 1개 + 부위 메시 전부 + 애니메이션)
│   ├─ Humanoid_Parts.json     부위 목록(슬롯·설명·쓰는 색 역할·정점 수), 뼈 이름, 클립 정보
│   ├─ Humanoid_Presets.json   캐릭터 프리셋(부위 조합 + 색)
│   └─ Presets/<ID>.glb        프리셋을 조립해 둔 완성 캐릭터(애니메이션 포함)
├─ Creatures/Quadruped/        네 발 짐승 — 몸틀 3종 × 부위 조립식
│   ├─ Quadruped_<몸틀>.blend   Canid / Cervid / Heavy 원본
│   ├─ <몸틀>_Parts.glb         몸틀별 부위 라이브러리
│   ├─ Quadruped_Parts.json    몸틀·부위·클립 정보
│   ├─ Quadruped_Species.json  종(프리셋) 목록 — 위험/중립/순함
│   └─ Species/<ID>.glb        조립해 둔 완성 짐승
├─ Buildings/                  건축 키트 + 생활도구
│   ├─ Buildings.blend
│   ├─ Buildings_Kit.glb       집·신전 조각(노드 이름 = 조각 이름)
│   ├─ Goods_Kit.glb           생활도구·가구·시장 물건
│   ├─ Buildings.json          조각 목록(크기·기준점) + 조립 예시(프리팹) 배치표
│   └─ Prefabs/<이름>.glb       조립 예시(작은 집·긴 집·섬마을 집·도리스식 신전·시장 귀퉁이)
├─ Nature/                     자연환경
│   ├─ Nature.blend
│   ├─ Textures/T_<재질>.png   이어 붙는 512×512 색 텍스처 + T_<재질>_N.png 노멀맵
│   ├─ Ground_Tiles.glb        땅·물 타일(텍스처 재질)
│   ├─ Nature_Kit.glb          나무·덤불·풀·꽃·바위·통나무(단색 로우폴리)
│   └─ Nature.json
├─ Animations/
│   ├─ AnimGraph_Humanoid.json            인간형 애니메이션 조합 정보
│   └─ AnimGraph_Quadruped_<몸틀>.json     짐승 몸틀별 조합 정보
├─ Previews/                   검수용 렌더 이미지
└─ Tools/                      생성 스크립트(Blender 파이썬)
```

## 공통 규칙

| 항목 | 값 |
|---|---|
| 단위 | 1 블렌더 단위 = 1 m |
| 축(블렌더) | Z 위, 캐릭터·짐승의 앞 = -Y, 왼쪽 = +X |
| 축(.glb) | glTF 표준 Y 위, 앞 = +Z (내보낼 때 자동 변환). 게임은 Z 위이므로 불러올 때 (x, y, z)gl → (x, -z, y)게임 으로 되돌리면 블렌더와 같아짐 |
| 기준점 | 캐릭터·짐승은 발밑 원점, 건축·자연물은 바닥 가운데 |
| 프레임 속도 | 30 fps, 루프 클립은 마지막 키 = 첫 키(재생 구간 [0, frames)) |
| 재질 | 면마다 단색(Principled BSDF의 Base Color·Metallic·Roughness·Emission). 땅·물 타일만 텍스처 |
| 색 | [Tools/palette.py](Tools/palette.py) 한 곳 — 저물어 가는 지중해의 황혼(따뜻하지만 살짝 바랜 색, 금·청동은 또렷하게) |

## 1. 인간형(7등신, 부위 조립식)

키 1.76 m(머리 하나 0.25 m). 모든 부위가 **같은 뼈대 하나**(`Humanoid_Rig`, 31개 뼈)에 스킨되어 있습니다. 슬롯마다 부위를 하나씩 골라 붙이면 한 캐릭터가 됩니다(레고 방식).

| 슬롯 | 부위 |
|---|---|
| Head | `Head_M`(각진 턱), `Head_F`(갸름한 턱) — 목·귀 포함 |
| Face | `Face_Calm/Stern/Smile/Angry/Weary/Old/Sly/Fem/FemStern/FemSmile/Glow` |
| Hair | `Hair_Short/Curly/Wild/Bun/Long/LongFem` |
| Beard | `Beard_Full/Short/Long/Goatee` |
| Headwear | 투구 `Helm_Crested/Athena/Ares`, 관 `Crown_Gold/Spiked/Polos`, 화관 `Wreath_Laurel/Rose/IvyGrape/Pomegranate`, `Hat_PetasosWinged`, `Cap_Pilos`, `Hood`, `Veil`, `Horns_Goat`, `Mask_Party` (여러 개 겹쳐 써도 됨 — 헤라 = 관 + 베일) |
| Torso | `Torso_Bare/Tunic/Robe/Exomis/Himation/Dress/Cuirass/Aegis` + 몸형 `_M`/`_F` |
| Arms | `Arms_Bare/ShortSleeve/LongSleeve/Bracers` |
| Hands | `Hands_Bare` |
| Lower | `Lower_ShortSkirt/LongRobe/Pteruges/Loincloth/FurHips` + `_M`/`_F` |
| Legs | `Legs_Bare/Greaves/Goat` |
| Feet | `Feet_Bare/Sandals/WingedSandals/Boots/Hooves` |
| Back | `Back_CapeShort/CapeLong` |
| Accessory | `Acc_Apron/Satchel/Shackles/Wraps` |
| PropR / PropL | 오른손 `Prop_<이름>.R`, 왼손 `Prop_<이름>.L` — 창·삼지창·두 갈래 창·독수리 홀·연꽃 홀·번개·전령 지팡이·망치·지팡이·튀르소스·술잔·금빛 사과·거울·리라·석류·방패·칼·횃불·물렛가락·눈금 막대·가위·낫(하르페)·갈대 피리·두루마리·월계수 가지·불씨 |

**소품 소켓**: `prop.R`·`prop.L` 뼈가 주먹 안 쥐는 점입니다. 소품을 바꿀 때는 그 뼈에 붙이면 됩니다(라이브러리의 소품은 이미 그 뼈에 100% 스킨됨).

**색 역할(Role)**: 부위 메시의 재질 이름은 `Skin`, `Hair`, `Eye`, `Lip`, `Line`(속눈썹·주름), `ClothA`(주 옷감), `ClothB`(보조 옷감·망토·모자), `Trim`(띠·장식), `Metal`, `Leather`, `Accent`/`Accent2`(잎·꽃·보석), `Fur`, `Crest`(투구 깃털), `Glow`(빛나는 눈), `Horn`, `Feather` 같은 **역할 이름**입니다. 프리셋이 역할마다 팔레트 색을 정하므로 같은 부위도 캐릭터마다 다른 색이 됩니다. 프리셋 .glb에서는 재질 이름이 `<프리셋ID>.<역할>`입니다. `EyeWhite`, `Gold`, `Bronze`처럼 팔레트 이름이 그대로인 재질은 늘 같은 색입니다.

**프리셋** 56종([Humanoid_Presets.json](Characters/Humanoid/Humanoid_Presets.json)): `PLAYER`, 올림포스의 신들(제우스·포세이돈·하데스·아프로디테·헤르메스·헤라·아테나·아레스·디오니소스·헤파이스토스·판·아폴론·페르세포네), 에리스(노파/정체), 화로의 신·에스카로스, 프로메테우스, 모이라이·클로토·라케시스·아트로포스, 경계석의 신, 크로노스의 잔향(2.1배), 피티아(+신들린 모습), 이오(사람), 탄탈로스, 마을 사람·사제·철학자·연회 손님·조난자·영혼·돌아온 자들, 청동 전사. ID는 `Data/Story.txt`의 화자 ID와 같습니다.

신마다 한눈에 알아볼 특징을 넣었습니다. 제우스는 흰 곱슬머리·수염·금관·보랏빛 히마티온·번개와 독수리 홀, 포세이돈은 바닷빛 머리·삼지창, 하데스는 반쯤 감긴 눈·쇠관·검은 예복·서류, 아프로디테는 금발·장미 화관·거울, 헤르메스는 날개 모자·날개 샌들·편지 가방, 헤라는 높은 관·베일, 아테나는 회색 눈·밀어 올린 투구·아이기스·창과 방패, 아레스는 볼가리개 투구·흉갑·붉은 망토, 판은 염소 뿔과 다리입니다.

## 2. 네 발 짐승(몸틀 3종, 부위 조립식)

| 몸틀 | 걸음 | 부위 | 종(프리셋) |
|---|---|---|---|
| **Canid**(포식자, 발가락으로 디딤) | 걷기 4박자 / 달리기 회전 갤럽(척추가 접혔다 펴짐) / 공격: 웅크렸다 달려들며 물기(턱 뼈) | 몸통 `Lean/Muscular`, 머리 `Wolf/Lion`, 귀 `Pointed/Round`, 꼬리 `Bushy/Tuft`, `Mane`, 다리 | `WOLF`·`SHADOW_WOLF`(보랏빛 눈)·`LION` — **위험** |
| **Cervid**(가는 다리 + 발굽) | 걷기 4박자 / 달리기 바운드(뒷다리 둘이 함께 참) / 공격: 뒤로 젖혔다 들이받기 | 몸통 `Sleek/Wool`, 머리 `Deer/Sheep`, 귀 `Long/Droopy`, 뿔 `Antlers/Ram`, 꼬리 `Short`, 다리 | `DEER`·`SHEEP` — **순함**, `RAM` — **중립** |
| **Heavy**(짧고 굵은 다리) | 걷기 4박자 / 달리기 무거운 갤럽 / 공격: 앞발로 땅을 긁고 머리를 낮춰 들이받은 뒤 쳐올림 | 몸통 `Boar/Bovine`, 머리 `Boar/Bull`, 귀 `Small/Side`, 뿔 `Bull/Short`, 꼬리 `Thin/Tuft`, 다리 | `BOAR`·`BULL` — **중립**, `CHR_IO`(하얀 암소) — **순함** |

세 몸틀의 뼈 이름은 같습니다(`root, pelvis, spine, chest, neck, head, jaw, ear.L/R, tail1~3, scapula, upperarm, forearm, frontfoot, fronttoe, thigh, shin, hindfoot, hindtoe`). 같은 몸틀 안에서는 머리·몸통·귀·뿔·꼬리를 자유롭게 바꿔 끼울 수 있습니다(예: 사슴에서 `Horns_Antlers`를 빼면 암사슴, 양에 `Horns_Ram`을 끼우면 숫양). 크기는 종마다 뼈대 배율(`scale`)로 정합니다(멧돼지 = Heavy × 0.62). 색 역할은 `Fur`, `FurDark`(등·얼굴), `FurLight`(배·주둥이), `Mane`, `Eye`, `Nose`, `Horn`, `Hoof`입니다. `Quadruped_Species.json`의 `gameKind`는 게임 코드의 `AnimalKind`(Wolf/ShadowWolf/Deer/WhiteCow)와 이어집니다.

## 3. 애니메이션

인간형과 짐승 몸틀 모두 같은 클립 이름을 씁니다. 모든 .glb에 들어 있습니다(glTF animation 이름 = 클립 이름).

| 클립 | 루프 | 설명 |
|---|---|---|
| `Pose_Base` | – | 기준 자세 1프레임 — 회전·전환 보간용 |
| `Idle` | ○ | 숨쉬기·고개 돌리기(짐승은 꼬리·귀) |
| `Walk` | ○ | 걷기(제자리) — `speed`(m/s)·발 디딤 마커 |
| `Run` | ○ | 달리기(제자리) — Walk와 같은 정규화 시간에 같은 발이 디딤 |
| `Turn_L90` / `Turn_R90` | – | 제자리 90° 회전(발을 차례로 디딤) — root yaw 루트 모션 |
| `Attack` | – | 공격(인간형: 창 찌르기, 짐승: 몸틀별) — `hit`·`recover` 마커 |
| `Jump_Start` → `Jump_Air` → `Jump_Land` | –/○/– | 단계별 점프(물리로 높이를 처리할 때) |
| `Jump` | – | 한 번짜리 점프(root 높이 루트 모션) |

**조합 정보**는 [Animations/](Animations)의 JSON에 있습니다. 걷기↔달리기는 1차원 블렌드(속도)에 발 디딤 동기화를 씁니다. 회전은 서 있을 때 45° 이상이면 회전 클립을 쓰고, 그 밖에는 모델만 돌립니다. 걷거나 달리면서 공격하면 상체 마스크 레이어에 공격 클립을 씁니다. 점프 단계 전환, 블렌드 시간, 마커 시각, 재생 속도 맞추는 식도 들어 있습니다. 수치는 클립을 만든 코드에서 바로 뽑았습니다.

## 4. 건축·생활도구

벽 한 칸은 폭 2 m, 높이 2.4 m, 두께 0.25 m이고 2 m 격자에 맞춥니다. 벽 앞면은 -Y, 기준점은 바닥 가운데입니다. 모서리엔 `Wall_Corner`를 세웁니다. 지붕 조각은 벽 높이 위에 미리 올려 두었으므로 집 가운데 줄에 그대로 두면 됩니다.

- **집**: `Wall_Plain/Door/Window/WindowLit/Half`, `Wall_Corner`, `Floor_Stone`, `Roof_Gable`(깊이 4 m 집용), `Roof_GableEnd`, `Roof_Flat`, `Stairs_Outside`, `Awning`, `Pergola`, `Fence_Wood`, `Wall_DryStone`, `Gate_Wood`
- **신전**: `Temple_Step`, `Column_Doric/Ionic/Corinthian/Broken`, `Entablature`, `Pediment`, `Temple_Roof`, `Temple_Wall`, `Temple_Door`, `Roman_Stairs`, `Inscription_Gold`(IVNO), `Altar`, `Tripod`, `Statue`, `Statue_Fallen`, `Stele`, `Herm`, `Brazier`
- **생활도구**: 항아리 `Amphora/Pithos/Hydria/Krater/Kylix`, `Oil_Lamp`, `Lamp_Stand`, `Loom`, `Table`, `Table_Marble`, `Chair_Klismos`, `Stool`, `Kline`, `Chest`, `Crate`, `Basket`, `Sack`, `Well`, `Quern`, `Anvil`, `Forge`, `Campfire`, `Torch_Wall`, `Bench`, `Ladder`, `Shelf_Pottery`, `Cart`, `Boat_Small`, `Net_Rack`, `Bread_Oven`, `Scroll_Rack`, `Signpost`, `Stall`, `Tent`, `Banner`
- **조립 예시**: `Buildings.json`의 `prefabs`에 조각 이름·위치·회전 목록이 있습니다. 코드에서 같은 방식으로 조립할 수 있습니다.

## 5. 자연환경

- **재질(텍스처)**: `Grass`, `GrassDry`, `Dirt`, `StonePaving`, `Sand`, `Rock`, `Snow`, `Water`, `Marble`, `Plaster`. 512×512 색 + 노멀맵이고 네 변이 이어집니다(주기 노이즈로 만듦).
- **땅 타일**: `Tile_*` 2×2 m(UV 0~1 = 텍스처 한 장), `Water_Tile`(반투명, 5 cm 낮음), `Water_Large` 20×20 m
- **나무**: `Tree_Olive`(A·B·C), `Tree_Cypress`, `Tree_Oak`, `Tree_Pine`(우산소나무), `Tree_Laurel`, `Tree_Dead`
- **풀·꽃**: `Bush_Round`, `Bush_Oleander`, `Grass_Tuft/TuftDry/Patch`, `Reeds`, `Flowers_Poppy/Anemone/Daisy/Yellow`
- **바위·나무토막**: `Rock_Small/Medium/Large/Snowy/Mossy/Ash`, `Cliff`, `Pebbles`, `Stump`, `Log`, `Mushrooms`

## 다시 만들기

```bash
"C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Resource/Tools/build_all.py
```

일부만 만들려면 `--` 뒤에 단계 이름을 적습니다(`humanoid`, `quadruped`, `buildings`, `nature`, `animgraph`, `previews`).

```bash
"C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Resource/Tools/build_all.py -- humanoid animgraph
```

| 바꾸고 싶은 것 | 고칠 곳 |
|---|---|
| 색 | [Tools/palette.py](Tools/palette.py) |
| 캐릭터 조합·색 | [Tools/humanoid_presets.py](Tools/humanoid_presets.py) |
| 인간형 부위 모양 | [Tools/humanoid_parts.py](Tools/humanoid_parts.py) |
| 인간형 동작 | [Tools/humanoid_anims.py](Tools/humanoid_anims.py) |
| 짐승 부위·몸틀 | [Tools/quadruped_parts.py](Tools/quadruped_parts.py) |
| 짐승 동작 | [Tools/quadruped_anims.py](Tools/quadruped_anims.py) |
| 짐승 종 | [Tools/quadruped.py](Tools/quadruped.py) |
| 건축·생활도구 | [Tools/buildings.py](Tools/buildings.py) |
| 자연·텍스처 | [Tools/nature.py](Tools/nature.py) |

새 애니메이션을 더하려면 `*_anims.py`에 자세 함수를 만들고 `CLIPS`/`Clips()` 목록에 한 줄 넣습니다. 자세는 발·손 위치(IK 표적)와 몸통 회전으로 잡고, 빌드할 때 FK 키로 구워집니다.

## 아직 없는 것

- 그림자 퓌톤(뱀) 같은 네 발이 아닌 괴물 — 요청 범위(네 발 짐승) 밖이라 만들지 않음
- 바람에 흔들리는 나무 애니메이션(게임 셰이더에서 처리하는 편이 가벼움)
- 짐승의 풀 뜯기·눕기, 인간형의 맨손 주먹질·피격·쓰러짐 같은 추가 동작(요청 시 같은 방식으로 추가 가능)
- 게임 코드에 불러오는 기능(glTF 로더) — 다른 작업에서 진행
