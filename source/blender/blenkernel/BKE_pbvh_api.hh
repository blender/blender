/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bke
 * \brief A BVH for high poly meshes.
 */

#include <optional>
#include <string>

#include "BLI_bit_group_vector.hh"
#include "BLI_bounds_types.hh"
#include "BLI_compiler_compat.h"
#include "BLI_function_ref.hh"
#include "BLI_index_mask.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_offset_indices.hh"
#include "BLI_span.hh"
#include "BLI_vector.hh"

#include "DNA_customdata_types.h"

/* For embedding CCGKey in iterator. */
#include "BKE_ccg.h"
#include "BKE_pbvh.hh"

#include "bmesh.hh"

struct BMLog;
struct BMesh;
struct CCGElem;
struct CCGKey;
struct CustomData;
struct IsectRayPrecalc;
struct Mesh;
struct PBVH;
struct PBVHNode;
struct SubdivCCG;
struct Image;
struct ImageUser;
namespace blender {
namespace bke {
enum class AttrDomain : int8_t;
}
namespace draw::pbvh {
struct PBVHBatches;
struct PBVH_GPU_Args;
}  // namespace draw::pbvh
}  // namespace blender

struct PBVHProxyNode {
  blender::Vector<blender::float3> co;
};

struct PBVHColorBufferNode {
  float (*color)[4] = nullptr;
};

struct PBVHPixels {
  /**
   * Storage for texture painting on PBVH level.
   *
   * Contains #blender::bke::pbvh::pixels::PBVHData
   */
  void *data;
};

struct PBVHPixelsNode {
  /**
   * Contains triangle/pixel data used during texture painting.
   *
   * Contains #blender::bke::pbvh::pixels::NodeData.
   */
  void *node_data = nullptr;
};

struct PBVHFrustumPlanes {
  float (*planes)[4];
  int num_planes;
};

BLI_INLINE BMesh *BKE_pbvh_get_bmesh(PBVH *pbvh)
{
  return ((PBVHPublic *)pbvh)->bm;
}

Mesh *BKE_pbvh_get_mesh(PBVH *pbvh);

BLI_INLINE PBVHVertRef BKE_pbvh_make_vref(intptr_t i)
{
  PBVHVertRef ret = {i};
  return ret;
}

BLI_INLINE int BKE_pbvh_vertex_to_index(PBVH *pbvh, PBVHVertRef v)
{
  return (BKE_pbvh_type(pbvh) == PBVH_BMESH && v.i != PBVH_REF_NONE ?
              BM_elem_index_get((BMVert *)(v.i)) :
              (v.i));
}

BLI_INLINE PBVHVertRef BKE_pbvh_index_to_vertex(PBVH *pbvh, int index)
{
  switch (BKE_pbvh_type(pbvh)) {
    case PBVH_FACES:
    case PBVH_GRIDS:
      return BKE_pbvh_make_vref(index);
    case PBVH_BMESH:
      return BKE_pbvh_make_vref((intptr_t)BKE_pbvh_get_bmesh(pbvh)->vtable[index]);
  }

  return BKE_pbvh_make_vref(PBVH_REF_NONE);
}

/* Callbacks */

namespace blender::bke::pbvh {

/**
 * Do a full rebuild with on Mesh data structure.
 */
PBVH *build_mesh(Mesh *mesh);
void update_mesh_pointers(PBVH *pbvh, Mesh *mesh);
/**
 * Do a full rebuild with on Grids data structure.
 */
PBVH *build_grids(const CCGKey *key, Mesh *mesh, SubdivCCG *subdiv_ccg);
/**
 * Build a PBVH from a BMesh.
 */
PBVH *build_bmesh(BMesh *bm, BMLog *log, int cd_vert_node_offset, int cd_face_node_offset);

void update_bmesh_offsets(PBVH *pbvh, int cd_vert_node_offset, int cd_face_node_offset);

void build_pixels(PBVH *pbvh, Mesh *mesh, Image *image, ImageUser *image_user);
void free(PBVH *pbvh);

/* Hierarchical Search in the BVH, two methods:
 * - For each hit calling a callback.
 * - Gather nodes in an array (easy to multi-thread) see blender::bke::pbvh::search_gather.
 */

void search_callback(PBVH &pbvh,
                     FunctionRef<bool(PBVHNode &)> filter_fn,
                     FunctionRef<void(PBVHNode &)> hit_fn);

/* Ray-cast
 * the hit callback is called for all leaf nodes intersecting the ray;
 * it's up to the callback to find the primitive within the leaves that is
 * hit first */

void raycast(PBVH *pbvh,
             FunctionRef<void(PBVHNode &node, float *tmin)> cb,
             const float ray_start[3],
             const float ray_normal[3],
             bool original);

bool raycast_node(PBVH *pbvh,
                  PBVHNode *node,
                  float (*origco)[3],
                  bool use_origco,
                  Span<int> corner_verts,
                  Span<bool> hide_poly,
                  const float ray_start[3],
                  const float ray_normal[3],
                  IsectRayPrecalc *isect_precalc,
                  float *depth,
                  PBVHVertRef *active_vertex,
                  int *active_face_grid_index,
                  float *face_normal);

bool bmesh_node_raycast_detail(PBVHNode *node,
                               const float ray_start[3],
                               IsectRayPrecalc *isect_precalc,
                               float *depth,
                               float *r_edge_length);

/**
 * For orthographic cameras, project the far away ray segment points to the root node so
 * we can have better precision.
 *
 * Note: the interval is not guaranteed to lie between ray_start and ray_end; this is
 * not necessary for orthographic views and is impossible anyhow due to the necessity of
 * projecting the far clipping plane into the local object space.  This works out to
 * dividing view3d->clip_end by the object scale, which for small object and large
 * clip_end's can easily lead to floating-point overflows.
 */
void clip_ray_ortho(
    PBVH *pbvh, bool original, float ray_start[3], float ray_end[3], float ray_normal[3]);

void find_nearest_to_ray(PBVH *pbvh,
                         const FunctionRef<void(PBVHNode &node, float *tmin)> fn,
                         const float ray_start[3],
                         const float ray_normal[3],
                         bool original);

bool find_nearest_to_ray_node(PBVH *pbvh,
                              PBVHNode *node,
                              float (*origco)[3],
                              bool use_origco,
                              Span<int> corner_verts,
                              Span<bool> hide_poly,
                              const float ray_start[3],
                              const float ray_normal[3],
                              float *depth,
                              float *dist_sq);

/* Drawing */
void set_frustum_planes(PBVH *pbvh, PBVHFrustumPlanes *planes);
void get_frustum_planes(const PBVH *pbvh, PBVHFrustumPlanes *planes);

void draw_cb(const Mesh &mesh,
             PBVH *pbvh,
             bool update_only_visible,
             const PBVHFrustumPlanes &update_frustum,
             const PBVHFrustumPlanes &draw_frustum,
             FunctionRef<void(draw::pbvh::PBVHBatches *batches,
                              const draw::pbvh::PBVH_GPU_Args &args)> draw_fn);

}  // namespace blender::bke::pbvh

/**
 * Get the PBVH root's bounding box.
 */
blender::Bounds<blender::float3> BKE_pbvh_bounding_box(const PBVH *pbvh);

void BKE_pbvh_sync_visibility_from_verts(PBVH *pbvh, Mesh *mesh);

namespace blender::bke::pbvh {

/**
 * Returns the number of visible quads in the nodes' grids.
 */
int count_grid_quads(const BitGroupVector<> &grid_visibility,
                     Span<int> grid_indices,
                     int gridsize,
                     int display_gridsize);

}  // namespace blender::bke::pbvh

/**
 * Multi-res level, only valid for type == #PBVH_GRIDS.
 */
const CCGKey *BKE_pbvh_get_grid_key(const PBVH *pbvh);

int BKE_pbvh_get_grid_num_verts(const PBVH *pbvh);
int BKE_pbvh_get_grid_num_faces(const PBVH *pbvh);

/**
 * Only valid for type == #PBVH_BMESH.
 */
void BKE_pbvh_bmesh_detail_size_set(PBVH *pbvh, float detail_size);

enum PBVHTopologyUpdateMode {
  PBVH_Subdivide = 1,
  PBVH_Collapse = 2,
};
ENUM_OPERATORS(PBVHTopologyUpdateMode, PBVH_Collapse);

namespace blender::bke::pbvh {

/**
 * Collapse short edges, subdivide long edges.
 */
bool bmesh_update_topology(PBVH *pbvh,
                           PBVHTopologyUpdateMode mode,
                           const float center[3],
                           const float view_normal[3],
                           float radius,
                           bool use_frontface,
                           bool use_projected);

}  // namespace blender::bke::pbvh

/* Node Access */

void BKE_pbvh_node_mark_update(PBVHNode *node);
void BKE_pbvh_node_mark_update_mask(PBVHNode *node);
void BKE_pbvh_node_mark_update_color(PBVHNode *node);
void BKE_pbvh_node_mark_update_face_sets(PBVHNode *node);
void BKE_pbvh_node_mark_update_visibility(PBVHNode *node);
void BKE_pbvh_node_mark_rebuild_draw(PBVHNode *node);
void BKE_pbvh_node_mark_redraw(PBVHNode *node);
void BKE_pbvh_node_mark_positions_update(PBVHNode *node);
void BKE_pbvh_node_mark_topology_update(PBVHNode *node);
void BKE_pbvh_node_fully_hidden_set(PBVHNode *node, int fully_hidden);
bool BKE_pbvh_node_fully_hidden_get(const PBVHNode *node);
void BKE_pbvh_node_fully_masked_set(PBVHNode *node, int fully_masked);
bool BKE_pbvh_node_fully_masked_get(const PBVHNode *node);
void BKE_pbvh_node_fully_unmasked_set(PBVHNode *node, int fully_masked);
bool BKE_pbvh_node_fully_unmasked_get(const PBVHNode *node);

void BKE_pbvh_mark_rebuild_pixels(PBVH *pbvh);

blender::Span<int> BKE_pbvh_node_get_grid_indices(const PBVHNode &node);

int BKE_pbvh_node_num_unique_verts(const PBVH &pbvh, const PBVHNode &node);
blender::Span<int> BKE_pbvh_node_get_vert_indices(const PBVHNode *node);
blender::Span<int> BKE_pbvh_node_get_unique_vert_indices(const PBVHNode *node);
blender::Span<int> BKE_pbvh_node_get_loops(const PBVHNode *node);

namespace blender::bke::pbvh {

/**
 * Gather the indices of all faces (not triangles) used by the node.
 * For convenience, pass a reference to the data in the result.
 */
Span<int> node_face_indices_calc_mesh(const PBVH &pbvh, const PBVHNode &node, Vector<int> &faces);

/**
 * Gather the indices of all base mesh faces in the node.
 * For convenience, pass a reference to the data in the result.
 */
Span<int> node_face_indices_calc_grids(const PBVH &pbvh, const PBVHNode &node, Vector<int> &faces);

}  // namespace blender::bke::pbvh

blender::Bounds<blender::float3> BKE_pbvh_node_get_BB(const PBVHNode *node);
blender::Bounds<blender::float3> BKE_pbvh_node_get_original_BB(const PBVHNode *node);

float BKE_pbvh_node_get_tmin(const PBVHNode *node);

/**
 * Test if AABB is at least partially inside the #PBVHFrustumPlanes volume.
 */
bool BKE_pbvh_node_frustum_contain_AABB(const PBVHNode *node, const PBVHFrustumPlanes *frustum);
/**
 * Test if AABB is at least partially outside the #PBVHFrustumPlanes volume.
 */
bool BKE_pbvh_node_frustum_exclude_AABB(const PBVHNode *node, const PBVHFrustumPlanes *frustum);

const blender::Set<BMVert *, 0> &BKE_pbvh_bmesh_node_unique_verts(PBVHNode *node);
const blender::Set<BMVert *, 0> &BKE_pbvh_bmesh_node_other_verts(PBVHNode *node);
const blender::Set<BMFace *, 0> &BKE_pbvh_bmesh_node_faces(PBVHNode *node);

/**
 * In order to perform operations on the original node coordinates
 * (currently just ray-cast), store the node's triangles and vertices.
 *
 * Skips triangles that are hidden.
 */
void BKE_pbvh_bmesh_node_save_orig(BMesh *bm, BMLog *log, PBVHNode *node, bool use_original);
void BKE_pbvh_bmesh_after_stroke(PBVH *pbvh);

namespace blender::bke::pbvh {
void update_bounds(PBVH &pbvh, int flags);
void update_mask(PBVH &pbvh);
void update_visibility(PBVH &pbvh);
void update_normals(PBVH &pbvh, SubdivCCG *subdiv_ccg);
}  // namespace blender::bke::pbvh

blender::Bounds<blender::float3> BKE_pbvh_redraw_BB(PBVH *pbvh);
namespace blender::bke::pbvh {
IndexMask nodes_to_face_selection_grids(const SubdivCCG &subdiv_ccg,
                                        Span<const PBVHNode *> nodes,
                                        IndexMaskMemory &memory);
}
void BKE_pbvh_grids_update(PBVH *pbvh, const CCGKey *key);
void BKE_pbvh_subdiv_cgg_set(PBVH *pbvh, SubdivCCG *subdiv_ccg);

void BKE_pbvh_vert_coords_apply(PBVH *pbvh, blender::Span<blender::float3> vert_positions);
bool BKE_pbvh_is_deformed(const PBVH *pbvh);

/* Vertex Iterator. */

/* This iterator has quite a lot of code, but it's designed to:
 * - allow the compiler to eliminate dead code and variables
 * - spend most of the time in the relatively simple inner loop */

/* NOTE: PBVH_ITER_ALL does not skip hidden vertices,
 * PBVH_ITER_UNIQUE does */
#define PBVH_ITER_ALL 0
#define PBVH_ITER_UNIQUE 1

struct PBVHVertexIter {
  /* iteration */
  int g;
  int width;
  int height;
  int gx;
  int gy;
  int i;
  int index;
  PBVHVertRef vertex;

  /* grid */
  CCGKey key;
  CCGElem *const *grids;
  CCGElem *grid;
  const blender::BitGroupVector<> *grid_hidden;
  std::optional<blender::BoundedBitSpan> gh;
  const int *grid_indices;
  int totgrid;
  int gridsize;

  /* mesh */
  blender::MutableSpan<blender::float3> vert_positions;
  blender::Span<blender::float3> vert_normals;
  const bool *hide_vert;
  int totvert;
  const int *vert_indices;
  const float *vmask;
  bool is_mesh;

  /* bmesh */
  std::optional<blender::Set<BMVert *, 0>::Iterator> bm_unique_verts;
  std::optional<blender::Set<BMVert *, 0>::Iterator> bm_unique_verts_end;
  std::optional<blender::Set<BMVert *, 0>::Iterator> bm_other_verts;
  std::optional<blender::Set<BMVert *, 0>::Iterator> bm_other_verts_end;
  CustomData *bm_vdata;
  int cd_vert_mask_offset;

  /* result: these are all computed in the macro, but we assume
   * that compiler optimization's will skip the ones we don't use */
  BMVert *bm_vert;
  float *co;
  const float *no;
  const float *fno;
  float mask;
  bool visible;
};

void pbvh_vertex_iter_init(PBVH *pbvh, PBVHNode *node, PBVHVertexIter *vi, int mode);

#define BKE_pbvh_vertex_iter_begin(pbvh, node, vi, mode) \
  pbvh_vertex_iter_init(pbvh, node, &vi, mode); \
\
  for (vi.i = 0, vi.g = 0; vi.g < vi.totgrid; vi.g++) { \
    if (vi.grids) { \
      vi.width = vi.gridsize; \
      vi.height = vi.gridsize; \
      vi.index = vi.vertex.i = vi.grid_indices[vi.g] * vi.key.grid_area - 1; \
      vi.grid = CCG_elem_offset(&vi.key, vi.grids[vi.grid_indices[vi.g]], -1); \
      if (mode == PBVH_ITER_UNIQUE) { \
        if (vi.grid_hidden) { \
          vi.gh.emplace((*vi.grid_hidden)[vi.grid_indices[vi.g]]); \
        } \
        else { \
          vi.gh.reset(); \
        } \
      } \
    } \
    else { \
      vi.width = vi.totvert; \
      vi.height = 1; \
    } \
\
    for (vi.gy = 0; vi.gy < vi.height; vi.gy++) { \
      for (vi.gx = 0; vi.gx < vi.width; vi.gx++, vi.i++) { \
        if (vi.grid) { \
          vi.grid = CCG_elem_next(&vi.key, vi.grid); \
          vi.co = CCG_elem_co(&vi.key, vi.grid); \
          vi.fno = CCG_elem_no(&vi.key, vi.grid); \
          vi.mask = vi.key.has_mask ? *CCG_elem_mask(&vi.key, vi.grid) : 0.0f; \
          vi.index++; \
          vi.vertex.i++; \
          vi.visible = true; \
          if (vi.gh) { \
            if ((*vi.gh)[vi.gy * vi.gridsize + vi.gx]) { \
              continue; \
            } \
          } \
        } \
        else if (!vi.vert_positions.is_empty()) { \
          vi.visible = !(vi.hide_vert && vi.hide_vert[vi.vert_indices[vi.gx]]); \
          if (mode == PBVH_ITER_UNIQUE && !vi.visible) { \
            continue; \
          } \
          vi.co = vi.vert_positions[vi.vert_indices[vi.gx]]; \
          vi.no = vi.vert_normals[vi.vert_indices[vi.gx]]; \
          vi.index = vi.vertex.i = vi.vert_indices[vi.i]; \
          vi.mask = vi.vmask ? vi.vmask[vi.index] : 0.0f; \
        } \
        else { \
          if (*vi.bm_unique_verts != *vi.bm_unique_verts_end) { \
            vi.bm_vert = **vi.bm_unique_verts; \
            (*vi.bm_unique_verts)++; \
          } \
          else { \
            vi.bm_vert = **vi.bm_other_verts; \
            (*vi.bm_other_verts)++; \
          } \
          vi.visible = !BM_elem_flag_test_bool(vi.bm_vert, BM_ELEM_HIDDEN); \
          if (mode == PBVH_ITER_UNIQUE && !vi.visible) { \
            continue; \
          } \
          vi.co = vi.bm_vert->co; \
          vi.fno = vi.bm_vert->no; \
          vi.vertex = BKE_pbvh_make_vref((intptr_t)vi.bm_vert); \
          vi.index = BM_elem_index_get(vi.bm_vert); \
          vi.mask = BM_ELEM_CD_GET_FLOAT(vi.bm_vert, vi.cd_vert_mask_offset); \
        }

#define BKE_pbvh_vertex_iter_end \
  } \
  } \
  } \
  ((void)0)

#define PBVH_FACE_ITER_VERTS_RESERVED 8

blender::MutableSpan<PBVHProxyNode> BKE_pbvh_node_get_proxies(PBVHNode *node);
void BKE_pbvh_node_free_proxies(PBVHNode *node);
PBVHProxyNode &BKE_pbvh_node_add_proxy(PBVH &pbvh, PBVHNode &node);
void BKE_pbvh_node_get_bm_orco_data(PBVHNode *node,
                                    int (**r_orco_tris)[3],
                                    int *r_orco_tris_num,
                                    float (**r_orco_coords)[3],
                                    BMVert ***r_orco_verts);

bool pbvh_has_mask(const PBVH *pbvh);

bool pbvh_has_face_sets(PBVH *pbvh);

blender::Span<blender::float3> BKE_pbvh_get_vert_positions(const PBVH *pbvh);
blender::MutableSpan<blender::float3> BKE_pbvh_get_vert_positions(PBVH *pbvh);
blender::Span<blender::float3> BKE_pbvh_get_vert_normals(const PBVH *pbvh);

PBVHColorBufferNode *BKE_pbvh_node_color_buffer_get(PBVHNode *node);
void BKE_pbvh_node_color_buffer_free(PBVH *pbvh);
bool BKE_pbvh_get_color_layer(Mesh *mesh,
                              CustomDataLayer **r_layer,
                              blender::bke::AttrDomain *r_domain);

/* Swaps colors at each element in indices (of domain pbvh->vcol_domain)
 * with values in colors. */
void BKE_pbvh_swap_colors(PBVH *pbvh,
                          blender::Span<int> indices,
                          blender::MutableSpan<blender::float4> r_colors);

/* Stores colors from the elements in indices (of domain pbvh->vcol_domain)
 * into colors. */
void BKE_pbvh_store_colors(PBVH *pbvh,
                           blender::Span<int> indices,
                           blender::MutableSpan<blender::float4> r_colors);

/* Like BKE_pbvh_store_colors but handles loop->vert conversion */
void BKE_pbvh_store_colors_vertex(PBVH *pbvh,
                                  blender::Span<int> indices,
                                  blender::MutableSpan<blender::float4> r_colors);

bool BKE_pbvh_is_drawing(const PBVH *pbvh);

void BKE_pbvh_update_active_vcol(PBVH *pbvh, Mesh *mesh);

void BKE_pbvh_vertex_color_set(PBVH *pbvh, PBVHVertRef vertex, const float color[4]);
void BKE_pbvh_vertex_color_get(const PBVH *pbvh, PBVHVertRef vertex, float r_color[4]);

void BKE_pbvh_ensure_node_loops(PBVH *pbvh);
int BKE_pbvh_debug_draw_gen_get(PBVHNode *node);

void BKE_pbvh_pmap_set(PBVH *pbvh, blender::GroupedSpan<int> vert_to_face_map);

namespace blender::bke::pbvh {
Vector<PBVHNode *> search_gather(PBVH *pbvh,
                                 FunctionRef<bool(PBVHNode &)> scb,
                                 PBVHNodeFlags leaf_flag = PBVH_Leaf);
Vector<PBVHNode *> gather_proxies(PBVH *pbvh);

void node_update_mask_mesh(Span<float> mask, PBVHNode &node);
void node_update_mask_grids(const CCGKey &key, Span<CCGElem *> grids, PBVHNode &node);
void node_update_mask_bmesh(int mask_offset, PBVHNode &node);

void node_update_visibility_mesh(Span<bool> hide_vert, PBVHNode &node);
void node_update_visibility_grids(const BitGroupVector<> &grid_hidden, PBVHNode &node);
void node_update_visibility_bmesh(PBVHNode &node);

void update_node_bounds_mesh(Span<float3> positions, PBVHNode &node);
void update_node_bounds_grids(const CCGKey &key, Span<CCGElem *> grids, PBVHNode &node);
void update_node_bounds_bmesh(PBVHNode &node);

}  // namespace blender::bke::pbvh
