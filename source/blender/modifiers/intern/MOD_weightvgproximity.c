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
 * The Original Code is Copyright (C) 2011 by Bastien Montagne.
 * All rights reserved.
 */

/** \file
 * \ingroup modifiers
 */

#include "BLI_utildefines.h"

#include "BLI_ghash.h"
#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_rand.h"
#include "BLI_task.h"

#include "BLT_translation.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"

#include "BKE_bvhutils.h"
#include "BKE_context.h"
#include "BKE_curve.h"
#include "BKE_customdata.h"
#include "BKE_deform.h"
#include "BKE_lib_id.h"
#include "BKE_lib_query.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"
#include "BKE_screen.h"
#include "BKE_texture.h" /* Texture masking. */

#include "UI_interface.h"
#include "UI_resources.h"

#include "RNA_access.h"

#include "DEG_depsgraph_build.h"
#include "DEG_depsgraph_query.h"

#include "MEM_guardedalloc.h"

#include "MOD_modifiertypes.h"
#include "MOD_ui_common.h"
#include "MOD_util.h"
#include "MOD_weightvg_util.h"

//#define USE_TIMEIT

#ifdef USE_TIMEIT
#  include "PIL_time.h"
#  include "PIL_time_utildefines.h"
#endif

/**************************************
 * Util functions.                    *
 **************************************/

/* Util macro. */
#define OUT_OF_MEMORY() ((void)printf("WeightVGProximity: Out of memory.\n"))

typedef struct Vert2GeomData {
  /* Read-only data */
  float (*v_cos)[3];

  const SpaceTransform *loc2trgt;

  BVHTreeFromMesh *treeData[3];

  /* Write data, but not needing locking (two different threads will never write same index). */
  float *dist[3];
} Vert2GeomData;

/**
 * Data which is localized to each computed chunk
 * (i.e. thread-safe, and with continuous subset of index range).
 */
typedef struct Vert2GeomDataChunk {
  /* Read-only data */
  float last_hit_co[3][3];
  bool is_init[3];
} Vert2GeomDataChunk;

/**
 * Callback used by BLI_task 'for loop' helper.
 */
static void vert2geom_task_cb_ex(void *__restrict userdata,
                                 const int iter,
                                 const TaskParallelTLS *__restrict tls)
{
  Vert2GeomData *data = userdata;
  Vert2GeomDataChunk *data_chunk = tls->userdata_chunk;

  float tmp_co[3];
  int i;

  /* Convert the vertex to tree coordinates. */
  copy_v3_v3(tmp_co, data->v_cos[iter]);
  BLI_space_transform_apply(data->loc2trgt, tmp_co);

  for (i = 0; i < ARRAY_SIZE(data->dist); i++) {
    if (data->dist[i]) {
      BVHTreeNearest nearest = {0};

      /* Note that we use local proximity heuristics (to reduce the nearest search).
       *
       * If we already had an hit before in same chunk of tasks (i.e. previous vertex by index),
       * we assume this vertex is going to have a close hit to that other vertex,
       * so we can initiate the "nearest.dist" with the expected value to that last hit.
       * This will lead in pruning of the search tree.
       */
      nearest.dist_sq = data_chunk->is_init[i] ?
                            len_squared_v3v3(tmp_co, data_chunk->last_hit_co[i]) :
                            FLT_MAX;
      nearest.index = -1;

      /* Compute and store result. If invalid (-1 idx), keep FLT_MAX dist. */
      BLI_bvhtree_find_nearest(data->treeData[i]->tree,
                               tmp_co,
                               &nearest,
                               data->treeData[i]->nearest_callback,
                               data->treeData[i]);
      data->dist[i][iter] = sqrtf(nearest.dist_sq);

      if (nearest.index != -1) {
        copy_v3_v3(data_chunk->last_hit_co[i], nearest.co);
        data_chunk->is_init[i] = true;
      }
    }
  }
}

/**
 * Find nearest vertex and/or edge and/or face, for each vertex (adapted from shrinkwrap.c).
 */
static void get_vert2geom_distance(int numVerts,
                                   float (*v_cos)[3],
                                   float *dist_v,
                                   float *dist_e,
                                   float *dist_f,
                                   Mesh *target,
                                   const SpaceTransform *loc2trgt)
{
  Vert2GeomData data = {0};
  Vert2GeomDataChunk data_chunk = {{{0}}};

  BVHTreeFromMesh treeData_v = {NULL};
  BVHTreeFromMesh treeData_e = {NULL};
  BVHTreeFromMesh treeData_f = {NULL};

  if (dist_v) {
    /* Create a bvh-tree of the given target's verts. */
    BKE_bvhtree_from_mesh_get(&treeData_v, target, BVHTREE_FROM_VERTS, 2);
    if (treeData_v.tree == NULL) {
      OUT_OF_MEMORY();
      return;
    }
  }
  if (dist_e) {
    /* Create a bvh-tree of the given target's edges. */
    BKE_bvhtree_from_mesh_get(&treeData_e, target, BVHTREE_FROM_EDGES, 2);
    if (treeData_e.tree == NULL) {
      OUT_OF_MEMORY();
      return;
    }
  }
  if (dist_f) {
    /* Create a bvh-tree of the given target's faces. */
    BKE_bvhtree_from_mesh_get(&treeData_f, target, BVHTREE_FROM_LOOPTRI, 2);
    if (treeData_f.tree == NULL) {
      OUT_OF_MEMORY();
      return;
    }
  }

  data.v_cos = v_cos;
  data.loc2trgt = loc2trgt;
  data.treeData[0] = &treeData_v;
  data.treeData[1] = &treeData_e;
  data.treeData[2] = &treeData_f;
  data.dist[0] = dist_v;
  data.dist[1] = dist_e;
  data.dist[2] = dist_f;

  TaskParallelSettings settings;
  BLI_parallel_range_settings_defaults(&settings);
  settings.use_threading = (numVerts > 10000);
  settings.userdata_chunk = &data_chunk;
  settings.userdata_chunk_size = sizeof(data_chunk);
  BLI_task_parallel_range(0, numVerts, &data, vert2geom_task_cb_ex, &settings);

  if (dist_v) {
    free_bvhtree_from_mesh(&treeData_v);
  }
  if (dist_e) {
    free_bvhtree_from_mesh(&treeData_e);
  }
  if (dist_f) {
    free_bvhtree_from_mesh(&treeData_f);
  }
}

/**
 * Returns the real distance between a vertex and another reference object.
 * Note that it works in final world space (i.e. with constraints etc. applied).
 */
static void get_vert2ob_distance(
    int numVerts, float (*v_cos)[3], float *dist, Object *ob, Object *obr)
{
  /* Vertex and ref object coordinates. */
  float v_wco[3];
  uint i = numVerts;

  while (i-- > 0) {
    /* Get world-coordinates of the vertex (constraints and anim included). */
    mul_v3_m4v3(v_wco, ob->obmat, v_cos[i]);
    /* Return distance between both coordinates. */
    dist[i] = len_v3v3(v_wco, obr->obmat[3]);
  }
}

/**
 * Returns the real distance between an object and another reference object.
 * Note that it works in final world space (i.e. with constraints etc. applied).
 */
static float get_ob2ob_distance(const Object *ob, const Object *obr)
{
  return len_v3v3(ob->obmat[3], obr->obmat[3]);
}

/**
 * Maps distances to weights, with an optional "smoothing" mapping.
 */
static void do_map(Object *ob,
                   float *weights,
                   const int nidx,
                   const float min_d,
                   const float max_d,
                   short mode,
                   const bool do_invert_mapping)
{
  const float range_inv = 1.0f / (max_d - min_d); /* invert since multiplication is faster */
  uint i = nidx;
  if (max_d == min_d) {
    while (i-- > 0) {
      weights[i] = (weights[i] >= max_d) ? 1.0f : 0.0f; /* "Step" behavior... */
    }
  }
  else if (max_d > min_d) {
    while (i-- > 0) {
      if (weights[i] >= max_d) {
        weights[i] = 1.0f; /* most likely case first */
      }
      else if (weights[i] <= min_d) {
        weights[i] = 0.0f;
      }
      else {
        weights[i] = (weights[i] - min_d) * range_inv;
      }
    }
  }
  else {
    while (i-- > 0) {
      if (weights[i] <= max_d) {
        weights[i] = 1.0f; /* most likely case first */
      }
      else if (weights[i] >= min_d) {
        weights[i] = 0.0f;
      }
      else {
        weights[i] = (weights[i] - min_d) * range_inv;
      }
    }
  }

  BLI_assert(mode != MOD_WVG_MAPPING_CURVE);
  if (do_invert_mapping || mode != MOD_WVG_MAPPING_NONE) {
    RNG *rng = NULL;

    if (mode == MOD_WVG_MAPPING_RANDOM) {
      rng = BLI_rng_new_srandom(BLI_ghashutil_strhash(ob->id.name + 2));
    }

    weightvg_do_map(nidx, weights, mode, do_invert_mapping, NULL, rng);

    if (rng) {
      BLI_rng_free(rng);
    }
  }
}

/**************************************
 * Modifiers functions.               *
 **************************************/
static void initData(ModifierData *md)
{
  WeightVGProximityModifierData *wmd = (WeightVGProximityModifierData *)md;

  wmd->proximity_mode = MOD_WVG_PROXIMITY_OBJECT;
  wmd->proximity_flags = MOD_WVG_PROXIMITY_GEOM_VERTS;

  wmd->falloff_type = MOD_WVG_MAPPING_NONE;

  wmd->mask_constant = 1.0f;
  wmd->mask_tex_use_channel = MOD_WVG_MASK_TEX_USE_INT; /* Use intensity by default. */
  wmd->mask_tex_mapping = MOD_DISP_MAP_LOCAL;
  wmd->max_dist = 1.0f; /* vert arbitrary distance, but don't use 0 */
}

static void requiredDataMask(Object *UNUSED(ob),
                             ModifierData *md,
                             CustomData_MeshMasks *r_cddata_masks)
{
  WeightVGProximityModifierData *wmd = (WeightVGProximityModifierData *)md;

  /* We need vertex groups! */
  r_cddata_masks->vmask |= CD_MASK_MDEFORMVERT;

  /* Ask for UV coordinates if we need them. */
  if (wmd->mask_tex_mapping == MOD_DISP_MAP_UV) {
    r_cddata_masks->fmask |= CD_MASK_MTFACE;
  }

  /* No need to ask for CD_PREVIEW_MLOOPCOL... */
}

static bool dependsOnTime(ModifierData *md)
{
  WeightVGProximityModifierData *wmd = (WeightVGProximityModifierData *)md;

  if (wmd->mask_texture) {
    return BKE_texture_dependsOnTime(wmd->mask_texture);
  }
  return 0;
}

static void foreachObjectLink(ModifierData *md, Object *ob, ObjectWalkFunc walk, void *userData)
{
  WeightVGProximityModifierData *wmd = (WeightVGProximityModifierData *)md;
  walk(userData, ob, &wmd->proximity_ob_target, IDWALK_CB_NOP);
  walk(userData, ob, &wmd->mask_tex_map_obj, IDWALK_CB_NOP);
}

static void foreachIDLink(ModifierData *md, Object *ob, IDWalkFunc walk, void *userData)
{
  WeightVGProximityModifierData *wmd = (WeightVGProximityModifierData *)md;

  walk(userData, ob, (ID **)&wmd->mask_texture, IDWALK_CB_USER);

  foreachObjectLink(md, ob, (ObjectWalkFunc)walk, userData);
}

static void foreachTexLink(ModifierData *md, Object *ob, TexWalkFunc walk, void *userData)
{
  walk(userData, ob, md, "mask_texture");
}

static void updateDepsgraph(ModifierData *md, const ModifierUpdateDepsgraphContext *ctx)
{
  WeightVGProximityModifierData *wmd = (WeightVGProximityModifierData *)md;
  bool need_transform_relation = false;

  if (wmd->proximity_ob_target != NULL) {
    DEG_add_object_relation(
        ctx->node, wmd->proximity_ob_target, DEG_OB_COMP_TRANSFORM, "WeightVGProximity Modifier");
    if (wmd->proximity_ob_target->data != NULL &&
        wmd->proximity_mode == MOD_WVG_PROXIMITY_GEOMETRY) {
      DEG_add_object_relation(
          ctx->node, wmd->proximity_ob_target, DEG_OB_COMP_GEOMETRY, "WeightVGProximity Modifier");
    }
    need_transform_relation = true;
  }

  if (wmd->mask_texture != NULL) {
    DEG_add_generic_id_relation(ctx->node, &wmd->mask_texture->id, "WeightVGProximity Modifier");

    if (wmd->mask_tex_map_obj != NULL && wmd->mask_tex_mapping == MOD_DISP_MAP_OBJECT) {
      MOD_depsgraph_update_object_bone_relation(
          ctx->node, wmd->mask_tex_map_obj, wmd->mask_tex_map_bone, "WeightVGProximity Modifier");
      need_transform_relation = true;
    }
    else if (wmd->mask_tex_mapping == MOD_DISP_MAP_GLOBAL) {
      need_transform_relation = true;
    }
  }

  if (need_transform_relation) {
    DEG_add_modifier_to_transform_relation(ctx->node, "WeightVGProximity Modifier");
  }
}

static bool isDisabled(const struct Scene *UNUSED(scene),
                       ModifierData *md,
                       bool UNUSED(useRenderParams))
{
  WeightVGProximityModifierData *wmd = (WeightVGProximityModifierData *)md;
  /* If no vertex group, bypass. */
  if (wmd->defgrp_name[0] == '\0') {
    return 1;
  }
  /* If no target object, bypass. */
  return (wmd->proximity_ob_target == NULL);
}

static Mesh *modifyMesh(ModifierData *md, const ModifierEvalContext *ctx, Mesh *mesh)
{
  BLI_assert(mesh != NULL);

  WeightVGProximityModifierData *wmd = (WeightVGProximityModifierData *)md;
  MDeformVert *dvert = NULL;
  MDeformWeight **dw, **tdw;
  float(*v_cos)[3] = NULL; /* The vertices coordinates. */
  Object *ob = ctx->object;
  Object *obr = NULL; /* Our target object. */
  int defgrp_index;
  float *tw = NULL;
  float *org_w = NULL;
  float *new_w = NULL;
  int *tidx, *indices = NULL;
  int numIdx = 0;
  int i;
  const bool invert_vgroup_mask = (wmd->proximity_flags & MOD_WVG_PROXIMITY_INVERT_VGROUP_MASK) !=
                                  0;
  const bool do_normalize = (wmd->proximity_flags & MOD_WVG_PROXIMITY_WEIGHTS_NORMALIZE) != 0;
  /* Flags. */
#if 0
  const bool do_prev = (wmd->modifier.mode & eModifierMode_DoWeightPreview) != 0;
#endif

#ifdef USE_TIMEIT
  TIMEIT_START(perf);
#endif

  /* Get number of verts. */
  const int numVerts = mesh->totvert;

  /* Check if we can just return the original mesh.
   * Must have verts and therefore verts assigned to vgroups to do anything useful!
   */
  if ((numVerts == 0) || BLI_listbase_is_empty(&ctx->object->defbase)) {
    return mesh;
  }

  /* Get our target object. */
  obr = wmd->proximity_ob_target;
  if (obr == NULL) {
    return mesh;
  }

  /* Get vgroup idx from its name. */
  defgrp_index = BKE_object_defgroup_name_index(ob, wmd->defgrp_name);
  if (defgrp_index == -1) {
    return mesh;
  }

  const bool has_mdef = CustomData_has_layer(&mesh->vdata, CD_MDEFORMVERT);
  /* If no vertices were ever added to an object's vgroup, dvert might be NULL. */
  /* As this modifier never add vertices to vgroup, just return. */
  if (!has_mdef) {
    return mesh;
  }

  dvert = CustomData_duplicate_referenced_layer(&mesh->vdata, CD_MDEFORMVERT, numVerts);
  /* Ultimate security check. */
  if (!dvert) {
    return mesh;
  }
  mesh->dvert = dvert;

  /* Find out which vertices to work on (all vertices in vgroup), and get their relevant weight. */
  tidx = MEM_malloc_arrayN(numVerts, sizeof(int), "WeightVGProximity Modifier, tidx");
  tw = MEM_malloc_arrayN(numVerts, sizeof(float), "WeightVGProximity Modifier, tw");
  tdw = MEM_malloc_arrayN(numVerts, sizeof(MDeformWeight *), "WeightVGProximity Modifier, tdw");
  for (i = 0; i < numVerts; i++) {
    MDeformWeight *_dw = BKE_defvert_find_index(&dvert[i], defgrp_index);
    if (_dw) {
      tidx[numIdx] = i;
      tw[numIdx] = _dw->weight;
      tdw[numIdx++] = _dw;
    }
  }
  /* If no vertices found, return org data! */
  if (numIdx == 0) {
    MEM_freeN(tidx);
    MEM_freeN(tw);
    MEM_freeN(tdw);
    return mesh;
  }
  if (numIdx != numVerts) {
    indices = MEM_malloc_arrayN(numIdx, sizeof(int), "WeightVGProximity Modifier, indices");
    memcpy(indices, tidx, sizeof(int) * numIdx);
    org_w = MEM_malloc_arrayN(numIdx, sizeof(float), "WeightVGProximity Modifier, org_w");
    memcpy(org_w, tw, sizeof(float) * numIdx);
    dw = MEM_malloc_arrayN(numIdx, sizeof(MDeformWeight *), "WeightVGProximity Modifier, dw");
    memcpy(dw, tdw, sizeof(MDeformWeight *) * numIdx);
    MEM_freeN(tw);
    MEM_freeN(tdw);
  }
  else {
    org_w = tw;
    dw = tdw;
  }
  new_w = MEM_malloc_arrayN(numIdx, sizeof(float), "WeightVGProximity Modifier, new_w");
  MEM_freeN(tidx);

  /* Get our vertex coordinates. */
  if (numIdx != numVerts) {
    float(*tv_cos)[3] = BKE_mesh_vert_coords_alloc(mesh, NULL);
    v_cos = MEM_malloc_arrayN(numIdx, sizeof(float[3]), "WeightVGProximity Modifier, v_cos");
    for (i = 0; i < numIdx; i++) {
      copy_v3_v3(v_cos[i], tv_cos[indices[i]]);
    }
    MEM_freeN(tv_cos);
  }
  else {
    v_cos = BKE_mesh_vert_coords_alloc(mesh, NULL);
  }

  /* Compute wanted distances. */
  if (wmd->proximity_mode == MOD_WVG_PROXIMITY_OBJECT) {
    const float dist = get_ob2ob_distance(ob, obr);
    for (i = 0; i < numIdx; i++) {
      new_w[i] = dist;
    }
  }
  else if (wmd->proximity_mode == MOD_WVG_PROXIMITY_GEOMETRY) {
    const bool use_trgt_verts = (wmd->proximity_flags & MOD_WVG_PROXIMITY_GEOM_VERTS) != 0;
    const bool use_trgt_edges = (wmd->proximity_flags & MOD_WVG_PROXIMITY_GEOM_EDGES) != 0;
    const bool use_trgt_faces = (wmd->proximity_flags & MOD_WVG_PROXIMITY_GEOM_FACES) != 0;

    if (use_trgt_verts || use_trgt_edges || use_trgt_faces) {
      Mesh *target_mesh = BKE_modifier_get_evaluated_mesh_from_evaluated_object(obr, false);

      /* We must check that we do have a valid target_mesh! */
      if (target_mesh != NULL) {
        SpaceTransform loc2trgt;
        float *dists_v = use_trgt_verts ? MEM_malloc_arrayN(numIdx, sizeof(float), "dists_v") :
                                          NULL;
        float *dists_e = use_trgt_edges ? MEM_malloc_arrayN(numIdx, sizeof(float), "dists_e") :
                                          NULL;
        float *dists_f = use_trgt_faces ? MEM_malloc_arrayN(numIdx, sizeof(float), "dists_f") :
                                          NULL;

        BLI_SPACE_TRANSFORM_SETUP(&loc2trgt, ob, obr);
        get_vert2geom_distance(numIdx, v_cos, dists_v, dists_e, dists_f, target_mesh, &loc2trgt);
        for (i = 0; i < numIdx; i++) {
          new_w[i] = dists_v ? dists_v[i] : FLT_MAX;
          if (dists_e) {
            new_w[i] = min_ff(dists_e[i], new_w[i]);
          }
          if (dists_f) {
            new_w[i] = min_ff(dists_f[i], new_w[i]);
          }
        }

        MEM_SAFE_FREE(dists_v);
        MEM_SAFE_FREE(dists_e);
        MEM_SAFE_FREE(dists_f);
      }
      /* Else, fall back to default obj2vert behavior. */
      else {
        get_vert2ob_distance(numIdx, v_cos, new_w, ob, obr);
      }
    }
    else {
      get_vert2ob_distance(numIdx, v_cos, new_w, ob, obr);
    }
  }

  /* Map distances to weights. */
  do_map(ob,
         new_w,
         numIdx,
         wmd->min_dist,
         wmd->max_dist,
         wmd->falloff_type,
         (wmd->proximity_flags & MOD_WVG_PROXIMITY_INVERT_FALLOFF) != 0);

  /* Do masking. */
  struct Scene *scene = DEG_get_evaluated_scene(ctx->depsgraph);
  weightvg_do_mask(ctx,
                   numIdx,
                   indices,
                   org_w,
                   new_w,
                   ob,
                   mesh,
                   wmd->mask_constant,
                   wmd->mask_defgrp_name,
                   scene,
                   wmd->mask_texture,
                   wmd->mask_tex_use_channel,
                   wmd->mask_tex_mapping,
                   wmd->mask_tex_map_obj,
                   wmd->mask_tex_map_bone,
                   wmd->mask_tex_uvlayer_name,
                   invert_vgroup_mask);

  /* Update vgroup. Note we never add nor remove vertices from vgroup here. */
  weightvg_update_vg(
      dvert, defgrp_index, dw, numIdx, indices, org_w, false, 0.0f, false, 0.0f, do_normalize);

  /* If weight preview enabled... */
#if 0 /* XXX Currently done in mod stack :/ */
  if (do_prev) {
    DM_update_weight_mcol(ob, dm, 0, org_w, numIdx, indices);
  }
#endif

  /* Freeing stuff. */
  MEM_freeN(org_w);
  MEM_freeN(new_w);
  MEM_freeN(dw);
  MEM_freeN(v_cos);
  MEM_SAFE_FREE(indices);

#ifdef USE_TIMEIT
  TIMEIT_END(perf);
#endif

  /* Return the vgroup-modified mesh. */
  return mesh;
}

static void panel_draw(const bContext *C, Panel *panel)
{
  uiLayout *row, *col, *sub;
  uiLayout *layout = panel->layout;

  PointerRNA ptr;
  PointerRNA ob_ptr;
  modifier_panel_get_property_pointers(C, panel, &ob_ptr, &ptr);

  uiLayoutSetPropSep(layout, true);

  uiItemPointerR(layout, &ptr, "vertex_group", &ob_ptr, "vertex_groups", NULL, ICON_NONE);

  uiItemR(layout, &ptr, "target", 0, NULL, ICON_NONE);

  uiItemS(layout);

  uiItemR(layout, &ptr, "proximity_mode", 0, NULL, ICON_NONE);
  if (RNA_enum_get(&ptr, "proximity_mode") == MOD_WVG_PROXIMITY_GEOMETRY) {
    uiItemR(layout, &ptr, "proximity_geometry", UI_ITEM_R_EXPAND, IFACE_("Geometry"), ICON_NONE);
  }

  col = uiLayoutColumn(layout, true);
  uiItemR(col, &ptr, "min_dist", 0, NULL, ICON_NONE);
  uiItemR(col, &ptr, "max_dist", 0, NULL, ICON_NONE);

  uiItemS(layout);

  row = uiLayoutRow(layout, true);
  uiItemR(row, &ptr, "falloff_type", 0, NULL, ICON_NONE);
  sub = uiLayoutRow(row, true);
  uiLayoutSetPropSep(sub, false);
  uiItemR(row, &ptr, "invert_falloff", 0, "", ICON_ARROW_LEFTRIGHT);
  modifier_panel_end(layout, &ptr);

  uiItemS(layout);

  uiItemR(layout, &ptr, "normalize", 0, NULL, ICON_NONE);
}

static void influence_panel_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA ptr;
  PointerRNA ob_ptr;
  modifier_panel_get_property_pointers(C, panel, &ob_ptr, &ptr);

  weightvg_ui_common(C, &ob_ptr, &ptr, layout);
}

static void panelRegister(ARegionType *region_type)
{
  PanelType *panel_type = modifier_panel_register(
      region_type, eModifierType_WeightVGProximity, panel_draw);
  modifier_subpanel_register(
      region_type, "influence", "Influence", NULL, influence_panel_draw, panel_type);
}

ModifierTypeInfo modifierType_WeightVGProximity = {
    /* name */ "VertexWeightProximity",
    /* structName */ "WeightVGProximityModifierData",
    /* structSize */ sizeof(WeightVGProximityModifierData),
    /* type */ eModifierTypeType_NonGeometrical,
    /* flags */ eModifierTypeFlag_AcceptsMesh | eModifierTypeFlag_SupportsMapping |
        eModifierTypeFlag_SupportsEditmode | eModifierTypeFlag_UsesPreview,

    /* copyData */ BKE_modifier_copydata_generic,

    /* deformVerts */ NULL,
    /* deformMatrices */ NULL,
    /* deformVertsEM */ NULL,
    /* deformMatricesEM */ NULL,
    /* modifyMesh */ modifyMesh,
    /* modifyHair */ NULL,
    /* modifyPointCloud */ NULL,
    /* modifyVolume */ NULL,

    /* initData */ initData,
    /* requiredDataMask */ requiredDataMask,
    /* freeData */ NULL,
    /* isDisabled */ isDisabled,
    /* updateDepsgraph */ updateDepsgraph,
    /* dependsOnTime */ dependsOnTime,
    /* dependsOnNormals */ NULL,
    /* foreachObjectLink */ foreachObjectLink,
    /* foreachIDLink */ foreachIDLink,
    /* foreachTexLink */ foreachTexLink,
    /* freeRuntimeData */ NULL,
    /* panelRegister */ panelRegister,
};
