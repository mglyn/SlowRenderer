#pragma once
#include "Math.h"
#include "Base.h"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <string>
#include <iostream>

enum Actions :int {
	none = 0,
	moveForward = 1 << 0,
	moveLeft = 1 << 1,
	moveBack = 1 << 2,
	moveRight = 1 << 3,
	moveUp = 1 << 4,
	moveDown = 1 << 5,
	turnUp = 1 << 6,
	turnLeft = 1 << 7,
	turnDown = 1 << 8,
	turnRight = 1 << 9,
};

class Object {
protected:
	Math::vec3 wPos;	//λ��
	Math::vec3 g;		//����
	Math::vec3 up;		//���Ϸ���

	int state;
	float speed;
	float rspeed;
public:
	Object(Math::vec3 wPos, Math::vec3 g, Math::vec3 up, int state, float speed, float rspeed) :
		wPos(wPos), g(g), up(up), state(state), speed(speed), rspeed(rspeed) {}

	void setAttitude(Math::vec3 wPos_, Math::vec3 g_, Math::vec3 up_) {
		wPos = wPos_;
		g = g_;
		up = up_;
	}

	void setState(bool remove, int op) {
		if (remove) state &= ~op;
		else state |= op;
	}

	void toggleState(int op) {
		state ^= op;
	}

	void updateAtiitude() {
		if (state == Actions::none) return;

		auto Rodrigues = [](const Math::vec3& k, const Math::vec3& v, float theta)->Math::vec3 {
			return (v * cosf(theta) + k.cross(v) * sinf(theta) + k * (k.dot(v) * (1.f - cosf(theta)))).normalized();
			};

		Math::vec3 Actions = Math::vec3{ g[0], 0, g[2] }.normalized();
		Math::vec3 vActions = Actions.cross(Math::vec3{ 0,1,0 });
		if (state & Actions::moveForward) wPos = wPos + speed * Actions;
		if (state & Actions::moveBack) wPos = wPos - speed * Actions;
		if (state & Actions::moveLeft) wPos = wPos - speed * vActions;
		if (state & Actions::moveRight) wPos = wPos + speed * vActions;
		if (state & Actions::moveUp) wPos = wPos + speed * up;
		if (state & Actions::moveDown) wPos = wPos - speed * up;

		Math::vec3 gxup = g.cross(up);
		if (state & Actions::turnUp) {
			g = Rodrigues(gxup, g, rspeed);
			up = Rodrigues(gxup, up, rspeed);
		}
		if (state & Actions::turnDown) {
			g = Rodrigues(gxup, g, -rspeed);
			up = Rodrigues(gxup, up, -rspeed);

		}
		if (state & Actions::turnLeft) {
			g = Rodrigues({ 0, 1, 0 }, g, rspeed);
			up = Rodrigues({ 0, 1, 0 }, up, rspeed);
		}
		if (state & Actions::turnRight) {
			g = Rodrigues({ 0, 1, 0 }, g, -rspeed);
			up = Rodrigues({ 0, 1, 0 }, up, -rspeed);
		}
	}

	std::string debugInfo() const {
		char str[512];

		std::string strState;
		if (state & moveForward)strState += " moveForward";
		if (state & moveLeft)strState += " moveLeft";
		if (state & moveBack)strState += " moveBack";
		if (state & moveRight)strState += " moveRight";
		if (state & moveUp)strState += " moveUp";
		if (state & moveDown)strState += " moveDown";
		if (state & turnUp)strState += " turnUp";
		if (state & turnLeft)strState += " turnLeft";
		if (state & turnDown)strState += " turnDown";
		if (state & turnRight)strState += " turnRight";

		sprintf(str,
			R"(attitude:
    pos: %.2f %.2f %.2f
    face to: %.2f %.2f %.2f
    up: %.2f %.2f %.2f
    state: %s
)",
wPos[0], wPos[1], wPos[2],
g[0], g[1], g[2],
up[0], up[1], up[2],
strState.c_str());

		return std::string(str);
	}
};

class Camera :public Object {
	friend class FragmentShader;
	friend class Renderer;

	const Setting& set;

public:
	Camera(Object object, const Setting& setting) : Object(object), set(setting) {}

	Math::mat4 calcMatrixP() const {
		float n = set.zNear, f = set.zFar;
		float t = abs(n) * tanf(set.fov / 2.f);
		float b = -t;
		float r = t * set.aspect;
		float l = -r;
		/*viewToProjection
		Math::mat4 squeeze:		Math::mat4 translate:		Math::mat4 scale:
		n, 0, 0, 0,				1, 0, 0, -(r + l) / 2.f,	2.f / (r - l), 0, 0, 0,
		0, n, 0, 0,				0, 1, 0, -(t + d) / 2.f,	0, 2.f / (t - d), 0, 0,
		0, 0, n + f, -n * f,	0, 0, 1, -(n + f) / 2.f,	0, 0, 2.f / (n - f), 0,
		0, 0, 1, 0				0, 0, 0, 1					0, 0, 0, 1
		perspective projection = scale * translate * squeeze;
		*/
		return {
			2 * n / (r - l), 0, (l + r) / (l - r), 0,
			0, 2 * n / (t - b), (b + t) / (b - t), 0,
			0, 0, (n + f) / (n - f), 2 * n * f / (f - n),
			0, 0, 1, 0
		};
	}

	Math::mat4 calcMatrixV() const {
		Math::vec3 gxup = g.cross(up);
		/*wordToView
		Math::mat4 translate:	Math::mat4 rotation
		1, 0, 0, -wPos[0],		gxup[0], gxup[1], gxup[2], 0,
		0, 1, 0, -wPos[1],		up[0],	 up[1],	  up[2],   0,
		0, 0, 1, -wPos[2],		-g[0],	 -g[1],	  -g[2],   0,
		0, 0, 0, 1				0,		 0,		  0,	   1
		view transformation =  rotation * translate;
		*/
		return {
			gxup[0],gxup[1],gxup[2],-wPos[0] * gxup[0] - wPos[1] * gxup[1] - wPos[2] * gxup[2],
			up[0],	up[1],	up[2],	-wPos[0] * up[0] - wPos[1] * up[1] - wPos[2] * up[2],
			-g[0],	-g[1],	-g[2],	wPos[0] * g[0] + wPos[1] * g[1] + wPos[2] * g[2],
			0,		0,		0,		1
		};
	}
};

class Model :public Object {
	friend class FragmentShader;
	friend class Renderer;

	std::string name;
	Mesh mesh;
	Matirial mtl;

	bool noNormal = false;
	bool noUV = false;

	std::vector<std::string> seprateLine(const std::string& str) {
		std::vector<std::string> vec;
		std::string tmp;
		for (int i = 0; i < str.length(); i++) {
			if (str[i] != ' ') {
				tmp += str[i];
			}
			else {
				vec.push_back(tmp);
				tmp.clear();
			}
		}
		if (!tmp.empty())vec.push_back(tmp);
		return vec;
	};

	std::vector<int> seprate(const std::string& str) {
		std::vector<int> vec(3, 0);

		int p = str.find_first_of("/");
		int q = str.find_last_of("/");

		if (p < 0) {
			vec[0] = std::stoi(str) - 1;
		}
		else {
			vec[0] = std::stoi(str.substr(0, p)) - 1;
			if (q - p - 1 > 0) vec[1] = std::stoi(str.substr(p + 1, q - p - 1)) - 1;
			if (str.length() - q - 1 > 0) vec[2] = std::stoi(str.substr(q + 1, str.length() - q - 1)) - 1;
		}

		return vec;
	};

	void genNormals() { //���������������Ȩ���ɶ��㷨��
		std::vector<std::vector<Math::vec3>> adjFacesNormal;
		adjFacesNormal.resize(mesh.mPos.size());

		for (auto& face : mesh.tInfo) {
			Math::vec3 mPos[3];
			for (int i = 0; i < 3; i++) {
				mPos[i] = mesh.mPos[face[i][0]];
				face[i][2] = face[i][0];
			}
			Math::vec3 mNormal = (mPos[0] - mPos[1]).cross(mPos[1] - mPos[2]);
			for (int i = 0; i < 3; i++) {
				adjFacesNormal[face[i][0]].push_back(mNormal);
			}
		}

		mesh.mNormal.resize(mesh.mPos.size());

		for (int i = 0; i < mesh.mPos.size(); i++) {

			float totArea = 0;
			for (int j = 0; j < adjFacesNormal[i].size(); j++) {
				totArea += sqrt(adjFacesNormal[i][j].dot(adjFacesNormal[i][j]));
			}
			for (int j = 0; j < adjFacesNormal[i].size(); j++) {
				float area = sqrt(adjFacesNormal[i][j].dot(adjFacesNormal[i][j]));
				mesh.mNormal[i] = mesh.mNormal[i] + area / totArea * adjFacesNormal[i][j];
			}
			mesh.mNormal[i] = mesh.mNormal[i].normalized();
		}
	}
public:
	Model(Object object, Matirial mtl) : Object(object), mtl(mtl) {}

	bool loadOBJ(const std::string& path, const std::string _name) {
		std::ifstream ifs;
		ifs.open((path + "/" + _name).c_str());
		if (!ifs.is_open())return false;

		name = _name;

		std::string str;
		while (std::getline(ifs, str)) {
			std::vector<std::string> vec = seprateLine(str);
			if (vec.empty())continue;

			std::string mtl;
			if (vec[0] == "v") {
				float x = std::stof(vec[1].c_str());
				float y = std::stof(vec[2].c_str());
				float z = std::stof(vec[3].c_str());
				mesh.mPos.push_back({ x, y, z });
			}
			else if (vec[0] == "vt") {
				float u = std::stof(vec[1].c_str());
				float v = std::stof(vec[2].c_str());
				mesh.texCoord.push_back({ u, v });
			}
			else if (vec[0] == "vn") {
				float x = std::stof(vec[1].c_str());
				float y = std::stof(vec[2].c_str());
				float z = std::stof(vec[3].c_str());
				mesh.mNormal.push_back({ x, y, z });
			}
			else if (vec[0] == "usemtl") {
				mtl = vec[1];
			}
			else if (vec[0] == "f") {
				if (vec.size() == 4) {
					std::vector<int> posTexNormA = seprate(vec[1]);
					std::vector<int> posTexNormB = seprate(vec[2]);
					std::vector<int> posTexNormC = seprate(vec[3]);
					std::vector<std::vector<int>> tri;
					tri.push_back(posTexNormA);
					tri.push_back(posTexNormB);
					tri.push_back(posTexNormC);
					mesh.tInfo.push_back(tri);
				}
				else if (vec.size() == 5) {
					std::vector<int> posTexNormA = seprate(vec[1]);
					std::vector<int> posTexNormB = seprate(vec[2]);
					std::vector<int> posTexNormC = seprate(vec[3]);
					std::vector<int> posTexNormD = seprate(vec[4]);
					std::vector<std::vector<int>> tri1;
					tri1.push_back(posTexNormA);
					tri1.push_back(posTexNormB);
					tri1.push_back(posTexNormC);
					mesh.tInfo.push_back(tri1);
					std::vector<std::vector<int>> tri2;
					tri2.push_back(posTexNormA);
					tri2.push_back(posTexNormC);
					tri2.push_back(posTexNormD);
					mesh.tInfo.push_back(tri2);
				}
			}
		}
		if (mesh.mNormal.empty()) {
			noNormal = true;
			genNormals();
		}
		if (mesh.texCoord.empty()) {
			noUV = true;
		}
		return true;
	}

	const Math::mat4 calcMatrixM() const {
		Math::vec3 gxup = g.cross(up);
		Math::mat4 translate = {
			1, 0, 0, wPos[0],
			0, 1, 0, wPos[1],
			0, 0, 1, wPos[2],
			0, 0, 0, 1
		};
		Math::mat4 rotation = {
			gxup[0], up[0], -g[0], 0,
			gxup[1], up[1], -g[1], 0,
			gxup[2], up[2], -g[2], 0,
			0, 0, 0, 1
		};
		return translate * rotation;
	}

	std::string debugInfo() const {
		char str[512];
		sprintf(str,
			R"(
model attributes:
name: %s
vertices: %llu
normals: %llu %s
triangles: %llu
)",
name.c_str(),
mesh.mPos.size(),
mesh.mNormal.size(), noNormal ? "(AutoGen)" : "",
mesh.tInfo.size());

		return std::string(str) + Object::debugInfo();
	}
};


