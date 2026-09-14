#pragma once

#include <functional>
#include <string>
#include "Mesh.h"

// 절차적으로 생성한 모델(정점 데이터)을 파일로 캐싱한다. 처음 요청할 때만
// generator를 실행해 모델을 만들고 ./Cache/<key>.mesh 파일에 저장하며,
// 이후 같은 key로 다시 요청하면 (다음 실행 포함) 파일에서 바로 읽어온다.
namespace MeshCache
{
	MeshData GetOrCreate(const std::string& key, const std::function<MeshData()>& generator);
}
