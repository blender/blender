/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_float4x4.hh"
#include "BLI_kdtree.h"
#include "BLI_math_vector.hh"
#include "BLI_span.hh"

#include "BKE_bvhutils.h"
#include "BKE_curves.hh"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

namespace blender::geometry {

struct AddCurvesOnMeshInputs {
  /** Information about the root points where new curves should be generated. */
  Span<float3> root_positions_cu;
  Span<float3> bary_coords;
  Span<int> looptri_indices;

  /** Determines shape of new curves. */
  bool interpolate_length = false;
  bool interpolate_shape = false;
  bool interpolate_point_count = false;
  float fallback_curve_length = 0.0f;
  int fallback_point_count = 0;

  /** Information about the surface that the new curves are attached to. */
  const Mesh *surface = nullptr;
  BVHTreeFromMesh *surface_bvh = nullptr;
  Span<MLoopTri> surface_looptris;
  Span<float2> surface_uv_map;
  Span<float3> corner_normals_su;

  /** Transformation matrices. */
  float4x4 curves_to_surface_mat;
  float4x4 surface_to_curves_normal_mat;

  /**
   * KD-Tree that contains the root points of existing curves. This is only necessary when
   * interpolation is used.
   */
  KDTree_3d *old_roots_kdtree = nullptr;
};

/**
 * Generate new curves on a mesh surface with the given inputs. Existing curves stay intact.
 */
void add_curves_on_mesh(bke::CurvesGeometry &curves, const AddCurvesOnMeshInputs &inputs);

}  // namespace blender::geometry
