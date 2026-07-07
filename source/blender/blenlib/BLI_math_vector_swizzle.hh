/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 */

#include "BLI_assert.h"

#include <array>
#include <type_traits>

namespace blender {

/* Needed forward declaration to allow the swizzle to reference it. */
template<typename T, int Size> struct VecBase;

/**
 * Swizzle functions for vector of non-trivial types.
 * Does not support assignment.
 */
template<typename T, int Size> struct VecSwizzleFunc {};

template<typename T> struct VecSwizzleFunc<T, 2> {
  [[nodiscard]] VecBase<T, 2> xy() const
  {
    const VecBase<T, 2> &vec = *reinterpret_cast<const VecBase<T, 2> *>(this);
    return {vec.x, vec.y};
  }
};

template<typename T> struct VecSwizzleFunc<T, 3> : VecSwizzleFunc<T, 2> {
  [[nodiscard]] VecBase<T, 3> xyz() const
  {
    const VecBase<T, 3> &vec = *reinterpret_cast<const VecBase<T, 3> *>(this);
    return {vec.x, vec.y, vec.z};
  }

  [[nodiscard]] VecBase<T, 2> yz() const
  {
    const VecBase<T, 3> &vec = *reinterpret_cast<const VecBase<T, 3> *>(this);
    return {vec.y, vec.z};
  }
};

template<typename T> struct VecSwizzleFunc<T, 4> : VecSwizzleFunc<T, 3> {
  [[nodiscard]] VecBase<T, 2> zw() const
  {
    const VecBase<T, 4> &vec = *reinterpret_cast<const VecBase<T, 4> *>(this);
    return {vec.z, vec.w};
  }

  [[nodiscard]] VecBase<T, 3> yzw() const
  {
    const VecBase<T, 4> &vec = *reinterpret_cast<const VecBase<T, 4> *>(this);
    return {vec.y, vec.z, vec.w};
  }

  [[nodiscard]] VecBase<T, 4> xyzw() const
  {
    const VecBase<T, 4> &vec = *reinterpret_cast<const VecBase<T, 4> *>(this);
    return {vec.x, vec.y, vec.z, vec.w};
  }
};

/**
 * Swizzle class that supports reordering of component.
 * Will decay to a vector of the same size.
 * Does not support assignment (but is still copy & move constructible, see below).
 *
 * IMPORTANT: Must be declared at the same memory location as the first component referenced in the
 * swizzle. So for `zzwy` it is `y`. We do this to allow the copy constructor to copy only the
 * referenced component in the case of something like `a.zzy = b.zzy`. This is because we do not
 * want to override (or delete) the copy constructor as it would make the vector types non-trivial.
 */
template<typename T, int Size, int x, int y, int z = y, int w = z> struct VecSwizzleReadOnly {
  using VecT = VecBase<T, Size>;
  static constexpr int max_comp = std::max(std::max(std::max(x, y), z), w);
  static constexpr int min_comp = std::min(std::min(std::min(x, y), z), w);
  static constexpr int effective_len = max_comp - min_comp + 1;

 private:
  std::array<T, effective_len> values_;

 public:
  VecSwizzleReadOnly() = default;

  [[nodiscard]] VecT operator()() const
  {
    /* Can only do this when VecT has been instantiated. */
    BLI_STATIC_ASSERT(alignof(VecT) <= alignof(T),
                      "VecSwizzleReadOnly is not compatible with aligned type for now.");
    BLI_STATIC_ASSERT(std::is_trivial_v<VecT>, "Can only swizzle trivial vectors.");
    BLI_STATIC_ASSERT(Size >= 2 && Size <= 4, "Only small vector supports swizzles");
    if constexpr (Size == 4) {
      return {values_[x - min_comp],
              values_[y - min_comp],
              values_[z - min_comp],
              values_[w - min_comp]};
    }
    else if constexpr (Size == 3) {
      return {values_[x - min_comp], values_[y - min_comp], values_[z - min_comp]};
    }
    else if constexpr (Size == 2) {
      return {values_[x - min_comp], values_[y - min_comp]};
    }
    return {};
  }
};

/**
 * Swizzle class that supports assignment. Does not support reordering of component.
 * Will decay to a vector of the same size.
 * Can only be used if all swizzled components points to different memory locations (no `xxyy`).
 *
 * IMPORTANT: Must be declared at the same memory location as the first component referenced in the
 * swizzle. So for `yz` it is `y`. We do this to allow the copy constructor to copy only the
 * referenced component in the case of something like `a.zzy = b.zzy`. This is because we do not
 * want to override (or delete) the copy constructor as it would make the vector types non-trivial.
 */
template<typename T, int Size> struct VecSwizzleReadWrite {
  using VecT = VecBase<T, Size>;

 private:
  std::array<T, Size> values_;

 public:
  [[nodiscard]] VecT &operator()()
  {
    /* Can only do this when VecT has been instantiated. */
    BLI_STATIC_ASSERT(alignof(VecT) <= alignof(T),
                      "VecSwizzleReadWrite is not compatible with aligned type for now.");
    BLI_STATIC_ASSERT(std::is_trivial_v<VecT>, "Can only swizzle trivial vectors.");
    BLI_STATIC_ASSERT(Size >= 2 && Size <= 4, "Only small vector supports swizzles");
    return *reinterpret_cast<VecT *>(this);
  }

  [[nodiscard]] const VecT &operator()() const
  {
    /* Can only do this when VecT has been instantiated. */
    BLI_STATIC_ASSERT(alignof(VecT) <= alignof(T),
                      "VecSwizzleReadWrite is not compatible with aligned type for now.");
    BLI_STATIC_ASSERT(std::is_trivial_v<VecT>, "Can only swizzle trivial vectors.");
    BLI_STATIC_ASSERT(Size >= 2 && Size <= 4, "Only small vector supports swizzles");
    return *reinterpret_cast<const VecT *>(this);
  }
};

/**
 * List of all swizzles we support.
 * We do not support non-contiguous component swizzle (e.g. xwxw) as they would have undefined
 * behavior in some corner cases.
 * We only generate the variant we use to reduce compile time and binary size.
 * Uncomment at when needed.
 */

/* Swizzles containing X. Must be declared in a union alongside X. */
#define X_SWIZZLES \
  VecSwizzleReadOnly<T, 2, 0, 0> xx; \
  VecSwizzleReadOnly<T, 3, 0, 0, 0> xxx; \
  VecSwizzleReadOnly<T, 4, 0, 0, 0, 0> xxxx;

/* Swizzles containing Y. Must be declared in a union alongside Y. */
#define Y_SWIZZLES \
  VecSwizzleReadOnly<T, 2, 1, 1> yy; \
  VecSwizzleReadOnly<T, 3, 1, 1, 1> yyy; \
  VecSwizzleReadOnly<T, 4, 1, 1, 1, 1> yyyy;

/* Swizzles containing Z. Must be declared in a union alongside Z. */
#define Z_SWIZZLES \
  VecSwizzleReadOnly<T, 2, 2, 2> zz; \
  VecSwizzleReadOnly<T, 3, 2, 2, 2> zzz; \
  VecSwizzleReadOnly<T, 4, 2, 2, 2, 2> zzzz;

/* Swizzles containing W. Must be declared in a union alongside W. */
#define W_SWIZZLES \
  VecSwizzleReadOnly<T, 2, 3, 3> ww; \
  VecSwizzleReadOnly<T, 3, 3, 3, 3> www; \
  VecSwizzleReadOnly<T, 4, 3, 3, 3, 3> wwww;

/* Swizzles containing XY. Must be declared in a union alongside X. */
#define XY_SWIZZLES \
  VecSwizzleReadWrite<T, 2> xy; \
  VecSwizzleReadOnly<T, 2, 1, 0> yx; \
  /* VecSwizzleReadOnly<T, 3, 0, 0, 1> xxy; */ \
  /* VecSwizzleReadOnly<T, 3, 0, 1, 0> xyx; */ \
  /* VecSwizzleReadOnly<T, 3, 0, 1, 1> xyy; */ \
  /* VecSwizzleReadOnly<T, 3, 1, 0, 0> yxx; */ \
  /* VecSwizzleReadOnly<T, 3, 1, 0, 1> yxy; */ \
  /* VecSwizzleReadOnly<T, 3, 1, 1, 0> yyx; */ \
  /* VecSwizzleReadOnly<T, 4, 0, 0, 0, 1> xxxy; */ \
  /* VecSwizzleReadOnly<T, 4, 0, 0, 1, 0> xxyx; */ \
  /* VecSwizzleReadOnly<T, 4, 0, 0, 1, 1> xxyy; */ \
  /* VecSwizzleReadOnly<T, 4, 0, 1, 0, 0> xyxx; */ \
  VecSwizzleReadOnly<T, 4, 0, 1, 0, 1> xyxy; \
  /* VecSwizzleReadOnly<T, 4, 0, 1, 1, 0> xyyx; */ \
  /* VecSwizzleReadOnly<T, 4, 0, 1, 1, 1> xyyy; */ \
  /* VecSwizzleReadOnly<T, 4, 1, 0, 0, 0> yxxx; */ \
  /* VecSwizzleReadOnly<T, 4, 1, 0, 0, 1> yxxy; */ \
  /* VecSwizzleReadOnly<T, 4, 1, 0, 1, 0> yxyx; */ \
  /* VecSwizzleReadOnly<T, 4, 1, 0, 1, 1> yxyy; */ \
  /* VecSwizzleReadOnly<T, 4, 1, 1, 0, 0> yyxx; */ \
  /* VecSwizzleReadOnly<T, 4, 1, 1, 0, 1> yyxy; */ \
  /* VecSwizzleReadOnly<T, 4, 1, 1, 1, 0> yyyx; */

/* Swizzles containing YZ. Must be declared in a union alongside Y. */
#define YZ_SWIZZLES \
  VecSwizzleReadWrite<T, 2> yz; \
  VecSwizzleReadOnly<T, 2, 2, 1> zy; \
  /* VecSwizzleReadOnly<T, 3, 1, 1, 2> yyz; */ \
  /* VecSwizzleReadOnly<T, 3, 1, 2, 1> yzy; */ \
  /* VecSwizzleReadOnly<T, 3, 1, 2, 2> yzz; */ \
  /* VecSwizzleReadOnly<T, 3, 2, 1, 1> zyy; */ \
  /* VecSwizzleReadOnly<T, 3, 2, 1, 2> zyz; */ \
  /* VecSwizzleReadOnly<T, 3, 2, 2, 1> zzy; */ \
  /* VecSwizzleReadOnly<T, 4, 1, 1, 1, 2> yyyz; */ \
  /* VecSwizzleReadOnly<T, 4, 1, 1, 2, 1> yyzy; */ \
  /* VecSwizzleReadOnly<T, 4, 1, 1, 2, 2> yyzz; */ \
  /* VecSwizzleReadOnly<T, 4, 1, 2, 1, 1> yzyy; */ \
  /* VecSwizzleReadOnly<T, 4, 1, 2, 1, 2> yzyz; */ \
  /* VecSwizzleReadOnly<T, 4, 1, 2, 2, 1> yzzy; */ \
  /* VecSwizzleReadOnly<T, 4, 1, 2, 2, 2> yzzz; */ \
  /* VecSwizzleReadOnly<T, 4, 2, 1, 1, 1> zyyy; */ \
  /* VecSwizzleReadOnly<T, 4, 2, 1, 1, 2> zyyz; */ \
  /* VecSwizzleReadOnly<T, 4, 2, 1, 2, 1> zyzy; */ \
  /* VecSwizzleReadOnly<T, 4, 2, 1, 2, 2> zyzz; */ \
  /* VecSwizzleReadOnly<T, 4, 2, 2, 1, 1> zzyy; */ \
  /* VecSwizzleReadOnly<T, 4, 2, 2, 1, 2> zzyz; */ \
  /* VecSwizzleReadOnly<T, 4, 2, 2, 2, 1> zzzy; */

/* Swizzles containing ZW. Must be declared in a union alongside Z. */
#define ZW_SWIZZLES \
  VecSwizzleReadWrite<T, 2> zw; \
  /* VecSwizzleReadOnly<T, 2, 3, 2> wz; */ \
  /* VecSwizzleReadOnly<T, 3, 2, 2, 3> zzw; */ \
  /* VecSwizzleReadOnly<T, 3, 2, 3, 2> zwz; */ \
  /* VecSwizzleReadOnly<T, 3, 2, 3, 3> zww; */ \
  /* VecSwizzleReadOnly<T, 3, 3, 2, 2> wzz; */ \
  /* VecSwizzleReadOnly<T, 3, 3, 2, 3> wzw; */ \
  /* VecSwizzleReadOnly<T, 3, 3, 3, 2> wwz; */ \
  /* VecSwizzleReadOnly<T, 4, 2, 2, 2, 3> zzzw; */ \
  /* VecSwizzleReadOnly<T, 4, 2, 2, 3, 2> zzwz; */ \
  /* VecSwizzleReadOnly<T, 4, 2, 2, 3, 3> zzww; */ \
  /* VecSwizzleReadOnly<T, 4, 2, 3, 2, 2> zwzz; */ \
  /* VecSwizzleReadOnly<T, 4, 2, 3, 2, 3> zwzw; */ \
  /* VecSwizzleReadOnly<T, 4, 2, 3, 3, 2> zwwz; */ \
  /* VecSwizzleReadOnly<T, 4, 2, 3, 3, 3> zwww; */ \
  /* VecSwizzleReadOnly<T, 4, 3, 2, 2, 2> wzzz; */ \
  /* VecSwizzleReadOnly<T, 4, 3, 2, 2, 3> wzzw; */ \
  /* VecSwizzleReadOnly<T, 4, 3, 2, 3, 2> wzwz; */ \
  /* VecSwizzleReadOnly<T, 4, 3, 2, 3, 3> wzww; */ \
  /* VecSwizzleReadOnly<T, 4, 3, 3, 2, 2> wwzz; */ \
  /* VecSwizzleReadOnly<T, 4, 3, 3, 2, 3> wwzw; */ \
  /* VecSwizzleReadOnly<T, 4, 3, 3, 3, 2> wwwz; */

/* Swizzles containing XYZ. Must be declared in a union alongside X. */
#define XYZ_SWIZZLES \
  VecSwizzleReadWrite<T, 3> xyz; \
  VecSwizzleReadOnly<T, 3, 0, 2, 1> xzy; \
  /* VecSwizzleReadOnly<T, 3, 1, 0, 2> yxz; */ \
  VecSwizzleReadOnly<T, 3, 1, 2, 0> yzx; \
  /* VecSwizzleReadOnly<T, 3, 2, 0, 1> zxy; */ \
  VecSwizzleReadOnly<T, 3, 2, 1, 0> zyx; \
  /* VecSwizzleReadOnly<T, 4, 0, 0, 1, 2> xxyz; */ \
  /* VecSwizzleReadOnly<T, 4, 0, 0, 2, 1> xxzy; */ \
  /* VecSwizzleReadOnly<T, 4, 0, 1, 0, 2> xyxz; */ \
  /* VecSwizzleReadOnly<T, 4, 0, 1, 1, 2> xyyz; */ \
  /* VecSwizzleReadOnly<T, 4, 0, 1, 2, 0> xyzx; */ \
  /* VecSwizzleReadOnly<T, 4, 0, 1, 2, 1> xyzy; */ \
  VecSwizzleReadOnly<T, 4, 0, 1, 2, 2> xyzz; \
  /* VecSwizzleReadOnly<T, 4, 0, 2, 0, 1> xzxy; */ \
  /* VecSwizzleReadOnly<T, 4, 0, 2, 1, 0> xzyx; */ \
  /* VecSwizzleReadOnly<T, 4, 0, 2, 1, 1> xzyy; */ \
  /* VecSwizzleReadOnly<T, 4, 0, 2, 1, 2> xzyz; */ \
  /* VecSwizzleReadOnly<T, 4, 0, 2, 2, 1> xzzy; */ \
  /* VecSwizzleReadOnly<T, 4, 1, 0, 0, 2> yxxz; */ \
  /* VecSwizzleReadOnly<T, 4, 1, 0, 1, 2> yxyz; */ \
  /* VecSwizzleReadOnly<T, 4, 1, 0, 2, 0> yxzx; */ \
  /* VecSwizzleReadOnly<T, 4, 1, 0, 2, 1> yxzy; */ \
  /* VecSwizzleReadOnly<T, 4, 1, 0, 2, 2> yxzz; */ \
  /* VecSwizzleReadOnly<T, 4, 1, 1, 0, 2> yyxz; */ \
  /* VecSwizzleReadOnly<T, 4, 1, 1, 2, 0> yyzx; */ \
  /* VecSwizzleReadOnly<T, 4, 1, 2, 0, 0> yzxx; */ \
  /* VecSwizzleReadOnly<T, 4, 1, 2, 0, 1> yzxy; */ \
  /* VecSwizzleReadOnly<T, 4, 1, 2, 0, 2> yzxz; */ \
  /* VecSwizzleReadOnly<T, 4, 1, 2, 1, 0> yzyx; */ \
  /* VecSwizzleReadOnly<T, 4, 1, 2, 2, 0> yzzx; */ \
  /* VecSwizzleReadOnly<T, 4, 2, 0, 0, 1> zxxy; */ \
  /* VecSwizzleReadOnly<T, 4, 2, 0, 1, 0> zxyx; */ \
  /* VecSwizzleReadOnly<T, 4, 2, 0, 1, 1> zxyy; */ \
  /* VecSwizzleReadOnly<T, 4, 2, 0, 1, 2> zxyz; */ \
  /* VecSwizzleReadOnly<T, 4, 2, 0, 2, 1> zxzy; */ \
  /* VecSwizzleReadOnly<T, 4, 2, 1, 0, 0> zyxx; */ \
  /* VecSwizzleReadOnly<T, 4, 2, 1, 0, 1> zyxy; */ \
  /* VecSwizzleReadOnly<T, 4, 2, 1, 0, 2> zyxz; */ \
  /* VecSwizzleReadOnly<T, 4, 2, 1, 1, 0> zyyx; */ \
  /* VecSwizzleReadOnly<T, 4, 2, 1, 2, 0> zyzx; */ \
  /* VecSwizzleReadOnly<T, 4, 2, 2, 0, 1> zzxy; */ \
  /* VecSwizzleReadOnly<T, 4, 2, 2, 1, 0> zzyx; */

/* Swizzles containing YZW. Must be declared in a union alongside Y. */
#define YZW_SWIZZLES \
  VecSwizzleReadWrite<T, 3> yzw; \
  /* VecSwizzleReadOnly<T, 3, 1, 3, 2> ywz; */ \
  /* VecSwizzleReadOnly<T, 3, 2, 1, 3> zyw; */ \
  /* VecSwizzleReadOnly<T, 3, 2, 3, 1> zwy; */ \
  /* VecSwizzleReadOnly<T, 3, 3, 1, 2> wyz; */ \
  /* VecSwizzleReadOnly<T, 3, 3, 2, 1> wzy; */ \
  /* VecSwizzleReadOnly<T, 4, 1, 1, 2, 3> yyzw; */ \
  /* VecSwizzleReadOnly<T, 4, 1, 1, 3, 2> yywz; */ \
  /* VecSwizzleReadOnly<T, 4, 1, 2, 1, 3> yzyw; */ \
  /* VecSwizzleReadOnly<T, 4, 1, 2, 2, 3> yzzw; */ \
  /* VecSwizzleReadOnly<T, 4, 1, 2, 3, 1> yzwy; */ \
  /* VecSwizzleReadOnly<T, 4, 1, 2, 3, 2> yzwz; */ \
  /* VecSwizzleReadOnly<T, 4, 1, 2, 3, 3> yzww; */ \
  /* VecSwizzleReadOnly<T, 4, 1, 3, 1, 2> ywyz; */ \
  /* VecSwizzleReadOnly<T, 4, 1, 3, 2, 1> ywzy; */ \
  /* VecSwizzleReadOnly<T, 4, 1, 3, 2, 2> ywzz; */ \
  /* VecSwizzleReadOnly<T, 4, 1, 3, 2, 3> ywzw; */ \
  /* VecSwizzleReadOnly<T, 4, 1, 3, 3, 2> ywwz; */ \
  /* VecSwizzleReadOnly<T, 4, 2, 1, 1, 3> zyyw; */ \
  /* VecSwizzleReadOnly<T, 4, 2, 1, 2, 3> zyzw; */ \
  /* VecSwizzleReadOnly<T, 4, 2, 1, 3, 1> zywy; */ \
  /* VecSwizzleReadOnly<T, 4, 2, 1, 3, 2> zywz; */ \
  /* VecSwizzleReadOnly<T, 4, 2, 1, 3, 3> zyww; */ \
  /* VecSwizzleReadOnly<T, 4, 2, 2, 1, 3> zzyw; */ \
  /* VecSwizzleReadOnly<T, 4, 2, 2, 3, 1> zzwy; */ \
  /* VecSwizzleReadOnly<T, 4, 2, 3, 1, 1> zwyy; */ \
  /* VecSwizzleReadOnly<T, 4, 2, 3, 1, 2> zwyz; */ \
  /* VecSwizzleReadOnly<T, 4, 2, 3, 1, 3> zwyw; */ \
  /* VecSwizzleReadOnly<T, 4, 2, 3, 2, 1> zwzy; */ \
  /* VecSwizzleReadOnly<T, 4, 2, 3, 3, 1> zwwy; */ \
  /* VecSwizzleReadOnly<T, 4, 3, 1, 1, 2> wyyz; */ \
  /* VecSwizzleReadOnly<T, 4, 3, 1, 2, 1> wyzy; */ \
  /* VecSwizzleReadOnly<T, 4, 3, 1, 2, 2> wyzz; */ \
  /* VecSwizzleReadOnly<T, 4, 3, 1, 2, 3> wyzw; */ \
  /* VecSwizzleReadOnly<T, 4, 3, 1, 3, 2> wywz; */ \
  /* VecSwizzleReadOnly<T, 4, 3, 2, 1, 1> wzyy; */ \
  /* VecSwizzleReadOnly<T, 4, 3, 2, 1, 2> wzyz; */ \
  /* VecSwizzleReadOnly<T, 4, 3, 2, 1, 3> wzyw; */ \
  /* VecSwizzleReadOnly<T, 4, 3, 2, 2, 1> wzzy; */ \
  /* VecSwizzleReadOnly<T, 4, 3, 2, 3, 1> wzwy; */ \
  /* VecSwizzleReadOnly<T, 4, 3, 3, 1, 2> wwyz; */ \
  /* VecSwizzleReadOnly<T, 4, 3, 3, 2, 1> wwzy; */

/* Swizzles containing XYZW. Must be declared in a union alongside X. */
#define XYZW_SWIZZLES \
  VecSwizzleReadWrite<T, 4> xyzw; \
  /* VecSwizzleReadOnly<T, 4, 0, 1, 3, 2> xywz; */ \
  /* VecSwizzleReadOnly<T, 4, 0, 2, 1, 3> xzyw; */ \
  /* VecSwizzleReadOnly<T, 4, 0, 2, 3, 1> xzwy; */ \
  /* VecSwizzleReadOnly<T, 4, 0, 3, 1, 2> xwyz; */ \
  /* VecSwizzleReadOnly<T, 4, 0, 3, 2, 1> xwzy; */ \
  /* VecSwizzleReadOnly<T, 4, 1, 0, 2, 3> yxzw; */ \
  /* VecSwizzleReadOnly<T, 4, 1, 0, 3, 2> yxwz; */ \
  /* VecSwizzleReadOnly<T, 4, 1, 2, 0, 3> yzxw; */ \
  /* VecSwizzleReadOnly<T, 4, 1, 2, 3, 0> yzwx; */ \
  /* VecSwizzleReadOnly<T, 4, 1, 3, 0, 2> ywxz; */ \
  /* VecSwizzleReadOnly<T, 4, 1, 3, 2, 0> ywzx; */ \
  /* VecSwizzleReadOnly<T, 4, 2, 0, 1, 3> zxyw; */ \
  /* VecSwizzleReadOnly<T, 4, 2, 0, 3, 1> zxwy; */ \
  /* VecSwizzleReadOnly<T, 4, 2, 1, 0, 3> zyxw; */ \
  /* VecSwizzleReadOnly<T, 4, 2, 1, 3, 0> zywx; */ \
  /* VecSwizzleReadOnly<T, 4, 2, 3, 0, 1> zwxy; */ \
  /* VecSwizzleReadOnly<T, 4, 2, 3, 1, 0> zwyx; */ \
  /* VecSwizzleReadOnly<T, 4, 3, 0, 1, 2> wxyz; */ \
  /* VecSwizzleReadOnly<T, 4, 3, 0, 2, 1> wxzy; */ \
  /* VecSwizzleReadOnly<T, 4, 3, 1, 0, 2> wyxz; */ \
  /* VecSwizzleReadOnly<T, 4, 3, 1, 2, 0> wyzx; */ \
  /* VecSwizzleReadOnly<T, 4, 3, 2, 0, 1> wzxy; */ \
  VecSwizzleReadOnly<T, 4, 3, 2, 1, 0> wzyx;

}  // namespace blender
