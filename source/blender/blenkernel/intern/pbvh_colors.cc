/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include "MEM_guardedalloc.h"

#include "BLI_bitmap.h"
#include "BLI_color.hh"
#include "BLI_ghash.h"
#include "BLI_index_range.hh"
#include "BLI_math_color.h"
#include "BLI_math_vector.h"
#include "BLI_rand.h"
#include "BLI_span.hh"
#include "BLI_task.h"
#include "BLI_time.h"
#include "BLI_utildefines.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BKE_attribute.hh"
#include "BKE_ccg.hh"
#include "BKE_mesh.hh"
#include "BKE_mesh_mapping.hh"
#include "BKE_paint.hh"
#include "BKE_pbvh_api.hh"
#include "BKE_subdiv_ccg.hh"

#include "bmesh.hh"

#include "atomic_ops.h"

#include "pbvh_intern.hh"

#include <climits>

using blender::IndexRange;

namespace blender::bke {

template<typename Func> inline void to_static_color_type(const CPPType &type, const Func &func)
{
  if (type.is<ColorGeometry4f>()) {
    func(MPropCol());
  }
  else if (type.is<ColorGeometry4b>()) {
    func(MLoopCol());
  }
}

template<typename T> float4 to_float(const T &src);

template<> float4 to_float(const MLoopCol &src)
{
  float4 dst;
  rgba_uchar_to_float(dst, reinterpret_cast<const uchar *>(&src));
  srgb_to_linearrgb_v3_v3(dst, dst);
  return dst;
}
template<> float4 to_float(const MPropCol &src)
{
  return src.color;
}

template<typename T> void from_float(const float src[4], T &dst);

template<> void from_float(const float src[4], MLoopCol &dst)
{
  float temp[4];
  linearrgb_to_srgb_v3_v3(temp, src);
  temp[3] = src[3];
  rgba_float_to_uchar(reinterpret_cast<uchar *>(&dst), temp);
}
template<> void from_float(const float src[4], MPropCol &dst)
{
  copy_v4_v4(dst.color, src);
}

template<typename T>
static float4 pbvh_vertex_color_get(const blender::OffsetIndices<int> faces,
                                    const blender::Span<int> corner_verts,
                                    const blender::GroupedSpan<int> vert_to_face_map,
                                    const blender::GSpan color_attribute,
                                    const blender::bke::AttrDomain color_domain,
                                    const int vert)
{
  const T *colors_typed = static_cast<const T *>(color_attribute.data());
  if (color_domain == AttrDomain::Corner) {
    float4 r_color(0.0f);
    for (const int face : vert_to_face_map[vert]) {
      const int corner = mesh::face_find_corner_from_vert(faces[face], corner_verts, vert);
      r_color += to_float(colors_typed[corner]);
    }
    return r_color / float(vert_to_face_map[vert].size());
  }
  return to_float(colors_typed[vert]);
}

template<typename T>
static void pbvh_vertex_color_set(const blender::OffsetIndices<int> faces,
                                  const blender::Span<int> corner_verts,
                                  const blender::GroupedSpan<int> vert_to_face_map,
                                  const blender::GMutableSpan color_attribute,
                                  const blender::bke::AttrDomain color_domain,
                                  const int vert,
                                  const float color[4])
{
  if (color_domain == AttrDomain::Corner) {
    for (const int i_face : vert_to_face_map[vert]) {
      const IndexRange face = faces[i_face];
      MutableSpan<T> colors{static_cast<T *>(color_attribute.data()) + face.start(), face.size()};
      Span<int> face_verts = corner_verts.slice(face);

      for (const int i : IndexRange(face.size())) {
        if (face_verts[i] == vert) {
          from_float(color, colors[i]);
        }
      }
    }
  }
  else {
    from_float(color, static_cast<T *>(color_attribute.data())[vert]);
  }
}

}  // namespace blender::bke

blender::float4 BKE_pbvh_vertex_color_get(const blender::OffsetIndices<int> faces,
                                          const blender::Span<int> corner_verts,
                                          const blender::GroupedSpan<int> vert_to_face_map,
                                          const blender::GSpan color_attribute,
                                          const blender::bke::AttrDomain color_domain,
                                          const int vert)
{
  blender::float4 color;
  blender::bke::to_static_color_type(color_attribute.type(), [&](auto dummy) {
    using T = decltype(dummy);
    color = blender::bke::pbvh_vertex_color_get<T>(
        faces, corner_verts, vert_to_face_map, color_attribute, color_domain, vert);
  });
  return color;
}

void BKE_pbvh_vertex_color_set(const blender::OffsetIndices<int> faces,
                               const blender::Span<int> corner_verts,
                               const blender::GroupedSpan<int> vert_to_face_map,
                               const blender::bke::AttrDomain color_domain,
                               const int vert,
                               const blender::float4 &color,
                               const blender::GMutableSpan color_attribute)
{
  blender::bke::to_static_color_type(color_attribute.type(), [&](auto dummy) {
    using T = decltype(dummy);
    blender::bke::pbvh_vertex_color_set<T>(
        faces, corner_verts, vert_to_face_map, color_attribute, color_domain, vert, color);
  });
}

void BKE_pbvh_swap_colors(const blender::Span<int> indices,
                          blender::GMutableSpan color_attribute,
                          blender::MutableSpan<blender::float4> r_colors)
{
  blender::bke::to_static_color_type(color_attribute.type(), [&](auto dummy) {
    using T = decltype(dummy);
    T *colors_typed = static_cast<T *>(color_attribute.data());
    for (const int i : indices.index_range()) {
      T temp = colors_typed[indices[i]];
      blender::bke::from_float(r_colors[i], colors_typed[indices[i]]);
      r_colors[i] = blender::bke::to_float(temp);
    }
  });
}

void BKE_pbvh_store_colors(const blender::GSpan color_attribute,
                           const blender::Span<int> indices,
                           blender::MutableSpan<blender::float4> r_colors)
{
  blender::bke::to_static_color_type(color_attribute.type(), [&](auto dummy) {
    using T = decltype(dummy);
    const T *colors_typed = static_cast<const T *>(color_attribute.data());
    for (const int i : indices.index_range()) {
      r_colors[i] = blender::bke::to_float(colors_typed[indices[i]]);
    }
  });
}

void BKE_pbvh_store_colors_vertex(const blender::OffsetIndices<int> faces,
                                  const blender::Span<int> corner_verts,
                                  const blender::GroupedSpan<int> vert_to_face_map,
                                  const blender::GSpan color_attribute,
                                  const blender::bke::AttrDomain color_domain,
                                  const blender::Span<int> verts,
                                  const blender::MutableSpan<blender::float4> r_colors)
{
  if (color_domain == blender::bke::AttrDomain::Point) {
    BKE_pbvh_store_colors(color_attribute, verts, r_colors);
  }
  else {
    blender::bke::to_static_color_type(color_attribute.type(), [&](auto dummy) {
      using T = decltype(dummy);
      for (const int i : verts.index_range()) {
        r_colors[i] = blender::bke::pbvh_vertex_color_get<T>(
            faces, corner_verts, vert_to_face_map, color_attribute, color_domain, verts[i]);
      }
    });
  }
}
