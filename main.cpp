#include <windows.h>
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include "Renderer.h"

const int frameWidth = 100 * 16;
const int frameHeight = 100 * 9;

HINSTANCE hInst;     // 当前实例

// 此代码模块中包含的函数的前向声明:
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR    lpCmdLine,
	_In_ int       nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	hInst = hInstance; // 将实例句柄存储在全局变量中

	//注册主窗口
	WNDCLASS wc = { 0 };
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wc.hCursor = NULL;
	wc.hIcon = NULL;
	wc.hInstance = hInstance;
	wc.lpfnWndProc = WndProc;
	wc.lpszClassName = L"Main";
	wc.lpszMenuName = NULL;
	wc.style = CS_HREDRAW | CS_VREDRAW;
	RegisterClass(&wc);

	HWND hWndMain = CreateWindowEx(
		0,
		L"Main",
		L"Win32 RenderToy",
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		frameWidth,
		frameHeight + 30,
		nullptr,
		nullptr,
		hInstance,
		nullptr);

	ShowWindow(hWndMain, nCmdShow);
	UpdateWindow(hWndMain);

	Camera camera(Object({ 0,0,2 }, { 0,0,-1 }, { 0,1,0 }, 0, 0.01, 0.02));

	Model model(Object({0,0,0}, { 0,0,-1 }, { 0,1,0 }, Actions::turnLeft, 0, 0.0015),
		Matirial({ 0.005, 0.005, 0.005 }, { 0.8, 0.86, 0.88 }, { 0.2, 0.2, 0.2 }));
	model.loadOBJ(L"models", L"dragon.obj");

	std::vector<Light> light;
	light.push_back({ {0,30,30},{500,500,500} });
	light.push_back({ {30,30,30},{1000,1000,1000} });

	Math::vec3 amb_light{ 10,10,10 };

	Setting setting;

	Canvas canvas(frameWidth, frameHeight, hWndMain, { 0.08,0,0.07 }, { 0.6,0.6,0.6 });

	Renderer renderer;

	Timer timer;

	int fps = 0, frameCnt = 0;
	bool showInfo = false;

	// 主消息循环:
	MSG msg = {};
	while (msg.message != WM_QUIT) { 

		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
			DispatchMessage(&msg);

			bool keyup = msg.message == WM_KEYUP;
			if (msg.message == WM_KEYDOWN || msg.message == WM_KEYUP) {
				if (msg.wParam == 'W') camera.setState(keyup, Actions::moveForward);
				else if (msg.wParam == 'A') camera.setState(keyup, Actions::moveLeft);
				else if (msg.wParam == 'S') camera.setState(keyup, Actions::moveBack);
				else if (msg.wParam == 'D') camera.setState(keyup, Actions::moveRight);
				else if (msg.wParam == VK_SPACE) camera.setState(keyup, Actions::moveUp);
				else if (msg.wParam == VK_SHIFT) camera.setState(keyup, Actions::moveDown);
				else if (msg.wParam == VK_UP) camera.setState(keyup, Actions::turnUp);
				else if (msg.wParam == VK_LEFT) camera.setState(keyup, Actions::turnLeft);
				else if (msg.wParam == VK_DOWN) camera.setState(keyup, Actions::turnDown);
				else if (msg.wParam == VK_RIGHT) camera.setState(keyup, Actions::turnRight);
				else if (msg.wParam == '1' && !keyup) setting.mod = Setting::Mod::PhongShading;
				else if (msg.wParam == '2' && !keyup) setting.mod = Setting::Mod::zColoring;
				else if (msg.wParam == '3' && !keyup) setting.mod = Setting::Mod::framework;
				else if (msg.wParam == 'B' && !keyup) setting.backfaceCulling = !setting.backfaceCulling;
				else if (msg.wParam == 'F' && !keyup) showInfo = !showInfo;
			}
		}
		
		//1.更新相机和物体姿态
		camera.updateAtiitude();
		model.updateAtiitude();
		
		//2.绘制到缓冲
		renderer.draw(canvas, camera, setting, model, light, amb_light);

		//3.计算帧率
		frameCnt++;
		if (timer.second()) {
			fps = frameCnt;
			frameCnt = 0;
		}

		//4.绘制debug信息
		std::wstring debug;
		//= //L"\nfps: " + std::to_wstring(fps) +L"\n" + 
		//	L"[ F ] for more info";
		if (showInfo) {
			 debug += 
				canvas.debugInfo() +
				renderer.debugInfo() +
				setting.debugInfo() +
				camera.debugInfo() +
				model.debugInfo();
		}
		canvas.drawDebugInfo(debug);

		//5.更新缓冲
		canvas.update();
	}

	return (int)msg.wParam;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
	if (message == WM_DESTROY) PostQuitMessage(0);
	return DefWindowProc(hWnd, message, wParam, lParam);
}