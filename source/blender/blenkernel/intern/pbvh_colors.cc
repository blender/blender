/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include "MEM_guardedalloc.h"

#include "BLI_math_color.h"
#include "BLI_math_vector.h"
#include "BLI_utildefines.h"

#include "BLI_bitmap.h"
#include "BLI_ghash.h"
#include "BLI_index_range.hh"
#include "BLI_rand.h"
#include "BLI_span.hh"
#include "BLI_task.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BKE_attribute.h"
#include "BKE_ccg.h"
#include "BKE_mesh.hh"
#include "BKE_mesh_mapping.hh"
#include "BKE_paint.hh"
#include "BKE_pbvh_api.hh"
#include "BKE_subdiv_ccg.hh"

#include "PIL_time.h"

#include "bmesh.h"

#include "atomic_ops.h"

#include "pbvh_intern.hh"

#include <climits>

using blender::IndexRange;

namespace blender::bke {

template<typename Func>
inline void to_static_color_type(const eCustomDataType type, const Func &func)
{
  switch (type) {
    case CD_PROP_COLOR:
      func(MPropCol());
      break;
    case CD_PROP_BYTE_COLOR:
      func(MLoopCol());
      break;
    default:
      BLI_assert_unreachable();
      break;
  }
}

template<typename T> void to_float(const T &src, float dst[4]);

template<> void to_float(const MLoopCol &src, float dst[4])
{
  rgba_uchar_to_float(dst, reinterpret_cast<const uchar *>(&src));
  srgb_to_linearrgb_v3_v3(dst, dst);
}
template<> void to_float(const MPropCol &src, float dst[4])
{
  copy_v4_v4(dst, src.color);
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
static void pbvh_vertex_color_get(const PBVH &pbvh, PBVHVertRef vertex, float r_color[4])
{
  int index = vertex.i;

  if (pbvh.color_domain == ATTR_DOMAIN_CORNER) {
    int count = 0;
    zero_v4(r_color);
    for (const int i_face : pbvh.pmap[index]) {
      const IndexRange face = pbvh.faces[i_face];
      Span<T> colors{static_cast<const T *>(pbvh.color_layer->data) + face.start(), face.size()};
      Span<int> face_verts = pbvh.corner_verts.slice(face);

      for (const int i : IndexRange(face.size())) {
        if (face_verts[i] == index) {
          float temp[4];
          to_float(colors[i], temp);

          add_v4_v4(r_color, temp);
          count++;
        }
      }
    }

    if (count) {
      mul_v4_fl(r_color, 1.0f / float(count));
    }
  }
  else {
    to_float(static_cast<T *>(pbvh.color_layer->data)[index], r_color);
  }
}

template<typename T>
static void pbvh_vertex_color_set(PBVH &pbvh, PBVHVertRef vertex, const float color[4])
{
  int index = vertex.i;

  if (pbvh.color_domain == ATTR_DOMAIN_CORNER) {
    for (const int i_face : pbvh.pmap[index]) {
      const IndexRange face = pbvh.faces[i_face];
      MutableSpan<T> colors{static_cast<T *>(pbvh.color_layer->data) + face.start(), face.size()};
      Span<int> face_verts = pbvh.corner_verts.slice(face);

      for (const int i : IndexRange(face.size())) {
        if (face_verts[i] == index) {
          from_float(color, colors[i]);
        }
      }
    }
  }
  else {
    from_float(color, static_cast<T *>(pbvh.color_layer->data)[index]);
  }
}

template<typename T>
static void pbvh_vertex_color_get_bmesh(const PBVH &pbvh, PBVHVertRef vertex, float r_color[4])
{
  BMVert *v = reinterpret_cast<BMVert *>(vertex.i);

  if (pbvh.color_domain == ATTR_DOMAIN_CORNER) {
    float4 color = {};
    int count = 0;

    int cd_color = pbvh.cd_vcol_offset;

    BMEdge *e = v->e;
    do {
      BMLoop *l = e->l;

      if (!l) {
        continue;
      }

      if (l->v != v) {
        l = l->next;
      }

      float4 color2;
      T vcol = *reinterpret_cast<T *> BM_ELEM_CD_GET_VOID_P(l, cd_color);

      to_float(vcol, color2);

      color += color2;
      count++;
    } while ((e = BM_DISK_EDGE_NEXT(e, v)) != v->e);

    if (count > 0) {
      color *= 1.0f / (float)count;
    }

    copy_v4_v4(r_color, color);
  }
  else {
    to_float(*static_cast<T *>(BM_ELEM_CD_GET_VOID_P(v, pbvh.cd_vcol_offset)), r_color);
  }
}

template<typename T>
static void pbvh_vertex_color_set_bmesh(PBVH &pbvh, PBVHVertRef vertex, const float color[4])
{
  int index = vertex.i;

  BMVert *v = reinterpret_cast<BMVert *>(vertex.i);

  if (pbvh.color_domain == ATTR_DOMAIN_CORNER) {
    BMEdge *e = v->e;
    int cd_color = pbvh.cd_vcol_offset;

    do {
      BMLoop *l = e->l;
      do {
        T *l_color;

        if (l->v != v) {
          l_color = reinterpret_cast<T *>(BM_ELEM_CD_GET_VOID_P(l->next, cd_color));
        }
        else {
          l_color = reinterpret_cast<T *>(BM_ELEM_CD_GET_VOID_P(l, cd_color));
        }

        from_float(color, *l_color);
      } while ((l = l->radial_next) != e->l);

    } while ((e = BM_DISK_EDGE_NEXT(e, v)) != v->e);
  }
  else {
    from_float(color, *reinterpret_cast<T *>(BM_ELEM_CD_GET_VOID_P(v, pbvh.cd_vcol_offset)));
  }
}

}  // namespace blender::bke

void BKE_pbvh_vertex_color_get(const PBVH *pbvh, PBVHVertRef vertex, float r_color[4])
{
  blender::bke::to_static_color_type(eCustomDataType(pbvh->color_layer->type), [&](auto dummy) {
    using T = decltype(dummy);

    if (BKE_pbvh_type(pbvh) == PBVH_BMESH) {
      blender::bke::pbvh_vertex_color_get_bmesh<T>(*pbvh, vertex, r_color);
    }
    else {
      blender::bke::pbvh_vertex_color_get<T>(*pbvh, vertex, r_color);
    }
  });
}

void BKE_pbvh_vertex_color_set(PBVH *pbvh, PBVHVertRef vertex, const float color[4])
{
  blender::bke::to_static_color_type(eCustomDataType(pbvh->color_layer->type), [&](auto dummy) {
    using T = decltype(dummy);

    if (BKE_pbvh_type(pbvh) == PBVH_BMESH) {
      blender::bke::pbvh_vertex_color_set_bmesh<T>(*pbvh, vertex, color);
    }
    else {
      blender::bke::pbvh_vertex_color_set<T>(*pbvh, vertex, color);
    }
  });
}

void BKE_pbvh_swap_colors(PBVH *pbvh,
                          const int *indices,
                          const int indices_num,
                          float (*r_colors)[4])
{
  blender::bke::to_static_color_type(eCustomDataType(pbvh->color_layer->type), [&](auto dummy) {
    using T = decltype(dummy);
    T *pbvh_colors = static_cast<T *>(pbvh->color_layer->data);
    for (const int i : IndexRange(indices_num)) {
      T temp = pbvh_colors[indices[i]];
      blender::bke::from_float(r_colors[i], pbvh_colors[indices[i]]);
      blender::bke::to_float(temp, r_colors[i]);
    }
  });
}

void BKE_pbvh_store_colors(PBVH *pbvh,
                           const int *indices,
                           const int indices_num,
                           float (*r_colors)[4])
{
  blender::bke::to_static_color_type(eCustomDataType(pbvh->color_layer->type), [&](auto dummy) {
    using T = decltype(dummy);
    T *pbvh_colors = static_cast<T *>(pbvh->color_layer->data);
    for (const int i : IndexRange(indices_num)) {
      blender::bke::to_float(pbvh_colors[indices[i]], r_colors[i]);
    }
  });
}

void BKE_pbvh_store_colors_vertex(PBVH *pbvh,
                                  const int *indices,
                                  const int indices_num,
                                  float (*r_colors)[4])
{
  if (pbvh->color_domain == ATTR_DOMAIN_POINT) {
    BKE_pbvh_store_colors(pbvh, indices, indices_num, r_colors);
  }
  else {
    blender::bke::to_static_color_type(eCustomDataType(pbvh->color_layer->type), [&](auto dummy) {
      using T = decltype(dummy);
      for (const int i : IndexRange(indices_num)) {
        blender::bke::pbvh_vertex_color_get<T>(*pbvh, BKE_pbvh_make_vref(indices[i]), r_colors[i]);
      }
    });
  }
}
