#pragma once

#include <vector>

enum class TileType
{
	Grass,
	Stone,
	Water,
	Path, // 마을과 호수를 잇는 다져진 길
};

// 월드 원점을 중심으로 한 고정 크기 타일 그리드.
// 레벨이 작아서 맵 전체가 항상 메모리에 상주하며, 아직 청크 단위 스트리밍은
// 없다 (추후 과제).
class TileMap
{
public:
	TileMap(int width, int height);

	TileType GetTile(int gridX, int gridY) const;
	void SetTile(int gridX, int gridY, TileType type);

	// Water 타일은 걸을 수 없는 벽으로 취급하고, 그 외에는 모두 통행 가능하다.
	// 그리드 범위를 벗어나면 통행 불가로 취급한다 (맵 경계).
	bool IsWalkable(int gridX, int gridY) const;
	bool IsWorldPositionWalkable(float worldX, float worldY) const;

	int GetWidth() const { return m_Width; }
	int GetHeight() const { return m_Height; }

	// 타일 중심의 월드 좌표 (그리드는 원점을 중심으로 배치됨).
	float GetWorldX(int gridX) const;
	float GetWorldY(int gridY) const;

	// GetWorldX/GetWorldY의 역변환: 월드 좌표가 속한 타일의 그리드 좌표.
	int GetGridX(float worldX) const;
	int GetGridY(float worldY) const;

private:
	int m_Width;
	int m_Height;
	std::vector<TileType> m_Tiles;
};
