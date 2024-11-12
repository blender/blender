/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * GLSL to C++ stubs.
 *
 * The goal of this header is to make the GLSL source file compile using a modern C++ compiler.
 * This allows for linting and IDE functionalities to work.
 *
 * This file can be included inside any GLSL file to make the GLSL syntax to work.
 * Then your IDE must to be configured to associate `.glsl` files to C++ so that the
 * C++ linter does the analysis.
 *
 * This is why the implementation of each function is not needed. However, we make sure that type
 * casting is always explicit. This is because implicit casts are not always supported on all
 * implementations.
 *
 * Float types are set to double to accept float literals without trailing f and avoid casting
 * issues.
 *
 * Some of the features of GLSL are omitted by design. They are either:
 * - Not needed (e.g. per component matrix multiplication).
 * - Against our code-style (e.g. `stpq` swizzle).
 * - Unsupported by our Metal Shading Language layer (e.g. mixed vector-scalar matrix constructor).
 *
 * IMPORTANT: Please ask the module team if you need some feature that are not listed in this file.
 */

#pragma once

#include <type_traits>

/* Some compilers complain about lack of return values. Keep it short. */
#define RET \
  { \
    return {}; \
  }

/* -------------------------------------------------------------------- */
/** \name Vector Types
 * \{ */

template<typename T, int Sz> struct VecBase {};

template<typename T, int Sz> struct VecOp {
  using VecT = VecBase<T, Sz>;

  const T &operator[](int) const
  {
    return *reinterpret_cast<const T *>(this);
  }
  T &operator[](int)
  {
    return *reinterpret_cast<T *>(this);
  }

  VecT operator+() RET;
  VecT operator-() RET;

  friend VecT operator+(VecT, VecT) RET;
  friend VecT operator-(VecT, VecT) RET;
  friend VecT operator/(VecT, VecT) RET;
  friend VecT operator*(VecT, VecT) RET;

  friend VecT operator+(VecT, T) RET;
  friend VecT operator-(VecT, T) RET;
  friend VecT operator/(VecT, T) RET;
  friend VecT operator*(VecT, T) RET;

  friend VecT operator+(T, VecT) RET;
  friend VecT operator-(T, VecT) RET;
  friend VecT operator/(T, VecT) RET;
  friend VecT operator*(T, VecT) RET;

  friend VecT operator+=(VecT, VecT) RET;
  friend VecT operator-=(VecT, VecT) RET;
  friend VecT operator/=(VecT, VecT) RET;
  friend VecT operator*=(VecT, VecT) RET;

  friend VecT operator+=(VecT, T) RET;
  friend VecT operator-=(VecT, T) RET;
  friend VecT operator/=(VecT, T) RET;
  friend VecT operator*=(VecT, T) RET;

#define INT_OP \
  template<typename U = T, typename std::enable_if_t<std::is_integral_v<U>> * = nullptr>

  INT_OP friend VecT operator~(VecT) RET;

  INT_OP friend VecT operator%(VecT, VecT) RET;
  INT_OP friend VecT operator&(VecT, VecT) RET;
  INT_OP friend VecT operator|(VecT, VecT) RET;
  INT_OP friend VecT operator^(VecT, VecT) RET;

  INT_OP friend VecT operator%(VecT, T) RET;
  INT_OP friend VecT operator&(VecT, T) RET;
  INT_OP friend VecT operator|(VecT, T) RET;
  INT_OP friend VecT operator^(VecT, T) RET;

  INT_OP friend VecT operator%(T, VecT) RET;
  INT_OP friend VecT operator&(T, VecT) RET;
  INT_OP friend VecT operator|(T, VecT) RET;
  INT_OP friend VecT operator^(T, VecT) RET;

  INT_OP friend VecT operator%=(VecT, VecT) RET;
  INT_OP friend VecT operator&=(VecT, VecT) RET;
  INT_OP friend VecT operator|=(VecT, VecT) RET;
  INT_OP friend VecT operator^=(VecT, VecT) RET;

  INT_OP friend VecT operator%=(VecT, T) RET;
  INT_OP friend VecT operator&=(VecT, T) RET;
  INT_OP friend VecT operator|=(VecT, T) RET;
  INT_OP friend VecT operator^=(VecT, T) RET;

  INT_OP friend VecT operator<<(VecT, VecT) RET;
  INT_OP friend VecT operator>>(VecT, VecT) RET;
  INT_OP friend VecT operator<<=(VecT, VecT) RET;
  INT_OP friend VecT operator>>=(VecT, VecT) RET;

  INT_OP friend VecT operator<<(T, VecT) RET;
  INT_OP friend VecT operator>>(T, VecT) RET;
  INT_OP friend VecT operator<<=(T, VecT) RET;
  INT_OP friend VecT operator>>=(T, VecT) RET;

  INT_OP friend VecT operator<<(VecT, T) RET;
  INT_OP friend VecT operator>>(VecT, T) RET;
  INT_OP friend VecT operator<<=(VecT, T) RET;
  INT_OP friend VecT operator>>=(VecT, T) RET;

#undef INT_OP
};

template<typename T> struct VecSwizzle2 {
  static VecBase<T, 2> &xx, &xy, &yx, &yy;
  static VecBase<T, 3> &xxx, &xxy, &xyx, &xyy, &yxx, &yxy, &yyx, &yyy;
  static VecBase<T, 4> &xxxx, &xxxy, &xxyx, &xxyy, &xyxx, &xyxy, &xyyx, &xyyy, &yxxx, &yxxy, &yxyx,
      &yxyy, &yyxx, &yyxy, &yyyx, &yyyy;
};

template<typename T> struct ColSwizzle2 {
  static VecBase<T, 2> &rr, &rg, &gr, &gg;
  static VecBase<T, 3> &rrr, &rrg, &rgr, &rgg, &grr, &grg, &ggr, &ggg;
  static VecBase<T, 4> &rrrr, &rrrg, &rrgr, &rrgg, &rgrr, &rgrg, &rggr, &rggg, &grrr, &grrg, &grgr,
      &grgg, &ggrr, &ggrg, &gggr, &gggg;
};

template<typename T> struct VecSwizzle3 : VecSwizzle2<T> {
  static VecBase<T, 2> &xz, &yz, &zx, &zy, &zz, &zw;
  static VecBase<T, 3> &xxz, &xyz, &xzx, &xzy, &xzz, &yxz, &yyz, &yzx, &yzy, &yzz, &zxx, &zxy,
      &zxz, &zyx, &zyy, &zyz, &zzx, &zzy, &zzz;
  static VecBase<T, 4> &xxxz, &xxyz, &xxzx, &xxzy, &xxzz, &xyxz, &xyyz, &xyzx, &xyzy, &xyzz, &xzxx,
      &xzxy, &xzxz, &xzyx, &xzyy, &xzyz, &xzzx, &xzzy, &xzzz, &yxxz, &yxyz, &yxzx, &yxzy, &yxzz,
      &yyxz, &yyyz, &yyzx, &yyzy, &yyzz, &yzxx, &yzxy, &yzxz, &yzyx, &yzyy, &yzyz, &yzzx, &yzzy,
      &yzzz, &zxxx, &zxxy, &zxxz, &zxyx, &zxyy, &zxyz, &zxzx, &zxzy, &zxzz, &zyxx, &zyxy, &zyxz,
      &zyyx, &zyyy, &zyyz, &zyzx, &zyzy, &zyzz, &zzxx, &zzxy, &zzxz, &zzyx, &zzyy, &zzyz, &zzzx,
      &zzzy, &zzzz;
};

template<typename T> struct ColSwizzle3 : ColSwizzle2<T> {
  static VecBase<T, 2> &rb, &gb, &br, &bg, &bb, &bw;
  static VecBase<T, 3> &rrb, &rgb, &rbr, &rbg, &rbb, &grb, &ggb, &gbr, &gbg, &gbb, &brr, &brg,
      &brb, &bgr, &bgg, &bgb, &bbr, &bbg, &bbb;
  static VecBase<T, 4> &rrrb, &rrgb, &rrbr, &rrbg, &rrbb, &rgrb, &rggb, &rgbr, &rgbg, &rgbb, &rbrr,
      &rbrg, &rbrb, &rbgr, &rbgg, &rbgb, &rbbr, &rbbg, &rbbb, &grrb, &grgb, &grbr, &grbg, &grbb,
      &ggrb, &gggb, &ggbr, &ggbg, &ggbb, &gbrr, &gbrg, &gbrb, &gbgr, &gbgg, &gbgb, &gbbr, &gbbg,
      &gbbb, &brrr, &brrg, &brrb, &brgr, &brgg, &brgb, &brbr, &brbg, &brbb, &bgrr, &bgrg, &bgrb,
      &bggr, &bggg, &bggb, &bgbr, &bgbg, &bgbb, &bbrr, &bbrg, &bbrb, &bbgr, &bbgg, &bbgb, &bbbr,
      &bbbg, &bbbb;
};

template<typename T> struct VecSwizzle4 : VecSwizzle3<T> {
  static VecBase<T, 2> &xw, &yw, &wx, &wy, &wz, &ww;
  static VecBase<T, 3> &xxw, &xyw, &xzw, &xwx, &xwy, &xwz, &xww, &yxw, &yyw, &yzw, &ywx, &ywy,
      &ywz, &yww, &zxw, &zyw, &zzw, &zwx, &zwy, &zwz, &zww, &wxx, &wxy, &wxz, &wxw, &wyx, &wyy,
      &wyz, &wyw, &wzx, &wzy, &wzz, &wzw, &wwx, &wwy, &wwz, &www;
  static VecBase<T, 4> &xxxw, &xxyw, &xxzw, &xxwx, &xxwy, &xxwz, &xxww, &xyxw, &xyyw, &xyzw, &xywx,
      &xywy, &xywz, &xyww, &xzxw, &xzyw, &xzzw, &xzwx, &xzwy, &xzwz, &xzww, &xwxx, &xwxy, &xwxz,
      &xwxw, &xwyx, &xwyy, &xwyz, &xwyw, &xwzx, &xwzy, &xwzz, &xwzw, &xwwx, &xwwy, &xwwz, &xwww,
      &yxxw, &yxyw, &yxzw, &yxwx, &yxwy, &yxwz, &yxww, &yyxw, &yyyw, &yyzw, &yywx, &yywy, &yywz,
      &yyww, &yzxw, &yzyw, &yzzw, &yzwx, &yzwy, &yzwz, &yzww, &ywxx, &ywxy, &ywxz, &ywxw, &ywyx,
      &ywyy, &ywyz, &ywyw, &ywzx, &ywzy, &ywzz, &ywzw, &ywwx, &ywwy, &ywwz, &ywww, &zxxw, &zxyw,
      &zxzw, &zxwx, &zxwy, &zxwz, &zxww, &zyxw, &zyyw, &zyzw, &zywx, &zywy, &zywz, &zyww, &zzxw,
      &zzyw, &zzzw, &zzwx, &zzwy, &zzwz, &zzww, &zwxx, &zwxy, &zwxz, &zwxw, &zwyx, &zwyy, &zwyz,
      &zwyw, &zwzx, &zwzy, &zwzz, &zwzw, &zwwx, &zwwy, &zwwz, &zwww, &wxxx, &wxxy, &wxxz, &wxxw,
      &wxyx, &wxyy, &wxyz, &wxyw, &wxzx, &wxzy, &wxzz, &wxzw, &wxwx, &wxwy, &wxwz, &wxww, &wyxx,
      &wyxy, &wyxz, &wyxw, &wyyx, &wyyy, &wyyz, &wyyw, &wyzx, &wyzy, &wyzz, &wyzw, &wywx, &wywy,
      &wywz, &wyww, &wzxx, &wzxy, &wzxz, &wzxw, &wzyx, &wzyy, &wzyz, &wzyw, &wzzx, &wzzy, &wzzz,
      &wzzw, &wzwx, &wzwy, &wzwz, &wzww, &wwxx, &wwxy, &wwxz, &wwxw, &wwyx, &wwyy, &wwyz, &wwyw,
      &wwzx, &wwzy, &wwzz, &wwzw, &wwwx, &wwwy, &wwwz, &wwww;
};

template<typename T> struct ColSwizzle4 : ColSwizzle3<T> {
  static VecBase<T, 2> &ra, &ga, &ar, &ag, &ab, &aa;
  static VecBase<T, 3> &rra, &rga, &rba, &rar, &rag, &rab, &raa, &gra, &gga, &gba, &gar, &gag,
      &gab, &gaa, &bra, &bga, &bba, &bar, &bag, &bab, &baa, &arr, &arg, &arb, &ara, &agr, &agg,
      &agb, &aga, &abr, &abg, &abb, &aba, &aar, &aag, &aab, &aaa;
  static VecBase<T, 4> &rrra, &rrga, &rrba, &rrar, &rrag, &rrab, &rraa, &rgra, &rgga, &rgba, &rgar,
      &rgag, &rgab, &rgaa, &rbra, &rbga, &rbba, &rbar, &rbag, &rbab, &rbaa, &rarr, &rarg, &rarb,
      &rara, &ragr, &ragg, &ragb, &raga, &rabr, &rabg, &rabb, &raba, &raar, &raag, &raab, &raaa,
      &grra, &grga, &grba, &grar, &grag, &grab, &graa, &ggra, &ggga, &ggba, &ggar, &ggag, &ggab,
      &ggaa, &gbra, &gbga, &gbba, &gbar, &gbag, &gbab, &gbaa, &garr, &garg, &garb, &gara, &gagr,
      &gagg, &gagb, &gaga, &gabr, &gabg, &gabb, &gaba, &gaar, &gaag, &gaab, &gaaa, &brra, &brga,
      &brba, &brar, &brag, &brab, &braa, &bgra, &bgga, &bgba, &bgar, &bgag, &bgab, &bgaa, &bbra,
      &bbga, &bbba, &bbar, &bbag, &bbab, &bbaa, &barr, &barg, &barb, &bara, &bagr, &bagg, &bagb,
      &baga, &babr, &babg, &babb, &baba, &baar, &baag, &baab, &baaa, &arrr, &arrg, &arrb, &arra,
      &argr, &argg, &argb, &arga, &arbr, &arbg, &arbb, &arba, &arar, &arag, &arab, &araa, &agrr,
      &agrg, &agrb, &agra, &aggr, &aggg, &aggb, &agga, &agbr, &agbg, &agbb, &agba, &agar, &agag,
      &agab, &agaa, &abrr, &abrg, &abrb, &abra, &abgr, &abgg, &abgb, &abga, &abbr, &abbg, &abbb,
      &abba, &abar, &abag, &abab, &abaa, &aarr, &aarg, &aarb, &aara, &aagr, &aagg, &aagb, &aaga,
      &aabr, &aabg, &aabb, &aaba, &aaar, &aaag, &aaab, &aaaa;
};

template<typename T> struct VecBase<T, 1> {
  VecBase() = default;
  template<typename U> explicit VecBase(VecBase<U, 1>) {}
  VecBase(T) {}

  operator T() RET;
};

template<typename T> struct VecBase<T, 2> : VecOp<T, 2>, VecSwizzle2<T>, ColSwizzle2<T> {
  T x, y;
  T r, g;

  VecBase() = default;
  template<typename U> explicit VecBase(VecBase<U, 2>) {}
  explicit VecBase(T) {}
  explicit VecBase(T, T) {}
};

template<typename T> struct VecBase<T, 3> : VecOp<T, 3>, VecSwizzle3<T>, ColSwizzle3<T> {
  T x, y, z;
  T r, g, b;

  VecBase() = default;
  template<typename U> explicit VecBase(VecBase<U, 3>) {}
  explicit VecBase(T) {}
  explicit VecBase(T, T, T) {}
  explicit VecBase(VecBase<T, 2>, T) {}
  explicit VecBase(T, VecBase<T, 2>) {}
};

template<typename T> struct VecBase<T, 4> : VecOp<T, 4>, VecSwizzle4<T>, ColSwizzle4<T> {
  T x, y, z, w;
  T r, g, b, a;

  VecBase() = default;
  template<typename U> explicit VecBase(VecBase<U, 4>) {}
  explicit VecBase(T) {}
  explicit VecBase(T, T, T, T) {}
  explicit VecBase(VecBase<T, 2>, T, T) {}
  explicit VecBase(T, VecBase<T, 2>, T) {}
  explicit VecBase(T, T, VecBase<T, 2>) {}
  explicit VecBase(VecBase<T, 2>, VecBase<T, 2>) {}
  explicit VecBase(VecBase<T, 3>, T) {}
  explicit VecBase(T, VecBase<T, 3>) {}
};

/* Boolean vectors do not have operators and are not convertible from other types. */

template<> struct VecBase<bool, 2> : VecSwizzle2<bool> {
  bool x, y;

  VecBase() = default;
  explicit VecBase(bool) {}
  explicit VecBase(bool, bool) {}
};

template<> struct VecBase<bool, 3> : VecSwizzle3<bool> {
  bool x, y, z;

  VecBase() = default;
  explicit VecBase(bool) {}
  explicit VecBase(bool, bool, bool) {}
  explicit VecBase(VecBase<bool, 2>, bool) {}
  explicit VecBase(bool, VecBase<bool, 2>) {}
};

template<> struct VecBase<bool, 4> : VecSwizzle4<bool> {
  bool x, y, z, w;

  VecBase() = default;
  explicit VecBase(bool) {}
  explicit VecBase(bool, bool, bool, bool) {}
  explicit VecBase(VecBase<bool, 2>, bool, bool) {}
  explicit VecBase(bool, VecBase<bool, 2>, bool) {}
  explicit VecBase(bool, bool, VecBase<bool, 2>) {}
  explicit VecBase(VecBase<bool, 2>, VecBase<bool, 2>) {}
  explicit VecBase(VecBase<bool, 3>, bool) {}
  explicit VecBase(bool, VecBase<bool, 3>) {}
};

using uint = unsigned int;
using uint32_t = unsigned int; /* For typed enums. */

using float2 = VecBase<double, 2>;
using float3 = VecBase<double, 3>;
using float4 = VecBase<double, 4>;

using uint2 = VecBase<uint, 2>;
using uint3 = VecBase<uint, 3>;
using uint4 = VecBase<uint, 4>;

using int2 = VecBase<int, 2>;
using int3 = VecBase<int, 3>;
using int4 = VecBase<int, 4>;

using bool2 = VecBase<bool, 2>;
using bool3 = VecBase<bool, 3>;
using bool4 = VecBase<bool, 4>;

using vec2 = float2;
using vec3 = float3;
using vec4 = float4;

using ivec2 = int2;
using ivec3 = int3;
using ivec4 = int4;

using uvec2 = uint2;
using uvec3 = uint3;
using uvec4 = uint4;

using bvec2 = bool2;
using bvec3 = bool3;
using bvec4 = bool4;

using FLOAT = float;
using VEC2 = float2;
using VEC3 = float3;
using VEC4 = float4;

using INT = int;
using IVEC2 = int2;
using IVEC3 = int3;
using IVEC4 = int4;

using UINT = uint;
using UVEC2 = uint2;
using UVEC3 = uint3;
using UVEC4 = uint4;

using BOOL = bool;
using BVEC2 = bool2;
using BVEC3 = bool3;
using BVEC4 = bool4;

/** \} */

/* -------------------------------------------------------------------- */
/** \name Matrix Types
 * \{ */

template<int C, int R> struct MatBase {};

template<int C, int R> struct MatOp {
  using MatT = MatBase<C, R>;
  using ColT = VecBase<double, R>;
  using RowT = VecBase<double, C>;

  const ColT &operator[](int) const
  {
    return *reinterpret_cast<const ColT *>(this);
  }
  ColT &operator[](int)
  {
    return *reinterpret_cast<ColT *>(this);
  }

  MatT operator+() RET;
  MatT operator-() RET;

  MatT operator*(MatT) const RET;

  friend ColT operator*(RowT, MatT) RET;
  friend RowT operator*(MatT, ColT) RET;
};

template<int R> struct MatBase<2, R> : MatOp<2, R> {
  using T = double;
  using ColT = VecBase<double, R>;
  ColT x, y;

  MatBase() = default;
  explicit MatBase(T) {}
  explicit MatBase(T, T, T, T) {}
  explicit MatBase(ColT, ColT) {}
  template<int OtherC, int OtherR> explicit MatBase(const MatBase<OtherC, OtherR> &) {}
};

template<int R> struct MatBase<3, R> : MatOp<3, R> {
  using T = double;
  using ColT = VecBase<double, R>;
  ColT x, y, z;

  MatBase() = default;
  explicit MatBase(T) {}
  explicit MatBase(T, T, T, T, T, T, T, T, T) {}
  explicit MatBase(ColT, ColT, ColT) {}
  template<int OtherC, int OtherR> explicit MatBase(const MatBase<OtherC, OtherR> &) {}
};

template<int R> struct MatBase<4, R> : MatOp<4, R> {
  using T = double;
  using ColT = VecBase<double, R>;
  ColT x, y, z, w;

  MatBase() = default;
  explicit MatBase(T) {}
  explicit MatBase(T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T) {}
  explicit MatBase(ColT, ColT, ColT, ColT) {}
  template<int OtherC, int OtherR> explicit MatBase(const MatBase<OtherC, OtherR> &) {}
};

using float2x2 = MatBase<2, 2>;
using float2x3 = MatBase<2, 3>;
using float2x4 = MatBase<2, 4>;
using float3x2 = MatBase<3, 2>;
using float3x3 = MatBase<3, 3>;
using float3x4 = MatBase<3, 4>;
using float4x2 = MatBase<4, 2>;
using float4x3 = MatBase<4, 3>;
using float4x4 = MatBase<4, 4>;

using mat2x2 = float2x2;
using mat2x3 = float2x3;
using mat2x4 = float2x4;
using mat3x2 = float3x2;
using mat3x3 = float3x3;
using mat3x4 = float3x4;
using mat4x2 = float4x2;
using mat4x3 = float4x3;
using mat4x4 = float4x4;

using mat2 = float2x2;
using mat3 = float3x3;
using mat4 = float4x4;

using MAT4 = float4x4;
using MAT3 = float3x3;

/* Matrix reshaping functions. */
#define RESHAPE(mat_to, mat_from, ...) \
  mat_to to_##mat_to(mat_from m) \
  { \
    return mat_to(__VA_ARGS__); \
  }

/* clang-format off */
RESHAPE(float2x2, float3x3, m[0].xy, m[1].xy)
RESHAPE(float2x2, float4x4, m[0].xy, m[1].xy)
RESHAPE(float3x3, float4x4, m[0].xyz, m[1].xyz, m[2].xyz)
RESHAPE(float3x3, float2x2, m[0].x, m[0].y, 0, m[1].x, m[1].y, 0, 0, 0, 1)
RESHAPE(float4x4, float2x2, m[0].x, m[0].y, 0, 0, m[1].x, m[1].y, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1)
RESHAPE(float4x4, float3x3, m[0].x, m[0].y, m[0].z, 0, m[1].x, m[1].y, m[1].z, 0, m[2].x, m[2].y, m[2].z, 0, 0, 0, 0, 1)
/* clang-format on */
/* TODO(fclem): Remove. Use Transform instead. */
RESHAPE(float3x3, float3x4, m[0].xyz, m[1].xyz, m[2].xyz)
#undef RESHAPE

/** \} */

/* -------------------------------------------------------------------- */
/** \name Sampler Types
 * \{ */

template<typename T, int Dimensions, bool Cube = false, bool Array = false> struct SamplerBase {
  static constexpr int coord_dim = Dimensions + int(Cube) + int(Array);
  static constexpr int deriv_dim = Dimensions + int(Cube);
  static constexpr int extent_dim = Dimensions + int(Array);

  using int_coord_type = VecBase<int, coord_dim>;
  using flt_coord_type = VecBase<double, coord_dim>;
  using derivative_type = VecBase<double, deriv_dim>;
  using data_vec_type = VecBase<T, 4>;
  using size_vec_type = VecBase<int, extent_dim>;
};

#define TEX_TEMPLATE \
  template<typename T, \
           typename IntCoord = typename T::int_coord_type, \
           typename FltCoord = typename T::flt_coord_type, \
           typename DerivVec = typename T::derivative_type, \
           typename DataVec = typename T::data_vec_type, \
           typename SizeVec = typename T::size_vec_type>

TEX_TEMPLATE SizeVec textureSize(T, int) RET;
TEX_TEMPLATE DataVec texelFetch(T, IntCoord, int) RET;
TEX_TEMPLATE DataVec texelFetchOffset(T, IntCoord, int, IntCoord) RET;
TEX_TEMPLATE DataVec texture(T, FltCoord, double /*bias*/ = 0.0) RET;
TEX_TEMPLATE DataVec textureGather(T, FltCoord) RET;
TEX_TEMPLATE DataVec textureGrad(T, FltCoord, DerivVec, DerivVec) RET;
TEX_TEMPLATE DataVec textureLod(T, FltCoord, double) RET;

#undef TEX_TEMPLATE

using samplerBuffer = SamplerBase<double, 1>;
using sampler1D = SamplerBase<double, 1>;
using sampler2D = SamplerBase<double, 2>;
using sampler3D = SamplerBase<double, 3>;
using isamplerBuffer = SamplerBase<int, 1>;
using isampler1D = SamplerBase<int, 1>;
using isampler2D = SamplerBase<int, 2>;
using isampler3D = SamplerBase<int, 3>;
using usamplerBuffer = SamplerBase<uint, 1>;
using usampler1D = SamplerBase<uint, 1>;
using usampler2D = SamplerBase<uint, 2>;
using usampler3D = SamplerBase<uint, 3>;

using sampler1DArray = SamplerBase<double, 1, false, true>;
using sampler2DArray = SamplerBase<double, 2, false, true>;
using isampler1DArray = SamplerBase<int, 1, false, true>;
using isampler2DArray = SamplerBase<int, 2, false, true>;
using usampler1DArray = SamplerBase<uint, 1, false, true>;
using usampler2DArray = SamplerBase<uint, 2, false, true>;

using samplerCube = SamplerBase<double, 2, true>;
using isamplerCube = SamplerBase<int, 2, true>;
using usamplerCube = SamplerBase<uint, 2, true>;

using samplerCubeArray = SamplerBase<double, 2, true, true>;
using isamplerCubeArray = SamplerBase<int, 2, true, true>;
using usamplerCubeArray = SamplerBase<uint, 2, true, true>;

using depth2D = sampler2D;
using depth2DArray = sampler2DArray;
using depthCube = samplerCube;
using depthCubeArray = samplerCubeArray;

/* Sampler Buffers do not have LOD. */
float4 texelFetch(samplerBuffer, int) RET;
int4 texelFetch(isamplerBuffer, int) RET;
uint4 texelFetch(usamplerBuffer, int) RET;

/** \} */

/* -------------------------------------------------------------------- */
/** \name Image Types
 * \{ */

template<typename T, int Dimensions, bool Array = false> struct ImageBase {
  static constexpr int coord_dim = Dimensions + int(Array);

  using int_coord_type = VecBase<int, coord_dim>;
  using data_vec_type = VecBase<T, 4>;
  using size_vec_type = VecBase<int, coord_dim>;
};

#define IMG_TEMPLATE \
  template<typename T, \
           typename IntCoord = typename T::int_coord_type, \
           typename DataVec = typename T::data_vec_type, \
           typename SizeVec = typename T::size_vec_type>

IMG_TEMPLATE SizeVec imageSize(const T &) RET;
IMG_TEMPLATE DataVec imageLoad(const T &, IntCoord) RET;
IMG_TEMPLATE void imageStore(T &, IntCoord, DataVec) {}
IMG_TEMPLATE void imageFence(T &) {}
/* Cannot write to a read only image. */
IMG_TEMPLATE void imageStore(const T &, IntCoord, DataVec) = delete;
IMG_TEMPLATE void imageFence(const T &) = delete;

#define imageLoadFast imageLoad
#define imageStoreFast imageStore

IMG_TEMPLATE uint imageAtomicAdd(T &, IntCoord, uint) RET;
IMG_TEMPLATE uint imageAtomicMin(T &, IntCoord, uint) RET;
IMG_TEMPLATE uint imageAtomicMax(T &, IntCoord, uint) RET;
IMG_TEMPLATE uint imageAtomicAnd(T &, IntCoord, uint) RET;
IMG_TEMPLATE uint imageAtomicXor(T &, IntCoord, uint) RET;
IMG_TEMPLATE uint imageAtomicExchange(T &, IntCoord, uint) RET;
IMG_TEMPLATE uint imageAtomicCompSwap(T &, IntCoord, uint, uint) RET;
/* Cannot write to a read only image. */
IMG_TEMPLATE uint imageAtomicAdd(const T &, IntCoord, uint) = delete;
IMG_TEMPLATE uint imageAtomicMin(const T &, IntCoord, uint) = delete;
IMG_TEMPLATE uint imageAtomicMax(const T &, IntCoord, uint) = delete;
IMG_TEMPLATE uint imageAtomicAnd(const T &, IntCoord, uint) = delete;
IMG_TEMPLATE uint imageAtomicXor(const T &, IntCoord, uint) = delete;
IMG_TEMPLATE uint imageAtomicExchange(const T &, IntCoord, uint) = delete;
IMG_TEMPLATE uint imageAtomicCompSwap(const T &, IntCoord, uint, uint) = delete;

#undef IMG_TEMPLATE

using image1D = ImageBase<double, 1>;
using image2D = ImageBase<double, 2>;
using image3D = ImageBase<double, 3>;
using iimage1D = ImageBase<int, 1>;
using iimage2D = ImageBase<int, 2>;
using iimage3D = ImageBase<int, 3>;
using uimage1D = ImageBase<uint, 1>;
using uimage2D = ImageBase<uint, 2>;
using uimage3D = ImageBase<uint, 3>;

using image1DArray = ImageBase<double, 1, true>;
using image2DArray = ImageBase<double, 2, true>;
using iimage1DArray = ImageBase<int, 1, true>;
using iimage2DArray = ImageBase<int, 2, true>;
using uimage1DArray = ImageBase<uint, 1, true>;
using uimage2DArray = ImageBase<uint, 2, true>;

/* Forbid Cube and cube arrays. Bind them as 3D textures instead. */

/** \} */

/* -------------------------------------------------------------------- */
/** \name Builtin Functions
 * \{ */

template<typename T, int D> VecBase<bool, D> greaterThan(VecBase<T, D>, VecBase<T, D>) RET;
template<typename T, int D> VecBase<bool, D> lessThan(VecBase<T, D>, VecBase<T, D>) RET;
template<typename T, int D> VecBase<bool, D> lessThanEqual(VecBase<T, D>, VecBase<T, D>) RET;
template<typename T, int D> VecBase<bool, D> greaterThanEqual(VecBase<T, D>, VecBase<T, D>) RET;
template<typename T, int D> VecBase<bool, D> equal(VecBase<T, D>, VecBase<T, D>) RET;
template<typename T, int D> VecBase<bool, D> notEqual(VecBase<T, D>, VecBase<T, D>) RET;
template<int D> bool any(VecBase<bool, D>) RET;
template<int D> bool all(VecBase<bool, D>) RET;
/* `not` is a C++ keyword that aliases the `!` operator. Simply overload it. */
template<int D> VecBase<bool, D> operator!(VecBase<bool, D>) RET;

template<int D> VecBase<int, D> bitCount(VecBase<int, D>) RET;
template<int D> VecBase<int, D> bitCount(VecBase<uint, D>) RET;
template<int D> VecBase<int, D> bitfieldExtract(VecBase<int, D>, int, int) RET;
template<int D> VecBase<int, D> bitfieldExtract(VecBase<uint, D>, int, int) RET;
template<int D> VecBase<int, D> bitfieldInsert(VecBase<int, D>, VecBase<int, D>, int, int) RET;
template<int D> VecBase<int, D> bitfieldInsert(VecBase<uint, D>, VecBase<uint, D>, int, int) RET;
template<int D> VecBase<int, D> bitfieldReverse(VecBase<int, D>) RET;
template<int D> VecBase<int, D> bitfieldReverse(VecBase<uint, D>) RET;
static inline int bitCount(int) RET;
static inline int bitCount(uint) RET;
static inline int bitfieldExtract(int) RET;
static inline int bitfieldExtract(uint) RET;
static inline int bitfieldInsert(int) RET;
static inline int bitfieldInsert(uint) RET;
static inline int bitfieldReverse(int) RET;
static inline int bitfieldReverse(uint) RET;

template<int D> VecBase<int, D> findLSB(VecBase<int, D>) RET;
template<int D> VecBase<int, D> findLSB(VecBase<uint, D>) RET;
template<int D> VecBase<int, D> findMSB(VecBase<int, D>) RET;
template<int D> VecBase<int, D> findMSB(VecBase<uint, D>) RET;
static inline int findMSB(int) RET;
static inline int findMSB(uint) RET;

/* Math Functions. */
template<typename T> T abs(T) RET;
template<typename T> T max(T, T) RET;
template<typename T> T min(T, T) RET;
template<typename T> T sign(T) RET;
template<typename T, typename U> T clamp(T, U, U) RET;
template<typename T> T clamp(T, double, double) RET;
template<typename T, typename U> T max(T, U) RET;
template<typename T, typename U> T min(T, U) RET;
/* TODO(fclem): These should be restricted to floats. */
template<typename T> T ceil(T) RET;
template<typename T> T exp(T) RET;
template<typename T> T exp2(T) RET;
template<typename T> T floor(T) RET;
template<typename T> T fma(T, T, T) RET;
#ifndef _MSC_VER /* Avoid function redefinition which triggers a compile time error. */
double fma(double, double, double) RET;
#endif
template<typename T> T fract(T) RET;
template<typename T> T frexp(T, T) RET;
template<typename T> T inversesqrt(T) RET;
template<typename T> T isinf(T) RET;
template<typename T> T isnan(T) RET;
template<typename T> T log(T) RET;
template<typename T> T log2(T) RET;
double mod(double, double) RET;
template<int D> VecBase<double, D> mod(VecBase<double, D>, double) RET;
template<int D> VecBase<double, D> mod(VecBase<double, D>, VecBase<double, D>) RET;
template<typename T> T modf(T, T);
template<typename T, typename U> T pow(T, U) RET;
template<typename T> T round(T) RET;
template<typename T> T smoothstep(T, T, T) RET;
template<typename T> T sqrt(T) RET;
template<typename T> T step(T, T) RET;
template<typename T> T trunc(T) RET;
template<typename T, typename U> T ldexp(T, U) RET;
static inline double smoothstep(double, double, double) RET;

template<typename T> T acos(T) RET;
template<typename T> T acosh(T) RET;
template<typename T> T asin(T) RET;
template<typename T> T asinh(T) RET;
template<typename T> T atan(T, T) RET;
template<typename T> T atan(T) RET;
template<typename T> T atanh(T) RET;
template<typename T> T cos(T) RET;
template<typename T> T cosh(T) RET;
template<typename T> T sin(T) RET;
template<typename T> T sinh(T) RET;
template<typename T> T tan(T) RET;
template<typename T> T tanh(T) RET;

template<typename T> T degrees(T) RET;
template<typename T> T radians(T) RET;

/* Declared explicitly to avoid type errors. */
static inline double mix(double, double, double) RET;
template<int D> VecBase<double, D> mix(VecBase<double, D>, VecBase<double, D>, double) RET;
template<int D>
VecBase<double, D> mix(VecBase<double, D>, VecBase<double, D>, VecBase<double, D>) RET;
template<typename T, int D> VecBase<T, D> mix(VecBase<T, D>, VecBase<T, D>, VecBase<bool, D>) RET;

#define select(A, B, C) mix(A, B, C)

static inline VecBase<double, 3> cross(VecBase<double, 3>, VecBase<double, 3>) RET;
template<int D> float dot(VecBase<double, D>, VecBase<double, D>) RET;
template<int D> float distance(VecBase<double, D>, VecBase<double, D>) RET;
template<int D> float length(VecBase<double, D>) RET;
template<int D> VecBase<double, D> normalize(VecBase<double, D>) RET;

template<int D> VecBase<int, D> floatBitsToInt(VecBase<double, D>) RET;
template<int D> VecBase<uint, D> floatBitsToUint(VecBase<double, D>) RET;
template<int D> VecBase<double, D> intBitsToFloat(VecBase<int, D>) RET;
template<int D> VecBase<double, D> uintBitsToFloat(VecBase<uint, D>) RET;
static inline int floatBitsToInt(double) RET;
static inline uint floatBitsToUint(double) RET;
static inline double intBitsToFloat(int) RET;
static inline double uintBitsToFloat(uint) RET;

namespace gl_FragmentShader {
/* Derivative functions. */
template<typename T> T dFdx(T) RET;
template<typename T> T dFdy(T) RET;
template<typename T> T fwidth(T) RET;
}  // namespace gl_FragmentShader

/* Geometric functions. */
template<typename T, int D> float faceforward(VecBase<T, D>, VecBase<T, D>, VecBase<T, D>) RET;
template<typename T, int D> float reflect(VecBase<T, D>, VecBase<T, D>) RET;
template<typename T, int D> float refract(VecBase<T, D>, VecBase<T, D>, double) RET;

/* Atomic operations. */
static inline int atomicAdd(int &, int) RET;
static inline int atomicAnd(int &, int) RET;
static inline int atomicOr(int &, int) RET;
static inline int atomicXor(int &, int) RET;
static inline int atomicMin(int &, int) RET;
static inline int atomicMax(int &, int) RET;
static inline int atomicExchange(int &, int) RET;
static inline int atomicCompSwap(int &, int, int) RET;
static inline uint atomicAdd(uint &, uint) RET;
static inline uint atomicAnd(uint &, uint) RET;
static inline uint atomicOr(uint &, uint) RET;
static inline uint atomicXor(uint &, uint) RET;
static inline uint atomicMin(uint &, uint) RET;
static inline uint atomicMax(uint &, uint) RET;
static inline uint atomicExchange(uint &, uint) RET;
static inline uint atomicCompSwap(uint &, uint, uint) RET;

/* Packing functions. */
static inline uint packHalf2x16(float2) RET;
static inline uint packUnorm2x16(float2) RET;
static inline uint packSnorm2x16(float2) RET;
static inline uint packUnorm4x8(float4) RET;
static inline uint packSnorm4x8(float4) RET;
static inline float2 unpackHalf2x16(uint) RET;
static inline float2 unpackUnorm2x16(uint) RET;
static inline float2 unpackSnorm2x16(uint) RET;
static inline float4 unpackUnorm4x8(uint) RET;
static inline float4 unpackSnorm4x8(uint) RET;

/* Matrices functions. */
template<int C, int R> float determinant(MatBase<C, R>) RET;
template<int C, int R> MatBase<C, R> inverse(MatBase<C, R>) RET;
template<int C, int R> MatBase<R, C> transpose(MatBase<C, R>) RET;

/* TODO(@fclem): Should be in a lib instead of being implemented by each backend. */
static inline bool is_zero(vec2) RET;
static inline bool is_zero(vec3) RET;
static inline bool is_zero(vec4) RET;

#undef RET

/** \} */

/* -------------------------------------------------------------------- */
/** \name Special Variables
 * \{ */

namespace gl_VertexShader {

const int gl_VertexID = 0;
const int gl_InstanceID = 0;
const int gl_BaseVertex = 0;
const int gl_BaseInstance = 0;
float4 gl_Position = float4(0);
double gl_PointSize = 0;
float gl_ClipDistance[6] = {0};
int gpu_Layer = 0;
int gpu_ViewportIndex = 0;

}  // namespace gl_VertexShader

namespace gl_FragmentShader {

const float4 gl_FragCoord = float4(0);
const bool gl_FrontFacing = true;
const float2 gl_PointCoord = float2(0);
const int gl_PrimitiveID = 0;
float gl_FragDepth = 0;
const float gl_ClipDistance[6] = {0};
const int gpu_Layer = 0;
const int gpu_ViewportIndex = 0;

}  // namespace gl_FragmentShader

namespace gl_ComputeShader {

const uint3 gl_NumWorkGroups = {};
constexpr uint3 gl_WorkGroupSize = {};
const uint3 gl_WorkGroupID = {};
const uint3 gl_LocalInvocationID = {};
const uint3 gl_GlobalInvocationID = {};
const uint gl_LocalInvocationIndex = {};

}  // namespace gl_ComputeShader

/** \} */

/* -------------------------------------------------------------------- */
/** \name Keywords
 * \{ */

/* Note: Cannot easily mutate them. Pass every by copy for now. */

/* Pass argument by reference. */
#define inout
/* Pass argument by reference but only write to it. Its initial value is undefined. */
#define out
/* Pass argument by copy (default). */
#define in

/* Discards the output of the current fragment shader invocation and halts its execution. */
#define discard

/* Decorate a variable in global scope that is common to all threads in a thread-group. */
#define shared

namespace gl_ComputeShader {
static inline void barrier() {}
static inline void memoryBarrier() {}
static inline void memoryBarrierShared() {}
static inline void memoryBarrierImage() {}
static inline void memoryBarrierBuffer() {}
static inline void groupMemoryBarrier() {}
}  // namespace gl_ComputeShader

/** \} */

/* -------------------------------------------------------------------- */
/** \name Compatibility
 * \{ */

/* Array syntax compatibility. */
/* clang-format off */
#define float_array(...) { __VA_ARGS__ }
#define float2_array(...) { __VA_ARGS__ }
#define float3_array(...) { __VA_ARGS__ }
#define float4_array(...) { __VA_ARGS__ }
#define int_array(...) { __VA_ARGS__ }
#define int2_array(...) { __VA_ARGS__ }
#define int3_array(...) { __VA_ARGS__ }
#define int4_array(...) { __VA_ARGS__ }
#define uint_array(...) { __VA_ARGS__ }
#define uint2_array(...) { __VA_ARGS__ }
#define uint3_array(...) { __VA_ARGS__ }
#define uint4_array(...) { __VA_ARGS__ }
#define bool_array(...) { __VA_ARGS__ }
#define bool2_array(...) { __VA_ARGS__ }
#define bool3_array(...) { __VA_ARGS__ }
#define bool4_array(...) { __VA_ARGS__ }
/* clang-format on */

/** \} */

/* GLSL main function must return void. C++ need to return int.
 * Inject real main (C++) inside the GLSL main definition. */
#define main() \
  /* Fake main prototype. */ \
  /* void */ _fake_main(); \
  /* Real main. */ \
  int main() \
  { \
    _fake_main(); \
    return 0; \
  } \
  /* Fake main definition. */ \
  void _fake_main()

#define GLSL_CPP_STUBS

#include "GPU_shader_shared.hh"
