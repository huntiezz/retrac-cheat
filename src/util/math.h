#pragma once
#include "common.h"
#include <cstddef>
#include <d3d9.h>
#include <xmmintrin.h>
#include <array>
#include <cstdlib>
#include <random>
#include <direct.h>
#include <numbers>
#define M_PI 3.14159265358979323846264338327950288

#include "../menu/ImGui/imgui.h"
#include "../menu/ImGui/imgui_impl_dx11.h"
#include "../menu/ImGui/imgui_impl_win32.h"
#include "../driver/communication.h"

class Vector2 {
public:
	Vector2() : x(0.f), y(0.f) {}
	Vector2(float _x, float _y) : x(_x), y(_y) {}
	~Vector2() {}

	float x;
	float y;
};

class Vector3 {
public:
	Vector3() : x(0.f), y(0.f), z(0.f) {}

	Vector3(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}
	~Vector3() {}

	float x;
	float y;
	float z;

	__forceinline float Dot(Vector3 v) {
		return x * v.x + y * v.y + z * v.z;
	}

	__forceinline float Distance(Vector3 v) {
		return float(sqrtf(powf(v.x - x, 2.0) + powf(v.y - y, 2.0) + powf(v.z - z, 2.0)));
	}

	__forceinline float Length() {
		return sqrt(x * x + y * y + z * z);
	}

	__forceinline Vector3 operator+(Vector3 v) {
		return Vector3(x + v.x, y + v.y, z + v.z);
	}

	__forceinline Vector3 operator-(Vector3 v) {
		return Vector3(x - v.x, y - v.y, z - v.z);
	}

	__forceinline Vector3 operator*(float flNum) { return Vector3(x * flNum, y * flNum, z * flNum); }
    
    __forceinline Vector3 operator/(float flNum) { return Vector3(x / flNum, y / flNum, z / flNum); }
};

struct FPlane : Vector3 {
	float W;

	FPlane() : W(0) {}
	FPlane(float W) : W(W) {}
};

class FMatrix {
public:
	float m[4][4];
	FPlane XPlane, YPlane, ZPlane, WPlane;

	FMatrix() : XPlane(), YPlane(), ZPlane(), WPlane() {}
	FMatrix(FPlane XPlane, FPlane YPlane, FPlane ZPlane, FPlane WPlane)
		: XPlane(XPlane), YPlane(YPlane), ZPlane(ZPlane), WPlane(WPlane) {
	}

	D3DMATRIX ToD3DMATRIX() const {
		D3DMATRIX Result;
		Result._11 = XPlane.x; Result._12 = XPlane.y; Result._13 = XPlane.z; Result._14 = XPlane.W;
		Result._21 = YPlane.x; Result._22 = YPlane.y; Result._23 = YPlane.z; Result._24 = YPlane.W;
		Result._31 = ZPlane.x; Result._32 = ZPlane.y; Result._33 = ZPlane.z; Result._34 = ZPlane.W;
		Result._41 = WPlane.x; Result._42 = WPlane.y; Result._43 = WPlane.z; Result._44 = WPlane.W;
		return Result;
	}
};

struct FQuat
{
	float x;
	float y;
	float z;
	float w;
};

struct FTransform
{
	FQuat rot;
	Vector3 translation;
	char pad[4];
	Vector3 scale;
	char pad1[4];

	D3DMATRIX ToMatrixWithScale() const
	{
		D3DMATRIX m{};
		m._41 = translation.x;
		m._42 = translation.y;
		m._43 = translation.z;
		const float sx = scale.x == 0.f ? 1.f : scale.x;
		const float sy = scale.y == 0.f ? 1.f : scale.y;
		const float sz = scale.z == 0.f ? 1.f : scale.z;
		float x2 = rot.x + rot.x;
		float y2 = rot.y + rot.y;
		float z2 = rot.z + rot.z;
		float xx2 = rot.x * x2;
		float yy2 = rot.y * y2;
		float zz2 = rot.z * z2;
		m._11 = (1.0f - (yy2 + zz2)) * sx;
		m._22 = (1.0f - (xx2 + zz2)) * sy;
		m._33 = (1.0f - (xx2 + yy2)) * sz;
		float yz2 = rot.y * z2;
		float wx2 = rot.w * x2;
		m._32 = (yz2 - wx2) * sz;
		m._23 = (yz2 + wx2) * sy;
		float xy2 = rot.x * y2;
		float wz2 = rot.w * z2;
		m._21 = (xy2 - wz2) * sy;
		m._12 = (xy2 + wz2) * sx;
		float xz2 = rot.x * z2;
		float wy2 = rot.w * y2;
		m._31 = (xz2 + wy2) * sz;
		m._13 = (xz2 - wy2) * sx;
		m._14 = 0.0f;
		m._24 = 0.0f;
		m._34 = 0.0f;
		m._44 = 1.0f;
		return m;
	}
};

static_assert(sizeof(FTransform) == 0x30, "FTransform stride");
static_assert(offsetof(FTransform, translation) == 0x10, "FTransform translation");
static_assert(offsetof(FTransform, scale) == 0x20, "FTransform scale");

__forceinline D3DMATRIX MatrixMultiplication(D3DMATRIX pm1, D3DMATRIX pm2)
{
    D3DMATRIX pout;
    __m128 row0 = _mm_loadu_ps(&pm2.m[0][0]);
    __m128 row1 = _mm_loadu_ps(&pm2.m[1][0]);
    __m128 row2 = _mm_loadu_ps(&pm2.m[2][0]);
    __m128 row3 = _mm_loadu_ps(&pm2.m[3][0]);
    for (int i = 0; i < 4; i++) {
    __m128 a_row = _mm_loadu_ps(&pm1.m[i][0]);
    __m128 x = _mm_shuffle_ps(a_row, a_row, _MM_SHUFFLE(0, 0, 0, 0));
    __m128 y = _mm_shuffle_ps(a_row, a_row, _MM_SHUFFLE(1, 1, 1, 1));
    __m128 z = _mm_shuffle_ps(a_row, a_row, _MM_SHUFFLE(2, 2, 2, 2));
    __m128 w = _mm_shuffle_ps(a_row, a_row, _MM_SHUFFLE(3, 3, 3, 3));
    __m128 result = _mm_mul_ps(x, row0);
    result = _mm_add_ps(result, _mm_mul_ps(y, row1));
    result = _mm_add_ps(result, _mm_mul_ps(z, row2));
    result = _mm_add_ps(result, _mm_mul_ps(w, row3));
    _mm_storeu_ps(&pout.m[i][0], result);
    }
    return pout;
}

__forceinline D3DMATRIX InverseRotationMatrix(const D3DMATRIX& m) {
	D3DMATRIX r{};
	r.m[0][0] = m.m[0][0];
	r.m[0][1] = m.m[1][0];
	r.m[0][2] = m.m[2][0];
	r.m[0][3] = 0.f;
	r.m[1][0] = m.m[0][1];
	r.m[1][1] = m.m[1][1];
	r.m[1][2] = m.m[2][1];
	r.m[1][3] = 0.f;
	r.m[2][0] = m.m[0][2];
	r.m[2][1] = m.m[1][2];
	r.m[2][2] = m.m[2][2];
	r.m[2][3] = 0.f;
	r.m[3][0] = 0.f;
	r.m[3][1] = 0.f;
	r.m[3][2] = 0.f;
	r.m[3][3] = 1.f;
	return r;
}

__forceinline D3DMATRIX TranslationMatrix(float tx, float ty, float tz) {
	D3DMATRIX m{};
	m.m[0][0] = m.m[1][1] = m.m[2][2] = m.m[3][3] = 1.f;
	m.m[3][0] = tx;
	m.m[3][1] = ty;
	m.m[3][2] = tz;
	return m;
}

__forceinline D3DMATRIX Matrix(Vector3 rot, Vector3 origin = Vector3(0, 0, 0))
{
	Vector3 scale;

	Vector3 AdjustedScale
	(
		(scale.x == 0.0) ? 1.0 : scale.x,
		(scale.y == 0.0) ? 1.0 : scale.y,
		(scale.z == 0.0) ? 1.0 : scale.z
	);

	float radpitch = (rot.x * M_PI / 180);
	float radyaw = (rot.y * M_PI / 180);
	float radroll = (rot.z * M_PI / 180);
	float sp = sinf(radpitch);
	float cp = cosf(radpitch);
	float sy = sinf(radyaw);
	float cy = cosf(radyaw);
	float sr = sinf(radroll);
	float cr = cosf(radroll);
	D3DMATRIX matrix{};
	matrix.m[0][0] = cp * cy;
	matrix.m[0][1] = cp * sy;
	matrix.m[0][2] = sp;
	matrix.m[0][3] = 0.f;
	matrix.m[1][0] = sr * sp * cy - cr * sy;
	matrix.m[1][1] = sr * sp * sy + cr * cy;
	matrix.m[1][2] = -sr * cp;
	matrix.m[1][3] = 0.f;
	matrix.m[2][0] = -(cr * sp * cy + sr * sy);
	matrix.m[2][1] = cy * sr - cr * sp * sy;
	matrix.m[2][2] = cr * cp;
	matrix.m[2][3] = 0.f;
	matrix.m[3][0] = origin.x;
	matrix.m[3][1] = origin.y;
	matrix.m[3][2] = origin.z;
	matrix.m[3][3] = 1.f;
	return matrix;
}

// View axes match legacy W2S: X=right(row1), Y=up(row2), Z=forward(row0) depth for clip.W.
__forceinline D3DMATRIX BuildUnrealViewMatrix(const Vector3& location, const Vector3& rotation) {
	const D3DMATRIX axes = Matrix(rotation);
	const Vector3 forward(axes.m[0][0], axes.m[0][1], axes.m[0][2]);
	const Vector3 right(axes.m[1][0], axes.m[1][1], axes.m[1][2]);
	const Vector3 up(axes.m[2][0], axes.m[2][1], axes.m[2][2]);
	D3DMATRIX m{};
	m.m[0][0] = right.x;
	m.m[1][0] = right.y;
	m.m[2][0] = right.z;
	m.m[3][0] = -(right.x * location.x + right.y * location.y + right.z * location.z);
	m.m[0][1] = up.x;
	m.m[1][1] = up.y;
	m.m[2][1] = up.z;
	m.m[3][1] = -(up.x * location.x + up.y * location.y + up.z * location.z);
	m.m[0][2] = forward.x;
	m.m[1][2] = forward.y;
	m.m[2][2] = forward.z;
	m.m[3][2] = -(forward.x * location.x + forward.y * location.y + forward.z * location.z);
	m.m[3][3] = 1.f;
	return m;
}

// FMinimalViewInfo::FOV is horizontal (degrees). Perspective matrix below uses vertical FOV.
__forceinline float HorizontalFovDegreesToVertical(float hFovDegrees, float aspectRatio) {
	if (!(aspectRatio >= 1e-3f) || !(hFovDegrees >= 1.f))
		return hFovDegrees;
	const float hHalf = hFovDegrees * (float)M_PI / 360.f;
	const float vHalf = atanf(tanf(hHalf) / aspectRatio);
	return vHalf * 360.f / (float)M_PI;
}

// Vertical FOV in degrees (width/height = aspectRatio).
__forceinline D3DMATRIX BuildUnrealReversedZPerspectiveMatrix(float verticalFovDegrees,
                                                              float aspectRatio,
                                                              float nearClip = 1.f) {
	const float tanHalf = tanf(verticalFovDegrees * (float)M_PI / 360.f);
	if (tanHalf < 1e-6f || aspectRatio < 1e-3f)
		return D3DMATRIX{};
	const float invTan = 1.f / tanHalf;
	D3DMATRIX m{};
	m.m[0][0] = invTan / aspectRatio;
	m.m[1][1] = invTan;
	m.m[2][3] = 1.f;
	m.m[3][2] = nearClip;
	return m;
}

// FMinimalViewInfo::FOV — horizontal degrees (matches UE FPerspectiveMatrix / FReversedZPerspectiveMatrix).
__forceinline D3DMATRIX BuildUnrealReversedZPerspectiveMatrixHorizontal(float horizontalFovDegrees,
                                                                        float aspectRatio,
                                                                        float nearClip = 1.f) {
	const float tanHalf = tanf(horizontalFovDegrees * (float)M_PI / 360.f);
	if (tanHalf < 1e-6f || aspectRatio < 1e-3f)
		return D3DMATRIX{};
	const float invTan = 1.f / tanHalf;
	D3DMATRIX m{};
	m.m[0][0] = invTan;
	m.m[1][1] = invTan / aspectRatio;
	m.m[2][3] = 1.f;
	m.m[3][2] = nearClip;
	return m;
}

__forceinline void UnrealTransformFVector4(const D3DMATRIX& m, float vx, float vy, float vz, float vw,
                                           float out[4]) {
	out[0] = vx * m.m[0][0] + vy * m.m[1][0] + vz * m.m[2][0] + vw * m.m[3][0];
	out[1] = vx * m.m[0][1] + vy * m.m[1][1] + vz * m.m[2][1] + vw * m.m[3][1];
	out[2] = vx * m.m[0][2] + vy * m.m[1][2] + vz * m.m[2][2] + vw * m.m[3][2];
	out[3] = vx * m.m[0][3] + vy * m.m[1][3] + vz * m.m[2][3] + vw * m.m[3][3];
}

__forceinline D3DMATRIX BuildUnrealViewProjectionMatrix(const Vector3& location, const Vector3& rotation,
                                                        float horizontalFovDegrees, float aspectRatio,
                                                        float nearClip = 1.f) {
	const D3DMATRIX view = BuildUnrealViewMatrix(location, rotation);
	const D3DMATRIX proj = BuildUnrealReversedZPerspectiveMatrixHorizontal(
	    horizontalFovDegrees, aspectRatio, nearClip);
	return MatrixMultiplication(view, proj);
}

__forceinline bool Matrix4Finite(const D3DMATRIX& m) {
	for (int r = 0; r < 4; ++r) {
		for (int c = 0; c < 4; ++c) {
			if (!std::isfinite(m.m[r][c]))
				return false;
		}
	}
	return true;
}

// Clip-space frustum gate shared by W2S and boot self-tests (NDC in [-1, 1], finite W).
__forceinline bool WorldPointVisibleInCameraFrustum(const D3DMATRIX& viewProjection,
                                                    const Vector3& world, float* outNdcX = nullptr,
                                                    float* outNdcY = nullptr) {
	if (!Matrix4Finite(viewProjection))
		return false;
	float clip[4]{};
	UnrealTransformFVector4(viewProjection, world.x, world.y, world.z, 1.f, clip);
	if (!std::isfinite(clip[3]) || clip[3] <= 1e-4f)
		return false;
	const float ndcX = clip[0] / clip[3];
	const float ndcY = clip[1] / clip[3];
	if (!std::isfinite(ndcX) || !std::isfinite(ndcY))
		return false;
	if (outNdcX)
		*outNdcX = ndcX;
	if (outNdcY)
		*outNdcY = ndcY;
	return ndcX >= -1.f && ndcX <= 1.f && ndcY >= -1.f && ndcY <= 1.f;
}

__forceinline bool ProjectWorldWithViewProjection(const D3DMATRIX& viewProjection, float viewMinX,
                                                  float viewMinY, float viewWidth, float viewHeight,
                                                  const Vector3& world, Vector3* outScreen) {
	if (!outScreen || viewWidth < 1.f || viewHeight < 1.f)
		return false;
	if (!Matrix4Finite(viewProjection))
		return false;
	float clip[4];
	UnrealTransformFVector4(viewProjection, world.x, world.y, world.z, 1.f, clip);
	if (!std::isfinite(clip[0]) || !std::isfinite(clip[1]) || !std::isfinite(clip[2]) ||
	    !std::isfinite(clip[3]))
		return false;
	if (clip[3] <= 1e-4f)
		return false;
	const float ndcX = clip[0] / clip[3];
	const float ndcY = clip[1] / clip[3];
	outScreen->x = viewMinX + (0.5f + ndcX * 0.5f) * viewWidth;
	outScreen->y = viewMinY + (0.5f - ndcY * 0.5f) * viewHeight;
	outScreen->z = 0.f;
	return true;
}

template<class type>
class tarray {
public:
	tarray() : data(nullptr), count(std::int32_t()), maxx(std::int32_t()) {}
	tarray(type* data, std::int32_t count, std::int32_t maxx) : data(data), count(count), maxx(maxx) {}

	const bool is_valid() const noexcept {
		return !(this->data == nullptr);
	}

	const std::int32_t size() const noexcept {
		return this->count;
	}

	type& operator[](std::int32_t index) noexcept {
		return this->data[index];
	}

	const type& operator[](std::int32_t index) const noexcept {
		return this->data[index];
	}

	bool is_valid_index(std::int32_t index) const noexcept {
		return index < this->size();
	}

	type* data;
	std::int32_t count;
	std::int32_t maxx;
};

inline Vector3 CalcAngle(Vector3 Src, Vector3 Dst) {
	Vector3 angle;
	Vector3 delta = Dst - Src;
	float hyp = sqrt(delta.x * delta.x + delta.y * delta.y);
	angle.x = atan(delta.z / hyp) * (180.0f / M_PI);
	angle.y = atan2(delta.y, delta.x) * (180.0f / M_PI);
	angle.z = 0;
    if (angle.x > 90.f) angle.x -= 360.f;
    if (angle.x < -90.f) angle.x += 360.f;
	return angle;
}
