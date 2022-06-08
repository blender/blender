/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bke
 */

#include "BLI_generic_virtual_array.hh"
#include "BLI_math_vec_types.hh"

#include "BKE_attribute.h"

struct Mesh;

namespace blender::bke {
struct ReadAttributeLookup;
class OutputAttribute;
}  // namespace blender::bke

namespace blender::bke::mesh_surface_sample {

void sample_point_attribute(const Mesh &mesh,
                            Span<int> looptri_indices,
                            Span<float3> bary_coords,
                            const GVArray &data_in,
                            const IndexMask mask,
                            GMutableSpan data_out);

void sample_corner_attribute(const Mesh &mesh,
                             Span<int> looptri_indices,
                             Span<float3> bary_coords,
                             const GVArray &data_in,
                             const IndexMask mask,
                             GMutableSpan data_out);

void sample_face_attribute(const Mesh &mesh,
                           Span<int> looptri_indices,
                           const GVArray &data_in,
                           const IndexMask mask,
                           GMutableSpan data_out);

enum class eAttributeMapMode {
  INTERPOLATED,
  NEAREST,
};

/**
 * A utility class that performs attribute interpolation from a source mesh.
 *
 * The interpolator is only valid as long as the mesh is valid.
 * Barycentric weights are needed when interpolating point or corner domain attributes,
 * these are computed lazily when needed and re-used.
 */
class MeshAttributeInterpolator {
 private:
  const Mesh *mesh_;
  const IndexMask mask_;
  const Span<float3> positions_;
  const Span<int> looptri_indices_;

  Array<float3> bary_coords_;
  Array<float3> nearest_weights_;

 public:
  MeshAttributeInterpolator(const Mesh *mesh,
                            const IndexMask mask,
                            const Span<float3> positions,
                            const Span<int> looptri_indices);

  void sample_data(const GVArray &src,
                   eAttrDomain domain,
                   eAttributeMapMode mode,
                   const GMutableSpan dst);

  void sample_attribute(const ReadAttributeLookup &src_attribute,
                        OutputAttribute &dst_attribute,
                        eAttributeMapMode mode);

 protected:
  Span<float3> ensure_barycentric_coords();
  Span<float3> ensure_nearest_weights();
};

}  // namespace blender::bke::mesh_surface_sample
