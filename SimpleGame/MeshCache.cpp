#include "stdafx.h"
#include "MeshCache.h"

#include <cstdint>
#include <direct.h>
#include <fstream>
#include <iostream>

namespace
{
	std::string CachePath(const std::string& key)
	{
		return "./Cache/" + key + ".mesh";
	}

	bool LoadFromFile(const std::string& path, MeshData* outMesh)
	{
		std::ifstream file(path, std::ios::binary);
		if (!file.is_open())
		{
			return false;
		}

		uint32_t primitiveType = 0;
		uint32_t vertexFloatCount = 0;
		file.read(reinterpret_cast<char*>(&primitiveType), sizeof(primitiveType));
		file.read(reinterpret_cast<char*>(&vertexFloatCount), sizeof(vertexFloatCount));
		if (!file)
		{
			return false;
		}

		outMesh->primitiveType = (GLenum)primitiveType;
		outMesh->vertices.resize(vertexFloatCount);
		if (vertexFloatCount > 0)
		{
			file.read(reinterpret_cast<char*>(outMesh->vertices.data()), vertexFloatCount * sizeof(float));
		}

		return (bool)file;
	}

	void SaveToFile(const std::string& path, const MeshData& mesh)
	{
		// Cache 폴더가 없을 수 있으니 미리 만들어둔다 (이미 있으면 -1을
		// 반환하지만 그건 정상 상황이라 무시해도 된다).
		_mkdir("./Cache");

		std::ofstream file(path, std::ios::binary);
		if (!file.is_open())
		{
			std::cout << "[모델 캐시] " << path << " 파일을 쓰지 못했습니다.\n";
			return;
		}

		uint32_t primitiveType = (uint32_t)mesh.primitiveType;
		uint32_t vertexFloatCount = (uint32_t)mesh.vertices.size();
		file.write(reinterpret_cast<const char*>(&primitiveType), sizeof(primitiveType));
		file.write(reinterpret_cast<const char*>(&vertexFloatCount), sizeof(vertexFloatCount));
		if (vertexFloatCount > 0)
		{
			file.write(reinterpret_cast<const char*>(mesh.vertices.data()), vertexFloatCount * sizeof(float));
		}
	}
}

MeshData MeshCache::GetOrCreate(const std::string& key, const std::function<MeshData()>& generator)
{
	std::string path = CachePath(key);

	MeshData mesh;
	if (LoadFromFile(path, &mesh))
	{
		std::cout << "[모델 캐시] " << key << " 을(를) 캐시에서 불러왔습니다.\n";
		return mesh;
	}

	mesh = generator();
	SaveToFile(path, mesh);
	std::cout << "[모델 캐시] " << key << " 을(를) 새로 생성해 캐시에 저장했습니다.\n";
	return mesh;
}
