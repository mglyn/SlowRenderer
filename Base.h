#pragma once
#include "Math.h"
#include <vector>
#include <string>

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

	Math::vec3 bgColor;
	Math::vec3 textColor;

	enum class Mod {
		PhongShading,
		zColoring,
		framework
	};

	Mod mod;
	bool backfaceCulling;

	std::string debugInfo() const {
		char str[512];
		sprintf(str,
			R"(
resolution: %dx%d
super sampling: %dx%d

backface culling: %s [ B ]
color mod: %s [ 1/2/3 ]

[ W/A/S/D/DIR ] to move camera
camera attributes:
FOV: %.2f
aspect: %.2f
zNear: %.2f
zFar: %.2f
)",
width / cWidth, height / cHeight,
width, height,
backfaceCulling ? "enabled" : "disabled",
[this]()->const char* {
				if (mod == Setting::Mod::PhongShading)
					return { "1.Blinn-Phong shading" };
				else if (mod == Setting::Mod::zColoring)
					return { "2.depth" };
				else if (mod == Setting::Mod::framework)
					return { "3.framework" };
				return {};
			}(), fov, aspect, zNear, zFar);

		return std::string(str);
	}
};

//{ vertex1{ posID, texCoordID, normalID}, vertex2{}, vertex3{} }
using Ind = std::vector<std::vector<int>>;

struct Mesh {
	std::vector<Ind> tInfo;
	std::vector<Math::vec3> mPos;
	std::vector<Math::vec2> texCoord;							//texture uv
	std::vector<Math::vec3> mNormal;
};

struct Vertex {
	Math::vec3 wPos;			//world space pos
	union {
		Math::vec4 cPos;		//clip space pos
		Math::vec4 sPos;		//screen space pos
	};
	Math::vec3 wNormal;			//world space normal

	Vertex(const Vertex& x) :
		wPos(x.wPos),
		cPos(x.cPos), 
		wNormal(x.wNormal) {}

	Vertex(const Math::vec3& wPos, 
		const Math::vec4& cPos, 
		const Math::vec3& wNormal) :
		wPos(wPos), 
		cPos(cPos), 
		wNormal(wNormal) {}

	Vertex() :wPos(Math::vec3{}), 
		cPos(Math::vec4{}), 
		wNormal(Math::vec3{}) {}

	Vertex operator = (const Vertex& x) {
		wPos = x.wPos;
		cPos = x.cPos;
		wNormal = x.wNormal;
		return { *this };
	}
};

struct Triangle {
	Vertex ver[3];
};

struct Light {
	Math::vec3 wPos;
	Math::vec3 intensity;
};

struct Fragment {
	int pid;
	float depth;
	Math::vec3 wPos;
	Math::vec3 wNormal;
};

struct Matirial {
	Math::vec3 ka;
	Math::vec3 kd;
	Math::vec3 ks;
};

