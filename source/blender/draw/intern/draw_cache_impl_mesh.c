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
 * The Original Code is Copyright (C) 2017 by Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup draw
 *
 * \brief Mesh API for render engines
 */

#include "MEM_guardedalloc.h"

#include "BLI_alloca.h"
#include "BLI_bitmap.h"
#include "BLI_buffer.h"
#include "BLI_edgehash.h"
#include "BLI_listbase.h"
#include "BLI_math_bits.h"
#include "BLI_math_vector.h"
#include "BLI_string.h"
#include "BLI_task.h"
#include "BLI_utildefines.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_customdata.h"
#include "BKE_deform.h"
#include "BKE_editmesh.h"
#include "BKE_editmesh_cache.h"
#include "BKE_editmesh_tangent.h"
#include "BKE_mesh.h"
#include "BKE_mesh_runtime.h"
#include "BKE_mesh_tangent.h"
#include "BKE_modifier.h"
#include "BKE_object_deform.h"

#include "atomic_ops.h"

#include "bmesh.h"

#include "GPU_batch.h"
#include "GPU_material.h"

#include "DRW_render.h"

#include "ED_mesh.h"
#include "ED_uvedit.h"

#include "draw_cache_extract.h"
#include "draw_cache_inline.h"

#include "draw_cache_impl.h" /* own include */

static void mesh_batch_cache_clear(Mesh *me);

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
  atomic_fetch_and_or_uint32((uint32_t *)a, *(uint32_t *)&b);
}

BLI_INLINE void mesh_cd_layers_type_clear(DRW_MeshCDMask *a)
{
  *((uint32_t *)a) = 0;
}

static void mesh_cd_calc_edit_uv_layer(const Mesh *UNUSED(me), DRW_MeshCDMask *cd_used)
{
  cd_used->edit_uv = 1;
}

BLI_INLINE const CustomData *mesh_cd_ldata_get_from_mesh(const Mesh *me)
{
  switch ((eMeshWrapperType)me->runtime.wrapper_type) {
    case ME_WRAPPER_TYPE_MDATA:
      return &me->ldata;
      break;
    case ME_WRAPPER_TYPE_BMESH:
      return &me->edit_mesh->bm->ldata;
      break;
  }

  BLI_assert(0);
  return &me->ldata;
}

BLI_INLINE const CustomData *mesh_cd_vdata_get_from_mesh(const Mesh *me)
{
  switch ((eMeshWrapperType)me->runtime.wrapper_type) {
    case ME_WRAPPER_TYPE_MDATA:
      return &me->vdata;
      break;
    case ME_WRAPPER_TYPE_BMESH:
      return &me->edit_mesh->bm->vdata;
      break;
  }

  BLI_assert(0);
  return &me->vdata;
}

static void mesh_cd_calc_active_uv_layer(const Mesh *me, DRW_MeshCDMask *cd_used)
{
  const Mesh *me_final = (me->edit_mesh) ? me->edit_mesh->mesh_eval_final : me;
  const CustomData *cd_ldata = mesh_cd_ldata_get_from_mesh(me_final);
  int layer = CustomData_get_active_layer(cd_ldata, CD_MLOOPUV);
  if (layer != -1) {
    cd_used->uv |= (1 << layer);
  }
}

static void mesh_cd_calc_active_mask_uv_layer(const Mesh *me, DRW_MeshCDMask *cd_used)
{
  const Mesh *me_final = (me->edit_mesh) ? me->edit_mesh->mesh_eval_final : me;
  const CustomData *cd_ldata = mesh_cd_ldata_get_from_mesh(me_final);
  int layer = CustomData_get_stencil_layer(cd_ldata, CD_MLOOPUV);
  if (layer != -1) {
    cd_used->uv |= (1 << layer);
  }
}

static void mesh_cd_calc_active_vcol_layer(const Mesh *me, DRW_MeshCDMask *cd_used)
{
  const Mesh *me_final = (me->edit_mesh) ? me->edit_mesh->mesh_eval_final : me;
  const CustomData *cd_vdata = mesh_cd_vdata_get_from_mesh(me_final);

  int layer = CustomData_get_active_layer(cd_vdata, CD_PROP_COLOR);
  if (layer != -1) {
    cd_used->sculpt_vcol |= (1 << layer);
  }
}

static void mesh_cd_calc_active_mloopcol_layer(const Mesh *me, DRW_MeshCDMask *cd_used)
{
  const Mesh *me_final = (me->edit_mesh) ? me->edit_mesh->mesh_eval_final : me;
  const CustomData *cd_ldata = &me_final->ldata;

  int layer = CustomData_get_active_layer(cd_ldata, CD_MLOOPCOL);
  if (layer != -1) {
    cd_used->vcol |= (1 << layer);
  }
}

static DRW_MeshCDMask mesh_cd_calc_used_gpu_layers(const Mesh *me,
                                                   struct GPUMaterial **gpumat_array,
                                                   int gpumat_array_len)
{
  const Mesh *me_final = (me->edit_mesh) ? me->edit_mesh->mesh_eval_final : me;
  const CustomData *cd_ldata = mesh_cd_ldata_get_from_mesh(me_final);
  const CustomData *cd_vdata = mesh_cd_vdata_get_from_mesh(me_final);

  /* See: DM_vertex_attributes_from_gpu for similar logic */
  DRW_MeshCDMask cd_used;
  mesh_cd_layers_type_clear(&cd_used);

  for (int i = 0; i < gpumat_array_len; i++) {
    GPUMaterial *gpumat = gpumat_array[i];
    if (gpumat) {
      ListBase gpu_attrs = GPU_material_attributes(gpumat);
      LISTBASE_FOREACH (GPUMaterialAttribute *, gpu_attr, &gpu_attrs) {
        const char *name = gpu_attr->name;
        int type = gpu_attr->type;
        int layer = -1;

        if (type == CD_AUTO_FROM_NAME) {
          /* We need to deduct what exact layer is used.
           *
           * We do it based on the specified name.
           */
          if (name[0] != '\0') {
            layer = CustomData_get_named_layer(cd_ldata, CD_MLOOPUV, name);
            type = CD_MTFACE;

            if (layer == -1) {
              layer = CustomData_get_named_layer(cd_ldata, CD_MLOOPCOL, name);
              type = CD_MCOL;
            }

            if (layer == -1) {
              layer = CustomData_get_named_layer(cd_vdata, CD_PROP_COLOR, name);
              type = CD_PROP_COLOR;
            }
#if 0 /* Tangents are always from UV's - this will never happen. */
            if (layer == -1) {
              layer = CustomData_get_named_layer(cd_ldata, CD_TANGENT, name);
              type = CD_TANGENT;
            }
#endif
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
              layer = (name[0] != '\0') ? CustomData_get_named_layer(cd_ldata, CD_MLOOPUV, name) :
                                          CustomData_get_render_layer(cd_ldata, CD_MLOOPUV);
            }
            if (layer != -1) {
              cd_used.uv |= (1 << layer);
            }
            break;
          }
          case CD_TANGENT: {
            if (layer == -1) {
              layer = (name[0] != '\0') ? CustomData_get_named_layer(cd_ldata, CD_MLOOPUV, name) :
                                          CustomData_get_render_layer(cd_ldata, CD_MLOOPUV);

              /* Only fallback to orco (below) when we have no UV layers, see: T56545 */
              if (layer == -1 && name[0] != '\0') {
                layer = CustomData_get_render_layer(cd_ldata, CD_MLOOPUV);
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
          case CD_PROP_COLOR: {
            /* Sculpt Vertex Colors */
            if (layer == -1) {
              layer = (name[0] != '\0') ?
                          CustomData_get_named_layer(cd_vdata, CD_PROP_COLOR, name) :
                          CustomData_get_render_layer(cd_vdata, CD_PROP_COLOR);
            }
            if (layer != -1) {
              cd_used.sculpt_vcol |= (1 << layer);
            }
            break;
          }
          case CD_MCOL: {
            /* Vertex Color Data */
            if (layer == -1) {
              layer = (name[0] != '\0') ? CustomData_get_named_layer(cd_ldata, CD_MLOOPCOL, name) :
                                          CustomData_get_render_layer(cd_ldata, CD_MLOOPCOL);
            }
            if (layer != -1) {
              cd_used.vcol |= (1 << layer);
            }

            break;
          }
          case CD_ORCO: {
            cd_used.orco = 1;
            break;
          }
        }
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
static void drw_mesh_weight_state_clear(struct DRW_MeshWeightState *wstate)
{
  MEM_SAFE_FREE(wstate->defgroup_sel);
  MEM_SAFE_FREE(wstate->defgroup_locked);
  MEM_SAFE_FREE(wstate->defgroup_unlocked);

  memset(wstate, 0, sizeof(*wstate));

  wstate->defgroup_active = -1;
}

/** Copy selection data from one structure to another, including heap memory. */
static void drw_mesh_weight_state_copy(struct DRW_MeshWeightState *wstate_dst,
                                       const struct DRW_MeshWeightState *wstate_src)
{
  MEM_SAFE_FREE(wstate_dst->defgroup_sel);
  MEM_SAFE_FREE(wstate_dst->defgroup_locked);
  MEM_SAFE_FREE(wstate_dst->defgroup_unlocked);

  memcpy(wstate_dst, wstate_src, sizeof(*wstate_dst));

  if (wstate_src->defgroup_sel) {
    wstate_dst->defgroup_sel = MEM_dupallocN(wstate_src->defgroup_sel);
  }
  if (wstate_src->defgroup_locked) {
    wstate_dst->defgroup_locked = MEM_dupallocN(wstate_src->defgroup_locked);
  }
  if (wstate_src->defgroup_unlocked) {
    wstate_dst->defgroup_unlocked = MEM_dupallocN(wstate_src->defgroup_unlocked);
  }
}

static bool drw_mesh_flags_equal(const bool *array1, const bool *array2, int size)
{
  return ((!array1 && !array2) ||
          (array1 && array2 && memcmp(array1, array2, size * sizeof(bool)) == 0));
}

/** Compare two selection structures. */
static bool drw_mesh_weight_state_compare(const struct DRW_MeshWeightState *a,
                                          const struct DRW_MeshWeightState *b)
{
  return a->defgroup_active == b->defgroup_active && a->defgroup_len == b->defgroup_len &&
         a->flags == b->flags && a->alert_mode == b->alert_mode &&
         a->defgroup_sel_count == b->defgroup_sel_count &&
         drw_mesh_flags_equal(a->defgroup_sel, b->defgroup_sel, a->defgroup_len) &&
         drw_mesh_flags_equal(a->defgroup_locked, b->defgroup_locked, a->defgroup_len) &&
         drw_mesh_flags_equal(a->defgroup_unlocked, b->defgroup_unlocked, a->defgroup_len);
}

static void drw_mesh_weight_state_extract(Object *ob,
                                          Mesh *me,
                                          const ToolSettings *ts,
                                          bool paint_mode,
                                          struct DRW_MeshWeightState *wstate)
{
  /* Extract complete vertex weight group selection state and mode flags. */
  memset(wstate, 0, sizeof(*wstate));

  wstate->defgroup_active = ob->actdef - 1;
  wstate->defgroup_len = BLI_listbase_count(&ob->defbase);

  wstate->alert_mode = ts->weightuser;

  if (paint_mode && ts->multipaint) {
    /* Multi-paint needs to know all selected bones, not just the active group.
     * This is actually a relatively expensive operation, but caching would be difficult. */
    wstate->defgroup_sel = BKE_object_defgroup_selected_get(
        ob, wstate->defgroup_len, &wstate->defgroup_sel_count);

    if (wstate->defgroup_sel_count > 1) {
      wstate->flags |= DRW_MESH_WEIGHT_STATE_MULTIPAINT |
                       (ts->auto_normalize ? DRW_MESH_WEIGHT_STATE_AUTO_NORMALIZE : 0);

      if (me->editflag & ME_EDIT_MIRROR_X) {
        BKE_object_defgroup_mirror_selection(ob,
                                             wstate->defgroup_len,
                                             wstate->defgroup_sel,
                                             wstate->defgroup_sel,
                                             &wstate->defgroup_sel_count);
      }
    }
    /* With only one selected bone Multipaint reverts to regular mode. */
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
                                                      wstate->defgroup_sel_count)) {
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

static bool mesh_batch_cache_valid(Mesh *me)
{
  MeshBatchCache *cache = me->runtime.batch_cache;

  if (cache == NULL) {
    return false;
  }

  if (cache->is_editmode != (me->edit_mesh != NULL)) {
    return false;
  }

  if (cache->is_dirty) {
    return false;
  }

  if (cache->mat_len != mesh_render_mat_len_get(me)) {
    return false;
  }

  return true;
}

static void mesh_batch_cache_init(Mesh *me)
{
  MeshBatchCache *cache = me->runtime.batch_cache;

  if (!cache) {
    cache = me->runtime.batch_cache = MEM_callocN(sizeof(*cache), __func__);
  }
  else {
    memset(cache, 0, sizeof(*cache));
  }

  cache->is_editmode = me->edit_mesh != NULL;

  if (cache->is_editmode == false) {
    // cache->edge_len = mesh_render_edges_len_get(me);
    // cache->tri_len = mesh_render_looptri_len_get(me);
    // cache->poly_len = mesh_render_polys_len_get(me);
    // cache->vert_len = mesh_render_verts_len_get(me);
  }

  cache->mat_len = mesh_render_mat_len_get(me);
  cache->surface_per_mat = MEM_callocN(sizeof(*cache->surface_per_mat) * cache->mat_len, __func__);

  cache->is_dirty = false;
  cache->batch_ready = 0;
  cache->batch_requested = 0;

  drw_mesh_weight_state_clear(&cache->weight_state);
}

void DRW_mesh_batch_cache_validate(Mesh *me)
{
  if (!mesh_batch_cache_valid(me)) {
    mesh_batch_cache_clear(me);
    mesh_batch_cache_init(me);
  }
}

static MeshBatchCache *mesh_batch_cache_get(Mesh *me)
{
  return me->runtime.batch_cache;
}

static void mesh_batch_cache_check_vertex_group(MeshBatchCache *cache,
                                                const struct DRW_MeshWeightState *wstate)
{
  if (!drw_mesh_weight_state_compare(&cache->weight_state, wstate)) {
    FOREACH_MESH_BUFFER_CACHE (cache, mbufcache) {
      GPU_VERTBUF_DISCARD_SAFE(mbufcache->vbo.weights);
    }
    GPU_BATCH_CLEAR_SAFE(cache->batch.surface_weights);

    cache->batch_ready &= ~MBC_SURFACE_WEIGHTS;

    drw_mesh_weight_state_clear(&cache->weight_state);
  }
}

static void mesh_batch_cache_discard_shaded_batches(MeshBatchCache *cache)
{
  if (cache->surface_per_mat) {
    for (int i = 0; i < cache->mat_len; i++) {
      GPU_BATCH_DISCARD_SAFE(cache->surface_per_mat[i]);
    }
  }
  cache->batch_ready &= ~MBC_SURF_PER_MAT;
}

static void mesh_batch_cache_discard_shaded_tri(MeshBatchCache *cache)
{
  FOREACH_MESH_BUFFER_CACHE (cache, mbufcache) {
    GPU_VERTBUF_DISCARD_SAFE(mbufcache->vbo.pos_nor);
    GPU_VERTBUF_DISCARD_SAFE(mbufcache->vbo.uv);
    GPU_VERTBUF_DISCARD_SAFE(mbufcache->vbo.tan);
    GPU_VERTBUF_DISCARD_SAFE(mbufcache->vbo.vcol);
    GPU_VERTBUF_DISCARD_SAFE(mbufcache->vbo.orco);
  }
  mesh_batch_cache_discard_shaded_batches(cache);
  mesh_cd_layers_type_clear(&cache->cd_used);

  MEM_SAFE_FREE(cache->surface_per_mat);

  cache->mat_len = 0;
}

static void mesh_batch_cache_discard_uvedit(MeshBatchCache *cache)
{
  FOREACH_MESH_BUFFER_CACHE (cache, mbufcache) {
    GPU_VERTBUF_DISCARD_SAFE(mbufcache->vbo.stretch_angle);
    GPU_VERTBUF_DISCARD_SAFE(mbufcache->vbo.stretch_area);
    GPU_VERTBUF_DISCARD_SAFE(mbufcache->vbo.uv);
    GPU_VERTBUF_DISCARD_SAFE(mbufcache->vbo.edituv_data);
    GPU_VERTBUF_DISCARD_SAFE(mbufcache->vbo.fdots_uv);
    GPU_VERTBUF_DISCARD_SAFE(mbufcache->vbo.fdots_edituv_data);
    GPU_INDEXBUF_DISCARD_SAFE(mbufcache->ibo.edituv_tris);
    GPU_INDEXBUF_DISCARD_SAFE(mbufcache->ibo.edituv_lines);
    GPU_INDEXBUF_DISCARD_SAFE(mbufcache->ibo.edituv_points);
    GPU_INDEXBUF_DISCARD_SAFE(mbufcache->ibo.edituv_fdots);
  }
  GPU_BATCH_DISCARD_SAFE(cache->batch.edituv_faces_stretch_area);
  GPU_BATCH_DISCARD_SAFE(cache->batch.edituv_faces_stretch_angle);
  GPU_BATCH_DISCARD_SAFE(cache->batch.edituv_faces);
  GPU_BATCH_DISCARD_SAFE(cache->batch.edituv_edges);
  GPU_BATCH_DISCARD_SAFE(cache->batch.edituv_verts);
  GPU_BATCH_DISCARD_SAFE(cache->batch.edituv_fdots);
  GPU_BATCH_DISCARD_SAFE(cache->batch.wire_loops_uvs);

  cache->tot_area = 0.0f;
  cache->tot_uv_area = 0.0f;

  cache->batch_ready &= ~MBC_EDITUV;

  /* We discarded the vbo.uv so we need to reset the cd_used flag. */
  cache->cd_used.uv = 0;
  cache->cd_used.edit_uv = 0;

  /* Discard other batches that uses vbo.uv */
  mesh_batch_cache_discard_shaded_batches(cache);

  GPU_BATCH_DISCARD_SAFE(cache->batch.surface);
  cache->batch_ready &= ~MBC_SURFACE;
}

static void mesh_batch_cache_discard_uvedit_select(MeshBatchCache *cache)
{
  FOREACH_MESH_BUFFER_CACHE (cache, mbufcache) {
    GPU_VERTBUF_DISCARD_SAFE(mbufcache->vbo.edituv_data);
    GPU_VERTBUF_DISCARD_SAFE(mbufcache->vbo.fdots_edituv_data);
    GPU_INDEXBUF_DISCARD_SAFE(mbufcache->ibo.edituv_tris);
    GPU_INDEXBUF_DISCARD_SAFE(mbufcache->ibo.edituv_lines);
    GPU_INDEXBUF_DISCARD_SAFE(mbufcache->ibo.edituv_points);
    GPU_INDEXBUF_DISCARD_SAFE(mbufcache->ibo.edituv_fdots);
  }
  GPU_BATCH_DISCARD_SAFE(cache->batch.edituv_faces_stretch_area);
  GPU_BATCH_DISCARD_SAFE(cache->batch.edituv_faces_stretch_angle);
  GPU_BATCH_DISCARD_SAFE(cache->batch.edituv_faces);
  GPU_BATCH_DISCARD_SAFE(cache->batch.edituv_edges);
  GPU_BATCH_DISCARD_SAFE(cache->batch.edituv_verts);
  GPU_BATCH_DISCARD_SAFE(cache->batch.edituv_fdots);
  GPU_BATCH_DISCARD_SAFE(cache->batch.wire_loops_uvs);
  cache->batch_ready &= ~MBC_EDITUV;
}

void DRW_mesh_batch_cache_dirty_tag(Mesh *me, int mode)
{
  MeshBatchCache *cache = me->runtime.batch_cache;
  if (cache == NULL) {
    return;
  }
  switch (mode) {
    case BKE_MESH_BATCH_DIRTY_SELECT:
      FOREACH_MESH_BUFFER_CACHE (cache, mbufcache) {
        GPU_VERTBUF_DISCARD_SAFE(mbufcache->vbo.edit_data);
        GPU_VERTBUF_DISCARD_SAFE(mbufcache->vbo.fdots_nor);
      }
      GPU_BATCH_DISCARD_SAFE(cache->batch.edit_triangles);
      GPU_BATCH_DISCARD_SAFE(cache->batch.edit_vertices);
      GPU_BATCH_DISCARD_SAFE(cache->batch.edit_edges);
      GPU_BATCH_DISCARD_SAFE(cache->batch.edit_fdots);
      GPU_BATCH_DISCARD_SAFE(cache->batch.edit_selection_verts);
      GPU_BATCH_DISCARD_SAFE(cache->batch.edit_selection_edges);
      GPU_BATCH_DISCARD_SAFE(cache->batch.edit_selection_faces);
      GPU_BATCH_DISCARD_SAFE(cache->batch.edit_selection_fdots);
      GPU_BATCH_DISCARD_SAFE(cache->batch.edit_mesh_analysis);
      cache->batch_ready &= ~(MBC_EDIT_TRIANGLES | MBC_EDIT_VERTICES | MBC_EDIT_EDGES |
                              MBC_EDIT_FACEDOTS | MBC_EDIT_SELECTION_FACEDOTS |
                              MBC_EDIT_SELECTION_FACES | MBC_EDIT_SELECTION_EDGES |
                              MBC_EDIT_SELECTION_VERTS | MBC_EDIT_MESH_ANALYSIS);
      /* Because visible UVs depends on edit mode selection, discard topology. */
      mesh_batch_cache_discard_uvedit_select(cache);
      break;
    case BKE_MESH_BATCH_DIRTY_SELECT_PAINT:
      /* Paint mode selection flag is packed inside the nor attribute.
       * Note that it can be slow if auto smooth is enabled. (see T63946) */
      FOREACH_MESH_BUFFER_CACHE (cache, mbufcache) {
        GPU_INDEXBUF_DISCARD_SAFE(mbufcache->ibo.lines_paint_mask);
        GPU_VERTBUF_DISCARD_SAFE(mbufcache->vbo.pos_nor);
        GPU_VERTBUF_DISCARD_SAFE(mbufcache->vbo.lnor);
      }
      GPU_BATCH_DISCARD_SAFE(cache->batch.surface);
      GPU_BATCH_DISCARD_SAFE(cache->batch.wire_loops);
      GPU_BATCH_DISCARD_SAFE(cache->batch.wire_edges);
      if (cache->surface_per_mat) {
        for (int i = 0; i < cache->mat_len; i++) {
          GPU_BATCH_DISCARD_SAFE(cache->surface_per_mat[i]);
        }
      }
      cache->batch_ready &= ~(MBC_SURFACE | MBC_WIRE_EDGES | MBC_WIRE_LOOPS | MBC_SURF_PER_MAT);
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
      FOREACH_MESH_BUFFER_CACHE (cache, mbufcache) {
        GPU_VERTBUF_DISCARD_SAFE(mbufcache->vbo.edituv_data);
        GPU_VERTBUF_DISCARD_SAFE(mbufcache->vbo.fdots_edituv_data);
      }
      GPU_BATCH_DISCARD_SAFE(cache->batch.edituv_faces_stretch_area);
      GPU_BATCH_DISCARD_SAFE(cache->batch.edituv_faces_stretch_angle);
      GPU_BATCH_DISCARD_SAFE(cache->batch.edituv_faces);
      GPU_BATCH_DISCARD_SAFE(cache->batch.edituv_edges);
      GPU_BATCH_DISCARD_SAFE(cache->batch.edituv_verts);
      GPU_BATCH_DISCARD_SAFE(cache->batch.edituv_fdots);
      cache->batch_ready &= ~MBC_EDITUV;
      break;
    default:
      BLI_assert(0);
  }
}

static void mesh_batch_cache_clear(Mesh *me)
{
  MeshBatchCache *cache = me->runtime.batch_cache;
  if (!cache) {
    return;
  }
  FOREACH_MESH_BUFFER_CACHE (cache, mbufcache) {
    GPUVertBuf **vbos = (GPUVertBuf **)&mbufcache->vbo;
    GPUIndexBuf **ibos = (GPUIndexBuf **)&mbufcache->ibo;
    for (int i = 0; i < sizeof(mbufcache->vbo) / sizeof(void *); i++) {
      GPU_VERTBUF_DISCARD_SAFE(vbos[i]);
    }
    for (int i = 0; i < sizeof(mbufcache->ibo) / sizeof(void *); i++) {
      GPU_INDEXBUF_DISCARD_SAFE(ibos[i]);
    }
  }
  for (int i = 0; i < sizeof(cache->batch) / sizeof(void *); i++) {
    GPUBatch **batch = (GPUBatch **)&cache->batch;
    GPU_BATCH_DISCARD_SAFE(batch[i]);
  }

  mesh_batch_cache_discard_shaded_tri(cache);

  mesh_batch_cache_discard_uvedit(cache);

  cache->batch_ready = 0;

  drw_mesh_weight_state_clear(&cache->weight_state);
}

void DRW_mesh_batch_cache_free(Mesh *me)
{
  mesh_batch_cache_clear(me);
  MEM_SAFE_FREE(me->runtime.batch_cache);
}

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Public API
 * \{ */

static void texpaint_request_active_uv(MeshBatchCache *cache, Mesh *me)
{
  DRW_MeshCDMask cd_needed;
  mesh_cd_layers_type_clear(&cd_needed);
  mesh_cd_calc_active_uv_layer(me, &cd_needed);

  BLI_assert(cd_needed.uv != 0 &&
             "No uv layer available in texpaint, but batches requested anyway!");

  mesh_cd_calc_active_mask_uv_layer(me, &cd_needed);
  mesh_cd_layers_type_merge(&cache->cd_needed, cd_needed);
}

static void texpaint_request_active_vcol(MeshBatchCache *cache, Mesh *me)
{
  DRW_MeshCDMask cd_needed;
  mesh_cd_layers_type_clear(&cd_needed);
  mesh_cd_calc_active_mloopcol_layer(me, &cd_needed);

  BLI_assert(cd_needed.vcol != 0 &&
             "No MLOOPCOL layer available in vertpaint, but batches requested anyway!");

  mesh_cd_layers_type_merge(&cache->cd_needed, cd_needed);
}

static void sculpt_request_active_vcol(MeshBatchCache *cache, Mesh *me)
{
  DRW_MeshCDMask cd_needed;
  mesh_cd_layers_type_clear(&cd_needed);
  mesh_cd_calc_active_vcol_layer(me, &cd_needed);

  BLI_assert(cd_needed.sculpt_vcol != 0 &&
             "No MPropCol layer available in Sculpt, but batches requested anyway!");

  mesh_cd_layers_type_merge(&cache->cd_needed, cd_needed);
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
  mesh_batch_cache_add_request(cache, MBC_SURFACE);
  return DRW_batch_request(&cache->batch.surface);
}

GPUBatch *DRW_mesh_batch_cache_get_loose_edges(Mesh *me)
{
  MeshBatchCache *cache = mesh_batch_cache_get(me);
  mesh_batch_cache_add_request(cache, MBC_LOOSE_EDGES);
  if (cache->no_loose_wire) {
    return NULL;
  }
  else {
    return DRW_batch_request(&cache->batch.loose_edges);
  }
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

GPUBatch **DRW_mesh_batch_cache_get_surface_shaded(Mesh *me,
                                                   struct GPUMaterial **gpumat_array,
                                                   uint gpumat_array_len)
{
  MeshBatchCache *cache = mesh_batch_cache_get(me);
  DRW_MeshCDMask cd_needed = mesh_cd_calc_used_gpu_layers(me, gpumat_array, gpumat_array_len);

  BLI_assert(gpumat_array_len == cache->mat_len);

  mesh_cd_layers_type_merge(&cache->cd_needed, cd_needed);

  mesh_batch_cache_add_request(cache, MBC_SURF_PER_MAT);

  for (int i = 0; i < cache->mat_len; i++) {
    DRW_batch_request(&cache->surface_per_mat[i]);
  }
  return cache->surface_per_mat;
}

GPUBatch **DRW_mesh_batch_cache_get_surface_texpaint(Mesh *me)
{
  MeshBatchCache *cache = mesh_batch_cache_get(me);
  mesh_batch_cache_add_request(cache, MBC_SURF_PER_MAT);
  texpaint_request_active_uv(cache, me);
  for (int i = 0; i < cache->mat_len; i++) {
    DRW_batch_request(&cache->surface_per_mat[i]);
  }
  return cache->surface_per_mat;
}

GPUBatch *DRW_mesh_batch_cache_get_surface_texpaint_single(Mesh *me)
{
  MeshBatchCache *cache = mesh_batch_cache_get(me);
  texpaint_request_active_uv(cache, me);
  mesh_batch_cache_add_request(cache, MBC_SURFACE);
  return DRW_batch_request(&cache->batch.surface);
}

GPUBatch *DRW_mesh_batch_cache_get_surface_vertpaint(Mesh *me)
{
  MeshBatchCache *cache = mesh_batch_cache_get(me);
  texpaint_request_active_vcol(cache, me);
  mesh_batch_cache_add_request(cache, MBC_SURFACE);
  return DRW_batch_request(&cache->batch.surface);
}

GPUBatch *DRW_mesh_batch_cache_get_surface_sculpt(Mesh *me)
{
  MeshBatchCache *cache = mesh_batch_cache_get(me);
  sculpt_request_active_vcol(cache, me);
  mesh_batch_cache_add_request(cache, MBC_SURFACE);
  return DRW_batch_request(&cache->batch.surface);
}

int DRW_mesh_material_count_get(Mesh *me)
{
  return mesh_render_mat_len_get(me);
}

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Edit Mode API
 * \{ */

GPUVertBuf *DRW_mesh_batch_cache_pos_vertbuf_get(Mesh *me)
{
  MeshBatchCache *cache = mesh_batch_cache_get(me);
  /* Request surface to trigger the vbo filling. Otherwise it may do nothing. */
  mesh_batch_cache_add_request(cache, MBC_SURFACE);
  DRW_batch_request(&cache->batch.surface);

  DRW_vbo_request(NULL, &cache->final.vbo.pos_nor);
  return cache->final.vbo.pos_nor;
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

GPUBatch *DRW_mesh_batch_cache_get_edit_vnors(Mesh *me)
{
  MeshBatchCache *cache = mesh_batch_cache_get(me);
  mesh_batch_cache_add_request(cache, MBC_EDIT_VNOR);
  return DRW_batch_request(&cache->batch.edit_vnor);
}

GPUBatch *DRW_mesh_batch_cache_get_edit_lnors(Mesh *me)
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

static void edituv_request_active_uv(MeshBatchCache *cache, Mesh *me)
{
  DRW_MeshCDMask cd_needed;
  mesh_cd_layers_type_clear(&cd_needed);
  mesh_cd_calc_active_uv_layer(me, &cd_needed);
  mesh_cd_calc_edit_uv_layer(me, &cd_needed);

  BLI_assert(cd_needed.edit_uv != 0 &&
             "No uv layer available in edituv, but batches requested anyway!");

  mesh_cd_calc_active_mask_uv_layer(me, &cd_needed);
  mesh_cd_layers_type_merge(&cache->cd_needed, cd_needed);
}

/* Creates the GPUBatch for drawing the UV Stretching Area Overlay.
 * Optional retrieves the total area or total uv area of the mesh.
 *
 * The `cache->tot_area` and cache->tot_uv_area` update are calculation are
 * only valid after calling `DRW_mesh_batch_cache_create_requested`. */
GPUBatch *DRW_mesh_batch_cache_get_edituv_faces_stretch_area(Mesh *me,
                                                             float **tot_area,
                                                             float **tot_uv_area)
{
  MeshBatchCache *cache = mesh_batch_cache_get(me);
  edituv_request_active_uv(cache, me);
  mesh_batch_cache_add_request(cache, MBC_EDITUV_FACES_STRETCH_AREA);

  if (tot_area != NULL) {
    *tot_area = &cache->tot_area;
  }
  if (tot_uv_area != NULL) {
    *tot_uv_area = &cache->tot_uv_area;
  }
  return DRW_batch_request(&cache->batch.edituv_faces_stretch_area);
}

GPUBatch *DRW_mesh_batch_cache_get_edituv_faces_stretch_angle(Mesh *me)
{
  MeshBatchCache *cache = mesh_batch_cache_get(me);
  edituv_request_active_uv(cache, me);
  mesh_batch_cache_add_request(cache, MBC_EDITUV_FACES_STRETCH_ANGLE);
  return DRW_batch_request(&cache->batch.edituv_faces_stretch_angle);
}

GPUBatch *DRW_mesh_batch_cache_get_edituv_faces(Mesh *me)
{
  MeshBatchCache *cache = mesh_batch_cache_get(me);
  edituv_request_active_uv(cache, me);
  mesh_batch_cache_add_request(cache, MBC_EDITUV_FACES);
  return DRW_batch_request(&cache->batch.edituv_faces);
}

GPUBatch *DRW_mesh_batch_cache_get_edituv_edges(Mesh *me)
{
  MeshBatchCache *cache = mesh_batch_cache_get(me);
  edituv_request_active_uv(cache, me);
  mesh_batch_cache_add_request(cache, MBC_EDITUV_EDGES);
  return DRW_batch_request(&cache->batch.edituv_edges);
}

GPUBatch *DRW_mesh_batch_cache_get_edituv_verts(Mesh *me)
{
  MeshBatchCache *cache = mesh_batch_cache_get(me);
  edituv_request_active_uv(cache, me);
  mesh_batch_cache_add_request(cache, MBC_EDITUV_VERTS);
  return DRW_batch_request(&cache->batch.edituv_verts);
}

GPUBatch *DRW_mesh_batch_cache_get_edituv_facedots(Mesh *me)
{
  MeshBatchCache *cache = mesh_batch_cache_get(me);
  edituv_request_active_uv(cache, me);
  mesh_batch_cache_add_request(cache, MBC_EDITUV_FACEDOTS);
  return DRW_batch_request(&cache->batch.edituv_fdots);
}

GPUBatch *DRW_mesh_batch_cache_get_uv_edges(Mesh *me)
{
  MeshBatchCache *cache = mesh_batch_cache_get(me);
  edituv_request_active_uv(cache, me);
  mesh_batch_cache_add_request(cache, MBC_WIRE_LOOPS_UVS);
  return DRW_batch_request(&cache->batch.wire_loops_uvs);
}

GPUBatch *DRW_mesh_batch_cache_get_surface_edges(Mesh *me)
{
  MeshBatchCache *cache = mesh_batch_cache_get(me);
  texpaint_request_active_uv(cache, me);
  mesh_batch_cache_add_request(cache, MBC_WIRE_LOOPS);
  return DRW_batch_request(&cache->batch.wire_loops);
}

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Grouped batch generation
 * \{ */

/* Thread safety need to be assured by caller. Don't call this during drawing.
 * Note: For now this only free the shading batches / vbo if any cd layers is
 * not needed anymore. */
void DRW_mesh_batch_cache_free_old(Mesh *me, int ctime)
{
  MeshBatchCache *cache = me->runtime.batch_cache;

  if (cache == NULL) {
    return;
  }

  if (mesh_cd_layers_type_equal(cache->cd_used_over_time, cache->cd_used)) {
    cache->lastmatch = ctime;
  }

  if (ctime - cache->lastmatch > U.vbotimeout) {
    mesh_batch_cache_discard_shaded_tri(cache);
  }

  mesh_cd_layers_type_clear(&cache->cd_used_over_time);
}

/* Can be called for any surface type. Mesh *me is the final mesh. */
void DRW_mesh_batch_cache_create_requested(struct TaskGraph *task_graph,
                                           Object *ob,
                                           Mesh *me,
                                           const Scene *scene,
                                           const bool is_paint_mode,
                                           const bool use_hide)
{
  BLI_assert(task_graph);
  GPUIndexBuf **saved_elem_ranges = NULL;
  const ToolSettings *ts = NULL;
  if (scene) {
    ts = scene->toolsettings;
  }
  MeshBatchCache *cache = mesh_batch_cache_get(me);
  bool cd_uv_update = false;

  /* Early out */
  if (cache->batch_requested == 0) {
#ifdef DEBUG
    goto check;
#else
    return;
#endif
  }

  /* Sanity check. */
  if ((me->edit_mesh != NULL) && (ob->mode & OB_MODE_EDIT)) {
    BLI_assert(me->edit_mesh->mesh_eval_final != NULL);
  }

  const bool is_editmode = (me->edit_mesh != NULL) && DRW_object_is_in_edit_mode(ob);

  DRWBatchFlag batch_requested = cache->batch_requested;
  cache->batch_requested = 0;

  if (batch_requested & MBC_SURFACE_WEIGHTS) {
    /* Check vertex weights. */
    if ((cache->batch.surface_weights != NULL) && (ts != NULL)) {
      struct DRW_MeshWeightState wstate;
      BLI_assert(ob->type == OB_MESH);
      drw_mesh_weight_state_extract(ob, me, ts, is_paint_mode, &wstate);
      mesh_batch_cache_check_vertex_group(cache, &wstate);
      drw_mesh_weight_state_copy(&cache->weight_state, &wstate);
      drw_mesh_weight_state_clear(&wstate);
    }
  }

  /* HACK: if MBC_SURF_PER_MAT is requested and ibo.tris is already available, it won't have it's
   * index ranges initialized. So discard ibo.tris in order to recreate it.
   * This needs to happen before saved_elem_ranges is populated. */
  if ((batch_requested & MBC_SURF_PER_MAT) != 0 && (cache->batch_ready & MBC_SURF_PER_MAT) == 0) {
    FOREACH_MESH_BUFFER_CACHE (cache, mbuffercache) {
      GPU_INDEXBUF_DISCARD_SAFE(mbuffercache->ibo.tris);
    }
    /* Clear all batches that reference ibo.tris. */
    GPU_BATCH_CLEAR_SAFE(cache->batch.surface);
    GPU_BATCH_CLEAR_SAFE(cache->batch.surface_weights);
    GPU_BATCH_CLEAR_SAFE(cache->batch.edit_mesh_analysis);
    GPU_BATCH_CLEAR_SAFE(cache->batch.edit_triangles);
    GPU_BATCH_CLEAR_SAFE(cache->batch.edit_lnor);
    GPU_BATCH_CLEAR_SAFE(cache->batch.edit_selection_faces);
    for (int i = 0; i < cache->mat_len; i++) {
      GPU_BATCH_CLEAR_SAFE(cache->surface_per_mat[i]);
    }

    cache->batch_ready &= ~(MBC_SURFACE | MBC_SURFACE_WEIGHTS | MBC_EDIT_MESH_ANALYSIS |
                            MBC_EDIT_TRIANGLES | MBC_EDIT_LNOR | MBC_EDIT_SELECTION_FACES);
  }

  if (batch_requested &
      (MBC_SURFACE | MBC_SURF_PER_MAT | MBC_WIRE_LOOPS_UVS | MBC_EDITUV_FACES_STRETCH_AREA |
       MBC_EDITUV_FACES_STRETCH_ANGLE | MBC_EDITUV_FACES | MBC_EDITUV_EDGES | MBC_EDITUV_VERTS)) {
    /* Modifiers will only generate an orco layer if the mesh is deformed. */
    if (cache->cd_needed.orco != 0) {
      /* Orco is always extracted from final mesh. */
      Mesh *me_final = (me->edit_mesh) ? me->edit_mesh->mesh_eval_final : me;
      if (CustomData_get_layer(&me_final->vdata, CD_ORCO) == NULL) {
        /* Skip orco calculation */
        cache->cd_needed.orco = 0;
      }
    }

    /* Verify that all surface batches have needed attribute layers.
     */
    /* TODO(fclem): We could be a bit smarter here and only do it per
     * material. */
    bool cd_overlap = mesh_cd_layers_type_overlap(cache->cd_used, cache->cd_needed);
    if (cd_overlap == false) {
      FOREACH_MESH_BUFFER_CACHE (cache, mbuffercache) {
        if ((cache->cd_used.uv & cache->cd_needed.uv) != cache->cd_needed.uv) {
          GPU_VERTBUF_DISCARD_SAFE(mbuffercache->vbo.uv);
          cd_uv_update = true;
        }
        if ((cache->cd_used.tan & cache->cd_needed.tan) != cache->cd_needed.tan ||
            cache->cd_used.tan_orco != cache->cd_needed.tan_orco) {
          GPU_VERTBUF_DISCARD_SAFE(mbuffercache->vbo.tan);
        }
        if (cache->cd_used.orco != cache->cd_needed.orco) {
          GPU_VERTBUF_DISCARD_SAFE(mbuffercache->vbo.orco);
        }
        if (((cache->cd_used.vcol & cache->cd_needed.vcol) != cache->cd_needed.vcol) ||
            ((cache->cd_used.sculpt_vcol & cache->cd_needed.sculpt_vcol) !=
             cache->cd_needed.sculpt_vcol)) {
          GPU_VERTBUF_DISCARD_SAFE(mbuffercache->vbo.vcol);
        }
      }
      /* XXX save element buffer to avoid recreating them.
       * This is only if the cd_needed changes so it is ok to keep them.*/
      if (cache->surface_per_mat[0] && cache->surface_per_mat[0]->elem) {
        saved_elem_ranges = MEM_callocN(sizeof(saved_elem_ranges) * cache->mat_len, __func__);
        for (int i = 0; i < cache->mat_len; i++) {
          saved_elem_ranges[i] = cache->surface_per_mat[i]->elem;
          /* Avoid deletion as the batch is owner. */
          cache->surface_per_mat[i]->elem = NULL;
          cache->surface_per_mat[i]->owns_flag &= ~GPU_BATCH_OWNS_INDEX;
        }
      }
      /* We can't discard batches at this point as they have been
       * referenced for drawing. Just clear them in place. */
      for (int i = 0; i < cache->mat_len; i++) {
        GPU_BATCH_CLEAR_SAFE(cache->surface_per_mat[i]);
      }
      GPU_BATCH_CLEAR_SAFE(cache->batch.surface);
      cache->batch_ready &= ~(MBC_SURFACE | MBC_SURF_PER_MAT);

      mesh_cd_layers_type_merge(&cache->cd_used, cache->cd_needed);
    }
    mesh_cd_layers_type_merge(&cache->cd_used_over_time, cache->cd_needed);
    mesh_cd_layers_type_clear(&cache->cd_needed);
  }

  if (batch_requested & MBC_EDITUV) {
    /* Discard UV batches if sync_selection changes */
    const bool is_uvsyncsel = ts && (ts->uv_flag & UV_SYNC_SELECTION);
    if (cd_uv_update || (cache->is_uvsyncsel != is_uvsyncsel)) {
      cache->is_uvsyncsel = is_uvsyncsel;
      FOREACH_MESH_BUFFER_CACHE (cache, mbuffercache) {
        GPU_VERTBUF_DISCARD_SAFE(mbuffercache->vbo.edituv_data);
        GPU_VERTBUF_DISCARD_SAFE(mbuffercache->vbo.fdots_uv);
        GPU_INDEXBUF_DISCARD_SAFE(mbuffercache->ibo.edituv_tris);
        GPU_INDEXBUF_DISCARD_SAFE(mbuffercache->ibo.edituv_lines);
        GPU_INDEXBUF_DISCARD_SAFE(mbuffercache->ibo.edituv_points);
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
    goto check;
#else
    return;
#endif
  }

  cache->batch_ready |= batch_requested;

  const bool do_cage = (is_editmode &&
                        (me->edit_mesh->mesh_eval_final != me->edit_mesh->mesh_eval_cage));

  const bool do_uvcage = is_editmode && !me->edit_mesh->mesh_eval_final->runtime.is_original;

  MeshBufferCache *mbufcache = &cache->final;

  /* Initialize batches and request VBO's & IBO's. */
  if (DRW_batch_requested(cache->batch.surface, GPU_PRIM_TRIS)) {
    DRW_ibo_request(cache->batch.surface, &mbufcache->ibo.tris);
    /* Order matters. First ones override latest VBO's attributes. */
    DRW_vbo_request(cache->batch.surface, &mbufcache->vbo.lnor);
    DRW_vbo_request(cache->batch.surface, &mbufcache->vbo.pos_nor);
    if (cache->cd_used.uv != 0) {
      DRW_vbo_request(cache->batch.surface, &mbufcache->vbo.uv);
    }
    if (cache->cd_used.vcol != 0 || cache->cd_used.sculpt_vcol != 0) {
      DRW_vbo_request(cache->batch.surface, &mbufcache->vbo.vcol);
    }
  }
  if (DRW_batch_requested(cache->batch.all_verts, GPU_PRIM_POINTS)) {
    DRW_vbo_request(cache->batch.all_verts, &mbufcache->vbo.pos_nor);
  }
  if (DRW_batch_requested(cache->batch.all_edges, GPU_PRIM_LINES)) {
    DRW_ibo_request(cache->batch.all_edges, &mbufcache->ibo.lines);
    DRW_vbo_request(cache->batch.all_edges, &mbufcache->vbo.pos_nor);
  }
  if (DRW_batch_requested(cache->batch.loose_edges, GPU_PRIM_LINES)) {
    DRW_ibo_request(NULL, &mbufcache->ibo.lines);
    DRW_ibo_request(cache->batch.loose_edges, &mbufcache->ibo.lines_loose);
    DRW_vbo_request(cache->batch.loose_edges, &mbufcache->vbo.pos_nor);
  }
  if (DRW_batch_requested(cache->batch.edge_detection, GPU_PRIM_LINES_ADJ)) {
    DRW_ibo_request(cache->batch.edge_detection, &mbufcache->ibo.lines_adjacency);
    DRW_vbo_request(cache->batch.edge_detection, &mbufcache->vbo.pos_nor);
  }
  if (DRW_batch_requested(cache->batch.surface_weights, GPU_PRIM_TRIS)) {
    DRW_ibo_request(cache->batch.surface_weights, &mbufcache->ibo.tris);
    DRW_vbo_request(cache->batch.surface_weights, &mbufcache->vbo.pos_nor);
    DRW_vbo_request(cache->batch.surface_weights, &mbufcache->vbo.weights);
  }
  if (DRW_batch_requested(cache->batch.wire_loops, GPU_PRIM_LINES)) {
    DRW_ibo_request(cache->batch.wire_loops, &mbufcache->ibo.lines_paint_mask);
    /* Order matters. First ones override latest VBO's attributes. */
    DRW_vbo_request(cache->batch.wire_loops, &mbufcache->vbo.lnor);
    DRW_vbo_request(cache->batch.wire_loops, &mbufcache->vbo.pos_nor);
  }
  if (DRW_batch_requested(cache->batch.wire_edges, GPU_PRIM_LINES)) {
    DRW_ibo_request(cache->batch.wire_edges, &mbufcache->ibo.lines);
    DRW_vbo_request(cache->batch.wire_edges, &mbufcache->vbo.pos_nor);
    DRW_vbo_request(cache->batch.wire_edges, &mbufcache->vbo.edge_fac);
  }
  if (DRW_batch_requested(cache->batch.wire_loops_uvs, GPU_PRIM_LINES)) {
    DRW_ibo_request(cache->batch.wire_loops_uvs, &mbufcache->ibo.edituv_lines);
    /* For paint overlay. Active layer should have been queried. */
    if (cache->cd_used.uv != 0) {
      DRW_vbo_request(cache->batch.wire_loops_uvs, &mbufcache->vbo.uv);
    }
  }
  if (DRW_batch_requested(cache->batch.edit_mesh_analysis, GPU_PRIM_TRIS)) {
    DRW_ibo_request(cache->batch.edit_mesh_analysis, &mbufcache->ibo.tris);
    DRW_vbo_request(cache->batch.edit_mesh_analysis, &mbufcache->vbo.pos_nor);
    DRW_vbo_request(cache->batch.edit_mesh_analysis, &mbufcache->vbo.mesh_analysis);
  }

  /* Per Material */
  for (int i = 0; i < cache->mat_len; i++) {
    if (DRW_batch_requested(cache->surface_per_mat[i], GPU_PRIM_TRIS)) {
      if (saved_elem_ranges && saved_elem_ranges[i]) {
        /* XXX assign old element buffer range (it did not change).*/
        GPU_batch_elembuf_set(cache->surface_per_mat[i], saved_elem_ranges[i], true);
      }
      else {
        DRW_ibo_request(cache->surface_per_mat[i], &mbufcache->ibo.tris);
      }
      /* Order matters. First ones override latest VBO's attributes. */
      DRW_vbo_request(cache->surface_per_mat[i], &mbufcache->vbo.lnor);
      DRW_vbo_request(cache->surface_per_mat[i], &mbufcache->vbo.pos_nor);
      if (cache->cd_used.uv != 0) {
        DRW_vbo_request(cache->surface_per_mat[i], &mbufcache->vbo.uv);
      }
      if ((cache->cd_used.tan != 0) || (cache->cd_used.tan_orco != 0)) {
        DRW_vbo_request(cache->surface_per_mat[i], &mbufcache->vbo.tan);
      }
      if (cache->cd_used.vcol != 0 || cache->cd_used.sculpt_vcol != 0) {
        DRW_vbo_request(cache->surface_per_mat[i], &mbufcache->vbo.vcol);
      }
      if (cache->cd_used.orco != 0) {
        DRW_vbo_request(cache->surface_per_mat[i], &mbufcache->vbo.orco);
      }
    }
  }

  MEM_SAFE_FREE(saved_elem_ranges);

  mbufcache = (do_cage) ? &cache->cage : &cache->final;

  /* Edit Mesh */
  if (DRW_batch_requested(cache->batch.edit_triangles, GPU_PRIM_TRIS)) {
    DRW_ibo_request(cache->batch.edit_triangles, &mbufcache->ibo.tris);
    DRW_vbo_request(cache->batch.edit_triangles, &mbufcache->vbo.pos_nor);
    DRW_vbo_request(cache->batch.edit_triangles, &mbufcache->vbo.edit_data);
  }
  if (DRW_batch_requested(cache->batch.edit_vertices, GPU_PRIM_POINTS)) {
    DRW_ibo_request(cache->batch.edit_vertices, &mbufcache->ibo.points);
    DRW_vbo_request(cache->batch.edit_vertices, &mbufcache->vbo.pos_nor);
    DRW_vbo_request(cache->batch.edit_vertices, &mbufcache->vbo.edit_data);
  }
  if (DRW_batch_requested(cache->batch.edit_edges, GPU_PRIM_LINES)) {
    DRW_ibo_request(cache->batch.edit_edges, &mbufcache->ibo.lines);
    DRW_vbo_request(cache->batch.edit_edges, &mbufcache->vbo.pos_nor);
    DRW_vbo_request(cache->batch.edit_edges, &mbufcache->vbo.edit_data);
  }
  if (DRW_batch_requested(cache->batch.edit_vnor, GPU_PRIM_POINTS)) {
    DRW_ibo_request(cache->batch.edit_vnor, &mbufcache->ibo.points);
    DRW_vbo_request(cache->batch.edit_vnor, &mbufcache->vbo.pos_nor);
  }
  if (DRW_batch_requested(cache->batch.edit_lnor, GPU_PRIM_POINTS)) {
    DRW_ibo_request(cache->batch.edit_lnor, &mbufcache->ibo.tris);
    DRW_vbo_request(cache->batch.edit_lnor, &mbufcache->vbo.pos_nor);
    DRW_vbo_request(cache->batch.edit_lnor, &mbufcache->vbo.lnor);
  }
  if (DRW_batch_requested(cache->batch.edit_fdots, GPU_PRIM_POINTS)) {
    DRW_ibo_request(cache->batch.edit_fdots, &mbufcache->ibo.fdots);
    DRW_vbo_request(cache->batch.edit_fdots, &mbufcache->vbo.fdots_pos);
    DRW_vbo_request(cache->batch.edit_fdots, &mbufcache->vbo.fdots_nor);
  }
  if (DRW_batch_requested(cache->batch.edit_skin_roots, GPU_PRIM_POINTS)) {
    DRW_vbo_request(cache->batch.edit_skin_roots, &mbufcache->vbo.skin_roots);
  }

  /* Selection */
  if (DRW_batch_requested(cache->batch.edit_selection_verts, GPU_PRIM_POINTS)) {
    DRW_ibo_request(cache->batch.edit_selection_verts, &mbufcache->ibo.points);
    DRW_vbo_request(cache->batch.edit_selection_verts, &mbufcache->vbo.pos_nor);
    DRW_vbo_request(cache->batch.edit_selection_verts, &mbufcache->vbo.vert_idx);
  }
  if (DRW_batch_requested(cache->batch.edit_selection_edges, GPU_PRIM_LINES)) {
    DRW_ibo_request(cache->batch.edit_selection_edges, &mbufcache->ibo.lines);
    DRW_vbo_request(cache->batch.edit_selection_edges, &mbufcache->vbo.pos_nor);
    DRW_vbo_request(cache->batch.edit_selection_edges, &mbufcache->vbo.edge_idx);
  }
  if (DRW_batch_requested(cache->batch.edit_selection_faces, GPU_PRIM_TRIS)) {
    DRW_ibo_request(cache->batch.edit_selection_faces, &mbufcache->ibo.tris);
    DRW_vbo_request(cache->batch.edit_selection_faces, &mbufcache->vbo.pos_nor);
    DRW_vbo_request(cache->batch.edit_selection_faces, &mbufcache->vbo.poly_idx);
  }
  if (DRW_batch_requested(cache->batch.edit_selection_fdots, GPU_PRIM_POINTS)) {
    DRW_ibo_request(cache->batch.edit_selection_fdots, &mbufcache->ibo.fdots);
    DRW_vbo_request(cache->batch.edit_selection_fdots, &mbufcache->vbo.fdots_pos);
    DRW_vbo_request(cache->batch.edit_selection_fdots, &mbufcache->vbo.fdot_idx);
  }

  /**
   * TODO: The code and data structure is ready to support modified UV display
   * but the selection code for UVs needs to support it first. So for now, only
   * display the cage in all cases.
   */
  mbufcache = (do_uvcage) ? &cache->uv_cage : &cache->final;

  /* Edit UV */
  if (DRW_batch_requested(cache->batch.edituv_faces, GPU_PRIM_TRIS)) {
    DRW_ibo_request(cache->batch.edituv_faces, &mbufcache->ibo.edituv_tris);
    DRW_vbo_request(cache->batch.edituv_faces, &mbufcache->vbo.uv);
    DRW_vbo_request(cache->batch.edituv_faces, &mbufcache->vbo.edituv_data);
  }
  if (DRW_batch_requested(cache->batch.edituv_faces_stretch_area, GPU_PRIM_TRIS)) {
    DRW_ibo_request(cache->batch.edituv_faces_stretch_area, &mbufcache->ibo.edituv_tris);
    DRW_vbo_request(cache->batch.edituv_faces_stretch_area, &mbufcache->vbo.uv);
    DRW_vbo_request(cache->batch.edituv_faces_stretch_area, &mbufcache->vbo.edituv_data);
    DRW_vbo_request(cache->batch.edituv_faces_stretch_area, &mbufcache->vbo.stretch_area);
  }
  if (DRW_batch_requested(cache->batch.edituv_faces_stretch_angle, GPU_PRIM_TRIS)) {
    DRW_ibo_request(cache->batch.edituv_faces_stretch_angle, &mbufcache->ibo.edituv_tris);
    DRW_vbo_request(cache->batch.edituv_faces_stretch_angle, &mbufcache->vbo.uv);
    DRW_vbo_request(cache->batch.edituv_faces_stretch_angle, &mbufcache->vbo.edituv_data);
    DRW_vbo_request(cache->batch.edituv_faces_stretch_angle, &mbufcache->vbo.stretch_angle);
  }
  if (DRW_batch_requested(cache->batch.edituv_edges, GPU_PRIM_LINES)) {
    DRW_ibo_request(cache->batch.edituv_edges, &mbufcache->ibo.edituv_lines);
    DRW_vbo_request(cache->batch.edituv_edges, &mbufcache->vbo.uv);
    DRW_vbo_request(cache->batch.edituv_edges, &mbufcache->vbo.edituv_data);
  }
  if (DRW_batch_requested(cache->batch.edituv_verts, GPU_PRIM_POINTS)) {
    DRW_ibo_request(cache->batch.edituv_verts, &mbufcache->ibo.edituv_points);
    DRW_vbo_request(cache->batch.edituv_verts, &mbufcache->vbo.uv);
    DRW_vbo_request(cache->batch.edituv_verts, &mbufcache->vbo.edituv_data);
  }
  if (DRW_batch_requested(cache->batch.edituv_fdots, GPU_PRIM_POINTS)) {
    DRW_ibo_request(cache->batch.edituv_fdots, &mbufcache->ibo.edituv_fdots);
    DRW_vbo_request(cache->batch.edituv_fdots, &mbufcache->vbo.fdots_uv);
    DRW_vbo_request(cache->batch.edituv_fdots, &mbufcache->vbo.fdots_edituv_data);
  }

  /* Meh loose Scene const correctness here. */
  const bool use_subsurf_fdots = scene ? BKE_modifiers_uses_subsurf_facedots((Scene *)scene, ob) :
                                         false;

  if (do_uvcage) {
    mesh_buffer_cache_create_requested(task_graph,
                                       cache,
                                       cache->uv_cage,
                                       me,
                                       is_editmode,
                                       is_paint_mode,
                                       ob->obmat,
                                       false,
                                       true,
                                       false,
                                       &cache->cd_used,
                                       scene,
                                       ts,
                                       true);
  }

  if (do_cage) {
    mesh_buffer_cache_create_requested(task_graph,
                                       cache,
                                       cache->cage,
                                       me,
                                       is_editmode,
                                       is_paint_mode,
                                       ob->obmat,
                                       false,
                                       false,
                                       use_subsurf_fdots,
                                       &cache->cd_used,
                                       scene,
                                       ts,
                                       true);
  }

  mesh_buffer_cache_create_requested(task_graph,
                                     cache,
                                     cache->final,
                                     me,
                                     is_editmode,
                                     is_paint_mode,
                                     ob->obmat,
                                     true,
                                     false,
                                     use_subsurf_fdots,
                                     &cache->cd_used,
                                     scene,
                                     ts,
                                     use_hide);
#ifdef DEBUG
check:
  /* Make sure all requested batches have been setup. */
  /* TODO(jbakker): we should move this to the draw_manager but that needs refactoring and
   * additional looping.*/
  BLI_task_graph_work_and_wait(task_graph);
  for (int i = 0; i < sizeof(cache->batch) / sizeof(void *); i++) {
    BLI_assert(!DRW_batch_requested(((GPUBatch **)&cache->batch)[i], 0));
  }
  for (int i = 0; i < sizeof(cache->final.vbo) / sizeof(void *); i++) {
    BLI_assert(!DRW_vbo_requested(((GPUVertBuf **)&cache->final.vbo)[i]));
  }
  for (int i = 0; i < sizeof(cache->final.ibo) / sizeof(void *); i++) {
    BLI_assert(!DRW_ibo_requested(((GPUIndexBuf **)&cache->final.ibo)[i]));
  }
  for (int i = 0; i < sizeof(cache->cage.vbo) / sizeof(void *); i++) {
    BLI_assert(!DRW_vbo_requested(((GPUVertBuf **)&cache->cage.vbo)[i]));
  }
  for (int i = 0; i < sizeof(cache->cage.ibo) / sizeof(void *); i++) {
    BLI_assert(!DRW_ibo_requested(((GPUIndexBuf **)&cache->cage.ibo)[i]));
  }
  for (int i = 0; i < sizeof(cache->uv_cage.vbo) / sizeof(void *); i++) {
    BLI_assert(!DRW_vbo_requested(((GPUVertBuf **)&cache->uv_cage.vbo)[i]));
  }
  for (int i = 0; i < sizeof(cache->uv_cage.ibo) / sizeof(void *); i++) {
    BLI_assert(!DRW_ibo_requested(((GPUIndexBuf **)&cache->uv_cage.ibo)[i]));
  }
#endif
}

/** \} */
