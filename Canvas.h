#pragma once
#include "Math.h"
#include <windows.h>
#include <string>

class Canvas {
	friend class Renderer;
	//win32 draw
	HDC memDC = nullptr;
	HDC DC = nullptr;
	HBITMAP bitMap = nullptr;
	HGDIOBJ pen = nullptr;

	int width, height;
	Math::vec3 bgColor;
	Math::vec3 textColor;

	unsigned int* colorBuf;

public:

	Canvas(int w, int h, HWND hWnd, Math::vec3 bgColor, Math::vec3 textColor) : 
		width(w), height(h), bgColor(bgColor), textColor(textColor)
	{
		DC = GetDC(hWnd);
		memDC = CreateCompatibleDC(DC);
		pen = CreatePen(PS_SOLID, 1, 0);
		pen = SelectObject(DC, pen);
		SetBkColor(memDC, RGB(255 * bgColor[0], 255 * bgColor[1], 255 * bgColor[2]));
		SetTextColor(memDC, RGB(255 * textColor[0], 255 * textColor[1], 255 * textColor[2]));

		BITMAPINFO bi = { { sizeof(BITMAPINFOHEADER), w, h, 1, 32, BI_RGB,
			(DWORD)w * h * 4, 0, 0, 0, 0 } };
		bitMap = CreateDIBSection(memDC, &bi, DIB_RGB_COLORS, (void**)&colorBuf, 0, 0);

		if (bitMap)SelectObject(memDC, bitMap);
		else throw EXCEPTION_BREAKPOINT;
	}

	~Canvas() {
		DeleteDC(memDC);
		DeleteObject(bitMap);
		pen = SelectObject(DC, pen);
		DeleteObject(pen);
		DeleteDC(DC);
	}

	void drawPixel(int pid, const Math::vec3& color) {
		colorBuf[pid] = RGB(255 * color[0], 255 * color[1], 255 * color[2]);
	}

	bool Cohen_Sutherland(float& x0, float& y0, float& x1, float& y1) const {
		const int LEFT = 1, RIGHT = 2, BOTTOM = 4, TOP = 8;
		const float ymax = height - 1.f, xmax = width - 1.f;
		const float ymin = 0.f, xmin = 0.f;
		auto encode = [this](float x, float y)->int {
			int code = 0;
			if (x < 0)code |= LEFT;
			else if (x > width - 1.f)code |= RIGHT;
			if (y < 0)code |= BOTTOM;
			else if (y > height - 1.f)code |= TOP;
			return code;
			};

		int code0 = encode(x0, y0);
		int code1 = encode(x1, y1);
		bool accept = false;
		while (true) {
			if (!(code0 | code1)) {
				accept = true;
				break;
			}
			else if (code0 & code1) {
				break;
			}
			else {
				float x = 0.f, y = 0.f;
				int code = code1 > code0 ? code1 : code0;
				if (code & TOP) {
					x = x0 + (x1 - x0) * (ymax - y0) / (y1 - y0);
					y = ymax;
				}
				else if (code & BOTTOM) {
					x = x0 + (x1 - x0) * (ymin - y0) / (y1 - y0);
					y = ymin;
				}
				else if (code & RIGHT) {
					y = y0 + (y1 - y0) * (xmax - x0) / (x1 - x0);
					x = xmax;
				}
				else if (code & LEFT) {
					y = y0 + (y1 - y0) * (xmin - x0) / (x1 - x0);
					x = xmin;
				}

				if (code == code0) {
					x0 = x;
					y0 = y;
					code0 = encode(x0, y0);
				}
				else {
					x1 = x;
					y1 = y;
					code1 = encode(x1, y1);
				}
			}
		}
		return accept;
	}

	void Bresenham(int x0, int y0, int x1, int y1) {
		x0 = min(max(0, x0), width - 1);
		x1 = min(max(0, x1), width - 1);
		y0 = min(max(0, y0), height - 1);
		y1 = min(max(0, y1), height - 1);
		bool steep = abs(y1 - y0) > abs(x1 - x0);
		if (steep) {
			std::swap(x0, y0);
			std::swap(x1, y1);
		}
		if (x0 > x1) {
			std::swap(x0, x1);
			std::swap(y0, y1);
		}
		int dx = x1 - x0, dy = abs(y1 - y0);
		int error = dx / 2.f;
		int ystep = y0 < y1 ? 1 : -1;
		int y = y0;
		for (int x = x0; x <= x1; x++) {
			if (steep)drawPixel(x * width + y, { 100,100,100 });
			else drawPixel(y * width + x, { 100,100,100 });
			error -= dy;
			if (error < 0) {
				y += ystep;
				error += dx;
			}
		}
	}

	std::wstring debugInfo() {
		return L"\nresolution:" + std::to_wstring(width) + L"x" + std::to_wstring(height);
	}

	void drawDebugInfo(const std::wstring& str) {
		RECT rect;
		rect.left = 16;
		rect.top = 0;
		rect.right = 400;
		rect.bottom = 500;

		DrawTextW(memDC, str.c_str(), str.length(), &rect, DT_LEFT | DT_TOP);
	}

	void update() const {
		BitBlt(DC, 0, 0, width, height, memDC, 0, 0, SRCCOPY);
	}
};