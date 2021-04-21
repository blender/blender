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

#include "BKE_attribute_math.hh"
#include "BKE_mesh_runtime.h"
#include "BKE_mesh_sample.hh"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

namespace blender::bke::mesh_surface_sample {

static Span<MLoopTri> get_mesh_looptris(const Mesh &mesh)
{
  /* This only updates a cache and can be considered to be logically const. */
  const MLoopTri *looptris = BKE_mesh_runtime_looptri_ensure(const_cast<Mesh *>(&mesh));
  const int looptris_len = BKE_mesh_runtime_looptri_len(&mesh);
  return {looptris, looptris_len};
}

template<typename T>
BLI_NOINLINE static void sample_point_attribute(const Mesh &mesh,
                                                const Span<int> looptri_indices,
                                                const Span<float3> bary_coords,
                                                const VArray<T> &data_in,
                                                const MutableSpan<T> data_out)
{
  const Span<MLoopTri> looptris = get_mesh_looptris(mesh);

  for (const int i : bary_coords.index_range()) {
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
        mesh, looptri_indices, bary_coords, data_in.typed<T>(), data_out.typed<T>());
  });
}

template<typename T>
BLI_NOINLINE static void sample_corner_attribute(const Mesh &mesh,
                                                 const Span<int> looptri_indices,
                                                 const Span<float3> bary_coords,
                                                 const VArray<T> &data_in,
                                                 const MutableSpan<T> data_out)
{
  Span<MLoopTri> looptris = get_mesh_looptris(mesh);

  for (const int i : bary_coords.index_range()) {
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
        mesh, looptri_indices, bary_coords, data_in.typed<T>(), data_out.typed<T>());
  });
}

template<typename T>
void sample_face_attribute(const Mesh &mesh,
                           const Span<int> looptri_indices,
                           const VArray<T> &data_in,
                           const MutableSpan<T> data_out)
{
  Span<MLoopTri> looptris = get_mesh_looptris(mesh);

  for (const int i : data_out.index_range()) {
    const int looptri_index = looptri_indices[i];
    const MLoopTri &looptri = looptris[looptri_index];
    const int poly_index = looptri.poly;
    data_out[i] = data_in[poly_index];
  }
}

void sample_face_attribute(const Mesh &mesh,
                           const Span<int> looptri_indices,
                           const GVArray &data_in,
                           const GMutableSpan data_out)
{
  BLI_assert(data_out.size() == looptri_indices.size());
  BLI_assert(data_in.size() == mesh.totpoly);
  BLI_assert(data_in.type() == data_out.type());

  const CPPType &type = data_in.type();
  attribute_math::convert_to_static_type(type, [&](auto dummy) {
    using T = decltype(dummy);
    sample_face_attribute<T>(mesh, looptri_indices, data_in.typed<T>(), data_out.typed<T>());
  });
}

}  // namespace blender::bke::mesh_surface_sample
