#include <windows.h>
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>
#include <cassert>
#include "Renderer.h"

int main() {

	HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
	DWORD mode;
	GetConsoleMode(hStdin, &mode);
	mode &= ~ENABLE_QUICK_EDIT_MODE;
	mode &= ~ENABLE_INSERT_MODE;
	mode &= ~ENABLE_MOUSE_INPUT;
	SetConsoleMode(hStdin, mode);
	CONSOLE_CURSOR_INFO cur = { 1,0 };
	SetConsoleCursorInfo(GetStdHandle(STD_OUTPUT_HANDLE), &cur);

	Setting setting;
	setting.backfaceCulling = true;
	setting.bgColor = { 0.08,0,0.07 };
	setting.textColor = { 0.6,0.6,0.6 };
	setting.mod = Setting::Mod::PhongShading;

	setting.width = 280 * 4;
	setting.height = 156 * 4;
	setting.cWidth = 4;
	setting.cHeight = 8;

	assert(setting.height % setting.cHeight == 0 && setting.width % setting.cWidth == 0);

	setting.fov = 0.4 * Math::pi;
	setting.aspect = (float)setting.width / setting.height;
	setting.zFar = -50.f;
	setting.zNear = -0.1f;

	setting.numCalculatingThreads = 16;

	setting.showInfo = false;

	Camera camera(Object(
		{ 0,0,2 },   //world coord
		{ 0,0,-1 },  //towarding 
		{ 0,1,0 },   //up
		0,			 //actions
		0.01,  		 //speed
		0.01),		 //rotation speed
		setting);   	 

	Model model(Object({ 0,0,0 }, { 0,0,-1 }, { 0,1,0 }, Actions::turnLeft, 0, 0.005),
		Matirial(
			{ 0.005, 0.005, 0.005 },  //kambient
			{ 0.8, 0.86, 0.88 },      //kdiffuse
			{ 0.2, 0.2, 0.2 }));      //kspecular

	model.loadOBJ("models", "dragon.obj");

	std::vector<Light> light;   //point light source
	light.push_back({
		{0,20,0}, 				//world coord
	{400,400,400} });			//rgb intensity
	light.push_back({ {20,0,20},{100,100,100} });

	Math::vec3 amb_light{ 5,5,5 }; //ambient light intensity

	Canvas canvas(setting);

	Renderer renderer(setting);
	
	bool shutdown = false;

	// rendering loop
	std::jthread renderThread(&Renderer::renderingLoop, &renderer, std::ref(canvas), 
		std::ref(camera), std::ref(model), std::ref(light), std::ref(amb_light), std::ref(shutdown));

	// msg loop
	std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
	while (true) {
		camera.setState(!(GetKeyState('W') & 0x8000), Actions::moveForward);
		camera.setState(!(GetKeyState('A') & 0x8000), Actions::moveLeft);
		camera.setState(!(GetKeyState('S') & 0x8000), Actions::moveBack);
		camera.setState(!(GetKeyState('D') & 0x8000), Actions::moveRight);
		camera.setState(!(GetKeyState(VK_SPACE) & 0x8000), Actions::moveUp);
		camera.setState(!(GetKeyState(VK_SHIFT) & 0x8000), Actions::moveDown);
		camera.setState(!(GetKeyState(VK_UP) & 0x8000), Actions::turnUp);
		camera.setState(!(GetKeyState(VK_LEFT) & 0x8000), Actions::turnLeft);
		camera.setState(!(GetKeyState(VK_DOWN) & 0x8000), Actions::turnDown);
		camera.setState(!(GetKeyState(VK_RIGHT) & 0x8000), Actions::turnRight);

		if (GetKeyState('1') & 0x8000 || GetKeyState('2') & 0x8000 
			|| GetKeyState('3') & 0x8000 || GetKeyState('F') & 0x8000 
			|| GetKeyState('B') & 0x8000)
		{
			shutdown = true;
			renderThread.join();
			shutdown = false;

			if (GetKeyState('1') & 0x8000)
				setting.mod = Setting::Mod::PhongShading;
			if (GetKeyState('2') & 0x8000)
				setting.mod = Setting::Mod::zColoring;
			if (GetKeyState('3') & 0x8000)
				setting.mod = Setting::Mod::framework;

			if (GetKeyState('B') & 0x8000)
				setting.backfaceCulling = GetKeyState('B') & 1;

			if (GetKeyState('F') & 0x8000)
				setting.showInfo = GetKeyState('F') & 1;

			renderThread = std::jthread(&Renderer::renderingLoop, &renderer, std::ref(canvas),
				std::ref(camera), std::ref(model), std::ref(light), std::ref(amb_light), std::ref(shutdown));
		}

		std::chrono::duration<double, std::milli> delta = std::chrono::system_clock::now() - now;
		now = std::chrono::system_clock::now();
		if (delta.count() < 6.944) {  //144 logic fps
			std::chrono::duration<double, std::milli> delta_ms(6.944 - delta.count());
			auto delta_ms_duration = std::chrono::duration_cast<std::chrono::milliseconds>(delta_ms);
			std::this_thread::sleep_for(std::chrono::milliseconds(delta_ms_duration.count()));
		}

		camera.updateAtiitude();
		model.updateAtiitude();
	}
}
