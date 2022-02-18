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
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 */

/** \file
 * \ingroup bke
 *
 * This file contains code for polygon tessellation
 * (creating triangles from polygons).
 *
 * \see bmesh_mesh_tessellate.c for the #BMesh equivalent of this file.
 */

#include <limits.h>

#include "MEM_guardedalloc.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BLI_math.h"
#include "BLI_memarena.h"
#include "BLI_polyfill_2d.h"
#include "BLI_task.h"
#include "BLI_utildefines.h"

#include "BKE_customdata.h"
#include "BKE_mesh.h" /* Own include. */

#include "BLI_strict_flags.h"

/** Compared against total loops. */
#define MESH_FACE_TESSELLATE_THREADED_LIMIT 4096

/* -------------------------------------------------------------------- */
/** \name MFace Tessellation
 *
 * #MFace is a legacy data-structure that should be avoided, use #MLoopTri instead.
 * \{ */

/**
 * Convert all CD layers from loop/poly to tessface data.
 *
 * \param loopindices: is an array of an int[4] per tessface,
 * mapping tessface's verts to loops indices.
 *
 * \note when mface is not NULL, mface[face_index].v4
 * is used to test quads, else, loopindices[face_index][3] is used.
 */
static void mesh_loops_to_tessdata(CustomData *fdata,
                                   CustomData *ldata,
                                   MFace *mface,
                                   const int *polyindices,
                                   uint (*loopindices)[4],
                                   const int num_faces)
{
  /* NOTE(mont29): performances are sub-optimal when we get a NULL #MFace,
   * we could be ~25% quicker with dedicated code.
   * The issue is, unless having two different functions with nearly the same code,
   * there's not much ways to solve this. Better IMHO to live with it for now (sigh). */
  const int numUV = CustomData_number_of_layers(ldata, CD_MLOOPUV);
  const int numCol = CustomData_number_of_layers(ldata, CD_MLOOPCOL);
  const bool hasPCol = CustomData_has_layer(ldata, CD_PREVIEW_MLOOPCOL);
  const bool hasOrigSpace = CustomData_has_layer(ldata, CD_ORIGSPACE_MLOOP);
  const bool hasLoopNormal = CustomData_has_layer(ldata, CD_NORMAL);
  const bool hasLoopTangent = CustomData_has_layer(ldata, CD_TANGENT);
  int findex, i, j;
  const int *pidx;
  uint(*lidx)[4];

  for (i = 0; i < numUV; i++) {
    MTFace *texface = CustomData_get_layer_n(fdata, CD_MTFACE, i);
    MLoopUV *mloopuv = CustomData_get_layer_n(ldata, CD_MLOOPUV, i);

    for (findex = 0, pidx = polyindices, lidx = loopindices; findex < num_faces;
         pidx++, lidx++, findex++, texface++) {
      for (j = (mface ? mface[findex].v4 : (*lidx)[3]) ? 4 : 3; j--;) {
        copy_v2_v2(texface->uv[j], mloopuv[(*lidx)[j]].uv);
      }
    }
  }

  for (i = 0; i < numCol; i++) {
    MCol(*mcol)[4] = CustomData_get_layer_n(fdata, CD_MCOL, i);
    MLoopCol *mloopcol = CustomData_get_layer_n(ldata, CD_MLOOPCOL, i);

    for (findex = 0, lidx = loopindices; findex < num_faces; lidx++, findex++, mcol++) {
      for (j = (mface ? mface[findex].v4 : (*lidx)[3]) ? 4 : 3; j--;) {
        MESH_MLOOPCOL_TO_MCOL(&mloopcol[(*lidx)[j]], &(*mcol)[j]);
      }
    }
  }

  if (hasPCol) {
    MCol(*mcol)[4] = CustomData_get_layer(fdata, CD_PREVIEW_MCOL);
    MLoopCol *mloopcol = CustomData_get_layer(ldata, CD_PREVIEW_MLOOPCOL);

    for (findex = 0, lidx = loopindices; findex < num_faces; lidx++, findex++, mcol++) {
      for (j = (mface ? mface[findex].v4 : (*lidx)[3]) ? 4 : 3; j--;) {
        MESH_MLOOPCOL_TO_MCOL(&mloopcol[(*lidx)[j]], &(*mcol)[j]);
      }
    }
  }

  if (hasOrigSpace) {
    OrigSpaceFace *of = CustomData_get_layer(fdata, CD_ORIGSPACE);
    OrigSpaceLoop *lof = CustomData_get_layer(ldata, CD_ORIGSPACE_MLOOP);

    for (findex = 0, lidx = loopindices; findex < num_faces; lidx++, findex++, of++) {
      for (j = (mface ? mface[findex].v4 : (*lidx)[3]) ? 4 : 3; j--;) {
        copy_v2_v2(of->uv[j], lof[(*lidx)[j]].uv);
      }
    }
  }

  if (hasLoopNormal) {
    short(*fnors)[4][3] = CustomData_get_layer(fdata, CD_TESSLOOPNORMAL);
    float(*lnors)[3] = CustomData_get_layer(ldata, CD_NORMAL);

    for (findex = 0, lidx = loopindices; findex < num_faces; lidx++, findex++, fnors++) {
      for (j = (mface ? mface[findex].v4 : (*lidx)[3]) ? 4 : 3; j--;) {
        normal_float_to_short_v3((*fnors)[j], lnors[(*lidx)[j]]);
      }
    }
  }

  if (hasLoopTangent) {
    /* Need to do for all UV maps at some point. */
    float(*ftangents)[4] = CustomData_get_layer(fdata, CD_TANGENT);
    float(*ltangents)[4] = CustomData_get_layer(ldata, CD_TANGENT);

    for (findex = 0, pidx = polyindices, lidx = loopindices; findex < num_faces;
         pidx++, lidx++, findex++) {
      int nverts = (mface ? mface[findex].v4 : (*lidx)[3]) ? 4 : 3;
      for (j = nverts; j--;) {
        copy_v4_v4(ftangents[findex * 4 + j], ltangents[(*lidx)[j]]);
      }
    }
  }
}

int BKE_mesh_tessface_calc_ex(CustomData *fdata,
                              CustomData *ldata,
                              CustomData *pdata,
                              MVert *mvert,
                              int totface,
                              int totloop,
                              int totpoly)
{
#define USE_TESSFACE_SPEEDUP
#define USE_TESSFACE_QUADS

/* We abuse #MFace.edcode to tag quad faces. See below for details. */
#define TESSFACE_IS_QUAD 1

  const int looptri_num = poly_to_tri_count(totpoly, totloop);

  MPoly *mp, *mpoly;
  MLoop *ml, *mloop;
  MFace *mface, *mf;
  MemArena *arena = NULL;
  int *mface_to_poly_map;
  uint(*lindices)[4];
  int poly_index, mface_index;
  uint j;

  mpoly = CustomData_get_layer(pdata, CD_MPOLY);
  mloop = CustomData_get_layer(ldata, CD_MLOOP);

  /* Allocate the length of `totfaces`, avoid many small reallocation's,
   * if all faces are triangles it will be correct, `quads == 2x` allocations. */
  /* Take care since memory is _not_ zeroed so be sure to initialize each field. */
  mface_to_poly_map = MEM_malloc_arrayN((size_t)looptri_num, sizeof(*mface_to_poly_map), __func__);
  mface = MEM_malloc_arrayN((size_t)looptri_num, sizeof(*mface), __func__);
  lindices = MEM_malloc_arrayN((size_t)looptri_num, sizeof(*lindices), __func__);

  mface_index = 0;
  mp = mpoly;
  for (poly_index = 0; poly_index < totpoly; poly_index++, mp++) {
    const uint mp_loopstart = (uint)mp->loopstart;
    const uint mp_totloop = (uint)mp->totloop;
    uint l1, l2, l3, l4;
    uint *lidx;
    if (mp_totloop < 3) {
      /* Do nothing. */
    }

#ifdef USE_TESSFACE_SPEEDUP

#  define ML_TO_MF(i1, i2, i3) \
    mface_to_poly_map[mface_index] = poly_index; \
    mf = &mface[mface_index]; \
    lidx = lindices[mface_index]; \
    /* Set loop indices, transformed to vert indices later. */ \
    l1 = mp_loopstart + i1; \
    l2 = mp_loopstart + i2; \
    l3 = mp_loopstart + i3; \
    mf->v1 = mloop[l1].v; \
    mf->v2 = mloop[l2].v; \
    mf->v3 = mloop[l3].v; \
    mf->v4 = 0; \
    lidx[0] = l1; \
    lidx[1] = l2; \
    lidx[2] = l3; \
    lidx[3] = 0; \
    mf->mat_nr = mp->mat_nr; \
    mf->flag = mp->flag; \
    mf->edcode = 0; \
    (void)0

/* ALMOST IDENTICAL TO DEFINE ABOVE (see EXCEPTION) */
#  define ML_TO_MF_QUAD() \
    mface_to_poly_map[mface_index] = poly_index; \
    mf = &mface[mface_index]; \
    lidx = lindices[mface_index]; \
    /* Set loop indices, transformed to vert indices later. */ \
    l1 = mp_loopstart + 0; /* EXCEPTION */ \
    l2 = mp_loopstart + 1; /* EXCEPTION */ \
    l3 = mp_loopstart + 2; /* EXCEPTION */ \
    l4 = mp_loopstart + 3; /* EXCEPTION */ \
    mf->v1 = mloop[l1].v; \
    mf->v2 = mloop[l2].v; \
    mf->v3 = mloop[l3].v; \
    mf->v4 = mloop[l4].v; \
    lidx[0] = l1; \
    lidx[1] = l2; \
    lidx[2] = l3; \
    lidx[3] = l4; \
    mf->mat_nr = mp->mat_nr; \
    mf->flag = mp->flag; \
    mf->edcode = TESSFACE_IS_QUAD; \
    (void)0

    else if (mp_totloop == 3) {
      ML_TO_MF(0, 1, 2);
      mface_index++;
    }
    else if (mp_totloop == 4) {
#  ifdef USE_TESSFACE_QUADS
      ML_TO_MF_QUAD();
      mface_index++;
#  else
      ML_TO_MF(0, 1, 2);
      mface_index++;
      ML_TO_MF(0, 2, 3);
      mface_index++;
#  endif
    }
#endif /* USE_TESSFACE_SPEEDUP */
    else {
      const float *co_curr, *co_prev;

      float normal[3];

      float axis_mat[3][3];
      float(*projverts)[2];
      uint(*tris)[3];

      const uint totfilltri = mp_totloop - 2;

      if (UNLIKELY(arena == NULL)) {
        arena = BLI_memarena_new(BLI_MEMARENA_STD_BUFSIZE, __func__);
      }

      tris = BLI_memarena_alloc(arena, sizeof(*tris) * (size_t)totfilltri);
      projverts = BLI_memarena_alloc(arena, sizeof(*projverts) * (size_t)mp_totloop);

      zero_v3(normal);

      /* Calculate the normal, flipped: to get a positive 2D cross product. */
      ml = mloop + mp_loopstart;
      co_prev = mvert[ml[mp_totloop - 1].v].co;
      for (j = 0; j < mp_totloop; j++, ml++) {
        co_curr = mvert[ml->v].co;
        add_newell_cross_v3_v3v3(normal, co_prev, co_curr);
        co_prev = co_curr;
      }
      if (UNLIKELY(normalize_v3(normal) == 0.0f)) {
        normal[2] = 1.0f;
      }

      /* Project verts to 2D. */
      axis_dominant_v3_to_m3_negate(axis_mat, normal);

      ml = mloop + mp_loopstart;
      for (j = 0; j < mp_totloop; j++, ml++) {
        mul_v2_m3v3(projverts[j], axis_mat, mvert[ml->v].co);
      }

      BLI_polyfill_calc_arena(projverts, mp_totloop, 1, tris, arena);

      /* Apply fill. */
      for (j = 0; j < totfilltri; j++) {
        uint *tri = tris[j];
        lidx = lindices[mface_index];

        mface_to_poly_map[mface_index] = poly_index;
        mf = &mface[mface_index];

        /* Set loop indices, transformed to vert indices later. */
        l1 = mp_loopstart + tri[0];
        l2 = mp_loopstart + tri[1];
        l3 = mp_loopstart + tri[2];

        mf->v1 = mloop[l1].v;
        mf->v2 = mloop[l2].v;
        mf->v3 = mloop[l3].v;
        mf->v4 = 0;

        lidx[0] = l1;
        lidx[1] = l2;
        lidx[2] = l3;
        lidx[3] = 0;

        mf->mat_nr = mp->mat_nr;
        mf->flag = mp->flag;
        mf->edcode = 0;

        mface_index++;
      }

      BLI_memarena_clear(arena);
    }
  }

  if (arena) {
    BLI_memarena_free(arena);
    arena = NULL;
  }

  CustomData_free(fdata, totface);
  totface = mface_index;

  BLI_assert(totface <= looptri_num);

  /* Not essential but without this we store over-allocated memory in the #CustomData layers. */
  if (LIKELY(looptri_num != totface)) {
    mface = MEM_reallocN(mface, sizeof(*mface) * (size_t)totface);
    mface_to_poly_map = MEM_reallocN(mface_to_poly_map,
                                     sizeof(*mface_to_poly_map) * (size_t)totface);
  }

  CustomData_add_layer(fdata, CD_MFACE, CD_ASSIGN, mface, totface);

  /* #CD_ORIGINDEX will contain an array of indices from tessellation-faces to the polygons
   * they are directly tessellated from. */
  CustomData_add_layer(fdata, CD_ORIGINDEX, CD_ASSIGN, mface_to_poly_map, totface);
  CustomData_from_bmeshpoly(fdata, ldata, totface);

  /* NOTE: quad detection issue - fourth vertidx vs fourth loopidx:
   * Polygons take care of their loops ordering, hence not of their vertices ordering.
   * Currently, our tfaces' fourth vertex index might be 0 even for a quad.
   * However, we know our fourth loop index is never 0 for quads
   * (because they are sorted for polygons, and our quads are still mere copies of their polygons).
   * So we pass NULL as MFace pointer, and #mesh_loops_to_tessdata
   * will use the fourth loop index as quad test. */
  mesh_loops_to_tessdata(fdata, ldata, NULL, mface_to_poly_map, lindices, totface);

  /* NOTE: quad detection issue - fourth vertidx vs fourth loopidx:
   * ...However, most TFace code uses 'MFace->v4 == 0' test to check whether it is a tri or quad.
   * BKE_mesh_mface_index_validate() will check this and rotate the tessellated face if needed.
   */
#ifdef USE_TESSFACE_QUADS
  mf = mface;
  for (mface_index = 0; mface_index < totface; mface_index++, mf++) {
    if (mf->edcode == TESSFACE_IS_QUAD) {
      BKE_mesh_mface_index_validate(mf, fdata, mface_index, 4);
      mf->edcode = 0;
    }
  }
#endif

  MEM_freeN(lindices);

  return totface;

#undef USE_TESSFACE_SPEEDUP
#undef USE_TESSFACE_QUADS

#undef ML_TO_MF
#undef ML_TO_MF_QUAD
}

void BKE_mesh_tessface_calc(Mesh *mesh)
{
  mesh->totface = BKE_mesh_tessface_calc_ex(&mesh->fdata,
                                            &mesh->ldata,
                                            &mesh->pdata,
                                            mesh->mvert,
                                            mesh->totface,
                                            mesh->totloop,
                                            mesh->totpoly);

  BKE_mesh_update_customdata_pointers(mesh, true);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Loop Tessellation
 *
 * Fill in #MLoopTri data-structure.
 * \{ */

/**
 * \param face_normal: This will be optimized out as a constant.
 */
BLI_INLINE void mesh_calc_tessellation_for_face_impl(const MLoop *mloop,
                                                     const MPoly *mpoly,
                                                     const MVert *mvert,
                                                     uint poly_index,
                                                     MLoopTri *mlt,
                                                     MemArena **pf_arena_p,
                                                     const bool face_normal,
                                                     const float normal_precalc[3])
{
  const uint mp_loopstart = (uint)mpoly[poly_index].loopstart;
  const uint mp_totloop = (uint)mpoly[poly_index].totloop;

#define ML_TO_MLT(i1, i2, i3) \
  { \
    ARRAY_SET_ITEMS(mlt->tri, mp_loopstart + i1, mp_loopstart + i2, mp_loopstart + i3); \
    mlt->poly = poly_index; \
  } \
  ((void)0)

  switch (mp_totloop) {
    case 3: {
      ML_TO_MLT(0, 1, 2);
      break;
    }
    case 4: {
      ML_TO_MLT(0, 1, 2);
      MLoopTri *mlt_a = mlt++;
      ML_TO_MLT(0, 2, 3);
      MLoopTri *mlt_b = mlt;

      if (UNLIKELY(face_normal ? is_quad_flip_v3_first_third_fast_with_normal(
                                     /* Simpler calculation (using the normal). */
                                     mvert[mloop[mlt_a->tri[0]].v].co,
                                     mvert[mloop[mlt_a->tri[1]].v].co,
                                     mvert[mloop[mlt_a->tri[2]].v].co,
                                     mvert[mloop[mlt_b->tri[2]].v].co,
                                     normal_precalc) :
                                 is_quad_flip_v3_first_third_fast(
                                     /* Expensive calculation (no normal). */
                                     mvert[mloop[mlt_a->tri[0]].v].co,
                                     mvert[mloop[mlt_a->tri[1]].v].co,
                                     mvert[mloop[mlt_a->tri[2]].v].co,
                                     mvert[mloop[mlt_b->tri[2]].v].co))) {
        /* Flip out of degenerate 0-2 state. */
        mlt_a->tri[2] = mlt_b->tri[2];
        mlt_b->tri[0] = mlt_a->tri[1];
      }
      break;
    }
    default: {
      const MLoop *ml;
      float axis_mat[3][3];

      /* Calculate `axis_mat` to project verts to 2D. */
      if (face_normal == false) {
        float normal[3];
        const float *co_curr, *co_prev;

        zero_v3(normal);

        /* Calc normal, flipped: to get a positive 2D cross product. */
        ml = mloop + mp_loopstart;
        co_prev = mvert[ml[mp_totloop - 1].v].co;
        for (uint j = 0; j < mp_totloop; j++, ml++) {
          co_curr = mvert[ml->v].co;
          add_newell_cross_v3_v3v3(normal, co_prev, co_curr);
          co_prev = co_curr;
        }
        if (UNLIKELY(normalize_v3(normal) == 0.0f)) {
          normal[2] = 1.0f;
        }
        axis_dominant_v3_to_m3_negate(axis_mat, normal);
      }
      else {
        axis_dominant_v3_to_m3_negate(axis_mat, normal_precalc);
      }

      const uint totfilltri = mp_totloop - 2;

      MemArena *pf_arena = *pf_arena_p;
      if (UNLIKELY(pf_arena == NULL)) {
        pf_arena = *pf_arena_p = BLI_memarena_new(BLI_MEMARENA_STD_BUFSIZE, __func__);
      }

      uint(*tris)[3] = tris = BLI_memarena_alloc(pf_arena, sizeof(*tris) * (size_t)totfilltri);
      float(*projverts)[2] = projverts = BLI_memarena_alloc(
          pf_arena, sizeof(*projverts) * (size_t)mp_totloop);

      ml = mloop + mp_loopstart;
      for (uint j = 0; j < mp_totloop; j++, ml++) {
        mul_v2_m3v3(projverts[j], axis_mat, mvert[ml->v].co);
      }

      BLI_polyfill_calc_arena(projverts, mp_totloop, 1, tris, pf_arena);

      /* Apply fill. */
      for (uint j = 0; j < totfilltri; j++, mlt++) {
        const uint *tri = tris[j];
        ML_TO_MLT(tri[0], tri[1], tri[2]);
      }

      BLI_memarena_clear(pf_arena);

      break;
    }
  }
#undef ML_TO_MLT
}

static void mesh_calc_tessellation_for_face(const MLoop *mloop,
                                            const MPoly *mpoly,
                                            const MVert *mvert,
                                            uint poly_index,
                                            MLoopTri *mlt,
                                            MemArena **pf_arena_p)
{
  mesh_calc_tessellation_for_face_impl(
      mloop, mpoly, mvert, poly_index, mlt, pf_arena_p, false, NULL);
}

static void mesh_calc_tessellation_for_face_with_normal(const MLoop *mloop,
                                                        const MPoly *mpoly,
                                                        const MVert *mvert,
                                                        uint poly_index,
                                                        MLoopTri *mlt,
                                                        MemArena **pf_arena_p,
                                                        const float normal_precalc[3])
{
  mesh_calc_tessellation_for_face_impl(
      mloop, mpoly, mvert, poly_index, mlt, pf_arena_p, true, normal_precalc);
}

static void mesh_recalc_looptri__single_threaded(const MLoop *mloop,
                                                 const MPoly *mpoly,
                                                 const MVert *mvert,
                                                 int totloop,
                                                 int totpoly,
                                                 MLoopTri *mlooptri,
                                                 const float (*poly_normals)[3])
{
  MemArena *pf_arena = NULL;
  const MPoly *mp = mpoly;
  uint tri_index = 0;

  if (poly_normals != NULL) {
    for (uint poly_index = 0; poly_index < (uint)totpoly; poly_index++, mp++) {
      mesh_calc_tessellation_for_face_with_normal(mloop,
                                                  mpoly,
                                                  mvert,
                                                  poly_index,
                                                  &mlooptri[tri_index],
                                                  &pf_arena,
                                                  poly_normals[poly_index]);
      tri_index += (uint)(mp->totloop - 2);
    }
  }
  else {
    for (uint poly_index = 0; poly_index < (uint)totpoly; poly_index++, mp++) {
      mesh_calc_tessellation_for_face(
          mloop, mpoly, mvert, poly_index, &mlooptri[tri_index], &pf_arena);
      tri_index += (uint)(mp->totloop - 2);
    }
  }

  if (pf_arena) {
    BLI_memarena_free(pf_arena);
    pf_arena = NULL;
  }
  BLI_assert(tri_index == (uint)poly_to_tri_count(totpoly, totloop));
  UNUSED_VARS_NDEBUG(totloop);
}

struct TessellationUserData {
  const MLoop *mloop;
  const MPoly *mpoly;
  const MVert *mvert;

  /** Output array. */
  MLoopTri *mlooptri;

  /** Optional pre-calculated polygon normals array. */
  const float (*poly_normals)[3];
};

struct TessellationUserTLS {
  MemArena *pf_arena;
};

static void mesh_calc_tessellation_for_face_fn(void *__restrict userdata,
                                               const int index,
                                               const TaskParallelTLS *__restrict tls)
{
  const struct TessellationUserData *data = userdata;
  struct TessellationUserTLS *tls_data = tls->userdata_chunk;
  const int tri_index = poly_to_tri_count(index, data->mpoly[index].loopstart);
  mesh_calc_tessellation_for_face_impl(data->mloop,
                                       data->mpoly,
                                       data->mvert,
                                       (uint)index,
                                       &data->mlooptri[tri_index],
                                       &tls_data->pf_arena,
                                       false,
                                       NULL);
}

static void mesh_calc_tessellation_for_face_with_normal_fn(void *__restrict userdata,
                                                           const int index,
                                                           const TaskParallelTLS *__restrict tls)
{
  const struct TessellationUserData *data = userdata;
  struct TessellationUserTLS *tls_data = tls->userdata_chunk;
  const int tri_index = poly_to_tri_count(index, data->mpoly[index].loopstart);
  mesh_calc_tessellation_for_face_impl(data->mloop,
                                       data->mpoly,
                                       data->mvert,
                                       (uint)index,
                                       &data->mlooptri[tri_index],
                                       &tls_data->pf_arena,
                                       true,
                                       data->poly_normals[index]);
}

static void mesh_calc_tessellation_for_face_free_fn(const void *__restrict UNUSED(userdata),
                                                    void *__restrict tls_v)
{
  struct TessellationUserTLS *tls_data = tls_v;
  if (tls_data->pf_arena) {
    BLI_memarena_free(tls_data->pf_arena);
  }
}

static void mesh_recalc_looptri__multi_threaded(const MLoop *mloop,
                                                const MPoly *mpoly,
                                                const MVert *mvert,
                                                int UNUSED(totloop),
                                                int totpoly,
                                                MLoopTri *mlooptri,
                                                const float (*poly_normals)[3])
{
  struct TessellationUserTLS tls_data_dummy = {NULL};

  struct TessellationUserData data = {
      .mloop = mloop,
      .mpoly = mpoly,
      .mvert = mvert,
      .mlooptri = mlooptri,
      .poly_normals = poly_normals,
  };

  TaskParallelSettings settings;
  BLI_parallel_range_settings_defaults(&settings);

  settings.userdata_chunk = &tls_data_dummy;
  settings.userdata_chunk_size = sizeof(tls_data_dummy);

  settings.func_free = mesh_calc_tessellation_for_face_free_fn;

  BLI_task_parallel_range(0,
                          totpoly,
                          &data,
                          poly_normals ? mesh_calc_tessellation_for_face_with_normal_fn :
                                         mesh_calc_tessellation_for_face_fn,
                          &settings);
}

void BKE_mesh_recalc_looptri(const MLoop *mloop,
                             const MPoly *mpoly,
                             const MVert *mvert,
                             int totloop,
                             int totpoly,
                             MLoopTri *mlooptri)
{
  if (totloop < MESH_FACE_TESSELLATE_THREADED_LIMIT) {
    mesh_recalc_looptri__single_threaded(mloop, mpoly, mvert, totloop, totpoly, mlooptri, NULL);
  }
  else {
    mesh_recalc_looptri__multi_threaded(mloop, mpoly, mvert, totloop, totpoly, mlooptri, NULL);
  }
}

void BKE_mesh_recalc_looptri_with_normals(const MLoop *mloop,
                                          const MPoly *mpoly,
                                          const MVert *mvert,
                                          int totloop,
                                          int totpoly,
                                          MLoopTri *mlooptri,
                                          const float (*poly_normals)[3])
{
  BLI_assert(poly_normals != NULL);
  if (totloop < MESH_FACE_TESSELLATE_THREADED_LIMIT) {
    mesh_recalc_looptri__single_threaded(
        mloop, mpoly, mvert, totloop, totpoly, mlooptri, poly_normals);
  }
  else {
    mesh_recalc_looptri__multi_threaded(
        mloop, mpoly, mvert, totloop, totpoly, mlooptri, poly_normals);
  }
}

/** \} */
