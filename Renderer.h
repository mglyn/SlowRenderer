#pragma once
#include "Math.h"
#include "Base.h"
#include "Objects.h"
#include "Thread.h"
#include "Canvas.h"

struct Setting {
	enum Mod {
		PhongShading,
		zColoring,
		framework
	};
	Mod mod = PhongShading;
	bool backfaceCulling = true;

	std::wstring debugInfo() const {
		wchar_t str[512];
		swprintf(str, 512,
			LR"(
backface culling: %s [ B ]
color mod: %s [ 1/2/3 ]
)",
backfaceCulling ? L"enabled" : L"disabled",
			[this]()->const wchar_t* {
				if (mod == Setting::Mod::PhongShading)
					return { L"1.Blinn-Phong shading" };
				else if (mod == Setting::Mod::zColoring)
					return { L"2.depth" };
				else if (mod == Setting::Mod::framework)
					return { L"3.framework" };
				return {};
			}());

		return std::wstring(str);
	}
};

struct TempFragBuffer {  //临时像素缓冲
	static constexpr int maxBatchSize = 4096;
	std::vector<Fragment>& dst;
	std::vector<float>& depthBuf;
	std::mutex& mtx;
	std::vector<Fragment> buffer;

	TempFragBuffer(std::vector<Fragment>& dst,
		std::vector<float>& depthBuf,
		std::mutex& mtx) :
		dst(dst),
		depthBuf(depthBuf),
		mtx(mtx) {
		buffer.reserve(2 * maxBatchSize);
	}

	~TempFragBuffer() {
		if (buffer.size())transfer();
	}

	void transfer() {
		mtx.lock();
		for (auto& f : buffer) {
			if (f.depth > depthBuf[f.pid]) {       //earlyZ
				depthBuf[f.pid] = f.depth;
				dst.push_back(f);
			}
		}
		mtx.unlock();
		buffer.clear();
	}

	void push_back(const Fragment& fragment) {
		buffer.push_back(fragment);
		if (buffer.size() >= maxBatchSize)transfer();
	}
};

class FragmentShader {
	const Matirial& mtl;
	const Camera& camera;
	const std::vector<Light>& light;
	const Math::vec3& amb_light;

public:
	FragmentShader(const Matirial& mtl, 
		const Camera& camera, 
		const std::vector<Light>& light,
		const Math::vec3& amb_light) :
		mtl(mtl), camera(camera), light(light), amb_light(amb_light) {}

	Math::vec3 run(Fragment& f) const {
		Math::vec3 diffuse;
		Math::vec3 specular;
		Math::vec3 ambient;

		Math::vec3 v = (camera.wPos - f.wPos).normalized();		//object to camera
		for (auto& li : light) {
			Math::vec3 l = li.wPos - f.wPos;			//object to lightsource

			float r_2 = l.dot(l);
			l = l.normalized();
			Math::vec3 h = (l + v).normalized();//half

			ambient = ambient + mtl.ka.cwiseProduct(amb_light);
			diffuse = diffuse + mtl.kd.cwiseProduct(li.intensity) * (max(0.f, f.wNormal.dot(l)) / r_2);
			specular = specular + mtl.ks.cwiseProduct(li.intensity) * (powf(max(0.f, f.wNormal.dot(h)), 300) / r_2);
		}
		return (diffuse + specular + ambient).clamped(0.f, 1.f, 0.f, 1.f);
	}
};

class Renderer {
	Math::mat4 M, invTransM, PV;

	//顶点信息
	std::vector<Math::vec3> wPos;
	std::vector<Math::vec4> cPos;
	std::vector<Math::vec3> wNormal;

	//像素信息
	std::vector<float> depthBuf;
	std::vector<Fragment> fragment;

	//threads
	int numThreads = std::thread::hardware_concurrency();
	ThreadPool threads;
	std::mutex mtx;

	void updateMatrix(const Camera& camera, const Model& model) {
		M = model.calcMatrixM();
		invTransM = M.inverse().transpose();
		PV = camera.calcMatrixP() * camera.calcMatrixV();
	}

	void clear(Canvas& canvas) {
		auto clearTask = [&](int st, int ed) {
			for (int y = st; y < ed; y++) {
				for (int x = 0; x < canvas.width; x++) {
					int pid = y * canvas.width + x;
					canvas.drawPixel(pid, canvas.bgColor);
					depthBuf[pid] = -1e8;
				}
			}
			};

		int num = canvas.height;
		int blockSize = max(min(num / (4 * numThreads), 512), 1);

		for (int i = 0; i < canvas.height; i += blockSize) {
			threads.addTask(clearTask, i, min(i + blockSize, canvas.height));
		}

		fragment.clear();
	}

	void vertexProcess(const Model& model) {
		wPos.resize(model.mesh.mPos.size());
		cPos.resize(model.mesh.mPos.size());
		wNormal.resize(model.mesh.mNormal.size());

		auto vertexProcessTask1 = [&](int st, int ed) {
			for (int id = st; id < ed; id++) {
				auto& mPos = model.mesh.mPos[id];
				Math::vec4 pos = { mPos[0],mPos[1],mPos[2],1.f };
				pos = M * pos;
				wPos[id] = Math::vec3{ pos[0], pos[1], pos[2] };

				pos = PV * pos;
				cPos[id] = pos;
			}
			};

		int num = model.mesh.mPos.size();
		int blockSize = max(min(512, num / (8 * numThreads)), 1);

		for (int i = 0; i < num; i += blockSize) {
			threads.addTask(vertexProcessTask1, i, min(i + blockSize, num));
		}

		auto vertexProcessTask2 = [&](int st, int ed) {
			for (int id = st; id < ed; id++) {
				auto& mNormal = model.mesh.mNormal[id];
				Math::vec4 normal = { mNormal[0],mNormal[1],mNormal[2],0.f };
				normal = invTransM * normal;
				wNormal[id] = { normal[0], normal[1], normal[2] };
			}
			};

		num = model.mesh.mNormal.size();
		blockSize = max(min(512, num / (8 * numThreads)), 1);

		for (int i = 0; i < num; i += blockSize) {
			threads.addTask(vertexProcessTask2, i, min(i + blockSize, num));
		}

		threads.barrier();
	}

	std::vector<Triangle> clipTriangle(Triangle& t, float vZPlane) {//clip triangles using the z plane of view space
		bool out[3] = {
			t.ver[0].cPos[3] >= vZPlane,
			t.ver[1].cPos[3] >= vZPlane,
			t.ver[2].cPos[3] >= vZPlane
		};

		if (out[0] && out[1] && out[2])return{};

		if (!out[0] && !out[1] && !out[2]) return { t };

		std::vector<Vertex> clipVertex;
		for (int i = 0, j = 1; i < 3; i++, j = (j + 1) % 3) {
			float da = vZPlane - t.ver[i].cPos[3];
			float db = vZPlane - t.ver[j].cPos[3];

			if (da * db < 0) {
				//interpolate vertex arrtribute
				float alpha = da / (da - db);
				auto interpolate = [&alpha](auto& attribA, auto& attribB) {
					return attribA * (1 - alpha) + attribB * alpha;
					};

				auto itp_wPos = interpolate(t.ver[i].wPos, t.ver[j].wPos);
				auto itp_cPos = interpolate(t.ver[i].cPos, t.ver[j].cPos);
				auto itp_wNormal = interpolate(t.ver[i].wNormal, t.ver[j].wNormal);
				clipVertex.emplace_back(itp_wPos, itp_cPos, itp_wNormal);
			}
			if (db > 0) clipVertex.emplace_back(t.ver[j].wPos, t.ver[j].cPos, t.ver[j].wNormal);
		}

		std::vector<Triangle> triangles;
		for (int i = 2; i < clipVertex.size(); i++) {
			triangles.emplace_back(Triangle{ clipVertex[0], clipVertex[i - 1], clipVertex[i] });
		}
		return triangles;
	}

	void setup_rasterize_triangle(Canvas& canvas, const Camera& camera, const Model& model, const Setting& setting) {
		auto setup_rasterize_triangle_task = [&](int st, int ed) {
			TempFragBuffer buf(fragment, depthBuf, mtx);

			for (int id = st; id < ed; id++) {
				auto& face = model.mesh.tInfo[id];

				//1 construct triangle
				Triangle t;
				for (int j = 0; j < 3; j++) {
					t.ver[j].wPos = wPos[face[j][0]];
					t.ver[j].cPos = cPos[face[j][0]];
					t.ver[j].wNormal = wNormal[face[j][2]];
				}

				//2 clip origin triangle
				std::vector<Triangle> triangles = clipTriangle(t, camera.zNear);

				//3 apply perspective division and viewport transform to get screen space coord
				for (auto& t : triangles) {
					for (int i = 0; i < 3; i++) {
						t.ver[i].cPos[0] /= t.ver[i].cPos[3];
						t.ver[i].cPos[1] /= t.ver[i].cPos[3];
					}
					for (int i = 0; i < 3; i++) {
						t.ver[i].sPos[0] = 0.5f * canvas.width * (t.ver[i].cPos[0] + 1.f);
						t.ver[i].sPos[1] = 0.5f * canvas.height * (t.ver[i].cPos[1] + 1.f);
					}

					float area = (t.ver[0].sPos[0] - t.ver[1].sPos[0]) * (t.ver[1].sPos[1] - t.ver[2].sPos[1]) -
						(t.ver[1].sPos[0] - t.ver[2].sPos[0]) * (t.ver[0].sPos[1] - t.ver[1].sPos[1]);

					if (setting.backfaceCulling && area < 0) continue;	//backface culling

					if (setting.mod == Setting::Mod::framework) drawTriangleFrame(canvas, t);
					else halfSpaceRasterize(canvas, t, area, buf);
				}
			}
			};

		int num = model.mesh.tInfo.size();
		int blockSize = max(min(512, num / (8 * numThreads)), 1);

		for (int i = 0; i < num; i += blockSize) {
			threads.addTask(setup_rasterize_triangle_task, i, min(i + blockSize, num));
		}

		threads.barrier();
	}

	void halfSpaceRasterize(Canvas& canvas, Triangle& t, float area, TempFragBuffer& buf) {
		//bounding box
		int lbound = min(max(min(min(t.ver[0].sPos[0], t.ver[1].sPos[0]), t.ver[2].sPos[0]), 0), canvas.width - 1);
		int rbound = min(max(max(max(t.ver[0].sPos[0], t.ver[1].sPos[0]), t.ver[2].sPos[0]), 0), canvas.width - 1);
		int bbound = min(max(min(min(t.ver[0].sPos[1], t.ver[1].sPos[1]), t.ver[2].sPos[1]), 0), canvas.height - 1);
		int tbound = min(max(max(max(t.ver[0].sPos[1], t.ver[1].sPos[1]), t.ver[2].sPos[1]), 0), canvas.height - 1);
		if (lbound > rbound)return;

		for (int y = bbound; y <= tbound; y++) {
			bool met = false;
			for (int x = lbound; x <= rbound; x++) {
				bool inside = true;
				float S[3];			//area of PAB PBC PCA ABC

				for (int i = 0, j = 1; i < 3; i++, j = (j + 1) % 3) {
					S[i] = (t.ver[j].sPos[0] - t.ver[i].sPos[0]) * (y - t.ver[i].sPos[1]) -
						(t.ver[j].sPos[1] - t.ver[i].sPos[1]) * (x - t.ver[i].sPos[0]);
					if (S[i] < 0) {
						inside = false;
						break;
					}
				}

				if (!inside) {
					if (met) break;
					else continue;
				}
				met = true;

				int pid = y * canvas.width + x;

				//corrected interpolation 
				float alpha = S[1] / area, beta = S[2] / area, gama = S[0] / area;
				float z0 = t.ver[0].cPos[3], z1 = t.ver[1].cPos[3], z2 = t.ver[2].cPos[3];
				float Z = 1.f / (alpha / z0 + beta / z1 + gama / z2);

				auto interpolate = [&](auto& attribA, auto& attribB, auto& attribC) {
					return Z * (attribA * (alpha / z0) + attribB * (beta / z1) + attribC * (gama / z2));
					};

				Math::vec3 itp_worldPos = interpolate(t.ver[0].wPos, t.ver[1].wPos, t.ver[2].wPos);
				Math::vec3 itp_worldNormal = interpolate(t.ver[0].wNormal, t.ver[1].wNormal, t.ver[2].wNormal);

				buf.push_back({ pid, Z, itp_worldPos, itp_worldNormal });
			}
		}
	}

	void drawTriangleFrame(Canvas& canvas, const Triangle& t) {
		auto st0 = t.ver[0].sPos, st1 = t.ver[1].sPos, st2 = t.ver[2].sPos;
		auto ed0 = t.ver[1].sPos, ed1 = t.ver[2].sPos, ed2 = t.ver[0].sPos;
		if (canvas.Cohen_Sutherland(st0[0], st0[1], ed0[0], ed0[1]))
			canvas.Bresenham(st0[0], st0[1], ed0[0], ed0[1]);
		if (canvas.Cohen_Sutherland(st1[0], st1[1], ed1[0], ed1[1]))
			canvas.Bresenham(st1[0], st1[1], ed1[0], ed1[1]);
		if (canvas.Cohen_Sutherland(st2[0], st2[1], ed2[0], ed2[1]))
			canvas.Bresenham(st2[0], st2[1], ed2[0], ed2[1]);
	}

	void fragmentProcess(Canvas& canvas, const FragmentShader& fragmentShader, const Setting& setting) {
		std::function<void(int, int)> fragmentShadingTask;
		if (setting.mod == Setting::Mod::PhongShading) {
			fragmentShadingTask = [&](int st, int ed) {
				for (int id = st; id < ed; id++) {
					auto& f = fragment[id];
					if (f.depth == depthBuf[f.pid]) {
						canvas.drawPixel(f.pid, fragmentShader.run(f));
					}
				}
				};
		}
		else if (setting.mod == Setting::Mod::zColoring) {
			fragmentShadingTask = [&](int st, int ed) {
				for (int id = st; id < ed; id++) {
					auto& f = fragment[id];
					if (f.depth == depthBuf[f.pid]) {
						Math::vec3 color = { depthBuf[f.pid], depthBuf[f.pid], depthBuf[f.pid] };
						canvas.drawPixel(f.pid, color.clamped(-4, 0, 0, 1));
					}
				}
				};
		}

		int num = fragment.size();
		int blockSize = max(min(512, num / (8 * numThreads)), 1);

		for (int i = 0; i < fragment.size(); i += blockSize) {
			threads.addTask(fragmentShadingTask, i, (int)min(i + blockSize, fragment.size()));
		}

		threads.barrier();
	}

public:
	Renderer() :threads(numThreads) {}

	void draw(Canvas& canvas,
		const Camera& camera, 
		const Setting& setting,
		const Model& model,
		const std::vector<Light>& light,
		const Math::vec3& amb_light) 
	{
		//1.清空缓冲
		depthBuf.resize(canvas.width * canvas.height);
		clear(canvas);

		//2.更新矩阵
		updateMatrix(camera, model);

		//3.顶点变换
		vertexProcess(model);

		//4.组装、光栅化三角形
		setup_rasterize_triangle(canvas, camera, model, setting);

		//5.渲染像素
		FragmentShader fragmentShader(model.mtl, camera, light, amb_light);
		fragmentProcess(canvas, fragmentShader, setting);
	}

	std::wstring debugInfo() {
		wchar_t str[512];
		swprintf(str, 512,
LR"(
threads: %d
)",
			numThreads);

		return std::wstring(str);
	}
};



