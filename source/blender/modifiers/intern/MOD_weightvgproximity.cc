/* SPDX-FileCopyrightText: 2011 by Bastien Montagne. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

#include "BLI_utildefines.h"

#include "BLI_ghash.h"
#include "BLI_listbase.h"
#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"
#include "BLI_rand.h"
#include "BLI_task.h"

#include "BLT_translation.hh"

#include "DNA_color_types.h" /* CurveMapping. */
#include "DNA_defaults.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"
#include "DNA_texture_types.h"

#include "BKE_bvhutils.hh"
#include "BKE_colortools.hh" /* CurveMapping. */
#include "BKE_customdata.hh"
#include "BKE_deform.hh"
#include "BKE_lib_query.hh"
#include "BKE_mesh.hh"
#include "BKE_mesh_wrapper.hh"
#include "BKE_modifier.hh"
#include "BKE_texture.h" /* Texture masking. */

#include "UI_interface.hh"
#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "BLO_read_write.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "DEG_depsgraph_build.hh"
#include "DEG_depsgraph_query.hh"

#include "MEM_guardedalloc.h"

#include "MOD_ui_common.hh"
#include "MOD_util.hh"
#include "MOD_weightvg_util.hh"

// #define USE_TIMEIT

#ifdef USE_TIMEIT
#  include "BLI_time.h"
#  include "BLI_time_utildefines.h"
#endif

/**************************************
 * Util functions.                    *
 **************************************/

/* Util macro. */
#define OUT_OF_MEMORY() (void)printf("WeightVGProximity: Out of memory.\n")

struct Vert2GeomData {
  /* Read-only data */
  blender::Span<blender::float3> positions;

  const int *indices;

  const SpaceTransform *loc2trgt;

  blender::bke::BVHTreeFromMesh *treeData[3];

  /* Write data, but not needing locking (two different threads will never write same index). */
  float *dist[3];
};

/**
 * Data which is localized to each computed chunk
 * (i.e. thread-safe, and with continuous subset of index range).
 */
struct Vert2GeomDataChunk {
  /* Read-only data */
  float last_hit_co[3][3];
  bool is_init[3];
};

/**
 * Callback used by BLI_task 'for loop' helper.
 */
static void vert2geom_task_cb_ex(void *__restrict userdata,
                                 const int iter,
                                 const TaskParallelTLS *__restrict tls)
{
  Vert2GeomData *data = static_cast<Vert2GeomData *>(userdata);
  Vert2GeomDataChunk *data_chunk = static_cast<Vert2GeomDataChunk *>(tls->userdata_chunk);

  float tmp_co[3];
  int i;

  /* Convert the vertex to tree coordinates. */
  copy_v3_v3(tmp_co, data->positions[data->indices ? data->indices[iter] : iter]);
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
 * Find nearest vertex and/or edge and/or face, for each vertex (adapted from `shrinkwrap.cc`).
 */
static void get_vert2geom_distance(int verts_num,
                                   const blender::Span<blender::float3> positions,
                                   const int *indices,
                                   float *dist_v,
                                   float *dist_e,
                                   float *dist_f,
                                   Mesh *target,
                                   const SpaceTransform *loc2trgt)
{
  Vert2GeomData data{};
  Vert2GeomDataChunk data_chunk = {{{0}}};

  blender::bke::BVHTreeFromMesh treeData_v{};
  blender::bke::BVHTreeFromMesh treeData_e{};
  blender::bke::BVHTreeFromMesh treeData_f{};

  if (dist_v) {
    /* Create a BVH-tree of the given target's verts. */
    treeData_v = target->bvh_verts();
    if (treeData_v.tree == nullptr) {
      OUT_OF_MEMORY();
      return;
    }
  }
  if (dist_e) {
    /* Create a BVH-tree of the given target's edges. */
    treeData_e = target->bvh_edges();
    if (treeData_e.tree == nullptr) {
      OUT_OF_MEMORY();
      return;
    }
  }
  if (dist_f) {
    /* Create a BVH-tree of the given target's faces. */
    treeData_f = target->bvh_corner_tris();
    if (treeData_f.tree == nullptr) {
      OUT_OF_MEMORY();
      return;
    }
  }

  data.positions = positions;
  data.indices = indices;
  data.loc2trgt = loc2trgt;
  data.treeData[0] = &treeData_v;
  data.treeData[1] = &treeData_e;
  data.treeData[2] = &treeData_f;
  data.dist[0] = dist_v;
  data.dist[1] = dist_e;
  data.dist[2] = dist_f;

  TaskParallelSettings settings;
  BLI_parallel_range_settings_defaults(&settings);
  settings.use_threading = (verts_num > 10000);
  settings.userdata_chunk = &data_chunk;
  settings.userdata_chunk_size = sizeof(data_chunk);
  BLI_task_parallel_range(0, verts_num, &data, vert2geom_task_cb_ex, &settings);
}

/**
 * Returns the real distance between a vertex and another reference object.
 * Note that it works in final world space (i.e. with constraints etc. applied).
 */
static void get_vert2ob_distance(int verts_num,
                                 const blender::Span<blender::float3> positions,
                                 const int *indices,
                                 float *dist,
                                 Object *ob,
                                 Object *obr)
{
  /* Vertex and ref object coordinates. */
  float v_wco[3];
  uint i = verts_num;

  while (i-- > 0) {
    /* Get world-coordinates of the vertex (constraints and anim included). */
    mul_v3_m4v3(v_wco, ob->object_to_world().ptr(), positions[indices ? indices[i] : i]);
    /* Return distance between both coordinates. */
    dist[i] = len_v3v3(v_wco, obr->object_to_world().location());
  }
}

/**
 * Returns the real distance between an object and another reference object.
 * Note that it works in final world space (i.e. with constraints etc. applied).
 */
static float get_ob2ob_distance(const Object *ob, const Object *obr)
{
  return len_v3v3(ob->object_to_world().location(), obr->object_to_world().location());
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
                   const bool do_invert_mapping,
                   CurveMapping *cmap)
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

  if (do_invert_mapping || mode != MOD_WVG_MAPPING_NONE) {
    RNG *rng = nullptr;

    if (mode == MOD_WVG_MAPPING_RANDOM) {
      rng = BLI_rng_new_srandom(BLI_ghashutil_strhash(ob->id.name + 2));
    }

    weightvg_do_map(nidx, weights, mode, do_invert_mapping, cmap, rng);

    if (rng) {
      BLI_rng_free(rng);
    }
  }
}

/**************************************
 * Modifiers functions.               *
 **************************************/
static void init_data(ModifierData *md)
{
  WeightVGProximityModifierData *wmd = (WeightVGProximityModifierData *)md;

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(wmd, modifier));

  MEMCPY_STRUCT_AFTER(wmd, DNA_struct_default_get(WeightVGProximityModifierData), modifier);

  wmd->cmap_curve = BKE_curvemapping_add(1, 0.0, 0.0, 1.0, 1.0);
  BKE_curvemapping_init(wmd->cmap_curve);
}

static void free_data(ModifierData *md)
{
  WeightVGProximityModifierData *wmd = (WeightVGProximityModifierData *)md;
  BKE_curvemapping_free(wmd->cmap_curve);
}

static void copy_data(const ModifierData *md, ModifierData *target, const int flag)
{
  const WeightVGProximityModifierData *wmd = (const WeightVGProximityModifierData *)md;
  WeightVGProximityModifierData *twmd = (WeightVGProximityModifierData *)target;

  BKE_modifier_copydata_generic(md, target, flag);

  twmd->cmap_curve = BKE_curvemapping_copy(wmd->cmap_curve);
}

static void required_data_mask(ModifierData *md, CustomData_MeshMasks *r_cddata_masks)
{
  WeightVGProximityModifierData *wmd = (WeightVGProximityModifierData *)md;

  /* We need vertex groups! */
  r_cddata_masks->vmask |= CD_MASK_MDEFORMVERT;

  /* Ask for UV coordinates if we need them. */
  if (wmd->mask_tex_mapping == MOD_DISP_MAP_UV) {
    r_cddata_masks->fmask |= CD_MASK_MTFACE;
  }
}

static bool depends_on_time(Scene * /*scene*/, ModifierData *md)
{
  WeightVGProximityModifierData *wmd = (WeightVGProximityModifierData *)md;

  if (wmd->mask_texture) {
    return BKE_texture_dependsOnTime(wmd->mask_texture);
  }
  return false;
}

static void foreach_ID_link(ModifierData *md, Object *ob, IDWalkFunc walk, void *user_data)
{
  WeightVGProximityModifierData *wmd = (WeightVGProximityModifierData *)md;

  walk(user_data, ob, (ID **)&wmd->mask_texture, IDWALK_CB_USER);
  walk(user_data, ob, (ID **)&wmd->proximity_ob_target, IDWALK_CB_NOP);
  walk(user_data, ob, (ID **)&wmd->mask_tex_map_obj, IDWALK_CB_NOP);
}

static void foreach_tex_link(ModifierData *md, Object *ob, TexWalkFunc walk, void *user_data)
{
  PointerRNA ptr = RNA_pointer_create_discrete(&ob->id, &RNA_Modifier, md);
  PropertyRNA *prop = RNA_struct_find_property(&ptr, "mask_texture");
  walk(user_data, ob, md, &ptr, prop);
}

static void update_depsgraph(ModifierData *md, const ModifierUpdateDepsgraphContext *ctx)
{
  WeightVGProximityModifierData *wmd = (WeightVGProximityModifierData *)md;
  bool need_transform_relation = false;

  if (wmd->proximity_ob_target != nullptr) {
    DEG_add_object_relation(
        ctx->node, wmd->proximity_ob_target, DEG_OB_COMP_TRANSFORM, "WeightVGProximity Modifier");
    if (wmd->proximity_ob_target->data != nullptr &&
        wmd->proximity_mode == MOD_WVG_PROXIMITY_GEOMETRY)
    {
      DEG_add_object_relation(
          ctx->node, wmd->proximity_ob_target, DEG_OB_COMP_GEOMETRY, "WeightVGProximity Modifier");
    }
    need_transform_relation = true;
  }

  if (wmd->mask_texture != nullptr) {
    DEG_add_generic_id_relation(ctx->node, &wmd->mask_texture->id, "WeightVGProximity Modifier");

    if (wmd->mask_tex_map_obj != nullptr && wmd->mask_tex_mapping == MOD_DISP_MAP_OBJECT) {
      MOD_depsgraph_update_object_bone_relation(
          ctx->node, wmd->mask_tex_map_obj, wmd->mask_tex_map_bone, "WeightVGProximity Modifier");
      need_transform_relation = true;
    }
    else if (wmd->mask_tex_mapping == MOD_DISP_MAP_GLOBAL) {
      need_transform_relation = true;
    }
  }

  if (need_transform_relation) {
    DEG_add_depends_on_transform_relation(ctx->node, "WeightVGProximity Modifier");
  }
}

static bool is_disabled(const Scene * /*scene*/, ModifierData *md, bool /*use_render_params*/)
{
  WeightVGProximityModifierData *wmd = (WeightVGProximityModifierData *)md;
  /* If no vertex group, bypass. */
  if (wmd->defgrp_name[0] == '\0') {
    return true;
  }
  /* If no target object, bypass. */
  return (wmd->proximity_ob_target == nullptr);
}

static Mesh *modify_mesh(ModifierData *md, const ModifierEvalContext *ctx, Mesh *mesh)
{
  BLI_assert(mesh != nullptr);

  WeightVGProximityModifierData *wmd = (WeightVGProximityModifierData *)md;
  MDeformWeight **dw, **tdw;
  Object *ob = ctx->object;
  Object *obr = nullptr; /* Our target object. */
  int defgrp_index;
  float *tw = nullptr;
  float *org_w = nullptr;
  float *new_w = nullptr;
  int *tidx, *indices = nullptr;
  int index_num = 0;
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
  const int verts_num = mesh->verts_num;

  /* Check if we can just return the original mesh.
   * Must have verts and therefore verts assigned to vgroups to do anything useful!
   */
  if ((verts_num == 0) || BLI_listbase_is_empty(&mesh->vertex_group_names)) {
    return mesh;
  }

  /* Get our target object. */
  obr = wmd->proximity_ob_target;
  if (obr == nullptr) {
    return mesh;
  }

  /* Get vgroup idx from its name. */
  defgrp_index = BKE_id_defgroup_name_index(&mesh->id, wmd->defgrp_name);
  if (defgrp_index == -1) {
    return mesh;
  }
  const bool has_mdef = !mesh->deform_verts().is_empty();
  /* If no vertices were ever added to an object's vgroup, dvert might be nullptr. */
  /* As this modifier never add vertices to vgroup, just return. */
  if (!has_mdef) {
    return mesh;
  }

  MDeformVert *dvert = mesh->deform_verts_for_write().data();
  /* Ultimate security check. */
  if (!dvert) {
    return mesh;
  }

  /* Find out which vertices to work on (all vertices in vgroup), and get their relevant weight. */
  tidx = MEM_malloc_arrayN<int>(size_t(verts_num), __func__);
  tw = MEM_malloc_arrayN<float>(size_t(verts_num), __func__);
  tdw = MEM_malloc_arrayN<MDeformWeight *>(size_t(verts_num), __func__);
  for (i = 0; i < verts_num; i++) {
    MDeformWeight *_dw = BKE_defvert_find_index(&dvert[i], defgrp_index);
    if (_dw) {
      tidx[index_num] = i;
      tw[index_num] = _dw->weight;
      tdw[index_num++] = _dw;
    }
  }
  /* If no vertices found, return org data! */
  if (index_num == 0) {
    MEM_freeN(tidx);
    MEM_freeN(tw);
    MEM_freeN(tdw);
    return mesh;
  }
  if (index_num != verts_num) {
    indices = MEM_malloc_arrayN<int>(size_t(index_num), __func__);
    memcpy(indices, tidx, sizeof(int) * index_num);
    org_w = MEM_malloc_arrayN<float>(size_t(index_num), __func__);
    memcpy(org_w, tw, sizeof(float) * index_num);
    dw = MEM_malloc_arrayN<MDeformWeight *>(size_t(index_num), __func__);
    memcpy(dw, tdw, sizeof(MDeformWeight *) * index_num);
    MEM_freeN(tw);
    MEM_freeN(tdw);
  }
  else {
    org_w = tw;
    dw = tdw;
  }
  new_w = MEM_malloc_arrayN<float>(size_t(index_num), __func__);
  MEM_freeN(tidx);

  const blender::Span<blender::float3> positions = mesh->vert_positions();

  /* Compute wanted distances. */
  if (wmd->proximity_mode == MOD_WVG_PROXIMITY_OBJECT) {
    const float dist = get_ob2ob_distance(ob, obr);
    for (i = 0; i < index_num; i++) {
      new_w[i] = dist;
    }
  }
  else if (wmd->proximity_mode == MOD_WVG_PROXIMITY_GEOMETRY) {
    const bool use_trgt_verts = (wmd->proximity_flags & MOD_WVG_PROXIMITY_GEOM_VERTS) != 0;
    const bool use_trgt_edges = (wmd->proximity_flags & MOD_WVG_PROXIMITY_GEOM_EDGES) != 0;
    const bool use_trgt_faces = (wmd->proximity_flags & MOD_WVG_PROXIMITY_GEOM_FACES) != 0;

    if (use_trgt_verts || use_trgt_edges || use_trgt_faces) {
      Mesh *target_mesh = BKE_modifier_get_evaluated_mesh_from_evaluated_object(obr);

      /* We must check that we do have a valid target_mesh! */
      if (target_mesh != nullptr) {

        /* TODO: edit-mode versions of the BVH lookup functions are available so it could be
         * avoided. */
        BKE_mesh_wrapper_ensure_mdata(target_mesh);

        SpaceTransform loc2trgt;
        float *dists_v = use_trgt_verts ? MEM_malloc_arrayN<float>(size_t(index_num), __func__) :
                                          nullptr;
        float *dists_e = use_trgt_edges ? MEM_malloc_arrayN<float>(size_t(index_num), __func__) :
                                          nullptr;
        float *dists_f = use_trgt_faces ? MEM_malloc_arrayN<float>(size_t(index_num), __func__) :
                                          nullptr;

        BLI_SPACE_TRANSFORM_SETUP(&loc2trgt, ob, obr);
        get_vert2geom_distance(
            index_num, positions, indices, dists_v, dists_e, dists_f, target_mesh, &loc2trgt);
        for (i = 0; i < index_num; i++) {
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
        get_vert2ob_distance(index_num, positions, indices, new_w, ob, obr);
      }
    }
    else {
      get_vert2ob_distance(index_num, positions, indices, new_w, ob, obr);
    }
  }

  /* Map distances to weights. */
  do_map(ob,
         new_w,
         index_num,
         wmd->min_dist,
         wmd->max_dist,
         wmd->falloff_type,
         (wmd->proximity_flags & MOD_WVG_PROXIMITY_INVERT_FALLOFF) != 0,
         wmd->cmap_curve);

  /* Do masking. */
  Scene *scene = DEG_get_evaluated_scene(ctx->depsgraph);
  weightvg_do_mask(ctx,
                   index_num,
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
      dvert, defgrp_index, dw, index_num, indices, org_w, false, 0.0f, false, 0.0f, do_normalize);

  /* If weight preview enabled... */
#if 0 /* XXX Currently done in mod stack :/ */
  if (do_prev) {
    DM_update_weight_mcol(ob, dm, 0, org_w, index_num, indices);
  }
#endif

  /* Freeing stuff. */
  MEM_freeN(org_w);
  MEM_freeN(new_w);
  MEM_freeN(dw);
  MEM_SAFE_FREE(indices);

#ifdef USE_TIMEIT
  TIMEIT_END(perf);
#endif

  mesh->runtime->is_original_bmesh = false;

  /* Return the vgroup-modified mesh. */
  return mesh;
}

static void panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *col;
  uiLayout *layout = panel->layout;

  PointerRNA ob_ptr;
  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, &ob_ptr);

  layout->use_property_split_set(true);

  layout->prop_search(
      ptr, "vertex_group", &ob_ptr, "vertex_groups", std::nullopt, ICON_GROUP_VERTEX);

  layout->prop(ptr, "target", UI_ITEM_NONE, std::nullopt, ICON_NONE);

  layout->separator();

  layout->prop(ptr, "proximity_mode", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  if (RNA_enum_get(ptr, "proximity_mode") == MOD_WVG_PROXIMITY_GEOMETRY) {
    layout->prop(ptr, "proximity_geometry", UI_ITEM_R_EXPAND, IFACE_("Geometry"), ICON_NONE);
  }

  col = &layout->column(true);
  col->prop(ptr, "min_dist", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  col->prop(ptr, "max_dist", UI_ITEM_NONE, std::nullopt, ICON_NONE);

  layout->prop(ptr, "normalize", UI_ITEM_NONE, std::nullopt, ICON_NONE);
}

static void falloff_panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *row, *sub;
  uiLayout *layout = panel->layout;

  PointerRNA ob_ptr;
  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, &ob_ptr);

  layout->use_property_split_set(true);

  row = &layout->row(true);
  row->prop(ptr, "falloff_type", UI_ITEM_NONE, IFACE_("Type"), ICON_NONE);
  sub = &row->row(true);
  sub->use_property_split_set(false);
  row->prop(ptr, "invert_falloff", UI_ITEM_NONE, "", ICON_ARROW_LEFTRIGHT);
  if (RNA_enum_get(ptr, "falloff_type") == MOD_WVG_MAPPING_CURVE) {
    uiTemplateCurveMapping(layout, ptr, "map_curve", 0, false, false, false, false, false);
  }
  modifier_error_message_draw(layout, ptr);
}

static void influence_panel_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA ob_ptr;
  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, &ob_ptr);

  weightvg_ui_common(C, &ob_ptr, ptr, layout);
}

static void panel_register(ARegionType *region_type)
{
  PanelType *panel_type = modifier_panel_register(
      region_type, eModifierType_WeightVGProximity, panel_draw);
  modifier_subpanel_register(
      region_type, "falloff", "Falloff", nullptr, falloff_panel_draw, panel_type);
  modifier_subpanel_register(
      region_type, "influence", "Influence", nullptr, influence_panel_draw, panel_type);
}

static void blend_write(BlendWriter *writer, const ID * /*id_owner*/, const ModifierData *md)
{
  const WeightVGProximityModifierData *wmd = (const WeightVGProximityModifierData *)md;

  BLO_write_struct(writer, WeightVGProximityModifierData, wmd);

  if (wmd->cmap_curve) {
    BKE_curvemapping_blend_write(writer, wmd->cmap_curve);
  }
}

static void blend_read(BlendDataReader *reader, ModifierData *md)
{
  WeightVGProximityModifierData *wmd = (WeightVGProximityModifierData *)md;

  BLO_read_struct(reader, CurveMapping, &wmd->cmap_curve);
  if (wmd->cmap_curve) {
    BKE_curvemapping_blend_read(reader, wmd->cmap_curve);
  }
}

ModifierTypeInfo modifierType_WeightVGProximity = {
    /*idname*/ "VertexWeightProximity",
    /*name*/ N_("VertexWeightProximity"),
    /*struct_name*/ "WeightVGProximityModifierData",
    /*struct_size*/ sizeof(WeightVGProximityModifierData),
    /*srna*/ &RNA_VertexWeightProximityModifier,
    /*type*/ ModifierTypeType::NonGeometrical,
    /*flags*/ eModifierTypeFlag_AcceptsMesh | eModifierTypeFlag_SupportsMapping |
        eModifierTypeFlag_SupportsEditmode,
    /*icon*/ ICON_MOD_VERTEX_WEIGHT,

    /*copy_data*/ copy_data,

    /*deform_verts*/ nullptr,
    /*deform_matrices*/ nullptr,
    /*deform_verts_EM*/ nullptr,
    /*deform_matrices_EM*/ nullptr,
    /*modify_mesh*/ modify_mesh,
    /*modify_geometry_set*/ nullptr,

    /*init_data*/ init_data,
    /*required_data_mask*/ required_data_mask,
    /*free_data*/ free_data,
    /*is_disabled*/ is_disabled,
    /*update_depsgraph*/ update_depsgraph,
    /*depends_on_time*/ depends_on_time,
    /*depends_on_normals*/ nullptr,
    /*foreach_ID_link*/ foreach_ID_link,
    /*foreach_tex_link*/ foreach_tex_link,
    /*free_runtime_data*/ nullptr,
    /*panel_register*/ panel_register,
    /*blend_write*/ blend_write,
    /*blend_read*/ blend_read,
    /*foreach_cache*/ nullptr,
    /*foreach_working_space_color*/ nullptr,
};
