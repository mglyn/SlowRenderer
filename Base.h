#pragma once
#include "Math.h"
#include <vector>
#include <string>

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

