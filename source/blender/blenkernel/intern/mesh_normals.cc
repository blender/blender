/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 *
 * Mesh normal calculation functions.
 *
 * \see `bmesh_mesh_normals.cc` for the equivalent #BMesh functionality.
 */

#include <climits>

#include "BLI_math_geom.h"
#include "BLI_math_vector.h"

#include "BLI_array_utils.hh"
#include "BLI_bit_vector.hh"
#include "BLI_enumerable_thread_specific.hh"
#include "BLI_linklist.h"
#include "BLI_math_base.hh"
#include "BLI_math_vector.hh"
#include "BLI_memarena.h"
#include "BLI_span.hh"
#include "BLI_task.hh"
#include "BLI_utildefines.h"

#include "BKE_attribute.hh"
#include "BKE_global.hh"
#include "BKE_mesh.hh"
#include "BKE_mesh_mapping.hh"

// #define DEBUG_TIME

#ifdef DEBUG_TIME
#  include "BLI_timeit.hh"
#endif

/* -------------------------------------------------------------------- */
/** \name Public Utility Functions
 *
 * Related to managing normals but not directly related to calculating normals.
 * \{ */

namespace blender::bke {

void mesh_vert_normals_assign(Mesh &mesh, Span<float3> vert_normals)
{
  mesh.runtime->vert_normals_true_cache.ensure(
      [&](Vector<float3> &r_data) { r_data = vert_normals; });
}

void mesh_vert_normals_assign(Mesh &mesh, Vector<float3> vert_normals)
{
  mesh.runtime->vert_normals_true_cache.ensure(
      [&](Vector<float3> &r_data) { r_data = std::move(vert_normals); });
}

MutableSpan<float3> NormalsCache::ensure_vector_size(const int size)
{
  if (auto *vector = std::get_if<Vector<float3>>(&this->data)) {
    vector->resize(size);
  }
  else {
    this->data = Vector<float3>(size);
  }
  return std::get<Vector<float3>>(this->data).as_mutable_span();
}

Span<float3> NormalsCache::get_span() const
{
  if (const auto *vector = std::get_if<Vector<float3>>(&this->data)) {
    return vector->as_span();
  }
  return std::get<Span<float3>>(this->data);
}

void NormalsCache::store_varray(const VArray<float3> &data)
{
  if (data.is_span()) {
    this->data = data.get_internal_span();
  }
  else {
    data.materialize(this->ensure_vector_size(data.size()));
  }
}

void NormalsCache::store_vector(Vector<float3> &&data)
{
  this->data = std::move(data);
}

}  // namespace blender::bke

bool BKE_mesh_vert_normals_are_dirty(const Mesh *mesh)
{
  return mesh->runtime->vert_normals_cache.is_dirty();
}

bool BKE_mesh_face_normals_are_dirty(const Mesh *mesh)
{
  return mesh->runtime->face_normals_cache.is_dirty();
}

/** \} */

namespace blender::bke::mesh {

/* -------------------------------------------------------------------- */
/** \name Mesh Normal Calculation (Polygons)
 * \{ */

/*
 * COMPUTE POLY NORMAL
 *
 * Computes the normal of a planar
 * face See Graphics Gems for
 * computing newell normal.
 */
static float3 normal_calc_ngon(const Span<float3> vert_positions, const Span<int> face_verts)
{
  float3 normal(0);

  /* Newell's Method */
  const float *v_prev = vert_positions[face_verts.last()];
  for (const int i : face_verts.index_range()) {
    const float *v_curr = vert_positions[face_verts[i]];
    add_newell_cross_v3_v3v3(normal, v_prev, v_curr);
    v_prev = v_curr;
  }

  if (UNLIKELY(normalize_v3(normal) == 0.0f)) {
    /* Other axis are already set to zero. */
    normal[2] = 1.0f;
  }

  return normal;
}

float3 face_normal_calc(const Span<float3> vert_positions, const Span<int> face_verts)
{
  float3 normal;
  if (face_verts.size() == 4) {
    normal_quad_v3(normal,
                   vert_positions[face_verts[0]],
                   vert_positions[face_verts[1]],
                   vert_positions[face_verts[2]],
                   vert_positions[face_verts[3]]);
  }
  else if (face_verts.size() == 3) {
    normal = math::normal_tri(vert_positions[face_verts[0]],
                              vert_positions[face_verts[1]],
                              vert_positions[face_verts[2]]);
  }
  else {
    BLI_assert(face_verts.size() > 4);
    normal = normal_calc_ngon(vert_positions, face_verts);
  }

  if (UNLIKELY(math::is_zero(normal))) {
    normal.z = 1.0f;
  }

  BLI_ASSERT_UNIT_V3(normal);
  return normal;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Mesh Normal Calculation (Polygons & Vertices)
 *
 * Take care making optimizations to this function as improvements to low-poly
 * meshes can slow down high-poly meshes. For details on performance, see D11993.
 * \{ */

void normals_calc_faces(const Span<float3> positions,
                        const OffsetIndices<int> faces,
                        const Span<int> corner_verts,
                        MutableSpan<float3> face_normals)
{
  BLI_assert(faces.size() == face_normals.size());
  threading::parallel_for(faces.index_range(), 1024, [&](const IndexRange range) {
    for (const int i : range) {
      face_normals[i] = normal_calc_ngon(positions, corner_verts.slice(faces[i]));
    }
  });
}

void normals_calc_verts(const Span<float3> vert_positions,
                        const OffsetIndices<int> faces,
                        const Span<int> corner_verts,
                        const GroupedSpan<int> vert_to_face_map,
                        const Span<float3> face_normals,
                        MutableSpan<float3> vert_normals)
{
  const Span<float3> positions = vert_positions;
  threading::parallel_for(positions.index_range(), 1024, [&](const IndexRange range) {
    for (const int vert : range) {
      const Span<int> vert_faces = vert_to_face_map[vert];
      if (vert_faces.is_empty()) {
        vert_normals[vert] = math::normalize(positions[vert]);
        continue;
      }

      float3 vert_normal(0);
      for (const int face : vert_faces) {
        const int2 adjacent_verts = face_find_adjacent_verts(faces[face], corner_verts, vert);
        const float3 dir_prev = math::normalize(positions[adjacent_verts[0]] - positions[vert]);
        const float3 dir_next = math::normalize(positions[adjacent_verts[1]] - positions[vert]);
        const float factor = math::safe_acos_approx(math::dot(dir_prev, dir_next));

        vert_normal += face_normals[face] * factor;
      }

      vert_normals[vert] = math::normalize(vert_normal);
    }
  });
}

/** \} */

static void mix_normals_corner_to_vert(const Span<float3> vert_positions,
                                       const OffsetIndices<int> faces,
                                       const Span<int> corner_verts,
                                       const GroupedSpan<int> vert_to_face_map,
                                       const Span<float3> corner_normals,
                                       MutableSpan<float3> vert_normals)
{
  const Span<float3> positions = vert_positions;
  threading::parallel_for(positions.index_range(), 1024, [&](const IndexRange range) {
    for (const int vert : range) {
      const Span<int> vert_faces = vert_to_face_map[vert];
      if (vert_faces.is_empty()) {
        vert_normals[vert] = math::normalize(positions[vert]);
        continue;
      }

      float3 vert_normal(0);
      for (const int face : vert_faces) {
        const int corner = mesh::face_find_corner_from_vert(faces[face], corner_verts, vert);
        const int2 adjacent_verts{corner_verts[mesh::face_corner_prev(faces[face], corner)],
                                  corner_verts[mesh::face_corner_next(faces[face], corner)]};

        const float3 dir_prev = math::normalize(positions[adjacent_verts[0]] - positions[vert]);
        const float3 dir_next = math::normalize(positions[adjacent_verts[1]] - positions[vert]);
        const float factor = math::safe_acos_approx(math::dot(dir_prev, dir_next));

        vert_normal += corner_normals[corner] * factor;
      }

      vert_normals[vert] = math::normalize(vert_normal);
    }
  });
}

static void mix_normals_vert_to_face(const OffsetIndices<int> faces,
                                     const Span<int> corner_verts,
                                     const Span<float3> vert_normals,
                                     MutableSpan<float3> face_normals)
{
  threading::parallel_for(faces.index_range(), 1024, [&](const IndexRange range) {
    for (const int face : range) {
      float3 sum(0);
      for (const int vert : corner_verts.slice(faces[face])) {
        sum += vert_normals[vert];
      }
      face_normals[face] = math::normalize(sum);
    }
  });
}

static void mix_normals_corner_to_face(const OffsetIndices<int> faces,
                                       const Span<float3> corner_normals,
                                       MutableSpan<float3> face_normals)
{
  threading::parallel_for(faces.index_range(), 1024, [&](const IndexRange range) {
    for (const int face : range) {
      const Span<float3> face_corner_normals = corner_normals.slice(faces[face]);
      const float3 sum = std::accumulate(
          face_corner_normals.begin(), face_corner_normals.end(), float3(0));
      face_normals[face] = math::normalize(sum);
    }
  });
}

}  // namespace blender::bke::mesh

/* -------------------------------------------------------------------- */
/** \name Mesh Normal Calculation
 * \{ */

blender::bke::MeshNormalDomain Mesh::normals_domain(const bool support_sharp_face) const
{
  using namespace blender;
  using namespace blender::bke;
  if (this->faces_num == 0) {
    return MeshNormalDomain::Point;
  }

  const bke::AttributeAccessor attributes = this->attributes();
  if (const std::optional<AttributeMetaData> custom = attributes.lookup_meta_data("custom_normal"))
  {
    switch (custom->domain) {
      case AttrDomain::Point:
        return MeshNormalDomain::Point;
      case AttrDomain::Edge:
        break;
      case AttrDomain::Face:
        return MeshNormalDomain::Face;
      case AttrDomain::Corner:
        return MeshNormalDomain::Corner;
      default:
        BLI_assert_unreachable();
    }
  }

  const VArray<bool> sharp_faces = *attributes.lookup_or_default<bool>(
      "sharp_face", AttrDomain::Face, false);

  const array_utils::BooleanMix face_mix = array_utils::booleans_mix_calc(sharp_faces);
  if (face_mix == array_utils::BooleanMix::AllTrue) {
    return MeshNormalDomain::Face;
  }

  const VArray<bool> sharp_edges = *attributes.lookup_or_default<bool>(
      "sharp_edge", AttrDomain::Edge, false);
  const array_utils::BooleanMix edge_mix = array_utils::booleans_mix_calc(sharp_edges);
  if (edge_mix == array_utils::BooleanMix::AllTrue) {
    return MeshNormalDomain::Face;
  }

  if (edge_mix == array_utils::BooleanMix::AllFalse &&
      (face_mix == array_utils::BooleanMix::AllFalse || support_sharp_face))
  {
    return MeshNormalDomain::Point;
  }

  return MeshNormalDomain::Corner;
}

blender::Span<blender::float3> Mesh::vert_normals() const
{
  using namespace blender;
  using namespace blender::bke;
  this->runtime->vert_normals_cache.ensure([&](NormalsCache &r_data) {
    if (const GAttributeReader custom = this->attributes().lookup("custom_normal")) {
      if (custom.varray.type().is<float3>()) {
        if (custom.domain == AttrDomain::Point) {
          r_data.store_varray(custom.varray.typed<float3>());
          return;
        }
        if (custom.domain == AttrDomain::Face) {
          mesh::normals_calc_verts(this->vert_positions(),
                                   this->faces(),
                                   this->corner_verts(),
                                   this->vert_to_face_map(),
                                   VArraySpan<float3>(custom.varray.typed<float3>()),
                                   r_data.ensure_vector_size(this->verts_num));

          return;
        }
        if (custom.domain == AttrDomain::Corner) {
          mesh::mix_normals_corner_to_vert(this->vert_positions(),
                                           this->faces(),
                                           this->corner_verts(),
                                           this->vert_to_face_map(),
                                           VArraySpan<float3>(custom.varray.typed<float3>()),
                                           r_data.ensure_vector_size(this->verts_num));
          return;
        }
      }
      else if (custom.varray.type().is<short2>() && custom.domain == AttrDomain::Corner) {
        mesh::mix_normals_corner_to_vert(this->vert_positions(),
                                         this->faces(),
                                         this->corner_verts(),
                                         this->vert_to_face_map(),
                                         this->corner_normals(),
                                         r_data.ensure_vector_size(this->verts_num));
        return;
      }
    }
    r_data.data = NormalsCache::UseTrueCache();
  });
  if (std::holds_alternative<NormalsCache::UseTrueCache>(
          this->runtime->vert_normals_cache.data().data))
  {
    return this->vert_normals_true();
  }

  return this->runtime->vert_normals_cache.data().get_span();
}

blender::Span<blender::float3> Mesh::vert_normals_true() const
{
  using namespace blender;
  using namespace blender::bke;
  this->runtime->vert_normals_true_cache.ensure([&](Vector<float3> &r_data) {
    r_data.reinitialize(this->verts_num);
    mesh::normals_calc_verts(this->vert_positions(),
                             this->faces(),
                             this->corner_verts(),
                             this->vert_to_face_map(),
                             this->face_normals_true(),
                             r_data);
  });
  return this->runtime->vert_normals_true_cache.data();
}

blender::Span<blender::float3> Mesh::face_normals() const
{
  using namespace blender;
  using namespace blender::bke;
  if (this->faces_num == 0) {
    return {};
  }
  this->runtime->face_normals_cache.ensure([&](NormalsCache &r_data) {
    if (const GAttributeReader custom = this->attributes().lookup("custom_normal")) {
      if (custom.varray.type().is<float3>()) {
        if (custom.domain == AttrDomain::Face) {
          r_data.store_varray(custom.varray.typed<float3>());
          return;
        }
        if (custom.domain == AttrDomain::Point) {
          mesh::mix_normals_vert_to_face(this->faces(),
                                         this->corner_verts(),
                                         VArraySpan<float3>(custom.varray.typed<float3>()),
                                         r_data.ensure_vector_size(this->faces_num));
          return;
        }
        if (custom.domain == AttrDomain::Corner) {
          mesh::mix_normals_corner_to_face(this->faces(),
                                           VArraySpan<float3>(custom.varray.typed<float3>()),
                                           r_data.ensure_vector_size(this->faces_num));
          return;
        }
      }
      else if (custom.varray.type().is<short2>() && custom.domain == AttrDomain::Corner) {
        mesh::mix_normals_corner_to_face(
            this->faces(), this->corner_normals(), r_data.ensure_vector_size(this->faces_num));
        return;
      }
    }
    r_data.data = NormalsCache::UseTrueCache();
  });
  if (std::holds_alternative<NormalsCache::UseTrueCache>(
          this->runtime->face_normals_cache.data().data))
  {
    return this->face_normals_true();
  }
  return this->runtime->face_normals_cache.data().get_span();
}

blender::Span<blender::float3> Mesh::face_normals_true() const
{
  using namespace blender;
  using namespace blender::bke;
  this->runtime->face_normals_true_cache.ensure([&](Vector<float3> &r_data) {
    r_data.reinitialize(this->faces_num);
    mesh::normals_calc_faces(this->vert_positions(), this->faces(), this->corner_verts(), r_data);
  });
  return this->runtime->face_normals_true_cache.data();
}

blender::Span<blender::float3> Mesh::corner_normals() const
{
  using namespace blender;
  using namespace blender::bke;
  if (this->faces_num == 0) {
    return {};
  }
  this->runtime->corner_normals_cache.ensure([&](NormalsCache &r_data) {
    const OffsetIndices<int> faces = this->faces();
    switch (this->normals_domain()) {
      case MeshNormalDomain::Point: {
        MutableSpan<float3> data = r_data.ensure_vector_size(this->corners_num);
        array_utils::gather(this->vert_normals(), this->corner_verts(), data);
        break;
      }
      case MeshNormalDomain::Face: {
        MutableSpan<float3> data = r_data.ensure_vector_size(this->corners_num);
        const Span<float3> face_normals = this->face_normals();
        array_utils::gather_to_groups(faces, faces.index_range(), face_normals, data);
        break;
      }
      case MeshNormalDomain::Corner: {
        const AttributeAccessor attributes = this->attributes();
        const GAttributeReader custom = attributes.lookup("custom_normal");
        if (custom && custom.varray.type().is<float3>()) {
          if (custom.domain == bke::AttrDomain::Corner) {
            r_data.store_varray(custom.varray.typed<float3>());
          }
          return;
        }
        MutableSpan<float3> data = r_data.ensure_vector_size(this->corners_num);
        const VArraySpan sharp_edges = *attributes.lookup<bool>("sharp_edge", AttrDomain::Edge);
        const VArraySpan sharp_faces = *attributes.lookup<bool>("sharp_face", AttrDomain::Face);
        mesh::normals_calc_corners(this->vert_positions(),
                                   this->faces(),
                                   this->corner_verts(),
                                   this->corner_edges(),
                                   this->vert_to_face_map(),
                                   this->face_normals_true(),
                                   sharp_edges,
                                   sharp_faces,
                                   VArraySpan<short2>(custom.varray.typed<short2>()),
                                   nullptr,
                                   data);
      }
    }
  });
  return this->runtime->corner_normals_cache.data().get_span();
}

void BKE_lnor_spacearr_init(MLoopNorSpaceArray *lnors_spacearr,
                            const int numLoops,
                            const char data_type)
{
  if (!(lnors_spacearr->lspacearr && lnors_spacearr->loops_pool)) {
    MemArena *mem;

    if (!lnors_spacearr->mem) {
      lnors_spacearr->mem = BLI_memarena_new(BLI_MEMARENA_STD_BUFSIZE, __func__);
    }
    mem = lnors_spacearr->mem;
    if (numLoops > 0) {
      lnors_spacearr->lspacearr = (MLoopNorSpace **)BLI_memarena_calloc(
          mem, sizeof(MLoopNorSpace *) * size_t(numLoops));
      lnors_spacearr->loops_pool = (LinkNode *)BLI_memarena_alloc(
          mem, sizeof(LinkNode) * size_t(numLoops));
    }
    else {
      lnors_spacearr->lspacearr = nullptr;
      lnors_spacearr->loops_pool = nullptr;
    }

    lnors_spacearr->spaces_num = 0;
  }
  BLI_assert(ELEM(data_type, MLNOR_SPACEARR_BMLOOP_PTR, MLNOR_SPACEARR_LOOP_INDEX));
  lnors_spacearr->data_type = data_type;
}

void BKE_lnor_spacearr_tls_init(MLoopNorSpaceArray *lnors_spacearr,
                                MLoopNorSpaceArray *lnors_spacearr_tls)
{
  *lnors_spacearr_tls = *lnors_spacearr;
  lnors_spacearr_tls->mem = BLI_memarena_new(BLI_MEMARENA_STD_BUFSIZE, __func__);
}

void BKE_lnor_spacearr_tls_join(MLoopNorSpaceArray *lnors_spacearr,
                                MLoopNorSpaceArray *lnors_spacearr_tls)
{
  BLI_assert(lnors_spacearr->data_type == lnors_spacearr_tls->data_type);
  BLI_assert(lnors_spacearr->mem != lnors_spacearr_tls->mem);
  lnors_spacearr->spaces_num += lnors_spacearr_tls->spaces_num;
  BLI_memarena_merge(lnors_spacearr->mem, lnors_spacearr_tls->mem);
  BLI_memarena_free(lnors_spacearr_tls->mem);
  lnors_spacearr_tls->mem = nullptr;
  BKE_lnor_spacearr_clear(lnors_spacearr_tls);
}

void BKE_lnor_spacearr_clear(MLoopNorSpaceArray *lnors_spacearr)
{
  lnors_spacearr->spaces_num = 0;
  lnors_spacearr->lspacearr = nullptr;
  lnors_spacearr->loops_pool = nullptr;
  if (lnors_spacearr->mem != nullptr) {
    BLI_memarena_clear(lnors_spacearr->mem);
  }
}

void BKE_lnor_spacearr_free(MLoopNorSpaceArray *lnors_spacearr)
{
  lnors_spacearr->spaces_num = 0;
  lnors_spacearr->lspacearr = nullptr;
  lnors_spacearr->loops_pool = nullptr;
  BLI_memarena_free(lnors_spacearr->mem);
  lnors_spacearr->mem = nullptr;
}

MLoopNorSpace *BKE_lnor_space_create(MLoopNorSpaceArray *lnors_spacearr)
{
  lnors_spacearr->spaces_num++;
  return (MLoopNorSpace *)BLI_memarena_calloc(lnors_spacearr->mem, sizeof(MLoopNorSpace));
}

/* This threshold is a bit touchy (usual float precision issue), this value seems OK. */
#define LNOR_SPACE_TRIGO_THRESHOLD (1.0f - 1e-4f)

namespace blender::bke::mesh {

static CornerNormalSpace corner_fan_space_define(const float3 &lnor,
                                                 const float3 &vec_ref,
                                                 const float3 &vec_other,
                                                 const Span<float3> edge_vectors)
{
  CornerNormalSpace lnor_space{};
  const float pi2 = float(M_PI) * 2.0f;
  const float dtp_ref = math::dot(vec_ref, lnor);
  const float dtp_other = math::dot(vec_other, lnor);

  if (UNLIKELY(std::abs(dtp_ref) >= LNOR_SPACE_TRIGO_THRESHOLD ||
               std::abs(dtp_other) >= LNOR_SPACE_TRIGO_THRESHOLD))
  {
    /* If vec_ref or vec_other are too much aligned with lnor, we can't build lnor space,
     * tag it as invalid and abort. */
    lnor_space.ref_alpha = lnor_space.ref_beta = 0.0f;
    return lnor_space;
  }

  lnor_space.vec_lnor = lnor;

  /* Compute ref alpha, average angle of all available edge vectors to lnor. */
  if (!edge_vectors.is_empty()) {
    float alpha = 0.0f;
    for (const float3 &vec : edge_vectors) {
      alpha += math::safe_acos_approx(math::dot(vec, lnor));
    }
    /* This piece of code shall only be called for more than one loop. */
    /* NOTE: In theory, this could be `count > 2`,
     * but there is one case where we only have two edges for two loops:
     * a smooth vertex with only two edges and two faces (our Monkey's nose has that, e.g.).
     */
    BLI_assert(edge_vectors.size() >= 2);
    lnor_space.ref_alpha = alpha / float(edge_vectors.size());
  }
  else {
    lnor_space.ref_alpha = (math::safe_acos_approx(math::dot(vec_ref, lnor)) +
                            math::safe_acos_approx(math::dot(vec_other, lnor))) /
                           2.0f;
  }

  /* Project vec_ref on lnor's ortho plane. */
  lnor_space.vec_ref = math::normalize(vec_ref - lnor * dtp_ref);
  lnor_space.vec_ortho = math::normalize(math::cross(lnor, lnor_space.vec_ref));

  /* Project vec_other on lnor's ortho plane. */
  const float3 vec_other_proj = math::normalize(vec_other - lnor * dtp_other);

  /* Beta is angle between ref_vec and other_vec, around lnor. */
  const float dtp = math::dot(lnor_space.vec_ref, vec_other_proj);
  if (LIKELY(dtp < LNOR_SPACE_TRIGO_THRESHOLD)) {
    const float beta = math::safe_acos_approx(dtp);
    lnor_space.ref_beta = (math::dot(lnor_space.vec_ortho, vec_other_proj) < 0.0f) ? pi2 - beta :
                                                                                     beta;
  }
  else {
    lnor_space.ref_beta = pi2;
  }

  return lnor_space;
}

}  // namespace blender::bke::mesh

void BKE_lnor_space_define(MLoopNorSpace *lnor_space,
                           const float lnor[3],
                           const float vec_ref[3],
                           const float vec_other[3],
                           const blender::Span<blender::float3> edge_vectors)
{
  using namespace blender::bke::mesh;
  const CornerNormalSpace space = corner_fan_space_define(lnor, vec_ref, vec_other, edge_vectors);
  copy_v3_v3(lnor_space->vec_lnor, space.vec_lnor);
  copy_v3_v3(lnor_space->vec_ref, space.vec_ref);
  copy_v3_v3(lnor_space->vec_ortho, space.vec_ortho);
  lnor_space->ref_alpha = space.ref_alpha;
  lnor_space->ref_beta = space.ref_beta;
}

void BKE_lnor_space_add_loop(MLoopNorSpaceArray *lnors_spacearr,
                             MLoopNorSpace *lnor_space,
                             const int corner,
                             void *bm_loop,
                             const bool is_single)
{
  BLI_assert((lnors_spacearr->data_type == MLNOR_SPACEARR_LOOP_INDEX && bm_loop == nullptr) ||
             (lnors_spacearr->data_type == MLNOR_SPACEARR_BMLOOP_PTR && bm_loop != nullptr));

  lnors_spacearr->lspacearr[corner] = lnor_space;
  if (bm_loop == nullptr) {
    bm_loop = POINTER_FROM_INT(corner);
  }
  if (is_single) {
    BLI_assert(lnor_space->loops == nullptr);
    lnor_space->flags |= MLNOR_SPACE_IS_SINGLE;
    lnor_space->loops = (LinkNode *)bm_loop;
  }
  else {
    BLI_assert((lnor_space->flags & MLNOR_SPACE_IS_SINGLE) == 0);
    BLI_linklist_prepend_nlink(&lnor_space->loops, bm_loop, &lnors_spacearr->loops_pool[corner]);
  }
}

MINLINE float unit_short_to_float(const short val)
{
  return float(val) / float(SHRT_MAX);
}

MINLINE short unit_float_to_short(const float val)
{
  /* Rounding. */
  return short(floorf(val * float(SHRT_MAX) + 0.5f));
}

namespace blender::bke::mesh {

static float3 corner_space_custom_data_to_normal(const CornerNormalSpace &lnor_space,
                                                 const short2 clnor_data)
{
  /* NOP custom normal data or invalid lnor space, return. */
  if (clnor_data[0] == 0 || lnor_space.ref_alpha == 0.0f || lnor_space.ref_beta == 0.0f) {
    return lnor_space.vec_lnor;
  }

  float3 custom_lnor;

  /* TODO: Check whether using #sincosf() gives any noticeable benefit
   * (could not even get it working under linux though)! */
  const float pi2 = float(M_PI * 2.0);
  const float alphafac = unit_short_to_float(clnor_data[0]);
  const float alpha = (alphafac > 0.0f ? lnor_space.ref_alpha : pi2 - lnor_space.ref_alpha) *
                      alphafac;
  const float betafac = unit_short_to_float(clnor_data[1]);

  mul_v3_v3fl(custom_lnor, lnor_space.vec_lnor, cosf(alpha));

  if (betafac == 0.0f) {
    madd_v3_v3fl(custom_lnor, lnor_space.vec_ref, sinf(alpha));
  }
  else {
    const float sinalpha = sinf(alpha);
    const float beta = (betafac > 0.0f ? lnor_space.ref_beta : pi2 - lnor_space.ref_beta) *
                       betafac;
    madd_v3_v3fl(custom_lnor, lnor_space.vec_ref, sinalpha * cosf(beta));
    madd_v3_v3fl(custom_lnor, lnor_space.vec_ortho, sinalpha * sinf(beta));
  }

  return custom_lnor;
}

}  // namespace blender::bke::mesh

void BKE_lnor_space_custom_data_to_normal(const MLoopNorSpace *lnor_space,
                                          const short clnor_data[2],
                                          float r_custom_lnor[3])
{
  using namespace blender::bke::mesh;
  CornerNormalSpace space;
  space.vec_lnor = lnor_space->vec_lnor;
  space.vec_ref = lnor_space->vec_ref;
  space.vec_ortho = lnor_space->vec_ortho;
  space.ref_alpha = lnor_space->ref_alpha;
  space.ref_beta = lnor_space->ref_beta;
  copy_v3_v3(r_custom_lnor, corner_space_custom_data_to_normal(space, clnor_data));
}

namespace blender::bke::mesh {

short2 corner_space_custom_normal_to_data(const CornerNormalSpace &lnor_space,
                                          const float3 &custom_lnor)
{
  /* We use zero vector as NOP custom normal (can be simpler than giving auto-computed `lnor`). */
  if (is_zero_v3(custom_lnor) || compare_v3v3(lnor_space.vec_lnor, custom_lnor, 1e-4f)) {
    return short2(0);
  }

  short2 clnor_data;

  const float pi2 = float(M_PI * 2.0);
  const float cos_alpha = math::dot(lnor_space.vec_lnor, custom_lnor);

  const float alpha = math::safe_acos_approx(cos_alpha);
  if (alpha > lnor_space.ref_alpha) {
    /* Note we could stick to [0, pi] range here,
     * but makes decoding more complex, not worth it. */
    clnor_data[0] = unit_float_to_short(-(pi2 - alpha) / (pi2 - lnor_space.ref_alpha));
  }
  else {
    clnor_data[0] = unit_float_to_short(alpha / lnor_space.ref_alpha);
  }

  /* Project custom lnor on (vec_ref, vec_ortho) plane. */
  const float3 vec = math::normalize(lnor_space.vec_lnor * -cos_alpha + custom_lnor);

  const float cos_beta = math::dot(lnor_space.vec_ref, vec);

  if (cos_beta < LNOR_SPACE_TRIGO_THRESHOLD) {
    float beta = math::safe_acos_approx(cos_beta);
    if (math::dot(lnor_space.vec_ortho, vec) < 0.0f) {
      beta = pi2 - beta;
    }

    if (beta > lnor_space.ref_beta) {
      clnor_data[1] = unit_float_to_short(-(pi2 - beta) / (pi2 - lnor_space.ref_beta));
    }
    else {
      clnor_data[1] = unit_float_to_short(beta / lnor_space.ref_beta);
    }
  }
  else {
    clnor_data[1] = 0;
  }

  return clnor_data;
}

}  // namespace blender::bke::mesh

void BKE_lnor_space_custom_normal_to_data(const MLoopNorSpace *lnor_space,
                                          const float custom_lnor[3],
                                          short r_clnor_data[2])
{
  using namespace blender::bke::mesh;
  CornerNormalSpace space;
  space.vec_lnor = lnor_space->vec_lnor;
  space.vec_ref = lnor_space->vec_ref;
  space.vec_ortho = lnor_space->vec_ortho;
  space.ref_alpha = lnor_space->ref_alpha;
  space.ref_beta = lnor_space->ref_beta;
  copy_v2_v2_short(r_clnor_data, corner_space_custom_normal_to_data(space, custom_lnor));
}

namespace blender::bke {

namespace mesh {

#define INDEX_UNSET INT_MIN
#define INDEX_INVALID -1
/* See comment about edge_to_corners below. */
#define IS_EDGE_SHARP(_e2l) ELEM((_e2l)[1], INDEX_UNSET, INDEX_INVALID)

static void mesh_edges_sharp_tag(const OffsetIndices<int> faces,
                                 const Span<int> corner_verts,
                                 const Span<int> corner_edges,
                                 const Span<int> corner_to_face_map,
                                 const Span<float3> face_normals,
                                 const Span<bool> sharp_faces,
                                 const Span<bool> sharp_edges,
                                 const float split_angle,
                                 MutableSpan<int2> edge_to_corners,
                                 MutableSpan<bool> r_sharp_edges)
{
  const float split_angle_cos = cosf(split_angle);
  auto face_is_smooth = [&](const int face_i) {
    return sharp_faces.is_empty() || !sharp_faces[face_i];
  };

  for (const int face_i : faces.index_range()) {
    for (const int corner : faces[face_i]) {
      const int vert = corner_verts[corner];
      const int edge = corner_edges[corner];

      int2 &e2l = edge_to_corners[edge];

      /* Check whether current edge might be smooth or sharp */
      if ((e2l[0] | e2l[1]) == 0) {
        /* 'Empty' edge until now, set e2l[0] (and e2l[1] to INDEX_UNSET to tag it as unset). */
        e2l[0] = corner;
        /* We have to check this here too, else we might miss some flat faces!!! */
        e2l[1] = face_is_smooth(face_i) ? INDEX_UNSET : INDEX_INVALID;
      }
      else if (e2l[1] == INDEX_UNSET) {
        const bool is_angle_sharp = math::dot(face_normals[corner_to_face_map[e2l[0]]],
                                              face_normals[face_i]) < split_angle_cos;

        /* Second corner using this edge, time to test its sharpness.
         * An edge is sharp if it is tagged as such, or its face is not smooth,
         * or both faces have opposed (flipped) normals, i.e. both corners on the same edge share
         * the same vertex, or angle between both its faces' normals is above split_angle value. */
        if (!face_is_smooth(face_i) || (!sharp_edges.is_empty() && sharp_edges[edge]) ||
            vert == corner_verts[e2l[0]] || is_angle_sharp)
        {
          /* NOTE: we are sure that corner != 0 here ;). */
          e2l[1] = INDEX_INVALID;

          /* We want to avoid tagging edges as sharp when it is already defined as such by
           * other causes than angle threshold. */
          if (is_angle_sharp) {
            r_sharp_edges[edge] = true;
          }
        }
        else {
          e2l[1] = corner;
        }
      }
      else if (!IS_EDGE_SHARP(e2l)) {
        /* More than two corners using this edge, tag as sharp if not yet done. */
        e2l[1] = INDEX_INVALID;

        /* We want to avoid tagging edges as sharp when it is already defined as such by
         * other causes than angle threshold. */
        r_sharp_edges[edge] = false;
      }
      /* Else, edge is already 'disqualified' (i.e. sharp)! */
    }
  }
}

void edges_sharp_from_angle_set(const OffsetIndices<int> faces,
                                const Span<int> corner_verts,
                                const Span<int> corner_edges,
                                const Span<float3> face_normals,
                                const Span<int> corner_to_face,
                                const Span<bool> sharp_faces,
                                const float split_angle,
                                MutableSpan<bool> sharp_edges)
{
  if (split_angle >= float(M_PI)) {
    /* Nothing to do! */
    return;
  }

  /* Mapping edge -> corners. */
  Array<int2> edge_to_corners(sharp_edges.size(), int2(0));

  mesh_edges_sharp_tag(faces,
                       corner_verts,
                       corner_edges,
                       corner_to_face,
                       face_normals,
                       sharp_faces,
                       sharp_edges,
                       split_angle,
                       edge_to_corners,
                       sharp_edges);
}

struct VertCornerInfo {
  int face;
  int corner;
  int corner_prev;
  int corner_next;
  int vert_prev;
  int vert_next;
  int local_edge_prev;
  int local_edge_next;
};

/**
 * Gather data related to all the connected faces / face corners. This makes accessing it simpler
 * later on in the various per-vertex hot loops. It also means we can be sure it will be in CPU
 * caches. Gathering it into a single Vector of an "info" struct rather than multiple vectors is
 * expected to be worth it because there are typically very few connected corners; the overhead of
 * a Vector for each piece of data would be significant.
 */
static void collect_corner_info(const OffsetIndices<int> faces,
                                const Span<int> corner_verts,
                                const Span<int> vert_faces,
                                const int vert,
                                MutableSpan<VertCornerInfo> r_corner_infos)
{
  for (const int i : vert_faces.index_range()) {
    const int face = vert_faces[i];
    r_corner_infos[i].face = face;
    r_corner_infos[i].corner = face_find_corner_from_vert(faces[face], corner_verts, vert);
    r_corner_infos[i].corner_prev = face_corner_prev(faces[face], r_corner_infos[i].corner);
    r_corner_infos[i].corner_next = face_corner_next(faces[face], r_corner_infos[i].corner);
    r_corner_infos[i].vert_prev = corner_verts[r_corner_infos[i].corner_prev];
    r_corner_infos[i].vert_next = corner_verts[r_corner_infos[i].corner_next];
  }
}

/** The edge hasn't been handled yet while the edge info is being created. */
using EdgeUninitialized = std::monostate;

/**
 * The first corner has been added to the edge. For boundary edges, this is the only corner. We
 * store whether the winding direction of the face was towards or away from the vertex to be able
 * to detect when the winding direction of two neighboring faces doesn't match.
 */
struct EdgeOneCorner {
  int local_corner_1;
  bool winding_torwards_vert;
};

/**
 * The edge is manifold and is used by two faces/corners. The actual faces and corners have to be
 * retrieved with the data in #VertCornerInfo.
 */
struct EdgeTwoCorners {
  int local_corner_1;
  int local_corner_2;
};

/**
 * The edge "breaks" the topology flow of faces around the vertex. It could be marked sharp
 * explicitly, it could be used by a sharp face, it could have mismatched face winding directions,
 * or it might be non-manifold and used by more than two faces.
 */
struct EdgeSharp {};

using VertEdgeInfo = std::variant<EdgeUninitialized, EdgeOneCorner, EdgeTwoCorners, EdgeSharp>;

static void add_corner_to_edge(const Span<int> corner_edges,
                               const Span<bool> sharp_edges,
                               const int local_corner,
                               const int corner,
                               const int other_corner,
                               const bool winding_torwards_vert,
                               VertEdgeInfo &info)
{
  if (std::holds_alternative<EdgeUninitialized>(info)) {
    if (!sharp_edges.is_empty()) {
      /* The first time we encounter the edge, we check if it is marked sharp. In that case corner
       * fans shouldn't propagate past it. To find the edge we need to check if the current corner
       * references the edge connected to `other_corner` or if `other_corner` uses the edge. */
      if (sharp_edges[corner_edges[winding_torwards_vert ? other_corner : corner]]) {
        info = EdgeSharp{};
        return;
      }
    }
    info = EdgeOneCorner{local_corner, winding_torwards_vert};
  }
  else if (const EdgeOneCorner *info_one_edge = std::get_if<EdgeOneCorner>(&info)) {
    /* If the edge ends up being used by faces, we still have to check if the winding direction
     * changes. Though it's an undesirable situation for the mesh to be in, we shouldn't propagate
     * smooth normals across edges facing opposite directions. Breaking the flow on these winding
     * direction changes also simplifies the fan traversal later on; without it the we couldn't
     * traverse by just continuing to use the next/previous corner. */
    if (info_one_edge->winding_torwards_vert == winding_torwards_vert) {
      info = EdgeSharp{};
      return;
    }
    info = EdgeTwoCorners{info_one_edge->local_corner_1, local_corner};
  }
  else {
    /* The edge is either already sharp, or we're trying to add a third corner. */
    info = EdgeSharp{};
  }
}

/** Use a custom VectorSet type to use int32 instead of int64 for the key indices. */
using LocalEdgeVectorSet = VectorSet<int,
                                     16,
                                     DefaultProbingStrategy,
                                     DefaultHash<int>,
                                     DefaultEquality<int>,
                                     SimpleVectorSetSlot<int, int>,
                                     GuardedAllocator>;

/**
 * Create a local indexing for the edges connected to the vertex (not including loose edges of
 * course). We could look up the edge indices from the VectorSet as necessary later, but it should
 * be better to just use a bit more space in #VertCornerInfo to simplify things instead.
 */
static void calc_local_edge_indices(MutableSpan<VertCornerInfo> corner_infos,
                                    LocalEdgeVectorSet &r_other_vert_to_edge)
{
  r_other_vert_to_edge.reserve(corner_infos.size());
  for (VertCornerInfo &info : corner_infos) {
    info.local_edge_prev = r_other_vert_to_edge.index_of_or_add(info.vert_prev);
    info.local_edge_next = r_other_vert_to_edge.index_of_or_add(info.vert_next);
  }
}

static void calc_connecting_edge_info(const Span<int> corner_edges,
                                      const Span<bool> sharp_edges,
                                      const Span<bool> sharp_faces,
                                      const Span<VertCornerInfo> corner_infos,
                                      MutableSpan<VertEdgeInfo> edge_infos)
{
  for (const int local_corner : corner_infos.index_range()) {
    const VertCornerInfo &info = corner_infos[local_corner];
    if (!sharp_faces.is_empty() && sharp_faces[info.face]) {
      /* Sharp faces implicitly cause sharp edges. */
      edge_infos[info.local_edge_prev] = EdgeSharp{};
      edge_infos[info.local_edge_next] = EdgeSharp{};
      continue;
    }
    /* The "previous" edge is winding towards the vertex, the "next" edge is winding away. */
    add_corner_to_edge(corner_edges,
                       sharp_edges,
                       local_corner,
                       info.corner,
                       info.corner_prev,
                       true,
                       edge_infos[info.local_edge_prev]);
    add_corner_to_edge(corner_edges,
                       sharp_edges,
                       local_corner,
                       info.corner,
                       info.corner_next,
                       false,
                       edge_infos[info.local_edge_next]);
  }
}

/**
 * From a starting corner, follow the connected edges to find the other corners "fanning" around
 * the vertex. Crucially, we've removed ambiguity from the process already by marking edges
 * connected to three faces and edges between faces with opposite winding direction sharp.
 */
static void traverse_fan_local_corners(const Span<VertCornerInfo> corner_infos,
                                       const Span<VertEdgeInfo> edge_infos,
                                       const int start_local_corner,
                                       Vector<int, 16> &result_fan)
{
  result_fan.append(start_local_corner);
  {
    /* Travel around the vertex in a right-handed clockwise direction (based on the normal). The
     * corners found in this traversal are reversed so the direction matches with the next
     * traversal (or so that the next traversal doesn't have to be added at the beginning of the
     * vector). */
    int current = start_local_corner;
    int local_edge = corner_infos[current].local_edge_next;
    bool found_cyclic_fan = false;
    while (const EdgeTwoCorners *edge = std::get_if<EdgeTwoCorners>(&edge_infos[local_edge])) {
      current = mesh::edge_other_vert(int2(edge->local_corner_1, edge->local_corner_2), current);
      if (current == start_local_corner) {
        found_cyclic_fan = true;
        break;
      }
      result_fan.append(current);
      local_edge = corner_infos[current].local_edge_next;
    }
    /* Reverse the corners added so the final order is consistent with the next traversal. */
    result_fan.as_mutable_span().reverse();

    if (found_cyclic_fan) {
      /* To match behavior from the previous implementation of face corner normal calculation, the
       * final fan is rotated so that the smallest face corner index comes first. */
      int *fan_first_corner = std::min_element(
          result_fan.begin(), result_fan.end(), [&](const int a, const int b) {
            return corner_infos[a].corner < corner_infos[b].corner;
          });
      std::rotate(result_fan.begin(), fan_first_corner, result_fan.end());
      return;
    }
  }

  /* Travel in the other direction. */
  int current = start_local_corner;
  int local_edge = corner_infos[current].local_edge_prev;
  while (const EdgeTwoCorners *edge = std::get_if<EdgeTwoCorners>(&edge_infos[local_edge])) {
    current = current == edge->local_corner_1 ? edge->local_corner_2 : edge->local_corner_1;
    /* Cyclic fans have already been found, so there's no need to check for them here. */
    result_fan.append(current);
    local_edge = corner_infos[current].local_edge_prev;
  }
}

/**
 * The edge directions are used to compute factors for the face normals from each corner. Since
 * they involve a normalization it's worth it to compute them once, especially since we've
 * deduplicated the edge indices and can easily index them with #VertCornerInfo.
 */
static void calc_edge_directions(const Span<float3> vert_positions,
                                 const Span<int> local_edge_by_vert,
                                 const float3 &vert_position,
                                 MutableSpan<float3> edge_dirs)
{
  for (const int i : local_edge_by_vert.index_range()) {
    edge_dirs[i] = math::normalize(vert_positions[local_edge_by_vert[i]] - vert_position);
  }
}

/** The normal for all the corners in the fan is a weighted combination of their face normals. */
static float3 accumulate_fan_normal(const Span<VertCornerInfo> corner_infos,
                                    const Span<float3> edge_dirs,
                                    const Span<float3> face_normals,
                                    const Span<int> local_corners_in_fan)
{
  if (local_corners_in_fan.size() == 1) {
    /* Logically this special case is unnecessary, but due to floating point precision it is
     * required for the output to be the same as previous versions of the algorithm. */
    return face_normals[corner_infos[local_corners_in_fan.first()].face];
  }
  float3 fan_normal(0);
  for (const int local_corner : local_corners_in_fan) {
    const VertCornerInfo &info = corner_infos[local_corner];
    const float3 &dir_prev = edge_dirs[info.local_edge_prev];
    const float3 &dir_next = edge_dirs[info.local_edge_next];
    const float factor = math::safe_acos_approx(math::dot(dir_prev, dir_next));
    fan_normal += face_normals[info.face] * factor;
  }
  return math::normalize(fan_normal);
}

struct CornerSpaceGroup {
  /* Maybe acyclic and unordered set of adjacent corners in same smooth group around vertex. */
  Array<int> fan_corners;
  CornerNormalSpace space;
};

/** Don't inline this function to simplify the code path without custom normals. */
BLI_NOINLINE static void handle_fan_result_and_custom_normals(
    const Span<short2> custom_normals,
    const Span<VertCornerInfo> corner_infos,
    const Span<float3> edge_dirs,
    const Span<int> local_corners_in_fan,
    float3 &fan_normal,
    CornerNormalSpaceArray *r_fan_spaces,
    Vector<CornerSpaceGroup, 0> *r_local_space_groups)
{
  const int local_edge_first = corner_infos[local_corners_in_fan.first()].local_edge_next;
  const int local_edge_last = corner_infos[local_corners_in_fan.last()].local_edge_prev;

  Vector<float3, 16> fan_edge_dirs;
  if (local_corners_in_fan.size() > 1) {
    fan_edge_dirs.reserve(local_corners_in_fan.size() + 1);
    for (const int local_corner : local_corners_in_fan) {
      const VertCornerInfo &info = corner_infos[local_corner];
      fan_edge_dirs.append_unchecked(edge_dirs[info.local_edge_next]);
    }
    if (local_edge_last != local_edge_first) {
      fan_edge_dirs.append_unchecked(edge_dirs[local_edge_last]);
    }
  }

  const CornerNormalSpace fan_space = corner_fan_space_define(
      fan_normal, edge_dirs[local_edge_first], edge_dirs[local_edge_last], fan_edge_dirs);

  if (!custom_normals.is_empty()) {
    int2 average_custom_normal(0);
    for (const int local_corner : local_corners_in_fan) {
      const VertCornerInfo &info = corner_infos[local_corner];
      average_custom_normal += int2(custom_normals[info.corner]);
    }
    average_custom_normal /= local_corners_in_fan.size();
    fan_normal = corner_space_custom_data_to_normal(fan_space, short2(average_custom_normal));
  }

  if (!r_fan_spaces) {
    return;
  }

  Array<int> fan_corners(local_corners_in_fan.size());
  for (const int i : local_corners_in_fan.index_range()) {
    const VertCornerInfo &info = corner_infos[local_corners_in_fan[i]];
    fan_corners[i] = info.corner;
  }
  r_local_space_groups->append({std::move(fan_corners), fan_space});
}

void normals_calc_corners(const Span<float3> vert_positions,
                          const OffsetIndices<int> faces,
                          const Span<int> corner_verts,
                          const Span<int> corner_edges,
                          const GroupedSpan<int> vert_to_face_map,
                          const Span<float3> face_normals,
                          const Span<bool> sharp_edges,
                          const Span<bool> sharp_faces,
                          const Span<short2> custom_normals,
                          CornerNormalSpaceArray *r_fan_spaces,
                          MutableSpan<float3> r_corner_normals)
{
  BLI_assert(corner_verts.size() == corner_edges.size());
  BLI_assert(custom_normals.is_empty() || corner_verts.size() == custom_normals.size());
  BLI_assert(corner_verts.size() == r_corner_normals.size());
  BLI_assert(corner_verts.size() == vert_to_face_map.offsets.total_size());

  /* Mesh is not empty, but there are no faces, so no normals. */
  if (corner_verts.is_empty()) {
    return;
  }

  threading::EnumerableThreadSpecific<Vector<CornerSpaceGroup, 0>> space_groups;

  threading::parallel_for(vert_positions.index_range(), 256, [&](const IndexRange range) {
    Vector<VertCornerInfo, 16> corner_infos;
    LocalEdgeVectorSet local_edge_by_vert;
    Vector<VertEdgeInfo, 16> edge_infos;
    Vector<float3, 16> edge_dirs;
    Vector<bool, 16> local_corner_visited;
    Vector<int, 16> corners_in_fan;

    Vector<CornerSpaceGroup, 0> *local_space_groups = r_fan_spaces ? &space_groups.local() :
                                                                     nullptr;

    for (const int vert : range) {
      const float3 vert_position = vert_positions[vert];
      const Span<int> vert_faces = vert_to_face_map[vert];

      /* Because we're iterating over vertices in order to batch work for their connected face
       * corners, we have to handle loose vertices and vertices not used by faces. */
      if (vert_faces.is_empty()) {
        continue;
      }

      corner_infos.resize(vert_faces.size());
      collect_corner_info(faces, corner_verts, vert_faces, vert, corner_infos);

      local_edge_by_vert.clear_and_keep_capacity();
      calc_local_edge_indices(corner_infos, local_edge_by_vert);

      edge_infos.clear();
      edge_infos.resize(local_edge_by_vert.size());
      calc_connecting_edge_info(corner_edges, sharp_edges, sharp_faces, corner_infos, edge_infos);

      edge_dirs.resize(edge_infos.size());
      calc_edge_directions(vert_positions, local_edge_by_vert, vert_position, edge_dirs);

      /* Though we are protected from traversing to the same corner twice by the fact that 3-way
       * connections are marked sharp, we need to maintain the "visited" status of each corner so
       * we can find the next start corner for each subsequent fan traversal. Keeping track of the
       * number of visited corners is a quick way to avoid this book keeping for the final fan (and
       * there are usually just two, so that should be worth it). */
      int visited_count = 0;
      local_corner_visited.resize(vert_faces.size());
      local_corner_visited.fill(false);

      int start_local_corner = 0;
      while (true) {
        corners_in_fan.clear();
        traverse_fan_local_corners(corner_infos, edge_infos, start_local_corner, corners_in_fan);

        float3 fan_normal = accumulate_fan_normal(
            corner_infos, edge_dirs, face_normals, corners_in_fan);

        if (!custom_normals.is_empty() || r_fan_spaces) {
          handle_fan_result_and_custom_normals(custom_normals,
                                               corner_infos,
                                               edge_dirs,
                                               corners_in_fan,
                                               fan_normal,
                                               r_fan_spaces,
                                               local_space_groups);
        }

        for (const int local_corner : corners_in_fan) {
          const VertCornerInfo &info = corner_infos[local_corner];
          r_corner_normals[info.corner] = fan_normal;
        }

        visited_count += corners_in_fan.size();
        if (visited_count == corner_infos.size()) {
          break;
        }

        local_corner_visited.as_mutable_span().fill_indices(corners_in_fan.as_span(), true);
        BLI_assert(!local_corner_visited.as_span().take_front(start_local_corner).contains(false));
        BLI_assert(local_corner_visited.as_span().drop_front(start_local_corner).contains(false));
        /* Will start traversing the next smooth fan mixed in shared index space. */
        while (local_corner_visited[start_local_corner]) {
          start_local_corner++;
        }
      }
      BLI_assert(visited_count == corner_infos.size());
    }
  });

  if (!r_fan_spaces) {
    return;
  }

  Vector<int> space_groups_count;
  Vector<Vector<CornerSpaceGroup, 0>> all_space_groups;
  /* WARNING: can't use `auto` here, causes build failure on GCC 15.2, WITH_TBB=OFF. */
  for (Vector<CornerSpaceGroup, 0> &groups : space_groups) {
    space_groups_count.append(groups.size());
    all_space_groups.append(std::move(groups));
  }
  space_groups_count.append(0);
  const OffsetIndices<int> space_offsets = offset_indices::accumulate_counts_to_offsets(
      space_groups_count);

  r_fan_spaces->spaces.reinitialize(space_offsets.total_size());
  r_fan_spaces->corner_space_indices.reinitialize(corner_verts.size());
  if (r_fan_spaces->create_corners_by_space) {
    r_fan_spaces->corners_by_space.reinitialize(space_offsets.total_size());
  }

  /* Copy the data from each local data vector to the final array. it's expected that
   * multi-threading has some benefit here, even though the work is largely just copying memory,
   * but choose a large grain size to err on the size of less parallelization. */
  const int64_t mean_size = std::max<int64_t>(1,
                                              space_offsets.total_size() / space_offsets.size());
  const int64_t grain_size = std::max<int64_t>(1, 1024 * 16 / mean_size);
  threading::parallel_for(all_space_groups.index_range(), grain_size, [&](const IndexRange range) {
    for (const int thread_i : range) {
      Vector<CornerSpaceGroup, 0> &local_space_groups = all_space_groups[thread_i];
      for (const int group_i : local_space_groups.index_range()) {
        const int space_index = space_offsets[thread_i][group_i];
        r_fan_spaces->spaces[space_index] = local_space_groups[group_i].space;
        r_fan_spaces->corner_space_indices.as_mutable_span().fill_indices(
            local_space_groups[group_i].fan_corners.as_span(), space_index);
      }
      if (!r_fan_spaces->create_corners_by_space) {
        continue;
      }
      for (const int group_i : local_space_groups.index_range()) {
        const int space_index = space_offsets[thread_i][group_i];
        r_fan_spaces->corners_by_space[space_index] = std::move(
            local_space_groups[group_i].fan_corners);
      }
    }
  });
}

#undef INDEX_UNSET
#undef INDEX_INVALID
#undef IS_EDGE_SHARP

/**
 * Compute internal representation of given custom normals (as an array of float[2]).
 * It also makes sure the mesh matches those custom normals, by setting sharp edges flag as needed
 * to get a same custom lnor for all corners sharing the same smooth fan.
 * If use_vertices if true, r_custom_corner_normals is assumed to be per-vertex, not per-corner
 * (this allows to set whole vert's normals at once, useful in some cases).
 * r_custom_corner_normals is expected to have normalized normals, or zero ones,
 * in which case they will be replaced by default corner/vertex normal.
 */

static void mesh_normals_corner_custom_set(const Span<float3> positions,
                                           const OffsetIndices<int> faces,
                                           const Span<int> corner_verts,
                                           const Span<int> corner_edges,
                                           const GroupedSpan<int> vert_to_face_map,
                                           const Span<float3> vert_normals,
                                           const Span<float3> face_normals,
                                           const Span<bool> sharp_faces,
                                           const bool use_vertices,
                                           MutableSpan<float3> r_custom_corner_normals,
                                           MutableSpan<bool> sharp_edges,
                                           MutableSpan<short2> r_clnors_data)
{
  /* We *may* make that poor #bke::mesh::normals_calc_corners() even more complex by making it
   * handling that feature too, would probably be more efficient in absolute. However, this
   * function *is not* performance-critical, since it is mostly expected to be called by IO add-ons
   * when importing custom normals, and modifier (and perhaps from some editing tools later?). So
   * better to keep some simplicity here, and just call #bke::mesh::normals_calc_corners() twice!
   */
  CornerNormalSpaceArray lnors_spacearr;
  lnors_spacearr.create_corners_by_space = true;
  BitVector<> done_corners(corner_verts.size(), false);
  Array<float3> corner_normals(corner_verts.size());
  const Array<int> corner_to_face = build_corner_to_face_map(faces);

  /* Compute current lnor spacearr. */
  normals_calc_corners(positions,
                       faces,
                       corner_verts,
                       corner_edges,
                       vert_to_face_map,
                       face_normals,
                       sharp_edges,
                       sharp_faces,
                       r_clnors_data,
                       &lnors_spacearr,
                       corner_normals);

  /* Set all given zero vectors to their default value. */
  if (use_vertices) {
    for (const int i : positions.index_range()) {
      if (is_zero_v3(r_custom_corner_normals[i])) {
        copy_v3_v3(r_custom_corner_normals[i], vert_normals[i]);
      }
    }
  }
  else {
    for (const int i : corner_verts.index_range()) {
      if (is_zero_v3(r_custom_corner_normals[i])) {
        copy_v3_v3(r_custom_corner_normals[i], corner_normals[i]);
      }
    }
  }

  /* Now, check each current smooth fan (one lnor space per smooth fan!),
   * and if all its matching custom corner_normals are not (enough) equal, add sharp edges as
   * needed. This way, next time we run bke::mesh::normals_calc_corners(), we'll get lnor
   * spacearr/smooth fans matching given custom corner_normals. Note this code *will never* unsharp
   * edges! And quite obviously, when we set custom normals per vertices, running this is
   * absolutely useless. */
  if (use_vertices) {
    done_corners.fill(true);
  }
  else {
    for (const int i : corner_verts.index_range()) {
      if (done_corners[i]) {
        continue;
      }

      const int space_index = lnors_spacearr.corner_space_indices[i];
      const Span<int> fan_corners = lnors_spacearr.corners_by_space[space_index];

      /* Notes:
       * - In case of mono-corner smooth fan, we have nothing to do.
       * - Loops in this linklist are ordered (in reversed order compared to how they were
       *   discovered by bke::mesh::normals_calc_corners(), but this is not a problem).
       *   Which means if we find a mismatching clnor,
       *   we know all remaining corners will have to be in a new, different smooth fan/lnor space.
       * - In smooth fan case, we compare each clnor against a ref one,
       *   to avoid small differences adding up into a real big one in the end!
       */
      if (fan_corners.is_empty()) {
        done_corners[i].set();
        continue;
      }

      int prev_corner = -1;
      const float *org_nor = nullptr;

      for (int i = fan_corners.index_range().last(); i >= 0; i--) {
        const int corner = fan_corners[i];
        float *nor = r_custom_corner_normals[corner];

        if (!org_nor) {
          org_nor = nor;
        }
        else if (dot_v3v3(org_nor, nor) < LNOR_SPACE_TRIGO_THRESHOLD) {
          /* Current normal differs too much from org one, we have to tag the edge between
           * previous corner's face and current's one as sharp.
           * We know those two corners do not point to the same edge,
           * since we do not allow reversed winding in a same smooth fan. */
          const IndexRange face = faces[corner_to_face[corner]];
          const int corner_prev = face_corner_prev(face, corner);
          const int edge = corner_edges[corner];
          const int edge_prev = corner_edges[corner_prev];
          const int prev_edge = corner_edges[prev_corner];
          sharp_edges[prev_edge == edge_prev ? prev_edge : edge] = true;

          org_nor = nor;
        }

        prev_corner = corner;
        done_corners[corner].set();
      }

      /* We also have to check between last and first corners,
       * otherwise we may miss some sharp edges here!
       * This is just a simplified version of above while loop.
       * See #45984. */
      if (fan_corners.size() > 1 && org_nor) {
        const int corner = fan_corners.last();
        float *nor = r_custom_corner_normals[corner];

        if (dot_v3v3(org_nor, nor) < LNOR_SPACE_TRIGO_THRESHOLD) {
          const IndexRange face = faces[corner_to_face[corner]];
          const int corner_prev = face_corner_prev(face, corner);
          const int edge = corner_edges[corner];
          const int edge_prev = corner_edges[corner_prev];
          const int prev_edge = corner_edges[prev_corner];
          sharp_edges[prev_edge == edge_prev ? prev_edge : edge] = true;
        }
      }
    }

    /* And now, recompute our new auto `corner_normals` and lnor spacearr! */
    normals_calc_corners(positions,
                         faces,
                         corner_verts,
                         corner_edges,
                         vert_to_face_map,
                         face_normals,
                         sharp_edges,
                         sharp_faces,
                         r_clnors_data,
                         &lnors_spacearr,
                         corner_normals);
  }

  /* And we just have to convert plain object-space custom normals to our
   * lnor space-encoded ones. */
  for (const int i : corner_verts.index_range()) {
    if (lnors_spacearr.corner_space_indices[i] == -1) {
      done_corners[i].reset();
      if (G.debug & G_DEBUG) {
        printf("WARNING! Still getting invalid nullptr corner space in second for loop %d!\n", i);
      }
      continue;
    }
    if (!done_corners[i]) {
      continue;
    }

    const int space_index = lnors_spacearr.corner_space_indices[i];
    const Span<int> fan_corners = lnors_spacearr.corners_by_space[space_index];

    /* Note we accumulate and average all custom normals in current smooth fan,
     * to avoid getting different clnors data (tiny differences in plain custom normals can
     * give rather huge differences in computed 2D factors). */
    if (fan_corners.size() < 2) {
      const int nidx = use_vertices ? corner_verts[i] : i;
      r_clnors_data[i] = corner_space_custom_normal_to_data(lnors_spacearr.spaces[space_index],
                                                            r_custom_corner_normals[nidx]);
      done_corners[i].reset();
    }
    else {
      float3 avg_nor(0.0f);
      for (const int corner : fan_corners) {
        const int nidx = use_vertices ? corner_verts[corner] : corner;
        avg_nor += r_custom_corner_normals[nidx];
        done_corners[corner].reset();
      }

      mul_v3_fl(avg_nor, 1.0f / float(fan_corners.size()));
      short2 clnor_data_tmp = corner_space_custom_normal_to_data(
          lnors_spacearr.spaces[space_index], avg_nor);

      r_clnors_data.fill_indices(fan_corners, clnor_data_tmp);
    }
  }
}

void normals_corner_custom_set(const Span<float3> vert_positions,
                               const OffsetIndices<int> faces,
                               const Span<int> corner_verts,
                               const Span<int> corner_edges,
                               const GroupedSpan<int> vert_to_face_map,
                               const Span<float3> vert_normals,
                               const Span<float3> face_normals,
                               const Span<bool> sharp_faces,
                               MutableSpan<bool> sharp_edges,
                               MutableSpan<float3> r_custom_corner_normals,
                               MutableSpan<short2> r_clnors_data)
{
  mesh_normals_corner_custom_set(vert_positions,
                                 faces,
                                 corner_verts,
                                 corner_edges,
                                 vert_to_face_map,
                                 vert_normals,
                                 face_normals,
                                 sharp_faces,
                                 false,
                                 r_custom_corner_normals,
                                 sharp_edges,
                                 r_clnors_data);
}

void normals_corner_custom_set_from_verts(const Span<float3> vert_positions,
                                          const OffsetIndices<int> faces,
                                          const Span<int> corner_verts,
                                          const Span<int> corner_edges,
                                          const GroupedSpan<int> vert_to_face_map,
                                          const Span<float3> vert_normals,
                                          const Span<float3> face_normals,
                                          const Span<bool> sharp_faces,
                                          MutableSpan<bool> sharp_edges,
                                          MutableSpan<float3> r_custom_vert_normals,
                                          MutableSpan<short2> r_clnors_data)
{
  mesh_normals_corner_custom_set(vert_positions,
                                 faces,
                                 corner_verts,
                                 corner_edges,
                                 vert_to_face_map,
                                 vert_normals,
                                 face_normals,
                                 sharp_faces,
                                 true,
                                 r_custom_vert_normals,
                                 sharp_edges,
                                 r_clnors_data);
}

static void mesh_set_custom_normals(Mesh &mesh,
                                    MutableSpan<float3> r_custom_nors,
                                    const bool use_vertices)
{
  MutableAttributeAccessor attributes = mesh.attributes_for_write();
  SpanAttributeWriter custom_normals = attributes.lookup_or_add_for_write_span<short2>(
      "custom_normal", AttrDomain::Corner);
  if (!custom_normals) {
    return;
  }
  SpanAttributeWriter<bool> sharp_edges = attributes.lookup_or_add_for_write_span<bool>(
      "sharp_edge", AttrDomain::Edge);
  const VArraySpan sharp_faces = *attributes.lookup<bool>("sharp_face", AttrDomain::Face);

  mesh_normals_corner_custom_set(mesh.vert_positions(),
                                 mesh.faces(),
                                 mesh.corner_verts(),
                                 mesh.corner_edges(),
                                 mesh.vert_to_face_map(),
                                 mesh.vert_normals_true(),
                                 mesh.face_normals_true(),
                                 sharp_faces,
                                 use_vertices,
                                 r_custom_nors,
                                 sharp_edges.span,
                                 custom_normals.span);

  sharp_edges.finish();
  custom_normals.finish();
}

}  // namespace mesh

static void normalize_vecs(MutableSpan<float3> normals)
{
  threading::parallel_for(normals.index_range(), 4096, [&](const IndexRange range) {
    for (const int i : range) {
      normals[i] = math::normalize(normals[i]);
    }
  });
}

void mesh_set_custom_normals(Mesh &mesh, MutableSpan<float3> corner_normals)
{
  normalize_vecs(corner_normals);
  mesh::mesh_set_custom_normals(mesh, corner_normals, false);
}

void mesh_set_custom_normals_normalized(Mesh &mesh, MutableSpan<float3> corner_normals)
{
  mesh::mesh_set_custom_normals(mesh, corner_normals, false);
}

void mesh_set_custom_normals_from_verts(Mesh &mesh, MutableSpan<float3> vert_normals)
{
  normalize_vecs(vert_normals);
  mesh::mesh_set_custom_normals(mesh, vert_normals, true);
}

void mesh_set_custom_normals_from_verts_normalized(Mesh &mesh, MutableSpan<float3> vert_normals)
{
  mesh::mesh_set_custom_normals(mesh, vert_normals, true);
}

namespace mesh {

constexpr AttributeMetaData CORNER_FAN_META_DATA{AttrDomain::Corner, AttrType::Int16_2D};

bool is_corner_fan_normals(const AttributeMetaData &meta_data)
{
  return meta_data == CORNER_FAN_META_DATA;
}

static bke::AttrDomain normal_domain_to_domain(bke::MeshNormalDomain domain)
{
  switch (domain) {
    case bke::MeshNormalDomain::Point:
      return bke::AttrDomain::Point;
    case bke::MeshNormalDomain::Face:
      return bke::AttrDomain::Face;
    case bke::MeshNormalDomain::Corner:
      return bke::AttrDomain::Corner;
  }
  BLI_assert_unreachable();
  return bke::AttrDomain::Point;
}

void NormalJoinInfo::add_no_custom_normals(const bke::MeshNormalDomain domain)
{
  this->add_domain(normal_domain_to_domain(domain));
}

void NormalJoinInfo::add_corner_fan_normals()
{
  this->add_domain(bke::AttrDomain::Corner);
  if (this->result_type == Output::None) {
    this->result_type = Output::CornerFan;
  }
}

void NormalJoinInfo::add_domain(const bke::AttrDomain domain)
{
  if (this->result_domain) {
    /* Any combination of point/face domains puts the result normals on the corner domain. */
    if (this->result_domain != domain) {
      this->result_domain = bke::AttrDomain::Corner;
    }
  }
  else {
    this->result_domain = domain;
  }
}

void NormalJoinInfo::add_free_normals(const bke::AttrDomain domain)
{
  this->add_domain(domain);
  this->result_type = Output::Free;
}

void NormalJoinInfo::add_mesh(const Mesh &mesh)
{
  const bke::AttributeAccessor attributes = mesh.attributes();
  const std::optional<bke::AttributeMetaData> custom_normal = attributes.lookup_meta_data(
      "custom_normal");
  if (!custom_normal) {
    this->add_no_custom_normals(mesh.normals_domain());
    return;
  }
  if (custom_normal->data_type == bke::AttrType::Float3) {
    if (custom_normal->domain == bke::AttrDomain::Edge) {
      /* Skip invalid storage on the edge domain. */
      this->add_no_custom_normals(mesh.normals_domain());
      return;
    }
    this->add_free_normals(custom_normal->domain);
  }
  else if (*custom_normal == CORNER_FAN_META_DATA) {
    this->add_corner_fan_normals();
  }
}

}  // namespace mesh

}  // namespace blender::bke

#undef LNOR_SPACE_TRIGO_THRESHOLD

/** \} */
