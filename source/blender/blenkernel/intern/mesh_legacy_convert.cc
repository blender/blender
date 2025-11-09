/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 *
 * Functions to convert mesh data to and from legacy formats like #MFace.
 */

#define DNA_DEPRECATED_ALLOW

#include "MEM_guardedalloc.h"

#include "DNA_customdata_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"

#include "BLI_array_utils.hh"
#include "BLI_listbase.h"
#include "BLI_map.hh"
#include "BLI_math_geom.h"
#include "BLI_math_matrix.h"
#include "BLI_math_rotation.h"
#include "BLI_math_vector_types.hh"
#include "BLI_memarena.h"
#include "BLI_multi_value_map.hh"
#include "BLI_ordered_edge.hh"
#include "BLI_polyfill_2d.h"
#include "BLI_string.h"
#include "BLI_string_utf8.h"
#include "BLI_task.hh"
#include "BLI_utildefines.h"

#include "BKE_attribute.h"
#include "BKE_attribute.hh"
#include "BKE_customdata.hh"
#include "BKE_global.hh"
#include "BKE_idprop.hh"
#include "BKE_lib_id.hh"
#include "BKE_main.hh"
#include "BKE_main_namemap.hh"
#include "BKE_mesh.hh"
#include "BKE_mesh_legacy_convert.hh"
#include "BKE_modifier.hh"
#include "BKE_multires.hh"
#include "BKE_node.hh"
#include "BKE_node_runtime.hh"
#include "BKE_node_tree_update.hh"

#include "BLT_translation.hh"

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
static void to_edgesort(EdgeSort *ed, uint v1, uint v2, char is_loose, short is_draw)
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
  const EdgeSort *x1 = static_cast<const EdgeSort *>(v1);
  const EdgeSort *x2 = static_cast<const EdgeSort *>(v2);

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
                                  int faces_num,
                                  MEdge **r_medge,
                                  int *r_totedge)
{
  const MPoly *mpoly;
  const MFace *mface;
  MEdge *edges, *edge;
  EdgeSort *edsort, *ed;
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
    *r_medge = nullptr;
    *r_totedge = 0;
    return;
  }

  ed = edsort = MEM_malloc_arrayN<EdgeSort>(totedge, "EdgeSort");

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

  qsort(edsort, totedge, sizeof(EdgeSort), vergedgesort);

  /* count final amount */
  for (a = totedge, ed = edsort; a > 1; a--, ed++) {
    /* edge is unique when it differs from next edge, or is last */
    if (ed->v1 != (ed + 1)->v1 || ed->v2 != (ed + 1)->v2) {
      totedge_final++;
    }
  }
  totedge_final++;

  edges = MEM_calloc_arrayN<MEdge>(totedge_final, __func__);

  for (a = totedge, edge = edges, ed = edsort; a > 1; a--, ed++) {
    /* edge is unique when it differs from next edge, or is last */
    if (ed->v1 != (ed + 1)->v1 || ed->v2 != (ed + 1)->v2) {
      edge->v1 = ed->v1;
      edge->v2 = ed->v2;

      /* order is swapped so extruding this edge as a surface won't flip face normals
       * with cyclic curves */
      if (ed->v1 + 1 != ed->v2) {
        std::swap(edge->v1, edge->v2);
      }
      edge++;
    }
    else {
      /* Equal edge, merge the draw-flag. */
      (ed + 1)->is_draw |= ed->is_draw;
    }
  }
  /* last edge */
  edge->v1 = ed->v1;
  edge->v2 = ed->v2;

  MEM_freeN(edsort);

  /* set edge members of mloops */
  blender::Map<blender::OrderedEdge, int> hash;
  hash.reserve(totedge_final);
  for (edge_index = 0, edge = edges; edge_index < totedge_final; edge_index++, edge++) {
    hash.add({edge->v1, edge->v2}, edge_index);
  }

  mpoly = allpoly;
  for (a = 0; a < faces_num; a++, mpoly++) {
    MLoop *ml, *ml_next;
    int i = mpoly->totloop;

    ml_next = allloop + mpoly->loopstart; /* first loop */
    ml = &ml_next[i - 1];                 /* last loop */

    while (i-- != 0) {
      ml->e = hash.lookup({ml->v, ml_next->v});
      ml = ml_next;
      ml_next++;
    }
  }

  BLI_assert(totedge_final > 0);
  *r_medge = edges;
  *r_totedge = totedge_final;
}

void BKE_mesh_calc_edges_legacy(Mesh *mesh)
{
  using namespace blender;
  MEdge *edges;
  int totedge = 0;
  const Span<MVert> verts(
      static_cast<const MVert *>(CustomData_get_layer(&mesh->vert_data, CD_MVERT)),
      mesh->verts_num);

  mesh_calc_edges_mdata(
      verts.data(),
      mesh->mface,
      static_cast<MLoop *>(
          CustomData_get_layer_for_write(&mesh->corner_data, CD_MLOOP, mesh->corners_num)),
      static_cast<const MPoly *>(CustomData_get_layer(&mesh->face_data, CD_MPOLY)),
      verts.size(),
      mesh->totface_legacy,
      mesh->corners_num,
      mesh->faces_num,
      &edges,
      &totedge);

  if (totedge == 0) {
    BLI_assert(edges == nullptr);
    mesh->edges_num = 0;
    return;
  }

  edges = (MEdge *)CustomData_add_layer_with_data(
      &mesh->edge_data, CD_MEDGE, edges, totedge, nullptr);
  mesh->edges_num = totedge;

  mesh->tag_topology_changed();
  BKE_mesh_strip_loose_faces(mesh);
}

void BKE_mesh_strip_loose_faces(Mesh *mesh)
{
  /* NOTE: We need to keep this for edge creation (for now?), and some old `readfile.cc` code. */
  MFace *f;
  int a, b;
  MFace *mfaces = mesh->mface;

  for (a = b = 0, f = mfaces; a < mesh->totface_legacy; a++, f++) {
    if (f->v3) {
      if (a != b) {
        memcpy(&mfaces[b], f, sizeof(mfaces[b]));
        CustomData_copy_data(&mesh->fdata_legacy, &mesh->fdata_legacy, a, b, 1);
      }
      b++;
    }
  }
  if (a != b) {
    CustomData_free_elem(&mesh->fdata_legacy, b, a - b);
    mesh->totface_legacy = b;
  }
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

  const Span<MVert> verts(
      static_cast<const MVert *>(CustomData_get_layer(&mesh->vert_data, CD_MVERT)),
      mesh->verts_num);
  const Span<MEdge> edges(
      static_cast<const MEdge *>(CustomData_get_layer(&mesh->edge_data, CD_MEDGE)),
      mesh->edges_num);

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

#define MESH_MLOOPCOL_FROM_MCOL(_mloopcol, _mcol) \
  { \
    MLoopCol *mloopcol__tmp = _mloopcol; \
    const MCol *mcol__tmp = _mcol; \
    mloopcol__tmp->r = mcol__tmp->b; \
    mloopcol__tmp->g = mcol__tmp->g; \
    mloopcol__tmp->b = mcol__tmp->r; \
    mloopcol__tmp->a = mcol__tmp->a; \
  } \
  (void)0

static void bm_corners_to_loops_ex(ID *id,
                                   CustomData *fdata_legacy,
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
        fdata_legacy, CD_MTFACE, findex, i, totface);

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
        fdata_legacy, CD_MCOL, findex, i, totface);

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

  if (CustomData_has_layer(fdata_legacy, CD_TESSLOOPNORMAL)) {
    float (*loop_normals)[3] = (float (*)[3])CustomData_get_for_write(
        ldata, loopstart, CD_NORMAL, totloop);
    const short (*tessloop_normals)[3] = (short (*)[3])CustomData_get_for_write(
        fdata_legacy, findex, CD_TESSLOOPNORMAL, totface);
    const int max = mf->v4 ? 4 : 3;

    for (int i = 0; i < max; i++, loop_normals++, tessloop_normals++) {
      normal_short_to_float_v3(*loop_normals, *tessloop_normals);
    }
  }

  if (CustomData_has_layer(fdata_legacy, CD_MDISPS)) {
    MDisps *ld = (MDisps *)CustomData_get_for_write(ldata, loopstart, CD_MDISPS, totloop);
    const MDisps *fd = (const MDisps *)CustomData_get_for_write(
        fdata_legacy, findex, CD_MDISPS, totface);
    const float (*disps)[3] = fd->disps;
    int tot = mf->v4 ? 4 : 3;
    int corners;

    if (CustomData_external_test(fdata_legacy, CD_MDISPS)) {
      if (id && fdata_legacy->external) {
        CustomData_external_add(ldata, id, CD_MDISPS, totloop, fdata_legacy->external->filepath);
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

        ld->disps = MEM_malloc_arrayN<float[3]>(size_t(side_sq), "converted loop mdisps");
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

static void CustomData_to_bmeshpoly(CustomData *fdata_legacy, CustomData *ldata, int totloop)
{
  for (int i = 0; i < fdata_legacy->totlayer; i++) {
    if (fdata_legacy->layers[i].type == CD_MTFACE) {
      CustomData_add_layer_named(
          ldata, CD_PROP_FLOAT2, CD_SET_DEFAULT, totloop, fdata_legacy->layers[i].name);
    }
    else if (fdata_legacy->layers[i].type == CD_MCOL) {
      CustomData_add_layer_named(
          ldata, CD_PROP_BYTE_COLOR, CD_SET_DEFAULT, totloop, fdata_legacy->layers[i].name);
    }
    else if (fdata_legacy->layers[i].type == CD_MDISPS) {
      CustomData_add_layer_named(
          ldata, CD_MDISPS, CD_SET_DEFAULT, totloop, fdata_legacy->layers[i].name);
    }
    else if (fdata_legacy->layers[i].type == CD_TESSLOOPNORMAL) {
      CustomData_add_layer_named(
          ldata, CD_NORMAL, CD_SET_DEFAULT, totloop, fdata_legacy->layers[i].name);
    }
  }
}

static void convert_mfaces_to_mpolys(ID *id,
                                     CustomData *fdata_legacy,
                                     CustomData *ldata,
                                     CustomData *pdata,
                                     int totedge_i,
                                     int totface_i,
                                     int /*totloop_i*/,
                                     int /*faces_num_i*/,
                                     blender::int2 *edges,
                                     MFace *mface,
                                     int *r_totloop,
                                     int *r_faces_num)
{
  MFace *mf;
  MLoop *ml, *mloop;
  MPoly *poly, *mpoly;
  int numTex, numCol;
  int i, j, totloop, faces_num, *polyindex;

  /* just in case some of these layers are filled in (can happen with python created meshes) */
  CustomData_free(ldata);
  CustomData_free(pdata);

  faces_num = totface_i;
  mpoly = (MPoly *)CustomData_add_layer(pdata, CD_MPOLY, CD_SET_DEFAULT, faces_num);
  int *material_indices = static_cast<int *>(
      CustomData_get_layer_named_for_write(pdata, CD_PROP_INT32, "material_index", faces_num));
  if (material_indices == nullptr) {
    material_indices = static_cast<int *>(CustomData_add_layer_named(
        pdata, CD_PROP_INT32, CD_SET_DEFAULT, faces_num, "material_index"));
  }
  bool *sharp_faces = static_cast<bool *>(
      CustomData_get_layer_named_for_write(pdata, CD_PROP_BOOL, "sharp_face", faces_num));
  if (!sharp_faces) {
    sharp_faces = static_cast<bool *>(
        CustomData_add_layer_named(pdata, CD_PROP_BOOL, CD_SET_DEFAULT, faces_num, "sharp_face"));
  }

  numTex = CustomData_number_of_layers(fdata_legacy, CD_MTFACE);
  numCol = CustomData_number_of_layers(fdata_legacy, CD_MCOL);

  totloop = 0;
  mf = mface;
  for (i = 0; i < totface_i; i++, mf++) {
    totloop += mf->v4 ? 4 : 3;
  }

  mloop = (MLoop *)CustomData_add_layer(ldata, CD_MLOOP, CD_SET_DEFAULT, totloop);

  CustomData_to_bmeshpoly(fdata_legacy, ldata, totloop);

  if (id) {
    /* ensure external data is transferred */
    /* TODO(sergey): Use multiresModifier_ensure_external_read(). */
    CustomData_external_read(fdata_legacy, id, CD_MASK_MDISPS, totface_i);
  }

  blender::Map<blender::OrderedEdge, int> eh;
  eh.reserve(totedge_i);

  /* build edge hash */
  for (i = 0; i < totedge_i; i++) {
    eh.add(edges[i], i);
  }

  polyindex = (int *)CustomData_get_layer(fdata_legacy, CD_ORIGINDEX);

  j = 0; /* current loop index */
  ml = mloop;
  mf = mface;
  poly = mpoly;
  for (i = 0; i < totface_i; i++, mf++, poly++) {
    poly->loopstart = j;

    poly->totloop = mf->v4 ? 4 : 3;

    material_indices[i] = mf->mat_nr;
    sharp_faces[i] = (mf->flag & ME_SMOOTH) == 0;

#define ML(v1, v2) \
  { \
    ml->v = mf->v1; \
    ml->e = eh.lookup({mf->v1, mf->v2}); \
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
        id, fdata_legacy, totface_i, ldata, mface, totloop, i, poly->loopstart, numTex, numCol);

    if (polyindex) {
      *polyindex = i;
      polyindex++;
    }
  }

  /* NOTE: we don't convert NGons at all, these are not even real ngons,
   * they have their own UVs, colors etc - it's more an editing feature. */

  *r_faces_num = faces_num;
  *r_totloop = totloop;
}

static void update_active_fdata_layers(Mesh &mesh, CustomData *fdata_legacy, CustomData *ldata)
{
  int act;

  if (CustomData_has_layer(ldata, CD_PROP_FLOAT2)) {
    act = CustomData_get_active_layer(ldata, CD_PROP_FLOAT2);
    CustomData_set_layer_active(fdata_legacy, CD_MTFACE, act);

    act = CustomData_get_render_layer(ldata, CD_PROP_FLOAT2);
    CustomData_set_layer_render(fdata_legacy, CD_MTFACE, act);

    act = CustomData_get_clone_layer(ldata, CD_PROP_FLOAT2);
    CustomData_set_layer_clone(fdata_legacy, CD_MTFACE, act);

    act = CustomData_get_stencil_layer(ldata, CD_PROP_FLOAT2);
    CustomData_set_layer_stencil(fdata_legacy, CD_MTFACE, act);
  }

  if (CustomData_has_layer(ldata, CD_PROP_BYTE_COLOR)) {
    if (mesh.active_color_attribute != nullptr) {
      act = CustomData_get_named_layer(ldata, CD_PROP_BYTE_COLOR, mesh.active_color_attribute);
      /* The active color layer may be of #CD_PROP_COLOR type. */
      if (act != -1) {
        CustomData_set_layer_active(fdata_legacy, CD_MCOL, act);
      }
    }

    if (mesh.default_color_attribute != nullptr) {
      act = CustomData_get_named_layer(ldata, CD_PROP_BYTE_COLOR, mesh.default_color_attribute);
      /* The active color layer may be of #CD_PROP_COLOR type. */
      if (act != -1) {
        CustomData_set_layer_render(fdata_legacy, CD_MCOL, act);
      }
    }

    act = CustomData_get_clone_layer(ldata, CD_PROP_BYTE_COLOR);
    CustomData_set_layer_clone(fdata_legacy, CD_MCOL, act);

    act = CustomData_get_stencil_layer(ldata, CD_PROP_BYTE_COLOR);
    CustomData_set_layer_stencil(fdata_legacy, CD_MCOL, act);
  }
}

#ifndef NDEBUG
/**
 * Debug check, used to assert when we expect layers to be in/out of sync.
 *
 * \param fallback: Use when there are no layers to handle,
 * since callers may expect success or failure.
 */
static bool check_matching_legacy_layer_counts(CustomData *fdata_legacy,
                                               CustomData *ldata,
                                               bool fallback)
{
  int a_num = 0, b_num = 0;
#  define LAYER_CMP(l_a, t_a, l_b, t_b) \
    ((a_num += CustomData_number_of_layers(l_a, t_a)) == \
     (b_num += CustomData_number_of_layers(l_b, t_b)))

  if (!LAYER_CMP(ldata, CD_PROP_FLOAT2, fdata_legacy, CD_MTFACE)) {
    return false;
  }
  if (!LAYER_CMP(ldata, CD_PROP_BYTE_COLOR, fdata_legacy, CD_MCOL)) {
    return false;
  }
  if (!LAYER_CMP(ldata, CD_ORIGSPACE_MLOOP, fdata_legacy, CD_ORIGSPACE)) {
    return false;
  }
  if (!LAYER_CMP(ldata, CD_NORMAL, fdata_legacy, CD_TESSLOOPNORMAL)) {
    return false;
  }

#  undef LAYER_CMP

  /* if no layers are on either CustomData's,
   * then there was nothing to do... */
  return a_num ? true : fallback;
}
#endif /* !NDEBUG */

static void add_mface_layers(Mesh &mesh, CustomData *fdata_legacy, CustomData *ldata, int total)
{
  /* avoid accumulating extra layers */
  BLI_assert(!check_matching_legacy_layer_counts(fdata_legacy, ldata, false));

  for (int i = 0; i < ldata->totlayer; i++) {
    if (ldata->layers[i].type == CD_PROP_FLOAT2) {
      CustomData_add_layer_named(
          fdata_legacy, CD_MTFACE, CD_SET_DEFAULT, total, ldata->layers[i].name);
    }
    if (ldata->layers[i].type == CD_PROP_BYTE_COLOR) {
      CustomData_add_layer_named(
          fdata_legacy, CD_MCOL, CD_SET_DEFAULT, total, ldata->layers[i].name);
    }
    else if (ldata->layers[i].type == CD_ORIGSPACE_MLOOP) {
      CustomData_add_layer_named(
          fdata_legacy, CD_ORIGSPACE, CD_SET_DEFAULT, total, ldata->layers[i].name);
    }
    else if (ldata->layers[i].type == CD_NORMAL) {
      CustomData_add_layer_named(
          fdata_legacy, CD_TESSLOOPNORMAL, CD_SET_DEFAULT, total, ldata->layers[i].name);
    }
  }

  update_active_fdata_layers(mesh, fdata_legacy, ldata);
}

static void mesh_ensure_tessellation_customdata(Mesh *mesh)
{
  if (UNLIKELY((mesh->totface_legacy != 0) && (mesh->faces_num == 0))) {
    /* Pass, otherwise this function clears 'mface' before
     * versioning 'mface -> mpoly' code kicks in #30583.
     *
     * Callers could also check but safer to do here - campbell */
  }
  else {
    const int tottex_original = CustomData_number_of_layers(&mesh->corner_data, CD_PROP_FLOAT2);
    const int totcol_original = CustomData_number_of_layers(&mesh->corner_data,
                                                            CD_PROP_BYTE_COLOR);

    const int tottex_tessface = CustomData_number_of_layers(&mesh->fdata_legacy, CD_MTFACE);
    const int totcol_tessface = CustomData_number_of_layers(&mesh->fdata_legacy, CD_MCOL);

    if (tottex_tessface != tottex_original || totcol_tessface != totcol_original) {
      BKE_mesh_tessface_clear(mesh);

      add_mface_layers(*mesh, &mesh->fdata_legacy, &mesh->corner_data, mesh->totface_legacy);

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
                           &mesh->fdata_legacy,
                           &mesh->corner_data,
                           &mesh->face_data,
                           mesh->edges_num,
                           mesh->totface_legacy,
                           mesh->corners_num,
                           mesh->faces_num,
                           mesh->edges_for_write().data(),
                           (MFace *)CustomData_get_layer(&mesh->fdata_legacy, CD_MFACE),
                           &mesh->corners_num,
                           &mesh->faces_num);
  BKE_mesh_legacy_convert_loops_to_corners(mesh);
  BKE_mesh_legacy_convert_polys_to_offsets(mesh);

  mesh_ensure_tessellation_customdata(mesh);
}

/**
 * Update active indices for active/render/clone/stencil custom data layers
 * based on indices from fdata_legacy layers
 * used when creating fdata_legacy and ldata for pre-bmesh
 * meshes and needed to preserve active/render/clone/stencil flags set in pre-bmesh files.
 */
static void CustomData_bmesh_do_versions_update_active_layers(CustomData *fdata_legacy,
                                                              CustomData *corner_data)
{
  int act;

  if (CustomData_has_layer(fdata_legacy, CD_MTFACE)) {
    act = CustomData_get_active_layer(fdata_legacy, CD_MTFACE);
    CustomData_set_layer_active(corner_data, CD_PROP_FLOAT2, act);

    act = CustomData_get_render_layer(fdata_legacy, CD_MTFACE);
    CustomData_set_layer_render(corner_data, CD_PROP_FLOAT2, act);

    act = CustomData_get_clone_layer(fdata_legacy, CD_MTFACE);
    CustomData_set_layer_clone(corner_data, CD_PROP_FLOAT2, act);

    act = CustomData_get_stencil_layer(fdata_legacy, CD_MTFACE);
    CustomData_set_layer_stencil(corner_data, CD_PROP_FLOAT2, act);
  }

  if (CustomData_has_layer(fdata_legacy, CD_MCOL)) {
    act = CustomData_get_active_layer(fdata_legacy, CD_MCOL);
    CustomData_set_layer_active(corner_data, CD_PROP_BYTE_COLOR, act);

    act = CustomData_get_render_layer(fdata_legacy, CD_MCOL);
    CustomData_set_layer_render(corner_data, CD_PROP_BYTE_COLOR, act);

    act = CustomData_get_clone_layer(fdata_legacy, CD_MCOL);
    CustomData_set_layer_clone(corner_data, CD_PROP_BYTE_COLOR, act);

    act = CustomData_get_stencil_layer(fdata_legacy, CD_MCOL);
    CustomData_set_layer_stencil(corner_data, CD_PROP_BYTE_COLOR, act);
  }
}

void BKE_mesh_do_versions_convert_mfaces_to_mpolys(Mesh *mesh)
{
  convert_mfaces_to_mpolys(&mesh->id,
                           &mesh->fdata_legacy,
                           &mesh->corner_data,
                           &mesh->face_data,
                           mesh->edges_num,
                           mesh->totface_legacy,
                           mesh->corners_num,
                           mesh->faces_num,
                           mesh->edges_for_write().data(),
                           (MFace *)CustomData_get_layer(&mesh->fdata_legacy, CD_MFACE),
                           &mesh->corners_num,
                           &mesh->faces_num);
  BKE_mesh_legacy_convert_loops_to_corners(mesh);
  BKE_mesh_legacy_convert_polys_to_offsets(mesh);

  CustomData_bmesh_do_versions_update_active_layers(&mesh->fdata_legacy, &mesh->corner_data);

  mesh_ensure_tessellation_customdata(mesh);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name MFace Tessellation
 *
 * #MFace is a legacy data-structure that should be avoided, use #Mesh::corner_tris() instead.
 * \{ */

#define MESH_MLOOPCOL_TO_MCOL(_mloopcol, _mcol) \
  { \
    const MLoopCol *mloopcol__tmp = _mloopcol; \
    MCol *mcol__tmp = _mcol; \
    mcol__tmp->b = mloopcol__tmp->r; \
    mcol__tmp->g = mloopcol__tmp->g; \
    mcol__tmp->r = mloopcol__tmp->b; \
    mcol__tmp->a = mloopcol__tmp->a; \
  } \
  (void)0

/**
 * Convert all CD layers from loop/poly to tessface data.
 *
 * \param loopindices: is an array of an int[4] per tessface,
 * mapping tessface's verts to loops indices.
 *
 * \note when mface is not null, mface[face_index].v4
 * is used to test quads, else, loopindices[face_index][3] is used.
 */
static void mesh_loops_to_tessdata(CustomData *fdata_legacy,
                                   CustomData *corner_data,
                                   MFace *mface,
                                   const int *polyindices,
                                   uint (*loopindices)[4],
                                   const int num_faces)
{
  /* NOTE(mont29): performances are sub-optimal when we get a null #MFace,
   * we could be ~25% quicker with dedicated code.
   * The issue is, unless having two different functions with nearly the same code,
   * there's not much ways to solve this. Better IMHO to live with it for now (sigh). */
  const int numUV = CustomData_number_of_layers(corner_data, CD_PROP_FLOAT2);
  const int numCol = CustomData_number_of_layers(corner_data, CD_PROP_BYTE_COLOR);
  const bool hasOrigSpace = CustomData_has_layer(corner_data, CD_ORIGSPACE_MLOOP);
  const bool hasLoopNormal = CustomData_has_layer(corner_data, CD_NORMAL);
  int findex, i, j;
  const int *pidx;
  uint(*lidx)[4];

  for (i = 0; i < numUV; i++) {
    MTFace *texface = (MTFace *)CustomData_get_layer_n_for_write(
        fdata_legacy, CD_MTFACE, i, num_faces);
    const blender::float2 *uv = static_cast<const blender::float2 *>(
        CustomData_get_layer_n(corner_data, CD_PROP_FLOAT2, i));

    for (findex = 0, pidx = polyindices, lidx = loopindices; findex < num_faces;
         pidx++, lidx++, findex++, texface++)
    {
      for (j = (mface ? mface[findex].v4 : (*lidx)[3]) ? 4 : 3; j--;) {
        copy_v2_v2(texface->uv[j], uv[(*lidx)[j]]);
      }
    }
  }

  for (i = 0; i < numCol; i++) {
    MCol(*mcol)[4] = (MCol(*)[4])CustomData_get_layer_n_for_write(
        fdata_legacy, CD_MCOL, i, num_faces);
    const MLoopCol *mloopcol = (const MLoopCol *)CustomData_get_layer_n(
        corner_data, CD_PROP_BYTE_COLOR, i);

    for (findex = 0, lidx = loopindices; findex < num_faces; lidx++, findex++, mcol++) {
      for (j = (mface ? mface[findex].v4 : (*lidx)[3]) ? 4 : 3; j--;) {
        MESH_MLOOPCOL_TO_MCOL(&mloopcol[(*lidx)[j]], &(*mcol)[j]);
      }
    }
  }

  if (hasOrigSpace) {
    OrigSpaceFace *of = (OrigSpaceFace *)CustomData_get_layer(fdata_legacy, CD_ORIGSPACE);
    const OrigSpaceLoop *lof = (const OrigSpaceLoop *)CustomData_get_layer(corner_data,
                                                                           CD_ORIGSPACE_MLOOP);

    for (findex = 0, lidx = loopindices; findex < num_faces; lidx++, findex++, of++) {
      for (j = (mface ? mface[findex].v4 : (*lidx)[3]) ? 4 : 3; j--;) {
        copy_v2_v2(of->uv[j], lof[(*lidx)[j]].uv);
      }
    }
  }

  if (hasLoopNormal) {
    short (*face_normals)[4][3] = (short (*)[4][3])CustomData_get_layer(fdata_legacy,
                                                                        CD_TESSLOOPNORMAL);
    const float (*loop_normals)[3] = (const float (*)[3])CustomData_get_layer(corner_data,
                                                                              CD_NORMAL);

    for (findex = 0, lidx = loopindices; findex < num_faces; lidx++, findex++, face_normals++) {
      for (j = (mface ? mface[findex].v4 : (*lidx)[3]) ? 4 : 3; j--;) {
        normal_float_to_short_v3((*face_normals)[j], loop_normals[(*lidx)[j]]);
      }
    }
  }
}

int BKE_mesh_mface_index_validate(MFace *mface, CustomData *fdata_legacy, int mfindex, int nr)
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
        mface->v1 == mface->v2 || mface->v2 == mface->v3 || mface->v3 == mface->v1)
    {
      return 0;
    }
  }
  else if (nr == 4) {
    if (
        /* real edges */
        mface->v1 == mface->v2 || mface->v2 == mface->v3 || mface->v3 == mface->v4 ||
        mface->v4 == mface->v1 ||
        /* across the face */
        mface->v1 == mface->v3 || mface->v2 == mface->v4)
    {
      return 0;
    }
  }

  /* prevent a zero at wrong index location */
  if (nr == 3) {
    if (mface->v3 == 0) {
      static int corner_indices[4] = {1, 2, 0, 3};

      std::swap(mface->v1, mface->v2);
      std::swap(mface->v2, mface->v3);

      if (fdata_legacy) {
        CustomData_swap_corners(fdata_legacy, mfindex, corner_indices);
      }
    }
  }
  else if (nr == 4) {
    if (mface->v3 == 0 || mface->v4 == 0) {
      static int corner_indices[4] = {2, 3, 0, 1};

      std::swap(mface->v1, mface->v3);
      std::swap(mface->v2, mface->v4);

      if (fdata_legacy) {
        CustomData_swap_corners(fdata_legacy, mfindex, corner_indices);
      }
    }
  }

  return nr;
}

static int mesh_tessface_calc(Mesh &mesh,
                              CustomData *fdata_legacy,
                              CustomData *ldata,
                              CustomData *pdata,
                              float (*positions)[3],
                              int totface,
                              int totloop,
                              int faces_num)
{
#define USE_TESSFACE_SPEEDUP
#define USE_TESSFACE_QUADS

/* We abuse #MFace.edcode to tag quad faces. See below for details. */
#define TESSFACE_IS_QUAD 1

  const int corner_tris_num = poly_to_tri_count(faces_num, totloop);

  MFace *mface, *mf;
  MemArena *arena = nullptr;
  int *mface_to_poly_map;
  uint(*lindices)[4];
  int poly_index, mface_index;
  uint j;

  const blender::OffsetIndices faces = mesh.faces();
  const Span<int> corner_verts = mesh.corner_verts();
  const int *material_indices = static_cast<const int *>(
      CustomData_get_layer_named(pdata, CD_PROP_INT32, "material_index"));
  const bool *sharp_faces = static_cast<const bool *>(
      CustomData_get_layer_named(pdata, CD_PROP_BOOL, "sharp_face"));

  /* Allocate the length of `totfaces`, avoid many small reallocation's,
   * if all faces are triangles it will be correct, `quads == 2x` allocations. */
  /* Take care since memory is _not_ zeroed so be sure to initialize each field. */
  mface_to_poly_map = MEM_malloc_arrayN<int>(size_t(corner_tris_num), __func__);
  mface = MEM_malloc_arrayN<MFace>(size_t(corner_tris_num), __func__);
  lindices = MEM_malloc_arrayN<uint[4]>(size_t(corner_tris_num), __func__);

  mface_index = 0;
  for (poly_index = 0; poly_index < faces_num; poly_index++) {
    const uint mp_loopstart = uint(faces[poly_index].start());
    const uint mp_totloop = uint(faces[poly_index].size());
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
    mf->v1 = corner_verts[l1]; \
    mf->v2 = corner_verts[l2]; \
    mf->v3 = corner_verts[l3]; \
    mf->v4 = 0; \
    lidx[0] = l1; \
    lidx[1] = l2; \
    lidx[2] = l3; \
    lidx[3] = 0; \
    mf->mat_nr = material_indices ? material_indices[poly_index] : 0; \
    mf->flag = (sharp_faces && sharp_faces[poly_index]) ? 0 : ME_SMOOTH; \
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
    mf->v1 = corner_verts[l1]; \
    mf->v2 = corner_verts[l2]; \
    mf->v3 = corner_verts[l3]; \
    mf->v4 = corner_verts[l4]; \
    lidx[0] = l1; \
    lidx[1] = l2; \
    lidx[2] = l3; \
    lidx[3] = l4; \
    mf->mat_nr = material_indices ? material_indices[poly_index] : 0; \
    mf->flag = (sharp_faces && sharp_faces[poly_index]) ? 0 : ME_SMOOTH; \
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
      float (*projverts)[2];
      uint(*tris)[3];

      const uint totfilltri = mp_totloop - 2;

      if (UNLIKELY(arena == nullptr)) {
        arena = BLI_memarena_new(BLI_MEMARENA_STD_BUFSIZE, __func__);
      }

      tris = (uint(*)[3])BLI_memarena_alloc(arena, sizeof(*tris) * size_t(totfilltri));
      projverts = (float (*)[2])BLI_memarena_alloc(arena, sizeof(*projverts) * size_t(mp_totloop));

      zero_v3(normal);

      /* Calculate the normal, flipped: to get a positive 2D cross product. */
      co_prev = positions[corner_verts[mp_loopstart + mp_totloop - 1]];
      for (j = 0; j < mp_totloop; j++) {
        const int vert = corner_verts[mp_loopstart + j];
        co_curr = positions[vert];
        add_newell_cross_v3_v3v3(normal, co_prev, co_curr);
        co_prev = co_curr;
      }
      if (UNLIKELY(normalize_v3(normal) == 0.0f)) {
        normal[2] = 1.0f;
      }

      /* Project verts to 2D. */
      axis_dominant_v3_to_m3_negate(axis_mat, normal);

      for (j = 0; j < mp_totloop; j++) {
        const int vert = corner_verts[mp_loopstart + j];
        mul_v2_m3v3(projverts[j], axis_mat, positions[vert]);
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

        mf->v1 = corner_verts[l1];
        mf->v2 = corner_verts[l2];
        mf->v3 = corner_verts[l3];
        mf->v4 = 0;

        lidx[0] = l1;
        lidx[1] = l2;
        lidx[2] = l3;
        lidx[3] = 0;

        mf->mat_nr = material_indices ? material_indices[poly_index] : 0;
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

  CustomData_free(fdata_legacy);
  totface = mface_index;

  BLI_assert(totface <= corner_tris_num);

  /* Not essential but without this we store over-allocated memory in the #CustomData layers. */
  if (LIKELY(corner_tris_num != totface)) {
    mface = (MFace *)MEM_reallocN(mface, sizeof(*mface) * size_t(totface));
    mface_to_poly_map = (int *)MEM_reallocN(mface_to_poly_map,
                                            sizeof(*mface_to_poly_map) * size_t(totface));
  }

  CustomData_add_layer_with_data(fdata_legacy, CD_MFACE, mface, totface, nullptr);

  /* #CD_ORIGINDEX will contain an array of indices from tessellation-faces to the polygons
   * they are directly tessellated from. */
  CustomData_add_layer_with_data(fdata_legacy, CD_ORIGINDEX, mface_to_poly_map, totface, nullptr);
  add_mface_layers(mesh, fdata_legacy, ldata, totface);

  /* NOTE: quad detection issue - fourth vertex-index vs fourth loop-index:
   * Polygons take care of their loops ordering, hence not of their vertices ordering.
   * Currently, the #TFace fourth vertex index might be 0 even for a quad.
   * However, we know our fourth loop index is never 0 for quads
   * (because they are sorted for polygons, and our quads are still mere copies of their polygons).
   * So we pass nullptr as #MFace pointer, and #mesh_loops_to_tessdata
   * will use the fourth loop index as quad test. */
  mesh_loops_to_tessdata(fdata_legacy, ldata, nullptr, mface_to_poly_map, lindices, totface);

  /* NOTE: quad detection issue - fourth vert-index vs fourth loop-index:
   * ...However, most #TFace code uses `MFace->v4 == 0` test to check whether it is a tri or quad.
   * BKE_mesh_mface_index_validate() will check this and rotate the tessellated face if needed.
   */
#ifdef USE_TESSFACE_QUADS
  mf = mface;
  for (mface_index = 0; mface_index < totface; mface_index++, mf++) {
    if (mf->edcode == TESSFACE_IS_QUAD) {
      BKE_mesh_mface_index_validate(mf, fdata_legacy, mface_index, 4);
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
  mesh->totface_legacy = mesh_tessface_calc(
      *mesh,
      &mesh->fdata_legacy,
      &mesh->corner_data,
      &mesh->face_data,
      reinterpret_cast<float (*)[3]>(mesh->vert_positions_for_write().data()),
      mesh->totface_legacy,
      mesh->corners_num,
      mesh->faces_num);

  mesh_ensure_tessellation_customdata(mesh);
}

void BKE_mesh_tessface_ensure(Mesh *mesh)
{
  if (mesh->faces_num && mesh->totface_legacy == 0) {
    BKE_mesh_tessface_calc(mesh);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Sharp Edge Conversion
 * \{ */

void BKE_mesh_legacy_sharp_faces_from_flags(Mesh *mesh)
{
  using namespace blender;
  using namespace blender::bke;
  MutableAttributeAccessor attributes = mesh->attributes_for_write();
  if (attributes.contains("sharp_face") || !CustomData_get_layer(&mesh->face_data, CD_MPOLY)) {
    return;
  }
  const Span<MPoly> polys(
      static_cast<const MPoly *>(CustomData_get_layer(&mesh->face_data, CD_MPOLY)),
      mesh->faces_num);
  if (std::any_of(polys.begin(), polys.end(), [](const MPoly &poly) {
        return !(poly.flag_legacy & ME_SMOOTH);
      }))
  {
    SpanAttributeWriter<bool> sharp_faces = attributes.lookup_or_add_for_write_only_span<bool>(
        "sharp_face", AttrDomain::Face);
    threading::parallel_for(polys.index_range(), 4096, [&](const IndexRange range) {
      for (const int i : range) {
        sharp_faces.span[i] = !(polys[i].flag_legacy & ME_SMOOTH);
      }
    });
    sharp_faces.finish();
  }
  else {
    attributes.remove("sharp_face");
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Face Set Conversion
 * \{ */

void BKE_mesh_legacy_face_set_to_generic(Mesh *mesh)
{
  using namespace blender;
  if (mesh->attributes().contains(".sculpt_face_set")) {
    return;
  }
  void *faceset_data = nullptr;
  const ImplicitSharingInfo *faceset_sharing_info = nullptr;
  for (const int i : IndexRange(mesh->face_data.totlayer)) {
    CustomDataLayer &layer = mesh->face_data.layers[i];
    if (layer.type == CD_SCULPT_FACE_SETS) {
      faceset_data = layer.data;
      faceset_sharing_info = layer.sharing_info;
      layer.data = nullptr;
      layer.sharing_info = nullptr;
      CustomData_free_layer(&mesh->face_data, CD_SCULPT_FACE_SETS, i);
      break;
    }
  }
  if (faceset_data != nullptr) {
    CustomData_add_layer_named_with_data(&mesh->face_data,
                                         CD_PROP_INT32,
                                         faceset_data,
                                         mesh->faces_num,
                                         ".sculpt_face_set",
                                         faceset_sharing_info);
  }
  if (faceset_sharing_info != nullptr) {
    faceset_sharing_info->remove_user_and_delete_if_last();
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Face Map Conversion
 * \{ */

static void move_face_map_data_to_attributes(Mesh *mesh)
{
  using namespace blender;
  if (mesh->attributes().contains("face_maps")) {
    return;
  }
  int *data = nullptr;
  const ImplicitSharingInfo *sharing_info = nullptr;
  for (const int i : IndexRange(mesh->face_data.totlayer)) {
    CustomDataLayer &layer = mesh->face_data.layers[i];
    if (layer.type == CD_FACEMAP) {
      data = static_cast<int *>(layer.data);
      sharing_info = layer.sharing_info;
      layer.data = nullptr;
      layer.sharing_info = nullptr;
      CustomData_free_layer(&mesh->face_data, CD_FACEMAP, i);
      break;
    }
  }
  if (!data) {
    return;
  }

  CustomData_add_layer_named_with_data(
      &mesh->face_data, CD_PROP_INT32, data, mesh->faces_num, "face_maps", sharing_info);
  if (sharing_info != nullptr) {
    sharing_info->remove_user_and_delete_if_last();
  }

  MultiValueMap<int, int> groups;
  for (const int i : IndexRange(mesh->faces_num)) {
    if (data[i] == -1) {
      /* -1 values "didn't have" a face map. */
      continue;
    }
    groups.add(data[i], i);
  }

  bke::MutableAttributeAccessor attributes = mesh->attributes_for_write();
  for (const auto item : groups.items()) {
    bke::SpanAttributeWriter<bool> attribute = attributes.lookup_or_add_for_write_span<bool>(
        ".temp_face_map_" + std::to_string(item.key), bke::AttrDomain::Face);
    if (attribute) {
      attribute.span.fill_indices(item.value.as_span(), true);
      attribute.finish();
    }
  }
}

void BKE_mesh_legacy_face_map_to_generic(Main *bmain)
{
  LISTBASE_FOREACH (Mesh *, mesh, &bmain->meshes) {
    move_face_map_data_to_attributes(mesh);
  }

  LISTBASE_FOREACH (Object *, object, &bmain->objects) {
    if (object->type != OB_MESH) {
      continue;
    }
    Mesh *mesh = static_cast<Mesh *>(object->data);
    int i;
    LISTBASE_FOREACH_INDEX (bFaceMap *, face_map, &object->fmaps, i) {
      mesh->attributes_for_write().rename(".temp_face_map_" + std::to_string(i), face_map->name);
    }
    BLI_freelistN(&object->fmaps);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Bevel Weight Conversion
 * \{ */

void BKE_mesh_legacy_bevel_weight_to_layers(Mesh *mesh)
{
  using namespace blender;
  if (mesh->mvert && !CustomData_has_layer(&mesh->vert_data, CD_BWEIGHT)) {
    const Span<MVert> verts(mesh->mvert, mesh->verts_num);
    if (mesh->cd_flag & ME_CDFLAG_VERT_BWEIGHT) {
      float *weights = static_cast<float *>(
          CustomData_add_layer(&mesh->vert_data, CD_BWEIGHT, CD_CONSTRUCT, verts.size()));
      for (const int i : verts.index_range()) {
        weights[i] = verts[i].bweight_legacy / 255.0f;
      }
    }
  }

  if (mesh->medge && !CustomData_has_layer(&mesh->edge_data, CD_BWEIGHT)) {
    const Span<MEdge> edges(mesh->medge, mesh->edges_num);
    if (mesh->cd_flag & ME_CDFLAG_EDGE_BWEIGHT) {
      float *weights = static_cast<float *>(
          CustomData_add_layer(&mesh->edge_data, CD_BWEIGHT, CD_CONSTRUCT, edges.size()));
      for (const int i : edges.index_range()) {
        weights[i] = edges[i].bweight_legacy / 255.0f;
      }
    }
  }
}

static void replace_custom_data_layer_with_named(CustomData &custom_data,
                                                 const eCustomDataType old_type,
                                                 const eCustomDataType new_type,
                                                 const int elems_num,
                                                 const char *new_name)
{
  using namespace blender;
  void *data = nullptr;
  const ImplicitSharingInfo *sharing_info = nullptr;
  for (const int i : IndexRange(custom_data.totlayer)) {
    CustomDataLayer &layer = custom_data.layers[i];
    if (layer.type == old_type) {
      data = layer.data;
      sharing_info = layer.sharing_info;
      layer.data = nullptr;
      layer.sharing_info = nullptr;
      CustomData_free_layer(&custom_data, old_type, i);
      break;
    }
  }
  if (data != nullptr) {
    CustomData_add_layer_named_with_data(
        &custom_data, new_type, data, elems_num, new_name, sharing_info);
  }
  if (sharing_info != nullptr) {
    sharing_info->remove_user_and_delete_if_last();
  }
}

void BKE_mesh_legacy_bevel_weight_to_generic(Mesh *mesh)
{
  if (!mesh->attributes().contains("bevel_weight_vert")) {
    replace_custom_data_layer_with_named(
        mesh->vert_data, CD_BWEIGHT, CD_PROP_FLOAT, mesh->verts_num, "bevel_weight_vert");
  }
  if (!mesh->attributes().contains("bevel_weight_edge")) {
    replace_custom_data_layer_with_named(
        mesh->edge_data, CD_BWEIGHT, CD_PROP_FLOAT, mesh->edges_num, "bevel_weight_edge");
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Crease Conversion
 * \{ */

void BKE_mesh_legacy_edge_crease_to_layers(Mesh *mesh)
{
  using namespace blender;
  if (!mesh->medge) {
    return;
  }
  if (CustomData_has_layer(&mesh->edge_data, CD_CREASE)) {
    return;
  }
  const Span<MEdge> edges(mesh->medge, mesh->edges_num);
  if (mesh->cd_flag & ME_CDFLAG_EDGE_CREASE) {
    float *creases = static_cast<float *>(
        CustomData_add_layer(&mesh->edge_data, CD_CREASE, CD_CONSTRUCT, edges.size()));
    for (const int i : edges.index_range()) {
      creases[i] = edges[i].crease_legacy / 255.0f;
    }
  }
}

void BKE_mesh_legacy_crease_to_generic(Mesh *mesh)
{
  if (!mesh->attributes().contains("crease_vert")) {
    replace_custom_data_layer_with_named(
        mesh->vert_data, CD_CREASE, CD_PROP_FLOAT, mesh->verts_num, "crease_vert");
  }
  if (!mesh->attributes().contains("crease_edge")) {
    replace_custom_data_layer_with_named(
        mesh->edge_data, CD_CREASE, CD_PROP_FLOAT, mesh->edges_num, "crease_edge");
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Sharp Edge Conversion
 * \{ */

void BKE_mesh_legacy_sharp_edges_from_flags(Mesh *mesh)
{
  using namespace blender;
  using namespace blender::bke;
  if (!mesh->medge) {
    return;
  }
  const Span<MEdge> edges(mesh->medge, mesh->edges_num);
  MutableAttributeAccessor attributes = mesh->attributes_for_write();
  if (attributes.contains("sharp_edge")) {
    return;
  }
  if (std::any_of(edges.begin(), edges.end(), [](const MEdge &edge) {
        return edge.flag_legacy & ME_SHARP;
      }))
  {
    SpanAttributeWriter<bool> sharp_edges = attributes.lookup_or_add_for_write_only_span<bool>(
        "sharp_edge", AttrDomain::Edge);
    threading::parallel_for(edges.index_range(), 4096, [&](const IndexRange range) {
      for (const int i : range) {
        sharp_edges.span[i] = edges[i].flag_legacy & ME_SHARP;
      }
    });
    sharp_edges.finish();
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name UV Seam Conversion
 * \{ */

void BKE_mesh_legacy_uv_seam_from_flags(Mesh *mesh)
{
  using namespace blender;
  using namespace blender::bke;
  if (!mesh->medge) {
    return;
  }
  MutableSpan<MEdge> edges(mesh->medge, mesh->edges_num);
  MutableAttributeAccessor attributes = mesh->attributes_for_write();
  if (attributes.contains(".uv_seam")) {
    return;
  }
  if (std::any_of(edges.begin(), edges.end(), [](const MEdge &edge) {
        return edge.flag_legacy & ME_SEAM;
      }))
  {
    SpanAttributeWriter<bool> uv_seams = attributes.lookup_or_add_for_write_only_span<bool>(
        ".uv_seam", AttrDomain::Edge);
    threading::parallel_for(edges.index_range(), 4096, [&](const IndexRange range) {
      for (const int i : range) {
        uv_seams.span[i] = edges[i].flag_legacy & ME_SEAM;
      }
    });
    uv_seams.finish();
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Hide Attribute and Legacy Flag Conversion
 * \{ */

void BKE_mesh_legacy_convert_flags_to_hide_layers(Mesh *mesh)
{
  using namespace blender;
  using namespace blender::bke;
  MutableAttributeAccessor attributes = mesh->attributes_for_write();
  if (!mesh->mvert || attributes.contains(".hide_vert") || attributes.contains(".hide_edge") ||
      attributes.contains(".hide_poly"))
  {
    return;
  }
  const Span<MVert> verts(mesh->mvert, mesh->verts_num);
  if (std::any_of(verts.begin(), verts.end(), [](const MVert &vert) {
        return vert.flag_legacy & ME_HIDE;
      }))
  {
    SpanAttributeWriter<bool> hide_vert = attributes.lookup_or_add_for_write_only_span<bool>(
        ".hide_vert", AttrDomain::Point);
    threading::parallel_for(verts.index_range(), 4096, [&](IndexRange range) {
      for (const int i : range) {
        hide_vert.span[i] = verts[i].flag_legacy & ME_HIDE;
      }
    });
    hide_vert.finish();
  }

  if (mesh->medge) {
    const Span<MEdge> edges(mesh->medge, mesh->edges_num);
    if (std::any_of(edges.begin(), edges.end(), [](const MEdge &edge) {
          return edge.flag_legacy & ME_HIDE;
        }))
    {
      SpanAttributeWriter<bool> hide_edge = attributes.lookup_or_add_for_write_only_span<bool>(
          ".hide_edge", AttrDomain::Edge);
      threading::parallel_for(edges.index_range(), 4096, [&](IndexRange range) {
        for (const int i : range) {
          hide_edge.span[i] = edges[i].flag_legacy & ME_HIDE;
        }
      });
      hide_edge.finish();
    }
  }

  const Span<MPoly> polys(
      static_cast<const MPoly *>(CustomData_get_layer(&mesh->face_data, CD_MPOLY)),
      mesh->faces_num);
  if (std::any_of(polys.begin(), polys.end(), [](const MPoly &poly) {
        return poly.flag_legacy & ME_HIDE;
      }))
  {
    SpanAttributeWriter<bool> hide_poly = attributes.lookup_or_add_for_write_only_span<bool>(
        ".hide_poly", AttrDomain::Face);
    threading::parallel_for(polys.index_range(), 4096, [&](IndexRange range) {
      for (const int i : range) {
        hide_poly.span[i] = polys[i].flag_legacy & ME_HIDE;
      }
    });
    hide_poly.finish();
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Material Index Conversion
 * \{ */

void BKE_mesh_legacy_convert_mpoly_to_material_indices(Mesh *mesh)
{
  using namespace blender;
  using namespace blender::bke;
  MutableAttributeAccessor attributes = mesh->attributes_for_write();
  if (!CustomData_has_layer(&mesh->face_data, CD_MPOLY) || attributes.contains("material_index")) {
    return;
  }
  const Span<MPoly> polys(
      static_cast<const MPoly *>(CustomData_get_layer(&mesh->face_data, CD_MPOLY)),
      mesh->faces_num);
  if (std::any_of(
          polys.begin(), polys.end(), [](const MPoly &poly) { return poly.mat_nr_legacy != 0; }))
  {
    SpanAttributeWriter<int> material_indices = attributes.lookup_or_add_for_write_only_span<int>(
        "material_index", AttrDomain::Face);
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

void BKE_mesh_legacy_convert_uvs_to_generic(Mesh *mesh)
{
  using namespace blender;
  using namespace blender::bke;
  if (!CustomData_has_layer(&mesh->corner_data, CD_MLOOPUV)) {
    return;
  }

  /* Store layer names since they will be removed, used to set the active status of new layers.
   * Use intermediate #StringRef because the names can be null. */

  Array<std::string> uv_names(CustomData_number_of_layers(&mesh->corner_data, CD_MLOOPUV));
  for (const int i : uv_names.index_range()) {
    uv_names[i] = CustomData_get_layer_name(&mesh->corner_data, CD_MLOOPUV, i);
  }
  const int active_name_i = uv_names.as_span().first_index_try(
      StringRef(CustomData_get_active_layer_name(&mesh->corner_data, CD_MLOOPUV)));
  const int default_name_i = uv_names.as_span().first_index_try(
      StringRef(CustomData_get_render_layer_name(&mesh->corner_data, CD_MLOOPUV)));

  for (const int i : uv_names.index_range()) {
    const MLoopUV *mloopuv = static_cast<const MLoopUV *>(
        CustomData_get_layer_named(&mesh->corner_data, CD_MLOOPUV, uv_names[i]));
    const uint32_t needed_boolean_attributes = threading::parallel_reduce(
        IndexRange(mesh->corners_num),
        4096,
        0,
        [&](const IndexRange range, uint32_t init) {
          for (const int i : range) {
            init |= mloopuv[i].flag;
          }
          return init;
        },
        [](const uint32_t a, const uint32_t b) { return a | b; });

    float2 *coords = MEM_malloc_arrayN<float2>(size_t(mesh->corners_num), __func__);
    bool *pin = nullptr;
    if (needed_boolean_attributes & MLOOPUV_PINNED) {
      pin = MEM_malloc_arrayN<bool>(size_t(mesh->corners_num), __func__);
    }

    threading::parallel_for(IndexRange(mesh->corners_num), 4096, [&](IndexRange range) {
      for (const int i : range) {
        coords[i] = mloopuv[i].uv;
      }
      if (pin) {
        for (const int i : range) {
          pin[i] = mloopuv[i].flag & MLOOPUV_PINNED;
        }
      }
    });

    CustomData_free_layer_named(&mesh->corner_data, uv_names[i]);

    AttributeOwner owner = AttributeOwner::from_id(&mesh->id);
    const std::string new_name = BKE_attribute_calc_unique_name(owner, uv_names[i].c_str());
    uv_names[i] = new_name;

    CustomData_add_layer_named_with_data(
        &mesh->corner_data, CD_PROP_FLOAT2, coords, mesh->corners_num, new_name, nullptr);
    char buffer[MAX_CUSTOMDATA_LAYER_NAME];
    if (pin) {
      CustomData_add_layer_named_with_data(&mesh->corner_data,
                                           CD_PROP_BOOL,
                                           pin,
                                           mesh->corners_num,
                                           BKE_uv_map_pin_name_get(new_name, buffer),
                                           nullptr);
    }
  }

  if (active_name_i != -1) {
    CustomData_set_layer_active_index(&mesh->corner_data,
                                      CD_PROP_FLOAT2,
                                      CustomData_get_named_layer_index(&mesh->corner_data,
                                                                       CD_PROP_FLOAT2,
                                                                       uv_names[active_name_i]));
  }
  if (default_name_i != -1) {
    CustomData_set_layer_render_index(&mesh->corner_data,
                                      CD_PROP_FLOAT2,
                                      CustomData_get_named_layer_index(&mesh->corner_data,
                                                                       CD_PROP_FLOAT2,
                                                                       uv_names[default_name_i]));
  }
}

/** \} */

/** \name Selection Attribute and Legacy Flag Conversion
 * \{ */

void BKE_mesh_legacy_convert_flags_to_selection_layers(Mesh *mesh)
{
  using namespace blender;
  using namespace blender::bke;
  MutableAttributeAccessor attributes = mesh->attributes_for_write();
  if (!mesh->mvert || attributes.contains(".select_vert") || attributes.contains(".select_edge") ||
      attributes.contains(".select_poly"))
  {
    return;
  }

  const Span<MVert> verts(mesh->mvert, mesh->verts_num);
  if (std::any_of(
          verts.begin(), verts.end(), [](const MVert &vert) { return vert.flag_legacy & SELECT; }))
  {
    SpanAttributeWriter<bool> select_vert = attributes.lookup_or_add_for_write_only_span<bool>(
        ".select_vert", AttrDomain::Point);
    threading::parallel_for(verts.index_range(), 4096, [&](IndexRange range) {
      for (const int i : range) {
        select_vert.span[i] = verts[i].flag_legacy & SELECT;
      }
    });
    select_vert.finish();
  }

  if (mesh->medge) {
    const Span<MEdge> edges(mesh->medge, mesh->edges_num);
    if (std::any_of(edges.begin(), edges.end(), [](const MEdge &edge) {
          return edge.flag_legacy & SELECT;
        }))
    {
      SpanAttributeWriter<bool> select_edge = attributes.lookup_or_add_for_write_only_span<bool>(
          ".select_edge", AttrDomain::Edge);
      threading::parallel_for(edges.index_range(), 4096, [&](IndexRange range) {
        for (const int i : range) {
          select_edge.span[i] = edges[i].flag_legacy & SELECT;
        }
      });
      select_edge.finish();
    }
  }

  const Span<MPoly> polys(
      static_cast<const MPoly *>(CustomData_get_layer(&mesh->face_data, CD_MPOLY)),
      mesh->faces_num);
  if (std::any_of(polys.begin(), polys.end(), [](const MPoly &poly) {
        return poly.flag_legacy & ME_FACE_SEL;
      }))
  {
    SpanAttributeWriter<bool> select_poly = attributes.lookup_or_add_for_write_only_span<bool>(
        ".select_poly", AttrDomain::Face);
    threading::parallel_for(polys.index_range(), 4096, [&](IndexRange range) {
      for (const int i : range) {
        select_poly.span[i] = polys[i].flag_legacy & ME_FACE_SEL;
      }
    });
    select_poly.finish();
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Vertex and Position Conversion
 * \{ */

void BKE_mesh_legacy_convert_verts_to_positions(Mesh *mesh)
{
  using namespace blender;
  using namespace blender::bke;
  const MVert *mvert = static_cast<const MVert *>(
      CustomData_get_layer(&mesh->vert_data, CD_MVERT));
  if (!mvert || CustomData_has_layer_named(&mesh->vert_data, CD_PROP_FLOAT3, "position")) {
    return;
  }

  const Span<MVert> verts(mvert, mesh->verts_num);
  MutableSpan<float3> positions(
      static_cast<float3 *>(CustomData_add_layer_named(
          &mesh->vert_data, CD_PROP_FLOAT3, CD_CONSTRUCT, mesh->verts_num, "position")),
      mesh->verts_num);
  threading::parallel_for(verts.index_range(), 2048, [&](IndexRange range) {
    for (const int i : range) {
      positions[i] = verts[i].co_legacy;
    }
  });

  CustomData_free_layers(&mesh->vert_data, CD_MVERT);
  mesh->mvert = nullptr;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name MEdge and int2 conversion
 * \{ */

void BKE_mesh_legacy_convert_edges_to_generic(Mesh *mesh)
{
  using namespace blender;
  using namespace blender::bke;
  const MEdge *medge = static_cast<const MEdge *>(
      CustomData_get_layer(&mesh->edge_data, CD_MEDGE));
  if (!medge || CustomData_has_layer_named(&mesh->edge_data, CD_PROP_INT32_2D, ".edge_verts")) {
    return;
  }

  const Span<MEdge> legacy_edges(medge, mesh->edges_num);
  MutableSpan<int2> edges(
      static_cast<int2 *>(CustomData_add_layer_named(
          &mesh->edge_data, CD_PROP_INT32_2D, CD_CONSTRUCT, mesh->edges_num, ".edge_verts")),
      mesh->edges_num);
  threading::parallel_for(legacy_edges.index_range(), 2048, [&](IndexRange range) {
    for (const int i : range) {
      edges[i] = int2(legacy_edges[i].v1, legacy_edges[i].v2);
    }
  });

  CustomData_free_layers(&mesh->edge_data, CD_MEDGE);
  mesh->medge = nullptr;
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

  active_from_flags(mesh->vert_data);
  active_from_flags(mesh->corner_data);
  active_from_indices(mesh->vert_data);
  active_from_indices(mesh->corner_data);

  default_from_flags(mesh->vert_data);
  default_from_flags(mesh->corner_data);
  default_from_indices(mesh->vert_data);
  default_from_indices(mesh->corner_data);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Face Corner Conversion
 * \{ */

void BKE_mesh_legacy_convert_loops_to_corners(Mesh *mesh)
{
  using namespace blender;
  if (CustomData_has_layer_named(&mesh->corner_data, CD_PROP_INT32, ".corner_vert") &&
      CustomData_has_layer_named(&mesh->corner_data, CD_PROP_INT32, ".corner_edge"))
  {
    return;
  }
  const Span<MLoop> loops(
      static_cast<const MLoop *>(CustomData_get_layer(&mesh->corner_data, CD_MLOOP)),
      mesh->corners_num);
  MutableSpan<int> corner_verts(
      static_cast<int *>(CustomData_add_layer_named(
          &mesh->corner_data, CD_PROP_INT32, CD_CONSTRUCT, mesh->corners_num, ".corner_vert")),
      mesh->corners_num);
  MutableSpan<int> corner_edges(
      static_cast<int *>(CustomData_add_layer_named(
          &mesh->corner_data, CD_PROP_INT32, CD_CONSTRUCT, mesh->corners_num, ".corner_edge")),
      mesh->corners_num);
  threading::parallel_for(loops.index_range(), 2048, [&](IndexRange range) {
    for (const int i : range) {
      corner_verts[i] = loops[i].v;
      corner_edges[i] = loops[i].e;
    }
  });

  CustomData_free_layers(&mesh->corner_data, CD_MLOOP);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Poly Offset Conversion
 * \{ */

static bool poly_loops_orders_match(const Span<MPoly> polys)
{
  for (const int i : polys.index_range().drop_back(1)) {
    if (polys[i].loopstart > polys[i + 1].loopstart) {
      return false;
    }
  }
  return true;
}

void BKE_mesh_legacy_convert_polys_to_offsets(Mesh *mesh)
{
  using namespace blender;
  if (mesh->face_offset_indices) {
    return;
  }
  const Span<MPoly> polys(
      static_cast<const MPoly *>(CustomData_get_layer(&mesh->face_data, CD_MPOLY)),
      mesh->faces_num);

  BKE_mesh_face_offsets_ensure_alloc(mesh);
  MutableSpan<int> offsets = mesh->face_offsets_for_write();

  if (poly_loops_orders_match(polys)) {
    for (const int i : polys.index_range()) {
      offsets[i] = polys[i].loopstart;
    }
  }
  else {
    /* Reorder mesh polygons to match the order of their loops. */
    Array<int> orig_indices(polys.size());
    array_utils::fill_index_range<int>(orig_indices);
    std::stable_sort(orig_indices.begin(), orig_indices.end(), [polys](const int a, const int b) {
      return polys[a].loopstart < polys[b].loopstart;
    });
    CustomData old_poly_data = mesh->face_data;
    CustomData_reset(&mesh->face_data);
    CustomData_init_layout_from(
        &old_poly_data, &mesh->face_data, CD_MASK_MESH.pmask, CD_CONSTRUCT, mesh->faces_num);

    int offset = 0;
    for (const int i : orig_indices.index_range()) {
      offsets[i] = offset;
      offset += polys[orig_indices[i]].totloop;
    }

    threading::parallel_for(orig_indices.index_range(), 1024, [&](const IndexRange range) {
      for (const int i : range) {
        CustomData_copy_data(&old_poly_data, &mesh->face_data, orig_indices[i], i, 1);
      }
    });

    CustomData_free(&old_poly_data);
  }

  CustomData_free_layers(&mesh->face_data, CD_MPOLY);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Auto Smooth Conversion
 * \{ */

namespace blender::bke {

static bNodeTree *add_auto_smooth_node_tree(Main &bmain, Library *owner_library)
{
  bNodeTree *group = node_tree_add_in_lib(
      &bmain, owner_library, DATA_("Auto Smooth"), "GeometryNodeTree");
  if (!group->geometry_node_asset_traits) {
    group->geometry_node_asset_traits = MEM_callocN<GeometryNodeAssetTraits>(__func__);
  }
  group->geometry_node_asset_traits->flag |= GEO_NODE_ASSET_MODIFIER;

  group->tree_interface.add_socket(
      DATA_("Geometry"), "", "NodeSocketGeometry", NODE_INTERFACE_SOCKET_OUTPUT, nullptr);
  group->tree_interface.add_socket(
      DATA_("Geometry"), "", "NodeSocketGeometry", NODE_INTERFACE_SOCKET_INPUT, nullptr);
  bNodeTreeInterfaceSocket *angle_io_socket = group->tree_interface.add_socket(
      DATA_("Angle"), "", "NodeSocketFloat", NODE_INTERFACE_SOCKET_INPUT, nullptr);
  auto &angle_data = *static_cast<bNodeSocketValueFloat *>(angle_io_socket->socket_data);
  angle_data.min = 0.0f;
  angle_data.max = DEG2RADF(180.0f);
  angle_data.subtype = PROP_ANGLE;

  bNode *group_output = node_add_node(nullptr, *group, "NodeGroupOutput");
  group_output->location[0] = 480.0f;
  group_output->location[1] = -100.0f;
  bNode *group_input_angle = node_add_node(nullptr, *group, "NodeGroupInput");
  group_input_angle->location[0] = -420.0f;
  group_input_angle->location[1] = -300.0f;
  LISTBASE_FOREACH (bNodeSocket *, socket, &group_input_angle->outputs) {
    if (!STREQ(socket->identifier, "Socket_2")) {
      socket->flag |= SOCK_HIDDEN;
    }
  }
  bNode *group_input_mesh = node_add_node(nullptr, *group, "NodeGroupInput");
  group_input_mesh->location[0] = -60.0f;
  group_input_mesh->location[1] = -100.0f;
  LISTBASE_FOREACH (bNodeSocket *, socket, &group_input_mesh->outputs) {
    if (!STREQ(socket->identifier, "Socket_1")) {
      socket->flag |= SOCK_HIDDEN;
    }
  }
  bNode *shade_smooth_edge = node_add_node(nullptr, *group, "GeometryNodeSetShadeSmooth");
  shade_smooth_edge->custom1 = int16_t(bke::AttrDomain::Edge);
  shade_smooth_edge->location[0] = 120.0f;
  shade_smooth_edge->location[1] = -100.0f;
  bNode *shade_smooth_face = node_add_node(nullptr, *group, "GeometryNodeSetShadeSmooth");
  shade_smooth_face->custom1 = int16_t(bke::AttrDomain::Face);
  shade_smooth_face->location[0] = 300.0f;
  shade_smooth_face->location[1] = -100.0f;
  bNode *edge_angle = node_add_node(nullptr, *group, "GeometryNodeInputMeshEdgeAngle");
  edge_angle->location[0] = -420.0f;
  edge_angle->location[1] = -220.0f;
  bNode *edge_smooth = node_add_node(nullptr, *group, "GeometryNodeInputEdgeSmooth");
  edge_smooth->location[0] = -60.0f;
  edge_smooth->location[1] = -160.0f;
  bNode *face_smooth = node_add_node(nullptr, *group, "GeometryNodeInputShadeSmooth");
  face_smooth->location[0] = -240.0f;
  face_smooth->location[1] = -340.0f;
  bNode *boolean_and = node_add_node(nullptr, *group, "FunctionNodeBooleanMath");
  boolean_and->custom1 = NODE_BOOLEAN_MATH_AND;
  boolean_and->location[0] = -60.0f;
  boolean_and->location[1] = -220.0f;
  bNode *less_than_or_equal = node_add_node(nullptr, *group, "FunctionNodeCompare");
  static_cast<NodeFunctionCompare *>(less_than_or_equal->storage)->operation =
      NODE_COMPARE_LESS_EQUAL;
  less_than_or_equal->location[0] = -240.0f;
  less_than_or_equal->location[1] = -180.0f;

  node_add_link(*group,
                *edge_angle,
                *node_find_socket(*edge_angle, SOCK_OUT, "Unsigned Angle"),
                *less_than_or_equal,
                *node_find_socket(*less_than_or_equal, SOCK_IN, "A"));
  node_add_link(*group,
                *shade_smooth_face,
                *node_find_socket(*shade_smooth_face, SOCK_OUT, "Geometry"),
                *group_output,
                *node_find_socket(*group_output, SOCK_IN, "Socket_0"));
  node_add_link(*group,
                *group_input_angle,
                *node_find_socket(*group_input_angle, SOCK_OUT, "Socket_2"),
                *less_than_or_equal,
                *node_find_socket(*less_than_or_equal, SOCK_IN, "B"));
  node_add_link(*group,
                *less_than_or_equal,
                *node_find_socket(*less_than_or_equal, SOCK_OUT, "Result"),
                *boolean_and,
                *node_find_socket(*boolean_and, SOCK_IN, "Boolean"));
  node_add_link(*group,
                *face_smooth,
                *node_find_socket(*face_smooth, SOCK_OUT, "Smooth"),
                *boolean_and,
                *node_find_socket(*boolean_and, SOCK_IN, "Boolean_001"));
  node_add_link(*group,
                *group_input_mesh,
                *node_find_socket(*group_input_mesh, SOCK_OUT, "Socket_1"),
                *shade_smooth_edge,
                *node_find_socket(*shade_smooth_edge, SOCK_IN, "Geometry"));
  node_add_link(*group,
                *edge_smooth,
                *node_find_socket(*edge_smooth, SOCK_OUT, "Smooth"),
                *shade_smooth_edge,
                *node_find_socket(*shade_smooth_edge, SOCK_IN, "Selection"));
  node_add_link(*group,
                *shade_smooth_edge,
                *node_find_socket(*shade_smooth_edge, SOCK_OUT, "Geometry"),
                *shade_smooth_face,
                *node_find_socket(*shade_smooth_face, SOCK_IN, "Geometry"));
  node_add_link(*group,
                *boolean_and,
                *node_find_socket(*boolean_and, SOCK_OUT, "Boolean"),
                *shade_smooth_edge,
                *node_find_socket(*shade_smooth_edge, SOCK_IN, "Shade Smooth"));

  LISTBASE_FOREACH (bNode *, node, &group->nodes) {
    node_set_selected(*node, false);
  }

  BKE_ntree_update_after_single_tree_change(bmain, *group);

  return group;
}

static VectorSet<const bNodeSocket *> build_socket_indices(const Span<const bNode *> nodes)
{
  VectorSet<const bNodeSocket *> result;
  for (const bNode *node : nodes) {
    LISTBASE_FOREACH (const bNodeSocket *, socket, &node->inputs) {
      result.add_new(socket);
    }
    LISTBASE_FOREACH (const bNodeSocket *, socket, &node->outputs) {
      result.add_new(socket);
    }
  }
  return result;
}

/* Checks if the node group is the same as the one generated by #create_auto_smooth_modifier. */
static bool is_auto_smooth_node_tree(const bNodeTree &group)
{
  if (group.type != NTREE_GEOMETRY) {
    return false;
  }
  const Span<const bNode *> nodes = group.all_nodes();
  if (nodes.size() != 10) {
    return false;
  }
  if (!group.geometry_node_asset_traits) {
    return false;
  }
  if (group.geometry_node_asset_traits->flag != GEO_NODE_ASSET_MODIFIER) {
    return false;
  }
  const std::array<StringRef, 10> idnames({"NodeGroupOutput",
                                           "NodeGroupInput",
                                           "NodeGroupInput",
                                           "GeometryNodeSetShadeSmooth",
                                           "GeometryNodeSetShadeSmooth",
                                           "GeometryNodeInputMeshEdgeAngle",
                                           "GeometryNodeInputEdgeSmooth",
                                           "GeometryNodeInputShadeSmooth",
                                           "FunctionNodeBooleanMath",
                                           "FunctionNodeCompare"});
  for (const int i : nodes.index_range()) {
    if (nodes[i]->idname != idnames[i]) {
      return false;
    }
  }
  if (nodes[3]->custom1 != int16_t(bke::AttrDomain::Edge)) {
    return false;
  }
  if (static_cast<bNodeSocket *>(nodes[4]->inputs.last)
          ->default_value_typed<bNodeSocketValueBoolean>()
          ->value != 1)
  {
    return false;
  }
  if (nodes[4]->custom1 != int16_t(bke::AttrDomain::Face)) {
    return false;
  }
  if (nodes[8]->custom1 != NODE_BOOLEAN_MATH_AND) {
    return false;
  }
  if (static_cast<NodeFunctionCompare *>(nodes[9]->storage)->operation != NODE_COMPARE_LESS_EQUAL)
  {
    return false;
  }
  if (BLI_listbase_count(&group.links) != 9) {
    return false;
  }

  const std::array<int, 9> link_from_socket_indices({16, 15, 3, 36, 19, 5, 18, 11, 22});
  const std::array<int, 9> link_to_socket_indices({23, 0, 24, 20, 21, 8, 9, 12, 10});
  const VectorSet<const bNodeSocket *> socket_indices = build_socket_indices(nodes);
  int i;
  LISTBASE_FOREACH_INDEX (const bNodeLink *, link, &group.links, i) {
    if (socket_indices.index_of(link->fromsock) != link_from_socket_indices[i]) {
      return false;
    }
    if (socket_indices.index_of(link->tosock) != link_to_socket_indices[i]) {
      return false;
    }
  }

  return true;
}

static ModifierData *create_auto_smooth_modifier(
    Object &object,
    const FunctionRef<bNodeTree *(Library *owner_library)> get_node_group,
    const float angle)
{
  auto *md = reinterpret_cast<NodesModifierData *>(BKE_modifier_new(eModifierType_Nodes));
  STRNCPY_UTF8(md->modifier.name, DATA_("Auto Smooth"));
  BKE_modifier_unique_name(&object.modifiers, &md->modifier);
  md->node_group = get_node_group(object.id.lib);
  id_us_plus(&md->node_group->id);

  md->settings.properties = idprop::create_group("Nodes Modifier Settings").release();
  IDProperty *angle_prop = idprop::create("Socket_2", angle).release();
  auto *ui_data = reinterpret_cast<IDPropertyUIDataFloat *>(IDP_ui_data_ensure(angle_prop));
  ui_data->base.rna_subtype = PROP_ANGLE;
  ui_data->soft_min = 0.0f;
  ui_data->soft_max = DEG2RADF(180.0f);
  IDP_AddToGroup(md->settings.properties, angle_prop);
  IDP_AddToGroup(md->settings.properties, idprop::create("Socket_2_use_attribute", 0).release());
  IDP_AddToGroup(md->settings.properties, idprop::create("Socket_2_attribute_name", "").release());

  BKE_modifiers_persistent_uid_init(object, md->modifier);
  return &md->modifier;
}

}  // namespace blender::bke

void BKE_main_mesh_legacy_convert_auto_smooth(Main &bmain)
{
  using namespace blender;
  using namespace blender::bke;

  /* Add the node group lazily and share it among all objects in the same library. */
  Map<Library *, bNodeTree *> group_by_library;
  const auto add_node_group = [&](Library *owner_library) {
    if (bNodeTree **group = group_by_library.lookup_ptr(owner_library)) {
      /* Node tree has already been found/created for this versioning call. */
      return *group;
    }
    /* Try to find an existing group added by previous versioning to avoid adding duplicates. */
    LISTBASE_FOREACH (bNodeTree *, existing_group, &bmain.nodetrees) {
      if (existing_group->id.lib != owner_library) {
        continue;
      }
      if (is_auto_smooth_node_tree(*existing_group)) {
        group_by_library.add_new(owner_library, existing_group);
        return existing_group;
      }
    }
    bNodeTree *new_group = add_auto_smooth_node_tree(bmain, owner_library);
    /* Remove the default user. The count is tracked manually when assigning to modifiers. */
    id_us_min(&new_group->id);
    group_by_library.add_new(owner_library, new_group);
    return new_group;
  };

  LISTBASE_FOREACH (Object *, object, &bmain.objects) {
    if (object->type != OB_MESH) {
      continue;
    }
    Mesh *mesh = static_cast<Mesh *>(object->data);
    const float angle = mesh->smoothresh_legacy;
    if (!(mesh->flag & ME_AUTOSMOOTH_LEGACY)) {
      continue;
    }

    /* Auto-smooth disabled sharp edge tagging when the evaluated mesh had custom normals.
     * When the original mesh has custom normals, that's a good sign the evaluated mesh will
     * have custom normals as well. */
    bool has_custom_normals = CustomData_has_layer(&mesh->corner_data, CD_CUSTOMLOOPNORMAL) ||
                              CustomData_has_layer_named(
                                  &mesh->corner_data, CD_PROP_INT16_2D, "custom_normal");
    if (has_custom_normals) {
      continue;
    }

    /* The "Weighted Normal" modifier has a "Keep Sharp" option that used to recalculate the sharp
     * edge tags based on the mesh's smoothing angle. To keep the same behavior, a new modifier has
     * to be added before that modifier when the option is on. */
    LISTBASE_FOREACH (ModifierData *, md, &object->modifiers) {
      if (ELEM(md->type, eModifierType_WeightedNormal, eModifierType_NormalEdit)) {
        has_custom_normals = true;
      }
      if (md->type == eModifierType_Bevel) {
        BevelModifierData *bmd = reinterpret_cast<BevelModifierData *>(md);
        if (bmd->flags & MOD_BEVEL_HARDEN_NORMALS) {
          has_custom_normals = true;
        }
      }
      if (md->type == eModifierType_WeightedNormal) {
        WeightedNormalModifierData *nmd = reinterpret_cast<WeightedNormalModifierData *>(md);
        if ((nmd->flag & MOD_WEIGHTEDNORMAL_KEEP_SHARP) != 0) {
          ModifierData *new_md = create_auto_smooth_modifier(*object, add_node_group, angle);
          BLI_insertlinkbefore(&object->modifiers, object->modifiers.last, new_md);
        }
      }
      if (md->type == eModifierType_Nodes) {
        NodesModifierData *nmd = reinterpret_cast<NodesModifierData *>(md);
        if (nmd->node_group && is_auto_smooth_node_tree(*nmd->node_group)) {
          /* This object has already been processed by versioning. If the mesh is linked from
           * another file its auto-smooth flag may not be cleared, so this check is necessary to
           * avoid adding a duplicate modifier. */
          has_custom_normals = true;
          break;
        }
      }
    }

    /* Some modifiers always generate custom normals which disabled sharp edge tagging, making
     * adding a modifier at the end unnecessary. Conceptually this is similar to checking if the
     * evaluated mesh had custom normals. */
    if (has_custom_normals) {
      continue;
    }

    ModifierData *last_md = static_cast<ModifierData *>(object->modifiers.last);
    ModifierData *new_md = create_auto_smooth_modifier(*object, add_node_group, angle);
    if (last_md && last_md->type == eModifierType_Subsurf && has_custom_normals &&
        (reinterpret_cast<SubsurfModifierData *>(last_md)->flags &
         eSubsurfModifierFlag_UseCustomNormals) != 0)
    {
      /* Add the auto smooth node group before the last subdivision surface modifier if possible.
       * Subdivision surface modifiers have special handling for interpolating custom normals. */
      BLI_insertlinkbefore(&object->modifiers, object->modifiers.last, new_md);
    }
    else {
      BLI_addtail(&object->modifiers, new_md);
    }
  }

  LISTBASE_FOREACH (Mesh *, mesh, &bmain.meshes) {
    mesh->flag &= ~ME_AUTOSMOOTH_LEGACY;
  }
}

namespace blender::bke {

void mesh_sculpt_mask_to_generic(Mesh &mesh)
{
  if (mesh.attributes().contains(".sculpt_mask")) {
    return;
  }
  void *data = nullptr;
  const ImplicitSharingInfo *sharing_info = nullptr;
  for (const int i : IndexRange(mesh.vert_data.totlayer)) {
    CustomDataLayer &layer = mesh.vert_data.layers[i];
    if (layer.type == CD_PAINT_MASK) {
      data = layer.data;
      sharing_info = layer.sharing_info;
      layer.data = nullptr;
      layer.sharing_info = nullptr;
      CustomData_free_layer(&mesh.vert_data, CD_PAINT_MASK, i);
      break;
    }
  }
  if (data != nullptr) {
    CustomData_add_layer_named_with_data(
        &mesh.vert_data, CD_PROP_FLOAT, data, mesh.verts_num, ".sculpt_mask", sharing_info);
  }
  if (sharing_info != nullptr) {
    sharing_info->remove_user_and_delete_if_last();
  }
}

void mesh_freestyle_marks_to_generic(Mesh &mesh)
{
  {
    void *data = nullptr;
    const ImplicitSharingInfo *sharing_info = nullptr;
    for (const int i : IndexRange(mesh.edge_data.totlayer)) {
      CustomDataLayer &layer = mesh.edge_data.layers[i];
      if (layer.type == CD_FREESTYLE_EDGE) {
        data = layer.data;
        sharing_info = layer.sharing_info;
        layer.data = nullptr;
        layer.sharing_info = nullptr;
        CustomData_free_layer(&mesh.edge_data, CD_FREESTYLE_EDGE, i);
        break;
      }
    }
    if (data != nullptr) {
      static_assert(sizeof(FreestyleEdge) == sizeof(bool));
      static_assert(char(FREESTYLE_EDGE_MARK) == char(true));
      CustomData_add_layer_named_with_data(
          &mesh.edge_data, CD_PROP_BOOL, data, mesh.edges_num, "freestyle_edge", sharing_info);
    }
    if (sharing_info != nullptr) {
      sharing_info->remove_user_and_delete_if_last();
    }
  }
  {
    void *data = nullptr;
    const ImplicitSharingInfo *sharing_info = nullptr;
    for (const int i : IndexRange(mesh.face_data.totlayer)) {
      CustomDataLayer &layer = mesh.face_data.layers[i];
      if (layer.type == CD_FREESTYLE_FACE) {
        data = layer.data;
        sharing_info = layer.sharing_info;
        layer.data = nullptr;
        layer.sharing_info = nullptr;
        CustomData_free_layer(&mesh.face_data, CD_FREESTYLE_FACE, i);
        break;
      }
    }
    if (data != nullptr) {
      static_assert(sizeof(FreestyleFace) == sizeof(bool));
      static_assert(char(FREESTYLE_FACE_MARK) == char(true));
      CustomData_add_layer_named_with_data(
          &mesh.face_data, CD_PROP_BOOL, data, mesh.faces_num, "freestyle_face", sharing_info);
    }
    if (sharing_info != nullptr) {
      sharing_info->remove_user_and_delete_if_last();
    }
  }
}

void mesh_freestyle_marks_to_legacy(AttributeStorage::BlendWriteData &attr_write_data,
                                    CustomData &edge_data,
                                    CustomData &face_data,
                                    Vector<CustomDataLayer, 16> &edge_layers,
                                    Vector<CustomDataLayer, 16> &face_layers)
{
  Array<bool, 64> attrs_to_remove(attr_write_data.attributes.size(), false);
  for (const int i : attr_write_data.attributes.index_range()) {
    const ::Attribute &dna_attr = attr_write_data.attributes[i];
    if (dna_attr.data_type != int8_t(AttrType::Bool)) {
      continue;
    }
    if (dna_attr.storage_type != int8_t(AttrStorageType::Array)) {
      continue;
    }
    if (dna_attr.domain == int8_t(AttrDomain::Edge)) {
      if (STREQ(dna_attr.name, "freestyle_edge")) {
        const auto &array_dna = *static_cast<const ::AttributeArray *>(dna_attr.data);
        static_assert(sizeof(FreestyleEdge) == sizeof(bool));
        static_assert(char(FREESTYLE_EDGE_MARK) == char(true));
        CustomDataLayer layer{};
        layer.type = CD_FREESTYLE_EDGE;
        layer.data = array_dna.data;
        layer.sharing_info = array_dna.sharing_info;
        edge_layers.append(layer);
        std::stable_sort(
            edge_layers.begin(),
            edge_layers.end(),
            [](const CustomDataLayer &a, const CustomDataLayer &b) { return a.type < b.type; });
        if (!edge_data.layers) {
          /* edge_data.layers must not be null, or the layers will not be written. Its address
           * doesn't really matter, but it must be unique within this ID.*/
          edge_data.layers = edge_layers.data();
        }
        edge_data.totlayer = edge_layers.size();
        edge_data.maxlayer = edge_data.totlayer;
        attrs_to_remove[i] = true;
      }
    }
    else if (dna_attr.domain == int8_t(AttrDomain::Face)) {
      if (STREQ(dna_attr.name, "freestyle_face")) {
        const auto &array_dna = *static_cast<const ::AttributeArray *>(dna_attr.data);
        static_assert(sizeof(FreestyleFace) == sizeof(bool));
        static_assert(char(FREESTYLE_FACE_MARK) == char(true));
        CustomDataLayer layer{};
        layer.type = CD_FREESTYLE_FACE;
        layer.data = array_dna.data;
        layer.sharing_info = array_dna.sharing_info;
        face_layers.append(layer);
        std::stable_sort(
            face_layers.begin(),
            face_layers.end(),
            [](const CustomDataLayer &a, const CustomDataLayer &b) { return a.type < b.type; });
        if (!face_data.layers) {
          /* face_data.layers must not be null, or the layers will not be written. Its address
           * doesn't really matter, but it must be unique within this ID.*/
          face_data.layers = face_layers.data();
        }
        face_data.totlayer = face_layers.size();
        face_data.maxlayer = face_data.totlayer;
        attrs_to_remove[i] = true;
      }
    }
  }
  attr_write_data.attributes.remove_if([&](const ::Attribute &attr) {
    const int i = &attr - attr_write_data.attributes.begin();
    return attrs_to_remove[i];
  });
}

void mesh_custom_normals_to_generic(Mesh &mesh)
{
  if (mesh.attributes().contains("custom_normal")) {
    return;
  }
  void *data = nullptr;
  const ImplicitSharingInfo *sharing_info = nullptr;
  for (const int i : IndexRange(mesh.corner_data.totlayer)) {
    CustomDataLayer &layer = mesh.corner_data.layers[i];
    if (layer.type == CD_CUSTOMLOOPNORMAL) {
      data = layer.data;
      sharing_info = layer.sharing_info;
      layer.data = nullptr;
      layer.sharing_info = nullptr;
      CustomData_free_layer(&mesh.corner_data, CD_CUSTOMLOOPNORMAL, i);
      break;
    }
  }
  if (data != nullptr) {
    CustomData_add_layer_named_with_data(&mesh.corner_data,
                                         CD_PROP_INT16_2D,
                                         data,
                                         mesh.corners_num,
                                         "custom_normal",
                                         sharing_info);
  }
  if (sharing_info != nullptr) {
    sharing_info->remove_user_and_delete_if_last();
  }
}

void mesh_uv_select_to_single_attribute(Mesh &mesh)
{
  const char *name = CustomData_get_active_layer_name(&mesh.corner_data, CD_PROP_FLOAT2);
  if (!name) {
    return;
  }
  const std::string uv_select_vert_name_shared = ".uv_select_vert";
  const std::string uv_select_edge_name_shared = ".uv_select_edge";
  const std::string uv_select_face_name_shared = ".uv_select_face";

  const std::string uv_select_vert_prefix = ".vs.";
  const std::string uv_select_edge_prefix = ".es.";

  const std::string uv_select_vert_name = uv_select_vert_prefix + name;
  const std::string uv_select_edge_name = uv_select_edge_prefix + name;

  const int uv_select_vert = CustomData_get_named_layer_index(
      &mesh.corner_data, CD_PROP_BOOL, uv_select_vert_name);
  const int uv_select_edge = CustomData_get_named_layer_index(
      &mesh.corner_data, CD_PROP_BOOL, uv_select_edge_name);

  if (uv_select_vert != -1 && uv_select_edge != -1) {
    /* Unlikely either exist but ensure there are no duplicate names. */
    CustomData_free_layer_named(&mesh.corner_data, uv_select_vert_name_shared);
    CustomData_free_layer_named(&mesh.corner_data, uv_select_edge_name_shared);
    CustomData_free_layer_named(&mesh.face_data, uv_select_face_name_shared);

    STRNCPY_UTF8(mesh.corner_data.layers[uv_select_vert].name, uv_select_vert_name_shared.c_str());
    STRNCPY_UTF8(mesh.corner_data.layers[uv_select_edge].name, uv_select_edge_name_shared.c_str());

    bool *uv_select_face = MEM_malloc_arrayN<bool>(mesh.faces_num, __func__);
    CustomData_add_layer_named_with_data(&mesh.face_data,
                                         CD_PROP_BOOL,
                                         uv_select_face,
                                         mesh.faces_num,
                                         uv_select_face_name_shared,
                                         nullptr);

    /* Create a face selection layer (flush from edges). */
    if (mesh.faces_num > 0) {
      const OffsetIndices<int> faces = mesh.faces();
      const Span<bool> uv_select_edge_data(
          static_cast<bool *>(mesh.corner_data.layers[uv_select_edge].data), mesh.corners_num);
      threading::parallel_for(faces.index_range(), 1024, [&](const IndexRange range) {
        for (const int face : range) {
          uv_select_face[face] = !uv_select_edge_data.slice(faces[face]).contains(false);
        }
      });
    }
  }

  /* Logically a set as names are expected to be unique.
   * If there are duplicates, this will remove those too. */
  Vector<std::string> attributes_to_remove;
  for (const int i : IndexRange(mesh.corner_data.totlayer)) {
    const CustomDataLayer &layer = mesh.corner_data.layers[i];
    if (layer.type != CD_PROP_BOOL) {
      continue;
    }
    StringRef layer_name = StringRef(layer.name);
    if (layer_name.startswith(uv_select_vert_prefix) ||
        layer_name.startswith(uv_select_edge_prefix))
    {
      attributes_to_remove.append(layer.name);
    }
  }

  for (const StringRef name_to_remove : attributes_to_remove) {
    CustomData_free_layer_named(&mesh.corner_data, name_to_remove);
  }
}

}  // namespace blender::bke

/** \} */

void BKE_mesh_calc_edges_tessface(Mesh *mesh)
{
  const int nulegacy_faces = mesh->totface_legacy;
  blender::VectorSet<blender::OrderedEdge> eh;
  eh.reserve(nulegacy_faces);
  MFace *legacy_faces = (MFace *)CustomData_get_layer_for_write(
      &mesh->fdata_legacy, CD_MFACE, mesh->totface_legacy);

  MFace *mf = legacy_faces;
  for (int i = 0; i < nulegacy_faces; i++, mf++) {
    eh.add({mf->v1, mf->v2});
    eh.add({mf->v2, mf->v3});

    if (mf->v4) {
      eh.add({mf->v3, mf->v4});
      eh.add({mf->v4, mf->v1});
    }
    else {
      eh.add({mf->v3, mf->v1});
    }
  }

  const int numEdges = eh.size();

  /* write new edges into a temporary CustomData */
  CustomData edgeData;
  CustomData_reset(&edgeData);
  CustomData_add_layer_named(&edgeData, CD_PROP_INT32_2D, CD_CONSTRUCT, numEdges, ".edge_verts");
  CustomData_add_layer(&edgeData, CD_ORIGINDEX, CD_SET_DEFAULT, numEdges);

  blender::int2 *ege = (blender::int2 *)CustomData_get_layer_named_for_write(
      &edgeData, CD_PROP_INT32_2D, ".edge_verts", mesh->edges_num);
  int *index = (int *)CustomData_get_layer_for_write(&edgeData, CD_ORIGINDEX, mesh->edges_num);

  memset(index, ORIGINDEX_NONE, sizeof(int) * numEdges);
  MutableSpan(ege, numEdges).copy_from(eh.as_span().cast<blender::int2>());

  /* free old CustomData and assign new one */
  CustomData_free(&mesh->edge_data);
  mesh->edge_data = edgeData;
  mesh->edges_num = numEdges;
}
