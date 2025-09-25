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
/** \name Sampler Types
 * \{ */

template<typename T,
         int Dimensions,
         bool Cube = false,
         bool Array = false,
         bool Atomic = false,
         bool Depth = false>
struct SamplerBase {
  static constexpr int coord_dim = Dimensions + int(Cube) + int(Array);
  static constexpr int deriv_dim = Dimensions + int(Cube);
  static constexpr int extent_dim = Dimensions + int(Array);

  using int_coord_type = VecBase<int, coord_dim>;
  using flt_coord_type = VecBase<float, coord_dim>;
  using derivative_type = VecBase<float, deriv_dim>;
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
TEX_TEMPLATE DataVec texture(T, FltCoord, float /*bias*/ = 0.0f) RET;
TEX_TEMPLATE DataVec textureGather(T, FltCoord) RET;
TEX_TEMPLATE DataVec textureGrad(T, FltCoord, DerivVec, DerivVec) RET;
TEX_TEMPLATE DataVec textureLod(T, FltCoord, float) RET;
TEX_TEMPLATE DataVec textureLodOffset(T, FltCoord, float, IntCoord) RET;

#undef TEX_TEMPLATE

using samplerBuffer = SamplerBase<float, 1>;
using sampler1D = SamplerBase<float, 1>;
using sampler2D = SamplerBase<float, 2>;
using sampler3D = SamplerBase<float, 3>;
using isamplerBuffer = SamplerBase<int, 1>;
using isampler1D = SamplerBase<int, 1>;
using isampler2D = SamplerBase<int, 2>;
using isampler3D = SamplerBase<int, 3>;
using usamplerBuffer = SamplerBase<uint, 1>;
using usampler1D = SamplerBase<uint, 1>;
using usampler2D = SamplerBase<uint, 2>;
using usampler3D = SamplerBase<uint, 3>;

using sampler1DArray = SamplerBase<float, 1, false, true>;
using sampler2DArray = SamplerBase<float, 2, false, true>;
using isampler1DArray = SamplerBase<int, 1, false, true>;
using isampler2DArray = SamplerBase<int, 2, false, true>;
using usampler1DArray = SamplerBase<uint, 1, false, true>;
using usampler2DArray = SamplerBase<uint, 2, false, true>;

using samplerCube = SamplerBase<float, 2, true>;
using isamplerCube = SamplerBase<int, 2, true>;
using usamplerCube = SamplerBase<uint, 2, true>;

using samplerCubeArray = SamplerBase<float, 2, true, true>;
using isamplerCubeArray = SamplerBase<int, 2, true, true>;
using usamplerCubeArray = SamplerBase<uint, 2, true, true>;

using usampler1DAtomic = SamplerBase<uint, 1, false, false, true>;
using usampler2DAtomic = SamplerBase<uint, 2, false, false, true>;
using usampler2DArrayAtomic = SamplerBase<uint, 2, false, true, true>;
using usampler3DAtomic = SamplerBase<uint, 3, false, false, true>;

using isampler1DAtomic = SamplerBase<int, 1, false, false, true>;
using isampler2DAtomic = SamplerBase<int, 2, false, false, true>;
using isampler2DArrayAtomic = SamplerBase<int, 2, false, true, true>;
using isampler3DAtomic = SamplerBase<int, 3, false, false, true>;

using sampler2DDepth = SamplerBase<float, 2, false, false, false, true>;
using sampler2DArrayDepth = SamplerBase<float, 2, false, true, false, true>;
using samplerCubeDepth = SamplerBase<float, 2, true, false, false, true>;
using samplerCubeArrayDepth = SamplerBase<float, 2, true, true, false, true>;

/* Sampler Buffers do not have LOD. */
float4 texelFetch(samplerBuffer, int) RET;
int4 texelFetch(isamplerBuffer, int) RET;
uint4 texelFetch(usamplerBuffer, int) RET;

/** \} */

#undef RET
