#pragma once
#include <initializer_list>
#include <algorithm>
#include <concepts>

namespace Math {
	inline const float eps = 1e-6f;
	inline const float pi = 3.1415926f;

	inline static float Q_rsqrt(float number) {   
		long i;
		float x2, y;
		const float threehalfs = 1.5F;

		x2 = number * 0.5F;
		y = number;
		i = *(long*)&y;
		i = 0x5f3759df - (i >> 1);
		y = *(float*)&i;
		y = y * (threehalfs - (x2 * y * y));

		return y;
	}


	template<int len, class Tx>
	requires requires (Tx a, Tx b){
		requires len > 0 && len <= 4;
		a + b;
		a - b;
		a * b;
		a / b;
	}
	class vec {
		Tx v[len];
	public:
		vec<len, Tx>() {
			for (int i = 0; i < len; i++)
				v[i] = 0;
		}
		vec<len, Tx>(const vec<len, Tx>& x) {
			for (int i = 0; i < len; i++)
				v[i] = x.v[i];
		}
		vec<len, Tx>(const std::initializer_list<Tx>& x) {
			auto it = x.begin();
			for (int i = 0; i < len; i++) {
				if (it != x.end()) {
					v[i] = *it;
					it++;
				}
				else v[i] = 0;
			}
		}

		vec<len, Tx> operator = (const vec<len, Tx>& x) {
			for (int i = 0; i < len; i++)
				v[i] = x[i];
			return { *this };
		}
		vec<len, Tx> operator = (const std::initializer_list<float>& x) {
			auto it = x.begin();
			for (int i = 0; i < len; i++) {
				if (it != x.end()) {
					v[i] = *it;
					it++;
				}
				else v[i] = 0;
			}
			return { *this };
		}

		Tx& operator[] (int x) { return v[x]; }
		const Tx& operator[] (int x) const { return v[x]; }

		vec<len, Tx> operator +(const vec<len, Tx>& u) const {
			vec<len, Tx> ret;
			for (int i = 0; i < len; i++)ret[i] = v[i] + u[i];
			return ret;
		}
		vec<len, Tx> operator -(const vec<len, Tx>& u) const {
			vec<len, Tx> ret;
			for (int i = 0; i < len; i++)ret[i] = v[i] - u[i];
			return ret;
		}
		vec<len, Tx> operator -() const {
			vec<len, Tx> ret;
			for (int i = 0; i < len; i++)ret[i] = -v[i];
			return ret;
		}
		vec<len, Tx> operator *(Tx k) const {
			vec<len, Tx> ret;
			for (int i = 0; i < len; i++)ret[i] = v[i] * k;
			return ret;
		}
		vec<len, Tx> operator /(Tx k) const {
			vec<len, Tx> ret;
			for (int i = 0; i < len; i++)ret[i] = v[i] / k;
			return ret;
		}
		vec<len, Tx> cwiseProduct(const vec<len, Tx>& u) const {
			vec<len, Tx> ret;
			for (int i = 0; i < len; i++)ret[i] = v[i] * u[i];
			return ret;
		}
		Tx dot(const vec<len, Tx>& u) const {
			Tx ret = 0;
			for (int i = 0; i < len; i++)ret += v[i] * u[i];
			return ret;
		}
		vec<3, Tx> cross(const vec<3, Tx>& u) const {
			return { v[1] * u[2] - v[2] * u[1], v[2] * u[0] - v[0] * u[2], v[0] * u[1] - v[1] * u[0] };
		}

		vec<len, Tx> normalized() const {
			vec<len, Tx> ret;
			float t = 0;
			for (int i = 0; i < len; i++)
				t += v[i] * v[i], ret.v[i] = v[i];
			t = Math::Q_rsqrt(t);
			for (int i = 0; i < len; i++) ret.v[i] *= t;
			return ret;
		}
		vec<len, Tx> clamped(Tx sl, Tx sr, Tx tl, Tx tr) const {
			vec<len, Tx> ret;
			for (int i = 0; i < len; i++) {
				if (v[i] > sr)ret.v[i] = tr;
				else if (v[i] < sl)ret.v[i] = tl;
				else ret.v[i] = (v[i] - sl) / (sr - sl) * (tr - tl) + tl;
			}
			return ret;
		}
	};

	template<int size, class Tx>
		requires requires (Tx a, Tx b) {
		requires size > 0 && size <= 4;
		a + b;
		a - b;
		a * b;
		a / b;
	}
	class mat {
		Tx m[size][size];
	public:
		mat<size, Tx>() {
			for (int i = 0; i < size; i++) {
				for (int j = 0; j < size; j++) {
					m[i][j] = 0;
				}
			}
		}
		mat<size, Tx>(const mat<size, Tx>& B) {
			for (int i = 0; i < size; i++) {
				for (int j = 0; j < size; j++) {
					m[i][j] = B[i][j];
				}
			}
		}
		mat<size, Tx>(const std::initializer_list<float>& x) {
			auto it = x.begin();
			for (int i = 0; i < size; i++) {
				for (int j = 0; j < size; j++) {
					if (it != x.end()) {
						m[i][j] = *it;
						it++;
					}
					else {
						m[i][j] = 0;
					}
				}
			}
		}

		mat<size, Tx> operator = (const mat<size, Tx>& B) {
			for (int i = 0; i < size; i++) {
				for (int j = 0; j < size; j++) {
					m[i][j] = B[i][j];
				}
			}
			return { *this };
		}
		mat<size, Tx> operator = (const std::initializer_list<float> x) {
			auto it = x.begin();
			for (int i = 0; i < size * size && it != x.end(); i++, it++)
				m[i] = *it;
			return { *this };
		}

		mat<size, Tx> identity() {
			mat<size, Tx> ret;
			for (int i = 0; i < size; i++) {
				for (int j = 0; j < size; j++) {
					ret[i][j] = i == j;
				}
			}
			return ret;
		}

		Tx* operator[] (int x) { return m[x]; }
		const Tx* operator[] (int x) const { return m[x]; }

		vec<size, Tx> operator *(const vec<size, Tx>& u) const {
			vec<size, Tx> ret;
			for (int i = 0; i < size; i++) {
				for (int j = 0; j < size; j++) {
					ret[i] += m[i][j] * u[j];
				}
			}
			return ret;
		}
		mat<size, Tx> operator *(const mat<size, Tx>& B) const {
			mat<size, Tx> ret;
			for (int i = 0; i < size; i++) {
				for (int k = 0; k < size; k++) {
					for (int j = 0; j < size; j++) {
						ret[i][j] += m[i][k] * B[k][j];
					}
				}
			}
			return ret;
		}
		mat<size, Tx> inverse() {
			mat<size, Tx> ret = identity();
			mat<size, Tx> self(*this);
			for (int i = 0; i < size - 1; i++) {
				int pivot = i;
				float pivotsize = self[i][i];
				pivotsize = abs(pivotsize);
				for (int j = i + 1; j < size; j++) {
					float tmp = abs(self[j][i]);
					if (tmp > pivotsize) {
						pivot = j;
						pivotsize = tmp;
					}
				}
				if (abs(pivotsize) < Math::eps) {
					return mat<size, Tx>{};
				}
				if (pivot != i) {
					for (int j = 0; j < size; j++) {
						std::swap(self[i][j], self[pivot][j]);
						std::swap(ret[i][j], ret[pivot][j]);
					}
				}
				for (int j = i + 1; j < size; j++) {
					float f = self[j][i] / self[i][i];
					for (int k = 0; k < size; k++) {
						self[j][k] -= self[i][k] * f;
						ret[j][k] -= ret[i][k] * f;
					}
				}
			}
			for (int i = size - 1; i >= 0; i--) {
				float f;
				f = self[i][i];
				if (f == 0)return mat<size, Tx>{};
				for (int j = 0; j < size; j++) {
					self[i][j] /= f;
					ret[i][j] /= f;
				}
				for (int j = 0; j < i; j++) {
					f = self[j][i];
					for (int k = 0; k < size; k++) {
						self[j][k] -= f * self[i][k];
						ret[j][k] -= f * ret[i][k];
					}
				}
			}
			return ret;
		}
		mat<size, Tx> transpose() {
			mat<size, Tx> ret = {};
			for (int i = 0; i < size; i++) {
				for (int j = 0; j < size; j++) {
					ret[i][j] = (*this)[j][i];
				}
			}
			return ret;
		}
	};

	template<int len, class Tx>
	vec<len, Tx> operator*(Tx k, const vec<len, Tx>& u) {
		vec<len, Tx> ret;
		for (int i = 0; i < len; i++)ret[i] = u[i] * k;
		return ret;
	}

	using vec2 = vec<2, float>;
	using vec2i = vec<2, int>;
	using vec3 = vec<3, float>;
	using vec4 = vec<4, float>;
	using mat4 = mat<4, float>;
};