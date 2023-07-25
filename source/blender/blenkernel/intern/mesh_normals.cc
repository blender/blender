/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 *
 * Mesh normal calculation functions.
 *
 * \see bmesh_mesh_normals.c for the equivalent #BMesh functionality.
 */

#include <climits>

#include "MEM_guardedalloc.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BLI_array_utils.hh"
#include "BLI_bit_vector.hh"
#include "BLI_linklist.h"
#include "BLI_math.h"
#include "BLI_math_vector.hh"
#include "BLI_memarena.h"
#include "BLI_span.hh"
#include "BLI_task.hh"
#include "BLI_timeit.hh"
#include "BLI_utildefines.h"

#include "BKE_attribute.hh"
#include "BKE_customdata.h"
#include "BKE_editmesh_cache.hh"
#include "BKE_global.h"
#include "BKE_mesh.hh"
#include "BKE_mesh_mapping.h"

#include "atomic_ops.h"

// #define DEBUG_TIME

#ifdef DEBUG_TIME
#  include "BLI_timeit.hh"
#endif

/* -------------------------------------------------------------------- */
/** \name Private Utility Functions
 * \{ */

/**
 * A thread-safe version of #add_v3_v3 that uses a spin-lock.
 *
 * \note Avoid using this when the chance of contention is high.
 */
static void add_v3_v3_atomic(float r[3], const float a[3])
{
#define FLT_EQ_NONAN(_fa, _fb) (*((const uint32_t *)&_fa) == *((const uint32_t *)&_fb))

  float virtual_lock = r[0];
  while (true) {
    /* This loops until following conditions are met:
     * - `r[0]` has same value as virtual_lock (i.e. it did not change since last try).
     * - `r[0]` was not `FLT_MAX`, i.e. it was not locked by another thread. */
    const float test_lock = atomic_cas_float(&r[0], virtual_lock, FLT_MAX);
    if (_ATOMIC_LIKELY(FLT_EQ_NONAN(test_lock, virtual_lock) && (test_lock != FLT_MAX))) {
      break;
    }
    virtual_lock = test_lock;
  }
  virtual_lock += a[0];
  r[1] += a[1];
  r[2] += a[2];

  /* Second atomic operation to 'release'
   * our lock on that vector and set its first scalar value. */
  /* Note that we do not need to loop here, since we 'locked' `r[0]`,
   * nobody should have changed it in the mean time. */
  virtual_lock = atomic_cas_float(&r[0], FLT_MAX, virtual_lock);
  BLI_assert(virtual_lock == FLT_MAX);

#undef FLT_EQ_NONAN
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Public Utility Functions
 *
 * Related to managing normals but not directly related to calculating normals.
 * \{ */

float (*BKE_mesh_vert_normals_for_write(Mesh *mesh))[3]
{
  mesh->runtime->vert_normals.reinitialize(mesh->totvert);
  return reinterpret_cast<float(*)[3]>(mesh->runtime->vert_normals.data());
}

void BKE_mesh_vert_normals_clear_dirty(Mesh *mesh)
{
  mesh->runtime->vert_normals_dirty = false;
  BLI_assert(mesh->runtime->vert_normals.size() == mesh->totvert);
}

bool BKE_mesh_vert_normals_are_dirty(const Mesh *mesh)
{
  return mesh->runtime->vert_normals_dirty;
}

bool BKE_mesh_face_normals_are_dirty(const Mesh *mesh)
{
  return mesh->runtime->face_normals_dirty;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Mesh Normal Calculation (Polygons)
 * \{ */

namespace blender::bke::mesh {

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
    normal[2] = 1.0f; /* other axis set to 0.0 */
  }

  return normal;
}

float3 face_normal_calc(const Span<float3> vert_positions, const Span<int> face_verts)
{
  if (face_verts.size() > 4) {
    return normal_calc_ngon(vert_positions, face_verts);
  }
  if (face_verts.size() == 3) {
    return math::normal_tri(vert_positions[face_verts[0]],
                            vert_positions[face_verts[1]],
                            vert_positions[face_verts[2]]);
  }
  if (face_verts.size() == 4) {
    float3 normal;
    normal_quad_v3(normal,
                   vert_positions[face_verts[0]],
                   vert_positions[face_verts[1]],
                   vert_positions[face_verts[2]],
                   vert_positions[face_verts[3]]);
    return normal;
  }
  /* horrible, two sided face! */
  return float3(0);
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
      face_normals[i] = face_normal_calc(positions, corner_verts.slice(faces[i]));
    }
  });
}

void normals_calc_face_vert(const Span<float3> positions,
                            const OffsetIndices<int> faces,
                            const Span<int> corner_verts,
                            MutableSpan<float3> face_normals,
                            MutableSpan<float3> vert_normals)
{

  /* Zero the vertex normal array for accumulation. */
  {
    memset(vert_normals.data(), 0, vert_normals.as_span().size_in_bytes());
  }

  /* Compute face normals, accumulating them into vertex normals. */
  {
    threading::parallel_for(faces.index_range(), 1024, [&](const IndexRange range) {
      for (const int face_i : range) {
        const Span<int> face_verts = corner_verts.slice(faces[face_i]);

        float3 &pnor = face_normals[face_i];

        const int i_end = face_verts.size() - 1;

        /* Polygon Normal and edge-vector. */
        /* Inline version of #face_normal_calc, also does edge-vectors. */
        {
          zero_v3(pnor);
          /* Newell's Method */
          const float *v_curr = positions[face_verts[i_end]];
          for (int i_next = 0; i_next <= i_end; i_next++) {
            const float *v_next = positions[face_verts[i_next]];
            add_newell_cross_v3_v3v3(pnor, v_curr, v_next);
            v_curr = v_next;
          }
          if (UNLIKELY(normalize_v3(pnor) == 0.0f)) {
            pnor[2] = 1.0f; /* Other axes set to zero. */
          }
        }

        /* Accumulate angle weighted face normal into the vertex normal. */
        /* Inline version of #accumulate_vertex_normals_poly_v3. */
        {
          float edvec_prev[3], edvec_next[3], edvec_end[3];
          const float *v_curr = positions[face_verts[i_end]];
          sub_v3_v3v3(edvec_prev, positions[face_verts[i_end - 1]], v_curr);
          normalize_v3(edvec_prev);
          copy_v3_v3(edvec_end, edvec_prev);

          for (int i_next = 0, i_curr = i_end; i_next <= i_end; i_curr = i_next++) {
            const float *v_next = positions[face_verts[i_next]];

            /* Skip an extra normalization by reusing the first calculated edge. */
            if (i_next != i_end) {
              sub_v3_v3v3(edvec_next, v_curr, v_next);
              normalize_v3(edvec_next);
            }
            else {
              copy_v3_v3(edvec_next, edvec_end);
            }

            /* Calculate angle between the two face edges incident on this vertex. */
            const float fac = saacos(-dot_v3v3(edvec_prev, edvec_next));
            const float vnor_add[3] = {pnor[0] * fac, pnor[1] * fac, pnor[2] * fac};

            float *vnor = vert_normals[face_verts[i_curr]];
            add_v3_v3_atomic(vnor, vnor_add);
            v_curr = v_next;
            copy_v3_v3(edvec_prev, edvec_next);
          }
        }
      }
    });
  }

  /* Normalize and validate computed vertex normals. */
  {
    threading::parallel_for(positions.index_range(), 1024, [&](const IndexRange range) {
      for (const int vert_i : range) {
        float *no = vert_normals[vert_i];

        if (UNLIKELY(normalize_v3(no) == 0.0f)) {
          /* Following Mesh convention; we use vertex coordinate itself for normal in this case. */
          normalize_v3_v3(no, positions[vert_i]);
        }
      }
    });
  }
}

/** \} */

}  // namespace blender::bke::mesh

/* -------------------------------------------------------------------- */
/** \name Mesh Normal Calculation
 * \{ */

blender::Span<blender::float3> Mesh::vert_normals() const
{
  using namespace blender;
  if (!this->runtime->vert_normals_dirty) {
    BLI_assert(this->runtime->vert_normals.size() == this->totvert);
    return this->runtime->vert_normals;
  }

  std::lock_guard lock{this->runtime->normals_mutex};
  if (!this->runtime->vert_normals_dirty) {
    BLI_assert(this->runtime->vert_normals.size() == this->totvert);
    return this->runtime->vert_normals;
  }

  /* Isolate task because a mutex is locked and computing normals is multi-threaded. */
  threading::isolate_task([&]() {
    const Span<float3> positions = this->vert_positions();
    const OffsetIndices faces = this->faces();
    const Span<int> corner_verts = this->corner_verts();

    this->runtime->vert_normals.reinitialize(positions.size());
    this->runtime->face_normals.reinitialize(faces.size());
    bke::mesh::normals_calc_face_vert(
        positions, faces, corner_verts, this->runtime->face_normals, this->runtime->vert_normals);

    this->runtime->vert_normals_dirty = false;
    this->runtime->face_normals_dirty = false;
  });

  return this->runtime->vert_normals;
}

blender::Span<blender::float3> Mesh::face_normals() const
{
  using namespace blender;
  if (!this->runtime->face_normals_dirty) {
    BLI_assert(this->runtime->face_normals.size() == this->faces_num);
    return this->runtime->face_normals;
  }

  std::lock_guard lock{this->runtime->normals_mutex};
  if (!this->runtime->face_normals_dirty) {
    BLI_assert(this->runtime->face_normals.size() == this->faces_num);
    return this->runtime->face_normals;
  }

  /* Isolate task because a mutex is locked and computing normals is multi-threaded. */
  threading::isolate_task([&]() {
    const Span<float3> positions = this->vert_positions();
    const OffsetIndices faces = this->faces();
    const Span<int> corner_verts = this->corner_verts();

    this->runtime->face_normals.reinitialize(faces.size());
    bke::mesh::normals_calc_faces(positions, faces, corner_verts, this->runtime->face_normals);

    this->runtime->face_normals_dirty = false;
  });

  return this->runtime->face_normals;
}

const float (*BKE_mesh_vert_normals_ensure(const Mesh *mesh))[3]
{
  return reinterpret_cast<const float(*)[3]>(mesh->vert_normals().data());
}

void BKE_mesh_ensure_normals_for_display(Mesh *mesh)
{
  switch (mesh->runtime->wrapper_type) {
    case ME_WRAPPER_TYPE_SUBD:
    case ME_WRAPPER_TYPE_MDATA:
      mesh->vert_normals();
      mesh->face_normals();
      break;
    case ME_WRAPPER_TYPE_BMESH: {
      BMEditMesh *em = mesh->edit_mesh;
      if (blender::bke::EditMeshData *emd = mesh->runtime->edit_data) {
        if (!emd->vertexCos.is_empty()) {
          BKE_editmesh_cache_ensure_vert_normals(em, emd);
          BKE_editmesh_cache_ensure_face_normals(em, emd);
        }
      }
      return;
    }
  }
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

static CornerNormalSpace lnor_space_define(const float lnor[3],
                                           float vec_ref[3],
                                           float vec_other[3],
                                           const Span<float3> edge_vectors)
{
  CornerNormalSpace lnor_space{};
  const float pi2 = float(M_PI) * 2.0f;
  float tvec[3], dtp;
  const float dtp_ref = dot_v3v3(vec_ref, lnor);
  const float dtp_other = dot_v3v3(vec_other, lnor);

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
      alpha += saacosf(dot_v3v3(vec, lnor));
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
    lnor_space.ref_alpha = (saacosf(dot_v3v3(vec_ref, lnor)) +
                            saacosf(dot_v3v3(vec_other, lnor))) /
                           2.0f;
  }

  /* Project vec_ref on lnor's ortho plane. */
  mul_v3_v3fl(tvec, lnor, dtp_ref);
  sub_v3_v3(vec_ref, tvec);
  normalize_v3_v3(lnor_space.vec_ref, vec_ref);

  cross_v3_v3v3(tvec, lnor, lnor_space.vec_ref);
  normalize_v3_v3(lnor_space.vec_ortho, tvec);

  /* Project vec_other on lnor's ortho plane. */
  mul_v3_v3fl(tvec, lnor, dtp_other);
  sub_v3_v3(vec_other, tvec);
  normalize_v3(vec_other);

  /* Beta is angle between ref_vec and other_vec, around lnor. */
  dtp = dot_v3v3(lnor_space.vec_ref, vec_other);
  if (LIKELY(dtp < LNOR_SPACE_TRIGO_THRESHOLD)) {
    const float beta = saacos(dtp);
    lnor_space.ref_beta = (dot_v3v3(lnor_space.vec_ortho, vec_other) < 0.0f) ? pi2 - beta : beta;
  }
  else {
    lnor_space.ref_beta = pi2;
  }

  return lnor_space;
}

}  // namespace blender::bke::mesh

void BKE_lnor_space_define(MLoopNorSpace *lnor_space,
                           const float lnor[3],
                           float vec_ref[3],
                           float vec_other[3],
                           const blender::Span<blender::float3> edge_vectors)
{
  using namespace blender::bke::mesh;
  const CornerNormalSpace space = lnor_space_define(lnor, vec_ref, vec_other, edge_vectors);
  copy_v3_v3(lnor_space->vec_lnor, space.vec_lnor);
  copy_v3_v3(lnor_space->vec_ref, space.vec_ref);
  copy_v3_v3(lnor_space->vec_ortho, space.vec_ortho);
  lnor_space->ref_alpha = space.ref_alpha;
  lnor_space->ref_beta = space.ref_beta;
}

void BKE_lnor_space_add_loop(MLoopNorSpaceArray *lnors_spacearr,
                             MLoopNorSpace *lnor_space,
                             const int ml_index,
                             void *bm_loop,
                             const bool is_single)
{
  BLI_assert((lnors_spacearr->data_type == MLNOR_SPACEARR_LOOP_INDEX && bm_loop == nullptr) ||
             (lnors_spacearr->data_type == MLNOR_SPACEARR_BMLOOP_PTR && bm_loop != nullptr));

  lnors_spacearr->lspacearr[ml_index] = lnor_space;
  if (bm_loop == nullptr) {
    bm_loop = POINTER_FROM_INT(ml_index);
  }
  if (is_single) {
    BLI_assert(lnor_space->loops == nullptr);
    lnor_space->flags |= MLNOR_SPACE_IS_SINGLE;
    lnor_space->loops = (LinkNode *)bm_loop;
  }
  else {
    BLI_assert((lnor_space->flags & MLNOR_SPACE_IS_SINGLE) == 0);
    BLI_linklist_prepend_nlink(&lnor_space->loops, bm_loop, &lnors_spacearr->loops_pool[ml_index]);
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

static float3 lnor_space_custom_data_to_normal(const CornerNormalSpace &lnor_space,
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
  copy_v3_v3(r_custom_lnor, lnor_space_custom_data_to_normal(space, clnor_data));
}

namespace blender::bke::mesh {

short2 lnor_space_custom_normal_to_data(const CornerNormalSpace &lnor_space,
                                        const float3 &custom_lnor)
{
  /* We use zero vector as NOP custom normal (can be simpler than giving auto-computed `lnor`). */
  if (is_zero_v3(custom_lnor) || compare_v3v3(lnor_space.vec_lnor, custom_lnor, 1e-4f)) {
    return short2(0);
  }

  short2 r_clnor_data;

  const float pi2 = float(M_PI * 2.0);
  const float cos_alpha = dot_v3v3(lnor_space.vec_lnor, custom_lnor);
  float vec[3], cos_beta;
  float alpha;

  alpha = saacosf(cos_alpha);
  if (alpha > lnor_space.ref_alpha) {
    /* Note we could stick to [0, pi] range here,
     * but makes decoding more complex, not worth it. */
    r_clnor_data[0] = unit_float_to_short(-(pi2 - alpha) / (pi2 - lnor_space.ref_alpha));
  }
  else {
    r_clnor_data[0] = unit_float_to_short(alpha / lnor_space.ref_alpha);
  }

  /* Project custom lnor on (vec_ref, vec_ortho) plane. */
  mul_v3_v3fl(vec, lnor_space.vec_lnor, -cos_alpha);
  add_v3_v3(vec, custom_lnor);
  normalize_v3(vec);

  cos_beta = dot_v3v3(lnor_space.vec_ref, vec);

  if (cos_beta < LNOR_SPACE_TRIGO_THRESHOLD) {
    float beta = saacosf(cos_beta);
    if (dot_v3v3(lnor_space.vec_ortho, vec) < 0.0f) {
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
  copy_v2_v2_short(r_clnor_data, lnor_space_custom_normal_to_data(space, custom_lnor));
}

namespace blender::bke::mesh {

struct LoopSplitTaskDataCommon {
  /* Read/write.
   * Note we do not need to protect it, though, since two different tasks will *always* affect
   * different elements in the arrays. */
  CornerNormalSpaceArray *lnors_spacearr;
  MutableSpan<float3> loop_normals;
  MutableSpan<short2> clnors_data;

  /* Read-only. */
  Span<float3> positions;
  Span<int2> edges;
  Span<int> corner_verts;
  Span<int> corner_edges;
  OffsetIndices<int> faces;
  Span<int2> edge_to_loops;
  Span<int> loop_to_face;
  Span<float3> face_normals;
  Span<float3> vert_normals;
};

#define INDEX_UNSET INT_MIN
#define INDEX_INVALID -1
/* See comment about edge_to_loops below. */
#define IS_EDGE_SHARP(_e2l) ELEM((_e2l)[1], INDEX_UNSET, INDEX_INVALID)

static void mesh_edges_sharp_tag(const OffsetIndices<int> faces,
                                 const Span<int> corner_verts,
                                 const Span<int> corner_edges,
                                 const Span<int> loop_to_face_map,
                                 const Span<float3> face_normals,
                                 const Span<bool> sharp_faces,
                                 const Span<bool> sharp_edges,
                                 const bool check_angle,
                                 const float split_angle,
                                 MutableSpan<int2> edge_to_loops,
                                 MutableSpan<bool> r_sharp_edges)
{
  const float split_angle_cos = check_angle ? cosf(split_angle) : -1.0f;
  auto face_is_smooth = [&](const int face_i) {
    return sharp_faces.is_empty() || !sharp_faces[face_i];
  };

  for (const int face_i : faces.index_range()) {
    for (const int loop_index : faces[face_i]) {
      const int vert_i = corner_verts[loop_index];
      const int edge_i = corner_edges[loop_index];

      int2 &e2l = edge_to_loops[edge_i];

      /* Check whether current edge might be smooth or sharp */
      if ((e2l[0] | e2l[1]) == 0) {
        /* 'Empty' edge until now, set e2l[0] (and e2l[1] to INDEX_UNSET to tag it as unset). */
        e2l[0] = loop_index;
        /* We have to check this here too, else we might miss some flat faces!!! */
        e2l[1] = face_is_smooth(face_i) ? INDEX_UNSET : INDEX_INVALID;
      }
      else if (e2l[1] == INDEX_UNSET) {
        const bool is_angle_sharp = (check_angle &&
                                     dot_v3v3(face_normals[loop_to_face_map[e2l[0]]],
                                              face_normals[face_i]) < split_angle_cos);

        /* Second loop using this edge, time to test its sharpness.
         * An edge is sharp if it is tagged as such, or its face is not smooth,
         * or both faces have opposed (flipped) normals, i.e. both loops on the same edge share the
         * same vertex, or angle between both its faces' normals is above split_angle value.
         */
        if (!face_is_smooth(face_i) || (!sharp_edges.is_empty() && sharp_edges[edge_i]) ||
            vert_i == corner_verts[e2l[0]] || is_angle_sharp)
        {
          /* NOTE: we are sure that loop != 0 here ;). */
          e2l[1] = INDEX_INVALID;

          /* We want to avoid tagging edges as sharp when it is already defined as such by
           * other causes than angle threshold. */
          if (!r_sharp_edges.is_empty() && is_angle_sharp) {
            r_sharp_edges[edge_i] = true;
          }
        }
        else {
          e2l[1] = loop_index;
        }
      }
      else if (!IS_EDGE_SHARP(e2l)) {
        /* More than two loops using this edge, tag as sharp if not yet done. */
        e2l[1] = INDEX_INVALID;

        /* We want to avoid tagging edges as sharp when it is already defined as such by
         * other causes than angle threshold. */
        if (!r_sharp_edges.is_empty()) {
          r_sharp_edges[edge_i] = false;
        }
      }
      /* Else, edge is already 'disqualified' (i.e. sharp)! */
    }
  }
}

void edges_sharp_from_angle_set(const OffsetIndices<int> faces,
                                const Span<int> corner_verts,
                                const Span<int> corner_edges,
                                const Span<float3> face_normals,
                                const bool *sharp_faces,
                                const float split_angle,
                                MutableSpan<bool> sharp_edges)
{
  if (split_angle >= float(M_PI)) {
    /* Nothing to do! */
    return;
  }

  /* Mapping edge -> loops. See #bke::mesh::normals_calc_loop for details. */
  Array<int2> edge_to_loops(sharp_edges.size(), int2(0));

  /* Simple mapping from a loop to its face index. */
  const Array<int> loop_to_face = build_loop_to_face_map(faces);

  mesh_edges_sharp_tag(faces,
                       corner_verts,
                       corner_edges,
                       loop_to_face,
                       face_normals,
                       Span<bool>(sharp_faces, sharp_faces ? faces.size() : 0),
                       sharp_edges,
                       true,
                       split_angle,
                       edge_to_loops,
                       sharp_edges);
}

static void loop_manifold_fan_around_vert_next(const Span<int> corner_verts,
                                               const OffsetIndices<int> faces,
                                               const Span<int> loop_to_face,
                                               const int2 e2lfan_curr,
                                               const int vert_pivot,
                                               int *r_mlfan_curr_index,
                                               int *r_mlfan_vert_index)
{
  const int mlfan_curr_orig = *r_mlfan_curr_index;
  const int vert_fan_orig = corner_verts[mlfan_curr_orig];

  /* WARNING: This is rather complex!
   * We have to find our next edge around the vertex (fan mode).
   * First we find the next loop, which is either previous or next to mlfan_curr_index, depending
   * whether both loops using current edge are in the same direction or not, and whether
   * mlfan_curr_index actually uses the vertex we are fanning around!
   * mlfan_curr_index is the index of mlfan_next here, and mlfan_next is not the real next one
   * (i.e. not the future `mlfan_curr`). */
  *r_mlfan_curr_index = (e2lfan_curr[0] == *r_mlfan_curr_index) ? e2lfan_curr[1] : e2lfan_curr[0];

  BLI_assert(*r_mlfan_curr_index >= 0);

  const int vert_fan_next = corner_verts[*r_mlfan_curr_index];
  const IndexRange face_fan_next = faces[loop_to_face[*r_mlfan_curr_index]];
  if ((vert_fan_orig == vert_fan_next && vert_fan_orig == vert_pivot) ||
      !ELEM(vert_fan_orig, vert_fan_next, vert_pivot))
  {
    /* We need the previous loop, but current one is our vertex's loop. */
    *r_mlfan_vert_index = *r_mlfan_curr_index;
    *r_mlfan_curr_index = face_corner_prev(face_fan_next, *r_mlfan_curr_index);
  }
  else {
    /* We need the next loop, which is also our vertex's loop. */
    *r_mlfan_curr_index = face_corner_next(face_fan_next, *r_mlfan_curr_index);
    *r_mlfan_vert_index = *r_mlfan_curr_index;
  }
}

static void lnor_space_for_single_fan(LoopSplitTaskDataCommon *common_data,
                                      const int ml_curr_index,
                                      const int space_index)
{
  const Span<int> loop_to_face = common_data->loop_to_face;
  const Span<float3> face_normals = common_data->face_normals;
  MutableSpan<float3> loop_normals = common_data->loop_normals;

  loop_normals[ml_curr_index] = face_normals[loop_to_face[ml_curr_index]];

  if (CornerNormalSpaceArray *lnors_spacearr = common_data->lnors_spacearr) {
    const Span<float3> positions = common_data->positions;
    const Span<int2> edges = common_data->edges;
    const OffsetIndices faces = common_data->faces;
    const Span<int> corner_verts = common_data->corner_verts;
    const Span<int> corner_edges = common_data->corner_edges;
    const Span<short2> clnors_data = common_data->clnors_data;

    float3 vec_curr;
    float3 vec_prev;
    const int face_index = loop_to_face[ml_curr_index];
    const int ml_prev_index = mesh::face_corner_prev(faces[face_index], ml_curr_index);

    /* The vertex we are "fanning" around. */
    const int vert_pivot = corner_verts[ml_curr_index];
    const int vert_2 = edge_other_vert(edges[corner_edges[ml_curr_index]], vert_pivot);
    const int vert_3 = edge_other_vert(edges[corner_edges[ml_prev_index]], vert_pivot);

    sub_v3_v3v3(vec_curr, positions[vert_2], positions[vert_pivot]);
    normalize_v3(vec_curr);
    sub_v3_v3v3(vec_prev, positions[vert_3], positions[vert_pivot]);
    normalize_v3(vec_prev);

    CornerNormalSpace &lnor_space = lnors_spacearr->spaces[space_index];
    lnor_space = lnor_space_define(loop_normals[ml_curr_index], vec_curr, vec_prev, {});
    lnors_spacearr->corner_space_indices[ml_curr_index] = space_index;

    if (!clnors_data.is_empty()) {
      loop_normals[ml_curr_index] = lnor_space_custom_data_to_normal(lnor_space,
                                                                     clnors_data[ml_curr_index]);
    }

    if (!lnors_spacearr->corners_by_space.is_empty()) {
      lnors_spacearr->corners_by_space[space_index] = {ml_curr_index};
    }
  }
}

static void split_loop_nor_fan_do(LoopSplitTaskDataCommon *common_data,
                                  const int ml_curr_index,
                                  const int space_index,
                                  Vector<float3> *edge_vectors)
{
  CornerNormalSpaceArray *lnors_spacearr = common_data->lnors_spacearr;
  MutableSpan<float3> loop_normals = common_data->loop_normals;
  MutableSpan<short2> clnors_data = common_data->clnors_data;

  const Span<float3> positions = common_data->positions;
  const Span<int2> edges = common_data->edges;
  const OffsetIndices faces = common_data->faces;
  const Span<int> corner_verts = common_data->corner_verts;
  const Span<int> corner_edges = common_data->corner_edges;
  const Span<int2> edge_to_loops = common_data->edge_to_loops;
  const Span<int> loop_to_face = common_data->loop_to_face;
  const Span<float3> face_normals = common_data->face_normals;

  const int face_index = loop_to_face[ml_curr_index];
  const int ml_prev_index = face_corner_prev(faces[face_index], ml_curr_index);

  /* Sigh! we have to fan around current vertex, until we find the other non-smooth edge,
   * and accumulate face normals into the vertex!
   * Note in case this vertex has only one sharp edges, this is a waste because the normal is the
   * same as the vertex normal, but I do not see any easy way to detect that (would need to count
   * number of sharp edges per vertex, I doubt the additional memory usage would be worth it,
   * especially as it should not be a common case in real-life meshes anyway). */
  const int vert_pivot = corner_verts[ml_curr_index]; /* The vertex we are "fanning" around! */

  /* `ml_curr_index` would be mlfan_prev if we needed that one. */
  const int2 &edge_orig = edges[corner_edges[ml_curr_index]];

  float3 vec_curr;
  float3 vec_prev;
  float3 vec_org;
  float3 lnor(0.0f);

  /* We validate clnors data on the fly - cheapest way to do! */
  int2 clnors_avg(0);
  const short2 *clnor_ref = nullptr;
  int clnors_count = 0;
  bool clnors_invalid = false;

  Vector<int, 8> processed_corners;

  /* `mlfan_vert_index` the loop of our current edge might not be the loop of our current vertex!
   */
  int mlfan_curr_index = ml_prev_index;
  int mlfan_vert_index = ml_curr_index;

  BLI_assert(mlfan_curr_index >= 0);
  BLI_assert(mlfan_vert_index >= 0);

  /* Only need to compute previous edge's vector once, then we can just reuse old current one! */
  {
    const int vert_2 = edge_other_vert(edge_orig, vert_pivot);
    sub_v3_v3v3(vec_org, positions[vert_2], positions[vert_pivot]);
    normalize_v3(vec_org);
    copy_v3_v3(vec_prev, vec_org);

    if (lnors_spacearr) {
      edge_vectors->append(vec_org);
    }
  }

  // printf("FAN: vert %d, start edge %d\n", vert_pivot, ml_curr->e);

  while (true) {
    const int2 &edge = edges[corner_edges[mlfan_curr_index]];
    /* Compute edge vectors.
     * NOTE: We could pre-compute those into an array, in the first iteration, instead of computing
     *       them twice (or more) here. However, time gained is not worth memory and time lost,
     *       given the fact that this code should not be called that much in real-life meshes.
     */
    {
      const int vert_2 = edge_other_vert(edge, vert_pivot);
      sub_v3_v3v3(vec_curr, positions[vert_2], positions[vert_pivot]);
      normalize_v3(vec_curr);
    }

    // printf("\thandling edge %d / loop %d\n", corner_edges[mlfan_curr_index], mlfan_curr_index);

    /* Code similar to accumulate_vertex_normals_poly_v3. */
    /* Calculate angle between the two face edges incident on this vertex. */
    lnor += face_normals[loop_to_face[mlfan_curr_index]] * saacos(math::dot(vec_curr, vec_prev));

    if (!clnors_data.is_empty()) {
      /* Accumulate all clnors, if they are not all equal we have to fix that! */
      const short2 &clnor = clnors_data[mlfan_vert_index];
      if (clnors_count) {
        clnors_invalid |= *clnor_ref != clnor;
      }
      else {
        clnor_ref = &clnor;
      }
      clnors_avg += int2(clnor);
      clnors_count++;
    }

    processed_corners.append(mlfan_vert_index);

    if (lnors_spacearr) {
      if (edge != edge_orig) {
        /* We store here all edges-normalized vectors processed. */
        edge_vectors->append(vec_curr);
      }
      if (!lnors_spacearr->corners_by_space.is_empty()) {
        lnors_spacearr->corners_by_space[space_index] = processed_corners.as_span();
      }
    }

    if (IS_EDGE_SHARP(edge_to_loops[corner_edges[mlfan_curr_index]]) || (edge == edge_orig)) {
      /* Current edge is sharp and we have finished with this fan of faces around this vert,
       * or this vert is smooth, and we have completed a full turn around it. */
      break;
    }

    vec_prev = vec_curr;

    /* Find next loop of the smooth fan. */
    loop_manifold_fan_around_vert_next(corner_verts,
                                       faces,
                                       loop_to_face,
                                       edge_to_loops[corner_edges[mlfan_curr_index]],
                                       vert_pivot,
                                       &mlfan_curr_index,
                                       &mlfan_vert_index);
  }

  float length;
  lnor = math::normalize_and_get_length(lnor, length);

  /* If we are generating lnor spacearr, we can now define the one for this fan,
   * and optionally compute final lnor from custom data too!
   */
  if (lnors_spacearr) {
    if (UNLIKELY(length == 0.0f)) {
      /* Use vertex normal as fallback! */
      lnor = loop_normals[mlfan_vert_index];
      length = 1.0f;
    }

    CornerNormalSpace &lnor_space = lnors_spacearr->spaces[space_index];
    lnor_space = lnor_space_define(lnor, vec_org, vec_curr, *edge_vectors);
    lnors_spacearr->corner_space_indices.as_mutable_span().fill_indices(
        processed_corners.as_span(), space_index);
    edge_vectors->clear();

    if (!clnors_data.is_empty()) {
      if (clnors_invalid) {
        clnors_avg /= clnors_count;
        /* Fix/update all clnors of this fan with computed average value. */
        if (G.debug & G_DEBUG) {
          printf("Invalid clnors in this fan!\n");
        }
        clnors_data.fill_indices(processed_corners.as_span(), short2(clnors_avg));
      }
      /* Extra bonus: since small-stack is local to this function,
       * no more need to empty it at all cost! */

      lnor = lnor_space_custom_data_to_normal(lnor_space, *clnor_ref);
    }
  }

  /* In case we get a zero normal here, just use vertex normal already set! */
  if (LIKELY(length != 0.0f)) {
    /* Copy back the final computed normal into all related loop-normals. */
    loop_normals.fill_indices(processed_corners.as_span(), lnor);
  }
}

/**
 * Check whether given loop is part of an unknown-so-far cyclic smooth fan, or not.
 * Needed because cyclic smooth fans have no obvious 'entry point',
 * and yet we need to walk them once, and only once.
 */
static bool loop_split_generator_check_cyclic_smooth_fan(const Span<int> corner_verts,
                                                         const Span<int> corner_edges,
                                                         const OffsetIndices<int> faces,
                                                         const Span<int2> edge_to_loops,
                                                         const Span<int> loop_to_face,
                                                         const int2 e2l_prev,
                                                         MutableBitSpan skip_loops,
                                                         const int ml_curr_index,
                                                         const int ml_prev_index)
{
  /* The vertex we are "fanning" around. */
  const int vert_pivot = corner_verts[ml_curr_index];

  int2 e2lfan_curr = e2l_prev;
  if (IS_EDGE_SHARP(e2lfan_curr)) {
    /* Sharp loop, so not a cyclic smooth fan. */
    return false;
  }

  /* `mlfan_vert_index` the loop of our current edge might not be the loop of our current vertex!
   */
  int mlfan_curr_index = ml_prev_index;
  int mlfan_vert_index = ml_curr_index;

  BLI_assert(mlfan_curr_index >= 0);
  BLI_assert(mlfan_vert_index >= 0);

  BLI_assert(!skip_loops[mlfan_vert_index]);
  skip_loops[mlfan_vert_index].set();

  while (true) {
    /* Find next loop of the smooth fan. */
    loop_manifold_fan_around_vert_next(corner_verts,
                                       faces,
                                       loop_to_face,
                                       e2lfan_curr,
                                       vert_pivot,
                                       &mlfan_curr_index,
                                       &mlfan_vert_index);

    e2lfan_curr = edge_to_loops[corner_edges[mlfan_curr_index]];

    if (IS_EDGE_SHARP(e2lfan_curr)) {
      /* Sharp loop/edge, so not a cyclic smooth fan. */
      return false;
    }
    /* Smooth loop/edge. */
    if (skip_loops[mlfan_vert_index]) {
      if (mlfan_vert_index == ml_curr_index) {
        /* We walked around a whole cyclic smooth fan without finding any already-processed loop,
         * means we can use initial current / previous edge as start for this smooth fan. */
        return true;
      }
      /* Already checked in some previous looping, we can abort. */
      return false;
    }

    /* We can skip it in future, and keep checking the smooth fan. */
    skip_loops[mlfan_vert_index].set();
  }
}

static void loop_split_generator(LoopSplitTaskDataCommon *common_data,
                                 Vector<int> &r_single_corners,
                                 Vector<int> &r_fan_corners)
{
  const Span<int> corner_verts = common_data->corner_verts;
  const Span<int> corner_edges = common_data->corner_edges;
  const OffsetIndices faces = common_data->faces;
  const Span<int> loop_to_face = common_data->loop_to_face;
  const Span<int2> edge_to_loops = common_data->edge_to_loops;

  BitVector<> skip_loops(corner_verts.size(), false);

#ifdef DEBUG_TIME
  SCOPED_TIMER_AVERAGED(__func__);
#endif

  /* We now know edges that can be smoothed (with their vector, and their two loops),
   * and edges that will be hard! Now, time to generate the normals.
   */
  for (const int face_index : faces.index_range()) {
    const IndexRange face = faces[face_index];

    for (const int ml_curr_index : face) {
      const int ml_prev_index = mesh::face_corner_prev(face, ml_curr_index);

#if 0
      printf("Checking loop %d / edge %u / vert %u (sharp edge: %d, skiploop: %d)",
             ml_curr_index,
             corner_edges[ml_curr_index],
             corner_verts[ml_curr_index],
             IS_EDGE_SHARP(edge_to_loops[corner_edges[ml_curr_index]]),
             skip_loops[ml_curr_index]);
#endif

      /* A smooth edge, we have to check for cyclic smooth fan case.
       * If we find a new, never-processed cyclic smooth fan, we can do it now using that loop/edge
       * as 'entry point', otherwise we can skip it. */

      /* NOTE: In theory, we could make #loop_split_generator_check_cyclic_smooth_fan() store
       * mlfan_vert_index'es and edge indexes in two stacks, to avoid having to fan again around
       * the vert during actual computation of `clnor` & `clnorspace`.
       * However, this would complicate the code, add more memory usage, and despite its logical
       * complexity, #loop_manifold_fan_around_vert_next() is quite cheap in term of CPU cycles,
       * so really think it's not worth it. */
      if (!IS_EDGE_SHARP(edge_to_loops[corner_edges[ml_curr_index]]) &&
          (skip_loops[ml_curr_index] || !loop_split_generator_check_cyclic_smooth_fan(
                                            corner_verts,
                                            corner_edges,
                                            faces,
                                            edge_to_loops,
                                            loop_to_face,
                                            edge_to_loops[corner_edges[ml_prev_index]],
                                            skip_loops,
                                            ml_curr_index,
                                            ml_prev_index)))
      {
        // printf("SKIPPING!\n");
      }
      else {
        if (IS_EDGE_SHARP(edge_to_loops[corner_edges[ml_curr_index]]) &&
            IS_EDGE_SHARP(edge_to_loops[corner_edges[ml_prev_index]]))
        {
          /* Simple case (both edges around that vertex are sharp in current face),
           * this corner just takes its face normal. */
          r_single_corners.append(ml_curr_index);
        }
        else {
          /* We do not need to check/tag loops as already computed. Due to the fact that a loop
           * only points to one of its two edges, the same fan will never be walked more than once.
           * Since we consider edges that have neighbor faces with inverted (flipped) normals as
           * sharp, we are sure that no fan will be skipped, even only considering the case (sharp
           * current edge, smooth previous edge), and not the alternative (smooth current edge,
           * sharp previous edge). All this due/thanks to the link between normals and loop
           * ordering (i.e. winding). */
          r_fan_corners.append(ml_curr_index);
        }
      }
    }
  }
}

void normals_calc_loop(const Span<float3> vert_positions,
                       const Span<int2> edges,
                       const OffsetIndices<int> faces,
                       const Span<int> corner_verts,
                       const Span<int> corner_edges,
                       const Span<int> loop_to_face_map,
                       const Span<float3> vert_normals,
                       const Span<float3> face_normals,
                       const bool *sharp_edges,
                       const bool *sharp_faces,
                       bool use_split_normals,
                       float split_angle,
                       short2 *clnors_data,
                       CornerNormalSpaceArray *r_lnors_spacearr,
                       MutableSpan<float3> r_loop_normals)
{
  /* For now this is not supported.
   * If we do not use split normals, we do not generate anything fancy! */
  BLI_assert(use_split_normals || !(r_lnors_spacearr));

  if (!use_split_normals) {
    /* In this case, simply fill `r_loop_normals` with `vert_normals`
     * (or `face_normals` for flat faces), quite simple!
     * Note this is done here to keep some logic and consistency in this quite complex code,
     * since we may want to use loop_normals even when mesh's 'autosmooth' is disabled
     * (see e.g. mesh mapping code). As usual, we could handle that on case-by-case basis,
     * but simpler to keep it well confined here. */
    for (const int face_index : faces.index_range()) {
      const bool is_face_flat = sharp_faces && sharp_faces[face_index];
      for (const int corner : faces[face_index]) {
        if (is_face_flat) {
          copy_v3_v3(r_loop_normals[corner], face_normals[face_index]);
        }
        else {
          copy_v3_v3(r_loop_normals[corner], vert_normals[corner_verts[corner]]);
        }
      }
    }
    return;
  }

  /**
   * Mapping edge -> loops.
   * If that edge is used by more than two loops (faces),
   * it is always sharp (and tagged as such, see below).
   * We also use the second loop index as a kind of flag:
   *
   * - smooth edge: > 0.
   * - sharp edge: < 0 (INDEX_INVALID || INDEX_UNSET).
   * - unset: INDEX_UNSET.
   *
   * Note that currently we only have two values for second loop of sharp edges.
   * However, if needed, we can store the negated value of loop index instead of INDEX_INVALID
   * to retrieve the real value later in code).
   * Note also that loose edges always have both values set to 0! */
  Array<int2> edge_to_loops(edges.size(), int2(0));

  /* Simple mapping from a loop to its face index. */
  Span<int> loop_to_face;
  Array<int> local_loop_to_face_map;
  if (loop_to_face_map.is_empty()) {
    local_loop_to_face_map = build_loop_to_face_map(faces);
    loop_to_face = local_loop_to_face_map;
  }
  else {
    loop_to_face = loop_to_face_map;
  }

  /* When using custom loop normals, disable the angle feature! */
  const bool check_angle = (split_angle < float(M_PI)) && (clnors_data == nullptr);

  CornerNormalSpaceArray _lnors_spacearr;

#ifdef DEBUG_TIME
  SCOPED_TIMER_AVERAGED(__func__);
#endif

  if (!r_lnors_spacearr && clnors_data) {
    /* We need to compute lnor spacearr if some custom lnor data are given to us! */
    r_lnors_spacearr = &_lnors_spacearr;
  }

  /* Init data common to all tasks. */
  LoopSplitTaskDataCommon common_data;
  common_data.lnors_spacearr = r_lnors_spacearr;
  common_data.loop_normals = r_loop_normals;
  common_data.clnors_data = {reinterpret_cast<short2 *>(clnors_data),
                             clnors_data ? corner_verts.size() : 0};
  common_data.positions = vert_positions;
  common_data.edges = edges;
  common_data.faces = faces;
  common_data.corner_verts = corner_verts;
  common_data.corner_edges = corner_edges;
  common_data.edge_to_loops = edge_to_loops;
  common_data.loop_to_face = loop_to_face;
  common_data.face_normals = face_normals;
  common_data.vert_normals = vert_normals;

  /* Pre-populate all loop normals as if their verts were all smooth.
   * This way we don't have to compute those later! */
  array_utils::gather(vert_normals, corner_verts, r_loop_normals, 1024);

  /* This first loop check which edges are actually smooth, and compute edge vectors. */
  mesh_edges_sharp_tag(faces,
                       corner_verts,
                       corner_edges,
                       loop_to_face,
                       face_normals,
                       Span<bool>(sharp_faces, sharp_faces ? faces.size() : 0),
                       Span<bool>(sharp_edges, sharp_edges ? edges.size() : 0),
                       check_angle,
                       split_angle,
                       edge_to_loops,
                       {});

  Vector<int> single_corners;
  Vector<int> fan_corners;
  loop_split_generator(&common_data, single_corners, fan_corners);

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
    Vector<float3> edge_vectors;
    for (const int i : range) {
      const int corner = fan_corners[i];
      const int space_index = single_corners.size() + i;
      split_loop_nor_fan_do(&common_data, corner, space_index, &edge_vectors);
    }
  });
}

#undef INDEX_UNSET
#undef INDEX_INVALID
#undef IS_EDGE_SHARP

/**
 * Compute internal representation of given custom normals (as an array of float[2]).
 * It also makes sure the mesh matches those custom normals, by setting sharp edges flag as needed
 * to get a same custom lnor for all loops sharing a same smooth fan.
 * If use_vertices if true, r_custom_loop_normals is assumed to be per-vertex, not per-loop
 * (this allows to set whole vert's normals at once, useful in some cases).
 * r_custom_loop_normals is expected to have normalized normals, or zero ones,
 * in which case they will be replaced by default loop/vertex normal.
 */

static void mesh_normals_loop_custom_set(Span<float3> positions,
                                         Span<int2> edges,
                                         const OffsetIndices<int> faces,
                                         Span<int> corner_verts,
                                         Span<int> corner_edges,
                                         Span<float3> vert_normals,
                                         Span<float3> face_normals,
                                         const bool *sharp_faces,
                                         const bool use_vertices,
                                         MutableSpan<float3> r_custom_loop_normals,
                                         MutableSpan<bool> sharp_edges,
                                         MutableSpan<short2> r_clnors_data)
{
  /* We *may* make that poor #bke::mesh::normals_calc_loop() even more complex by making it
   * handling that feature too, would probably be more efficient in absolute. However, this
   * function *is not* performance-critical, since it is mostly expected to be called by IO add-ons
   * when importing custom normals, and modifier (and perhaps from some editing tools later?). So
   * better to keep some simplicity here, and just call #bke::mesh::normals_calc_loop() twice! */
  CornerNormalSpaceArray lnors_spacearr;
  lnors_spacearr.create_corners_by_space = true;
  BitVector<> done_loops(corner_verts.size(), false);
  Array<float3> loop_normals(corner_verts.size());
  const Array<int> loop_to_face = build_loop_to_face_map(faces);
  /* In this case we always consider split nors as ON,
   * and do not want to use angle to define smooth fans! */
  const bool use_split_normals = true;
  const float split_angle = float(M_PI);

  /* Compute current lnor spacearr. */
  normals_calc_loop(positions,
                    edges,
                    faces,
                    corner_verts,
                    corner_edges,
                    loop_to_face,
                    vert_normals,
                    face_normals,
                    sharp_edges.data(),
                    sharp_faces,
                    use_split_normals,
                    split_angle,
                    r_clnors_data.data(),
                    &lnors_spacearr,
                    loop_normals);

  /* Set all given zero vectors to their default value. */
  if (use_vertices) {
    for (const int i : positions.index_range()) {
      if (is_zero_v3(r_custom_loop_normals[i])) {
        copy_v3_v3(r_custom_loop_normals[i], vert_normals[i]);
      }
    }
  }
  else {
    for (const int i : corner_verts.index_range()) {
      if (is_zero_v3(r_custom_loop_normals[i])) {
        copy_v3_v3(r_custom_loop_normals[i], loop_normals[i]);
      }
    }
  }

  /* Now, check each current smooth fan (one lnor space per smooth fan!),
   * and if all its matching custom loop_normals are not (enough) equal, add sharp edges as needed.
   * This way, next time we run bke::mesh::normals_calc_loop(), we'll get lnor spacearr/smooth fans
   * matching given custom loop_normals.
   * Note this code *will never* unsharp edges! And quite obviously,
   * when we set custom normals per vertices, running this is absolutely useless. */
  if (use_vertices) {
    done_loops.fill(true);
  }
  else {
    for (const int i : corner_verts.index_range()) {
      if (lnors_spacearr.corner_space_indices[i] == -1) {
        /* This should not happen in theory, but in some rare case (probably ugly geometry)
         * we can get some missing loopspacearr at this point. :/
         * Maybe we should set those loops' edges as sharp? */
        done_loops[i].set();
        if (G.debug & G_DEBUG) {
          printf("WARNING! Getting invalid nullptr loop space for loop %d!\n", i);
        }
        continue;
      }
      if (done_loops[i]) {
        continue;
      }

      const int space_index = lnors_spacearr.corner_space_indices[i];
      const Span<int> fan_corners = lnors_spacearr.corners_by_space[space_index];

      /* Notes:
       * - In case of mono-loop smooth fan, we have nothing to do.
       * - Loops in this linklist are ordered (in reversed order compared to how they were
       *   discovered by bke::mesh::normals_calc_loop(), but this is not a problem).
       *   Which means if we find a mismatching clnor,
       *   we know all remaining loops will have to be in a new, different smooth fan/lnor space.
       * - In smooth fan case, we compare each clnor against a ref one,
       *   to avoid small differences adding up into a real big one in the end!
       */
      if (fan_corners.is_empty()) {
        done_loops[i].set();
        continue;
      }

      int prev_corner = -1;
      const float *org_nor = nullptr;

      for (const int lidx : fan_corners) {
        float *nor = r_custom_loop_normals[lidx];

        if (!org_nor) {
          org_nor = nor;
        }
        else if (dot_v3v3(org_nor, nor) < LNOR_SPACE_TRIGO_THRESHOLD) {
          /* Current normal differs too much from org one, we have to tag the edge between
           * previous loop's face and current's one as sharp.
           * We know those two loops do not point to the same edge,
           * since we do not allow reversed winding in a same smooth fan. */
          const IndexRange face = faces[loop_to_face[lidx]];
          const int mlp = (lidx == face.start()) ? face.start() + face.size() - 1 : lidx - 1;
          const int edge = corner_edges[lidx];
          const int edge_p = corner_edges[mlp];
          const int prev_edge = corner_edges[prev_corner];
          sharp_edges[prev_edge == edge_p ? prev_edge : edge] = true;

          org_nor = nor;
        }

        prev_corner = lidx;
        done_loops[lidx].set();
      }

      /* We also have to check between last and first loops,
       * otherwise we may miss some sharp edges here!
       * This is just a simplified version of above while loop.
       * See #45984. */
      if (fan_corners.size() > 1 && org_nor) {
        const int lidx = fan_corners.last();
        float *nor = r_custom_loop_normals[lidx];

        if (dot_v3v3(org_nor, nor) < LNOR_SPACE_TRIGO_THRESHOLD) {
          const IndexRange face = faces[loop_to_face[lidx]];
          const int mlp = (lidx == face.start()) ? face.start() + face.size() - 1 : lidx - 1;
          const int edge = corner_edges[lidx];
          const int edge_p = corner_edges[mlp];
          const int prev_edge = corner_edges[prev_corner];
          sharp_edges[prev_edge == edge_p ? prev_edge : edge] = true;
        }
      }
    }

    /* And now, recompute our new auto `loop_normals` and lnor spacearr! */
    normals_calc_loop(positions,
                      edges,
                      faces,
                      corner_verts,
                      corner_edges,
                      loop_to_face,
                      vert_normals,
                      face_normals,
                      sharp_edges.data(),
                      sharp_faces,
                      use_split_normals,
                      split_angle,
                      r_clnors_data.data(),
                      &lnors_spacearr,
                      loop_normals);
  }

  /* And we just have to convert plain object-space custom normals to our
   * lnor space-encoded ones. */
  for (const int i : corner_verts.index_range()) {
    if (lnors_spacearr.corner_space_indices[i] == -1) {
      done_loops[i].reset();
      if (G.debug & G_DEBUG) {
        printf("WARNING! Still getting invalid nullptr loop space in second loop for loop %d!\n",
               i);
      }
      continue;
    }
    if (!done_loops[i]) {
      continue;
    }

    const int space_index = lnors_spacearr.corner_space_indices[i];
    const Span<int> fan_corners = lnors_spacearr.corners_by_space[space_index];

    /* Note we accumulate and average all custom normals in current smooth fan,
     * to avoid getting different clnors data (tiny differences in plain custom normals can
     * give rather huge differences in computed 2D factors). */
    if (fan_corners.size() < 2) {
      const int nidx = use_vertices ? corner_verts[i] : i;
      r_clnors_data[i] = lnor_space_custom_normal_to_data(lnors_spacearr.spaces[space_index],
                                                          r_custom_loop_normals[nidx]);
      done_loops[i].reset();
    }
    else {
      float3 avg_nor(0.0f);
      for (const int lidx : fan_corners) {
        const int nidx = use_vertices ? corner_verts[lidx] : lidx;
        avg_nor += r_custom_loop_normals[nidx];
        done_loops[lidx].reset();
      }

      mul_v3_fl(avg_nor, 1.0f / float(fan_corners.size()));
      short2 clnor_data_tmp = lnor_space_custom_normal_to_data(lnors_spacearr.spaces[space_index],
                                                               avg_nor);

      r_clnors_data.fill_indices(fan_corners, clnor_data_tmp);
    }
  }
}

void normals_loop_custom_set(const Span<float3> vert_positions,
                             const Span<int2> edges,
                             const OffsetIndices<int> faces,
                             const Span<int> corner_verts,
                             const Span<int> corner_edges,
                             const Span<float3> vert_normals,
                             const Span<float3> face_normals,
                             const bool *sharp_faces,
                             MutableSpan<bool> sharp_edges,
                             MutableSpan<float3> r_custom_loop_normals,
                             MutableSpan<short2> r_clnors_data)
{
  mesh_normals_loop_custom_set(vert_positions,
                               edges,
                               faces,
                               corner_verts,
                               corner_edges,
                               vert_normals,
                               face_normals,
                               sharp_faces,
                               false,
                               r_custom_loop_normals,
                               sharp_edges,
                               r_clnors_data);
}

void normals_loop_custom_set_from_verts(const Span<float3> vert_positions,
                                        const Span<int2> edges,
                                        const OffsetIndices<int> faces,
                                        const Span<int> corner_verts,
                                        const Span<int> corner_edges,
                                        const Span<float3> vert_normals,
                                        const Span<float3> face_normals,
                                        const bool *sharp_faces,
                                        MutableSpan<bool> sharp_edges,
                                        MutableSpan<float3> r_custom_vert_normals,
                                        MutableSpan<short2> r_clnors_data)
{
  mesh_normals_loop_custom_set(vert_positions,
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
      CustomData_get_layer_for_write(&mesh->loop_data, CD_CUSTOMLOOPNORMAL, mesh->totloop));
  if (clnors != nullptr) {
    memset(clnors, 0, sizeof(*clnors) * mesh->totloop);
  }
  else {
    clnors = static_cast<short2 *>(CustomData_add_layer(
        &mesh->loop_data, CD_CUSTOMLOOPNORMAL, CD_SET_DEFAULT, mesh->totloop));
  }
  MutableAttributeAccessor attributes = mesh->attributes_for_write();
  SpanAttributeWriter<bool> sharp_edges = attributes.lookup_or_add_for_write_span<bool>(
      "sharp_edge", ATTR_DOMAIN_EDGE);
  const bool *sharp_faces = static_cast<const bool *>(
      CustomData_get_layer_named(&mesh->face_data, CD_PROP_BOOL, "sharp_face"));

  mesh_normals_loop_custom_set(
      mesh->vert_positions(),
      mesh->edges(),
      mesh->faces(),
      mesh->corner_verts(),
      mesh->corner_edges(),
      mesh->vert_normals(),
      mesh->face_normals(),
      sharp_faces,
      use_vertices,
      {reinterpret_cast<float3 *>(r_custom_nors), use_vertices ? mesh->totvert : mesh->totloop},
      sharp_edges.span,
      {clnors, mesh->totloop});

  sharp_edges.finish();
}

}  // namespace blender::bke::mesh

void BKE_mesh_set_custom_normals(Mesh *mesh, float (*r_custom_loop_normals)[3])
{
  blender::bke::mesh::mesh_set_custom_normals(mesh, r_custom_loop_normals, false);
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
