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

#include "BKE_attribute.h"
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
#include "BKE_subdiv_modifier.h"

#include "atomic_ops.h"

#include "bmesh.h"

#include "GPU_batch.h"
#include "GPU_material.h"

#include "DRW_render.h"

#include "ED_mesh.h"
#include "ED_uvedit.h"

#include "draw_cache_extract.h"
#include "draw_cache_inline.h"
#include "draw_subdivision.h"

#include "draw_cache_impl.h" /* own include */

#include "mesh_extractors/extract_mesh.h"

/* ---------------------------------------------------------------------- */
/** \name Dependencies between buffer and batch
 * \{ */

/* clang-format off */

#define BUFFER_INDEX(buff_name) ((offsetof(MeshBufferList, buff_name) - offsetof(MeshBufferList, vbo)) / sizeof(void *))
#define BUFFER_LEN (sizeof(MeshBufferList) / sizeof(void *))

#define _BATCH_FLAG1(b) (1u << MBC_BATCH_INDEX(b))
#define _BATCH_FLAG2(b1, b2) _BATCH_FLAG1(b1) | _BATCH_FLAG1(b2)
#define _BATCH_FLAG3(b1, b2, b3) _BATCH_FLAG2(b1, b2) | _BATCH_FLAG1(b3)
#define _BATCH_FLAG4(b1, b2, b3, b4) _BATCH_FLAG3(b1, b2, b3) | _BATCH_FLAG1(b4)
#define _BATCH_FLAG5(b1, b2, b3, b4, b5) _BATCH_FLAG4(b1, b2, b3, b4) | _BATCH_FLAG1(b5)
#define _BATCH_FLAG6(b1, b2, b3, b4, b5, b6) _BATCH_FLAG5(b1, b2, b3, b4, b5) | _BATCH_FLAG1(b6)
#define _BATCH_FLAG7(b1, b2, b3, b4, b5, b6, b7) _BATCH_FLAG6(b1, b2, b3, b4, b5, b6) | _BATCH_FLAG1(b7)
#define _BATCH_FLAG8(b1, b2, b3, b4, b5, b6, b7, b8) _BATCH_FLAG7(b1, b2, b3, b4, b5, b6, b7) | _BATCH_FLAG1(b8)
#define _BATCH_FLAG9(b1, b2, b3, b4, b5, b6, b7, b8, b9) _BATCH_FLAG8(b1, b2, b3, b4, b5, b6, b7, b8) | _BATCH_FLAG1(b9)
#define _BATCH_FLAG10(b1, b2, b3, b4, b5, b6, b7, b8, b9, b10) _BATCH_FLAG9(b1, b2, b3, b4, b5, b6, b7, b8, b9) | _BATCH_FLAG1(b10)
#define _BATCH_FLAG18(b1, b2, b3, b4, b5, b6, b7, b8, b9, b10, b11, b12, b13, b14, b15, b16, b17, b18) _BATCH_FLAG10(b1, b2, b3, b4, b5, b6, b7, b8, b9, b10) | _BATCH_FLAG8(b11, b12, b13, b14, b15, b16, b17, b18)

#define BATCH_FLAG(...) VA_NARGS_CALL_OVERLOAD(_BATCH_FLAG, __VA_ARGS__)

#define _BATCH_MAP1(a) g_buffer_deps[BUFFER_INDEX(a)]
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

#ifndef NDEBUG
#  define MDEPS_ASSERT_INDEX(buffer_index, batch_flag) \
    g_buffer_deps_d[buffer_index] |= batch_flag; \
    BLI_assert(g_buffer_deps[buffer_index] & batch_flag)

#  define _MDEPS_ASSERT2(b, n1) MDEPS_ASSERT_INDEX(BUFFER_INDEX(n1), b)
#  define _MDEPS_ASSERT3(b, n1, n2) _MDEPS_ASSERT2(b, n1); _MDEPS_ASSERT2(b, n2)
#  define _MDEPS_ASSERT4(b, n1, n2, n3) _MDEPS_ASSERT3(b, n1, n2); _MDEPS_ASSERT2(b, n3)
#  define _MDEPS_ASSERT5(b, n1, n2, n3, n4) _MDEPS_ASSERT4(b, n1, n2, n3); _MDEPS_ASSERT2(b, n4)
#  define _MDEPS_ASSERT6(b, n1, n2, n3, n4, n5) _MDEPS_ASSERT5(b, n1, n2, n3, n4); _MDEPS_ASSERT2(b, n5)
#  define _MDEPS_ASSERT7(b, n1, n2, n3, n4, n5, n6) _MDEPS_ASSERT6(b, n1, n2, n3, n4, n5); _MDEPS_ASSERT2(b, n6)
#  define _MDEPS_ASSERT8(b, n1, n2, n3, n4, n5, n6, n7) _MDEPS_ASSERT7(b, n1, n2, n3, n4, n5, n6); _MDEPS_ASSERT2(b, n7)
#  define _MDEPS_ASSERT21(b, n1, n2, n3, n4, n5, n6, n7, n8, n9, n10, n11, n12, n13, n14, n15, n16, n17, n18, n19, n20) _MDEPS_ASSERT8(b, n1, n2, n3, n4, n5, n6, n7); _MDEPS_ASSERT8(b, n8, n9, n10, n11, n12, n13, n14); _MDEPS_ASSERT7(b, n15, n16, n17, n18, n19, n20)
#  define _MDEPS_ASSERT22(b, n1, n2, n3, n4, n5, n6, n7, n8, n9, n10, n11, n12, n13, n14, n15, n16, n17, n18, n19, n20, n21) _MDEPS_ASSERT21(b, n1, n2, n3, n4, n5, n6, n7, n8, n9, n10, n11, n12, n13, n14, n15, n16, n17, n18, n19, n20); _MDEPS_ASSERT2(b, n21);

#  define MDEPS_ASSERT_FLAG(...) VA_NARGS_CALL_OVERLOAD(_MDEPS_ASSERT, __VA_ARGS__)
#  define MDEPS_ASSERT(batch_name, ...) MDEPS_ASSERT_FLAG(BATCH_FLAG(batch_name), __VA_ARGS__)
#  define MDEPS_ASSERT_MAP_INDEX(buff_index) BLI_assert(g_buffer_deps_d[buff_index] == g_buffer_deps[buff_index])
#  define MDEPS_ASSERT_MAP(buff_name) MDEPS_ASSERT_MAP_INDEX(BUFFER_INDEX(buff_name))
#else
#  define MDEPS_ASSERT_INDEX(buffer_index, batch_flag)
#  define MDEPS_ASSERT_FLAG(...)
#  define MDEPS_ASSERT(batch_name, ...)
#  define MDEPS_ASSERT_MAP_INDEX(buff_index)
#  define MDEPS_ASSERT_MAP(buff_name)
#endif

/* clang-format on */

#define TRIS_PER_MAT_INDEX BUFFER_LEN
#define SURFACE_PER_MAT_FLAG (1u << MBC_BATCH_LEN)

static const DRWBatchFlag g_buffer_deps[] = {
    [BUFFER_INDEX(vbo.pos_nor)] = BATCH_FLAG(surface,
                                             surface_weights,
                                             edit_triangles,
                                             edit_vertices,
                                             edit_edges,
                                             edit_vnor,
                                             edit_lnor,
                                             edit_mesh_analysis,
                                             edit_selection_verts,
                                             edit_selection_edges,
                                             edit_selection_faces,
                                             all_verts,
                                             all_edges,
                                             loose_edges,
                                             edge_detection,
                                             wire_edges,
                                             wire_loops,
                                             sculpt_overlays) |
                                  SURFACE_PER_MAT_FLAG,
    [BUFFER_INDEX(vbo.lnor)] = BATCH_FLAG(surface, edit_lnor, wire_loops) | SURFACE_PER_MAT_FLAG,
    [BUFFER_INDEX(vbo.edge_fac)] = BATCH_FLAG(wire_edges),
    [BUFFER_INDEX(vbo.weights)] = BATCH_FLAG(surface_weights),
    [BUFFER_INDEX(vbo.uv)] = BATCH_FLAG(surface,
                                        edituv_faces_stretch_area,
                                        edituv_faces_stretch_angle,
                                        edituv_faces,
                                        edituv_edges,
                                        edituv_verts,
                                        wire_loops_uvs) |
                             SURFACE_PER_MAT_FLAG,
    [BUFFER_INDEX(vbo.tan)] = SURFACE_PER_MAT_FLAG,
    [BUFFER_INDEX(vbo.vcol)] = BATCH_FLAG(surface) | SURFACE_PER_MAT_FLAG,
    [BUFFER_INDEX(vbo.sculpt_data)] = BATCH_FLAG(sculpt_overlays),
    [BUFFER_INDEX(vbo.orco)] = SURFACE_PER_MAT_FLAG,
    [BUFFER_INDEX(vbo.edit_data)] = BATCH_FLAG(edit_triangles, edit_edges, edit_vertices),
    [BUFFER_INDEX(vbo.edituv_data)] = BATCH_FLAG(edituv_faces,
                                                 edituv_faces_stretch_area,
                                                 edituv_faces_stretch_angle,
                                                 edituv_edges,
                                                 edituv_verts),
    [BUFFER_INDEX(vbo.edituv_stretch_area)] = BATCH_FLAG(edituv_faces_stretch_area),
    [BUFFER_INDEX(vbo.edituv_stretch_angle)] = BATCH_FLAG(edituv_faces_stretch_angle),
    [BUFFER_INDEX(vbo.mesh_analysis)] = BATCH_FLAG(edit_mesh_analysis),
    [BUFFER_INDEX(vbo.fdots_pos)] = BATCH_FLAG(edit_fdots, edit_selection_fdots),
    [BUFFER_INDEX(vbo.fdots_nor)] = BATCH_FLAG(edit_fdots),
    [BUFFER_INDEX(vbo.fdots_uv)] = BATCH_FLAG(edituv_fdots),
    [BUFFER_INDEX(vbo.fdots_edituv_data)] = BATCH_FLAG(edituv_fdots),
    [BUFFER_INDEX(vbo.skin_roots)] = BATCH_FLAG(edit_skin_roots),
    [BUFFER_INDEX(vbo.vert_idx)] = BATCH_FLAG(edit_selection_verts),
    [BUFFER_INDEX(vbo.edge_idx)] = BATCH_FLAG(edit_selection_edges),
    [BUFFER_INDEX(vbo.poly_idx)] = BATCH_FLAG(edit_selection_faces),
    [BUFFER_INDEX(vbo.fdot_idx)] = BATCH_FLAG(edit_selection_fdots),
    [BUFFER_INDEX(vbo.attr) + 0] = BATCH_FLAG(surface) | SURFACE_PER_MAT_FLAG,
    [BUFFER_INDEX(vbo.attr) + 1] = BATCH_FLAG(surface) | SURFACE_PER_MAT_FLAG,
    [BUFFER_INDEX(vbo.attr) + 2] = BATCH_FLAG(surface) | SURFACE_PER_MAT_FLAG,
    [BUFFER_INDEX(vbo.attr) + 3] = BATCH_FLAG(surface) | SURFACE_PER_MAT_FLAG,
    [BUFFER_INDEX(vbo.attr) + 4] = BATCH_FLAG(surface) | SURFACE_PER_MAT_FLAG,
    [BUFFER_INDEX(vbo.attr) + 5] = BATCH_FLAG(surface) | SURFACE_PER_MAT_FLAG,
    [BUFFER_INDEX(vbo.attr) + 6] = BATCH_FLAG(surface) | SURFACE_PER_MAT_FLAG,
    [BUFFER_INDEX(vbo.attr) + 7] = BATCH_FLAG(surface) | SURFACE_PER_MAT_FLAG,
    [BUFFER_INDEX(vbo.attr) + 8] = BATCH_FLAG(surface) | SURFACE_PER_MAT_FLAG,
    [BUFFER_INDEX(vbo.attr) + 9] = BATCH_FLAG(surface) | SURFACE_PER_MAT_FLAG,
    [BUFFER_INDEX(vbo.attr) + 10] = BATCH_FLAG(surface) | SURFACE_PER_MAT_FLAG,
    [BUFFER_INDEX(vbo.attr) + 11] = BATCH_FLAG(surface) | SURFACE_PER_MAT_FLAG,
    [BUFFER_INDEX(vbo.attr) + 12] = BATCH_FLAG(surface) | SURFACE_PER_MAT_FLAG,
    [BUFFER_INDEX(vbo.attr) + 13] = BATCH_FLAG(surface) | SURFACE_PER_MAT_FLAG,
    [BUFFER_INDEX(vbo.attr) + 14] = BATCH_FLAG(surface) | SURFACE_PER_MAT_FLAG,

    [BUFFER_INDEX(ibo.tris)] = BATCH_FLAG(surface,
                                          surface_weights,
                                          edit_triangles,
                                          edit_lnor,
                                          edit_mesh_analysis,
                                          edit_selection_faces,
                                          sculpt_overlays),
    [BUFFER_INDEX(ibo.lines)] = BATCH_FLAG(
        edit_edges, edit_selection_edges, all_edges, wire_edges),
    [BUFFER_INDEX(ibo.lines_loose)] = BATCH_FLAG(loose_edges),
    [BUFFER_INDEX(ibo.points)] = BATCH_FLAG(edit_vnor, edit_vertices, edit_selection_verts),
    [BUFFER_INDEX(ibo.fdots)] = BATCH_FLAG(edit_fdots, edit_selection_fdots),
    [BUFFER_INDEX(ibo.lines_paint_mask)] = BATCH_FLAG(wire_loops),
    [BUFFER_INDEX(ibo.lines_adjacency)] = BATCH_FLAG(edge_detection),
    [BUFFER_INDEX(ibo.edituv_tris)] = BATCH_FLAG(
        edituv_faces, edituv_faces_stretch_area, edituv_faces_stretch_angle),
    [BUFFER_INDEX(ibo.edituv_lines)] = BATCH_FLAG(edituv_edges, wire_loops_uvs),
    [BUFFER_INDEX(ibo.edituv_points)] = BATCH_FLAG(edituv_verts),
    [BUFFER_INDEX(ibo.edituv_fdots)] = BATCH_FLAG(edituv_fdots),
    [TRIS_PER_MAT_INDEX] = SURFACE_PER_MAT_FLAG,
};

#ifndef NDEBUG
static DRWBatchFlag g_buffer_deps_d[ARRAY_SIZE(g_buffer_deps)] = {0};
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

  if (batch_map & SURFACE_PER_MAT_FLAG) {
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

BLI_INLINE const Mesh *editmesh_final_or_this(const Object *object, const Mesh *me)
{
  if (me->edit_mesh != NULL) {
    Mesh *editmesh_eval_final = BKE_object_get_editmesh_eval_final(object);
    if (editmesh_eval_final != NULL) {
      return editmesh_eval_final;
    }
  }

  return me;
}

static void mesh_cd_calc_edit_uv_layer(const Mesh *UNUSED(me), DRW_MeshCDMask *cd_used)
{
  cd_used->edit_uv = 1;
}

/** \name DRW_MeshAttributes
 *
 * Utilities for handling requested attributes.
 * \{ */

/* Return true if the given DRW_AttributeRequest is already in the requests. */
static bool has_request(const DRW_MeshAttributes *requests, DRW_AttributeRequest req)
{
  for (int i = 0; i < requests->num_requests; i++) {
    const DRW_AttributeRequest src_req = requests->requests[i];
    if (src_req.domain != req.domain) {
      continue;
    }
    if (src_req.layer_index != req.layer_index) {
      continue;
    }
    if (src_req.cd_type != req.cd_type) {
      continue;
    }
    return true;
  }
  return false;
}

static void mesh_attrs_merge_requests(const DRW_MeshAttributes *src_requests,
                                      DRW_MeshAttributes *dst_requests)
{
  for (int i = 0; i < src_requests->num_requests; i++) {
    if (dst_requests->num_requests == GPU_MAX_ATTR) {
      return;
    }

    if (has_request(dst_requests, src_requests->requests[i])) {
      continue;
    }

    dst_requests->requests[dst_requests->num_requests] = src_requests->requests[i];
    dst_requests->num_requests += 1;
  }
}

static void drw_mesh_attributes_clear(DRW_MeshAttributes *attributes)
{
  memset(attributes, 0, sizeof(DRW_MeshAttributes));
}

static void drw_mesh_attributes_merge(DRW_MeshAttributes *dst,
                                      const DRW_MeshAttributes *src,
                                      ThreadMutex *mesh_render_mutex)
{
  BLI_mutex_lock(mesh_render_mutex);
  mesh_attrs_merge_requests(src, dst);
  BLI_mutex_unlock(mesh_render_mutex);
}

/* Return true if all requests in b are in a. */
static bool drw_mesh_attributes_overlap(DRW_MeshAttributes *a, DRW_MeshAttributes *b)
{
  for (int i = 0; i < b->num_requests; i++) {
    if (!has_request(a, b->requests[i])) {
      return false;
    }
  }

  return true;
}

static void drw_mesh_attributes_add_request(DRW_MeshAttributes *attrs,
                                            CustomDataType type,
                                            int layer,
                                            AttributeDomain domain)
{
  if (attrs->num_requests >= GPU_MAX_ATTR) {
    return;
  }

  DRW_AttributeRequest *req = &attrs->requests[attrs->num_requests];
  req->cd_type = type;
  req->layer_index = layer;
  req->domain = domain;
  attrs->num_requests += 1;
}

/** \} */

BLI_INLINE const CustomData *mesh_cd_ldata_get_from_mesh(const Mesh *me)
{
  switch ((eMeshWrapperType)me->runtime.wrapper_type) {
    case ME_WRAPPER_TYPE_SUBD:
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

BLI_INLINE const CustomData *mesh_cd_pdata_get_from_mesh(const Mesh *me)
{
  switch ((eMeshWrapperType)me->runtime.wrapper_type) {
    case ME_WRAPPER_TYPE_SUBD:
    case ME_WRAPPER_TYPE_MDATA:
      return &me->pdata;
      break;
    case ME_WRAPPER_TYPE_BMESH:
      return &me->edit_mesh->bm->pdata;
      break;
  }

  BLI_assert(0);
  return &me->pdata;
}

BLI_INLINE const CustomData *mesh_cd_edata_get_from_mesh(const Mesh *me)
{
  switch ((eMeshWrapperType)me->runtime.wrapper_type) {
    case ME_WRAPPER_TYPE_SUBD:
    case ME_WRAPPER_TYPE_MDATA:
      return &me->edata;
      break;
    case ME_WRAPPER_TYPE_BMESH:
      return &me->edit_mesh->bm->edata;
      break;
  }

  BLI_assert(0);
  return &me->edata;
}

BLI_INLINE const CustomData *mesh_cd_vdata_get_from_mesh(const Mesh *me)
{
  switch ((eMeshWrapperType)me->runtime.wrapper_type) {
    case ME_WRAPPER_TYPE_SUBD:
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

static void mesh_cd_calc_active_uv_layer(const Object *object,
                                         const Mesh *me,
                                         DRW_MeshCDMask *cd_used)
{
  const Mesh *me_final = editmesh_final_or_this(object, me);
  const CustomData *cd_ldata = mesh_cd_ldata_get_from_mesh(me_final);
  int layer = CustomData_get_active_layer(cd_ldata, CD_MLOOPUV);
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
  int layer = CustomData_get_stencil_layer(cd_ldata, CD_MLOOPUV);
  if (layer != -1) {
    cd_used->uv |= (1 << layer);
  }
}

static void mesh_cd_calc_active_vcol_layer(const Object *object,
                                           const Mesh *me,
                                           DRW_MeshAttributes *attrs_used)
{
  const Mesh *me_final = editmesh_final_or_this(object, me);
  const CustomData *cd_vdata = mesh_cd_vdata_get_from_mesh(me_final);

  int layer = CustomData_get_active_layer(cd_vdata, CD_PROP_COLOR);
  if (layer != -1) {
    drw_mesh_attributes_add_request(attrs_used, CD_PROP_COLOR, layer, ATTR_DOMAIN_POINT);
  }
}

static void mesh_cd_calc_active_mloopcol_layer(const Object *object,
                                               const Mesh *me,
                                               DRW_MeshCDMask *cd_used)
{
  const Mesh *me_final = editmesh_final_or_this(object, me);
  const CustomData *cd_ldata = mesh_cd_ldata_get_from_mesh(me_final);

  int layer = CustomData_get_active_layer(cd_ldata, CD_MLOOPCOL);
  if (layer != -1) {
    cd_used->vcol |= (1 << layer);
  }
}

static bool custom_data_match_attribute(const CustomData *custom_data,
                                        const char *name,
                                        int *r_layer_index,
                                        int *r_type)
{
  const int possible_attribute_types[6] = {
      CD_PROP_BOOL,
      CD_PROP_INT32,
      CD_PROP_FLOAT,
      CD_PROP_FLOAT2,
      CD_PROP_FLOAT3,
      CD_PROP_COLOR,
  };

  for (int i = 0; i < ARRAY_SIZE(possible_attribute_types); i++) {
    const int attr_type = possible_attribute_types[i];
    int layer_index = CustomData_get_named_layer(custom_data, attr_type, name);
    if (layer_index == -1) {
      continue;
    }

    *r_layer_index = layer_index;
    *r_type = attr_type;
    return true;
  }

  return false;
}

static DRW_MeshCDMask mesh_cd_calc_used_gpu_layers(const Object *object,
                                                   const Mesh *me,
                                                   struct GPUMaterial **gpumat_array,
                                                   int gpumat_array_len,
                                                   DRW_MeshAttributes *attributes)
{
  const Mesh *me_final = editmesh_final_or_this(object, me);
  const CustomData *cd_ldata = mesh_cd_ldata_get_from_mesh(me_final);
  const CustomData *cd_pdata = mesh_cd_pdata_get_from_mesh(me_final);
  const CustomData *cd_vdata = mesh_cd_vdata_get_from_mesh(me_final);
  const CustomData *cd_edata = mesh_cd_edata_get_from_mesh(me_final);

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
        /* ATTR_DOMAIN_NUM is standard for "invalid value". */
        AttributeDomain domain = ATTR_DOMAIN_NUM;

        if (type == CD_AUTO_FROM_NAME) {
          /* We need to deduce what exact layer is used.
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

#if 0 /* Tangents are always from UV's - this will never happen. */
            if (layer == -1) {
              layer = CustomData_get_named_layer(cd_ldata, CD_TANGENT, name);
              type = CD_TANGENT;
            }
#endif
            if (layer == -1) {
              /* Try to match a generic attribute, we use the first attribute domain with a
               * matching name. */
              if (custom_data_match_attribute(cd_vdata, name, &layer, &type)) {
                domain = ATTR_DOMAIN_POINT;
              }
              else if (custom_data_match_attribute(cd_ldata, name, &layer, &type)) {
                domain = ATTR_DOMAIN_CORNER;
              }
              else if (custom_data_match_attribute(cd_pdata, name, &layer, &type)) {
                domain = ATTR_DOMAIN_FACE;
              }
              else if (custom_data_match_attribute(cd_edata, name, &layer, &type)) {
                domain = ATTR_DOMAIN_EDGE;
              }
              else {
                layer = -1;
                domain = ATTR_DOMAIN_NUM;
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
          case CD_PROP_BOOL:
          case CD_PROP_INT32:
          case CD_PROP_FLOAT:
          case CD_PROP_FLOAT2:
          case CD_PROP_FLOAT3:
          case CD_PROP_COLOR: {
            if (layer != -1 && domain != ATTR_DOMAIN_NUM) {
              drw_mesh_attributes_add_request(attributes, type, layer, domain);
            }
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

static bool mesh_batch_cache_valid(Object *object, Mesh *me)
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

  if (cache->mat_len != mesh_render_mat_len_get(object, me)) {
    return false;
  }

  return true;
}

static void mesh_batch_cache_init(Object *object, Mesh *me)
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

  cache->mat_len = mesh_render_mat_len_get(object, me);
  cache->surface_per_mat = MEM_callocN(sizeof(*cache->surface_per_mat) * cache->mat_len, __func__);
  cache->tris_per_mat = MEM_callocN(sizeof(*cache->tris_per_mat) * cache->mat_len, __func__);

  cache->is_dirty = false;
  cache->batch_ready = 0;
  cache->batch_requested = 0;

  drw_mesh_weight_state_clear(&cache->weight_state);
}

void DRW_mesh_batch_cache_validate(Object *object, Mesh *me)
{
  if (!mesh_batch_cache_valid(object, me)) {
    mesh_batch_cache_clear(me);
    mesh_batch_cache_init(object, me);
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
    GPU_VERTBUF_DISCARD_SAFE(mbc->buff.vbo.vcol);
    GPU_VERTBUF_DISCARD_SAFE(mbc->buff.vbo.orco);
  }
  DRWBatchFlag batch_map = BATCH_MAP(vbo.uv, vbo.tan, vbo.vcol, vbo.orco);
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
  MeshBatchCache *cache = me->runtime.batch_cache;
  if (cache == NULL) {
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
       * Note that it can be slow if auto smooth is enabled. (see T63946) */
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

  MEM_SAFE_FREE(mbc->loose_geom.verts);
  MEM_SAFE_FREE(mbc->loose_geom.edges);
  mbc->loose_geom.edge_len = 0;
  mbc->loose_geom.vert_len = 0;

  MEM_SAFE_FREE(mbc->poly_sorted.tri_first_index);
  MEM_SAFE_FREE(mbc->poly_sorted.mat_tri_len);
  mbc->poly_sorted.visible_tri_len = 0;
}

static void mesh_batch_cache_free_subdiv_cache(MeshBatchCache *cache)
{
  if (cache->subdiv_cache) {
    draw_subdiv_cache_free(cache->subdiv_cache);
    MEM_freeN(cache->subdiv_cache);
    cache->subdiv_cache = NULL;
  }
}

static void mesh_batch_cache_clear(Mesh *me)
{
  MeshBatchCache *cache = me->runtime.batch_cache;
  if (!cache) {
    return;
  }
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

  cache->batch_ready = 0;
  drw_mesh_weight_state_clear(&cache->weight_state);

  mesh_batch_cache_free_subdiv_cache(cache);
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

static void texpaint_request_active_vcol(MeshBatchCache *cache, Object *object, Mesh *me)
{
  DRW_MeshCDMask cd_needed;
  mesh_cd_layers_type_clear(&cd_needed);
  mesh_cd_calc_active_mloopcol_layer(object, me, &cd_needed);

  BLI_assert(cd_needed.vcol != 0 &&
             "No MLOOPCOL layer available in vertpaint, but batches requested anyway!");

  mesh_cd_layers_type_merge(&cache->cd_needed, cd_needed);
}

static void sculpt_request_active_vcol(MeshBatchCache *cache, Object *object, Mesh *me)
{
  DRW_MeshAttributes attrs_needed;
  drw_mesh_attributes_clear(&attrs_needed);
  mesh_cd_calc_active_vcol_layer(object, me, &attrs_needed);

  BLI_assert(attrs_needed.num_requests != 0 &&
             "No MPropCol layer available in Sculpt, but batches requested anyway!");

  drw_mesh_attributes_merge(&cache->attr_needed, &attrs_needed, me->runtime.render_mutex);
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

GPUBatch **DRW_mesh_batch_cache_get_surface_shaded(Object *object,
                                                   Mesh *me,
                                                   struct GPUMaterial **gpumat_array,
                                                   uint gpumat_array_len)
{
  MeshBatchCache *cache = mesh_batch_cache_get(me);
  DRW_MeshAttributes attrs_needed;
  drw_mesh_attributes_clear(&attrs_needed);
  DRW_MeshCDMask cd_needed = mesh_cd_calc_used_gpu_layers(
      object, me, gpumat_array, gpumat_array_len, &attrs_needed);

  BLI_assert(gpumat_array_len == cache->mat_len);

  mesh_cd_layers_type_merge(&cache->cd_needed, cd_needed);
  ThreadMutex *mesh_render_mutex = (ThreadMutex *)me->runtime.render_mutex;
  drw_mesh_attributes_merge(&cache->attr_needed, &attrs_needed, mesh_render_mutex);
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
  texpaint_request_active_vcol(cache, object, me);
  mesh_batch_cache_request_surface_batches(cache);
  return cache->batch.surface;
}

GPUBatch *DRW_mesh_batch_cache_get_surface_sculpt(Object *object, Mesh *me)
{
  MeshBatchCache *cache = mesh_batch_cache_get(me);
  sculpt_request_active_vcol(cache, object, me);
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

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Edit Mode API
 * \{ */

GPUVertBuf *DRW_mesh_batch_cache_pos_vertbuf_get(Mesh *me)
{
  MeshBatchCache *cache = mesh_batch_cache_get(me);
  /* Request surface to trigger the vbo filling. Otherwise it may do nothing. */
  mesh_batch_cache_request_surface_batches(cache);

  DRW_vbo_request(NULL, &cache->final.buff.vbo.pos_nor);
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

  if (tot_area != NULL) {
    *tot_area = &cache->tot_area;
  }
  if (tot_uv_area != NULL) {
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
  MeshBatchCache *cache = me->runtime.batch_cache;

  if (cache == NULL) {
    return;
  }

  if (mesh_cd_layers_type_equal(cache->cd_used_over_time, cache->cd_used)) {
    cache->lastmatch = ctime;
  }

  if (drw_mesh_attributes_overlap(&cache->attr_used_over_time, &cache->attr_used)) {
    cache->lastmatch = ctime;
  }

  if (ctime - cache->lastmatch > U.vbotimeout) {
    mesh_batch_cache_discard_shaded_tri(cache);
  }

  mesh_cd_layers_type_clear(&cache->cd_used_over_time);
  drw_mesh_attributes_clear(&cache->attr_used_over_time);
}

static void drw_add_attributes_vbo(GPUBatch *batch,
                                   MeshBufferList *mbuflist,
                                   DRW_MeshAttributes *attr_used)
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
   * some issues (See T77867 where we needed to disable this function in order to debug what was
   * happening in release builds). */
  BLI_task_graph_work_and_wait(task_graph);
  for (int i = 0; i < MBC_BATCH_LEN; i++) {
    BLI_assert(!DRW_batch_requested(((GPUBatch **)&cache->batch)[i], 0));
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
    BLI_assert(BKE_object_get_editmesh_eval_final(ob) != NULL);
  }

  const bool is_editmode = (me->edit_mesh != NULL) &&
                           (BKE_object_get_editmesh_eval_final(ob) != NULL) &&
                           DRW_object_is_in_edit_mode(ob);

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
      Mesh *me_final = (me->edit_mesh) ? BKE_object_get_editmesh_eval_final(ob) : me;
      if (CustomData_get_layer(&me_final->vdata, CD_ORCO) == NULL) {
        /* Skip orco calculation */
        cache->cd_needed.orco = 0;
      }
    }

    ThreadMutex *mesh_render_mutex = (ThreadMutex *)me->runtime.render_mutex;

    /* Verify that all surface batches have needed attribute layers.
     */
    /* TODO(fclem): We could be a bit smarter here and only do it per
     * material. */
    bool cd_overlap = mesh_cd_layers_type_overlap(cache->cd_used, cache->cd_needed);
    bool attr_overlap = drw_mesh_attributes_overlap(&cache->attr_used, &cache->attr_needed);
    if (cd_overlap == false || attr_overlap == false) {
      FOREACH_MESH_BUFFER_CACHE (cache, mbc) {
        if ((cache->cd_used.uv & cache->cd_needed.uv) != cache->cd_needed.uv) {
          GPU_VERTBUF_DISCARD_SAFE(mbc->buff.vbo.uv);
          cd_uv_update = true;
        }
        if ((cache->cd_used.tan & cache->cd_needed.tan) != cache->cd_needed.tan ||
            cache->cd_used.tan_orco != cache->cd_needed.tan_orco) {
          GPU_VERTBUF_DISCARD_SAFE(mbc->buff.vbo.tan);
        }
        if (cache->cd_used.orco != cache->cd_needed.orco) {
          GPU_VERTBUF_DISCARD_SAFE(mbc->buff.vbo.orco);
        }
        if (cache->cd_used.sculpt_overlays != cache->cd_needed.sculpt_overlays) {
          GPU_VERTBUF_DISCARD_SAFE(mbc->buff.vbo.sculpt_data);
        }
        if ((cache->cd_used.vcol & cache->cd_needed.vcol) != cache->cd_needed.vcol) {
          GPU_VERTBUF_DISCARD_SAFE(mbc->buff.vbo.vcol);
        }
        if (!drw_mesh_attributes_overlap(&cache->attr_used, &cache->attr_needed)) {
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
      drw_mesh_attributes_merge(&cache->attr_used, &cache->attr_needed, mesh_render_mutex);
    }
    mesh_cd_layers_type_merge(&cache->cd_used_over_time, cache->cd_needed);
    mesh_cd_layers_type_clear(&cache->cd_needed);

    drw_mesh_attributes_merge(&cache->attr_used_over_time, &cache->attr_needed, mesh_render_mutex);
    drw_mesh_attributes_clear(&cache->attr_needed);
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
    Mesh *mesh = ob->data;
    BKE_pbvh_update_normals(ob->sculpt->pbvh, mesh->runtime.subdiv_ccg);
  }

  cache->batch_ready |= batch_requested;

  bool do_cage = false, do_uvcage = false;
  if (is_editmode) {
    Mesh *editmesh_eval_final = BKE_object_get_editmesh_eval_final(ob);
    Mesh *editmesh_eval_cage = BKE_object_get_editmesh_eval_cage(ob);

    do_cage = editmesh_eval_final != editmesh_eval_cage;
    do_uvcage = !editmesh_eval_final->runtime.is_original;
  }

  const int required_mode = BKE_subsurf_modifier_eval_required_mode(DRW_state_is_scene_render(),
                                                                    is_editmode);
  const bool do_subdivision = BKE_subsurf_modifier_can_do_gpu_subdiv(scene, ob, me, required_mode);

  MeshBufferList *mbuflist = &cache->final.buff;

  /* Initialize batches and request VBO's & IBO's. */
  MDEPS_ASSERT(surface,
               ibo.tris,
               vbo.lnor,
               vbo.pos_nor,
               vbo.uv,
               vbo.vcol,
               vbo.attr[0],
               vbo.attr[1],
               vbo.attr[2],
               vbo.attr[3],
               vbo.attr[4],
               vbo.attr[5],
               vbo.attr[6],
               vbo.attr[7],
               vbo.attr[8],
               vbo.attr[9],
               vbo.attr[10],
               vbo.attr[11],
               vbo.attr[12],
               vbo.attr[13],
               vbo.attr[14]);
  if (DRW_batch_requested(cache->batch.surface, GPU_PRIM_TRIS)) {
    DRW_ibo_request(cache->batch.surface, &mbuflist->ibo.tris);
    /* Order matters. First ones override latest VBO's attributes. */
    DRW_vbo_request(cache->batch.surface, &mbuflist->vbo.lnor);
    DRW_vbo_request(cache->batch.surface, &mbuflist->vbo.pos_nor);
    if (cache->cd_used.uv != 0) {
      DRW_vbo_request(cache->batch.surface, &mbuflist->vbo.uv);
    }
    if (cache->cd_used.vcol != 0) {
      DRW_vbo_request(cache->batch.surface, &mbuflist->vbo.vcol);
    }
    drw_add_attributes_vbo(cache->batch.surface, mbuflist, &cache->attr_used);
  }
  MDEPS_ASSERT(all_verts, vbo.pos_nor);
  if (DRW_batch_requested(cache->batch.all_verts, GPU_PRIM_POINTS)) {
    DRW_vbo_request(cache->batch.all_verts, &mbuflist->vbo.pos_nor);
  }
  MDEPS_ASSERT(sculpt_overlays, ibo.tris, vbo.pos_nor, vbo.sculpt_data);
  if (DRW_batch_requested(cache->batch.sculpt_overlays, GPU_PRIM_TRIS)) {
    DRW_ibo_request(cache->batch.sculpt_overlays, &mbuflist->ibo.tris);
    DRW_vbo_request(cache->batch.sculpt_overlays, &mbuflist->vbo.pos_nor);
    DRW_vbo_request(cache->batch.sculpt_overlays, &mbuflist->vbo.sculpt_data);
  }
  MDEPS_ASSERT(all_edges, ibo.lines, vbo.pos_nor);
  if (DRW_batch_requested(cache->batch.all_edges, GPU_PRIM_LINES)) {
    DRW_ibo_request(cache->batch.all_edges, &mbuflist->ibo.lines);
    DRW_vbo_request(cache->batch.all_edges, &mbuflist->vbo.pos_nor);
  }
  MDEPS_ASSERT(loose_edges, ibo.lines_loose, vbo.pos_nor);
  if (DRW_batch_requested(cache->batch.loose_edges, GPU_PRIM_LINES)) {
    DRW_ibo_request(NULL, &mbuflist->ibo.lines);
    DRW_ibo_request(cache->batch.loose_edges, &mbuflist->ibo.lines_loose);
    DRW_vbo_request(cache->batch.loose_edges, &mbuflist->vbo.pos_nor);
  }
  MDEPS_ASSERT(edge_detection, ibo.lines_adjacency, vbo.pos_nor);
  if (DRW_batch_requested(cache->batch.edge_detection, GPU_PRIM_LINES_ADJ)) {
    DRW_ibo_request(cache->batch.edge_detection, &mbuflist->ibo.lines_adjacency);
    DRW_vbo_request(cache->batch.edge_detection, &mbuflist->vbo.pos_nor);
  }
  MDEPS_ASSERT(surface_weights, ibo.tris, vbo.pos_nor, vbo.weights);
  if (DRW_batch_requested(cache->batch.surface_weights, GPU_PRIM_TRIS)) {
    DRW_ibo_request(cache->batch.surface_weights, &mbuflist->ibo.tris);
    DRW_vbo_request(cache->batch.surface_weights, &mbuflist->vbo.pos_nor);
    DRW_vbo_request(cache->batch.surface_weights, &mbuflist->vbo.weights);
  }
  MDEPS_ASSERT(wire_loops, ibo.lines_paint_mask, vbo.lnor, vbo.pos_nor);
  if (DRW_batch_requested(cache->batch.wire_loops, GPU_PRIM_LINES)) {
    DRW_ibo_request(cache->batch.wire_loops, &mbuflist->ibo.lines_paint_mask);
    /* Order matters. First ones override latest VBO's attributes. */
    DRW_vbo_request(cache->batch.wire_loops, &mbuflist->vbo.lnor);
    DRW_vbo_request(cache->batch.wire_loops, &mbuflist->vbo.pos_nor);
  }
  MDEPS_ASSERT(wire_edges, ibo.lines, vbo.pos_nor, vbo.edge_fac);
  if (DRW_batch_requested(cache->batch.wire_edges, GPU_PRIM_LINES)) {
    DRW_ibo_request(cache->batch.wire_edges, &mbuflist->ibo.lines);
    DRW_vbo_request(cache->batch.wire_edges, &mbuflist->vbo.pos_nor);
    DRW_vbo_request(cache->batch.wire_edges, &mbuflist->vbo.edge_fac);
  }
  MDEPS_ASSERT(wire_loops_uvs, ibo.edituv_lines, vbo.uv);
  if (DRW_batch_requested(cache->batch.wire_loops_uvs, GPU_PRIM_LINES)) {
    DRW_ibo_request(cache->batch.wire_loops_uvs, &mbuflist->ibo.edituv_lines);
    /* For paint overlay. Active layer should have been queried. */
    if (cache->cd_used.uv != 0) {
      DRW_vbo_request(cache->batch.wire_loops_uvs, &mbuflist->vbo.uv);
    }
  }
  MDEPS_ASSERT(edit_mesh_analysis, ibo.tris, vbo.pos_nor, vbo.mesh_analysis);
  if (DRW_batch_requested(cache->batch.edit_mesh_analysis, GPU_PRIM_TRIS)) {
    DRW_ibo_request(cache->batch.edit_mesh_analysis, &mbuflist->ibo.tris);
    DRW_vbo_request(cache->batch.edit_mesh_analysis, &mbuflist->vbo.pos_nor);
    DRW_vbo_request(cache->batch.edit_mesh_analysis, &mbuflist->vbo.mesh_analysis);
  }

  /* Per Material */
  MDEPS_ASSERT_FLAG(SURFACE_PER_MAT_FLAG,
                    vbo.lnor,
                    vbo.pos_nor,
                    vbo.uv,
                    vbo.tan,
                    vbo.vcol,
                    vbo.orco,
                    vbo.attr[0],
                    vbo.attr[1],
                    vbo.attr[2],
                    vbo.attr[3],
                    vbo.attr[4],
                    vbo.attr[5],
                    vbo.attr[6],
                    vbo.attr[7],
                    vbo.attr[8],
                    vbo.attr[9],
                    vbo.attr[10],
                    vbo.attr[11],
                    vbo.attr[12],
                    vbo.attr[13],
                    vbo.attr[14]);
  MDEPS_ASSERT_INDEX(TRIS_PER_MAT_INDEX, SURFACE_PER_MAT_FLAG);
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
      if (cache->cd_used.vcol != 0) {
        DRW_vbo_request(cache->surface_per_mat[i], &mbuflist->vbo.vcol);
      }
      if (cache->cd_used.orco != 0) {
        DRW_vbo_request(cache->surface_per_mat[i], &mbuflist->vbo.orco);
      }
      drw_add_attributes_vbo(cache->surface_per_mat[i], mbuflist, &cache->attr_used);
    }
  }

  mbuflist = (do_cage) ? &cache->cage.buff : &cache->final.buff;

  /* Edit Mesh */
  MDEPS_ASSERT(edit_triangles, ibo.tris, vbo.pos_nor, vbo.edit_data);
  if (DRW_batch_requested(cache->batch.edit_triangles, GPU_PRIM_TRIS)) {
    DRW_ibo_request(cache->batch.edit_triangles, &mbuflist->ibo.tris);
    DRW_vbo_request(cache->batch.edit_triangles, &mbuflist->vbo.pos_nor);
    DRW_vbo_request(cache->batch.edit_triangles, &mbuflist->vbo.edit_data);
  }
  MDEPS_ASSERT(edit_vertices, ibo.points, vbo.pos_nor, vbo.edit_data);
  if (DRW_batch_requested(cache->batch.edit_vertices, GPU_PRIM_POINTS)) {
    DRW_ibo_request(cache->batch.edit_vertices, &mbuflist->ibo.points);
    DRW_vbo_request(cache->batch.edit_vertices, &mbuflist->vbo.pos_nor);
    DRW_vbo_request(cache->batch.edit_vertices, &mbuflist->vbo.edit_data);
  }
  MDEPS_ASSERT(edit_edges, ibo.lines, vbo.pos_nor, vbo.edit_data);
  if (DRW_batch_requested(cache->batch.edit_edges, GPU_PRIM_LINES)) {
    DRW_ibo_request(cache->batch.edit_edges, &mbuflist->ibo.lines);
    DRW_vbo_request(cache->batch.edit_edges, &mbuflist->vbo.pos_nor);
    DRW_vbo_request(cache->batch.edit_edges, &mbuflist->vbo.edit_data);
  }
  MDEPS_ASSERT(edit_vnor, ibo.points, vbo.pos_nor);
  if (DRW_batch_requested(cache->batch.edit_vnor, GPU_PRIM_POINTS)) {
    DRW_ibo_request(cache->batch.edit_vnor, &mbuflist->ibo.points);
    DRW_vbo_request(cache->batch.edit_vnor, &mbuflist->vbo.pos_nor);
  }
  MDEPS_ASSERT(edit_lnor, ibo.tris, vbo.pos_nor, vbo.lnor);
  if (DRW_batch_requested(cache->batch.edit_lnor, GPU_PRIM_POINTS)) {
    DRW_ibo_request(cache->batch.edit_lnor, &mbuflist->ibo.tris);
    DRW_vbo_request(cache->batch.edit_lnor, &mbuflist->vbo.pos_nor);
    DRW_vbo_request(cache->batch.edit_lnor, &mbuflist->vbo.lnor);
  }
  MDEPS_ASSERT(edit_fdots, ibo.fdots, vbo.fdots_pos, vbo.fdots_nor);
  if (DRW_batch_requested(cache->batch.edit_fdots, GPU_PRIM_POINTS)) {
    DRW_ibo_request(cache->batch.edit_fdots, &mbuflist->ibo.fdots);
    DRW_vbo_request(cache->batch.edit_fdots, &mbuflist->vbo.fdots_pos);
    DRW_vbo_request(cache->batch.edit_fdots, &mbuflist->vbo.fdots_nor);
  }
  MDEPS_ASSERT(edit_skin_roots, vbo.skin_roots);
  if (DRW_batch_requested(cache->batch.edit_skin_roots, GPU_PRIM_POINTS)) {
    DRW_vbo_request(cache->batch.edit_skin_roots, &mbuflist->vbo.skin_roots);
  }

  /* Selection */
  MDEPS_ASSERT(edit_selection_verts, ibo.points, vbo.pos_nor, vbo.vert_idx);
  if (DRW_batch_requested(cache->batch.edit_selection_verts, GPU_PRIM_POINTS)) {
    DRW_ibo_request(cache->batch.edit_selection_verts, &mbuflist->ibo.points);
    DRW_vbo_request(cache->batch.edit_selection_verts, &mbuflist->vbo.pos_nor);
    DRW_vbo_request(cache->batch.edit_selection_verts, &mbuflist->vbo.vert_idx);
  }
  MDEPS_ASSERT(edit_selection_edges, ibo.lines, vbo.pos_nor, vbo.edge_idx);
  if (DRW_batch_requested(cache->batch.edit_selection_edges, GPU_PRIM_LINES)) {
    DRW_ibo_request(cache->batch.edit_selection_edges, &mbuflist->ibo.lines);
    DRW_vbo_request(cache->batch.edit_selection_edges, &mbuflist->vbo.pos_nor);
    DRW_vbo_request(cache->batch.edit_selection_edges, &mbuflist->vbo.edge_idx);
  }
  MDEPS_ASSERT(edit_selection_faces, ibo.tris, vbo.pos_nor, vbo.poly_idx);
  if (DRW_batch_requested(cache->batch.edit_selection_faces, GPU_PRIM_TRIS)) {
    DRW_ibo_request(cache->batch.edit_selection_faces, &mbuflist->ibo.tris);
    DRW_vbo_request(cache->batch.edit_selection_faces, &mbuflist->vbo.pos_nor);
    DRW_vbo_request(cache->batch.edit_selection_faces, &mbuflist->vbo.poly_idx);
  }
  MDEPS_ASSERT(edit_selection_fdots, ibo.fdots, vbo.fdots_pos, vbo.fdot_idx);
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
  MDEPS_ASSERT(edituv_faces, ibo.edituv_tris, vbo.uv, vbo.edituv_data);
  if (DRW_batch_requested(cache->batch.edituv_faces, GPU_PRIM_TRIS)) {
    DRW_ibo_request(cache->batch.edituv_faces, &mbuflist->ibo.edituv_tris);
    DRW_vbo_request(cache->batch.edituv_faces, &mbuflist->vbo.uv);
    DRW_vbo_request(cache->batch.edituv_faces, &mbuflist->vbo.edituv_data);
  }
  MDEPS_ASSERT(edituv_faces_stretch_area,
               ibo.edituv_tris,
               vbo.uv,
               vbo.edituv_data,
               vbo.edituv_stretch_area);
  if (DRW_batch_requested(cache->batch.edituv_faces_stretch_area, GPU_PRIM_TRIS)) {
    DRW_ibo_request(cache->batch.edituv_faces_stretch_area, &mbuflist->ibo.edituv_tris);
    DRW_vbo_request(cache->batch.edituv_faces_stretch_area, &mbuflist->vbo.uv);
    DRW_vbo_request(cache->batch.edituv_faces_stretch_area, &mbuflist->vbo.edituv_data);
    DRW_vbo_request(cache->batch.edituv_faces_stretch_area, &mbuflist->vbo.edituv_stretch_area);
  }
  MDEPS_ASSERT(edituv_faces_stretch_angle,
               ibo.edituv_tris,
               vbo.uv,
               vbo.edituv_data,
               vbo.edituv_stretch_angle);
  if (DRW_batch_requested(cache->batch.edituv_faces_stretch_angle, GPU_PRIM_TRIS)) {
    DRW_ibo_request(cache->batch.edituv_faces_stretch_angle, &mbuflist->ibo.edituv_tris);
    DRW_vbo_request(cache->batch.edituv_faces_stretch_angle, &mbuflist->vbo.uv);
    DRW_vbo_request(cache->batch.edituv_faces_stretch_angle, &mbuflist->vbo.edituv_data);
    DRW_vbo_request(cache->batch.edituv_faces_stretch_angle, &mbuflist->vbo.edituv_stretch_angle);
  }
  MDEPS_ASSERT(edituv_edges, ibo.edituv_lines, vbo.uv, vbo.edituv_data);
  if (DRW_batch_requested(cache->batch.edituv_edges, GPU_PRIM_LINES)) {
    DRW_ibo_request(cache->batch.edituv_edges, &mbuflist->ibo.edituv_lines);
    DRW_vbo_request(cache->batch.edituv_edges, &mbuflist->vbo.uv);
    DRW_vbo_request(cache->batch.edituv_edges, &mbuflist->vbo.edituv_data);
  }
  MDEPS_ASSERT(edituv_verts, ibo.edituv_points, vbo.uv, vbo.edituv_data);
  if (DRW_batch_requested(cache->batch.edituv_verts, GPU_PRIM_POINTS)) {
    DRW_ibo_request(cache->batch.edituv_verts, &mbuflist->ibo.edituv_points);
    DRW_vbo_request(cache->batch.edituv_verts, &mbuflist->vbo.uv);
    DRW_vbo_request(cache->batch.edituv_verts, &mbuflist->vbo.edituv_data);
  }
  MDEPS_ASSERT(edituv_fdots, ibo.edituv_fdots, vbo.fdots_uv, vbo.fdots_edituv_data);
  if (DRW_batch_requested(cache->batch.edituv_fdots, GPU_PRIM_POINTS)) {
    DRW_ibo_request(cache->batch.edituv_fdots, &mbuflist->ibo.edituv_fdots);
    DRW_vbo_request(cache->batch.edituv_fdots, &mbuflist->vbo.fdots_uv);
    DRW_vbo_request(cache->batch.edituv_fdots, &mbuflist->vbo.fdots_edituv_data);
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
  for (int i = 0; i < GPU_MAX_ATTR; i++) {
    MDEPS_ASSERT_MAP(vbo.attr[i]);
  }

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

  MDEPS_ASSERT_MAP_INDEX(TRIS_PER_MAT_INDEX);

  /* Meh loose Scene const correctness here. */
  const bool use_subsurf_fdots = scene ? BKE_modifiers_uses_subsurf_facedots(scene, ob) : false;

  if (do_uvcage) {
    mesh_buffer_cache_create_requested(task_graph,
                                       cache,
                                       &cache->uv_cage,
                                       ob,
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
                                       ob,
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

  if (do_subdivision) {
    DRW_create_subdivision(scene,
                           ob,
                           me,
                           cache,
                           &cache->final,
                           is_editmode,
                           is_paint_mode,
                           is_mode_active,
                           ob->obmat,
                           true,
                           false,
                           use_subsurf_fdots,
                           ts,
                           use_hide);
  }
  else {
    /* The subsurf modifier may have been recently removed, or another modifier was added after it,
     * so free any potential subdivision cache as it is not needed anymore. */
    mesh_batch_cache_free_subdiv_cache(cache);
  }

  mesh_buffer_cache_create_requested(task_graph,
                                     cache,
                                     &cache->final,
                                     ob,
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
