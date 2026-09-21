#pragma once

#include <string>

// 글자 한 덩어리를 그려 넣은 OpenGL 텍스처. 알파 채널이 글자가 칠해진 정도(커버리지)다.
struct TextTexture
{
	unsigned int id = 0; // OpenGL 텍스처 이름(GLuint). 0이면 없음
	int width = 0;       // 픽셀
	int height = 0;
};

// Windows GDI로 글자를 비트맵에 그려서 OpenGL 텍스처로 만든다. 이 프로젝트엔 한글을 그릴 수 있는
// 폰트 렌더링이 없는데, GDI는 시스템에 설치된 글꼴(한글 포함)로 안티앨리어싱된 글자를 그려주므로
// 한글 대사를 그대로 화면에 보여줄 수 있다. 문장 하나당 텍스처 하나를 만들어 쓰고 지우는 방식이라
// 채팅창처럼 짧게 떴다 사라지는 글자에 알맞다.
class TextRasterizer
{
public:
	explicit TextRasterizer(int fontPixelHeight);
	~TextRasterizer();

	TextRasterizer(const TextRasterizer&) = delete;
	TextRasterizer& operator=(const TextRasterizer&) = delete;

	// UTF-8 문장을 maxWidth 픽셀 안에서 (공백 기준으로) 줄바꿈하며 그린 텍스처를 만든다.
	// 텍스처 크기는 글자 영역 + 여백이다. 실패하면 id가 0인 텍스처를 돌려준다.
	// OpenGL 컨텍스트가 현재 상태여야 한다.
	TextTexture Create(const std::string& utf8Text, int maxWidth) const;

	static void Destroy(TextTexture& texture);

private:
	// Windows 핸들(HDC/HFONT). windows.h를 헤더에 끌어들이지 않으려고 void*로 들고 있다.
	void* m_Dc = nullptr;
	void* m_Font = nullptr;
};
