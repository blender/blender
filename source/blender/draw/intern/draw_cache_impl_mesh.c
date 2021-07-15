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
#include "BKE_paint.h"
#include "BKE_pbvh.h"

#include "atomic_ops.h"

#include "bmesh.h"

#include "GPU_batch.h"
#include "GPU_material.h"

#include "DRW_render.h"

#include "ED_mesh.h"
#include "ED_uvedit.h"

#include "draw_cache_extract.h"
#include "draw_cache_extract_mesh_private.h"
#include "draw_cache_inline.h"

#include "draw_cache_impl.h" /* own include */

/* ---------------------------------------------------------------------- */
/** \name Dependencies between buffer and batch
 * \{ */

/* clang-format off */

#define _BUFFER_INDEX(buff_name) ((offsetof(MeshBufferCache, buff_name) - offsetof(MeshBufferCache, vbo)) / sizeof(void *))

#define _MDEPS_CREATE1(b) (1u << MBC_BATCH_INDEX(b))
#define _MDEPS_CREATE2(b1, b2) _MDEPS_CREATE1(b1) | _MDEPS_CREATE1(b2)
#define _MDEPS_CREATE3(b1, b2, b3) _MDEPS_CREATE2(b1, b2) | _MDEPS_CREATE1(b3)
#define _MDEPS_CREATE4(b1, b2, b3, b4) _MDEPS_CREATE3(b1, b2, b3) | _MDEPS_CREATE1(b4)
#define _MDEPS_CREATE5(b1, b2, b3, b4, b5) _MDEPS_CREATE4(b1, b2, b3, b4) | _MDEPS_CREATE1(b5)
#define _MDEPS_CREATE6(b1, b2, b3, b4, b5, b6) _MDEPS_CREATE5(b1, b2, b3, b4, b5) | _MDEPS_CREATE1(b6)
#define _MDEPS_CREATE7(b1, b2, b3, b4, b5, b6, b7) _MDEPS_CREATE6(b1, b2, b3, b4, b5, b6) | _MDEPS_CREATE1(b7)
#define _MDEPS_CREATE8(b1, b2, b3, b4, b5, b6, b7, b8) _MDEPS_CREATE7(b1, b2, b3, b4, b5, b6, b7) | _MDEPS_CREATE1(b8)
#define _MDEPS_CREATE9(b1, b2, b3, b4, b5, b6, b7, b8, b9) _MDEPS_CREATE8(b1, b2, b3, b4, b5, b6, b7, b8) | _MDEPS_CREATE1(b9)
#define _MDEPS_CREATE10(b1, b2, b3, b4, b5, b6, b7, b8, b9, b10) _MDEPS_CREATE9(b1, b2, b3, b4, b5, b6, b7, b8, b9) | _MDEPS_CREATE1(b10)
#define _MDEPS_CREATE19(b1, b2, b3, b4, b5, b6, b7, b8, b9, b10, b11, b12, b13, b14, b15, b16, b17, b18, b19) _MDEPS_CREATE10(b1, b2, b3, b4, b5, b6, b7, b8, b9, b10) | _MDEPS_CREATE9(b11, b12, b13, b14, b15, b16, b17, b18, b19)

#define MDEPS_CREATE(buff_name, ...) [_BUFFER_INDEX(buff_name)] = VA_NARGS_CALL_OVERLOAD(_MDEPS_CREATE, __VA_ARGS__)

#define _MDEPS_CREATE_MAP1(a) g_buffer_desps[_BUFFER_INDEX(a)]
#define _MDEPS_CREATE_MAP2(a, b) _MDEPS_CREATE_MAP1(a) | _MDEPS_CREATE_MAP1(b)
#define _MDEPS_CREATE_MAP3(a, b, c) _MDEPS_CREATE_MAP2(a, b) | _MDEPS_CREATE_MAP1(c)
#define _MDEPS_CREATE_MAP4(a, b, c, d) _MDEPS_CREATE_MAP3(a, b, c) | _MDEPS_CREATE_MAP1(d)
#define _MDEPS_CREATE_MAP5(a, b, c, d, e) _MDEPS_CREATE_MAP4(a, b, c, d) | _MDEPS_CREATE_MAP1(e)
#define _MDEPS_CREATE_MAP6(a, b, c, d, e, f) _MDEPS_CREATE_MAP5(a, b, c, d, e) | _MDEPS_CREATE_MAP1(f)
#define _MDEPS_CREATE_MAP7(a, b, c, d, e, f, g) _MDEPS_CREATE_MAP6(a, b, c, d, e, f) | _MDEPS_CREATE_MAP1(g)
#define _MDEPS_CREATE_MAP8(a, b, c, d, e, f, g, h) _MDEPS_CREATE_MAP7(a, b, c, d, e, f, g) | _MDEPS_CREATE_MAP1(h)
#define _MDEPS_CREATE_MAP9(a, b, c, d, e, f, g, h, i) _MDEPS_CREATE_MAP8(a, b, c, d, e, f, g, h) | _MDEPS_CREATE_MAP1(i)
#define _MDEPS_CREATE_MAP10(a, b, c, d, e, f, g, h, i, j) _MDEPS_CREATE_MAP9(a, b, c, d, e, f, g, h, i) | _MDEPS_CREATE_MAP1(j)

#define MDEPS_CREATE_MAP(...) VA_NARGS_CALL_OVERLOAD(_MDEPS_CREATE_MAP, __VA_ARGS__)

#ifndef NDEBUG
#  define _MDEPS_ASSERT2(b, name) \
    g_buffer_desps_d[_BUFFER_INDEX(name)] |= _MDEPS_CREATE1(b); \
    BLI_assert(g_buffer_desps[_BUFFER_INDEX(name)] & _MDEPS_CREATE1(b))
#  define _MDEPS_ASSERT3(b, n1, n2) _MDEPS_ASSERT2(b, n1); _MDEPS_ASSERT2(b, n2)
#  define _MDEPS_ASSERT4(b, n1, n2, n3) _MDEPS_ASSERT3(b, n1, n2); _MDEPS_ASSERT2(b, n3)
#  define _MDEPS_ASSERT5(b, n1, n2, n3, n4) _MDEPS_ASSERT4(b, n1, n2, n3); _MDEPS_ASSERT2(b, n4)
#  define _MDEPS_ASSERT6(b, n1, n2, n3, n4, n5) _MDEPS_ASSERT5(b, n1, n2, n3, n4); _MDEPS_ASSERT2(b, n5)
#  define _MDEPS_ASSERT7(b, n1, n2, n3, n4, n5, n6) _MDEPS_ASSERT6(b, n1, n2, n3, n4, n5); _MDEPS_ASSERT2(b, n6)
#  define _MDEPS_ASSERT8(b, n1, n2, n3, n4, n5, n6, n7) _MDEPS_ASSERT7(b, n1, n2, n3, n4, n5, n6); _MDEPS_ASSERT2(b, n7)

#  define MDEPS_ASSERT(...) VA_NARGS_CALL_OVERLOAD(_MDEPS_ASSERT, __VA_ARGS__)
#  define MDEPS_ASSERT_MAP(name) BLI_assert(g_buffer_desps_d[_BUFFER_INDEX(name)] == g_buffer_desps[_BUFFER_INDEX(name)])
#else
#  define MDEPS_ASSERT(...)
#  define MDEPS_ASSERT_MAP(name)
#endif

/* clang-format on */

static const DRWBatchFlag g_buffer_desps[] = {
    MDEPS_CREATE(vbo.pos_nor,
                 batch.surface,
                 batch.surface_weights,
                 batch.edit_triangles,
                 batch.edit_vertices,
                 batch.edit_edges,
                 batch.edit_vnor,
                 batch.edit_lnor,
                 batch.edit_mesh_analysis,
                 batch.edit_selection_verts,
                 batch.edit_selection_edges,
                 batch.edit_selection_faces,
                 batch.all_verts,
                 batch.all_edges,
                 batch.loose_edges,
                 batch.edge_detection,
                 batch.wire_edges,
                 batch.wire_loops,
                 batch.sculpt_overlays,
                 surface_per_mat),
    MDEPS_CREATE(vbo.lnor, batch.surface, batch.edit_lnor, batch.wire_loops, surface_per_mat),
    MDEPS_CREATE(vbo.edge_fac, batch.wire_edges),
    MDEPS_CREATE(vbo.weights, batch.surface_weights),
    MDEPS_CREATE(vbo.uv,
                 batch.surface,
                 batch.edituv_faces_stretch_area,
                 batch.edituv_faces_stretch_angle,
                 batch.edituv_faces,
                 batch.edituv_edges,
                 batch.edituv_verts,
                 batch.wire_loops_uvs,
                 surface_per_mat),
    MDEPS_CREATE(vbo.tan, surface_per_mat),
    MDEPS_CREATE(vbo.vcol, batch.surface, surface_per_mat),
    MDEPS_CREATE(vbo.sculpt_data, batch.sculpt_overlays),
    MDEPS_CREATE(vbo.orco, surface_per_mat),
    MDEPS_CREATE(vbo.edit_data, batch.edit_triangles, batch.edit_edges, batch.edit_vertices),
    MDEPS_CREATE(vbo.edituv_data,
                 batch.edituv_faces,
                 batch.edituv_faces_stretch_area,
                 batch.edituv_faces_stretch_angle,
                 batch.edituv_edges,
                 batch.edituv_verts),
    MDEPS_CREATE(vbo.edituv_stretch_area, batch.edituv_faces_stretch_area),
    MDEPS_CREATE(vbo.edituv_stretch_angle, batch.edituv_faces_stretch_angle),
    MDEPS_CREATE(vbo.mesh_analysis, batch.edit_mesh_analysis),
    MDEPS_CREATE(vbo.fdots_pos, batch.edit_fdots, batch.edit_selection_fdots),
    MDEPS_CREATE(vbo.fdots_nor, batch.edit_fdots),
    MDEPS_CREATE(vbo.fdots_uv, batch.edituv_fdots),
    MDEPS_CREATE(vbo.fdots_edituv_data, batch.edituv_fdots),
    MDEPS_CREATE(vbo.skin_roots, batch.edit_skin_roots),
    MDEPS_CREATE(vbo.vert_idx, batch.edit_selection_verts),
    MDEPS_CREATE(vbo.edge_idx, batch.edit_selection_edges),
    MDEPS_CREATE(vbo.poly_idx, batch.edit_selection_faces),
    MDEPS_CREATE(vbo.fdot_idx, batch.edit_selection_fdots),

    MDEPS_CREATE(ibo.tris,
                 batch.surface,
                 batch.surface_weights,
                 batch.edit_triangles,
                 batch.edit_lnor,
                 batch.edit_mesh_analysis,
                 batch.edit_selection_faces,
                 batch.sculpt_overlays),
    MDEPS_CREATE(ibo.lines,
                 batch.edit_edges,
                 batch.edit_selection_edges,
                 batch.all_edges,
                 batch.wire_edges),
    MDEPS_CREATE(ibo.lines_loose, batch.loose_edges),
    MDEPS_CREATE(ibo.points, batch.edit_vnor, batch.edit_vertices, batch.edit_selection_verts),
    MDEPS_CREATE(ibo.fdots, batch.edit_fdots, batch.edit_selection_fdots),
    MDEPS_CREATE(ibo.lines_paint_mask, batch.wire_loops),
    MDEPS_CREATE(ibo.lines_adjacency, batch.edge_detection),
    MDEPS_CREATE(ibo.edituv_tris,
                 batch.edituv_faces,
                 batch.edituv_faces_stretch_area,
                 batch.edituv_faces_stretch_angle),
    MDEPS_CREATE(ibo.edituv_lines, batch.edituv_edges, batch.wire_loops_uvs),
    MDEPS_CREATE(ibo.edituv_points, batch.edituv_verts),
    MDEPS_CREATE(ibo.edituv_fdots, batch.edituv_fdots),

    MDEPS_CREATE(tris_per_mat, surface_per_mat),
};

#ifndef NDEBUG
static DRWBatchFlag g_buffer_desps_d[sizeof(g_buffer_desps)] = {0};
#endif

static void mesh_batch_cache_discard_surface_batches(MeshBatchCache *cache);
static void mesh_batch_cache_clear(Mesh *me);

static void mesh_batch_cache_discard_batch(MeshBatchCache *cache, const DRWBatchFlag batch_map)
{
  for (int i = 0; i < MBC_BATCH_LEN; i++) {
    DRWBatchFlag batch_requested = (1u << i);
    if (batch_map & batch_requested) {
      GPU_BATCH_DISCARD_SAFE(((GPUBatch **)&cache->batch)[i]);
      cache->batch_ready &= ~batch_requested;
    }
  }

  if (batch_map & (1u << MBC_BATCH_INDEX(surface_per_mat))) {
    mesh_batch_cache_discard_surface_batches(cache);
  }
}

/* Return true is all layers in _b_ are inside _a_. */
BLI_INLINE bool mesh_cd_layers_type_overlap(DRW_MeshCDMask a, DRW_MeshCDMask b)
{
  return (*((uint64_t *)&a) & *((uint64_t *)&b)) == *((uint64_t *)&b);
}

BLI_INLINE bool mesh_cd_layers_type_equal(DRW_MeshCDMask a, DRW_MeshCDMask b)
{
  return *((uint64_t *)&a) == *((uint64_t *)&b);
}

BLI_INLINE void mesh_cd_layers_type_merge(DRW_MeshCDMask *a, DRW_MeshCDMask b)
{
  uint32_t *a_p = (uint32_t *)a;
  uint32_t *b_p = (uint32_t *)&b;
  atomic_fetch_and_or_uint32(a_p, *b_p);
  atomic_fetch_and_or_uint32(a_p + 1, *(b_p + 1));
}

BLI_INLINE void mesh_cd_layers_type_clear(DRW_MeshCDMask *a)
{
  *((uint64_t *)a) = 0;
}

BLI_INLINE const Mesh *editmesh_final_or_this(const Mesh *me)
{
  return (me->edit_mesh && me->edit_mesh->mesh_eval_final) ? me->edit_mesh->mesh_eval_final : me;
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
  const Mesh *me_final = editmesh_final_or_this(me);
  const CustomData *cd_ldata = mesh_cd_ldata_get_from_mesh(me_final);
  int layer = CustomData_get_active_layer(cd_ldata, CD_MLOOPUV);
  if (layer != -1) {
    cd_used->uv |= (1 << layer);
  }
}

static void mesh_cd_calc_active_mask_uv_layer(const Mesh *me, DRW_MeshCDMask *cd_used)
{
  const Mesh *me_final = editmesh_final_or_this(me);
  const CustomData *cd_ldata = mesh_cd_ldata_get_from_mesh(me_final);
  int layer = CustomData_get_stencil_layer(cd_ldata, CD_MLOOPUV);
  if (layer != -1) {
    cd_used->uv |= (1 << layer);
  }
}

static void mesh_cd_calc_active_vcol_layer(const Mesh *me, DRW_MeshCDMask *cd_used)
{
  const Mesh *me_final = editmesh_final_or_this(me);
  const CustomData *cd_vdata = mesh_cd_vdata_get_from_mesh(me_final);

  int layer = CustomData_get_active_layer(cd_vdata, CD_PROP_COLOR);
  if (layer != -1) {
    cd_used->sculpt_vcol |= (1 << layer);
  }
}

static void mesh_cd_calc_active_mloopcol_layer(const Mesh *me, DRW_MeshCDMask *cd_used)
{
  const Mesh *me_final = editmesh_final_or_this(me);
  const CustomData *cd_ldata = mesh_cd_ldata_get_from_mesh(me_final);

  int layer = CustomData_get_active_layer(cd_ldata, CD_MLOOPCOL);
  if (layer != -1) {
    cd_used->vcol |= (1 << layer);
  }
}

static DRW_MeshCDMask mesh_cd_calc_used_gpu_layers(const Mesh *me,
                                                   struct GPUMaterial **gpumat_array,
                                                   int gpumat_array_len)
{
  const Mesh *me_final = editmesh_final_or_this(me);
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
              if (U.experimental.use_sculpt_vertex_colors) {
                layer = CustomData_get_named_layer(cd_vdata, CD_PROP_COLOR, name);
                type = CD_PROP_COLOR;
              }
            }

            if (layer == -1) {
              layer = CustomData_get_named_layer(cd_ldata, CD_MLOOPCOL, name);
              type = CD_MCOL;
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
            bool use_mloop_cols = false;
            if (layer == -1) {
              layer = (name[0] != '\0') ?
                          CustomData_get_named_layer(cd_vdata, CD_PROP_COLOR, name) :
                          CustomData_get_render_layer(cd_vdata, CD_PROP_COLOR);
              /* Fallback to Vertex Color data */
              if (layer == -1) {
                layer = (name[0] != '\0') ?
                            CustomData_get_named_layer(cd_ldata, CD_MLOOPCOL, name) :
                            CustomData_get_render_layer(cd_ldata, CD_MLOOPCOL);
                use_mloop_cols = true;
              }
            }
            if (layer != -1) {
              if (use_mloop_cols) {
                cd_used.vcol |= (1 << layer);
              }
              else {
                cd_used.sculpt_vcol |= (1 << layer);
              }
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
  cache->final.tris_per_mat = MEM_callocN(sizeof(*cache->final.tris_per_mat) * cache->mat_len,
                                          __func__);

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

static void mesh_batch_cache_request_surface_batches(MeshBatchCache *cache)
{
  mesh_batch_cache_add_request(cache, MBC_SURFACE);
  DRW_batch_request(&cache->batch.surface);
  for (int i = 0; i < cache->mat_len; i++) {
    DRW_batch_request(&cache->surface_per_mat[i]);
  }
}

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
  FOREACH_MESH_BUFFER_CACHE (cache, mbufcache) {
    GPU_VERTBUF_DISCARD_SAFE(mbufcache->vbo.uv);
    GPU_VERTBUF_DISCARD_SAFE(mbufcache->vbo.tan);
    GPU_VERTBUF_DISCARD_SAFE(mbufcache->vbo.vcol);
    GPU_VERTBUF_DISCARD_SAFE(mbufcache->vbo.orco);
  }
  DRWBatchFlag batch_map = MDEPS_CREATE_MAP(vbo.uv, vbo.tan, vbo.vcol, vbo.orco);
  mesh_batch_cache_discard_batch(cache, batch_map);
  mesh_cd_layers_type_clear(&cache->cd_used);
}

static void mesh_batch_cache_discard_uvedit(MeshBatchCache *cache)
{
  FOREACH_MESH_BUFFER_CACHE (cache, mbufcache) {
    GPU_VERTBUF_DISCARD_SAFE(mbufcache->vbo.edituv_stretch_angle);
    GPU_VERTBUF_DISCARD_SAFE(mbufcache->vbo.edituv_stretch_area);
    GPU_VERTBUF_DISCARD_SAFE(mbufcache->vbo.uv);
    GPU_VERTBUF_DISCARD_SAFE(mbufcache->vbo.edituv_data);
    GPU_VERTBUF_DISCARD_SAFE(mbufcache->vbo.fdots_uv);
    GPU_VERTBUF_DISCARD_SAFE(mbufcache->vbo.fdots_edituv_data);
    GPU_INDEXBUF_DISCARD_SAFE(mbufcache->ibo.edituv_tris);
    GPU_INDEXBUF_DISCARD_SAFE(mbufcache->ibo.edituv_lines);
    GPU_INDEXBUF_DISCARD_SAFE(mbufcache->ibo.edituv_points);
    GPU_INDEXBUF_DISCARD_SAFE(mbufcache->ibo.edituv_fdots);
  }
  DRWBatchFlag batch_map = MDEPS_CREATE_MAP(vbo.edituv_stretch_angle,
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
  FOREACH_MESH_BUFFER_CACHE (cache, mbufcache) {
    GPU_VERTBUF_DISCARD_SAFE(mbufcache->vbo.edituv_data);
    GPU_VERTBUF_DISCARD_SAFE(mbufcache->vbo.fdots_edituv_data);
    GPU_INDEXBUF_DISCARD_SAFE(mbufcache->ibo.edituv_tris);
    GPU_INDEXBUF_DISCARD_SAFE(mbufcache->ibo.edituv_lines);
    GPU_INDEXBUF_DISCARD_SAFE(mbufcache->ibo.edituv_points);
    GPU_INDEXBUF_DISCARD_SAFE(mbufcache->ibo.edituv_fdots);
  }
  DRWBatchFlag batch_map = MDEPS_CREATE_MAP(vbo.edituv_data,
                                            vbo.fdots_edituv_data,
                                            ibo.edituv_tris,
                                            ibo.edituv_lines,
                                            ibo.edituv_points,
                                            ibo.edituv_fdots);
  mesh_batch_cache_discard_batch(cache, batch_map);
}

void DRW_mesh_batch_cache_dirty_tag(Mesh *me, eMeshBatchDirtyMode mode)
{
  MeshBatchCache *cache = me->runtime.batch_cache;
  if (cache == NULL) {
    return;
  }
  DRWBatchFlag batch_map;
  switch (mode) {
    case BKE_MESH_BATCH_DIRTY_SELECT:
      FOREACH_MESH_BUFFER_CACHE (cache, mbufcache) {
        GPU_VERTBUF_DISCARD_SAFE(mbufcache->vbo.edit_data);
        GPU_VERTBUF_DISCARD_SAFE(mbufcache->vbo.fdots_nor);
      }
      batch_map = MDEPS_CREATE_MAP(vbo.edit_data, vbo.fdots_nor);
      mesh_batch_cache_discard_batch(cache, batch_map);

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
      batch_map = MDEPS_CREATE_MAP(ibo.lines_paint_mask, vbo.pos_nor, vbo.lnor);
      mesh_batch_cache_discard_batch(cache, batch_map);
      break;
    case BKE_MESH_BATCH_DIRTY_ALL:
      cache->is_dirty = true;
      break;
    case BKE_MESH_BATCH_DIRTY_SHADING:
      mesh_batch_cache_discard_shaded_tri(cache);
      mesh_batch_cache_discard_uvedit(cache);
      break;
    case BKE_MESH_BATCH_DIRTY_DEFORM:
      FOREACH_MESH_BUFFER_CACHE (cache, mbufcache) {
        GPU_VERTBUF_DISCARD_SAFE(mbufcache->vbo.pos_nor);
        GPU_VERTBUF_DISCARD_SAFE(mbufcache->vbo.lnor);
        GPU_VERTBUF_DISCARD_SAFE(mbufcache->vbo.fdots_pos);
        GPU_VERTBUF_DISCARD_SAFE(mbufcache->vbo.fdots_nor);
        GPU_INDEXBUF_DISCARD_SAFE(mbufcache->ibo.tris);
      }
      batch_map = MDEPS_CREATE_MAP(vbo.pos_nor, vbo.lnor, vbo.fdots_pos, vbo.fdots_nor, ibo.tris);
      mesh_batch_cache_discard_batch(cache, batch_map);
      break;
    case BKE_MESH_BATCH_DIRTY_UVEDIT_ALL:
      mesh_batch_cache_discard_uvedit(cache);
      break;
    case BKE_MESH_BATCH_DIRTY_UVEDIT_SELECT:
      FOREACH_MESH_BUFFER_CACHE (cache, mbufcache) {
        GPU_VERTBUF_DISCARD_SAFE(mbufcache->vbo.edituv_data);
        GPU_VERTBUF_DISCARD_SAFE(mbufcache->vbo.fdots_edituv_data);
      }
      batch_map = MDEPS_CREATE_MAP(vbo.edituv_data, vbo.fdots_edituv_data);
      mesh_batch_cache_discard_batch(cache, batch_map);
      break;
    default:
      BLI_assert(0);
  }
}

static void mesh_buffer_cache_clear(MeshBufferCache *mbufcache)
{
  GPUVertBuf **vbos = (GPUVertBuf **)&mbufcache->vbo;
  GPUIndexBuf **ibos = (GPUIndexBuf **)&mbufcache->ibo;
  for (int i = 0; i < sizeof(mbufcache->vbo) / sizeof(void *); i++) {
    GPU_VERTBUF_DISCARD_SAFE(vbos[i]);
  }
  for (int i = 0; i < sizeof(mbufcache->ibo) / sizeof(void *); i++) {
    GPU_INDEXBUF_DISCARD_SAFE(ibos[i]);
  }
}

static void mesh_buffer_extraction_cache_clear(MeshBufferExtractionCache *extraction_cache)
{
  MEM_SAFE_FREE(extraction_cache->loose_geom.verts);
  MEM_SAFE_FREE(extraction_cache->loose_geom.edges);
  extraction_cache->loose_geom.edge_len = 0;
  extraction_cache->loose_geom.vert_len = 0;

  MEM_SAFE_FREE(extraction_cache->mat_offsets.tri);
}

static void mesh_batch_cache_clear(Mesh *me)
{
  MeshBatchCache *cache = me->runtime.batch_cache;
  if (!cache) {
    return;
  }
  FOREACH_MESH_BUFFER_CACHE (cache, mbufcache) {
    mesh_buffer_cache_clear(mbufcache);
  }

  mesh_buffer_extraction_cache_clear(&cache->final_extraction_cache);
  mesh_buffer_extraction_cache_clear(&cache->cage_extraction_cache);
  mesh_buffer_extraction_cache_clear(&cache->uv_cage_extraction_cache);

  for (int i = 0; i < cache->mat_len; i++) {
    GPU_INDEXBUF_DISCARD_SAFE(cache->final.tris_per_mat[i]);
  }
  MEM_SAFE_FREE(cache->final.tris_per_mat);

  for (int i = 0; i < sizeof(cache->batch) / sizeof(void *); i++) {
    GPUBatch **batch = (GPUBatch **)&cache->batch;
    GPU_BATCH_DISCARD_SAFE(batch[i]);
  }

  mesh_batch_cache_discard_shaded_tri(cache);
  mesh_batch_cache_discard_uvedit(cache);
  MEM_SAFE_FREE(cache->surface_per_mat);
  cache->mat_len = 0;

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
  mesh_batch_cache_request_surface_batches(cache);
  return cache->batch.surface;
}

GPUBatch *DRW_mesh_batch_cache_get_loose_edges(Mesh *me)
{
  MeshBatchCache *cache = mesh_batch_cache_get(me);
  mesh_batch_cache_add_request(cache, MBC_LOOSE_EDGES);
  if (cache->no_loose_wire) {
    return NULL;
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

GPUBatch **DRW_mesh_batch_cache_get_surface_shaded(Mesh *me,
                                                   struct GPUMaterial **gpumat_array,
                                                   uint gpumat_array_len)
{
  MeshBatchCache *cache = mesh_batch_cache_get(me);
  DRW_MeshCDMask cd_needed = mesh_cd_calc_used_gpu_layers(me, gpumat_array, gpumat_array_len);

  BLI_assert(gpumat_array_len == cache->mat_len);

  mesh_cd_layers_type_merge(&cache->cd_needed, cd_needed);
  mesh_batch_cache_request_surface_batches(cache);
  return cache->surface_per_mat;
}

GPUBatch **DRW_mesh_batch_cache_get_surface_texpaint(Mesh *me)
{
  MeshBatchCache *cache = mesh_batch_cache_get(me);
  texpaint_request_active_uv(cache, me);
  mesh_batch_cache_request_surface_batches(cache);
  return cache->surface_per_mat;
}

GPUBatch *DRW_mesh_batch_cache_get_surface_texpaint_single(Mesh *me)
{
  MeshBatchCache *cache = mesh_batch_cache_get(me);
  texpaint_request_active_uv(cache, me);
  mesh_batch_cache_request_surface_batches(cache);
  return cache->batch.surface;
}

GPUBatch *DRW_mesh_batch_cache_get_surface_vertpaint(Mesh *me)
{
  MeshBatchCache *cache = mesh_batch_cache_get(me);
  texpaint_request_active_vcol(cache, me);
  mesh_batch_cache_request_surface_batches(cache);
  return cache->batch.surface;
}

GPUBatch *DRW_mesh_batch_cache_get_surface_sculpt(Mesh *me)
{
  MeshBatchCache *cache = mesh_batch_cache_get(me);
  sculpt_request_active_vcol(cache, me);
  mesh_batch_cache_request_surface_batches(cache);
  return cache->batch.surface;
}

int DRW_mesh_material_count_get(const Mesh *me)
{
  return mesh_render_mat_len_get(me);
}

GPUBatch *DRW_mesh_batch_cache_get_sculpt_overlays(Mesh *me)
{
  MeshBatchCache *cache = mesh_batch_cache_get(me);

  cache->cd_needed.sculpt_overlays = 1;
  mesh_batch_cache_add_request(cache, MBC_SCULPT_OVERLAYS);
  DRW_batch_request(&cache->batch.sculpt_overlays);

  return cache->batch.sculpt_overlays;
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
 * NOTE: For now this only free the shading batches / vbo if any cd layers is
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

#ifdef DEBUG
/* Sanity check function to test if all requested batches are available. */
static void drw_mesh_batch_cache_check_available(struct TaskGraph *task_graph, Mesh *me)
{
  MeshBatchCache *cache = mesh_batch_cache_get(me);
  /* Make sure all requested batches have been setup. */
  /* NOTE: The next line creates a different scheduling than during release builds what can lead to
   * some issues (See T77867 where we needed to disable this function in order to debug what was
   * happening in release builds). */
  BLI_task_graph_work_and_wait(task_graph);
  for (int i = 0; i < MBC_BATCH_LEN; i++) {
    BLI_assert(!DRW_batch_requested(((GPUBatch **)&cache->batch)[i], 0));
  }
  for (int i = 0; i < MBC_VBO_LEN; i++) {
    BLI_assert(!DRW_vbo_requested(((GPUVertBuf **)&cache->final.vbo)[i]));
  }
  for (int i = 0; i < MBC_IBO_LEN; i++) {
    BLI_assert(!DRW_ibo_requested(((GPUIndexBuf **)&cache->final.ibo)[i]));
  }
  for (int i = 0; i < MBC_VBO_LEN; i++) {
    BLI_assert(!DRW_vbo_requested(((GPUVertBuf **)&cache->cage.vbo)[i]));
  }
  for (int i = 0; i < MBC_IBO_LEN; i++) {
    BLI_assert(!DRW_ibo_requested(((GPUIndexBuf **)&cache->cage.ibo)[i]));
  }
  for (int i = 0; i < MBC_VBO_LEN; i++) {
    BLI_assert(!DRW_vbo_requested(((GPUVertBuf **)&cache->uv_cage.vbo)[i]));
  }
  for (int i = 0; i < MBC_IBO_LEN; i++) {
    BLI_assert(!DRW_ibo_requested(((GPUIndexBuf **)&cache->uv_cage.ibo)[i]));
  }
}
#endif

/* Can be called for any surface type. Mesh *me is the final mesh. */
void DRW_mesh_batch_cache_create_requested(struct TaskGraph *task_graph,
                                           Object *ob,
                                           Mesh *me,
                                           const Scene *scene,
                                           const bool is_paint_mode,
                                           const bool use_hide)
{
  BLI_assert(task_graph);
  const ToolSettings *ts = NULL;
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

  /* Sanity check. */
  if ((me->edit_mesh != NULL) && (ob->mode & OB_MODE_EDIT)) {
    BLI_assert(me->edit_mesh->mesh_eval_final != NULL);
  }

  /* Don't check `DRW_object_is_in_edit_mode(ob)` here because it means the same mesh
   * may draw with edit-mesh data and regular mesh data.
   * In this case the custom-data layers used won't always match in `me->runtime.batch_cache`.
   * If we want to display regular mesh data, we should have a separate cache for the edit-mesh.
   * See T77359. */
  const bool is_editmode = (me->edit_mesh != NULL) &&
                           /* In rare cases we have the edit-mode data but not the generated cache.
                            * This can happen when switching an objects data to a mesh which
                            * happens to be in edit-mode in another scene, see: T82952. */
                           (me->edit_mesh->mesh_eval_final !=
                            NULL) /* && DRW_object_is_in_edit_mode(ob) */;

  /* This could be set for paint mode too, currently it's only used for edit-mode. */
  const bool is_mode_active = is_editmode && DRW_object_is_in_edit_mode(ob);

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

  if (batch_requested &
      (MBC_SURFACE | MBC_WIRE_LOOPS_UVS | MBC_EDITUV_FACES_STRETCH_AREA |
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
        if (cache->cd_used.sculpt_overlays != cache->cd_needed.sculpt_overlays) {
          GPU_VERTBUF_DISCARD_SAFE(mbuffercache->vbo.sculpt_data);
        }
        if (((cache->cd_used.vcol & cache->cd_needed.vcol) != cache->cd_needed.vcol) ||
            ((cache->cd_used.sculpt_vcol & cache->cd_needed.sculpt_vcol) !=
             cache->cd_needed.sculpt_vcol)) {
          GPU_VERTBUF_DISCARD_SAFE(mbuffercache->vbo.vcol);
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
        GPU_VERTBUF_DISCARD_SAFE(mbuffercache->vbo.fdots_edituv_data);
        GPU_INDEXBUF_DISCARD_SAFE(mbuffercache->ibo.edituv_tris);
        GPU_INDEXBUF_DISCARD_SAFE(mbuffercache->ibo.edituv_lines);
        GPU_INDEXBUF_DISCARD_SAFE(mbuffercache->ibo.edituv_points);
        GPU_INDEXBUF_DISCARD_SAFE(mbuffercache->ibo.edituv_fdots);
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
    Mesh *mesh = ob->data;
    BKE_pbvh_update_normals(ob->sculpt->pbvh, mesh->runtime.subdiv_ccg);
  }

  cache->batch_ready |= batch_requested;

  const bool do_cage = (is_editmode &&
                        (me->edit_mesh->mesh_eval_final != me->edit_mesh->mesh_eval_cage));

  const bool do_uvcage = is_editmode && !me->edit_mesh->mesh_eval_final->runtime.is_original;

  MeshBufferCache *mbufcache = &cache->final;

  /* Initialize batches and request VBO's & IBO's. */
  MDEPS_ASSERT(batch.surface, ibo.tris, vbo.lnor, vbo.pos_nor, vbo.uv, vbo.vcol);
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
  MDEPS_ASSERT(batch.all_verts, vbo.pos_nor);
  if (DRW_batch_requested(cache->batch.all_verts, GPU_PRIM_POINTS)) {
    DRW_vbo_request(cache->batch.all_verts, &mbufcache->vbo.pos_nor);
  }
  MDEPS_ASSERT(batch.sculpt_overlays, ibo.tris, vbo.pos_nor, vbo.sculpt_data);
  if (DRW_batch_requested(cache->batch.sculpt_overlays, GPU_PRIM_TRIS)) {
    DRW_ibo_request(cache->batch.sculpt_overlays, &mbufcache->ibo.tris);
    DRW_vbo_request(cache->batch.sculpt_overlays, &mbufcache->vbo.pos_nor);
    DRW_vbo_request(cache->batch.sculpt_overlays, &mbufcache->vbo.sculpt_data);
  }
  MDEPS_ASSERT(batch.all_edges, ibo.lines, vbo.pos_nor);
  if (DRW_batch_requested(cache->batch.all_edges, GPU_PRIM_LINES)) {
    DRW_ibo_request(cache->batch.all_edges, &mbufcache->ibo.lines);
    DRW_vbo_request(cache->batch.all_edges, &mbufcache->vbo.pos_nor);
  }
  MDEPS_ASSERT(batch.loose_edges, ibo.lines_loose, vbo.pos_nor);
  if (DRW_batch_requested(cache->batch.loose_edges, GPU_PRIM_LINES)) {
    DRW_ibo_request(NULL, &mbufcache->ibo.lines);
    DRW_ibo_request(cache->batch.loose_edges, &mbufcache->ibo.lines_loose);
    DRW_vbo_request(cache->batch.loose_edges, &mbufcache->vbo.pos_nor);
  }
  MDEPS_ASSERT(batch.edge_detection, ibo.lines_adjacency, vbo.pos_nor);
  if (DRW_batch_requested(cache->batch.edge_detection, GPU_PRIM_LINES_ADJ)) {
    DRW_ibo_request(cache->batch.edge_detection, &mbufcache->ibo.lines_adjacency);
    DRW_vbo_request(cache->batch.edge_detection, &mbufcache->vbo.pos_nor);
  }
  MDEPS_ASSERT(batch.surface_weights, ibo.tris, vbo.pos_nor, vbo.weights);
  if (DRW_batch_requested(cache->batch.surface_weights, GPU_PRIM_TRIS)) {
    DRW_ibo_request(cache->batch.surface_weights, &mbufcache->ibo.tris);
    DRW_vbo_request(cache->batch.surface_weights, &mbufcache->vbo.pos_nor);
    DRW_vbo_request(cache->batch.surface_weights, &mbufcache->vbo.weights);
  }
  MDEPS_ASSERT(batch.wire_loops, ibo.lines_paint_mask, vbo.lnor, vbo.pos_nor);
  if (DRW_batch_requested(cache->batch.wire_loops, GPU_PRIM_LINES)) {
    DRW_ibo_request(cache->batch.wire_loops, &mbufcache->ibo.lines_paint_mask);
    /* Order matters. First ones override latest VBO's attributes. */
    DRW_vbo_request(cache->batch.wire_loops, &mbufcache->vbo.lnor);
    DRW_vbo_request(cache->batch.wire_loops, &mbufcache->vbo.pos_nor);
  }
  MDEPS_ASSERT(batch.wire_edges, ibo.lines, vbo.pos_nor, vbo.edge_fac);
  if (DRW_batch_requested(cache->batch.wire_edges, GPU_PRIM_LINES)) {
    DRW_ibo_request(cache->batch.wire_edges, &mbufcache->ibo.lines);
    DRW_vbo_request(cache->batch.wire_edges, &mbufcache->vbo.pos_nor);
    DRW_vbo_request(cache->batch.wire_edges, &mbufcache->vbo.edge_fac);
  }
  MDEPS_ASSERT(batch.wire_loops_uvs, ibo.edituv_lines, vbo.uv);
  if (DRW_batch_requested(cache->batch.wire_loops_uvs, GPU_PRIM_LINES)) {
    DRW_ibo_request(cache->batch.wire_loops_uvs, &mbufcache->ibo.edituv_lines);
    /* For paint overlay. Active layer should have been queried. */
    if (cache->cd_used.uv != 0) {
      DRW_vbo_request(cache->batch.wire_loops_uvs, &mbufcache->vbo.uv);
    }
  }
  MDEPS_ASSERT(batch.edit_mesh_analysis, ibo.tris, vbo.pos_nor, vbo.mesh_analysis);
  if (DRW_batch_requested(cache->batch.edit_mesh_analysis, GPU_PRIM_TRIS)) {
    DRW_ibo_request(cache->batch.edit_mesh_analysis, &mbufcache->ibo.tris);
    DRW_vbo_request(cache->batch.edit_mesh_analysis, &mbufcache->vbo.pos_nor);
    DRW_vbo_request(cache->batch.edit_mesh_analysis, &mbufcache->vbo.mesh_analysis);
  }

  /* Per Material */
  MDEPS_ASSERT(
      surface_per_mat, tris_per_mat, vbo.lnor, vbo.pos_nor, vbo.uv, vbo.tan, vbo.vcol, vbo.orco);
  for (int i = 0; i < cache->mat_len; i++) {
    if (DRW_batch_requested(cache->surface_per_mat[i], GPU_PRIM_TRIS)) {
      DRW_ibo_request(cache->surface_per_mat[i], &mbufcache->tris_per_mat[i]);
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

  mbufcache = (do_cage) ? &cache->cage : &cache->final;

  /* Edit Mesh */
  MDEPS_ASSERT(batch.edit_triangles, ibo.tris, vbo.pos_nor, vbo.edit_data);
  if (DRW_batch_requested(cache->batch.edit_triangles, GPU_PRIM_TRIS)) {
    DRW_ibo_request(cache->batch.edit_triangles, &mbufcache->ibo.tris);
    DRW_vbo_request(cache->batch.edit_triangles, &mbufcache->vbo.pos_nor);
    DRW_vbo_request(cache->batch.edit_triangles, &mbufcache->vbo.edit_data);
  }
  MDEPS_ASSERT(batch.edit_vertices, ibo.points, vbo.pos_nor, vbo.edit_data);
  if (DRW_batch_requested(cache->batch.edit_vertices, GPU_PRIM_POINTS)) {
    DRW_ibo_request(cache->batch.edit_vertices, &mbufcache->ibo.points);
    DRW_vbo_request(cache->batch.edit_vertices, &mbufcache->vbo.pos_nor);
    DRW_vbo_request(cache->batch.edit_vertices, &mbufcache->vbo.edit_data);
  }
  MDEPS_ASSERT(batch.edit_edges, ibo.lines, vbo.pos_nor, vbo.edit_data);
  if (DRW_batch_requested(cache->batch.edit_edges, GPU_PRIM_LINES)) {
    DRW_ibo_request(cache->batch.edit_edges, &mbufcache->ibo.lines);
    DRW_vbo_request(cache->batch.edit_edges, &mbufcache->vbo.pos_nor);
    DRW_vbo_request(cache->batch.edit_edges, &mbufcache->vbo.edit_data);
  }
  MDEPS_ASSERT(batch.edit_vnor, ibo.points, vbo.pos_nor);
  if (DRW_batch_requested(cache->batch.edit_vnor, GPU_PRIM_POINTS)) {
    DRW_ibo_request(cache->batch.edit_vnor, &mbufcache->ibo.points);
    DRW_vbo_request(cache->batch.edit_vnor, &mbufcache->vbo.pos_nor);
  }
  MDEPS_ASSERT(batch.edit_lnor, ibo.tris, vbo.pos_nor, vbo.lnor);
  if (DRW_batch_requested(cache->batch.edit_lnor, GPU_PRIM_POINTS)) {
    DRW_ibo_request(cache->batch.edit_lnor, &mbufcache->ibo.tris);
    DRW_vbo_request(cache->batch.edit_lnor, &mbufcache->vbo.pos_nor);
    DRW_vbo_request(cache->batch.edit_lnor, &mbufcache->vbo.lnor);
  }
  MDEPS_ASSERT(batch.edit_fdots, ibo.fdots, vbo.fdots_pos, vbo.fdots_nor);
  if (DRW_batch_requested(cache->batch.edit_fdots, GPU_PRIM_POINTS)) {
    DRW_ibo_request(cache->batch.edit_fdots, &mbufcache->ibo.fdots);
    DRW_vbo_request(cache->batch.edit_fdots, &mbufcache->vbo.fdots_pos);
    DRW_vbo_request(cache->batch.edit_fdots, &mbufcache->vbo.fdots_nor);
  }
  MDEPS_ASSERT(batch.edit_skin_roots, vbo.skin_roots);
  if (DRW_batch_requested(cache->batch.edit_skin_roots, GPU_PRIM_POINTS)) {
    DRW_vbo_request(cache->batch.edit_skin_roots, &mbufcache->vbo.skin_roots);
  }

  /* Selection */
  MDEPS_ASSERT(batch.edit_selection_verts, ibo.points, vbo.pos_nor, vbo.vert_idx);
  if (DRW_batch_requested(cache->batch.edit_selection_verts, GPU_PRIM_POINTS)) {
    DRW_ibo_request(cache->batch.edit_selection_verts, &mbufcache->ibo.points);
    DRW_vbo_request(cache->batch.edit_selection_verts, &mbufcache->vbo.pos_nor);
    DRW_vbo_request(cache->batch.edit_selection_verts, &mbufcache->vbo.vert_idx);
  }
  MDEPS_ASSERT(batch.edit_selection_edges, ibo.lines, vbo.pos_nor, vbo.edge_idx);
  if (DRW_batch_requested(cache->batch.edit_selection_edges, GPU_PRIM_LINES)) {
    DRW_ibo_request(cache->batch.edit_selection_edges, &mbufcache->ibo.lines);
    DRW_vbo_request(cache->batch.edit_selection_edges, &mbufcache->vbo.pos_nor);
    DRW_vbo_request(cache->batch.edit_selection_edges, &mbufcache->vbo.edge_idx);
  }
  MDEPS_ASSERT(batch.edit_selection_faces, ibo.tris, vbo.pos_nor, vbo.poly_idx);
  if (DRW_batch_requested(cache->batch.edit_selection_faces, GPU_PRIM_TRIS)) {
    DRW_ibo_request(cache->batch.edit_selection_faces, &mbufcache->ibo.tris);
    DRW_vbo_request(cache->batch.edit_selection_faces, &mbufcache->vbo.pos_nor);
    DRW_vbo_request(cache->batch.edit_selection_faces, &mbufcache->vbo.poly_idx);
  }
  MDEPS_ASSERT(batch.edit_selection_fdots, ibo.fdots, vbo.fdots_pos, vbo.fdot_idx);
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
  MDEPS_ASSERT(batch.edituv_faces, ibo.edituv_tris, vbo.uv, vbo.edituv_data);
  if (DRW_batch_requested(cache->batch.edituv_faces, GPU_PRIM_TRIS)) {
    DRW_ibo_request(cache->batch.edituv_faces, &mbufcache->ibo.edituv_tris);
    DRW_vbo_request(cache->batch.edituv_faces, &mbufcache->vbo.uv);
    DRW_vbo_request(cache->batch.edituv_faces, &mbufcache->vbo.edituv_data);
  }
  MDEPS_ASSERT(batch.edituv_faces_stretch_area,
               ibo.edituv_tris,
               vbo.uv,
               vbo.edituv_data,
               vbo.edituv_stretch_area);
  if (DRW_batch_requested(cache->batch.edituv_faces_stretch_area, GPU_PRIM_TRIS)) {
    DRW_ibo_request(cache->batch.edituv_faces_stretch_area, &mbufcache->ibo.edituv_tris);
    DRW_vbo_request(cache->batch.edituv_faces_stretch_area, &mbufcache->vbo.uv);
    DRW_vbo_request(cache->batch.edituv_faces_stretch_area, &mbufcache->vbo.edituv_data);
    DRW_vbo_request(cache->batch.edituv_faces_stretch_area, &mbufcache->vbo.edituv_stretch_area);
  }
  MDEPS_ASSERT(batch.edituv_faces_stretch_angle,
               ibo.edituv_tris,
               vbo.uv,
               vbo.edituv_data,
               vbo.edituv_stretch_angle);
  if (DRW_batch_requested(cache->batch.edituv_faces_stretch_angle, GPU_PRIM_TRIS)) {
    DRW_ibo_request(cache->batch.edituv_faces_stretch_angle, &mbufcache->ibo.edituv_tris);
    DRW_vbo_request(cache->batch.edituv_faces_stretch_angle, &mbufcache->vbo.uv);
    DRW_vbo_request(cache->batch.edituv_faces_stretch_angle, &mbufcache->vbo.edituv_data);
    DRW_vbo_request(cache->batch.edituv_faces_stretch_angle, &mbufcache->vbo.edituv_stretch_angle);
  }
  MDEPS_ASSERT(batch.edituv_edges, ibo.edituv_lines, vbo.uv, vbo.edituv_data);
  if (DRW_batch_requested(cache->batch.edituv_edges, GPU_PRIM_LINES)) {
    DRW_ibo_request(cache->batch.edituv_edges, &mbufcache->ibo.edituv_lines);
    DRW_vbo_request(cache->batch.edituv_edges, &mbufcache->vbo.uv);
    DRW_vbo_request(cache->batch.edituv_edges, &mbufcache->vbo.edituv_data);
  }
  MDEPS_ASSERT(batch.edituv_verts, ibo.edituv_points, vbo.uv, vbo.edituv_data);
  if (DRW_batch_requested(cache->batch.edituv_verts, GPU_PRIM_POINTS)) {
    DRW_ibo_request(cache->batch.edituv_verts, &mbufcache->ibo.edituv_points);
    DRW_vbo_request(cache->batch.edituv_verts, &mbufcache->vbo.uv);
    DRW_vbo_request(cache->batch.edituv_verts, &mbufcache->vbo.edituv_data);
  }
  MDEPS_ASSERT(batch.edituv_fdots, ibo.edituv_fdots, vbo.fdots_uv, vbo.fdots_edituv_data);
  if (DRW_batch_requested(cache->batch.edituv_fdots, GPU_PRIM_POINTS)) {
    DRW_ibo_request(cache->batch.edituv_fdots, &mbufcache->ibo.edituv_fdots);
    DRW_vbo_request(cache->batch.edituv_fdots, &mbufcache->vbo.fdots_uv);
    DRW_vbo_request(cache->batch.edituv_fdots, &mbufcache->vbo.fdots_edituv_data);
  }

  MDEPS_ASSERT_MAP(vbo.lnor);
  MDEPS_ASSERT_MAP(vbo.pos_nor);
  MDEPS_ASSERT_MAP(vbo.uv);
  MDEPS_ASSERT_MAP(vbo.vcol);
  MDEPS_ASSERT_MAP(vbo.sculpt_data);
  MDEPS_ASSERT_MAP(vbo.weights);
  MDEPS_ASSERT_MAP(vbo.edge_fac);
  MDEPS_ASSERT_MAP(vbo.mesh_analysis);
  MDEPS_ASSERT_MAP(vbo.tan);
  MDEPS_ASSERT_MAP(vbo.orco);
  MDEPS_ASSERT_MAP(vbo.edit_data);
  MDEPS_ASSERT_MAP(vbo.fdots_pos);
  MDEPS_ASSERT_MAP(vbo.fdots_nor);
  MDEPS_ASSERT_MAP(vbo.skin_roots);
  MDEPS_ASSERT_MAP(vbo.vert_idx);
  MDEPS_ASSERT_MAP(vbo.edge_idx);
  MDEPS_ASSERT_MAP(vbo.poly_idx);
  MDEPS_ASSERT_MAP(vbo.fdot_idx);
  MDEPS_ASSERT_MAP(vbo.edituv_data);
  MDEPS_ASSERT_MAP(vbo.edituv_stretch_area);
  MDEPS_ASSERT_MAP(vbo.edituv_stretch_angle);
  MDEPS_ASSERT_MAP(vbo.fdots_uv);
  MDEPS_ASSERT_MAP(vbo.fdots_edituv_data);

  MDEPS_ASSERT_MAP(ibo.tris);
  MDEPS_ASSERT_MAP(ibo.lines);
  MDEPS_ASSERT_MAP(ibo.lines_loose);
  MDEPS_ASSERT_MAP(ibo.lines_adjacency);
  MDEPS_ASSERT_MAP(ibo.lines_paint_mask);
  MDEPS_ASSERT_MAP(ibo.points);
  MDEPS_ASSERT_MAP(ibo.fdots);
  MDEPS_ASSERT_MAP(ibo.edituv_tris);
  MDEPS_ASSERT_MAP(ibo.edituv_lines);
  MDEPS_ASSERT_MAP(ibo.edituv_points);
  MDEPS_ASSERT_MAP(ibo.edituv_fdots);

  MDEPS_ASSERT_MAP(tris_per_mat);

  /* Meh loose Scene const correctness here. */
  const bool use_subsurf_fdots = scene ? BKE_modifiers_uses_subsurf_facedots(scene, ob) : false;

  if (do_uvcage) {
    mesh_buffer_cache_create_requested(task_graph,
                                       cache,
                                       &cache->uv_cage,
                                       &cache->uv_cage_extraction_cache,
                                       me,
                                       is_editmode,
                                       is_paint_mode,
                                       is_mode_active,
                                       ob->obmat,
                                       false,
                                       true,
                                       false,
                                       scene,
                                       ts,
                                       true);
  }

  if (do_cage) {
    mesh_buffer_cache_create_requested(task_graph,
                                       cache,
                                       &cache->cage,
                                       &cache->cage_extraction_cache,
                                       me,
                                       is_editmode,
                                       is_paint_mode,
                                       is_mode_active,
                                       ob->obmat,
                                       false,
                                       false,
                                       use_subsurf_fdots,
                                       scene,
                                       ts,
                                       true);
  }

  mesh_buffer_cache_create_requested(task_graph,
                                     cache,
                                     &cache->final,
                                     &cache->final_extraction_cache,
                                     me,
                                     is_editmode,
                                     is_paint_mode,
                                     is_mode_active,
                                     ob->obmat,
                                     true,
                                     false,
                                     use_subsurf_fdots,
                                     scene,
                                     ts,
                                     use_hide);

  /* Ensure that all requested batches have finished.
   * Ideally we want to remove this sync, but there are cases where this doesn't work.
   * See T79038 for example.
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
