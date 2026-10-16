#pragma once

#include <cstdint>
#include <map>
#include <string>

#include "TextRasterizer.h"

// 대화창에 띄우는 인물 초상화(신·주요 인물). 이 프로젝트엔 이미지 에셋이 없어서, 인물마다 정해 둔 생김새(머리·수염·눈·
// 옷 색, 관·투구·두건, 손에 든 물건, 배경 무늬 — 게임 속 모습(WorldMaps.cpp의 *Look)과 같은 특징)를 Windows GDI+로
// 안티앨리어싱해서 그린 흉상을 처음 필요할 때 한 번 만들어 OpenGL 텍스처로 들고 있는다(Portrait.cpp의 표).
// 작업 폴더 기준 ./Portraits/<화자ID>.png가 있으면 그 그림을 대신 쓴다(나중에 진짜 그림으로 바꿀 수 있게 — 표에 없는
// 화자도 파일만 넣으면 초상화가 생긴다). 텍스처는 화면 크기(kWidth x kHeight)의 두 배 해상도로 그려서 창을 키워도
// 또렷하다. 글자 텍스처와 같은 TextTexture로 돌려주므로 UiDraw로 그린다(셰이더가 텍스처 색 × 유니폼 색).
class PortraitLibrary
{
public:
	// 화면(800x600 UI 픽셀)에 그리는 크기. 텍스처는 그 두 배(384x476) 해상도다.
	static const int kWidth = 192;
	static const int kHeight = 238;

	PortraitLibrary();
	~PortraitLibrary();

	PortraitLibrary(const PortraitLibrary&) = delete;
	PortraitLibrary& operator=(const PortraitLibrary&) = delete;

	// speakerId(Data/Story.txt의 화자 ID)에게 초상화가 있는지 — 그림 표에 있거나 ./Portraits/<ID>.png가 있다.
	// 그리지는 않으므로 대화를 열 때 부담 없이 물어볼 수 있다.
	bool Has(const std::string& speakerId);

	// 초상화 텍스처. 처음 부를 때 그려서(또는 파일을 읽어서) 만들고, 없거나 실패하면 nullptr.
	// OpenGL 컨텍스트가 현재 상태여야 한다.
	const TextTexture* Get(const std::string& speakerId);

private:
	bool StartGdiplus();

	std::map<std::string, bool> m_Has;
	std::map<std::string, TextTexture> m_Textures; // 만들다 실패한 것도 id 0으로 넣어 둬서 다시 시도하지 않는다
	std::uintptr_t m_GdiplusToken = 0;             // ULONG_PTR(헤더에 GDI+를 끌어들이지 않으려고 정수로 든다)
	bool m_GdiplusStarted = false;
	bool m_GdiplusFailed = false;
};
