/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2001-2002 NaN Holding BV. All rights reserved. */

/** \file
 * \ingroup bke
 *
 * Functions to convert mesh data to and from legacy formats like #MFace.
 */

#define DNA_DEPRECATED_ALLOW

#include "MEM_guardedalloc.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BLI_edgehash.h"
#include "BLI_math.h"
#include "BLI_memarena.h"
#include "BLI_polyfill_2d.h"
#include "BLI_task.hh"
#include "BLI_utildefines.h"

#include "BKE_attribute.hh"
#include "BKE_customdata.h"
#include "BKE_mesh.h"
#include "BKE_mesh_legacy_convert.h"
#include "BKE_multires.h"

/* -------------------------------------------------------------------- */
/** \name NGon Tessellation (NGon to MFace Conversion)
 * \{ */

static void bm_corners_to_loops_ex(ID *id,
                                   CustomData *fdata,
                                   CustomData *ldata,
                                   MFace *mface,
                                   int totloop,
                                   int findex,
                                   int loopstart,
                                   int numTex,
                                   int numCol)
{
  MFace *mf = mface + findex;

  for (int i = 0; i < numTex; i++) {
    const MTFace *texface = (const MTFace *)CustomData_get_n(fdata, CD_MTFACE, findex, i);

    MLoopUV *mloopuv = (MLoopUV *)CustomData_get_n(ldata, CD_MLOOPUV, loopstart, i);
    copy_v2_v2(mloopuv->uv, texface->uv[0]);
    mloopuv++;
    copy_v2_v2(mloopuv->uv, texface->uv[1]);
    mloopuv++;
    copy_v2_v2(mloopuv->uv, texface->uv[2]);
    mloopuv++;

    if (mf->v4) {
      copy_v2_v2(mloopuv->uv, texface->uv[3]);
      mloopuv++;
    }
  }

  for (int i = 0; i < numCol; i++) {
    MLoopCol *mloopcol = (MLoopCol *)CustomData_get_n(ldata, CD_PROP_BYTE_COLOR, loopstart, i);
    const MCol *mcol = (const MCol *)CustomData_get_n(fdata, CD_MCOL, findex, i);

    MESH_MLOOPCOL_FROM_MCOL(mloopcol, &mcol[0]);
    mloopcol++;
    MESH_MLOOPCOL_FROM_MCOL(mloopcol, &mcol[1]);
    mloopcol++;
    MESH_MLOOPCOL_FROM_MCOL(mloopcol, &mcol[2]);
    mloopcol++;
    if (mf->v4) {
      MESH_MLOOPCOL_FROM_MCOL(mloopcol, &mcol[3]);
      mloopcol++;
    }
  }

  if (CustomData_has_layer(fdata, CD_TESSLOOPNORMAL)) {
    float(*lnors)[3] = (float(*)[3])CustomData_get(ldata, loopstart, CD_NORMAL);
    const short(*tlnors)[3] = (short(*)[3])CustomData_get(fdata, findex, CD_TESSLOOPNORMAL);
    const int max = mf->v4 ? 4 : 3;

    for (int i = 0; i < max; i++, lnors++, tlnors++) {
      normal_short_to_float_v3(*lnors, *tlnors);
    }
  }

  if (CustomData_has_layer(fdata, CD_MDISPS)) {
    MDisps *ld = (MDisps *)CustomData_get(ldata, loopstart, CD_MDISPS);
    const MDisps *fd = (const MDisps *)CustomData_get(fdata, findex, CD_MDISPS);
    const float(*disps)[3] = fd->disps;
    int tot = mf->v4 ? 4 : 3;
    int corners;

    if (CustomData_external_test(fdata, CD_MDISPS)) {
      if (id && fdata->external) {
        CustomData_external_add(ldata, id, CD_MDISPS, totloop, fdata->external->filepath);
      }
    }

    corners = multires_mdisp_corners(fd);

    if (corners == 0) {
      /* Empty #MDisp layers appear in at least one of the `sintel.blend` files.
       * Not sure why this happens, but it seems fine to just ignore them here.
       * If `corners == 0` for a non-empty layer though, something went wrong. */
      BLI_assert(fd->totdisp == 0);
    }
    else {
      const int side = (int)sqrtf((float)(fd->totdisp / corners));
      const int side_sq = side * side;

      for (int i = 0; i < tot; i++, disps += side_sq, ld++) {
        ld->totdisp = side_sq;
        ld->level = (int)(logf((float)side - 1.0f) / (float)M_LN2) + 1;

        if (ld->disps) {
          MEM_freeN(ld->disps);
        }

        ld->disps = (float(*)[3])MEM_malloc_arrayN(
            (size_t)side_sq, sizeof(float[3]), "converted loop mdisps");
        if (fd->disps) {
          memcpy(ld->disps, disps, (size_t)side_sq * sizeof(float[3]));
        }
        else {
          memset(ld->disps, 0, (size_t)side_sq * sizeof(float[3]));
        }
      }
    }
  }
}

static void CustomData_to_bmeshpoly(CustomData *fdata, CustomData *ldata, int totloop)
{
  for (int i = 0; i < fdata->totlayer; i++) {
    if (fdata->layers[i].type == CD_MTFACE) {
      CustomData_add_layer_named(
          ldata, CD_MLOOPUV, CD_SET_DEFAULT, nullptr, totloop, fdata->layers[i].name);
    }
    else if (fdata->layers[i].type == CD_MCOL) {
      CustomData_add_layer_named(
          ldata, CD_PROP_BYTE_COLOR, CD_SET_DEFAULT, nullptr, totloop, fdata->layers[i].name);
    }
    else if (fdata->layers[i].type == CD_MDISPS) {
      CustomData_add_layer_named(
          ldata, CD_MDISPS, CD_SET_DEFAULT, nullptr, totloop, fdata->layers[i].name);
    }
    else if (fdata->layers[i].type == CD_TESSLOOPNORMAL) {
      CustomData_add_layer_named(
          ldata, CD_NORMAL, CD_SET_DEFAULT, nullptr, totloop, fdata->layers[i].name);
    }
  }
}

static void convert_mfaces_to_mpolys(ID *id,
                                     CustomData *fdata,
                                     CustomData *ldata,
                                     CustomData *pdata,
                                     int totedge_i,
                                     int totface_i,
                                     int totloop_i,
                                     int totpoly_i,
                                     MEdge *medge,
                                     MFace *mface,
                                     int *r_totloop,
                                     int *r_totpoly,
                                     MLoop **r_mloop,
                                     MPoly **r_mpoly)
{
  MFace *mf;
  MLoop *ml, *mloop;
  MPoly *mp, *mpoly;
  MEdge *me;
  EdgeHash *eh;
  int numTex, numCol;
  int i, j, totloop, totpoly, *polyindex;

  /* old flag, clear to allow for reuse */
#define ME_FGON (1 << 3)

  /* just in case some of these layers are filled in (can happen with python created meshes) */
  CustomData_free(ldata, totloop_i);
  CustomData_free(pdata, totpoly_i);

  totpoly = totface_i;
  mpoly = (MPoly *)MEM_calloc_arrayN((size_t)totpoly, sizeof(MPoly), "mpoly converted");
  CustomData_add_layer(pdata, CD_MPOLY, CD_ASSIGN, mpoly, totpoly);

  numTex = CustomData_number_of_layers(fdata, CD_MTFACE);
  numCol = CustomData_number_of_layers(fdata, CD_MCOL);

  totloop = 0;
  mf = mface;
  for (i = 0; i < totface_i; i++, mf++) {
    totloop += mf->v4 ? 4 : 3;
  }

  mloop = (MLoop *)MEM_calloc_arrayN((size_t)totloop, sizeof(MLoop), "mloop converted");

  CustomData_add_layer(ldata, CD_MLOOP, CD_ASSIGN, mloop, totloop);

  CustomData_to_bmeshpoly(fdata, ldata, totloop);

  if (id) {
    /* ensure external data is transferred */
    /* TODO(sergey): Use multiresModifier_ensure_external_read(). */
    CustomData_external_read(fdata, id, CD_MASK_MDISPS, totface_i);
  }

  eh = BLI_edgehash_new_ex(__func__, (uint)totedge_i);

  /* build edge hash */
  me = medge;
  for (i = 0; i < totedge_i; i++, me++) {
    BLI_edgehash_insert(eh, me->v1, me->v2, POINTER_FROM_UINT(i));

    /* unrelated but avoid having the FGON flag enabled,
     * so we can reuse it later for something else */
    me->flag &= ~ME_FGON;
  }

  polyindex = (int *)CustomData_get_layer(fdata, CD_ORIGINDEX);

  j = 0; /* current loop index */
  ml = mloop;
  mf = mface;
  mp = mpoly;
  for (i = 0; i < totface_i; i++, mf++, mp++) {
    mp->loopstart = j;

    mp->totloop = mf->v4 ? 4 : 3;

    mp->mat_nr = mf->mat_nr;
    mp->flag = mf->flag;

#define ML(v1, v2) \
  { \
    ml->v = mf->v1; \
    ml->e = POINTER_AS_UINT(BLI_edgehash_lookup(eh, mf->v1, mf->v2)); \
    ml++; \
    j++; \
  } \
  (void)0

    ML(v1, v2);
    ML(v2, v3);
    if (mf->v4) {
      ML(v3, v4);
      ML(v4, v1);
    }
    else {
      ML(v3, v1);
    }

#undef ML

    bm_corners_to_loops_ex(id, fdata, ldata, mface, totloop, i, mp->loopstart, numTex, numCol);

    if (polyindex) {
      *polyindex = i;
      polyindex++;
    }
  }

  /* NOTE: we don't convert NGons at all, these are not even real ngons,
   * they have their own UV's, colors etc - its more an editing feature. */

  BLI_edgehash_free(eh, nullptr);

  *r_totpoly = totpoly;
  *r_totloop = totloop;
  *r_mpoly = mpoly;
  *r_mloop = mloop;

#undef ME_FGON
}

void BKE_mesh_convert_mfaces_to_mpolys(Mesh *mesh)
{
  convert_mfaces_to_mpolys(&mesh->id,
                           &mesh->fdata,
                           &mesh->ldata,
                           &mesh->pdata,
                           mesh->totedge,
                           mesh->totface,
                           mesh->totloop,
                           mesh->totpoly,
                           mesh->medge,
                           mesh->mface,
                           &mesh->totloop,
                           &mesh->totpoly,
                           &mesh->mloop,
                           &mesh->mpoly);

  BKE_mesh_update_customdata_pointers(mesh, true);
}

/**
 * Update active indices for active/render/clone/stencil custom data layers
 * based on indices from fdata layers
 * used when creating pdata and ldata for pre-bmesh
 * meshes and needed to preserve active/render/clone/stencil flags set in pre-bmesh files.
 */
static void CustomData_bmesh_do_versions_update_active_layers(CustomData *fdata, CustomData *ldata)
{
  int act;

  if (CustomData_has_layer(fdata, CD_MTFACE)) {
    act = CustomData_get_active_layer(fdata, CD_MTFACE);
    CustomData_set_layer_active(ldata, CD_MLOOPUV, act);

    act = CustomData_get_render_layer(fdata, CD_MTFACE);
    CustomData_set_layer_render(ldata, CD_MLOOPUV, act);

    act = CustomData_get_clone_layer(fdata, CD_MTFACE);
    CustomData_set_layer_clone(ldata, CD_MLOOPUV, act);

    act = CustomData_get_stencil_layer(fdata, CD_MTFACE);
    CustomData_set_layer_stencil(ldata, CD_MLOOPUV, act);
  }

  if (CustomData_has_layer(fdata, CD_MCOL)) {
    act = CustomData_get_active_layer(fdata, CD_MCOL);
    CustomData_set_layer_active(ldata, CD_PROP_BYTE_COLOR, act);

    act = CustomData_get_render_layer(fdata, CD_MCOL);
    CustomData_set_layer_render(ldata, CD_PROP_BYTE_COLOR, act);

    act = CustomData_get_clone_layer(fdata, CD_MCOL);
    CustomData_set_layer_clone(ldata, CD_PROP_BYTE_COLOR, act);

    act = CustomData_get_stencil_layer(fdata, CD_MCOL);
    CustomData_set_layer_stencil(ldata, CD_PROP_BYTE_COLOR, act);
  }
}

void BKE_mesh_do_versions_convert_mfaces_to_mpolys(Mesh *mesh)
{
  convert_mfaces_to_mpolys(&mesh->id,
                           &mesh->fdata,
                           &mesh->ldata,
                           &mesh->pdata,
                           mesh->totedge,
                           mesh->totface,
                           mesh->totloop,
                           mesh->totpoly,
                           mesh->medge,
                           mesh->mface,
                           &mesh->totloop,
                           &mesh->totpoly,
                           &mesh->mloop,
                           &mesh->mpoly);

  CustomData_bmesh_do_versions_update_active_layers(&mesh->fdata, &mesh->ldata);

  BKE_mesh_update_customdata_pointers(mesh, true);
}

/** \} */

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
 * \note when mface is not null, mface[face_index].v4
 * is used to test quads, else, loopindices[face_index][3] is used.
 */
static void mesh_loops_to_tessdata(CustomData *fdata,
                                   CustomData *ldata,
                                   MFace *mface,
                                   const int *polyindices,
                                   uint (*loopindices)[4],
                                   const int num_faces)
{
  /* NOTE(mont29): performances are sub-optimal when we get a null #MFace,
   * we could be ~25% quicker with dedicated code.
   * The issue is, unless having two different functions with nearly the same code,
   * there's not much ways to solve this. Better IMHO to live with it for now (sigh). */
  const int numUV = CustomData_number_of_layers(ldata, CD_MLOOPUV);
  const int numCol = CustomData_number_of_layers(ldata, CD_PROP_BYTE_COLOR);
  const bool hasPCol = CustomData_has_layer(ldata, CD_PREVIEW_MLOOPCOL);
  const bool hasOrigSpace = CustomData_has_layer(ldata, CD_ORIGSPACE_MLOOP);
  const bool hasLoopNormal = CustomData_has_layer(ldata, CD_NORMAL);
  const bool hasLoopTangent = CustomData_has_layer(ldata, CD_TANGENT);
  int findex, i, j;
  const int *pidx;
  uint(*lidx)[4];

  for (i = 0; i < numUV; i++) {
    MTFace *texface = (MTFace *)CustomData_get_layer_n(fdata, CD_MTFACE, i);
    const MLoopUV *mloopuv = (const MLoopUV *)CustomData_get_layer_n(ldata, CD_MLOOPUV, i);

    for (findex = 0, pidx = polyindices, lidx = loopindices; findex < num_faces;
         pidx++, lidx++, findex++, texface++) {
      for (j = (mface ? mface[findex].v4 : (*lidx)[3]) ? 4 : 3; j--;) {
        copy_v2_v2(texface->uv[j], mloopuv[(*lidx)[j]].uv);
      }
    }
  }

  for (i = 0; i < numCol; i++) {
    MCol(*mcol)[4] = (MCol(*)[4])CustomData_get_layer_n(fdata, CD_MCOL, i);
    const MLoopCol *mloopcol = (const MLoopCol *)CustomData_get_layer_n(
        ldata, CD_PROP_BYTE_COLOR, i);

    for (findex = 0, lidx = loopindices; findex < num_faces; lidx++, findex++, mcol++) {
      for (j = (mface ? mface[findex].v4 : (*lidx)[3]) ? 4 : 3; j--;) {
        MESH_MLOOPCOL_TO_MCOL(&mloopcol[(*lidx)[j]], &(*mcol)[j]);
      }
    }
  }

  if (hasPCol) {
    MCol(*mcol)[4] = (MCol(*)[4])CustomData_get_layer(fdata, CD_PREVIEW_MCOL);
    const MLoopCol *mloopcol = (const MLoopCol *)CustomData_get_layer(ldata, CD_PREVIEW_MLOOPCOL);

    for (findex = 0, lidx = loopindices; findex < num_faces; lidx++, findex++, mcol++) {
      for (j = (mface ? mface[findex].v4 : (*lidx)[3]) ? 4 : 3; j--;) {
        MESH_MLOOPCOL_TO_MCOL(&mloopcol[(*lidx)[j]], &(*mcol)[j]);
      }
    }
  }

  if (hasOrigSpace) {
    OrigSpaceFace *of = (OrigSpaceFace *)CustomData_get_layer(fdata, CD_ORIGSPACE);
    const OrigSpaceLoop *lof = (const OrigSpaceLoop *)CustomData_get_layer(ldata,
                                                                           CD_ORIGSPACE_MLOOP);

    for (findex = 0, lidx = loopindices; findex < num_faces; lidx++, findex++, of++) {
      for (j = (mface ? mface[findex].v4 : (*lidx)[3]) ? 4 : 3; j--;) {
        copy_v2_v2(of->uv[j], lof[(*lidx)[j]].uv);
      }
    }
  }

  if (hasLoopNormal) {
    short(*fnors)[4][3] = (short(*)[4][3])CustomData_get_layer(fdata, CD_TESSLOOPNORMAL);
    const float(*lnors)[3] = (const float(*)[3])CustomData_get_layer(ldata, CD_NORMAL);

    for (findex = 0, lidx = loopindices; findex < num_faces; lidx++, findex++, fnors++) {
      for (j = (mface ? mface[findex].v4 : (*lidx)[3]) ? 4 : 3; j--;) {
        normal_float_to_short_v3((*fnors)[j], lnors[(*lidx)[j]]);
      }
    }
  }

  if (hasLoopTangent) {
    /* Need to do for all UV maps at some point. */
    float(*ftangents)[4] = (float(*)[4])CustomData_get_layer(fdata, CD_TANGENT);
    const float(*ltangents)[4] = (const float(*)[4])CustomData_get_layer(ldata, CD_TANGENT);

    for (findex = 0, pidx = polyindices, lidx = loopindices; findex < num_faces;
         pidx++, lidx++, findex++) {
      int nverts = (mface ? mface[findex].v4 : (*lidx)[3]) ? 4 : 3;
      for (j = nverts; j--;) {
        copy_v4_v4(ftangents[findex * 4 + j], ltangents[(*lidx)[j]]);
      }
    }
  }
}

int BKE_mesh_mface_index_validate(MFace *mface, CustomData *fdata, int mfindex, int nr)
{
  /* first test if the face is legal */
  if ((mface->v3 || nr == 4) && mface->v3 == mface->v4) {
    mface->v4 = 0;
    nr--;
  }
  if ((mface->v2 || mface->v4) && mface->v2 == mface->v3) {
    mface->v3 = mface->v4;
    mface->v4 = 0;
    nr--;
  }
  if (mface->v1 == mface->v2) {
    mface->v2 = mface->v3;
    mface->v3 = mface->v4;
    mface->v4 = 0;
    nr--;
  }

  /* Check corrupt cases, bow-tie geometry,
   * can't handle these because edge data won't exist so just return 0. */
  if (nr == 3) {
    if (
        /* real edges */
        mface->v1 == mface->v2 || mface->v2 == mface->v3 || mface->v3 == mface->v1) {
      return 0;
    }
  }
  else if (nr == 4) {
    if (
        /* real edges */
        mface->v1 == mface->v2 || mface->v2 == mface->v3 || mface->v3 == mface->v4 ||
        mface->v4 == mface->v1 ||
        /* across the face */
        mface->v1 == mface->v3 || mface->v2 == mface->v4) {
      return 0;
    }
  }

  /* prevent a zero at wrong index location */
  if (nr == 3) {
    if (mface->v3 == 0) {
      static int corner_indices[4] = {1, 2, 0, 3};

      SWAP(uint, mface->v1, mface->v2);
      SWAP(uint, mface->v2, mface->v3);

      if (fdata) {
        CustomData_swap_corners(fdata, mfindex, corner_indices);
      }
    }
  }
  else if (nr == 4) {
    if (mface->v3 == 0 || mface->v4 == 0) {
      static int corner_indices[4] = {2, 3, 0, 1};

      SWAP(uint, mface->v1, mface->v3);
      SWAP(uint, mface->v2, mface->v4);

      if (fdata) {
        CustomData_swap_corners(fdata, mfindex, corner_indices);
      }
    }
  }

  return nr;
}

static int mesh_tessface_calc(CustomData *fdata,
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

  const MPoly *mp, *mpoly;
  const MLoop *ml, *mloop;
  MFace *mface, *mf;
  MemArena *arena = nullptr;
  int *mface_to_poly_map;
  uint(*lindices)[4];
  int poly_index, mface_index;
  uint j;

  mpoly = (const MPoly *)CustomData_get_layer(pdata, CD_MPOLY);
  mloop = (const MLoop *)CustomData_get_layer(ldata, CD_MLOOP);

  /* Allocate the length of `totfaces`, avoid many small reallocation's,
   * if all faces are triangles it will be correct, `quads == 2x` allocations. */
  /* Take care since memory is _not_ zeroed so be sure to initialize each field. */
  mface_to_poly_map = (int *)MEM_malloc_arrayN(
      (size_t)looptri_num, sizeof(*mface_to_poly_map), __func__);
  mface = (MFace *)MEM_malloc_arrayN((size_t)looptri_num, sizeof(*mface), __func__);
  lindices = (uint(*)[4])MEM_malloc_arrayN((size_t)looptri_num, sizeof(*lindices), __func__);

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

      if (UNLIKELY(arena == nullptr)) {
        arena = BLI_memarena_new(BLI_MEMARENA_STD_BUFSIZE, __func__);
      }

      tris = (uint(*)[3])BLI_memarena_alloc(arena, sizeof(*tris) * (size_t)totfilltri);
      projverts = (float(*)[2])BLI_memarena_alloc(arena, sizeof(*projverts) * (size_t)mp_totloop);

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
    arena = nullptr;
  }

  CustomData_free(fdata, totface);
  totface = mface_index;

  BLI_assert(totface <= looptri_num);

  /* Not essential but without this we store over-allocated memory in the #CustomData layers. */
  if (LIKELY(looptri_num != totface)) {
    mface = (MFace *)MEM_reallocN(mface, sizeof(*mface) * (size_t)totface);
    mface_to_poly_map = (int *)MEM_reallocN(mface_to_poly_map,
                                            sizeof(*mface_to_poly_map) * (size_t)totface);
  }

  CustomData_add_layer(fdata, CD_MFACE, CD_ASSIGN, mface, totface);

  /* #CD_ORIGINDEX will contain an array of indices from tessellation-faces to the polygons
   * they are directly tessellated from. */
  CustomData_add_layer(fdata, CD_ORIGINDEX, CD_ASSIGN, mface_to_poly_map, totface);
  BKE_mesh_add_mface_layers(fdata, ldata, totface);

  /* NOTE: quad detection issue - fourth vertidx vs fourth loopidx:
   * Polygons take care of their loops ordering, hence not of their vertices ordering.
   * Currently, our tfaces' fourth vertex index might be 0 even for a quad.
   * However, we know our fourth loop index is never 0 for quads
   * (because they are sorted for polygons, and our quads are still mere copies of their polygons).
   * So we pass nullptr as MFace pointer, and #mesh_loops_to_tessdata
   * will use the fourth loop index as quad test. */
  mesh_loops_to_tessdata(fdata, ldata, nullptr, mface_to_poly_map, lindices, totface);

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
  mesh->totface = mesh_tessface_calc(&mesh->fdata,
                                     &mesh->ldata,
                                     &mesh->pdata,
                                     mesh->mvert,
                                     mesh->totface,
                                     mesh->totloop,
                                     mesh->totpoly);

  BKE_mesh_update_customdata_pointers(mesh, true);
}

void BKE_mesh_tessface_ensure(struct Mesh *mesh)
{
  if (mesh->totpoly && mesh->totface == 0) {
    BKE_mesh_tessface_calc(mesh);
  }
}

#ifndef NDEBUG
/**
 * Debug check, used to assert when we expect layers to be in/out of sync.
 *
 * \param fallback: Use when there are no layers to handle,
 * since callers may expect success or failure.
 */
static bool check_matching_legacy_layer_counts(CustomData *fdata, CustomData *ldata, bool fallback)
{
  int a_num = 0, b_num = 0;
#  define LAYER_CMP(l_a, t_a, l_b, t_b) \
    ((a_num += CustomData_number_of_layers(l_a, t_a)) == \
     (b_num += CustomData_number_of_layers(l_b, t_b)))

  if (!LAYER_CMP(ldata, CD_MLOOPUV, fdata, CD_MTFACE)) {
    return false;
  }
  if (!LAYER_CMP(ldata, CD_PROP_BYTE_COLOR, fdata, CD_MCOL)) {
    return false;
  }
  if (!LAYER_CMP(ldata, CD_PREVIEW_MLOOPCOL, fdata, CD_PREVIEW_MCOL)) {
    return false;
  }
  if (!LAYER_CMP(ldata, CD_ORIGSPACE_MLOOP, fdata, CD_ORIGSPACE)) {
    return false;
  }
  if (!LAYER_CMP(ldata, CD_NORMAL, fdata, CD_TESSLOOPNORMAL)) {
    return false;
  }
  if (!LAYER_CMP(ldata, CD_TANGENT, fdata, CD_TANGENT)) {
    return false;
  }

#  undef LAYER_CMP

  /* if no layers are on either CustomData's,
   * then there was nothing to do... */
  return a_num ? true : fallback;
}
#endif

void BKE_mesh_add_mface_layers(CustomData *fdata, CustomData *ldata, int total)
{
  /* avoid accumulating extra layers */
  BLI_assert(!check_matching_legacy_layer_counts(fdata, ldata, false));

  for (int i = 0; i < ldata->totlayer; i++) {
    if (ldata->layers[i].type == CD_MLOOPUV) {
      CustomData_add_layer_named(
          fdata, CD_MTFACE, CD_SET_DEFAULT, nullptr, total, ldata->layers[i].name);
    }
    if (ldata->layers[i].type == CD_PROP_BYTE_COLOR) {
      CustomData_add_layer_named(
          fdata, CD_MCOL, CD_SET_DEFAULT, nullptr, total, ldata->layers[i].name);
    }
    else if (ldata->layers[i].type == CD_PREVIEW_MLOOPCOL) {
      CustomData_add_layer_named(
          fdata, CD_PREVIEW_MCOL, CD_SET_DEFAULT, nullptr, total, ldata->layers[i].name);
    }
    else if (ldata->layers[i].type == CD_ORIGSPACE_MLOOP) {
      CustomData_add_layer_named(
          fdata, CD_ORIGSPACE, CD_SET_DEFAULT, nullptr, total, ldata->layers[i].name);
    }
    else if (ldata->layers[i].type == CD_NORMAL) {
      CustomData_add_layer_named(
          fdata, CD_TESSLOOPNORMAL, CD_SET_DEFAULT, nullptr, total, ldata->layers[i].name);
    }
    else if (ldata->layers[i].type == CD_TANGENT) {
      CustomData_add_layer_named(
          fdata, CD_TANGENT, CD_SET_DEFAULT, nullptr, total, ldata->layers[i].name);
    }
  }

  CustomData_bmesh_update_active_layers(fdata, ldata);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Hide Attribute and Legacy Flag Conversion
 * \{ */

void BKE_mesh_legacy_convert_hide_layers_to_flags(Mesh *mesh)
{
  using namespace blender;
  using namespace blender::bke;
  const AttributeAccessor attributes = mesh_attributes(*mesh);

  MutableSpan<MVert> verts(mesh->mvert, mesh->totvert);
  const VArray<bool> hide_vert = attributes.lookup_or_default<bool>(
      ".hide_vert", ATTR_DOMAIN_POINT, false);
  threading::parallel_for(verts.index_range(), 4096, [&](IndexRange range) {
    for (const int i : range) {
      SET_FLAG_FROM_TEST(verts[i].flag, hide_vert[i], ME_HIDE);
    }
  });

  MutableSpan<MEdge> edges(mesh->medge, mesh->totedge);
  const VArray<bool> hide_edge = attributes.lookup_or_default<bool>(
      ".hide_edge", ATTR_DOMAIN_EDGE, false);
  threading::parallel_for(edges.index_range(), 4096, [&](IndexRange range) {
    for (const int i : range) {
      SET_FLAG_FROM_TEST(edges[i].flag, hide_edge[i], ME_HIDE);
    }
  });

  MutableSpan<MPoly> polys(mesh->mpoly, mesh->totpoly);
  const VArray<bool> hide_poly = attributes.lookup_or_default<bool>(
      ".hide_poly", ATTR_DOMAIN_FACE, false);
  threading::parallel_for(polys.index_range(), 4096, [&](IndexRange range) {
    for (const int i : range) {
      SET_FLAG_FROM_TEST(polys[i].flag, hide_poly[i], ME_HIDE);
    }
  });
}

void BKE_mesh_legacy_convert_flags_to_hide_layers(Mesh *mesh)
{
  using namespace blender;
  using namespace blender::bke;
  MutableAttributeAccessor attributes = mesh_attributes_for_write(*mesh);

  const Span<MVert> verts(mesh->mvert, mesh->totvert);
  if (std::any_of(
          verts.begin(), verts.end(), [](const MVert &vert) { return vert.flag & ME_HIDE; })) {
    SpanAttributeWriter<bool> hide_vert = attributes.lookup_or_add_for_write_only_span<bool>(
        ".hide_vert", ATTR_DOMAIN_POINT);
    threading::parallel_for(verts.index_range(), 4096, [&](IndexRange range) {
      for (const int i : range) {
        hide_vert.span[i] = verts[i].flag & ME_HIDE;
      }
    });
    hide_vert.finish();
  }

  const Span<MEdge> edges(mesh->medge, mesh->totedge);
  if (std::any_of(
          edges.begin(), edges.end(), [](const MEdge &edge) { return edge.flag & ME_HIDE; })) {
    SpanAttributeWriter<bool> hide_edge = attributes.lookup_or_add_for_write_only_span<bool>(
        ".hide_edge", ATTR_DOMAIN_EDGE);
    threading::parallel_for(edges.index_range(), 4096, [&](IndexRange range) {
      for (const int i : range) {
        hide_edge.span[i] = edges[i].flag & ME_HIDE;
      }
    });
    hide_edge.finish();
  }

  const Span<MPoly> polys(mesh->mpoly, mesh->totpoly);
  if (std::any_of(
          polys.begin(), polys.end(), [](const MPoly &poly) { return poly.flag & ME_HIDE; })) {
    SpanAttributeWriter<bool> hide_poly = attributes.lookup_or_add_for_write_only_span<bool>(
        ".hide_poly", ATTR_DOMAIN_FACE);
    threading::parallel_for(polys.index_range(), 4096, [&](IndexRange range) {
      for (const int i : range) {
        hide_poly.span[i] = polys[i].flag & ME_HIDE;
      }
    });
    hide_poly.finish();
  }
}

/** \} */
/* -------------------------------------------------------------------- */
/** \name Material Index Conversion
 * \{ */

void BKE_mesh_legacy_convert_material_indices_to_mpoly(Mesh *mesh)
{
  using namespace blender;
  using namespace blender::bke;
  const AttributeAccessor attributes = mesh_attributes(*mesh);
  MutableSpan<MPoly> polys(mesh->mpoly, mesh->totpoly);
  const VArray<int> material_indices = attributes.lookup_or_default<int>(
      "material_index", ATTR_DOMAIN_FACE, 0);
  threading::parallel_for(polys.index_range(), 4096, [&](IndexRange range) {
    for (const int i : range) {
      polys[i].mat_nr = material_indices[i];
    }
  });
}

void BKE_mesh_legacy_convert_mpoly_to_material_indices(Mesh *mesh)
{
  using namespace blender;
  using namespace blender::bke;
  MutableAttributeAccessor attributes = mesh_attributes_for_write(*mesh);
  const Span<MPoly> polys(mesh->mpoly, mesh->totpoly);
  if (std::any_of(
          polys.begin(), polys.end(), [](const MPoly &poly) { return poly.mat_nr != 0; })) {
    SpanAttributeWriter<int> material_indices = attributes.lookup_or_add_for_write_only_span<int>(
        "material_index", ATTR_DOMAIN_FACE);
    threading::parallel_for(polys.index_range(), 4096, [&](IndexRange range) {
      for (const int i : range) {
        material_indices.span[i] = polys[i].mat_nr;
      }
    });
    material_indices.finish();
  }
}

/** \} */
