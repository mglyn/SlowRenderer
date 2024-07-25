#pragma once
#include "Math.h"
#include <string>

class Canvas {
	friend class Renderer;

	const Setting& set;

	unsigned int* buffer;
	char* cBuffer;

	ThreadPool threads;

	int brightness = 1;
	char brush[3][32] = {
			{7, ' ','.',':','+','%','#','@'},
			{18, ' ','.',',','^',':','-','+','a','b','c','d','w','f','$','&','%','#','@'},
			{3, ' ','.',':'}
	};

public:

	Canvas(const Setting& setting) :set(setting), threads(setting.numCalculatingThreads) {
		buffer = new unsigned int[set.width * set.height](0);
		cBuffer = new char[(set.width / set.cWidth + 1) * set.height / set.cHeight + 1]('\0');
	}

	~Canvas() {
		delete[] buffer;
		delete[] cBuffer;
	}

	void drawPixel(int pid, const Math::vec3& color) {
		buffer[pid] = RGB(color[0] * 255, color[1] * 255, color[2] * 255);
	}

	bool Cohen_Sutherland(float& x0, float& y0, float& x1, float& y1) const {
		const int LEFT = 1, RIGHT = 2, BOTTOM = 4, TOP = 8;
		const float ymax = set.height - 1.f, xmax = set.width - 1.f;
		const float ymin = 0.f, xmin = 0.f;
		auto encode = [this](float x, float y)->int {
			int code = 0;
			if (x < 0)code |= LEFT;
			else if (x > set.width - 1.f)code |= RIGHT;
			if (y < 0)code |= BOTTOM;
			else if (y > set.height - 1.f)code |= TOP;
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
		x0 = (std::min)((std::max)(0, x0), set.width - 1);
		x1 = (std::min)((std::max)(0, x1), set.width - 1);
		y0 = (std::min)((std::max)(0, y0), set.height - 1);
		y1 = (std::min)((std::max)(0, y1), set.height - 1);
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
			if (steep)drawPixel(x * set.width + y, { 100,100,100 });
			else drawPixel(y * set.width + x, { 100,100,100 });
			error -= dy;
			if (error < 0) {
				y += ystep;
				error += dx;
			}
		}
	}

	void drawTriangleFrame(const Triangle& t) {
		auto st0 = t.ver[0].sPos, st1 = t.ver[1].sPos, st2 = t.ver[2].sPos;
		auto ed0 = t.ver[1].sPos, ed1 = t.ver[2].sPos, ed2 = t.ver[0].sPos;
		if (Cohen_Sutherland(st0[0], st0[1], ed0[0], ed0[1]))
			Bresenham(st0[0], st0[1], ed0[0], ed0[1]);
		if (Cohen_Sutherland(st1[0], st1[1], ed1[0], ed1[1]))
			Bresenham(st1[0], st1[1], ed1[0], ed1[1]);
		if (Cohen_Sutherland(st2[0], st2[1], ed2[0], ed2[1]))
			Bresenham(st2[0], st2[1], ed2[0], ed2[1]);
	}

	void blend() {
		auto blendTask = [&](int st, int ed) {
			for (int cy = st; cy < ed; cy++) {
				for (int cx = 0; cx < set.width / set.cWidth; cx++) {
					
					float val = 0;
					for (int i = 0; i < set.cHeight; i++) {
						for (int j = 0; j < set.cWidth; j++) {

							int y = cy * set.cHeight + i, x = cx * set.cWidth + j;
							int pid = (set.height - (y + 1)) * set.width + x;
							val += (GetRValue(buffer[pid]) + GetGValue(buffer[pid]) + GetBValue(buffer[pid]));
						}
					}

					int level = val * brush[brightness][0] / 3.f / 256 / set.cHeight / set.cWidth;
					cBuffer[cy * (set.width / set.cWidth + 1) + cx] = brush[brightness][level + 1];
				}
				cBuffer[cy * (set.width / set.cWidth + 1) + set.width / set.cWidth] = '\n';
			}
			};

		int num = set.height / set.cHeight;
		int blockSize = std::clamp(num / (8 * set.numCalculatingThreads), 32 / set.cHeight, 512 / set.cHeight);

		for (int i = 0; i < num; i += blockSize) {
			threads.addTask(blendTask, i, (std::min)(i + blockSize, num));
		}

		threads.barrier();
	}

	void blendDebugInfo(const std::string& str) {

		int line = 0;
		char* p = cBuffer + line++ * (set.width / set.cWidth + 1);
		for (const char& c : str) {
			if (c != '\n') *p++ = c;
			else p = cBuffer + line++ * (set.width / set.cWidth + 1);
			if (line >= set.height / set.cHeight)break;
		}
	}

	void display() {
		COORD coord = { 0, 0 };
		SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), coord);
		fputs(cBuffer, stdout);
	}
};//178 x 50