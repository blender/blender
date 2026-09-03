/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spz
 */

#pragma once

#include <cmath>

#include "BLI_array.hh"
#include "BLI_math_constants.hh"
#include "BLI_math_quaternion_types.hh"
#include "BLI_math_vector.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_span.hh"
#include "BLI_unroll.hh"

#include "IO_gsplat.hh"

namespace blender::io::spz {

enum HeaderFlag {
  SPZ_HEADER_ANTIALIASED = 0x01,
  SPZ_HEADER_HAS_EXTENSIONS = 0x02,
};

namespace internal {

template<class ReaderType>
[[nodiscard]] inline bool read_single_position_as_fixed(ReaderType &reader, int3 &fixed)
{
  using PackedFloat = std::array<uint8_t, 3>;
  using PackedFloat3 = std::array<PackedFloat, 3>;
  PackedFloat3 packed;
  if (!reader.read(packed)) {
    return false;
  }

  unroll<3>([&](auto i) {
    fixed[i] = packed[i][0];
    fixed[i] |= packed[i][1] << 8;
    fixed[i] |= packed[i][2] << 16;
    fixed[i] |= (fixed[i] & 0x800000) ? 0xff000000 : 0;
  });

  return true;
}

inline float inv_sigmoid(const float x)
{
  return std::log(x / (1.0f - x));
}

}  // namespace internal

template<class ReaderType>
[[nodiscard]] inline bool read_positions(ReaderType &reader,
                                         const int fractional_bits,
                                         const MutableSpan<float3> positions)
{
  /* From documentation:
   *
   *   Positions are represented as (x, y, z) coordinates, each as a 24-bit fixed point signed
   *   integer. The number of fractional bits is determined by the fractionalBits field in the
   *   header. */
  BLI_assert(fractional_bits >= 0 && fractional_bits < 24);
  const float scale = 1.0f / (1 << fractional_bits);
  for (float3 &position : positions) {
    int3 fixed;
    if (!internal::read_single_position_as_fixed(reader, fixed)) {
      return false;
    }
    position = float3(fixed) * scale;
  }
  return true;
}

template<class ReaderType>
[[nodiscard]] inline bool read_alphas(ReaderType &reader, const MutableSpan<float4> radiance_base)
{
  for (float4 &base : radiance_base) {
    uint8_t alpha;
    if (!reader.read(alpha)) {
      return false;
    }
    if (false) {
      /* The naive implementation.
       * The inv_sigmoid() and decode_opacity() are actually canceling each other.
       * Code kept for the reference and possible situation when other activation functions are
       * used in the future. */
      base.w = internal::inv_sigmoid(float(alpha) / 255.0f);
      base.w = gsplat::OriginalActivationFunctions::decode_opacity(base.w);
    }
    else {
      base.w = float(alpha) / 255.0f;
    }
  }
  return true;
}

template<class ReaderType>
[[nodiscard]] inline bool read_colors(ReaderType &reader, const MutableSpan<float4> radiance_base)
{
  /* From SPZ:
   *   Scale factor for DC color components. To convert to RGB, we should multiply by 0.282, but it
   *   can be useful to represent base colors that are out of range if the higher spherical
   *   harmonics bands bring them back into range so we multiply by a smaller value. */
  constexpr float COLOR_SCALE = 0.15f;
  const float INV_COLOR_SCALE = 1.0f / COLOR_SCALE;

  for (float4 &base : radiance_base) {
    std::array<uint8_t, 3> color_buffer;
    if (!reader.read_buffer(color_buffer)) {
      return false;
    }
    base.x = ((color_buffer[0] / 255.0f) - 0.5f) * INV_COLOR_SCALE;
    base.y = ((color_buffer[1] / 255.0f) - 0.5f) * INV_COLOR_SCALE;
    base.z = ((color_buffer[2] / 255.0f) - 0.5f) * INV_COLOR_SCALE;
  }

  return true;
}

template<class ReaderType>
[[nodiscard]] inline bool read_scales(ReaderType &reader, const MutableSpan<float3> scales)
{
  /* From documentation:
   *
   *   Scales are represented as (x, y, z) components, each represented as an 8-bit log-encoded
   *   integer. */
  for (float3 &scale : scales) {
    std::array<uint8_t, 3> encoded;
    if (!reader.read_buffer(encoded)) {
      return false;
    }
    scale = float3(encoded[0], encoded[1], encoded[2]) / 16.0f - 10.0f;
    scale = gsplat::OriginalActivationFunctions::decode_scale(scale);
  }
  return true;
}

template<class ReaderType>
[[nodiscard]] inline bool read_rotations(ReaderType &reader,
                                         const int version,
                                         const MutableSpan<math::Quaternion> rotations)
{
  if (version == 2) {
    /* From documentation:
     *
     *   In version 2, rotations are represented as the (x, y, z) components of the normalized
     *   rotation quaternion. The w component can be derived from the others and is not stored.
     *   Each component is encoded as an 8-bit signed integer. */
    for (math::Quaternion &rotation : rotations) {
      std::array<uint8_t, 3> packed;
      if (!reader.read_buffer(packed)) {
        return false;
      }

      const float3 first3 = float3(packed[0], packed[1], packed[2]) / 127.5f - 1.0f;
      rotation.w = std::sqrt(std::max(0.0f, 1.0f - math::dot(first3, first3)));
      rotation.x = first3.x;
      rotation.y = first3.y;
      rotation.z = first3.z;
    }

    return true;
  }

  if (version >= 3) {
    /* From documentation:
     *
     *   In version 3, rotations are represented as the smallest three components of the normalized
     *   rotation quaternion, for optimal rotation accuracy. The largest component can be derived
     *   from the others and is not stored. Its index is stored on 2 bits and each of the smallest
     *   three components is encoded as a 10-bit signed integer. */
    constexpr uint32_t COMPONENT_MASK = (1 << 9) - 1;
    for (math::Quaternion &rotation : rotations) {
      /* Read as individual bytes and re-pack to integer to ensure endianess. */
      std::array<uint8_t, 4> packed_bytes;
      if (!reader.read_buffer(packed_bytes)) {
        return false;
      }
      float spz_rotation[4];
      uint32_t packed = packed_bytes[0] + (packed_bytes[1] << 8) + (packed_bytes[2] << 16) +
                        (packed_bytes[3] << 24);
      const int largest_index = packed >> 30;
      float sum_sq = 0.0f;
      for (int i = 3; i >= 0; --i) {
        if (i == largest_index) {
          continue;
        }
        const uint32_t mag = packed & COMPONENT_MASK;
        const float sign = ((packed >> 9) & 0x1) == 1 ? -1.0f : 1.0f;
        spz_rotation[i] = M_SQRT1_2 * ((float)mag) / float(COMPONENT_MASK) * sign;
        sum_sq += spz_rotation[i] * spz_rotation[i];
        packed = packed >> 10;
      }
      spz_rotation[largest_index] = sqrt(math::max(1.0f - sum_sq, 0.0f));
      /* Convert to Blender's (w, x, y, z). */
      rotation = math::Quaternion(
          spz_rotation[3], spz_rotation[0], spz_rotation[1], spz_rotation[2]);
    }
    return true;
  }

  return false;
}

template<class ReaderType>
[[nodiscard]] inline bool read_sh(ReaderType &reader, const Span<MutableSpan<float3>> sh)
{
  /* From documentation:
   *
   *   The coefficients for a gaussian are organized such that the color channel is the inner
   *   (faster varying) axis, and the coefficient is the outer (slower varying) axis
   *   Each coefficient is represented as an 8-bit signed integer. Additional quantization can be
   *   performed to attain a higher compression ratio */

  if (sh.is_empty()) {
    return true;
  }

  const int64_t num_points = sh[0].size();
  const int num_coefficients = sh.size();

  Array<uint8_t> packed_coefficients(3 * num_coefficients);

  for (int64_t i = 0; i < num_points; ++i) {
    if (!reader.read_buffer(packed_coefficients)) {
      return false;
    }
    for (int j = 0; j < num_coefficients; ++j) {
      const float3 coefficients(packed_coefficients[3 * j + 0],
                                packed_coefficients[3 * j + 1],
                                packed_coefficients[3 * j + 2]);
      sh[j][i] = (coefficients - 128.0f) / 128.0f;
    }
  }

  return true;
}

void convert_axis_to_blender(MutableSpan<float3> positions,
                             MutableSpan<math::Quaternion> rotations,
                             Span<MutableSpan<float3>> sh_attrs);

}  // namespace blender::io::spz
