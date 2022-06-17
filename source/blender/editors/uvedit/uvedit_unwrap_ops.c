/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2001-2002 NaN Holding BV. All rights reserved. */

/** \file
 * \ingroup eduv
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_camera_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BLI_alloca.h"
#include "BLI_array.h"
#include "BLI_linklist.h"
#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_memarena.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"
#include "BLI_uvproject.h"

#include "BLT_translation.h"

#include "BKE_cdderivedmesh.h"
#include "BKE_context.h"
#include "BKE_customdata.h"
#include "BKE_editmesh.h"
#include "BKE_image.h"
#include "BKE_layer.h"
#include "BKE_lib_id.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_mesh.h"
#include "BKE_report.h"
#include "BKE_scene.h"
#include "BKE_subsurf.h"

#include "DEG_depsgraph.h"

#include "GEO_uv_parametrizer.h"

#include "PIL_time.h"

#include "UI_interface.h"
#include "UI_view2d.h"

#include "ED_image.h"
#include "ED_mesh.h"
#include "ED_screen.h"
#include "ED_uvedit.h"
#include "ED_view3d.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"

#include "uvedit_intern.h"

/* -------------------------------------------------------------------- */
/** \name Utility Functions
 * \{ */

static void modifier_unwrap_state(Object *obedit, const Scene *scene, bool *r_use_subsurf)
{
  ModifierData *md;
  bool subsurf = (scene->toolsettings->uvcalc_flag & UVCALC_USESUBSURF) != 0;

  md = obedit->modifiers.first;

  /* subsurf will take the modifier settings only if modifier is first or right after mirror */
  if (subsurf) {
    if (md && md->type == eModifierType_Subsurf) {
      subsurf = true;
    }
    else {
      subsurf = false;
    }
  }

  *r_use_subsurf = subsurf;
}

static bool ED_uvedit_ensure_uvs(Object *obedit)
{
  if (ED_uvedit_test(obedit)) {
    return 1;
  }

  BMEditMesh *em = BKE_editmesh_from_object(obedit);
  BMFace *efa;
  BMIter iter;
  int cd_loop_uv_offset;

  if (em && em->bm->totface && !CustomData_has_layer(&em->bm->ldata, CD_MLOOPUV)) {
    ED_mesh_uv_add(obedit->data, NULL, true, true, NULL);
  }

  /* Happens when there are no faces. */
  if (!ED_uvedit_test(obedit)) {
    return 0;
  }

  cd_loop_uv_offset = CustomData_get_offset(&em->bm->ldata, CD_MLOOPUV);

  /* select new UV's (ignore UV_SYNC_SELECTION in this case) */
  BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
    BMIter liter;
    BMLoop *l;

    BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
      MLoopUV *luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
      luv->flag |= (MLOOPUV_VERTSEL | MLOOPUV_EDGESEL);
    }
  }

  return 1;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name UDIM Access
 * \{ */

bool ED_uvedit_udim_params_from_image_space(const SpaceImage *sima,
                                            bool use_active,
                                            struct UVMapUDIM_Params *udim_params)
{
  memset(udim_params, 0, sizeof(*udim_params));

  udim_params->grid_shape[0] = 1;
  udim_params->grid_shape[1] = 1;
  udim_params->target_udim = 0;
  udim_params->use_target_udim = false;

  if (sima == NULL) {
    return false;
  }

  udim_params->image = sima->image;
  udim_params->grid_shape[0] = sima->tile_grid_shape[0];
  udim_params->grid_shape[1] = sima->tile_grid_shape[1];

  if (use_active) {
    int active_udim = 1001;
    /* NOTE: Presently, when UDIM grid and tiled image are present together, only active tile for
     * the tiled image is considered. */
    const Image *image = sima->image;
    if (image && image->source == IMA_SRC_TILED) {
      ImageTile *active_tile = BLI_findlink(&image->tiles, image->active_tile_index);
      if (active_tile) {
        active_udim = active_tile->tile_number;
      }
    }
    else {
      /* TODO: Support storing an active UDIM when there are no tiles present.
       * Until then, use 2D cursor to find the active tile index for the UDIM grid. */
      if (uv_coords_isect_udim(sima->image, sima->tile_grid_shape, sima->cursor)) {
        int tile_number = 1001;
        tile_number += floorf(sima->cursor[1]) * 10;
        tile_number += floorf(sima->cursor[0]);
        active_udim = tile_number;
      }
    }

    udim_params->target_udim = active_udim;
    udim_params->use_target_udim = true;
  }

  return true;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Parametrizer Conversion
 * \{ */

typedef struct UnwrapOptions {
  /** Connectivity based on UV coordinates instead of seams. */
  bool topology_from_uvs;
  /** Also use seams as well as UV coordinates (only valid when `topology_from_uvs` is enabled). */
  bool topology_from_uvs_use_seams;
  /** Only affect selected faces. */
  bool only_selected_faces;
  /**
   * Only affect selected UV's.
   * \note Disable this for operations that don't run in the image-window.
   * Unwrapping from the 3D view for example, where only 'only_selected_faces' should be used.
   */
  bool only_selected_uvs;
  /** Fill holes to better preserve shape. */
  bool fill_holes;
  /** Correct for mapped image texture aspect ratio. */
  bool correct_aspect;
} UnwrapOptions;

typedef struct UnwrapResultInfo {
  int count_changed;
  int count_failed;
} UnwrapResultInfo;

static bool uvedit_have_selection(const Scene *scene, BMEditMesh *em, const UnwrapOptions *options)
{
  BMFace *efa;
  BMLoop *l;
  BMIter iter, liter;
  const int cd_loop_uv_offset = CustomData_get_offset(&em->bm->ldata, CD_MLOOPUV);

  if (cd_loop_uv_offset == -1) {
    return (em->bm->totfacesel != 0);
  }

  /* verify if we have any selected uv's before unwrapping,
   * so we can cancel the operator early */
  BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
    if (scene->toolsettings->uv_flag & UV_SYNC_SELECTION) {
      if (BM_elem_flag_test(efa, BM_ELEM_HIDDEN)) {
        continue;
      }
    }
    else if (!BM_elem_flag_test(efa, BM_ELEM_SELECT)) {
      continue;
    }

    BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
      if (uvedit_uv_select_test(scene, l, cd_loop_uv_offset)) {
        break;
      }
    }

    if (options->topology_from_uvs && !l) {
      continue;
    }

    return true;
  }

  return false;
}

static bool uvedit_have_selection_multi(const Scene *scene,
                                        Object **objects,
                                        const uint objects_len,
                                        const UnwrapOptions *options)
{
  bool have_select = false;
  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *obedit = objects[ob_index];
    BMEditMesh *em = BKE_editmesh_from_object(obedit);
    if (uvedit_have_selection(scene, em, options)) {
      have_select = true;
      break;
    }
  }
  return have_select;
}

void ED_uvedit_get_aspect_from_material(Object *ob,
                                        const int material_index,
                                        float *r_aspx,
                                        float *r_aspy)
{
  if (UNLIKELY(material_index < 0 || material_index >= ob->totcol)) {
    *r_aspx = 1.0f;
    *r_aspy = 1.0f;
    return;
  }
  Image *ima;
  ED_object_get_active_image(ob, material_index + 1, &ima, NULL, NULL, NULL);
  ED_image_get_uv_aspect(ima, NULL, r_aspx, r_aspy);
}

void ED_uvedit_get_aspect(Object *ob, float *r_aspx, float *r_aspy)
{
  BMEditMesh *em = BKE_editmesh_from_object(ob);
  BLI_assert(em != NULL);
  bool sloppy = true;
  bool selected = false;
  BMFace *efa = BM_mesh_active_face_get(em->bm, sloppy, selected);
  if (!efa) {
    *r_aspx = 1.0f;
    *r_aspy = 1.0f;
    return;
  }

  ED_uvedit_get_aspect_from_material(ob, efa->mat_nr, r_aspx, r_aspy);
}

static bool uvedit_is_face_affected(const Scene *scene,
                                    BMFace *efa,
                                    const UnwrapOptions *options,
                                    const int cd_loop_uv_offset)
{
  if (BM_elem_flag_test(efa, BM_ELEM_HIDDEN)) {
    return false;
  }

  if (options->only_selected_faces && !BM_elem_flag_test(efa, BM_ELEM_SELECT)) {
    return false;
  }

  if (options->topology_from_uvs && options->only_selected_uvs &&
      !uvedit_face_select_test(scene, efa, cd_loop_uv_offset)) {
    return false;
  }

  return true;
}

/* Prepare unique indices for each unique pinned UV, even if it shares a BMVert.
 */
static void uvedit_prepare_pinned_indices(ParamHandle *handle,
                                          BMFace *efa,
                                          const int cd_loop_uv_offset)
{
  BMIter liter;
  BMLoop *l;
  BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
    MLoopUV *luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
    if (luv->flag & MLOOPUV_PINNED) {
      int bmvertindex = BM_elem_index_get(l->v);
      GEO_uv_prepare_pin_index(handle, bmvertindex, luv->uv);
    }
  }
}

static void construct_param_handle_face_add(ParamHandle *handle,
                                            const Scene *scene,
                                            BMFace *efa,
                                            ParamKey face_index,
                                            const int cd_loop_uv_offset)
{
  ParamKey *vkeys = BLI_array_alloca(vkeys, efa->len);
  bool *pin = BLI_array_alloca(pin, efa->len);
  bool *select = BLI_array_alloca(select, efa->len);
  const float **co = BLI_array_alloca(co, efa->len);
  float **uv = BLI_array_alloca(uv, efa->len);
  int i;

  BMIter liter;
  BMLoop *l;

  /* let parametrizer split the ngon, it can make better decisions
   * about which split is best for unwrapping than poly-fill. */
  BM_ITER_ELEM_INDEX (l, &liter, efa, BM_LOOPS_OF_FACE, i) {
    MLoopUV *luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);

    vkeys[i] = GEO_uv_find_pin_index(handle, BM_elem_index_get(l->v), luv->uv);
    co[i] = l->v->co;
    uv[i] = luv->uv;
    pin[i] = (luv->flag & MLOOPUV_PINNED) != 0;
    select[i] = uvedit_uv_select_test(scene, l, cd_loop_uv_offset);
  }

  GEO_uv_parametrizer_face_add(handle, face_index, i, vkeys, co, uv, pin, select);
}

/* See: construct_param_handle_multi to handle multiple objects at once. */
static ParamHandle *construct_param_handle(const Scene *scene,
                                           Object *ob,
                                           BMesh *bm,
                                           const UnwrapOptions *options,
                                           UnwrapResultInfo *result_info)
{
  BMFace *efa;
  BMEdge *eed;
  BMIter iter;
  int i;

  ParamHandle *handle = GEO_uv_parametrizer_construct_begin();

  if (options->correct_aspect) {
    float aspx, aspy;

    ED_uvedit_get_aspect(ob, &aspx, &aspy);

    if (aspx != aspy) {
      GEO_uv_parametrizer_aspect_ratio(handle, aspx, aspy);
    }
  }

  /* we need the vert indices */
  BM_mesh_elem_index_ensure(bm, BM_VERT);

  const int cd_loop_uv_offset = CustomData_get_offset(&bm->ldata, CD_MLOOPUV);
  BM_ITER_MESH_INDEX (efa, &iter, bm, BM_FACES_OF_MESH, i) {
    if (uvedit_is_face_affected(scene, efa, options, cd_loop_uv_offset)) {
      uvedit_prepare_pinned_indices(handle, efa, cd_loop_uv_offset);
    }
  }

  BM_ITER_MESH_INDEX (efa, &iter, bm, BM_FACES_OF_MESH, i) {
    if (uvedit_is_face_affected(scene, efa, options, cd_loop_uv_offset)) {
      construct_param_handle_face_add(handle, scene, efa, i, cd_loop_uv_offset);
    }
  }

  if (!options->topology_from_uvs || options->topology_from_uvs_use_seams) {
    BM_ITER_MESH (eed, &iter, bm, BM_EDGES_OF_MESH) {
      if (BM_elem_flag_test(eed, BM_ELEM_SEAM)) {
        ParamKey vkeys[2];
        vkeys[0] = (ParamKey)BM_elem_index_get(eed->v1);
        vkeys[1] = (ParamKey)BM_elem_index_get(eed->v2);
        GEO_uv_parametrizer_edge_set_seam(handle, vkeys);
      }
    }
  }

  GEO_uv_parametrizer_construct_end(handle,
                                    options->fill_holes,
                                    options->topology_from_uvs,
                                    result_info ? &result_info->count_failed : NULL);

  return handle;
}

/**
 * Version of #construct_param_handle_single that handles multiple objects.
 */
static ParamHandle *construct_param_handle_multi(const Scene *scene,
                                                 Object **objects,
                                                 const uint objects_len,
                                                 const UnwrapOptions *options,
                                                 int *count_fail)
{
  BMFace *efa;
  BMEdge *eed;
  BMIter iter;
  int i;

  ParamHandle *handle = GEO_uv_parametrizer_construct_begin();

  if (options->correct_aspect) {
    Object *ob = objects[0];
    float aspx, aspy;

    ED_uvedit_get_aspect(ob, &aspx, &aspy);
    if (aspx != aspy) {
      GEO_uv_parametrizer_aspect_ratio(handle, aspx, aspy);
    }
  }

  /* we need the vert indices */
  EDBM_mesh_elem_index_ensure_multi(objects, objects_len, BM_VERT);

  int offset = 0;

  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *obedit = objects[ob_index];
    BMEditMesh *em = BKE_editmesh_from_object(obedit);
    BMesh *bm = em->bm;

    const int cd_loop_uv_offset = CustomData_get_offset(&bm->ldata, CD_MLOOPUV);

    if (cd_loop_uv_offset == -1) {
      continue;
    }

    BM_ITER_MESH_INDEX (efa, &iter, bm, BM_FACES_OF_MESH, i) {
      if (uvedit_is_face_affected(scene, efa, options, cd_loop_uv_offset)) {
        uvedit_prepare_pinned_indices(handle, efa, cd_loop_uv_offset);
      }
    }

    BM_ITER_MESH_INDEX (efa, &iter, bm, BM_FACES_OF_MESH, i) {
      if (uvedit_is_face_affected(scene, efa, options, cd_loop_uv_offset)) {
        construct_param_handle_face_add(handle, scene, efa, i + offset, cd_loop_uv_offset);
      }
    }

    if (!options->topology_from_uvs || options->topology_from_uvs_use_seams) {
      BM_ITER_MESH (eed, &iter, bm, BM_EDGES_OF_MESH) {
        if (BM_elem_flag_test(eed, BM_ELEM_SEAM)) {
          ParamKey vkeys[2];
          vkeys[0] = (ParamKey)BM_elem_index_get(eed->v1);
          vkeys[1] = (ParamKey)BM_elem_index_get(eed->v2);
          GEO_uv_parametrizer_edge_set_seam(handle, vkeys);
        }
      }
    }
    offset += bm->totface;
  }

  GEO_uv_parametrizer_construct_end(
      handle, options->fill_holes, options->topology_from_uvs, count_fail);

  return handle;
}

static void texface_from_original_index(const Scene *scene,
                                        const int cd_loop_uv_offset,
                                        BMFace *efa,
                                        int index,
                                        float **r_uv,
                                        bool *r_pin,
                                        bool *r_select)
{
  BMLoop *l;
  BMIter liter;
  MLoopUV *luv;

  *r_uv = NULL;
  *r_pin = 0;
  *r_select = 1;

  if (index == ORIGINDEX_NONE) {
    return;
  }

  BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
    if (BM_elem_index_get(l->v) == index) {
      luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
      *r_uv = luv->uv;
      *r_pin = (luv->flag & MLOOPUV_PINNED) ? 1 : 0;
      *r_select = uvedit_uv_select_test(scene, l, cd_loop_uv_offset);
      break;
    }
  }
}

/**
 * Unwrap handle initialization for subsurf aware-unwrapper.
 * The many modifications required to make the original function(see above)
 * work justified the existence of a new function.
 */
static ParamHandle *construct_param_handle_subsurfed(const Scene *scene,
                                                     Object *ob,
                                                     BMEditMesh *em,
                                                     const UnwrapOptions *options,
                                                     UnwrapResultInfo *result_info)
{
  /* index pointers */
  MPoly *mpoly;
  MLoop *mloop;
  MEdge *edge;
  int i;

  /* pointers to modifier data for unwrap control */
  ModifierData *md;
  SubsurfModifierData *smd_real;
  /* Modifier initialization data, will  control what type of subdivision will happen. */
  SubsurfModifierData smd = {{NULL}};
  /* Used to hold subsurfed Mesh */
  DerivedMesh *derivedMesh, *initialDerived;
  /* holds original indices for subsurfed mesh */
  const int *origVertIndices, *origEdgeIndices, *origPolyIndices;
  /* Holds vertices of subsurfed mesh */
  MVert *subsurfedVerts;
  MEdge *subsurfedEdges;
  MPoly *subsurfedPolys;
  MLoop *subsurfedLoops;
  /* Number of vertices and faces for subsurfed mesh. */
  int numOfEdges, numOfFaces;

  /* holds a map to editfaces for every subsurfed MFace.
   * These will be used to get hidden/ selected flags etc. */
  BMFace **faceMap;
  /* similar to the above, we need a way to map edges to their original ones */
  BMEdge **edgeMap;

  const int cd_loop_uv_offset = CustomData_get_offset(&em->bm->ldata, CD_MLOOPUV);

  ParamHandle *handle = GEO_uv_parametrizer_construct_begin();

  if (options->correct_aspect) {
    float aspx, aspy;

    ED_uvedit_get_aspect(ob, &aspx, &aspy);

    if (aspx != aspy) {
      GEO_uv_parametrizer_aspect_ratio(handle, aspx, aspy);
    }
  }

  /* number of subdivisions to perform */
  md = ob->modifiers.first;
  smd_real = (SubsurfModifierData *)md;

  smd.levels = smd_real->levels;
  smd.subdivType = smd_real->subdivType;

  {
    Mesh *me_from_em = BKE_mesh_from_bmesh_for_eval_nomain(em->bm, NULL, ob->data);
    initialDerived = CDDM_from_mesh(me_from_em);
    derivedMesh = subsurf_make_derived_from_derived(
        initialDerived, &smd, scene, NULL, SUBSURF_IN_EDIT_MODE);

    initialDerived->release(initialDerived);
    BKE_id_free(NULL, me_from_em);
  }

  /* get the derived data */
  subsurfedVerts = derivedMesh->getVertArray(derivedMesh);
  subsurfedEdges = derivedMesh->getEdgeArray(derivedMesh);
  subsurfedPolys = derivedMesh->getPolyArray(derivedMesh);
  subsurfedLoops = derivedMesh->getLoopArray(derivedMesh);

  origVertIndices = derivedMesh->getVertDataArray(derivedMesh, CD_ORIGINDEX);
  origEdgeIndices = derivedMesh->getEdgeDataArray(derivedMesh, CD_ORIGINDEX);
  origPolyIndices = derivedMesh->getPolyDataArray(derivedMesh, CD_ORIGINDEX);

  numOfEdges = derivedMesh->getNumEdges(derivedMesh);
  numOfFaces = derivedMesh->getNumPolys(derivedMesh);

  faceMap = MEM_mallocN(numOfFaces * sizeof(BMFace *), "unwrap_edit_face_map");

  BM_mesh_elem_index_ensure(em->bm, BM_VERT);
  BM_mesh_elem_table_ensure(em->bm, BM_EDGE | BM_FACE);

  /* map subsurfed faces to original editFaces */
  for (i = 0; i < numOfFaces; i++) {
    faceMap[i] = BM_face_at_index(em->bm, origPolyIndices[i]);
  }

  edgeMap = MEM_mallocN(numOfEdges * sizeof(BMEdge *), "unwrap_edit_edge_map");

  /* map subsurfed edges to original editEdges */
  for (i = 0; i < numOfEdges; i++) {
    /* not all edges correspond to an old edge */
    edgeMap[i] = (origEdgeIndices[i] != ORIGINDEX_NONE) ?
                     BM_edge_at_index(em->bm, origEdgeIndices[i]) :
                     NULL;
  }

  /* Prepare and feed faces to the solver */
  for (i = 0, mpoly = subsurfedPolys; i < numOfFaces; i++, mpoly++) {
    ParamKey key, vkeys[4];
    bool pin[4], select[4];
    const float *co[4];
    float *uv[4];
    BMFace *origFace = faceMap[i];

    if (scene->toolsettings->uv_flag & UV_SYNC_SELECTION) {
      if (BM_elem_flag_test(origFace, BM_ELEM_HIDDEN)) {
        continue;
      }
    }
    else {
      if (BM_elem_flag_test(origFace, BM_ELEM_HIDDEN) ||
          (options->only_selected_faces && !BM_elem_flag_test(origFace, BM_ELEM_SELECT))) {
        continue;
      }
    }

    mloop = &subsurfedLoops[mpoly->loopstart];

    /* We will not check for v4 here. Sub-surface faces always have 4 vertices. */
    BLI_assert(mpoly->totloop == 4);
    key = (ParamKey)i;
    vkeys[0] = (ParamKey)mloop[0].v;
    vkeys[1] = (ParamKey)mloop[1].v;
    vkeys[2] = (ParamKey)mloop[2].v;
    vkeys[3] = (ParamKey)mloop[3].v;

    co[0] = subsurfedVerts[mloop[0].v].co;
    co[1] = subsurfedVerts[mloop[1].v].co;
    co[2] = subsurfedVerts[mloop[2].v].co;
    co[3] = subsurfedVerts[mloop[3].v].co;

    /* This is where all the magic is done.
     * If the vertex exists in the, we pass the original uv pointer to the solver, thus
     * flushing the solution to the edit mesh. */
    texface_from_original_index(scene,
                                cd_loop_uv_offset,
                                origFace,
                                origVertIndices[mloop[0].v],
                                &uv[0],
                                &pin[0],
                                &select[0]);
    texface_from_original_index(scene,
                                cd_loop_uv_offset,
                                origFace,
                                origVertIndices[mloop[1].v],
                                &uv[1],
                                &pin[1],
                                &select[1]);
    texface_from_original_index(scene,
                                cd_loop_uv_offset,
                                origFace,
                                origVertIndices[mloop[2].v],
                                &uv[2],
                                &pin[2],
                                &select[2]);
    texface_from_original_index(scene,
                                cd_loop_uv_offset,
                                origFace,
                                origVertIndices[mloop[3].v],
                                &uv[3],
                                &pin[3],
                                &select[3]);

    GEO_uv_parametrizer_face_add(handle, key, 4, vkeys, co, uv, pin, select);
  }

  /* these are calculated from original mesh too */
  for (edge = subsurfedEdges, i = 0; i < numOfEdges; i++, edge++) {
    if ((edgeMap[i] != NULL) && BM_elem_flag_test(edgeMap[i], BM_ELEM_SEAM)) {
      ParamKey vkeys[2];
      vkeys[0] = (ParamKey)edge->v1;
      vkeys[1] = (ParamKey)edge->v2;
      GEO_uv_parametrizer_edge_set_seam(handle, vkeys);
    }
  }

  GEO_uv_parametrizer_construct_end(handle,
                                    options->fill_holes,
                                    options->topology_from_uvs,
                                    result_info ? &result_info->count_failed : NULL);

  /* cleanup */
  MEM_freeN(faceMap);
  MEM_freeN(edgeMap);
  derivedMesh->release(derivedMesh);

  return handle;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Minimize Stretch Operator
 * \{ */

typedef struct MinStretch {
  const Scene *scene;
  Object **objects_edit;
  uint objects_len;
  ParamHandle *handle;
  float blend;
  double lasttime;
  int i, iterations;
  wmTimer *timer;
} MinStretch;

static bool minimize_stretch_init(bContext *C, wmOperator *op)
{
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);

  const UnwrapOptions options = {
      .topology_from_uvs = true,
      .fill_holes = RNA_boolean_get(op->ptr, "fill_holes"),
      .only_selected_faces = true,
      .only_selected_uvs = true,
      .correct_aspect = true,
  };

  uint objects_len = 0;
  Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data_with_uvs(
      view_layer, CTX_wm_view3d(C), &objects_len);

  if (!uvedit_have_selection_multi(scene, objects, objects_len, &options)) {
    MEM_freeN(objects);
    return false;
  }

  MinStretch *ms = MEM_callocN(sizeof(MinStretch), "MinStretch");
  ms->scene = scene;
  ms->objects_edit = objects;
  ms->objects_len = objects_len;
  ms->blend = RNA_float_get(op->ptr, "blend");
  ms->iterations = RNA_int_get(op->ptr, "iterations");
  ms->i = 0;
  ms->handle = construct_param_handle_multi(scene, objects, objects_len, &options, NULL);
  ms->lasttime = PIL_check_seconds_timer();

  GEO_uv_parametrizer_stretch_begin(ms->handle);
  if (ms->blend != 0.0f) {
    GEO_uv_parametrizer_stretch_blend(ms->handle, ms->blend);
  }

  op->customdata = ms;

  return true;
}

static void minimize_stretch_iteration(bContext *C, wmOperator *op, bool interactive)
{
  MinStretch *ms = op->customdata;
  ScrArea *area = CTX_wm_area(C);
  const Scene *scene = CTX_data_scene(C);
  ToolSettings *ts = scene->toolsettings;
  const bool synced_selection = (ts->uv_flag & UV_SYNC_SELECTION) != 0;

  GEO_uv_parametrizer_stretch_blend(ms->handle, ms->blend);
  GEO_uv_parametrizer_stretch_iter(ms->handle);

  ms->i++;
  RNA_int_set(op->ptr, "iterations", ms->i);

  if (interactive && (PIL_check_seconds_timer() - ms->lasttime > 0.5)) {
    char str[UI_MAX_DRAW_STR];

    GEO_uv_parametrizer_flush(ms->handle);

    if (area) {
      BLI_snprintf(str, sizeof(str), TIP_("Minimize Stretch. Blend %.2f"), ms->blend);
      ED_area_status_text(area, str);
      ED_workspace_status_text(C, TIP_("Press + and -, or scroll wheel to set blending"));
    }

    ms->lasttime = PIL_check_seconds_timer();

    for (uint ob_index = 0; ob_index < ms->objects_len; ob_index++) {
      Object *obedit = ms->objects_edit[ob_index];
      BMEditMesh *em = BKE_editmesh_from_object(obedit);

      if (synced_selection && (em->bm->totfacesel == 0)) {
        continue;
      }

      DEG_id_tag_update(obedit->data, ID_RECALC_GEOMETRY);
      WM_event_add_notifier(C, NC_GEOM | ND_DATA, obedit->data);
    }
  }
}

static void minimize_stretch_exit(bContext *C, wmOperator *op, bool cancel)
{
  MinStretch *ms = op->customdata;
  ScrArea *area = CTX_wm_area(C);
  const Scene *scene = CTX_data_scene(C);
  ToolSettings *ts = scene->toolsettings;
  const bool synced_selection = (ts->uv_flag & UV_SYNC_SELECTION) != 0;

  ED_area_status_text(area, NULL);
  ED_workspace_status_text(C, NULL);

  if (ms->timer) {
    WM_event_remove_timer(CTX_wm_manager(C), CTX_wm_window(C), ms->timer);
  }

  if (cancel) {
    GEO_uv_parametrizer_flush_restore(ms->handle);
  }
  else {
    GEO_uv_parametrizer_flush(ms->handle);
  }

  GEO_uv_parametrizer_stretch_end(ms->handle);
  GEO_uv_parametrizer_delete(ms->handle);

  for (uint ob_index = 0; ob_index < ms->objects_len; ob_index++) {
    Object *obedit = ms->objects_edit[ob_index];
    BMEditMesh *em = BKE_editmesh_from_object(obedit);

    if (synced_selection && (em->bm->totfacesel == 0)) {
      continue;
    }

    DEG_id_tag_update(obedit->data, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GEOM | ND_DATA, obedit->data);
  }

  MEM_freeN(ms->objects_edit);
  MEM_freeN(ms);
  op->customdata = NULL;
}

static int minimize_stretch_exec(bContext *C, wmOperator *op)
{
  int i, iterations;

  if (!minimize_stretch_init(C, op)) {
    return OPERATOR_CANCELLED;
  }

  iterations = RNA_int_get(op->ptr, "iterations");
  for (i = 0; i < iterations; i++) {
    minimize_stretch_iteration(C, op, false);
  }
  minimize_stretch_exit(C, op, false);

  return OPERATOR_FINISHED;
}

static int minimize_stretch_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
  MinStretch *ms;

  if (!minimize_stretch_init(C, op)) {
    return OPERATOR_CANCELLED;
  }

  minimize_stretch_iteration(C, op, true);

  ms = op->customdata;
  WM_event_add_modal_handler(C, op);
  ms->timer = WM_event_add_timer(CTX_wm_manager(C), CTX_wm_window(C), TIMER, 0.01f);

  return OPERATOR_RUNNING_MODAL;
}

static int minimize_stretch_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  MinStretch *ms = op->customdata;

  switch (event->type) {
    case EVT_ESCKEY:
    case RIGHTMOUSE:
      minimize_stretch_exit(C, op, true);
      return OPERATOR_CANCELLED;
    case EVT_RETKEY:
    case EVT_PADENTER:
    case LEFTMOUSE:
      minimize_stretch_exit(C, op, false);
      return OPERATOR_FINISHED;
    case EVT_PADPLUSKEY:
    case WHEELUPMOUSE:
      if (event->val == KM_PRESS) {
        if (ms->blend < 0.95f) {
          ms->blend += 0.1f;
          ms->lasttime = 0.0f;
          RNA_float_set(op->ptr, "blend", ms->blend);
          minimize_stretch_iteration(C, op, true);
        }
      }
      break;
    case EVT_PADMINUS:
    case WHEELDOWNMOUSE:
      if (event->val == KM_PRESS) {
        if (ms->blend > 0.05f) {
          ms->blend -= 0.1f;
          ms->lasttime = 0.0f;
          RNA_float_set(op->ptr, "blend", ms->blend);
          minimize_stretch_iteration(C, op, true);
        }
      }
      break;
    case TIMER:
      if (ms->timer == event->customdata) {
        double start = PIL_check_seconds_timer();

        do {
          minimize_stretch_iteration(C, op, true);
        } while (PIL_check_seconds_timer() - start < 0.01);
      }
      break;
  }

  if (ms->iterations && ms->i >= ms->iterations) {
    minimize_stretch_exit(C, op, false);
    return OPERATOR_FINISHED;
  }

  return OPERATOR_RUNNING_MODAL;
}

static void minimize_stretch_cancel(bContext *C, wmOperator *op)
{
  minimize_stretch_exit(C, op, true);
}

void UV_OT_minimize_stretch(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Minimize Stretch";
  ot->idname = "UV_OT_minimize_stretch";
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_GRAB_CURSOR_XY | OPTYPE_BLOCKING;
  ot->description = "Reduce UV stretching by relaxing angles";

  /* api callbacks */
  ot->exec = minimize_stretch_exec;
  ot->invoke = minimize_stretch_invoke;
  ot->modal = minimize_stretch_modal;
  ot->cancel = minimize_stretch_cancel;
  ot->poll = ED_operator_uvedit;

  /* properties */
  RNA_def_boolean(ot->srna,
                  "fill_holes",
                  1,
                  "Fill Holes",
                  "Virtual fill holes in mesh before unwrapping, to better avoid overlaps and "
                  "preserve symmetry");
  RNA_def_float_factor(ot->srna,
                       "blend",
                       0.0f,
                       0.0f,
                       1.0f,
                       "Blend",
                       "Blend factor between stretch minimized and original",
                       0.0f,
                       1.0f);
  RNA_def_int(ot->srna,
              "iterations",
              0,
              0,
              INT_MAX,
              "Iterations",
              "Number of iterations to run, 0 is unlimited when run interactively",
              0,
              100);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Pack UV Islands Operator
 * \{ */

static void uvedit_pack_islands(const Scene *scene, Object *ob, BMesh *bm)
{
  const UnwrapOptions options = {
      .topology_from_uvs = true,
      .only_selected_faces = false,
      .only_selected_uvs = true,
      .fill_holes = false,
      .correct_aspect = false,
  };

  bool rotate = true;
  bool ignore_pinned = false;

  ParamHandle *handle = construct_param_handle(scene, ob, bm, &options, NULL);
  GEO_uv_parametrizer_pack(handle, scene->toolsettings->uvcalc_margin, rotate, ignore_pinned);
  GEO_uv_parametrizer_flush(handle);
  GEO_uv_parametrizer_delete(handle);
}

/**
 * \warning Since this uses #ParamHandle it doesn't work with non-manifold meshes (see T82637).
 * Use #ED_uvedit_pack_islands_multi for a more general solution.
 *
 * TODO: remove this function, in favor of #ED_uvedit_pack_islands_multi.
 */
static void uvedit_pack_islands_multi(const Scene *scene,
                                      Object **objects,
                                      const uint objects_len,
                                      const UnwrapOptions *options,
                                      bool rotate,
                                      bool ignore_pinned)
{
  ParamHandle *handle = construct_param_handle_multi(scene, objects, objects_len, options, NULL);
  GEO_uv_parametrizer_pack(handle, scene->toolsettings->uvcalc_margin, rotate, ignore_pinned);
  GEO_uv_parametrizer_flush(handle);
  GEO_uv_parametrizer_delete(handle);

  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *obedit = objects[ob_index];
    DEG_id_tag_update(obedit->data, ID_RECALC_GEOMETRY);
    WM_main_add_notifier(NC_GEOM | ND_DATA, obedit->data);
  }
}

/* Packing targets. */
enum {
  PACK_UDIM_SRC_CLOSEST = 0,
  PACK_UDIM_SRC_ACTIVE = 1,
};

static int pack_islands_exec(bContext *C, wmOperator *op)
{
  ViewLayer *view_layer = CTX_data_view_layer(C);
  const Scene *scene = CTX_data_scene(C);
  const SpaceImage *sima = CTX_wm_space_image(C);

  const UnwrapOptions options = {
      .topology_from_uvs = true,
      .only_selected_faces = true,
      .only_selected_uvs = true,
      .fill_holes = false,
      .correct_aspect = true,
  };

  uint objects_len = 0;
  Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data_with_uvs(
      view_layer, CTX_wm_view3d(C), &objects_len);

  /* Early exit in case no UVs are selected. */
  if (!uvedit_have_selection_multi(scene, objects, objects_len, &options)) {
    MEM_freeN(objects);
    return OPERATOR_CANCELLED;
  }

  /* RNA props */
  const bool rotate = RNA_boolean_get(op->ptr, "rotate");
  const int udim_source = RNA_enum_get(op->ptr, "udim_source");
  if (RNA_struct_property_is_set(op->ptr, "margin")) {
    scene->toolsettings->uvcalc_margin = RNA_float_get(op->ptr, "margin");
  }
  else {
    RNA_float_set(op->ptr, "margin", scene->toolsettings->uvcalc_margin);
  }

  struct UVMapUDIM_Params udim_params;
  const bool use_active = (udim_source == PACK_UDIM_SRC_ACTIVE);
  const bool use_udim_params = ED_uvedit_udim_params_from_image_space(
      sima, use_active, &udim_params);

  ED_uvedit_pack_islands_multi(scene,
                               objects,
                               objects_len,
                               use_udim_params ? &udim_params : NULL,
                               &(struct UVPackIsland_Params){
                                   .rotate = rotate,
                                   .rotate_align_axis = -1,
                                   .only_selected_uvs = true,
                                   .only_selected_faces = true,
                                   .correct_aspect = true,
                               });

  MEM_freeN(objects);
  return OPERATOR_FINISHED;
}

void UV_OT_pack_islands(wmOperatorType *ot)
{
  static const EnumPropertyItem pack_target[] = {
      {PACK_UDIM_SRC_CLOSEST, "CLOSEST_UDIM", 0, "Closest UDIM", "Pack islands to closest UDIM"},
      {PACK_UDIM_SRC_ACTIVE,
       "ACTIVE_UDIM",
       0,
       "Active UDIM",
       "Pack islands to active UDIM image tile or UDIM grid tile where 2D cursor is located"},
      {0, NULL, 0, NULL, NULL},
  };
  /* identifiers */
  ot->name = "Pack Islands";
  ot->idname = "UV_OT_pack_islands";
  ot->description =
      "Transform all islands so that they fill up the UV/UDIM space as much as possible";

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* api callbacks */
  ot->exec = pack_islands_exec;
  ot->poll = ED_operator_uvedit;

  /* properties */
  RNA_def_enum(ot->srna, "udim_source", pack_target, PACK_UDIM_SRC_CLOSEST, "Pack to", "");
  RNA_def_boolean(ot->srna, "rotate", true, "Rotate", "Rotate islands for best fit");
  RNA_def_float_factor(
      ot->srna, "margin", 0.001f, 0.0f, 1.0f, "Margin", "Space between islands", 0.0f, 1.0f);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Average UV Islands Scale Operator
 * \{ */

static int average_islands_scale_exec(bContext *C, wmOperator *UNUSED(op))
{
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  ToolSettings *ts = scene->toolsettings;
  const bool synced_selection = (ts->uv_flag & UV_SYNC_SELECTION) != 0;

  const UnwrapOptions options = {
      .topology_from_uvs = true,
      .only_selected_faces = true,
      .only_selected_uvs = true,
      .fill_holes = false,
      .correct_aspect = true,
  };

  uint objects_len = 0;
  Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data_with_uvs(
      view_layer, CTX_wm_view3d(C), &objects_len);

  if (!uvedit_have_selection_multi(scene, objects, objects_len, &options)) {
    MEM_freeN(objects);
    return OPERATOR_CANCELLED;
  }

  ParamHandle *handle = construct_param_handle_multi(scene, objects, objects_len, &options, NULL);
  GEO_uv_parametrizer_average(handle, false);
  GEO_uv_parametrizer_flush(handle);
  GEO_uv_parametrizer_delete(handle);

  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *obedit = objects[ob_index];
    BMEditMesh *em = BKE_editmesh_from_object(obedit);

    if (synced_selection && (em->bm->totvertsel == 0)) {
      continue;
    }

    DEG_id_tag_update(obedit->data, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GEOM | ND_DATA, obedit->data);
  }
  MEM_freeN(objects);
  return OPERATOR_FINISHED;
}

void UV_OT_average_islands_scale(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Average Islands Scale";
  ot->idname = "UV_OT_average_islands_scale";
  ot->description = "Average the size of separate UV islands, based on their area in 3D space";

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* api callbacks */
  ot->exec = average_islands_scale_exec;
  ot->poll = ED_operator_uvedit;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Live UV Unwrap
 * \{ */

static struct {
  ParamHandle **handles;
  uint len, len_alloc;
} g_live_unwrap = {NULL};

void ED_uvedit_live_unwrap_begin(Scene *scene, Object *obedit)
{
  ParamHandle *handle = NULL;
  BMEditMesh *em = BKE_editmesh_from_object(obedit);
  const bool abf = (scene->toolsettings->unwrapper == 0);
  bool use_subsurf;

  modifier_unwrap_state(obedit, scene, &use_subsurf);

  if (!ED_uvedit_test(obedit)) {
    return;
  }

  const UnwrapOptions options = {
      .topology_from_uvs = false,
      .only_selected_faces = false,
      .only_selected_uvs = true,
      .fill_holes = (scene->toolsettings->uvcalc_flag & UVCALC_FILLHOLES) != 0,
      .correct_aspect = (scene->toolsettings->uvcalc_flag & UVCALC_NO_ASPECT_CORRECT) == 0,
  };

  if (use_subsurf) {
    handle = construct_param_handle_subsurfed(scene, obedit, em, &options, NULL);
  }
  else {
    handle = construct_param_handle(scene, obedit, em->bm, &options, NULL);
  }

  GEO_uv_parametrizer_lscm_begin(handle, true, abf);

  /* Create or increase size of g_live_unwrap.handles array */
  if (g_live_unwrap.handles == NULL) {
    g_live_unwrap.len_alloc = 32;
    g_live_unwrap.handles = MEM_mallocN(sizeof(ParamHandle *) * g_live_unwrap.len_alloc,
                                        "uvedit_live_unwrap_liveHandles");
    g_live_unwrap.len = 0;
  }
  if (g_live_unwrap.len >= g_live_unwrap.len_alloc) {
    g_live_unwrap.len_alloc *= 2;
    g_live_unwrap.handles = MEM_reallocN(g_live_unwrap.handles,
                                         sizeof(ParamHandle *) * g_live_unwrap.len_alloc);
  }
  g_live_unwrap.handles[g_live_unwrap.len] = handle;
  g_live_unwrap.len++;
}

void ED_uvedit_live_unwrap_re_solve(void)
{
  if (g_live_unwrap.handles) {
    for (int i = 0; i < g_live_unwrap.len; i++) {
      GEO_uv_parametrizer_lscm_solve(g_live_unwrap.handles[i], NULL, NULL);
      GEO_uv_parametrizer_flush(g_live_unwrap.handles[i]);
    }
  }
}

void ED_uvedit_live_unwrap_end(short cancel)
{
  if (g_live_unwrap.handles) {
    for (int i = 0; i < g_live_unwrap.len; i++) {
      GEO_uv_parametrizer_lscm_end(g_live_unwrap.handles[i]);
      if (cancel) {
        GEO_uv_parametrizer_flush_restore(g_live_unwrap.handles[i]);
      }
      GEO_uv_parametrizer_delete(g_live_unwrap.handles[i]);
    }
    MEM_freeN(g_live_unwrap.handles);
    g_live_unwrap.handles = NULL;
    g_live_unwrap.len = 0;
    g_live_unwrap.len_alloc = 0;
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name UV Map Common Transforms
 * \{ */

#define VIEW_ON_EQUATOR 0
#define VIEW_ON_POLES 1
#define ALIGN_TO_OBJECT 2

#define POLAR_ZX 0
#define POLAR_ZY 1

static void uv_map_transform_calc_bounds(BMEditMesh *em, float r_min[3], float r_max[3])
{
  BMFace *efa;
  BMIter iter;
  INIT_MINMAX(r_min, r_max);
  BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
    if (BM_elem_flag_test(efa, BM_ELEM_SELECT)) {
      BM_face_calc_bounds_expand(efa, r_min, r_max);
    }
  }
}

static void uv_map_transform_calc_center_median(BMEditMesh *em, float r_center[3])
{
  BMFace *efa;
  BMIter iter;
  uint center_accum_num = 0;
  zero_v3(r_center);
  BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
    if (BM_elem_flag_test(efa, BM_ELEM_SELECT)) {
      float center[3];
      BM_face_calc_center_median(efa, center);
      add_v3_v3(r_center, center);
      center_accum_num += 1;
    }
  }
  mul_v3_fl(r_center, 1.0f / (float)center_accum_num);
}

static void uv_map_transform_center(const Scene *scene,
                                    View3D *v3d,
                                    Object *ob,
                                    BMEditMesh *em,
                                    float r_center[3],
                                    float r_bounds[2][3])
{
  /* only operates on the edit object - this is all that's needed now */
  const int around = (v3d) ? scene->toolsettings->transform_pivot_point : V3D_AROUND_CENTER_BOUNDS;

  float bounds[2][3];
  INIT_MINMAX(bounds[0], bounds[1]);
  bool is_minmax_set = false;

  switch (around) {
    case V3D_AROUND_CENTER_BOUNDS: /* bounding box center */
    {
      uv_map_transform_calc_bounds(em, bounds[0], bounds[1]);
      is_minmax_set = true;
      mid_v3_v3v3(r_center, bounds[0], bounds[1]);
      break;
    }
    case V3D_AROUND_CENTER_MEDIAN: {
      uv_map_transform_calc_center_median(em, r_center);
      break;
    }
    case V3D_AROUND_CURSOR: /* cursor center */
    {
      invert_m4_m4(ob->imat, ob->obmat);
      mul_v3_m4v3(r_center, ob->imat, scene->cursor.location);
      break;
    }
    case V3D_AROUND_ACTIVE: {
      BMEditSelection ese;
      if (BM_select_history_active_get(em->bm, &ese)) {
        BM_editselection_center(&ese, r_center);
        break;
      }
      ATTR_FALLTHROUGH;
    }
    case V3D_AROUND_LOCAL_ORIGINS: /* object center */
    default:
      zero_v3(r_center);
      break;
  }

  /* if this is passed, always set! */
  if (r_bounds) {
    if (!is_minmax_set) {
      uv_map_transform_calc_bounds(em, bounds[0], bounds[1]);
    }
    copy_v3_v3(r_bounds[0], bounds[0]);
    copy_v3_v3(r_bounds[1], bounds[1]);
  }
}

static void uv_map_rotation_matrix_ex(float result[4][4],
                                      RegionView3D *rv3d,
                                      Object *ob,
                                      float upangledeg,
                                      float sideangledeg,
                                      float radius,
                                      const float offset[4])
{
  float rotup[4][4], rotside[4][4], viewmatrix[4][4], rotobj[4][4];
  float sideangle = 0.0f, upangle = 0.0f;

  /* get rotation of the current view matrix */
  if (rv3d) {
    copy_m4_m4(viewmatrix, rv3d->viewmat);
  }
  else {
    unit_m4(viewmatrix);
  }

  /* but shifting */
  zero_v3(viewmatrix[3]);

  /* get rotation of the current object matrix */
  copy_m4_m4(rotobj, ob->obmat);
  zero_v3(rotobj[3]);

  /* but shifting */
  add_v4_v4(rotobj[3], offset);
  rotobj[3][3] = 0.0f;

  zero_m4(rotup);
  zero_m4(rotside);

  /* Compensate front/side.. against opengl x,y,z world definition.
   * This is "a sledgehammer to crack a nut" (overkill), a few plus minus 1 will do here.
   * I wanted to keep the reason here, so we're rotating. */
  sideangle = (float)M_PI * (sideangledeg + 180.0f) / 180.0f;
  rotside[0][0] = cosf(sideangle);
  rotside[0][1] = -sinf(sideangle);
  rotside[1][0] = sinf(sideangle);
  rotside[1][1] = cosf(sideangle);
  rotside[2][2] = 1.0f;

  upangle = (float)M_PI * upangledeg / 180.0f;
  rotup[1][1] = cosf(upangle) / radius;
  rotup[1][2] = -sinf(upangle) / radius;
  rotup[2][1] = sinf(upangle) / radius;
  rotup[2][2] = cosf(upangle) / radius;
  rotup[0][0] = 1.0f / radius;

  /* Calculate transforms. */
  mul_m4_series(result, rotup, rotside, viewmatrix, rotobj);
}

static void uv_map_rotation_matrix(float result[4][4],
                                   RegionView3D *rv3d,
                                   Object *ob,
                                   float upangledeg,
                                   float sideangledeg,
                                   float radius)
{
  const float offset[4] = {0};
  uv_map_rotation_matrix_ex(result, rv3d, ob, upangledeg, sideangledeg, radius, offset);
}

static void uv_map_transform(bContext *C, wmOperator *op, float rotmat[4][4])
{
  /* context checks are messy here, making it work in both 3d view and uv editor */
  Object *obedit = CTX_data_edit_object(C);
  RegionView3D *rv3d = CTX_wm_region_view3d(C);
  /* common operator properties */
  int align = RNA_enum_get(op->ptr, "align");
  int direction = RNA_enum_get(op->ptr, "direction");
  float radius = RNA_struct_find_property(op->ptr, "radius") ? RNA_float_get(op->ptr, "radius") :
                                                               1.0f;
  float upangledeg, sideangledeg;

  if (direction == VIEW_ON_EQUATOR) {
    upangledeg = 90.0f;
    sideangledeg = 0.0f;
  }
  else {
    upangledeg = 0.0f;
    if (align == POLAR_ZY) {
      sideangledeg = 0.0f;
    }
    else {
      sideangledeg = 90.0f;
    }
  }

  /* be compatible to the "old" sphere/cylinder mode */
  if (direction == ALIGN_TO_OBJECT) {
    unit_m4(rotmat);
  }
  else {
    uv_map_rotation_matrix(rotmat, rv3d, obedit, upangledeg, sideangledeg, radius);
  }
}

static void uv_transform_properties(wmOperatorType *ot, int radius)
{
  static const EnumPropertyItem direction_items[] = {
      {VIEW_ON_EQUATOR, "VIEW_ON_EQUATOR", 0, "View on Equator", "3D view is on the equator"},
      {VIEW_ON_POLES, "VIEW_ON_POLES", 0, "View on Poles", "3D view is on the poles"},
      {ALIGN_TO_OBJECT,
       "ALIGN_TO_OBJECT",
       0,
       "Align to Object",
       "Align according to object transform"},
      {0, NULL, 0, NULL, NULL},
  };
  static const EnumPropertyItem align_items[] = {
      {POLAR_ZX, "POLAR_ZX", 0, "Polar ZX", "Polar 0 is X"},
      {POLAR_ZY, "POLAR_ZY", 0, "Polar ZY", "Polar 0 is Y"},
      {0, NULL, 0, NULL, NULL},
  };

  RNA_def_enum(ot->srna,
               "direction",
               direction_items,
               VIEW_ON_EQUATOR,
               "Direction",
               "Direction of the sphere or cylinder");
  RNA_def_enum(ot->srna,
               "align",
               align_items,
               VIEW_ON_EQUATOR,
               "Align",
               "How to determine rotation around the pole");
  if (radius) {
    RNA_def_float(ot->srna,
                  "radius",
                  1.0f,
                  0.0f,
                  FLT_MAX,
                  "Radius",
                  "Radius of the sphere or cylinder",
                  0.0001f,
                  100.0f);
  }
}

static void shrink_loop_uv_by_aspect_ratio(BMFace *efa,
                                           const int cd_loop_uv_offset,
                                           const float aspect_y)
{
  BLI_assert(aspect_y != 1.0f); /* Nothing to do, should be handled by caller. */
  BLI_assert(aspect_y > 0.0f);  /* Negative aspect ratios are not supported. */

  BMLoop *l;
  BMIter iter;
  BM_ITER_ELEM (l, &iter, efa, BM_LOOPS_OF_FACE) {
    MLoopUV *luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
    if (aspect_y > 1.0f) {
      /* Reduce round-off error, i.e. `u = (u - 0.5) / aspect_y + 0.5`. */
      luv->uv[0] = luv->uv[0] / aspect_y + (0.5f - 0.5f / aspect_y);
    }
    else {
      /* Reduce round-off error, i.e. `v = (v - 0.5) * aspect_y + 0.5`. */
      luv->uv[1] = luv->uv[1] * aspect_y + (0.5f - 0.5f * aspect_y);
    }
  }
}

static void correct_uv_aspect(Object *ob, BMEditMesh *em)
{
  const int cd_loop_uv_offset = CustomData_get_offset(&em->bm->ldata, CD_MLOOPUV);
  float aspx, aspy;
  ED_uvedit_get_aspect(ob, &aspx, &aspy);
  const float aspect_y = aspx / aspy;
  if (aspect_y == 1.0f) {
    /* Scaling by 1.0 has no effect. */
    return;
  }
  BMFace *efa;
  BMIter iter;
  BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
    if (BM_elem_flag_test(efa, BM_ELEM_SELECT)) {
      shrink_loop_uv_by_aspect_ratio(efa, cd_loop_uv_offset, aspect_y);
    }
  }
}

static void correct_uv_aspect_per_face(Object *ob, BMEditMesh *em)
{
  const int materials_num = ob->totcol;
  if (materials_num == 0) {
    /* Without any materials, there is no aspect_y information and nothing to do. */
    return;
  }

  float *material_aspect_y = BLI_array_alloca(material_aspect_y, materials_num);
  /* Lazily initialize aspect ratio for materials. */
  copy_vn_fl(material_aspect_y, materials_num, -1.0f);

  const int cd_loop_uv_offset = CustomData_get_offset(&em->bm->ldata, CD_MLOOPUV);

  BMFace *efa;
  BMIter iter;
  BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
    if (!BM_elem_flag_test(efa, BM_ELEM_SELECT)) {
      continue;
    }

    const int material_index = efa->mat_nr;
    if (UNLIKELY(material_index < 0 || material_index >= materials_num)) {
      /* The index might be for a material slot which is not currently setup. */
      continue;
    }

    float aspect_y = material_aspect_y[material_index];
    if (aspect_y == -1.0f) {
      /* Lazily initialize aspect ratio for materials. */
      float aspx, aspy;
      ED_uvedit_get_aspect_from_material(ob, material_index, &aspx, &aspy);
      aspect_y = aspx / aspy;
      material_aspect_y[material_index] = aspect_y;
    }

    if (aspect_y == 1.0f) {
      /* Scaling by 1.0 has no effect. */
      continue;
    }
    shrink_loop_uv_by_aspect_ratio(efa, cd_loop_uv_offset, aspect_y);
  }
}

#undef VIEW_ON_EQUATOR
#undef VIEW_ON_POLES
#undef ALIGN_TO_OBJECT

#undef POLAR_ZX
#undef POLAR_ZY

/** \} */

/* -------------------------------------------------------------------- */
/** \name UV Map Clip & Correct
 * \{ */

static void uv_map_clip_correct_properties_ex(wmOperatorType *ot, bool clip_to_bounds)
{
  RNA_def_boolean(ot->srna,
                  "correct_aspect",
                  1,
                  "Correct Aspect",
                  "Map UVs taking image aspect ratio into account");
  /* Optional, since not all unwrapping types need to be clipped. */
  if (clip_to_bounds) {
    RNA_def_boolean(ot->srna,
                    "clip_to_bounds",
                    0,
                    "Clip to Bounds",
                    "Clip UV coordinates to bounds after unwrapping");
  }
  RNA_def_boolean(ot->srna,
                  "scale_to_bounds",
                  0,
                  "Scale to Bounds",
                  "Scale UV coordinates to bounds after unwrapping");
}

static void uv_map_clip_correct_properties(wmOperatorType *ot)
{
  uv_map_clip_correct_properties_ex(ot, true);
}

/**
 * \param per_face_aspect: Calculate the aspect ratio per-face,
 * otherwise use a single aspect for all UV's based on the material of the active face.
 * TODO: using per-face aspect may split UV islands so more advanced UV projection methods
 * such as "Unwrap" & "Smart UV Projections" will need to handle aspect correction themselves.
 * For now keep using a single aspect for all faces in this case.
 */
static void uv_map_clip_correct_multi(Object **objects,
                                      uint objects_len,
                                      wmOperator *op,
                                      bool per_face_aspect)
{
  BMFace *efa;
  BMLoop *l;
  BMIter iter, liter;
  MLoopUV *luv;
  float dx, dy, min[2], max[2];
  const bool correct_aspect = RNA_boolean_get(op->ptr, "correct_aspect");
  const bool clip_to_bounds = (RNA_struct_find_property(op->ptr, "clip_to_bounds") &&
                               RNA_boolean_get(op->ptr, "clip_to_bounds"));
  const bool scale_to_bounds = RNA_boolean_get(op->ptr, "scale_to_bounds");

  INIT_MINMAX2(min, max);

  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *ob = objects[ob_index];

    BMEditMesh *em = BKE_editmesh_from_object(ob);
    const int cd_loop_uv_offset = CustomData_get_offset(&em->bm->ldata, CD_MLOOPUV);

    /* Correct for image aspect ratio. */
    if (correct_aspect) {
      if (per_face_aspect) {
        correct_uv_aspect_per_face(ob, em);
      }
      else {
        correct_uv_aspect(ob, em);
      }
    }

    if (scale_to_bounds) {
      /* find uv limits */
      BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
        if (!BM_elem_flag_test(efa, BM_ELEM_SELECT)) {
          continue;
        }

        BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
          luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
          minmax_v2v2_v2(min, max, luv->uv);
        }
      }
    }
    else if (clip_to_bounds) {
      /* clipping and wrapping */
      BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
        if (!BM_elem_flag_test(efa, BM_ELEM_SELECT)) {
          continue;
        }

        BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
          luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
          clamp_v2(luv->uv, 0.0f, 1.0f);
        }
      }
    }
  }

  if (scale_to_bounds) {
    /* rescale UV to be in 1/1 */
    dx = (max[0] - min[0]);
    dy = (max[1] - min[1]);

    if (dx > 0.0f) {
      dx = 1.0f / dx;
    }
    if (dy > 0.0f) {
      dy = 1.0f / dy;
    }

    if (dx == 1.0f && dy == 1.0f) {
      /* Scaling by 1.0 has no effect. */
      return;
    }

    for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
      Object *ob = objects[ob_index];

      BMEditMesh *em = BKE_editmesh_from_object(ob);
      const int cd_loop_uv_offset = CustomData_get_offset(&em->bm->ldata, CD_MLOOPUV);

      BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
        if (!BM_elem_flag_test(efa, BM_ELEM_SELECT)) {
          continue;
        }

        BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
          luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);

          luv->uv[0] = (luv->uv[0] - min[0]) * dx;
          luv->uv[1] = (luv->uv[1] - min[1]) * dy;
        }
      }
    }
  }
}

static void uv_map_clip_correct(Object *ob, wmOperator *op)
{
  uv_map_clip_correct_multi(&ob, 1, op, true);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name UV Unwrap Operator
 * \{ */

/* Assumes UV Map exists, doesn't run update funcs. */
static void uvedit_unwrap(const Scene *scene,
                          Object *obedit,
                          const UnwrapOptions *options,
                          UnwrapResultInfo *result_info)
{
  BMEditMesh *em = BKE_editmesh_from_object(obedit);
  if (!CustomData_has_layer(&em->bm->ldata, CD_MLOOPUV)) {
    return;
  }

  bool use_subsurf;
  modifier_unwrap_state(obedit, scene, &use_subsurf);

  ParamHandle *handle;
  if (use_subsurf) {
    handle = construct_param_handle_subsurfed(scene, obedit, em, options, result_info);
  }
  else {
    handle = construct_param_handle(scene, obedit, em->bm, options, result_info);
  }

  GEO_uv_parametrizer_lscm_begin(handle, false, scene->toolsettings->unwrapper == 0);
  GEO_uv_parametrizer_lscm_solve(handle,
                                 result_info ? &result_info->count_changed : NULL,
                                 result_info ? &result_info->count_failed : NULL);
  GEO_uv_parametrizer_lscm_end(handle);

  GEO_uv_parametrizer_average(handle, true);

  GEO_uv_parametrizer_flush(handle);

  GEO_uv_parametrizer_delete(handle);
}

static void uvedit_unwrap_multi(const Scene *scene,
                                Object **objects,
                                const int objects_len,
                                const UnwrapOptions *options,
                                UnwrapResultInfo *result_info)
{
  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *obedit = objects[ob_index];
    uvedit_unwrap(scene, obedit, options, result_info);
    DEG_id_tag_update(obedit->data, ID_RECALC_GEOMETRY);
    WM_main_add_notifier(NC_GEOM | ND_DATA, obedit->data);
  }
}

void ED_uvedit_live_unwrap(const Scene *scene, Object **objects, int objects_len)
{
  if (scene->toolsettings->edge_mode_live_unwrap) {
    const UnwrapOptions options = {
        .topology_from_uvs = false,
        .only_selected_faces = false,
        .only_selected_uvs = true,
        .fill_holes = (scene->toolsettings->uvcalc_flag & UVCALC_FILLHOLES) != 0,
        .correct_aspect = (scene->toolsettings->uvcalc_flag & UVCALC_NO_ASPECT_CORRECT) == 0,
    };

    bool rotate = true;
    bool ignore_pinned = true;

    uvedit_unwrap_multi(scene, objects, objects_len, &options, NULL);
    uvedit_pack_islands_multi(scene, objects, objects_len, &options, rotate, ignore_pinned);
  }
}

enum {
  UNWRAP_ERROR_NONUNIFORM = (1 << 0),
  UNWRAP_ERROR_NEGATIVE = (1 << 1),
};

static int unwrap_exec(bContext *C, wmOperator *op)
{
  ViewLayer *view_layer = CTX_data_view_layer(C);
  const Scene *scene = CTX_data_scene(C);
  int method = RNA_enum_get(op->ptr, "method");
  const bool use_subsurf = RNA_boolean_get(op->ptr, "use_subsurf_data");
  int reported_errors = 0;
  /* We will report an error unless at least one object
   * has the subsurf modifier in the right place. */
  bool subsurf_error = use_subsurf;

  uint objects_len = 0;
  Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      view_layer, CTX_wm_view3d(C), &objects_len);

  const UnwrapOptions options = {
      .topology_from_uvs = false,
      .only_selected_faces = true,
      .only_selected_uvs = true,
      .fill_holes = RNA_boolean_get(op->ptr, "fill_holes"),
      .correct_aspect = RNA_boolean_get(op->ptr, "correct_aspect"),
  };

  bool rotate = true;
  bool ignore_pinned = true;

  if (!uvedit_have_selection_multi(scene, objects, objects_len, &options)) {
    MEM_freeN(objects);
    return OPERATOR_CANCELLED;
  }

  /* add uvs if they don't exist yet */
  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *obedit = objects[ob_index];
    float obsize[3];
    bool use_subsurf_final;

    if (!ED_uvedit_ensure_uvs(obedit)) {
      continue;
    }

    if (subsurf_error) {
      /* Double up the check here but better keep uvedit_unwrap interface simple and not
       * pass operator for warning append. */
      modifier_unwrap_state(obedit, scene, &use_subsurf_final);
      if (use_subsurf_final) {
        subsurf_error = false;
      }
    }

    if (reported_errors & (UNWRAP_ERROR_NONUNIFORM | UNWRAP_ERROR_NEGATIVE)) {
      continue;
    }

    mat4_to_size(obsize, obedit->obmat);
    if (!(fabsf(obsize[0] - obsize[1]) < 1e-4f && fabsf(obsize[1] - obsize[2]) < 1e-4f)) {
      if ((reported_errors & UNWRAP_ERROR_NONUNIFORM) == 0) {
        BKE_report(op->reports,
                   RPT_INFO,
                   "Object has non-uniform scale, unwrap will operate on a non-scaled version of "
                   "the mesh");
        reported_errors |= UNWRAP_ERROR_NONUNIFORM;
      }
    }
    else if (is_negative_m4(obedit->obmat)) {
      if ((reported_errors & UNWRAP_ERROR_NEGATIVE) == 0) {
        BKE_report(
            op->reports,
            RPT_INFO,
            "Object has negative scale, unwrap will operate on a non-flipped version of the mesh");
        reported_errors |= UNWRAP_ERROR_NEGATIVE;
      }
    }
  }

  if (subsurf_error) {
    BKE_report(op->reports,
               RPT_INFO,
               "Subdivision Surface modifier needs to be first to work with unwrap");
  }

  /* remember last method for live unwrap */
  if (RNA_struct_property_is_set(op->ptr, "method")) {
    scene->toolsettings->unwrapper = method;
  }
  else {
    RNA_enum_set(op->ptr, "method", scene->toolsettings->unwrapper);
  }

  /* remember packing margin */
  if (RNA_struct_property_is_set(op->ptr, "margin")) {
    scene->toolsettings->uvcalc_margin = RNA_float_get(op->ptr, "margin");
  }
  else {
    RNA_float_set(op->ptr, "margin", scene->toolsettings->uvcalc_margin);
  }

  if (options.fill_holes) {
    scene->toolsettings->uvcalc_flag |= UVCALC_FILLHOLES;
  }
  else {
    scene->toolsettings->uvcalc_flag &= ~UVCALC_FILLHOLES;
  }

  if (options.correct_aspect) {
    scene->toolsettings->uvcalc_flag &= ~UVCALC_NO_ASPECT_CORRECT;
  }
  else {
    scene->toolsettings->uvcalc_flag |= UVCALC_NO_ASPECT_CORRECT;
  }

  if (use_subsurf) {
    scene->toolsettings->uvcalc_flag |= UVCALC_USESUBSURF;
  }
  else {
    scene->toolsettings->uvcalc_flag &= ~UVCALC_USESUBSURF;
  }

  /* execute unwrap */
  UnwrapResultInfo result_info = {
      .count_changed = 0,
      .count_failed = 0,
  };
  uvedit_unwrap_multi(scene, objects, objects_len, &options, &result_info);
  uvedit_pack_islands_multi(scene, objects, objects_len, &options, rotate, ignore_pinned);

  MEM_freeN(objects);

  if (result_info.count_failed == 0 && result_info.count_changed == 0) {
    BKE_report(op->reports,
               RPT_WARNING,
               "Unwrap could not solve any island(s), edge seams may need to be added");
  }
  else if (result_info.count_failed) {
    BKE_reportf(op->reports,
                RPT_WARNING,
                "Unwrap failed to solve %d of %d island(s), edge seams may need to be added",
                result_info.count_failed,
                result_info.count_changed + result_info.count_failed);
  }

  return OPERATOR_FINISHED;
}

void UV_OT_unwrap(wmOperatorType *ot)
{
  static const EnumPropertyItem method_items[] = {
      {0, "ANGLE_BASED", 0, "Angle Based", ""},
      {1, "CONFORMAL", 0, "Conformal", ""},
      {0, NULL, 0, NULL, NULL},
  };

  /* identifiers */
  ot->name = "Unwrap";
  ot->description = "Unwrap the mesh of the object being edited";
  ot->idname = "UV_OT_unwrap";
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* api callbacks */
  ot->exec = unwrap_exec;
  ot->poll = ED_operator_uvmap;

  /* properties */
  RNA_def_enum(ot->srna,
               "method",
               method_items,
               0,
               "Method",
               "Unwrapping method (Angle Based usually gives better results than Conformal, while "
               "being somewhat slower)");
  RNA_def_boolean(ot->srna,
                  "fill_holes",
                  1,
                  "Fill Holes",
                  "Virtual fill holes in mesh before unwrapping, to better avoid overlaps and "
                  "preserve symmetry");
  RNA_def_boolean(ot->srna,
                  "correct_aspect",
                  1,
                  "Correct Aspect",
                  "Map UVs taking image aspect ratio into account");
  RNA_def_boolean(
      ot->srna,
      "use_subsurf_data",
      0,
      "Use Subdivision Surface",
      "Map UVs taking vertex position after Subdivision Surface modifier has been applied");
  RNA_def_float_factor(
      ot->srna, "margin", 0.001f, 0.0f, 1.0f, "Margin", "Space between islands", 0.0f, 1.0f);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Smart UV Project Operator
 * \{ */

/* Ignore all areas below this, as the UV's get zeroed. */
static const float smart_uv_project_area_ignore = 1e-12f;

typedef struct ThickFace {
  float area;
  BMFace *efa;
} ThickFace;

static int smart_uv_project_thickface_area_cmp_fn(const void *tf_a_p, const void *tf_b_p)
{

  const ThickFace *tf_a = (ThickFace *)tf_a_p;
  const ThickFace *tf_b = (ThickFace *)tf_b_p;

  /* Ignore the area of small faces.
   * Also, order checks so `!isfinite(...)` values are counted as zero area. */
  if (!((tf_a->area > smart_uv_project_area_ignore) ||
        (tf_b->area > smart_uv_project_area_ignore))) {
    return 0;
  }

  if (tf_a->area < tf_b->area) {
    return 1;
  }
  if (tf_a->area > tf_b->area) {
    return -1;
  }
  return 0;
}

static uint smart_uv_project_calculate_project_normals(const ThickFace *thick_faces,
                                                       const uint thick_faces_len,
                                                       BMesh *bm,
                                                       const float project_angle_limit_half_cos,
                                                       const float project_angle_limit_cos,
                                                       const float area_weight,
                                                       float (**r_project_normal_array)[3])
{
  if (UNLIKELY(thick_faces_len == 0)) {
    *r_project_normal_array = NULL;
    return 0;
  }

  const float *project_normal = thick_faces[0].efa->no;

  const ThickFace **project_thick_faces = NULL;
  BLI_array_declare(project_thick_faces);

  float(*project_normal_array)[3] = NULL;
  BLI_array_declare(project_normal_array);

  BM_mesh_elem_hflag_disable_all(bm, BM_FACE, BM_ELEM_TAG, false);

  while (true) {
    for (int f_index = thick_faces_len - 1; f_index >= 0; f_index--) {
      if (BM_elem_flag_test(thick_faces[f_index].efa, BM_ELEM_TAG)) {
        continue;
      }

      if (dot_v3v3(thick_faces[f_index].efa->no, project_normal) > project_angle_limit_half_cos) {
        BLI_array_append(project_thick_faces, &thick_faces[f_index]);
        BM_elem_flag_set(thick_faces[f_index].efa, BM_ELEM_TAG, true);
      }
    }

    float average_normal[3] = {0.0f, 0.0f, 0.0f};

    if (area_weight <= 0.0f) {
      for (int f_proj_index = 0; f_proj_index < BLI_array_len(project_thick_faces);
           f_proj_index++) {
        const ThickFace *tf = project_thick_faces[f_proj_index];
        add_v3_v3(average_normal, tf->efa->no);
      }
    }
    else if (area_weight >= 1.0f) {
      for (int f_proj_index = 0; f_proj_index < BLI_array_len(project_thick_faces);
           f_proj_index++) {
        const ThickFace *tf = project_thick_faces[f_proj_index];
        madd_v3_v3fl(average_normal, tf->efa->no, tf->area);
      }
    }
    else {
      for (int f_proj_index = 0; f_proj_index < BLI_array_len(project_thick_faces);
           f_proj_index++) {
        const ThickFace *tf = project_thick_faces[f_proj_index];
        const float area_blend = (tf->area * area_weight) + (1.0f - area_weight);
        madd_v3_v3fl(average_normal, tf->efa->no, area_blend);
      }
    }

    /* Avoid NAN. */
    if (normalize_v3(average_normal) != 0.0f) {
      float(*normal)[3] = BLI_array_append_ret(project_normal_array);
      copy_v3_v3(*normal, average_normal);
    }

    /* Find the most unique angle that points away from other normals. */
    float anble_best = 1.0f;
    uint angle_best_index = 0;

    for (int f_index = thick_faces_len - 1; f_index >= 0; f_index--) {
      if (BM_elem_flag_test(thick_faces[f_index].efa, BM_ELEM_TAG)) {
        continue;
      }

      float angle_test = -1.0f;
      for (int p_index = 0; p_index < BLI_array_len(project_normal_array); p_index++) {
        angle_test = max_ff(angle_test,
                            dot_v3v3(project_normal_array[p_index], thick_faces[f_index].efa->no));
      }

      if (angle_test < anble_best) {
        anble_best = angle_test;
        angle_best_index = f_index;
      }
    }

    if (anble_best < project_angle_limit_cos) {
      project_normal = thick_faces[angle_best_index].efa->no;
      BLI_array_clear(project_thick_faces);
      BLI_array_append(project_thick_faces, &thick_faces[angle_best_index]);
      BM_elem_flag_enable(thick_faces[angle_best_index].efa, BM_ELEM_TAG);
    }
    else {
      if (BLI_array_len(project_normal_array) >= 1) {
        break;
      }
    }
  }

  BLI_array_free(project_thick_faces);
  BM_mesh_elem_hflag_disable_all(bm, BM_FACE, BM_ELEM_TAG, false);

  *r_project_normal_array = project_normal_array;
  return BLI_array_len(project_normal_array);
}

static int smart_project_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);

  /* May be NULL. */
  View3D *v3d = CTX_wm_view3d(C);

  const float project_angle_limit = RNA_float_get(op->ptr, "angle_limit");
  const float island_margin = RNA_float_get(op->ptr, "island_margin");
  const float area_weight = RNA_float_get(op->ptr, "area_weight");

  const float project_angle_limit_cos = cosf(project_angle_limit);
  const float project_angle_limit_half_cos = cosf(project_angle_limit / 2);

  /* Memory arena for list links (cleared for each object). */
  MemArena *arena = BLI_memarena_new(BLI_MEMARENA_STD_BUFSIZE, __func__);

  uint objects_len = 0;
  Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      view_layer, v3d, &objects_len);

  Object **objects_changed = MEM_mallocN(sizeof(*objects_changed) * objects_len, __func__);
  uint object_changed_len = 0;

  BMFace *efa;
  BMIter iter;
  uint ob_index;

  for (ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *obedit = objects[ob_index];
    BMEditMesh *em = BKE_editmesh_from_object(obedit);
    bool changed = false;

    if (!ED_uvedit_ensure_uvs(obedit)) {
      continue;
    }

    const uint cd_loop_uv_offset = CustomData_get_offset(&em->bm->ldata, CD_MLOOPUV);
    ThickFace *thick_faces = MEM_mallocN(sizeof(*thick_faces) * em->bm->totface, __func__);

    uint thick_faces_len = 0;
    BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
      if (!BM_elem_flag_test(efa, BM_ELEM_SELECT)) {
        continue;
      }
      thick_faces[thick_faces_len].area = BM_face_calc_area(efa);
      thick_faces[thick_faces_len].efa = efa;
      thick_faces_len++;
    }

    qsort(thick_faces, thick_faces_len, sizeof(ThickFace), smart_uv_project_thickface_area_cmp_fn);

    /* Remove all zero area faces. */
    while ((thick_faces_len > 0) &&
           !(thick_faces[thick_faces_len - 1].area > smart_uv_project_area_ignore)) {

      /* Zero UV's so they don't overlap with other faces being unwrapped. */
      BMIter liter;
      BMLoop *l;
      BM_ITER_ELEM (l, &liter, thick_faces[thick_faces_len - 1].efa, BM_LOOPS_OF_FACE) {
        MLoopUV *luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
        zero_v2(luv->uv);
        changed = true;
      }

      thick_faces_len -= 1;
    }

    float(*project_normal_array)[3] = NULL;
    int project_normals_len = smart_uv_project_calculate_project_normals(
        thick_faces,
        thick_faces_len,
        em->bm,
        project_angle_limit_half_cos,
        project_angle_limit_cos,
        area_weight,
        &project_normal_array);

    if (project_normals_len == 0) {
      MEM_freeN(thick_faces);
      BLI_assert(project_normal_array == NULL);
      continue;
    }

    /* After finding projection vectors, we find the uv positions. */
    LinkNode **thickface_project_groups = MEM_callocN(
        sizeof(*thickface_project_groups) * project_normals_len, __func__);

    BLI_memarena_clear(arena);

    for (int f_index = thick_faces_len - 1; f_index >= 0; f_index--) {
      const float *f_normal = thick_faces[f_index].efa->no;

      float angle_best = dot_v3v3(f_normal, project_normal_array[0]);
      uint angle_best_index = 0;

      for (int p_index = 1; p_index < project_normals_len; p_index++) {
        const float angle_test = dot_v3v3(f_normal, project_normal_array[p_index]);
        if (angle_test > angle_best) {
          angle_best = angle_test;
          angle_best_index = p_index;
        }
      }

      BLI_linklist_prepend_arena(
          &thickface_project_groups[angle_best_index], &thick_faces[f_index], arena);
    }

    for (int p_index = 0; p_index < project_normals_len; p_index++) {
      if (thickface_project_groups[p_index] == NULL) {
        continue;
      }

      float axis_mat[3][3];
      axis_dominant_v3_to_m3(axis_mat, project_normal_array[p_index]);

      for (LinkNode *list = thickface_project_groups[p_index]; list; list = list->next) {
        ThickFace *tf = list->link;
        BMIter liter;
        BMLoop *l;
        BM_ITER_ELEM (l, &liter, tf->efa, BM_LOOPS_OF_FACE) {
          MLoopUV *luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
          mul_v2_m3v3(luv->uv, axis_mat, l->v->co);
        }
        changed = true;
      }
    }

    MEM_freeN(thick_faces);
    MEM_freeN(project_normal_array);

    /* No need to free the lists in 'thickface_project_groups' values as the 'arena' is used. */
    MEM_freeN(thickface_project_groups);

    if (changed) {
      objects_changed[object_changed_len] = objects[ob_index];
      object_changed_len += 1;
    }
  }

  BLI_memarena_free(arena);

  MEM_freeN(objects);

  /* Pack islands & Stretch to UV bounds */
  if (object_changed_len > 0) {

    scene->toolsettings->uvcalc_margin = island_margin;

    /* Depsgraph refresh functions are called here. */
    const bool correct_aspect = RNA_boolean_get(op->ptr, "correct_aspect");
    ED_uvedit_pack_islands_multi(scene,
                                 objects_changed,
                                 object_changed_len,
                                 NULL,
                                 &(struct UVPackIsland_Params){
                                     .rotate = true,
                                     /* We could make this optional. */
                                     .rotate_align_axis = 1,
                                     .only_selected_faces = true,
                                     .correct_aspect = correct_aspect,
                                     .use_seams = true,
                                 });

    /* #ED_uvedit_pack_islands_multi only supports `per_face_aspect = false`. */
    const bool per_face_aspect = false;
    uv_map_clip_correct_multi(objects_changed, object_changed_len, op, per_face_aspect);
  }

  MEM_freeN(objects_changed);

  return OPERATOR_FINISHED;
}

void UV_OT_smart_project(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Smart UV Project";
  ot->idname = "UV_OT_smart_project";
  ot->description = "Projection unwraps the selected faces of mesh objects";

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* api callbacks */
  ot->exec = smart_project_exec;
  ot->poll = ED_operator_uvmap;
  ot->invoke = WM_operator_props_popup_confirm;

  /* properties */
  prop = RNA_def_float_rotation(ot->srna,
                                "angle_limit",
                                0,
                                NULL,
                                DEG2RADF(0.0f),
                                DEG2RADF(90.0f),
                                "Angle Limit",
                                "Lower for more projection groups, higher for less distortion",
                                DEG2RADF(0.0f),
                                DEG2RADF(89.0f));
  RNA_def_property_float_default(prop, DEG2RADF(66.0f));

  RNA_def_float(ot->srna,
                "island_margin",
                0.0f,
                0.0f,
                1.0f,
                "Island Margin",
                "Margin to reduce bleed from adjacent islands",
                0.0f,
                1.0f);
  RNA_def_float(ot->srna,
                "area_weight",
                0.0f,
                0.0f,
                1.0f,
                "Area Weight",
                "Weight projection's vector by faces with larger areas",
                0.0f,
                1.0f);

  uv_map_clip_correct_properties_ex(ot, false);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Project UV From View Operator
 * \{ */

static int uv_from_view_exec(bContext *C, wmOperator *op);

static int uv_from_view_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
  View3D *v3d = CTX_wm_view3d(C);
  RegionView3D *rv3d = CTX_wm_region_view3d(C);
  Camera *camera = ED_view3d_camera_data_get(v3d, rv3d);
  PropertyRNA *prop;

  prop = RNA_struct_find_property(op->ptr, "camera_bounds");
  if (!RNA_property_is_set(op->ptr, prop)) {
    RNA_property_boolean_set(op->ptr, prop, (camera != NULL));
  }
  prop = RNA_struct_find_property(op->ptr, "correct_aspect");
  if (!RNA_property_is_set(op->ptr, prop)) {
    RNA_property_boolean_set(op->ptr, prop, (camera == NULL));
  }

  return uv_from_view_exec(C, op);
}

static int uv_from_view_exec(bContext *C, wmOperator *op)
{
  ViewLayer *view_layer = CTX_data_view_layer(C);
  const Scene *scene = CTX_data_scene(C);
  ARegion *region = CTX_wm_region(C);
  View3D *v3d = CTX_wm_view3d(C);
  RegionView3D *rv3d = CTX_wm_region_view3d(C);
  Camera *camera = ED_view3d_camera_data_get(v3d, rv3d);
  BMFace *efa;
  BMLoop *l;
  BMIter iter, liter;
  MLoopUV *luv;
  float rotmat[4][4];
  float objects_pos_offset[4];
  bool changed_multi = false;

  const bool use_orthographic = RNA_boolean_get(op->ptr, "orthographic");

  /* NOTE: objects that aren't touched are set to NULL (to skip clipping). */
  uint objects_len = 0;
  Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      view_layer, v3d, &objects_len);

  if (use_orthographic) {
    /* Calculate average object position. */
    float objects_pos_avg[4] = {0};

    for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
      add_v4_v4(objects_pos_avg, objects[ob_index]->obmat[3]);
    }

    mul_v4_fl(objects_pos_avg, 1.0f / objects_len);
    negate_v4_v4(objects_pos_offset, objects_pos_avg);
  }

  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *obedit = objects[ob_index];
    BMEditMesh *em = BKE_editmesh_from_object(obedit);
    bool changed = false;

    /* add uvs if they don't exist yet */
    if (!ED_uvedit_ensure_uvs(obedit)) {
      continue;
    }

    const int cd_loop_uv_offset = CustomData_get_offset(&em->bm->ldata, CD_MLOOPUV);

    if (use_orthographic) {
      uv_map_rotation_matrix_ex(rotmat, rv3d, obedit, 90.0f, 0.0f, 1.0f, objects_pos_offset);

      BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
        if (!BM_elem_flag_test(efa, BM_ELEM_SELECT)) {
          continue;
        }

        BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
          luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
          BLI_uvproject_from_view_ortho(luv->uv, l->v->co, rotmat);
        }
        changed = true;
      }
    }
    else if (camera) {
      const bool camera_bounds = RNA_boolean_get(op->ptr, "camera_bounds");
      struct ProjCameraInfo *uci = BLI_uvproject_camera_info(
          v3d->camera,
          obedit->obmat,
          camera_bounds ? (scene->r.xsch * scene->r.xasp) : 1.0f,
          camera_bounds ? (scene->r.ysch * scene->r.yasp) : 1.0f);

      if (uci) {
        BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
          if (!BM_elem_flag_test(efa, BM_ELEM_SELECT)) {
            continue;
          }

          BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
            luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
            BLI_uvproject_from_camera(luv->uv, l->v->co, uci);
          }
          changed = true;
        }

        MEM_freeN(uci);
      }
    }
    else {
      copy_m4_m4(rotmat, obedit->obmat);

      BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
        if (!BM_elem_flag_test(efa, BM_ELEM_SELECT)) {
          continue;
        }

        BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
          luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
          BLI_uvproject_from_view(
              luv->uv, l->v->co, rv3d->persmat, rotmat, region->winx, region->winy);
        }
        changed = true;
      }
    }

    if (changed) {
      changed_multi = true;
      DEG_id_tag_update(obedit->data, ID_RECALC_GEOMETRY);
      WM_event_add_notifier(C, NC_GEOM | ND_DATA, obedit->data);
    }
    else {
      ARRAY_DELETE_REORDER_LAST(objects, ob_index, 1, objects_len);
      objects_len -= 1;
      ob_index -= 1;
    }
  }

  if (changed_multi) {
    uv_map_clip_correct_multi(objects, objects_len, op, true);
  }

  MEM_freeN(objects);

  if (changed_multi) {
    return OPERATOR_FINISHED;
  }
  return OPERATOR_CANCELLED;
}

static bool uv_from_view_poll(bContext *C)
{
  RegionView3D *rv3d = CTX_wm_region_view3d(C);

  if (!ED_operator_uvmap(C)) {
    return 0;
  }

  return (rv3d != NULL);
}

void UV_OT_project_from_view(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Project from View";
  ot->idname = "UV_OT_project_from_view";
  ot->description = "Project the UV vertices of the mesh as seen in current 3D view";

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* api callbacks */
  ot->invoke = uv_from_view_invoke;
  ot->exec = uv_from_view_exec;
  ot->poll = uv_from_view_poll;

  /* properties */
  RNA_def_boolean(ot->srna, "orthographic", 0, "Orthographic", "Use orthographic projection");
  RNA_def_boolean(ot->srna,
                  "camera_bounds",
                  1,
                  "Camera Bounds",
                  "Map UVs to the camera region taking resolution and aspect into account");
  uv_map_clip_correct_properties(ot);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Reset UV Operator
 * \{ */

static int reset_exec(bContext *C, wmOperator *UNUSED(op))
{
  ViewLayer *view_layer = CTX_data_view_layer(C);
  View3D *v3d = CTX_wm_view3d(C);

  uint objects_len = 0;
  Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      view_layer, v3d, &objects_len);
  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *obedit = objects[ob_index];
    Mesh *me = (Mesh *)obedit->data;
    BMEditMesh *em = BKE_editmesh_from_object(obedit);

    if (em->bm->totfacesel == 0) {
      continue;
    }

    /* add uvs if they don't exist yet */
    if (!ED_uvedit_ensure_uvs(obedit)) {
      continue;
    }

    ED_mesh_uv_loop_reset(C, me);

    DEG_id_tag_update(obedit->data, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GEOM | ND_DATA, obedit->data);
  }
  MEM_freeN(objects);

  return OPERATOR_FINISHED;
}

void UV_OT_reset(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Reset";
  ot->idname = "UV_OT_reset";
  ot->description = "Reset UV projection";

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* api callbacks */
  ot->exec = reset_exec;
  ot->poll = ED_operator_uvmap;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Sphere UV Project Operator
 * \{ */

static void uv_sphere_project(float target[2],
                              const float source[3],
                              const float center[3],
                              const float rotmat[4][4])
{
  float pv[3];

  sub_v3_v3v3(pv, source, center);
  mul_m4_v3(rotmat, pv);

  map_to_sphere(&target[0], &target[1], pv[0], pv[1], pv[2]);

  /* split line is always zero */
  if (target[0] >= 1.0f) {
    target[0] -= 1.0f;
  }
}

static void uv_map_mirror(BMEditMesh *em, BMFace *efa)
{
  BMLoop *l;
  BMIter liter;
  MLoopUV *luv;
  float **uvs = BLI_array_alloca(uvs, efa->len);
  float dx;
  int i, mi;

  const int cd_loop_uv_offset = CustomData_get_offset(&em->bm->ldata, CD_MLOOPUV);

  BM_ITER_ELEM_INDEX (l, &liter, efa, BM_LOOPS_OF_FACE, i) {
    luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
    uvs[i] = luv->uv;
  }

  mi = 0;
  for (i = 1; i < efa->len; i++) {
    if (uvs[i][0] > uvs[mi][0]) {
      mi = i;
    }
  }

  for (i = 0; i < efa->len; i++) {
    if (i != mi) {
      dx = uvs[mi][0] - uvs[i][0];
      if (dx > 0.5f) {
        uvs[i][0] += 1.0f;
      }
    }
  }
}

static int sphere_project_exec(bContext *C, wmOperator *op)
{
  const Scene *scene = CTX_data_scene(C);
  View3D *v3d = CTX_wm_view3d(C);

  ViewLayer *view_layer = CTX_data_view_layer(C);
  uint objects_len = 0;
  Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      view_layer, v3d, &objects_len);
  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *obedit = objects[ob_index];
    BMEditMesh *em = BKE_editmesh_from_object(obedit);
    BMFace *efa;
    BMLoop *l;
    BMIter iter, liter;
    MLoopUV *luv;

    if (em->bm->totfacesel == 0) {
      continue;
    }

    /* add uvs if they don't exist yet */
    if (!ED_uvedit_ensure_uvs(obedit)) {
      continue;
    }

    const int cd_loop_uv_offset = CustomData_get_offset(&em->bm->ldata, CD_MLOOPUV);
    float center[3], rotmat[4][4];

    uv_map_transform(C, op, rotmat);
    uv_map_transform_center(scene, v3d, obedit, em, center, NULL);

    BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
      if (!BM_elem_flag_test(efa, BM_ELEM_SELECT)) {
        continue;
      }

      BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
        luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);

        uv_sphere_project(luv->uv, l->v->co, center, rotmat);
      }

      uv_map_mirror(em, efa);
    }

    uv_map_clip_correct(obedit, op);

    DEG_id_tag_update(obedit->data, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GEOM | ND_DATA, obedit->data);
  }
  MEM_freeN(objects);

  return OPERATOR_FINISHED;
}

void UV_OT_sphere_project(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Sphere Projection";
  ot->idname = "UV_OT_sphere_project";
  ot->description = "Project the UV vertices of the mesh over the curved surface of a sphere";

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* api callbacks */
  ot->exec = sphere_project_exec;
  ot->poll = ED_operator_uvmap;

  /* properties */
  uv_transform_properties(ot, 0);
  uv_map_clip_correct_properties(ot);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Cylinder UV Project Operator
 * \{ */

static void uv_cylinder_project(float target[2],
                                const float source[3],
                                const float center[3],
                                const float rotmat[4][4])
{
  float pv[3];

  sub_v3_v3v3(pv, source, center);
  mul_m4_v3(rotmat, pv);

  map_to_tube(&target[0], &target[1], pv[0], pv[1], pv[2]);

  /* split line is always zero */
  if (target[0] >= 1.0f) {
    target[0] -= 1.0f;
  }
}

static int cylinder_project_exec(bContext *C, wmOperator *op)
{
  const Scene *scene = CTX_data_scene(C);
  View3D *v3d = CTX_wm_view3d(C);

  ViewLayer *view_layer = CTX_data_view_layer(C);
  uint objects_len = 0;
  Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      view_layer, v3d, &objects_len);
  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *obedit = objects[ob_index];
    BMEditMesh *em = BKE_editmesh_from_object(obedit);
    BMFace *efa;
    BMLoop *l;
    BMIter iter, liter;
    MLoopUV *luv;

    if (em->bm->totfacesel == 0) {
      continue;
    }

    /* add uvs if they don't exist yet */
    if (!ED_uvedit_ensure_uvs(obedit)) {
      continue;
    }

    const int cd_loop_uv_offset = CustomData_get_offset(&em->bm->ldata, CD_MLOOPUV);
    float center[3], rotmat[4][4];

    uv_map_transform(C, op, rotmat);
    uv_map_transform_center(scene, v3d, obedit, em, center, NULL);

    BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
      if (!BM_elem_flag_test(efa, BM_ELEM_SELECT)) {
        continue;
      }

      BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
        luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);

        uv_cylinder_project(luv->uv, l->v->co, center, rotmat);
      }

      uv_map_mirror(em, efa);
    }

    uv_map_clip_correct(obedit, op);

    DEG_id_tag_update(obedit->data, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GEOM | ND_DATA, obedit->data);
  }
  MEM_freeN(objects);

  return OPERATOR_FINISHED;
}

void UV_OT_cylinder_project(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Cylinder Projection";
  ot->idname = "UV_OT_cylinder_project";
  ot->description = "Project the UV vertices of the mesh over the curved wall of a cylinder";

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* api callbacks */
  ot->exec = cylinder_project_exec;
  ot->poll = ED_operator_uvmap;

  /* properties */
  uv_transform_properties(ot, 1);
  uv_map_clip_correct_properties(ot);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Cube UV Project Operator
 * \{ */

static void uvedit_unwrap_cube_project(BMesh *bm,
                                       float cube_size,
                                       bool use_select,
                                       const float center[3])
{
  BMFace *efa;
  BMLoop *l;
  BMIter iter, liter;
  MLoopUV *luv;
  float loc[3];
  int cox, coy;

  int cd_loop_uv_offset;

  cd_loop_uv_offset = CustomData_get_offset(&bm->ldata, CD_MLOOPUV);

  if (center) {
    copy_v3_v3(loc, center);
  }
  else {
    zero_v3(loc);
  }

  if (UNLIKELY(cube_size == 0.0f)) {
    cube_size = 1.0f;
  }

  /* choose x,y,z axis for projection depending on the largest normal
   * component, but clusters all together around the center of map. */

  BM_ITER_MESH (efa, &iter, bm, BM_FACES_OF_MESH) {
    if (use_select && !BM_elem_flag_test(efa, BM_ELEM_SELECT)) {
      continue;
    }

    axis_dominant_v3(&cox, &coy, efa->no);

    BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
      luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
      luv->uv[0] = 0.5f + ((l->v->co[cox] - loc[cox]) / cube_size);
      luv->uv[1] = 0.5f + ((l->v->co[coy] - loc[coy]) / cube_size);
    }
  }
}

static int cube_project_exec(bContext *C, wmOperator *op)
{
  const Scene *scene = CTX_data_scene(C);
  View3D *v3d = CTX_wm_view3d(C);

  PropertyRNA *prop_cube_size = RNA_struct_find_property(op->ptr, "cube_size");
  const float cube_size_init = RNA_property_float_get(op->ptr, prop_cube_size);

  ViewLayer *view_layer = CTX_data_view_layer(C);
  uint objects_len = 0;
  Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      view_layer, v3d, &objects_len);
  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *obedit = objects[ob_index];
    BMEditMesh *em = BKE_editmesh_from_object(obedit);

    if (em->bm->totfacesel == 0) {
      continue;
    }

    /* add uvs if they don't exist yet */
    if (!ED_uvedit_ensure_uvs(obedit)) {
      continue;
    }

    float bounds[2][3];
    float(*bounds_buf)[3] = NULL;

    if (!RNA_property_is_set(op->ptr, prop_cube_size)) {
      bounds_buf = bounds;
    }

    float center[3];
    uv_map_transform_center(scene, v3d, obedit, em, center, bounds_buf);

    /* calculate based on bounds */
    float cube_size = cube_size_init;
    if (bounds_buf) {
      float dims[3];
      sub_v3_v3v3(dims, bounds[1], bounds[0]);
      cube_size = max_fff(UNPACK3(dims));
      if (ob_index == 0) {
        /* This doesn't fit well with, multiple objects. */
        RNA_property_float_set(op->ptr, prop_cube_size, cube_size);
      }
    }

    uvedit_unwrap_cube_project(em->bm, cube_size, true, center);

    uv_map_clip_correct(obedit, op);

    DEG_id_tag_update(obedit->data, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GEOM | ND_DATA, obedit->data);
  }
  MEM_freeN(objects);

  return OPERATOR_FINISHED;
}

void UV_OT_cube_project(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Cube Projection";
  ot->idname = "UV_OT_cube_project";
  ot->description = "Project the UV vertices of the mesh over the six faces of a cube";

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* api callbacks */
  ot->exec = cube_project_exec;
  ot->poll = ED_operator_uvmap;

  /* properties */
  RNA_def_float(ot->srna,
                "cube_size",
                1.0f,
                0.0f,
                FLT_MAX,
                "Cube Size",
                "Size of the cube to project on",
                0.001f,
                100.0f);
  uv_map_clip_correct_properties(ot);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Simple UVs for Texture Painting
 * \{ */

void ED_uvedit_add_simple_uvs(Main *bmain, const Scene *scene, Object *ob)
{
  Mesh *me = ob->data;
  bool sync_selection = (scene->toolsettings->uv_flag & UV_SYNC_SELECTION) != 0;

  BMesh *bm = BM_mesh_create(&bm_mesh_allocsize_default,
                             &((struct BMeshCreateParams){
                                 .use_toolflags = false,
                             }));

  /* turn sync selection off,
   * since we are not in edit mode we need to ensure only the uv flags are tested */
  scene->toolsettings->uv_flag &= ~UV_SYNC_SELECTION;

  ED_mesh_uv_ensure(me, NULL);

  BM_mesh_bm_from_me(bm,
                     me,
                     (&(struct BMeshFromMeshParams){
                         .calc_face_normal = true,
                         .calc_vert_normal = true,
                     }));
  /* select all uv loops first - pack parameters needs this to make sure charts are registered */
  ED_uvedit_select_all(bm);
  /* A cube size of 2.0 maps [-1..1] vertex coords to [0.0..1.0] in UV coords. */
  uvedit_unwrap_cube_project(bm, 2.0, false, NULL);
  /* Set the margin really quickly before the packing operation. */
  scene->toolsettings->uvcalc_margin = 0.001f;
  uvedit_pack_islands(scene, ob, bm);
  BM_mesh_bm_to_me(bmain, bm, me, (&(struct BMeshToMeshParams){0}));
  BM_mesh_free(bm);

  if (sync_selection) {
    scene->toolsettings->uv_flag |= UV_SYNC_SELECTION;
  }
}

/** \} */
