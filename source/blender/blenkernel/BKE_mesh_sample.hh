/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#pragma once

/** \file
 * \ingroup bke
 */

#include "FN_generic_virtual_array.hh"

#include "BLI_float3.hh"

#include "BKE_attribute.h"

struct Mesh;

namespace blender::bke {
struct ReadAttributeLookup;
class OutputAttribute;
}  // namespace blender::bke

namespace blender::bke::mesh_surface_sample {

using fn::CPPType;
using fn::GMutableSpan;
using fn::GSpan;
using fn::GVArray;

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
  const Span<float3> positions_;
  const Span<int> looptri_indices_;

  Array<float3> bary_coords_;
  Array<float3> nearest_weights_;

 public:
  MeshAttributeInterpolator(const Mesh *mesh,
                            const Span<float3> positions,
                            const Span<int> looptri_indices);

  void sample_data(const GVArray &src,
                   const AttributeDomain domain,
                   const eAttributeMapMode mode,
                   const IndexMask mask,
                   const GMutableSpan dst);

  void sample_attribute(const ReadAttributeLookup &src_attribute,
                        OutputAttribute &dst_attribute,
                        eAttributeMapMode mode);

 protected:
  Span<float3> ensure_barycentric_coords();
  Span<float3> ensure_nearest_weights();
};

}  // namespace blender::bke::mesh_surface_sample
