/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * C++ stubs for shading language.
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

#define STD_OP \
  template<typename U = T, typename std::enable_if_t<!std::is_same_v<bool, U>> * = nullptr>

  STD_OP VecT operator+() const RET;
  STD_OP VecT operator-() const RET;

  STD_OP friend VecT operator+(VecT, VecT) RET;
  STD_OP friend VecT operator-(VecT, VecT) RET;
  STD_OP friend VecT operator/(VecT, VecT) RET;
  STD_OP friend VecT operator*(VecT, VecT) RET;

  STD_OP friend VecT operator+(VecT, T) RET;
  STD_OP friend VecT operator-(VecT, T) RET;
  STD_OP friend VecT operator/(VecT, T) RET;
  STD_OP friend VecT operator*(VecT, T) RET;

  STD_OP friend VecT operator+(T, VecT) RET;
  STD_OP friend VecT operator-(T, VecT) RET;
  STD_OP friend VecT operator/(T, VecT) RET;
  STD_OP friend VecT operator*(T, VecT) RET;

  STD_OP friend VecT operator+=(VecT, VecT) RET;
  STD_OP friend VecT operator-=(VecT, VecT) RET;
  STD_OP friend VecT operator/=(VecT, VecT) RET;
  STD_OP friend VecT operator*=(VecT, VecT) RET;

  STD_OP friend VecT operator+=(VecT, T) RET;
  STD_OP friend VecT operator-=(VecT, T) RET;
  STD_OP friend VecT operator/=(VecT, T) RET;
  STD_OP friend VecT operator*=(VecT, T) RET;

#define INT_OP \
  template<typename U = T, \
           typename std::enable_if_t<std::is_integral_v<U>> * = nullptr, \
           typename std::enable_if_t<!std::is_same_v<bool, U>> * = nullptr>

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

template<typename T, int Sz> struct SwizzleBase : VecOp<T, Sz> {
  using VecT = VecBase<T, Sz>;

  SwizzleBase() = default;
  SwizzleBase(T) {}

  constexpr VecT operator=(const VecT &) RET;
  operator VecT() const RET;
};

#define SWIZZLE_XY(T) \
  SwizzleBase<T, 2> xx, xy, yx, yy; \
  SwizzleBase<T, 3> xxx, xxy, xyx, xyy, yxx, yxy, yyx, yyy; \
  SwizzleBase<T, 4> xxxx, xxxy, xxyx, xxyy, xyxx, xyxy, xyyx, xyyy, yxxx, yxxy, yxyx, yxyy, yyxx, \
      yyxy, yyyx, yyyy;

#define SWIZZLE_RG(T) \
  SwizzleBase<T, 2> rr, rg, gr, gg; \
  SwizzleBase<T, 3> rrr, rrg, rgr, rgg, grr, grg, ggr, ggg; \
  SwizzleBase<T, 4> rrrr, rrrg, rrgr, rrgg, rgrr, rgrg, rggr, rggg, grrr, grrg, grgr, grgg, ggrr, \
      ggrg, gggr, gggg;

#define SWIZZLE_XYZ(T) \
  SWIZZLE_XY(T) \
  SwizzleBase<T, 2> xz, yz, zx, zy, zz; \
  SwizzleBase<T, 3> xxz, xyz, xzx, xzy, xzz, yxz, yyz, yzx, yzy, yzz, zxx, zxy, zxz, zyx, zyy, \
      zyz, zzx, zzy, zzz; \
  SwizzleBase<T, 4> xxxz, xxyz, xxzx, xxzy, xxzz, xyxz, xyyz, xyzx, xyzy, xyzz, xzxx, xzxy, xzxz, \
      xzyx, xzyy, xzyz, xzzx, xzzy, xzzz, yxxz, yxyz, yxzx, yxzy, yxzz, yyxz, yyyz, yyzx, yyzy, \
      yyzz, yzxx, yzxy, yzxz, yzyx, yzyy, yzyz, yzzx, yzzy, yzzz, zxxx, zxxy, zxxz, zxyx, zxyy, \
      zxyz, zxzx, zxzy, zxzz, zyxx, zyxy, zyxz, zyyx, zyyy, zyyz, zyzx, zyzy, zyzz, zzxx, zzxy, \
      zzxz, zzyx, zzyy, zzyz, zzzx, zzzy, zzzz;

#define SWIZZLE_RGB(T) \
  SWIZZLE_RG(T) \
  SwizzleBase<T, 2> rb, gb, br, bg, bb; \
  SwizzleBase<T, 3> rrb, rgb, rbr, rbg, rbb, grb, ggb, gbr, gbg, gbb, brr, brg, brb, bgr, bgg, \
      bgb, bbr, bbg, bbb; \
  SwizzleBase<T, 4> rrrb, rrgb, rrbr, rrbg, rrbb, rgrb, rggb, rgbr, rgbg, rgbb, rbrr, rbrg, rbrb, \
      rbgr, rbgg, rbgb, rbbr, rbbg, rbbb, grrb, grgb, grbr, grbg, grbb, ggrb, gggb, ggbr, ggbg, \
      ggbb, gbrr, gbrg, gbrb, gbgr, gbgg, gbgb, gbbr, gbbg, gbbb, brrr, brrg, brrb, brgr, brgg, \
      brgb, brbr, brbg, brbb, bgrr, bgrg, bgrb, bggr, bggg, bggb, bgbr, bgbg, bgbb, bbrr, bbrg, \
      bbrb, bbgr, bbgg, bbgb, bbbr, bbbg, bbbb;

#define SWIZZLE_XYZW(T) \
  SWIZZLE_XYZ(T) \
  SwizzleBase<T, 2> xw, yw, zw, wx, wy, wz, ww; \
  SwizzleBase<T, 3> xxw, xyw, xzw, xwx, xwy, xwz, xww, yxw, yyw, yzw, ywx, ywy, ywz, yww, zxw, \
      zyw, zzw, zwx, zwy, zwz, zww, wxx, wxy, wxz, wxw, wyx, wyy, wyz, wyw, wzx, wzy, wzz, wzw, \
      wwx, wwy, wwz, www; \
  SwizzleBase<T, 4> xxxw, xxyw, xxzw, xxwx, xxwy, xxwz, xxww, xyxw, xyyw, xyzw, xywx, xywy, xywz, \
      xyww, xzxw, xzyw, xzzw, xzwx, xzwy, xzwz, xzww, xwxx, xwxy, xwxz, xwxw, xwyx, xwyy, xwyz, \
      xwyw, xwzx, xwzy, xwzz, xwzw, xwwx, xwwy, xwwz, xwww, yxxw, yxyw, yxzw, yxwx, yxwy, yxwz, \
      yxww, yyxw, yyyw, yyzw, yywx, yywy, yywz, yyww, yzxw, yzyw, yzzw, yzwx, yzwy, yzwz, yzww, \
      ywxx, ywxy, ywxz, ywxw, ywyx, ywyy, ywyz, ywyw, ywzx, ywzy, ywzz, ywzw, ywwx, ywwy, ywwz, \
      ywww, zxxw, zxyw, zxzw, zxwx, zxwy, zxwz, zxww, zyxw, zyyw, zyzw, zywx, zywy, zywz, zyww, \
      zzxw, zzyw, zzzw, zzwx, zzwy, zzwz, zzww, zwxx, zwxy, zwxz, zwxw, zwyx, zwyy, zwyz, zwyw, \
      zwzx, zwzy, zwzz, zwzw, zwwx, zwwy, zwwz, zwww, wxxx, wxxy, wxxz, wxxw, wxyx, wxyy, wxyz, \
      wxyw, wxzx, wxzy, wxzz, wxzw, wxwx, wxwy, wxwz, wxww, wyxx, wyxy, wyxz, wyxw, wyyx, wyyy, \
      wyyz, wyyw, wyzx, wyzy, wyzz, wyzw, wywx, wywy, wywz, wyww, wzxx, wzxy, wzxz, wzxw, wzyx, \
      wzyy, wzyz, wzyw, wzzx, wzzy, wzzz, wzzw, wzwx, wzwy, wzwz, wzww, wwxx, wwxy, wwxz, wwxw, \
      wwyx, wwyy, wwyz, wwyw, wwzx, wwzy, wwzz, wwzw, wwwx, wwwy, wwwz, wwww;

#define SWIZZLE_RGBA(T) \
  SWIZZLE_RGB(T) \
  SwizzleBase<T, 2> ra, ga, ba, ar, ag, ab, aa; \
  SwizzleBase<T, 3> rra, rga, rba, rar, rag, rab, raa, gra, gga, gba, gar, gag, gab, gaa, bra, \
      bga, bba, bar, bag, bab, baa, arr, arg, arb, ara, agr, agg, agb, aga, abr, abg, abb, aba, \
      aar, aag, aab, aaa; \
  SwizzleBase<T, 4> rrra, rrga, rrba, rrar, rrag, rrab, rraa, rgra, rgga, rgba, rgar, rgag, rgab, \
      rgaa, rbra, rbga, rbba, rbar, rbag, rbab, rbaa, rarr, rarg, rarb, rara, ragr, ragg, ragb, \
      raga, rabr, rabg, rabb, raba, raar, raag, raab, raaa, grra, grga, grba, grar, grag, grab, \
      graa, ggra, ggga, ggba, ggar, ggag, ggab, ggaa, gbra, gbga, gbba, gbar, gbag, gbab, gbaa, \
      garr, garg, garb, gara, gagr, gagg, gagb, gaga, gabr, gabg, gabb, gaba, gaar, gaag, gaab, \
      gaaa, brra, brga, brba, brar, brag, brab, braa, bgra, bgga, bgba, bgar, bgag, bgab, bgaa, \
      bbra, bbga, bbba, bbar, bbag, bbab, bbaa, barr, barg, barb, bara, bagr, bagg, bagb, baga, \
      babr, babg, babb, baba, baar, baag, baab, baaa, arrr, arrg, arrb, arra, argr, argg, argb, \
      arga, arbr, arbg, arbb, arba, arar, arag, arab, araa, agrr, agrg, agrb, agra, aggr, aggg, \
      aggb, agga, agbr, agbg, agbb, agba, agar, agag, agab, agaa, abrr, abrg, abrb, abra, abgr, \
      abgg, abgb, abga, abbr, abbg, abbb, abba, abar, abag, abab, abaa, aarr, aarg, aarb, aara, \
      aagr, aagg, aagb, aaga, aabr, aabg, aabb, aaba, aaar, aaag, aaab, aaaa;

template<typename T> struct VecBase<T, 1> {
  VecBase() = default;
  template<typename U> explicit VecBase(VecOp<U, 1>) {}
  VecBase(T) {}

  operator T() RET;
};

template<typename T> struct VecBase<T, 2> : VecOp<T, 2> {
 private:
  /* Weird non-zero value to avoid error about division by zero in constexpr. */
  static constexpr T V = T(0.123f);

 public:
  union {
    struct {
      T x, y;
    };
    struct {
      T r, g;
    };
    SWIZZLE_XY(T);
    SWIZZLE_RG(T);
  };

  VecBase() = default;
  template<typename U> explicit VecBase(VecOp<U, 2>) {}
  constexpr explicit VecBase(T) : x(V), y(V) {}
  /* Implemented correctly for GCC to compile the constexpr float2 arrays. */
  constexpr explicit VecBase(T x_, T y_) : x(x_), y(y_) {}
};

template<typename T> struct VecBase<T, 3> : VecOp<T, 3> {
 private:
  /* Weird non-zero value to avoid error about division by zero in constexpr. */
  static constexpr T V = T(0.123f);

 public:
  union {
    struct {
      T x, y, z;
    };
    struct {
      T r, g, b;
    };
    SWIZZLE_XYZ(T);
    SWIZZLE_RGB(T);
  };

  VecBase() = default;
  template<typename U> explicit VecBase(VecOp<U, 3>) {}
  constexpr explicit VecBase(T) : x(V), y(V), z(V) {}
  /* Implemented correctly for GCC to compile the constexpr gl_WorkGroupSize. */
  constexpr explicit VecBase(T x_, T y_, T z_) : x(x_), y(y_), z(z_) {}
  constexpr explicit VecBase(VecOp<T, 2>, T) : x(V), y(V), z(V) {}
  constexpr explicit VecBase(T, VecOp<T, 2>) : x(V), y(V), z(V) {}
};

template<typename T> struct VecBase<T, 4> : VecOp<T, 4> {
 private:
  /* Weird non-zero value to avoid error about division by zero in constexpr. */
  static constexpr T V = T(0.123f);

 public:
  union {
    struct {
      T x, y, z, w;
    };
    struct {
      T r, g, b, a;
    };
    SWIZZLE_XYZW(T);
    SWIZZLE_RGBA(T);
  };

  VecBase() = default;
  template<typename U> explicit VecBase(VecOp<U, 4>) {}
  constexpr explicit VecBase(T) : x(V), y(V), z(V), w(V) {}
  /* Implemented correctly for GCC to compile the constexpr. */
  constexpr explicit VecBase(T x_, T y_, T z_, T w_) : x(x_), y(y_), z(z_), w(w_) {}
  constexpr explicit VecBase(VecOp<T, 2>, T, T) : x(V), y(V), z(V), w(V) {}
  constexpr explicit VecBase(T, VecOp<T, 2>, T) : x(V), y(V), z(V), w(V) {}
  constexpr explicit VecBase(T, T, VecOp<T, 2>) : x(V), y(V), z(V), w(V) {}
  constexpr explicit VecBase(VecOp<T, 2>, VecOp<T, 2>) : x(V), y(V), z(V), w(V) {}
  constexpr explicit VecBase(VecOp<T, 3>, T) : x(V), y(V), z(V), w(V) {}
  constexpr explicit VecBase(T, VecOp<T, 3>) : x(V), y(V), z(V), w(V) {}
};

/* Boolean vectors do not have operators and are not convertible from other types. */

template<> struct VecBase<bool, 2> : VecOp<bool, 2> {
  union {
    struct {
      bool x, y;
    };
    SWIZZLE_XY(bool);
  };

  VecBase() = default;
  explicit VecBase(bool) {}
  explicit VecBase(bool, bool) {}
  /* Should be forbidden, but is used by SMAA. */
  explicit VecBase(VecOp<float, 2>) {}
};

template<> struct VecBase<bool, 3> : VecOp<bool, 3> {
  union {
    struct {
      bool x, y, z;
    };
    SWIZZLE_XYZ(bool);
  };

  VecBase() = default;
  explicit VecBase(bool) {}
  explicit VecBase(bool, bool, bool) {}
  explicit VecBase(VecOp<bool, 2>, bool) {}
  explicit VecBase(bool, VecOp<bool, 2>) {}
};

template<> struct VecBase<bool, 4> : VecOp<bool, 4> {
  union {
    struct {
      bool x, y, z, w;
    };
    SWIZZLE_XYZW(bool);
  };

  VecBase() = default;
  explicit VecBase(bool) {}
  explicit VecBase(bool, bool, bool, bool) {}
  explicit VecBase(VecOp<bool, 2>, bool, bool) {}
  explicit VecBase(bool, VecOp<bool, 2>, bool) {}
  explicit VecBase(bool, bool, VecOp<bool, 2>) {}
  explicit VecBase(VecOp<bool, 2>, VecOp<bool, 2>) {}
  explicit VecBase(VecOp<bool, 3>, bool) {}
  explicit VecBase(bool, VecOp<bool, 3>) {}
};

using uint = unsigned int;
using uint32_t = unsigned int; /* For typed enums. */

using float2 = VecBase<float, 2>;
using float3 = VecBase<float, 3>;
using float4 = VecBase<float, 4>;

using uint2 = VecBase<uint, 2>;
using uint3 = VecBase<uint, 3>;
using uint4 = VecBase<uint, 4>;

using int2 = VecBase<int, 2>;
using int3 = VecBase<int, 3>;
using int4 = VecBase<int, 4>;

using uchar = unsigned int;
using uchar2 = VecBase<uchar, 2>;
using uchar3 = VecBase<uchar, 3>;
using uchar4 = VecBase<uchar, 4>;

using char2 = VecBase<char, 2>;
using char3 = VecBase<char, 3>;
using char4 = VecBase<char, 4>;

using ushort = unsigned short;
using ushort2 = VecBase<ushort, 2>;
using ushort3 = VecBase<ushort, 3>;
using ushort4 = VecBase<ushort, 4>;

using short2 = VecBase<short, 2>;
using short3 = VecBase<short, 3>;
using short4 = VecBase<short, 4>;

using half = float;
using half2 = VecBase<half, 2>;
using half3 = VecBase<half, 3>;
using half4 = VecBase<half, 4>;

using bool2 = VecBase<bool, 2>;
using bool3 = VecBase<bool, 3>;
using bool4 = VecBase<bool, 4>;

using bool32_t = uint;

/** Packed types are needed for MSL which have different alignment rules for float3. */
using packed_float2 = float2;
using packed_float3 = float3;
using packed_float4 = float4;
using packed_int2 = int2;
using packed_int3 = int3;
using packed_int4 = int4;
using packed_uint2 = uint2;
using packed_uint3 = uint3;
using packed_uint4 = uint4;

/** \} */

#undef RET
