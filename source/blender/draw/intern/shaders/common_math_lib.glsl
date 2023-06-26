
/* WORKAROUND: to guard against double include in EEVEE. */
#ifndef COMMON_MATH_LIB_GLSL
#define COMMON_MATH_LIB_GLSL

/* ---------------------------------------------------------------------- */
/** \name Common Math Utilities
 * \{ */

#define M_PI 3.14159265358979323846      /* pi */
#define M_2PI 6.28318530717958647692     /* 2*pi */
#define M_PI_2 1.57079632679489661923    /* pi/2 */
#define M_PI_4 0.78539816339744830962    /* pi/4 */
#define M_1_PI 0.318309886183790671538   /* 1/pi */
#define M_1_2PI 0.159154943091895335768  /* 1/(2*pi) */
#define M_1_PI2 0.101321183642337771443  /* 1/(pi^2) */
#define M_SQRT2 1.41421356237309504880   /* sqrt(2) */
#define M_SQRT1_2 0.70710678118654752440 /* 1/sqrt(2) */
#ifndef FLT_MAX
#  define FLT_MAX 3.402823e+38
#  define FLT_MIN 1.175494e-38
#  define FLT_EPSILON 1.192092896e-07F
#endif

vec3 mul(mat3 m, vec3 v)
{
  return m * v;
}
mat3 mul(mat3 m1, mat3 m2)
{
  return m1 * m2;
}
/* WORKAROUND: To be removed once we port all code to use gpu_shader_math_base_lib.glsl. */
#ifndef GPU_SHADER_MATH_MATRIX_LIB_GLSL
vec3 transform_direction(mat4 m, vec3 v)
{
  return mat3(m) * v;
}
vec3 transform_point(mat4 m, vec3 v)
{
  return (m * vec4(v, 1.0)).xyz;
}
vec3 project_point(mat4 m, vec3 v)
{
  vec4 tmp = m * vec4(v, 1.0);
  return tmp.xyz / tmp.w;
}
#endif

mat2 rot2_from_angle(float a)
{
  float c = cos(a);
  float s = sin(a);
  return mat2(c, -s, s, c);
}

/* Computes the full argmax of the given vector, that is, the index of the greatest component will
 * be in the returned x component, the index of the smallest component will be in the returned z
 * component, and the index of the middle component will be in the returned y component.
 *
 * This is computed by utilizing the fact that booleans are converted to the integers 0 and 1 for
 * false and true respectively. So if we compare every component to all other components using the
 * greaterThan comparator, we get 0 for the greatest component, because no other component is
 * greater, 1 for the middle component, and 2 for the smallest component. */
ivec3 argmax(vec3 v)
{
  return ivec3(greaterThan(v, v.xxx)) + ivec3(greaterThan(v, v.yyy)) +
         ivec3(greaterThan(v, v.zzz));
}

#define min3(a, b, c) min(a, min(b, c))
#define min4(a, b, c, d) min(a, min3(b, c, d))
#define min5(a, b, c, d, e) min(a, min4(b, c, d, e))
#define min6(a, b, c, d, e, f) min(a, min5(b, c, d, e, f))
#define min7(a, b, c, d, e, f, g) min(a, min6(b, c, d, e, f, g))
#define min8(a, b, c, d, e, f, g, h) min(a, min7(b, c, d, e, f, g, h))
#define min9(a, b, c, d, e, f, g, h, i) min(a, min8(b, c, d, e, f, g, h, i))

#define max3(a, b, c) max(a, max(b, c))
#define max4(a, b, c, d) max(a, max3(b, c, d))
#define max5(a, b, c, d, e) max(a, max4(b, c, d, e))
#define max6(a, b, c, d, e, f) max(a, max5(b, c, d, e, f))
#define max7(a, b, c, d, e, f, g) max(a, max6(b, c, d, e, f, g))
#define max8(a, b, c, d, e, f, g, h) max(a, max7(b, c, d, e, f, g, h))
#define max9(a, b, c, d, e, f, g, h, i) max(a, max8(b, c, d, e, f, g, h, i))

#define avg3(a, b, c) (a + b + c) * (1.0 / 3.0)
#define avg4(a, b, c, d) (a + b + c + d) * (1.0 / 4.0)
#define avg5(a, b, c, d, e) (a + b + c + d + e) * (1.0 / 5.0)
#define avg6(a, b, c, d, e, f) (a + b + c + d + e + f) * (1.0 / 6.0)
#define avg7(a, b, c, d, e, f, g) (a + b + c + d + e + f + g) * (1.0 / 7.0)
#define avg8(a, b, c, d, e, f, g, h) (a + b + c + d + e + f + g + h) * (1.0 / 8.0)
#define avg9(a, b, c, d, e, f, g, h, i) (a + b + c + d + e + f + g + h + i) * (1.0 / 9.0)

/* clang-format off */
#define min_v2(v) min((v).x, (v).y)
#define min_v3(v) min((v).x, min((v).y, (v).z))
#define min_v4(v) min(min((v).x, (v).y), min((v).z, (v).w))
#define max_v2(v) max((v).x, (v).y)
#define max_v3(v) max((v).x, max((v).y, (v).z))
#define max_v4(v) max(max((v).x, (v).y), max((v).z, (v).w))

float sum(vec2 v) { return dot(vec2(1.0), v); }
float sum(vec3 v) { return dot(vec3(1.0), v); }
float sum(vec4 v) { return dot(vec4(1.0), v); }

float avg(vec2 v) { return dot(vec2(1.0 / 2.0), v); }
float avg(vec3 v) { return dot(vec3(1.0 / 3.0), v); }
float avg(vec4 v) { return dot(vec4(1.0 / 4.0), v); }

/* WORKAROUND: To be removed once we port all code to use gpu_shader_math_base_lib.glsl. */
#ifndef GPU_SHADER_MATH_BASE_LIB_GLSL
float safe_rcp(float a) { return (a != 0.0) ? (1.0 / a) : 0.0; }
#endif
#ifndef GPU_SHADER_MATH_VECTOR_LIB_GLSL
vec2 safe_rcp(vec2 a) { return select(vec2(0.0), (1.0 / a), notEqual(a, vec2(0.0))); }
vec3 safe_rcp(vec3 a) { return select(vec3(0.0), (1.0 / a), notEqual(a, vec3(0.0))); }
vec4 safe_rcp(vec4 a) { return select(vec4(0.0), (1.0 / a), notEqual(a, vec4(0.0))); }
#endif

float safe_sqrt(float a) { return sqrt(max(a, 0.0)); }

float safe_acos(float a) { return acos(clamp(a, -1.0, 1.0)); }

float sqr(float a) { return a * a; }
vec2 sqr(vec2 a) { return a * a; }
vec3 sqr(vec3 a) { return a * a; }
vec4 sqr(vec4 a) { return a * a; }

/* Use manual powers for fixed powers. pow() can have unpredictable results on some implementations.
 * (see #87369, #87541) */
float pow6(float x) { return sqr(sqr(x) * x); }
float pow8(float x) { return sqr(sqr(sqr(x))); }

float len_squared(vec3 a) { return dot(a, a); }
float len_squared(vec2 a) { return dot(a, a); }

/* WORKAROUND: To be removed once we port all code to use gpu_shader_math_base_lib.glsl. */
#ifndef GPU_SHADER_UTILDEFINES_GLSL
bool flag_test(uint flag, uint val) { return (flag & val) != 0u; }
bool flag_test(int flag, uint val) { return flag_test(uint(flag), val); }
bool flag_test(int flag, int val) { return (flag & val) != 0; }

void set_flag_from_test(inout uint value, bool test, uint flag) { if (test) { value |= flag; } else { value &= ~flag; } }
void set_flag_from_test(inout int value, bool test, int flag) { if (test) { value |= flag; } else { value &= ~flag; } }
#endif

#define weighted_sum(val0, val1, val2, val3, weights) ((val0 * weights[0] + val1 * weights[1] + val2 * weights[2] + val3 * weights[3]) * safe_rcp(sum(weights)))
#define weighted_sum_array(val, weights) ((val[0] * weights[0] + val[1] * weights[1] + val[2] * weights[2] + val[3] * weights[3]) * safe_rcp(sum(weights)))

/* clang-format on */

#define saturate(a) clamp(a, 0.0, 1.0)

#define in_range_inclusive(val, min_v, max_v) \
  (all(greaterThanEqual(val, min_v)) && all(lessThanEqual(val, max_v)))
#define in_range_exclusive(val, min_v, max_v) \
  (all(greaterThan(val, min_v)) && all(lessThan(val, max_v)))
#define in_texture_range(texel, tex) \
  (all(greaterThanEqual(texel, ivec2(0))) && all(lessThan(texel, textureSize(tex, 0).xy)))

/* WORKAROUND: To be removed once we port all code to use gpu_shader_math_base_lib.glsl. */
#ifndef GPU_SHADER_MATH_BASE_LIB_GLSL
uint divide_ceil(uint visible_count, uint divisor)
{
  return (visible_count + (divisor - 1u)) / divisor;
}

int divide_ceil(int visible_count, int divisor)
{
  return (visible_count + (divisor - 1)) / divisor;
}
#endif

/* WORKAROUND: To be removed once we port all code to use gpu_shader_math_base_lib.glsl. */
#ifndef GPU_SHADER_MATH_VECTOR_LIB_GLSL
ivec2 divide_ceil(ivec2 visible_count, ivec2 divisor)
{
  return (visible_count + (divisor - 1)) / divisor;
}
#endif

uint bit_field_mask(uint bit_width, uint bit_min)
{
  /* Cannot bit shift more than 31 positions. */
  uint mask = (bit_width > 31u) ? 0x0u : (0xFFFFFFFFu << bit_width);
  return ~mask << bit_min;
}

/* WORKAROUND: To be removed once we port all code to use gpu_shader_math_base_lib.glsl. */
#ifndef GPU_SHADER_UTILDEFINES_GLSL
uvec2 unpackUvec2x16(uint data)
{
  return (uvec2(data) >> uvec2(0u, 16u)) & uvec2(0xFFFFu);
}

uint packUvec2x16(uvec2 data)
{
  data = (data & 0xFFFFu) << uvec2(0u, 16u);
  return data.x | data.y;
}

uvec4 unpackUvec4x8(uint data)
{
  return (uvec4(data) >> uvec4(0u, 8u, 16u, 24u)) & uvec4(0xFFu);
}

uint packUvec4x8(uvec4 data)
{
  data = (data & 0xFFu) << uvec4(0u, 8u, 16u, 24u);
  return data.x | data.y | data.z | data.w;
}
#endif

/* WORKAROUND: To be removed once we port all code to use gpu_shader_math_base_lib.glsl. */
#ifndef GPU_SHADER_MATH_VECTOR_LIB_GLSL
float distance_squared(vec2 a, vec2 b)
{
  a -= b;
  return dot(a, a);
}

float distance_squared(vec3 a, vec3 b)
{
  a -= b;
  return dot(a, a);
}

vec3 safe_normalize(vec3 v)
{
  float len = length(v);
  if (isnan(len) || len == 0.0) {
    return vec3(1.0, 0.0, 0.0);
  }
  return v / len;
}
#endif

vec2 safe_normalize_len(vec2 v, out float len)
{
  len = length(v);
  if (isnan(len) || len == 0.0) {
    return vec2(1.0, 0.0);
  }
  return v / len;
}

/* WORKAROUND: To be removed once we port all code to use gpu_shader_math_base_lib.glsl. */
#ifndef GPU_SHADER_MATH_VECTOR_LIB_GLSL
vec2 safe_normalize(vec2 v)
{
  float len;
  return safe_normalize_len(v, len);
}
#endif

vec3 normalize_len(vec3 v, out float len)
{
  len = length(v);
  return v / len;
}

vec4 safe_color(vec4 c)
{
  /* Clamp to avoid black square artifacts if a pixel goes NaN. */
  return clamp(c, vec4(0.0), vec4(1e20)); /* 1e20 arbitrary. */
}
vec3 safe_color(vec3 c)
{
  /* Clamp to avoid black square artifacts if a pixel goes NaN. */
  return clamp(c, vec3(0.0), vec3(1e20)); /* 1e20 arbitrary. */
}

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Fast Math
 * \{ */

/* [Drobot2014a] Low Level Optimizations for GCN */
float fast_sqrt(float v)
{
  return intBitsToFloat(0x1fbd1df5 + (floatBitsToInt(v) >> 1));
}

vec2 fast_sqrt(vec2 v)
{
  return intBitsToFloat(0x1fbd1df5 + (floatBitsToInt(v) >> 1));
}

/* [Eberly2014] GPGPU Programming for Games and Science */
float fast_acos(float v)
{
  float res = -0.156583 * abs(v) + M_PI_2;
  res *= fast_sqrt(1.0 - abs(v));
  return (v >= 0) ? res : M_PI - res;
}

vec2 fast_acos(vec2 v)
{
  vec2 res = -0.156583 * abs(v) + M_PI_2;
  res *= fast_sqrt(1.0 - abs(v));
  v.x = (v.x >= 0) ? res.x : M_PI - res.x;
  v.y = (v.y >= 0) ? res.y : M_PI - res.y;
  return v;
}

/** \} */

/*
 * For debugging purpose mainly.
 * From https://www.shadertoy.com/view/4dsSzr
 * By Morgan McGuire @morgan3d, http://graphicscodex.com
 * Reuse permitted under the BSD license.
 */
vec3 neon_gradient(float t)
{
  return clamp(vec3(t * 1.3 + 0.1, sqr(abs(0.43 - t) * 1.7), (1.0 - t) * 1.7), 0.0, 1.0);
}
vec3 heatmap_gradient(float t)
{
  float a = pow(t, 1.5) * 0.8 + 0.2;
  float b = smoothstep(0.0, 0.35, t) + t * 0.5;
  float c = smoothstep(0.5, 1.0, t);
  float d = max(1.0 - t * 1.7, t * 7.0 - 6.0);
  return saturate(a * vec3(b, c, d));
}
vec3 hue_gradient(float t)
{
  vec3 p = abs(fract(t + vec3(1.0, 2.0 / 3.0, 1.0 / 3.0)) * 6.0 - 3.0);
  return (clamp(p - 1.0, 0.0, 1.0));
}

#endif /* COMMON_MATH_LIB_GLSL */
