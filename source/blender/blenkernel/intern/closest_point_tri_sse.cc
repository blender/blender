// see Fast Distance Queries for Triangles, Lines, and
// Points using SSE Instructions
// http://jcgt.org/published/0003/04/05/paper.pdf

typedef struct DistRet {
  float dist[4];
} DistRet;

#if defined(__SSE2__) || defined(__SSE4__)
#  include <cstdint>
#  include <immintrin.h>
#  include <smmintrin.h>
#  include <xmmintrin.h>

struct sseb {
  // data
  union {
    __m128 m128;
    uint32_t v[4];
  };

  sseb()
  {
  }

  sseb(__m128 m)
  {
    m128 = m;
  }
};

struct ssef {
  // data
  union {
    __m128 m128;
    float v[4];
    int i[4];
  };

  ssef(float a, float b, float c, float d)
  {
    v[0] = a;
    v[1] = b;
    v[2] = c;
    v[3] = d;
  }

  ssef()
  {
  }

  ssef(__m128 f)
  {
    m128 = f;
  }

  ssef(float f)
  {
    v[0] = v[1] = v[2] = v[3] = f;
  }
};
#endif

#if defined(__SSE2__) && !defined(__SSE4__)

__m128 my_mm_blendv_ps(__m128 a, __m128 b, __m128 mask)
{
  ssef fa(a);
  ssef fb(b);
  sseb fm(mask);

  ssef ret;
  for (int i = 0; i < 4; i++) {
    if (fm.v[i] & (1 << 31)) {
      ret.v[i] = fb.v[i];
    }
    else {
      ret.v[i] = fa.v[i];
    }
  }

  return ret.m128;
}
#  define _mm_blendv_ps my_mm_blendv_ps
#endif

#ifdef __SSE2__

#  include <array>
#  include <vector>

static inline struct sseb makeSSSEB(__m128 val)
{
  sseb r;
  r.m128 = val;

  return r;
}

// operations
const sseb operator&(const sseb &a, const sseb &b)
{
  return makeSSSEB(_mm_and_ps(a.m128, b.m128));
}
const sseb operator|(const sseb &a, const sseb &b)
{
  return makeSSSEB(_mm_or_ps(a.m128, b.m128));
}
const sseb operator|=(const sseb &a, const sseb &b)
{
  return a | b;
}
const sseb operator^(const sseb &a, const sseb &b)
{
  return makeSSSEB(_mm_xor_ps(a.m128, b.m128));
}
bool all(const sseb &b)
{
  return _mm_movemask_ps(b.m128) == 0xf;
}
bool any(const sseb &b)
{
  return _mm_movemask_ps(b.m128) != 0x0;
}
bool none(const sseb &b)
{
  return _mm_movemask_ps(b.m128) == 0x0;
}
static inline struct ssef makeSSSEF(__m128 val)
{
  ssef r;
  r.m128 = val;

  return r;
}

// operations
const ssef operator+(const ssef &a, const ssef &b)
{
  return makeSSSEF(_mm_add_ps(a.m128, b.m128));
}
const ssef operator-(const ssef &a, const ssef &b)
{
  return makeSSSEF(_mm_sub_ps(a.m128, b.m128));
}
const ssef operator*(const ssef &a, const ssef &b)
{
  return makeSSSEF(_mm_mul_ps(a.m128, b.m128));
}
const ssef operator/(const ssef &a, const ssef &b)
{
  return makeSSSEF(_mm_div_ps(a.m128, b.m128));
}

const sseb operator>=(const ssef &a, const ssef &b)
{
  __m128 r1 = _mm_cmpgt_ss(a.m128, b.m128);
  __m128 r2 = _mm_cmpeq_ss(a.m128, b.m128);

  return makeSSSEB(_mm_or_ps(r1, r2));
}
const sseb operator<=(const ssef &a, const ssef &b)
{
  __m128 r1 = _mm_cmplt_ss(a.m128, b.m128);
  __m128 r2 = _mm_cmpeq_ss(a.m128, b.m128);

  return makeSSSEB(_mm_or_ps(r1, r2));
}

const sseb operator>(const ssef &a, const ssef &b)
{
  return makeSSSEB(_mm_cmpgt_ss(a.m128, b.m128));
}

const sseb operator<(const ssef &a, const ssef &b)
{
  return makeSSSEB(_mm_cmplt_ss(a.m128, b.m128));
}

const ssef min(const ssef &a, const ssef &b)
{
  return makeSSSEF(_mm_min_ps(a.m128, b.m128));
}
const ssef sqr(const ssef &a)
{
  return makeSSSEF(_mm_mul_ps(a.m128, a.m128));
}
const ssef sqrt(const ssef &a)
{
  return makeSSSEF(_mm_sqrt_ps(a.m128));
}
const ssef select(const sseb &mask, const ssef &t, const ssef &f)
{
  return makeSSSEF(_mm_blendv_ps(f.m128, t.m128, mask.m128));
}

template<typename T> struct Vec3 {
  // data
  T x, y, z;

  Vec3(T x, T y, T z) : x(x), y(y), z(z)
  {
  }

  Vec3(T v) : x(v), y(v), z(v)
  {
  }
};

// operations
template<typename T> Vec3<T> operator+(const Vec3<T> &a, const Vec3<T> &b)
{
  return Vec3<T>(a.x + b.x, a.y + b.y, a.z + b.z);
}
template<typename T> Vec3<T> operator-(const Vec3<T> &a, const Vec3<T> &b)
{
  return Vec3<T>(a.x - b.x, a.y - b.y, a.z - b.z);
}
template<typename T> Vec3<T> operator*(const Vec3<T> &a, const Vec3<T> &b)
{
  return Vec3<T>(a.x * b.x, a.y * b.y, a.z * b.z);
}
template<typename T> Vec3<T> operator*(const T &a, const Vec3<T> &b)
{
  return Vec3<T>(b.x * a, b.y * a, b.z * a);
}
template<typename T> Vec3<T> operator*(const Vec3<T> &b, const T &a)
{
  return Vec3<T>(b.x * a, b.y * a, b.z * a);
}

template<typename T> inline Vec3<T> rcp(const Vec3<T> &a)
{
  return Vec3<T>(rcp(a.x), rcp(a.y), rcp(a.z));
}
template<typename T> inline Vec3<T> rsqrt(const Vec3<T> &a)
{
  return Vec3<T>(rsqrt(a.x), rsqrt(a.y), rsqrt(a.z));
}
template<typename T> T dot(const Vec3<T> &a, const Vec3<T> &b)
{
  return a.x * b.x + a.y * b.y + a.z * b.z;
}
template<typename T> T length2b(const Vec3<T> &a)
{
  return dot(a, a);
}

typedef sseb simdBool;
typedef ssef simdFloat;
typedef Vec3<ssef> simdFloatVec;
typedef std::array<simdFloatVec, 3> simdTriangle_type;
typedef std::array<simdFloatVec, 2> simdLine_type;
typedef simdFloatVec simdPoint_type;

static inline simdFloat length2(const simdFloatVec &a)
{
  return a.x * a.x + a.y * a.y + a.z * a.z;
}

static simdFloat zero = {0};
static simdFloat one = {1.0f, 1.0f, 1.0f, 1.0f};

const simdFloatVec select(const sseb &mask, const simdFloatVec &t, const simdFloatVec &f)
{
  simdFloatVec r2(select(mask, t.x, f.x), select(mask, t.y, f.y), select(mask, t.z, f.z));

  return r2;
}

template<typename T> T clamp(const T &x, const T &lower, const T &upper)
{
  return max(lower, min(x, upper));
}

const simdFloat simdTriPoint2(simdFloatVec &oTriPoint,
                              const simdTriangle_type &iTri,
                              const simdPoint_type &iPoint)
{
  const simdFloatVec ab = iTri[1] - iTri[0];
  const simdFloatVec ac = iTri[2] - iTri[0];
  const simdFloatVec ap = iPoint - iTri[0];
  const simdFloat d1 = dot(ab, ap);
  const simdFloat d2 = dot(ac, ap);
  const simdBool mask1 = (d1 <= simdFloat(zero)) & (d2 <= simdFloat(zero));
  oTriPoint = iTri[0];
  simdBool exit(mask1);
  if (all(exit))
    return length2(oTriPoint - iPoint);

  const simdFloatVec bp = iPoint - iTri[1];
  const simdFloat d3 = dot(ab, bp);
  const simdFloat d4 = dot(ac, bp);
  const simdBool mask2 = (d3 >= simdFloat(zero)) & (d4 <= d3);
  // Closest point is the point iTri[1]. Update if necessary.
  oTriPoint = select(exit, oTriPoint, select(mask2, iTri[1], oTriPoint));
  exit = exit | mask2;
  if (all(exit))
    return length2(oTriPoint - iPoint);

  const simdFloatVec cp = iPoint - iTri[2];
  const simdFloat d5 = dot(ab, cp);
  const simdFloat d6 = dot(ac, cp);
  const simdBool mask3 = (d6 >= simdFloat(zero)) & (d5 <= d6);
  // Closest point is the point iTri[2]. Update if necessary.
  oTriPoint = select(exit, oTriPoint, select(mask3, iTri[2], oTriPoint));
  exit |= mask3;
  if (all(exit))
    return length2(oTriPoint - iPoint);

  const simdFloat vc = d1 * d4 - d3 * d2;
  const simdBool mask4 = (vc <= simdFloat(zero)) & (d1 >= simdFloat(zero)) &
                         (d3 <= simdFloat(zero));
  const simdFloat v1 = d1 / (d1 - d3);
  const simdFloatVec answer1 = iTri[0] + v1 * ab;
  // Closest point is on the line ab. Update if necessary.
  oTriPoint = select(exit, oTriPoint, select(mask4, answer1, oTriPoint));
  exit |= mask4;
  if (all(exit))
    return length2(oTriPoint - iPoint);

  const simdFloat vb = d5 * d2 - d1 * d6;
  const simdBool mask5 = (vb <= simdFloat(zero)) & (d2 >= simdFloat(zero)) &
                         (d6 <= simdFloat(zero));
  const simdFloat w1 = d2 / (d2 - d6);
  const simdFloatVec answer2 = iTri[0] + w1 * ac;
  // Closest point is on the line ac. Update if necessary.
  oTriPoint = select(exit, oTriPoint, select(mask5, answer2, oTriPoint));
  exit |= mask5;
  if (all(exit))
    return length2(oTriPoint - iPoint);

  const simdFloat va = d3 * d6 - d5 * d4;
  const simdBool mask6 = (va <= simdFloat(zero)) & ((d4 - d3) >= simdFloat(zero)) &
                         ((d5 - d6) >= simdFloat(zero));
  simdFloat w2 = (d4 - d3) / ((d4 - d3) + (d5 - d6));
  const simdFloatVec answer3 = iTri[1] + w2 * (iTri[2] - iTri[1]);
  // Closest point is on the line bc. Update if necessary.
  oTriPoint = select(exit, oTriPoint, select(mask6, answer3, oTriPoint));
  exit |= mask6;
  if (all(exit))
    return length2(oTriPoint - iPoint);

  const simdFloat denom = simdFloat(one) / (va + vb + vc);
  const simdFloat v2 = vb * denom;
  const simdFloat w3 = vc * denom;
  const simdFloatVec answer4 = iTri[0] + ab * v2 + ac * w3;
  const simdBool mask7 = length2(answer4 - iPoint) < length2(oTriPoint - iPoint);
  // Closest point is inside triangle. Update if necessary.
  oTriPoint = select(exit, oTriPoint, select(mask7, answer4, oTriPoint));
  return length2(oTriPoint - iPoint);
}

extern "C" struct DistRet dist_to_tri_sphere_fast_4(
    float p[3], float v1[4][3], float v2[4][3], float v3[4][3], float n[4][3])
{
  simdFloatVec mp((simdFloat(p[0]), simdFloat(p[1]), simdFloat(p[2])));
  simdFloatVec t1(simdFloat(v1[0][0], v1[1][0], v1[2][0], v1[3][0]),
                  simdFloat(v1[0][1], v1[1][1], v1[2][1], v1[3][1]),
                  simdFloat(v1[0][2], v1[1][2], v1[2][2], v1[3][2]));
  simdFloatVec t2(simdFloat(v2[0][0], v2[1][0], v2[2][0], v2[3][0]),
                  simdFloat(v2[0][1], v2[1][1], v2[2][1], v2[3][1]),
                  simdFloat(v2[0][2], v2[1][2], v2[2][2], v2[3][2]));
  simdFloatVec t3(simdFloat(v3[0][0], v3[1][0], v3[2][0], v3[3][0]),
                  simdFloat(v3[0][1], v3[1][1], v3[2][1], v3[3][1]),
                  simdFloat(v3[0][2], v3[1][2], v3[2][2], v3[3][2]));

  simdTriangle_type tri = {t1, t2, t3};

  struct DistRet ret;

  ssef f = simdTriPoint2(mp, tri, mp);

  ret.dist[0] = f.v[0];
  ret.dist[1] = f.v[1];
  ret.dist[2] = f.v[2];
  ret.dist[3] = f.v[3];

  return ret;
}
#else
#include "BLI_math.h"

extern "C" struct DistRet dist_to_tri_sphere_fast_4(
    float p[3], float v1[4][3], float v2[4][3], float v3[4][3], float n[4][3])
{
  DistRet ret;
  float r[3];

  for (int i=0; i<4; i++) {
    closest_on_tri_to_point_v3(r, p, v1[i], v2[i], v3[i]);
    ret.dist[i] = len_squared_v3v3(r, p);
  }

  return ret;
}

#endif
