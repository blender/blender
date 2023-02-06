/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_kdtree.h"
#include "BLI_math_matrix_types.hh"
#include "BLI_math_vector.hh"
#include "BLI_span.hh"

#include "BKE_bvhutils.h"
#include "BKE_curves.hh"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "GEO_reverse_uv_sampler.hh"

namespace blender::geometry {

struct AddCurvesOnMeshInputs {
  /** UV Coordinates at which the new curves should be added. */
  Span<float2> uvs;

  /** Determines shape of new curves. */
  bool interpolate_length = false;
  bool interpolate_shape = false;
  bool interpolate_point_count = false;
  float fallback_curve_length = 0.0f;
  int fallback_point_count = 0;

  /** Information about the surface that the new curves are attached to. */
  const Mesh *surface = nullptr;
  Span<MLoopTri> surface_looptris;
  const ReverseUVSampler *reverse_uv_sampler = nullptr;
  Span<float3> corner_normals_su;

  bke::CurvesSurfaceTransforms *transforms = nullptr;

  /**
   * KD-Tree that contains the root points of existing curves. This is only necessary when
   * interpolation is used.
   */
  KDTree_3d *old_roots_kdtree = nullptr;

  bool r_uv_error = false;
};

struct AddCurvesOnMeshOutputs {
  bool uv_error = false;
  IndexRange new_curves_range;
  IndexRange new_points_range;
};

/**
 * Generate new curves on a mesh surface with the given inputs. Existing curves stay intact.
 */
AddCurvesOnMeshOutputs add_curves_on_mesh(bke::CurvesGeometry &curves,
                                          const AddCurvesOnMeshInputs &inputs);

float3 compute_surface_point_normal(const MLoopTri &looptri,
                                    const float3 &bary_coord,
                                    const Span<float3> corner_normals);

}  // namespace blender::geometry
