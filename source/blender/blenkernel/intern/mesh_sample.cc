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

#include "BKE_attribute_access.hh"
#include "BKE_attribute_math.hh"
#include "BKE_mesh_runtime.h"
#include "BKE_mesh_sample.hh"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

namespace blender::bke::mesh_surface_sample {

template<typename T>
BLI_NOINLINE static void sample_point_attribute(const Mesh &mesh,
                                                const Span<int> looptri_indices,
                                                const Span<float3> bary_coords,
                                                const VArray<T> &data_in,
                                                const IndexMask mask,
                                                const MutableSpan<T> data_out)
{
  const Span<MLoopTri> looptris{BKE_mesh_runtime_looptri_ensure(&mesh),
                                BKE_mesh_runtime_looptri_len(&mesh)};

  for (const int i : mask) {
    const int looptri_index = looptri_indices[i];
    const MLoopTri &looptri = looptris[looptri_index];
    const float3 &bary_coord = bary_coords[i];

    const int v0_index = mesh.mloop[looptri.tri[0]].v;
    const int v1_index = mesh.mloop[looptri.tri[1]].v;
    const int v2_index = mesh.mloop[looptri.tri[2]].v;

    const T v0 = data_in[v0_index];
    const T v1 = data_in[v1_index];
    const T v2 = data_in[v2_index];

    const T interpolated_value = attribute_math::mix3(bary_coord, v0, v1, v2);
    data_out[i] = interpolated_value;
  }
}

void sample_point_attribute(const Mesh &mesh,
                            const Span<int> looptri_indices,
                            const Span<float3> bary_coords,
                            const GVArray &data_in,
                            const IndexMask mask,
                            const GMutableSpan data_out)
{
  BLI_assert(data_out.size() == looptri_indices.size());
  BLI_assert(data_out.size() == bary_coords.size());
  BLI_assert(data_in.size() == mesh.totvert);
  BLI_assert(data_in.type() == data_out.type());

  const CPPType &type = data_in.type();
  attribute_math::convert_to_static_type(type, [&](auto dummy) {
    using T = decltype(dummy);
    sample_point_attribute<T>(
        mesh, looptri_indices, bary_coords, data_in.typed<T>(), mask, data_out.typed<T>());
  });
}

template<typename T>
BLI_NOINLINE static void sample_corner_attribute(const Mesh &mesh,
                                                 const Span<int> looptri_indices,
                                                 const Span<float3> bary_coords,
                                                 const VArray<T> &data_in,
                                                 const IndexMask mask,
                                                 const MutableSpan<T> data_out)
{
  const Span<MLoopTri> looptris{BKE_mesh_runtime_looptri_ensure(&mesh),
                                BKE_mesh_runtime_looptri_len(&mesh)};

  for (const int i : mask) {
    const int looptri_index = looptri_indices[i];
    const MLoopTri &looptri = looptris[looptri_index];
    const float3 &bary_coord = bary_coords[i];

    const int loop_index_0 = looptri.tri[0];
    const int loop_index_1 = looptri.tri[1];
    const int loop_index_2 = looptri.tri[2];

    const T v0 = data_in[loop_index_0];
    const T v1 = data_in[loop_index_1];
    const T v2 = data_in[loop_index_2];

    const T interpolated_value = attribute_math::mix3(bary_coord, v0, v1, v2);
    data_out[i] = interpolated_value;
  }
}

void sample_corner_attribute(const Mesh &mesh,
                             const Span<int> looptri_indices,
                             const Span<float3> bary_coords,
                             const GVArray &data_in,
                             const IndexMask mask,
                             const GMutableSpan data_out)
{
  BLI_assert(data_out.size() == looptri_indices.size());
  BLI_assert(data_out.size() == bary_coords.size());
  BLI_assert(data_in.size() == mesh.totloop);
  BLI_assert(data_in.type() == data_out.type());

  const CPPType &type = data_in.type();
  attribute_math::convert_to_static_type(type, [&](auto dummy) {
    using T = decltype(dummy);
    sample_corner_attribute<T>(
        mesh, looptri_indices, bary_coords, data_in.typed<T>(), mask, data_out.typed<T>());
  });
}

template<typename T>
void sample_face_attribute(const Mesh &mesh,
                           const Span<int> looptri_indices,
                           const VArray<T> &data_in,
                           const IndexMask mask,
                           const MutableSpan<T> data_out)
{
  const Span<MLoopTri> looptris{BKE_mesh_runtime_looptri_ensure(&mesh),
                                BKE_mesh_runtime_looptri_len(&mesh)};

  for (const int i : mask) {
    const int looptri_index = looptri_indices[i];
    const MLoopTri &looptri = looptris[looptri_index];
    const int poly_index = looptri.poly;
    data_out[i] = data_in[poly_index];
  }
}

void sample_face_attribute(const Mesh &mesh,
                           const Span<int> looptri_indices,
                           const GVArray &data_in,
                           const IndexMask mask,
                           const GMutableSpan data_out)
{
  BLI_assert(data_out.size() == looptri_indices.size());
  BLI_assert(data_in.size() == mesh.totpoly);
  BLI_assert(data_in.type() == data_out.type());

  const CPPType &type = data_in.type();
  attribute_math::convert_to_static_type(type, [&](auto dummy) {
    using T = decltype(dummy);
    sample_face_attribute<T>(mesh, looptri_indices, data_in.typed<T>(), mask, data_out.typed<T>());
  });
}

MeshAttributeInterpolator::MeshAttributeInterpolator(const Mesh *mesh,
                                                     const Span<float3> positions,
                                                     const Span<int> looptri_indices)
    : mesh_(mesh), positions_(positions), looptri_indices_(looptri_indices)
{
  BLI_assert(positions.size() == looptri_indices.size());
}

Span<float3> MeshAttributeInterpolator::ensure_barycentric_coords()
{
  if (!bary_coords_.is_empty()) {
    BLI_assert(bary_coords_.size() == positions_.size());
    return bary_coords_;
  }
  bary_coords_.reinitialize(positions_.size());

  const Span<MLoopTri> looptris{BKE_mesh_runtime_looptri_ensure(mesh_),
                                BKE_mesh_runtime_looptri_len(mesh_)};

  for (const int i : bary_coords_.index_range()) {
    const int looptri_index = looptri_indices_[i];
    const MLoopTri &looptri = looptris[looptri_index];

    const int v0_index = mesh_->mloop[looptri.tri[0]].v;
    const int v1_index = mesh_->mloop[looptri.tri[1]].v;
    const int v2_index = mesh_->mloop[looptri.tri[2]].v;

    interp_weights_tri_v3(bary_coords_[i],
                          mesh_->mvert[v0_index].co,
                          mesh_->mvert[v1_index].co,
                          mesh_->mvert[v2_index].co,
                          positions_[i]);
  }
  return bary_coords_;
}

Span<float3> MeshAttributeInterpolator::ensure_nearest_weights()
{
  if (!nearest_weights_.is_empty()) {
    BLI_assert(nearest_weights_.size() == positions_.size());
    return nearest_weights_;
  }
  nearest_weights_.reinitialize(positions_.size());

  const Span<MLoopTri> looptris{BKE_mesh_runtime_looptri_ensure(mesh_),
                                BKE_mesh_runtime_looptri_len(mesh_)};

  for (const int i : nearest_weights_.index_range()) {
    const int looptri_index = looptri_indices_[i];
    const MLoopTri &looptri = looptris[looptri_index];

    const int v0_index = mesh_->mloop[looptri.tri[0]].v;
    const int v1_index = mesh_->mloop[looptri.tri[1]].v;
    const int v2_index = mesh_->mloop[looptri.tri[2]].v;

    const float d0 = len_squared_v3v3(positions_[i], mesh_->mvert[v0_index].co);
    const float d1 = len_squared_v3v3(positions_[i], mesh_->mvert[v1_index].co);
    const float d2 = len_squared_v3v3(positions_[i], mesh_->mvert[v2_index].co);

    nearest_weights_[i] = MIN3_PAIR(d0, d1, d2, float3(1, 0, 0), float3(0, 1, 0), float3(0, 0, 1));
  }
  return nearest_weights_;
}

void MeshAttributeInterpolator::sample_data(const GVArray &src,
                                            const AttributeDomain domain,
                                            const eAttributeMapMode mode,
                                            const IndexMask mask,
                                            const GMutableSpan dst)
{
  if (src.is_empty() || dst.is_empty()) {
    return;
  }

  /* Compute barycentric coordinates only when they are needed. */
  Span<float3> weights;
  if (ELEM(domain, ATTR_DOMAIN_POINT, ATTR_DOMAIN_CORNER)) {
    switch (mode) {
      case eAttributeMapMode::INTERPOLATED:
        weights = ensure_barycentric_coords();
        break;
      case eAttributeMapMode::NEAREST:
        weights = ensure_nearest_weights();
        break;
    }
  }

  /* Interpolate the source attributes on the surface. */
  switch (domain) {
    case ATTR_DOMAIN_POINT: {
      sample_point_attribute(*mesh_, looptri_indices_, weights, src, mask, dst);
      break;
    }
    case ATTR_DOMAIN_FACE: {
      sample_face_attribute(*mesh_, looptri_indices_, src, mask, dst);
      break;
    }
    case ATTR_DOMAIN_CORNER: {
      sample_corner_attribute(*mesh_, looptri_indices_, weights, src, mask, dst);
      break;
    }
    case ATTR_DOMAIN_EDGE: {
      /* Not yet supported. */
      break;
    }
    default: {
      BLI_assert_unreachable();
      break;
    }
  }
}

void MeshAttributeInterpolator::sample_attribute(const ReadAttributeLookup &src_attribute,
                                                 OutputAttribute &dst_attribute,
                                                 eAttributeMapMode mode)
{
  if (src_attribute && dst_attribute) {
    this->sample_data(*src_attribute.varray,
                      src_attribute.domain,
                      mode,
                      IndexMask(dst_attribute->size()),
                      dst_attribute.as_span());
  }
}

}  // namespace blender::bke::mesh_surface_sample
