/* SPDX-FileCopyrightText: 2017 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 *
 * \brief Mesh API for render engines
 */

#include <optional>

#include "MEM_guardedalloc.h"

#include "BLI_bitmap.h"
#include "BLI_buffer.h"
#include "BLI_edgehash.h"
#include "BLI_index_range.hh"
#include "BLI_listbase.h"
#include "BLI_map.hh"
#include "BLI_math_bits.h"
#include "BLI_math_vector.h"
#include "BLI_span.hh"
#include "BLI_string.h"
#include "BLI_string_ref.hh"
#include "BLI_task.h"
#include "BLI_utildefines.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_attribute.h"
#include "BKE_customdata.h"
#include "BKE_deform.h"
#include "BKE_editmesh.h"
#include "BKE_editmesh_cache.h"
#include "BKE_editmesh_tangent.h"
#include "BKE_mesh.hh"
#include "BKE_mesh_runtime.h"
#include "BKE_mesh_tangent.h"
#include "BKE_modifier.h"
#include "BKE_object_deform.h"
#include "BKE_paint.h"
#include "BKE_pbvh_api.hh"
#include "BKE_subdiv_modifier.h"
#include "BKE_sculpt.h" //NotForPR

#include "atomic_ops.h"

#include "bmesh.h"

#include "GPU_batch.h"
#include "GPU_material.h"

#include "DRW_render.h"

#include "ED_mesh.h"
#include "ED_uvedit.h"

#include "draw_cache_extract.hh"
#include "draw_cache_inline.h"
#include "draw_subdivision.h"

#include "draw_cache_impl.h" /* own include */
#include "draw_manager.h"

#include "mesh_extractors/extract_mesh.hh"

using blender::IndexRange;
using blender::Map;
using blender::Span;
using blender::StringRefNull;

/* ---------------------------------------------------------------------- */
/** \name Dependencies between buffer and batch
 * \{ */

/* clang-format off */

#define BUFFER_INDEX(buff_name) ((offsetof(MeshBufferList, buff_name) - offsetof(MeshBufferList, vbo)) / sizeof(void *))
#define BUFFER_LEN (sizeof(MeshBufferList) / sizeof(void *))

#define _BATCH_MAP1(a) batches_that_use_buffer(BUFFER_INDEX(a))
#define _BATCH_MAP2(a, b) _BATCH_MAP1(a) | _BATCH_MAP1(b)
#define _BATCH_MAP3(a, b, c) _BATCH_MAP2(a, b) | _BATCH_MAP1(c)
#define _BATCH_MAP4(a, b, c, d) _BATCH_MAP3(a, b, c) | _BATCH_MAP1(d)
#define _BATCH_MAP5(a, b, c, d, e) _BATCH_MAP4(a, b, c, d) | _BATCH_MAP1(e)
#define _BATCH_MAP6(a, b, c, d, e, f) _BATCH_MAP5(a, b, c, d, e) | _BATCH_MAP1(f)
#define _BATCH_MAP7(a, b, c, d, e, f, g) _BATCH_MAP6(a, b, c, d, e, f) | _BATCH_MAP1(g)
#define _BATCH_MAP8(a, b, c, d, e, f, g, h) _BATCH_MAP7(a, b, c, d, e, f, g) | _BATCH_MAP1(h)
#define _BATCH_MAP9(a, b, c, d, e, f, g, h, i) _BATCH_MAP8(a, b, c, d, e, f, g, h) | _BATCH_MAP1(i)
#define _BATCH_MAP10(a, b, c, d, e, f, g, h, i, j) _BATCH_MAP9(a, b, c, d, e, f, g, h, i) | _BATCH_MAP1(j)

#define BATCH_MAP(...) VA_NARGS_CALL_OVERLOAD(_BATCH_MAP, __VA_ARGS__)

/* clang-format on */

#define TRIS_PER_MAT_INDEX BUFFER_LEN

static constexpr DRWBatchFlag batches_that_use_buffer(const int buffer_index)
{
  switch (buffer_index) {
    case BUFFER_INDEX(vbo.pos_nor):
      return MBC_SURFACE | MBC_SURFACE_WEIGHTS | MBC_EDIT_TRIANGLES | MBC_EDIT_VERTICES |
             MBC_EDIT_EDGES | MBC_EDIT_VNOR | MBC_EDIT_LNOR | MBC_EDIT_MESH_ANALYSIS |
             MBC_EDIT_SELECTION_VERTS | MBC_EDIT_SELECTION_EDGES | MBC_EDIT_SELECTION_FACES |
             MBC_ALL_VERTS | MBC_ALL_EDGES | MBC_LOOSE_EDGES | MBC_EDGE_DETECTION |
             MBC_WIRE_EDGES | MBC_WIRE_LOOPS | MBC_SCULPT_OVERLAYS | MBC_VIEWER_ATTRIBUTE_OVERLAY |
             MBC_SURFACE_PER_MAT;
    case BUFFER_INDEX(vbo.lnor):
      return MBC_SURFACE | MBC_EDIT_LNOR | MBC_WIRE_LOOPS | MBC_SURFACE_PER_MAT;
    case BUFFER_INDEX(vbo.edge_fac):
      return MBC_WIRE_EDGES;
    case BUFFER_INDEX(vbo.weights):
      return MBC_SURFACE_WEIGHTS;
    case BUFFER_INDEX(vbo.uv):
      return MBC_SURFACE | MBC_EDITUV_FACES_STRETCH_AREA | MBC_EDITUV_FACES_STRETCH_ANGLE |
             MBC_EDITUV_FACES | MBC_EDITUV_EDGES | MBC_EDITUV_VERTS | MBC_WIRE_LOOPS_UVS |
             MBC_SURFACE_PER_MAT;
    case BUFFER_INDEX(vbo.tan):
      return MBC_SURFACE_PER_MAT;
    case BUFFER_INDEX(vbo.sculpt_data):
      return MBC_SCULPT_OVERLAYS;
    case BUFFER_INDEX(vbo.orco):
      return MBC_SURFACE_PER_MAT;
    case BUFFER_INDEX(vbo.edit_data):
      return MBC_EDIT_TRIANGLES | MBC_EDIT_EDGES | MBC_EDIT_VERTICES;
    case BUFFER_INDEX(vbo.edituv_data):
      return MBC_EDITUV_FACES | MBC_EDITUV_FACES_STRETCH_AREA | MBC_EDITUV_FACES_STRETCH_ANGLE |
             MBC_EDITUV_EDGES | MBC_EDITUV_VERTS;
    case BUFFER_INDEX(vbo.edituv_stretch_area):
      return MBC_EDITUV_FACES_STRETCH_AREA;
    case BUFFER_INDEX(vbo.edituv_stretch_angle):
      return MBC_EDITUV_FACES_STRETCH_ANGLE;
    case BUFFER_INDEX(vbo.mesh_analysis):
      return MBC_EDIT_MESH_ANALYSIS;
    case BUFFER_INDEX(vbo.fdots_pos):
      return MBC_EDIT_FACEDOTS | MBC_EDIT_SELECTION_FACEDOTS;
    case BUFFER_INDEX(vbo.fdots_nor):
      return MBC_EDIT_FACEDOTS;
    case BUFFER_INDEX(vbo.fdots_uv):
      return MBC_EDITUV_FACEDOTS;
    case BUFFER_INDEX(vbo.fdots_edituv_data):
      return MBC_EDITUV_FACEDOTS;
    case BUFFER_INDEX(vbo.skin_roots):
      return MBC_SKIN_ROOTS;
    case BUFFER_INDEX(vbo.vert_idx):
      return MBC_EDIT_SELECTION_VERTS;
    case BUFFER_INDEX(vbo.edge_idx):
      return MBC_EDIT_SELECTION_EDGES;
    case BUFFER_INDEX(vbo.poly_idx):
      return MBC_EDIT_SELECTION_FACES;
    case BUFFER_INDEX(vbo.fdot_idx):
      return MBC_EDIT_SELECTION_FACEDOTS;
    case BUFFER_INDEX(vbo.attr[0]):
    case BUFFER_INDEX(vbo.attr[1]):
    case BUFFER_INDEX(vbo.attr[2]):
    case BUFFER_INDEX(vbo.attr[3]):
    case BUFFER_INDEX(vbo.attr[4]):
    case BUFFER_INDEX(vbo.attr[5]):
    case BUFFER_INDEX(vbo.attr[6]):
    case BUFFER_INDEX(vbo.attr[7]):
    case BUFFER_INDEX(vbo.attr[8]):
    case BUFFER_INDEX(vbo.attr[9]):
    case BUFFER_INDEX(vbo.attr[10]):
    case BUFFER_INDEX(vbo.attr[11]):
    case BUFFER_INDEX(vbo.attr[12]):
    case BUFFER_INDEX(vbo.attr[13]):
    case BUFFER_INDEX(vbo.attr[14]):
      return MBC_SURFACE | MBC_SURFACE_PER_MAT;
    case BUFFER_INDEX(vbo.attr_viewer):
      return MBC_VIEWER_ATTRIBUTE_OVERLAY;
    case BUFFER_INDEX(ibo.tris):
      return MBC_SURFACE | MBC_SURFACE_WEIGHTS | MBC_EDIT_TRIANGLES | MBC_EDIT_LNOR |
             MBC_EDIT_MESH_ANALYSIS | MBC_EDIT_SELECTION_FACES | MBC_SCULPT_OVERLAYS |
             MBC_VIEWER_ATTRIBUTE_OVERLAY;
    case BUFFER_INDEX(ibo.lines):
      return MBC_EDIT_EDGES | MBC_EDIT_SELECTION_EDGES | MBC_ALL_EDGES | MBC_WIRE_EDGES;
    case BUFFER_INDEX(ibo.lines_loose):
      return MBC_LOOSE_EDGES;
    case BUFFER_INDEX(ibo.points):
      return MBC_EDIT_VNOR | MBC_EDIT_VERTICES | MBC_EDIT_SELECTION_VERTS;
    case BUFFER_INDEX(ibo.fdots):
      return MBC_EDIT_FACEDOTS | MBC_EDIT_SELECTION_FACEDOTS;
    case BUFFER_INDEX(ibo.lines_paint_mask):
      return MBC_WIRE_LOOPS;
    case BUFFER_INDEX(ibo.lines_adjacency):
      return MBC_EDGE_DETECTION;
    case BUFFER_INDEX(ibo.edituv_tris):
      return MBC_EDITUV_FACES | MBC_EDITUV_FACES_STRETCH_AREA | MBC_EDITUV_FACES_STRETCH_ANGLE;
    case BUFFER_INDEX(ibo.edituv_lines):
      return MBC_EDITUV_EDGES | MBC_WIRE_LOOPS_UVS;
    case BUFFER_INDEX(ibo.edituv_points):
      return MBC_EDITUV_VERTS;
    case BUFFER_INDEX(ibo.edituv_fdots):
      return MBC_EDITUV_FACEDOTS;
    case TRIS_PER_MAT_INDEX:
      return MBC_SURFACE_PER_MAT;
  }
  return (DRWBatchFlag)0;
}

static void mesh_batch_cache_discard_surface_batches(MeshBatchCache *cache);
static void mesh_batch_cache_clear(MeshBatchCache *cache);

static void mesh_batch_cache_discard_batch(MeshBatchCache *cache, const DRWBatchFlag batch_map)
{
  for (int i = 0; i < MBC_BATCH_LEN; i++) {
    DRWBatchFlag batch_requested = (DRWBatchFlag)(1u << i);
    if (batch_map & batch_requested) {
      GPU_BATCH_DISCARD_SAFE(((GPUBatch **)&cache->batch)[i]);
      cache->batch_ready &= ~batch_requested;
    }
  }

  if (batch_map & MBC_SURFACE_PER_MAT) {
    mesh_batch_cache_discard_surface_batches(cache);
  }
}

/* Return true is all layers in _b_ are inside _a_. */
BLI_INLINE bool mesh_cd_layers_type_overlap(DRW_MeshCDMask a, DRW_MeshCDMask b)
{
  return (*((uint32_t *)&a) & *((uint32_t *)&b)) == *((uint32_t *)&b);
}

BLI_INLINE bool mesh_cd_layers_type_equal(DRW_MeshCDMask a, DRW_MeshCDMask b)
{
  return *((uint32_t *)&a) == *((uint32_t *)&b);
}

BLI_INLINE void mesh_cd_layers_type_merge(DRW_MeshCDMask *a, DRW_MeshCDMask b)
{
  uint32_t *a_p = (uint32_t *)a;
  uint32_t *b_p = (uint32_t *)&b;
  atomic_fetch_and_or_uint32(a_p, *b_p);
}

BLI_INLINE void mesh_cd_layers_type_clear(DRW_MeshCDMask *a)
{
  *((uint32_t *)a) = 0;
}

static void mesh_cd_calc_edit_uv_layer(const Mesh * /*me*/, DRW_MeshCDMask *cd_used)
{
  cd_used->edit_uv = 1;
}

static void mesh_cd_calc_active_uv_layer(const Object *object,
                                         const Mesh *me,
                                         DRW_MeshCDMask *cd_used)
{
  const Mesh *me_final = editmesh_final_or_this(object, me);
  const CustomData *cd_ldata = mesh_cd_ldata_get_from_mesh(me_final);
  int layer = CustomData_get_active_layer(cd_ldata, CD_PROP_FLOAT2);
  if (layer != -1) {
    cd_used->uv |= (1 << layer);
  }
}

static void mesh_cd_calc_active_mask_uv_layer(const Object *object,
                                              const Mesh *me,
                                              DRW_MeshCDMask *cd_used)
{
  const Mesh *me_final = editmesh_final_or_this(object, me);
  const CustomData *cd_ldata = mesh_cd_ldata_get_from_mesh(me_final);
  int layer = CustomData_get_stencil_layer(cd_ldata, CD_PROP_FLOAT2);
  if (layer != -1) {
    cd_used->uv |= (1 << layer);
  }
}

static DRW_MeshCDMask mesh_cd_calc_used_gpu_layers(const Object *object,
                                                   const Mesh *me,
                                                   GPUMaterial **gpumat_array,
                                                   int gpumat_array_len,
                                                   DRW_Attributes *attributes)
{
  const Mesh *me_final = editmesh_final_or_this(object, me);
  const CustomData *cd_ldata = mesh_cd_ldata_get_from_mesh(me_final);
  const CustomData *cd_pdata = mesh_cd_pdata_get_from_mesh(me_final);
  const CustomData *cd_vdata = mesh_cd_vdata_get_from_mesh(me_final);
  const CustomData *cd_edata = mesh_cd_edata_get_from_mesh(me_final);

  /* See: DM_vertex_attributes_from_gpu for similar logic */
  DRW_MeshCDMask cd_used;
  mesh_cd_layers_type_clear(&cd_used);

  const StringRefNull default_color_name = me_final->default_color_attribute ?
                                               me_final->default_color_attribute :
                                               "";

  for (int i = 0; i < gpumat_array_len; i++) {
    GPUMaterial *gpumat = gpumat_array[i];
    if (gpumat == nullptr) {
      continue;
    }
    ListBase gpu_attrs = GPU_material_attributes(gpumat);
    LISTBASE_FOREACH (GPUMaterialAttribute *, gpu_attr, &gpu_attrs) {
      const char *name = gpu_attr->name;
      eCustomDataType type = static_cast<eCustomDataType>(gpu_attr->type);
      int layer = -1;
      std::optional<eAttrDomain> domain;

      if (gpu_attr->is_default_color) {
        name = default_color_name.c_str();
      }

      if (type == CD_AUTO_FROM_NAME) {
        /* We need to deduce what exact layer is used.
         *
         * We do it based on the specified name.
         */
        if (name[0] != '\0') {
          layer = CustomData_get_named_layer(cd_ldata, CD_PROP_FLOAT2, name);
          type = CD_MTFACE;

#if 0 /* Tangents are always from UVs - this will never happen. */
            if (layer == -1) {
              layer = CustomData_get_named_layer(cd_ldata, CD_TANGENT, name);
              type = CD_TANGENT;
            }
#endif
          if (layer == -1) {
            /* Try to match a generic attribute, we use the first attribute domain with a
             * matching name. */
            if (drw_custom_data_match_attribute(cd_vdata, name, &layer, &type)) {
              domain = ATTR_DOMAIN_POINT;
            }
            else if (drw_custom_data_match_attribute(cd_ldata, name, &layer, &type)) {
              domain = ATTR_DOMAIN_CORNER;
            }
            else if (drw_custom_data_match_attribute(cd_pdata, name, &layer, &type)) {
              domain = ATTR_DOMAIN_FACE;
            }
            else if (drw_custom_data_match_attribute(cd_edata, name, &layer, &type)) {
              domain = ATTR_DOMAIN_EDGE;
            }
            else {
              layer = -1;
            }
          }

          if (layer == -1) {
            continue;
          }
        }
        else {
          /* Fall back to the UV layer, which matches old behavior. */
          type = CD_MTFACE;
        }
      }

      switch (type) {
        case CD_MTFACE: {
          if (layer == -1) {
            layer = (name[0] != '\0') ?
                        CustomData_get_named_layer(cd_ldata, CD_PROP_FLOAT2, name) :
                        CustomData_get_render_layer(cd_ldata, CD_PROP_FLOAT2);
          }
          if (layer != -1 && !CustomData_layer_is_anonymous(cd_ldata, CD_PROP_FLOAT2, layer)) {
            cd_used.uv |= (1 << layer);
          }
          break;
        }
        case CD_TANGENT: {
          if (layer == -1) {
            layer = (name[0] != '\0') ?
                        CustomData_get_named_layer(cd_ldata, CD_PROP_FLOAT2, name) :
                        CustomData_get_render_layer(cd_ldata, CD_PROP_FLOAT2);

            /* Only fallback to orco (below) when we have no UV layers, see: #56545 */
            if (layer == -1 && name[0] != '\0') {
              layer = CustomData_get_render_layer(cd_ldata, CD_PROP_FLOAT2);
            }
          }
          if (layer != -1) {
            cd_used.tan |= (1 << layer);
          }
          else {
            /* no UV layers at all => requesting orco */
            cd_used.tan_orco = 1;
            cd_used.orco = 1;
          }
          break;
        }

        case CD_ORCO: {
          cd_used.orco = 1;
          break;
        }
        case CD_PROP_BYTE_COLOR:
        case CD_PROP_COLOR:
        case CD_PROP_QUATERNION:
        case CD_PROP_FLOAT3:
        case CD_PROP_BOOL:
        case CD_PROP_INT8:
        case CD_PROP_INT32:
        case CD_PROP_INT32_2D:
        case CD_PROP_FLOAT:
        case CD_PROP_FLOAT2: {
          if (layer != -1 && domain.has_value()) {
            drw_attributes_add_request(attributes, name, type, layer, *domain);
          }
          break;
        }
        default:
          break;
      }
    }
  }
  return cd_used;
}

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Vertex Group Selection
 * \{ */

/** Reset the selection structure, deallocating heap memory as appropriate. */
static void drw_mesh_weight_state_clear(DRW_MeshWeightState *wstate)
{
  MEM_SAFE_FREE(wstate->defgroup_sel);
  MEM_SAFE_FREE(wstate->defgroup_locked);
  MEM_SAFE_FREE(wstate->defgroup_unlocked);

  memset(wstate, 0, sizeof(*wstate));

  wstate->defgroup_active = -1;
}

/** Copy selection data from one structure to another, including heap memory. */
static void drw_mesh_weight_state_copy(DRW_MeshWeightState *wstate_dst,
                                       const DRW_MeshWeightState *wstate_src)
{
  MEM_SAFE_FREE(wstate_dst->defgroup_sel);
  MEM_SAFE_FREE(wstate_dst->defgroup_locked);
  MEM_SAFE_FREE(wstate_dst->defgroup_unlocked);

  memcpy(wstate_dst, wstate_src, sizeof(*wstate_dst));

  if (wstate_src->defgroup_sel) {
    wstate_dst->defgroup_sel = static_cast<bool *>(MEM_dupallocN(wstate_src->defgroup_sel));
  }
  if (wstate_src->defgroup_locked) {
    wstate_dst->defgroup_locked = static_cast<bool *>(MEM_dupallocN(wstate_src->defgroup_locked));
  }
  if (wstate_src->defgroup_unlocked) {
    wstate_dst->defgroup_unlocked = static_cast<bool *>(
        MEM_dupallocN(wstate_src->defgroup_unlocked));
  }
}

static bool drw_mesh_flags_equal(const bool *array1, const bool *array2, int size)
{
  return ((!array1 && !array2) ||
          (array1 && array2 && memcmp(array1, array2, size * sizeof(bool)) == 0));
}

/** Compare two selection structures. */
static bool drw_mesh_weight_state_compare(const DRW_MeshWeightState *a,
                                          const DRW_MeshWeightState *b)
{
  return a->defgroup_active == b->defgroup_active && a->defgroup_len == b->defgroup_len &&
         a->flags == b->flags && a->alert_mode == b->alert_mode &&
         a->defgroup_sel_count == b->defgroup_sel_count &&
         drw_mesh_flags_equal(a->defgroup_sel, b->defgroup_sel, a->defgroup_len) &&
         drw_mesh_flags_equal(a->defgroup_locked, b->defgroup_locked, a->defgroup_len) &&
         drw_mesh_flags_equal(a->defgroup_unlocked, b->defgroup_unlocked, a->defgroup_len);
}

static void drw_mesh_weight_state_extract(
    Object *ob, Mesh *me, const ToolSettings *ts, bool paint_mode, DRW_MeshWeightState *wstate)
{
  /* Extract complete vertex weight group selection state and mode flags. */
  memset(wstate, 0, sizeof(*wstate));

  wstate->defgroup_active = me->vertex_group_active_index - 1;
  wstate->defgroup_len = BLI_listbase_count(&me->vertex_group_names);

  wstate->alert_mode = ts->weightuser;

  if (paint_mode && ts->multipaint) {
    /* Multi-paint needs to know all selected bones, not just the active group.
     * This is actually a relatively expensive operation, but caching would be difficult. */
    wstate->defgroup_sel = BKE_object_defgroup_selected_get(
        ob, wstate->defgroup_len, &wstate->defgroup_sel_count);

    if (wstate->defgroup_sel_count > 1) {
      wstate->flags |= DRW_MESH_WEIGHT_STATE_MULTIPAINT |
                       (ts->auto_normalize ? DRW_MESH_WEIGHT_STATE_AUTO_NORMALIZE : 0);

      if (ME_USING_MIRROR_X_VERTEX_GROUPS(me)) {
        BKE_object_defgroup_mirror_selection(ob,
                                             wstate->defgroup_len,
                                             wstate->defgroup_sel,
                                             wstate->defgroup_sel,
                                             &wstate->defgroup_sel_count);
      }
    }
    /* With only one selected bone Multi-paint reverts to regular mode. */
    else {
      wstate->defgroup_sel_count = 0;
      MEM_SAFE_FREE(wstate->defgroup_sel);
    }
  }

  if (paint_mode && ts->wpaint_lock_relative) {
    /* Set of locked vertex groups for the lock relative mode. */
    wstate->defgroup_locked = BKE_object_defgroup_lock_flags_get(ob, wstate->defgroup_len);
    wstate->defgroup_unlocked = BKE_object_defgroup_validmap_get(ob, wstate->defgroup_len);

    /* Check that a deform group is active, and none of selected groups are locked. */
    if (BKE_object_defgroup_check_lock_relative(
            wstate->defgroup_locked, wstate->defgroup_unlocked, wstate->defgroup_active) &&
        BKE_object_defgroup_check_lock_relative_multi(wstate->defgroup_len,
                                                      wstate->defgroup_locked,
                                                      wstate->defgroup_sel,
                                                      wstate->defgroup_sel_count))
    {
      wstate->flags |= DRW_MESH_WEIGHT_STATE_LOCK_RELATIVE;

      /* Compute the set of locked and unlocked deform vertex groups. */
      BKE_object_defgroup_split_locked_validmap(wstate->defgroup_len,
                                                wstate->defgroup_locked,
                                                wstate->defgroup_unlocked,
                                                wstate->defgroup_locked, /* out */
                                                wstate->defgroup_unlocked);
    }
    else {
      MEM_SAFE_FREE(wstate->defgroup_unlocked);
      MEM_SAFE_FREE(wstate->defgroup_locked);
    }
  }
}

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Mesh GPUBatch Cache
 * \{ */

BLI_INLINE void mesh_batch_cache_add_request(MeshBatchCache *cache, DRWBatchFlag new_flag)
{
  atomic_fetch_and_or_uint32((uint32_t *)(&cache->batch_requested), *(uint32_t *)&new_flag);
}

/* GPUBatch cache management. */

static bool mesh_batch_cache_valid(Object *object, Mesh *me)
{
  MeshBatchCache *cache = static_cast<MeshBatchCache *>(me->runtime->batch_cache);

  if (cache == nullptr) {
    return false;
  }

  /* Note: PBVH draw data should not be checked here. */

  if (cache->is_editmode != (me->edit_mesh != nullptr)) {
    return false;
  }

  if (cache->is_dirty) {
    return false;
  }

  if (cache->mat_len != mesh_render_mat_len_get(object, me)) {
    return false;
  }

  return true;
}

static void mesh_batch_cache_init(Object *object, Mesh *me)
{
  if (!me->runtime->batch_cache) {
    me->runtime->batch_cache = MEM_new<MeshBatchCache>(__func__);
  }
  else {
    *static_cast<MeshBatchCache *>(me->runtime->batch_cache) = {};
  }
  MeshBatchCache *cache = static_cast<MeshBatchCache *>(me->runtime->batch_cache);

  cache->is_editmode = me->edit_mesh != nullptr;

  if (object->sculpt && object->sculpt->pbvh) {
    cache->pbvh_is_drawing = BKE_pbvh_is_drawing(object->sculpt->pbvh);
  }

  if (cache->is_editmode == false) {
    // cache->edge_len = mesh_render_edges_len_get(me);
    // cache->tri_len = mesh_render_looptri_len_get(me);
    // cache->poly_len = mesh_render_polys_len_get(me);
    // cache->vert_len = mesh_render_verts_len_get(me);
  }

  cache->mat_len = mesh_render_mat_len_get(object, me);
  cache->surface_per_mat = static_cast<GPUBatch **>(
      MEM_callocN(sizeof(*cache->surface_per_mat) * cache->mat_len, __func__));
  cache->tris_per_mat = static_cast<GPUIndexBuf **>(
      MEM_callocN(sizeof(*cache->tris_per_mat) * cache->mat_len, __func__));

  cache->is_dirty = false;
  cache->batch_ready = (DRWBatchFlag)0;
  cache->batch_requested = (DRWBatchFlag)0;

  drw_mesh_weight_state_clear(&cache->weight_state);
}

void DRW_mesh_batch_cache_validate(Object *object, Mesh *me)
{
  if (!mesh_batch_cache_valid(object, me)) {
    if (me->runtime->batch_cache) {
      mesh_batch_cache_clear(static_cast<MeshBatchCache *>(me->runtime->batch_cache));
    }
    mesh_batch_cache_init(object, me);
  }
}

static MeshBatchCache *mesh_batch_cache_get(Mesh *me)
{
  return static_cast<MeshBatchCache *>(me->runtime->batch_cache);
}

static void mesh_batch_cache_check_vertex_group(MeshBatchCache *cache,
                                                const DRW_MeshWeightState *wstate)
{
  if (!drw_mesh_weight_state_compare(&cache->weight_state, wstate)) {
    FOREACH_MESH_BUFFER_CACHE (cache, mbc) {
      GPU_VERTBUF_DISCARD_SAFE(mbc->buff.vbo.weights);
    }
    GPU_BATCH_CLEAR_SAFE(cache->batch.surface_weights);

    cache->batch_ready &= ~MBC_SURFACE_WEIGHTS;

    drw_mesh_weight_state_clear(&cache->weight_state);
  }
}

static void mesh_batch_cache_request_surface_batches(MeshBatchCache *cache)
{
  mesh_batch_cache_add_request(cache, MBC_SURFACE);
  DRW_batch_request(&cache->batch.surface);
  for (int i = 0; i < cache->mat_len; i++) {
    DRW_batch_request(&cache->surface_per_mat[i]);
  }
}

/* Free batches with material-mapped looptris.
 * NOTE: The updating of the indices buffers (#tris_per_mat) is handled in the extractors.
 * No need to discard they here. */
static void mesh_batch_cache_discard_surface_batches(MeshBatchCache *cache)
{
  GPU_BATCH_DISCARD_SAFE(cache->batch.surface);
  for (int i = 0; i < cache->mat_len; i++) {
    GPU_BATCH_DISCARD_SAFE(cache->surface_per_mat[i]);
  }
  cache->batch_ready &= ~MBC_SURFACE;
}

static void mesh_batch_cache_discard_shaded_tri(MeshBatchCache *cache)
{
  FOREACH_MESH_BUFFER_CACHE (cache, mbc) {
    GPU_VERTBUF_DISCARD_SAFE(mbc->buff.vbo.uv);
    GPU_VERTBUF_DISCARD_SAFE(mbc->buff.vbo.tan);
    GPU_VERTBUF_DISCARD_SAFE(mbc->buff.vbo.orco);
  }
  DRWBatchFlag batch_map = BATCH_MAP(vbo.uv, vbo.tan, vbo.orco);
  mesh_batch_cache_discard_batch(cache, batch_map);
  mesh_cd_layers_type_clear(&cache->cd_used);
}

static void mesh_batch_cache_discard_uvedit(MeshBatchCache *cache)
{
  FOREACH_MESH_BUFFER_CACHE (cache, mbc) {
    GPU_VERTBUF_DISCARD_SAFE(mbc->buff.vbo.edituv_stretch_angle);
    GPU_VERTBUF_DISCARD_SAFE(mbc->buff.vbo.edituv_stretch_area);
    GPU_VERTBUF_DISCARD_SAFE(mbc->buff.vbo.uv);
    GPU_VERTBUF_DISCARD_SAFE(mbc->buff.vbo.edituv_data);
    GPU_VERTBUF_DISCARD_SAFE(mbc->buff.vbo.fdots_uv);
    GPU_VERTBUF_DISCARD_SAFE(mbc->buff.vbo.fdots_edituv_data);
    GPU_INDEXBUF_DISCARD_SAFE(mbc->buff.ibo.edituv_tris);
    GPU_INDEXBUF_DISCARD_SAFE(mbc->buff.ibo.edituv_lines);
    GPU_INDEXBUF_DISCARD_SAFE(mbc->buff.ibo.edituv_points);
    GPU_INDEXBUF_DISCARD_SAFE(mbc->buff.ibo.edituv_fdots);
  }
  DRWBatchFlag batch_map = BATCH_MAP(vbo.edituv_stretch_angle,
                                     vbo.edituv_stretch_area,
                                     vbo.uv,
                                     vbo.edituv_data,
                                     vbo.fdots_uv,
                                     vbo.fdots_edituv_data,
                                     ibo.edituv_tris,
                                     ibo.edituv_lines,
                                     ibo.edituv_points,
                                     ibo.edituv_fdots);
  mesh_batch_cache_discard_batch(cache, batch_map);

  cache->tot_area = 0.0f;
  cache->tot_uv_area = 0.0f;

  cache->batch_ready &= ~MBC_EDITUV;

  /* We discarded the vbo.uv so we need to reset the cd_used flag. */
  cache->cd_used.uv = 0;
  cache->cd_used.edit_uv = 0;
}

static void mesh_batch_cache_discard_uvedit_select(MeshBatchCache *cache)
{
  FOREACH_MESH_BUFFER_CACHE (cache, mbc) {
    GPU_VERTBUF_DISCARD_SAFE(mbc->buff.vbo.edituv_data);
    GPU_VERTBUF_DISCARD_SAFE(mbc->buff.vbo.fdots_edituv_data);
    GPU_INDEXBUF_DISCARD_SAFE(mbc->buff.ibo.edituv_tris);
    GPU_INDEXBUF_DISCARD_SAFE(mbc->buff.ibo.edituv_lines);
    GPU_INDEXBUF_DISCARD_SAFE(mbc->buff.ibo.edituv_points);
    GPU_INDEXBUF_DISCARD_SAFE(mbc->buff.ibo.edituv_fdots);
  }
  DRWBatchFlag batch_map = BATCH_MAP(vbo.edituv_data,
                                     vbo.fdots_edituv_data,
                                     ibo.edituv_tris,
                                     ibo.edituv_lines,
                                     ibo.edituv_points,
                                     ibo.edituv_fdots);
  mesh_batch_cache_discard_batch(cache, batch_map);
}

void DRW_mesh_batch_cache_dirty_tag(Mesh *me, eMeshBatchDirtyMode mode)
{
  MeshBatchCache *cache = static_cast<MeshBatchCache *>(me->runtime->batch_cache);
  if (cache == nullptr) {
    return;
  }
  DRWBatchFlag batch_map;
  switch (mode) {
    case BKE_MESH_BATCH_DIRTY_SELECT:
      FOREACH_MESH_BUFFER_CACHE (cache, mbc) {
        GPU_VERTBUF_DISCARD_SAFE(mbc->buff.vbo.edit_data);
        GPU_VERTBUF_DISCARD_SAFE(mbc->buff.vbo.fdots_nor);
      }
      batch_map = BATCH_MAP(vbo.edit_data, vbo.fdots_nor);
      mesh_batch_cache_discard_batch(cache, batch_map);

      /* Because visible UVs depends on edit mode selection, discard topology. */
      mesh_batch_cache_discard_uvedit_select(cache);
      break;
    case BKE_MESH_BATCH_DIRTY_SELECT_PAINT:
      /* Paint mode selection flag is packed inside the nor attribute.
       * Note that it can be slow if auto smooth is enabled. (see #63946) */
      FOREACH_MESH_BUFFER_CACHE (cache, mbc) {
        GPU_INDEXBUF_DISCARD_SAFE(mbc->buff.ibo.lines_paint_mask);
        GPU_VERTBUF_DISCARD_SAFE(mbc->buff.vbo.pos_nor);
        GPU_VERTBUF_DISCARD_SAFE(mbc->buff.vbo.lnor);
      }
      batch_map = BATCH_MAP(ibo.lines_paint_mask, vbo.pos_nor, vbo.lnor);
      mesh_batch_cache_discard_batch(cache, batch_map);
      break;
    case BKE_MESH_BATCH_DIRTY_ALL:
      cache->is_dirty = true;
      break;
    case BKE_MESH_BATCH_DIRTY_SHADING:
      mesh_batch_cache_discard_shaded_tri(cache);
      mesh_batch_cache_discard_uvedit(cache);
      break;
    case BKE_MESH_BATCH_DIRTY_UVEDIT_ALL:
      mesh_batch_cache_discard_uvedit(cache);
      break;
    case BKE_MESH_BATCH_DIRTY_UVEDIT_SELECT:
      FOREACH_MESH_BUFFER_CACHE (cache, mbc) {
        GPU_VERTBUF_DISCARD_SAFE(mbc->buff.vbo.edituv_data);
        GPU_VERTBUF_DISCARD_SAFE(mbc->buff.vbo.fdots_edituv_data);
      }
      batch_map = BATCH_MAP(vbo.edituv_data, vbo.fdots_edituv_data);
      mesh_batch_cache_discard_batch(cache, batch_map);
      break;
    default:
      BLI_assert(0);
  }
}

static void mesh_buffer_list_clear(MeshBufferList *mbuflist)
{
  GPUVertBuf **vbos = (GPUVertBuf **)&mbuflist->vbo;
  GPUIndexBuf **ibos = (GPUIndexBuf **)&mbuflist->ibo;
  for (int i = 0; i < sizeof(mbuflist->vbo) / sizeof(void *); i++) {
    GPU_VERTBUF_DISCARD_SAFE(vbos[i]);
  }
  for (int i = 0; i < sizeof(mbuflist->ibo) / sizeof(void *); i++) {
    GPU_INDEXBUF_DISCARD_SAFE(ibos[i]);
  }
}

static void mesh_buffer_cache_clear(MeshBufferCache *mbc)
{
  mesh_buffer_list_clear(&mbc->buff);

  mbc->loose_geom = {};
  mbc->poly_sorted = {};
}

static void mesh_batch_cache_free_subdiv_cache(MeshBatchCache *cache)
{
  if (cache->subdiv_cache) {
    draw_subdiv_cache_free(cache->subdiv_cache);
    MEM_freeN(cache->subdiv_cache);
    cache->subdiv_cache = nullptr;
  }
}

static void mesh_batch_cache_clear(MeshBatchCache *cache)
{
  FOREACH_MESH_BUFFER_CACHE (cache, mbc) {
    mesh_buffer_cache_clear(mbc);
  }

  for (int i = 0; i < cache->mat_len; i++) {
    GPU_INDEXBUF_DISCARD_SAFE(cache->tris_per_mat[i]);
  }
  MEM_SAFE_FREE(cache->tris_per_mat);

  for (int i = 0; i < sizeof(cache->batch) / sizeof(void *); i++) {
    GPUBatch **batch = (GPUBatch **)&cache->batch;
    GPU_BATCH_DISCARD_SAFE(batch[i]);
  }

  mesh_batch_cache_discard_shaded_tri(cache);
  mesh_batch_cache_discard_uvedit(cache);
  MEM_SAFE_FREE(cache->surface_per_mat);
  cache->mat_len = 0;

  cache->batch_ready = (DRWBatchFlag)0;
  drw_mesh_weight_state_clear(&cache->weight_state);

  mesh_batch_cache_free_subdiv_cache(cache);
}

void DRW_mesh_batch_cache_free(void *batch_cache)
{
  MeshBatchCache *cache = static_cast<MeshBatchCache *>(batch_cache);
  mesh_batch_cache_clear(cache);
  MEM_delete(cache);
}

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Public API
 * \{ */

static void texpaint_request_active_uv(MeshBatchCache *cache, Object *object, Mesh *me)
{
  DRW_MeshCDMask cd_needed;
  mesh_cd_layers_type_clear(&cd_needed);
  mesh_cd_calc_active_uv_layer(object, me, &cd_needed);

  BLI_assert(cd_needed.uv != 0 &&
             "No uv layer available in texpaint, but batches requested anyway!");

  mesh_cd_calc_active_mask_uv_layer(object, me, &cd_needed);
  mesh_cd_layers_type_merge(&cache->cd_needed, cd_needed);
}

static void request_active_and_default_color_attributes(const Object &object,
                                                        const Mesh &mesh,
                                                        DRW_Attributes &attributes)
{
  const Mesh *me_final = editmesh_final_or_this(&object, &mesh);
  const CustomData *cd_vdata = mesh_cd_vdata_get_from_mesh(me_final);
  const CustomData *cd_ldata = mesh_cd_ldata_get_from_mesh(me_final);

  auto request_color_attribute = [&](const char *name) {
    if (name) {
      int layer_index;
      eCustomDataType type;
      if (drw_custom_data_match_attribute(cd_vdata, name, &layer_index, &type)) {
        drw_attributes_add_request(&attributes, name, type, layer_index, ATTR_DOMAIN_POINT);
      }
      else if (drw_custom_data_match_attribute(cd_ldata, name, &layer_index, &type)) {
        drw_attributes_add_request(&attributes, name, type, layer_index, ATTR_DOMAIN_CORNER);
      }
    }
  };

  request_color_attribute(me_final->active_color_attribute);
  request_color_attribute(me_final->default_color_attribute);
}

GPUBatch *DRW_mesh_batch_cache_get_all_verts(Mesh *me)
{
  MeshBatchCache *cache = mesh_batch_cache_get(me);
  mesh_batch_cache_add_request(cache, MBC_ALL_VERTS);
  return DRW_batch_request(&cache->batch.all_verts);
}

GPUBatch *DRW_mesh_batch_cache_get_all_edges(Mesh *me)
{
  MeshBatchCache *cache = mesh_batch_cache_get(me);
  mesh_batch_cache_add_request(cache, MBC_ALL_EDGES);
  return DRW_batch_request(&cache->batch.all_edges);
}

GPUBatch *DRW_mesh_batch_cache_get_surface(Mesh *me)
{
  MeshBatchCache *cache = mesh_batch_cache_get(me);
  mesh_batch_cache_request_surface_batches(cache);

  return cache->batch.surface;
}

GPUBatch *DRW_mesh_batch_cache_get_loose_edges(Mesh *me)
{
  MeshBatchCache *cache = mesh_batch_cache_get(me);
  mesh_batch_cache_add_request(cache, MBC_LOOSE_EDGES);
  if (cache->no_loose_wire) {
    return nullptr;
  }

  return DRW_batch_request(&cache->batch.loose_edges);
}

GPUBatch *DRW_mesh_batch_cache_get_surface_weights(Mesh *me)
{
  MeshBatchCache *cache = mesh_batch_cache_get(me);
  mesh_batch_cache_add_request(cache, MBC_SURFACE_WEIGHTS);
  return DRW_batch_request(&cache->batch.surface_weights);
}

GPUBatch *DRW_mesh_batch_cache_get_edge_detection(Mesh *me, bool *r_is_manifold)
{
  MeshBatchCache *cache = mesh_batch_cache_get(me);
  mesh_batch_cache_add_request(cache, MBC_EDGE_DETECTION);
  /* Even if is_manifold is not correct (not updated),
   * the default (not manifold) is just the worst case. */
  if (r_is_manifold) {
    *r_is_manifold = cache->is_manifold;
  }
  return DRW_batch_request(&cache->batch.edge_detection);
}

GPUBatch *DRW_mesh_batch_cache_get_wireframes_face(Mesh *me)
{
  MeshBatchCache *cache = mesh_batch_cache_get(me);
  mesh_batch_cache_add_request(cache, MBC_WIRE_EDGES);
  return DRW_batch_request(&cache->batch.wire_edges);
}

GPUBatch *DRW_mesh_batch_cache_get_edit_mesh_analysis(Mesh *me)
{
  MeshBatchCache *cache = mesh_batch_cache_get(me);
  mesh_batch_cache_add_request(cache, MBC_EDIT_MESH_ANALYSIS);
  return DRW_batch_request(&cache->batch.edit_mesh_analysis);
}

void DRW_mesh_get_attributes(Object *object,
                             Mesh *me,
                             GPUMaterial **gpumat_array,
                             int gpumat_array_len,
                             DRW_Attributes *r_attrs,
                             DRW_MeshCDMask *r_cd_needed)
{
  DRW_Attributes attrs_needed;
  drw_attributes_clear(&attrs_needed);
  DRW_MeshCDMask cd_needed = mesh_cd_calc_used_gpu_layers(
      object, me, gpumat_array, gpumat_array_len, &attrs_needed);

  if (r_attrs) {
    *r_attrs = attrs_needed;
  }

  if (r_cd_needed) {
    *r_cd_needed = cd_needed;
  }
}

GPUBatch **DRW_mesh_batch_cache_get_surface_shaded(Object *object,
                                                   Mesh *me,
                                                   GPUMaterial **gpumat_array,
                                                   uint gpumat_array_len)
{
  MeshBatchCache *cache = mesh_batch_cache_get(me);
  DRW_Attributes attrs_needed;
  drw_attributes_clear(&attrs_needed);
  DRW_MeshCDMask cd_needed = mesh_cd_calc_used_gpu_layers(
      object, me, gpumat_array, gpumat_array_len, &attrs_needed);

  BLI_assert(gpumat_array_len == cache->mat_len);

  mesh_cd_layers_type_merge(&cache->cd_needed, cd_needed);
  drw_attributes_merge(&cache->attr_needed, &attrs_needed, me->runtime->render_mutex);
  mesh_batch_cache_request_surface_batches(cache);
  return cache->surface_per_mat;
}

GPUBatch **DRW_mesh_batch_cache_get_surface_texpaint(Object *object, Mesh *me)
{
  MeshBatchCache *cache = mesh_batch_cache_get(me);
  texpaint_request_active_uv(cache, object, me);
  mesh_batch_cache_request_surface_batches(cache);
  return cache->surface_per_mat;
}

GPUBatch *DRW_mesh_batch_cache_get_surface_texpaint_single(Object *object, Mesh *me)
{
  MeshBatchCache *cache = mesh_batch_cache_get(me);
  texpaint_request_active_uv(cache, object, me);
  mesh_batch_cache_request_surface_batches(cache);
  return cache->batch.surface;
}

GPUBatch *DRW_mesh_batch_cache_get_surface_vertpaint(Object *object, Mesh *me)
{
  MeshBatchCache *cache = mesh_batch_cache_get(me);

  DRW_Attributes attrs_needed{};
  request_active_and_default_color_attributes(*object, *me, attrs_needed);

  drw_attributes_merge(&cache->attr_needed, &attrs_needed, me->runtime->render_mutex);

  mesh_batch_cache_request_surface_batches(cache);
  return cache->batch.surface;
}

GPUBatch *DRW_mesh_batch_cache_get_surface_sculpt(Object *object, Mesh *me)
{
  MeshBatchCache *cache = mesh_batch_cache_get(me);

  DRW_Attributes attrs_needed{};
  request_active_and_default_color_attributes(*object, *me, attrs_needed);

  drw_attributes_merge(&cache->attr_needed, &attrs_needed, me->runtime->render_mutex);

  mesh_batch_cache_request_surface_batches(cache);
  return cache->batch.surface;
}

int DRW_mesh_material_count_get(const Object *object, const Mesh *me)
{
  return mesh_render_mat_len_get(object, me);
}

GPUBatch *DRW_mesh_batch_cache_get_sculpt_overlays(Mesh *me)
{
  MeshBatchCache *cache = mesh_batch_cache_get(me);

  cache->cd_needed.sculpt_overlays = 1;
  mesh_batch_cache_add_request(cache, MBC_SCULPT_OVERLAYS);
  DRW_batch_request(&cache->batch.sculpt_overlays);

  return cache->batch.sculpt_overlays;
}

GPUBatch *DRW_mesh_batch_cache_get_surface_viewer_attribute(Mesh *me)
{
  MeshBatchCache *cache = mesh_batch_cache_get(me);

  mesh_batch_cache_add_request(cache, MBC_VIEWER_ATTRIBUTE_OVERLAY);
  DRW_batch_request(&cache->batch.surface_viewer_attribute);

  return cache->batch.surface_viewer_attribute;
}

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Edit Mode API
 * \{ */

GPUVertBuf *DRW_mesh_batch_cache_pos_vertbuf_get(Mesh *me)
{
  MeshBatchCache *cache = mesh_batch_cache_get(me);
  /* Request surface to trigger the vbo filling. Otherwise it may do nothing. */
  mesh_batch_cache_request_surface_batches(cache);

  DRW_vbo_request(nullptr, &cache->final.buff.vbo.pos_nor);
  return cache->final.buff.vbo.pos_nor;
}

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Edit Mode API
 * \{ */

GPUBatch *DRW_mesh_batch_cache_get_edit_triangles(Mesh *me)
{
  MeshBatchCache *cache = mesh_batch_cache_get(me);
  mesh_batch_cache_add_request(cache, MBC_EDIT_TRIANGLES);
  return DRW_batch_request(&cache->batch.edit_triangles);
}

GPUBatch *DRW_mesh_batch_cache_get_edit_edges(Mesh *me)
{
  MeshBatchCache *cache = mesh_batch_cache_get(me);
  mesh_batch_cache_add_request(cache, MBC_EDIT_EDGES);
  return DRW_batch_request(&cache->batch.edit_edges);
}

GPUBatch *DRW_mesh_batch_cache_get_edit_vertices(Mesh *me)
{
  MeshBatchCache *cache = mesh_batch_cache_get(me);
  mesh_batch_cache_add_request(cache, MBC_EDIT_VERTICES);
  return DRW_batch_request(&cache->batch.edit_vertices);
}

GPUBatch *DRW_mesh_batch_cache_get_edit_vert_normals(Mesh *me)
{
  MeshBatchCache *cache = mesh_batch_cache_get(me);
  mesh_batch_cache_add_request(cache, MBC_EDIT_VNOR);
  return DRW_batch_request(&cache->batch.edit_vnor);
}

GPUBatch *DRW_mesh_batch_cache_get_edit_loop_normals(Mesh *me)
{
  MeshBatchCache *cache = mesh_batch_cache_get(me);
  mesh_batch_cache_add_request(cache, MBC_EDIT_LNOR);
  return DRW_batch_request(&cache->batch.edit_lnor);
}

GPUBatch *DRW_mesh_batch_cache_get_edit_facedots(Mesh *me)
{
  MeshBatchCache *cache = mesh_batch_cache_get(me);
  mesh_batch_cache_add_request(cache, MBC_EDIT_FACEDOTS);
  return DRW_batch_request(&cache->batch.edit_fdots);
}

GPUBatch *DRW_mesh_batch_cache_get_edit_skin_roots(Mesh *me)
{
  MeshBatchCache *cache = mesh_batch_cache_get(me);
  mesh_batch_cache_add_request(cache, MBC_SKIN_ROOTS);
  return DRW_batch_request(&cache->batch.edit_skin_roots);
}

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Edit Mode selection API
 * \{ */

GPUBatch *DRW_mesh_batch_cache_get_triangles_with_select_id(Mesh *me)
{
  MeshBatchCache *cache = mesh_batch_cache_get(me);
  mesh_batch_cache_add_request(cache, MBC_EDIT_SELECTION_FACES);
  return DRW_batch_request(&cache->batch.edit_selection_faces);
}

GPUBatch *DRW_mesh_batch_cache_get_facedots_with_select_id(Mesh *me)
{
  MeshBatchCache *cache = mesh_batch_cache_get(me);
  mesh_batch_cache_add_request(cache, MBC_EDIT_SELECTION_FACEDOTS);
  return DRW_batch_request(&cache->batch.edit_selection_fdots);
}

GPUBatch *DRW_mesh_batch_cache_get_edges_with_select_id(Mesh *me)
{
  MeshBatchCache *cache = mesh_batch_cache_get(me);
  mesh_batch_cache_add_request(cache, MBC_EDIT_SELECTION_EDGES);
  return DRW_batch_request(&cache->batch.edit_selection_edges);
}

GPUBatch *DRW_mesh_batch_cache_get_verts_with_select_id(Mesh *me)
{
  MeshBatchCache *cache = mesh_batch_cache_get(me);
  mesh_batch_cache_add_request(cache, MBC_EDIT_SELECTION_VERTS);
  return DRW_batch_request(&cache->batch.edit_selection_verts);
}

/** \} */

/* ---------------------------------------------------------------------- */
/** \name UV Image editor API
 * \{ */

static void edituv_request_active_uv(MeshBatchCache *cache, Object *object, Mesh *me)
{
  DRW_MeshCDMask cd_needed;
  mesh_cd_layers_type_clear(&cd_needed);
  mesh_cd_calc_active_uv_layer(object, me, &cd_needed);
  mesh_cd_calc_edit_uv_layer(me, &cd_needed);

  BLI_assert(cd_needed.edit_uv != 0 &&
             "No uv layer available in edituv, but batches requested anyway!");

  mesh_cd_calc_active_mask_uv_layer(object, me, &cd_needed);
  mesh_cd_layers_type_merge(&cache->cd_needed, cd_needed);
}

GPUBatch *DRW_mesh_batch_cache_get_edituv_faces_stretch_area(Object *object,
                                                             Mesh *me,
                                                             float **tot_area,
                                                             float **tot_uv_area)
{
  MeshBatchCache *cache = mesh_batch_cache_get(me);
  edituv_request_active_uv(cache, object, me);
  mesh_batch_cache_add_request(cache, MBC_EDITUV_FACES_STRETCH_AREA);

  if (tot_area != nullptr) {
    *tot_area = &cache->tot_area;
  }
  if (tot_uv_area != nullptr) {
    *tot_uv_area = &cache->tot_uv_area;
  }
  return DRW_batch_request(&cache->batch.edituv_faces_stretch_area);
}

GPUBatch *DRW_mesh_batch_cache_get_edituv_faces_stretch_angle(Object *object, Mesh *me)
{
  MeshBatchCache *cache = mesh_batch_cache_get(me);
  edituv_request_active_uv(cache, object, me);
  mesh_batch_cache_add_request(cache, MBC_EDITUV_FACES_STRETCH_ANGLE);
  return DRW_batch_request(&cache->batch.edituv_faces_stretch_angle);
}

GPUBatch *DRW_mesh_batch_cache_get_edituv_faces(Object *object, Mesh *me)
{
  MeshBatchCache *cache = mesh_batch_cache_get(me);
  edituv_request_active_uv(cache, object, me);
  mesh_batch_cache_add_request(cache, MBC_EDITUV_FACES);
  return DRW_batch_request(&cache->batch.edituv_faces);
}

GPUBatch *DRW_mesh_batch_cache_get_edituv_edges(Object *object, Mesh *me)
{
  MeshBatchCache *cache = mesh_batch_cache_get(me);
  edituv_request_active_uv(cache, object, me);
  mesh_batch_cache_add_request(cache, MBC_EDITUV_EDGES);
  return DRW_batch_request(&cache->batch.edituv_edges);
}

GPUBatch *DRW_mesh_batch_cache_get_edituv_verts(Object *object, Mesh *me)
{
  MeshBatchCache *cache = mesh_batch_cache_get(me);
  edituv_request_active_uv(cache, object, me);
  mesh_batch_cache_add_request(cache, MBC_EDITUV_VERTS);
  return DRW_batch_request(&cache->batch.edituv_verts);
}

GPUBatch *DRW_mesh_batch_cache_get_edituv_facedots(Object *object, Mesh *me)
{
  MeshBatchCache *cache = mesh_batch_cache_get(me);
  edituv_request_active_uv(cache, object, me);
  mesh_batch_cache_add_request(cache, MBC_EDITUV_FACEDOTS);
  return DRW_batch_request(&cache->batch.edituv_fdots);
}

GPUBatch *DRW_mesh_batch_cache_get_uv_edges(Object *object, Mesh *me)
{
  MeshBatchCache *cache = mesh_batch_cache_get(me);
  edituv_request_active_uv(cache, object, me);
  mesh_batch_cache_add_request(cache, MBC_WIRE_LOOPS_UVS);
  return DRW_batch_request(&cache->batch.wire_loops_uvs);
}

GPUBatch *DRW_mesh_batch_cache_get_surface_edges(Object *object, Mesh *me)
{
  MeshBatchCache *cache = mesh_batch_cache_get(me);
  texpaint_request_active_uv(cache, object, me);
  mesh_batch_cache_add_request(cache, MBC_WIRE_LOOPS);
  return DRW_batch_request(&cache->batch.wire_loops);
}

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Grouped batch generation
 * \{ */

void DRW_mesh_batch_cache_free_old(Mesh *me, int ctime)
{
  MeshBatchCache *cache = static_cast<MeshBatchCache *>(me->runtime->batch_cache);

  if (cache == nullptr) {
    return;
  }

  if (mesh_cd_layers_type_equal(cache->cd_used_over_time, cache->cd_used)) {
    cache->lastmatch = ctime;
  }

  if (drw_attributes_overlap(&cache->attr_used_over_time, &cache->attr_used)) {
    cache->lastmatch = ctime;
  }

  if (ctime - cache->lastmatch > U.vbotimeout) {
    mesh_batch_cache_discard_shaded_tri(cache);
  }

  mesh_cd_layers_type_clear(&cache->cd_used_over_time);
  drw_attributes_clear(&cache->attr_used_over_time);
}

static void drw_add_attributes_vbo(GPUBatch *batch,
                                   MeshBufferList *mbuflist,
                                   DRW_Attributes *attr_used)
{
  for (int i = 0; i < attr_used->num_requests; i++) {
    DRW_vbo_request(batch, &mbuflist->vbo.attr[i]);
  }
}

#ifdef DEBUG
/* Sanity check function to test if all requested batches are available. */
static void drw_mesh_batch_cache_check_available(struct TaskGraph *task_graph, Mesh *me)
{
  MeshBatchCache *cache = mesh_batch_cache_get(me);
  /* Make sure all requested batches have been setup. */
  /* NOTE: The next line creates a different scheduling than during release builds what can lead to
   * some issues (See #77867 where we needed to disable this function in order to debug what was
   * happening in release builds). */
  BLI_task_graph_work_and_wait(task_graph);
  for (int i = 0; i < MBC_BATCH_LEN; i++) {
    BLI_assert(!DRW_batch_requested(((GPUBatch **)&cache->batch)[i], (GPUPrimType)0));
  }
  for (int i = 0; i < MBC_VBO_LEN; i++) {
    BLI_assert(!DRW_vbo_requested(((GPUVertBuf **)&cache->final.buff.vbo)[i]));
  }
  for (int i = 0; i < MBC_IBO_LEN; i++) {
    BLI_assert(!DRW_ibo_requested(((GPUIndexBuf **)&cache->final.buff.ibo)[i]));
  }
  for (int i = 0; i < MBC_VBO_LEN; i++) {
    BLI_assert(!DRW_vbo_requested(((GPUVertBuf **)&cache->cage.buff.vbo)[i]));
  }
  for (int i = 0; i < MBC_IBO_LEN; i++) {
    BLI_assert(!DRW_ibo_requested(((GPUIndexBuf **)&cache->cage.buff.ibo)[i]));
  }
  for (int i = 0; i < MBC_VBO_LEN; i++) {
    BLI_assert(!DRW_vbo_requested(((GPUVertBuf **)&cache->uv_cage.buff.vbo)[i]));
  }
  for (int i = 0; i < MBC_IBO_LEN; i++) {
    BLI_assert(!DRW_ibo_requested(((GPUIndexBuf **)&cache->uv_cage.buff.ibo)[i]));
  }
}
#endif

void DRW_mesh_batch_cache_create_requested(TaskGraph *task_graph,
                                           Object *ob,
                                           Mesh *me,
                                           const Scene *scene,
                                           const bool is_paint_mode,
                                           const bool use_hide)
{
  BLI_assert(task_graph);
  const ToolSettings *ts = nullptr;
  if (scene) {
    ts = scene->toolsettings;
  }
  MeshBatchCache *cache = mesh_batch_cache_get(me);
  bool cd_uv_update = false;

  /* Early out */
  if (cache->batch_requested == 0) {
#ifdef DEBUG
    drw_mesh_batch_cache_check_available(task_graph, me);
#endif
    return;
  }

#ifdef DEBUG
  /* Map the index of a buffer to a flag containing all batches that use it. */
  Map<int, DRWBatchFlag> batches_that_use_buffer_local;

  auto assert_deps_valid = [&](DRWBatchFlag batch_flag, Span<int> used_buffer_indices) {
    for (const int buffer_index : used_buffer_indices) {
      batches_that_use_buffer_local.add_or_modify(
          buffer_index,
          [&](DRWBatchFlag *value) { *value = batch_flag; },
          [&](DRWBatchFlag *value) { *value |= batch_flag; });
      BLI_assert(batches_that_use_buffer(buffer_index) & batch_flag);
    }
  };
#else
  auto assert_deps_valid = [&](DRWBatchFlag /*batch_flag*/, Span<int> /*used_buffer_indices*/) {};

#endif

  /* Sanity check. */
  if ((me->edit_mesh != nullptr) && (ob->mode & OB_MODE_EDIT)) {
    BLI_assert(BKE_object_get_editmesh_eval_final(ob) != nullptr);
  }

  const bool is_editmode = (me->edit_mesh != nullptr) &&
                           (BKE_object_get_editmesh_eval_final(ob) != nullptr) &&
                           DRW_object_is_in_edit_mode(ob);

  /* This could be set for paint mode too, currently it's only used for edit-mode. */
  const bool is_mode_active = (is_editmode && DRW_object_is_in_edit_mode(ob)) ||
                              ((ob->mode == OB_MODE_SCULPT) && ob->sculpt && ob->sculpt->bm);

  DRWBatchFlag batch_requested = cache->batch_requested;
  cache->batch_requested = (DRWBatchFlag)0;

  if (batch_requested & MBC_SURFACE_WEIGHTS) {
    /* Check vertex weights. */
    if ((cache->batch.surface_weights != nullptr) && (ts != nullptr)) {
      DRW_MeshWeightState wstate;
      BLI_assert(ob->type == OB_MESH);
      drw_mesh_weight_state_extract(ob, me, ts, is_paint_mode, &wstate);
      mesh_batch_cache_check_vertex_group(cache, &wstate);
      drw_mesh_weight_state_copy(&cache->weight_state, &wstate);
      drw_mesh_weight_state_clear(&wstate);
    }
  }

  if (batch_requested &
      (MBC_SURFACE | MBC_WIRE_LOOPS_UVS | MBC_EDITUV_FACES_STRETCH_AREA |
       MBC_EDITUV_FACES_STRETCH_ANGLE | MBC_EDITUV_FACES | MBC_EDITUV_EDGES | MBC_EDITUV_VERTS))
  {
    /* Modifiers will only generate an orco layer if the mesh is deformed. */
    if (cache->cd_needed.orco != 0) {
      /* Orco is always extracted from final mesh. */
      Mesh *me_final = (me->edit_mesh) ? BKE_object_get_editmesh_eval_final(ob) : me;
      if (CustomData_get_layer(&me_final->vdata, CD_ORCO) == nullptr) {
        /* Skip orco calculation */
        cache->cd_needed.orco = 0;
      }
    }

    /* Verify that all surface batches have needed attribute layers.
     */
    /* TODO(fclem): We could be a bit smarter here and only do it per
     * material. */
    bool cd_overlap = mesh_cd_layers_type_overlap(cache->cd_used, cache->cd_needed);
    bool attr_overlap = drw_attributes_overlap(&cache->attr_used, &cache->attr_needed);
    if (cd_overlap == false || attr_overlap == false) {
      FOREACH_MESH_BUFFER_CACHE (cache, mbc) {
        if ((cache->cd_used.uv & cache->cd_needed.uv) != cache->cd_needed.uv) {
          GPU_VERTBUF_DISCARD_SAFE(mbc->buff.vbo.uv);
          cd_uv_update = true;
        }
        if ((cache->cd_used.tan & cache->cd_needed.tan) != cache->cd_needed.tan ||
            cache->cd_used.tan_orco != cache->cd_needed.tan_orco)
        {
          GPU_VERTBUF_DISCARD_SAFE(mbc->buff.vbo.tan);
        }
        if (cache->cd_used.orco != cache->cd_needed.orco) {
          GPU_VERTBUF_DISCARD_SAFE(mbc->buff.vbo.orco);
        }
        if (cache->cd_used.sculpt_overlays != cache->cd_needed.sculpt_overlays) {
          GPU_VERTBUF_DISCARD_SAFE(mbc->buff.vbo.sculpt_data);
        }
        if (!drw_attributes_overlap(&cache->attr_used, &cache->attr_needed)) {
          for (int i = 0; i < GPU_MAX_ATTR; i++) {
            GPU_VERTBUF_DISCARD_SAFE(mbc->buff.vbo.attr[i]);
          }
        }
      }
      /* We can't discard batches at this point as they have been
       * referenced for drawing. Just clear them in place. */
      for (int i = 0; i < cache->mat_len; i++) {
        GPU_BATCH_CLEAR_SAFE(cache->surface_per_mat[i]);
      }
      GPU_BATCH_CLEAR_SAFE(cache->batch.surface);
      cache->batch_ready &= ~(MBC_SURFACE);

      mesh_cd_layers_type_merge(&cache->cd_used, cache->cd_needed);
      drw_attributes_merge(&cache->attr_used, &cache->attr_needed, me->runtime->render_mutex);
    }
    mesh_cd_layers_type_merge(&cache->cd_used_over_time, cache->cd_needed);
    mesh_cd_layers_type_clear(&cache->cd_needed);

    drw_attributes_merge(
        &cache->attr_used_over_time, &cache->attr_needed, me->runtime->render_mutex);
    drw_attributes_clear(&cache->attr_needed);
  }

  if (batch_requested & MBC_EDITUV) {
    /* Discard UV batches if sync_selection changes */
    const bool is_uvsyncsel = ts && (ts->uv_flag & UV_SYNC_SELECTION);
    if (cd_uv_update || (cache->is_uvsyncsel != is_uvsyncsel)) {
      cache->is_uvsyncsel = is_uvsyncsel;
      FOREACH_MESH_BUFFER_CACHE (cache, mbc) {
        GPU_VERTBUF_DISCARD_SAFE(mbc->buff.vbo.edituv_data);
        GPU_VERTBUF_DISCARD_SAFE(mbc->buff.vbo.fdots_uv);
        GPU_VERTBUF_DISCARD_SAFE(mbc->buff.vbo.fdots_edituv_data);
        GPU_INDEXBUF_DISCARD_SAFE(mbc->buff.ibo.edituv_tris);
        GPU_INDEXBUF_DISCARD_SAFE(mbc->buff.ibo.edituv_lines);
        GPU_INDEXBUF_DISCARD_SAFE(mbc->buff.ibo.edituv_points);
        GPU_INDEXBUF_DISCARD_SAFE(mbc->buff.ibo.edituv_fdots);
      }
      /* We only clear the batches as they may already have been
       * referenced. */
      GPU_BATCH_CLEAR_SAFE(cache->batch.wire_loops_uvs);
      GPU_BATCH_CLEAR_SAFE(cache->batch.edituv_faces_stretch_area);
      GPU_BATCH_CLEAR_SAFE(cache->batch.edituv_faces_stretch_angle);
      GPU_BATCH_CLEAR_SAFE(cache->batch.edituv_faces);
      GPU_BATCH_CLEAR_SAFE(cache->batch.edituv_edges);
      GPU_BATCH_CLEAR_SAFE(cache->batch.edituv_verts);
      GPU_BATCH_CLEAR_SAFE(cache->batch.edituv_fdots);
      cache->batch_ready &= ~MBC_EDITUV;
    }
  }

  /* Second chance to early out */
  if ((batch_requested & ~cache->batch_ready) == 0) {
#ifdef DEBUG
    drw_mesh_batch_cache_check_available(task_graph, me);
#endif
    return;
  }

  /* TODO(pablodp606): This always updates the sculpt normals for regular drawing (non-PBVH).
   * This makes tools that sample the surface per step get wrong normals until a redraw happens.
   * Normal updates should be part of the brush loop and only run during the stroke when the
   * brush needs to sample the surface. The drawing code should only update the normals
   * per redraw when smooth shading is enabled. */
  const bool do_update_sculpt_normals = ob->sculpt && ob->sculpt->pbvh;
  if (do_update_sculpt_normals) {
    Mesh *mesh = static_cast<Mesh *>(ob->data);
    BKE_pbvh_update_normals(ob->sculpt->pbvh, mesh->runtime->subdiv_ccg);
  }

  cache->batch_ready |= batch_requested;

  bool do_cage = false, do_uvcage = false;
  if (is_editmode && is_mode_active) {
    Mesh *editmesh_eval_final = BKE_object_get_editmesh_eval_final(ob);
    Mesh *editmesh_eval_cage = BKE_object_get_editmesh_eval_cage(ob);

    do_cage = editmesh_eval_final != editmesh_eval_cage;
    do_uvcage = !(editmesh_eval_final->runtime->is_original_bmesh &&
                  editmesh_eval_final->runtime->wrapper_type == ME_WRAPPER_TYPE_BMESH);
  }

  const bool do_subdivision = BKE_subsurf_modifier_has_gpu_subdiv(me);

  MeshBufferList *mbuflist = &cache->final.buff;

  /* Initialize batches and request VBO's & IBO's. */
  assert_deps_valid(MBC_SURFACE,
                    {BUFFER_INDEX(ibo.tris),
                     BUFFER_INDEX(vbo.lnor),
                     BUFFER_INDEX(vbo.pos_nor),
                     BUFFER_INDEX(vbo.uv),
                     BUFFER_INDEX(vbo.attr[0]),
                     BUFFER_INDEX(vbo.attr[1]),
                     BUFFER_INDEX(vbo.attr[2]),
                     BUFFER_INDEX(vbo.attr[3]),
                     BUFFER_INDEX(vbo.attr[4]),
                     BUFFER_INDEX(vbo.attr[5]),
                     BUFFER_INDEX(vbo.attr[6]),
                     BUFFER_INDEX(vbo.attr[7]),
                     BUFFER_INDEX(vbo.attr[8]),
                     BUFFER_INDEX(vbo.attr[9]),
                     BUFFER_INDEX(vbo.attr[10]),
                     BUFFER_INDEX(vbo.attr[11]),
                     BUFFER_INDEX(vbo.attr[12]),
                     BUFFER_INDEX(vbo.attr[13]),
                     BUFFER_INDEX(vbo.attr[14])});
  if (DRW_batch_requested(cache->batch.surface, GPU_PRIM_TRIS)) {
    DRW_ibo_request(cache->batch.surface, &mbuflist->ibo.tris);
    /* Order matters. First ones override latest VBO's attributes. */
    DRW_vbo_request(cache->batch.surface, &mbuflist->vbo.lnor);
    DRW_vbo_request(cache->batch.surface, &mbuflist->vbo.pos_nor);
    if (cache->cd_used.uv != 0) {
      DRW_vbo_request(cache->batch.surface, &mbuflist->vbo.uv);
    }
    drw_add_attributes_vbo(cache->batch.surface, mbuflist, &cache->attr_used);
  }
  assert_deps_valid(MBC_ALL_VERTS, {BUFFER_INDEX(vbo.pos_nor)});
  if (DRW_batch_requested(cache->batch.all_verts, GPU_PRIM_POINTS)) {
    DRW_vbo_request(cache->batch.all_verts, &mbuflist->vbo.pos_nor);
  }
  assert_deps_valid(
      MBC_SCULPT_OVERLAYS,
      {BUFFER_INDEX(ibo.tris), BUFFER_INDEX(vbo.pos_nor), BUFFER_INDEX(vbo.sculpt_data)});
  if (DRW_batch_requested(cache->batch.sculpt_overlays, GPU_PRIM_TRIS)) {
    DRW_ibo_request(cache->batch.sculpt_overlays, &mbuflist->ibo.tris);
    DRW_vbo_request(cache->batch.sculpt_overlays, &mbuflist->vbo.pos_nor);
    DRW_vbo_request(cache->batch.sculpt_overlays, &mbuflist->vbo.sculpt_data);
  }
  assert_deps_valid(MBC_ALL_EDGES, {BUFFER_INDEX(ibo.lines), BUFFER_INDEX(vbo.pos_nor)});
  if (DRW_batch_requested(cache->batch.all_edges, GPU_PRIM_LINES)) {
    DRW_ibo_request(cache->batch.all_edges, &mbuflist->ibo.lines);
    DRW_vbo_request(cache->batch.all_edges, &mbuflist->vbo.pos_nor);
  }
  assert_deps_valid(MBC_LOOSE_EDGES, {BUFFER_INDEX(ibo.lines_loose), BUFFER_INDEX(vbo.pos_nor)});
  if (DRW_batch_requested(cache->batch.loose_edges, GPU_PRIM_LINES)) {
    DRW_ibo_request(nullptr, &mbuflist->ibo.lines);
    DRW_ibo_request(cache->batch.loose_edges, &mbuflist->ibo.lines_loose);
    DRW_vbo_request(cache->batch.loose_edges, &mbuflist->vbo.pos_nor);
  }
  assert_deps_valid(MBC_EDGE_DETECTION,
                    {BUFFER_INDEX(ibo.lines_adjacency), BUFFER_INDEX(vbo.pos_nor)});
  if (DRW_batch_requested(cache->batch.edge_detection, GPU_PRIM_LINES_ADJ)) {
    DRW_ibo_request(cache->batch.edge_detection, &mbuflist->ibo.lines_adjacency);
    DRW_vbo_request(cache->batch.edge_detection, &mbuflist->vbo.pos_nor);
  }
  assert_deps_valid(
      MBC_SURFACE_WEIGHTS,
      {BUFFER_INDEX(ibo.tris), BUFFER_INDEX(vbo.pos_nor), BUFFER_INDEX(vbo.weights)});
  if (DRW_batch_requested(cache->batch.surface_weights, GPU_PRIM_TRIS)) {
    DRW_ibo_request(cache->batch.surface_weights, &mbuflist->ibo.tris);
    DRW_vbo_request(cache->batch.surface_weights, &mbuflist->vbo.pos_nor);
    DRW_vbo_request(cache->batch.surface_weights, &mbuflist->vbo.weights);
  }
  assert_deps_valid(
      MBC_WIRE_LOOPS,
      {BUFFER_INDEX(ibo.lines_paint_mask), BUFFER_INDEX(vbo.lnor), BUFFER_INDEX(vbo.pos_nor)});
  if (DRW_batch_requested(cache->batch.wire_loops, GPU_PRIM_LINES)) {
    DRW_ibo_request(cache->batch.wire_loops, &mbuflist->ibo.lines_paint_mask);
    /* Order matters. First ones override latest VBO's attributes. */
    DRW_vbo_request(cache->batch.wire_loops, &mbuflist->vbo.lnor);
    DRW_vbo_request(cache->batch.wire_loops, &mbuflist->vbo.pos_nor);
  }
  assert_deps_valid(
      MBC_WIRE_EDGES,
      {BUFFER_INDEX(ibo.lines), BUFFER_INDEX(vbo.pos_nor), BUFFER_INDEX(vbo.edge_fac)});
  if (DRW_batch_requested(cache->batch.wire_edges, GPU_PRIM_LINES)) {
    DRW_ibo_request(cache->batch.wire_edges, &mbuflist->ibo.lines);
    DRW_vbo_request(cache->batch.wire_edges, &mbuflist->vbo.pos_nor);
    DRW_vbo_request(cache->batch.wire_edges, &mbuflist->vbo.edge_fac);
  }
  assert_deps_valid(MBC_WIRE_LOOPS_UVS, {BUFFER_INDEX(ibo.edituv_lines), BUFFER_INDEX(vbo.uv)});
  if (DRW_batch_requested(cache->batch.wire_loops_uvs, GPU_PRIM_LINES)) {
    DRW_ibo_request(cache->batch.wire_loops_uvs, &mbuflist->ibo.edituv_lines);
    /* For paint overlay. Active layer should have been queried. */
    if (cache->cd_used.uv != 0) {
      DRW_vbo_request(cache->batch.wire_loops_uvs, &mbuflist->vbo.uv);
    }
  }
  assert_deps_valid(
      MBC_EDIT_MESH_ANALYSIS,
      {BUFFER_INDEX(ibo.tris), BUFFER_INDEX(vbo.pos_nor), BUFFER_INDEX(vbo.mesh_analysis)});
  if (DRW_batch_requested(cache->batch.edit_mesh_analysis, GPU_PRIM_TRIS)) {
    DRW_ibo_request(cache->batch.edit_mesh_analysis, &mbuflist->ibo.tris);
    DRW_vbo_request(cache->batch.edit_mesh_analysis, &mbuflist->vbo.pos_nor);
    DRW_vbo_request(cache->batch.edit_mesh_analysis, &mbuflist->vbo.mesh_analysis);
  }

  /* Per Material */
  assert_deps_valid(
      MBC_SURFACE_PER_MAT,
      {BUFFER_INDEX(vbo.lnor),     BUFFER_INDEX(vbo.pos_nor),  BUFFER_INDEX(vbo.uv),
       BUFFER_INDEX(vbo.tan),      BUFFER_INDEX(vbo.orco),     BUFFER_INDEX(vbo.attr[0]),
       BUFFER_INDEX(vbo.attr[1]),  BUFFER_INDEX(vbo.attr[2]),  BUFFER_INDEX(vbo.attr[3]),
       BUFFER_INDEX(vbo.attr[4]),  BUFFER_INDEX(vbo.attr[5]),  BUFFER_INDEX(vbo.attr[6]),
       BUFFER_INDEX(vbo.attr[7]),  BUFFER_INDEX(vbo.attr[8]),  BUFFER_INDEX(vbo.attr[9]),
       BUFFER_INDEX(vbo.attr[10]), BUFFER_INDEX(vbo.attr[11]), BUFFER_INDEX(vbo.attr[12]),
       BUFFER_INDEX(vbo.attr[13]), BUFFER_INDEX(vbo.attr[14])});
  assert_deps_valid(MBC_SURFACE_PER_MAT, {TRIS_PER_MAT_INDEX});
  for (int i = 0; i < cache->mat_len; i++) {
    if (DRW_batch_requested(cache->surface_per_mat[i], GPU_PRIM_TRIS)) {
      DRW_ibo_request(cache->surface_per_mat[i], &cache->tris_per_mat[i]);
      /* Order matters. First ones override latest VBO's attributes. */
      DRW_vbo_request(cache->surface_per_mat[i], &mbuflist->vbo.lnor);
      DRW_vbo_request(cache->surface_per_mat[i], &mbuflist->vbo.pos_nor);
      if (cache->cd_used.uv != 0) {
        DRW_vbo_request(cache->surface_per_mat[i], &mbuflist->vbo.uv);
      }
      if ((cache->cd_used.tan != 0) || (cache->cd_used.tan_orco != 0)) {
        DRW_vbo_request(cache->surface_per_mat[i], &mbuflist->vbo.tan);
      }
      if (cache->cd_used.orco != 0) {
        DRW_vbo_request(cache->surface_per_mat[i], &mbuflist->vbo.orco);
      }
      drw_add_attributes_vbo(cache->surface_per_mat[i], mbuflist, &cache->attr_used);
    }
  }

  mbuflist = (do_cage) ? &cache->cage.buff : &cache->final.buff;

  /* Edit Mesh */
  assert_deps_valid(
      MBC_EDIT_TRIANGLES,
      {BUFFER_INDEX(ibo.tris), BUFFER_INDEX(vbo.pos_nor), BUFFER_INDEX(vbo.edit_data)});
  if (DRW_batch_requested(cache->batch.edit_triangles, GPU_PRIM_TRIS)) {
    DRW_ibo_request(cache->batch.edit_triangles, &mbuflist->ibo.tris);
    DRW_vbo_request(cache->batch.edit_triangles, &mbuflist->vbo.pos_nor);
    DRW_vbo_request(cache->batch.edit_triangles, &mbuflist->vbo.edit_data);
  }
  assert_deps_valid(
      MBC_EDIT_VERTICES,
      {BUFFER_INDEX(ibo.points), BUFFER_INDEX(vbo.pos_nor), BUFFER_INDEX(vbo.edit_data)});
  if (DRW_batch_requested(cache->batch.edit_vertices, GPU_PRIM_POINTS)) {
    DRW_ibo_request(cache->batch.edit_vertices, &mbuflist->ibo.points);
    DRW_vbo_request(cache->batch.edit_vertices, &mbuflist->vbo.pos_nor);
    DRW_vbo_request(cache->batch.edit_vertices, &mbuflist->vbo.edit_data);
  }
  assert_deps_valid(
      MBC_EDIT_EDGES,
      {BUFFER_INDEX(ibo.lines), BUFFER_INDEX(vbo.pos_nor), BUFFER_INDEX(vbo.edit_data)});
  if (DRW_batch_requested(cache->batch.edit_edges, GPU_PRIM_LINES)) {
    DRW_ibo_request(cache->batch.edit_edges, &mbuflist->ibo.lines);
    DRW_vbo_request(cache->batch.edit_edges, &mbuflist->vbo.pos_nor);
    DRW_vbo_request(cache->batch.edit_edges, &mbuflist->vbo.edit_data);
  }
  assert_deps_valid(MBC_EDIT_VNOR, {BUFFER_INDEX(ibo.points), BUFFER_INDEX(vbo.pos_nor)});
  if (DRW_batch_requested(cache->batch.edit_vnor, GPU_PRIM_POINTS)) {
    DRW_ibo_request(cache->batch.edit_vnor, &mbuflist->ibo.points);
    DRW_vbo_request(cache->batch.edit_vnor, &mbuflist->vbo.pos_nor);
  }
  assert_deps_valid(MBC_EDIT_LNOR,
                    {BUFFER_INDEX(ibo.tris), BUFFER_INDEX(vbo.pos_nor), BUFFER_INDEX(vbo.lnor)});
  if (DRW_batch_requested(cache->batch.edit_lnor, GPU_PRIM_POINTS)) {
    DRW_ibo_request(cache->batch.edit_lnor, &mbuflist->ibo.tris);
    DRW_vbo_request(cache->batch.edit_lnor, &mbuflist->vbo.pos_nor);
    DRW_vbo_request(cache->batch.edit_lnor, &mbuflist->vbo.lnor);
  }
  assert_deps_valid(
      MBC_EDIT_FACEDOTS,
      {BUFFER_INDEX(ibo.fdots), BUFFER_INDEX(vbo.fdots_pos), BUFFER_INDEX(vbo.fdots_nor)});
  if (DRW_batch_requested(cache->batch.edit_fdots, GPU_PRIM_POINTS)) {
    DRW_ibo_request(cache->batch.edit_fdots, &mbuflist->ibo.fdots);
    DRW_vbo_request(cache->batch.edit_fdots, &mbuflist->vbo.fdots_pos);
    DRW_vbo_request(cache->batch.edit_fdots, &mbuflist->vbo.fdots_nor);
  }
  assert_deps_valid(MBC_SKIN_ROOTS, {BUFFER_INDEX(vbo.skin_roots)});
  if (DRW_batch_requested(cache->batch.edit_skin_roots, GPU_PRIM_POINTS)) {
    DRW_vbo_request(cache->batch.edit_skin_roots, &mbuflist->vbo.skin_roots);
  }

  /* Selection */
  assert_deps_valid(
      MBC_EDIT_SELECTION_VERTS,
      {BUFFER_INDEX(ibo.points), BUFFER_INDEX(vbo.pos_nor), BUFFER_INDEX(vbo.vert_idx)});
  if (DRW_batch_requested(cache->batch.edit_selection_verts, GPU_PRIM_POINTS)) {
    DRW_ibo_request(cache->batch.edit_selection_verts, &mbuflist->ibo.points);
    DRW_vbo_request(cache->batch.edit_selection_verts, &mbuflist->vbo.pos_nor);
    DRW_vbo_request(cache->batch.edit_selection_verts, &mbuflist->vbo.vert_idx);
  }
  assert_deps_valid(
      MBC_EDIT_SELECTION_EDGES,
      {BUFFER_INDEX(ibo.lines), BUFFER_INDEX(vbo.pos_nor), BUFFER_INDEX(vbo.edge_idx)});
  if (DRW_batch_requested(cache->batch.edit_selection_edges, GPU_PRIM_LINES)) {
    DRW_ibo_request(cache->batch.edit_selection_edges, &mbuflist->ibo.lines);
    DRW_vbo_request(cache->batch.edit_selection_edges, &mbuflist->vbo.pos_nor);
    DRW_vbo_request(cache->batch.edit_selection_edges, &mbuflist->vbo.edge_idx);
  }
  assert_deps_valid(
      MBC_EDIT_SELECTION_FACES,
      {BUFFER_INDEX(ibo.tris), BUFFER_INDEX(vbo.pos_nor), BUFFER_INDEX(vbo.poly_idx)});
  if (DRW_batch_requested(cache->batch.edit_selection_faces, GPU_PRIM_TRIS)) {
    DRW_ibo_request(cache->batch.edit_selection_faces, &mbuflist->ibo.tris);
    DRW_vbo_request(cache->batch.edit_selection_faces, &mbuflist->vbo.pos_nor);
    DRW_vbo_request(cache->batch.edit_selection_faces, &mbuflist->vbo.poly_idx);
  }
  assert_deps_valid(
      MBC_EDIT_SELECTION_FACEDOTS,
      {BUFFER_INDEX(ibo.fdots), BUFFER_INDEX(vbo.fdots_pos), BUFFER_INDEX(vbo.fdot_idx)});
  if (DRW_batch_requested(cache->batch.edit_selection_fdots, GPU_PRIM_POINTS)) {
    DRW_ibo_request(cache->batch.edit_selection_fdots, &mbuflist->ibo.fdots);
    DRW_vbo_request(cache->batch.edit_selection_fdots, &mbuflist->vbo.fdots_pos);
    DRW_vbo_request(cache->batch.edit_selection_fdots, &mbuflist->vbo.fdot_idx);
  }

  /**
   * TODO: The code and data structure is ready to support modified UV display
   * but the selection code for UVs needs to support it first. So for now, only
   * display the cage in all cases.
   */
  mbuflist = (do_uvcage) ? &cache->uv_cage.buff : &cache->final.buff;

  /* Edit UV */
  assert_deps_valid(
      MBC_EDITUV_FACES,
      {BUFFER_INDEX(ibo.edituv_tris), BUFFER_INDEX(vbo.uv), BUFFER_INDEX(vbo.edituv_data)});
  if (DRW_batch_requested(cache->batch.edituv_faces, GPU_PRIM_TRIS)) {
    DRW_ibo_request(cache->batch.edituv_faces, &mbuflist->ibo.edituv_tris);
    DRW_vbo_request(cache->batch.edituv_faces, &mbuflist->vbo.uv);
    DRW_vbo_request(cache->batch.edituv_faces, &mbuflist->vbo.edituv_data);
  }
  assert_deps_valid(MBC_EDITUV_FACES_STRETCH_AREA,
                    {BUFFER_INDEX(ibo.edituv_tris),
                     BUFFER_INDEX(vbo.uv),
                     BUFFER_INDEX(vbo.edituv_data),
                     BUFFER_INDEX(vbo.edituv_stretch_area)});
  if (DRW_batch_requested(cache->batch.edituv_faces_stretch_area, GPU_PRIM_TRIS)) {
    DRW_ibo_request(cache->batch.edituv_faces_stretch_area, &mbuflist->ibo.edituv_tris);
    DRW_vbo_request(cache->batch.edituv_faces_stretch_area, &mbuflist->vbo.uv);
    DRW_vbo_request(cache->batch.edituv_faces_stretch_area, &mbuflist->vbo.edituv_data);
    DRW_vbo_request(cache->batch.edituv_faces_stretch_area, &mbuflist->vbo.edituv_stretch_area);
  }
  assert_deps_valid(MBC_EDITUV_FACES_STRETCH_ANGLE,
                    {BUFFER_INDEX(ibo.edituv_tris),
                     BUFFER_INDEX(vbo.uv),
                     BUFFER_INDEX(vbo.edituv_data),
                     BUFFER_INDEX(vbo.edituv_stretch_angle)});
  if (DRW_batch_requested(cache->batch.edituv_faces_stretch_angle, GPU_PRIM_TRIS)) {
    DRW_ibo_request(cache->batch.edituv_faces_stretch_angle, &mbuflist->ibo.edituv_tris);
    DRW_vbo_request(cache->batch.edituv_faces_stretch_angle, &mbuflist->vbo.uv);
    DRW_vbo_request(cache->batch.edituv_faces_stretch_angle, &mbuflist->vbo.edituv_data);
    DRW_vbo_request(cache->batch.edituv_faces_stretch_angle, &mbuflist->vbo.edituv_stretch_angle);
  }
  assert_deps_valid(
      MBC_EDITUV_EDGES,
      {BUFFER_INDEX(ibo.edituv_lines), BUFFER_INDEX(vbo.uv), BUFFER_INDEX(vbo.edituv_data)});
  if (DRW_batch_requested(cache->batch.edituv_edges, GPU_PRIM_LINES)) {
    DRW_ibo_request(cache->batch.edituv_edges, &mbuflist->ibo.edituv_lines);
    DRW_vbo_request(cache->batch.edituv_edges, &mbuflist->vbo.uv);
    DRW_vbo_request(cache->batch.edituv_edges, &mbuflist->vbo.edituv_data);
  }
  assert_deps_valid(
      MBC_EDITUV_VERTS,
      {BUFFER_INDEX(ibo.edituv_points), BUFFER_INDEX(vbo.uv), BUFFER_INDEX(vbo.edituv_data)});
  if (DRW_batch_requested(cache->batch.edituv_verts, GPU_PRIM_POINTS)) {
    DRW_ibo_request(cache->batch.edituv_verts, &mbuflist->ibo.edituv_points);
    DRW_vbo_request(cache->batch.edituv_verts, &mbuflist->vbo.uv);
    DRW_vbo_request(cache->batch.edituv_verts, &mbuflist->vbo.edituv_data);
  }
  assert_deps_valid(MBC_EDITUV_FACEDOTS,
                    {BUFFER_INDEX(ibo.edituv_fdots),
                     BUFFER_INDEX(vbo.fdots_uv),
                     BUFFER_INDEX(vbo.fdots_edituv_data)});
  if (DRW_batch_requested(cache->batch.edituv_fdots, GPU_PRIM_POINTS)) {
    DRW_ibo_request(cache->batch.edituv_fdots, &mbuflist->ibo.edituv_fdots);
    DRW_vbo_request(cache->batch.edituv_fdots, &mbuflist->vbo.fdots_uv);
    DRW_vbo_request(cache->batch.edituv_fdots, &mbuflist->vbo.fdots_edituv_data);
  }
  assert_deps_valid(
      MBC_VIEWER_ATTRIBUTE_OVERLAY,
      {BUFFER_INDEX(ibo.tris), BUFFER_INDEX(vbo.pos_nor), BUFFER_INDEX(vbo.attr_viewer)});
  if (DRW_batch_requested(cache->batch.surface_viewer_attribute, GPU_PRIM_TRIS)) {
    DRW_ibo_request(cache->batch.surface_viewer_attribute, &mbuflist->ibo.tris);
    DRW_vbo_request(cache->batch.surface_viewer_attribute, &mbuflist->vbo.pos_nor);
    DRW_vbo_request(cache->batch.surface_viewer_attribute, &mbuflist->vbo.attr_viewer);
  }

#ifdef DEBUG
  auto assert_final_deps_valid = [&](const int buffer_index) {
    BLI_assert(batches_that_use_buffer(buffer_index) ==
               batches_that_use_buffer_local.lookup(buffer_index));
  };
  assert_final_deps_valid(BUFFER_INDEX(vbo.lnor));
  assert_final_deps_valid(BUFFER_INDEX(vbo.pos_nor));
  assert_final_deps_valid(BUFFER_INDEX(vbo.uv));
  assert_final_deps_valid(BUFFER_INDEX(vbo.sculpt_data));
  assert_final_deps_valid(BUFFER_INDEX(vbo.weights));
  assert_final_deps_valid(BUFFER_INDEX(vbo.edge_fac));
  assert_final_deps_valid(BUFFER_INDEX(vbo.mesh_analysis));
  assert_final_deps_valid(BUFFER_INDEX(vbo.tan));
  assert_final_deps_valid(BUFFER_INDEX(vbo.orco));
  assert_final_deps_valid(BUFFER_INDEX(vbo.edit_data));
  assert_final_deps_valid(BUFFER_INDEX(vbo.fdots_pos));
  assert_final_deps_valid(BUFFER_INDEX(vbo.fdots_nor));
  assert_final_deps_valid(BUFFER_INDEX(vbo.skin_roots));
  assert_final_deps_valid(BUFFER_INDEX(vbo.vert_idx));
  assert_final_deps_valid(BUFFER_INDEX(vbo.edge_idx));
  assert_final_deps_valid(BUFFER_INDEX(vbo.poly_idx));
  assert_final_deps_valid(BUFFER_INDEX(vbo.fdot_idx));
  assert_final_deps_valid(BUFFER_INDEX(vbo.edituv_data));
  assert_final_deps_valid(BUFFER_INDEX(vbo.edituv_stretch_area));
  assert_final_deps_valid(BUFFER_INDEX(vbo.edituv_stretch_angle));
  assert_final_deps_valid(BUFFER_INDEX(vbo.fdots_uv));
  assert_final_deps_valid(BUFFER_INDEX(vbo.fdots_edituv_data));
  for (const int i : IndexRange(GPU_MAX_ATTR)) {
    assert_final_deps_valid(BUFFER_INDEX(vbo.attr[i]));
  }
  assert_final_deps_valid(BUFFER_INDEX(vbo.attr_viewer));

  assert_final_deps_valid(BUFFER_INDEX(ibo.tris));
  assert_final_deps_valid(BUFFER_INDEX(ibo.lines));
  assert_final_deps_valid(BUFFER_INDEX(ibo.lines_loose));
  assert_final_deps_valid(BUFFER_INDEX(ibo.lines_adjacency));
  assert_final_deps_valid(BUFFER_INDEX(ibo.lines_paint_mask));
  assert_final_deps_valid(BUFFER_INDEX(ibo.points));
  assert_final_deps_valid(BUFFER_INDEX(ibo.fdots));
  assert_final_deps_valid(BUFFER_INDEX(ibo.edituv_tris));
  assert_final_deps_valid(BUFFER_INDEX(ibo.edituv_lines));
  assert_final_deps_valid(BUFFER_INDEX(ibo.edituv_points));
  assert_final_deps_valid(BUFFER_INDEX(ibo.edituv_fdots));

  assert_final_deps_valid(TRIS_PER_MAT_INDEX);
#endif

  if (do_uvcage) {
    blender::draw::mesh_buffer_cache_create_requested(task_graph,
                                                      cache,
                                                      &cache->uv_cage,
                                                      ob,
                                                      me,
                                                      is_editmode,
                                                      is_paint_mode,
                                                      is_mode_active,
                                                      ob->object_to_world,
                                                      false,
                                                      true,
                                                      scene,
                                                      ts,
                                                      true);
  }

  if (do_cage) {
    blender::draw::mesh_buffer_cache_create_requested(task_graph,
                                                      cache,
                                                      &cache->cage,
                                                      ob,
                                                      me,
                                                      is_editmode,
                                                      is_paint_mode,
                                                      is_mode_active,
                                                      ob->object_to_world,
                                                      false,
                                                      false,
                                                      scene,
                                                      ts,
                                                      true);
  }

  if (do_subdivision) {
    DRW_create_subdivision(ob,
                           me,
                           cache,
                           &cache->final,
                           is_editmode,
                           is_paint_mode,
                           is_mode_active,
                           ob->object_to_world,
                           true,
                           false,
                           do_cage,
                           ts,
                           use_hide);
  }
  else {
    /* The subsurf modifier may have been recently removed, or another modifier was added after it,
     * so free any potential subdivision cache as it is not needed anymore. */
    mesh_batch_cache_free_subdiv_cache(cache);
  }

  blender::draw::mesh_buffer_cache_create_requested(task_graph,
                                                    cache,
                                                    &cache->final,
                                                    ob,
                                                    me,
                                                    is_editmode,
                                                    is_paint_mode,
                                                    is_mode_active,
                                                    ob->object_to_world,
                                                    true,
                                                    false,
                                                    scene,
                                                    ts,
                                                    use_hide);

  /* Ensure that all requested batches have finished.
   * Ideally we want to remove this sync, but there are cases where this doesn't work.
   * See #79038 for example.
   *
   * An idea to improve this is to separate the Object mode from the edit mode draw caches. And
   * based on the mode the correct one will be updated. Other option is to look into using
   * drw_batch_cache_generate_requested_delayed. */
  BLI_task_graph_work_and_wait(task_graph);
#ifdef DEBUG
  drw_mesh_batch_cache_check_available(task_graph, me);
#endif
}

/** \} */
