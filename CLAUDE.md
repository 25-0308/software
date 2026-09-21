# SimpleGame — 프로젝트 개발 지침

이 문서는 프로젝트의 기본 방향/설정을 담은 지침입니다. 새 세션에서도 이 문서를 기준으로 작업을 이어갑니다. 여기 적힌 기본 틀(장르, 세계관, 핵심 시스템, 개발 순서)은 유지하며, 세부 내용(퀘스트 목록, 신 목록, 능력 상세 등)은 계속 추가/확장됩니다.

## 기술 스택

- 언어: C++
- 그래픽: OpenGL 3.3 core + GLEW + freeglut
- 빌드: Visual Studio 2022, x64 (`SimpleGame.sln`)
- 저장소: GitHub [25-0308/software](https://github.com/25-0308/software)

## 게임 방향

- **장르**: 오픈월드 RPG
- **렌더링**: 실시간 렌더링 기반 (정적 데모 아님)
- **시점/표현**: 쿼터뷰(비스듬한 고정 각도) 카메라, 2.5D로 표현 (진짜 3D 카메라/깊이버퍼가 아니라 월드 좌표를 화면에 투영 + 그리기 순서로 깊이를 흉내)

## 세계관 / 스토리

- **배경**: 그리스·로마 신화
- **톤**: 다크하지만 약간의 유머가 섞임 — 순수 비극이 아니라, 표면은 장엄하지만 신들 내부는 정치질·질투·권력다툼으로 얼룩진 "막장 가족극"에 가까운 분위기
- **중심 이야기**: 제우스와 그 주변 인물들
- **시간적 배경 방향성**: 신들의 권위가 예전 같지 않은, 신들의 시대가 저물어가는 황혼기

### 메인 퀘스트 트리 (초안, 2026-09-07 기준)

```
프롤로그 — 신탁의 균열
    ↓
메인퀘 1. 수호신의 선택 (플레이어가 수호신을 고르고 첫 축복을 받음)
    ├─ 제우스 / 포세이돈 / 하데스 / 아프로디테 / 기타 루트로 분기
    ↓ (중반부터 공용 줄기로 합류)
메인퀘 2. 흔들리는 왕좌 (제우스 권위를 위협하는 사건 표면화)
    ↓
메인퀘 3. 배신자의 이름 (음모 세력이 드러남)
    ↓
메인퀘 4. 올림포스 내전 (제우스 편 vs 반란 세력 편, 선택한 수호신에 따라 압박 발생)
    ↓
메인퀘 5 (최종). 신들의 황혼 (수호신 + 4단계 선택 조합에 따라 다중 엔딩)
```

이 구조는 확정이 아니라 "기본 틀"이며, 세부 신 목록/분기 수/엔딩 개수는 계속 다듬어질 수 있습니다.

## 핵심 게임플레이 시스템

- 플레이어는 게임 초반 **수호신(patron god)**을 선택
- 선택한 신이 자신의 권능 영역에 맞는 **축복(blessing)**을 내리고, 이것이 플레이어의 능력/스킬 트리의 뿌리가 됨
  - 예: 제우스 축복 → 번개/권위 계열, 포세이돈 축복 → 물/바다 계열, 하데스 축복 → 죽음/영혼 계열
- 능력/스킬/아이템 설계는 범용 클래스 시스템이 아니라 **"신의 영역 → 축복 → 능력"** 구조를 기준으로 함
- (미정, 추후 결정 필요) 축복을 하나만 고정할지, 진행하며 다른 신의 축복도 추가로 얻을 수 있는지 / 선택한 신에 따라 다른 신들과의 평판이 달라지는지 / 축복이 전투 외 탐험에도 영향을 주는지

## 개발 진행 순서 (권장, 단계적으로 진행)

한 번에 전체를 설계하지 않고 아래 순서로 점진적으로 확장합니다.

1. ✅ 게임 루프 (델타 타임, 업데이트/렌더 분리)
2. ✅ 카메라 + 쿼터뷰 월드→스크린 투영
3. ✅ 다중 오브젝트 렌더링 + 그리기 순서(깊이) 정렬
4. 🟡 타일맵/청크 기반 월드 — 타일맵(랜덤 생성+연결성 보장)은 구현됨, 청크 스트리밍은 아직
5. 🟡 엔티티(플레이어, NPC, 아이템) 구조 — Player/NPC/Item/Animal/Building 타입 구현, 정식 컴포넌트 시스템은 아님
6. 🟡 월드 스트리밍, 세이브 등 오픈월드 요소 — 경험치/레벨업(간단한 스탯 저장)까지는 구현, 스트리밍/세이브 파일은 아직

### 현재 구현 상태 (2026-09-14 기준, `dev` 브랜치)

- **게임 루프**: `std::chrono` 기반 델타 타임, `Update()`/`RenderScene()` 분리 ([SimpleGame.cpp](SimpleGame/SimpleGame.cpp))
- **카메라**: yaw 45°/pitch 55° 고정 쿼터뷰 + 직교 투영, MVP 행렬 기반 렌더링. `Camera::SetFocus()`로 플레이어를 따라다님. `Camera::SetFlip(horizontal, vertical)`로 화면을 좌우/상하 반전할 수 있고(투영 뒤 NDC x·y 부호만 뒤집으므로 카메라 방향·보이는 면·깊이 순서·컬링은 그대로, 그림만 뒤집힘) 현재는 둘 다 켜져 있음(그림이 180도 회전, `SimpleGame.cpp`의 `main()`에서 설정). HUD는 별도 화면 좌표계라 뒤집히지 않고, 이름표는 뒤집힌 화면 위치를 따라가며, WASD는 월드 축 기준이라 반전 후엔 화면상 방향이 반대로 보이므로 좌우(A/D)만 입력을 뒤집어 화면 방향에 맞춤(`Update()`의 `kInvertHorizontalInput`, W/S는 그대로) ([Camera.h](SimpleGame/Camera.h)/[.cpp](SimpleGame/Camera.cpp), [Math3D.h](SimpleGame/Math3D.h)). 이 월드는 Z축이 높이이므로 yaw는 `RotateZ`로 회전시킴 — 이전엔 `RotateY`를 써서 높이가 화면 가로 위치에 섞여 들어가고 Y축 이동이 대각선으로 투영되지 않는 비대칭 왜곡이 있었음
- **조작**: WASD 이동(델타타임 기반, 대각선 정규화, 월드 경계 클램프), E 상호작용(반경 내 가장 가까운 대상), C 뷰 컬링 켜기/끄기(드로우 콜 비교용), 마우스 휠 줌(`0.25~4.0` 배율)
- **월드**: 32×32 타일맵(면적 기준 4배로 확장, 기존 16×16) — 마을(돌바닥) / 길(다진 흙길) / 호수(물, 전용 셰이더로 일렁임) / 숲(잔디+나무) ([TileMap.h](SimpleGame/TileMap.h)/[.cpp](SimpleGame/TileMap.cpp)). 플레이어 이동 클램프(`kWorldHalfExtent`)와 카메라 최소 줌(0.25배로 32칸 전체를 화면에 담을 수 있음)도 맵 크기에 맞춰져 있음
- **Actor / 씬 그래프**: 화면에 배치되는 모든 오브젝트(타일·나무·건물·아이템·횃불·NPC·짐승·플레이어·바닥 링)는 [Actor.h](SimpleGame/Actor.h)의 `Actor` 클래스로 표현하고, 배치된 액터는 전부 [SceneGraph.h](SimpleGame/SceneGraph.h)의 `SceneGraph`가 트리로 소유·제어함(예전 `GameObject`/`g_Entities`/`g_Animals` 전역 벡터는 없어짐). `Actor`는 부모/자식 계층(`AddChild`), 로컬 위치와 월드 위치(`GetWorldX/Y/Z`, 부모 이동만 상속·회전/스케일은 상속 안 함), 수명(`Destroy()`는 예약만 하고 씬이 프레임 끝에 정리), 가상 훅 `OnUpdate`/`OnRender`/`OnRenderShadow`/`GetCollisionRadius`/`CastsShadow`를 가짐. `SceneGraph`는 갱신(`Update`), 그리기(`Render`: Ground → Decal → 그림자 → Object를 `(y+z)` 깊이 정렬 후 페인터 알고리즘), 검색(`FindNearest`), 충돌 질의(`IsBlocked`), 순회(`ForEach`)를 담당하며, 게임 코드는 액터를 직접 들고 다니지 않고 씬에 물어봄. **씬 그래프의 용도는 뷰 컬링 등 공간 최적화**임: 모든 노드가 자신과 자손을 감싸는 경계 구(`BoundingSphere`, [Bounds.h](SimpleGame/Bounds.h))를 캐시(더티 플래그로 지연 재계산, 서브트리 경계는 로컬 좌표라 조상이 움직여도 안 낡음)하는 경계 계층을 이루고, `SceneGraph::Render()`는 카메라 뷰 볼륨(`ViewVolume`, 직교 투영이라 구 중심을 NDC로 옮겨 [-1,1]과 겹치는지만 봄, 판정은 항상 보수적)으로 화면 밖 노드를 서브트리째 건너뜀(컬링 후 정렬하므로 정렬 비용도 줄고, 결과는 `GetLastRenderStats()`). 검색(`FindNearest`)과 충돌 질의(`IsBlocked`)도 같은 경계 계층으로 닿을 수 없는 서브트리를 가지치기함. 그래서 공간적으로 가까운 액터를 그룹 노드 밑에 묶을수록 효과가 커서, 타일은 8×8칸씩 청크 그룹 노드 밑에 묶어 생성함(`SpawnTileActors`). 규칙: 각 액터는 보이는 모든 파츠(그림자·애니메이션으로 튀어나오는 팔다리 포함)를 감싸는 경계를 `SetBoundingSphere`로 넉넉히 지정해야 하고(경계 없는 그려지는 자손이 있으면 그 서브트리는 안전하게 컬링에서 제외됨), 충돌 반경 원과 액터 원점은 자기 경계 구 안에 들어와야 함. `C` 키로 컬링을 켜고 끌 수 있어 드로우 콜 출력으로 효과를 바로 비교 가능. 계층 활용 예: 횃불(`FireActor`)은 건물의 자식, 플레이어 발밑 마커(`RingActor`)는 플레이어의 자식이라 부모를 자동으로 따라다님. 구체 액터: [WorldActors.h](SimpleGame/WorldActors.h)(`TileActor`/`TreeActor`/`BuildingActor`/`ItemActor`/`FireActor`/`RingActor`), [CharacterActors.h](SimpleGame/CharacterActors.h)(`CharacterActor` 기반의 `NpcActor`/`PlayerActor`/`AnimalActor`). 레벨 구성은 [LevelBuilder.h](SimpleGame/LevelBuilder.h)(`SpawnTileActors`/`SpawnLevelActors`)가 씬에 액터를 생성함
- **캐릭터 렌더링**: Player/NPC/Animal은 몸통+머리+팔+다리 파츠로 조립해서 그림(`CharacterActor::OnRender`, [CharacterActors.cpp](SimpleGame/CharacterActors.cpp)). `Actor::GetFacing()`(이동/공격 방향)을 기준으로 팔다리가 회전 배치되어 실제로 바라보는 방향을 알 수 있음. 이동 중엔 팔다리가 반대 위상으로 흔들리는 걷기 애니메이션 + 상하 바운스, 플레이어 공격(Space) 시 오른팔이 `facing` 방향으로 휘둘러지는 모션 재생 — 실제 스프라이트 이미지는 아직 없음(텍스처 에셋 없음). 사람(Player/NPC)은 옷 색(`obj.r/g/b`)과 무관하게 머리·팔은 고정 피부색, 다리는 고정 중립 바지색으로 통일해 파츠 구분이 단순하고 직관적으로 보이게 함 — 이전엔 옷 색의 명암 변형(예: `obj.r*0.6`)만 써서 파츠가 잘 구분되지 않았음. 짐승(Animal)은 지금도 몸통 색에서 파생된 밝은 머리/어두운 다리 톤(털 색 느낌) 유지. 예전엔 머리에 facing 방향을 가리키는 작은 "코" 돌기가 있었으나 시각적 잡음이라 판단해 제거함(이름표로 식별 가능해졌으므로)
- **건물**: 한 변이 `size`인 큐브(`BuildingActor::OnRender`, `Shapes::DrawBox`) — 카메라가 월드 (+x,+y,+z) 쪽에서 내려다보므로 실제로 보이는 위(지붕색)/화면 왼쪽 면(+y, 벽색×0.85)/화면 오른쪽 면(+x, 벽색×0.62) 세 면만 회전시킨 사각형으로 그리고, 문(왼쪽 면)과 불 켜진 창문(오른쪽 면, 블룸으로 은은히 빛남)을 사각 장식으로 붙임. 충돌은 원이 아니라 눈에 보이는 큐브 발자국(정사각형)과 이동체 원의 겹침으로 판정(`BuildingActor::BlocksCircle`). 횃불은 카메라 쪽 문 옆에 자식 액터로 붙임(반대편에 두면 큐브에 가려짐). 텍스처 매핑은 아직 없음
- **그림자**: 캐릭터/건물 발밑에 반투명 원형 그라데이션 블롭 섀도우 (`Renderer::DrawShadow`, `Shaders/Shadow.vs/.fs`) — 실시간 쉐도우맵은 아님
- **물 이펙트**: 시간 기반 잔물결+반짝임 셰이더 (`Renderer::DrawWater`, `Shaders/Water.vs/.fs`)
- **포스트프로세싱**: HDR(RGBA16F) 오프스크린 렌더 → 밝기 추출 → 가우시안 블러(블룸) → 톤매핑+비네트 합성 → 색보정(그림자/하이라이트 틴트)+필름 그레인, 4패스 파이프라인 ([PostProcess.h](SimpleGame/PostProcess.h)/[.cpp](SimpleGame/PostProcess.cpp), `Shaders/BrightExtract.fs`, `Shaders/Blur.fs`, `Shaders/Composite.fs`, `Shaders/Grade.fs`)
- **공용 셰이더 유틸**: `Renderer`/`PostProcess`가 공유하는 셰이더 컴파일 로직을 [ShaderUtil.h](SimpleGame/ShaderUtil.h)/[.cpp](SimpleGame/ShaderUtil.cpp)로 분리
- **튜토리얼 레벨**: 마을(장로+마을사람 7명)+숲(나무 36그루, 야생동물 7마리: 사슴 4+늑대 3)+호수+아이템 9개(퀘스트 아이템 1+약초 8). 장로에게 말 걸기 → 호수 근처 퀘스트 아이템 습득 → 장로에게 전달 → 완료 시 플레이어가 빛나는 보상 효과. 대사와 각종 로그는 화면 왼쪽 아래 채팅창에 출력(아래 "채팅창" 항목 참고)
- **랜덤 레벨 생성**: 마을/호수 위치·크기를 매 실행 무작위로 배치하고(호수는 마을에서 6~10칸 거리), 호수 때문에 갈 수 없는 영역이 생기면 BFS로 검증해 길(Path)을 뚫어 전체 맵이 항상 하나로 연결되도록 보장 ([LevelGenerator.h](SimpleGame/LevelGenerator.h)/[.cpp](SimpleGame/LevelGenerator.cpp))
- **이동 충돌**: 플레이어는 물 타일(`TileMap::IsWorldPositionWalkable()`) 위로도, 건물·나무 줄기(`SceneGraph::IsBlocked()`가 액터의 가상 함수 `BlocksCircle()`로 판정 — 나무 줄기는 `GetCollisionRadius()` 원-원, 건물은 큐브 발자국 정사각형-원)로도 이동할 수 없음 — 둘 다 축별 슬라이딩 충돌이라 벽/물가에 붙어 미끄러지듯 이동함. 화면에 덩어리로 보이는 오브젝트는 실제로도 막히도록(시각-충돌 불일치 방지) 의도적으로 맞춤
- **모델 캐싱**: 원/타원 등 절차적 메시를 생성해 `./Cache/*.mesh` 파일로 저장하고, 다음 실행부터는 파일에서 로딩만 함 ([Mesh.h](SimpleGame/Mesh.h)/[.cpp](SimpleGame/Mesh.cpp), [MeshCache.h](SimpleGame/MeshCache.h)/[.cpp](SimpleGame/MeshCache.cpp)) — 캐릭터 머리엔 원형 메시, 나무 수관·아이템엔 타원 메시 적용
- **에셋 구성 원칙**: 모든 오브젝트는 사각형(`DrawObject`)/원·타원(`DrawMesh` + 캐시된 메시)의 조합으로만 구성 — 입체 도형(큐브/원기둥/타원체)도 새 메시나 셰이더 없이 [Shapes.h](SimpleGame/Shapes.h)의 함수로 회전시킨 사각형·원판만 써서 조립함(고정 카메라에서 보이는 면만 그리고 면마다 밝기를 달리해 입체감을 냄, 면끼리 그리기 순서는 무관). 나무는 원기둥 줄기(`DrawCylinder`: 바닥 원판→카메라를 향한 옆면 사각형→윗면 원판이라 위아래가 타원으로 둥글게 보임)+카메라를 향해 세운 타원 수관 두 겹(`DrawUprightEllipse`, 큰 타원+왼쪽 위로 치우친 밝은 하이라이트 타원, 바람에 살짝 흔들림), 아이템(약초/퀘스트템)은 위아래로 떠다니는 작은 타원, 건물은 큐브, 캐릭터는 사각 몸통·팔·다리+원형 머리 조합 (각 액터 클래스의 `OnRender`: `CharacterActor`/`BuildingActor`/`TreeActor`/`ItemActor`). 원 메시는 큰 수관도 매끄럽도록 32각형(`circle_32`)
- **불 이펙트**: 횃불이 시간 기반으로 일렁이는 전용 셰이더로 렌더링 (`Renderer::DrawFire`, `Shaders/Fire.fs`, `EntityType::Fire`)
- **행동 쿨타임**: 공격(Space)과 상호작용(E)은 하나의 쿨타임(1초, `kActionCooldownSeconds`)을 공유함 — 공격하거나 실제로 상호작용하면 그 시간이 지나기 전에는 공격도 상호작용도 다시 할 수 없고, 쿨타임 중 입력은 무시(공격 모션도 재생 안 함)됨. 허공에 휘두른 공격도 쿨타임이 시작되지만, 근처에 대상이 없어 상호작용이 일어나지 않은 E 입력은 쿨타임을 시작하지 않음(헛 눌렀다고 공격까지 막히지 않게). HUD의 경험치바 아래 작은 막대가 충전 상태를 보여줌(충전 중 주황색으로 차오르고 다 차면 초록색, `Hud::DrawStatus`의 `actionReadiness`). 키를 누르고 있어도 1초에 한 번만 실행됨
- **전투/성장**: Space로 근접 공격, 야생 짐승 처치 시 경험치 획득, 레벨업 시 공격력/최대체력 상승(레벨1에서 즉시 적용). 약초 아이템 습득으로도 경험치 획득
- **몬스터 AI**: 짐승(`AnimalActor`)은 생성 시 `isAggressive`로 사슴(비공격, 배회만 함)과 늑대(공격형)를 구분. 늑대는 감지 반경(`kMonsterDetectRadius`) 안에 플레이어가 들어오면 추적을 시작해 직선으로 쫓아오고(`AnimalActor::UpdateMonsterAI`, 씬을 통해 플레이어를 찾고 물/건물/나무는 피해서 이동 — 우회 경로탐색은 아님), 공격 사거리에 닿으면 쿨다운마다 데미지를 입힘(`PlayerActor::TakeDamage`). 플레이어보다 느려서(2.3 vs 4.0) 도망칠 여지가 있고, 너무 멀어지거나(`kMonsterGiveUpRadius`) 자기 anchor에서 너무 벗어나면(`kMonsterLeashRadius`) 추적을 포기하고 배회로 복귀. 플레이어 체력이 0 이하가 되면 페널티 없이 마을 스폰 지점으로 리스폰(`PlayerActor::Respawn`, 세이브/로드가 없는 프로토타입이라 단순하게 처리)
- **콘솔 한글 출력**: `main()` 시작 시 `SetConsoleOutputCP`/`SetConsoleCP`(windows.h)로 콘솔 코드페이지를 UTF-8(65001)로 맞추고, 프로젝트 전체에 `/utf-8` 컴파일 옵션을 추가해 소스 인코딩(UTF-8)과 실행 문자셋을 일치시킴 — 이전엔 콘솔 로그의 한글이 깨졌음
- **HUD**: 화면 왼쪽 위에 레벨 배지(원형 메시+숫자)·체력바(빨강)·경험치바(하늘색) 표시. 아직 폰트/텍스트 렌더링이 없어 숫자는 계산기 표시창처럼 사각형 세그먼트 조각(7세그먼트 방식)으로 그림. 월드 카메라와 별개인 화면 고정 좌표계(`Mat4::Ortho(0,800,0,600,...)`)를 써서 카메라 줌/회전이나 후처리(블룸 등)와 무관하게 항상 또렷하게 보이고, `PostProcess::EndCaptureAndPresent()` 이후(기본 프레임버퍼)에 그려서 후처리 영향을 받지 않음 (`Hud::DrawStatus`, [Hud.cpp](SimpleGame/Hud.cpp))
- **전투 연출**: 짐승이 피격당하면 잠깐 하얗게 번쩍이는 히트플래시(`CharacterActor::TriggerFlash`(`FlashKind::White`)), 플레이어가 몬스터에게 맞으면 붉게 번쩍임(`FlashKind::Red`, 동물의 흰색과 구분되는 톤). 플레이어 발밑엔 위치를 짚어주는 은은하게 펄스하는 타원 마커(플레이어의 자식 `RingActor`)
- **조준 하이라이트**: 공격 사거리 안에 짐승이 들어오면 발밑에 붉은 펄스 링, 상호작용 사거리 안에 NPC/아이템이 들어오면 하늘색 펄스 링이 표시됨(대상 지정용 `RingActor` 2개, 매 프레임 `Update()`에서 `SetTarget`으로 갱신) — Space/E를 누르기 전에 "지금 뭐가 맞을지/상호작용될지"를 미리 보여줌. `FindNearestAttackTarget()`/`FindNearestInteractable()`(둘 다 `SceneGraph::FindNearest` 사용)를 실제 `TryAttack()`/`TryInteract()`와 그대로 공유해서, 링이 보이면 반드시 그 판정이 성공하도록 보장
- **이름표**: Player/NPC/Animal 머리 위에 영문 이름(Player/Elder/Villager/Deer/Wolf, `Actor::GetName()`)을 표시. 이 프로젝트엔 자체 폰트가 없어서 freeglut 내장 비트맵 폰트(`glutBitmapCharacter`, `GLUT_BITMAP_HELVETICA_10`)를 그대로 씀 — 셰이더가 아니라 레거시 고정기능 래스터 경로라 한글 글리프는 없음(그래서 영문 라벨만 가능). 화면 위치는 `Math3D.h`의 `TransformToNDC()`로 월드 좌표를 직접 NDC로 계산해 `glRasterPos`에 넘김(레거시 모델뷰/프로젝션 행렬을 이 프로젝트에서 전혀 건드리지 않아 항등행렬 상태이므로 가능). HUD와 마찬가지로 후처리 이후 기본 프레임버퍼에 그려서 블룸 등의 영향을 받지 않음(`Hud::DrawNameTags`가 `SceneGraph::ForEach`로 액터를 순회)
- **미니맵**: 화면 오른쪽 위에 128×128 픽셀 패널로 맵 전체를 표시([MiniMap.h](SimpleGame/MiniMap.h)/[.cpp](SimpleGame/MiniMap.cpp)). 카메라와 같은 yaw·좌우/상하 반전으로 놓은, 세로로 눌리지 않은 마름모 모양이라 미니맵에서 가는 방향이 본 화면과 같아 보임(`Camera::GetYawRadians`/`IsFlipped*`로 방향을 읽고, 바뀌면 정적 부분을 다시 만듦). 풀밭은 회전한 사각형 하나, 물/돌바닥/길/나무/건물은 화면 좌표 삼각형 목록으로 한 번 만들어 두고, NPC(장로 노랑/마을 사람 베이지)·아이템(약초 연두/퀘스트 하늘색)·짐승(사슴 갈색/늑대 빨강)·플레이어(흰색)는 매 프레임 씬에서 모아 색깔별로 그림 — 타일 1024개를 하나씩 그리지 않고 패널 포함 총 15회 안팎의 드로우 콜로 끝나도록 `Renderer::DrawTriangles`(CPU가 만든 삼각형 목록을 임시 VBO에 올려 한 번에 그림)를 추가함. 후처리 이후 HUD와 같은 화면 고정 좌표계로 그려서 카메라 반전/줌의 영향을 받지 않음
- **드로우 콜 카운터**: [DrawCallHook.h](SimpleGame/DrawCallHook.h)/[.cpp](SimpleGame/DrawCallHook.cpp)가 실행 파일의 임포트 주소 테이블(IAT)에서 `opengl32.dll`의 `glDrawArrays`(있으면 `glDrawElements`도) 주소를 후크 함수로 바꿔치기해서, 드로우 콜이 실행되는 순간 후크로 점프해 카운트한 뒤 원본을 이어서 호출함(그리는 코드는 수정 불필요, 새 드로우 콜도 자동으로 셈). `main()`에서 `Install()`, `RenderScene()` 시작/끝에서 `BeginFrame()`/`EndFrame()`을 호출하고, 1초마다(`kReportIntervalSeconds`, 0이면 매 프레임) 콘솔에 "이번 프레임 N회, 평균/최소/최대, FPS"와 씬 컬링 통계("씬 액터 M개 중 K개 렌더, J개 컬링")를 출력. 우리 exe가 직접 부르는 호출만 세며 freeglut.dll 안의 이름표 글자 그리기(`glBitmap`)는 세지 않음
- **채팅창**: NPC 대사와 게임 로그(전투/피격/경험치·레벨업/상호작용 안내)를 화면 왼쪽 아래 채팅창으로 출력하고 콘솔에는 찍지 않음([ChatWindow.h](SimpleGame/ChatWindow.h)/[.cpp](SimpleGame/ChatWindow.cpp)). 각 메시지는 채팅창에 나타난 뒤 3초가 지나면 사라지고(마지막 0.5초는 서서히 투명해짐) 최대 8줄까지 보이며 종류별로 색이 다름(대사 노랑/안내 하늘색/공격 흰색/피해 빨강/보상 금색). 액터와 게임 코드는 화면을 몰라도 `GameLog::Add(GameLog::Kind, UTF-8 문자열)`로 메시지만 넣으면 됨([GameLog.h](SimpleGame/GameLog.h), 채팅창이 매 프레임 `Pop`으로 꺼내감). 한글을 그리기 위해 [TextRasterizer.h](SimpleGame/TextRasterizer.h)가 Windows GDI(맑은 고딕, 16px, 안티앨리어싱)로 문장을 비트맵에 그려 OpenGL 텍스처(RGBA, 알파=글자 커버리지)로 만들고, `Renderer::DrawTexture`(`Shaders/Text.vs/.fs`, 새 텍스처 그리기 경로)로 줄마다 배경 사각형과 함께 그림 — 이름표와 달리 레거시 GL 경로가 아니라 기존 셰이더 파이프라인을 쓰므로 한글이 나오고 컨텍스트 종류에 상관없이 동작함. 콘솔에는 진단/성능 메시지(셰이더 컴파일, 모델 캐시, 드로우 콜)만 남음
- 아직 없음: 실제 이미지 스프라이트/텍스처 파이프라인(글자 텍스처 외), 청크 스트리밍, 세이브/로드 파일, 축복(블레싱) 시스템 실제 구현, 대화 선택지 같은 상호작용형 대화 UI

이 항목들은 실제 구현이 진행될 때마다 이 섹션을 갱신합니다. (문서만 보고 "아직 구현 안 됨"으로 오해하지 않도록, 이 절이 항상 최신 상태를 반영해야 함)

## 저장소/빌드 관리 방침

- **활성 개발 브랜치는 `dev`** — 프로토타입/포스트프로세싱/줌 등 신규 구현은 모두 `dev`에서 진행하며 `origin/dev`에 푸시되어 있음. `main`은 `dev` 분기 이전 상태(초기 코드 정리 시점)에 멈춰 있으므로, 최신 구현 여부를 확인할 때는 반드시 `dev` 브랜치 기준으로 볼 것
- 빌드 산출물(`.obj`, `.pdb`, `.tlog`, `.exe`, `.pch`, `.ilk`, `.idb`, `.log` 등, `x64/` 폴더 하위)은 git에 커밋하지 않음 — `.gitignore`로 관리
- 단, `x64/Debug`·`x64/Release`의 `freeglut.dll`, `glew32.dll`은 예외적으로 계속 추적함 — 빌드 후 자동 복사(post-build copy) 단계가 없어서, 추적하지 않으면 새로 클론한 환경에서 실행 파일이 DLL을 찾지 못해 실행되지 않기 때문. 추후 post-build 복사 단계나 vcpkg를 도입하면 이 예외는 제거 가능

## 코드 컨벤션

- 클래스/함수/메서드: PascalCase (`Renderer`, `DrawObject`, `GenerateVillageLevel`)
- 멤버 변수: `m_` 접두사 (`m_Width`), 전역 변수: `g_` 접두사 (`g_Player`), 상수: `k` 접두사 (`kInteractElder`)
- 들여쓰기는 탭(tab) 사용, 중괄호는 다음 줄에 (Allman 스타일)
- 함수/블록 내부에서도 의미 단위가 바뀌는 지점(변수 선언부 이후, 반복문/조건 분기 앞뒤 등)에는 빈 줄을 넣어 가독성을 확보
- 주석/로그/콘솔 출력 문자열은 한글로 작성 (원본 라이선스 헤더 등 법적 텍스트는 예외)
- 셰이더 컴파일처럼 여러 클래스가 공유하는 로직은 `ShaderUtil` 같은 네임스페이스 유틸로 분리
- 파일 하나에만 쓰이는 헬퍼 함수/상수는 `namespace { ... }` (익명 네임스페이스)로 감싸서 외부 노출을 막음

## 작업 스타일

- **빌드/실행은 전적으로 사용자가 직접 함.** 코드 수정 후 컴파일 확인 삼아 MSBuild를 돌리는 것도 하지 않음 — 명시적으로 요청받거나 정적으로 도저히 확인할 수 없는 특별한 이유가 있을 때만 예외적으로 진행하며, 그 경우도 먼저 물어보는 것을 우선함
