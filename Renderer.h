#pragma once
#include "Math.h"
#include "Base.h"
#include "Objects.h"
#include "Thread.h"
#include "Canvas.h"

struct TempFragBuffer {  //��ʱ���ػ���
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
			diffuse = diffuse + mtl.kd.cwiseProduct(li.intensity) * ((std::max)(0.f, f.wNormal.dot(l)) / r_2);
			specular = specular + mtl.ks.cwiseProduct(li.intensity) * (powf((std::max)(0.f, f.wNormal.dot(h)), 300) / r_2);
		}
		return (diffuse + specular + ambient).clamped(0.f, 1.f, 0.f, 1.f);
	}
};

class Renderer {
	const Setting& set;

	Math::mat4 M, invTransM, PV;

	//������Ϣ
	std::vector<Math::vec3> wPos;
	std::vector<Math::vec4> cPos;
	std::vector<Math::vec3> wNormal;

	//������Ϣ
	std::vector<float> depthBuf;
	std::vector<Fragment> fragment;

	//threads
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
				for (int x = 0; x < set.width; x++) {
					int pid = y * set.width + x;
					canvas.drawPixel(pid, set.bgColor);
					depthBuf[pid] = -1e8;
				}
			}
			};

		int num = set.height;
		int blockSize = (std::max)((std::min)(num / (8 * set.numCalculatingThreads), 512), 1);

		for (int i = 0; i < num; i += blockSize) {
			threads.addTask(clearTask, i, (std::min)(i + blockSize, num));
		}

		fragment.clear();

		threads.barrier();
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
		int blockSize = std::clamp(num / (8 * set.numCalculatingThreads), 32, 512);

		for (int i = 0; i < num; i += blockSize) {
			threads.addTask(vertexProcessTask1, i, (std::min)(i + blockSize, num));
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
		blockSize = std::clamp(num / (8 * set.numCalculatingThreads), 32, 512);

		for (int i = 0; i < num; i += blockSize) {
			threads.addTask(vertexProcessTask2, i, (std::min)(i + blockSize, num));
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

	void setup_rasterize_triangle(Canvas& canvas, const Camera& camera, const Model& model) {
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
				std::vector<Triangle> triangles = clipTriangle(t, set.zNear);

				//3 apply perspective division and viewport transform to get screen space coord
				for (auto& t : triangles) {
					for (int i = 0; i < 3; i++) {
						t.ver[i].cPos[0] /= t.ver[i].cPos[3];
						t.ver[i].cPos[1] /= t.ver[i].cPos[3];
					}
					for (int i = 0; i < 3; i++) {
						t.ver[i].sPos[0] = 0.5f * set.width * (t.ver[i].cPos[0] + 1.f);
						t.ver[i].sPos[1] = 0.5f * set.height * (t.ver[i].cPos[1] + 1.f);
					}

					float area = (t.ver[0].sPos[0] - t.ver[1].sPos[0]) * (t.ver[1].sPos[1] - t.ver[2].sPos[1]) -
						(t.ver[1].sPos[0] - t.ver[2].sPos[0]) * (t.ver[0].sPos[1] - t.ver[1].sPos[1]);

					if (set.backfaceCulling && area < 0) continue;	//backface culling

					if (set.mod == Setting::Mod::framework) canvas.drawTriangleFrame(t);
					else halfSpaceRasterize(canvas, t, area, buf);
				}
			}
			};

		int num = model.mesh.tInfo.size();
		int blockSize = std::clamp(num / (8 * set.numCalculatingThreads), 32, 512);

		for (int i = 0; i < num; i += blockSize) {
			threads.addTask(setup_rasterize_triangle_task, i, (std::min)(i + blockSize, num));
		}

		threads.barrier();
	}

	void halfSpaceRasterize(Canvas& canvas, Triangle& t, float area, TempFragBuffer& buf) {
		//bounding box
		int lbound = (std::min)((std::max)((std::min)((std::min)(t.ver[0].sPos[0], t.ver[1].sPos[0]), t.ver[2].sPos[0]), 0.f), float(set.width - 1));
		int rbound = (std::min)((std::max)((std::max)((std::max)(t.ver[0].sPos[0], t.ver[1].sPos[0]), t.ver[2].sPos[0]), 0.f), float(set.width - 1));
		int bbound = (std::min)((std::max)((std::min)((std::min)(t.ver[0].sPos[1], t.ver[1].sPos[1]), t.ver[2].sPos[1]), 0.f), float(set.height - 1));
		int tbound = (std::min)((std::max)((std::max)((std::max)(t.ver[0].sPos[1], t.ver[1].sPos[1]), t.ver[2].sPos[1]), 0.f), float(set.height - 1));
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
					if (met) break;     //little optimize: break when the triangle has already passed
					else continue;
				}
				met = true;

				int pid = y * set.width + x;

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

	void fragmentProcess(Canvas& canvas, const FragmentShader& fragmentShader) {
		std::function<void(int, int)> fragmentShadingTask;
		if (set.mod == Setting::Mod::PhongShading) {
			fragmentShadingTask = [&](int st, int ed) {
				for (int id = st; id < ed; id++) {
					auto& f = fragment[id];
					if (f.depth == depthBuf[f.pid]) {
						canvas.drawPixel(f.pid, fragmentShader.run(f));
					}
				}
				};
		}
		else if (set.mod == Setting::Mod::zColoring) {
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
		int blockSize = std::clamp(num / (8 * set.numCalculatingThreads), 32, 512);

		for (int i = 0; i < fragment.size(); i += blockSize) {
			threads.addTask(fragmentShadingTask, i, (int)(std::min)(i + blockSize, (int)fragment.size()));
		}

		threads.barrier();
	}

public:
	Renderer(const Setting& setting) :threads(setting.numCalculatingThreads), set(setting) {}

	void draw(Canvas& canvas,
		const Camera& camera,
		const Model& model,
		const std::vector<Light>& light,
		const Math::vec3& amb_light)
	{
		//1.init
		depthBuf.resize(set.width * set.height);
		clear(canvas);

		//2.���¾���
		updateMatrix(camera, model);

		//3.����任
		vertexProcess(model);

		//4.��װ����դ��������
		setup_rasterize_triangle(canvas, camera, model);

		//5.��Ⱦ����
		FragmentShader fragmentShader(model.mtl, camera, light, amb_light);
		fragmentProcess(canvas, fragmentShader);
	}

	void renderingLoop(Canvas& canvas,
		const Camera& camera,
		const Model& model,
		const std::vector<Light>& light,
		const Math::vec3& amb_light,
		const bool& shutdown) 
	{
		int frameCnt = 0, fps = 0;
		std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
		while (!shutdown) {

			//1.
			draw(canvas, camera, model, light, amb_light);

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
				"[ F ] for more info";
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
};



