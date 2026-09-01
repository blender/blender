/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "spz_read_common.hh"

#include <array>

#include "BLI_array.hh"
#include "BLI_assert.hh"
#include "BLI_enumerable_thread_specific.hh"
#include "BLI_math_rotation.hh"

#include "IO_gsplat.hh"

namespace blender::io::spz {

namespace {

/* Spherical harmonics rotation matrices for R_x(+pi/2).
 * Each entry operates on the 2l+1 coefficients of band l (l = 1..4).
 *
 * The table is adopted from the SPZ Library project:
 *
 *   MIT License
 *   Copyright (c) 2024 Niantic Labs
 *
 * There are modifications in the code to use Blender utilities, and also to operate on float3
 * elements rather than on scalars.
 *
 * The matrices structure seems to match description given in the
 *
 *   Didier Pinchon1 and Philip E. Hoggan
 *   "Rotation matrices for real spherical harmonics:  general rotations of atomic orbitals in
 *   space-fixed axes" (2007)
 *   Journal of Physics A: Mathematical and Theoretical.
 *   DOI:10.1088/1751-8113/40/7/011
 *
 * This could also be confirmed by checking against a naive implementation of the Wigner D-matrix
 * [WignerDMatrix]. When doing so a basis conversion needs to be applied to convert complex
 * spherical harmonics to real [SphericalRealForm].
 *
 * References:
 *   [WignerDMatrix]     https://en.wikipedia.org/wiki/Wigner_D-matrix
 *   [SphericalRealForm] https://en.wikipedia.org/wiki/Spherical_harmonics#Real_form */
inline constexpr std::array<void (*)(MutableSpan<float3>), 4>
    ANALYTIC_ROTATE_PLUS_PI_HALF_ABOUT_X_TABLE = {
        [](const MutableSpan<float3> p) {
          BLI_assert(p.size() == 3);
          const float3 t0 = p[0], t1 = p[1], t2 = p[2];
          p[0] = t1;
          p[1] = -t0;
          p[2] = t2;
        },
        [](const MutableSpan<float3> p) {
          BLI_assert(p.size() == 5);
          std::array<float3, 5> s;
          MutableSpan(s).copy_from(p);
          const float s3 = std::sqrt(3.0f);
          p[0] = s[3];
          p[1] = -s[1];
          p[2] = -0.5f * s[2] - (s3 / 2.0f) * s[4];
          p[3] = -s[0];
          p[4] = -(s3 / 2.0f) * s[2] + 0.5f * s[4];
        },
        [](const MutableSpan<float3> p) {
          BLI_assert(p.size() == 7);
          std::array<float3, 7> s{};
          MutableSpan(s).copy_from(p);
          const float s15 = std::sqrt(15.0f);
          p[0] = -std::sqrt(5.0f / 8.0f) * s[3] + std::sqrt(3.0f / 8.0f) * s[5];
          p[1] = -s[1];
          p[2] = -std::sqrt(3.0f / 8.0f) * s[3] - std::sqrt(5.0f / 8.0f) * s[5];
          p[3] = std::sqrt(5.0f / 8.0f) * s[0] + std::sqrt(3.0f / 8.0f) * s[2];
          p[4] = -0.25f * s[4] - (s15 / 4.0f) * s[6];
          p[5] = -std::sqrt(3.0f / 8.0f) * s[0] + std::sqrt(5.0f / 8.0f) * s[2];
          p[6] = -(s15 / 4.0f) * s[4] + 0.25f * s[6];
        },
        [](const MutableSpan<float3> p) {
          BLI_assert(p.size() == 9);
          std::array<float3, 9> s{};
          MutableSpan(s).copy_from(p);
          const float s2 = std::sqrt(2.0f);
          const float s5 = std::sqrt(5.0f);
          const float s7 = std::sqrt(7.0f);
          const float s14 = std::sqrt(14.0f);
          const float s35 = std::sqrt(35.0f);
          p[0] = -(s14 / 4.0f) * s[5] + (s2 / 4.0f) * s[7];
          p[1] = -0.75f * s[1] + (s7 / 4.0f) * s[3];
          p[2] = -(s2 / 4.0f) * s[5] - (s14 / 4.0f) * s[7];
          p[3] = (s7 / 4.0f) * s[1] + 0.75f * s[3];
          p[4] = (3.0f / 8.0f) * s[4] + (s5 / 4.0f) * s[6] + (s35 / 8.0f) * s[8];
          p[5] = (s14 / 4.0f) * s[0] + (s2 / 4.0f) * s[2];
          p[6] = (s5 / 4.0f) * s[4] + 0.5f * s[6] - (s7 / 4.0f) * s[8];
          p[7] = -(s2 / 4.0f) * s[0] + (s14 / 4.0f) * s[2];
          p[8] = (s35 / 8.0f) * s[4] - (s7 / 4.0f) * s[6] + 0.125f * s[8];
        },
};

}  // namespace

void convert_axis_to_blender(const MutableSpan<float3> positions,
                             const MutableSpan<math::Quaternion> rotations,
                             const Span<MutableSpan<float3>> sh_attrs)
{
  /* By default, SPZ stores data in an RUB (Right, Up, Back) coordinate system following the OpenGL
   * and three.js convention. Blender is RFU (Right, Forward, Up). */

  /* TODO(sergey): Handle extensions that define coordinate system. */
  /* Example: SPZ_ADOBE_coordinate_system. */

  const int sh_degrees = gsplat::degree_for_dimension(sh_attrs.size());

  struct LocalData {
    Array<float3> sh_data;
  };
  threading::EnumerableThreadSpecific<LocalData> all_tls;
  threading::parallel_for(IndexRange(positions.size()), 32, [&](const IndexRange range) {
    LocalData &tls = all_tls.local();
    for (const int i : range) {
      positions[i] = {positions[i].x, -positions[i].z, positions[i].y};

      /* Simplified expression for the following:
       * math::to_quaternion(math::AxisAngle(math::AxisSigned::X_POS, M_PI_2)) * rotations[i]. */
      rotations[i] = math::Quaternion(M_SQRT1_2 * (rotations[i].w - rotations[i].x),
                                      M_SQRT1_2 * (rotations[i].x + rotations[i].w),
                                      M_SQRT1_2 * (rotations[i].y - rotations[i].z),
                                      M_SQRT1_2 * (rotations[i].z + rotations[i].y));

      if (sh_degrees && tls.sh_data.is_empty()) {
        tls.sh_data.reinitialize(sh_attrs.size());
      }

      MutableSpan<float3> sh_data = tls.sh_data;
      gsplat::get_spherical_harmonics(sh_attrs, i, sh_data);
      for (int band = 0; band < sh_degrees; ++band) {
        ANALYTIC_ROTATE_PLUS_PI_HALF_ABOUT_X_TABLE[band](
            sh_data.slice(band * (band + 2), 2 * (band + 1) + 1));
      }
      gsplat::set_spherical_harmonics(sh_attrs, i, sh_data);
    }
  });
}

}  // namespace blender::io::spz
