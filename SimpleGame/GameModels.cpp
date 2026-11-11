#include "stdafx.h"
#include "GameModels.h"

#include <cmath>
#include <cstring>
#include <iostream>
#include <map>
#include <string>
#include <vector>

// 바닥 텍스처(PNG)는 GDI+로 읽는다(Portrait.cpp와 같은 방식 — GDI+ 헤더가 min/max를 Gdiplus 이름공간에서 찾게).
#include <Windows.h>
#include <objidl.h>
#include <algorithm>
namespace Gdiplus
{
	using std::min;
	using std::max;
}
#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")

#include "CharacterActors.h"
#include "Renderer.h"

namespace
{
	bool g_Enabled = true;

	const char* kPresetFolder = "Characters/Humanoid/Presets/";
	const char* kSpeciesFolder = "Creatures/Quadruped/Species/";
	const char* kNatureKit = "Nature/Nature_Kit.glb";
	const char* kBuildingsKit = "Buildings/Buildings_Kit.glb";
	const char* kGoodsKit = "Buildings/Goods_Kit.glb";
	const char* kPrefabFolder = "Buildings/Prefabs/";

	GLuint g_GroundTextures[Renderer::kGroundTextureCount] = {};

	const ModelAsset* Preset(const std::string& id)
	{
		return ModelLibrary::Get(std::string(kPresetFolder) + id + ".glb");
	}

	// 이야기 ID에 프리셋이 없을 때 대신 쓸 사람(새 마을 테베의 주민 등).
	const char* FallbackPreset(const std::string& id)
	{
		if (id == "NPC_THEBES_CITIZEN") return "NPC_ATHENS_CITIZEN";
		if (id.compare(0, 4, "NPC_") == 0) return "NPC_DELPHI_VILLAGER";
		return "NPC_PILGRIM";
	}

	// 마을 주민의 짝 프리셋(같은 마을에 남녀가 섞여 보이게 — variant가 홀수면 이쪽).
	const char* AlternatePreset(const std::string& id)
	{
		if (id == "NPC_DELPHI_VILLAGER") return "NPC_PILGRIM";
		if (id == "NPC_THEBES_CITIZEN") return "NPC_PAPHOS_SERVANT";
		if (id == "NPC_SHEPHERD") return "NPC_DELPHI_VILLAGER";
		if (id == "NPC_ATHENS_CITIZEN") return "NPC_DELPHI_VILLAGER";
		if (id == "NPC_FISHER") return "NPC_CASTAWAY_C";
		if (id == "NPC_PAPHOS_SERVANT") return "NPC_FISHER";
		if (id == "NPC_UNDYING") return "NPC_UNDYING_ELDER";
		if (id == "NPC_WEST_CITIZEN") return "NPC_DELPHI_VILLAGER";
		return nullptr;
	}

	GameModels::Piece FindPiece(const char* file, const char* name, bool kit)
	{
		GameModels::Piece piece;
		piece.asset = ModelLibrary::Get(file, kit);
		if (piece.asset != nullptr)
		{
			piece.part = kit ? piece.asset->FindPart(name) : piece.asset->GetWhole();
			if (piece.part == nullptr && kit)
			{
				std::cout << "[모델] " << file << "에 조각 " << name << "이(가) 없습니다.\n";
			}
		}
		return piece;
	}

	// 같은 조각을 여러 번 찾지 않게 이름별로 기억해 둔다.
	const GameModels::Piece& CachedPiece(const char* file, const char* name, bool kit)
	{
		static std::map<std::string, GameModels::Piece> s_Cache;
		std::string key = std::string(file) + "#" + name;
		std::map<std::string, GameModels::Piece>::iterator found = s_Cache.find(key);
		if (found != s_Cache.end())
		{
			return found->second;
		}
		return s_Cache[key] = FindPiece(file, name, kit);
	}

	// PNG 하나를 이어 붙는(GL_REPEAT) 밉맵 텍스처로 올리고 평균 색(감마 공간 0~1)을 구한다.
	GLuint LoadTexture(const std::string& path, float outAverage[3])
	{
		std::wstring widePath(path.begin(), path.end()); // 경로는 ASCII(Resource/Nature/Textures/...)
		Gdiplus::Bitmap bitmap(widePath.c_str());
		if (bitmap.GetLastStatus() != Gdiplus::Ok || bitmap.GetWidth() == 0)
		{
			std::cout << "[모델] 텍스처를 읽지 못했습니다: " << path << "\n";
			return 0;
		}

		int width = (int)bitmap.GetWidth();
		int height = (int)bitmap.GetHeight();
		Gdiplus::Rect rect(0, 0, width, height);
		Gdiplus::BitmapData data;
		if (bitmap.LockBits(&rect, Gdiplus::ImageLockModeRead, PixelFormat32bppARGB, &data) != Gdiplus::Ok)
		{
			return 0;
		}

		// GDI+는 BGRA(아래위 그대로) — RGBA로 옮기며 평균을 낸다.
		std::vector<unsigned char> pixels((size_t)width * (size_t)height * 4);
		double sum[3] = { 0.0, 0.0, 0.0 };
		for (int y = 0; y < height; ++y)
		{
			const unsigned char* row = (const unsigned char*)data.Scan0 + (size_t)y * (size_t)data.Stride;
			for (int x = 0; x < width; ++x)
			{
				unsigned char* out = &pixels[((size_t)y * (size_t)width + (size_t)x) * 4];
				out[0] = row[x * 4 + 2];
				out[1] = row[x * 4 + 1];
				out[2] = row[x * 4 + 0];
				out[3] = 255;
				sum[0] += out[0];
				sum[1] += out[1];
				sum[2] += out[2];
			}
		}
		bitmap.UnlockBits(&data);

		double count = (double)width * (double)height * 255.0;
		for (int c = 0; c < 3; ++c)
		{
			outAverage[c] = (float)(sum[c] / count);
		}

		GLuint texture = 0;
		glGenTextures(1, &texture);
		glBindTexture(GL_TEXTURE_2D, texture);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
		glGenerateMipmap(GL_TEXTURE_2D);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glBindTexture(GL_TEXTURE_2D, 0);
		return texture;
	}
}

bool GameModels::IsEnabled()
{
	return g_Enabled && !ModelLibrary::GetResourceRoot().empty();
}

void GameModels::SetEnabled(bool enabled)
{
	g_Enabled = enabled;
}

const ModelAsset* GameModels::ForPerson(const char* storyId, const char* nameTag, unsigned int variant, bool& outRecolor)
{
	outRecolor = false;
	if (storyId == nullptr)
	{
		return nullptr;
	}

	std::string id = storyId;
	std::string tag = (nameTag != nullptr) ? nameTag : "";

	// 같은 이야기 ID를 이름표로 가려 쓰는 사람들.
	if (id == "CHR_ERIS" && tag == "Eris") id = "CHR_ERIS_TRUE";
	if (id == "CHR_MOIRAI" && tag == "Clotho") id = "CHR_CLOTHO";
	if (id == "CHR_MOIRAI" && tag == "Lachesis") id = "CHR_LACHESIS";
	if (id == "CHR_MOIRAI" && tag == "Atropos") id = "CHR_ATROPOS";

	// 이름 없는 사람(NPC_)은 사람마다 옷·머리 색을 바꿔 칠하고, 마을 주민은 남녀를 섞는다.
	if (id.compare(0, 4, "NPC_") == 0)
	{
		outRecolor = true;
		const char* alternate = AlternatePreset(id);
		if (alternate != nullptr && (variant % 2u) == 1u)
		{
			id = alternate;
		}
	}

	const ModelAsset* model = Preset(id);
	if (model == nullptr)
	{
		model = Preset(FallbackPreset(id));
	}
	return model;
}

const ModelAsset* GameModels::ForPlayer()
{
	return Preset("PLAYER");
}

const ModelAsset* GameModels::ForAnimal(AnimalKind kind, bool& outRecolor)
{
	outRecolor = false;
	switch (kind)
	{
	case AnimalKind::Deer: return ModelLibrary::Get(std::string(kSpeciesFolder) + "DEER.glb");
	case AnimalKind::Wolf: return ModelLibrary::Get(std::string(kSpeciesFolder) + "WOLF.glb");
	case AnimalKind::ShadowWolf: return ModelLibrary::Get(std::string(kSpeciesFolder) + "SHADOW_WOLF.glb");
	case AnimalKind::Boar: return ModelLibrary::Get(std::string(kSpeciesFolder) + "BOAR.glb");
	case AnimalKind::WhiteCow: return ModelLibrary::Get(std::string(kSpeciesFolder) + "CHR_IO.glb");
	case AnimalKind::BronzeWarrior: return Preset("MON_BRONZE_WARRIOR");
	case AnimalKind::KronosEcho: return Preset("CHR_KRONOS_ECHO");
	case AnimalKind::Bandit: outRecolor = true; return Preset("NPC_WEST_CITIZEN");
	case AnimalKind::Maenad: outRecolor = true; return Preset("NPC_PAPHOS_SERVANT");
	default: return nullptr; // 뱀(그림자 퓌톤·바다뱀)은 리소스에 없어 도형으로
	}
}

GameModels::Piece GameModels::NaturePiece(const char* name)
{
	return CachedPiece(kNatureKit, name, true);
}

GameModels::Piece GameModels::BuildingPiece(const char* name)
{
	return CachedPiece(kBuildingsKit, name, true);
}

GameModels::Piece GameModels::GoodsPiece(const char* name)
{
	return CachedPiece(kGoodsKit, name, true);
}

GameModels::Piece GameModels::Prefab(const char* name)
{
	std::string file = std::string(kPrefabFolder) + name + ".glb";
	return CachedPiece(file.c_str(), "", false);
}

void GameModels::DrawPiece(const RenderContext& ctx, const Piece& piece, float x, float y, float z, float yaw,
	float scaleX, float scaleY, float scaleZ, float tint, float alpha)
{
	if (!piece.IsValid())
	{
		return;
	}

	Renderer::ModelDrawParams params;
	params.model = Mat4::Translate(x, y, z) * Mat4::RotateZ(yaw) * Mat4::Scale(scaleX, scaleY, scaleZ);
	params.tintR = params.tintG = params.tintB = tint;
	params.alpha = alpha;

	// 깊이 범위: 기준점(보통 바닥 가운데)에서 조각의 가장 먼 모서리까지(넉넉하게 덩어리 전체의 대각선).
	float extentX = piece.ExtentX() * scaleX;
	float extentY = piece.ExtentY() * scaleY;
	float height = piece.Height() * scaleZ;
	params.depthRadius = sqrtf(extentX * extentX + extentY * extentY + height * height) + 0.2f;
	ctx.renderer.DrawModel(piece.asset->GetMesh(), piece.part->firstIndex, piece.part->indexCount, ctx.viewProjection, params);
}

unsigned int GameModels::HashSpot(float x, float y)
{
	int gx = (int)floorf(x * 8.f);
	int gy = (int)floorf(y * 8.f);
	unsigned int hash = (unsigned int)gx * 73856093u ^ (unsigned int)gy * 19349663u;
	hash ^= hash >> 13;
	hash *= 0x5bd1e995u;
	hash ^= hash >> 15;
	return hash;
}

void GameModels::LoadGroundTextures(Renderer& renderer)
{
	const std::string& root = ModelLibrary::GetResourceRoot();
	if (root.empty())
	{
		return;
	}

	Gdiplus::GdiplusStartupInput input;
	ULONG_PTR token = 0;
	if (Gdiplus::GdiplusStartup(&token, &input, nullptr) != Gdiplus::Ok)
	{
		return;
	}

	// Renderer::kGroundTextureCount 순서: 잔디·돌바닥·흙길·바위.
	const char* kFiles[Renderer::kGroundTextureCount] = { "T_Grass.png", "T_StonePaving.png", "T_Dirt.png", "T_Rock.png" };
	float averages[Renderer::kGroundTextureCount * 3] = {};
	bool all = true;
	for (int i = 0; i < Renderer::kGroundTextureCount; ++i)
	{
		g_GroundTextures[i] = LoadTexture(root + "Nature/Textures/" + kFiles[i], &averages[i * 3]);
		all = all && (g_GroundTextures[i] != 0);
	}

	Gdiplus::GdiplusShutdown(token);

	if (all)
	{
		renderer.SetGroundTextures(g_GroundTextures, averages);
		std::cout << "[모델] 바닥 텍스처 " << Renderer::kGroundTextureCount << "장을 읽었습니다.\n";
	}
}

void GameModels::Shutdown()
{
	for (GLuint& texture : g_GroundTextures)
	{
		if (texture != 0)
		{
			glDeleteTextures(1, &texture);
			texture = 0;
		}
	}
	ModelLibrary::Shutdown();
}
