#pragma once

#include "Actor.h"
#include "Model.h"

enum class AnimalKind;

// 게임 오브젝트 ↔ Resource 폴더의 모델 연결표. 어느 인물·짐승·건물·나무·바위가 어느 프리셋·키트 조각으로 그려지는지를
// 한곳에 모으고(이름은 Resource/README.md와 각 *.json), 정적 조각을 그리는 도구와 바닥 텍스처 읽기를 둔다.
// Resource 폴더가 없거나 파일·조각이 없으면 빈 값을 돌려주고, 부르는 쪽은 예전처럼 도형으로 그린다. F3으로 모델과
// 도형을 바꿔 가며 비교할 수 있다(SetEnabled).
namespace GameModels
{
	// 3D 모델로 그릴지(끄면 모든 오브젝트가 예전 도형으로). Resource 폴더가 없으면 늘 거짓.
	bool IsEnabled();
	void SetEnabled(bool enabled);

	// 키트 조각·프리팹 하나(모델 파일 + 그 안의 덩어리).
	struct Piece
	{
		const ModelAsset* asset = nullptr;
		const ModelAsset::Part* part = nullptr;

		bool IsValid() const { return asset != nullptr && part != nullptr && part->indexCount > 0; }
		float Height() const { return IsValid() ? part->maxZ : 0.f; }
		float ExtentX() const { return IsValid() ? part->maxX - part->minX : 0.f; }
		float ExtentY() const { return IsValid() ? part->maxY - part->minY : 0.f; }
	};

	// 사람: 이야기 ID(+ 머리 위 이름표 — 같은 ID를 둘이 쓰는 경우: 노파/정체를 드러낸 에리스, 모이라이 셋)의 프리셋.
	// 마을 주민은 variant로 그 마을의 남녀 프리셋 중 하나를 고른다. outRecolor가 true면 옷·머리 색을 그 사람의 색으로
	// 바꿔 칠할 사람(이름 없는 주민·순례객 등 — 신·이야기 인물은 프리셋 색 그대로).
	const ModelAsset* ForPerson(const char* storyId, const char* nameTag, unsigned int variant, bool& outRecolor);
	const ModelAsset* ForPlayer();

	// 짐승·괴물(사람 모양 괴물은 인간형 프리셋). outRecolor는 ForPerson과 같음(산적·마이나스는 옷 색을 바꿈).
	const ModelAsset* ForAnimal(AnimalKind kind, bool& outRecolor);

	// 키트 조각(Nature_Kit·Buildings_Kit·Goods_Kit)과 프리팹(Buildings/Prefabs/<이름>.glb 전체).
	Piece NaturePiece(const char* name);
	Piece BuildingPiece(const char* name);
	Piece GoodsPiece(const char* name);
	Piece Prefab(const char* name);

	// 정적 조각을 그린다: 조각의 기준점(바닥 가운데)을 (x, y, z)에 두고 z축으로 yaw(라디안)만큼 돌려 축마다 크기를 곱한다.
	// tint는 전체 색에 곱하는 값.
	void DrawPiece(const RenderContext& ctx, const Piece& piece, float x, float y, float z, float yaw,
		float scaleX, float scaleY, float scaleZ, float tint = 1.f, float alpha = 1.f);

	// 월드 자리마다 정해지는 수(같은 자리면 늘 같은 값 — 나무·바위·집의 모양 고르기와 돌린 각도).
	unsigned int HashSpot(float x, float y);

	// 바닥 재질 텍스처(Resource/Nature/Textures의 잔디·돌바닥·흙길·바위)를 읽어 렌더러에 넘긴다. 없으면 아무것도 안 한다.
	void LoadGroundTextures(Renderer& renderer);

	// 읽어 둔 텍스처를 지운다(렌더러보다 먼저).
	void Shutdown();
}
