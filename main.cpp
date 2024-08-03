#include <windows.h>
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>
#include <cassert>
#include "Renderer.h"

struct Setting {
	int width;
	int height;
	int cWidth;
	int cHeight;

	float fov;
	float aspect;
	float zNear;
	float zFar;

	bool showInfo;

	int numCalculatingThreads;

	Mode mode;
	bool backfaceCulling;

	std::string debugInfo() const {
		char str[512];
		sprintf(str,
			R"(
resolution: %dx%d
super sampling: %dx%d

threads: %d

backface culling: %s [ B ]
color mod: %s [ 1/2/3 ]

camera attributes:
FOV: %.2f
aspect: %.2f
zNear: %.2f
zFar: %.2f
)",
width / cWidth, height / cHeight,
width, height,
numCalculatingThreads,
backfaceCulling ? "enabled" : "disabled",
[this]()->const char* {
				if (mode == Mode::PhongShading)
					return { "1.Blinn-Phong shading" };
				else if (mode == Mode::zColoring)
					return { "2.depth" };
				else if (mode == Mode::framework)
					return { "3.framework" };
				return {};
			}(), fov, aspect, zNear, zFar);

		return std::string(str);
	}
};

void renderingLoop(Canvas& canvas,
	const Camera& camera,
	const Model& model,
	const std::vector<Light>& light,
	const Math::vec3& amb_light,
	const Setting& set,
	bool& update,
	const bool& shutdown)
{
	Renderer renderer(canvas,
		set.numCalculatingThreads,
		set.backfaceCulling,
		set.mode);

	int frameCnt = 0, fps = 0;
	std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
	while (!shutdown) {

		/*while (!update && !shutdown) {
			std::mutex mtx;
			mtx.lock();
			mtx.unlock();
		}
		update = false;*/

		//1.
		renderer.draw(camera, model, light, amb_light);

		//2.calculate fps
		frameCnt++;
		std::chrono::duration<double, std::milli> delta = std::chrono::system_clock::now() - now;
		if (delta.count() > 500) {
			now = std::chrono::system_clock::now();
			fps = 2 * frameCnt;
			frameCnt = 0;
		}

		//3.blend pixels to char
		canvas.blend();

		//4.debug info
		std::string debug
			= "\nfps: " + std::to_string(fps) + "\n" +
			"[ F ] hide\n" + 
			"[ W/A/S/D/DIR ] move camera";
		if (set.showInfo) {
			debug +=
				set.debugInfo() +
				camera.debugInfo() +
				model.debugInfo();
		}
		canvas.blendDebugInfo(debug);

		//5.show buffer to screen
		canvas.display();
	}
}


int main() {

	Setting set;
	set.backfaceCulling = true;
	set.mode = Mode::PhongShading;

	set.width = 178 * 4;
	set.height = 100 * 4;
	set.cWidth = 4;
	set.cHeight = 8;

	assert(set.height % set.cHeight == 0 && set.width % set.cWidth == 0);

	set.fov = 0.4 * Math::pi;
	set.aspect = (float)set.width / set.height;
	set.zFar = -50.f;
	set.zNear = -0.1f;

	set.numCalculatingThreads = std::clamp((int)std::thread::hardware_concurrency(), 1, 16);

	set.showInfo = true;


	Camera camera(Object(
		{ 0,0,2 },   //world coord
		{ 0,0,-1 },  //towarding 
		{ 0,1,0 },   //up
		0,			 //actions
		2,  		 //speed
		Math::pi / 2.f),		 //rotation speed
		set.fov,
		set.aspect,
		set.zFar,
		set.zNear);   	 

	Model model(Object({ 0,0,0 }, { 0,0,-1 }, { 0,1,0 }, Actions::turnLeft, 0, Math::pi / 3),
		Matirial(
			{ 0.005, 0.005, 0.005 },  //kambient
			{ 0.8, 0.86, 0.88 },      //kdiffuse
			{ 0.4, 0.4, 0.4 }));      //kspecular

	model.loadOBJ("models", "dog.obj");

	std::vector<Light> light;   //point light source
	light.push_back({
		{0,15,0}, 				//world coord
	{500,500,500} });			//rgb intensity
	light.push_back({ {10,-3,10},{100,100,100} });

	Math::vec3 amb_light{ 5,5,5 }; //ambient light intensity

	Canvas canvas(set.width,
		set.height,
		set.cWidth,
		set.cHeight,
		set.numCalculatingThreads,
		{ 0,0,0 }); //bgColor
	
	bool renderRequest = true;
	bool shutdown = false;

	// rendering loop
	std::jthread renderThread(renderingLoop, std::ref(canvas),std::ref(camera), 
		std::ref(model), std::ref(light), std::ref(amb_light), 
		std::ref(set), std::ref(renderRequest), std::ref(shutdown));

	// logic loop
	std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
	while (true) {

		std::chrono::duration<double, std::milli> delta = std::chrono::system_clock::now() - now;
		now = std::chrono::system_clock::now();

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
				set.mode = Mode::PhongShading;
			if (GetKeyState('2') & 0x8000)
				set.mode = Mode::zColoring;
			if (GetKeyState('3') & 0x8000)
				set.mode = Mode::framework;

			if (GetKeyState('B') & 0x8000)
				set.backfaceCulling = GetKeyState('B') & 1;

			if (GetKeyState('F') & 0x8000)
				set.showInfo = GetKeyState('F') & 1;

			renderThread = std::jthread(renderingLoop, std::ref(canvas),std::ref(camera), 
				std::ref(model), std::ref(light), std::ref(amb_light), 
				std::ref(set), std::ref(renderRequest), std::ref(shutdown));
		}

		camera.updateAtiitude(delta.count() / 1000.f);
		model.updateAtiitude(delta.count() / 1000.f);
	}
}
