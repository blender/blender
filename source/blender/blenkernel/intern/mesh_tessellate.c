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
#include "BLI_utildefines.h"

#include "BKE_customdata.h"
#include "BKE_mesh.h" /* Own include. */

#include "BLI_strict_flags.h"

/* -------------------------------------------------------------------- */
/** \name MFace Tessellation
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
void BKE_mesh_loops_to_tessdata(CustomData *fdata,
                                CustomData *ldata,
                                MFace *mface,
                                const int *polyindices,
                                uint (*loopindices)[4],
                                const int num_faces)
{
  /* Note: performances are sub-optimal when we get a NULL mface,
   *       we could be ~25% quicker with dedicated code...
   *       Issue is, unless having two different functions with nearly the same code,
   *       there's not much ways to solve this. Better imho to live with it for now. :/ --mont29
   */
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
    /* need to do for all uv maps at some point */
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

/**
 * Recreate tessellation.
 *
 * \param do_face_nor_copy: Controls whether the normals from the poly
 * are copied to the tessellated faces.
 *
 * \return number of tessellation faces.
 */
int BKE_mesh_tessface_calc_ex(CustomData *fdata,
                              CustomData *ldata,
                              CustomData *pdata,
                              MVert *mvert,
                              int totface,
                              int totloop,
                              int totpoly,
                              const bool do_face_nor_copy)
{
  /* use this to avoid locking pthread for _every_ polygon
   * and calling the fill function */

#define USE_TESSFACE_SPEEDUP
#define USE_TESSFACE_QUADS /* NEEDS FURTHER TESTING */

/* We abuse MFace->edcode to tag quad faces. See below for details. */
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

  /* allocate the length of totfaces, avoid many small reallocs,
   * if all faces are tri's it will be correct, quads == 2x allocs */
  /* take care. we are _not_ calloc'ing so be sure to initialize each field */
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
      /* do nothing */
    }

#ifdef USE_TESSFACE_SPEEDUP

#  define ML_TO_MF(i1, i2, i3) \
    mface_to_poly_map[mface_index] = poly_index; \
    mf = &mface[mface_index]; \
    lidx = lindices[mface_index]; \
    /* set loop indices, transformed to vert indices later */ \
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
    /* set loop indices, transformed to vert indices later */ \
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

      /* calc normal, flipped: to get a positive 2d cross product */
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

      /* project verts to 2d */
      axis_dominant_v3_to_m3_negate(axis_mat, normal);

      ml = mloop + mp_loopstart;
      for (j = 0; j < mp_totloop; j++, ml++) {
        mul_v2_m3v3(projverts[j], axis_mat, mvert[ml->v].co);
      }

      BLI_polyfill_calc_arena(projverts, mp_totloop, 1, tris, arena);

      /* apply fill */
      for (j = 0; j < totfilltri; j++) {
        uint *tri = tris[j];
        lidx = lindices[mface_index];

        mface_to_poly_map[mface_index] = poly_index;
        mf = &mface[mface_index];

        /* set loop indices, transformed to vert indices later */
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

  /* not essential but without this we store over-alloc'd memory in the CustomData layers */
  if (LIKELY(looptri_num != totface)) {
    mface = MEM_reallocN(mface, sizeof(*mface) * (size_t)totface);
    mface_to_poly_map = MEM_reallocN(mface_to_poly_map,
                                     sizeof(*mface_to_poly_map) * (size_t)totface);
  }

  CustomData_add_layer(fdata, CD_MFACE, CD_ASSIGN, mface, totface);

  /* CD_ORIGINDEX will contain an array of indices from tessfaces to the polygons
   * they are directly tessellated from */
  CustomData_add_layer(fdata, CD_ORIGINDEX, CD_ASSIGN, mface_to_poly_map, totface);
  CustomData_from_bmeshpoly(fdata, ldata, totface);

  if (do_face_nor_copy) {
    /* If polys have a normals layer, copying that to faces can help
     * avoid the need to recalculate normals later */
    if (CustomData_has_layer(pdata, CD_NORMAL)) {
      float(*pnors)[3] = CustomData_get_layer(pdata, CD_NORMAL);
      float(*fnors)[3] = CustomData_add_layer(fdata, CD_NORMAL, CD_CALLOC, NULL, totface);
      for (mface_index = 0; mface_index < totface; mface_index++) {
        copy_v3_v3(fnors[mface_index], pnors[mface_to_poly_map[mface_index]]);
      }
    }
  }

  /* NOTE: quad detection issue - fourth vertidx vs fourth loopidx:
   * Polygons take care of their loops ordering, hence not of their vertices ordering.
   * Currently, our tfaces' fourth vertex index might be 0 even for a quad. However,
   * we know our fourth loop index is never 0 for quads (because they are sorted for polygons,
   * and our quads are still mere copies of their polygons).
   * So we pass NULL as MFace pointer, and BKE_mesh_loops_to_tessdata
   * will use the fourth loop index as quad test.
   * ...
   */
  BKE_mesh_loops_to_tessdata(fdata, ldata, NULL, mface_to_poly_map, lindices, totface);

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
  mesh->totface = BKE_mesh_tessface_calc_ex(
      &mesh->fdata,
      &mesh->ldata,
      &mesh->pdata,
      mesh->mvert,
      mesh->totface,
      mesh->totloop,
      mesh->totpoly,
      /* calc normals right after, don't copy from polys here */
      false);

  BKE_mesh_update_customdata_pointers(mesh, true);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Loop Tessellation
 * \{ */

/**
 * Calculate tessellation into #MLoopTri which exist only for this purpose.
 */
void BKE_mesh_recalc_looptri(const MLoop *mloop,
                             const MPoly *mpoly,
                             const MVert *mvert,
                             int totloop,
                             int totpoly,
                             MLoopTri *mlooptri)
{
  /* use this to avoid locking pthread for _every_ polygon
   * and calling the fill function */

#define USE_TESSFACE_SPEEDUP

  const MPoly *mp;
  const MLoop *ml;
  MLoopTri *mlt;
  MemArena *arena = NULL;
  int poly_index, mlooptri_index;
  uint j;

  mlooptri_index = 0;
  mp = mpoly;
  for (poly_index = 0; poly_index < totpoly; poly_index++, mp++) {
    const uint mp_loopstart = (uint)mp->loopstart;
    const uint mp_totloop = (uint)mp->totloop;
    uint l1, l2, l3;
    if (mp_totloop < 3) {
      /* do nothing */
    }

#ifdef USE_TESSFACE_SPEEDUP

#  define ML_TO_MLT(i1, i2, i3) \
    { \
      mlt = &mlooptri[mlooptri_index]; \
      l1 = mp_loopstart + i1; \
      l2 = mp_loopstart + i2; \
      l3 = mp_loopstart + i3; \
      ARRAY_SET_ITEMS(mlt->tri, l1, l2, l3); \
      mlt->poly = (uint)poly_index; \
    } \
    ((void)0)

    else if (mp_totloop == 3) {
      ML_TO_MLT(0, 1, 2);
      mlooptri_index++;
    }
    else if (mp_totloop == 4) {
      ML_TO_MLT(0, 1, 2);
      MLoopTri *mlt_a = mlt;
      mlooptri_index++;
      ML_TO_MLT(0, 2, 3);
      MLoopTri *mlt_b = mlt;
      mlooptri_index++;

      if (UNLIKELY(is_quad_flip_v3_first_third_fast(mvert[mloop[mlt_a->tri[0]].v].co,
                                                    mvert[mloop[mlt_a->tri[1]].v].co,
                                                    mvert[mloop[mlt_a->tri[2]].v].co,
                                                    mvert[mloop[mlt_b->tri[2]].v].co))) {
        /* flip out of degenerate 0-2 state. */
        mlt_a->tri[2] = mlt_b->tri[2];
        mlt_b->tri[0] = mlt_a->tri[1];
      }
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

      /* calc normal, flipped: to get a positive 2d cross product */
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

      /* project verts to 2d */
      axis_dominant_v3_to_m3_negate(axis_mat, normal);

      ml = mloop + mp_loopstart;
      for (j = 0; j < mp_totloop; j++, ml++) {
        mul_v2_m3v3(projverts[j], axis_mat, mvert[ml->v].co);
      }

      BLI_polyfill_calc_arena(projverts, mp_totloop, 1, tris, arena);

      /* apply fill */
      for (j = 0; j < totfilltri; j++) {
        uint *tri = tris[j];

        mlt = &mlooptri[mlooptri_index];

        /* set loop indices, transformed to vert indices later */
        l1 = mp_loopstart + tri[0];
        l2 = mp_loopstart + tri[1];
        l3 = mp_loopstart + tri[2];

        ARRAY_SET_ITEMS(mlt->tri, l1, l2, l3);
        mlt->poly = (uint)poly_index;

        mlooptri_index++;
      }

      BLI_memarena_clear(arena);
    }
  }

  if (arena) {
    BLI_memarena_free(arena);
    arena = NULL;
  }

  BLI_assert(mlooptri_index == poly_to_tri_count(totpoly, totloop));
  UNUSED_VARS_NDEBUG(totloop);

#undef USE_TESSFACE_SPEEDUP
#undef ML_TO_MLT
}

/** \} */
