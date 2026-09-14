#include "stdafx.h"
#include "TileMap.h"

#include <cmath>

TileMap::TileMap(int width, int height)
	: m_Width(width)
	, m_Height(height)
	, m_Tiles(width * height, TileType::Grass)
{
}

TileType TileMap::GetTile(int gridX, int gridY) const
{
	if (gridX < 0 || gridX >= m_Width || gridY < 0 || gridY >= m_Height)
	{
		return TileType::Grass;
	}
	return m_Tiles[gridY * m_Width + gridX];
}

void TileMap::SetTile(int gridX, int gridY, TileType type)
{
	if (gridX < 0 || gridX >= m_Width || gridY < 0 || gridY >= m_Height)
	{
		return;
	}
	m_Tiles[gridY * m_Width + gridX] = type;
}

bool TileMap::IsWalkable(int gridX, int gridY) const
{
	if (gridX < 0 || gridX >= m_Width || gridY < 0 || gridY >= m_Height)
	{
		return false;
	}
	return GetTile(gridX, gridY) != TileType::Water;
}

bool TileMap::IsWorldPositionWalkable(float worldX, float worldY) const
{
	return IsWalkable(GetGridX(worldX), GetGridY(worldY));
}

float TileMap::GetWorldX(int gridX) const
{
	return gridX - m_Width * 0.5f + 0.5f;
}

float TileMap::GetWorldY(int gridY) const
{
	return gridY - m_Height * 0.5f + 0.5f;
}

int TileMap::GetGridX(float worldX) const
{
	return (int)floorf(worldX + m_Width * 0.5f);
}

int TileMap::GetGridY(float worldY) const
{
	return (int)floorf(worldY + m_Height * 0.5f);
}
