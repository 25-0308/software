#include "stdafx.h"
#include "TextRasterizer.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <cstring>
#include <vector>

#include "Dependencies\glew.h"

namespace
{
	// 글자 영역 둘레의 여백(픽셀). 채팅 줄 배경이 글자에 딱 붙어 보이지 않게 한다.
	const int kPaddingX = 6;
	const int kPaddingY = 3;

	std::wstring ToWide(const std::string& utf8)
	{
		if (utf8.empty())
		{
			return std::wstring();
		}

		int length = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), nullptr, 0);
		if (length <= 0)
		{
			return std::wstring();
		}

		std::wstring wide((size_t)length, L'\0');
		MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), &wide[0], length);
		return wide;
	}
}

TextRasterizer::TextRasterizer(int fontPixelHeight)
{
	HDC screenDc = GetDC(nullptr);
	HDC memoryDc = CreateCompatibleDC(screenDc);
	ReleaseDC(nullptr, screenDc);

	// 한글이 들어 있는 맑은 고딕. 없는 시스템에서는 GDI가 비슷한 글꼴로 대신한다.
	// ANTIALIASED_QUALITY: 색 번짐 없는 회색조 안티앨리어싱(알파로 쓰기 좋다).
	HFONT font = CreateFontW(-fontPixelHeight, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
		DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
		DEFAULT_PITCH | FF_DONTCARE, L"Malgun Gothic");

	m_Dc = memoryDc;
	m_Font = font;
}

TextRasterizer::~TextRasterizer()
{
	if (m_Font != nullptr)
	{
		DeleteObject((HFONT)m_Font);
	}
	if (m_Dc != nullptr)
	{
		DeleteDC((HDC)m_Dc);
	}
}

TextTexture TextRasterizer::Create(const std::string& utf8Text, int maxWidth) const
{
	TextTexture result;

	std::wstring text = ToWide(utf8Text);
	if (text.empty() || m_Dc == nullptr || m_Font == nullptr)
	{
		return result;
	}

	HDC dc = (HDC)m_Dc;
	HFONT oldFont = (HFONT)SelectObject(dc, (HFONT)m_Font);

	// 1) 줄바꿈까지 반영한 글자 영역 크기를 먼저 잰다.
	RECT measure = { 0, 0, maxWidth - kPaddingX * 2, 0 };
	DrawTextW(dc, text.c_str(), (int)text.size(), &measure, DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX);

	int textWidth = measure.right - measure.left;
	int textHeight = measure.bottom - measure.top;
	if (textWidth <= 0 || textHeight <= 0)
	{
		SelectObject(dc, oldFont);
		return result;
	}

	int width = textWidth + kPaddingX * 2;
	int height = textHeight + kPaddingY * 2;

	// 2) 32비트 DIB(위에서 아래 순서라 biHeight가 음수)를 만들어 검은 바탕에 흰 글자를 그린다.
	BITMAPINFO info = {};
	info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	info.bmiHeader.biWidth = width;
	info.bmiHeader.biHeight = -height;
	info.bmiHeader.biPlanes = 1;
	info.bmiHeader.biBitCount = 32;
	info.bmiHeader.biCompression = BI_RGB;

	void* bits = nullptr;
	HBITMAP dib = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
	if (dib == nullptr || bits == nullptr)
	{
		SelectObject(dc, oldFont);
		return result;
	}

	HBITMAP oldBitmap = (HBITMAP)SelectObject(dc, dib);
	memset(bits, 0, (size_t)width * (size_t)height * 4);

	SetBkMode(dc, TRANSPARENT);
	SetTextColor(dc, RGB(255, 255, 255));

	RECT drawRect = { kPaddingX, kPaddingY, kPaddingX + textWidth, kPaddingY + textHeight };
	DrawTextW(dc, text.c_str(), (int)text.size(), &drawRect, DT_WORDBREAK | DT_NOPREFIX);
	GdiFlush();

	// 3) GDI가 그린 결과(BGRA, 흰 글자라 밝기 = 커버리지)를 RGBA로 바꾼다: 색은 흰색,
	//    알파에 커버리지를 넣는다. 실제 글자 색은 그릴 때 유니폼으로 입힌다.
	size_t pixelCount = (size_t)width * (size_t)height;
	std::vector<unsigned char> pixels(pixelCount * 4);
	const unsigned char* source = reinterpret_cast<const unsigned char*>(bits);
	for (size_t i = 0; i < pixelCount; ++i)
	{
		pixels[i * 4 + 0] = 255;
		pixels[i * 4 + 1] = 255;
		pixels[i * 4 + 2] = 255;
		pixels[i * 4 + 3] = source[i * 4 + 1]; // 초록 채널
	}

	SelectObject(dc, oldBitmap);
	DeleteObject(dib);
	SelectObject(dc, oldFont);

	// 4) OpenGL 텍스처로 올린다. 밉맵을 만들지 않으므로 MIN 필터를 밉맵이 필요 없는 값으로
	//    꼭 바꿔야 한다(기본값 그대로면 텍스처가 불완전해서 검게 나온다).
	GLuint id = 0;
	glGenTextures(1, &id);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, id);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	result.id = id;
	result.width = width;
	result.height = height;
	return result;
}

void TextRasterizer::Destroy(TextTexture& texture)
{
	if (texture.id != 0)
	{
		GLuint id = texture.id;
		glDeleteTextures(1, &id);
	}

	texture = TextTexture();
}
