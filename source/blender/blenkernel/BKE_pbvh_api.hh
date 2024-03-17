/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bke
 * \brief A BVH for high poly meshes.
 */

#include "BKE_attribute.h"
#include "BKE_attribute.hh"
#include "BKE_dyntopo_set.hh"

#include <optional>
#include <string>

#include "BLI_bit_group_vector.hh"
#include "BLI_bit_vector.hh"
#include "BLI_bounds.hh"
#include "BLI_bounds_types.hh"
#include "BLI_compiler_compat.h"
#include "BLI_function_ref.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_offset_indices.hh"
#include "BLI_span.hh"
#include "BLI_vector.hh"

#include "DNA_brush_enums.h" /* for eAttrCorrectMode */
#include "DNA_customdata_types.h"

/* For embedding CCGKey in iterator. */
#include "BKE_attribute.h"
#include "BKE_ccg.h"
#include "BKE_pbvh.hh"

#include "bmesh.hh"
#include "bmesh_log.hh"

#include <stdint.h>

struct Object;
struct Scene;
struct CCGElem;
struct CCGKey;
struct CustomData;
struct DMFlagMat;
struct IsectRayPrecalc;
struct MLoopTri;
struct Mesh;
struct PBVH;
struct PBVHNode;
struct SculptSession;
struct SubdivCCG;
struct TaskParallelSettings;
struct Image;
struct ImageUser;

namespace blender::draw::pbvh {
struct PBVH_GPU_Args;
}

using blender::bke::AttrDomain;
using blender::draw::pbvh::PBVH_GPU_Args;

struct PBVHTri {
  int v[3];      /* References into PBVHTriBuf->verts. */
  int eflag;     /* Bitmask of which edges in the tri are real edges in the mesh. */
  intptr_t l[3]; /* Loops, currently just BMLoop pointers for now. */
  PBVHFaceRef f;
  float no[3];
};

struct PBVHTriBuf {
  blender::Vector<PBVHTri> tris;
  blender::Vector<PBVHVertRef> verts;
  blender::Vector<int> edges;
  blender::Vector<uintptr_t> loops;

  int mat_nr = 0;

  float min[3], max[3];
};

/*
 * These structs represent logical verts/edges/faces.
 * for PBVH_GRIDS and PBVH_FACES they store integer
 * offsets, PBVH_BMESH stores pointers.
 *
 * The idea is to enforce stronger type checking by encapsulating
 * intptr_t's in structs.
 */

/* A generic PBVH vertex.
 *
 * NOTE: in PBVH_GRIDS we consider the final grid points
 * to be vertices.  This is not true of edges or faces which are pulled from
 * the base mesh.
 */

struct PBVHProxyNode {
  blender::Vector<blender::float3> co;
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

class PBVHAttrReq {
 public:
  PBVHAttrReq() = default;
  PBVHAttrReq(const AttrDomain domain, const eCustomDataType type) : domain(domain), type(type) {}

  std::string name;
  AttrDomain domain;
  eCustomDataType type;
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

BLI_INLINE PBVHEdgeRef BKE_pbvh_make_eref(intptr_t i)
{
  PBVHEdgeRef ret = {i};
  return ret;
}

BLI_INLINE PBVHFaceRef BKE_pbvh_make_fref(intptr_t i)
{
  PBVHFaceRef ret = {i};
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

BLI_INLINE int BKE_pbvh_edge_to_index(PBVH *pbvh, PBVHEdgeRef e)
{
  return (BKE_pbvh_type(pbvh) == PBVH_BMESH && e.i != PBVH_REF_NONE ?
              BM_elem_index_get((BMEdge *)(e.i)) :
              (e.i));
}

BLI_INLINE PBVHEdgeRef BKE_pbvh_index_to_edge(PBVH *pbvh, int index)
{
  switch (BKE_pbvh_type(pbvh)) {
    case PBVH_FACES:
    case PBVH_GRIDS:
      return BKE_pbvh_make_eref(index);
    case PBVH_BMESH:
      return BKE_pbvh_make_eref((intptr_t)BKE_pbvh_get_bmesh(pbvh)->etable[index]);
  }

  return BKE_pbvh_make_eref(PBVH_REF_NONE);
}

BLI_INLINE int BKE_pbvh_face_to_index(PBVH *pbvh, PBVHFaceRef f)
{
  return (BKE_pbvh_type(pbvh) == PBVH_BMESH && f.i != PBVH_REF_NONE ?
              BM_elem_index_get((BMFace *)(f.i)) :
              (f.i));
}

BLI_INLINE PBVHFaceRef BKE_pbvh_index_to_face(PBVH *pbvh, int index)
{
  switch (BKE_pbvh_type(pbvh)) {
    case PBVH_FACES:
    case PBVH_GRIDS:
      return BKE_pbvh_make_fref(index);
    case PBVH_BMESH:
      return BKE_pbvh_make_fref((intptr_t)BKE_pbvh_get_bmesh(pbvh)->ftable[index]);
  }

  return BKE_pbvh_make_fref(PBVH_REF_NONE);
}

/* Callbacks */

/**
 * Returns true if the search should continue from this node, false otherwise.
 */

typedef void (*BKE_pbvh_HitCallback)(PBVHNode &node, void *data);
typedef void (*BKE_pbvh_HitOccludedCallback)(PBVHNode &node, void *data, float *tmin);

typedef void (*BKE_pbvh_SearchNearestCallback)(PBVHNode &node, void *data, float *tmin);

PBVHNode *BKE_pbvh_get_node(PBVH *pbvh, int node);

/* Building */

PBVH *BKE_pbvh_new(PBVHType type);

/**
 * Do a full rebuild with on Mesh data structure.
 */
void BKE_pbvh_build_mesh(PBVH *pbvh, Mesh *mesh);
void BKE_pbvh_update_mesh_pointers(PBVH *pbvh, Mesh *mesh);

namespace blender::bke::pbvh {
PBVH *build_grids(const CCGKey *key, Mesh *mesh, SubdivCCG *subdiv_ccg);
PBVH *build_mesh(Mesh *mesh);

/**
 * Build a PBVH from a BMesh.
 */
void build_bmesh(PBVH *pbvh,
                 Mesh *me,
                 BMesh *bm,
                 BMLog *log,
                 BMIdMap *idmap,
                 const int cd_vert_node_offset,
                 const int cd_face_node_offset,
                 const int cd_face_areas,
                 const int cd_boundary_flag,
                 const int cd_edge_boundary,
                 const int cd_flag,
                 const int cd_valence,
                 const int cd_origco,
                 const int cd_origno);

void set_idmap(PBVH *pbvh, BMIdMap *idmap);

void update_offsets(PBVH *pbvh,
                    const int cd_vert_node_offset,
                    const int cd_face_node_offset,
                    const int cd_face_areas,
                    const int cd_boudnary_flags,
                    const int cd_edge_boundary,
                    const int cd_flag,
                    const int cd_valence,
                    const int cd_origco,
                    const int cd_origno,
                    const int cd_curvature_dir);

float bmesh_detail_size_avg_get(PBVH *pbvh);

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
             const FunctionRef<void(PBVHNode &node, float *tmin)> cb,
             const float ray_start[3],
             const float ray_normal[3],
             bool original,
             int stroke_id);

bool raycast_node(SculptSession *ss,
                  PBVH *pbvh,
                  PBVHNode *node,
                  float (*origco)[3],
                  bool use_origco,
                  const Span<int> corner_verts,
                  const Span<bool> hide_poly,
                  const float ray_start[3],
                  const float ray_normal[3],
                  IsectRayPrecalc *isect_precalc,
                  int *hit_count,
                  float *depth,
                  PBVHVertRef *active_vertex_index,
                  PBVHFaceRef *active_face_grid_index,
                  float *face_normal,
                  int stroke_id);
}  // namespace blender::bke::pbvh

void BKE_pbvh_set_bm_log(PBVH *pbvh, BMLog *log);
BMLog *BKE_pbvh_get_bm_log(PBVH *pbvh);

/**
checks if original data needs to be updated for v, and if so updates it.  Stroke_id
is provided by the sculpt code and is used to detect updates.  The reason we do it
inside the verts and not in the nodes is to allow splitting of the pbvh during the stroke.
*/
bool BKE_pbvh_bmesh_check_origdata(SculptSession *ss, BMVert *v, int stroke_id);

/** used so pbvh can differentiate between different strokes,
    see BKE_pbvh_bmesh_check_origdata */
void BKE_pbvh_set_stroke_id(PBVH *pbvh, int stroke_id);

bool BKE_pbvh_bmesh_node_raycast_detail(PBVH *pbvh,
                                        PBVHNode *node,
                                        const float ray_start[3],
                                        IsectRayPrecalc *isect_precalc,
                                        float *depth,
                                        float *r_edge_length);

namespace blender::bke::pbvh {
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

bool find_nearest_to_ray_node(SculptSession *ss,
                              PBVH *pbvh,
                              PBVHNode *node,
                              float (*origco)[3],
                              bool use_origco,
                              const float ray_start[3],
                              const float ray_normal[3],
                              float *depth,
                              float *dist_sq,
                              int stroke_id);
/* Drawing */
void set_frustum_planes(PBVH *pbvh, PBVHFrustumPlanes *planes);
void get_frustum_planes(PBVH *pbvh, PBVHFrustumPlanes *planes);

void draw_cb(const Mesh &mesh,
             PBVH *pbvh,
             bool update_only_visible,
             PBVHFrustumPlanes &update_frustum,
             PBVHFrustumPlanes &draw_frustum,
             const FunctionRef<void(draw::pbvh::PBVHBatches *batches,
                                    const draw::pbvh::PBVH_GPU_Args &args)> draw_fn);

}  // namespace blender::bke::pbvh

/* PBVH Access */

bool BKE_pbvh_has_faces(const PBVH *pbvh);

/**
 * Get the PBVH root's bounding box.
 */
blender::Bounds<blender::float3> BKE_pbvh_bounding_box(const PBVH *pbvh);

/**
 * Multi-res hidden data, only valid for type == PBVH_GRIDS.
 */
blender::BitGroupVector<> *BKE_pbvh_grid_hidden(const PBVH *pbvh);

void BKE_pbvh_sync_visibility_from_verts(PBVH *pbvh, Mesh *me);

namespace blender::bke::pbvh {
int count_grid_quads(const BitGroupVector<> &grid_visibility,
                     Span<int> grid_indices,
                     int gridsize,
                     int display_gridsize);

}

/**
 * Multi-res level, only valid for type == #PBVH_GRIDS.
 */
const CCGKey *BKE_pbvh_get_grid_key(const PBVH *pbvh);

CCGElem **BKE_pbvh_get_grids(const PBVH *pbvh);
blender::BitGroupVector<> *BKE_pbvh_get_grid_visibility(const PBVH *pbvh);
int BKE_pbvh_get_grid_num_verts(const PBVH *pbvh);
int BKE_pbvh_get_grid_num_faces(const PBVH *pbvh);

/* Node Access */

void BKE_pbvh_check_tri_areas(PBVH *pbvh, PBVHNode *node);
void BKE_pbvh_face_areas_begin(Object *ob, PBVH *pbvh);
void BKE_pbvh_face_areas_swap_buffers(Object *ob, PBVH *pbvh);

bool BKE_pbvh_bmesh_check_valence(PBVH *pbvh, PBVHVertRef vertex);
void BKE_pbvh_bmesh_update_valence(PBVH *pbvh, PBVHVertRef vertex);
void BKE_pbvh_bmesh_update_all_valence(PBVH *pbvh);
bool BKE_pbvh_bmesh_mark_update_valence(PBVH *pbvh, PBVHVertRef vertex);

/* if pbvh uses a split index buffer, will call BKE_pbvh_vert_tag_update_normal_triangulation;
   otherwise does nothing.  returns true if BKE_pbvh_vert_tag_update_normal_triangulation was
   called.*/
void BKE_pbvh_vert_tag_update_normal_triangulation(PBVHNode *node);
void BKE_pbvh_node_mark_original_update(PBVHNode *node);
void BKE_pbvh_vert_tag_update_normal_tri_area(PBVHNode *node);
void BKE_pbvh_update_all_tri_areas(PBVH *pbvh);
void BKE_pbvh_node_mark_update(PBVHNode *node);
void BKE_pbvh_node_mark_update_mask(PBVHNode *node);
void BKE_pbvh_node_mark_update_color(PBVHNode *node);
void BKE_pbvh_vert_tag_update_normal_visibility(PBVHNode *node);
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
bool BKE_pbvh_node_fully_unmasked_get(PBVHNode *node);
void BKE_pbvh_node_mark_curvature_update(PBVHNode *node);

void BKE_pbvh_mark_rebuild_pixels(PBVH *pbvh);

blender::Span<int> BKE_pbvh_node_get_grid_indices(const PBVHNode &node);

void BKE_pbvh_node_get_grids(PBVH *pbvh,
                             PBVHNode *node,
                             const int **grid_indices,
                             int *totgrid,
                             int *maxgrid,
                             int *gridsize,
                             CCGElem ***r_griddata);
int BKE_pbvh_node_num_unique_verts(const PBVH &pbvh, const PBVHNode &node);
blender::Span<int> BKE_pbvh_node_get_vert_indices(const PBVHNode *node);
blender::Span<int> BKE_pbvh_node_get_unique_vert_indices(const PBVHNode *node);
blender::Span<int> BKE_pbvh_node_get_corner_indices(const PBVHNode *node);

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

blender::Vector<int> BKE_pbvh_node_calc_face_indices(const PBVH &pbvh, const PBVHNode &node);

blender::Bounds<blender::float3> BKE_pbvh_node_get_BB(const PBVHNode *node);
blender::Bounds<blender::float3> BKE_pbvh_node_get_original_BB(const PBVHNode *node);

float BKE_pbvh_node_get_tmin(PBVHNode *node);

/**
 * Test if AABB is at least partially inside the #PBVHFrustumPlanes volume.
 */
bool BKE_pbvh_node_frustum_contain_AABB(PBVHNode *node, PBVHFrustumPlanes *frustum);
/**
 * Test if AABB is at least partially outside the #PBVHFrustumPlanes volume.
 */
bool BKE_pbvh_node_frustum_exclude_AABB(PBVHNode *node, PBVHFrustumPlanes *frustum);

blender::bke::dyntopo::DyntopoSet<BMVert> &BKE_pbvh_bmesh_node_unique_verts(PBVHNode *node);
blender::bke::dyntopo::DyntopoSet<BMVert> &BKE_pbvh_bmesh_node_other_verts(PBVHNode *node);
blender::bke::dyntopo::DyntopoSet<BMFace> &BKE_pbvh_bmesh_node_faces(PBVHNode *node);

void BKE_pbvh_bmesh_regen_node_verts(PBVH *pbvh, bool report);
void BKE_pbvh_bmesh_mark_node_regen(PBVH *pbvh, PBVHNode *node);

void BKE_pbvh_bmesh_after_stroke(PBVH *pbvh);

/* Update Bounding Box/Redraw and clear flags. */

namespace blender::bke::pbvh {
void update_bounds(PBVH &pbvh, int flags);

void update_mask(PBVH &pbvh);
void update_vertex_data(PBVH &pbvh, int flags);
void update_visibility(PBVH &pbvh);
void update_normals(PBVH &pbvh, SubdivCCG *subdiv_ccg);

}  // namespace blender::bke::pbvh
blender::Bounds<blender::float3> BKE_pbvh_redraw_BB(PBVH *pbvh);

void BKE_pbvh_get_grid_updates(PBVH *pbvh, bool clear, void ***r_gridfaces, int *r_totface);
void BKE_pbvh_grids_update(PBVH *pbvh, const CCGKey *key);
void BKE_pbvh_subdiv_ccg_set(PBVH *pbvh, SubdivCCG *subdiv_ccg);
void BKE_pbvh_face_sets_set(PBVH *pbvh, int *face_sets);

/**
 * If an operation causes the hide status stored in the mesh to change, this must be called
 * to update the references to those attributes, since they are only added when necessary.
 */
void BKE_pbvh_update_hide_attributes_from_mesh(PBVH *pbvh);

/* Vertex Deformer. */

void BKE_pbvh_vert_coords_apply(PBVH *pbvh, blender::Span<blender::float3> vert_positions);
bool BKE_pbvh_is_deformed(PBVH *pbvh);

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
  CCGElem **grids;
  CCGElem *grid;
  blender::BitGroupVector<> *grid_hidden;
  blender::BoundedBitSpan gh;
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
  int bi;
  int bm_cur_set;
  blender::bke::dyntopo::DyntopoSet<BMVert> *bm_unique_verts, *bm_other_verts;
  blender::bke::dyntopo::DyntopoSet<BMVert>::iterator bm_iter, bm_iter_end;

  CustomData *bm_vdata;
  int cd_vert_mask_offset;
  int cd_vcol_offset;

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
        vi.gh = (*vi.grid_hidden)[vi.grid_indices[vi.g]]; \
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
          if (!vi.gh.is_empty()) { \
            if (vi.gh[vi.gy * vi.gridsize + vi.gx]) { \
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
          if (vi.bm_iter == vi.bm_iter_end) { \
            if (vi.bm_cur_set == 0 && mode == PBVH_ITER_ALL) { \
              vi.bm_cur_set = 1; \
              vi.bm_iter = vi.bm_other_verts->begin(); \
              vi.bm_iter_end = vi.bm_other_verts->end(); \
              if (vi.bm_iter == vi.bm_iter_end) { \
                continue; \
              } \
            } \
            else { \
              continue; \
            } \
          } \
          BMVert *bv = *vi.bm_iter; \
          ++vi.bm_iter; \
          vi.bm_vert = bv; \
          vi.vertex.i = (intptr_t)bv; \
          vi.index = BM_elem_index_get(vi.bm_vert); \
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

struct PBVHFaceIter {
  PBVHFaceRef face;
  int index;
  bool *hide;
  int *face_set;
  int i;

  PBVHVertRef *verts;
  int verts_num;

  PBVHVertRef verts_reserved_[PBVH_FACE_ITER_VERTS_RESERVED];
  const PBVHNode *node_;
  PBVHType pbvh_type_;
  int verts_size_;
  blender::bke::dyntopo::DyntopoSet<BMFace>::iterator bm_iter_, bm_iter_end_;
  int cd_face_set_;
  bool *hide_poly_;
  int *face_sets_;
  blender::OffsetIndices<int> face_offsets_;
  blender::Span<int> looptri_faces_;
  blender::Span<int> corner_verts_;
  int prim_index_;
  const SubdivCCG *subdiv_ccg_;
  const BMesh *bm;
  CCGKey subdiv_key_;

  int last_face_index_;
};

void BKE_pbvh_face_iter_init(PBVH *pbvh, PBVHNode *node, PBVHFaceIter *fd);
void BKE_pbvh_face_iter_step(PBVHFaceIter *fd);
bool BKE_pbvh_face_iter_done(PBVHFaceIter *fd);
void BKE_pbvh_face_iter_finish(PBVHFaceIter *fd);

/**
 * Iterate over faces inside a #PBVHNode. These are either base mesh faces
 * (for PBVH_FACES and PBVH_GRIDS) or BMesh faces (for PBVH_BMESH).
 */
#define BKE_pbvh_face_iter_begin(pbvh, node, fd) \
  BKE_pbvh_face_iter_init(pbvh, node, &fd); \
  for (; !BKE_pbvh_face_iter_done(&fd); BKE_pbvh_face_iter_step(&fd)) {

#define BKE_pbvh_face_iter_end(fd) \
  } \
  BKE_pbvh_face_iter_finish(&fd)

blender::MutableSpan<PBVHProxyNode> BKE_pbvh_node_get_proxies(PBVHNode *node);
void BKE_pbvh_node_free_proxies(PBVHNode *node);
PBVHProxyNode &BKE_pbvh_node_add_proxy(PBVH &pbvh, PBVHNode &node);

// void BKE_pbvh_node_BB_reset(PBVHNode *node);
// void BKE_pbvh_node_BB_expand(PBVHNode *node, float co[3]);

bool pbvh_has_mask(const PBVH *pbvh);

bool pbvh_has_face_sets(PBVH *pbvh);

void pbvh_show_mask_set(PBVH *pbvh, bool show_mask);
void pbvh_show_face_sets_set(PBVH *pbvh, bool show_face_sets);

/* Parallelization. */

void BKE_pbvh_parallel_range_settings(TaskParallelSettings *settings,
                                      bool use_threading,
                                      int totnode);

blender::MutableSpan<blender::float3> BKE_pbvh_get_vert_positions(const PBVH *pbvh);
blender::Span<blender::float3> BKE_pbvh_get_vert_normals(const PBVH *pbvh);
const bool *BKE_pbvh_get_vert_hide(const PBVH *pbvh);
bool *BKE_pbvh_get_vert_hide_for_write(PBVH *pbvh);

const bool *BKE_pbvh_get_poly_hide(const PBVH *pbvh);

/* Get active color attribute; if pbvh is non-null
 * and is of type PBVH_BMESH the layer inside of
 * pbvh->header.bm will be returned, otherwise the
 * layer will be looked up inside of me.
 */
bool BKE_pbvh_get_color_layer(PBVH *pbvh,
                              Mesh *me,
                              CustomDataLayer **r_layer,
                              AttrDomain *r_domain);

/* Swaps colors at each element in indices (of domain pbvh->vcol_domain)
 * with values in colors. PBVH_FACES only.*/
void BKE_pbvh_swap_colors(PBVH *pbvh,
                          blender::Span<int> indices,
                          blender::MutableSpan<blender::float4> r_colors);

/* Stores colors from the elements in indices (of domain pbvh->vcol_domain)
 * into colors. PBVH_FACES only.*/
void BKE_pbvh_store_colors(PBVH *pbvh,
                           blender::Span<int> indices,
                           blender::MutableSpan<blender::float4> r_colors);

/* Like BKE_pbvh_store_colors but handles loop->vert conversion. PBVH_FACES only. */
void BKE_pbvh_store_colors_vertex(PBVH *pbvh,
                                  blender::Span<int> indices,
                                  blender::MutableSpan<blender::float4> r_colors);

bool BKE_pbvh_is_drawing(const PBVH *pbvh);

void BKE_pbvh_update_active_vcol(PBVH *pbvh, Mesh *mesh);

void BKE_pbvh_vertex_color_set(PBVH *pbvh, PBVHVertRef vertex, const float color[4]);
void BKE_pbvh_vertex_color_get(const PBVH *pbvh, PBVHVertRef vertex, float r_color[4]);

void BKE_pbvh_ensure_node_loops(PBVH *pbvh);
bool BKE_pbvh_draw_cache_invalid(const PBVH *pbvh);
int BKE_pbvh_debug_draw_gen_get(PBVHNode *node);

int BKE_pbvh_get_node_index(PBVH *pbvh, PBVHNode *node);
int BKE_pbvh_get_node_id(PBVH *pbvh, PBVHNode *node);

void BKE_pbvh_curvature_update_set(PBVHNode *node, bool state);
bool BKE_pbvh_curvature_update_get(PBVHNode *node);

int BKE_pbvh_get_totnodes(PBVH *pbvh);

bool BKE_pbvh_bmesh_check_tris(PBVH *pbvh, PBVHNode *node);
PBVHTriBuf *BKE_pbvh_bmesh_get_tris(PBVH *pbvh, PBVHNode *node);
void BKE_pbvh_bmesh_free_tris(PBVH *pbvh, PBVHNode *node);

/*recalculates boundary flags for *all* vertices.  used by
  symmetrize.*/
void BKE_pbvh_recalc_bmesh_boundary(PBVH *pbvh);
void BKE_pbvh_set_boundary_flags(PBVH *pbvh, int *boundary_flags);

void BKE_pbvh_bmesh_remove_face(PBVH *pbvh, BMFace *f, bool log_face);
void BKE_pbvh_bmesh_remove_edge(PBVH *pbvh, BMEdge *e, bool log_edge);
void BKE_pbvh_bmesh_remove_vertex(PBVH *pbvh, BMVert *v, bool log_vert);
void BKE_pbvh_bmesh_add_face(PBVH *pbvh, BMFace *f, bool log_face, bool force_tree_walk);

/* e_tri and f_example are allowed to be nullptr. */
BMFace *BKE_pbvh_face_create_bmesh(PBVH *pbvh,
                                   BMVert *v_tri[3],
                                   BMEdge *e_tri[3],
                                   const BMFace *f_example);

/* If node is nullptr then one will be found in the pbvh. */
BMVert *BKE_pbvh_vert_create_bmesh(
    PBVH *pbvh, float co[3], float no[3], PBVHNode *node, BMVert *v_example);
PBVHNode *BKE_pbvh_node_from_face_bmesh(PBVH *pbvh, BMFace *f);
PBVHNode *BKE_pbvh_node_from_index(PBVH *pbvh, int node_i);

PBVHNode *BKE_pbvh_get_node_leaf_safe(PBVH *pbvh, int i);

void BKE_pbvh_get_vert_face_areas(PBVH *pbvh, PBVHVertRef vertex, float *r_areas, int valence);
void BKE_pbvh_set_symmetry(PBVH *pbvh, int symmetry, int boundary_symmetry);

int BKE_pbvh_do_fset_symmetry(int fset, const int symflag, const float *co);

/*
Uses pmap to build an array of edge indices surrounding vertex
r_edges, r_edges_size, heap_alloc define an existing array to put data in.

final array is similarly put in these pointers.  note that calling code
may pass a stack allocated array (*heap_alloc should be false), and must
check if heap_alloc is true afterwards and free *r_edges.

r_polys is an array of integer pairs and must be same logical size as r_edges
*/
void BKE_pbvh_pmap_to_edges(PBVH *pbvh,
                            PBVHVertRef vertex,
                            int **r_edges,
                            int *r_edges_size,
                            bool *heap_alloc,
                            int **r_polys);

void BKE_pbvh_distort_correction_set(PBVH *pbvh, eAttrCorrectMode value);

void BKE_pbvh_set_face_areas(PBVH *pbvh, float *face_areas);
void BKE_pbvh_set_bmesh(PBVH *pbvh, BMesh *bm);
void BKE_pbvh_free_bmesh(PBVH *pbvh, BMesh *bm);

void BKE_pbvh_show_orig_set(PBVH *pbvh, bool show_orig);
bool BKE_pbvh_show_orig_get(PBVH *pbvh);

void BKE_pbvh_flush_tri_areas(Object *ob, PBVH *pbvh);
void BKE_pbvh_bmesh_check_nodes(PBVH *pbvh);

#include "BLI_math_vector.hh"

namespace blender::bke::pbvh {

void node_update_visibility_mesh(Span<bool> hide_vert, PBVHNode &node);
void node_update_visibility_grids(const BitGroupVector<> &grid_hidden, PBVHNode &node);
void node_update_visibility_bmesh(PBVHNode &node);
void update_visibility(PBVH &pbvh);

void set_flags_valence(PBVH *pbvh, uint8_t *flags, int *valence);
void set_original(PBVH *pbvh, Span<float3> origco, Span<float3> origno);
void update_vert_boundary_bmesh(int cd_faceset_offset,
                                int cd_vert_node_offset,
                                int cd_face_node_offset,
                                int cd_vcol,
                                int cd_boundary_flag,
                                const int cd_flag,
                                const int cd_valence,
                                BMVert *v,
                                const CustomData *ldata,
                                float sharp_angle_limit);
void update_sharp_vertex_bmesh(BMVert *v, int cd_boundary_flag, const float sharp_angle_limit);

void update_vert_boundary_faces(int *boundary_flags,
                                const int *face_sets,
                                const bool *hide_poly,
                                const blender::int2 *medge,
                                const int *corner_verts,
                                const int *corner_edges,
                                blender::OffsetIndices<int> polys,
                                const blender::GroupedSpan<int> &pmap,
                                PBVHVertRef vertex,
                                const bool *sharp_edges,
                                const bool *seam_edges,
                                uint8_t *flags,
                                int *valence);
void update_edge_boundary_bmesh(BMEdge *e,
                                int cd_faceset_offset,
                                int cd_edge_boundary,
                                const int cd_flag,
                                const int cd_valence,
                                const CustomData *ldata,
                                float sharp_angle_limit);
void update_edge_boundary_faces(int edge,
                                Span<float3> vertex_positions,
                                Span<float3> vertex_normals,
                                Span<blender::int2> edges,
                                OffsetIndices<int> polys,
                                Span<float3> poly_normals,
                                int *edge_boundary_flags,
                                const int *vert_boundary_flags,
                                const int *face_sets,
                                const bool *sharp_edge,
                                const bool *seam_edge,
                                const GroupedSpan<int> &pmap,
                                const GroupedSpan<int> &epmap,
                                const CustomData *ldata,
                                float sharp_angle_limit,
                                blender::Span<int> corner_verts,
                                blender::Span<int> corner_edges);
void update_edge_boundary_grids(int edge,
                                Span<blender::int2> edges,
                                OffsetIndices<int> polys,
                                int *edge_boundary_flags,
                                const int *vert_boundary_flags,
                                const int *face_sets,
                                const bool *sharp_edge,
                                const bool *seam_edge,
                                const GroupedSpan<int> &pmap,
                                const GroupedSpan<int> &epmap,
                                const CustomData *ldata,
                                SubdivCCG *subdiv_ccg,
                                const CCGKey *key,
                                float sharp_angle_limit,
                                blender::Span<int> corner_verts,
                                blender::Span<int> corner_edges);
void update_vert_boundary_grids(PBVH *pbvh, int vertex, const int *face_sets);

bool check_vert_boundary(PBVH *pbvh, PBVHVertRef vertex, const int *face_sets);
bool check_edge_boundary(PBVH *pbvh, PBVHEdgeRef edge, const int *face_sets);

Vector<PBVHNode *> search_gather(PBVH *pbvh,
                                 FunctionRef<bool(PBVHNode &)> scb,
                                 PBVHNodeFlags leaf_flag = PBVH_Leaf);
Vector<PBVHNode *> gather_proxies(PBVH *pbvh);

void node_update_mask_mesh(const Span<float> mask, PBVHNode &node);
void node_update_mask_grids(const CCGKey &key, Span<CCGElem *> grids, PBVHNode &node);
void node_update_mask_bmesh(int mask_offset, PBVHNode &node);

Vector<PBVHNode *> get_flagged_nodes(PBVH *pbvh, int flag);
void set_pmap(PBVH *pbvh, GroupedSpan<int> pmap);
void set_vemap(PBVH *pbvh, GroupedSpan<int> vemap);
GroupedSpan<int> get_pmap(PBVH *pbvh);

void sharp_limit_set(PBVH *pbvh, float limit);
float test_sharp_faces_bmesh(BMFace *f1, BMFace *f2, float limit);
float test_sharp_faces_mesh(int f1,
                            int f2,
                            float limit,
                            blender::Span<blender::float3> positions,
                            blender::OffsetIndices<int> &polys,
                            blender::Span<blender::float3> poly_normals,
                            blender::Span<int> corner_verts);

blender::Span<blender::float3> get_poly_normals(const PBVH *pbvh);
void set_vert_boundary_map(PBVH *pbvh, blender::BitVector<> *vert_boundary_map);
void on_stroke_start(PBVH *pbvh);

}  // namespace blender::bke::pbvh
