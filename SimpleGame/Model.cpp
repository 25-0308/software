#include "stdafx.h"
#include "Model.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>

#include "Json.h"

static_assert(ModelAsset::RoleCount == Renderer::kModelRoleCount, "모델 색 역할 수가 Renderer와 같아야 한다");

namespace
{
	// glTF(Y 위, 앞 +Z) → 게임(Z 위): (x, y, z) → (x, -z, y). 열 우선 행렬.
	Mat4 AxisConversion()
	{
		Mat4 c;
		c.m[0] = 1.f;   // gl x → 게임 x
		c.m[6] = 1.f;   // gl y → 게임 z
		c.m[9] = -1.f;  // gl z → 게임 -y
		c.m[15] = 1.f;
		return c;
	}

	Mat4 FromTRS(const float t[3], const float r[4], const float s[3])
	{
		float x = r[0], y = r[1], z = r[2], w = r[3];
		Mat4 m;
		m.m[0] = (1.f - 2.f * (y * y + z * z)) * s[0];
		m.m[1] = (2.f * (x * y + z * w)) * s[0];
		m.m[2] = (2.f * (x * z - y * w)) * s[0];
		m.m[4] = (2.f * (x * y - z * w)) * s[1];
		m.m[5] = (1.f - 2.f * (x * x + z * z)) * s[1];
		m.m[6] = (2.f * (y * z + x * w)) * s[1];
		m.m[8] = (2.f * (x * z + y * w)) * s[2];
		m.m[9] = (2.f * (y * z - x * w)) * s[2];
		m.m[10] = (1.f - 2.f * (x * x + y * y)) * s[2];
		m.m[12] = t[0];
		m.m[13] = t[1];
		m.m[14] = t[2];
		m.m[15] = 1.f;
		return m;
	}

	void TransformPoint(const Mat4& m, const float in[3], float out[3])
	{
		out[0] = m.m[0] * in[0] + m.m[4] * in[1] + m.m[8] * in[2] + m.m[12];
		out[1] = m.m[1] * in[0] + m.m[5] * in[1] + m.m[9] * in[2] + m.m[13];
		out[2] = m.m[2] * in[0] + m.m[6] * in[1] + m.m[10] * in[2] + m.m[14];
	}

	void TransformDirection(const Mat4& m, const float in[3], float out[3])
	{
		out[0] = m.m[0] * in[0] + m.m[4] * in[1] + m.m[8] * in[2];
		out[1] = m.m[1] * in[0] + m.m[5] * in[1] + m.m[9] * in[2];
		out[2] = m.m[2] * in[0] + m.m[6] * in[1] + m.m[10] * in[2];

		float length = sqrtf(out[0] * out[0] + out[1] * out[1] + out[2] * out[2]);
		if (length > 0.000001f)
		{
			out[0] /= length;
			out[1] /= length;
			out[2] /= length;
		}
	}

	// glTF 색은 선형 값이라, 게임의 다른 색(감마 공간의 값을 그대로 씀)과 맞추려고 감마를 씌운다.
	float LinearToGamma(float value)
	{
		return powf((value > 0.f) ? value : 0.f, 1.f / 2.2f);
	}

	void NormalizeQuaternion(float q[4])
	{
		float length = sqrtf(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);
		if (length > 0.000001f)
		{
			for (int i = 0; i < 4; ++i)
			{
				q[i] /= length;
			}
		}
	}

	// 두 회전을 t만큼 섞는다(짧은 쪽으로 — 정규화 선형 보간).
	void BlendQuaternion(const float a[4], const float b[4], float t, float out[4])
	{
		float dot = a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3];
		float sign = (dot < 0.f) ? -1.f : 1.f;
		for (int i = 0; i < 4; ++i)
		{
			out[i] = a[i] * (1.f - t) + b[i] * sign * t;
		}
		NormalizeQuaternion(out);
	}

	bool ReadFile(const std::string& path, std::vector<char>& out)
	{
		std::ifstream file(path, std::ios::binary);
		if (!file)
		{
			return false;
		}

		file.seekg(0, std::ios::end);
		std::streamoff size = file.tellg();
		file.seekg(0, std::ios::beg);
		if (size <= 0)
		{
			return false;
		}

		out.resize((size_t)size);
		file.read(out.data(), size);
		return (bool)file;
	}

	// 재질 이름 "<프리셋>.<역할>"의 역할 → 번호.
	int RoleFromMaterialName(const std::string& name)
	{
		size_t dot = name.rfind('.');
		std::string role = (dot == std::string::npos) ? name : name.substr(dot + 1);

		if (role == "ClothA") return ModelAsset::RoleClothA;
		if (role == "ClothB") return ModelAsset::RoleClothB;
		if (role == "Hair") return ModelAsset::RoleHair;
		if (role == "Skin") return ModelAsset::RoleSkin;
		if (role == "Fur") return ModelAsset::RoleFur;
		if (role == "FurDark") return ModelAsset::RoleFurDark;
		if (role == "FurLight") return ModelAsset::RoleFurLight;
		if (role == "Trim") return ModelAsset::RoleTrim;
		return ModelAsset::RoleNone;
	}

	// glTF 접근자(accessor) 하나를 읽는 도구.
	class AccessorReader
	{
	public:
		AccessorReader(const JsonValue& root, const char* bin, size_t binSize)
			: m_Root(root)
			, m_Bin(bin)
			, m_BinSize(binSize)
		{
		}

		int Count(int accessor) const
		{
			return m_Root["accessors"].At((size_t)accessor)["count"].AsInt();
		}

		// 원소 하나의 성분 수(SCALAR 1, VEC2 2, VEC3 3, VEC4 4, MAT4 16).
		static int ComponentsOf(const std::string& type)
		{
			if (type == "SCALAR") return 1;
			if (type == "VEC2") return 2;
			if (type == "VEC3") return 3;
			if (type == "VEC4") return 4;
			if (type == "MAT4") return 16;
			return 0;
		}

		// 접근자를 float로 풀어 out에 채운다(정규화된 정수는 0~1로). 성분 수를 돌려준다(실패하면 0).
		int ReadFloats(int accessor, std::vector<float>& out) const
		{
			const JsonValue& info = m_Root["accessors"].At((size_t)accessor);
			int count = info["count"].AsInt();
			int components = ComponentsOf(info["type"].AsString());
			int componentType = info["componentType"].AsInt();
			bool normalized = info["normalized"].AsBool();

			const char* base = nullptr;
			size_t stride = 0;
			if (!Locate(info, components, componentType, base, stride))
			{
				return 0;
			}

			out.resize((size_t)count * (size_t)components);
			for (int i = 0; i < count; ++i)
			{
				const char* element = base + stride * (size_t)i;
				for (int c = 0; c < components; ++c)
				{
					out[(size_t)i * (size_t)components + (size_t)c] = ReadComponent(element, c, componentType, normalized);
				}
			}
			return components;
		}

		// 인덱스·뼈 번호처럼 정수인 접근자.
		bool ReadInts(int accessor, std::vector<unsigned int>& out) const
		{
			const JsonValue& info = m_Root["accessors"].At((size_t)accessor);
			int count = info["count"].AsInt();
			int components = ComponentsOf(info["type"].AsString());
			int componentType = info["componentType"].AsInt();

			const char* base = nullptr;
			size_t stride = 0;
			if (!Locate(info, components, componentType, base, stride))
			{
				return false;
			}

			out.resize((size_t)count * (size_t)components);
			for (int i = 0; i < count; ++i)
			{
				const char* element = base + stride * (size_t)i;
				for (int c = 0; c < components; ++c)
				{
					unsigned int value = 0;
					if (componentType == 5121) value = ((const unsigned char*)element)[c];
					else if (componentType == 5123) { unsigned short v; memcpy(&v, element + 2 * c, 2); value = v; }
					else if (componentType == 5125) memcpy(&value, element + 4 * c, 4);
					out[(size_t)i * (size_t)components + (size_t)c] = value;
				}
			}
			return true;
		}

	private:
		static int ComponentSize(int componentType)
		{
			switch (componentType)
			{
			case 5120: case 5121: return 1;
			case 5122: case 5123: return 2;
			case 5125: case 5126: return 4;
			}
			return 0;
		}

		static float ReadComponent(const char* element, int c, int componentType, bool normalized)
		{
			switch (componentType)
			{
			case 5126: { float v; memcpy(&v, element + 4 * c, 4); return v; }
			case 5121: { unsigned char v = ((const unsigned char*)element)[c]; return normalized ? v / 255.f : (float)v; }
			case 5123: { unsigned short v; memcpy(&v, element + 2 * c, 2); return normalized ? v / 65535.f : (float)v; }
			case 5120: { signed char v = ((const signed char*)element)[c]; return normalized ? (std::max)(v / 127.f, -1.f) : (float)v; }
			case 5122: { short v; memcpy(&v, element + 2 * c, 2); return normalized ? (std::max)(v / 32767.f, -1.f) : (float)v; }
			case 5125: { unsigned int v; memcpy(&v, element + 4 * c, 4); return (float)v; }
			}
			return 0.f;
		}

		bool Locate(const JsonValue& info, int components, int componentType, const char*& outBase, size_t& outStride) const
		{
			if (components == 0 || ComponentSize(componentType) == 0 || !info.Has("bufferView"))
			{
				return false;
			}

			const JsonValue& view = m_Root["bufferViews"].At((size_t)info["bufferView"].AsInt());
			size_t offset = (size_t)view["byteOffset"].AsInt() + (size_t)info["byteOffset"].AsInt();
			size_t elementSize = (size_t)ComponentSize(componentType) * (size_t)components;
			size_t stride = view.Has("byteStride") ? (size_t)view["byteStride"].AsInt() : elementSize;
			size_t count = (size_t)info["count"].AsInt();

			if (count > 0 && offset + stride * (count - 1) + elementSize > m_BinSize)
			{
				return false; // 버퍼 밖을 가리킴(깨진 파일)
			}

			outBase = m_Bin + offset;
			outStride = stride;
			return true;
		}

		const JsonValue& m_Root;
		const char* m_Bin;
		size_t m_BinSize;
	};

	// 재질 하나의 구운 색.
	struct MaterialColor
	{
		float r = 0.8f, g = 0.8f, b = 0.8f;
		float emissive = 0.f;
		int role = ModelAsset::RoleNone;
	};

	std::vector<MaterialColor> ReadMaterials(const JsonValue& root)
	{
		std::vector<MaterialColor> materials;
		const JsonValue& list = root["materials"];
		for (size_t i = 0; i < list.Size(); ++i)
		{
			const JsonValue& material = list.At(i);
			const JsonValue& factor = material["pbrMetallicRoughness"]["baseColorFactor"];

			MaterialColor color;
			if (factor.Size() >= 3)
			{
				color.r = LinearToGamma(factor.At(0).AsFloat());
				color.g = LinearToGamma(factor.At(1).AsFloat());
				color.b = LinearToGamma(factor.At(2).AsFloat());
			}

			// 발광: emissiveFactor(× KHR_materials_emissive_strength)의 가장 밝은 성분을 세기로.
			const JsonValue& emissive = material["emissiveFactor"];
			float strength = material["extensions"]["KHR_materials_emissive_strength"]["emissiveStrength"].AsFloat(1.f);
			float brightest = 0.f;
			for (size_t c = 0; c < emissive.Size() && c < 3; ++c)
			{
				brightest = (std::max)(brightest, emissive.At(c).AsFloat());
			}
			color.emissive = brightest * strength * 0.6f;

			color.role = RoleFromMaterialName(material["name"].AsString());
			materials.push_back(color);
		}
		return materials;
	}

	// 경계 상자를 점 하나로 넓힌다.
	void Grow(ModelAsset::Part& part, const float p[3], bool& first)
	{
		if (first)
		{
			part.minX = part.maxX = p[0];
			part.minY = part.maxY = p[1];
			part.minZ = part.maxZ = p[2];
			first = false;
			return;
		}

		part.minX = (std::min)(part.minX, p[0]); part.maxX = (std::max)(part.maxX, p[0]);
		part.minY = (std::min)(part.minY, p[1]); part.maxY = (std::max)(part.maxY, p[1]);
		part.minZ = (std::min)(part.minZ, p[2]); part.maxZ = (std::max)(part.maxZ, p[2]);
	}
}

ModelAsset::~ModelAsset()
{
	if (m_Renderer != nullptr)
	{
		m_Renderer->DestroyModelMesh(m_Mesh);
	}
}

bool ModelAsset::Load(const std::string& path, Renderer& renderer, bool kit)
{
	m_Path = path;
	m_Renderer = &renderer;
	m_Kit = kit;

	std::vector<char> file;
	if (!ReadFile(path, file))
	{
		std::cout << "[모델] 파일을 읽지 못했습니다: " << path << "\n";
		return false;
	}

	if (!Parse(file))
	{
		std::cout << "[모델] glTF를 읽지 못했습니다: " << path << "\n";
		return false;
	}
	return true;
}

bool ModelAsset::Parse(const std::vector<char>& file)
{
	// ---- .glb: 머리(12바이트) + JSON 덩어리 + BIN 덩어리 ----
	if (file.size() < 20 || memcmp(file.data(), "glTF", 4) != 0)
	{
		return false;
	}

	unsigned int jsonLength = 0;
	memcpy(&jsonLength, file.data() + 12, 4);
	if (memcmp(file.data() + 16, "JSON", 4) != 0 || 20 + (size_t)jsonLength > file.size())
	{
		return false;
	}

	const char* bin = nullptr;
	size_t binSize = 0;
	size_t binHeader = 20 + (size_t)jsonLength;
	if (binHeader + 8 <= file.size() && memcmp(file.data() + binHeader + 4, "BIN\0", 4) == 0)
	{
		unsigned int length = 0;
		memcpy(&length, file.data() + binHeader, 4);
		bin = file.data() + binHeader + 8;
		binSize = (std::min)((size_t)length, file.size() - binHeader - 8);
	}

	JsonValue root;
	std::string error;
	if (!JsonValue::Parse(file.data() + 20, jsonLength, root, error))
	{
		std::cout << "[모델] JSON 오류: " << error << "\n";
		return false;
	}

	AccessorReader reader(root, bin, binSize);

	// ---- 노드 ----
	const JsonValue& nodes = root["nodes"];
	m_Nodes.resize(nodes.Size());
	for (size_t i = 0; i < nodes.Size(); ++i)
	{
		const JsonValue& source = nodes.At(i);
		Node& node = m_Nodes[i];
		node.name = source["name"].AsString();
		node.mesh = source["mesh"].AsInt(-1);
		node.skin = source["skin"].AsInt(-1);

		for (int c = 0; c < 3; ++c)
		{
			node.translation[c] = source["translation"].At((size_t)c).AsFloat(0.f);
			node.scale[c] = source["scale"].At((size_t)c).AsFloat(1.f);
		}
		for (int c = 0; c < 4; ++c)
		{
			node.rotation[c] = source["rotation"].At((size_t)c).AsFloat((c == 3) ? 1.f : 0.f);
		}

		const JsonValue& children = source["children"];
		for (size_t c = 0; c < children.Size(); ++c)
		{
			node.children.push_back(children.At(c).AsInt());
		}
	}
	for (size_t i = 0; i < m_Nodes.size(); ++i)
	{
		for (int child : m_Nodes[i].children)
		{
			if (child >= 0 && child < (int)m_Nodes.size())
			{
				m_Nodes[(size_t)child].parent = (int)i;
			}
		}
	}

	// 부모가 자식보다 먼저 오는 순서(자세를 계산할 때 이 순서로 위에서부터 곱한다).
	m_Order.clear();
	std::vector<int> stack;
	for (size_t i = 0; i < m_Nodes.size(); ++i)
	{
		if (m_Nodes[i].parent < 0)
		{
			stack.push_back((int)i);
		}
	}
	while (!stack.empty())
	{
		int index = stack.back();
		stack.pop_back();
		m_Order.push_back(index);
		for (int child : m_Nodes[(size_t)index].children)
		{
			stack.push_back(child);
		}
	}

	// 기본 자세의 월드 행렬(glTF 축).
	std::vector<Mat4> rest(m_Nodes.size(), Mat4::Identity());
	for (int index : m_Order)
	{
		const Node& node = m_Nodes[(size_t)index];
		Mat4 local = FromTRS(node.translation, node.rotation, node.scale);
		rest[(size_t)index] = (node.parent >= 0) ? rest[(size_t)node.parent] * local : local;
	}

	// ---- 스킨(하나만 — 이 리소스는 모델마다 뼈대가 하나다) ----
	const JsonValue& skins = root["skins"];
	if (skins.Size() > 0)
	{
		const JsonValue& skin = skins.At(0);
		const JsonValue& joints = skin["joints"];
		for (size_t i = 0; i < joints.Size(); ++i)
		{
			m_SkinJoints.push_back(joints.At(i).AsInt());
		}

		std::vector<float> matrices;
		if (skin.Has("inverseBindMatrices") && reader.ReadFloats(skin["inverseBindMatrices"].AsInt(), matrices) == 16)
		{
			for (size_t i = 0; i + 16 <= matrices.size(); i += 16)
			{
				Mat4 m;
				memcpy(m.m, &matrices[i], sizeof(float) * 16);
				m_InverseBind.push_back(m);
			}
		}
		m_InverseBind.resize(m_SkinJoints.size(), Mat4::Identity());

		if ((int)m_SkinJoints.size() > Renderer::kModelMaxJoints)
		{
			std::cout << "[모델] 뼈가 너무 많습니다(" << m_SkinJoints.size() << "개 — " << Renderer::kModelMaxJoints << "개까지만): " << m_Path << "\n";
		}
	}

	// ---- 메시 → 정점·인덱스 ----
	std::vector<MaterialColor> materials = ReadMaterials(root);
	Mat4 axis = AxisConversion();
	std::vector<float> vertices;
	std::vector<unsigned int> indices;

	// 맨 위 노드 하나 = 조각 하나(키트). 그 노드 아래의 메시를 모아 한 구간으로.
	m_Parts.clear();
	m_Parts.push_back(Part()); // [0] = 파일 전체
	bool wholeFirst = true;

	const JsonValue& meshes = root["meshes"];
	const JsonValue& sceneNodes = root["scenes"].At((size_t)root["scene"].AsInt(0))["nodes"];

	for (size_t top = 0; top < sceneNodes.Size(); ++top)
	{
		int topIndex = sceneNodes.At(top).AsInt();
		if (topIndex < 0 || topIndex >= (int)m_Nodes.size())
		{
			continue;
		}

		Part part;
		part.name = m_Nodes[(size_t)topIndex].name;
		part.firstIndex = (int)indices.size();
		bool partFirst = true;

		// 키트면 맨 위 노드의 자리(키트 안에 늘어놓은 위치)를 빼서 조각마다 제 기준점이 원점에 오게 한다.
		Mat4 topInverse = Mat4::Identity();
		if (m_Kit)
		{
			const Node& topNode = m_Nodes[(size_t)topIndex];
			topInverse = Mat4::Translate(-topNode.translation[0], -topNode.translation[1], -topNode.translation[2]);
		}

		std::vector<int> subtree(1, topIndex);
		for (size_t s = 0; s < subtree.size(); ++s)
		{
			const Node& node = m_Nodes[(size_t)subtree[s]];
			for (int child : node.children)
			{
				subtree.push_back(child);
			}
			if (node.mesh < 0 || node.mesh >= (int)meshes.Size())
			{
				continue;
			}

			// 스킨 메시는 정점을 바인드 공간 그대로 두고(뼈 행렬이 축 변환까지 함께 함), 스킨 없는 메시는 노드의
			// 기본 자세 행렬과 축 변환을 정점에 구워 넣는다(그릴 때 뼈 행렬 = 단위 행렬).
			bool skinned = (node.skin >= 0) && !m_SkinJoints.empty();
			Mat4 bake = axis * topInverse * rest[(size_t)subtree[s]];
			part.skinned = part.skinned || skinned;

			const JsonValue& primitives = meshes.At((size_t)node.mesh)["primitives"];
			for (size_t p = 0; p < primitives.Size(); ++p)
			{
				const JsonValue& primitive = primitives.At(p);
				if (primitive["mode"].AsInt(4) != 4)
				{
					continue; // 삼각형만
				}

				const JsonValue& attributes = primitive["attributes"];
				std::vector<float> positions;
				std::vector<float> normals;
				std::vector<float> weights;
				std::vector<unsigned int> joints;

				if (reader.ReadFloats(attributes["POSITION"].AsInt(-1), positions) != 3)
				{
					continue;
				}
				size_t count = positions.size() / 3;

				if (!attributes.Has("NORMAL") || reader.ReadFloats(attributes["NORMAL"].AsInt(), normals) != 3)
				{
					normals.assign(count * 3, 0.f);
					for (size_t v = 0; v < count; ++v)
					{
						normals[v * 3 + 1] = 1.f;
					}
				}

				bool hasSkin = skinned && attributes.Has("JOINTS_0") && attributes.Has("WEIGHTS_0")
					&& reader.ReadInts(attributes["JOINTS_0"].AsInt(), joints) && reader.ReadFloats(attributes["WEIGHTS_0"].AsInt(), weights) == 4;

				MaterialColor color;
				int materialIndex = primitive["material"].AsInt(-1);
				if (materialIndex >= 0 && materialIndex < (int)materials.size())
				{
					color = materials[(size_t)materialIndex];
				}

				unsigned int baseVertex = (unsigned int)(vertices.size() / Renderer::kModelFloatsPerVertex);
				for (size_t v = 0; v < count; ++v)
				{
					float position[3] = { positions[v * 3], positions[v * 3 + 1], positions[v * 3 + 2] };
					float normal[3] = { normals[v * 3], normals[v * 3 + 1], normals[v * 3 + 2] };
					float outPosition[3];
					float outNormal[3];

					if (hasSkin)
					{
						outPosition[0] = position[0]; outPosition[1] = position[1]; outPosition[2] = position[2];
						outNormal[0] = normal[0]; outNormal[1] = normal[1]; outNormal[2] = normal[2];
					}
					else
					{
						TransformPoint(bake, position, outPosition);
						TransformDirection(bake, normal, outNormal);
					}

					// 경계는 게임 축의 바인드 자세로(스킨 메시는 바인드 공간에 축 변환만).
					float boundPoint[3];
					if (hasSkin)
					{
						TransformPoint(axis, position, boundPoint);
					}
					else
					{
						boundPoint[0] = outPosition[0]; boundPoint[1] = outPosition[1]; boundPoint[2] = outPosition[2];
					}
					Grow(part, boundPoint, partFirst);
					Grow(m_Parts[0], boundPoint, wholeFirst);

					vertices.push_back(outPosition[0]);
					vertices.push_back(outPosition[1]);
					vertices.push_back(outPosition[2]);
					vertices.push_back(outNormal[0]);
					vertices.push_back(outNormal[1]);
					vertices.push_back(outNormal[2]);
					vertices.push_back(color.r);
					vertices.push_back(color.g);
					vertices.push_back(color.b);
					vertices.push_back(color.emissive);
					vertices.push_back((float)color.role);

					if (hasSkin)
					{
						for (int c = 0; c < 4; ++c)
						{
							unsigned int joint = joints[v * 4 + (size_t)c];
							vertices.push_back((float)((joint < (unsigned int)Renderer::kModelMaxJoints) ? joint : 0u));
						}
						for (int c = 0; c < 4; ++c)
						{
							vertices.push_back(weights[v * 4 + (size_t)c]);
						}
					}
					else
					{
						vertices.push_back(0.f); vertices.push_back(0.f); vertices.push_back(0.f); vertices.push_back(0.f);
						vertices.push_back(1.f); vertices.push_back(0.f); vertices.push_back(0.f); vertices.push_back(0.f);
					}
				}

				// 인덱스(없으면 차례대로).
				std::vector<unsigned int> primitiveIndices;
				if (primitive.Has("indices"))
				{
					reader.ReadInts(primitive["indices"].AsInt(), primitiveIndices);
				}
				else
				{
					for (size_t v = 0; v < count; ++v)
					{
						primitiveIndices.push_back((unsigned int)v);
					}
				}

				for (unsigned int index : primitiveIndices)
				{
					if (index < count)
					{
						indices.push_back(baseVertex + index);
					}
				}
			}
		}

		part.indexCount = (int)indices.size() - part.firstIndex;
		if (part.indexCount > 0)
		{
			m_Parts.push_back(part);
		}
	}

	m_Parts[0].firstIndex = 0;
	m_Parts[0].indexCount = (int)indices.size();
	m_Parts[0].skinned = !m_SkinJoints.empty();

	// ---- 애니메이션 ----
	const JsonValue& animations = root["animations"];
	for (size_t a = 0; a < animations.Size(); ++a)
	{
		const JsonValue& animation = animations.At(a);
		Clip clip;
		clip.name = animation["name"].AsString();

		const JsonValue& samplers = animation["samplers"];
		const JsonValue& channels = animation["channels"];
		for (size_t c = 0; c < channels.Size(); ++c)
		{
			const JsonValue& source = channels.At(c);
			const std::string& path = source["target"]["path"].AsString();

			Channel channel;
			channel.node = source["target"]["node"].AsInt(-1);
			if (path == "translation") channel.path = 0;
			else if (path == "rotation") channel.path = 1;
			else if (path == "scale") channel.path = 2;
			else continue;

			const JsonValue& sampler = samplers.At((size_t)source["sampler"].AsInt());
			channel.step = (sampler["interpolation"].AsString() == "STEP");
			int width = (channel.path == 1) ? 4 : 3;
			if (reader.ReadFloats(sampler["input"].AsInt(), channel.times) != 1 || reader.ReadFloats(sampler["output"].AsInt(), channel.values) != width)
			{
				continue;
			}
			if (channel.times.empty() || channel.values.size() < channel.times.size() * (size_t)width)
			{
				continue;
			}

			clip.duration = (std::max)(clip.duration, channel.times.back());
			clip.channels.push_back(channel);
		}

		m_Clips.push_back(clip);
	}

	// ---- GPU에 올린다 ----
	m_Mesh = m_Renderer->CreateModelMesh(vertices.data(), (int)(vertices.size() / Renderer::kModelFloatsPerVertex),
		indices.data(), (int)indices.size());
	return m_Mesh.vao != 0;
}

const ModelAsset::Part* ModelAsset::FindPart(const std::string& name) const
{
	for (size_t i = 1; i < m_Parts.size(); ++i)
	{
		if (m_Parts[i].name == name)
		{
			return &m_Parts[i];
		}
	}
	return nullptr;
}

int ModelAsset::FindClip(const char* name) const
{
	for (size_t i = 0; i < m_Clips.size(); ++i)
	{
		if (m_Clips[i].name == name)
		{
			return (int)i;
		}
	}
	return -1;
}

float ModelAsset::GetClipDuration(int clip) const
{
	return (clip >= 0 && clip < (int)m_Clips.size()) ? m_Clips[(size_t)clip].duration : 0.f;
}

void ModelAsset::SampleClip(int clipIndex, float time, std::vector<NodePose>& poses) const
{
	if (clipIndex < 0 || clipIndex >= (int)m_Clips.size())
	{
		return;
	}

	const Clip& clip = m_Clips[(size_t)clipIndex];
	for (const Channel& channel : clip.channels)
	{
		if (channel.node < 0 || channel.node >= (int)poses.size())
		{
			continue;
		}

		int width = (channel.path == 1) ? 4 : 3;
		const std::vector<float>& times = channel.times;
		size_t last = times.size() - 1;

		// time이 놓인 두 키 [k, k+1]과 그 사이 비율.
		size_t k = 0;
		float t = 0.f;
		if (time <= times[0])
		{
			k = 0;
		}
		else if (time >= times[last])
		{
			k = last;
		}
		else
		{
			size_t low = 0;
			size_t high = last;
			while (high - low > 1)
			{
				size_t middle = (low + high) / 2;
				if (times[middle] <= time) low = middle;
				else high = middle;
			}
			k = low;
			float span = times[k + 1] - times[k];
			t = (span > 0.f && !channel.step) ? (time - times[k]) / span : 0.f;
		}

		const float* a = &channel.values[k * (size_t)width];
		const float* b = &channel.values[((k < last) ? k + 1 : k) * (size_t)width];
		NodePose& pose = poses[(size_t)channel.node];

		if (channel.path == 1)
		{
			BlendQuaternion(a, b, t, pose.rotation);
		}
		else
		{
			float* target = (channel.path == 0) ? pose.translation : pose.scale;
			for (int c = 0; c < 3; ++c)
			{
				target[c] = a[c] + (b[c] - a[c]) * t;
			}
		}
	}
}

void ModelAsset::ComputeJoints(int clipA, float timeA, int clipB, float timeB, float weightB, std::vector<Mat4>& outJoints) const
{
	outJoints.clear();
	if (m_SkinJoints.empty())
	{
		return;
	}

	// 노드마다 기본 자세에서 시작해 클립의 키로 덮는다.
	std::vector<NodePose> posesA(m_Nodes.size());
	for (size_t i = 0; i < m_Nodes.size(); ++i)
	{
		memcpy(posesA[i].translation, m_Nodes[i].translation, sizeof(float) * 3);
		memcpy(posesA[i].rotation, m_Nodes[i].rotation, sizeof(float) * 4);
		memcpy(posesA[i].scale, m_Nodes[i].scale, sizeof(float) * 3);
	}

	std::vector<NodePose> posesB;
	if (weightB > 0.f && clipB >= 0)
	{
		posesB = posesA;
		SampleClip(clipB, timeB, posesB);
	}
	SampleClip(clipA, timeA, posesA);

	if (!posesB.empty())
	{
		if (weightB >= 1.f)
		{
			posesA.swap(posesB);
		}
		else
		{
			for (size_t i = 0; i < posesA.size(); ++i)
			{
				for (int c = 0; c < 3; ++c)
				{
					posesA[i].translation[c] += (posesB[i].translation[c] - posesA[i].translation[c]) * weightB;
					posesA[i].scale[c] += (posesB[i].scale[c] - posesA[i].scale[c]) * weightB;
				}
				float rotation[4];
				BlendQuaternion(posesA[i].rotation, posesB[i].rotation, weightB, rotation);
				memcpy(posesA[i].rotation, rotation, sizeof(rotation));
			}
		}
	}

	std::vector<Mat4> world(m_Nodes.size(), Mat4::Identity());
	for (int index : m_Order)
	{
		const NodePose& pose = posesA[(size_t)index];
		Mat4 local = FromTRS(pose.translation, pose.rotation, pose.scale);
		int parent = m_Nodes[(size_t)index].parent;
		world[(size_t)index] = (parent >= 0) ? world[(size_t)parent] * local : local;
	}

	// 뼈 행렬 = 축 변환 × 뼈의 월드 행렬 × 역바인드.
	Mat4 axis = AxisConversion();
	size_t count = (std::min)(m_SkinJoints.size(), (size_t)Renderer::kModelMaxJoints);
	outJoints.resize(count);
	for (size_t i = 0; i < count; ++i)
	{
		int node = m_SkinJoints[i];
		Mat4 joint = (node >= 0 && node < (int)world.size()) ? world[(size_t)node] : Mat4::Identity();
		outJoints[i] = axis * joint * m_InverseBind[i];
	}
}

// ---------------------------------------------------------------- ModelPose

void ModelPose::Play(int clip, float rate, bool loop, float fadeSeconds)
{
	m_Rate = rate;
	m_Loop = loop;
	if (clip == m_Clip)
	{
		return;
	}

	m_PrevClip = m_Clip;
	m_PrevTime = m_Time;
	m_Clip = clip;
	m_Time = 0.f;
	m_FadeTotal = (m_PrevClip >= 0) ? fadeSeconds : 0.f;
	m_Fade = m_FadeTotal;
}

void ModelPose::SetProgress(const ModelAsset& asset, float progress)
{
	float duration = asset.GetClipDuration(m_Clip);
	m_Time = (std::max)(0.f, (std::min)(1.f, progress)) * duration;
}

void ModelPose::Advance(const ModelAsset& asset, float deltaSeconds)
{
	float duration = asset.GetClipDuration(m_Clip);
	m_Time += deltaSeconds * m_Rate;
	if (duration > 0.f)
	{
		if (m_Loop)
		{
			m_Time = fmodf(m_Time, duration);
			if (m_Time < 0.f)
			{
				m_Time += duration;
			}
		}
		else if (m_Time > duration)
		{
			m_Time = duration;
		}
	}

	if (m_Fade > 0.f)
	{
		m_Fade = (std::max)(0.f, m_Fade - deltaSeconds);
	}
}

void ModelPose::Compute(const ModelAsset& asset, std::vector<Mat4>& outJoints) const
{
	if (m_Fade > 0.f && m_FadeTotal > 0.f && m_PrevClip >= 0)
	{
		float weight = 1.f - m_Fade / m_FadeTotal;
		asset.ComputeJoints(m_PrevClip, m_PrevTime, m_Clip, m_Time, weight, outJoints);
	}
	else
	{
		asset.ComputeJoints(m_Clip, m_Time, -1, 0.f, 0.f, outJoints);
	}
}

// ------------------------------------------------------------- ModelLibrary

namespace
{
	Renderer* g_ModelRenderer = nullptr;
	std::map<std::string, std::unique_ptr<ModelAsset>> g_ModelCache; // 읽다 실패한 것도 nullptr로 넣어 다시 시도하지 않는다

	bool FileExists(const std::string& path)
	{
		std::ifstream file(path, std::ios::binary);
		return (bool)file;
	}
}

void ModelLibrary::SetRenderer(Renderer* renderer)
{
	g_ModelRenderer = renderer;
}

const std::string& ModelLibrary::GetResourceRoot()
{
	static std::string s_Root;
	static bool s_Searched = false;
	if (!s_Searched)
	{
		s_Searched = true;
		const char* kCandidates[] = { "../Resource/", "./Resource/", "../../Resource/" };
		for (const char* candidate : kCandidates)
		{
			if (FileExists(std::string(candidate) + "README.md"))
			{
				s_Root = candidate;
				break;
			}
		}

		if (s_Root.empty())
		{
			std::cout << "[모델] Resource 폴더를 찾지 못했습니다(작업 폴더 기준 ../Resource). 도형으로 그립니다.\n";
		}
		else
		{
			std::cout << "[모델] Resource 폴더: " << s_Root << "\n";
		}
	}
	return s_Root;
}

bool ModelLibrary::Exists(const std::string& relativePath)
{
	const std::string& root = GetResourceRoot();
	return !root.empty() && FileExists(root + relativePath);
}

const ModelAsset* ModelLibrary::Get(const std::string& relativePath, bool kit)
{
	std::map<std::string, std::unique_ptr<ModelAsset>>::iterator found = g_ModelCache.find(relativePath);
	if (found != g_ModelCache.end())
	{
		return found->second.get();
	}

	std::unique_ptr<ModelAsset>& slot = g_ModelCache[relativePath];
	const std::string& root = GetResourceRoot();
	if (root.empty() || g_ModelRenderer == nullptr || !FileExists(root + relativePath))
	{
		return nullptr;
	}

	std::unique_ptr<ModelAsset> asset(new ModelAsset());
	if (asset->Load(root + relativePath, *g_ModelRenderer, kit))
	{
		slot = std::move(asset);
	}
	return slot.get();
}

void ModelLibrary::Shutdown()
{
	g_ModelCache.clear();
}
