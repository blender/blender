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

#include "MEM_guardedalloc.h"

#include "BLI_math_geom.h"
#include "BLI_math_vector.h"

#include "BLI_array_utils.hh"
#include "BLI_bit_vector.hh"
#include "BLI_linklist.h"
#include "BLI_math_base.hh"
#include "BLI_math_vector.hh"
#include "BLI_memarena.h"
#include "BLI_span.hh"
#include "BLI_task.hh"
#include "BLI_utildefines.h"

#include "BKE_attribute.hh"
#include "BKE_customdata.hh"
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
  mesh.runtime->vert_normals_cache.ensure([&](Vector<float3> &r_data) { r_data = vert_normals; });
}

void mesh_vert_normals_assign(Mesh &mesh, Vector<float3> vert_normals)
{
  mesh.runtime->vert_normals_cache.ensure(
      [&](Vector<float3> &r_data) { r_data = std::move(vert_normals); });
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

  if (CustomData_has_layer(&this->corner_data, CD_CUSTOMLOOPNORMAL)) {
    return MeshNormalDomain::Corner;
  }

  const AttributeAccessor attributes = this->attributes();
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
  if (this->runtime->vert_normals_cache.is_cached()) {
    return this->runtime->vert_normals_cache.data();
  }
  const Span<float3> positions = this->vert_positions();
  const OffsetIndices faces = this->faces();
  const Span<int> corner_verts = this->corner_verts();
  const Span<float3> face_normals = this->face_normals();
  const GroupedSpan<int> vert_to_face = this->vert_to_face_map();
  this->runtime->vert_normals_cache.ensure([&](Vector<float3> &r_data) {
    r_data.reinitialize(positions.size());
    mesh::normals_calc_verts(positions, faces, corner_verts, vert_to_face, face_normals, r_data);
  });
  return this->runtime->vert_normals_cache.data();
}

blender::Span<blender::float3> Mesh::face_normals() const
{
  using namespace blender;
  this->runtime->face_normals_cache.ensure([&](Vector<float3> &r_data) {
    const Span<float3> positions = this->vert_positions();
    const OffsetIndices faces = this->faces();
    const Span<int> corner_verts = this->corner_verts();
    r_data.reinitialize(faces.size());
    bke::mesh::normals_calc_faces(positions, faces, corner_verts, r_data);
  });
  return this->runtime->face_normals_cache.data();
}

blender::Span<blender::float3> Mesh::corner_normals() const
{
  using namespace blender;
  using namespace blender::bke;
  this->runtime->corner_normals_cache.ensure([&](Vector<float3> &r_data) {
    r_data.reinitialize(this->corners_num);
    const OffsetIndices<int> faces = this->faces();
    switch (this->normals_domain()) {
      case MeshNormalDomain::Point: {
        array_utils::gather(this->vert_normals(), this->corner_verts(), r_data.as_mutable_span());
        break;
      }
      case MeshNormalDomain::Face: {
        const Span<float3> face_normals = this->face_normals();
        threading::parallel_for(faces.index_range(), 1024, [&](const IndexRange range) {
          for (const int i : range) {
            r_data.as_mutable_span().slice(faces[i]).fill(face_normals[i]);
          }
        });
        break;
      }
      case MeshNormalDomain::Corner: {
        const AttributeAccessor attributes = this->attributes();
        const VArraySpan sharp_edges = *attributes.lookup<bool>("sharp_edge", AttrDomain::Edge);
        const VArraySpan sharp_faces = *attributes.lookup<bool>("sharp_face", AttrDomain::Face);
        const short2 *custom_normals = static_cast<const short2 *>(
            CustomData_get_layer(&this->corner_data, CD_CUSTOMLOOPNORMAL));
        mesh::normals_calc_corners(this->vert_positions(),
                                   this->edges(),
                                   this->faces(),
                                   this->corner_verts(),
                                   this->corner_edges(),
                                   this->corner_to_face_map(),
                                   this->vert_normals(),
                                   this->face_normals(),
                                   sharp_edges,
                                   sharp_faces,
                                   custom_normals,
                                   nullptr,
                                   r_data);
        break;
      }
    }
  });
  return this->runtime->corner_normals_cache.data();
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

  if (UNLIKELY(fabsf(dtp_ref) >= LNOR_SPACE_TRIGO_THRESHOLD ||
               fabsf(dtp_other) >= LNOR_SPACE_TRIGO_THRESHOLD))
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

  float3 r_custom_lnor;

  /* TODO: Check whether using #sincosf() gives any noticeable benefit
   * (could not even get it working under linux though)! */
  const float pi2 = float(M_PI * 2.0);
  const float alphafac = unit_short_to_float(clnor_data[0]);
  const float alpha = (alphafac > 0.0f ? lnor_space.ref_alpha : pi2 - lnor_space.ref_alpha) *
                      alphafac;
  const float betafac = unit_short_to_float(clnor_data[1]);

  mul_v3_v3fl(r_custom_lnor, lnor_space.vec_lnor, cosf(alpha));

  if (betafac == 0.0f) {
    madd_v3_v3fl(r_custom_lnor, lnor_space.vec_ref, sinf(alpha));
  }
  else {
    const float sinalpha = sinf(alpha);
    const float beta = (betafac > 0.0f ? lnor_space.ref_beta : pi2 - lnor_space.ref_beta) *
                       betafac;
    madd_v3_v3fl(r_custom_lnor, lnor_space.vec_ref, sinalpha * cosf(beta));
    madd_v3_v3fl(r_custom_lnor, lnor_space.vec_ortho, sinalpha * sinf(beta));
  }

  return r_custom_lnor;
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

  short2 r_clnor_data;

  const float pi2 = float(M_PI * 2.0);
  const float cos_alpha = math::dot(lnor_space.vec_lnor, custom_lnor);

  const float alpha = math::safe_acos_approx(cos_alpha);
  if (alpha > lnor_space.ref_alpha) {
    /* Note we could stick to [0, pi] range here,
     * but makes decoding more complex, not worth it. */
    r_clnor_data[0] = unit_float_to_short(-(pi2 - alpha) / (pi2 - lnor_space.ref_alpha));
  }
  else {
    r_clnor_data[0] = unit_float_to_short(alpha / lnor_space.ref_alpha);
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
      r_clnor_data[1] = unit_float_to_short(-(pi2 - beta) / (pi2 - lnor_space.ref_beta));
    }
    else {
      r_clnor_data[1] = unit_float_to_short(beta / lnor_space.ref_beta);
    }
  }
  else {
    r_clnor_data[1] = 0;
  }

  return r_clnor_data;
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

namespace blender::bke::mesh {

struct CornerSplitTaskDataCommon {
  /* Read/write.
   * Note we do not need to protect it, though, since two different tasks will *always* affect
   * different elements in the arrays. */
  CornerNormalSpaceArray *lnors_spacearr;
  MutableSpan<float3> corner_normals;

  /* Read-only. */
  Span<float3> positions;
  Span<int2> edges;
  Span<int> corner_verts;
  Span<int> corner_edges;
  OffsetIndices<int> faces;
  Span<int2> edge_to_corners;
  Span<int> corner_to_face;
  Span<float3> face_normals;
  Span<float3> vert_normals;
  Span<short2> clnors_data;
};

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

/**
 * Builds a simplified map from edges to face corners, marking special values when
 * it encounters sharp edges or borders between faces with flipped winding orders.
 */
static void build_edge_to_corner_map_with_flip_and_sharp(const OffsetIndices<int> faces,
                                                         const Span<int> corner_verts,
                                                         const Span<int> corner_edges,
                                                         const Span<bool> sharp_faces,
                                                         const Span<bool> sharp_edges,
                                                         MutableSpan<int2> edge_to_corners)
{
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
        e2l[1] = !face_is_smooth(face_i) ? INDEX_INVALID : INDEX_UNSET;
      }
      else if (e2l[1] == INDEX_UNSET) {
        /* Second corner using this edge, time to test its sharpness.
         * An edge is sharp if it is tagged as such, or its face is not smooth,
         * or both face have opposed (flipped) normals, i.e. both corners on the same edge share
         * the same vertex. */
        if (!face_is_smooth(face_i) || (!sharp_edges.is_empty() && sharp_edges[edge]) ||
            vert == corner_verts[e2l[0]])
        {
          /* NOTE: we are sure that corner != 0 here ;). */
          e2l[1] = INDEX_INVALID;
        }
        else {
          e2l[1] = corner;
        }
      }
      else if (!IS_EDGE_SHARP(e2l)) {
        /* More than two corners using this edge, tag as sharp if not yet done. */
        e2l[1] = INDEX_INVALID;
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

  /* Mapping edge -> corners. See #bke::mesh::normals_calc_corners for details. */
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

static void corner_manifold_fan_around_vert_next(const Span<int> corner_verts,
                                                 const OffsetIndices<int> faces,
                                                 const Span<int> corner_to_face,
                                                 const int2 e2lfan_curr,
                                                 const int vert_pivot,
                                                 int *r_fan_corner,
                                                 int *r_vert_corner)
{
  const int fan_corner_orig = *r_fan_corner;
  const int vert_fan_orig = corner_verts[fan_corner_orig];

  /* WARNING: This is rather complex!
   * We have to find our next edge around the vertex (fan mode).
   * First we find the next corner, which is either previous or next to fan_corner, depending
   * whether both corners using current edge are in the same direction or not, and whether
   * fan_corner actually uses the vertex we are fanning around!
   * fan_corner is the index of the next corner here, and the next corner is not the real next one
   * (i.e. not the future `fan_corner`). */
  *r_fan_corner = (e2lfan_curr[0] == *r_fan_corner) ? e2lfan_curr[1] : e2lfan_curr[0];

  BLI_assert(*r_fan_corner >= 0);

  const int vert_fan_next = corner_verts[*r_fan_corner];
  const IndexRange face_fan_next = faces[corner_to_face[*r_fan_corner]];
  if ((vert_fan_orig == vert_fan_next && vert_fan_orig == vert_pivot) ||
      !ELEM(vert_fan_orig, vert_fan_next, vert_pivot))
  {
    /* We need the previous corner, but current one is our vertex's corner. */
    *r_vert_corner = *r_fan_corner;
    *r_fan_corner = face_corner_prev(face_fan_next, *r_fan_corner);
  }
  else {
    /* We need the next corner, which is also our vertex's corner. */
    *r_fan_corner = face_corner_next(face_fan_next, *r_fan_corner);
    *r_vert_corner = *r_fan_corner;
  }
}

static void lnor_space_for_single_fan(CornerSplitTaskDataCommon *common_data,
                                      const int corner,
                                      const int space_index)
{
  const Span<int> corner_to_face = common_data->corner_to_face;
  const Span<float3> face_normals = common_data->face_normals;
  MutableSpan<float3> corner_normals = common_data->corner_normals;

  corner_normals[corner] = face_normals[corner_to_face[corner]];

  if (CornerNormalSpaceArray *lnors_spacearr = common_data->lnors_spacearr) {
    const Span<float3> positions = common_data->positions;
    const Span<int2> edges = common_data->edges;
    const OffsetIndices faces = common_data->faces;
    const Span<int> corner_verts = common_data->corner_verts;
    const Span<int> corner_edges = common_data->corner_edges;
    const Span<short2> clnors_data = common_data->clnors_data;

    const int face_index = corner_to_face[corner];
    const int corner_prev = mesh::face_corner_prev(faces[face_index], corner);

    /* The vertex we are "fanning" around. */
    const int vert_pivot = corner_verts[corner];
    const int vert_2 = edge_other_vert(edges[corner_edges[corner]], vert_pivot);
    const int vert_3 = edge_other_vert(edges[corner_edges[corner_prev]], vert_pivot);

    const float3 vec_curr = math::normalize(positions[vert_2] - positions[vert_pivot]);
    const float3 vec_prev = math::normalize(positions[vert_3] - positions[vert_pivot]);

    CornerNormalSpace &space = lnors_spacearr->spaces[space_index];
    space = corner_fan_space_define(corner_normals[corner], vec_curr, vec_prev, {});
    lnors_spacearr->corner_space_indices[corner] = space_index;

    if (!clnors_data.is_empty()) {
      corner_normals[corner] = corner_space_custom_data_to_normal(space, clnors_data[corner]);
    }

    if (!lnors_spacearr->corners_by_space.is_empty()) {
      lnors_spacearr->corners_by_space[space_index] = {corner};
    }
  }
}

static void split_corner_normal_fan_do(CornerSplitTaskDataCommon *common_data,
                                       const int corner,
                                       const int space_index,
                                       Vector<float3, 16> *edge_vectors)
{
  CornerNormalSpaceArray *lnors_spacearr = common_data->lnors_spacearr;
  MutableSpan<float3> corner_normals = common_data->corner_normals;

  const Span<float3> positions = common_data->positions;
  const Span<int2> edges = common_data->edges;
  const OffsetIndices faces = common_data->faces;
  const Span<int> corner_verts = common_data->corner_verts;
  const Span<int> corner_edges = common_data->corner_edges;
  const Span<int2> edge_to_corners = common_data->edge_to_corners;
  const Span<int> corner_to_face = common_data->corner_to_face;
  const Span<float3> face_normals = common_data->face_normals;
  const Span<short2> clnors_data = common_data->clnors_data;

  const int face_index = corner_to_face[corner];
  const int corner_prev = face_corner_prev(faces[face_index], corner);

  /* Sigh! we have to fan around current vertex, until we find the other non-smooth edge,
   * and accumulate face normals into the vertex!
   * Note in case this vertex has only one sharp edges, this is a waste because the normal is the
   * same as the vertex normal, but I do not see any easy way to detect that (would need to count
   * number of sharp edges per vertex, I doubt the additional memory usage would be worth it,
   * especially as it should not be a common case in real-life meshes anyway). */
  const int vert_pivot = corner_verts[corner]; /* The vertex we are "fanning" around! */

  /* `corner` would be `corner_prev` if we needed that one. */
  const int2 &edge_orig = edges[corner_edges[corner]];

  float3 vec_curr;
  float3 vec_prev;
  float3 vec_org;
  float3 lnor(0.0f);

  int2 clnors_avg(0);

  Vector<int, 32> processed_corners;

  /* `vert_corner` the corner of our current edge might not be the corner of our current
   * vertex!
   */
  int fan_corner = corner_prev;
  int vert_corner = corner;

  BLI_assert(fan_corner >= 0);
  BLI_assert(vert_corner >= 0);

  /* Only need to compute previous edge's vector once, then we can just reuse old current one! */
  {
    const int vert_2 = edge_other_vert(edge_orig, vert_pivot);
    vec_org = math::normalize(positions[vert_2] - positions[vert_pivot]);
    vec_prev = vec_org;

    if (lnors_spacearr) {
      edge_vectors->append(vec_org);
    }
  }

  while (true) {
    const int2 &edge = edges[corner_edges[fan_corner]];
    /* Compute edge vectors.
     * NOTE: We could pre-compute those into an array, in the first iteration, instead of computing
     *       them twice (or more) here. However, time gained is not worth memory and time lost,
     *       given the fact that this code should not be called that much in real-life meshes.
     */
    {
      const int vert_2 = edge_other_vert(edge, vert_pivot);
      vec_curr = math::normalize(positions[vert_2] - positions[vert_pivot]);
    }

    /* Code similar to accumulate_vertex_normals_poly_v3. */
    /* Calculate angle between the two face edges incident on this vertex. */
    lnor += face_normals[corner_to_face[fan_corner]] *
            math::safe_acos_approx(math::dot(vec_curr, vec_prev));

    processed_corners.append(vert_corner);

    if (lnors_spacearr) {
      if (edge != edge_orig) {
        /* We store here all edges-normalized vectors processed. */
        edge_vectors->append(vec_curr);
      }
      if (!lnors_spacearr->corners_by_space.is_empty()) {
        lnors_spacearr->corners_by_space[space_index] = processed_corners.as_span();
      }
      if (!clnors_data.is_empty()) {
        clnors_avg += int2(clnors_data[vert_corner]);
      }
    }

    if (IS_EDGE_SHARP(edge_to_corners[corner_edges[fan_corner]]) || (edge == edge_orig)) {
      /* Current edge is sharp and we have finished with this fan of faces around this vert,
       * or this vert is smooth, and we have completed a full turn around it. */
      break;
    }

    vec_prev = vec_curr;

    /* Find next corner of the smooth fan. */
    corner_manifold_fan_around_vert_next(corner_verts,
                                         faces,
                                         corner_to_face,
                                         edge_to_corners[corner_edges[fan_corner]],
                                         vert_pivot,
                                         &fan_corner,
                                         &vert_corner);
  }

  float length;
  lnor = math::normalize_and_get_length(lnor, length);

  /* If we are generating lnor spacearr, we can now define the one for this fan,
   * and optionally compute final lnor from custom data too!
   */
  if (lnors_spacearr) {
    if (UNLIKELY(length == 0.0f)) {
      /* Use vertex normal as fallback! */
      lnor = corner_normals[vert_corner];
      length = 1.0f;
    }

    CornerNormalSpace &lnor_space = lnors_spacearr->spaces[space_index];
    lnor_space = corner_fan_space_define(lnor, vec_org, vec_curr, *edge_vectors);
    lnors_spacearr->corner_space_indices.as_mutable_span().fill_indices(
        processed_corners.as_span(), space_index);
    edge_vectors->clear();

    if (!clnors_data.is_empty()) {
      clnors_avg /= processed_corners.size();
      lnor = corner_space_custom_data_to_normal(lnor_space, short2(clnors_avg));
    }
  }

  /* In case we get a zero normal here, just use vertex normal already set! */
  if (LIKELY(length != 0.0f)) {
    /* Copy back the final computed normal into all related corner-normals. */
    corner_normals.fill_indices(processed_corners.as_span(), lnor);
  }
}

/**
 * Check whether given corner is part of an unknown-so-far cyclic smooth fan, or not.
 * Needed because cyclic smooth fans have no obvious 'entry point',
 * and yet we need to walk them once, and only once.
 */
static bool corner_split_generator_check_cyclic_smooth_fan(const Span<int> corner_verts,
                                                           const Span<int> corner_edges,
                                                           const OffsetIndices<int> faces,
                                                           const Span<int2> edge_to_corners,
                                                           const Span<int> corner_to_face,
                                                           const int2 e2l_prev,
                                                           MutableBitSpan skip_corners,
                                                           const int corner,
                                                           const int corner_prev)
{
  /* The vertex we are "fanning" around. */
  const int vert_pivot = corner_verts[corner];

  int2 e2lfan_curr = e2l_prev;
  if (IS_EDGE_SHARP(e2lfan_curr)) {
    /* Sharp corner, so not a cyclic smooth fan. */
    return false;
  }

  /* `vert_corner` the corner of our current edge might not be the corner of our current
   * vertex!
   */
  int fan_corner = corner_prev;
  int vert_corner = corner;

  BLI_assert(fan_corner >= 0);
  BLI_assert(vert_corner >= 0);

  BLI_assert(!skip_corners[vert_corner]);
  skip_corners[vert_corner].set();

  while (true) {
    /* Find next corner of the smooth fan. */
    corner_manifold_fan_around_vert_next(
        corner_verts, faces, corner_to_face, e2lfan_curr, vert_pivot, &fan_corner, &vert_corner);

    e2lfan_curr = edge_to_corners[corner_edges[fan_corner]];

    if (IS_EDGE_SHARP(e2lfan_curr)) {
      /* Sharp corner/edge, so not a cyclic smooth fan. */
      return false;
    }
    /* Smooth corner/edge. */
    if (skip_corners[vert_corner]) {
      if (vert_corner == corner) {
        /* We walked around a whole cyclic smooth fan without finding any already-processed corner,
         * means we can use initial current / previous edge as start for this smooth fan. */
        return true;
      }
      /* Already checked in some previous looping, we can abort. */
      return false;
    }

    /* We can skip it in future, and keep checking the smooth fan. */
    skip_corners[vert_corner].set();
  }
}

static void corner_split_generator(CornerSplitTaskDataCommon *common_data,
                                   Vector<int, 32> &r_single_corners,
                                   Vector<int, 32> &r_fan_corners)
{
  const Span<int> corner_verts = common_data->corner_verts;
  const Span<int> corner_edges = common_data->corner_edges;
  const OffsetIndices faces = common_data->faces;
  const Span<int> corner_to_face = common_data->corner_to_face;
  const Span<int2> edge_to_corners = common_data->edge_to_corners;

  BitVector<> skip_corners(corner_verts.size(), false);

#ifdef DEBUG_TIME
  SCOPED_TIMER_AVERAGED(__func__);
#endif

  /* We now know edges that can be smoothed (with their vector, and their two corners),
   * and edges that will be hard! Now, time to generate the normals.
   */
  for (const int face_index : faces.index_range()) {
    const IndexRange face = faces[face_index];

    for (const int corner : face) {
      const int corner_prev = mesh::face_corner_prev(face, corner);

#if 0
      printf("Checking corner %d / edge %u / vert %u (sharp edge: %d, skiploop: %d)",
             corner,
             corner_edges[corner],
             corner_verts[corner],
             IS_EDGE_SHARP(edge_to_corners[corner_edges[corner]]),
             skip_corners[corner]);
#endif

      /* A smooth edge, we have to check for cyclic smooth fan case.
       * If we find a new, never-processed cyclic smooth fan, we can do it now using that
       * corner/edge as 'entry point', otherwise we can skip it. */

      /* NOTE: In theory, we could make #corner_split_generator_check_cyclic_smooth_fan() store
       * vert_corner'es and edge indexes in two stacks, to avoid having to fan again around
       * the vert during actual computation of `clnor` & `clnorspace`.
       * However, this would complicate the code, add more memory usage, and despite its logical
       * complexity, #corner_manifold_fan_around_vert_next() is quite cheap in term of CPU cycles,
       * so really think it's not worth it. */
      if (!IS_EDGE_SHARP(edge_to_corners[corner_edges[corner]]) &&
          (skip_corners[corner] || !corner_split_generator_check_cyclic_smooth_fan(
                                       corner_verts,
                                       corner_edges,
                                       faces,
                                       edge_to_corners,
                                       corner_to_face,
                                       edge_to_corners[corner_edges[corner_prev]],
                                       skip_corners,
                                       corner,
                                       corner_prev)))
      {
        // printf("SKIPPING!\n");
      }
      else {
        if (IS_EDGE_SHARP(edge_to_corners[corner_edges[corner]]) &&
            IS_EDGE_SHARP(edge_to_corners[corner_edges[corner_prev]]))
        {
          /* Simple case (both edges around that vertex are sharp in current face),
           * this corner just takes its face normal. */
          r_single_corners.append(corner);
        }
        else {
          /* We do not need to check/tag corners as already computed. Due to the fact that a corner
           * only points to one of its two edges, the same fan will never be walked more than once.
           * Since we consider edges that have neighbor faces with inverted (flipped) normals as
           * sharp, we are sure that no fan will be skipped, even only considering the case (sharp
           * current edge, smooth previous edge), and not the alternative (smooth current edge,
           * sharp previous edge). All this due/thanks to the link between normals and corner
           * ordering (i.e. winding). */
          r_fan_corners.append(corner);
        }
      }
    }
  }
}

void normals_calc_corners(const Span<float3> vert_positions,
                          const Span<int2> edges,
                          const OffsetIndices<int> faces,
                          const Span<int> corner_verts,
                          const Span<int> corner_edges,
                          const Span<int> corner_to_face_map,
                          const Span<float3> vert_normals,
                          const Span<float3> face_normals,
                          const Span<bool> sharp_edges,
                          const Span<bool> sharp_faces,
                          const short2 *clnors_data,
                          CornerNormalSpaceArray *r_lnors_spacearr,
                          MutableSpan<float3> r_corner_normals)
{
  /**
   * Mapping edge -> corners.
   * If that edge is used by more than two corners (faces),
   * it is always sharp (and tagged as such, see below).
   * We also use the second corner index as a kind of flag:
   *
   * - smooth edge: > 0.
   * - sharp edge: < 0 (INDEX_INVALID || INDEX_UNSET).
   * - unset: INDEX_UNSET.
   *
   * Note that currently we only have two values for second corner of sharp edges.
   * However, if needed, we can store the negated value of corner index instead of INDEX_INVALID
   * to retrieve the real value later in code).
   * Note also that loose edges always have both values set to 0! */
  Array<int2> edge_to_corners(edges.size(), int2(0));

  CornerNormalSpaceArray _lnors_spacearr;

#ifdef DEBUG_TIME
  SCOPED_TIMER_AVERAGED(__func__);
#endif

  if (!r_lnors_spacearr && clnors_data) {
    /* We need to compute lnor spacearr if some custom lnor data are given to us! */
    r_lnors_spacearr = &_lnors_spacearr;
  }

  /* Init data common to all tasks. */
  CornerSplitTaskDataCommon common_data;
  common_data.lnors_spacearr = r_lnors_spacearr;
  common_data.corner_normals = r_corner_normals;
  common_data.clnors_data = {clnors_data, clnors_data ? corner_verts.size() : 0};
  common_data.positions = vert_positions;
  common_data.edges = edges;
  common_data.faces = faces;
  common_data.corner_verts = corner_verts;
  common_data.corner_edges = corner_edges;
  common_data.edge_to_corners = edge_to_corners;
  common_data.corner_to_face = corner_to_face_map;
  common_data.face_normals = face_normals;
  common_data.vert_normals = vert_normals;

  /* Pre-populate all corner normals as if their verts were all smooth.
   * This way we don't have to compute those later! */
  array_utils::gather(vert_normals, corner_verts, r_corner_normals, 1024);

  /* This first corner check which edges are actually smooth, and compute edge vectors. */
  build_edge_to_corner_map_with_flip_and_sharp(
      faces, corner_verts, corner_edges, sharp_faces, sharp_edges, edge_to_corners);

  Vector<int, 32> single_corners;
  Vector<int, 32> fan_corners;
  corner_split_generator(&common_data, single_corners, fan_corners);

  if (r_lnors_spacearr) {
    r_lnors_spacearr->spaces.reinitialize(single_corners.size() + fan_corners.size());
    r_lnors_spacearr->corner_space_indices = Array<int>(corner_verts.size(), -1);
    if (r_lnors_spacearr->create_corners_by_space) {
      r_lnors_spacearr->corners_by_space.reinitialize(r_lnors_spacearr->spaces.size());
    }
  }

  threading::parallel_for(single_corners.index_range(), 1024, [&](const IndexRange range) {
    for (const int i : range) {
      const int corner = single_corners[i];
      lnor_space_for_single_fan(&common_data, corner, i);
    }
  });

  threading::parallel_for(fan_corners.index_range(), 1024, [&](const IndexRange range) {
    Vector<float3, 16> edge_vectors;
    for (const int i : range) {
      const int corner = fan_corners[i];
      const int space_index = single_corners.size() + i;
      split_corner_normal_fan_do(&common_data, corner, space_index, &edge_vectors);
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
                                           const Span<int2> edges,
                                           const OffsetIndices<int> faces,
                                           const Span<int> corner_verts,
                                           const Span<int> corner_edges,
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
                       edges,
                       faces,
                       corner_verts,
                       corner_edges,
                       corner_to_face,
                       vert_normals,
                       face_normals,
                       sharp_edges,
                       sharp_faces,
                       r_clnors_data.data(),
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
      if (lnors_spacearr.corner_space_indices[i] == -1) {
        /* This should not happen in theory, but in some rare case (probably ugly geometry)
         * we can get some missing loopspacearr at this point. :/
         * Maybe we should set those corners' edges as sharp? */
        done_corners[i].set();
        if (G.debug & G_DEBUG) {
          printf("WARNING! Getting invalid nullptr corner space for corner %d!\n", i);
        }
        continue;
      }
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
        const int lidx = fan_corners[i];
        float *nor = r_custom_corner_normals[lidx];

        if (!org_nor) {
          org_nor = nor;
        }
        else if (dot_v3v3(org_nor, nor) < LNOR_SPACE_TRIGO_THRESHOLD) {
          /* Current normal differs too much from org one, we have to tag the edge between
           * previous corner's face and current's one as sharp.
           * We know those two corners do not point to the same edge,
           * since we do not allow reversed winding in a same smooth fan. */
          const IndexRange face = faces[corner_to_face[lidx]];
          const int corner_prev = face_corner_prev(face, lidx);
          const int edge = corner_edges[lidx];
          const int edge_prev = corner_edges[corner_prev];
          const int prev_edge = corner_edges[prev_corner];
          sharp_edges[prev_edge == edge_prev ? prev_edge : edge] = true;

          org_nor = nor;
        }

        prev_corner = lidx;
        done_corners[lidx].set();
      }

      /* We also have to check between last and first corners,
       * otherwise we may miss some sharp edges here!
       * This is just a simplified version of above while loop.
       * See #45984. */
      if (fan_corners.size() > 1 && org_nor) {
        const int lidx = fan_corners.last();
        float *nor = r_custom_corner_normals[lidx];

        if (dot_v3v3(org_nor, nor) < LNOR_SPACE_TRIGO_THRESHOLD) {
          const IndexRange face = faces[corner_to_face[lidx]];
          const int corner_prev = face_corner_prev(face, lidx);
          const int edge = corner_edges[lidx];
          const int edge_prev = corner_edges[corner_prev];
          const int prev_edge = corner_edges[prev_corner];
          sharp_edges[prev_edge == edge_prev ? prev_edge : edge] = true;
        }
      }
    }

    /* And now, recompute our new auto `corner_normals` and lnor spacearr! */
    normals_calc_corners(positions,
                         edges,
                         faces,
                         corner_verts,
                         corner_edges,
                         corner_to_face,
                         vert_normals,
                         face_normals,
                         sharp_edges,
                         sharp_faces,
                         r_clnors_data.data(),
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
      for (const int lidx : fan_corners) {
        const int nidx = use_vertices ? corner_verts[lidx] : lidx;
        avg_nor += r_custom_corner_normals[nidx];
        done_corners[lidx].reset();
      }

      mul_v3_fl(avg_nor, 1.0f / float(fan_corners.size()));
      short2 clnor_data_tmp = corner_space_custom_normal_to_data(
          lnors_spacearr.spaces[space_index], avg_nor);

      r_clnors_data.fill_indices(fan_corners, clnor_data_tmp);
    }
  }
}

void normals_corner_custom_set(const Span<float3> vert_positions,
                               const Span<int2> edges,
                               const OffsetIndices<int> faces,
                               const Span<int> corner_verts,
                               const Span<int> corner_edges,
                               const Span<float3> vert_normals,
                               const Span<float3> face_normals,
                               const Span<bool> sharp_faces,
                               MutableSpan<bool> sharp_edges,
                               MutableSpan<float3> r_custom_corner_normals,
                               MutableSpan<short2> r_clnors_data)
{
  mesh_normals_corner_custom_set(vert_positions,
                                 edges,
                                 faces,
                                 corner_verts,
                                 corner_edges,
                                 vert_normals,
                                 face_normals,
                                 sharp_faces,
                                 false,
                                 r_custom_corner_normals,
                                 sharp_edges,
                                 r_clnors_data);
}

void normals_corner_custom_set_from_verts(const Span<float3> vert_positions,
                                          const Span<int2> edges,
                                          const OffsetIndices<int> faces,
                                          const Span<int> corner_verts,
                                          const Span<int> corner_edges,
                                          const Span<float3> vert_normals,
                                          const Span<float3> face_normals,
                                          const Span<bool> sharp_faces,
                                          MutableSpan<bool> sharp_edges,
                                          MutableSpan<float3> r_custom_vert_normals,
                                          MutableSpan<short2> r_clnors_data)
{
  mesh_normals_corner_custom_set(vert_positions,
                                 edges,
                                 faces,
                                 corner_verts,
                                 corner_edges,
                                 vert_normals,
                                 face_normals,
                                 sharp_faces,
                                 true,
                                 r_custom_vert_normals,
                                 sharp_edges,
                                 r_clnors_data);
}

static void mesh_set_custom_normals(Mesh *mesh, float (*r_custom_nors)[3], const bool use_vertices)
{
  short2 *clnors = static_cast<short2 *>(
      CustomData_get_layer_for_write(&mesh->corner_data, CD_CUSTOMLOOPNORMAL, mesh->corners_num));
  if (clnors != nullptr) {
    memset(clnors, 0, sizeof(*clnors) * mesh->corners_num);
  }
  else {
    clnors = static_cast<short2 *>(CustomData_add_layer(
        &mesh->corner_data, CD_CUSTOMLOOPNORMAL, CD_SET_DEFAULT, mesh->corners_num));
  }
  MutableAttributeAccessor attributes = mesh->attributes_for_write();
  SpanAttributeWriter<bool> sharp_edges = attributes.lookup_or_add_for_write_span<bool>(
      "sharp_edge", AttrDomain::Edge);
  const VArraySpan sharp_faces = *attributes.lookup<bool>("sharp_face", AttrDomain::Face);

  mesh_normals_corner_custom_set(mesh->vert_positions(),
                                 mesh->edges(),
                                 mesh->faces(),
                                 mesh->corner_verts(),
                                 mesh->corner_edges(),
                                 mesh->vert_normals(),
                                 mesh->face_normals(),
                                 sharp_faces,
                                 use_vertices,
                                 {reinterpret_cast<float3 *>(r_custom_nors),
                                  use_vertices ? mesh->verts_num : mesh->corners_num},
                                 sharp_edges.span,
                                 {clnors, mesh->corners_num});

  sharp_edges.finish();
}

}  // namespace blender::bke::mesh

void BKE_mesh_set_custom_normals(Mesh *mesh, float (*r_custom_corner_normals)[3])
{
  blender::bke::mesh::mesh_set_custom_normals(mesh, r_custom_corner_normals, false);
}

void BKE_mesh_set_custom_normals_from_verts(Mesh *mesh, float (*r_custom_vert_normals)[3])
{
  blender::bke::mesh::mesh_set_custom_normals(mesh, r_custom_vert_normals, true);
}

void BKE_mesh_normals_loop_to_vertex(const int numVerts,
                                     const int *corner_verts,
                                     const int numLoops,
                                     const float (*clnors)[3],
                                     float (*r_vert_clnors)[3])
{
  int *vert_loops_count = (int *)MEM_calloc_arrayN(
      size_t(numVerts), sizeof(*vert_loops_count), __func__);

  copy_vn_fl((float *)r_vert_clnors, 3 * numVerts, 0.0f);

  int i;
  for (i = 0; i < numLoops; i++) {
    const int vert = corner_verts[i];
    add_v3_v3(r_vert_clnors[vert], clnors[i]);
    vert_loops_count[vert]++;
  }

  for (i = 0; i < numVerts; i++) {
    mul_v3_fl(r_vert_clnors[i], 1.0f / float(vert_loops_count[i]));
  }

  MEM_freeN(vert_loops_count);
}

#undef LNOR_SPACE_TRIGO_THRESHOLD

/** \} */
