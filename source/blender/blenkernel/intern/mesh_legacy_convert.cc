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
#include "DNA_object_types.h"

#include "BLI_edgehash.h"
#include "BLI_math.h"
#include "BLI_math_vector_types.hh"
#include "BLI_memarena.h"
#include "BLI_polyfill_2d.h"
#include "BLI_resource_scope.hh"
#include "BLI_task.hh"
#include "BLI_utildefines.h"

#include "BKE_attribute.hh"
#include "BKE_customdata.h"
#include "BKE_global.h"
#include "BKE_mesh.h"
#include "BKE_mesh_legacy_convert.h"
#include "BKE_multires.h"

using blender::MutableSpan;
using blender::Span;

/* -------------------------------------------------------------------- */
/** \name Legacy Edge Calculation
 * \{ */

struct EdgeSort {
  uint v1, v2;
  char is_loose, is_draw;
};

/* edges have to be added with lowest index first for sorting */
static void to_edgesort(struct EdgeSort *ed, uint v1, uint v2, char is_loose, short is_draw)
{
  if (v1 < v2) {
    ed->v1 = v1;
    ed->v2 = v2;
  }
  else {
    ed->v1 = v2;
    ed->v2 = v1;
  }
  ed->is_loose = is_loose;
  ed->is_draw = is_draw;
}

static int vergedgesort(const void *v1, const void *v2)
{
  const struct EdgeSort *x1 = static_cast<const struct EdgeSort *>(v1);
  const struct EdgeSort *x2 = static_cast<const struct EdgeSort *>(v2);

  if (x1->v1 > x2->v1) {
    return 1;
  }
  if (x1->v1 < x2->v1) {
    return -1;
  }
  if (x1->v2 > x2->v2) {
    return 1;
  }
  if (x1->v2 < x2->v2) {
    return -1;
  }

  return 0;
}

/* Create edges based on known verts and faces,
 * this function is only used when loading very old blend files */
static void mesh_calc_edges_mdata(const MVert * /*allvert*/,
                                  const MFace *allface,
                                  MLoop *allloop,
                                  const MPoly *allpoly,
                                  int /*totvert*/,
                                  int totface,
                                  int /*totloop*/,
                                  int totpoly,
                                  MEdge **r_medge,
                                  int *r_totedge)
{
  const MPoly *mpoly;
  const MFace *mface;
  MEdge *medge, *med;
  EdgeHash *hash;
  struct EdgeSort *edsort, *ed;
  int a, totedge = 0;
  uint totedge_final = 0;
  uint edge_index;

  /* we put all edges in array, sort them, and detect doubles that way */

  for (a = totface, mface = allface; a > 0; a--, mface++) {
    if (mface->v4) {
      totedge += 4;
    }
    else if (mface->v3) {
      totedge += 3;
    }
    else {
      totedge += 1;
    }
  }

  if (totedge == 0) {
    /* flag that mesh has edges */
    (*r_medge) = (MEdge *)MEM_callocN(0, __func__);
    (*r_totedge) = 0;
    return;
  }

  ed = edsort = (EdgeSort *)MEM_mallocN(totedge * sizeof(struct EdgeSort), "EdgeSort");

  for (a = totface, mface = allface; a > 0; a--, mface++) {
    to_edgesort(ed++, mface->v1, mface->v2, !mface->v3, mface->edcode & ME_V1V2);
    if (mface->v4) {
      to_edgesort(ed++, mface->v2, mface->v3, 0, mface->edcode & ME_V2V3);
      to_edgesort(ed++, mface->v3, mface->v4, 0, mface->edcode & ME_V3V4);
      to_edgesort(ed++, mface->v4, mface->v1, 0, mface->edcode & ME_V4V1);
    }
    else if (mface->v3) {
      to_edgesort(ed++, mface->v2, mface->v3, 0, mface->edcode & ME_V2V3);
      to_edgesort(ed++, mface->v3, mface->v1, 0, mface->edcode & ME_V3V1);
    }
  }

  qsort(edsort, totedge, sizeof(struct EdgeSort), vergedgesort);

  /* count final amount */
  for (a = totedge, ed = edsort; a > 1; a--, ed++) {
    /* edge is unique when it differs from next edge, or is last */
    if (ed->v1 != (ed + 1)->v1 || ed->v2 != (ed + 1)->v2) {
      totedge_final++;
    }
  }
  totedge_final++;

  medge = (MEdge *)MEM_callocN(sizeof(MEdge) * totedge_final, __func__);

  for (a = totedge, med = medge, ed = edsort; a > 1; a--, ed++) {
    /* edge is unique when it differs from next edge, or is last */
    if (ed->v1 != (ed + 1)->v1 || ed->v2 != (ed + 1)->v2) {
      med->v1 = ed->v1;
      med->v2 = ed->v2;

      /* order is swapped so extruding this edge as a surface won't flip face normals
       * with cyclic curves */
      if (ed->v1 + 1 != ed->v2) {
        std::swap(med->v1, med->v2);
      }
      med++;
    }
    else {
      /* Equal edge, merge the draw-flag. */
      (ed + 1)->is_draw |= ed->is_draw;
    }
  }
  /* last edge */
  med->v1 = ed->v1;
  med->v2 = ed->v2;

  MEM_freeN(edsort);

  /* set edge members of mloops */
  hash = BLI_edgehash_new_ex(__func__, totedge_final);
  for (edge_index = 0, med = medge; edge_index < totedge_final; edge_index++, med++) {
    BLI_edgehash_insert(hash, med->v1, med->v2, POINTER_FROM_UINT(edge_index));
  }

  mpoly = allpoly;
  for (a = 0; a < totpoly; a++, mpoly++) {
    MLoop *ml, *ml_next;
    int i = mpoly->totloop;

    ml_next = allloop + mpoly->loopstart; /* first loop */
    ml = &ml_next[i - 1];                 /* last loop */

    while (i-- != 0) {
      ml->e = POINTER_AS_UINT(BLI_edgehash_lookup(hash, ml->v, ml_next->v));
      ml = ml_next;
      ml_next++;
    }
  }

  BLI_edgehash_free(hash, nullptr);

  *r_medge = medge;
  *r_totedge = totedge_final;
}

void BKE_mesh_calc_edges_legacy(Mesh *me)
{
  using namespace blender;
  MEdge *medge;
  int totedge = 0;
  const Span<MVert> verts(static_cast<const MVert *>(CustomData_get_layer(&me->vdata, CD_MVERT)),
                          me->totvert);
  const Span<MPoly> polys = me->polys();
  MutableSpan<MLoop> loops = me->loops_for_write();

  mesh_calc_edges_mdata(verts.data(),
                        (MFace *)CustomData_get_layer(&me->fdata, CD_MFACE),
                        loops.data(),
                        polys.data(),
                        verts.size(),
                        me->totface,
                        loops.size(),
                        polys.size(),
                        &medge,
                        &totedge);

  if (totedge == 0) {
    /* flag that mesh has edges */
    me->totedge = 0;
    return;
  }

  medge = (MEdge *)CustomData_add_layer(&me->edata, CD_MEDGE, CD_ASSIGN, medge, totedge);
  me->totedge = totedge;

  BKE_mesh_tag_topology_changed(me);
  BKE_mesh_strip_loose_faces(me);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name CD Flag Initialization
 * \{ */

void BKE_mesh_do_versions_cd_flag_init(Mesh *mesh)
{
  using namespace blender;
  if (UNLIKELY(mesh->cd_flag)) {
    return;
  }

  const Span<MVert> verts(static_cast<const MVert *>(CustomData_get_layer(&mesh->vdata, CD_MVERT)),
                          mesh->totvert);
  const Span<MEdge> edges = mesh->edges();

  for (const MVert &vert : verts) {
    if (vert.bweight_legacy != 0) {
      mesh->cd_flag |= ME_CDFLAG_VERT_BWEIGHT;
      break;
    }
  }

  for (const MEdge &edge : edges) {
    if (edge.bweight_legacy != 0) {
      mesh->cd_flag |= ME_CDFLAG_EDGE_BWEIGHT;
      if (mesh->cd_flag & ME_CDFLAG_EDGE_CREASE) {
        break;
      }
    }
    if (edge.crease_legacy != 0) {
      mesh->cd_flag |= ME_CDFLAG_EDGE_CREASE;
      if (mesh->cd_flag & ME_CDFLAG_EDGE_BWEIGHT) {
        break;
      }
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name NGon Tessellation (NGon to MFace Conversion)
 * \{ */

static void bm_corners_to_loops_ex(ID *id,
                                   CustomData *fdata,
                                   const int totface,
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
    const MTFace *texface = (const MTFace *)CustomData_get_n_for_write(
        fdata, CD_MTFACE, findex, i, totface);

    blender::float2 *uv = static_cast<blender::float2 *>(
        CustomData_get_n_for_write(ldata, CD_PROP_FLOAT2, loopstart, i, totloop));
    copy_v2_v2(*uv, texface->uv[0]);
    uv++;
    copy_v2_v2(*uv, texface->uv[1]);
    uv++;
    copy_v2_v2(*uv, texface->uv[2]);
    uv++;

    if (mf->v4) {
      copy_v2_v2(*uv, texface->uv[3]);
      uv++;
    }
  }

  for (int i = 0; i < numCol; i++) {
    MLoopCol *mloopcol = (MLoopCol *)CustomData_get_n_for_write(
        ldata, CD_PROP_BYTE_COLOR, loopstart, i, totloop);
    const MCol *mcol = (const MCol *)CustomData_get_n_for_write(
        fdata, CD_MCOL, findex, i, totface);

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
    float(*loop_normals)[3] = (float(*)[3])CustomData_get_for_write(
        ldata, loopstart, CD_NORMAL, totloop);
    const short(*tessloop_normals)[3] = (short(*)[3])CustomData_get_for_write(
        fdata, findex, CD_TESSLOOPNORMAL, totface);
    const int max = mf->v4 ? 4 : 3;

    for (int i = 0; i < max; i++, loop_normals++, tessloop_normals++) {
      normal_short_to_float_v3(*loop_normals, *tessloop_normals);
    }
  }

  if (CustomData_has_layer(fdata, CD_MDISPS)) {
    MDisps *ld = (MDisps *)CustomData_get_for_write(ldata, loopstart, CD_MDISPS, totloop);
    const MDisps *fd = (const MDisps *)CustomData_get_for_write(fdata, findex, CD_MDISPS, totface);
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
      const int side = int(sqrtf(float(fd->totdisp / corners)));
      const int side_sq = side * side;

      for (int i = 0; i < tot; i++, disps += side_sq, ld++) {
        ld->totdisp = side_sq;
        ld->level = int(logf(float(side) - 1.0f) / float(M_LN2)) + 1;

        if (ld->disps) {
          MEM_freeN(ld->disps);
        }

        ld->disps = (float(*)[3])MEM_malloc_arrayN(
            size_t(side_sq), sizeof(float[3]), "converted loop mdisps");
        if (fd->disps) {
          memcpy(ld->disps, disps, size_t(side_sq) * sizeof(float[3]));
        }
        else {
          memset(ld->disps, 0, size_t(side_sq) * sizeof(float[3]));
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
          ldata, CD_PROP_FLOAT2, CD_SET_DEFAULT, nullptr, totloop, fdata->layers[i].name);
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
                                     int *r_totpoly)
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
  mpoly = (MPoly *)CustomData_add_layer(pdata, CD_MPOLY, CD_SET_DEFAULT, nullptr, totpoly);
  int *material_indices = static_cast<int *>(
      CustomData_get_layer_named_for_write(pdata, CD_PROP_INT32, "material_index", totpoly));
  if (material_indices == nullptr) {
    material_indices = static_cast<int *>(CustomData_add_layer_named(
        pdata, CD_PROP_INT32, CD_SET_DEFAULT, nullptr, totpoly, "material_index"));
  }

  numTex = CustomData_number_of_layers(fdata, CD_MTFACE);
  numCol = CustomData_number_of_layers(fdata, CD_MCOL);

  totloop = 0;
  mf = mface;
  for (i = 0; i < totface_i; i++, mf++) {
    totloop += mf->v4 ? 4 : 3;
  }

  mloop = (MLoop *)CustomData_add_layer(ldata, CD_MLOOP, CD_SET_DEFAULT, nullptr, totloop);

  CustomData_to_bmeshpoly(fdata, ldata, totloop);

  if (id) {
    /* ensure external data is transferred */
    /* TODO(sergey): Use multiresModifier_ensure_external_read(). */
    CustomData_external_read(fdata, id, CD_MASK_MDISPS, totface_i);
  }

  eh = BLI_edgehash_new_ex(__func__, uint(totedge_i));

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

    material_indices[i] = mf->mat_nr;
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

    bm_corners_to_loops_ex(
        id, fdata, totface_i, ldata, mface, totloop, i, mp->loopstart, numTex, numCol);

    if (polyindex) {
      *polyindex = i;
      polyindex++;
    }
  }

  /* NOTE: we don't convert NGons at all, these are not even real ngons,
   * they have their own UVs, colors etc - it's more an editing feature. */

  BLI_edgehash_free(eh, nullptr);

  *r_totpoly = totpoly;
  *r_totloop = totloop;

#undef ME_FGON
}

static void update_active_fdata_layers(Mesh &mesh, CustomData *fdata, CustomData *ldata)
{
  int act;

  if (CustomData_has_layer(ldata, CD_PROP_FLOAT2)) {
    act = CustomData_get_active_layer(ldata, CD_PROP_FLOAT2);
    CustomData_set_layer_active(fdata, CD_MTFACE, act);

    act = CustomData_get_render_layer(ldata, CD_PROP_FLOAT2);
    CustomData_set_layer_render(fdata, CD_MTFACE, act);

    act = CustomData_get_clone_layer(ldata, CD_PROP_FLOAT2);
    CustomData_set_layer_clone(fdata, CD_MTFACE, act);

    act = CustomData_get_stencil_layer(ldata, CD_PROP_FLOAT2);
    CustomData_set_layer_stencil(fdata, CD_MTFACE, act);
  }

  if (CustomData_has_layer(ldata, CD_PROP_BYTE_COLOR)) {
    if (mesh.active_color_attribute != nullptr) {
      act = CustomData_get_named_layer(ldata, CD_PROP_BYTE_COLOR, mesh.active_color_attribute);
      CustomData_set_layer_active(fdata, CD_MCOL, act);
    }

    if (mesh.default_color_attribute != nullptr) {
      act = CustomData_get_named_layer(ldata, CD_PROP_BYTE_COLOR, mesh.default_color_attribute);
      CustomData_set_layer_render(fdata, CD_MCOL, act);
    }

    act = CustomData_get_clone_layer(ldata, CD_PROP_BYTE_COLOR);
    CustomData_set_layer_clone(fdata, CD_MCOL, act);

    act = CustomData_get_stencil_layer(ldata, CD_PROP_BYTE_COLOR);
    CustomData_set_layer_stencil(fdata, CD_MCOL, act);
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

  if (!LAYER_CMP(ldata, CD_PROP_FLOAT2, fdata, CD_MTFACE)) {
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

static void add_mface_layers(Mesh &mesh, CustomData *fdata, CustomData *ldata, int total)
{
  /* avoid accumulating extra layers */
  BLI_assert(!check_matching_legacy_layer_counts(fdata, ldata, false));

  for (int i = 0; i < ldata->totlayer; i++) {
    if (ldata->layers[i].type == CD_PROP_FLOAT2) {
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

  update_active_fdata_layers(mesh, fdata, ldata);
}

static void mesh_ensure_tessellation_customdata(Mesh *me)
{
  if (UNLIKELY((me->totface != 0) && (me->totpoly == 0))) {
    /* Pass, otherwise this function  clears 'mface' before
     * versioning 'mface -> mpoly' code kicks in #30583.
     *
     * Callers could also check but safer to do here - campbell */
  }
  else {
    const int tottex_original = CustomData_number_of_layers(&me->ldata, CD_PROP_FLOAT2);
    const int totcol_original = CustomData_number_of_layers(&me->ldata, CD_PROP_BYTE_COLOR);

    const int tottex_tessface = CustomData_number_of_layers(&me->fdata, CD_MTFACE);
    const int totcol_tessface = CustomData_number_of_layers(&me->fdata, CD_MCOL);

    if (tottex_tessface != tottex_original || totcol_tessface != totcol_original) {
      BKE_mesh_tessface_clear(me);

      add_mface_layers(*me, &me->fdata, &me->ldata, me->totface);

      /* TODO: add some `--debug-mesh` option. */
      if (G.debug & G_DEBUG) {
        /* NOTE(campbell): this warning may be un-called for if we are initializing the mesh for
         * the first time from #BMesh, rather than giving a warning about this we could be smarter
         * and check if there was any data to begin with, for now just print the warning with
         * some info to help troubleshoot what's going on. */
        printf(
            "%s: warning! Tessellation uvs or vcol data got out of sync, "
            "had to reset!\n    CD_MTFACE: %d != CD_PROP_FLOAT2: %d || CD_MCOL: %d != "
            "CD_PROP_BYTE_COLOR: "
            "%d\n",
            __func__,
            tottex_tessface,
            tottex_original,
            totcol_tessface,
            totcol_original);
      }
    }
  }
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
                           mesh->edges_for_write().data(),
                           (MFace *)CustomData_get_layer(&mesh->fdata, CD_MFACE),
                           &mesh->totloop,
                           &mesh->totpoly);

  mesh_ensure_tessellation_customdata(mesh);
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
    CustomData_set_layer_active(ldata, CD_PROP_FLOAT2, act);

    act = CustomData_get_render_layer(fdata, CD_MTFACE);
    CustomData_set_layer_render(ldata, CD_PROP_FLOAT2, act);

    act = CustomData_get_clone_layer(fdata, CD_MTFACE);
    CustomData_set_layer_clone(ldata, CD_PROP_FLOAT2, act);

    act = CustomData_get_stencil_layer(fdata, CD_MTFACE);
    CustomData_set_layer_stencil(ldata, CD_PROP_FLOAT2, act);
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
                           mesh->edges_for_write().data(),
                           (MFace *)CustomData_get_layer(&mesh->fdata, CD_MFACE),
                           &mesh->totloop,
                           &mesh->totpoly);

  CustomData_bmesh_do_versions_update_active_layers(&mesh->fdata, &mesh->ldata);

  mesh_ensure_tessellation_customdata(mesh);
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
  const int numUV = CustomData_number_of_layers(ldata, CD_PROP_FLOAT2);
  const int numCol = CustomData_number_of_layers(ldata, CD_PROP_BYTE_COLOR);
  const bool hasPCol = CustomData_has_layer(ldata, CD_PREVIEW_MLOOPCOL);
  const bool hasOrigSpace = CustomData_has_layer(ldata, CD_ORIGSPACE_MLOOP);
  const bool hasLoopNormal = CustomData_has_layer(ldata, CD_NORMAL);
  const bool hasLoopTangent = CustomData_has_layer(ldata, CD_TANGENT);
  int findex, i, j;
  const int *pidx;
  uint(*lidx)[4];

  for (i = 0; i < numUV; i++) {
    MTFace *texface = (MTFace *)CustomData_get_layer_n_for_write(fdata, CD_MTFACE, i, num_faces);
    const blender::float2 *uv = static_cast<const blender::float2 *>(
        CustomData_get_layer_n(ldata, CD_PROP_FLOAT2, i));

    for (findex = 0, pidx = polyindices, lidx = loopindices; findex < num_faces;
         pidx++, lidx++, findex++, texface++) {
      for (j = (mface ? mface[findex].v4 : (*lidx)[3]) ? 4 : 3; j--;) {
        copy_v2_v2(texface->uv[j], uv[(*lidx)[j]]);
      }
    }
  }

  for (i = 0; i < numCol; i++) {
    MCol(*mcol)[4] = (MCol(*)[4])CustomData_get_layer_n_for_write(fdata, CD_MCOL, i, num_faces);
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
    short(*face_normals)[4][3] = (short(*)[4][3])CustomData_get_layer(fdata, CD_TESSLOOPNORMAL);
    const float(*loop_normals)[3] = (const float(*)[3])CustomData_get_layer(ldata, CD_NORMAL);

    for (findex = 0, lidx = loopindices; findex < num_faces; lidx++, findex++, face_normals++) {
      for (j = (mface ? mface[findex].v4 : (*lidx)[3]) ? 4 : 3; j--;) {
        normal_float_to_short_v3((*face_normals)[j], loop_normals[(*lidx)[j]]);
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

      std::swap(mface->v1, mface->v2);
      std::swap(mface->v2, mface->v3);

      if (fdata) {
        CustomData_swap_corners(fdata, mfindex, corner_indices);
      }
    }
  }
  else if (nr == 4) {
    if (mface->v3 == 0 || mface->v4 == 0) {
      static int corner_indices[4] = {2, 3, 0, 1};

      std::swap(mface->v1, mface->v3);
      std::swap(mface->v2, mface->v4);

      if (fdata) {
        CustomData_swap_corners(fdata, mfindex, corner_indices);
      }
    }
  }

  return nr;
}

static int mesh_tessface_calc(Mesh &mesh,
                              CustomData *fdata,
                              CustomData *ldata,
                              CustomData *pdata,
                              float (*positions)[3],
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
  const int *material_indices = static_cast<const int *>(
      CustomData_get_layer_named(pdata, CD_PROP_INT32, "material_index"));

  /* Allocate the length of `totfaces`, avoid many small reallocation's,
   * if all faces are triangles it will be correct, `quads == 2x` allocations. */
  /* Take care since memory is _not_ zeroed so be sure to initialize each field. */
  mface_to_poly_map = (int *)MEM_malloc_arrayN(
      size_t(looptri_num), sizeof(*mface_to_poly_map), __func__);
  mface = (MFace *)MEM_malloc_arrayN(size_t(looptri_num), sizeof(*mface), __func__);
  lindices = (uint(*)[4])MEM_malloc_arrayN(size_t(looptri_num), sizeof(*lindices), __func__);

  mface_index = 0;
  mp = mpoly;
  for (poly_index = 0; poly_index < totpoly; poly_index++, mp++) {
    const uint mp_loopstart = uint(mp->loopstart);
    const uint mp_totloop = uint(mp->totloop);
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
    mf->mat_nr = material_indices ? material_indices[poly_index] : 0; \
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
    mf->mat_nr = material_indices ? material_indices[poly_index] : 0; \
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

      tris = (uint(*)[3])BLI_memarena_alloc(arena, sizeof(*tris) * size_t(totfilltri));
      projverts = (float(*)[2])BLI_memarena_alloc(arena, sizeof(*projverts) * size_t(mp_totloop));

      zero_v3(normal);

      /* Calculate the normal, flipped: to get a positive 2D cross product. */
      ml = mloop + mp_loopstart;
      co_prev = positions[ml[mp_totloop - 1].v];
      for (j = 0; j < mp_totloop; j++, ml++) {
        co_curr = positions[ml->v];
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
        mul_v2_m3v3(projverts[j], axis_mat, positions[ml->v]);
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

        mf->mat_nr = material_indices ? material_indices[poly_index] : 0;
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
    mface = (MFace *)MEM_reallocN(mface, sizeof(*mface) * size_t(totface));
    mface_to_poly_map = (int *)MEM_reallocN(mface_to_poly_map,
                                            sizeof(*mface_to_poly_map) * size_t(totface));
  }

  CustomData_add_layer(fdata, CD_MFACE, CD_ASSIGN, mface, totface);

  /* #CD_ORIGINDEX will contain an array of indices from tessellation-faces to the polygons
   * they are directly tessellated from. */
  CustomData_add_layer(fdata, CD_ORIGINDEX, CD_ASSIGN, mface_to_poly_map, totface);
  add_mface_layers(mesh, fdata, ldata, totface);

  /* NOTE: quad detection issue - fourth vertex-index vs fourth loop-index:
   * Polygons take care of their loops ordering, hence not of their vertices ordering.
   * Currently, our tfaces' fourth vertex index might be 0 even for a quad.
   * However, we know our fourth loop index is never 0 for quads
   * (because they are sorted for polygons, and our quads are still mere copies of their polygons).
   * So we pass nullptr as #MFace pointer, and #mesh_loops_to_tessdata
   * will use the fourth loop index as quad test. */
  mesh_loops_to_tessdata(fdata, ldata, nullptr, mface_to_poly_map, lindices, totface);

  /* NOTE: quad detection issue - fourth vert-index vs fourth loop-index:
   * ...However, most #TFace code uses `MFace->v4 == 0` test to check whether it is a tri or quad.
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
  mesh->totface = mesh_tessface_calc(*mesh,
                                     &mesh->fdata,
                                     &mesh->ldata,
                                     &mesh->pdata,
                                     BKE_mesh_vert_positions_for_write(mesh),
                                     mesh->totface,
                                     mesh->totloop,
                                     mesh->totpoly);

  mesh_ensure_tessellation_customdata(mesh);
}

void BKE_mesh_tessface_ensure(struct Mesh *mesh)
{
  if (mesh->totpoly && mesh->totface == 0) {
    BKE_mesh_tessface_calc(mesh);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Face Set Conversion
 * \{ */

void BKE_mesh_legacy_face_set_from_generic(blender::MutableSpan<CustomDataLayer> poly_layers)
{
  using namespace blender;
  bool changed = false;
  for (CustomDataLayer &layer : poly_layers) {
    if (StringRef(layer.name) == ".sculpt_face_set") {
      layer.type = CD_SCULPT_FACE_SETS;
      layer.name[0] = '\0';
      changed = true;
      break;
    }
  }
  if (!changed) {
    return;
  }
  /* #CustomData expects the layers to be sorted in increasing order based on type. */
  std::stable_sort(
      poly_layers.begin(),
      poly_layers.end(),
      [](const CustomDataLayer &a, const CustomDataLayer &b) { return a.type < b.type; });
}

void BKE_mesh_legacy_face_set_to_generic(Mesh *mesh)
{
  using namespace blender;
  if (mesh->attributes().contains(".sculpt_face_set")) {
    return;
  }
  void *faceset_data = nullptr;
  for (const int i : IndexRange(mesh->pdata.totlayer)) {
    if (mesh->pdata.layers[i].type == CD_SCULPT_FACE_SETS) {
      faceset_data = mesh->pdata.layers[i].data;
      mesh->pdata.layers[i].data = nullptr;
      CustomData_free_layer(&mesh->pdata, CD_SCULPT_FACE_SETS, mesh->totpoly, i);
      break;
    }
  }
  if (faceset_data != nullptr) {
    CustomData_add_layer_named(
        &mesh->pdata, CD_PROP_INT32, CD_ASSIGN, faceset_data, mesh->totpoly, ".sculpt_face_set");
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Bevel Weight Conversion
 * \{ */

void BKE_mesh_legacy_bevel_weight_from_layers(Mesh *mesh)
{
  using namespace blender;
  MutableSpan<MVert> verts(mesh->mvert, mesh->totvert);
  if (const float *weights = static_cast<const float *>(
          CustomData_get_layer(&mesh->vdata, CD_BWEIGHT))) {
    mesh->cd_flag |= ME_CDFLAG_VERT_BWEIGHT;
    for (const int i : verts.index_range()) {
      verts[i].bweight_legacy = std::clamp(weights[i], 0.0f, 1.0f) * 255.0f;
    }
  }
  else {
    mesh->cd_flag &= ~ME_CDFLAG_VERT_BWEIGHT;
    for (const int i : verts.index_range()) {
      verts[i].bweight_legacy = 0;
    }
  }
  MutableSpan<MEdge> edges = mesh->edges_for_write();
  if (const float *weights = static_cast<const float *>(
          CustomData_get_layer(&mesh->edata, CD_BWEIGHT))) {
    mesh->cd_flag |= ME_CDFLAG_EDGE_BWEIGHT;
    for (const int i : edges.index_range()) {
      edges[i].bweight_legacy = std::clamp(weights[i], 0.0f, 1.0f) * 255.0f;
    }
  }
  else {
    mesh->cd_flag &= ~ME_CDFLAG_EDGE_BWEIGHT;
    for (const int i : edges.index_range()) {
      edges[i].bweight_legacy = 0;
    }
  }
}

void BKE_mesh_legacy_bevel_weight_to_layers(Mesh *mesh)
{
  using namespace blender;
  if (mesh->mvert && !CustomData_has_layer(&mesh->vdata, CD_BWEIGHT)) {
    const Span<MVert> verts(mesh->mvert, mesh->totvert);
    if (mesh->cd_flag & ME_CDFLAG_VERT_BWEIGHT) {
      float *weights = static_cast<float *>(
          CustomData_add_layer(&mesh->vdata, CD_BWEIGHT, CD_CONSTRUCT, nullptr, verts.size()));
      for (const int i : verts.index_range()) {
        weights[i] = verts[i].bweight_legacy / 255.0f;
      }
    }
  }

  const Span<MEdge> edges = mesh->edges();
  if (!CustomData_has_layer(&mesh->edata, CD_BWEIGHT)) {
    if (mesh->cd_flag & ME_CDFLAG_EDGE_BWEIGHT) {
      float *weights = static_cast<float *>(
          CustomData_add_layer(&mesh->edata, CD_BWEIGHT, CD_CONSTRUCT, nullptr, edges.size()));
      for (const int i : edges.index_range()) {
        weights[i] = edges[i].bweight_legacy / 255.0f;
      }
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Edge Crease Conversion
 * \{ */

void BKE_mesh_legacy_edge_crease_from_layers(Mesh *mesh)
{
  using namespace blender;
  MutableSpan<MEdge> edges = mesh->edges_for_write();
  if (const float *creases = static_cast<const float *>(
          CustomData_get_layer(&mesh->edata, CD_CREASE))) {
    mesh->cd_flag |= ME_CDFLAG_EDGE_CREASE;
    for (const int i : edges.index_range()) {
      edges[i].crease_legacy = std::clamp(creases[i], 0.0f, 1.0f) * 255.0f;
    }
  }
  else {
    mesh->cd_flag &= ~ME_CDFLAG_EDGE_CREASE;
    for (const int i : edges.index_range()) {
      edges[i].crease_legacy = 0;
    }
  }
}

void BKE_mesh_legacy_edge_crease_to_layers(Mesh *mesh)
{
  using namespace blender;
  if (CustomData_has_layer(&mesh->edata, CD_CREASE)) {
    return;
  }
  const Span<MEdge> edges = mesh->edges();
  if (mesh->cd_flag & ME_CDFLAG_EDGE_CREASE) {
    float *creases = static_cast<float *>(
        CustomData_add_layer(&mesh->edata, CD_CREASE, CD_CONSTRUCT, nullptr, edges.size()));
    for (const int i : edges.index_range()) {
      creases[i] = edges[i].crease_legacy / 255.0f;
    }
  }
}

/* -------------------------------------------------------------------- */
/** \name Sharp Edge Conversion
 * \{ */

void BKE_mesh_legacy_sharp_edges_to_flags(Mesh *mesh)
{
  using namespace blender;
  MutableSpan<MEdge> edges = mesh->edges_for_write();
  if (const bool *sharp_edges = static_cast<const bool *>(
          CustomData_get_layer_named(&mesh->edata, CD_PROP_BOOL, "sharp_edge"))) {
    threading::parallel_for(edges.index_range(), 4096, [&](const IndexRange range) {
      for (const int i : range) {
        SET_FLAG_FROM_TEST(edges[i].flag, sharp_edges[i], ME_SHARP);
      }
    });
  }
  else {
    for (const int i : edges.index_range()) {
      edges[i].flag &= ~ME_SHARP;
    }
  }
}

void BKE_mesh_legacy_sharp_edges_from_flags(Mesh *mesh)
{
  using namespace blender;
  using namespace blender::bke;
  const Span<MEdge> edges = mesh->edges();
  MutableAttributeAccessor attributes = mesh->attributes_for_write();
  if (attributes.contains("sharp_edge")) {
    return;
  }
  if (std::any_of(
          edges.begin(), edges.end(), [](const MEdge &edge) { return edge.flag & ME_SHARP; })) {
    SpanAttributeWriter<bool> sharp_edges = attributes.lookup_or_add_for_write_only_span<bool>(
        "sharp_edge", ATTR_DOMAIN_EDGE);
    threading::parallel_for(edges.index_range(), 4096, [&](const IndexRange range) {
      for (const int i : range) {
        sharp_edges.span[i] = edges[i].flag & ME_SHARP;
      }
    });
    sharp_edges.finish();
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Hide Attribute and Legacy Flag Conversion
 * \{ */

void BKE_mesh_legacy_convert_hide_layers_to_flags(Mesh *mesh)
{
  using namespace blender;
  using namespace blender::bke;
  const AttributeAccessor attributes = mesh->attributes();

  MutableSpan<MVert> verts(mesh->mvert, mesh->totvert);
  const VArray<bool> hide_vert = attributes.lookup_or_default<bool>(
      ".hide_vert", ATTR_DOMAIN_POINT, false);
  threading::parallel_for(verts.index_range(), 4096, [&](IndexRange range) {
    for (const int i : range) {
      SET_FLAG_FROM_TEST(verts[i].flag_legacy, hide_vert[i], ME_HIDE);
    }
  });

  MutableSpan<MEdge> edges = mesh->edges_for_write();
  const VArray<bool> hide_edge = attributes.lookup_or_default<bool>(
      ".hide_edge", ATTR_DOMAIN_EDGE, false);
  threading::parallel_for(edges.index_range(), 4096, [&](IndexRange range) {
    for (const int i : range) {
      SET_FLAG_FROM_TEST(edges[i].flag, hide_edge[i], ME_HIDE);
    }
  });

  MutableSpan<MPoly> polys = mesh->polys_for_write();
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
  MutableAttributeAccessor attributes = mesh->attributes_for_write();
  if (!mesh->mvert || attributes.contains(".hide_vert") || attributes.contains(".hide_edge") ||
      attributes.contains(".hide_poly")) {
    return;
  }
  const Span<MVert> verts(mesh->mvert, mesh->totvert);
  if (std::any_of(verts.begin(), verts.end(), [](const MVert &vert) {
        return vert.flag_legacy & ME_HIDE;
      })) {
    SpanAttributeWriter<bool> hide_vert = attributes.lookup_or_add_for_write_only_span<bool>(
        ".hide_vert", ATTR_DOMAIN_POINT);
    threading::parallel_for(verts.index_range(), 4096, [&](IndexRange range) {
      for (const int i : range) {
        hide_vert.span[i] = verts[i].flag_legacy & ME_HIDE;
      }
    });
    hide_vert.finish();
  }

  const Span<MEdge> edges = mesh->edges();
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

  const Span<MPoly> polys = mesh->polys();
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
  const AttributeAccessor attributes = mesh->attributes();
  MutableSpan<MPoly> polys = mesh->polys_for_write();
  const VArray<int> material_indices = attributes.lookup_or_default<int>(
      "material_index", ATTR_DOMAIN_FACE, 0);
  threading::parallel_for(polys.index_range(), 4096, [&](IndexRange range) {
    for (const int i : range) {
      polys[i].mat_nr_legacy = material_indices[i];
    }
  });
}

void BKE_mesh_legacy_convert_mpoly_to_material_indices(Mesh *mesh)
{
  using namespace blender;
  using namespace blender::bke;
  MutableAttributeAccessor attributes = mesh->attributes_for_write();
  if (attributes.contains("material_index")) {
    return;
  }
  const Span<MPoly> polys = mesh->polys();
  if (std::any_of(
          polys.begin(), polys.end(), [](const MPoly &poly) { return poly.mat_nr_legacy != 0; })) {
    SpanAttributeWriter<int> material_indices = attributes.lookup_or_add_for_write_only_span<int>(
        "material_index", ATTR_DOMAIN_FACE);
    threading::parallel_for(polys.index_range(), 4096, [&](IndexRange range) {
      for (const int i : range) {
        material_indices.span[i] = polys[i].mat_nr_legacy;
      }
    });
    material_indices.finish();
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Generic UV Map Conversion
 * \{ */

void BKE_mesh_legacy_convert_uvs_to_struct(
    Mesh *mesh,
    blender::ResourceScope &temp_mloopuv_for_convert,
    blender::Vector<CustomDataLayer, 16> &loop_layers_to_write)
{
  using namespace blender;
  using namespace blender::bke;
  Vector<CustomDataLayer, 16> new_layer_to_write;

  /* Don't write the boolean UV map sublayers which will be written in the legacy #MLoopUV type. */
  Set<std::string> uv_sublayers_to_skip;
  char vert_name[MAX_CUSTOMDATA_LAYER_NAME];
  char edge_name[MAX_CUSTOMDATA_LAYER_NAME];
  char pin_name[MAX_CUSTOMDATA_LAYER_NAME];
  for (const CustomDataLayer &layer : loop_layers_to_write) {
    if (layer.type == CD_PROP_FLOAT2) {
      uv_sublayers_to_skip.add_multiple_new(
          {BKE_uv_map_vert_select_name_get(layer.name, vert_name),
           BKE_uv_map_edge_select_name_get(layer.name, edge_name),
           BKE_uv_map_pin_name_get(layer.name, pin_name)});
    }
  }

  for (const CustomDataLayer &layer : loop_layers_to_write) {
    if (layer.name[0] && uv_sublayers_to_skip.contains_as(layer.name)) {
      continue;
    }
    if (layer.type != CD_PROP_FLOAT2) {
      new_layer_to_write.append(layer);
      continue;
    }
    const Span<float2> coords{static_cast<const float2 *>(layer.data), mesh->totloop};
    CustomDataLayer mloopuv_layer = layer;
    mloopuv_layer.type = CD_MLOOPUV;
    MutableSpan<MLoopUV> mloopuv = temp_mloopuv_for_convert.construct<Array<MLoopUV>>(
        mesh->totloop);
    mloopuv_layer.data = mloopuv.data();

    char buffer[MAX_CUSTOMDATA_LAYER_NAME];
    const bool *vert_selection = static_cast<const bool *>(CustomData_get_layer_named(
        &mesh->ldata, CD_PROP_BOOL, BKE_uv_map_vert_select_name_get(layer.name, buffer)));
    const bool *edge_selection = static_cast<const bool *>(CustomData_get_layer_named(
        &mesh->ldata, CD_PROP_BOOL, BKE_uv_map_edge_select_name_get(layer.name, buffer)));
    const bool *pin = static_cast<const bool *>(CustomData_get_layer_named(
        &mesh->ldata, CD_PROP_BOOL, BKE_uv_map_pin_name_get(layer.name, buffer)));

    threading::parallel_for(mloopuv.index_range(), 2048, [&](IndexRange range) {
      for (const int i : range) {
        copy_v2_v2(mloopuv[i].uv, coords[i]);
        SET_FLAG_FROM_TEST(mloopuv[i].flag, vert_selection && vert_selection[i], MLOOPUV_VERTSEL);
        SET_FLAG_FROM_TEST(mloopuv[i].flag, edge_selection && edge_selection[i], MLOOPUV_EDGESEL);
        SET_FLAG_FROM_TEST(mloopuv[i].flag, pin && pin[i], MLOOPUV_PINNED);
      }
    });
    new_layer_to_write.append(mloopuv_layer);
  }

  /* #CustomData expects the layers to be sorted in increasing order based on type. */
  std::stable_sort(
      new_layer_to_write.begin(),
      new_layer_to_write.end(),
      [](const CustomDataLayer &a, const CustomDataLayer &b) { return a.type < b.type; });

  loop_layers_to_write = new_layer_to_write;
  mesh->ldata.totlayer = new_layer_to_write.size();
  mesh->ldata.maxlayer = mesh->ldata.totlayer;
}

void BKE_mesh_legacy_convert_uvs_to_generic(Mesh *mesh)
{
  using namespace blender;
  using namespace blender::bke;
  if (!CustomData_has_layer(&mesh->ldata, CD_MLOOPUV)) {
    return;
  }

  /* Store layer names since they will be removed, used to set the active status of new layers.
   * Use intermediate #StringRef because the names can be null. */
  const std::string active_uv = StringRef(
      CustomData_get_active_layer_name(&mesh->ldata, CD_MLOOPUV));
  const std::string default_uv = StringRef(
      CustomData_get_render_layer_name(&mesh->ldata, CD_MLOOPUV));

  Vector<std::string> uv_layers_to_convert;
  for (const int uv_layer_i : IndexRange(CustomData_number_of_layers(&mesh->ldata, CD_MLOOPUV))) {
    uv_layers_to_convert.append(CustomData_get_layer_name(&mesh->ldata, CD_MLOOPUV, uv_layer_i));
  }

  for (const StringRefNull name : uv_layers_to_convert) {
    const MLoopUV *mloopuv = static_cast<const MLoopUV *>(
        CustomData_get_layer_named(&mesh->ldata, CD_MLOOPUV, name.c_str()));
    const uint32_t needed_boolean_attributes = threading::parallel_reduce(
        IndexRange(mesh->totloop),
        4096,
        0,
        [&](const IndexRange range, uint32_t init) {
          for (const int i : range) {
            init |= mloopuv[i].flag;
          }
          return init;
        },
        [](const uint32_t a, const uint32_t b) { return a | b; });

    float2 *coords = static_cast<float2 *>(
        MEM_malloc_arrayN(mesh->totloop, sizeof(float2), __func__));
    bool *vert_selection = nullptr;
    bool *edge_selection = nullptr;
    bool *pin = nullptr;
    if (needed_boolean_attributes & MLOOPUV_VERTSEL) {
      vert_selection = static_cast<bool *>(
          MEM_malloc_arrayN(mesh->totloop, sizeof(bool), __func__));
    }
    if (needed_boolean_attributes & MLOOPUV_EDGESEL) {
      edge_selection = static_cast<bool *>(
          MEM_malloc_arrayN(mesh->totloop, sizeof(bool), __func__));
    }
    if (needed_boolean_attributes & MLOOPUV_PINNED) {
      pin = static_cast<bool *>(MEM_malloc_arrayN(mesh->totloop, sizeof(bool), __func__));
    }

    threading::parallel_for(IndexRange(mesh->totloop), 4096, [&](IndexRange range) {
      for (const int i : range) {
        coords[i] = mloopuv[i].uv;
      }
      if (vert_selection) {
        for (const int i : range) {
          vert_selection[i] = mloopuv[i].flag & MLOOPUV_VERTSEL;
        }
      }
      if (edge_selection) {
        for (const int i : range) {
          edge_selection[i] = mloopuv[i].flag & MLOOPUV_EDGESEL;
        }
      }
      if (pin) {
        for (const int i : range) {
          pin[i] = mloopuv[i].flag & MLOOPUV_PINNED;
        }
      }
    });

    CustomData_free_layer_named(&mesh->ldata, name.c_str(), mesh->totloop);
    CustomData_add_layer_named(
        &mesh->ldata, CD_PROP_FLOAT2, CD_ASSIGN, coords, mesh->totloop, name.c_str());
    char buffer[MAX_CUSTOMDATA_LAYER_NAME];
    if (vert_selection) {
      CustomData_add_layer_named(&mesh->ldata,
                                 CD_PROP_BOOL,
                                 CD_ASSIGN,
                                 vert_selection,
                                 mesh->totloop,
                                 BKE_uv_map_vert_select_name_get(name.c_str(), buffer));
    }
    if (edge_selection) {
      CustomData_add_layer_named(&mesh->ldata,
                                 CD_PROP_BOOL,
                                 CD_ASSIGN,
                                 edge_selection,
                                 mesh->totloop,
                                 BKE_uv_map_edge_select_name_get(name.c_str(), buffer));
    }
    if (pin) {
      CustomData_add_layer_named(&mesh->ldata,
                                 CD_PROP_BOOL,
                                 CD_ASSIGN,
                                 pin,
                                 mesh->totloop,
                                 BKE_uv_map_pin_name_get(name.c_str(), buffer));
    }
  }

  CustomData_set_layer_active_index(
      &mesh->ldata,
      CD_PROP_FLOAT2,
      CustomData_get_named_layer_index(&mesh->ldata, CD_PROP_FLOAT2, active_uv.c_str()));
  CustomData_set_layer_render_index(
      &mesh->ldata,
      CD_PROP_FLOAT2,
      CustomData_get_named_layer_index(&mesh->ldata, CD_PROP_FLOAT2, default_uv.c_str()));
}

/** \name Selection Attribute and Legacy Flag Conversion
 * \{ */

void BKE_mesh_legacy_convert_selection_layers_to_flags(Mesh *mesh)
{
  using namespace blender;
  using namespace blender::bke;
  const AttributeAccessor attributes = mesh->attributes();

  MutableSpan<MVert> verts(mesh->mvert, mesh->totvert);
  const VArray<bool> select_vert = attributes.lookup_or_default<bool>(
      ".select_vert", ATTR_DOMAIN_POINT, false);
  threading::parallel_for(verts.index_range(), 4096, [&](IndexRange range) {
    for (const int i : range) {
      SET_FLAG_FROM_TEST(verts[i].flag_legacy, select_vert[i], SELECT);
    }
  });

  MutableSpan<MEdge> edges = mesh->edges_for_write();
  const VArray<bool> select_edge = attributes.lookup_or_default<bool>(
      ".select_edge", ATTR_DOMAIN_EDGE, false);
  threading::parallel_for(edges.index_range(), 4096, [&](IndexRange range) {
    for (const int i : range) {
      SET_FLAG_FROM_TEST(edges[i].flag, select_edge[i], SELECT);
    }
  });

  MutableSpan<MPoly> polys = mesh->polys_for_write();
  const VArray<bool> select_poly = attributes.lookup_or_default<bool>(
      ".select_poly", ATTR_DOMAIN_FACE, false);
  threading::parallel_for(polys.index_range(), 4096, [&](IndexRange range) {
    for (const int i : range) {
      SET_FLAG_FROM_TEST(polys[i].flag, select_poly[i], ME_FACE_SEL);
    }
  });
}

void BKE_mesh_legacy_convert_flags_to_selection_layers(Mesh *mesh)
{
  using namespace blender;
  using namespace blender::bke;
  MutableAttributeAccessor attributes = mesh->attributes_for_write();
  if (!mesh->mvert || attributes.contains(".select_vert") || attributes.contains(".select_edge") ||
      attributes.contains(".select_poly")) {
    return;
  }

  const Span<MVert> verts(mesh->mvert, mesh->totvert);
  if (std::any_of(verts.begin(), verts.end(), [](const MVert &vert) {
        return vert.flag_legacy & SELECT;
      })) {
    SpanAttributeWriter<bool> select_vert = attributes.lookup_or_add_for_write_only_span<bool>(
        ".select_vert", ATTR_DOMAIN_POINT);
    threading::parallel_for(verts.index_range(), 4096, [&](IndexRange range) {
      for (const int i : range) {
        select_vert.span[i] = verts[i].flag_legacy & SELECT;
      }
    });
    select_vert.finish();
  }

  const Span<MEdge> edges = mesh->edges();
  if (std::any_of(
          edges.begin(), edges.end(), [](const MEdge &edge) { return edge.flag & SELECT; })) {
    SpanAttributeWriter<bool> select_edge = attributes.lookup_or_add_for_write_only_span<bool>(
        ".select_edge", ATTR_DOMAIN_EDGE);
    threading::parallel_for(edges.index_range(), 4096, [&](IndexRange range) {
      for (const int i : range) {
        select_edge.span[i] = edges[i].flag & SELECT;
      }
    });
    select_edge.finish();
  }

  const Span<MPoly> polys = mesh->polys();
  if (std::any_of(
          polys.begin(), polys.end(), [](const MPoly &poly) { return poly.flag & ME_FACE_SEL; })) {
    SpanAttributeWriter<bool> select_poly = attributes.lookup_or_add_for_write_only_span<bool>(
        ".select_poly", ATTR_DOMAIN_FACE);
    threading::parallel_for(polys.index_range(), 4096, [&](IndexRange range) {
      for (const int i : range) {
        select_poly.span[i] = polys[i].flag & ME_FACE_SEL;
      }
    });
    select_poly.finish();
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Loose Edges
 * \{ */

void BKE_mesh_legacy_convert_loose_edges_to_flag(Mesh *mesh)
{
  using namespace blender;
  using namespace blender::bke;

  const LooseEdgeCache &loose_edges = mesh->loose_edges();
  MutableSpan<MEdge> edges = mesh->edges_for_write();
  threading::parallel_for(edges.index_range(), 4096, [&](const IndexRange range) {
    if (loose_edges.count == 0) {
      for (const int64_t i : range) {
        edges[i].flag &= ~ME_LOOSEEDGE;
      }
    }
    else {
      for (const int64_t i : range) {
        SET_FLAG_FROM_TEST(edges[i].flag, loose_edges.is_loose_bits[i], ME_LOOSEEDGE);
      }
    }
  });
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Vertex and Position Conversion
 * \{ */

MVert *BKE_mesh_legacy_convert_positions_to_verts(
    Mesh *mesh,
    blender::ResourceScope &temp_arrays_for_convert,
    blender::Vector<CustomDataLayer, 16> &vert_layers_to_write)
{
  using namespace blender;

  const Span<float3> positions = mesh->vert_positions();

  CustomDataLayer mvert_layer{};
  mvert_layer.type = CD_MVERT;
  MutableSpan<MVert> verts = temp_arrays_for_convert.construct<Array<MVert>>(mesh->totvert);
  mvert_layer.data = verts.data();

  threading::parallel_for(verts.index_range(), 2048, [&](IndexRange range) {
    for (const int i : range) {
      copy_v3_v3(verts[i].co_legacy, positions[i]);
    }
  });

  vert_layers_to_write.append(mvert_layer);
  return verts.data();
}

void BKE_mesh_legacy_convert_verts_to_positions(Mesh *mesh)
{
  using namespace blender;
  using namespace blender::bke;
  if (!mesh->mvert || CustomData_get_layer_named(&mesh->vdata, CD_PROP_FLOAT3, "position")) {
    return;
  }

  const Span<MVert> verts(static_cast<const MVert *>(CustomData_get_layer(&mesh->vdata, CD_MVERT)),
                          mesh->totvert);
  MutableSpan<float3> positions(
      static_cast<float3 *>(CustomData_add_layer_named(
          &mesh->vdata, CD_PROP_FLOAT3, CD_CONSTRUCT, nullptr, mesh->totvert, "position")),
      mesh->totvert);
  threading::parallel_for(verts.index_range(), 2048, [&](IndexRange range) {
    for (const int i : range) {
      positions[i] = verts[i].co_legacy;
    }
  });

  CustomData_free_layers(&mesh->vdata, CD_MVERT, mesh->totvert);
  mesh->mvert = nullptr;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Attribute Active Flag to String Conversion
 * \{ */

void BKE_mesh_legacy_attribute_flags_to_strings(Mesh *mesh)
{
  using namespace blender;
  /* It's not clear whether the active/render status was stored in the dedicated flags or in the
   * generic CustomData layer indices, so convert from both, preferring the explicit flags. */

  auto active_from_flags = [&](const CustomData &data) {
    if (!mesh->active_color_attribute) {
      for (const int i : IndexRange(data.totlayer)) {
        if (data.layers[i].flag & CD_FLAG_COLOR_ACTIVE) {
          mesh->active_color_attribute = BLI_strdup(data.layers[i].name);
        }
      }
    }
  };
  auto active_from_indices = [&](const CustomData &data) {
    if (!mesh->active_color_attribute) {
      const int i = CustomData_get_active_layer_index(&data, CD_PROP_COLOR);
      if (i != -1) {
        mesh->active_color_attribute = BLI_strdup(data.layers[i].name);
      }
    }
    if (!mesh->active_color_attribute) {
      const int i = CustomData_get_active_layer_index(&data, CD_PROP_BYTE_COLOR);
      if (i != -1) {
        mesh->active_color_attribute = BLI_strdup(data.layers[i].name);
      }
    }
  };
  auto default_from_flags = [&](const CustomData &data) {
    if (!mesh->default_color_attribute) {
      for (const int i : IndexRange(data.totlayer)) {
        if (data.layers[i].flag & CD_FLAG_COLOR_RENDER) {
          mesh->default_color_attribute = BLI_strdup(data.layers[i].name);
        }
      }
    }
  };
  auto default_from_indices = [&](const CustomData &data) {
    if (!mesh->default_color_attribute) {
      const int i = CustomData_get_render_layer_index(&data, CD_PROP_COLOR);
      if (i != -1) {
        mesh->default_color_attribute = BLI_strdup(data.layers[i].name);
      }
    }
    if (!mesh->default_color_attribute) {
      const int i = CustomData_get_render_layer_index(&data, CD_PROP_BYTE_COLOR);
      if (i != -1) {
        mesh->default_color_attribute = BLI_strdup(data.layers[i].name);
      }
    }
  };

  active_from_flags(mesh->vdata);
  active_from_flags(mesh->ldata);
  active_from_indices(mesh->vdata);
  active_from_indices(mesh->ldata);

  default_from_flags(mesh->vdata);
  default_from_flags(mesh->ldata);
  default_from_indices(mesh->vdata);
  default_from_indices(mesh->ldata);
}

void BKE_mesh_legacy_attribute_strings_to_flags(Mesh *mesh)
{
  using namespace blender;
  CustomData *vdata = &mesh->vdata;
  CustomData *ldata = &mesh->ldata;

  CustomData_clear_layer_flag(
      vdata, CD_PROP_BYTE_COLOR, CD_FLAG_COLOR_ACTIVE | CD_FLAG_COLOR_RENDER);
  CustomData_clear_layer_flag(ldata, CD_PROP_COLOR, CD_FLAG_COLOR_ACTIVE | CD_FLAG_COLOR_RENDER);

  if (const char *name = mesh->active_color_attribute) {
    int i;
    if ((i = CustomData_get_named_layer_index(vdata, CD_PROP_BYTE_COLOR, name)) != -1) {
      CustomData_set_layer_active_index(vdata, CD_PROP_BYTE_COLOR, i);
      vdata->layers[i].flag |= CD_FLAG_COLOR_ACTIVE;
    }
    else if ((i = CustomData_get_named_layer_index(vdata, CD_PROP_COLOR, name)) != -1) {
      CustomData_set_layer_active_index(vdata, CD_PROP_COLOR, i);
      vdata->layers[i].flag |= CD_FLAG_COLOR_ACTIVE;
    }
    else if ((i = CustomData_get_named_layer_index(ldata, CD_PROP_BYTE_COLOR, name)) != -1) {
      CustomData_set_layer_active_index(ldata, CD_PROP_BYTE_COLOR, i);
      ldata->layers[i].flag |= CD_FLAG_COLOR_ACTIVE;
    }
    else if ((i = CustomData_get_named_layer_index(ldata, CD_PROP_COLOR, name)) != -1) {
      CustomData_set_layer_active_index(ldata, CD_PROP_COLOR, i);
      ldata->layers[i].flag |= CD_FLAG_COLOR_ACTIVE;
    }
  }
  if (const char *name = mesh->default_color_attribute) {
    int i;
    if ((i = CustomData_get_named_layer_index(vdata, CD_PROP_BYTE_COLOR, name)) != -1) {
      CustomData_set_layer_render_index(vdata, CD_PROP_BYTE_COLOR, i);
      vdata->layers[i].flag |= CD_FLAG_COLOR_RENDER;
    }
    else if ((i = CustomData_get_named_layer_index(vdata, CD_PROP_COLOR, name)) != -1) {
      CustomData_set_layer_render_index(vdata, CD_PROP_COLOR, i);
      vdata->layers[i].flag |= CD_FLAG_COLOR_RENDER;
    }
    else if ((i = CustomData_get_named_layer_index(ldata, CD_PROP_BYTE_COLOR, name)) != -1) {
      CustomData_set_layer_render_index(ldata, CD_PROP_BYTE_COLOR, i);
      ldata->layers[i].flag |= CD_FLAG_COLOR_RENDER;
    }
    else if ((i = CustomData_get_named_layer_index(ldata, CD_PROP_COLOR, name)) != -1) {
      CustomData_set_layer_render_index(ldata, CD_PROP_COLOR, i);
      ldata->layers[i].flag |= CD_FLAG_COLOR_RENDER;
    }
  }
}

/** \} */
