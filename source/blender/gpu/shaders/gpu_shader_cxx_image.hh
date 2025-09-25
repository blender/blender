/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * C++ stubs for shading language.
 *
 * IMPORTANT: Please ask the module team if you need some feature that are not listed in this file.
 */

#pragma once

#include "gpu_shader_cxx_vector.hh"

/* Some compilers complain about lack of return values. Keep it short. */
#define RET \
  { \
    return {}; \
  }

/* -------------------------------------------------------------------- */
/** \name Image Types
 * \{ */

template<typename T, int Dimensions, bool Array = false, bool Atomic = false> struct ImageBase {
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
IMG_TEMPLATE uint imageAtomicOr(T &, IntCoord, uint) RET;
IMG_TEMPLATE uint imageAtomicExchange(T &, IntCoord, uint) RET;
IMG_TEMPLATE uint imageAtomicCompSwap(T &, IntCoord, uint, uint) RET;
/* Cannot write to a read only image. */
IMG_TEMPLATE uint imageAtomicAdd(const T &, IntCoord, uint) = delete;
IMG_TEMPLATE uint imageAtomicMin(const T &, IntCoord, uint) = delete;
IMG_TEMPLATE uint imageAtomicMax(const T &, IntCoord, uint) = delete;
IMG_TEMPLATE uint imageAtomicAnd(const T &, IntCoord, uint) = delete;
IMG_TEMPLATE uint imageAtomicXor(const T &, IntCoord, uint) = delete;
IMG_TEMPLATE uint imageAtomicOr(const T &, IntCoord, uint) = delete;
IMG_TEMPLATE uint imageAtomicExchange(const T &, IntCoord, uint) = delete;
IMG_TEMPLATE uint imageAtomicCompSwap(const T &, IntCoord, uint, uint) = delete;

#undef IMG_TEMPLATE

using image1D = ImageBase<float, 1>;
using image2D = ImageBase<float, 2>;
using image3D = ImageBase<float, 3>;
using iimage1D = ImageBase<int, 1>;
using iimage2D = ImageBase<int, 2>;
using iimage3D = ImageBase<int, 3>;
using uimage1D = ImageBase<uint, 1>;
using uimage2D = ImageBase<uint, 2>;
using uimage3D = ImageBase<uint, 3>;

using image1DArray = ImageBase<float, 1, true>;
using image2DArray = ImageBase<float, 2, true>;
using iimage1DArray = ImageBase<int, 1, true>;
using iimage2DArray = ImageBase<int, 2, true>;
using uimage1DArray = ImageBase<uint, 1, true>;
using uimage2DArray = ImageBase<uint, 2, true>;

using iimage2DAtomic = ImageBase<int, 2, false, true>;
using iimage3DAtomic = ImageBase<int, 3, false, true>;
using uimage2DAtomic = ImageBase<uint, 2, false, true>;
using uimage3DAtomic = ImageBase<uint, 3, false, true>;

using iimage2DArrayAtomic = ImageBase<int, 2, true, true>;
using uimage2DArrayAtomic = ImageBase<uint, 2, true, true>;

/* Forbid Cube and cube arrays. Bind them as 3D textures instead. */

/** \} */

#undef RET
