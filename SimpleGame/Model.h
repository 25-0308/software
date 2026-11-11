#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "Math3D.h"
#include "Renderer.h"

// Resource 폴더(저장소 루트의 Resource/ — Blender 스크립트로 만든 로우폴리 리소스)의 glTF 모델(.glb)을 읽어 그리는 도구.
//
//  - ModelAsset: .glb 파일 하나. 노드 계층(이동·회전·크기), 메시(면마다 단색 재질 — 정점 색으로 구움), 스킨(뼈대와
//    역바인드 행렬), 애니메이션 클립(뼈마다 이동·회전·크기 키, LINEAR/STEP)을 들고, 정점·인덱스를 GPU 버퍼 하나로 올린다.
//    키트 파일(Nature_Kit·Buildings_Kit·Goods_Kit)은 맨 위 노드 하나가 조각 하나라, 이름으로 조각(Part)을 골라 그린다.
//  - 축: glTF는 Y가 위·앞이 +Z인데 게임은 Z가 위다. 읽을 때 (x, y, z)gl → (x, -z, y)게임으로 돌린다(블렌더 축과 같아짐 —
//    캐릭터·짐승의 앞은 -y, 건물 벽 앞면도 -y). 기준점은 캐릭터·짐승 = 발밑, 건축·자연물 = 바닥 가운데.
//  - ModelPose: 한 캐릭터의 지금 동작(클립·재생 시각·이전 클립에서 넘어오는 중인 비율)을 들고, 그릴 때 뼈 행렬을 만든다.
//  - 색: glTF의 baseColorFactor는 선형 색이라, 게임의 다른 색(감마 공간 값을 그대로 씀)과 맞도록 감마(1/2.2)를 씌워 굽는다.
//    재질 이름이 "<프리셋>.<역할>"이면 역할(ClothA·Hair·Fur 등) 번호를 정점에 같이 담아 두어, 같은 모델을 사람마다 다른
//    옷·머리 색으로 바꿔 칠할 수 있다(ModelDrawParams::roleColors).
class ModelAsset
{
public:
	// 바꿔 칠할 수 있는 색 역할(정점의 role 값). 0은 바꾸지 않음.
	enum Role
	{
		RoleNone = 0,
		RoleClothA,
		RoleClothB,
		RoleHair,
		RoleSkin,
		RoleFur,
		RoleFurDark,
		RoleFurLight,
		RoleTrim,
		RoleCount,
	};

	struct Node
	{
		std::string name;
		int parent = -1;
		std::vector<int> children;
		float translation[3] = { 0.f, 0.f, 0.f };
		float rotation[4] = { 0.f, 0.f, 0.f, 1.f }; // x, y, z, w
		float scale[3] = { 1.f, 1.f, 1.f };
		int mesh = -1;
		int skin = -1;
	};

	struct Channel
	{
		int node = -1;
		int path = 0; // 0 이동, 1 회전, 2 크기
		bool step = false;
		std::vector<float> times;
		std::vector<float> values; // 이동·크기는 3개씩, 회전은 4개씩
	};

	struct Clip
	{
		std::string name;
		float duration = 0.f;
		std::vector<Channel> channels;
	};

	// 그릴 수 있는 덩어리: 인덱스 버퍼의 한 구간. 파일 전체(이름 "")와, 맨 위 노드마다 하나씩(키트의 조각).
	struct Part
	{
		std::string name;
		int firstIndex = 0;
		int indexCount = 0;
		float minX = 0.f, minY = 0.f, minZ = 0.f; // 바인드 자세의 경계(게임 축, 모델 원점 기준)
		float maxX = 0.f, maxY = 0.f, maxZ = 0.f;
		bool skinned = false;
	};

	ModelAsset() = default;
	~ModelAsset();

	ModelAsset(const ModelAsset&) = delete;
	ModelAsset& operator=(const ModelAsset&) = delete;

	// .glb를 읽고 GPU에 올린다. OpenGL 컨텍스트가 현재 상태여야 한다. 실패하면 false(콘솔에 이유).
	// kit이면 키트 파일(맨 위 노드 하나 = 조각 하나): 조각마다 맨 위 노드의 자리를 빼서 제 기준점이 원점에 오게 한다
	// (그 대신 파일 전체 덩어리는 조각들이 원점에 겹쳐 쓸모없다). 아니면 파일 전체가 지은 그대로의 한 덩어리(프리팹·캐릭터).
	bool Load(const std::string& path, Renderer& renderer, bool kit);

	const std::string& GetPath() const { return m_Path; }

	// 파일 전체 덩어리, 또는 이름이 같은 조각(키트). 없으면 nullptr.
	const Part* GetWhole() const { return m_Parts.empty() ? nullptr : &m_Parts[0]; }
	const Part* FindPart(const std::string& name) const;
	const std::vector<Part>& GetParts() const { return m_Parts; }

	bool IsSkinned() const { return !m_SkinJoints.empty(); }
	int GetJointCount() const { return (int)m_SkinJoints.size(); }

	// 이름이 같은 클립 번호(없으면 -1)와 그 길이(초).
	int FindClip(const char* name) const;
	float GetClipDuration(int clip) const;

	// 클립 둘을 섞은 자세의 뼈 행렬(게임 축으로 돌린 것 — 셰이더의 u_Joints)을 outJoints에 채운다.
	// weightB가 0이면 A만, 1이면 B만. 클립 번호가 -1이면 노드의 기본 자세.
	void ComputeJoints(int clipA, float timeA, int clipB, float timeB, float weightB, std::vector<Mat4>& outJoints) const;

	const Renderer::ModelMeshHandle& GetMesh() const { return m_Mesh; }

private:
	// 한 노드의 지금 이동·회전·크기(애니메이션을 입힌 것).
	struct NodePose
	{
		float translation[3];
		float rotation[4];
		float scale[3];
	};

	bool Parse(const std::vector<char>& file);

	// clip의 time 시각 키로 poses를 덮는다(키가 없는 노드는 그대로).
	void SampleClip(int clip, float time, std::vector<NodePose>& poses) const;

	std::string m_Path;
	Renderer* m_Renderer = nullptr;
	Renderer::ModelMeshHandle m_Mesh;

	bool m_Kit = false;
	std::vector<Node> m_Nodes;
	std::vector<int> m_Order;             // 부모가 자식보다 먼저 오는 노드 순서
	std::vector<int> m_SkinJoints;        // 뼈 = 노드 번호
	std::vector<Mat4> m_InverseBind;      // 뼈마다 역바인드 행렬(glTF 축)
	std::vector<Clip> m_Clips;
	std::vector<Part> m_Parts;
};

// 한 캐릭터가 지금 하고 있는 동작. 클립을 바꾸면 짧게(fadeSeconds) 이전 동작에서 섞어 넘어간다.
class ModelPose
{
public:
	// clip을 재생한다(이미 그 클립이면 그대로 이어서). rate는 재생 속도 배율, loop면 끝에서 처음으로 돈다.
	void Play(int clip, float rate, bool loop, float fadeSeconds);

	// 재생 시각을 직접 정한다(0~1 진행률 × 클립 길이 — 공격처럼 게임이 진행을 정하는 동작).
	void SetProgress(const ModelAsset& asset, float progress);

	void Advance(const ModelAsset& asset, float deltaSeconds);

	// 지금 자세의 뼈 행렬.
	void Compute(const ModelAsset& asset, std::vector<Mat4>& outJoints) const;

	int GetClip() const { return m_Clip; }

private:
	int m_Clip = -1;
	float m_Time = 0.f;
	float m_Rate = 1.f;
	bool m_Loop = true;

	int m_PrevClip = -1;
	float m_PrevTime = 0.f;
	float m_Fade = 0.f;      // 남은 섞임 시간
	float m_FadeTotal = 0.f;
};

// 모델 파일 캐시와 Resource 폴더 찾기. 같은 파일은 한 번만 읽는다. Resource 폴더가 없거나 파일이 없으면 nullptr을
// 돌려주고(콘솔에 한 번 알림), 부르는 쪽은 예전처럼 도형으로 그린다.
namespace ModelLibrary
{
	// 작업 폴더 기준으로 Resource 폴더를 찾는다(../Resource, ./Resource, ../../Resource). 못 찾으면 빈 문자열.
	const std::string& GetResourceRoot();

	// Resource 폴더 기준 상대 경로(예: "Characters/Humanoid/Presets/PLAYER.glb")의 모델. 처음 부를 때 읽는다.
	// kit이면 키트 파일로 읽는다(ModelAsset::Load). 같은 파일을 한쪽 방식으로만 쓴다.
	const ModelAsset* Get(const std::string& relativePath, bool kit = false);

	// Resource 폴더 기준 상대 경로의 파일이 있는지.
	bool Exists(const std::string& relativePath);

	// 읽어 둔 모델을 모두 지운다(GPU 버퍼 — 렌더러보다 먼저).
	void Shutdown();

	// 렌더러를 알려 준다(모델을 GPU에 올릴 때 쓴다).
	void SetRenderer(Renderer* renderer);
}
